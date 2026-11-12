/* The auto-map: see automap.h.
 *
 * From the disassembly (USA addresses):
 *   $FFFE76  the map the player is on (a new game starts in 6, the house; a door sets it
 *            from its object's +$1A at $1C7DA)
 *   $FFA4F2 / $FFA4F6  the camera (world pixels at the picture's top left)
 *   $FFAC12 / $FFAC16  the player's X / Y (world pixels)
 *   $FFA4FC / $FFA500  the camera's largest X / Y in this area: the area is that plus one
 *            screen (320 x 224) wide and tall
 *
 * The game keeps no map size this needs, so each area is a grid of chunks of CHUNK x CHUNK
 * map pixels (CELL world pixels each), made as the camera reaches them. The HUD is made of
 * sprites and the window plane, which are left out; frames in a fade (nothing bright on
 * screen) are skipped, so changing maps does not paint the old one dark. */
#include "automap.h"
#include "story.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genesis.h"
#include "ui.h"

#define CELL     4                      /* world pixels per map pixel */
#define CHUNK    64                     /* map pixels per chunk side (256 world pixels) */
#define GRID     32                     /* chunks per side: worlds up to 8192 pixels */
#define MAX_MAPS 256
#define MAX_DOORS 16
#define DOOR_NEAR 48                    /* world pixels: two exits nearer than this are one door */

typedef struct { int16_t x, y; uint32_t to; } Door;

typedef struct {
    uint32_t key;
    uint16_t *chunk[GRID * GRID];       /* RGB565 + 1 (0 = not seen); NULL until reached */
    uint32_t used;
    uint16_t world_w, world_h;          /* the area's size in world pixels (0 until seen) */
    uint16_t ndoors;
    Door door[MAX_DOORS];
    int8_t place;                       /* the game's place name for it (story.h), -1 unknown */
} Area;

static Area areas[MAX_MAPS];
static int count;
static uint32_t clock_;
static Area *here;
static float player_wx = -1, player_wy = -1;
static int frame_n;
static Area *view;                      /* the area the menu shows (NULL: the current one) */
static uint32_t last_key;               /* the area of the last frame in play, and where the */
static int16_t last_x, last_y;          /* player was in it */

/* The player left area `from` at (x, y) for `to`: keep it as a door unless one is near. */
static void add_door(Area *from, int16_t x, int16_t y, uint32_t to) {
    for (int i = 0; i < from->ndoors; i++) {
        Door *d = &from->door[i];
        if (d->to == to && abs(d->x - x) < DOOR_NEAR && abs(d->y - y) < DOOR_NEAR) return;
    }
    if (from->ndoors < MAX_DOORS) from->door[from->ndoors++] = (Door){ x, y, to };
}

static uint16_t peek16(uint32_t a) { a &= 0xFFFF; return (uint16_t)(bus.ram[a] << 8 | bus.ram[(a + 1) & 0xFFFF]); }

static uint32_t area_key(void) { return 0x10000u | peek16(0xFE76); }

static void free_area(Area *a) {
    for (int i = 0; i < GRID * GRID; i++) { free(a->chunk[i]); a->chunk[i] = NULL; }
    a->key = 0;
    a->ndoors = 0;
    a->world_w = a->world_h = 0;
    if (view == a) view = NULL;
}

static Area *find(uint32_t key, bool make) {
    for (int i = 0; i < count; i++)
        if (areas[i].key == key) { areas[i].used = ++clock_; return &areas[i]; }
    if (!make) return NULL;
    Area *a;
    if (count < MAX_MAPS) a = &areas[count++];
    else {                              /* full: the area not seen for longest goes */
        a = &areas[0];
        for (int i = 1; i < count; i++) if (areas[i].used < a->used) a = &areas[i];
        free_area(a);
    }
    a->key = key; a->used = ++clock_; a->place = -1;
    return a;
}

/* The map pixel at (mx, my), made when `make`; NULL outside the grid. */
static uint16_t *cell(Area *a, int mx, int my, bool make) {
    if (mx < 0 || my < 0 || mx >= GRID * CHUNK || my >= GRID * CHUNK) return NULL;
    uint16_t **c = &a->chunk[(my / CHUNK) * GRID + mx / CHUNK];
    if (!*c) {
        if (!make) return NULL;
        *c = calloc(CHUNK * CHUNK, sizeof **c);
        if (!*c) return NULL;
    }
    return &(*c)[(my % CHUNK) * CHUNK + mx % CHUNK];
}

static uint16_t to565(uint32_t c) {
    uint16_t v = (uint16_t)(((c >> 19) & 31) << 11 | ((c >> 10) & 63) << 5 | ((c >> 3) & 31));
    return v ? v : 1;                   /* 0 is "not seen" */
}

