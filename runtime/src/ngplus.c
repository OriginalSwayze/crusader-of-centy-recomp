/* New Game+: see ngplus.h.
 *
 * From the disassembly (USA addresses):
 *   $019188  sets up a new game: story flags cleared, $FFFE60 / $FFFE62 maximum health and
 *            health 8, $FFFE66 money 0, $FFFE6A animals 7 (none of the 16 yet), $FFFE76
 *            area 6
 *   $005896  keeps the maximum in range: above 64 it is set back to 32 ($0058A2). A second
 *            run that starts high and collects the health-ups again could pass 64, so once
 *            New Game+ has been used that instruction sets 64 instead (patch_health_over,
 *            config/patches.json)
 *   $0059A0  caps money at 9999
 *
 * While carried values wait, every frame that shows a game just set up (those new-game
 * values, every story flag clear) gets them; waiting ends when a story flag is set (the
 * story has begun) or a game with progress is loaded. A demo the title screen plays that
 * sets up a game the same way is covered: the real new game is set up again after it. */
#include "ngplus.h"

#include <stdio.h>
#include <string.h>

#include "genesis.h"
#include "recomp_rt.h"

int patch_health_over = 0x20;           /* what $0058A2 sets the maximum to past 64 (original 32) */

static int runs;
static bool waiting;
static uint16_t carry_max, carry_money;
static bool dirty;
static bool counted;                    /* this run is in `runs` */

static int ram16(uint32_t a) { return bus.ram[a & 0xFFFF] << 8 | bus.ram[(a + 1) & 0xFFFF]; }
static void wram16(uint32_t a, int v) { bus.ram[a & 0xFFFF] = (uint8_t)(v >> 8); bus.ram[(a + 1) & 0xFFFF] = (uint8_t)v; }

static bool flags_clear(void) {
    for (int i = 0; i < 64; i++) if (bus.ram[0xC500 + i]) return false;
    return true;
}

static bool in_play(void) { return ram16(0xAC00) != 0; }

bool ngplus_start(void) {
    if (!in_play()) return false;
    int max = ram16(0xFE60), money = ram16(0xFE66);
    carry_max = (uint16_t)(max < 8 ? 8 : max > 64 ? 64 : max);
    carry_money = (uint16_t)(money > 9999 ? 9999 : money);
    waiting = true;
    counted = false;
    return true;
}

void ngplus_frame(void) {
    if (!waiting) return;
    bool fresh = ram16(0xFE60) == 8 && ram16(0xFE66) == 0 && ram16(0xFE6A) == 0 && ram16(0xFE6C) == 7 && flags_clear();
    if (fresh) {
        wram16(0xFE60, carry_max);
        wram16(0xFE62, carry_max);
        wram16(0xFE66, carry_money);
        if (!counted) { runs++; counted = true; dirty = true; }
        patch_health_over = 0x40;
        return;
    }
    if (in_play() && !flags_clear()) waiting = false;    /* the story has begun, or a save was loaded */
}

int ngplus_runs(void) { return runs; }

void ngplus_load(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%snewgameplus.ini", base_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof line, f)) sscanf(line, "runs=%d", &runs);
    fclose(f);
    if (runs < 0) runs = 0;
    if (runs) patch_health_over = 0x40;
}

void ngplus_save(const char *base_dir) {
    if (!dirty) return;
    char path[1100];
    snprintf(path, sizeof path, "%snewgameplus.ini", base_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# New Game+ runs started (ngplus.c)\nruns=%d\n", runs);
    fclose(f);
    dirty = false;
}
