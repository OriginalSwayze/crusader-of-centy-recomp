/* Weather and ambience over the game world (fx.h): falling leaves, rain, snow or fireflies,
 * picked in EFFECTS > Weather. Purely a picture effect: the game never sees it.
 *
 * Particles live in world pixels (the camera, $FFA4F2 / $FFA4F6, places them), so they stay
 * put while the screen scrolls; one that leaves the view comes back at the far side. They
 * move once per game frame and are drawn onto the finished picture at its scale. Only
 * while the game is in play (HdPost.world); never over the title or story screens. */
#include "fx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"

enum { WEATHER_OFF, WEATHER_LEAVES, WEATHER_RAIN, WEATHER_SNOW, WEATHER_FIREFLIES };

#define MAX_P 220
typedef struct { float x, y, vx, vy, phase, size, land; int splash; uint32_t color; } P;
static P parts[MAX_P];
static int nparts, kind_now = -1;
static float view_x, view_y, view_w = 320, view_h = 224;
static unsigned frame_n;
static bool active;

static uint32_t seed = 0x9E3779B9u;
static inline uint32_t next(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static inline float frand(void) { return (next() & 0xFFFFFF) / 16777216.f; }

/* A new particle somewhere in the view (or above it, for what falls in). */
static void spawn(P *p, int kind, bool anywhere) {
    p->x = view_x - 16 + frand() * (view_w + 32);
    p->y = anywhere ? view_y - 16 + frand() * (view_h + 32) : view_y - 12 - frand() * 24;
    p->phase = frand() * 6.2831853f;
    p->splash = 0;
    switch (kind) {
    case WEATHER_LEAVES: {
        static const uint32_t leaf[4] = { 0x6FA83A, 0x9DBF3E, 0xD9902E, 0xB8562A };
        p->vx = .25f + frand() * .35f; p->vy = .35f + frand() * .3f;
        p->size = 3 + (float)(next() % 2); p->color = leaf[next() % 4];
        break;
    }
    case WEATHER_RAIN:
        p->vx = -.9f; p->vy = 5.5f + frand() * 1.5f; p->size = 9 + frand() * 5; p->color = 0xE4EEFA;
        p->land = view_y + 8 + frand() * (view_h - 8);     /* where it hits the ground */
        if (anywhere) p->y = p->land - frand() * 60;
        break;
    case WEATHER_SNOW:
        p->vx = 0; p->vy = .35f + frand() * .4f; p->size = 1 + (float)(next() % 2); p->color = 0xF4F8FF;
        break;
    default:                            /* fireflies drift anywhere in the view */
        p->x = view_x + frand() * view_w; p->y = view_y + frand() * view_h;
        p->vx = (frand() - .5f) * .4f; p->vy = (frand() - .5f) * .4f; p->size = 1; p->color = 0xE9FF8A;
        break;
    }
}

static int count_for(int kind) {
    switch (kind) {
    case WEATHER_LEAVES: return 36;
    case WEATHER_RAIN: return MAX_P;
    case WEATHER_SNOW: return 140;
    case WEATHER_FIREFLIES: return 28;
    }
    return 0;
}

void fx_clear(void) { nparts = 0; kind_now = -1; }

void fx_game_frame(HdFrame *f, bool world, float cam_x, float cam_y) {
    int kind = settings.weather;
    active = world && kind != WEATHER_OFF;
    if (!active) return;
    view_x = cam_x; view_y = cam_y;
    view_w = f && f->w > 0 ? (float)f->w : 320; view_h = f && f->h > 0 ? (float)f->h : 224;
    frame_n++;
    if (kind != kind_now) {             /* a new weather: fill the view at once */
        kind_now = kind;
        nparts = count_for(kind);
        for (int i = 0; i < nparts; i++) spawn(&parts[i], kind, true);
    }
    for (int i = 0; i < nparts; i++) {
        P *p = &parts[i];
        p->phase += kind == WEATHER_FIREFLIES ? .05f : .06f;
        switch (kind) {
        case WEATHER_LEAVES: p->x += p->vx + sinf(p->phase) * .5f; p->y += p->vy + cosf(p->phase * 1.3f) * .15f; break;
        case WEATHER_SNOW:   p->x += sinf(p->phase) * .3f; p->y += p->vy; break;
        case WEATHER_RAIN:
            if (p->splash) { if (++p->splash > 7) spawn(p, kind, false); continue; }
            p->x += p->vx; p->y += p->vy;
            if (p->y >= p->land) { p->y = p->land; p->splash = 1; }
            break;
        default:
            p->vx += (frand() - .5f) * .04f; p->vy += (frand() - .5f) * .04f;
            p->vx = fmaxf(-.35f, fminf(.35f, p->vx)); p->vy = fmaxf(-.35f, fminf(.35f, p->vy));
            p->x += p->vx; p->y += p->vy;
            break;
        }
        /* Left the view (it scrolled, or the particle fell out of it): back in. */
        bool out = p->y > view_y + view_h + 16 || p->x < view_x - 40 || p->x > view_x + view_w + 40 ||
                   (kind == WEATHER_FIREFLIES && (p->y < view_y - 30));
        if (out) spawn(p, kind, kind == WEATHER_FIREFLIES);
    }
}

/* Blends colour c into pixel q with alpha a (0-256). */
static inline void blend(uint32_t *q, uint32_t c, int a) {
    uint32_t d = *q;
    int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
    dr += ((int)((c >> 16) & 255) - dr) * a >> 8; dg += ((int)((c >> 8) & 255) - dg) * a >> 8; db += ((int)(c & 255) - db) * a >> 8;
    *q = (uint32_t)(dr << 16 | dg << 8 | db);
}
static inline void add(uint32_t *q, uint32_t c, float k) {
    uint32_t d = *q;
    int r = (int)((d >> 16) & 255) + (int)(((c >> 16) & 255) * k), g = (int)((d >> 8) & 255) + (int)(((c >> 8) & 255) * k),
        b = (int)(d & 255) + (int)((c & 255) * k);
    *q = (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}

/* The whole picture a little darker and tinted (rain's overcast, the fireflies' dusk). */
static void grade(uint32_t *px, int w, int h, int keep_r, int keep_g, int keep_b) {
    for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
        uint32_t d = px[i];
        px[i] = (((d >> 16) & 255) * keep_r >> 8) << 16 | (((d >> 8) & 255) * keep_g >> 8) << 8 | ((d & 255) * keep_b >> 8);
    }
}

void fx_draw(uint32_t *px, int w, int h, const HdPost *p) {
    if (!active || !p->world || !nparts) return;
    float s = (float)(p->scale < 1 ? 1 : p->scale);
    int kind = kind_now;
    if (kind == WEATHER_RAIN) grade(px, w, h, 196, 206, 226);
    if (kind == WEATHER_FIREFLIES) grade(px, w, h, 176, 186, 224);
    for (int i = 0; i < nparts; i++) {
        const P *q = &parts[i];
        float sx = (q->x - p->cam_x + p->extra) * s, sy = (q->y - p->cam_y) * s;
        if (kind == WEATHER_RAIN) {
            if (q->splash) {
                /* A small ring spreading where the drop landed. */
                float r = (1.5f + q->splash * .6f) * s, ry = r * .45f;
                int a = 170 - q->splash * 20;
                for (int k = 0; k < 24; k++) {
                    float t = k * 6.2831853f / 24;
                    int x = (int)(sx + cosf(t) * r), y = (int)(sy + sinf(t) * ry);
                    if (x >= 0 && y >= 0 && x < w && y < h) blend(&px[(size_t)y * w + x], q->color, a);
                }
                continue;
            }
            /* A slanted streak, a game pixel wide at most, brighter at its head. */
            int len = (int)(q->size * s), thick = (int)(s * .6f) < 1 ? 1 : (int)(s * .6f);
            for (int k = 0; k < len; k++)
                for (int t = 0; t < thick; t++) {
                    int x = (int)(sx - k * .16f) + t, y = (int)(sy - k);
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    blend(&px[(size_t)y * w + x], q->color, 200 - 130 * k / (len ? len : 1));
                }
        } else if (kind == WEATHER_FIREFLIES) {
            /* A pulsing point of light with a soft glow. */
            float k = .5f + .5f * sinf(q->phase * 3);
            float r = 3.5f * s;
            for (int y = (int)(sy - r); y <= (int)(sy + r); y++)
                for (int x = (int)(sx - r); x <= (int)(sx + r); x++) {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    float dx = x + .5f - sx, dy = y + .5f - sy, d = sqrtf(dx * dx + dy * dy) / r;
                    if (d < 1) add(&px[(size_t)y * w + x], q->color, (1 - d) * (1 - d) * .9f * k);
                }
        } else {
            /* Leaves and snow: small squares at the game's pixel size, leaves turning. */
            int sz = (int)(q->size * s);
            if (kind == WEATHER_LEAVES && sinf(q->phase * 2) < 0) sz = sz > (int)s ? sz - (int)s : sz;   /* edge-on */
            int x0 = (int)sx, y0 = (int)sy;
            for (int y = y0; y < y0 + sz; y++)
                for (int x = x0; x < x0 + (int)(q->size * s); x++) {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    blend(&px[(size_t)y * w + x], q->color, kind == WEATHER_SNOW ? 220 : 240);
                }
        }
    }
}
