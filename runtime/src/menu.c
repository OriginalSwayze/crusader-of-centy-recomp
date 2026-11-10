/* The pause menu and the launcher (menu.h), in the game's own look (theme.c): panels framed
 * like its dialogue boxes, apples for switches, a sword for the cursor, over a warm,
 * blurred copy of the game's picture. A tab for each group of settings; the card on the
 * right follows the selected row: the save slot's thumbnail, a live before/after preview of
 * a picture option (preview.c), the map, the stats, the title screen in the launcher, or
 * what the option does. Springs, rows cascading in, values sliding, and the menu dissolving
 * in from the game and back out to it. The launcher is the same menu with a START tab (play,
 * language, ROM) in place of GAME. */
#include "menu.h"
#include "controls.h"
#include "preview.h"
#include "combat.h"
#include "automap.h"
#include "theme.h"
#include "game.h"
#include "genesis.h"
#include "savestate.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ITEM_ACTION, ITEM_SLOT, ITEM_OPTION } ItemKind;
enum { ACT_RESUME, ACT_SAVE, ACT_LOAD, ACT_RESET, ACT_QUIT, ACT_CONTROLS, ACT_PRESET_ORIGINAL, ACT_PRESET_ENHANCED, ACT_STATS, ACT_MAP,
       ACT_PLAY, ACT_ROM, ACT_EXIT };
typedef struct { ItemKind kind; const char *label; int id; } Item;

#define TABS 6
#define MAX_ROWS 10
static const char *const tab_names[TABS] = { "GAME", "DISPLAY", "EFFECTS", "PLAY", "MORE", "SPEED" };
static const Item tab_items[TABS][MAX_ROWS] = {
    { { ITEM_ACTION, "Resume", ACT_RESUME }, { ITEM_SLOT, "Save state", ACT_SAVE }, { ITEM_SLOT, "Load state", ACT_LOAD },
      { ITEM_ACTION, "Map", ACT_MAP }, { ITEM_ACTION, "Stats", ACT_STATS },
      { ITEM_OPTION, "Rewind", OPT_REWIND }, { ITEM_OPTION, "Fast-forward", OPT_FAST },
      { ITEM_ACTION, "Reset game", ACT_RESET }, { ITEM_ACTION, "Quit game", ACT_QUIT } },
    { { ITEM_OPTION, "Integer scaling", OPT_INTEGER }, { ITEM_OPTION, "Internal resolution", OPT_INTERNAL },
      { ITEM_OPTION, "Scaler", OPT_SCALER }, { ITEM_OPTION, "Filter", OPT_FILTER },
      { ITEM_OPTION, "Sharpening", OPT_SHARPEN }, { ITEM_OPTION, "Sprite flicker fix", OPT_FLICKER },
      { ITEM_OPTION, "Slowdown fix", OPT_OVERCLOCK } },
    { { ITEM_OPTION, "Lighting", OPT_LIGHTING }, { ITEM_OPTION, "HDR look", OPT_HDR }, { ITEM_OPTION, "Water", OPT_WATER },
      { ITEM_OPTION, "Weather", OPT_WEATHER } },
    { { ITEM_OPTION, "Difficulty", OPT_DIFFICULTY }, { ITEM_OPTION, "Infinite health", OPT_CHEAT_HEALTH },
      { ITEM_OPTION, "Combat info", OPT_COMBAT }, { ITEM_OPTION, "Rumble", OPT_RUMBLE }, { ITEM_OPTION, "Play timer", OPT_TIMER } },
    { { ITEM_OPTION, "Language", OPT_LANGUAGE }, { ITEM_OPTION, "Volume", OPT_VOLUME }, { ITEM_OPTION, "Audio", OPT_AUDIO },
      { ITEM_ACTION, "Controls", ACT_CONTROLS },
      { ITEM_OPTION, "Texture pack", OPT_TEXPACK }, { ITEM_OPTION, "Dump tiles", OPT_DUMP },
      { ITEM_ACTION, "Original experience", ACT_PRESET_ORIGINAL }, { ITEM_ACTION, "Enhanced experience", ACT_PRESET_ENHANCED } },
    { { ITEM_OPTION, "Performance stats", OPT_PERF_STATS }, { ITEM_OPTION, "Auto quality", OPT_AUTO_QUALITY },
      { ITEM_OPTION, "GPU effects", OPT_GPU }, { ITEM_OPTION, "Merged finishing pass", OPT_MERGED },
      { ITEM_OPTION, "AVX2 pixel loops", OPT_AVX2 }, { ITEM_OPTION, "Slowdown fix", OPT_OVERCLOCK } },
};
static const int tab_count[TABS] = { 9, 7, 4, 5, 8, 6 };

/* The launcher's first tab, in place of GAME. */
static const Item start_items[] = {
    { ITEM_ACTION, "Play", ACT_PLAY }, { ITEM_OPTION, "Language", OPT_LANGUAGE },
    { ITEM_ACTION, "Choose ROM", ACT_ROM }, { ITEM_ACTION, "Controls", ACT_CONTROLS },
    { ITEM_OPTION, "Show this at startup", OPT_LAUNCHER }, { ITEM_ACTION, "Quit", ACT_EXIT },
};
static bool launcher;                   /* opened as the launcher */
static const Item *ITEMS(int t) { return launcher && t == 0 ? start_items : tab_items[t]; }
static int COUNT(int t) { return launcher && t == 0 ? (int)(sizeof start_items / sizeof *start_items) : tab_count[t]; }
static const char *TAB_NAME(int t) { return launcher && t == 0 ? "START" : tab_names[t]; }

