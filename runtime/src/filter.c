/* Display filters. Everything runs on the GPU through SDL's renderer (no shaders):
 *
 *  - an overlay texture the size of the picture, multiplied over it (SDL_BLENDMODE_MOD):
 *    scanlines lined up with the game's lines, aperture-grille stripes, the LCD grid
 *    and the vignette. It is rebuilt only when the size or the filter changes.
 *  - CRT glow: the picture drawn into a target 1/8 the size and added back on top,
 *    stretched with linear filtering, which blurs it.
 *  - CRT curvature: the picture and the overlay are drawn as a grid of triangles bent
 *    into a barrel shape (SDL_RenderGeometry).
 *  - Chromatic aberration: the picture drawn three times, added together, one colour each:
 *    red a little larger than the picture and blue a little smaller, both about the middle,
 *    so the fringes grow toward the edges as a lens's do.
 *  - Film grain: a tile of noise at the screen's resolution, moved every game frame, that
 *    brightens the picture by its value from one place and darkens it from another (a
 *    blend of dst * noise added, then taken away), so on average nothing changes. */
#include "filter.h"
#include "settings.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

static uint32_t *overlay_px;
static int ov_w, ov_h, ov_sw, ov_sh, ov_mode = -1;
static SDL_Texture *overlay_tex;
static SDL_Renderer *overlay_ren;
static SDL_Texture *glow_tex;
static int glow_w, glow_h;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

const uint32_t *filter_overlay(int w, int h, int src_w, int src_h, int mode) {
    if (overlay_px && w == ov_w && h == ov_h && src_w == ov_sw && src_h == ov_sh && mode == ov_mode) return overlay_px;
    free(overlay_px);
    overlay_px = malloc((size_t)w * h * 4);
    ov_w = w; ov_h = h; ov_sw = src_w; ov_sh = src_h; ov_mode = mode;
    if (overlay_tex) { SDL_DestroyTexture(overlay_tex); overlay_tex = NULL; }

    float line_px = (float)h / src_h, col_px = (float)w / src_w;
    /* Scanlines need a few screen pixels per line; fade them out when there are fewer. */
    float depth = mode == FILTER_CRT ? .62f : .5f;
    depth *= clampf((line_px - 1.5f) / 1.5f, 0, 1);
    for (int y = 0; y < h; y++) {
        float fy = (y + .5f) / line_px;
        fy -= floorf(fy);
        float d = fabsf(fy - .5f) * 2;                  /* 0 at the beam centre, 1 between lines */
        float scan = 1 - depth * powf(d, 2.2f);
        float ny = (y + .5f) / h * 2 - 1;
        for (int x = 0; x < w; x++) {
            float r = 1, g = 1, b = 1;
            if (mode == FILTER_SCANLINES || mode == FILTER_CRT) { r = g = b = scan; }
            if (mode == FILTER_CRT) {
                /* Aperture grille: red, green and blue phosphor stripes, three pixels wide. */
                static const float stripe[3][3] = { { 1, .72f, .72f }, { .72f, 1, .72f }, { .72f, .72f, 1 } };
                const float *s = stripe[x % 3];
                r *= s[0]; g *= s[1]; b *= s[2];
                float nx = (x + .5f) / w * 2 - 1;
                float v = 1 - .22f * powf(nx * nx + ny * ny, 2);   /* vignette */
                r *= v; g *= v; b *= v;
            } else if (mode == FILTER_LCD && col_px >= 2.5f && line_px >= 2.5f) {
                /* Pixel grid: a thin darker gap around every game pixel. */
                float fx = (x + .5f) / col_px;
                fx -= floorf(fx);
                float gap = .9f / col_px, gapy = .9f / line_px;
                bool edge = fx < gap || fy < gapy;
                float m = edge ? .78f : 1;
                r = g = b = m;
            }
            uint32_t R = (uint32_t)(clampf(r, 0, 1) * 255 + .5f), G = (uint32_t)(clampf(g, 0, 1) * 255 + .5f),
                     B = (uint32_t)(clampf(b, 0, 1) * 255 + .5f);
            overlay_px[(size_t)y * w + x] = R << 16 | G << 8 | B;
        }
    }
    return overlay_px;
}

/* Barrel-shaped grid: (u, v) in 0..1 to a point in dst. The middle of each edge
 * reaches dst's edge and the corners are pulled in, like a CRT's bulging glass. */
