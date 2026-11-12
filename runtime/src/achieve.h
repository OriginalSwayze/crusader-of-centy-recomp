/* Achievements (achieve.c): goals met while playing, kept on this computer.
 *
 * Each is worked out from what the runtime already follows: the animals and story events
 * (story.c), the places visited (automap.c), enemies defeated (combat.c), Corona's health
 * and money, New Game+ (ngplus.c). They are checked once a second during play; one met is
 * kept, with its date, in achievements.ini next to the game, and shown for a few seconds
 * over the game. */
#ifndef ACHIEVE_H
#define ACHIEVE_H

#include <stdbool.h>
#include <time.h>

void achieve_load(const char *base_dir);
void achieve_save(const char *base_dir);

/* Once a frame while playing (live play only: tests and the title screen do not count). */
void achieve_frame(void);

typedef struct {
    const char *title, *about;
    time_t unlocked;                    /* 0: not yet */
} Achievement;

/* How many there are; entry i. */
int achieve_count(void);
const Achievement *achieve_get(int i);

/* The one to show over the game now: those met are shown in turn, `seconds` each; NULL when
 * none is waiting. *age gets how long it has been shown, in seconds. */
const Achievement *achieve_recent(double seconds, double *age);

#endif
