/* settings.ini next to the executable; shared by the launcher and the in-game menu. */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

/* One ROM per version of the game (game.h's GAME_VERSION_COUNT, config/versions.json order). */
#define SETTINGS_VERSIONS 3

typedef struct {
    char rom_path[1024];                /* the ROM played: the one for `language` */
    char rom_paths[SETTINGS_VERSIONS][1024];   /* each version's ROM, "" if not chosen */
    char language[8];                   /* the version played (its id: us, jp, kr) */
    /* Display */
    int  widescreen;                    /* 0 off (4:3), 1 16:10, 2 16:9: the scrolling maps shown wider (widescreen.c) */
    bool fullscreen;
    int  window_scale;
    bool integer_scale;
    int  internal_scale;                /* 1 = native; 2, 3, 4 or 6 = layers redrawn at that scale (hd.c) */
    int  scaler;                        /* layer scaler: 0 edge (Scale2x/3x), 1 smooth (xBR rules), 2 sharp (plain blocks) */
    int  filter;                        /* FILTER_* (filter.h): off, scanlines, CRT, LCD */
    int  sharpen;                       /* sharpening: 0 off, 1 light, 2 medium, 3 strong (hd_sharpen) */
    bool no_sprite_limit;               /* the sprite flicker fix */
    /* Effects */
    int  lighting;                      /* glow of bright pixels: 0 off, 1 soft, 2 bright (hd_post) */
    int  hdr;                           /* HDR look: 0 off, 1 on, 2 strong (hd_post) */
    int  particles;                     /* hit sparks and defeat bursts: 0 off, 1 on, 2 lots (combat.c) */
    bool water;                         /* water swells, foams, glints and reflects (hd_water) */
    char texture_pack[64];              /* folder in texture_packs/, empty for none (texpack.h) */
    bool dump_tiles;                    /* write every tile shown to texture_packs/dump/ */
    /* Gameplay */
    int  difficulty;                    /* damage taken: 0 original, 1 easy (half), 2 hard (1.5x), 3 very hard (2x) */
    bool cheat_health;                  /* infinite health: no damage, health held full */
    int  combat_info;                   /* 0 off, 1 damage numbers, 2 numbers and enemy health bars (combat.c) */
    int  rumble;                        /* controller rumble on hits: 0 off, 1 light, 2 strong (combat.c) */
    bool play_timer;                    /* this session's play time in the picture's corner (combat.c) */
    int  fast_speed;                    /* fast-forward while its key is held: 0 2x, 1 3x, 2 4x */
    bool rewind;                        /* keep a rewind buffer (hold the rewind key to step back) */
    int  rewind_mb;                     /* rewind buffer size; frames cost ~1 KB, so 64 MB is many minutes */
    int  overclock;                     /* 68000 speed in percent (100 = original): removes slowdown */
    char cheat_codes[512];              /* extra RAM codes, "FFxxxx:VV[VV],..." (settings.ini only) */
    /* Sound */
    int  volume;                        /* 0-100 (main.c squares it: loudness is heard logarithmically) */
    int  audio;                         /* sound output: 0 original, 1 clean, 2 rich (sound_quality) */
    /* System */
    int  state_slot;                    /* save state slot, 1-9 */
    bool show_launcher;                 /* show the launcher at startup (--no-launcher skips it once) */
    bool perf_stats;                    /* pictures a second and each step's time, in the corner (main.c) */
    bool auto_quality;                  /* lower the internal resolution while pictures fall behind the display (main.c) */
    bool merged_pass;                   /* lighting, HDR and sharpening in one pass over the picture (hd.c) */
    bool avx2;                          /* the pixel loops' AVX2 versions, on CPUs that have it (hd.c) */
    bool gpu_effects;                   /* lighting, HDR and sharpening on the graphics card (gpu.c); from the next start */
    int  bind_key[11];                  /* input.h IN_*: SDL scancode for each action */
    int  bind_pad[11];                  /* input.h IN_*: SDL game controller button for each action */
} Settings;

extern Settings settings;

void settings_load(const char *path);
void settings_save(const char *path);

/* Options the launcher and the in-game menu show: a value label, and stepping it. */
enum {
    OPT_WIDESCREEN, OPT_WINDOW_ALL,     /* fullscreen or a window size, in one option */
    OPT_INTEGER, OPT_INTERNAL, OPT_SCALER, OPT_FILTER, OPT_SHARPEN, OPT_FLICKER,
    OPT_LIGHTING, OPT_HDR, OPT_PARTICLES, OPT_WATER, OPT_TEXPACK, OPT_DUMP,
    OPT_DIFFICULTY, OPT_CHEAT_HEALTH, OPT_COMBAT, OPT_RUMBLE, OPT_TIMER, OPT_FAST,
    OPT_REWIND_ALL,                     /* rewind off or its buffer size, in one option */
    OPT_OVERCLOCK,
    OPT_VOLUME, OPT_AUDIO,
    OPT_LANGUAGE,                       /* the version played: each has its own language */
    OPT_LAUNCHER, OPT_PERF_STATS, OPT_AUTO_QUALITY, OPT_GPU, OPT_MERGED, OPT_AVX2,
};
/* Set by the game when it knows: the GPU effects are running (the renderer is OpenGL and
 * the shader built), and whether the CPU has AVX2. The launcher leaves them false. */
extern bool gpu_effects_running, cpu_has_avx2;
/* The two presets: everything as the cartridge was, or the recommended enhancements. */
void settings_preset_original(void);
void settings_preset_enhanced(void);
void settings_set_volume_fraction(float v);         /* sets volume from a slider (0-1), in steps of 5 */
void settings_option_text(int id, char *out, size_t n);
void settings_option_change(int id, int dir);
/* True for the options that are only on or off; others, like Filter or Sharpening, can read
 * "Off" too but have more steps. */
bool settings_option_is_switch(int id);
int  settings_cycle(int v, int count, int dir);     /* (v + dir) wrapped into 0..count-1 */

/* Makes the ROM at path the one played, if it is a version this build has: remembers it as
 * that version's ROM and switches the language to it. False if it is none of them. */
bool settings_use_rom(const char *path);
/* How many languages there are to choose from: versions this build has, with a ROM. */
int  settings_language_count(void);

/* Pixels of the world added on each side for the widescreen setting (0 when off). */
int  settings_extra_width(void);

/* The folder next to the executable (texture packs are listed under it). */
void settings_set_base(const char *dir);

#endif
