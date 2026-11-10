/* The pause menu and the launcher (menu.h): clean and minimal. One column of large text on
 * the left, a thin gold accent for the selection, and on the right the picture large: the
 * title screen in the launcher, the paused game in the menu, or what the selected row
 * shows (its save slot, a before/after preview, the map, the stats) with a line or two on
 * what it does. Pages instead of tabs: the home page, Settings, and its groups; Esc goes
 * back a page. */
#include "menu.h"
#include "controls.h"
#include "preview.h"
#include "combat.h"
#include "automap.h"
#include "genesis.h"
#include "game.h"
#include "savestate.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { ITEM_ACTION, ITEM_SLOT, ITEM_OPTION, ITEM_PAGE } ItemKind;
enum { ACT_RESUME, ACT_SAVE, ACT_LOAD, ACT_RESET, ACT_QUIT, ACT_CONTROLS, ACT_PRESET_ORIGINAL, ACT_PRESET_ENHANCED,
       ACT_STATS, ACT_MAP, ACT_PLAY, ACT_ROM, ACT_EXIT };
typedef struct { ItemKind kind; const char *label; int id; } Item;

/* ---- The pages --------------------------------------------------------------------------- */
enum { PAGE_HOME, PAGE_SETTINGS, PAGE_DISPLAY, PAGE_EFFECTS, PAGE_GAMEPLAY, PAGE_SOUND, PAGE_SYSTEM, PAGES };
#define MAX_ROWS 10
typedef struct { const char *title; int count; Item items[MAX_ROWS]; } Page;

static const Page home_pause = { "Paused", 8, {
    { ITEM_ACTION, "Resume", ACT_RESUME }, { ITEM_SLOT, "Save state", ACT_SAVE }, { ITEM_SLOT, "Load state", ACT_LOAD },
    { ITEM_ACTION, "Map", ACT_MAP }, { ITEM_ACTION, "Stats", ACT_STATS }, { ITEM_PAGE, "Settings", PAGE_SETTINGS },
    { ITEM_ACTION, "Reset game", ACT_RESET }, { ITEM_ACTION, "Quit game", ACT_QUIT } } };
static const Page home_launcher = { NULL, 5, {
    { ITEM_ACTION, "Play", ACT_PLAY }, { ITEM_PAGE, "Settings", PAGE_SETTINGS }, { ITEM_OPTION, "Language", OPT_LANGUAGE },
    { ITEM_ACTION, "Choose ROM", ACT_ROM }, { ITEM_ACTION, "Quit", ACT_EXIT } } };
static const Page pages[PAGES] = {
    [PAGE_SETTINGS] = { "Settings", 8, {
        { ITEM_PAGE, "Display", PAGE_DISPLAY }, { ITEM_PAGE, "Effects", PAGE_EFFECTS }, { ITEM_PAGE, "Gameplay", PAGE_GAMEPLAY },
        { ITEM_PAGE, "Sound", PAGE_SOUND }, { ITEM_ACTION, "Controls", ACT_CONTROLS }, { ITEM_PAGE, "System", PAGE_SYSTEM },
        { ITEM_ACTION, "Original experience", ACT_PRESET_ORIGINAL }, { ITEM_ACTION, "Enhanced experience", ACT_PRESET_ENHANCED } } },
    [PAGE_DISPLAY] = { "Display", 7, {
        { ITEM_OPTION, "Window", OPT_WINDOW_ALL }, { ITEM_OPTION, "Integer scaling", OPT_INTEGER },
        { ITEM_OPTION, "Internal resolution", OPT_INTERNAL }, { ITEM_OPTION, "Scaler", OPT_SCALER },
        { ITEM_OPTION, "Filter", OPT_FILTER }, { ITEM_OPTION, "Sharpening", OPT_SHARPEN },
        { ITEM_OPTION, "Sprite flicker fix", OPT_FLICKER } } },
    [PAGE_EFFECTS] = { "Effects", 5, {
        { ITEM_OPTION, "Lighting", OPT_LIGHTING }, { ITEM_OPTION, "HDR look", OPT_HDR }, { ITEM_OPTION, "Water", OPT_WATER },
        { ITEM_OPTION, "Texture pack", OPT_TEXPACK }, { ITEM_OPTION, "Dump tiles", OPT_DUMP } } },
    [PAGE_GAMEPLAY] = { "Gameplay", 8, {
        { ITEM_OPTION, "Difficulty", OPT_DIFFICULTY }, { ITEM_OPTION, "Infinite health", OPT_CHEAT_HEALTH },
        { ITEM_OPTION, "Combat info", OPT_COMBAT }, { ITEM_OPTION, "Rumble", OPT_RUMBLE }, { ITEM_OPTION, "Play timer", OPT_TIMER },
        { ITEM_OPTION, "Fast-forward", OPT_FAST }, { ITEM_OPTION, "Rewind", OPT_REWIND_ALL },
        { ITEM_OPTION, "Slowdown fix", OPT_OVERCLOCK } } },
    [PAGE_SOUND] = { "Sound", 2, { { ITEM_OPTION, "Volume", OPT_VOLUME }, { ITEM_OPTION, "Audio", OPT_AUDIO } } },
    [PAGE_SYSTEM] = { "System", 7, {
        { ITEM_OPTION, "Language", OPT_LANGUAGE }, { ITEM_OPTION, "Launcher at startup", OPT_LAUNCHER },
        { ITEM_OPTION, "Performance stats", OPT_PERF_STATS }, { ITEM_OPTION, "Auto quality", OPT_AUTO_QUALITY },
        { ITEM_OPTION, "GPU effects", OPT_GPU }, { ITEM_OPTION, "Merged finishing pass", OPT_MERGED },
        { ITEM_OPTION, "AVX2 pixel loops", OPT_AVX2 } } },
};