#define GRID_X 40
#define GRID_Y 30
static void warp(float u, float v, SDL_Rect dst, float k, float *x, float *y) {
    float nx = u * 2 - 1, ny = v * 2 - 1;
    float wx = nx * (1 - k * ny * ny), wy = ny * (1 - k * nx * nx);
    *x = dst.x + (wx + 1) * .5f * dst.w;
    *y = dst.y + (wy + 1) * .5f * dst.h;
}

/* Draws tex's sub-rectangle (in texture coordinates 0..1) bent over dst. */
static void draw_curved(SDL_Renderer *ren, SDL_Texture *tex, float u0, float v0, float u1, float v1, SDL_Rect dst,
                        float k, SDL_Color color) {
    static SDL_Vertex verts[(GRID_X + 1) * (GRID_Y + 1)];
    static int idx[GRID_X * GRID_Y * 6];
    for (int j = 0; j <= GRID_Y; j++)
        for (int i = 0; i <= GRID_X; i++) {
            float u = (float)i / GRID_X, v = (float)j / GRID_Y;
            SDL_Vertex *p = &verts[j * (GRID_X + 1) + i];
            warp(u, v, dst, k, &p->position.x, &p->position.y);
            p->tex_coord.x = u0 + (u1 - u0) * u;
            p->tex_coord.y = v0 + (v1 - v0) * v;
            p->color = color;
        }
    int n = 0;
    for (int j = 0; j < GRID_Y; j++)
        for (int i = 0; i < GRID_X; i++) {
            int a = j * (GRID_X + 1) + i, b = a + 1, c = a + GRID_X + 1, d = c + 1;
            idx[n++] = a; idx[n++] = b; idx[n++] = c;
            idx[n++] = b; idx[n++] = d; idx[n++] = c;
        }
    SDL_RenderGeometry(ren, tex, verts, (GRID_X + 1) * (GRID_Y + 1), idx, n);
}

/* Sharp bilinear: the picture is first enlarged by a whole number with hard pixel edges
 * to just above dst's size, then smoothed only over the small remaining step. A plain
 * smooth stretch from, say, 1272x672 to 1920x1080 blurs every edge. */
static SDL_Texture *sharp_tex;
static SDL_Renderer *sharp_ren;
static int sharp_w, sharp_h;

static SDL_Texture *sharpen(SDL_Renderer *ren, SDL_Texture *frame, SDL_Rect *s, SDL_Rect dst) {
    int k = (dst.w + s->w - 1) / s->w, ky = (dst.h + s->h - 1) / s->h;
    if (ky > k) k = ky;
    if (k < 2 || (dst.w % s->w == 0 && dst.h % s->h == 0 && dst.w / s->w == dst.h / s->h)) return NULL;
    int w = s->w * k, h = s->h * k;
    if (w > 8192 || h > 8192) return NULL;
    if (!sharp_tex || sharp_ren != ren || sharp_w != w || sharp_h != h) {
        if (sharp_tex) SDL_DestroyTexture(sharp_tex);
        sharp_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!sharp_tex) return NULL;
        SDL_SetTextureScaleMode(sharp_tex, SDL_ScaleModeLinear);
        sharp_ren = ren; sharp_w = w; sharp_h = h;
    }
    SDL_ScaleMode old_mode;
    SDL_GetTextureScaleMode(frame, &old_mode);
    SDL_Texture *old_target = SDL_GetRenderTarget(ren);
    SDL_SetRenderTarget(ren, sharp_tex);
    SDL_SetTextureScaleMode(frame, SDL_ScaleModeNearest);
    SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(ren, frame, s, NULL);
    SDL_SetTextureScaleMode(frame, old_mode);
    SDL_SetRenderTarget(ren, old_target);
    *s = (SDL_Rect){ 0, 0, w, h };
    return sharp_tex;
}