/* What each option and action does, for the card on the right. */
static const char *about(const Item *it) {
    if (it->kind == ITEM_ACTION)
        switch (it->id) {
        case ACT_RESUME: return "Back to the game.";
        case ACT_RESET: return "Starts the game over from the beginning. Unsaved progress is lost.";
        case ACT_QUIT: return "Closes the game. Unsaved progress is lost; the game's own saves stay.";
        case ACT_CONTROLS: return "The keyboard key and controller button for every action.";
        case ACT_PLAY: return NULL;
        case ACT_ROM: return "Pick a ROM of Crusader of Centy (USA), or Ragnacenty from Japan or Korea. Each country's cartridge is a language you can switch between; ROMs in the rom folder next to the game are found by themselves.";
        case ACT_EXIT: return "Closes the game.";
        case ACT_STATS: return NULL;
        case ACT_MAP: return "Fills in as you explore: everything that has been on screen on this map, with your place blinking. Each map keeps its own, saved between sessions.";
        case ACT_PRESET_ORIGINAL: return "Puts every picture, speed and gameplay option back to the cartridge as it was: native resolution, no effects or weather, the original sprite limit, speed and difficulty. Controls, sound, saves and the language stay as they are.";
        case ACT_PRESET_ENHANCED: return "The recommended enhancements in one go: the art redrawn at 3x with rounded edges and light sharpening, a soft glow, the HDR look, living water, no sprite flicker and no slowdown. Difficulty and cheats stay as you set them.";
        }
    if (it->kind == ITEM_SLOT) return NULL;
    switch (it->id) {
    case OPT_INTEGER: return "Scales by whole numbers only, so every game pixel is the same size (black borders around the picture).";
    case OPT_INTERNAL: return "Draws each layer at a higher resolution, rounding the edges of the pixel art. Native is the original look; 6x (2544 x 1344) is for 1440p and 4K screens and needs a fast CPU.";
    case OPT_SCALER: return "How the layers are scaled for the internal resolution: Edge rounds corners, Smooth (xBR) follows curves and slopes, Sharp keeps every pixel square, exactly as drawn. Edge and Smooth leave dither patterns crisp.";
    case OPT_HDR: return "A high-dynamic-range grade for any screen: deeper shadows, brighter highlights and richer colour, so bright colours stand out. Strong goes further.";
    case OPT_FILTER: return "Scanlines, a curved CRT tube or an LCD grid laid over the picture, like the screens of the time.";
    case OPT_SHARPEN: return "Crisper edges after scaling, without halos: soft edges are sharpened, hard pixel edges are left alone.";
    case OPT_FLICKER: return "Lifts the hardware's limit of sprites on one line, so crowded scenes no longer flicker.";
    case OPT_OVERCLOCK: return "Gives the console's processor more time each frame, so busy scenes no longer slow down. Music and speed stay the same.";
    case OPT_LIGHTING: return "Fire, magic, gems and other bright spots glow softly into their surroundings.";
    case OPT_TEXPACK: return "High-resolution replacements for the game's tiles, from a folder in texture_packs next to the game. None is the original art.";
    case OPT_DUMP: return "Writes every tile shown on screen once to texture_packs/dump, named by its key: the starting point for making a pack.";
    case OPT_REWIND: return "Hold the rewind key to step back in time.";
    case OPT_LAUNCHER: return "This screen before the game starts. Off starts the game straight away; show_launcher=1 in settings.ini brings it back.";
    case OPT_WEATHER: return "Weather and ambience over the world, never over menus or story screens: leaves drifting on the wind, rain under a grey sky, snow, or fireflies at dusk. Only the picture changes; the game plays as before.";
    case OPT_WATER: return "Streams, lakes and the sea roll with slow swells, foam where they meet the shore and anyone standing in them, glint along the crests and reflect the people and things above them.";
    case OPT_DIFFICULTY: return "How much each hit takes from your apples: Original as the cartridge has it, Easy half, Hard one and a half times, Very hard twice. Fractions carry over to the next hit, so Easy really halves the damage over a fight.";
    case OPT_CHEAT_HEALTH: return "Hits take nothing, and your apples stay full whatever happens.";
    case OPT_COMBAT: return "The damage of every hit rises from the enemy as a number, gold for a defeat in one blow; with health bars, a bar over an enemy shows what it has left for a few seconds after each hit.";
    case OPT_RUMBLE: return "The controller rumbles when you land a hit, harder when an enemy is defeated and when you are hit.";
    case OPT_TIMER: return "This session's play time in the top corner of the picture, for speedruns and keeping track. The total is under Stats.";
    case OPT_FAST: return "How fast the game runs while the fast-forward key is held (Tab, or RB on a controller; change it in Controls).";
    case OPT_LANGUAGE: return settings_language_count() > 1
        ? "The version of the game played: each country's cartridge has its own language. Changing it starts that version from its title screen, with its own saves; unsaved progress here is lost."
        : "The version of the game played: each country's cartridge has its own language. Put another country's ROM in the rom folder next to the game to choose it here.";
    case OPT_PERF_STATS: return "Shows in the top-left corner how many pictures a second reach the screen, the internal resolution, and how long each picture takes: putting the layers together, the effects (on the CPU or the graphics card) and sending it to the graphics card.";
    case OPT_AUTO_QUALITY: return "When pictures cannot keep up with your display, the internal resolution steps down (6x, 4x, 3x, 2x) until they do, and back up when there is room again. It never goes above the Internal resolution you picked.";
    case OPT_GPU: return "Lighting, the HDR look and sharpening on the graphics card instead of the CPU, in one pass. It uses Direct3D 11 (or OpenGL where that is missing), picked as the window opens, so turning it on or off takes effect the next time the game starts; without either the CPU does them as before.";
    case OPT_MERGED: return "On the CPU, lighting, the HDR look and sharpening in one pass over each band of the picture instead of a pass each over the whole picture. It saves a trip through memory but works a little more per band: try it on a PC with slow memory. ";
    case OPT_AVX2: return "The CPU's effects loops work on eight pixels at a time with AVX2 instructions, on processors that have them (almost every PC since 2013). The result is the same.";
    case OPT_VOLUME: return "The game's volume.";
    case OPT_AUDIO: return "Original is the console's sound as emulated. Clean removes aliasing, the hum and clipping; Rich adds wider stereo and a light room.";
    }
    return NULL;
}

/* ---- Layout, in 1280x720 units ---------------------------------------------------------- */
#define TAB_X0  56
#define TAB_Y0  22
#define TAB_W0  596
#define TAB_H0  676
#define LIST_X  (TAB_X0 + 44)
#define LIST_R  (TAB_X0 + TAB_W0 - 44)
#define TABS_Y  (TAB_Y0 + 92)
#define TABS_H  66
#define TABS_X  (TAB_X0 + 26)
#define TABW    ((TAB_W0 - 52) / TABS)
#define LIST_Y  (TABS_Y + TABS_H + 22)
#define ROW_H   40
#define VOLUME_W 140                    /* the volume slider's track, left of its value */
#define VOLUME_X (LIST_R - 20 - 46 - 26 - VOLUME_W)
#define CARD_X  690
#define CARD_Y  96
#define CARD_W  548
#define CARD_H  520

void (*menu_sound)(int kind);
static void sound(int kind) { if (menu_sound) menu_sound(kind); }