/* What each row does, for the text under the picture. */
static const char *about(const Item *it) {
    if (it->kind == ITEM_PAGE)
        switch (it->id) {
        case PAGE_SETTINGS: return "Picture, effects, gameplay, sound, controls and system options.";
        case PAGE_DISPLAY: return "The window, the internal resolution and how the pixels are drawn.";
        case PAGE_EFFECTS: return "Lighting, the HDR look, living water and texture packs. Only the picture changes.";
        case PAGE_GAMEPLAY: return "Difficulty, combat info, rewind, fast-forward and the slowdown fix.";
        case PAGE_SOUND: return "The volume and how the console's sound is output.";
        case PAGE_SYSTEM: return "The language, the launcher and the renderer's options.";
        }
    if (it->kind == ITEM_ACTION)
        switch (it->id) {
        case ACT_RESUME: return "Back to the game.";
        case ACT_RESET: return "Starts the game over from the beginning. Unsaved progress is lost.";
        case ACT_QUIT: return "Closes the game. Unsaved progress is lost; the game's own saves stay.";
        case ACT_EXIT: return "Closes the game.";
        case ACT_CONTROLS: return "The keyboard key and controller button for every action.";
        case ACT_MAP: return "Fills in as you explore: everything that has been on screen on this map, with your place blinking.";
        case ACT_PRESET_ORIGINAL: return "Everything as the cartridge was: native resolution, no effects, the original sprite limit, speed and difficulty. Controls, sound and saves stay as they are.";
        case ACT_PRESET_ENHANCED: return "The recommended enhancements: the art redrawn at 3x with light sharpening, a soft glow, the HDR look, living water, no sprite flicker and no slowdown.";
        case ACT_ROM: return "Pick a ROM of Crusader of Centy (USA), or Ragnacenty from Japan or Korea. ROMs in the rom folder next to the game are found by themselves.";
        case ACT_PLAY: return NULL;
        }
    if (it->kind == ITEM_SLOT) return NULL;
    switch (it->id) {
    case OPT_WINDOW_ALL: return "Fullscreen, or a window of this many times the game's size. F11 switches at any time.";
    case OPT_INTEGER: return "Scales by whole numbers only, so every game pixel is the same size.";
    case OPT_INTERNAL: return "Draws each layer at a higher resolution, rounding the edges of the pixel art. Native is the original look; 6x is for 1440p and 4K screens and needs a fast CPU.";
    case OPT_SCALER: return "Edge rounds corners, Smooth (xBR) follows curves and slopes, Sharp keeps every pixel square, exactly as drawn.";
    case OPT_FILTER: return "Scanlines, a curved CRT tube or an LCD grid over the picture.";
    case OPT_SHARPEN: return "Crisper edges after scaling, without halos.";
    case OPT_FLICKER: return "Lifts the hardware's limit of sprites on one line, so crowded scenes no longer flicker.";
    case OPT_LIGHTING: return "Bright spots glow softly into their surroundings.";
    case OPT_HDR: return "Deeper shadows, brighter highlights and richer colour. Strong goes further.";
    case OPT_WATER: return "Streams, lakes and the sea swell, foam along the shore and around anyone standing in them, and reflect what is above them.";
    case OPT_TEXPACK: return "High-resolution replacements for the game's tiles, from a folder in texture_packs next to the game.";
    case OPT_DUMP: return "Writes every tile shown to texture_packs/dump: the starting point for making a pack.";
    case OPT_DIFFICULTY: return "How much each hit takes from your apples: Original, Easy (half), Hard (one and a half) or Very hard (twice).";
    case OPT_CHEAT_HEALTH: return "Hits take nothing, and your apples stay full.";
    case OPT_COMBAT: return "The damage of every hit rises from the enemy as a number; with bars, a health bar shows over an enemy after each hit.";
    case OPT_RUMBLE: return "The controller rumbles when you land a hit, harder for a defeat and when you are hit.";
    case OPT_TIMER: return "This session's play time in the corner of the picture. The total is under Stats.";
    case OPT_FAST: return "How fast the game runs while the fast-forward key is held (Tab, or RB).";
    case OPT_REWIND_ALL: return "Hold the rewind key (Backspace, or LB) to step back in time. The size is how far back it goes.";
    case OPT_OVERCLOCK: return "Gives the console's processor more time each frame, so busy scenes no longer slow down. Music and speed stay the same.";
    case OPT_VOLUME: return "The game's volume. It follows your ears: each step down sounds about as much quieter as the last.";
    case OPT_AUDIO: return "Original is the console's sound as emulated. Clean removes aliasing and the hum; Rich adds wider stereo and a light room.";
    case OPT_LANGUAGE: return settings_language_count() > 1
        ? "Each country's cartridge has its own language. Changing it starts that version from its title screen, with its own saves."
        : "Each country's cartridge has its own language. Put another country's ROM in the rom folder next to the game to choose it here.";
    case OPT_LAUNCHER: return "The launcher before the game starts. Off starts the game straight away.";
    case OPT_PERF_STATS: return "Pictures a second, the internal resolution and each step's time, in the top-left corner.";
    case OPT_AUTO_QUALITY: return "Lowers the internal resolution while pictures fall behind your display, and raises it again when there is room.";
    case OPT_GPU: return "Lighting, HDR and sharpening on the graphics card instead of the CPU. Takes effect the next time the game starts.";
    case OPT_MERGED: return "On the CPU, the finishing effects in one pass over the picture instead of one each.";
    case OPT_AVX2: return "The CPU's effect loops work on eight pixels at a time, on processors that have AVX2. The result is the same.";
    }
    return NULL;
}

