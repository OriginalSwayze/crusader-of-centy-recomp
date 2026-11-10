/* Combat info, rumble and stats (combat.c), from the health of the game's objects.
 *
 * After each game frame the health of every object in the game's list is compared with the
 * frame before: a drop shows the damage rising from the object and its health bar, rumbles
 * the controller and counts in the stats, as do the player's own hits. */
#ifndef COMBAT_H
#define COMBAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hd.h"

/* After a game frame. `live` is false while rewinding: nothing is counted or felt then. */
void combat_frame(const HdFrame *f, bool world, float cam_x, float cam_y, bool live);
void combat_clear(void);                /* after a load, reset or scene change */
/* --autopilot (tests): the pad for this frame, walking to the nearest enemy and attacking. */
uint16_t combat_autopilot(int frame, uint16_t attack);

/* Whether combat_draw has anything to draw (numbers, bars or the timer). */
bool combat_active(const HdPost *p);
/* Draws the numbers, bars and timer onto a finished picture. */
void combat_draw(uint32_t *px, int w, int h, const HdPost *p);

/* Stats: kept in stats.ini next to the game, over all sessions. */
void combat_stats_load(const char *base_dir);
void combat_stats_save(const char *base_dir);
/* The stats as lines of "label\tvalue", for the pause menu's card. */
int  combat_stats_lines(char lines[][64], int max);

#endif