static bool open_;
static int tab, tab_from, selected;     /* selected: a row of the tab, or -1 for the tab bar */
static uint32_t *backdrop;              /* blurred game frame and the tablet, at canvas size */
static uint32_t *frame;                 /* the paused frame, for save thumbnails */
static int frame_w, frame_h;
static SlotInfo slot_info;
static int slot_info_for = -1;
static float mouse_scale = 1;
static char toast[96];
static double toast_until, now, last_now, opened_at, tab_at, changed_at;
static int changed_row = -1;
static float hl_y, hl_v;               /* the highlight, on a spring to the selected row */
static float ind_x, ind_v;              /* the tabs' underline, on a spring to the open tab */
static float tab_on[TABS];              /* each tab's brightness, easing to the open one */
static float knob[MAX_ROWS];            /* each switch's knob, 0 off .. 1 on */
static int changed_dir = 1;             /* which way the last value changed (it slides in from there) */
static bool snap;                       /* the switches go straight to their side (a tab just opened) */
static double selected_at;              /* when the selection last moved (the card rises in) */
static double closing_at = -1;          /* when closing began (it dissolves back to the game), or -1 */
static uint32_t *sharp;                 /* the game frame where it shows, unblurred: the menu dissolves from it */
static bool open_pending;               /* the opening starts at the next frame drawn (now is old until then) */
static bool confirm_pending;            /* reset/quit asked once, waiting for a second press */
static int confirm_item;
static float mouse_x, mouse_y, mouse_t = -1;   /* for the light on the steel (layout units) */

#define CLOSE_AFTER .2                   /* seconds the closing takes (CLOSE_S) */

/* Open until the closing has dissolved back to the game. */
bool menu_is_open(void) {
    if (open_ && closing_at >= 0 && now - closing_at >= CLOSE_AFTER) {
        open_ = false;
        closing_at = -1;
        savestate_info_free(&slot_info);
        slot_info_for = -1;
    }
    return open_;
}

static void tab_state(void) {
    ind_x = TABS_X + tab * TABW + TABW / 2.f;
    for (int k = 0; k < TABS; k++) tab_on[k] = k == tab;
    snap = true;
}

static float row_y(int i) { return LIST_Y + i * ROW_H; }
static const Item *item_at(int i) { return i >= 0 && i < COUNT(tab) ? &ITEMS(tab)[i] : NULL; }

/* Box blur in place, three passes per axis (close to a Gaussian). */
static void blur_rgb(uint32_t *px, int w, int h, int r) {
    if (r < 1) return;
    uint32_t *tmp = malloc((size_t)w * h * 4);
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int x = -r; x <= r; x++) {
                if (x < 0 || x >= w) continue;
                uint32_t p = px[y * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++;
            }
            for (int x = 0; x < w; x++) {
                tmp[y * w + x] = (uint32_t)(sr / n) << 16 | (uint32_t)(sg / n) << 8 | (uint32_t)(sb / n);
                int out = x - r, in = x + r + 1;
                if (out >= 0) { uint32_t p = px[y * w + out]; sr -= (p >> 16) & 255; sg -= (p >> 8) & 255; sb -= p & 255; n--; }
                if (in < w) { uint32_t p = px[y * w + in]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++; }
            }
        }
        for (int x = 0; x < w; x++) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int y = -r; y <= r; y++) {
                if (y < 0 || y >= h) continue;
                uint32_t p = tmp[y * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++;
            }
            for (int y = 0; y < h; y++) {
                px[y * w + x] = (uint32_t)(sr / n) << 16 | (uint32_t)(sg / n) << 8 | (uint32_t)(sb / n);
                int out = y - r, in = y + r + 1;
                if (out >= 0) { uint32_t p = tmp[out * w + x]; sr -= (p >> 16) & 255; sg -= (p >> 8) & 255; sb -= p & 255; n--; }
                if (in < h) { uint32_t p = tmp[in * w + x]; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++; }
            }
        }
    }
    free(tmp);
}

static void open_common(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
                        int out_w, int out_h, float window_to_pixels) {
    bool reopen = open_;                /* the window changed size while open, or it was closing: keep the place */
    open_ = true;
    closing_at = -1;
    if (!reopen) {
        tab = tab_from = 0;
        selected = 0;
        confirm_pending = false;
        open_pending = true;
        tab_at = -10;
        hl_y = row_y(0);
        hl_v = 0;
        tab_state();
        sound(1);
    }
    slot_info_for = -1;
    mouse_scale = window_to_pixels;
    ui_begin(out_w, out_h);

    free(frame);
    frame_w = fb_w; frame_h = fb_h;
    frame = malloc((size_t)fb_w * fb_h * 4);
    for (int y = 0; y < fb_h; y++) memcpy(frame + (size_t)y * fb_w, fb + (size_t)y * fb_pitch, (size_t)fb_w * 4);

    /* Backdrop: the frame where the game shows it, blurred and warmed to golden hour, with
     * the shadows of the panel and the card. */
    free(backdrop);
    backdrop = calloc((size_t)out_w * out_h, 4);
    for (int y = 0; y < game_h; y++)
        for (int x = 0; x < game_w; x++) {
            int X = game_x + x, Y = game_y + y;
            if (X < 0 || Y < 0 || X >= out_w || Y >= out_h) continue;
            backdrop[Y * out_w + X] = frame[(y * fb_h / game_h) * fb_w + x * fb_w / game_w];
        }
    free(sharp);
    sharp = malloc((size_t)out_w * out_h * 4);
    if (sharp) memcpy(sharp, backdrop, (size_t)out_w * out_h * 4);
    if (launcher) theme_landscape(backdrop, out_w, out_h);   /* the launcher's own scene */
    else {
        blur_rgb(backdrop, out_w, out_h, (int)(9 * ui_s));
        theme_grade_backdrop(backdrop, out_w, out_h);
    }
    theme_shadow_into(backdrop, TAB_X0 + 8, TAB_Y0 + 12, TAB_W0, TAB_H0, 22);
    theme_shadow_into(backdrop, CARD_X + 8, CARD_Y + 12, CARD_W, CARD_H, 22);
}

void menu_open(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
               int out_w, int out_h, float window_to_pixels) {
    if (launcher && open_) open_ = false;   /* from the launcher to the game: a new menu */
    launcher = false;
    open_common(fb, fb_w, fb_h, fb_pitch, game_x, game_y, game_w, game_h, out_w, out_h, window_to_pixels);
}

void menu_open_launcher(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int out_w, int out_h, float window_to_pixels) {
    if (!launcher) open_ = false;
    launcher = true;
    /* The title screen fills the window behind it, at the game's shape. */
    int gh = out_h, gw = (int)(out_h * (4.0 / 3.0));
    if (gw < out_w) { gw = out_w; gh = (int)(out_w * .75); }
    open_common(fb, fb_w, fb_h, fb_pitch, (out_w - gw) / 2, (out_h - gh) / 2, gw, gh, out_w, out_h, window_to_pixels);
}

bool menu_is_launcher(void) { return open_ && launcher; }

void menu_close_now(void) {
    open_ = false;
    closing_at = -1;
    launcher = false;
    savestate_info_free(&slot_info);
    slot_info_for = -1;
}

void menu_select(int k, int row) {
    tab = tab_from = (k % TABS + TABS) % TABS;
    selected = row < -1 ? -1 : row >= COUNT(tab) ? COUNT(tab) - 1 : row;
    hl_y = row_y(selected < 0 ? 0 : selected);
    opened_at = tab_at = selected_at = -10;
    open_pending = false;
    tab_state();
}

