/* The Beyond Oasis Recomp features that hook into that game's code and RAM (its object
 * lists, camera, map layers and sound driver) and have not been mapped in Crusader of
 * Centy yet: widescreen, smooth motion, the outfit colours,
 * the element particles and the soundtrack player. Each is "off" here: the game runs and
 * draws exactly as the cartridge does. */
#include <string.h>
#include "genesis.h"
#include "motion.h"
#include "outfit.h"
#include "fx.h"
#include "ost.h"

/* ---- Widescreen (genesis.h) ---------------------------------------------------- */
void widescreen_patches(int extra_width) { (void)extra_width; }
void widescreen_frame(void) {}

/* ---- Smooth motion (motion.h): no sprite is known to belong to an object. -------- */
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

/* ---- Outfit colours (outfit.h) --------------------------------------------------- */
void (*outfit_sound)(int kind);
void outfit_defaults(void) {}
bool outfit_ini_load(const char *key, const char *value) { (void)key; (void)value; return false; }
void outfit_ini_save(FILE *f) { (void)f; }
void outfit_apply(void) { vdp_opts.outfit = false; }
void outfit_preview_save(const char *dir) { (void)dir; }
bool outfit_preview_load(const char *dir) { (void)dir; return false; }
void outfit_open(void) {}
bool outfit_is_open(void) { return false; }
bool outfit_event(const SDL_Event *e, float mx, float my) { (void)e; (void)mx; (void)my; return false; }
void outfit_render(double time_s) { (void)time_s; }

/* ---- Element particles and footsteps (fx.h) -------------------------------------- */
void fx_game_frame(HdFrame *f, bool world, float cam_x, float cam_y) { (void)f; (void)world; (void)cam_x; (void)cam_y; }
void fx_clear(void) {}
void fx_draw(uint32_t *px, int w, int h, const HdPost *p) { (void)px; (void)w; (void)h; (void)p; }

/* ---- The sound driver on its own (ost.h) ----------------------------------------- */
void ost_start(void) {}
void ost_end(void) {}
bool ost_command(uint16_t cmd) { (void)cmd; return false; }
void ost_frame(void) {}
