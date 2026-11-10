/* Crusader of Centy Recomp: window, input, frame pacing.
 *   CrusaderOfCenty.exe [rom path]                 play
 *   CrusaderOfCenty.exe --headless N --shot F.bmp  run N frames without a window, save the last frame
 *   headless extras: --press "FRAME:BUTTONS[:FRAMES],..." --shots EVERY DIR */
#include "genesis.h"
#include "game.h"
#include "settings.h"
#include "png.h"
#include "cheats.h"
#include "hd.h"
#include "gpu.h"
#include "filter.h"
#include "input.h"
#include "pads.h"
#include "controls.h"
#include "preview.h"
#include "recomp_rt.h"
#include "texpack.h"
#include "menu.h"
#include "savestate.h"
#include "ui.h"
#include "text.h"
#include <SDL.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FB_W (320 + MAX_EXTRA_WIDTH * 2)
#define FB_H 240

static uint32_t framebuffer[FB_W * FB_H];
extern uint8_t coverage[ROM_SIZE / 16];
extern uint32_t *pc_profile;

/* Prints the most executed instructions of the last frames, to find busy-wait loops. */
static void print_pc_profile(void) {
    for (int k = 0; k < 12; k++) {
        uint32_t best = 0, at = 0;
        for (uint32_t i = 0; i < ROM_SIZE / 2; i++) if (pc_profile[i] > best) { best = pc_profile[i]; at = i; }
        if (!best) break;
        printf("  $%06X  %u\n", at * 2, best);
        pc_profile[at] = 0;
    }
}

/* ---- Scripted input for headless runs ------------------------------------------
 * --press "FRAME:BUTTONS[:FRAMES],..." holds BUTTONS (letters U D L R A B C S) from
 * FRAME for FRAMES frames (default 4). Several entries may overlap. */
typedef struct { int frame, len; uint16_t bits; } Press;
static Press presses[256];
static int press_count;

static void parse_presses(const char *spec) {
    while (*spec && press_count < (int)(sizeof presses / sizeof *presses)) {
        Press p = { atoi(spec), 4, 0 };
        const char *c = strchr(spec, ':');
        if (!c) break;
        for (c++; *c && *c != ':' && *c != ','; c++) {
            switch (*c) {
            case 'U': p.bits |= PAD_UP; break;    case 'D': p.bits |= PAD_DOWN; break;
            case 'L': p.bits |= PAD_LEFT; break;  case 'R': p.bits |= PAD_RIGHT; break;
            case 'A': p.bits |= PAD_A; break;     case 'B': p.bits |= PAD_B; break;
            case 'C': p.bits |= PAD_C; break;     case 'S': p.bits |= PAD_START; break;
            }
        }
        if (*c == ':') p.len = atoi(++c);
        presses[press_count++] = p;
        const char *next = strchr(c, ',');
        if (!next) break;
        spec = next + 1;
    }
}

static uint16_t scripted_pad(int frame) {
    uint16_t bits = 0;
    for (int i = 0; i < press_count; i++)
        if (frame >= presses[i].frame && frame < presses[i].frame + presses[i].len) bits |= presses[i].bits;
    return bits;
}

static char base_dir[1024];

static void path_in_base(char *out, size_t n, const char *name) { snprintf(out, n, "%s%s", base_dir, name); }
/* A file of the version loaded (game_file_name), next to the executable. */
static void game_path_in_base(char *out, size_t n, const char *name) {
    char own[128];
    game_file_name(own, sizeof own, name);
    path_in_base(out, n, own);
}

static void save_coverage(void) {
    char p[1100];
    game_path_in_base(p, sizeof p, "coverage.bin");
    /* Merge with earlier runs so coverage accumulates across play sessions. */
    FILE *f = fopen(p, "rb");
    if (f) {
        uint8_t old[sizeof coverage];
        if (fread(old, 1, sizeof old, f) == sizeof old)
            for (size_t i = 0; i < sizeof old; i++) coverage[i] |= old[i];
        fclose(f);
    }
    f = fopen(p, "wb");
    if (f) { fwrite(coverage, 1, sizeof coverage, f); fclose(f); }
}

static void save_sram(void) {
    if (!bus.sram_dirty) return;
    char p[1100];
    game_path_in_base(p, sizeof p, "save.srm");
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(bus.sram, 1, SRAM_SIZE, f); fclose(f); bus.sram_dirty = false; }
}

static void load_sram(void) {
    char p[1100];
    game_path_in_base(p, sizeof p, "save.srm");
    memset(bus.sram, 0xFF, SRAM_SIZE);
    FILE *f = fopen(p, "rb");
    if (f) { if (fread(bus.sram, 1, SRAM_SIZE, f) != SRAM_SIZE) { /* keep what was read */ } fclose(f); }
}

static int frame_width(void) { return vdp_screen_width() + vdp_opts.extra_width * 2; }

static int save_bmp(const char *path) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(framebuffer, frame_width(), vdp_screen_height(), 32,
                                                        FB_W * 4, SDL_PIXELFORMAT_XRGB8888);
    if (!s) return -1;
    int r = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return r;
}

static int save_png(const char *path) { return png_write(path, framebuffer, frame_width(), vdp_screen_height(), FB_W); }

/* Saves as PNG or BMP depending on the file extension. */
static int save_shot(const char *path) {
    size_t n = strlen(path);
    return n > 4 && !strcmp(path + n - 4, ".png") ? save_png(path) : save_bmp(path);
}

/* The pause menu changed the language: save this version's save RAM, load the other
 * version's ROM, with its own save RAM. False if the language did not change, or its ROM
 * would not load (the language goes back). */
static bool switch_version(void) {
    if (!game || !strcmp(game->id, settings.language)) return false;
    save_sram();
    const char *err = bus_load_rom(settings.rom_path);
    if (err) {                          /* the ROM and `game` are as they were */
        fprintf(stderr, "could not switch to %s: %s\n", settings.rom_path, err);
        for (int i = 0; i < GAME_VERSION_COUNT; i++)
            if (game_versions[i] == game) settings_use_rom(settings.rom_paths[i]);
        return false;
    }
    load_sram();
    return true;
}

static void apply_settings(void) {
    vdp_opts.extra_width = 0;           /* widescreen needs the game's map code patched: not for this game yet */
    vdp_opts.no_sprite_limit = settings.no_sprite_limit;
    vdp_opts.hd_capture = true;         /* the layers are also what the menu's previews draw from */
    machine_opts.overclock = settings.overclock;
    cpu_has_avx2 = SDL_HasAVX2();
    hd_avx2 = settings.avx2 && cpu_has_avx2;
    hd_merged = settings.merged_pass;
    sound_quality = settings.audio;
    hd_scaler = settings.scaler;
    hd_water = false;                   /* made for Beyond Oasis's water colours */
    /* Texture packs: load a pack when it changes (it can be many images), dump on request. */
    static char loaded_pack[64] = "";
    static bool pack_tried;
    if (!pack_tried || strcmp(loaded_pack, settings.texture_pack)) {
        int n = texpack_load(base_dir, settings.texture_pack);
        if (settings.texture_pack[0]) fprintf(stderr, "texture pack %s: %d images\n", settings.texture_pack, n);
        snprintf(loaded_pack, sizeof loaded_pack, "%s", settings.texture_pack);
        pack_tried = true;
    }
    texpack_dump(base_dir, settings.dump_tiles);
    vdp_opts.tile_refs = texpack_active();
    hd_texpack = texpack_count() > 0;
}

/* Largest rectangle with the game's display aspect that fits the window. */
/* The window for Window N×: N times the game's size, or the largest whole N that fits the
 * screen's usable area (without the taskbar, and leaving room for the title bar), so a big
 * N on a small screen is never cut off. */
static void window_size(int *w, int *h) {
    SDL_Rect area;
    int room_w = 1 << 30, room_h = 1 << 30;
    if (SDL_GetDisplayUsableBounds(0, &area) == 0) { room_w = area.w; room_h = area.h - 40; }
    int s = settings.window_scale;
    while (s > 1 && (frame_width() * s * 15 / 14 > room_w || 224 * s > room_h)) s--;
    *w = frame_width() * s * 15 / 14;
    *h = 224 * s;
}