/* Starts the closing; menu_is_open() stays true until it has dissolved back to the game. */
void menu_close(void) {
    if (!open_ || closing_at >= 0) return;
    sound(2);
    closing_at = now;
}

static void notify(const char *msg) {
    snprintf(toast, sizeof toast, "%s", msg);
    toast_until = now + 2.2;
}

enum { HIT_TAB0 = 100 };

static int hit_test(float mx, float my) {
    for (int k = 0; k < TABS; k++)
        if (mx >= TABS_X + k * TABW && mx < TABS_X + (k + 1) * TABW && my >= TABS_Y && my < TABS_Y + TABS_H) return HIT_TAB0 + k;
    for (int i = 0; i < COUNT(tab); i++) {
        float y = row_y(i);
        if (mx >= LIST_X - 14 && mx <= LIST_R + 8 && my >= y && my < y + ROW_H) return i;
    }
    return -2;
}

static void select_row(int i) {
    if (i == selected) return;
    selected = i;
    selected_at = now;
    confirm_pending = false;
    sound(0);
}

static void move(int dir) {
    int n = COUNT(tab) + 1;         /* the tab bar, then the rows */
    select_row(settings_cycle(selected + 1, n, dir) - 1);
}

static void open_tab(int k) {
    k = (k % TABS + TABS) % TABS;
    if (k == tab) return;
    tab_from = tab;
    tab = k;
    tab_at = now;
    selected_at = now;
    snap = true;
    if (selected >= COUNT(tab)) selected = COUNT(tab) - 1;
    confirm_pending = false;
    sound(0);
}

/* Runs the selected item (activate) or steps its value (dir = -1 / +1). */
static MenuAction act(int dir, bool activate) {
    if (selected < 0) {                 /* on the tab bar: left and right switch tabs */
        if (dir) open_tab(tab + dir);
        return MENU_NONE;
    }
    const Item *it = item_at(selected);
    if (!it) return MENU_NONE;
    char msg[96];
    switch (it->kind) {
    case ITEM_OPTION:
        if (it->id == OPT_VOLUME && !dir) return MENU_NONE;
        settings_option_change(it->id, dir ? dir : 1);
        changed_row = selected;
        changed_dir = dir ? dir : 1;
        changed_at = now;
        sound(1);
        return MENU_SETTINGS;
    case ITEM_SLOT:
        if (dir) {
            settings.state_slot = settings_cycle(settings.state_slot - 1, 9, dir) + 1;
            changed_row = selected;
            changed_dir = dir;
            changed_at = now;
            sound(0);
            return MENU_SETTINGS;
        }
        if (!activate) return MENU_NONE;
        if (it->id == ACT_SAVE) {
            bool ok = savestate_save(settings.state_slot, frame, frame_w, frame_h, frame_w);
            snprintf(msg, sizeof msg, ok ? "Saved to slot %d" : "Could not save slot %d", settings.state_slot);
            notify(msg);
            sound(ok ? 1 : 3);
            slot_info_for = -1;
            return MENU_NONE;
        }
        if (savestate_load(settings.state_slot)) { sound(1); return MENU_LOADED; }
        snprintf(msg, sizeof msg, "Slot %d is empty or from another version", settings.state_slot);
        notify(msg);
        sound(3);
        return MENU_NONE;
    case ITEM_ACTION:
        if (!activate) return MENU_NONE;
        if (it->id == ACT_RESUME) return MENU_CLOSE;
        if (it->id == ACT_PLAY) { sound(1); return MENU_PLAY; }
        if (it->id == ACT_ROM) { sound(1); return MENU_ROM; }
        if (it->id == ACT_EXIT) return MENU_QUIT;
        if (it->id == ACT_CONTROLS) { sound(1); controls_open(); return MENU_NONE; }
        if (it->id == ACT_STATS || it->id == ACT_MAP) return MENU_NONE;     /* shown on the card */
        if (it->id == ACT_PRESET_ORIGINAL || it->id == ACT_PRESET_ENHANCED) {
            if (it->id == ACT_PRESET_ORIGINAL) settings_preset_original();
            else settings_preset_enhanced();
            notify(it->id == ACT_PRESET_ORIGINAL ? "Original experience set" : "Enhanced experience set");
            sound(1);
            return MENU_SETTINGS;
        }
        /* Reset and quit lose progress: ask for a second press. */
        if (!confirm_pending || confirm_item != selected) {
            confirm_pending = true;
            confirm_item = selected;
            notify(it->id == ACT_RESET ? "Press again to reset the game" : "Press again to quit");
            sound(3);
            return MENU_NONE;
        }
        confirm_pending = false;
        return it->id == ACT_RESET ? MENU_RESET : MENU_QUIT;
    }
    return MENU_NONE;
}

MenuAction menu_event(const SDL_Event *e) {
    int dir = 0;
    bool activate = false;
    if (closing_at >= 0) return MENU_NONE;  /* dissolving back to the game */
    if (controls_is_open()) {   /* a page takes all input while it is up */
        float mx = 0, my = 0;
        if (e->type == SDL_MOUSEMOTION) { mx = e->motion.x; my = e->motion.y; }
        if (e->type == SDL_MOUSEBUTTONDOWN) { mx = e->button.x; my = e->button.y; }
        mx = (mx * mouse_scale - ui_ox) / ui_s;
        my = (my * mouse_scale - ui_oy) / ui_s;
        bool changed = controls_event(e, mx, my);
        if (!controls_is_open()) sound(2);
        return changed ? MENU_SETTINGS : MENU_NONE;
    }
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE: return launcher ? MENU_NONE : MENU_CLOSE;
        case SDLK_UP: move(-1); break;
        case SDLK_DOWN: case SDLK_TAB: move(1); break;
        case SDLK_LEFT: dir = -1; break;
        case SDLK_RIGHT: dir = 1; break;
        case SDLK_q: case SDLK_PAGEUP: open_tab(tab - 1); break;
        case SDLK_e: case SDLK_PAGEDOWN: open_tab(tab + 1); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: case SDLK_z: activate = true; break;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_START: return launcher ? MENU_PLAY : MENU_CLOSE;
        case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: return launcher ? MENU_NONE : MENU_CLOSE;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: move(-1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: move(1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: dir = -1; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: dir = 1; break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: open_tab(tab - 1); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: open_tab(tab + 1); break;
        case SDL_CONTROLLER_BUTTON_A: activate = true; break;
        }
        break;
    case SDL_MOUSEMOTION: {
        float mx = (e->motion.x * mouse_scale - ui_ox) / ui_s, my = (e->motion.y * mouse_scale - ui_oy) / ui_s;
        mouse_x = mx; mouse_y = my; mouse_t = (float)now;
        int h = hit_test(mx, my);
        if (h >= 0 && h < HIT_TAB0) select_row(h);
        return MENU_NONE;
    }
    case SDL_MOUSEBUTTONDOWN: {
        float mx = (e->button.x * mouse_scale - ui_ox) / ui_s, my = (e->button.y * mouse_scale - ui_oy) / ui_s;
        int h = hit_test(mx, my);
        if (h >= HIT_TAB0) { open_tab(h - HIT_TAB0); selected = -1; selected_at = now; return MENU_NONE; }
        if (h < 0) return MENU_NONE;
        selected = h;
        const Item *it = item_at(h);
        if (e->button.button == SDL_BUTTON_LEFT && it->kind == ITEM_OPTION && it->id == OPT_VOLUME &&
            mx >= VOLUME_X - 8 && mx <= VOLUME_X + VOLUME_W + 8) {
            settings_set_volume_fraction(ui_slider_value(VOLUME_X, VOLUME_W, mx));   /* click on the track */
            sound(0);
            return MENU_SETTINGS;
        }
        if (e->button.button == SDL_BUTTON_RIGHT) dir = -1;
        else if (it->kind == ITEM_SLOT && mx > LIST_R - 150) dir = 1;   /* click the slot number to step it */
        else if (it->kind == ITEM_OPTION) dir = 1;
        else activate = true;
        break;
    }
    default:
        return MENU_NONE;
    }
    if (!dir && !activate) return MENU_NONE;
    return act(dir, activate);
}