/* ---- Layout, in 1280x720 units --------------------------------------------------------- */
#define COL_X   96                      /* the column's left edge */
#define COL_R   556                     /* and its right edge (values end here) */
#define HEAD_Y  86
#define LIST_Y  196
#define PIC_X   640
#define PIC_Y   120
#define PIC_W   544
#define PIC_H   408
#define TEXT_Y  (PIC_Y + PIC_H + 26)
#define VOLUME_W 120

#define C_TEXT  rgb_hex(0xF2F0EA)
#define C_DIM   rgb_hex(0x8C8F98)
#define C_FAINT rgb_hex(0x5A5D66)
#define C_GOLD  rgb_hex(0xE8C477)
#define C_RED   rgb_hex(0xF08A78)
#define C_BG    rgb_hex(0x0F1013)
#define FONT    "Segoe UI"
#define FONT_SB "Segoe UI Semibold"

void (*menu_sound)(int kind);
static void sound(int kind) { if (menu_sound) menu_sound(kind); }

static bool open_, launcher;
static int page = PAGE_HOME, selected;
static int stack[4], stack_sel[4], depth;       /* the pages above this one, and their selection */
static uint32_t *backdrop;              /* the background, at canvas size */
static uint32_t *frame;                 /* the paused frame or the title screen */
static int frame_w, frame_h;
static SlotInfo slot_info;
static int slot_info_for = -1;
static float mouse_scale = 1;
static char toast[96];
static double toast_until, now, last_now, opened_at, page_at, changed_at;
static int changed_row = -1, changed_dir = 1, page_dir = 1;
static float hl_y, hl_v;                /* the accent, on a spring to the selected row */
static double closing_at = -1;
static uint32_t *sharp;                 /* the window's picture before opening: the menu fades from it */
static bool open_pending, confirm_pending;
static int confirm_item;

#define CLOSE_S .18

