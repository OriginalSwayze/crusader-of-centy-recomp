/* The Crusader of Centy look: see theme.h. Everything is drawn per pixel into ui_px, with
 * the shapes measured in layout units so it is sharp at any window size. */
#include "theme.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HEAD_FACE "Palatino Linotype"

/* ---- The dialogue box ------------------------------------------------------------------ *
 * From the outside in: a dark outline, the checkered band (orange and gold squares, each
 * lit on its top left and shaded on its bottom right like the game's tiles), a dark line,
 * a thin warm highlight, then the slate body, a little lighter at the top. */
#define BORDER 13.f                     /* layout units from the outer edge to the body */
#define CHECK  4.5f                     /* one square of the band */

void theme_frame(float x, float y, float w, float h, bool leaves, float alpha) {
    float r = 10;
    int x0 = (int)floorf(ui_x(x)) - 1, y0 = (int)floorf(ui_y(y)) - 1, x1 = (int)ceilf(ui_x(x + w)) + 1, y1 = (int)ceilf(ui_y(y + h)) + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > ui_w) x1 = ui_w;
    if (y1 > ui_h) y1 = ui_h;
    float px1 = 1 / ui_s;               /* one pixel in layout units */
    for (int py = y0; py < y1; py++) {
        float v = (py + .5f - ui_oy) / ui_s;
        for (int pxx = x0; pxx < x1; pxx++) {
            float u = (pxx + .5f - ui_ox) / ui_s;
            float d = ui_rrect_dist(u, v, x, y, w, h, r);   /* negative inside */
            if (d > px1) continue;
            float cover = clampf(.5f - d / px1, 0, 1);
            float in = -d;                                   /* depth from the outer edge */
            Rgb c;
            if (in < 2.2f) c = rgb_hex(0x141216);            /* outline */
            else if (in < 2.2f + 8) {                        /* the checkered band */
                float bu = u - x, bv = v - y;
                int cx = (int)floorf(bu / CHECK), cy = (int)floorf(bv / CHECK);
                bool gold = ((cx + cy) & 1) == 0;
                Rgb base = gold ? rgb_hex(0xF6C443) : rgb_hex(0xE2682A);
                float fu = bu / CHECK - floorf(bu / CHECK), fv = bv / CHECK - floorf(bv / CHECK);
                if (fu < .22f || fv < .22f) base = mix(base, rgb(1, .97f, .85f), .38f);        /* lit edge */
                else if (fu > .78f || fv > .78f) base = scale(base, .62f);                      /* shaded edge */
                float band = (in - 2.2f) / 8;                                                    /* rounded band */
                c = scale(base, .78f + .3f * sinf(band * 3.14159f));
            } else if (in < 2.2f + 8 + 2) c = rgb_hex(0x16141A);                                /* inner line */
            else if (in < 2.2f + 8 + 3.2f) c = rgb_hex(0x6A5E4E);                               /* warm lip */
            else {
                float t = clampf((v - y) / h, 0, 1);
                c = mix(rgb_hex(0x383A44), rgb_hex(0x24252C), t);
                c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(py * 7919 + pxx)) - .5f) * .012f);   /* fine grain */
            }
            ui_blend_px(pxx, py, c, cover * alpha);
        }
    }
    if (leaves) {
        /* Sprigs of leaves over two corners, as the title's vines. */
        struct { float cx, cy, a; } corner[2] = { { x + 6, y + 6, 0 }, { x + w - 6, y + h - 6, 3.14159f } };
        for (int k = 0; k < 2; k++) {
            float cx = corner[k].cx, cy = corner[k].cy, a = corner[k].a;
            theme_leaf(cx + cosf(a + .2f) * 16, cy + sinf(a + .2f) * 16, 20, a + .2f, TH_LEAF_DK, alpha);
            theme_leaf(cx + cosf(a + 1.35f) * 16, cy + sinf(a + 1.35f) * 16, 20, a + 1.35f, TH_LEAF_DK, alpha);
            theme_leaf(cx + cosf(a + .78f) * 20, cy + sinf(a + .78f) * 20, 26, a + .78f, TH_LEAF, alpha);
            theme_apple(cx + cosf(a + .78f) * 3, cy + sinf(a + .78f) * 3, 7, 1, alpha);
        }
    }
}