/* ---- Drawing ----------------------------------------------------------------------------- *
 * The game's look (theme.c): dialogue-box panels, a storybook heading, line icons on the
 * tabs in cream with the open one in gold over a leafy rule on a spring, the sword cursor
 * on a spring, apples for switches, values sliding in from the side they changed to, rows
 * cascading in on a new tab, and the menu dissolving in from the game and back out to it. */
static float ease_out(float x) { x = clampf(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); }

#define C_LABEL TH_LABEL
#define C_GOLD  TH_GOLD
#define C_DIM   TH_DIM
#define OPEN_S  .35f                    /* the opening's length, seconds */
#define CLOSE_S .2f                     /* and the closing's */

static const int tab_icons[TABS] = { UI_ICON_GAME, UI_ICON_DISPLAY, UI_ICON_EFFECTS, UI_ICON_CHEATS, UI_ICON_MORE, UI_ICON_SPEED };

/* A picture of w x h pixels drawn into (x, y, pw, ph) layout units, fitted by height
 * (narrower ones get dark bars), with the gold picture frame. */
static void draw_picture(const uint32_t *img, int w, int h, float x, float y, float pw, float ph, float alpha) {
    int x0 = (int)ui_x(x), y0 = (int)ui_y(y), x1 = (int)ui_x(x + pw), y1 = (int)ui_y(y + ph);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            float u = (xx + .5f - ui_ox) / ui_s, v = (yy + .5f - ui_oy) / ui_s;
            Rgb c;
            if (img && w && h) {
                float tw = ph * w / h * (h == 224 && w == 256 ? 1.25f : 1);   /* 256-wide screens are wider pixels */
                float th = ph, ty0 = y;
                if (tw > pw) { th = ph * pw / tw; tw = pw; ty0 = y + (ph - th) / 2; }   /* too wide: fit by width */
                float tx0 = x + (pw - tw) / 2;
                if (v < ty0 || v >= ty0 + th) { ui_blend_px(xx, yy, rgb_hex(0x141216), alpha); continue; }
                float fx = (u - tx0) / tw * w;
                if (fx < 0 || fx >= w) c = rgb_hex(0x141216);
                else c = unpack(img[(int)clampf((v - ty0) / th * h, 0, h - 1) * w + (int)fx]);
            } else {
                c = mix(rgb_hex(0x1E1F25), rgb_hex(0x2A2B33), (v - y) / ph);
                c = add(c, rgb(1, 1, 1), hashf((uint32_t)(yy * 977 + xx)) * .02f);
            }
            ui_blend_px(xx, yy, c, alpha);
        }
    theme_picture_frame(x, y, pw, ph, alpha);
}

static void draw_slot(float x, float y, float w, float t, float alpha) {
    (void)t;
    if (slot_info_for != settings.state_slot) {
        savestate_info_free(&slot_info);
        savestate_info(settings.state_slot, &slot_info);
        slot_info_for = settings.state_slot;
    }
    float ph = w * 224 / 424.f;
    draw_picture(slot_info.thumb, slot_info.w, slot_info.h, x, y, w, ph, alpha);
    if (!slot_info.exists) {
        const char *empty = "Empty slot";
        float ew = ui_text_width(empty, "Segoe UI", 18, 400, 2);
        ui_draw_text(empty, "Segoe UI", 18, 400, 2, x + (w - ew) / 2, y + ph / 2 - 12, C_DIM, C_DIM, alpha);
    }
    char when[96];
    if (slot_info.exists) {
        struct tm *tm = localtime(&slot_info.saved_at);
        strftime(when, sizeof when, "Saved %d %b %Y, %H:%M", tm);
    } else {
        snprintf(when, sizeof when, "Nothing saved here yet");
    }
    ui_draw_text(when, "Segoe UI", 15, 400, 0, x, y + ph + 18, C_LABEL, C_LABEL, alpha);
    ui_keycap(x, y + ph + 44, "\xE2\x86\x90\xE2\x86\x92", "change slot", alpha);
}

/* Wraps `text` to `width` and draws it from (x, y); returns the y below it. */
static float draw_wrapped(const char *text, float x, float y, float width, float px, Rgb c, float alpha) {
    char line[256], word[96];
    line[0] = 0;
    const char *p = text;
    while (*p) {
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && n < 95) word[n++] = *p++;
        word[n] = 0;
        if (!n) break;
        char test[352];
        snprintf(test, sizeof test, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && ui_text_width(test, "Segoe UI", px, 400, 0) > width) {
            ui_draw_text(line, "Segoe UI", px, 400, 0, x, y, c, c, alpha);
            y += px * 1.45f;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", test);
        }
    }
    if (line[0]) { ui_draw_text(line, "Segoe UI", px, 400, 0, x, y, c, c, alpha); y += px * 1.45f; }
    return y;
}

/* A tab's icon large, in a gold ring with leaves, standing in for a picture. */
static void big_icon(float cx, float cy, float size, float t, float alpha) {
    (void)t;
    ui_glow(cx, cy, 30 * size, rgb_hex(0xF8C860), .16f * alpha);
    ui_capsule(cx, cy, cx, cy, 15.5f * size, 1.4f, TH_GOLD, .7f * alpha);
    theme_leaf(cx - 15.5f * size - 4, cy + 6 * size, 9 * size, 2.4f, TH_LEAF, alpha);
    theme_leaf(cx + 15.5f * size + 4, cy + 6 * size, 9 * size, .74f, TH_LEAF, alpha);
    ui_icon(tab_icons[tab], cx, cy, size, TH_CREAM, alpha);
}

