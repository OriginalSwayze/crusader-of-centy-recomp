/* The journal (story.c): the game's story events, named from its own dialogue.
 *
 * The game keeps 512 story flags (live at $FFC500). Most are set by its dialogue: the text
 * code E0 hi lo sets flag hi:lo when the line is shown. So each such flag is named by the
 * line that sets it (who says it, and what), read from the cartridge when it loads; nothing
 * of the game's text is kept in this project. The order the player met them in is kept in
 * journal.bin next to the game. Named for the USA version only (the others' text is in
 * another encoding); there the journal is empty. */
#ifndef STORY_H
#define STORY_H

#include <stdbool.h>
#include <stdint.h>

void story_load(const char *base_dir);
void story_save(const char *base_dir);

/* Flag `flag` was newly set while playing. */
void story_gained(int flag);

/* One event: who says it (may be "") and the line. */
typedef struct { const char *who, *line; } StoryEvent;

/* The events done in the running game (its live flags), newest first: those met while
 * playing in the order they came, then earlier ones (from a save made before) in the
 * cartridge's order. `done` of `total` named events. Returns how many went into `out`. */
int story_events(StoryEvent *out, int max, int *done, int *total);

#endif
