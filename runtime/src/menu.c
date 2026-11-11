/* The pause menu and the launcher (menu.h): professional, with the game's own colours. The
 * palette comes from its title art and its fields: the sand and peach of the logo's stone,
 * its moss lettering, the deep greens of the forests. The launcher shows the game's real
 * title logo (cut out of the title screen your ROM draws) over a deep forest green with a
 * warm light and a few faint leaves drifting, the menu under it, and on the right the latest
 * save state to continue from. One column of clean text, a sand accent for the selection,
 * and the picture large on the right: the save, a before/after preview, the map, the
 * stats, or what the row does. Pages: home, Settings and its groups; Esc goes back. */
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
       ACT_STATS, ACT_MAP, ACT_PLAY, ACT_ROM, ACT_EXIT, ACT_CONTINUE, ACT_PHOTO, ACT_AUTOLOAD };
typedef struct { ItemKind kind; const char *label; int id; } Item;

/* ---- The pages --------------------------------------------------------------------------- */
enum { PAGE_HOME, PAGE_SETTINGS, PAGE_DISPLAY, PAGE_EFFECTS, PAGE_GAMEPLAY, PAGE_SOUND, PAGE_GENERAL, PAGES };
#define MAX_ROWS 10
typedef struct { const char *title; int count; Item items[MAX_ROWS]; } Page;

static const Page home_pause = { "Paused", 10, {
    { ITEM_ACTION, "Resume", ACT_RESUME }, { ITEM_SLOT, "Save state", ACT_SAVE }, { ITEM_SLOT, "Load state", ACT_LOAD },
    { ITEM_ACTION, "Load autosave", ACT_AUTOLOAD },
    { ITEM_ACTION, "Map", ACT_MAP }, { ITEM_ACTION, "Photo mode", ACT_PHOTO }, { ITEM_ACTION, "Stats", ACT_STATS }, { ITEM_PAGE, "Settings", PAGE_SETTINGS },
    { ITEM_ACTION, "Reset game", ACT_RESET }, { ITEM_ACTION, "Quit game", ACT_QUIT } } };
/* The launcher's home: Continue appears when there is a save state to continue from. */
static const Item launcher_items[] = {
    { ITEM_ACTION, "Continue", ACT_CONTINUE }, { ITEM_ACTION, "New game", ACT_PLAY }, { ITEM_PAGE, "Settings", PAGE_SETTINGS },
    { ITEM_OPTION, "Language", OPT_LANGUAGE }, { ITEM_ACTION, "Choose ROM", ACT_ROM }, { ITEM_ACTION, "Quit", ACT_EXIT } };
static Page home_launcher;
static const Page pages[PAGES] = {
    [PAGE_SETTINGS] = { "Settings", 8, {
        { ITEM_PAGE, "Display", PAGE_DISPLAY }, { ITEM_PAGE, "Effects", PAGE_EFFECTS }, { ITEM_PAGE, "Gameplay", PAGE_GAMEPLAY },
        { ITEM_PAGE, "Sound", PAGE_SOUND }, { ITEM_ACTION, "Controls", ACT_CONTROLS }, { ITEM_PAGE, "General", PAGE_GENERAL },
        { ITEM_ACTION, "Original experience", ACT_PRESET_ORIGINAL }, { ITEM_ACTION, "Enhanced experience", ACT_PRESET_ENHANCED } } },
    [PAGE_DISPLAY] = { "Display", 8, {
        { ITEM_OPTION, "Widescreen", OPT_WIDESCREEN }, { ITEM_OPTION, "Window", OPT_WINDOW_ALL }, { ITEM_OPTION, "Integer scaling", OPT_INTEGER },
        { ITEM_OPTION, "Internal resolution", OPT_INTERNAL }, { ITEM_OPTION, "Scaler", OPT_SCALER },
        { ITEM_OPTION, "Filter", OPT_FILTER }, { ITEM_OPTION, "Sharpening", OPT_SHARPEN },
        { ITEM_OPTION, "Sprite flicker fix", OPT_FLICKER } } },
    [PAGE_EFFECTS] = { "Effects", 5, {
        { ITEM_OPTION, "Lighting", OPT_LIGHTING }, { ITEM_OPTION, "HDR look", OPT_HDR }, { ITEM_OPTION, "Hit effects", OPT_PARTICLES },
        { ITEM_OPTION, "Water", OPT_WATER },
        { ITEM_OPTION, "Texture pack", OPT_TEXPACK } } },
    [PAGE_GAMEPLAY] = { "Gameplay", 10, {
        { ITEM_OPTION, "Difficulty", OPT_DIFFICULTY }, { ITEM_OPTION, "Infinite health", OPT_CHEAT_HEALTH }, { ITEM_OPTION, "Walking speed", OPT_WALK },
        { ITEM_OPTION, "Combat info", OPT_COMBAT }, { ITEM_OPTION, "Rumble", OPT_RUMBLE }, { ITEM_OPTION, "Play timer", OPT_TIMER },
        { ITEM_OPTION, "Autosave", OPT_AUTOSAVE }, { ITEM_OPTION, "Fast-forward", OPT_FAST }, { ITEM_OPTION, "Rewind", OPT_REWIND_ALL },
        { ITEM_OPTION, "Slowdown fix", OPT_OVERCLOCK } } },
    [PAGE_SOUND] = { "Sound", 2, { { ITEM_OPTION, "Volume", OPT_VOLUME }, { ITEM_OPTION, "Audio", OPT_AUDIO } } },
    [PAGE_GENERAL] = { "General", 2, {
        { ITEM_OPTION, "Language", OPT_LANGUAGE }, { ITEM_OPTION, "Launcher at startup", OPT_LAUNCHER } } },
};

