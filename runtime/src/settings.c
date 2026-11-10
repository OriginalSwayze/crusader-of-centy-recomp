#include "settings.h"
#include "genesis.h"
#include "filter.h"
#include "input.h"
#include "texpack.h"
#include "game.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Settings settings = {
    .rom_path = "",
    .window_scale = 4,
    .internal_scale = 1,
    .filter = FILTER_OFF,
    .overclock = 100,
    .rewind = true,
    .rewind_mb = 64,
    .state_slot = 1,
    .show_launcher = true,
    .volume = 60,
    .avx2 = true,
};

bool gpu_effects_running, cpu_has_avx2;

static const char *filter_keys[FILTER_COUNT] = { "off", "scanlines", "crt", "lcd" };
static const char *filter_names[FILTER_COUNT] = { "Off", "Scanlines", "CRT", "LCD" };

static char base_dir[1024];
void settings_set_base(const char *dir) { snprintf(base_dir, sizeof base_dir, "%s", dir ? dir : ""); }

static bool parse_bool(const char *v) { return !strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "on"); }

_Static_assert(SETTINGS_VERSIONS == GAME_VERSION_COUNT, "settings.h: one ROM per version");

/* config/versions.json order, for the versions this build does not have too. */
static const char *const version_ids[SETTINGS_VERSIONS] = { "us", "jp", "kr" };
static int version_index(const char *id) {
    for (int i = 0; i < SETTINGS_VERSIONS; i++) if (!strcmp(id, version_ids[i])) return i;
    return -1;
}

static int clampi(int v, int lo, int hi, int fallback) { return v < lo || v > hi ? fallback : v; }

void settings_load(const char *path) {
    input_defaults();
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1200];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == ';') continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "rom_path")) snprintf(settings.rom_path, sizeof settings.rom_path, "%s", v);
        else if (!strcmp(k, "language")) snprintf(settings.language, sizeof settings.language, "%s", v);
        else if (!strncmp(k, "rom_", 4) && version_index(k + 4) >= 0)
            snprintf(settings.rom_paths[version_index(k + 4)], sizeof settings.rom_paths[0], "%s", v);
        else if (!strcmp(k, "fullscreen")) settings.fullscreen = parse_bool(v);
        else if (!strcmp(k, "window_scale")) settings.window_scale = atoi(v);
        else if (!strcmp(k, "integer_scale")) settings.integer_scale = parse_bool(v);
        else if (!strcmp(k, "internal_scale")) settings.internal_scale = atoi(v);
        else if (!strcmp(k, "scaler")) settings.scaler = !strcmp(v, "smooth") ? 1 : !strcmp(v, "sharp") ? 2 : 0;
        else if (!strcmp(k, "filter")) {
            for (int i = 0; i < FILTER_COUNT; i++) if (!strcmp(v, filter_keys[i])) settings.filter = i;
        }
        else if (!strcmp(k, "sharpen")) settings.sharpen = atoi(v);
        else if (!strcmp(k, "no_sprite_limit")) settings.no_sprite_limit = parse_bool(v);
        else if (!strcmp(k, "lighting")) settings.lighting = atoi(v);
        else if (!strcmp(k, "hdr_look")) settings.hdr = atoi(v);
        else if (!strcmp(k, "water")) settings.water = parse_bool(v);
        else if (!strcmp(k, "texture_pack")) snprintf(settings.texture_pack, sizeof settings.texture_pack, "%s", v);
        else if (!strcmp(k, "dump_tiles")) settings.dump_tiles = parse_bool(v);
        else if (!strcmp(k, "difficulty")) settings.difficulty = atoi(v);
        else if (!strcmp(k, "cheat_health")) settings.cheat_health = parse_bool(v);
        else if (!strcmp(k, "combat_info")) settings.combat_info = atoi(v);
        else if (!strcmp(k, "rumble")) settings.rumble = atoi(v);
        else if (!strcmp(k, "play_timer")) settings.play_timer = parse_bool(v);
        else if (!strcmp(k, "fast_speed")) settings.fast_speed = atoi(v);
        else if (!strcmp(k, "rewind")) settings.rewind = parse_bool(v);
        else if (!strcmp(k, "rewind_mb")) settings.rewind_mb = atoi(v);
        else if (!strcmp(k, "overclock")) settings.overclock = atoi(v);
        else if (!strcmp(k, "cheat_codes")) snprintf(settings.cheat_codes, sizeof settings.cheat_codes, "%s", v);
        else if (!strcmp(k, "volume")) settings.volume = atoi(v);
        else if (!strcmp(k, "audio")) settings.audio = atoi(v);
        else if (!strcmp(k, "state_slot")) settings.state_slot = atoi(v);
        else if (!strcmp(k, "show_launcher")) settings.show_launcher = parse_bool(v);
        else if (!strcmp(k, "perf_stats")) settings.perf_stats = parse_bool(v);
        else if (!strcmp(k, "auto_quality")) settings.auto_quality = parse_bool(v);
        else if (!strcmp(k, "merged_pass")) settings.merged_pass = parse_bool(v);
        else if (!strcmp(k, "avx2")) settings.avx2 = parse_bool(v);
        else if (!strcmp(k, "gpu_effects")) settings.gpu_effects = parse_bool(v);
        else input_ini_load(k, v);     /* key_* and pad_*; anything else (an older file's) is dropped */
    }
    fclose(f);
    settings.window_scale = clampi(settings.window_scale, 1, 10, 4);
    if (settings.internal_scale < 1 || settings.internal_scale > 6 || settings.internal_scale == 5) settings.internal_scale = 1;
    settings.scaler = clampi(settings.scaler, 0, 2, 0);
    settings.filter = clampi(settings.filter, 0, FILTER_COUNT - 1, FILTER_OFF);
    settings.sharpen = clampi(settings.sharpen, 0, 3, 0);
    settings.lighting = clampi(settings.lighting, 0, 2, 0);
    settings.hdr = clampi(settings.hdr, 0, 2, 0);
    settings.difficulty = clampi(settings.difficulty, 0, 3, 0);
    settings.combat_info = clampi(settings.combat_info, 0, 2, 0);
    settings.rumble = clampi(settings.rumble, 0, 2, 0);
    settings.fast_speed = clampi(settings.fast_speed, 0, 2, 0);
    settings.rewind_mb = clampi(settings.rewind_mb, 8, 2048, 64);
    settings.overclock = clampi(settings.overclock, 100, 400, 100);
    settings.volume = clampi(settings.volume, 0, 100, 60);
    settings.audio = clampi(settings.audio, 0, 2, 0);
    settings.state_slot = clampi(settings.state_slot, 1, 9, 1);
    /* The language picks the ROM played (an older settings.ini has only rom_path). */
    int lang = version_index(settings.language);
    if (lang >= 0 && settings.rom_paths[lang][0]) snprintf(settings.rom_path, sizeof settings.rom_path, "%s", settings.rom_paths[lang]);
}

