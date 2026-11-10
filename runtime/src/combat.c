/* Combat info, rumble and stats: see combat.h.
 *
 * From the disassembly (USA addresses):
 *   $FFC800  the object list: 64 records of $40 bytes (cleared together at $1E68).
 *            +$00 type (0 = free), +$02 health (sixteenths of a point: the game shows it
 *            shifted right by 4), +$04 X and +$08 Y (16.16, world pixels, the object's
 *            middle), +$20 a timer for being hit, +$24 damage waiting to be taken.
 *   $1C398   runs every object's update ($1C45E), which takes +$24 off +$02 at $1C4E6; at 0
 *            or below the object is defeated ($1C4EE: the death effect, then the type
 *            cleared). Objects whose health is 0 cannot be hurt.
 *   $FFA4F2 / $FFA4F6  the camera's X / Y (world pixels at the picture's top left).
 *   $FFAC00  the player's record (its first word is 0 until play starts);
 *   $FFFE62 / $FFFE60  the player's health and maximum, in apples.
 *
 * After each game frame the objects' health is compared with the frame before: a drop is a
 * hit (a number rises from the object and its health bar shows), a drop to 0 a defeat. */
#include "combat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genesis.h"
#include "pads.h"
#include "settings.h"

#define OBJ_LIST  0xC800
#define OBJ_SIZE  0x40
#define SLOTS     64

static uint16_t peek16(uint32_t a) { a &= 0xFFFF; return (uint16_t)(bus.ram[a] << 8 | bus.ram[(a + 1) & 0xFFFF]); }

/* In play: the player's record is set up. */
static bool in_play(void) { return peek16(0xAC00) != 0; }

/* ---- What each slot held last frame ---------------------------------------------------- */
typedef struct {
    uint16_t type, hp, max;
    float x, y;                         /* its middle, world pixels */
    int bar_age;                        /* frames since it was last hurt; <0: no bar */
    float shown;                        /* the bar's pale part, catching up with the health */
} Slot;
static Slot slots[SLOTS];
static uint16_t player_hp;
static bool have_last;

typedef struct { float x, y; int value, age; bool big; } Number;
#define MAX_NUMBERS 32
static Number numbers[MAX_NUMBERS];
static int nnumbers;

/* ---- Stats ------------------------------------------------------------------------------ */
static struct {
    double play_s;                      /* all sessions */
    uint32_t defeated, dealt, taken, hits_taken, deaths, biggest;
} total;
static double session_s;
bool combat_log;                        /* --combat-log: print every hit and defeat (tests) */
static uint32_t session_defeated;

void combat_clear(void) { have_last = false; nnumbers = 0; for (int i = 0; i < SLOTS; i++) slots[i].bar_age = -1; }

static void rumble(float low, float high, int ms) {
    if (!settings.rumble) return;
    float k = settings.rumble == 2 ? 1.f : .55f;
    pads_rumble(low * k, high * k, ms);
}

/* Health in the points the game counts (sixteenths, rounded; a hit is at least 1). */
static int points(int sixteenths) { int p = (sixteenths + 8) / 16; return p < 1 ? 1 : p; }

static void add_number(const Slot *s, int value, bool big) {
    if (nnumbers >= MAX_NUMBERS) memmove(numbers, numbers + 1, sizeof numbers[0] * --nnumbers);
    Number *n = &numbers[nnumbers++];
    n->x = s->x + (float)((rand() % 7) - 3);
    n->y = s->y - 10;
    n->value = value; n->age = 0; n->big = big;
}

