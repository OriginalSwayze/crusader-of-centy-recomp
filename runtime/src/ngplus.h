/* New Game+ (ngplus.c): the story over again, keeping Corona's maximum health and money.
 *
 * ngplus_start() takes them from the game being played; after the reset (the player picks an
 * empty file and names it), the moment a new game has been set up ($019188: 8 health, no
 * money, no animals, no story flags) they are put back. Animals are not carried: the scenes
 * where they join check whether they already have, and skipping those scenes could skip story
 * steps. The number of runs is kept in newgameplus.ini next to the game. */
#ifndef NGPLUS_H
#define NGPLUS_H

#include <stdbool.h>

void ngplus_load(const char *base_dir);
void ngplus_save(const char *base_dir);

/* Takes what is carried from the running game; false (nothing done) when no game is in play. */
bool ngplus_start(void);

/* Once a frame: puts the carried values into a newly set-up game. */
void ngplus_frame(void);

/* New Game+ runs started so far (0 = none). */
int ngplus_runs(void);

#endif