#define SETTLE 120                      /* frames */
static int settle;

/* The scenery's colour at a picture pixel: the top plane where it is drawn (not the window). */
static uint32_t ground_at(const HdFrame *f, int x, int y) {
    uint32_t a = f->a[y][x], b = f->b[y][x];
    if ((a >> 16) & HD_WINDOW) a = 0;
    bool ao = (a >> 16) & 0x0F, ah = (a >> 16) & 0x40, bo = (b >> 16) & 0x0F, bh = (b >> 16) & 0x40;
    uint32_t top = ao && (ah || !(bo && bh)) ? a : bo ? b : 0;
    return top ? vdp_color_rgb((uint16_t)top) : 0;
}

void automap_frame(const HdFrame *f, bool world, float cam_x, float cam_y) {
    if (!world || f->h <= 0) return;
    uint32_t key = area_key();
    if (last_key && key != last_key) {      /* went through a door */
        Area *from = find(last_key, false);
        if (from) add_door(from, last_x, last_y, key);
        settle = SETTLE;
    }
    here = find(key, true);
    if (!here) return;
    int place = story_place_now();
    if (place > 0) here->place = (int8_t)place;
    int ww = (int)peek16(0xA4FC) + 320, wh = (int)peek16(0xA500) + 224;
    if (ww > 320 && ww <= GRID * CHUNK * CELL) here->world_w = (uint16_t)ww;
    else if (ww == 320) here->world_w = 320;
    if (wh >= 224 && wh <= GRID * CHUNK * CELL) here->world_h = (uint16_t)wh;
    player_wx = (float)(int16_t)peek16(0xAC12);
    player_wy = (float)(int16_t)peek16(0xAC16);
    last_key = key;
    last_x = (int16_t)peek16(0xAC12); last_y = (int16_t)peek16(0xAC16);
    if (++frame_n % 4) return;          /* the scenery barely moves in four frames */
    /* A fade darkens every colour: skip frames with nothing bright in them. */
    int bright = 0;
    for (int y = 8; y < f->h && !bright; y += 16)
        for (int x = 8; x < f->w; x += 16) {
            uint32_t c = ground_at(f, x, y);
            if (((c >> 16) & 255) > 150 || ((c >> 8) & 255) > 150 || (c & 255) > 150) { bright = 1; break; }
        }
    /* The game changes the map number first and fades out after, with the old area still on
     * screen: after a change, nothing is painted until the fade has gone dark (or SETTLE frames). */
    if (settle > 0) {
        settle -= 4;
        if (!bright) settle = 0;
        return;
    }
    if (!bright) return;
    for (int y = CELL / 2; y < f->h; y += CELL) {
        int my = (int)floorf((y + cam_y) / CELL);
        for (int x = CELL / 2; x < f->w; x += CELL) {
            int mx = (int)floorf((x - f->extra + cam_x) / CELL);
            uint32_t c = ground_at(f, x, y);
            if (!c) continue;
            uint16_t *p = cell(here, mx, my, true);
            if (p) *p = to565(c);
        }
    }
}

void automap_view_reset(void) { view = NULL; }

void automap_view_step(int dir) {
    if (!count) return;
    Area *v = view ? view : here;
    int i = v ? (int)(v - areas) : 0;
    for (int n = 0; n < count; n++) {           /* the next area with something seen */
        i = (i + dir + count) % count;
        if (!areas[i].key) continue;
        bool seen = false;
        for (int c = 0; c < GRID * GRID && !seen; c++) seen = areas[i].chunk[c] != NULL;
        if (seen) break;
    }
    view = &areas[i] == here ? NULL : &areas[i];
}

static Area *shown(void) { return view ? view : here; }

/* A small diamond at canvas pixel (cx, cy), radius r. */
static void diamond(float cx, float cy, float r, Rgb fill, Rgb ring, float alpha) {
    for (int py = (int)(cy - r - 1); py <= (int)(cy + r + 1); py++)
        for (int pxx = (int)(cx - r - 1); pxx <= (int)(cx + r + 1); pxx++) {
            float d = fabsf(pxx + .5f - cx) + fabsf(py + .5f - cy);
            if (d < r * .62f) ui_blend_px(pxx, py, fill, alpha);
            else if (d < r) ui_blend_px(pxx, py, ring, alpha * .85f);
        }
}

