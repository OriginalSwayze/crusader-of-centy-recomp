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
 * hit (a number rises from the object and its health bar shows), a drop to 0 a defeat.
 * Both throw particles (the Hit effects setting): sparks from a hit, and from a defeat a
 * flash, a ring and a burst of embers and leaves, which fall and fade.
 *
 * Bosses: each type's set-up ($1B2D8, 32 bytes a type: the routine first) writes its type and
 * then its health, in sixteenths. Most foes start with 1 to 4 points; the large ones (64 pixel
 * hitboxes: types $0E, $1E, $36, $47, $81) and the late guardians 16 to 32. A foe of 16 points
 * or more (BOSS_HP) gets the boss bar at the bottom of the picture from its first hit until it
 * is gone, instead of the small bar over it. */
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
    bool boss;                          /* hurt, and BOSS_HP or more: shown on the boss bar */
} Slot;
#define BOSS_HP 0x100                   /* 16 points */
static struct { int slot, fade; uint16_t hp, max; float shown; } boss = { -1, 0, 0, 0, 0 };
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
bool combat_headless;                   /* test runs: effects as in play, stats untouched */
int combat_test_burst;                  /* --test-burst N: frames until a test burst */
int combat_test_boss;                   /* --test-boss N: frames until a pretend boss is hit (tests) */
static int test_boss_age = -1;
bool combat_log;                        /* --combat-log: print every hit and defeat (tests) */
static uint32_t session_defeated;

/* ---- Particles: world pixels, moved once a game frame --------------------------------- */
typedef struct { float x, y, vx, vy, size; int age, life; uint32_t color; uint8_t kind; } Particle;
enum { P_SPARK, P_EMBER, P_LEAF, P_FLASH, P_RING, P_DROP, P_RIPPLE };
#define MAX_PARTICLES 768
static Particle parts[MAX_PARTICLES];
static int nparts;

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static Particle *new_particle(void) {
    if (nparts >= MAX_PARTICLES) return NULL;
    Particle *p = &parts[nparts++];
    memset(p, 0, sizeof *p);
    return p;
}

static void spark_burst(float x, float y, bool defeat) {
    if (!settings.particles) return;
    int more = settings.particles == 2 ? 2 : 1;
    int n = (defeat ? 10 : 7) * more;
    for (int i = 0; i < n; i++) {               /* sparks: fast, short, white to gold */
        Particle *p = new_particle();
        if (!p) return;
        float a = frand() * 6.2832f, v = 1.4f + frand() * 2.2f;
        p->x = x; p->y = y - 4; p->vx = cosf(a) * v; p->vy = sinf(a) * v - .6f;
        p->size = 1.1f + frand() * .7f; p->life = 9 + rand() % 8; p->kind = P_SPARK;
        p->color = frand() < .5f ? 0xFFF6D8 : 0xFFD050;
    }
    if (!defeat) return;
    Particle *f = new_particle();               /* the flash and the ring */
    if (f) { f->x = x; f->y = y - 4; f->size = 14; f->life = 8; f->kind = P_FLASH; f->color = 0xFFE8B0; }
    Particle *r = new_particle();
    if (r) { r->x = x; r->y = y - 4; r->size = 4; r->life = 16; r->kind = P_RING; r->color = 0xFFD890; }
    for (int i = 0; i < 16 * more; i++) {       /* embers and leaves: slower, they fall */
        Particle *p = new_particle();
        if (!p) return;
        float a = frand() * 6.2832f, v = .5f + frand() * 1.6f;
        bool leaf = frand() < .3f;
        p->x = x + (frand() - .5f) * 8; p->y = y - 4 + (frand() - .5f) * 8;
        p->vx = cosf(a) * v; p->vy = sinf(a) * v - 1.2f;
        p->size = leaf ? 1.6f : 1.0f + frand() * 1.0f; p->life = 28 + rand() % 26;
        p->kind = leaf ? P_LEAF : P_EMBER;
        static const uint32_t embers[3] = { 0xFF9A3C, 0xFFC04A, 0xFF6A2C }, leaves[2] = { 0x8CD25A, 0x5FB048 };
        p->color = leaf ? leaves[rand() % 2] : embers[rand() % 3];
    }
}