/* ---- Speed: auto quality and the stats panel ------------------------------------------- */
static const int quality_steps[5] = { 1, 2, 3, 4, 6 };
static int quality_scale;               /* auto quality's internal resolution (0: the setting's) */

static struct {
    Uint64 last;                        /* when the last picture was done */
    double interval, work;              /* smoothed: ms between pictures, ms of work in one */
    int slow, roomy;                    /* pictures in a row behind the display / with room to spare */
    double layers, effects, upload;     /* the stats panel's sums since it was made */
    double game, rewind_ms, present, draw;   /* and the game frames', rewind's, drawing's and waiting for the display's */
    int frames;                         /* game frames since it was made */
    int pictures, scale, hz;
    Uint64 since;
    bool gpu, dirty;
} speed;

/* The step of quality_steps below (dir -1) or above (+1) `scale`. */
static int quality_step(int scale, int dir) {
    int k = 0;
    while (k < 4 && quality_steps[k + 1] <= scale) k++;
    k += dir;
    return quality_steps[k < 0 ? 0 : k > 4 ? 4 : k];
}

/* Called after each picture with the time its steps took (ms). */
static void speed_picture(SDL_Window *win, double layers, double effects, double upload, bool gpu, int scale) {
    Uint64 now = SDL_GetPerformanceCounter(), freq = SDL_GetPerformanceFrequency();
    SDL_DisplayMode mode;
    int hz = SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(win), &mode) == 0 && mode.refresh_rate > 0 ? mode.refresh_rate : 60;
    double work = layers + effects + upload, budget = 1000.0 / hz;
    if (speed.last) {
        double gap = (double)(now - speed.last) * 1000 / freq;
        speed.interval = speed.interval > 0 ? speed.interval * .9 + gap * .1 : gap;
    }
    speed.last = now;
    speed.work = speed.work > 0 ? speed.work * .9 + work * .1 : work;
    /* Auto quality: a step down after about 3/4 of a second behind the display while the
     * work fills the time; a step back up (never past the setting) once the next step's
     * work, which grows with its pixels, would leave a third of the time free, for 3 s. */
    if (settings.auto_quality) {
        int setting = settings.internal_scale, cur = quality_scale ? quality_scale : setting;
        if (cur > setting) cur = setting;
        speed.slow = speed.interval > budget * 1.15 && speed.work > budget * .75 ? speed.slow + 1 : 0;
        int up = quality_step(cur, 1);
        if (up > setting) up = setting;
        speed.roomy = cur < setting && speed.work * up * up / ((double)cur * cur) < budget * .65 ? speed.roomy + 1 : 0;
        int next = cur;
        if (speed.slow > hz * 3 / 4 && cur > 1) next = quality_step(cur, -1);
        else if (speed.roomy > hz * 3) next = up;
        if (next != cur) speed.slow = speed.roomy = 0, speed.work = speed.interval = 0, speed.last = 0;
        quality_scale = next;
    } else quality_scale = 0;
    /* The stats panel: sums, made into its lines twice a second. */
    speed.layers += layers; speed.effects += effects; speed.upload += upload;
    speed.pictures++;
    speed.gpu = gpu; speed.scale = scale; speed.hz = hz;
    if (!speed.since) speed.since = now;
    if (now - speed.since >= freq / 2) speed.dirty = true;
}

/* The panel in the top-left corner: pictures a second, the resolution, each step's time. */
static void draw_stats(SDL_Renderer *ren, int win_w, int win_h) {
    static SDL_Texture *panel;
    static int pw, ph;
    if (speed.dirty && speed.pictures) {
        Uint64 freq = SDL_GetPerformanceFrequency();
        double secs = (double)(SDL_GetPerformanceCounter() - speed.since) / freq, n = speed.pictures;
        char lines[4][160];
        snprintf(lines[0], sizeof lines[0], "%.0f pictures a second (display %d Hz)", n / secs, speed.hz);
        if (settings.auto_quality && speed.scale < settings.internal_scale)
            snprintf(lines[1], sizeof lines[1], "%dx internal resolution (auto quality; set to %dx)", speed.scale, settings.internal_scale);
        else snprintf(lines[1], sizeof lines[1], "%dx internal resolution%s", speed.scale, settings.auto_quality ? " (auto quality)" : "");
        char where[48];
        if (speed.gpu && gpu_fx_card_ms > 0) snprintf(where, sizeof where, "GPU, %.1f ms on the card", gpu_fx_card_ms);
        else snprintf(where, sizeof where, "%s", speed.gpu ? "GPU" : hd_avx2 ? "CPU, AVX2" : "CPU");
        snprintf(lines[2], sizeof lines[2], "layers %.1f ms   effects %.1f ms (%s)   upload %.1f ms", speed.layers / n, speed.effects / n,
                 where, speed.upload / n);
        double fr = speed.frames ? speed.frames : 1;
        snprintf(lines[3], sizeof lines[3], "game %.1f ms a frame (rewind %.1f ms)   drawing %.1f ms   waiting for the display %.1f ms", speed.game / fr, speed.rewind_ms / fr, speed.draw / n, speed.present / n);
        speed.layers = speed.effects = speed.upload = 0;
        speed.game = speed.rewind_ms = speed.present = speed.draw = 0;
        speed.frames = 0;
        speed.pictures = 0;
        speed.since = SDL_GetPerformanceCounter();
        speed.dirty = false;
        int px = win_h / 52 > 13 ? win_h / 52 : 13, line_h = px * 3 / 2, pad = px * 2 / 3;
        TextMask m[4];
        int w = 0;
        for (int k = 0; k < 4; k++) { m[k] = text_render(lines[k], "Segoe UI", px, k == 0 ? 600 : 400, 0); if (m[k].w > w) w = m[k].w; }
        int tw = w + pad * 2, th = line_h * 4 + pad * 2;
        uint32_t *img = calloc((size_t)tw * th, 4);
        if (img) {
            for (int i = 0; i < tw * th; i++) img[i] = 0xB4000000u;             /* a dark backing */
            for (int k = 0; k < 4; k++) {
                uint32_t c = k == 0 ? 0xFFF3DDA4u : 0xFFE6E9EFu;
                for (int y = 0; y < m[k].h; y++)
                    for (int x = 0; x < m[k].w; x++) {
                        int a = m[k].a ? m[k].a[y * m[k].w + x] : 0, ix = pad + x, iy = pad + k * line_h + y;
                        if (!a || ix >= tw || iy >= th) continue;
                        uint32_t d = img[iy * tw + ix];
                        int r = (int)((d >> 16) & 255) + ((int)((c >> 16) & 255) - (int)((d >> 16) & 255)) * a / 255;
                        int g = (int)((d >> 8) & 255) + ((int)((c >> 8) & 255) - (int)((d >> 8) & 255)) * a / 255;
                        int b = (int)(d & 255) + ((int)(c & 255) - (int)(d & 255)) * a / 255;
                        int al = 0xB4 + (255 - 0xB4) * a / 255;
                        img[iy * tw + ix] = (uint32_t)al << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
                    }
            }
            if (!panel || pw != tw || ph != th) {
                if (panel) SDL_DestroyTexture(panel);
                panel = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, tw, th);
                if (panel) SDL_SetTextureBlendMode(panel, SDL_BLENDMODE_BLEND);
                pw = tw; ph = th;
            }
            if (panel) SDL_UpdateTexture(panel, NULL, img, tw * 4);
            free(img);
        }
        for (int k = 0; k < 4; k++) text_free(&m[k]);
    }
    if (panel) {
        SDL_Rect r = { win_w / 100 + 8, win_h / 100 + 8, pw, ph };
        SDL_RenderCopy(ren, panel, NULL, &r);
    }
}