/* What each row does, for the text under the picture. */
static const char *about(const Item *it) {
    if (it->kind == ITEM_PAGE)
        switch (it->id) {
        case PAGE_SETTINGS: return "Picture, effects, gameplay, sound and controls.";
        case PAGE_DISPLAY: return "The window, the internal resolution and how the pixels are drawn.";
        case PAGE_EFFECTS: return "Lighting, the HDR look, hit effects, living water and texture packs. Only the picture changes.";
        case PAGE_GAMEPLAY: return "Difficulty, combat info, rewind, fast-forward and the slowdown fix.";
        case PAGE_SOUND: return "The volume and how the console's sound is output.";
        case PAGE_GENERAL: return "The language you play in, and whether this launcher comes first.";
        }
    if (it->kind == ITEM_ACTION)
        switch (it->id) {
        case ACT_RESUME: return "Back to the game.";
        case ACT_AUTOLOAD: return "Goes back to the last autosave, made each time the story moves on (Gameplay > Autosave).";
        case ACT_PHOTO: return "Freezes the game for a picture: hide the HUD, then save it at the internal resolution with every effect. F9 in play.";
        case ACT_RESET: return "Starts the game over from the beginning. Unsaved progress is lost.";
        case ACT_QUIT: return "Closes the game. Unsaved progress is lost; the game's own saves stay.";
        case ACT_EXIT: return "Closes the game.";
        case ACT_CONTROLS: return "The keyboard key and controller button for every action.";
        case ACT_MAP: return "Fills in as you explore, with your place blinking. Doors you have been through are marked with the area they lead to; \xE2\x86\x90 \xE2\x86\x92 look at the other areas.";
        case ACT_PRESET_ORIGINAL: return "Everything as the cartridge was: native resolution, no effects, the original sprite limit, speed and difficulty. Controls, sound and saves stay as they are.";
        case ACT_PRESET_ENHANCED: return "The recommended enhancements: the art redrawn at 3x with light sharpening, a soft glow, the HDR look, hit effects, living water, no sprite flicker and no slowdown.";
        case ACT_ROM: return "Pick a ROM of Crusader of Centy (USA), or Ragnacenty from Japan or Korea. ROMs in the rom folder next to the game are found by themselves.";
        case ACT_PLAY: return NULL;
        case ACT_CONTINUE: return NULL;
        }
    if (it->kind == ITEM_SLOT) return NULL;
    switch (it->id) {
    case OPT_WIDESCREEN: return "Shows more of the world at the sides while you explore, drawn by the game's own map code. Title, story and menu screens keep the original 4:3 picture with black sides.";
    case OPT_WINDOW_ALL: return "Fullscreen, or a window of this many times the game's size. F11 switches at any time.";
    case OPT_INTEGER: return "Scales by whole numbers only, so every game pixel is the same size.";
    case OPT_INTERNAL: return "Draws each layer at a higher resolution, rounding the edges of the pixel art. Native is the original look. 3-4x suits 1080p, 6x 1440p and 4K; 8x to 16x (up to 3584 lines) supersample for the sharpest picture and need a fast many-core CPU.";
    case OPT_SCALER: return "Edge rounds corners, Smooth (xBR) follows curves and slopes, Sharp keeps every pixel square, exactly as drawn.";
    case OPT_FILTER: return "Scanlines, a curved CRT tube or an LCD grid over the picture.";
    case OPT_SHARPEN: return "Crisper edges after scaling, without halos.";
    case OPT_FLICKER: return "Lifts the hardware's limit of sprites on one line, so crowded scenes no longer flicker.";
    case OPT_LIGHTING: return "Bright spots glow softly into their surroundings.";
    case OPT_HDR: return "Deeper shadows, brighter highlights and richer colour. Strong goes further.";
    case OPT_PARTICLES: return "Sparks fly from every hit and defeated enemies burst into embers and leaves. Lots throws more.";
    case OPT_WATER: return "Streams, lakes and the sea swell, foam along the shore and around anyone standing in them, and reflect what is above them.";
    case OPT_TEXPACK: return "High-resolution replacements for the game's tiles, from a folder in texture_packs next to the game.";
    case OPT_DUMP: return "Writes every tile shown to texture_packs/dump: the starting point for making a pack.";
    case OPT_DIFFICULTY: return "How much each hit takes from your apples: Original, Easy (half), Hard (one and a half) or Very hard (twice).";
    case OPT_CHEAT_HEALTH: return "Hits take nothing, and your apples stay full.";
    case OPT_COMBAT: return "The damage of every hit rises from the enemy as a number; with bars, a health bar shows over an enemy after each hit.";
    case OPT_RUMBLE: return "The controller rumbles when you land a hit, harder for a defeat and when you are hit.";
    case OPT_WALK: return "How fast you walk while steering: Original, Faster (a quarter more) or Fastest (half again). Scenes keep their own pace.";
    case OPT_AUTOSAVE: return "Saves a state to its own slot each time the story moves on. Load it from the pause menu, or Continue from the launcher.";
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

/* From the title art and the fields: the logo's sand and peach stone, its moss lettering,
 * the forests' deep greens. */
#define C_TEXT  rgb_hex(0xF5F1E4)
#define C_DIM   rgb_hex(0x97A08F)
#define C_FAINT rgb_hex(0x5E695C)
#define C_GOLD  rgb_hex(0xEBC98F)       /* sand: the accent */
#define C_PEACH rgb_hex(0xE8A27A)
#define C_MOSS  rgb_hex(0x8DB25A)
#define C_RED   rgb_hex(0xF08A78)
#define C_BG    rgb_hex(0x0D1511)
#define C_LINE  rgb_hex(0x2A352D)
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
static uint32_t *logo;                  /* the title logo cut out of the title screen (0 = clear) */
static int logo_w, logo_h;
static SlotInfo latest;                 /* the newest save state (the launcher's Continue) */
static int latest_slot;
static int confirm_item;

#define CLOSE_S .18

static const Page *cur(void) { return page == PAGE_HOME ? (launcher ? &home_launcher : &home_pause) : &pages[page]; }
static bool big_rows(void) { return page == PAGE_HOME || page == PAGE_SETTINGS; }
/* The launcher's home starts under the logo. */
static float list_y(void) { return launcher && page == PAGE_HOME ? 300 : LIST_Y; }
/* Rows are closer together on a page that would otherwise run into the key hints. */
static float row_h(void) {
    float h = big_rows() ? 52 : 44, fit = (648 - list_y()) / (float)(cur()->count > 0 ? cur()->count : 1);
    return h < fit ? h : fit;
}
static float row_y(int i) { return list_y() + i * row_h(); }
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

/* The title logo: the lit part of the title screen's top (its name over the stone tablet),
 * cropped to what is drawn, with black made clear. */
static void cut_logo(void) {
    free(logo);
    logo = NULL;
    logo_w = logo_h = 0;
    if (!frame || frame_w < 64 || frame_h < 140) return;
    int x0 = frame_w, y0 = 140, x1 = -1, y1 = -1;
    for (int y = 4; y < 132; y++)
        for (int x = 0; x < frame_w; x++) {
            uint32_t p = frame[y * frame_w + x];
            if (((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255) < 60) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < x0 || y1 < y0 || x1 - x0 < 40) return;
    logo_w = x1 - x0 + 1;
    logo_h = y1 - y0 + 1;
    logo = malloc((size_t)logo_w * logo_h * 4);
    if (!logo) { logo_w = logo_h = 0; return; }
    for (int y = 0; y < logo_h; y++)
        for (int x = 0; x < logo_w; x++) {
            uint32_t p = frame[(y0 + y) * frame_w + x0 + x];
            logo[y * logo_w + x] = ((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255) < 60 ? 0 : (p | 0xFF000000u);
        }
}

/* The newest save state of this version, for Continue; the home page lists it first. */
static void find_latest(void) {
    savestate_info_free(&latest);
    latest_slot = 0;
    time_t best = 0;
    for (int k = 1; k <= AUTOSAVE_SLOT; k++) {
        SlotInfo info = { 0 };
        if (savestate_info(k, &info) && info.exists && info.saved_at > best) {
            savestate_info_free(&latest);
            latest = info;
            latest_slot = k;
            best = info.saved_at;
        } else savestate_info_free(&info);
    }
    int skip = latest_slot ? 0 : 1;
    home_launcher.title = NULL;
    home_launcher.count = (int)(sizeof launcher_items / sizeof *launcher_items) - skip;
    memcpy(home_launcher.items, launcher_items + skip, sizeof(Item) * (size_t)home_launcher.count);
}

int menu_continue_slot(void) { return latest_slot; }

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
    if (launcher) {
        /* Deep forest green, lighter toward a warm light at the top right, a soft vignette. */
        for (int y = 0; y < out_h; y++)
            for (int x = 0; x < out_w; x++) {
                float u = (x + .5f - ui_ox) / ui_s, v = (y + .5f - ui_oy) / ui_s;
                Rgb c = mix(rgb_hex(0x16241B), rgb_hex(0x0A100D), clampf(v / 720, 0, 1));
                float dx = u - 1080, dy = v + 60, d = sqrtf(dx * dx + dy * dy);
                c = add(c, rgb_hex(0xE9C48C), .10f * expf(-d / 520));
                float vig = sqrtf((u - 640) * (u - 640) / (720.f * 720.f) + (v - 360) * (v - 360) / (420.f * 420.f));
                c = scale(c, 1 - .35f * smooth(.6f, 1.3f, vig));
                c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * out_w + x)) - .5f) * 2.5f / 255);
                backdrop[y * out_w + x] = pack(c);
            }
    } else {
        blur_rgb(backdrop, out_w, out_h, (int)(14 * ui_s));
        Rgb bg = C_BG;
        for (int y = 0; y < out_h; y++)
            for (int x = 0; x < out_w; x++) {
                float u = (x - ui_ox) / ui_s;
                float k = .22f + .1f * smooth(560, 900, u);
                Rgb c = mix(bg, unpack(backdrop[y * out_w + x]), k);
                c = add(c, rgb(1, 1, 1), (hashf((uint32_t)(y * out_w + x)) - .5f) * 2.f / 255);
                backdrop[y * out_w + x] = pack(c);
            }
    }
    if (launcher) {
        cut_logo();
        find_latest();
    }
}