/* The picture into dst, plain or bent (k > 0), with the chromatic aberration if it is on. */
static void draw_picture(SDL_Renderer *ren, SDL_Texture *frame, const SDL_Rect *s, int tw, int th, SDL_Rect dst,
                         float k) {
    float u0 = (float)s->x / tw, v0 = (float)s->y / th, u1 = (float)(s->x + s->w) / tw, v1 = (float)(s->y + s->h) / th;
    SDL_Color white = { 255, 255, 255, 255 };
    float e = settings.aberration == 2 ? .0045f : settings.aberration ? .0018f : 0;
    e *= dst.h;                                   /* pixels at the edges: 2 subtle, 5 strong at 1080p */
    if (e < .75f) {
        SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_NONE);
        if (k > 0) draw_curved(ren, frame, u0, v0, u1, v1, dst, k, white);
        else SDL_RenderCopy(ren, frame, s, &dst);
        return;
    }
    SDL_Rect old_clip;
    bool clipped = SDL_RenderIsClipEnabled(ren);
    SDL_RenderGetClipRect(ren, &old_clip);
    SDL_RenderSetClipRect(ren, &dst);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &dst);
    SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_ADD);
    static const SDL_Color tint[3] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 } };
    for (int c = 0; c < 3; c++) {
        float grow = c == 0 ? e : c == 2 ? -e : 0, gx = grow * dst.w / dst.h;
        if (k > 0) {
            /* draw_curved takes whole pixels; the grid's warp keeps it about the middle. */
            SDL_Rect r = { dst.x - (int)gx, dst.y - (int)grow, dst.w + 2 * (int)gx, dst.h + 2 * (int)grow };
            draw_curved(ren, frame, u0, v0, u1, v1, r, k, tint[c]);
        } else {
            SDL_FRect r = { dst.x - gx, dst.y - grow, dst.w + 2 * gx, dst.h + 2 * grow };
            SDL_SetTextureColorMod(frame, tint[c].r, tint[c].g, tint[c].b);
            SDL_RenderCopyF(ren, frame, s, &r);
        }
    }
    SDL_SetTextureColorMod(frame, 255, 255, 255);
    SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(ren, clipped ? &old_clip : NULL);
}

/* ---- Film grain ---------------------------------------------------------------------- */

#define GRAIN_N 256
static SDL_Texture *grain_tex, *grain_mod_tex;
static SDL_Renderer *grain_ren;
static SDL_BlendMode grain_add, grain_sub;
static bool grain_custom;                        /* the renderer has the custom blends */
static int grain_mod_level;

static uint32_t grain_rand(uint32_t *x) { *x = *x * 1664525u + 1013904223u; return *x >> 8; }

/* Grey noise, close to a bell curve (three draws added), 0-255 round 128. With a level,
 * the plain darkening tile used when the renderer has no custom blends. */
static void grain_noise(uint32_t *px, int level) {
    uint32_t r = 0x9E3779B9u;
    for (int i = 0; i < GRAIN_N * GRAIN_N; i++) {
        int v = (int)((grain_rand(&r) & 255) + (grain_rand(&r) & 255) + (grain_rand(&r) & 255)) / 3;
        if (level) v = 255 - v * (level == 2 ? 70 : 36) / 255;
        px[i] = (uint32_t)v * 0x010101u;
    }
}

static SDL_Texture *grain_make(SDL_Renderer *ren, int level) {
    static uint32_t px[GRAIN_N * GRAIN_N];
    grain_noise(px, level);
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, GRAIN_N, GRAIN_N);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, px, GRAIN_N * 4);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
    return t;
}

/* The tile over dst from (ox, oy), each noise pixel cell x cell screen pixels. */
static void grain_tile(SDL_Renderer *ren, SDL_Texture *t, SDL_Rect dst, int cell, int ox, int oy) {
    int size = GRAIN_N * cell;
    for (int y = dst.y - oy; y < dst.y + dst.h; y += size)
        for (int x = dst.x - ox; x < dst.x + dst.w; x += size) {
            SDL_Rect r = { x, y, size, size };
            SDL_RenderCopy(ren, t, NULL, &r);
        }
}

