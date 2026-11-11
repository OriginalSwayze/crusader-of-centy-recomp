/* The auto-map (automap.c): a picture of each area, filled in from what has been on screen.
 *
 * Every few game frames the scenery under the camera (the two planes, without sprites or
 * the HUD) is copied at a quarter of its size into the current area's picture. Areas are
 * told apart by the game's map number and tile sets ($FF16F6-$FF16FD) and the map's size
 * ($FF177A/$FF177C). Kept in automap.bin next to the game. */
#ifndef AUTOMAP_H
#define AUTOMAP_H

#include <stdbool.h>

#include "hd.h"

void automap_frame(const HdFrame *f, bool world, float cam_x, float cam_y);
void automap_load(const char *base_dir);
void automap_save(const char *base_dir);

/* The area shown: the current one when the menu opens (automap_view_reset); automap_view_step
 * goes through the others seen, in the order they were first visited. */
void automap_view_reset(void);
void automap_view_step(int dir);

/* Draws the shown area's map into the ui canvas, fitted into (x, y, w, h) layout units, with
 * its doors (each place the player left it for another area, a marker labelled with where
 * it leads) and, on the current area, the player's place blinking. False when nothing of
 * the area has been seen. */
bool automap_draw(float x, float y, float w, float h, float t, float alpha);

/* About the shown area: its map number, its place among the areas seen (1-based) and how many
 * those are, how much of it has been seen (0-100, -1 when its size is not known yet), its
 * doors found, and whether it is the one the player is in. */
typedef struct { int map, index, areas, percent, doors; bool current; } AutomapInfo;
void automap_info(AutomapInfo *out);

#endif