bool settings_use_rom(const char *path) {
    const GameVersion *v = bus_rom_version(path);
    if (!v) return false;
    int i = version_index(v->id);
    snprintf(settings.rom_paths[i], sizeof settings.rom_paths[0], "%s", path);
    snprintf(settings.rom_path, sizeof settings.rom_path, "%s", path);
    snprintf(settings.language, sizeof settings.language, "%s", v->id);
    return true;
}

/* The languages to choose from, in config/versions.json order. */
static int languages(int *out) {
    int n = 0;
    for (int i = 0; i < SETTINGS_VERSIONS; i++) if (game_versions[i] && settings.rom_paths[i][0]) out[n++] = i;
    return n;
}

int settings_language_count(void) {
    int list[SETTINGS_VERSIONS];
    return languages(list);
}

void settings_save(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Crusader of Centy Recomp settings\n");
    fprintf(f, "rom_path=%s\n", settings.rom_path);
    fprintf(f, "language=%s\n", settings.language);
    for (int i = 0; i < SETTINGS_VERSIONS; i++)
        if (settings.rom_paths[i][0]) fprintf(f, "rom_%s=%s\n", version_ids[i], settings.rom_paths[i]);
    fprintf(f, "\n# Display\n");
    fprintf(f, "fullscreen=%d\n", settings.fullscreen);
    fprintf(f, "window_scale=%d\n", settings.window_scale);
    fprintf(f, "integer_scale=%d\n", settings.integer_scale);
    fprintf(f, "internal_scale=%d\n", settings.internal_scale);
    fprintf(f, "scaler=%s\n", settings.scaler == 2 ? "sharp" : settings.scaler ? "smooth" : "edge");
    fprintf(f, "filter=%s\n", filter_keys[settings.filter]);
    fprintf(f, "sharpen=%d\n", settings.sharpen);
    fprintf(f, "no_sprite_limit=%d\n", settings.no_sprite_limit);
    fprintf(f, "\n# Effects\n");
    fprintf(f, "lighting=%d\n", settings.lighting);
    fprintf(f, "hdr_look=%d\n", settings.hdr);
    fprintf(f, "water=%d\n", settings.water);
    fprintf(f, "texture_pack=%s\n", settings.texture_pack);
    fprintf(f, "dump_tiles=%d\n", settings.dump_tiles);
    fprintf(f, "\n# Gameplay\n");
    fprintf(f, "difficulty=%d\n", settings.difficulty);
    fprintf(f, "cheat_health=%d\n", settings.cheat_health);
    fprintf(f, "combat_info=%d\n", settings.combat_info);
    fprintf(f, "rumble=%d\n", settings.rumble);
    fprintf(f, "play_timer=%d\n", settings.play_timer);
    fprintf(f, "fast_speed=%d\n", settings.fast_speed);
    fprintf(f, "rewind=%d\n", settings.rewind);
    fprintf(f, "rewind_mb=%d\n", settings.rewind_mb);
    fprintf(f, "overclock=%d\n", settings.overclock);
    fprintf(f, "# Extra RAM codes held every frame: FFxxxx:VV (byte) or FFxxxx:VVVV (word), comma-separated\n");
    fprintf(f, "cheat_codes=%s\n", settings.cheat_codes);
    fprintf(f, "\n# Sound\n");
    fprintf(f, "volume=%d\n", settings.volume);
    fprintf(f, "audio=%d\n", settings.audio);
    fprintf(f, "\n# System\n");
    fprintf(f, "state_slot=%d\n", settings.state_slot);
    fprintf(f, "show_launcher=%d\n", settings.show_launcher);
    fprintf(f, "perf_stats=%d\n", settings.perf_stats);
    fprintf(f, "auto_quality=%d\n", settings.auto_quality);
    fprintf(f, "merged_pass=%d\n", settings.merged_pass);
    fprintf(f, "avx2=%d\n", settings.avx2);
    fprintf(f, "gpu_effects=%d\n", settings.gpu_effects);
    fprintf(f, "\n# Controls\n");
    input_ini_save(f);
    fclose(f);
}

