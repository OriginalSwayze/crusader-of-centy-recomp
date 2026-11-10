/* Smooth motion's link from sprites to the game's objects (motion.h). The renderer can move
 * each sprite by its object's movement between game ticks (hd.c), but which sprite belongs
 * to which object has not been mapped in Crusader of Centy yet: until it is, no sprite has
 * an object and the pictures between ticks are the tick's own. */
#include <string.h>
#include "motion.h"

void motion_block(uint32_t pc) { (void)pc; }
void motion_reset(void) {}
unsigned motion_sprite_moves(const HdFrame *f, int dx[80], int dy[80], int *owned) {
    (void)f;
    memset(dx, 0, 80 * sizeof *dx);
    memset(dy, 0, 80 * sizeof *dy);
    if (owned) *owned = 0;
    return 0;
}
void motion_player_entries(const uint8_t *sat, int count, bool out[80]) { (void)sat; (void)count; memset(out, 0, 80 * sizeof *out); }
bool motion_entry_owner(int i, uint32_t *object, uint16_t *kind) { (void)i; (void)object; (void)kind; return false; }