bool automap_draw(float x, float y, float w, float h, float t, float alpha) {
    Area *here = shown();
    if (!here) return false;
    /* The part seen so far, by chunks, then by pixels at its edges. */
    int x0 = GRID * CHUNK, y0 = GRID * CHUNK, x1 = -1, y1 = -1;
    for (int cy = 0; cy < GRID; cy++)
        for (int cx = 0; cx < GRID; cx++) {
            const uint16_t *c = here->chunk[cy * GRID + cx];
            if (!c) continue;
            for (int i = 0; i < CHUNK * CHUNK; i++) {
                if (!c[i]) continue;
                int mx = cx * CHUNK + i % CHUNK, my = cy * CHUNK + i / CHUNK;
                if (mx < x0) x0 = mx;
                if (mx > x1) x1 = mx;
                if (my < y0) y0 = my;
                if (my > y1) y1 = my;
            }
        }
    if (x1 < 0) return false;
    int sw = x1 - x0 + 1, sh = y1 - y0 + 1;
    float k = w / sw < h / sh ? w / sw : h / sh;    /* layout units per map pixel */
    if (k > 6) k = 6;
    float ox = x + (w - sw * k) / 2, oy = y + (h - sh * k) / 2;
    int px0 = (int)ui_x(ox), py0 = (int)ui_y(oy), px1 = (int)ui_x(ox + sw * k), py1 = (int)ui_y(oy + sh * k);
    for (int py = py0; py < py1; py++) {
        int my = y0 + (int)((float)(py - py0) / (py1 - py0) * sh);
        for (int pxx = px0; pxx < px1; pxx++) {
            int mx = x0 + (int)((float)(pxx - px0) / (px1 - px0) * sw);
            const uint16_t *p = cell(here, mx, my, false);
            if (!p || !*p) continue;
            uint16_t v = *p;
            Rgb c = rgb(((v >> 11) & 31) / 31.f, ((v >> 5) & 63) / 63.f, (v & 31) / 31.f);
            ui_blend_px(pxx, py, c, alpha);
        }
    }
    /* Doors: a pale diamond each, with the number of the area it leads to. */
    for (int i = 0; i < here->ndoors; i++) {
        const Door *d = &here->door[i];
        float ax = ox + ((float)d->x / CELL - x0) * k, ay = oy + ((float)d->y / CELL - y0) * k;
        if (ax < x || ax > x + w || ay < y || ay > y + h) continue;
        float r = ui_x(ax + 8) - ui_x(ax);
        diamond(ui_x(ax), ui_y(ay), r, rgb_hex(0x9FE3FF), rgb_hex(0x0A1418), alpha);
        char label[16];
        snprintf(label, sizeof label, "%u", (unsigned)(d->to & 0xFFFF));
        float lw = ui_text_width(label, "Segoe UI Semibold", 12, 600, 0);
        float lx = ax + 9, ly = ay - 9;
        if (lx + lw > x + w) lx = ax - 9 - lw;
        ui_fill_rect(lx - 3, ly, lw + 6, 17, rgb_hex(0x0A1418), alpha * .75f);
        ui_draw_text(label, "Segoe UI Semibold", 12, 600, 0, lx, ly, rgb_hex(0x9FE3FF), rgb_hex(0x9FE3FF), alpha);
    }
    /* The player: a blinking gold dot with a dark ring. */
    if (here == shown() && !view && player_wx >= 0) {
        float ax = ox + (player_wx / CELL - x0) * k, ay = oy + (player_wy / CELL - y0) * k;
        float pulse = .6f + .4f * sinf(t * 6);
        float r = ui_x(ax + 7) - ui_x(ax), cx = ui_x(ax), cy = ui_y(ay);
        for (int py = (int)(cy - r - 2); py <= (int)(cy + r + 2); py++)
            for (int pxx = (int)(cx - r - 2); pxx <= (int)(cx + r + 2); pxx++) {
                float dx = pxx + .5f - cx, dy = py + .5f - cy, d = sqrtf(dx * dx + dy * dy);
                if (d < r * .6f) ui_blend_px(pxx, py, rgb_hex(0xFFD24A), alpha * pulse);
                else if (d < r) ui_blend_px(pxx, py, rgb_hex(0x140C08), alpha * .8f);
            }
    }
    return true;
}

int automap_places_seen(bool seen[32]) {
    memset(seen, 0, 32 * sizeof *seen);
    int n = 0;
    for (int i = 0; i < count; i++)
        if (areas[i].key && areas[i].place > 0 && areas[i].place < 32 && !seen[(int)areas[i].place]) { seen[(int)areas[i].place] = true; n++; }
    return n;
}

int automap_places(void) {
    bool seen[32];
    return automap_places_seen(seen);
}