/* The heading of the card: the row's name in small gold capitals over a leafy rule. */
static float card_head(const char *label, float x, float y, float in, float alpha) {
    char head[64];
    size_t k = 0;
    for (const char *p = label; *p && k < sizeof head - 1; p++) head[k++] = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
    head[k] = 0;
    ui_draw_text(head, "Palatino Linotype", 15, 700, 4, x, y, TH_GOLD_HI, TH_GOLD, alpha);
    theme_rule(x, y + 28, 70 * in + 20, alpha);
    return y + 46;
}

/* The card on the right, for the selected row; its content rises in when that changes. */
static void draw_card(float t, float alpha) {
    theme_frame(CARD_X, CARD_Y, CARD_W, CARD_H, false, 1);
    float in = ease_out((float)(now - selected_at) / .28f);
    alpha *= in;
    const Item *it = item_at(selected);
    float x = CARD_X + 34, w = CARD_W - 68, y = CARD_Y + 30 + (1 - in) * 8;
    if (!it) {                          /* on the tab bar: the tab itself */
        big_icon(CARD_X + CARD_W / 2.f, CARD_Y + 190 + (1 - in) * 8, 3.4f, t, alpha);
        const char *name = TAB_NAME(tab);
        float lw = ui_text_width(name, "Palatino Linotype", 28, 700, 8);
        theme_heading(name, CARD_X + (CARD_W - lw) / 2, CARD_Y + 296, 28, 8, alpha);
        float kx = CARD_X + CARD_W / 2.f - 96;
        kx = ui_keycap(kx, CARD_Y + 356, "\xE2\x86\x90\xE2\x86\x92", "or", alpha);
        ui_keycap(kx - 8, CARD_Y + 356, "Q  E", "change tabs", alpha);
        return;
    }
    if (it->kind == ITEM_ACTION && it->id == ACT_PLAY) {
        /* The launcher's cover: the title screen, the version and how to start. */
        float ph = w * 224 / 300.f;
        draw_picture(frame, frame_w, frame_h, x, y - 4, w, ph, alpha);
        float ty = y + ph + 20;
        if (game) {
            char line[160];
            snprintf(line, sizeof line, "%s  \xC2\xB7  %s", game->name, game->language);
            ui_draw_text(line, "Segoe UI Semibold", 16, 600, 0, x, ty, TH_GOLD_HI, TH_GOLD_HI, alpha);
            ty += 30;
        }
        draw_wrapped("Press Enter, A or Start to begin your adventure. Esc opens this menu again in the game.", x, ty, w, 15.5f, C_LABEL, alpha);
        return;
    }
    y = card_head(it->kind == ITEM_SLOT ? "Save slot" : it->label, x, y, in, alpha);
    if (it->kind == ITEM_SLOT) {
        char slot[32];
        snprintf(slot, sizeof slot, "Slot %d", settings.state_slot);
        ui_draw_text(slot, "Segoe UI Semibold", 15, 600, 0, x + w - ui_text_width(slot, "Segoe UI Semibold", 15, 600, 0), y - 46, TH_GOLD_HI, TH_GOLD_HI, alpha);
        draw_slot(x, y, w, t, alpha);
        return;
    }
    if (it->kind == ITEM_ACTION && it->id == ACT_MAP) {
        float mh = CARD_H - 46 - 30 - 112;
        ui_fill_rect(x, y, w, mh, rgb_hex(0x15161B), .8f * alpha);
        if (!automap_draw(x + 6, y + 6, w - 12, mh - 12, t, alpha)) {
            const char *none = "Nothing of this map seen yet.";
            float nw = ui_text_width(none, "Segoe UI", 15, 400, 0);
            ui_draw_text(none, "Segoe UI", 15, 400, 0, x + (w - nw) / 2, y + mh / 2 - 10, C_DIM, C_DIM, alpha);
        }
        theme_picture_frame(x, y, w, mh, alpha);
        draw_wrapped(about(it), x, y + mh + 18, w, 15.5f, C_LABEL, alpha);
        return;
    }
    if (it->kind == ITEM_ACTION && it->id == ACT_STATS) {
        char lines[12][64];
        int n = combat_stats_lines(lines, 12);
        for (int i = 0; i < n; i++) {
            char *tab_at = strchr(lines[i], '\t');
            if (!tab_at) continue;
            *tab_at = 0;
            float ly = y + i * 36 + (i >= 2 ? 22 : 0);   /* this session, then all sessions */
            if (i == 2) theme_rule(x, ly - 14, w, .6f * alpha);
            ui_draw_text(lines[i], "Segoe UI", 16, 400, 0, x, ly, C_LABEL, C_LABEL, alpha);
            float vw = ui_text_width(tab_at + 1, "Segoe UI Semibold", 17, 600, 0);
            ui_draw_text(tab_at + 1, "Segoe UI Semibold", 17, 600, 0, x + w - vw, ly - 1, TH_GOLD_HI, TH_GOLD_HI, alpha);
        }
        return;
    }
    if (it->kind == ITEM_OPTION && preview_supported(it->id) && preview_ready()) {
        float ph = w * 9 / 16.f;
        preview_draw(it->id, x, y, w, ph, t);
        theme_picture_frame(x, y, w, ph, alpha);
        y += ph + 20;
    } else {
        /* No picture: the tab's icon stands in for it. */
        big_icon(CARD_X + CARD_W / 2.f, y + 100, 2.8f, t, alpha);
        y += 210;
    }
    const char *text = about(it);
    if (text) y = draw_wrapped(text, x, y, w, 15.5f, C_LABEL, alpha);
    if (it->kind == ITEM_OPTION) {
        char value[64];
        settings_option_text(it->id, value, sizeof value);
        char line[96];
        snprintf(line, sizeof line, "Now: %s", value);
        ui_draw_text(line, "Segoe UI Semibold", 15, 600, 0, x, y + 8, TH_GOLD_HI, TH_GOLD_HI, alpha);
    }
}

/* An option shown as a switch: only on or off, and reading so now. */
static bool row_switch(const Item *it, char *value, size_t n) {
    if (it->kind != ITEM_OPTION || !settings_option_is_switch(it->id)) return false;
    settings_option_text(it->id, value, n);
    return !strcmp(value, "On") || !strcmp(value, "Off");
}