static SDL_Rect display_rect(int win_w, int win_h) {
    int fw = frame_width(), fh = vdp_screen_height();
    double par = (4.0 / 3.0) / (320.0 / 224.0);
    double aspect = fw * par / fh;
    SDL_Rect r;
    if (settings.integer_scale) {
        int s = win_h / fh;
        if (s < 1) s = 1;
        while (s > 1 && (int)(fh * s * aspect) > win_w) s--;
        r.h = fh * s;
        r.w = (int)(r.h * aspect);
    } else if ((double)win_w / win_h > aspect) {
        r.h = win_h;
        r.w = (int)(win_h * aspect);
    } else {
        r.w = win_w;
        r.h = (int)(win_w / aspect);
    }
    r.x = (win_w - r.w) / 2;
    r.y = (win_h - r.h) / 2;
    return r;
}

/* The finishing steps for this frame (hd_post). The effects that follow the game world
 * (particles, combat info) need its camera, which is not mapped for this game yet. */
static HdPost finishing(int scale, float t) {
    HdPost p = { settings.sharpen, settings.lighting, false, false,
                 false, 0, 0, t, scale < 1 ? 1 : scale, vdp_opts.extra_width, 0 };
    p.hdr = settings.hdr;
    return p;
}

/* The ROM to play when none is set or it no longer loads: this build's versions in the rom
 * folder next to the game (where Setup puts them; each is remembered as its version's ROM,
 * the last found is played), else the player picks one when `ask`. False if there is none. */
static bool find_rom(bool ask) {
    char dir[1100];
    path_in_base(dir, sizeof dir, "rom");
#ifdef _WIN32
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        bool found = false;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char p[1400];
            snprintf(p, sizeof p, "%s/%s", dir, fd.cFileName);
            if (settings_use_rom(p)) found = true;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        if (found) return true;
    }
    if (!ask) return false;
    wchar_t file[1024] = L"";
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFilter = L"Genesis / Mega Drive ROM (*.md;*.bin;*.gen)\0*.md;*.bin;*.gen\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 1024;
    ofn.lpstrTitle = L"Choose your Crusader of Centy / Ragnacenty ROM";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    char path[1024];
    WideCharToMultiByte(CP_UTF8, 0, file, -1, path, sizeof path, NULL, NULL);
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    if (!settings_use_rom(path)) snprintf(settings.rom_path, sizeof settings.rom_path, "%s", path);  /* the message says what is wrong */
    return true;
#else
    (void)dir; (void)ask;
    return false;
#endif
}

void console_attach(void);           /* console.c */