void theme_picture_frame(float x, float y, float w, float h, float alpha) {
    ui_panel(x - 3, y - 3, w + 6, h + 6, 4, rgb(0, 0, 0), rgb(0, 0, 0), 0, rgb_hex(0x141216), alpha);
    ui_panel(x - 2, y - 2, w + 4, h + 4, 3, rgb(0, 0, 0), rgb(0, 0, 0), 0, TH_GOLD, .9f * alpha);
    ui_panel(x - 1, y - 1, w + 2, h + 2, 2, rgb(0, 0, 0), rgb(0, 0, 0), 0, rgb_hex(0x8A5A1E), alpha);
}

void theme_shadow_into(uint32_t *px, float x, float y, float w, float h, float r) {
    for (int py = 0; py < ui_h; py++) {
        float v = (py + .5f - ui_oy) / ui_s;
        if (v < y - r || v > y + h + r) continue;
        for (int pxx = 0; pxx < ui_w; pxx++) {
            float u = (pxx + .5f - ui_ox) / ui_s;
            float d = ui_rrect_dist(u, v, x, y, w, h, 12);
            if (d > r) continue;
            float k = .55f * (1 - smooth(-r * .3f, r, d));
            Rgb c = scale(unpack(px[py * ui_w + pxx]), 1 - k);
            px[py * ui_w + pxx] = pack(c);
        }
    }
}

/* Golden hour: dimmed, warmed toward amber in the light and green-brown in the shadows,
 * with a soft vignette. */
void theme_grade_backdrop(uint32_t *px, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float u = (x - ui_ox) / ui_s, v = (y - ui_oy) / ui_s;
            float vig = 1 - .45f * smooth(.25f, 1.15f, sqrtf((u - 640) * (u - 640) / (640.f * 640.f) + (v - 360) * (v - 360) / (360.f * 360.f)));
            Rgb c = unpack(px[y * w + x]);
            float l = c.r * .3f + c.g * .59f + c.b * .11f;
            c = mix(c, rgb(l, l, l), .25f);
            c = scale(c, .55f * vig);
            c = mix(c, scale(rgb(.30f, .22f, .10f), vig), .28f);
            c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * w + x)) - .5f) * 2.5f / 255);
            px[y * w + x] = pack(c);
        }
}

/* ---- Small pictures ---------------------------------------------------------------------- */
void theme_leaf(float cx, float cy, float size, float a, Rgb c, float alpha) {
    float ca = cosf(a), sa = sinf(a), half = size / 2, wid = size * .26f;
    int r = (int)(half * ui_s) + 2, pcx = (int)ui_x(cx), pcy = (int)ui_y(cy);
    for (int py = pcy - r; py <= pcy + r; py++)
        for (int pxx = pcx - r; pxx <= pcx + r; pxx++) {
            float u = (pxx + .5f - ui_ox) / ui_s - cx, v = (py + .5f - ui_oy) / ui_s - cy;
            float along = u * ca + v * sa, across = -u * sa + v * ca;
            float t = along / half;                         /* -1 at the stem .. 1 at the tip */
            if (t < -1 || t > 1) continue;
            float edge = wid * sqrtf(1 - t * t) * (1 - .25f * t);
            float d = fabsf(across) - edge;
            if (d > 1 / ui_s) continue;
            float cover = clampf(.5f - d * ui_s, 0, 1);
            Rgb k = across < 0 ? scale(c, .82f) : mix(c, rgb(1, 1, .8f), .15f);   /* two halves */
            if (fabsf(across) < .5f && t < .8f) k = scale(c, .6f);                  /* the vein */
            if (fabsf(across) > edge - 1.1f) k = scale(c, .45f);                   /* outline */
            ui_blend_px(pxx, py, k, cover * alpha);
        }
}