/* The springs and eases, by dt seconds. */
static void animate(float dt) {
    if (selected >= 0) ui_spring(&hl_y, &hl_v, row_y(selected), dt, 320);
    ui_spring(&ind_x, &ind_v, TABS_X + tab * TABW + TABW / 2.f, dt, 220);
    float k = 1 - expf(-dt * 12);
    for (int i = 0; i < TABS; i++) tab_on[i] += ((i == tab ? 1.f : 0.f) - tab_on[i]) * k;
    for (int i = 0; i < COUNT(tab) && i < MAX_ROWS; i++) {
        char value[64];
        float target = row_switch(&ITEMS(tab)[i], value, sizeof value) && !strcmp(value, "On") ? 1.f : 0.f;
        knob[i] = snap ? target : knob[i] + (target - knob[i]) * (1 - expf(-dt * 14));
    }
    snap = false;
}

/* A small gold chevron pointing left (dir -1) or right (+1). */
static void chevron(float x, float cy, int dir, Rgb c, float alpha) {
    ui_capsule(x - 3 * dir, cy - 5, x + dir, cy, .95f, 0, c, alpha);
    ui_capsule(x + dir, cy, x - 3 * dir, cy + 5, .95f, 0, c, alpha);
}

static void draw_row(int i, float dx, float alpha, float t) {
    (void)t;
    const Item *it = &ITEMS(tab)[i];
    float y = row_y(i), cy = y + ROW_H / 2.f;
    bool focus = selected == i;
    bool danger = it->kind == ITEM_ACTION && (it->id == ACT_RESET || it->id == ACT_QUIT);
    bool play = it->kind == ITEM_ACTION && it->id == ACT_PLAY;
    Rgb lc = focus ? (danger && confirm_pending ? rgb_hex(0xFF9A80) : TH_CREAM) : C_LABEL;
    if (play) ui_draw_text(it->label, "Palatino Linotype", 22, 700, 3, LIST_X + dx, y + 5, TH_GOLD_HI, TH_GOLD, alpha);
    else ui_draw_text(it->label, "Segoe UI", 17, 400, 0, LIST_X + dx, y + 9, lc, lc, alpha);
    char value[64] = "";
    if (row_switch(it, value, sizeof value)) {
        theme_apple(LIST_R - 10 + dx, cy + 1, 10, knob[i], alpha);
        return;
    }
    if (it->kind == ITEM_OPTION) settings_option_text(it->id, value, sizeof value);
    else if (it->kind == ITEM_SLOT) snprintf(value, sizeof value, "Slot %d", settings.state_slot);
    if (!value[0]) {
        if (it->kind == ITEM_ACTION && focus) chevron(LIST_R - 4 + dx, cy, 1, danger ? rgb_hex(0xFF9A80) : TH_GOLD, alpha);
        return;
    }
    /* A new value slides in from the side it changed towards. */
    float s = changed_row == i ? ease_out((float)(now - changed_at) / .26f) : 1;
    float slide = (1 - s) * 16 * changed_dir, va = alpha * (.25f + .75f * s);
    Rgb vc = focus ? TH_GOLD_HI : C_GOLD;
    float vw = ui_text_width(value, "Segoe UI Semibold", 17, 600, 0);
    float vx = LIST_R - 20 - vw;
    if (it->kind == ITEM_OPTION && it->id == OPT_VOLUME) {
        vx = LIST_R - 20 - ui_text_width("100%", "Segoe UI Semibold", 17, 600, 0);
        ui_slider(VOLUME_X + dx, cy, VOLUME_W, settings.volume / 100.f, focus);
    }
    Rgb ac = focus ? TH_GOLD : rgb_hex(0x6A6252);
    float nudge = (1 - s) * 3, lx = vx - 12 - nudge * (changed_dir < 0) + dx, rx = LIST_R - 4 + nudge * (changed_dir > 0) + dx;
    chevron(lx + 1, cy, -1, ac, alpha);
    chevron(rx - 1, cy, 1, ac, alpha);
    ui_draw_text(value, "Segoe UI Semibold", 17, 600, 0, vx + dx + slide, y + 9, vc, vc, va);
}

/* The orchard in the wind, behind the panels: leaves drifting down and turning, and in the
 * launcher apples falling, bouncing where they land and rolling away, and a garland of
 * leaves swaying along the top. Everything follows from the time alone (each piece loops
 * on its own period), so it needs no state and looks the same at any frame rate. */
static void draw_falling(float t, bool full) {
    static const uint32_t leaf_colors[5] = { 0x7CC64E, 0x5FA83C, 0x9DCB4A, 0xE0A03A, 0xC8642C };
    int leaves = full ? 26 : 10;
    for (int k = 0; k < leaves; k++) {
        float period = 9 + hashf((uint32_t)k * 97u) * 7;            /* seconds to cross the screen */
        float u = fmodf(t / period + hashf((uint32_t)k * 13u), 1);
        float x0 = hashf((uint32_t)k * 31u) * 1400 - 120;
        float y = -40 + u * 800;
        float sway = sinf(t * (.8f + hashf((uint32_t)k * 7u) * .6f) + k) * (26 + 18 * hashf((uint32_t)k * 3u));
        float x = x0 + u * 140 + sway;                                /* the wind carries them right */
        float spin = t * (1.2f + hashf((uint32_t)k * 19u) * 1.8f) * (k & 1 ? 1 : -1) + k;
        float size = 14 + hashf((uint32_t)k * 41u) * 12;
        float fade = smooth(0, .06f, u) * (1 - smooth(.92f, 1, u));
        Rgb c = rgb_hex(leaf_colors[k % 5]);
        /* Turning in the air: narrower when edge-on, a little darker on the back. */
        float turn = fabsf(cosf(spin * .7f));
        theme_leaf(x, y, size * (.55f + .45f * turn), spin, scale(c, .75f + .25f * turn), (full ? .9f : .45f) * fade);
    }
    if (!full) return;
    for (int k = 0; k < 6; k++) {
        float period = 6 + hashf((uint32_t)k * 211u) * 5;
        float u = fmodf(t / period + hashf((uint32_t)k * 57u), 1), life = u * period;   /* seconds into its fall */
        float x = 80 + hashf((uint32_t)k * 77u + (uint32_t)floorf(t / period + hashf((uint32_t)k * 57u)) * 13u) * 1120;
        float ground = 700, fall_t = sqrtf((ground + 30) / 260.f);   /* falling with gravity, 260 units/s^2 */
        float y, roll = 0, a = 1;
        if (life < fall_t) y = -30 + 130 * life * life;
        else {
            float b = life - fall_t;                                  /* bounces, each smaller, then rolls */
            float hop = 0, s = b;
            for (float h = 90; h > 4 && s >= 0; h *= .35f) {
                float d = 2 * sqrtf(2 * h / 260.f);
                if (s < d) { float q = s - d / 2; hop = h - 130 * q * q; break; }
                s -= d;
            }
            y = ground - fmaxf(hop, 0);
            roll = b * 70 * (k & 1 ? 1 : -1);
            a = 1 - smooth(period * .55f, period * .8f, life);
        }
        float r = 11 + hashf((uint32_t)k * 5u) * 4;
        theme_apple(x + roll, y, r, 1, .95f * a);
    }
    /* The garland: leaves on a vine along the top, swaying. */
    for (int k = 0; k < 34; k++) {
        float x = k * 40 - 20, base = 6 + 6 * sinf(k * .9f);
        float sway = sinf(t * 1.3f + k * .55f) * .25f;
        theme_leaf(x, base + 10, 22, 1.57f + .6f * ((k & 1) ? 1 : -1) + sway, rgb_hex(k % 3 ? 0x5FA83C : 0x7CC64E), .85f);
        if (k % 7 == 3) theme_apple(x + 4, base + 22 + sinf(t * 1.3f + k * .55f) * 3, 8, 1, .9f);
    }
}