void combat_frame(const HdFrame *f, bool world, float cam_x, float cam_y, bool live) {
    (void)f; (void)cam_x; (void)cam_y;
    bool play = in_play();
    if (live && play) { session_s += machine_frame_s(); total.play_s += machine_frame_s(); }
    for (int i = 0; i < nnumbers; ) {          /* numbers rise and go */
        if (++numbers[i].age >= 54) { memmove(numbers + i, numbers + i + 1, sizeof numbers[0] * (size_t)(--nnumbers - i)); continue; }
        i++;
    }
    for (int i = 0; i < SLOTS; i++) {
        Slot *s = &slots[i];
        uint32_t a = OBJ_LIST + (uint32_t)i * OBJ_SIZE;
        uint16_t type = peek16(a), hp = peek16(a + 2);
        float x = (float)(int16_t)peek16(a + 4), y = (float)(int16_t)peek16(a + 8);
        /* Defeated: the update clears the health, then the type, in the same frame. */
        bool defeated = have_last && s->type && s->hp && !type && !hp;
        if (!defeated && (!have_last || type != s->type)) {
            /* A new object in this slot (or none): its health now is its full health. */
            s->type = type; s->hp = s->max = hp; s->bar_age = -1; s->shown = 1; s->x = x; s->y = y;
            continue;
        }
        if (type) { s->x = x; s->y = y; }
        if (hp > s->max) s->max = hp;
        if (hp < s->hp && world) {
            int d = s->hp - hp;
            if (combat_log) printf("hit: object %06X type %04X health %u -> %u (%d points)%s\n", 0xFF0000 + a, s->type, s->hp, hp, points(d), defeated ? ", defeated" : "");
            if (settings.combat_info) add_number(s, points(d), defeated && d >= s->max);
            if (live) {
                total.dealt += (uint32_t)points(d);
                if ((uint32_t)points(d) > total.biggest) total.biggest = (uint32_t)points(d);
                if (defeated) { total.defeated++; session_defeated++; }
                if (defeated) rumble(.45f, .7f, 150); else rumble(.2f, .45f, 80);
            }
            s->bar_age = defeated ? -1 : 0;
        } else if (s->bar_age >= 0 && ++s->bar_age > 150) s->bar_age = -1;
        float ratio = s->max ? (float)hp / s->max : 0;
        s->shown = s->shown > ratio ? fmaxf(ratio, s->shown - .012f) : ratio;
        s->hp = hp;
        if (defeated) s->type = s->max = 0;
    }
    uint16_t hp_now = peek16(0xFE62);
    if (have_last && live && play && hp_now < player_hp) {
        total.taken += player_hp - hp_now;
        total.hits_taken++;
        if (!hp_now) total.deaths++;
        rumble(.85f, .5f, 220);
    }
    player_hp = hp_now;
    have_last = true;
}

/* ---- Drawing: a small pixel font, at the game's pixel size ------------------------------- */
static const uint16_t glyphs[11] = {   /* 3x5, rows top to bottom, 3 bits each; 10 is ':' */
    075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717, 002020,
};

static inline void put(uint32_t *px, int w, int h, int x, int y, int size, uint32_t c, int a) {
    for (int yy = y; yy < y + size; yy++) {
        if (yy < 0 || yy >= h) continue;
        for (int xx = x; xx < x + size; xx++) {
            if (xx < 0 || xx >= w) continue;
            uint32_t *q = &px[(size_t)yy * w + xx], d = *q;
            int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
            dr += ((int)((c >> 16) & 255) - dr) * a >> 8; dg += ((int)((c >> 8) & 255) - dg) * a >> 8; db += ((int)(c & 255) - db) * a >> 8;
            *q = (uint32_t)(dr << 16 | dg << 8 | db);
        }
    }
}

/* Draws digits and ':' with the left edge at x, top at y; u is one font pixel. A dark
 * outline keeps it readable on any ground. */
static void draw_digits(uint32_t *px, int w, int h, const char *s, float x, float y, int u, uint32_t c, int a) {
    for (int pass = 0; pass < 2; pass++) {
        float cx = x;
        for (const char *p = s; *p; p++) {
            int g = *p == ':' ? 10 : *p - '0';
            if (g < 0 || g > 10) { cx += 4 * u; continue; }
            for (int row = 0; row < 5; row++)
                for (int col = 0; col < 3; col++) {
                    if (!((glyphs[g] >> ((4 - row) * 3 + (2 - col))) & 1)) continue;
                    int gx = (int)cx + col * u, gy = (int)y + row * u;
                    if (pass == 0) put(px, w, h, gx - u, gy - u, u * 3, 0x140C08, a * 3 / 4);
                    else put(px, w, h, gx, gy, u, c, a);
                }
            cx += (g == 10 ? 2 : 4) * u;
        }
    }
}

static float digits_width(const char *s, int u) {
    float wd = 0;
    for (const char *p = s; *p; p++) wd += (*p == ':' ? 2 : 4) * u;
    return wd - u;
}

bool combat_active(const HdPost *p) {
    return p->world && (settings.play_timer || (settings.combat_info && (nnumbers || settings.combat_info == 2)));
}