void theme_apple(float cx, float cy, float r, float k, float alpha) {
    int pr = (int)(r * 1.5f * ui_s) + 2, pcx = (int)ui_x(cx), pcy = (int)ui_y(cy);
    Rgb on = TH_APPLE, off = rgb_hex(0x4A4440);
    for (int py = pcy - pr; py <= pcy + pr; py++)
        for (int pxx = pcx - pr; pxx <= pcx + pr; pxx++) {
            float u = (pxx + .5f - ui_ox) / ui_s - cx, v = (py + .5f - ui_oy) / ui_s - cy;
            /* Two overlapping discs make the apple's shoulders. */
            float d1 = sqrtf((u + r * .32f) * (u + r * .32f) + (v + r * .05f) * (v + r * .05f)) - r * .78f;
            float d2 = sqrtf((u - r * .32f) * (u - r * .32f) + (v + r * .05f) * (v + r * .05f)) - r * .78f;
            float d = fminf(d1, d2);
            if (v < -r * .62f && fabsf(u) < r * .18f) d = fmaxf(d, r * .1f);   /* the dip at the top */
            if (d > 1 / ui_s) continue;
            float cover = clampf(.5f - d * ui_s, 0, 1);
            Rgb body = mix(off, on, k);
            float shade = clampf(.75f + (-u - v) / r * .3f, .45f, 1.15f);
            Rgb c = scale(body, shade);
            float hx = u + r * .35f, hy = v + r * .3f;
            if (hx * hx + hy * hy < r * r * .05f) c = mix(c, rgb(1, 1, 1), .75f * k + .15f);   /* shine */
            if (d > -1.2f) c = scale(body, .35f);                                               /* outline */
            ui_blend_px(pxx, py, c, cover * alpha * (.55f + .45f * k));
        }
    /* The stem and its leaf. */
    ui_capsule(cx, cy - r * .62f, cx + r * .12f, cy - r * 1.05f, .9f, 0, rgb_hex(0x5A3A1E), alpha);
    theme_leaf(cx + r * .42f, cy - r * .92f, r * .8f, -.5f, mix(rgb_hex(0x5C6A50), TH_LEAF, k), alpha);
}

void theme_sword(float x, float cy, float alpha) {
    /* blade, guard, grip, pommel, from the tip leftwards */
    ui_tri(x, cy, x - 7, cy - 3.2f, x - 7, cy + 3.2f, rgb_hex(0xF2F4F8), alpha);
    ui_fill_rect(x - 24, cy - 3.2f, 17, 6.4f, rgb_hex(0xD2D8E2), alpha);
    ui_fill_rect(x - 24, cy - 3.2f, 17, 1.6f, rgb_hex(0xFFFFFF), alpha * .8f);
    ui_fill_rect(x - 24, cy + 1.6f, 17, 1.6f, rgb_hex(0x8E98A8), alpha);
    ui_capsule(x - 26, cy - 8, x - 26, cy + 8, 2.2f, 0, TH_GOLD, alpha);
    ui_capsule(x - 26, cy - 8, x - 26, cy + 8, 2.2f, .9f, rgb_hex(0x6A4410), alpha);
    ui_fill_rect(x - 35, cy - 2.2f, 8, 4.4f, rgb_hex(0x6A3A1C), alpha);
    ui_capsule(x - 37, cy, x - 37, cy, 3, 0, TH_GOLD, alpha);
    ui_capsule(x - 37, cy, x - 37, cy, 3, .9f, rgb_hex(0x6A4410), alpha);
}

float theme_heading(const char *s, float x, float y, float px, float tracking, float alpha) {
    float o = px / 22.f;
    ui_draw_text(s, HEAD_FACE, px, 700, tracking, x + o * 1.6f, y + o * 2.2f, rgb(0, 0, 0), rgb(0, 0, 0), .55f * alpha);
    for (int k = 0; k < 8; k++) {
        float a = k * 3.14159f / 4;
        ui_draw_text(s, HEAD_FACE, px, 700, tracking, x + cosf(a) * o, y + sinf(a) * o, rgb_hex(0x2A1606), rgb_hex(0x2A1606), alpha);
    }
    ui_draw_text(s, HEAD_FACE, px, 700, tracking, x, y, TH_GOLD_HI, rgb_hex(0xE89A2C), alpha);
    return ui_text_width(s, HEAD_FACE, px, 700, tracking);
}

void theme_rule(float x, float y, float w, float alpha) {
    ui_fill_rect(x + 8, y - 1.2f, w - 16, 2.4f, rgb_hex(0x2A1606), .8f * alpha);
    ui_fill_rect(x + 8, y - .6f, w - 16, 1.2f, TH_GOLD, alpha);
    theme_leaf(x + 4, y, 11, 3.14159f + .35f, TH_LEAF, alpha);
    theme_leaf(x + w - 4, y, 11, -.35f, TH_LEAF, alpha);
}

