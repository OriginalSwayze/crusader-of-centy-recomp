/* Motion from the game's own data (motion.h).
 *
 * Crusader of Centy builds its sprite list in RAM once a game tick (60 Hz): the list's
 * address is at $FFA000 (8-byte entries as in the VDP's sprite table, x and y plus $80),
 * a sort key per entry goes to the list at $FFA004, and $FFA008 counts the entries. Every
 * entry is added by $019524 ($019504 first drops pieces that are off screen); its callers
 * leave the object being drawn in a0:
 *   a0 in $FFC800-$FFD7FF, on a $40 boundary   an object of the list (combat.c): its
 *                                              middle at +$04 / +$08 (16.16 world pixels)
 *   a0 = $FFB350                                the player character, at $FFAC12 / $FFAC16
 * Anything else (the HUD, text, the title) has no object and never moves between ticks.
 * The list is sorted before it reaches the VDP, so entries are matched to the sprites on
 * screen by what they show (position and pattern), not by their place in the list.
 *
 * Each object's anchor is its world position less the camera ($FFA4F2 / $FFA4F6) when it
 * drew: from one tick to the next its sprites move on screen as the anchor does. A frame
 * only matters above 60 Hz: the pictures between two ticks move each sprite part of the
 * way, as the planes already are (hd.c). */
#include "motion.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "genesis.h"
#include "recomp_rt.h"

#define LIST_PTR  0xA000                /* long: where the list is */
#define COUNT     0xA008
#define OBJ_FIRST 0xC800
#define OBJ_END   0xD800
#define OBJ_SIZE  0x40
#define PLAYER_A0 0xB350
#define PLAYER    0xAC00                /* the player's record: +$12 X, +$16 Y */
#define CAM_X     0xA4F2
#define CAM_Y     0xA4F6
#define MAX_JUMP  48                    /* a bigger move in one tick is a jump: no gliding */
#define MAX_ENT   80

typedef struct {
    uint32_t object;                    /* $FFxxxx */
    uint16_t kind;                      /* the object's first word (its type) when it drew */
    int16_t  x, y;                      /* world position less the camera */
} Anchor;

typedef struct {
    unsigned serial;                    /* 0 = empty */
    int      count;
    uint8_t  raw[MAX_ENT][8];
    int8_t   anchor[MAX_ENT];           /* anchor of each entry, or -1 */
    Anchor   anchors[MAX_ENT];
    int      anchor_count;
} Tick;

static Tick ring[4];
static unsigned serial;

static Tick build;                      /* the list being built */
static int seen_count;

static int ram16(uint32_t a) { return bus.ram[a & 0xFFFF] << 8 | bus.ram[(a + 1) & 0xFFFF]; }
static uint32_t ram32(uint32_t a) { return (uint32_t)ram16(a) << 16 | (uint32_t)ram16(a + 2); }

/* The object drawing now: its anchor in the list being built, or -1. */
static int owner_now(void) {
    uint32_t a0 = A(0) & 0xFFFFFF;
    if (a0 < 0xFF0000) return -1;
    uint32_t a = a0 & 0xFFFF;
    int16_t wx, wy;
    uint16_t kind;
    if (a >= OBJ_FIRST && a < OBJ_END && !((a - OBJ_FIRST) % OBJ_SIZE)) {
        kind = (uint16_t)ram16(a);
        if (!kind) return -1;
        wx = (int16_t)ram16(a + 4); wy = (int16_t)ram16(a + 8);
    } else if (a == PLAYER_A0) {
        kind = (uint16_t)ram16(PLAYER);
        wx = (int16_t)ram16(PLAYER + 0x12); wy = (int16_t)ram16(PLAYER + 0x16);
    } else return -1;
    for (int k = 0; k < build.anchor_count; k++) if (build.anchors[k].object == a) return k;
    if (build.anchor_count >= MAX_ENT) return -1;
    build.anchors[build.anchor_count] = (Anchor){ a, kind, (int16_t)(wx - ram16(CAM_X)), (int16_t)(wy - ram16(CAM_Y)) };
    return build.anchor_count++;
}

/* The list just finished becomes a tick. */
static void take_tick(void) {
    if (!build.count) return;
    build.serial = ++serial;
    ring[serial % 4] = build;
}

/* Called after each native block: a block that added entries ran with its object in a0
 * (the adding routine leaves a0 alone). */