/* ---- Options shown by the launcher and the in-game menu ---------------------------- */
static const int rewind_sizes[] = { 32, 64, 128, 256, 512 };
static const int overclocks[] = { 100, 150, 200, 300 };
static const char *sharpen_names[4] = { "Off", "Light", "Medium", "Strong" };
static const char *on_off(bool v) { return v ? "On" : "Off"; }

void settings_option_text(int id, char *out, size_t n) {
    switch (id) {
    case OPT_WINDOW_ALL:
        if (settings.fullscreen) snprintf(out, n, "Fullscreen");
        else snprintf(out, n, "Window %d\xC3\x97", settings.window_scale);
        break;
    case OPT_INTEGER:    snprintf(out, n, "%s", on_off(settings.integer_scale)); break;
    case OPT_INTERNAL:
        if (settings.internal_scale <= 1) snprintf(out, n, "Native");
        else if (settings.internal_scale == 6) snprintf(out, n, "6\xC3\x97 (4K)");
        else snprintf(out, n, "%d\xC3\x97", settings.internal_scale);
        break;
    case OPT_SCALER:     snprintf(out, n, "%s", settings.scaler == 2 ? "Sharp" : settings.scaler ? "Smooth (xBR)" : "Edge"); break;
    case OPT_FILTER:     snprintf(out, n, "%s", filter_names[settings.filter]); break;
    case OPT_SHARPEN:    snprintf(out, n, "%s", sharpen_names[settings.sharpen]); break;
    case OPT_FLICKER:    snprintf(out, n, "%s", on_off(settings.no_sprite_limit)); break;
    case OPT_LIGHTING:   snprintf(out, n, "%s", settings.lighting == 2 ? "Bright" : settings.lighting ? "Soft" : "Off"); break;
    case OPT_HDR:        snprintf(out, n, "%s", settings.hdr == 2 ? "Strong" : settings.hdr ? "On" : "Off"); break;
    case OPT_WATER:      snprintf(out, n, "%s", on_off(settings.water)); break;
    case OPT_TEXPACK:    snprintf(out, n, "%s", settings.texture_pack[0] ? settings.texture_pack : "None"); break;
    case OPT_DUMP:       snprintf(out, n, "%s", on_off(settings.dump_tiles)); break;
    case OPT_DIFFICULTY: {
        static const char *names[4] = { "Original", "Easy", "Hard", "Very hard" };
        snprintf(out, n, "%s", names[settings.difficulty & 3]);
        break;
    }
    case OPT_CHEAT_HEALTH: snprintf(out, n, "%s", on_off(settings.cheat_health)); break;
    case OPT_COMBAT:     snprintf(out, n, "%s", settings.combat_info == 2 ? "Numbers and bars" : settings.combat_info ? "Numbers" : "Off"); break;
    case OPT_RUMBLE:     snprintf(out, n, "%s", settings.rumble == 2 ? "Strong" : settings.rumble ? "Light" : "Off"); break;
    case OPT_TIMER:      snprintf(out, n, "%s", on_off(settings.play_timer)); break;
    case OPT_FAST:       snprintf(out, n, "%d\xC3\x97", settings.fast_speed + 2); break;
    case OPT_REWIND_ALL:
        if (!settings.rewind) snprintf(out, n, "Off");
        else snprintf(out, n, "%d MB", settings.rewind_mb);
        break;
    case OPT_OVERCLOCK:
        if (settings.overclock <= 100) snprintf(out, n, "Off");
        else snprintf(out, n, "%d%% CPU", settings.overclock);
        break;
    case OPT_VOLUME:     snprintf(out, n, "%d%%", settings.volume); break;
    case OPT_AUDIO:      snprintf(out, n, "%s", settings.audio == 2 ? "Rich" : settings.audio ? "Clean" : "Original"); break;
    case OPT_LANGUAGE: {
        int i = version_index(settings.language);
        snprintf(out, n, "%s", i >= 0 && game_versions[i] ? game_versions[i]->language : "English");
        break;
    }
    case OPT_LAUNCHER:   snprintf(out, n, "%s", on_off(settings.show_launcher)); break;
    case OPT_PERF_STATS: snprintf(out, n, "%s", on_off(settings.perf_stats)); break;
    case OPT_AUTO_QUALITY: snprintf(out, n, "%s", on_off(settings.auto_quality)); break;
    case OPT_GPU:        snprintf(out, n, "%s", !settings.gpu_effects ? "Off" : gpu_effects_running ? "On" : "On (from the next start)"); break;
    case OPT_MERGED:     snprintf(out, n, "%s", on_off(settings.merged_pass)); break;
    case OPT_AVX2:       snprintf(out, n, "%s", !settings.avx2 ? "Off" : cpu_has_avx2 || !gpu_effects_running ? "On" : "On (not on this CPU)"); break;
    default:             snprintf(out, n, "%s", ""); break;
    }
}