static const Page *cur(void) { return page == PAGE_HOME ? (launcher ? &home_launcher : &home_pause) : &pages[page]; }
static bool big_rows(void) { return page == PAGE_HOME || page == PAGE_SETTINGS; }
static float row_h(void) { return big_rows() ? 54 : 44; }
static float row_y(int i) { return LIST_Y + i * row_h(); }
static const Item *item_at(int i) { return i >= 0 && i < cur()->count ? &cur()->items[i] : NULL; }

bool menu_is_open(void) {
    if (open_ && closing_at >= 0 && now - closing_at >= CLOSE_S) {
        open_ = false;
        closing_at = -1;
        savestate_info_free(&slot_info);
        slot_info_for = -1;
    }
    return open_;
}

/* Box blur in place, three passes per axis (close to a Gaussian). */
static void blur_rgb(uint32_t *px, int w, int h, int r) {
    if (r < 1) return;
    uint32_t *tmp = malloc((size_t)w * h * 4);
    if (!tmp) return;
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

/* Opens on the home page. The background: the game's picture (or the title screen) blurred
 * and darkened far down, darkest behind the column so the text reads on anything. */
static void open_common(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
                        int out_w, int out_h, float window_to_pixels) {
    bool reopen = open_ && closing_at < 0;
    open_ = true;
    closing_at = -1;
    if (!reopen) {
        page = PAGE_HOME;
        depth = 0;
        selected = 0;
        confirm_pending = false;
        open_pending = true;
        hl_y = row_y(0);
        hl_v = 0;
        sound(1);
    }
    slot_info_for = -1;
    mouse_scale = window_to_pixels;
    ui_begin(out_w, out_h);

    free(frame);
    frame_w = fb_w; frame_h = fb_h;
    frame = malloc((size_t)fb_w * fb_h * 4);
    for (int y = 0; y < fb_h; y++) memcpy(frame + (size_t)y * fb_w, fb + (size_t)y * fb_pitch, (size_t)fb_w * 4);

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
    blur_rgb(backdrop, out_w, out_h, (int)(14 * ui_s));
    Rgb bg = C_BG;
    for (int y = 0; y < out_h; y++)
        for (int x = 0; x < out_w; x++) {
            float u = (x - ui_ox) / ui_s;
            float k = (launcher ? .16f : .22f) + .1f * smooth(560, 900, u);
            Rgb c = mix(bg, unpack(backdrop[y * out_w + x]), k);
            c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * out_w + x)) - .5f) * 2.f / 255);
            backdrop[y * out_w + x] = pack(c);
        }
}

void menu_open(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
               int out_w, int out_h, float window_to_pixels) {
    if (launcher) open_ = false;
    launcher = false;
    open_common(fb, fb_w, fb_h, fb_pitch, game_x, game_y, game_w, game_h, out_w, out_h, window_to_pixels);
}