static void move_particles(void) {
    for (int i = 0; i < nparts; ) {
        Particle *p = &parts[i];
        if (++p->age >= p->life) { parts[i] = parts[--nparts]; continue; }
        p->x += p->vx; p->y += p->vy;
        switch (p->kind) {
        case P_SPARK: p->vx *= .84f; p->vy = p->vy * .84f + .12f; break;
        case P_EMBER: p->vx *= .93f; p->vy = p->vy * .93f + .06f; break;
        case P_LEAF:  p->vx = p->vx * .9f + sinf(p->age * .35f + p->size * 9) * .18f; p->vy = p->vy * .9f + .05f; break;
        case P_RING:  p->size += 1.6f - p->age * .06f; break;
        case P_DROP:  p->vx *= .96f; p->vy += .16f; break;
        case P_RIPPLE: p->size += .45f; break;
        default: break;
        }
        i++;
    }
}

/* Wading: while the player walks through water (the Water effect on), droplets kick up
 * from the feet and rings spread on the surface. The ground under the feet is read from the
 * frame's planes, so sprites standing over it do not count. */
static void splashes(const HdFrame *f, bool world, float cam_x, float cam_y) {
    static float last_x, last_y;
    static int step;
    /* the record's X is a little left of the sprite's middle */
    float px = (float)(int16_t)peek16(0xAC12) + 7, py = (float)(int16_t)peek16(0xAC16);
    float moved = fabsf(px - last_x) + fabsf(py - last_y);
    last_x = px; last_y = py;
    if (!settings.water || !world || !in_play() || moved < .3f || moved > 12) { step = 0; return; }
    int gx = (int)(px - cam_x) + f->extra, gy = (int)(py - cam_y) + 6;   /* the feet */
    int wet = 0;
    for (int k = -4; k <= 4; k += 4) wet += hd_water_at(gx + k, gy) + hd_water_at(gx + k, gy + 3);
    if (wet < 4) { step = 0; return; }
    if (step++ % 7) return;
    Particle *r = new_particle();
    if (r) { r->x = px; r->y = py + 7; r->size = 3; r->life = 26; r->kind = P_RIPPLE; r->color = 0xE8FFFF; }
    for (int i = 0; i < 4; i++) {
        Particle *p = new_particle();
        if (!p) return;
        float a = 3.1416f + frand() * 3.1416f;  /* upward half */
        p->x = px + (frand() - .5f) * 8; p->y = py + 6;
        p->vx = cosf(a) * (.5f + frand() * .7f); p->vy = sinf(a) * (1.1f + frand() * .9f);
        p->size = 1.f + frand() * .5f; p->life = 12 + rand() % 7; p->kind = P_DROP;
        p->color = frand() < .5f ? 0xFFFFFF : 0xF0FCFF;
    }
}

/* ---- Bestiary ---------------------------------------------------------------------------- */
#define BEAST_MAX 96
#define BEAST_MAGIC 0x54534542u          /* "BEST" */
typedef struct { uint16_t type; uint8_t w, h; uint32_t defeated; uint32_t px[BEAST_PX * BEAST_PX]; } Beast;
static Beast beasts[BEAST_MAX];
static int nbeasts;
static bool beasts_dirty;
/* Each slot's enemy as last seen, kept only while its type has no picture yet. */
static uint32_t slot_pic[SLOTS][BEAST_PX * BEAST_PX];
static uint8_t slot_pw[SLOTS], slot_ph[SLOTS];
static uint16_t slot_pic_type[SLOTS];