void combat_draw(uint32_t *px, int w, int h, const HdPost *p) {
    if (!p->world) return;
    int u = p->scale < 1 ? 1 : p->scale;
    float s = (float)u;
    if (settings.combat_info == 2)
        for (int i = 0; i < SLOTS; i++) {
            const Slot *e = &slots[i];
            if (e->bar_age < 0 || !e->max || !e->type) continue;
            int a = e->bar_age > 120 ? (150 - e->bar_age) * 256 / 30 : 256;
            float bw = 20;
            float x = (e->x - bw / 2 - p->cam_x + p->extra) * s, y = (e->y - 18 - p->cam_y) * s;
            int iw = (int)(bw * s), ih = 2 * u;
            float ratio = (float)e->hp / e->max;
            uint32_t c = ratio > .5f ? 0x6EDC5A : ratio > .25f ? 0xF0C83C : 0xE6463A;
            for (int yy = (int)y - u; yy < (int)y + ih + u; yy++)
                for (int xx = (int)x - u; xx < (int)x + iw + u; xx++) {
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    bool inside = xx >= (int)x && xx < (int)x + iw && yy >= (int)y && yy < (int)y + ih;
                    uint32_t col = 0x140C08;
                    if (inside) {
                        float f = (xx - x) / iw;
                        col = f < ratio ? c : f < e->shown ? 0xF4E6C8 : 0x3A2A22;
                    }
                    put(px, w, h, xx, yy, 1, col, a * 7 / 8);
                }
        }
    if (settings.combat_info)
        for (int i = 0; i < nnumbers; i++) {
            const Number *n = &numbers[i];
            char text[12];
            snprintf(text, sizeof text, "%d", n->value);
            float rise = 14 * (1 - expf(-n->age / 10.f));
            int a = n->age > 38 ? (54 - n->age) * 256 / 16 : 256;
            int uu = n->big ? u * 2 : u;
            float tx = (n->x - p->cam_x + p->extra) * s - digits_width(text, uu) / 2, ty = (n->y - rise - p->cam_y) * s - 5 * uu;
            draw_digits(px, w, h, text, tx, ty, uu, n->big ? 0xFFD24A : 0xFFFFFF, a);
        }
    if (settings.play_timer) {
        int t = (int)session_s;
        char text[16];
        snprintf(text, sizeof text, "%d:%02d:%02d", t / 3600, t / 60 % 60, t % 60);
        draw_digits(px, w, h, text, w - digits_width(text, u) - 8 * s, 8 * s, u, 0xFFF1D0, 230);
    }
}

/* ---- Stats file -------------------------------------------------------------------------- */
void combat_stats_load(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sstats.ini", base_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *v = eq + 1;
        if (!strcmp(line, "play_seconds")) total.play_s = atof(v);
        else if (!strcmp(line, "enemies_defeated")) total.defeated = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(line, "damage_dealt")) total.dealt = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(line, "damage_taken")) total.taken = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(line, "hits_taken")) total.hits_taken = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(line, "deaths")) total.deaths = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(line, "biggest_hit")) total.biggest = (uint32_t)strtoul(v, NULL, 10);
    }
    fclose(f);
}

void combat_stats_save(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sstats.ini", base_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Crusader of Centy play stats, over all sessions\n");
    fprintf(f, "play_seconds=%.0f\n", total.play_s);
    fprintf(f, "enemies_defeated=%u\n", total.defeated);
    fprintf(f, "damage_dealt=%u\n", total.dealt);
    fprintf(f, "damage_taken=%u\n", total.taken);
    fprintf(f, "hits_taken=%u\n", total.hits_taken);
    fprintf(f, "deaths=%u\n", total.deaths);
    fprintf(f, "biggest_hit=%u\n", total.biggest);
    fclose(f);
}

static void hms(char *out, size_t n, double s) {
    int t = (int)s;
    snprintf(out, n, "%d:%02d:%02d", t / 3600, t / 60 % 60, t % 60);
}

int combat_stats_lines(char lines[][64], int max) {
    char a[24], b[24];
    hms(a, sizeof a, session_s);
    hms(b, sizeof b, total.play_s);
    int n = 0;
#define LINE(...) do { if (n < max) snprintf(lines[n++], 64, __VA_ARGS__); } while (0)
    LINE("This session\t%s", a);
    LINE("Enemies defeated this session\t%u", session_defeated);
    LINE("Total play time\t%s", b);
    LINE("Enemies defeated\t%u", total.defeated);
    LINE("Damage dealt\t%u", total.dealt);
    LINE("Biggest hit\t%u", total.biggest);
    LINE("Hits taken\t%u", total.hits_taken);
    LINE("Apples lost\t%u", total.taken);
    LINE("Deaths\t%u", total.deaths);
#undef LINE
    return n;
}