void menu_render(double time_s) {
    float dt = (float)(time_s - last_now);
    if (dt < 0 || dt > .25f) dt = 1 / 60.f;
    last_now = now = time_s;
    if (open_pending) { opened_at = selected_at = now; open_pending = false; }
    float t = (float)time_s;
    animate(dt);
    float in = ease_out((float)(now - opened_at) / OPEN_S);
    if (closing_at >= 0) in *= 1 - ease_out((float)(now - closing_at) / CLOSE_S);

    memcpy(ui_px, backdrop, (size_t)ui_w * ui_h * 4);
    /* Motes of light drifting up through the warm air. */
    for (int k = 0; k < 26; k++) {
        float sx = hashf((uint32_t)k * 31u) * 1280, sp = 8 + hashf((uint32_t)k * 71u) * 14;
        float sy = 720 - fmodf(t * sp + hashf((uint32_t)k * 13u) * 720, 760) + 20;
        float tw = .5f + .5f * sinf(t * (1 + hashf((uint32_t)k * 5u)) + k);
        ui_glow(sx + sinf(t * .4f + k) * 12, sy, 2.4f, rgb_hex(0xFFE6A0), .5f * tw);
    }

    draw_falling(t, launcher);

    /* The panel: the dialogue box, the heading, the tabs. */
    theme_frame(TAB_X0, TAB_Y0, TAB_W0, TAB_H0, true, 1);
    if (launcher && game && game->title) theme_heading(game->title, LIST_X - 2, TAB_Y0 + 30, 30, 4, 1);
    else theme_heading("PAUSED", LIST_X - 2, TAB_Y0 + 26, 40, 8, 1);
    for (int k = 0; k < TABS; k++) {
        float cx = TABS_X + k * TABW + TABW / 2.f, on = tab_on[k];
        Rgb ic = mix(rgb_hex(0x8A8470), TH_GOLD_HI, on);
        if (on > .02f) ui_glow(cx, TABS_Y + 22, 30, rgb_hex(0xF8C860), .10f * on);
        ui_icon(tab_icons[k], cx, TABS_Y + 22 - on * 2, 1, ic, .65f + .35f * on);
        Rgb tc = mix(rgb_hex(0x8C8672), TH_CREAM, on);
        const char *name = TAB_NAME(k);
        float tw = ui_text_width(name, "Segoe UI Semibold", 12, 600, 2.5f);
        ui_draw_text(name, "Segoe UI Semibold", 12, 600, 2.5f, cx - tw / 2, TABS_Y + 42, tc, tc, 1);
    }
    float uw = TABW - 20;
    if (selected < 0) ui_glow(ind_x, TABS_Y + TABS_H - 6, 40, rgb_hex(0xF8C860), .25f);
    theme_rule(ind_x - uw / 2, TABS_Y + TABS_H - 6, uw, 1);
    ui_fill_rect(TAB_X0 + 30, TABS_Y + TABS_H + 8, TAB_W0 - 60, 1, rgb_hex(0x6A5E4E), .7f);

    /* The highlight on a spring, with the sword pointing at it; the rows cascade in. */
    if (selected >= 0) {
        float a = ease_out((float)(now - tab_at - .1f) / .3f);
        ui_panel(LIST_X - 14, hl_y + 3, LIST_R - LIST_X + 22, ROW_H - 6, 6, rgb_hex(0x46603A), rgb_hex(0x34482C), .55f * a,
                 TH_LEAF, .35f * a);
        theme_sword(LIST_X - 18 + sinf(t * 5) * 2, hl_y + ROW_H / 2.f, a);
    }
    int dir = tab > tab_from ? 1 : -1;
    for (int i = 0; i < COUNT(tab); i++) {
        float e = ease_out((float)(now - fmax(tab_at, opened_at) - i * .03f) / .28f);
        if (e > 0) draw_row(i, (1 - e) * 24 * dir, e, t);
    }
    float kx = LIST_X, ky = TAB_Y0 + TAB_H0 - 50;
    kx = ui_keycap(kx, ky, "\xE2\x86\x91\xE2\x86\x93", "select", 1);
    kx = ui_keycap(kx, ky, "\xE2\x86\x90\xE2\x86\x92", "change", 1);
    kx = ui_keycap(kx, ky, "Q  E", "tabs", 1);
    kx = ui_keycap(kx, ky, "Enter", "choose", 1);
    if (!launcher) ui_keycap(kx, ky, "Esc", "resume", 1);

    draw_card(t, 1);

    if (now < toast_until) {
        float a = clampf((float)(toast_until - now) / .4f, 0, 1) * ease_out((float)(2.2 - (toast_until - now)) / .2f);
        float w = ui_text_width(toast, "Segoe UI Semibold", 16, 600, 1) + 64;
        float x = CARD_X + (CARD_W - w) / 2, y = CARD_Y + CARD_H + 22 + (1 - a) * 8;
        theme_frame(x, y, w, 52, false, a);
        ui_draw_text(toast, "Segoe UI Semibold", 16, 600, 1, x + 32, y + 15, TH_CREAM, TH_CREAM, a);
    }
    controls_render(time_s);

    /* Opening and closing: the menu dissolves in from the game itself, and back out to it. */
    if (in < 1 && sharp) {
        int a = (int)(clampf(in, 0, 1) * 256);
        for (size_t i = 0, n = (size_t)ui_w * ui_h; i < n; i++) {
            uint32_t m = ui_px[i], g = sharp[i];
            int r = (int)((g >> 16) & 255) + ((((int)((m >> 16) & 255) - (int)((g >> 16) & 255)) * a) >> 8);
            int gg = (int)((g >> 8) & 255) + ((((int)((m >> 8) & 255) - (int)((g >> 8) & 255)) * a) >> 8);
            int b = (int)(g & 255) + ((((int)(m & 255) - (int)(g & 255)) * a) >> 8);
            ui_px[i] = (uint32_t)(r << 16 | gg << 8 | b);
        }
    }
}