void menu_open(const uint32_t *fb, int fb_w, int fb_h, int fb_pitch, int game_x, int game_y, int game_w, int game_h,
               int out_w, int out_h, float window_to_pixels) {
    if (launcher) open_ = false;
    launcher = false;
    automap_view_reset();
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
        case ACT_PHOTO: sound(1); return MENU_PHOTO;
        case ACT_AUTOLOAD:
            if (savestate_load(AUTOSAVE_SLOT)) { sound(1); return MENU_LOADED; }
            notify("No autosave yet");
            sound(3);
            return MENU_NONE;
        case ACT_PLAY: sound(1); return MENU_PLAY;
        case ACT_CONTINUE: sound(1); return MENU_CONTINUE;
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
        case SDL_CONTROLLER_BUTTON_START: if (launcher) return latest_slot ? MENU_CONTINUE : MENU_PLAY; return depth ? back() : MENU_CLOSE;
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
    if (dir && item_at(selected) && item_at(selected)->kind == ITEM_ACTION) {
        if (item_at(selected)->id == ACT_MAP) { automap_view_step(dir); sound(0); }
        return MENU_NONE;
    }
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
    outline(x, y, pw, ph, C_LINE, alpha);
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
    if (it && it->kind == ITEM_ACTION && it->id == ACT_CONTINUE) {
        draw_picture(latest.thumb, latest.w, latest.h, PIC_X, PIC_Y, PIC_W, PIC_H, alpha);
        char when[64], line[128];
        strftime(when, sizeof when, "%d %b %Y, %H:%M", localtime(&latest.saved_at));
        if (latest_slot == AUTOSAVE_SLOT) snprintf(line, sizeof line, "Autosave  \xC2\xB7  %s", when);
        else snprintf(line, sizeof line, "Save state %d  \xC2\xB7  %s", latest_slot, when);
        ui_draw_text(line, FONT, 16, 400, 0, PIC_X, TEXT_Y, C_TEXT, C_TEXT, alpha);
        draw_wrapped("Picks up where you left off. F2 in the game saves a new state, and the game autosaves as the story moves on; this always offers the newest.", PIC_X, TEXT_Y + 28, PIC_W, 15.5f, C_DIM, alpha);
        return;
    }
    if (it && it->kind == ITEM_SLOT) { draw_slot_picture(alpha); return; }
    if (it && it->kind == ITEM_ACTION && it->id == ACT_MAP) {
        ui_fill_rect(PIC_X, PIC_Y, PIC_W, PIC_H, rgb_hex(0x080C0A), alpha);
        if (!automap_draw(PIC_X + 8, PIC_Y + 8, PIC_W - 16, PIC_H - 16, t, alpha)) {
            const char *none = "Nothing of this map seen yet.";
            float w = ui_text_width(none, FONT, 16, 400, 0);
            ui_draw_text(none, FONT, 16, 400, 0, PIC_X + (PIC_W - w) / 2, PIC_Y + PIC_H / 2 - 10, C_DIM, C_DIM, alpha);
        }
        outline(PIC_X, PIC_Y, PIC_W, PIC_H, C_LINE, alpha);
        AutomapInfo mi;
        automap_info(&mi);
        char line[128], sub[128];
        snprintf(line, sizeof line, "Area %d%s", mi.map, mi.current ? "  \xC2\xB7  you are here" : "");
        ui_draw_text(line, FONT_SB, 16, 600, 0, PIC_X, TEXT_Y, C_TEXT, C_TEXT, alpha);
        if (mi.areas > 1) {
            snprintf(sub, sizeof sub, "\xE2\x86\x90 %d of %d \xE2\x86\x92", mi.index, mi.areas);
            float sw = ui_text_width(sub, FONT, 15, 400, 0);
            ui_draw_text(sub, FONT, 15, 400, 0, PIC_X + PIC_W - sw, TEXT_Y + 1, C_DIM, C_DIM, alpha);
        }
        int n = 0;
        if (mi.percent >= 0) n += snprintf(sub + n, sizeof sub - n, "%d%% explored", mi.percent);
        if (mi.doors) n += snprintf(sub + n, sizeof sub - n, "%s%d door%s found", n ? "  \xC2\xB7  " : "", mi.doors, mi.doors == 1 ? "" : "s");
        if (!n) snprintf(sub, sizeof sub, "Not fully known yet");
        ui_draw_text(sub, FONT, 15, 400, 0, PIC_X, TEXT_Y + 25, C_DIM, C_DIM, alpha);
        draw_wrapped(about(it), PIC_X, TEXT_Y + 52, PIC_W, 15.5f, C_DIM, alpha);
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
            if (i == 2) { ui_fill_rect(PIC_X, y + 2, PIC_W, 1, C_LINE, alpha); y += 14; }
            ui_draw_text(lines[i], FONT, 16, 400, 0, PIC_X, y, C_DIM, C_DIM, alpha);
            float vw = ui_text_width(tab_at + 1, FONT_SB, 16, 600, 0);
            ui_draw_text(tab_at + 1, FONT_SB, 16, 600, 0, PIC_X + PIC_W - vw, y, C_TEXT, C_TEXT, alpha);
            y += 29;
        }
        /* The bestiary: each enemy type defeated, its picture and how many, most first. */
        y += 10;
        ui_draw_text("BESTIARY", FONT_SB, 12, 600, 2.5f, PIC_X, y, C_GOLD, C_GOLD, alpha);
        y += 26;
        int nb = combat_beast_count();
        if (!nb) {
            draw_wrapped("Enemies you defeat appear here, with how many of each.", PIC_X, y, PIC_W, 15.5f, C_FAINT, alpha);
            return;
        }
        const float cell = 68, gap = (PIC_W - 8 * cell) / 7;
        for (int i = 0; i < nb && i < 16; i++) {
            uint32_t count; int bw, bh; const uint32_t *px;
            if (!combat_beast(i, &count, &bw, &bh, &px)) break;
            float cx = PIC_X + (i % 8) * (cell + gap), cy = y + (i / 8) * (cell + 30);
            ui_panel(cx, cy, cell, cell, 8, rgb_hex(0x16211A), rgb_hex(0x111A14), alpha, C_LINE, alpha);
            if (bw && bh) {
                int k = (int)((cell - 12) / (bw > bh ? bw : bh));
                if (k < 1) k = 1;
                float ox = cx + (cell - bw * k) / 2, oy = cy + (cell - bh * k) / 2;
                for (int py = 0; py < bh; py++)
                    for (int pxx = 0; pxx < bw; pxx++) {
                        uint32_t c = px[py * BEAST_PX + pxx];
                        if (c >> 24) ui_fill_rect(ox + pxx * k, oy + py * k, (float)k, (float)k, rgb_hex(c & 0xFFFFFF), alpha);
                    }
            } else ui_draw_text("?", FONT_SB, 24, 600, 0, cx + cell / 2 - 6, cy + cell / 2 - 16, C_FAINT, C_FAINT, alpha);
            char num[16];
            snprintf(num, sizeof num, "\xC3\x97%u", count);
            float nw = ui_text_width(num, FONT_SB, 14, 600, 0);
            ui_draw_text(num, FONT_SB, 14, 600, 0, cx + (cell - nw) / 2, cy + cell + 5, C_TEXT, C_TEXT, alpha);
        }
        return;
    }
    if (it && it->kind == ITEM_OPTION && preview_supported(it->id) && preview_ready() && !launcher) {
        float ph = PIC_W * 9 / 16.f, py = PIC_Y + (PIC_H - ph) / 2;
        preview_draw(it->id, PIC_X, py, PIC_W, ph, t);
        outline(PIC_X, py, PIC_W, ph, C_LINE, alpha);
    } else {
        /* In the launcher the newest save stands for the game (the logo is already on the left). */
        if (launcher && latest_slot && !(it && it->kind == ITEM_ACTION && it->id == ACT_PLAY))
            draw_picture(latest.thumb, latest.w, latest.h, PIC_X, PIC_Y, PIC_W, PIC_H, alpha);
        else draw_picture(frame, frame_w, frame_h, PIC_X, PIC_Y, PIC_W, PIC_H, alpha);
    }
    float y = TEXT_Y;
    if (launcher && it && it->kind == ITEM_ACTION && it->id == ACT_PLAY && game) {
        char line[160];
        snprintf(line, sizeof line, "%s  \xC2\xB7  %s", game->name, game->language);
        ui_draw_text(line, FONT, 16, 400, 0, PIC_X, y, C_TEXT, C_TEXT, alpha);
        y += 28;
        draw_wrapped("Starts the game from the beginning, as the cartridge does. Your saves stay as they are.", PIC_X, y, PIC_W, 15.5f, C_DIM, alpha);
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

/* The title logo, crisp, with a soft shadow; the game's name under it in small capitals. */
static void draw_logo(float t) {
    (void)t;
    if (!logo) {
        const char *name = game && game->title ? game->title : "CRUSADER OF CENTY";
        ui_draw_text(name, FONT, 34, 300, 5, COL_X, HEAD_Y + 4, C_TEXT, C_TEXT, 1);
        ui_fill_rect(COL_X, HEAD_Y + 64, 44, 2, C_GOLD, 1);
        return;
    }
    /* Its pixels are a little wide on a 256-pixel screen (4:3 over 256 x 224). */
    float par = frame_w == 256 ? 1.1667f : 1, sy = 460.f / (logo_w * par), sx = sy * par;
    if (logo_h * sy > 190) { sy = 190.f / logo_h; sx = sy * par; }
    float x0 = COL_X - 4, y0 = 64;
    int px0 = (int)ui_x(x0), py0 = (int)ui_y(y0), px1 = (int)ui_x(x0 + logo_w * sx), py1 = (int)ui_y(y0 + logo_h * sy);
    for (int pass = 0; pass < 2; pass++) {
        int off = pass ? 0 : (int)(5 * ui_s);
        for (int py = py0; py < py1; py++) {
            int ly = (int)((float)(py - py0) / (py1 - py0) * logo_h);
            for (int px = px0; px < px1; px++) {
                uint32_t p = logo[ly * logo_w + (int)((float)(px - px0) / (px1 - px0) * logo_w)];
                if (!p) continue;
                if (pass == 0) ui_blend_px(px + off, py + off, rgb(0, 0, 0), .45f);
                else ui_blend_px(px, py, unpack(p & 0xFFFFFF), 1);
            }
        }
    }
    if (game && game->language) {
        char line[96];
        snprintf(line, sizeof line, "%s", game->name);
        ui_draw_text(line, FONT_SB, 12.5f, 600, 3, COL_X, y0 + logo_h * sy + 22, C_GOLD, C_GOLD, .85f);
    }
}

/* A leaf, size long, pointing at angle a: two arcs, a vein, a little lighter on one side. */
static void leaf(float cx, float cy, float size, float a, Rgb c, float alpha) {
    float ca = cosf(a), sa = sinf(a), half = size / 2, wid = size * .26f;
    int r = (int)(half * ui_s) + 2, pcx = (int)ui_x(cx), pcy = (int)ui_y(cy);
    for (int py = pcy - r; py <= pcy + r; py++)
        for (int px = pcx - r; px <= pcx + r; px++) {
            float u = (px + .5f - ui_ox) / ui_s - cx, v = (py + .5f - ui_oy) / ui_s - cy;
            float along = u * ca + v * sa, across = -u * sa + v * ca, k = along / half;
            if (k < -1 || k > 1) continue;
            float d = fabsf(across) - wid * sqrtf(1 - k * k) * (1 - .25f * k);
            if (d > 1 / ui_s) continue;
            Rgb col = across < 0 ? scale(c, .8f) : c;
            if (fabsf(across) < .45f && k < .8f) col = scale(c, .6f);
            ui_blend_px(px, py, col, clampf(.5f - d * ui_s, 0, 1) * alpha);
        }
}

/* A few leaves drifting slowly down through the launcher, faint, behind everything. */
static void draw_leaves(float t) {
    static const uint32_t colors[4] = { 0x6E9A48, 0x8DB25A, 0xC9A35A, 0x5C8040 };
    for (int k = 0; k < 9; k++) {
        float period = 22 + hashf((uint32_t)k * 97u) * 14;
        float u = fmodf(t / period + hashf((uint32_t)k * 13u), 1);
        float x = 600 + hashf((uint32_t)k * 31u) * 700 + u * 120 + sinf(t * .5f + k) * 28;
        float y = -30 + u * 790;
        float spin = t * (.5f + hashf((uint32_t)k * 19u) * .6f) * (k & 1 ? 1 : -1) + k;
        float turn = .55f + .45f * fabsf(cosf(spin * .7f));
        float fade = smooth(0, .08f, u) * (1 - smooth(.9f, 1, u));
        leaf(x, y, (16 + hashf((uint32_t)k * 41u) * 10) * turn, spin, rgb_hex(colors[k % 4]), .16f * fade);
    }
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
    if (home && launcher) draw_logo(t);
    else {
        ui_draw_text(p->title, FONT, 40, 300, 1, COL_X + dx, HEAD_Y, C_TEXT, C_TEXT, pa);
        ui_fill_rect(COL_X, HEAD_Y + 64, 44, 2, C_GOLD, 1);
    }
    if (launcher) draw_leaves(t);

    /* The selection: a faint band across the row and a sand bar at its edge, on a spring. */
    ui_fill_rect(COL_X - 22, hl_y + 4, COL_R - COL_X + 46, row_h() - 8, C_GOLD, .05f * pa);
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