void menu_open_launcher(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int out_w, int out_h, float window_to_pixels) {
    if (!launcher) open_ = false;
    launcher = true;
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

void menu_close(void) {
    if (!open_ || closing_at >= 0) return;
    sound(2);
    closing_at = now;
}

/* For tests: page `k` (0 home, 1 settings, 2-6 its groups) with row `row` selected. */
void menu_select(int k, int row) {
    page = k < 0 || k >= PAGES ? PAGE_HOME : k;
    depth = page == PAGE_HOME ? 0 : page == PAGE_SETTINGS ? 1 : 2;
    stack[0] = PAGE_HOME; stack[1] = PAGE_SETTINGS;
    stack_sel[0] = launcher ? 1 : 5; stack_sel[1] = 0;
    selected = row < 0 ? 0 : row >= cur()->count ? cur()->count - 1 : row;
    hl_y = row_y(selected);
    opened_at = page_at = -10;
    open_pending = false;
}

static void notify(const char *msg) {
    snprintf(toast, sizeof toast, "%s", msg);
    toast_until = now + 2.2;
}

static void go_page(int p) {
    if (depth >= 4) return;
    stack[depth] = page; stack_sel[depth] = selected; depth++;
    page = p; selected = 0; page_at = now; page_dir = 1;
    hl_y = row_y(0); hl_v = 0;
    confirm_pending = false;
    sound(1);
}

static bool go_back(void) {
    if (!depth) return false;
    depth--;
    page = stack[depth]; selected = stack_sel[depth]; page_at = now; page_dir = -1;
    hl_y = row_y(selected); hl_v = 0;
    confirm_pending = false;
    sound(2);
    return true;
}

static void select_row(int i) {
    if (i == selected || i < 0 || i >= cur()->count) return;
    selected = i;
    confirm_pending = false;
    sound(0);
}

static int hit_test(float mx, float my) {
    for (int i = 0; i < cur()->count; i++) {
        float y = row_y(i);
        if (mx >= COL_X - 24 && mx <= COL_R + 24 && my >= y && my < y + row_h()) return i;
    }
    return -1;
}

/* Runs the selected row (activate) or steps its value (dir = -1 / +1). */
static MenuAction act(int dir, bool activate) {
    const Item *it = item_at(selected);
    if (!it) return MENU_NONE;
    char msg[96];
    switch (it->kind) {
    case ITEM_PAGE:
        if (activate || dir > 0) go_page(it->id);
        return MENU_NONE;
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
            changed_row = selected; changed_dir = dir; changed_at = now;
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
        switch (it->id) {
        case ACT_RESUME: return MENU_CLOSE;
        case ACT_PLAY: sound(1); return MENU_PLAY;
        case ACT_ROM: sound(1); return MENU_ROM;
        case ACT_EXIT: return MENU_QUIT;
        case ACT_CONTROLS: sound(1); controls_open(); return MENU_NONE;
        case ACT_STATS: case ACT_MAP: return MENU_NONE;           /* shown on the right */
        case ACT_PRESET_ORIGINAL: case ACT_PRESET_ENHANCED:
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

static MenuAction back(void) {
    if (go_back()) return MENU_NONE;
    return launcher ? MENU_NONE : MENU_CLOSE;
}

MenuAction menu_event(const SDL_Event *e) {
    int dir = 0;
    bool activate = false;
    if (closing_at >= 0) return MENU_NONE;
    if (controls_is_open()) {           /* the controls page takes all input while it is up */
        float mx = 0, my = 0;
        if (e->type == SDL_MOUSEMOTION) { mx = e->motion.x; my = e->motion.y; }
        if (e->type == SDL_MOUSEBUTTONDOWN) { mx = e->button.x; my = e->button.y; }
        mx = (mx * mouse_scale - ui_ox) / ui_s;
        my = (my * mouse_scale - ui_oy) / ui_s;
        bool changed = controls_event(e, mx, my);
        if (!controls_is_open()) sound(2);
        return changed ? MENU_SETTINGS : MENU_NONE;
    }
    int n = cur()->count;
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE: return back();
        case SDLK_UP: select_row((selected + n - 1) % n); break;
        case SDLK_DOWN: case SDLK_TAB: select_row((selected + 1) % n); break;
        case SDLK_LEFT: dir = -1; break;
        case SDLK_RIGHT: dir = 1; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: case SDLK_z: activate = true; break;
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_START: if (launcher) return MENU_PLAY; return depth ? back() : MENU_CLOSE;
        case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_BACK: return back();
        case SDL_CONTROLLER_BUTTON_DPAD_UP: select_row((selected + n - 1) % n); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: select_row((selected + 1) % n); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: dir = -1; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: dir = 1; break;
        case SDL_CONTROLLER_BUTTON_A: activate = true; break;
        }
        break;
    case SDL_MOUSEMOTION: {
        float mx = (e->motion.x * mouse_scale - ui_ox) / ui_s, my = (e->motion.y * mouse_scale - ui_oy) / ui_s;
        int h = hit_test(mx, my);
        if (h >= 0) select_row(h);
        return MENU_NONE;
    }
    case SDL_MOUSEBUTTONDOWN: {
        float mx = (e->button.x * mouse_scale - ui_ox) / ui_s, my = (e->button.y * mouse_scale - ui_oy) / ui_s;
        if (e->button.button == SDL_BUTTON_X1) return back();
        int h = hit_test(mx, my);
        if (h < 0) return e->button.button == SDL_BUTTON_RIGHT ? back() : MENU_NONE;
        selected = h;
        const Item *it = item_at(h);
        if (it->kind == ITEM_OPTION && it->id == OPT_VOLUME && e->button.button == SDL_BUTTON_LEFT &&
            mx >= COL_R - 70 - VOLUME_W && mx <= COL_R - 62) {
            settings_set_volume_fraction(ui_slider_value(COL_R - 66 - VOLUME_W, VOLUME_W, mx));
            sound(0);
            return MENU_SETTINGS;
        }
        if (e->button.button == SDL_BUTTON_RIGHT) dir = -1;
        else if (it->kind == ITEM_OPTION || (it->kind == ITEM_SLOT && mx > COL_R - 120)) dir = 1;
        else activate = true;
        break;
    }
    default:
        return MENU_NONE;
    }
    if (!dir && !activate) return MENU_NONE;
    if (dir && item_at(selected) && item_at(selected)->kind == ITEM_ACTION) return MENU_NONE;
    return act(dir, activate);
}

/* ---- Drawing ----------------------------------------------------------------------------- */
static float ease_out(float x) { x = clampf(x, 0, 1); return 1 - (1 - x) * (1 - x) * (1 - x); }

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
        if (line[0] && ui_text_width(test, FONT, px, 400, 0) > width) {
            ui_draw_text(line, FONT, px, 400, 0, x, y, c, c, alpha);
            y += px * 1.5f;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", test);
        }
    }
    if (line[0]) { ui_draw_text(line, FONT, px, 400, 0, x, y, c, c, alpha); y += px * 1.5f; }
    return y;
}