static Beast *beast_of(uint16_t type, bool make) {
    for (int i = 0; i < nbeasts; i++) if (beasts[i].type == type) return &beasts[i];
    if (!make || nbeasts >= BEAST_MAX) return NULL;
    Beast *b = &beasts[nbeasts++];
    memset(b, 0, sizeof *b);
    b->type = type;
    return b;
}

/* The sprites drawn over the enemy's middle (sx, sy on the screen), put together front over
 * back, if they fit in BEAST_PX square. The player's own sprites are left out. */
static void capture(const HdFrame *f, int slot, uint16_t type, int sx, int sy) {
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20), found = 0;
    for (int k = 0; k < 80; k++) {
        const HdSprite *q = &f->sprites[k];
        if (!q->drawn || q->player) continue;
        if (sx < q->x - 2 || sx >= q->x + q->w + 2 || sy < q->y - 2 || sy >= q->y + q->h + 2) continue;
        if (q->x < x0) x0 = q->x;
        if (q->y < y0) y0 = q->y;
        if (q->x + q->w > x1) x1 = q->x + q->w;
        if (q->y + q->h > y1) y1 = q->y + q->h;
        found++;
    }
    if (!found || x1 - x0 > BEAST_PX || y1 - y0 > BEAST_PX) return;
    uint32_t *out = slot_pic[slot];
    memset(out, 0, sizeof slot_pic[slot]);
    for (int rank = 79; rank >= 0; rank--)          /* back to front */
        for (int k = 0; k < 80; k++) {
            const HdSprite *q = &f->sprites[k];
            if (!q->drawn || q->player || q->rank != rank) continue;
            if (sx < q->x - 2 || sx >= q->x + q->w + 2 || sy < q->y - 2 || sy >= q->y + q->h + 2) continue;
            for (int y = 0; y < q->h; y++)
                for (int x = 0; x < q->w; x++) {
                    uint32_t p = q->px[y * 32 + x];
                    if (!((p >> 16) & 0x0F)) continue;
                    out[(q->y - y0 + y) * BEAST_PX + (q->x - x0 + x)] = 0xFF000000u | vdp_color_rgb((uint16_t)p);
                }
        }
    slot_pw[slot] = (uint8_t)(x1 - x0); slot_ph[slot] = (uint8_t)(y1 - y0);
    slot_pic_type[slot] = type;
}

static void beast_defeated(int slot, uint16_t type) {
    Beast *b = beast_of(type, true);
    if (!b) return;
    b->defeated++;
    if (!b->w && slot_pic_type[slot] == type && slot_pw[slot]) {
        b->w = slot_pw[slot]; b->h = slot_ph[slot];
        memcpy(b->px, slot_pic[slot], sizeof b->px);
    }
    beasts_dirty = true;
}

static int beast_order[BEAST_MAX];
int combat_beast_count(void) {
    int n = 0;
    for (int i = 0; i < nbeasts; i++) beast_order[n++] = i;
    for (int i = 1; i < n; i++)                     /* most defeated first */
        for (int j = i; j > 0 && beasts[beast_order[j]].defeated > beasts[beast_order[j - 1]].defeated; j--) {
            int t = beast_order[j]; beast_order[j] = beast_order[j - 1]; beast_order[j - 1] = t;
        }
    return n;
}

bool combat_beast(int i, uint32_t *defeated, int *w, int *h, const uint32_t **px) {
    if (i < 0 || i >= nbeasts) return false;
    const Beast *b = &beasts[beast_order[i]];
    *defeated = b->defeated; *w = b->w; *h = b->h; *px = b->px;
    return true;
}

static void beasts_load(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sbestiary.bin", base_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint32_t head[3];
    if (fread(head, 4, 3, f) == 3 && head[0] == BEAST_MAGIC && head[1] == sizeof(Beast) && head[2] <= BEAST_MAX)
        nbeasts = (int)fread(beasts, sizeof(Beast), head[2], f);
    fclose(f);
}