static void draw_grain(SDL_Renderer *ren, SDL_Rect dst) {
    if (!settings.grain) return;
    if (grain_ren != ren) {
        if (grain_tex) SDL_DestroyTexture(grain_tex);
        if (grain_mod_tex) SDL_DestroyTexture(grain_mod_tex);
        grain_tex = grain_make(ren, 0); grain_mod_tex = NULL; grain_mod_level = 0;
        grain_ren = ren;
        grain_add = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                                               SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        /* dst - src: SDL2 calls it REV_SUBTRACT whatever its header says. */
        grain_sub = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_REV_SUBTRACT,
                                               SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        grain_custom = grain_tex && SDL_SetTextureBlendMode(grain_tex, grain_add) == 0 &&
                       SDL_SetTextureBlendMode(grain_tex, grain_sub) == 0;
    }
    /* A new place in the tile every game frame; the same between, on faster displays. */
    uint32_t seed = (SDL_GetTicks() * 60 / 1000) * 2654435761u + 12345u;
    int cell = dst.h / 900 > 1 ? dst.h / 900 : 1, size = GRAIN_N * cell;
    int ax = (int)(grain_rand(&seed) % size), ay = (int)(grain_rand(&seed) % size);
    int bx = (int)(grain_rand(&seed) % size), by = (int)(grain_rand(&seed) % size);
    SDL_Rect old_clip;
    bool clipped = SDL_RenderIsClipEnabled(ren);
    SDL_RenderGetClipRect(ren, &old_clip);
    SDL_RenderSetClipRect(ren, &dst);
    if (grain_custom) {
        Uint8 m = settings.grain == 2 ? 64 : 34;  /* noise x this: up to about 12% / 22% either way */
        SDL_SetTextureColorMod(grain_tex, m, m, m);
        SDL_SetTextureBlendMode(grain_tex, grain_add);
        grain_tile(ren, grain_tex, dst, cell, ax, ay);
        SDL_SetTextureBlendMode(grain_tex, grain_sub);
        grain_tile(ren, grain_tex, dst, cell, bx, by);
    } else {
        if (grain_mod_level != settings.grain) {
            if (grain_mod_tex) SDL_DestroyTexture(grain_mod_tex);
            grain_mod_tex = grain_make(ren, settings.grain);
            if (grain_mod_tex) SDL_SetTextureBlendMode(grain_mod_tex, SDL_BLENDMODE_MOD);
            grain_mod_level = settings.grain;
        }
        if (grain_mod_tex) grain_tile(ren, grain_mod_tex, dst, cell, ax, ay);
    }
    SDL_RenderSetClipRect(ren, clipped ? &old_clip : NULL);
}

void filter_draw(SDL_Renderer *ren, SDL_Texture *frame, const SDL_Rect *src, SDL_Rect dst,
                 int src_w, int src_h, int mode) {
    if (dst.w <= 0 || dst.h <= 0) return;
    int tw, th;
    SDL_QueryTexture(frame, NULL, NULL, &tw, &th);
    SDL_Rect s = src ? *src : (SDL_Rect){ 0, 0, tw, th };
    SDL_Texture *sharp = sharpen(ren, frame, &s, dst);
    if (sharp) { frame = sharp; tw = s.w; th = s.h; }
    if (mode <= FILTER_OFF || mode >= FILTER_COUNT) {
        draw_picture(ren, frame, &s, tw, th, dst, 0);
        draw_grain(ren, dst);
        return;
    }
    bool crt = mode == FILTER_CRT;
    float k = crt ? .045f : 0;
    SDL_Color white = { 255, 255, 255, 255 };

    /* The picture. */
    draw_picture(ren, frame, &s, tw, th, dst, k);

    /* The overlay, multiplied. */
    if (overlay_ren != ren || ov_w != dst.w || ov_h != dst.h || ov_sw != src_w || ov_sh != src_h || ov_mode != mode ||
        !overlay_tex) {
        const uint32_t *px = filter_overlay(dst.w, dst.h, src_w, src_h, mode);
        if (overlay_tex) SDL_DestroyTexture(overlay_tex);
        overlay_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, dst.w, dst.h);
        SDL_UpdateTexture(overlay_tex, NULL, px, dst.w * 4);
        SDL_SetTextureBlendMode(overlay_tex, SDL_BLENDMODE_MOD);
        overlay_ren = ren;
    }
    if (crt) draw_curved(ren, overlay_tex, 0, 0, 1, 1, dst, k, white);
    else SDL_RenderCopy(ren, overlay_tex, NULL, &dst);

    /* CRT glow: a blurred copy added on top, which also gives back the light the
     * scanlines and the mask take away. */
    if (crt) {
        int gw = dst.w / 8 > 1 ? dst.w / 8 : 1, gh = dst.h / 8 > 1 ? dst.h / 8 : 1;
        if (!glow_tex || glow_w != gw || glow_h != gh) {
            if (glow_tex) SDL_DestroyTexture(glow_tex);
            glow_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, gw, gh);
            SDL_SetTextureScaleMode(glow_tex, SDL_ScaleModeLinear);
            glow_w = gw; glow_h = gh;
        }
        if (glow_tex) {
            SDL_Texture *old = SDL_GetRenderTarget(ren);
            SDL_SetRenderTarget(ren, glow_tex);
            SDL_RenderCopy(ren, frame, &s, NULL);
            SDL_SetRenderTarget(ren, old);
            SDL_SetTextureBlendMode(glow_tex, SDL_BLENDMODE_ADD);
            SDL_Color glow = { 60, 60, 60, 255 };
            draw_curved(ren, glow_tex, 0, 0, 1, 1, dst, k, glow);
        }
    }
    draw_grain(ren, dst);
}