/* A thin rectangle outline. */
static void outline(float x, float y, float w, float h, Rgb c, float alpha) {
    ui_fill_rect(x - 1, y - 1, w + 2, 1, c, alpha);
    ui_fill_rect(x - 1, y + h, w + 2, 1, c, alpha);
    ui_fill_rect(x - 1, y, 1, h, c, alpha);
    ui_fill_rect(x + w, y, 1, h, c, alpha);
}

/* A picture of w x h pixels, fitted into (x, y, pw, ph) layout units with the console's
 * 4:3 shape (256- and 320-wide screens both fill it), on black. */
static void draw_picture(const uint32_t *img, int w, int h, float x, float y, float pw, float ph, float alpha) {
    ui_fill_rect(x, y, pw, ph, rgb(0, 0, 0), alpha);
    if (img && w && h) {
        float aspect = 4.f / 3.f * (w > 320 ? (float)w / 320 : 1), tw = ph * aspect, th = ph;
        if (tw > pw) { th = pw / aspect; tw = pw; }
        float tx = x + (pw - tw) / 2, ty = y + (ph - th) / 2;
        int x0 = (int)ui_x(tx), y0 = (int)ui_y(ty), x1 = (int)ui_x(tx + tw), y1 = (int)ui_y(ty + th);
        for (int yy = y0; yy < y1; yy++) {
            int sy = (int)((float)(yy - y0) / (y1 - y0) * h);
            for (int xx = x0; xx < x1; xx++) {
                int sx = (int)((float)(xx - x0) / (x1 - x0) * w);
                ui_blend_px(xx, yy, unpack(img[sy * w + sx]), alpha);
            }
        }
    }
    outline(x, y, pw, ph, rgb_hex(0x2C2E35), alpha);
}

static void draw_slot_picture(float alpha) {
    if (slot_info_for != settings.state_slot) {
        savestate_info_free(&slot_info);
        savestate_info(settings.state_slot, &slot_info);
        slot_info_for = settings.state_slot;
    }
    draw_picture(slot_info.exists ? slot_info.thumb : NULL, slot_info.w, slot_info.h, PIC_X, PIC_Y, PIC_W, PIC_H, alpha);
    char line[128];
    if (slot_info.exists) {
        char when[64];
        strftime(when, sizeof when, "%d %b %Y, %H:%M", localtime(&slot_info.saved_at));
        snprintf(line, sizeof line, "Slot %d  \xC2\xB7  saved %s", settings.state_slot, when);
        ui_draw_text(line, FONT, 16, 400, 0, PIC_X, TEXT_Y, C_TEXT, C_TEXT, alpha);
    } else {
        snprintf(line, sizeof line, "Slot %d is empty", settings.state_slot);
        float w = ui_text_width(line, FONT, 17, 400, 0);
        ui_draw_text(line, FONT, 17, 400, 0, PIC_X + (PIC_W - w) / 2, PIC_Y + PIC_H / 2 - 12, C_DIM, C_DIM, alpha);
    }
    ui_draw_text("\xE2\x86\x90 \xE2\x86\x92  change slot", FONT, 14, 400, 0, PIC_X, TEXT_Y + 28, C_DIM, C_DIM, alpha);
}