/* ---- The launcher's landscape -------------------------------------------------------- *
 * A storybook evening over Centy's fields: a sky from deep teal to warm gold, a low sun with
 * its glow, three rows of rolling hills (far ones hazy blue-green, near ones deep green) and
 * scattered round trees like the game's, all in layout units so it fills any window. */
static float hill(float u, float seed, float base, float amp) {
    return base - amp * (.55f * sinf(u * .0065f + seed) + .3f * sinf(u * .017f + seed * 2.3f) + .15f * sinf(u * .041f + seed * 5.1f));
}

void theme_landscape(uint32_t *px, int w, int h) {
    for (int y = 0; y < h; y++) {
        float v = (y + .5f - ui_oy) / ui_s;
        for (int x = 0; x < w; x++) {
            float u = (x + .5f - ui_ox) / ui_s;
            float t = clampf(v / 520, 0, 1);
            Rgb c = mix(rgb_hex(0x1E3A4A), rgb_hex(0x8C7A58), t);
            c = mix(c, rgb_hex(0xF2B862), smooth(.45f, 1, t) * .7f);
            float dx = u - 980, dy = v - 430, d = sqrtf(dx * dx + dy * dy);
            c = add(c, rgb_hex(0xFFD890), .55f * expf(-d / 160));                /* the sun's glow */
            if (d < 46) c = mix(c, rgb_hex(0xFFF0C0), smooth(46, 42, d));         /* the sun */
            /* hills, far to near */
            float h1 = hill(u, 1.3f, 470, 46), h2 = hill(u, 4.1f, 540, 52), h3 = hill(u, 7.7f, 625, 40);
            if (v > h1) c = mix(rgb_hex(0x5E8A72), rgb_hex(0x4A7660), clampf((v - h1) / 120, 0, 1));
            if (v > h2) c = mix(rgb_hex(0x3E6E3A), rgb_hex(0x2E5A2C), clampf((v - h2) / 120, 0, 1));
            if (v > h3) c = mix(rgb_hex(0x2A5424), rgb_hex(0x1A3818), clampf((v - h3) / 110, 0, 1));
            /* rim light on the near hills' tops, from the sun */
            if (v > h2 && v < h2 + 3) c = add(c, rgb_hex(0xF8C870), .35f);
            if (v > h3 && v < h3 + 3) c = add(c, rgb_hex(0xF8C870), .3f);
            c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * w + x)) - .5f) * 3.f / 255);
            px[y * w + x] = pack(c);
        }
    }
    /* Round trees on the middle hills, as the game draws them: a dark trunk, a ball of leaves. */
    for (int k = 0; k < 18; k++) {
        float u = hashf((uint32_t)k * 911u) * 1300 - 10, r = 14 + hashf((uint32_t)k * 37u) * 12;
        float base = hill(u, 4.1f, 540, 52) + 6 + hashf((uint32_t)k * 3u) * 20;
        int x0 = (int)ui_x(u - r - 2), x1 = (int)ui_x(u + r + 2), y0 = (int)ui_y(base - r * 2.6f), y1 = (int)ui_y(base + 2);
        for (int y = y0 < 0 ? 0 : y0; y < y1 && y < h; y++)
            for (int x = x0 < 0 ? 0 : x0; x < x1 && x < w; x++) {
                float uu = (x + .5f - ui_ox) / ui_s - u, vv = (y + .5f - ui_oy) / ui_s - (base - r * 1.5f);
                float d = sqrtf(uu * uu + vv * vv);
                Rgb c = unpack(px[y * w + x]);
                if (d < r) {
                    float lit = clampf(.6f + (uu + vv) / r * -.35f, .3f, 1);
                    c = scale(rgb_hex(0x3E7A30), lit);
                    if (d > r - 1.6f) c = rgb_hex(0x1C3414);
                } else if (fabsf(uu) < r * .16f && vv > 0 && vv < r * 1.5f) c = rgb_hex(0x3A2412);
                else continue;
                px[y * w + x] = pack(c);
            }
    }
}