void motion_block(uint32_t pc) {
    (void)pc;
    int count = ram16(COUNT);
    if (count == seen_count) return;
    if (count < seen_count) {           /* a new list: the last one is done */
        take_tick();
        build.count = 0;
        build.anchor_count = 0;
        seen_count = 0;
        if (!count) return;
    }
    int owner = owner_now();
    uint32_t end = A(1) & 0xFFFF;              /* just past the newest entry */
    for (int k = seen_count; k < count && k < MAX_ENT; k++) {
        build.anchor[k] = (int8_t)owner;
        for (int b = 0; b < 8; b++) build.raw[k][b] = bus.ram[(end - (uint32_t)(count - k) * 8 + (uint32_t)b) & 0xFFFF];
    }
    build.count = count < MAX_ENT ? count : MAX_ENT;
    seen_count = count;
}

void motion_reset(void) {
    memset(ring, 0, sizeof ring);
    memset(&build, 0, sizeof build);
    seen_count = ram16(COUNT);
}

/* The entry as the VDP shows it: y, x and pattern match (the link and size may differ). */
static bool same_entry(const uint8_t *e, const HdSprite *s) {
    int y = ((e[0] << 8 | e[1]) & 0x3FF) - 128, x = ((e[6] << 8 | e[7]) & 0x1FF) - 128;
    return y == s->y && x == s->x && (e[4] << 8 | e[5]) == s->attr;
}

/* Which entry of tick t each drawn sprite is (-1: none); false if a drawn sprite with an
 * object-like look matches nothing (t is not the tick on screen). */
static int match(const Tick *t, const HdFrame *f, int which[80]) {
    bool used[MAX_ENT] = { false };
    int found = 0, drawn = 0;
    for (int i = 0; i < 80; i++) {
        which[i] = -1;
        if (!f->sprites[i].drawn) continue;
        drawn++;
        for (int k = 0; k < t->count; k++)
            if (!used[k] && same_entry(t->raw[k], &f->sprites[i])) { used[k] = true; which[i] = k; found++; break; }
    }
    return drawn ? found * 100 / drawn : 0;
}

static const Tick *tick_numbered(unsigned s) {
    const Tick *t = &ring[s % 4];
    return s && t->serial == s ? t : NULL;
}

/* The newest tick whose list is what the frame shows (all its sprites found in it). */
static const Tick *shown_tick(const HdFrame *f, int which[80]) {
    for (unsigned s = serial; s && s + 4 > serial; s--) {
        const Tick *t = tick_numbered(s);
        if (t && match(t, f, which) == 100) return t;
    }
    return NULL;
}

void motion_player_entries(const uint8_t *sat, int count, bool out[80]) {
    memset(out, 0, 80 * sizeof *out);
    const Tick *t = tick_numbered(serial);
    if (!t) return;
    for (int i = 0; i < count && i < 80; i++) {
        const uint8_t *e = sat + i * 8;
        for (int k = 0; k < t->count; k++) {
            const uint8_t *m = t->raw[k];
            if (((e[0] ^ m[0]) & 3) || e[1] != m[1] || e[4] != m[4] || e[5] != m[5] || ((e[6] ^ m[6]) & 1) || e[7] != m[7]) continue;
            out[i] = t->anchor[k] >= 0 && t->anchors[t->anchor[k]].object == PLAYER_A0;
            break;
        }
    }
}

bool motion_entry_owner(int i, uint32_t *object, uint16_t *kind) {
    const Tick *t = tick_numbered(serial);
    if (!t || i < 0 || i >= t->count || t->anchor[i] < 0) return false;
    *object = 0xFF0000 + t->anchors[t->anchor[i]].object;
    *kind = t->anchors[t->anchor[i]].kind;
    return true;
}

unsigned motion_sprite_moves(const HdFrame *f, int dx[80], int dy[80], int *owned) {
    memset(dx, 0, 80 * sizeof *dx);
    memset(dy, 0, 80 * sizeof *dy);
    if (owned) *owned = 0;
    int which[80];
    const Tick *now = shown_tick(f, which);
    if (!now) return 0;
    const Tick *was = tick_numbered(now->serial - 1);
    for (int i = 0; i < 80; i++) {
        if (which[i] < 0 || now->anchor[which[i]] < 0) continue;
        if (owned) ++*owned;
        if (!was) continue;
        const Anchor *a = &now->anchors[now->anchor[which[i]]];
        for (int k = 0; k < was->anchor_count; k++) {
            const Anchor *b = &was->anchors[k];
            if (b->object != a->object || b->kind != a->kind) continue;
            int mx = a->x - b->x, my = a->y - b->y;
            if (abs(mx) <= MAX_JUMP && abs(my) <= MAX_JUMP) { dx[i] = mx; dy[i] = my; }
            break;
        }
    }
    return now->serial;
}