void automap_info(AutomapInfo *o) {
    memset(o, 0, sizeof *o);
    o->percent = -1;
    Area *a = shown();
    for (int i = 0; i < count; i++) {
        bool seen = false;
        for (int c = 0; c < GRID * GRID && !seen; c++) seen = areas[i].chunk[c] != NULL;
        if (!seen) continue;
        o->areas++;
        if (&areas[i] == a) o->index = o->areas;
    }
    if (!a) return;
    o->map = (int)(a->key & 0xFFFF);
    o->place = a->place > 0 ? story_place_name(a->place) : NULL;
    o->doors = a->ndoors;
    o->current = a == here;
    if (!a->world_w || !a->world_h) return;
    int mw = a->world_w / CELL, mh = a->world_h / CELL;
    long seen = 0;
    for (int my = 0; my < mh; my++)
        for (int mx = 0; mx < mw; mx++) {
            const uint16_t *p = cell(a, mx, my, false);
            if (p && *p) seen++;
        }
    int pc = (int)(seen * 100 / ((long)mw * mh));
    o->percent = pc > 100 ? 100 : pc;
}

/* ---- automap.bin: "CCAM", version, area count; per area its key, chunk count and (from version 2) size, then
 * per chunk its index and pixels, then (from version 3) its door count and doors, then (from version 4) its place ------------------------------------------------------------ */
#define MAGIC 0x4D414343u

void automap_save(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sautomap.bin", base_dir);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t head[3] = { MAGIC, 4, (uint32_t)count };
    fwrite(head, sizeof head, 1, f);
    for (int i = 0; i < count; i++) {
        uint32_t n = 0;
        for (int c = 0; c < GRID * GRID; c++) n += areas[i].chunk[c] != NULL;
        uint32_t a[3] = { areas[i].key, n, (uint32_t)areas[i].world_w << 16 | areas[i].world_h };
        fwrite(a, sizeof a, 1, f);
        for (uint32_t c = 0; c < GRID * GRID; c++) {
            if (!areas[i].chunk[c]) continue;
            fwrite(&c, sizeof c, 1, f);
            fwrite(areas[i].chunk[c], sizeof(uint16_t), CHUNK * CHUNK, f);
        }
        uint32_t nd = areas[i].ndoors;
        fwrite(&nd, sizeof nd, 1, f);
        for (uint32_t d = 0; d < nd; d++) {
            uint32_t rec[2] = { (uint32_t)(uint16_t)areas[i].door[d].x << 16 | (uint16_t)areas[i].door[d].y, areas[i].door[d].to };
            fwrite(rec, sizeof rec, 1, f);
        }
        int32_t place = areas[i].place;
        fwrite(&place, sizeof place, 1, f);
    }
    fclose(f);
}

void automap_load(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sautomap.bin", base_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint32_t head[3];
    if (fread(head, sizeof head, 1, f) == 1 && head[0] == MAGIC && head[1] >= 1 && head[1] <= 4)
        for (uint32_t i = 0; i < head[2] && i < MAX_MAPS; i++) {
            uint32_t a[3] = { 0, 0, 0 };
            uint32_t na = head[1] >= 2 ? 3 : 2;
            if (fread(a, 4, na, f) != na || a[1] > GRID * GRID) break;
            Area *ar = find(a[0], true);
            if (!ar) break;
            ar->world_w = (uint16_t)(a[2] >> 16); ar->world_h = (uint16_t)a[2];
            bool ok = true;
            for (uint32_t k = 0; k < a[1] && ok; k++) {
                uint32_t c;
                ok = fread(&c, sizeof c, 1, f) == 1 && c < GRID * GRID;
                if (ok && !ar->chunk[c]) ok = (ar->chunk[c] = calloc(CHUNK * CHUNK, sizeof(uint16_t))) != NULL;
                if (ok) ok = fread(ar->chunk[c], sizeof(uint16_t), CHUNK * CHUNK, f) == CHUNK * CHUNK;
            }
            uint32_t nd = 0;
            if (ok && head[1] >= 3) ok = fread(&nd, sizeof nd, 1, f) == 1 && nd <= MAX_DOORS;
            for (uint32_t d = 0; d < nd && ok; d++) {
                uint32_t rec[2];
                ok = fread(rec, sizeof rec, 1, f) == 1;
                if (ok) ar->door[d] = (Door){ (int16_t)(rec[0] >> 16), (int16_t)rec[0], rec[1] };
            }
            if (ok) ar->ndoors = (uint16_t)nd;
            int32_t place = -1;
            if (ok && head[1] >= 4) ok = fread(&place, sizeof place, 1, f) == 1;
            if (ok) ar->place = (int8_t)(place >= 1 && place < 32 ? place : -1);
            if (!ok) break;
        }
    fclose(f);
    here = NULL;
    view = NULL;
    last_key = 0;
    settle = 0;
}