static void beasts_save(const char *base_dir) {
    if (!beasts_dirty) return;
    char path[1100];
    snprintf(path, sizeof path, "%sbestiary.bin", base_dir);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t head[3] = { BEAST_MAGIC, (uint32_t)sizeof(Beast), (uint32_t)nbeasts };
    fwrite(head, 4, 3, f);
    fwrite(beasts, sizeof(Beast), (size_t)nbeasts, f);
    fclose(f);
    beasts_dirty = false;
}

void combat_clear(void) {
    have_last = false; nnumbers = 0; nparts = 0;
    for (int i = 0; i < SLOTS; i++) { slots[i].bar_age = -1; slots[i].boss = false; }
    boss.slot = -1; boss.fade = 0;
}

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

/* --autopilot BUTTON (tests): walk to the nearest object with health and swing at it when
 * close, so a fight can be found and checked without playing. */
uint16_t combat_autopilot(int frame, uint16_t attack) {
    float px = (float)(int16_t)peek16(0xAC12), py = (float)(int16_t)peek16(0xAC16), best = 1e9f, tx = 0, ty = 0;
    for (int i = 0; i < SLOTS; i++) {
        uint32_t a = OBJ_LIST + (uint32_t)i * OBJ_SIZE;
        uint16_t type = peek16(a), hp = peek16(a + 2);
        if (!type || !hp || hp >= 0x8000) continue;
        float x = (float)(int16_t)peek16(a + 4), y = (float)(int16_t)peek16(a + 8), d = fabsf(x - px) + fabsf(y - py);
        if (d < best) { best = d; tx = x; ty = y; }
    }
    if (best > 1e8f) return 0;
    float dx = tx - px, dy = ty - py;
    uint16_t dir = fabsf(dx) > fabsf(dy) ? (dx > 0 ? PAD_RIGHT : PAD_LEFT) : (dy > 0 ? PAD_DOWN : PAD_UP);
    /* wiggle around walls: every 64 frames try the other axis for a while */
    if ((frame / 32) % 4 == 3) dir = fabsf(dx) > fabsf(dy) ? (dy > 0 ? PAD_DOWN : PAD_UP) : (dx > 0 ? PAD_RIGHT : PAD_LEFT);
    if (best < 40 && frame % 12 < 4) return dir | attack;
    return best < 40 && frame % 12 < 8 ? 0 : dir;
}