int main(int argc, char **argv) {
    console_attach();
    char *bp = SDL_GetBasePath();
    if (bp) { snprintf(base_dir, sizeof base_dir, "%s", bp); SDL_free(bp); }
    settings_set_base(base_dir);
    char ini[1100];
    path_in_base(ini, sizeof ini, "settings.ini");
    savestate_init(base_dir);
    settings_load(ini);

    int headless_frames = 0;
    const char *shot = NULL;
    int profile_frames = 0;
    const char *wav = NULL;
    const char *hash_log = NULL;
    const char *shot_dir = NULL;
    int shot_every = 0;
    const char *dump = NULL;
    uint32_t watch[16];
    int watch_count = 0;
    int check_from = -1, check_to = -1;     /* --state-check A B */
    int rewind_check = 0;                   /* --rewind-check K */
    const char *menu_shot = NULL;
    int menu_w = 1920, menu_h = 1080;
    bool controls_shot = false;
    int menu_tab = -1, menu_row = 0;    /* --menu-tab TAB ROW, with --menu-shot */
    bool save_preview = false;          /* --save-preview: keep the last frame for the launcher's previews */
    int save_to = 0;
    int load_from = 0;
    int switch_at = -1;
    const char *switch_to = NULL;
    bool rom_from_args = false;         /* a ROM path was given on the command line */
    bool test_overrides = false;        /* settings overridden on the command line: don't save them */
    const char *hd_shot = NULL;
    int bench = 0;                      /* --bench N */
    double window_test = 0;             /* --window-test SECONDS F.png */
    const char *window_test_png = NULL;
    const char *sprite_dir = NULL;      /* --sprite-check EVERY DIR SCALE */
    int sprite_every = 0;
    const char *filter_shot = NULL;     /* --filter-shot F.png W H MODE */
    const char *gpu_shot = NULL, *gpu_cpu_shot = NULL;   /* --gpu-shot GPU.png CPU.png */
    const char *gpu_api = NULL;         /* --gpu-api direct3d11|opengl: the GPU effects on that one only */
    int filter_w = 1920, filter_h = 1080, filter_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless") && i + 1 < argc) headless_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--pc-profile") && i + 1 < argc) profile_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(argv[i], "--interpret")) machine_opts.recomp = false;
        else if (!strcmp(argv[i], "--hash-log") && i + 1 < argc) hash_log = argv[++i];
        else if (!strcmp(argv[i], "--verify")) machine_opts.verify = true;
        else if (!strcmp(argv[i], "--hide-layers") && i + 1 < argc) vdp_opts.hide_layers = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trace-vram") && i + 2 < argc) {
            extern uint32_t vdp_trace_lo, vdp_trace_hi;
            vdp_trace_lo = (uint32_t)strtoul(argv[++i], NULL, 16);
            vdp_trace_hi = (uint32_t)strtoul(argv[++i], NULL, 16);
        }
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
        else if (!strcmp(argv[i], "--state-check") && i + 2 < argc) { check_from = atoi(argv[++i]); check_to = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--rewind-check") && i + 1 < argc) rewind_check = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--watch") && i + 1 < argc) {
            /* --watch ADDR,ADDR,...: print these RAM bytes whenever one changes. */
            for (char *p = argv[++i]; *p && watch_count < 16; ) {
                watch[watch_count++] = (uint32_t)strtoul(p, &p, 16) & 0xFFFF;
                if (*p == ',') p++;
            }
        }
        else if (!strcmp(argv[i], "--press") && i + 1 < argc) parse_presses(argv[++i]);
        else if (!strcmp(argv[i], "--shots") && i + 2 < argc) { shot_every = atoi(argv[++i]); shot_dir = argv[++i]; }
        else if (!strcmp(argv[i], "--overclock") && i + 1 < argc) { settings.overclock = atoi(argv[++i]); test_overrides = true; }   /* percent */
        else if (!strcmp(argv[i], "--sharpen") && i + 1 < argc) { settings.sharpen = atoi(argv[++i]) & 3; test_overrides = true; }   /* 0-3 */
        else if (!strcmp(argv[i], "--merged") && i + 1 < argc) { settings.merged_pass = atoi(argv[++i]) != 0; test_overrides = true; }
        else if (!strcmp(argv[i], "--avx2") && i + 1 < argc) { settings.avx2 = atoi(argv[++i]) != 0; test_overrides = true; }
        else if (!strcmp(argv[i], "--hdr") && i + 1 < argc) { settings.hdr = atoi(argv[++i]) % 3; test_overrides = true; }   /* 0-2 */
        else if (!strcmp(argv[i], "--lighting") && i + 1 < argc) { settings.lighting = atoi(argv[++i]) % 3; test_overrides = true; }   /* 0-2 */
        else if (!strcmp(argv[i], "--texture-pack") && i + 1 < argc) { snprintf(settings.texture_pack, sizeof settings.texture_pack, "%s", argv[++i]); test_overrides = true; }
        else if (!strcmp(argv[i], "--dump-tiles") && i + 1 < argc) { settings.dump_tiles = atoi(argv[++i]) != 0; test_overrides = true; }
        else if (!strcmp(argv[i], "--scaler") && i + 1 < argc) { settings.scaler = atoi(argv[++i]) % 3; test_overrides = true; }   /* 0 edge, 1 smooth, 2 sharp */
        else if (!strcmp(argv[i], "--audio") && i + 1 < argc) { settings.audio = atoi(argv[++i]) % 3; test_overrides = true; }   /* 0-2 */
        else if (!strcmp(argv[i], "--volume") && i + 1 < argc) { settings.volume = atoi(argv[++i]); test_overrides = true; }   /* 0-100 */
        else if (!strcmp(argv[i], "--filter") && i + 1 < argc) { settings.filter = atoi(argv[++i]) % FILTER_COUNT; test_overrides = true; }   /* 0 off, 1 scanlines, 2 CRT, 3 LCD */
        else if (!strcmp(argv[i], "--png-check") && i + 1 < argc) {
            /* --png-check F.png: read a PNG and print its size and a checksum of its pixels. */
            int w = 0, h = 0;
            uint32_t *px = png_read(argv[++i], &w, &h);
            uint32_t sum = 0;
            for (int k = 0; px && k < w * h; k++) sum = sum * 31 + px[k];
            printf("png %s: %dx%d checksum %08X\n", px ? "read" : "NOT read", w, h, sum);
            free(px);
            return px ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--gpu-api") && i + 1 < argc) gpu_api = argv[++i];
        else if (!strcmp(argv[i], "--gpu-shot") && i + 2 < argc) { gpu_shot = argv[++i]; gpu_cpu_shot = argv[++i]; }
        else if (!strcmp(argv[i], "--filter-shot") && i + 4 < argc) {
            /* --filter-shot F.png W H MODE: draw the last frame into a W x H picture with
             * filter MODE (0 off, 1 scanlines, 2 CRT, 3 LCD), on the GPU in a hidden window. */
            filter_shot = argv[++i];
            filter_w = atoi(argv[++i]);
            filter_h = atoi(argv[++i]);
            filter_mode = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--sprite-check") && i + 3 < argc) {
            /* --sprite-check EVERY DIR SCALE: every EVERY frames, compare the frame drawn layer by
             * layer with the same frame drawn sprite by sprite (as smooth motion does). They
             * must match except at overlapping sprite edges; the worst frame is saved to DIR. */
            sprite_every = atoi(argv[++i]);
            sprite_dir = argv[++i];
            settings.internal_scale = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--window-test") && i + 2 < argc) { window_test = atof(argv[++i]); window_test_png = argv[++i]; }
        else if (!strcmp(argv[i], "--bench") && i + 1 < argc) bench = atoi(argv[++i]);   /* pictures per game frame, as the window draws them */
        else if (!strcmp(argv[i], "--hd-shot") && i + 2 < argc) {
            /* --hd-shot F.png SCALE: save the last frame at the internal resolution SCALE (2-4). */
            hd_shot = argv[++i];
            settings.internal_scale = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--save-to") && i + 1 < argc) save_to = atoi(argv[++i]);   /* save a slot at the end */
        else if (!strcmp(argv[i], "--states-base") && i + 1 < argc) savestate_init(argv[++i]);  /* e.g. "test_" -> test_states/ */
        else if (!strcmp(argv[i], "--slot") && i + 1 < argc) { settings.state_slot = atoi(argv[++i]); test_overrides = true; }
        else if (!strcmp(argv[i], "--load") && i + 1 < argc) load_from = atoi(argv[++i]);   /* load a slot first */
        else if (!strcmp(argv[i], "--switch-language") && i + 2 < argc) { switch_at = atoi(argv[++i]); switch_to = argv[++i]; }
        else if (!strcmp(argv[i], "--poke") && i + 1 < argc) cheats_add_codes(argv[++i]);   /* FFxxxx:VV[VV],... */
        else if (!strcmp(argv[i], "--menu-shot") && i + 3 < argc) {
            /* --menu-shot F.png W H: open the in-game menu over the last frame and render it. */
            menu_shot = argv[++i];
            menu_w = atoi(argv[++i]);
            menu_h = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--controls-shot")) controls_shot = true;   /* with --menu-shot: the Controls page */
        else if (!strcmp(argv[i], "--save-preview")) save_preview = true;
        else if (!strcmp(argv[i], "--menu-tab") && i + 2 < argc) { menu_tab = atoi(argv[++i]); menu_row = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--input-check") && i + 1 < argc) {
            /* --input-check FILE: rebind a few keys and buttons, save to FILE, load it back,
             * and print the bindings (never touches settings.ini). */
            const char *file = argv[++i];
            input_bind_key(IN_A, SDL_SCANCODE_J);
            input_bind_key(IN_B, SDL_SCANCODE_J);           /* taken by A: A gets B's old key */
            input_bind_button(IN_START, SDL_CONTROLLER_BUTTON_Y);
            input_bind_button(IN_REWIND, SDL_CONTROLLER_BUTTON_START);   /* taken by Start: swaps */
            settings_save(file);
            input_defaults();
            settings_load(file);
            for (int k = 0; k < IN_COUNT; k++)
                printf("  %-14s keyboard %-12s controller %s\n", input_labels[k], input_key_name(k), input_button_name(k));
            return 0;
        }
        else { snprintf(settings.rom_path, sizeof settings.rom_path, "%s", argv[i]); rom_from_args = true; }
    }

    if (SDL_Init(headless_frames ? 0 : SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (!headless_frames) pads_init();
    const char *err = settings.rom_path[0] ? bus_load_rom(settings.rom_path) : "no ROM selected";
    if (err && !rom_from_args && find_rom(!headless_frames))
        err = settings.rom_path[0] ? bus_load_rom(settings.rom_path) : "no ROM selected";
    if (err) {
        char msg[1400];
        snprintf(msg, sizeof msg, "Could not load the game's ROM:\n%s\n\nPath: %s", err, settings.rom_path);
        if (headless_frames) fprintf(stderr, "%s\n", msg);
        else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Crusader of Centy Recomp", msg, NULL);
        return 1;
    }
    /* The ROM played is its version's ROM and sets the language (an older settings.ini
     * named only the USA ROM). */
    settings_use_rom(settings.rom_path);
    if (!headless_frames && rom_from_args) {
        /* Remember a ROM path given on the command line, but not the other options
         * given there (--filter, --widescreen, ...): those are for this run. */
        Settings run = settings;
        settings_load(ini);
        snprintf(settings.rom_path, sizeof settings.rom_path, "%s", run.rom_path);
        settings_save(ini);
        settings = run;
    }
    /* Headless runs start from blank save RAM, so scripted routes play the same whatever
     * the player has saved (and they never write it back, see below). */
    if (headless_frames) memset(bus.sram, 0xFF, SRAM_SIZE);
    else load_sram();
    apply_settings();
    machine_reset();
    rewind_reset(settings.rewind ? (size_t)settings.rewind_mb << 20 : 0);

    if (headless_frames) {
        /* --state-check A B: snapshot before frame A, run to B, load the snapshot, run
         * A..B again with the same input; both runs must end in the same state.
         * --rewind-check K: record every frame, then rewind K frames; the result must
         * equal the snapshot taken K frames before the end. */
        static uint8_t snap_a[STATE_MAX], snap_b[STATE_MAX], snap_ref[STATE_MAX];
        size_t len_a = 0, len_b = 0, len_ref = 0;
        rewind_reset(rewind_check ? (size_t)512 << 20 : 0);
        if (load_from && !savestate_load(load_from)) fprintf(stderr, "could not load slot %d\n", load_from);
        /* --wav: record the audio of the run (16-bit stereo, 48 kHz). */
        FILE *wf = wav ? fopen(wav, "wb") : NULL;
        uint32_t wav_frames = 0;
        if (wf) fseek(wf, 44, SEEK_SET);
        /* --hash-log: one line per frame with a hash of RAM, VRAM and the 68000 PC,
         * to find the first frame where two runs (e.g. native vs --interpret) differ. */
        FILE *hf = hash_log ? fopen(hash_log, "w") : NULL;
        for (int f = 0; f < headless_frames; f++) {
            if (profile_frames && f == headless_frames - profile_frames) pc_profile = calloc(ROM_SIZE / 2, 4);
            pad_state[0] = scripted_pad(f);
            /* --switch-language F ID: at frame F, change the language as the pause menu does. */
            if (f == switch_at && switch_to) {
                for (int k = 0; k < SETTINGS_VERSIONS && strcmp(settings.language, switch_to); k++)
                    settings_option_change(OPT_LANGUAGE, 1);
                if (switch_version()) { apply_settings(); machine_reset(); }
                fprintf(stderr, "frame %d: switched to %s\n", f, game->name);
            }
            if (f == check_from) len_a = state_save(snap_a, sizeof snap_a);
            if (f == check_to) len_b = state_save(snap_b, sizeof snap_b);
            if (rewind_check && f == headless_frames - rewind_check) len_ref = state_save(snap_ref, sizeof snap_ref);
            rewind_push(pad_state[0]);
            cheats_frame();
            Uint64 g0 = SDL_GetPerformanceCounter();
            machine_run_frame(framebuffer, FB_W);
            if (vdp_opts.hd_capture) hd_frame_done();
            if (bench) {
                /* --bench N: the game frame, then N pictures of it as the window draws them
                 * (the layers put together at a point between frames, then the finishing
                 * steps); the average times over the run, in ms, at the end. */
                static double sum[7];
                static int n;
                double ms_per = 1000.0 / SDL_GetPerformanceFrequency();
                sum[0] += (double)(SDL_GetPerformanceCounter() - g0) * ms_per;
                for (int k = 0; k < bench; k++) {
                    float frac = (k + 1.f) / bench;
                    Uint64 t0 = SDL_GetPerformanceCounter();
                    (void)frac;
                    hd_compose(settings.internal_scale);
                    Uint64 t1 = SDL_GetPerformanceCounter();
                    HdPost post = finishing(hd_w / frame_width(), (f + frac) / 60.f);
                    const uint32_t *pic = hd_post(hd_px, hd_w, hd_h, hd_w, &post);
                    if (shot && n + 1 == headless_frames && k == bench - 1) { png_write(shot, pic, hd_w, hd_h, hd_w); shot = NULL; }   /* the last picture, finished */
                    sum[1] += (double)(t1 - t0) * ms_per;
                    sum[2] += (double)(SDL_GetPerformanceCounter() - t1) * ms_per;
                    for (int m = 0; m < 4; m++) sum[3 + m] += hd_ms[m];
                }
                if (++n == headless_frames)
                    printf("bench %dx%d: game %.2f ms a frame; a picture: layers together %.2f (scaling layers %.2f, "
                           "sprites %.2f + %.2f, compositing %.2f), finishing %.2f\n",
                           hd_w, hd_h, sum[0] / n, sum[1] / n / bench, sum[3] / n / bench, sum[4] / n / bench,
                           sum[5] / n / bench, sum[6] / n / bench, sum[2] / n / bench);
            }
            if (sprite_every > 0 && f >= 300 && f % sprite_every == 0) {
                static uint32_t *layers, *worst_a, *worst_b;
                static int worst = -1, worst_frame, checked, over;
                hd_compose(settings.internal_scale);
                size_t n = (size_t)hd_w * hd_h;
                layers = realloc(layers, n * 4);
                memcpy(layers, hd_px, n * 4);
                hd_compose_sprites(settings.internal_scale);
                int diff = 0;
                for (size_t k = 0; k < n; k++) diff += layers[k] != hd_px[k];
                checked++;
                if (diff > (int)(n / 200)) over++;      /* more than 0.5% of the picture */
                if (diff > worst) {
                    worst = diff; worst_frame = f;
                    worst_a = realloc(worst_a, n * 4); worst_b = realloc(worst_b, n * 4);
                    memcpy(worst_a, layers, n * 4); memcpy(worst_b, hd_px, n * 4);
                }
                if (f + sprite_every >= headless_frames) {
                    char p[1200];
                    printf("sprite check: %d frames, %d differ by more than 0.5%%; worst frame %d: %.2f%% of pixels\n",
                           checked, over, worst_frame, 100.0 * worst / n);
                    snprintf(p, sizeof p, "%s/layers.png", sprite_dir); png_write(p, worst_a, hd_w, hd_h, hd_w);
                    snprintf(p, sizeof p, "%s/sprites.png", sprite_dir); png_write(p, worst_b, hd_w, hd_h, hd_w);
                }
            }
            if (watch_count) {
                static uint8_t last[16];
                bool changed = f == 0;
                for (int k = 0; k < watch_count; k++) changed |= last[k] != bus.ram[watch[k]];
                if (changed) {
                    printf("%5d", f);
                    for (int k = 0; k < watch_count; k++) printf(" %02X", last[k] = bus.ram[watch[k]]);
                    printf("\n");
                }
            }
            if (shot_every && f % shot_every == 0) {
                char p[1200];
                snprintf(p, sizeof p, "%s/%05d.png", shot_dir, f);
                save_png(p);
            }
            if (wf) { fwrite(sound_out, 4, (size_t)sound_out_count, wf); wav_frames += (uint32_t)sound_out_count; }
            if (hf) {
                uint64_t h = 1469598103934665603ull;
                for (size_t k = 0; k < sizeof bus.ram; k++) h = (h ^ bus.ram[k]) * 1099511628211ull;
                uint64_t v = 1469598103934665603ull;
                for (size_t k = 0; k < sizeof vdp.vram; k++) v = (v ^ vdp.vram[k]) * 1099511628211ull;
                fprintf(hf, "%d ram=%016llx vram=%016llx pc=%06x\n", f, (unsigned long long)h, (unsigned long long)v,
                        m68k_get_reg(NULL, M68K_REG_PC));
            }
        }
        if (hf) fclose(hf);
        if (len_a && len_b) {
            static uint8_t again[STATE_MAX];
            bool loaded = state_load(snap_a, len_a);
            for (int f = check_from; f < check_to; f++) { pad_state[0] = scripted_pad(f); machine_run_frame(framebuffer, FB_W); }
            size_t len = state_save(again, sizeof again);
            printf("state check %d-%d: snapshot %zu bytes, %s\n", check_from, check_to, len_a,
                   loaded && len == len_b && !memcmp(again, snap_b, len) ? "replay identical" : "REPLAY DIFFERS");
        }
        if (rewind_check)
            printf("rewind buffer: %d frames in %.1f MB\n", rewind_frames(), rewind_bytes() / 1048576.0);
        if (rewind_check && len_ref) {
            static uint8_t now[STATE_MAX];
            rewind_push(0);
            uint16_t p;
            int steps = 0;
            while (steps < rewind_check && rewind_step(&p)) steps++;
            size_t len = state_save(now, sizeof now);
            printf("rewind check: %d frames back, %s\n", steps,
                   steps == rewind_check && len == len_ref && !memcmp(now, snap_ref, len) ? "state identical" : "STATE DIFFERS");
        }
        if (wf) {
            uint32_t data = wav_frames * 4, rate = (uint32_t)sound_out_rate, br = rate * 4;
            uint8_t h[44] = { 0x52,0x49,0x46,0x46, 0,0,0,0, 0x57,0x41,0x56,0x45, 0x66,0x6D,0x74,0x20,
                             16,0,0,0, 1,0, 2,0, 0,0,0,0, 0,0,0,0, 4,0, 16,0, 0x64,0x61,0x74,0x61 };
            uint32_t riff = 36 + data;
            memcpy(h + 4, &riff, 4); memcpy(h + 24, &rate, 4); memcpy(h + 28, &br, 4); memcpy(h + 40, &data, 4);
            fseek(wf, 0, SEEK_SET); fwrite(h, 1, 44, wf); fclose(wf);
        }
        if (pc_profile) print_pc_profile();
        if (dump) {
            /* --dump: raw machine state for offline analysis:
             * RAM 64K, VRAM 64K, CRAM 128, VSRAM 80, VDP registers 32. */
            FILE *df = fopen(dump, "wb");
            if (df) {
                fwrite(bus.ram, 1, sizeof bus.ram, df);
                fwrite(vdp.vram, 1, sizeof vdp.vram, df);
                fwrite(vdp.cram, 1, sizeof vdp.cram, df);
                fwrite(vdp.vsram, 1, sizeof vdp.vsram, df);
                fwrite(vdp.reg, 1, sizeof vdp.reg, df);
                fclose(df);
            }
        }
        if (shot && save_shot(shot) != 0) fprintf(stderr, "could not save %s\n", shot);
        if (filter_shot) {
            SDL_InitSubSystem(SDL_INIT_VIDEO);
            SDL_Window *hw = SDL_CreateWindow("filter", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
            SDL_Renderer *sr = hw ? SDL_CreateRenderer(hw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE) : NULL;
            SDL_Texture *target = sr ? SDL_CreateTexture(sr, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, filter_w, filter_h) : NULL;
            uint32_t *pixels = malloc((size_t)filter_w * filter_h * 4);
            if (target && pixels) {
                SDL_SetRenderTarget(sr, target);
                bool hd = settings.internal_scale > 1 || hd_texpack;   /* as the window draws it */
                hd_time = 1.0f;
                if (hd) hd_compose(settings.internal_scale);
                int tw = hd ? hd_w : frame_width(), th = hd ? hd_h : vdp_screen_height();
                SDL_Texture *t = SDL_CreateTexture(sr, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, tw, th);
                HdPost post = finishing(hd ? hd_w / frame_width() : 1, 1.0f);
                hd_post(hd ? hd_px : framebuffer, tw, th, hd ? hd_w : FB_W, &post);   /* once to set up, then timed */
                Uint64 p0 = SDL_GetPerformanceCounter();
                const uint32_t *finished = hd_post(hd ? hd_px : framebuffer, tw, th, hd ? hd_w : FB_W, &post);
                printf("finishing steps: %.2f ms at %dx%d\n", (SDL_GetPerformanceCounter() - p0) * 1000.0 / SDL_GetPerformanceFrequency(), tw, th);
                SDL_UpdateTexture(t, NULL, finished, tw * 4);
                SDL_SetRenderDrawColor(sr, 0, 0, 0, 255);
                SDL_RenderClear(sr);
                Uint64 t0 = SDL_GetPerformanceCounter();
                if (hd) SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
                filter_draw(sr, t, NULL, display_rect(filter_w, filter_h), frame_width(), vdp_screen_height(), filter_mode);
                SDL_RenderReadPixels(sr, NULL, SDL_PIXELFORMAT_XRGB8888, pixels, filter_w * 4);
                printf("filter %d drawn in %.1f ms (GPU, first draw builds the overlay)\n", filter_mode,
                       (double)(SDL_GetPerformanceCounter() - t0) * 1000 / SDL_GetPerformanceFrequency());
                png_write(filter_shot, pixels, filter_w, filter_h, filter_w);
                SDL_DestroyTexture(t);
            } else {
                fprintf(stderr, "no GPU renderer for --filter-shot: %s\n", SDL_GetError());
            }
            free(pixels);
            if (sr) SDL_DestroyRenderer(sr);
            if (hw) SDL_DestroyWindow(hw);
        }
        if (gpu_shot) {
            /* --gpu-shot GPU.png CPU.png: the last frame at the internal resolution, finished
             * by the GPU's effects (in a hidden OpenGL window, read back) and by the CPU's. */
            SDL_InitSubSystem(SDL_INIT_VIDEO);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, gpu_api ? gpu_api : "direct3d11");
            SDL_Window *hw = SDL_CreateWindow("gpu", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
            SDL_Renderer *sr = hw ? SDL_CreateRenderer(hw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE) : NULL;
            if (sr && gpu_fx_init(sr)) {
                hd_time = 1.0f;
                hd_compose(settings.internal_scale);
                HdPost post = finishing(hd_w / frame_width(), 1.0f);
                uint32_t *pixels = malloc((size_t)hd_w * hd_h * 4);
                double ms = 0;
                SDL_Texture *res = NULL;
                for (int k = 0; k < 11 && pixels; k++) {    /* the first builds the textures; the rest timed */
                    Uint64 t0 = SDL_GetPerformanceCounter();
                    res = gpu_fx_finish(sr, hd_px, hd_w, hd_h, hd_w, &post, true);
                    if (res) { SDL_SetRenderTarget(sr, res); SDL_RenderReadPixels(sr, NULL, SDL_PIXELFORMAT_XRGB8888, pixels, hd_w * 4); SDL_SetRenderTarget(sr, NULL); }
                    if (k) ms += (double)(SDL_GetPerformanceCounter() - t0) * 1000 / SDL_GetPerformanceFrequency();
                }
                if (res) {
                    printf("GPU effects: %.2f ms a picture at %dx%d with reading it back (upload %.2f ms)\n", ms / 10, hd_w, hd_h, gpu_fx_upload_ms);
                    png_write(gpu_shot, pixels, hd_w, hd_h, hd_w);
                    png_write(gpu_cpu_shot, hd_post(hd_px, hd_w, hd_h, hd_w, &post), hd_w, hd_h, hd_w);
                } else fprintf(stderr, "the GPU effects did not run\n");
                free(pixels);
            } else fprintf(stderr, "no OpenGL renderer for --gpu-shot: %s\n", SDL_GetError());
            if (sr) SDL_DestroyRenderer(sr);
            if (hw) SDL_DestroyWindow(hw);
        }
        if (hd_shot) {
            Uint64 t0 = SDL_GetPerformanceCounter();
            hd_compose(settings.internal_scale);
            double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000 / SDL_GetPerformanceFrequency();
            printf("hd frame %dx%d in %.2f ms\n", hd_w, hd_h, ms);
            HdPost post = finishing(hd_w / frame_width(), 0);     /* with the effects, as the window shows it */
            png_write(hd_shot, hd_post(hd_px, hd_w, hd_h, hd_w, &post), hd_w, hd_h, hd_w);
        }
        if (save_preview) {
            HdPost here = finishing(1, 0);
            preview_set_world(here.world, here.cam_x, here.cam_y, -1, -1);
            preview_save(base_dir);
        }
        if (save_to && !savestate_save(save_to, framebuffer, frame_width(), vdp_screen_height(), FB_W))
            fprintf(stderr, "could not save slot %d\n", save_to);
        if (menu_shot) {
            SDL_Rect r = display_rect(menu_w, menu_h);
            menu_open(framebuffer, frame_width(), vdp_screen_height(), FB_W, r.x, r.y, r.w, r.h, menu_w, menu_h, 1);
            if (controls_shot) controls_open();
            if (menu_tab >= 0) {
                HdPost here = finishing(1, 0);
                preview_set_world(here.world, here.cam_x, here.cam_y, -1, -1);
                menu_select(menu_tab, menu_row);
            }
            menu_render(1.0);
            png_write(menu_shot, ui_px, ui_w, ui_h, ui_w);
        }
        save_coverage();
        /* Headless runs are tests: they never write the player's save RAM (save.srm),
         * whatever the scripted input or a loaded state did to it. */
        printf("ran %d frames, screen %dx%d\n", headless_frames, frame_width(), vdp_screen_height());
        if (stat_total_cycles)
            printf("68000 at %d%%: %.1f%% of its time spare, %u game ticks, %u late (slowdown)\n", machine_opts.overclock,
                   100.0 * stat_wait_cycles / stat_total_cycles, stat_ticks, stat_late_ticks);
        if (stat_native_cycles + stat_interp_cycles)
            printf("68000 time: %.2f%% native, %.2f%% interpreted\n",
                   100.0 * stat_native_cycles / (stat_native_cycles + stat_interp_cycles),
                   100.0 * stat_interp_cycles / (stat_native_cycles + stat_interp_cycles));
        if (machine_opts.verify) printf("verified %u block runs, %u mismatches\n", verify_checked, verify_failed);
        return 0;
    }

    int start_w, start_h;
    window_size(&start_w, &start_h);
    SDL_Window *win = SDL_CreateWindow("Crusader of Centy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, start_w, start_h,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (window_test ? SDL_WINDOW_HIDDEN : 0) |
                                       (settings.fullscreen && !window_test ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    /* Direct3D 11 for every picture: it presents through a flip-model swap chain, where
     * SDL's default on Windows (Direct3D 9) can wait most of a refresh at each present.
     * With the GPU effects on, OpenGL is the second choice for them; failing both, SDL's
     * default and the CPU's effects. --gpu-api picks one for tests. */
    SDL_Renderer *ren = NULL;
    const char *apis[2] = { gpu_api ? gpu_api : "direct3d11", settings.gpu_effects && !gpu_api ? "opengl" : NULL };
    for (int k = 0; k < 2 && apis[k]; k++) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, apis[k]);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        if (!ren) continue;
        gpu_effects_running = settings.gpu_effects && gpu_fx_init(ren);
        if (gpu_effects_running || !settings.gpu_effects || !apis[k + 1]) break;
        SDL_DestroyRenderer(ren);       /* this one cannot run them: try the next */
        ren = NULL;
    }
    if (!ren) { SDL_SetHint(SDL_HINT_RENDER_DRIVER, ""); ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED); }
    SDL_RendererInfo ren_info;
    bool on_opengl = SDL_GetRendererInfo(ren, &ren_info) == 0 && !strcmp(ren_info.name, "opengl");
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H);
    SDL_Texture *hd_tex[2] = { NULL, NULL };   /* the frame at the internal resolution, two in turn */
    int hd_tex_now = 0;
    int hd_tex_w = 0, hd_tex_h = 0;
    bool vsync_on = false;
    SDL_AudioSpec want = { .freq = 48000, .format = AUDIO_S16SYS, .channels = 2, .samples = 1024 }, have;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio) { sound_out_rate = have.freq; SDL_PauseAudioDevice(audio, 0); }

    /* Short messages (save, load, slot) show in the window title for two seconds. */
    char message[128] = "";
    Uint64 message_until = 0;
#define NOTIFY(...) do { snprintf(message, sizeof message, __VA_ARGS__); \
                         message_until = SDL_GetPerformanceCounter() + SDL_GetPerformanceFrequency() * 2; } while (0)
    char shown_title[160] = "";

    Uint64 freq = SDL_GetPerformanceFrequency(), next = SDL_GetPerformanceCounter(), start = next;
    SDL_Texture *menu_tex = NULL;
    int menu_tex_w = 0, menu_tex_h = 0;
    bool running = true, fast_now = false;
    /* --window-test: the window loop as it runs for a player, hidden, from --load's slot;
     * the last picture as the window shows it is saved, with the speed state. */
    if (window_test && load_from && !savestate_load(load_from)) fprintf(stderr, "could not load slot %d\n", load_from);
    while (running) {
        SDL_Event e;
        int ww, wh;
        SDL_GetRendererOutputSize(ren, &ww, &wh);
        while (SDL_PollEvent(&e)) {
            pads_event(&e);
            /* The pause menu binding opens the menu; Esc and the Guide button always do too. */
            bool open_menu = input_event_is(IN_MENU, &e) ||
                             (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE && !e.key.repeat) ||
                             (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE);
            if (e.type == SDL_QUIT) running = false;
            else if (menu_is_open()) {
                bool was_rewind = settings.rewind;
                switch (menu_event(&e)) {
                case MENU_CLOSE: case MENU_LOADED: menu_close(); break;
                case MENU_SETTINGS:
                    if (switch_version()) {     /* another language: that version starts */
                        apply_settings();
                        machine_reset();
                        rewind_reset(settings.rewind ? (size_t)settings.rewind_mb << 20 : 0);
                        menu_close();
                        break;
                    }
                    apply_settings();
                    SDL_SetWindowFullscreen(win, settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    if (settings.rewind != was_rewind) rewind_reset(settings.rewind ? (size_t)settings.rewind_mb << 20 : 0);
                    break;
                case MENU_RESET:
                    machine_reset();
                    rewind_reset(settings.rewind ? (size_t)settings.rewind_mb << 20 : 0);
                    menu_close();
                    break;
                case MENU_QUIT: running = false; break;
                case MENU_NONE: break;
                }
            } else if (open_menu) {
                int win_w, win_h;
                SDL_GetWindowSize(win, &win_w, &win_h);
                SDL_Rect r = display_rect(ww, wh);
                HdPost here = finishing(1, 0);
                preview_set_world(here.world, here.cam_x, here.cam_y, -1, -1);
                preview_save(base_dir);         /* the frame the launcher's previews will show */
                menu_open(framebuffer, frame_width(), vdp_screen_height(), FB_W, r.x, r.y, r.w, r.h, ww, wh,
                          win_w ? (float)ww / win_w : 1);
            }
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11) {
                settings.fullscreen = !settings.fullscreen;
                SDL_SetWindowFullscreen(win, settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F2 && !e.key.repeat) {
                if (savestate_save(settings.state_slot, framebuffer, frame_width(), vdp_screen_height(), FB_W))
                    NOTIFY("Saved to slot %d", settings.state_slot);
                else NOTIFY("Could not save slot %d", settings.state_slot);
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F4 && !e.key.repeat) {
                if (savestate_load(settings.state_slot)) NOTIFY("Loaded slot %d", settings.state_slot);
                else NOTIFY("Slot %d is empty or from another version", settings.state_slot);
            } else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_F6 || e.key.keysym.sym == SDLK_F7)) {
                settings.state_slot = (settings.state_slot + (e.key.keysym.sym == SDLK_F7 ? 1 : 7)) % 9 + 1;
                NOTIFY("Slot %d", settings.state_slot);
            }
        }
        if (menu_is_open()) {
            if (ww != ui_w || wh != ui_h) {     /* resized or fullscreen toggled: rebuild the backdrop */
                int win_w, win_h;
                SDL_GetWindowSize(win, &win_w, &win_h);
                SDL_Rect r = display_rect(ww, wh);
                menu_open(framebuffer, frame_width(), vdp_screen_height(), FB_W, r.x, r.y, r.w, r.h, ww, wh,
                          win_w ? (float)ww / win_w : 1);
            }
            menu_render((double)(SDL_GetPerformanceCounter() - start) / freq);
            if (!menu_tex || menu_tex_w != ui_w || menu_tex_h != ui_h) {
                if (menu_tex) SDL_DestroyTexture(menu_tex);
                menu_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, ui_w, ui_h);
                menu_tex_w = ui_w; menu_tex_h = ui_h;
            }
            SDL_UpdateTexture(menu_tex, NULL, ui_px, ui_w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, menu_tex, NULL, NULL);
            SDL_RenderPresent(ren);
            if (strcmp(shown_title, "Crusader of Centy - Paused")) { SDL_SetWindowTitle(win, "Crusader of Centy - Paused"); strcpy(shown_title, "Crusader of Centy - Paused"); }
            next += (Uint64)(machine_frame_s() * freq);
            Uint64 now = SDL_GetPerformanceCounter();
            if (now < next) SDL_Delay((Uint32)((next - now) * 1000 / freq));
            else next = now;
            continue;
        }

        /* One pass of this loop is one display refresh: vsync paces it. A game frame runs
         * whenever one is due (60 times a second, 50 for a PAL cartridge, on any display), and every refresh shows
         * the moment it is now, so motion is even at 60, 120, 144 Hz and up. */
        Uint64 now = SDL_GetPerformanceCounter();
        const Uint64 frame_ticks = (Uint64)(machine_frame_s() * freq);
        if (now >= next) {
            Uint64 game_t0 = SDL_GetPerformanceCounter();
            const Uint8 *keys = SDL_GetKeyboardState(NULL);
            bool rewinding = settings.rewind && input_held(IN_REWIND, keys, pads_active());
            uint16_t old_pad;
            if (rewinding && rewind_step(&old_pad)) {
                /* Redraw the frame we stepped back to, then return to the state before it. */
                pad_state[0] = old_pad;
                machine_run_frame(framebuffer, FB_W);
                if (vdp_opts.hd_capture) hd_frame_done();
                rewind_restore_head();
                sound_out_count = 0;            /* silent while rewinding */
            } else if (!rewinding) {
                /* Fast-forward: while its key is held, two to four game frames per frame
                 * (the last one's sound is heard). */
                int runs = input_held(IN_FAST, keys, pads_active()) ? settings.fast_speed + 2 : 1;
                for (int r = 0; r < runs; r++) {
                    pad_state[0] = input_pad_bits(keys, pads_active());
                    Uint64 rw0 = SDL_GetPerformanceCounter();
                    rewind_push(pad_state[0]);
                    speed.rewind_ms += (double)(SDL_GetPerformanceCounter() - rw0) * 1000 / freq;
                    speed.frames++;
                    cheats_frame();
                    machine_run_frame(framebuffer, FB_W);
                    if (vdp_opts.hd_capture) hd_frame_done();
                }
                fast_now = runs > 1;
            } else {
                sound_out_count = 0;            /* at the start of the buffer: hold the picture */
            }
            save_sram();

            char title[160];
            if (rewinding) snprintf(title, sizeof title, "Crusader of Centy - Rewinding (%.1f s left)", rewind_frames() * machine_frame_s());
            else if (fast_now) snprintf(title, sizeof title, "Crusader of Centy - Fast-forward %d\xC3\x97", settings.fast_speed + 2);
            else if (SDL_GetPerformanceCounter() < message_until) snprintf(title, sizeof title, "Crusader of Centy - %s", message);
            else snprintf(title, sizeof title, "Crusader of Centy");
            if (strcmp(title, shown_title)) { SDL_SetWindowTitle(win, title); strcpy(shown_title, title); }
            if (audio) {
                /* Keep about 3 frames queued: drop audio if we fall far behind. */
                if (SDL_GetQueuedAudioSize(audio) < (Uint32)sound_out_rate * 4 / 10) {
                    if (settings.volume < 100) {
                        int v = settings.volume * 256 / 100;
                        for (int i = 0; i < sound_out_count * 2; i++) sound_out[i] = (int16_t)(sound_out[i] * v / 256);
                    }
                    SDL_QueueAudio(audio, sound_out, (Uint32)sound_out_count * 4);
                }
            }
            speed.game += (double)(SDL_GetPerformanceCounter() - game_t0) * 1000 / freq;
            next += frame_ticks;
            if (now > next + freq / 4) next = now + frame_ticks;  /* fell far behind: don't try to catch up */
        }
        /* OpenGL's vsync in a window or borderless fullscreen waits a whole extra refresh
         * whenever a picture misses one; Windows' compositor already keeps the picture from
         * tearing there, so with OpenGL the loop paces itself (after the present below). */
        if (!vsync_on) { SDL_RenderSetVSync(ren, on_opengl ? 0 : 1); vsync_on = true; }

        /* The moment this picture shows: how far we are into the newest game frame (0-1). */
        now = SDL_GetPerformanceCounter();
        float frac = next > now ? 1 - (float)(next - now) / frame_ticks : 1;
        if (frac < 0) frac = 0;
        SDL_Rect dst = display_rect(ww, wh);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        /* The internal resolution this picture: the setting's, or auto quality's step. */
        int iscale = settings.auto_quality && quality_scale ? quality_scale : settings.internal_scale;
        /* Shadows are drawn while the layers are put together, so they need this path too. */
        bool hd = (iscale > 1 || hd_texpack) &&
                  hd_frame.h == vdp_screen_height();
        hd_time = (float)(SDL_GetPerformanceCounter() - start) / freq;
        Uint64 t_start = SDL_GetPerformanceCounter(), t_layers = t_start, t_effects = t_start, t_upload = t_start;
        bool on_gpu = false;
        if (hd) {
            hd_time = (float)(SDL_GetPerformanceCounter() - start) / freq;
            hd_compose(iscale);
            t_layers = SDL_GetPerformanceCounter();
            HdPost post = finishing(hd_w / frame_width(), (float)(SDL_GetPerformanceCounter() - start) / freq);
            SDL_Texture *shown = NULL;
            if (gpu_effects_running && settings.gpu_effects && (post.lighting || post.hdr || post.sharpen)) {
                /* The card grades, lights and sharpens. */
                shown = gpu_fx_finish(ren, hd_px, hd_w, hd_h, hd_w, &post, iscale > 1);
                t_effects = SDL_GetPerformanceCounter();
                t_upload = t_effects;
                on_gpu = shown != NULL;
            }
            if (!shown) {
                /* Two textures used in turn: uploading into the one the GPU just drew from
                 * can make the upload wait for the GPU. */
                if (!hd_tex[0] || hd_tex_w != hd_w || hd_tex_h != hd_h) {
                    for (int k = 0; k < 2; k++) {
                        if (hd_tex[k]) SDL_DestroyTexture(hd_tex[k]);
                        hd_tex[k] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, hd_w, hd_h);
                        /* the last, non-integer step to the window; native pixels stay sharp */
                        SDL_SetTextureScaleMode(hd_tex[k], iscale > 1 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
                    }
                    hd_tex_w = hd_w; hd_tex_h = hd_h;
                }
                hd_tex_now ^= 1;
                const uint32_t *img = hd_post(hd_px, hd_w, hd_h, hd_w, &post);
                t_effects = SDL_GetPerformanceCounter();
                SDL_UpdateTexture(hd_tex[hd_tex_now], NULL, img, hd_w * 4);
                t_upload = SDL_GetPerformanceCounter();
                shown = hd_tex[hd_tex_now];
            }
            filter_draw(ren, shown, NULL, dst, frame_width(), vdp_screen_height(), settings.filter);
        } else {
            SDL_Rect src = { 0, 0, frame_width(), vdp_screen_height() };
            HdPost post = finishing(1, (float)(SDL_GetPerformanceCounter() - start) / freq);
            const uint32_t *img = hd_post(framebuffer, src.w, src.h, FB_W, &post);
            t_effects = SDL_GetPerformanceCounter();
            if (img != framebuffer) SDL_UpdateTexture(tex, &src, img, src.w * 4);
            else SDL_UpdateTexture(tex, NULL, framebuffer, FB_W * 4);
            t_upload = SDL_GetPerformanceCounter();
            filter_draw(ren, tex, &src, dst, frame_width(), vdp_screen_height(), settings.filter);
        }
        {
            double ms = 1000.0 / freq, effects = (t_effects - t_layers) * ms;
            /* On the card the upload is part of its effects call: counted on its own. */
            if (on_gpu) effects -= gpu_fx_upload_ms;
            speed_picture(win, (t_layers - t_start) * ms, effects, on_gpu ? gpu_fx_upload_ms : (t_upload - t_effects) * ms,
                          on_gpu, hd ? iscale : 1);
            SDL_RenderFlush(ren);           /* the drawing handed to the card, for its time */
            speed.draw += (double)(SDL_GetPerformanceCounter() - t_upload) * ms;
        }
        if (settings.perf_stats) draw_stats(ren, ww, wh);
        if (window_test && (double)(SDL_GetPerformanceCounter() - start) / freq >= window_test) {
            uint32_t *shot_px = malloc((size_t)ww * wh * 4);
            if (shot_px && SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_XRGB8888, shot_px, ww * 4) == 0) png_write(window_test_png, shot_px, ww, wh, ww);
            free(shot_px);
            SDL_RendererInfo ri;
            SDL_GetRendererInfo(ren, &ri);
            printf("window test: renderer %s, GPU effects %s, internal %dx (auto quality %s), %dx%d window\n", ri.name,
                   gpu_effects_running ? "running" : "off", iscale, settings.auto_quality ? "on" : "off", ww, wh);
            running = false;
        }
        Uint64 before_present = SDL_GetPerformanceCounter();
        SDL_RenderPresent(ren);
        speed.present += (double)(SDL_GetPerformanceCounter() - before_present) * 1000 / freq;
        /* If the driver ignores vsync (OpenGL often does in a borderless fullscreen window),
         * don't spin: wait out what is left of one refresh since the last picture went out.
         * Counted from the last picture, not from this present: a picture that took longer
         * than a refresh goes straight on instead of waiting a whole refresh more. */
        static Uint64 last_shown;
        Uint64 after = SDL_GetPerformanceCounter();
        if (after - before_present < freq / 2000) {
            SDL_DisplayMode mode;
            int refresh = SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(win), &mode) == 0 && mode.refresh_rate > 0
                          ? mode.refresh_rate : 60;
            Uint64 until = last_shown + freq / (Uint64)refresh;
            if (last_shown && until > after) {
                SDL_Delay((Uint32)((until - after) * 1000 / freq));
                after = SDL_GetPerformanceCounter();
            }
        }
        last_shown = after;
    }

    save_coverage();
    save_sram();
    if (!menu_is_open()) {
        HdPost here = finishing(1, 0);
        preview_set_world(here.world, here.cam_x, here.cam_y, -1, -1);
    }
    preview_save(base_dir);
    if (!test_overrides) settings_save(ini);
    SDL_Quit();
    return 0;
}