/* The right side: the picture for the selected row, and what the row does. */
static void draw_side(float t, float alpha) {
    const Item *it = item_at(selected);
    if (it && it->kind == ITEM_SLOT) { draw_slot_picture(alpha); return; }
    if (it && it->kind == ITEM_ACTION && it->id == ACT_MAP) {
        ui_fill_rect(PIC_X, PIC_Y, PIC_W, PIC_H, rgb_hex(0x08090B), alpha);
        if (!automap_draw(PIC_X + 8, PIC_Y + 8, PIC_W - 16, PIC_H - 16, t, alpha)) {
            const char *none = "Nothing of this map seen yet.";
            float w = ui_text_width(none, FONT, 16, 400, 0);
            ui_draw_text(none, FONT, 16, 400, 0, PIC_X + (PIC_W - w) / 2, PIC_Y + PIC_H / 2 - 10, C_DIM, C_DIM, alpha);
        }
        outline(PIC_X, PIC_Y, PIC_W, PIC_H, rgb_hex(0x2C2E35), alpha);
        draw_wrapped(about(it), PIC_X, TEXT_Y, PIC_W, 15.5f, C_DIM, alpha);
        return;
    }
    if (it && it->kind == ITEM_ACTION && it->id == ACT_STATS) {
        char lines[12][64];
        int n = combat_stats_lines(lines, 12);
        float y = PIC_Y + 6;
        for (int i = 0; i < n; i++) {
            char *tab_at = strchr(lines[i], '\t');
            if (!tab_at) continue;
            *tab_at = 0;
            if (i == 2) { ui_fill_rect(PIC_X, y + 4, PIC_W, 1, rgb_hex(0x2C2E35), alpha); y += 22; }
            ui_draw_text(lines[i], FONT, 17, 400, 0, PIC_X, y, C_DIM, C_DIM, alpha);
            float vw = ui_text_width(tab_at + 1, FONT_SB, 17, 600, 0);
            ui_draw_text(tab_at + 1, FONT_SB, 17, 600, 0, PIC_X + PIC_W - vw, y, C_TEXT, C_TEXT, alpha);
            y += 38;
        }
        return;
    }
    if (it && it->kind == ITEM_OPTION && preview_supported(it->id) && preview_ready() && !launcher) {
        float ph = PIC_W * 9 / 16.f, py = PIC_Y + (PIC_H - ph) / 2;
        preview_draw(it->id, PIC_X, py, PIC_W, ph, t);
        outline(PIC_X, py, PIC_W, ph, rgb_hex(0x2C2E35), alpha);
    } else {
        draw_picture(frame, frame_w, frame_h, PIC_X, PIC_Y, PIC_W, PIC_H, alpha);
    }
    float y = TEXT_Y;
    if (launcher && it && it->kind == ITEM_ACTION && it->id == ACT_PLAY && game) {
        char line[160];
        snprintf(line, sizeof line, "%s  \xC2\xB7  %s", game->name, game->language);
        ui_draw_text(line, FONT, 16, 400, 0, PIC_X, y, C_TEXT, C_TEXT, alpha);
        y += 28;
        draw_wrapped("Press Enter, A or Start to play. Esc opens the menu in the game.", PIC_X, y, PIC_W, 15.5f, C_DIM, alpha);
        return;
    }
    const char *text = it ? about(it) : NULL;
    if (text) draw_wrapped(text, PIC_X, y, PIC_W, 15.5f, C_DIM, alpha);
}

static void draw_row(int i, float dx, float alpha) {
    const Item *it = &cur()->items[i];
    float y = row_y(i), h = row_h();
    bool focus = selected == i, big = big_rows();
    bool danger = it->kind == ITEM_ACTION && (it->id == ACT_RESET || it->id == ACT_QUIT);
    float px = big ? 25 : 18;
    Rgb lc = focus ? (danger && confirm_pending ? C_RED : C_TEXT) : C_DIM;
    float ty = y + (h - px * 1.33f) / 2;
    ui_draw_text(it->label, FONT, px, focus ? 600 : 400, 0, COL_X + dx, ty, lc, lc, alpha);
    char value[64] = "";
    if (it->kind == ITEM_OPTION) settings_option_text(it->id, value, sizeof value);
    else if (it->kind == ITEM_SLOT) snprintf(value, sizeof value, "Slot %d", settings.state_slot);
    else if (it->kind == ITEM_PAGE && focus) snprintf(value, sizeof value, "\xE2\x80\xBA");   /* a chevron to go in */
    if (!value[0]) return;
    float s = changed_row == i ? ease_out((float)(now - changed_at) / .2f) : 1;
    float slide = (1 - s) * 10 * changed_dir;
    float vpx = big ? 22 : 17;
    Rgb vc = focus ? C_GOLD : rgb_hex(0xA88E5C);
    float vw = ui_text_width(value, FONT, vpx, 400, 0);
    float vx = COL_R - vw, vy = y + (h - vpx * 1.33f) / 2;
    if (it->kind == ITEM_OPTION && it->id == OPT_VOLUME) {
        ui_slider(COL_R - 66 - VOLUME_W + dx, y + h / 2, VOLUME_W, settings.volume / 100.f, focus);
        vx = COL_R - ui_text_width("100%", FONT, vpx, 400, 0);
    }
    if (focus && (it->kind == ITEM_OPTION || it->kind == ITEM_SLOT) && it->id != OPT_VOLUME) {
        /* Selected: arrows either side of the value, to say it can be changed. */
        ui_draw_text("\xE2\x80\xB9", FONT, vpx, 400, 0, vx - 22 + dx, vy, C_FAINT, C_FAINT, alpha);
        ui_draw_text(value, FONT, vpx, 400, 0, vx - 8 + dx + slide, vy, vc, vc, alpha * (.3f + .7f * s));
        ui_draw_text("\xE2\x80\xBA", FONT, vpx, 400, 0, COL_R + 4 + dx, vy, C_FAINT, C_FAINT, alpha);
        return;
    }
    ui_draw_text(value, FONT, vpx, 400, 0, vx + dx + slide, vy, vc, vc, alpha * (.3f + .7f * s));
}

