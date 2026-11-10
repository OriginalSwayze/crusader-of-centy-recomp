/* The Crusader of Centy look for the launcher and the pause menu (theme.c), drawn into the
 * ui canvas in its 1280x720 layout units.
 *
 * It follows the game's own screens: panels framed like its dialogue boxes (dark slate
 * inside an orange and gold checkered border), leaves at the corners as on its title, the
 * apples of its health bar for switches, a little sword for the cursor, warm gold
 * storybook headings, and the green and gold of its fields. */
#ifndef THEME_H
#define THEME_H

#include <stdbool.h>
#include <stdint.h>

#include "ui.h"

#define TH_CREAM   rgb_hex(0xFFF4D6)    /* text */
#define TH_LABEL   rgb_hex(0xE8DFC4)
#define TH_DIM     rgb_hex(0x9A9480)
#define TH_GOLD    rgb_hex(0xF8CC4A)
#define TH_GOLD_HI rgb_hex(0xFFE9A0)
#define TH_LEAF    rgb_hex(0x7CC64E)
#define TH_LEAF_DK rgb_hex(0x3E7A2C)
#define TH_APPLE   rgb_hex(0xE0402E)
#define TH_ORANGE  rgb_hex(0xEC7A2E)
#define TH_SLATE   rgb_hex(0x2E3038)

/* A dialogue-box panel over (x, y, w, h): slate inside, the checkered border, leaves on the
 * corners when `leaves`. */
void theme_frame(float x, float y, float w, float h, bool leaves, float alpha);
/* A smaller plain frame (a thin gold line with dark outline) around pictures. */
void theme_picture_frame(float x, float y, float w, float h, float alpha);
/* A drop shadow under a panel, into a pixel buffer at canvas size (for backdrops). */
void theme_shadow_into(uint32_t *px, float x, float y, float w, float h, float r);
/* The warm, dimmed, vignetted backdrop over a blurred picture (in place, canvas size). */
void theme_grade_backdrop(uint32_t *px, int w, int h);

/* The launcher's painted landscape at canvas size: sky, sun, hills and trees. */
void theme_landscape(uint32_t *px, int w, int h);

/* A leaf centred at (cx, cy), `size` long, pointing at angle `a` (radians). */
void theme_leaf(float cx, float cy, float size, float a, Rgb c, float alpha);
/* An apple of radius r: red and shining when on (k = 1), a dim outline when off (k = 0). */
void theme_apple(float cx, float cy, float r, float k, float alpha);
/* The cursor: a little sword pointing right, its tip at (x, cy). */
void theme_sword(float x, float cy, float alpha);
/* A heading in gold with a dark outline and shadow; returns its width. */
float theme_heading(const char *s, float x, float y, float px, float tracking, float alpha);
/* A gold rule from x to x + w with a leaf at each end (under the open tab, in cards). */
void theme_rule(float x, float y, float w, float alpha);

#endif