void combat_frame(const HdFrame *f, bool world, float cam_x, float cam_y, bool live) {
    (void)f; (void)cam_x; (void)cam_y;
    bool play = in_play();
    if (live && play) { session_s += machine_frame_s(); total.play_s += machine_frame_s(); }
    move_particles();
    splashes(f, world, cam_x, cam_y);
    if (combat_test_burst > 0 && --combat_test_burst == 0) {   /* --test-burst: a hit and a defeat by the player */
        float px = (float)(int16_t)peek16(0xAC12), py = (float)(int16_t)peek16(0xAC16);
        spark_burst(px - 20, py, false);
        spark_burst(px + 24, py, true);
    }
    for (int i = 0; i < nnumbers; ) {          /* numbers rise and go */
        if (++numbers[i].age >= 54) { memmove(numbers + i, numbers + i + 1, sizeof numbers[0] * (size_t)(--nnumbers - i)); continue; }
        i++;
    }
    for (int i = 0; i < SLOTS; i++) {
        Slot *s = &slots[i];
        uint32_t a = OBJ_LIST + (uint32_t)i * OBJ_SIZE;
        uint16_t type = peek16(a), hp = peek16(a + 2);
        float x = (float)(int16_t)peek16(a + 4), y = (float)(int16_t)peek16(a + 8);
        /* Defeated: the update clears the health and then either clears the type or turns
         * the object into the defeat puff (type 1) in the same frame. Leaving the screen
         * clears the type but keeps the health, so it is not counted. */
        bool defeated = have_last && s->type && s->type != 1 && s->hp && s->hp < 0x8000 && !hp && (!type || type == 1);
        if (!defeated && (!have_last || type != s->type)) {
            /* A new object in this slot (or none): its health now is its full health. */
            s->type = type; s->hp = s->max = hp; s->bar_age = -1; s->shown = 1; s->x = x; s->y = y; s->boss = false;
            continue;
        }
        if (type) { s->x = x; s->y = y; }
        if (hp > s->max) s->max = hp;
        /* An enemy whose type has no picture yet: keep how it looks while it is on screen. */
        if (world && type && hp && hp < 0x8000 && !defeated) {
            Beast *b = beast_of(type, false);
            if (!b || !b->w) capture(f, i, type, (int)(x - cam_x), (int)(y - cam_y));
        }
        if (hp < s->hp && s->hp < 0x8000 && world) {   /* health of $8000 or more: cannot be hurt */
            int d = s->hp - hp;
            if (combat_log) printf("hit: object %06X type %04X health %u -> %u (%d points)%s\n", 0xFF0000 + a, s->type, s->hp, hp, points(d), defeated ? ", defeated" : "");
            if (settings.combat_info) add_number(s, points(d), defeated && d >= s->max);
            if (live || combat_headless) spark_burst(s->x, s->y, defeated);
            if (defeated && (live || combat_headless)) beast_defeated(i, s->type);
            if (live) {
                total.dealt += (uint32_t)points(d);
                if ((uint32_t)points(d) > total.biggest) total.biggest = (uint32_t)points(d);
                if (defeated) { total.defeated++; session_defeated++; }
                if (defeated) rumble(.45f, .7f, 150); else rumble(.2f, .45f, 80);
            }
            s->bar_age = defeated ? -1 : 0;
            if (s->max >= BOSS_HP && s->max < 0x8000) s->boss = true;
        } else if (s->bar_age >= 0 && ++s->bar_age > 150) s->bar_age = -1;
        float ratio = s->max ? (float)hp / s->max : 0;
        s->shown = s->shown > ratio ? fmaxf(ratio, s->shown - .012f) : ratio;
        s->hp = hp;
        if (defeated) { s->type = type; s->max = 0; s->boss = false; }   /* empty, or the puff */
    }
    /* The boss bar follows the first boss in the list; when none is left it drains and fades. */
    int b = -1;
    for (int i = 0; i < SLOTS && b < 0; i++) if (slots[i].boss && slots[i].type && slots[i].hp) b = i;
    if (combat_test_boss > 0 && --combat_test_boss == 0) test_boss_age = 0;
    if (test_boss_age >= 0) {                   /* --test-boss: 32 points, 3 taken every 20 frames */
        int hp = 0x200 - test_boss_age++ / 20 * 0x30;
        if (hp > 0) { boss.slot = -1; boss.hp = (uint16_t)hp; boss.max = 0x200; if (boss.fade < 256) boss.fade += 16; }
        else { boss.hp = 0; test_boss_age = -1; }
    } else if (b >= 0) {
        boss.slot = b; boss.hp = slots[b].hp; boss.max = slots[b].max;
        if (boss.fade < 256) boss.fade += 16;
    } else if (boss.fade > 0) {
        if (boss.slot < 0 || !slots[boss.slot].boss) boss.hp = 0;    /* defeated, or gone */
        boss.fade -= boss.hp ? 32 : 6;
        if (boss.fade <= 0) { boss.fade = 0; boss.slot = -1; }
    }
    float ratio = boss.max ? (float)boss.hp / boss.max : 0;
    boss.shown = boss.shown > ratio ? fmaxf(ratio, boss.shown - .006f) : ratio;
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
    return p->world && (nparts ||settings.play_timer || (settings.combat_info && (nnumbers || settings.combat_info == 2 || boss.fade)));
}