void menu_render(double time_s) {
    float dt = (float)(time_s - last_now);
    if (dt < 0 || dt > .25f) dt = 1 / 60.f;
    last_now = now = time_s;
    if (open_pending) { opened_at = now; page_at = -10; open_pending = false; }
    float t = (float)time_s;
    ui_spring(&hl_y, &hl_v, row_y(selected), dt, 380);
    float in = ease_out((float)(now - opened_at) / .25f);
    if (closing_at >= 0) in *= 1 - ease_out((float)(now - closing_at) / CLOSE_S);

    memcpy(ui_px, backdrop, (size_t)ui_w * ui_h * 4);

    /* The heading: the game's name in the launcher, else where we are, with where we came
     * from over it. */
    float pe = ease_out((float)(now - page_at) / .22f), dx = (1 - pe) * 14 * page_dir, pa = .2f + .8f * pe;
    const Page *p = cur();
    const char *name = game && game->title ? game->title : "CRUSADER OF CENTY";
    bool home = page == PAGE_HOME;
    if (!home) {
        const char *crumb = depth >= 2 ? "SETTINGS" : launcher ? name : "PAUSED";
        ui_draw_text(crumb, FONT_SB, 12.5f, 600, 3, COL_X, HEAD_Y - 26, C_GOLD, C_GOLD, .85f);
    }
    if (home && launcher) ui_draw_text(name, FONT, 34, 300, 5, COL_X + dx, HEAD_Y + 4, C_TEXT, C_TEXT, pa);
    else ui_draw_text(p->title, FONT, 40, 300, 1, COL_X + dx, HEAD_Y, C_TEXT, C_TEXT, pa);
    ui_fill_rect(COL_X, HEAD_Y + 64, 44, 2, C_GOLD, 1);

    /* The selection: a thin gold bar at the column's edge, on a spring. */
    ui_fill_rect(COL_X - 22, hl_y + 10, 3, row_h() - 20, C_GOLD, pa);
    for (int i = 0; i < p->count; i++) {
        float e = ease_out((float)(now - fmax(page_at, opened_at) - i * .02f) / .22f);
        if (e > 0) draw_row(i, (1 - e) * 14 * page_dir, e);
    }

    draw_side(t, pa);

    /* The keys, quietly at the bottom. */
    const char *keys = launcher && home
        ? "\xE2\x86\x91\xE2\x86\x93  select      \xE2\x86\x90\xE2\x86\x92  change      Enter  choose"
        : "\xE2\x86\x91\xE2\x86\x93  select      \xE2\x86\x90\xE2\x86\x92  change      Enter  choose      Esc  back";
    ui_draw_text(keys, FONT, 13, 400, 1, COL_X, 660, C_FAINT, C_FAINT, 1);

    if (now < toast_until) {
        float a = clampf((float)(toast_until - now) / .4f, 0, 1) * ease_out((float)(2.2 - (toast_until - now)) / .15f);
        ui_draw_text(toast, FONT_SB, 15, 600, 0, PIC_X, 660, C_GOLD, C_GOLD, a);
    }
    controls_render(time_s);

    /* Opening and closing: a quick fade from the game's picture and back. */
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