void settings_set_volume_fraction(float v) {
    int x = (int)(v * 20 + .5f) * 5;
    settings.volume = x < 0 ? 0 : x > 100 ? 100 : x;
}

int settings_cycle(int v, int count, int dir) { return ((v + dir) % count + count) % count; }

bool settings_option_is_switch(int id) {
    switch (id) {
    case OPT_INTEGER: case OPT_FLICKER: case OPT_WATER: case OPT_DUMP: case OPT_CHEAT_HEALTH: case OPT_TIMER:
    case OPT_LAUNCHER: case OPT_PERF_STATS: case OPT_AUTO_QUALITY: case OPT_GPU: case OPT_MERGED: case OPT_AVX2:
        return true;
    default:
        return false;
    }
}

void settings_option_change(int id, int dir) {
    switch (id) {
    case OPT_WINDOW_ALL: {
        int cur = settings.fullscreen ? 0 : settings.window_scale - 1;      /* 0 = fullscreen, 1-5 = 2x-6x */
        if (cur < 0 || cur > 5) cur = 3;
        cur = settings_cycle(cur, 6, dir);
        settings.fullscreen = cur == 0;
        if (cur) settings.window_scale = cur + 1;
        break;
    }
    case OPT_INTEGER:    settings.integer_scale = !settings.integer_scale; break;
    case OPT_INTERNAL: {                /* Native, 2x, 3x, 4x, 6x */
        static const int scales[5] = { 1, 2, 3, 4, 6 };
        int at = 0;
        for (int k = 0; k < 5; k++) if (scales[k] == settings.internal_scale) at = k;
        settings.internal_scale = scales[settings_cycle(at, 5, dir)];
        break;
    }
    case OPT_SCALER:     settings.scaler = settings_cycle(settings.scaler, 3, dir); break;
    case OPT_FILTER:     settings.filter = settings_cycle(settings.filter, FILTER_COUNT, dir); break;
    case OPT_SHARPEN:    settings.sharpen = settings_cycle(settings.sharpen, 4, dir); break;
    case OPT_FLICKER:    settings.no_sprite_limit = !settings.no_sprite_limit; break;
    case OPT_LIGHTING:   settings.lighting = settings_cycle(settings.lighting, 3, dir); break;
    case OPT_HDR:        settings.hdr = settings_cycle(settings.hdr, 3, dir); break;
    case OPT_WATER:      settings.water = !settings.water; break;
    case OPT_TEXPACK: {                 /* None, then each folder in texture_packs/ */
        static char names[32][64];
        int n = texpack_list(base_dir, names, 32), at = 0;
        for (int k = 0; k < n; k++) if (!strcmp(names[k], settings.texture_pack)) at = k + 1;
        at = settings_cycle(at, n + 1, dir);
        snprintf(settings.texture_pack, sizeof settings.texture_pack, "%s", at ? names[at - 1] : "");
        break;
    }
    case OPT_DUMP:       settings.dump_tiles = !settings.dump_tiles; break;
    case OPT_DIFFICULTY: settings.difficulty = settings_cycle(settings.difficulty, 4, dir); break;
    case OPT_CHEAT_HEALTH: settings.cheat_health = !settings.cheat_health; break;
    case OPT_COMBAT:     settings.combat_info = settings_cycle(settings.combat_info, 3, dir); break;
    case OPT_RUMBLE:     settings.rumble = settings_cycle(settings.rumble, 3, dir); break;
    case OPT_TIMER:      settings.play_timer = !settings.play_timer; break;
    case OPT_FAST:       settings.fast_speed = settings_cycle(settings.fast_speed, 3, dir); break;
    case OPT_REWIND_ALL: {
        int cur = 0;                    /* 0 = off, 1.. = rewind_sizes */
        if (settings.rewind) for (int i = 0; i < 5; i++) if (rewind_sizes[i] == settings.rewind_mb) cur = i + 1;
        if (settings.rewind && !cur) cur = 2;
        cur = settings_cycle(cur, 6, dir);
        settings.rewind = cur > 0;
        if (cur) settings.rewind_mb = rewind_sizes[cur - 1];
        break;
    }
    case OPT_OVERCLOCK: {
        int cur = 0;
        for (int i = 0; i < 4; i++) if (overclocks[i] == settings.overclock) cur = i;
        settings.overclock = overclocks[settings_cycle(cur, 4, dir)];
        break;
    }
    case OPT_VOLUME:                    /* stops at the ends instead of wrapping around */
        settings.volume += dir * 5;
        settings.volume = settings.volume < 0 ? 0 : settings.volume > 100 ? 100 : settings.volume;
        break;
    case OPT_AUDIO:      settings.audio = settings_cycle(settings.audio, 3, dir); break;
    case OPT_LANGUAGE: {                /* the next version with a ROM (switching it restarts the game) */
        int list[SETTINGS_VERSIONS], n = languages(list), at = 0;
        if (n < 2) break;
        for (int k = 0; k < n; k++) if (list[k] == version_index(settings.language)) at = k;
        int i = list[settings_cycle(at, n, dir)];
        snprintf(settings.language, sizeof settings.language, "%s", version_ids[i]);
        snprintf(settings.rom_path, sizeof settings.rom_path, "%s", settings.rom_paths[i]);
        break;
    }
    case OPT_LAUNCHER:   settings.show_launcher = !settings.show_launcher; break;
    case OPT_PERF_STATS: settings.perf_stats = !settings.perf_stats; break;
    case OPT_AUTO_QUALITY: settings.auto_quality = !settings.auto_quality; break;
    case OPT_GPU:        settings.gpu_effects = !settings.gpu_effects; break;
    case OPT_MERGED:     settings.merged_pass = !settings.merged_pass; break;
    case OPT_AVX2:       settings.avx2 = !settings.avx2; break;
    }
}

/* ---- Presets -------------------------------------------------------------------------- */
/* Everything as the cartridge was: native resolution, no effects, the original speed,
 * sprite limit and difficulty. Controls, sound, saves and the language stay as they are. */
void settings_preset_original(void) {
    settings.internal_scale = 1;
    settings.scaler = 0;
    settings.filter = FILTER_OFF;
    settings.sharpen = 0;
    settings.lighting = 0;
    settings.hdr = 0;
    settings.water = false;
    settings.no_sprite_limit = false;
    settings.overclock = 100;
    settings.integer_scale = false;
    settings.difficulty = 0;
    settings.cheat_health = false;
    settings.texture_pack[0] = 0;
}

/* The recommended enhancements: the art redrawn at 3x with rounded edges and light
 * sharpening, a soft glow, the HDR look and living water, no sprite flicker and no
 * slowdown. Gameplay (difficulty, cheats) is left as the player set it. */
void settings_preset_enhanced(void) {
    settings.internal_scale = 3;
    settings.scaler = 0;
    settings.sharpen = 1;
    settings.lighting = 1;
    settings.hdr = 1;
    settings.water = true;
    settings.no_sprite_limit = true;
    settings.overclock = 200;
}