/* Light added to a pixel: c x a/256 (a up to 256). */
static inline void add_light(uint32_t *q, uint32_t c, int a) {
    uint32_t d = *q;
    int r = (int)((d >> 16) & 255) + ((int)((c >> 16) & 255) * a >> 8);
    int g = (int)((d >> 8) & 255) + ((int)((c >> 8) & 255) * a >> 8);
    int b = (int)(d & 255) + ((int)(c & 255) * a >> 8);
    *q = (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}

/* A soft round light of radius rad (pixels) at (cx, cy). mode 0 adds light, 1 adds only a
 * band at its edge (a ring), 2 lays the colour over (seen on light ground too). */
enum { GLOW_ADD, GLOW_RING, GLOW_SOLID };
static void glow(uint32_t *px, int w, int h, float cx, float cy, float rad, uint32_t c, int a, int mode) {
    if (rad < .5f || a <= 0) return;
    int x0 = (int)(cx - rad - 1), x1 = (int)(cx + rad + 1), y0 = (int)(cy - rad - 1), y1 = (int)(cy + rad + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;
    float inv = 1 / rad, band = rad * .18f + 1;
    for (int y = y0; y <= y1; y++) {
        float dy = (y + .5f - cy);
        for (int x = x0; x <= x1; x++) {
            float dx = (x + .5f - cx), d = sqrtf(dx * dx + dy * dy);
            float k;
            if (mode == GLOW_RING) { float e = fabsf(d - rad) / band; k = e < 1 ? 1 - e : 0; }
            else { float t = 1 - d * inv; k = t > 0 ? t * t * (3 - 2 * t) : 0; }   /* smooth to the edge */
            if (k <= 0) continue;
            if (mode == GLOW_SOLID) put(px, w, h, x, y, 1, c, (int)(a * (k > .5f ? 1 : k * 2)));
            else add_light(&px[(size_t)y * w + x], c, (int)(a * k));
        }
    }
}

static void draw_particles(uint32_t *px, int w, int h, const HdPost *p, float s) {
    for (int i = 0; i < nparts; i++) {
        const Particle *q = &parts[i];
        float t = (float)q->age / q->life, fade = 1 - t;
        float x = (q->x - p->cam_x + p->extra) * s, y = (q->y - p->cam_y) * s;
        switch (q->kind) {
        case P_FLASH: glow(px, w, h, x, y, q->size * s * (.6f + t), q->color, (int)(200 * fade * fade), GLOW_ADD); break;
        case P_RING:  glow(px, w, h, x, y, q->size * s, q->color, (int)(230 * fade), GLOW_RING); break;
        case P_DROP:  glow(px, w, h, x, y, q->size * s, q->color, (int)(235 * fade), GLOW_SOLID); break;
        case P_RIPPLE: {
            /* a flat ring on the water: an ellipse half as tall as it is wide */
            float rx = q->size * s, ry = rx * .45f, band = s * .9f + 1;
            int a = (int)(150 * fade);
            int x0 = (int)(x - rx - band), x1 = (int)(x + rx + band), y0 = (int)(y - ry - band), y1 = (int)(y + ry + band);
            for (int yy = y0 < 0 ? 0 : y0; yy <= y1 && yy < h; yy++)
                for (int xx = x0 < 0 ? 0 : x0; xx <= x1 && xx < w; xx++) {
                    float dx = (xx + .5f - x) / rx, dy = (yy + .5f - y) / ry;
                    float e = fabsf(sqrtf(dx * dx + dy * dy) - 1) * rx / band;
                    if (e < 1) add_light(&px[(size_t)yy * w + xx], q->color, (int)(a * (1 - e)));
                }
            break;
        }
        case P_SPARK: {
            /* a short streak along its path: three lights from where it was to where it is */
            for (int k = 0; k < 3; k++) {
                float b = k / 3.f;
                glow(px, w, h, x - q->vx * s * b, y - q->vy * s * b, q->size * s * (1 - b * .4f) * (.5f + fade * .5f),
                     q->color, (int)(255 * fade * (1 - b * .5f)), k ? GLOW_ADD : GLOW_SOLID);
            }
            break;
        }
        case P_LEAF: {
            /* leaves are solid, not light: a small dark-edged diamond that tumbles */
            float r = q->size * s * (fade < .3f ? fade / .3f : 1);
            int a = (int)(240 * (fade < .3f ? fade / .3f : 1));
            float tilt = .55f + .45f * sinf(q->age * .5f + q->size * 7);
            for (int yy = (int)(y - r); yy <= (int)(y + r); yy++)
                for (int xx = (int)(x - r * tilt); xx <= (int)(x + r * tilt); xx++) {
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    float e = fabsf(xx + .5f - x) / (r * tilt + .01f) + fabsf(yy + .5f - y) / (r + .01f);
                    if (e <= 1) put(px, w, h, xx, yy, 1, e > .7f ? 0x2A4A1E : q->color, a);
                }
            break;
        }
        default:
            glow(px, w, h, x, y, q->size * s * (1.6f + fade), q->color, (int)(110 * fade), GLOW_ADD);   /* its light */
            glow(px, w, h, x, y, q->size * s * (.6f + fade * .4f), q->color, (int)(255 * fade), GLOW_SOLID);
            glow(px, w, h, x, y, q->size * s * .35f, 0xFFF4E0, (int)(220 * fade), GLOW_ADD);   /* hot middle */
            break;
        }
    }
}

/* The boss bar: wide, at the bottom middle of the picture, with the boss's points. */
static void draw_boss_bar(uint32_t *px, int w, int h, float s, int u) {
    int a = boss.fade > 256 ? 256 : boss.fade;
    float ratio = boss.max ? (float)boss.hp / boss.max : 0;
    int bw = (int)(160 * s), bh = 4 * u, x = (w - bw) / 2, y = h - (int)(22 * s);
    uint32_t c = ratio > .5f ? 0xE6463A : ratio > .25f ? 0xF08A30 : 0xF0C83C;
    for (int yy = y - 2 * u; yy < y + bh + 2 * u; yy++)
        for (int xx = x - 2 * u; xx < x + bw + 2 * u; xx++) {
            bool inside = xx >= x && xx < x + bw && yy >= y && yy < y + bh;
            bool rim = !inside && xx >= x - u && xx < x + bw + u && yy >= y - u && yy < y + bh + u;
            uint32_t col = rim ? 0xC9A55A : 0x140C08;
            if (inside) {
                float f = (float)(xx - x) / bw;
                col = f < ratio ? (yy < y + u ? 0xFF8A70 : c) : f < boss.shown ? 0xF4E6C8 : 0x3A2A22;
            }
            put(px, w, h, xx, yy, 1, col, inside || rim ? a * 15 / 16 : a * 3 / 4);
        }
    char text[16];
    snprintf(text, sizeof text, "%d", boss.hp ? points(boss.hp) : 0);
    draw_digits(px, w, h, text, x + bw - digits_width(text, u), y - 9 * u, u, 0xFFF1D0, a);
}

void combat_draw(uint32_t *px, int w, int h, const HdPost *p) {
    if (!p->world) return;
    int u = p->scale < 1 ? 1 : p->scale;
    float s = (float)u;
    draw_particles(px, w, h, p, s);
    if (settings.combat_info == 2)
        for (int i = 0; i < SLOTS; i++) {
            const Slot *e = &slots[i];
            if (e->bar_age < 0 || !e->max || !e->type || e->boss) continue;
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
    if (settings.combat_info == 2 && boss.fade > 0) draw_boss_bar(px, w, h, s, u);
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
    beasts_load(base_dir);
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
    beasts_save(base_dir);
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
