/* Widescreen: the scrolling maps shown wider than the original 320 pixels.
 *
 * From the disassembly (USA addresses):
 *   $188D2   the field camera: eases toward its target, clamps its left edge to
 *            [$FFA4FA, $FFA4FC], then draws one new column ($624A) or row ($62DE) of 16x16
 *            map blocks each time it crosses a block. $FFA504 / $FFA506 are the camera's block
 *            column and row, $FFA50C / $FFA50E the same in the plane (32 x 16 blocks, which
 *            wraps), $FFA502 the VRAM address of that block in plane A's table.
 *   $FF2C04  the map: a block number per word, $FFA518 bytes per row.
 *   $FF0000  the blocks: four tile words each (top left, top right, bottom left, bottom right).
 *   $FFA4DE  non-zero while the camera is held (rooms that do not scroll, scenes).
 *
 * With widescreen on (ws_extra pixels each side, ws_cols blocks to cover them):
 *   - the camera's limits come in by ws_extra, and the column it draws moves out by ws_cols
 *     (config/patches.json, recomp_rt.h);
 *   - the row draw is replaced by row_draw() below, the same blocks over 23 + 2 ws_cols
 *     columns instead of 24, starting ws_cols left of the camera's;
 *   - objects near the screen live and show in the side areas (the 18 culling patches);
 *   - when a scrolling map comes up, or the camera jumps, the visible rows are drawn again
 *     the wider way, so the sides are filled at once;
 *   - screens that are not a scrolling map (titles, story text, rooms that hold the camera,
 *     the 256-pixel screens) get black sides: they were drawn for 320 pixels only.
 * Everything is the original with ws_extra = 0 (the patches' defaults). */
#include "genesis.h"
#include "game.h"
#include "recomp_rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ws_extra, ws_cols;

static uint16_t ram16(uint32_t a) { a &= 0xFFFF; return (uint16_t)(bus.ram[a] << 8 | bus.ram[(a + 1) & 0xFFFF]); }

/* One block's four tiles into plane A's table at VRAM address addr (its top-left cell). */
static void put_block(uint32_t addr, uint32_t block) {
    uint32_t pat = (0x0000 + (block & 0x3FF) * 8) & 0xFFFF;
    for (int k = 0; k < 4; k++) {
        uint32_t at = (addr + (k >= 2 ? 0x80 : 0) + (k & 1) * 2) & 0xFFFF;
        vdp.vram[at] = bus.ram[(pat + k * 2) & 0xFFFF];
        vdp.vram[(at + 1) & 0xFFFF] = bus.ram[(pat + k * 2 + 1) & 0xFFFF];
    }
}

/* The blocks of map row (camera row + dy), from ws_cols left of the camera's column over
 * 23 + 2 ws_cols columns, as the game's row draw does for its 24 from the camera's. */
static void draw_row(int dy) {
    int stride = (int16_t)ram16(0xA518);
    int row = (int16_t)ram16(0xA506) + dy;
    int col0 = -ws_cols;
    uint32_t map = 0x2C04 + (uint32_t)(row * stride) + (uint32_t)(((int16_t)ram16(0xA504) + col0) * 2);
    int pcol = (int16_t)ram16(0xA50C) + col0;
    int prow = (int16_t)ram16(0xA50E) + dy;
    uint32_t addr = ram16(0xA502) + (uint32_t)(dy << 8);
    while (prow >= 16) { prow -= 16; addr -= 0x1000; }
    addr += (uint32_t)(col0 * 4);
    while (pcol < 0) { pcol += 32; addr += 0x80; }
    int n = 23 + 2 * ws_cols;
    for (int i = 0; i < n; i++) {
        put_block(addr, ram16(map));
        map += 2;
        addr += 4;
        if (++pcol == 32) { pcol = 0; addr -= 0x80; }
    }
}

/* Replaces the game's row draw ($62DE, called with d6 = 0 for the top row or 15 for the
 * bottom one): saves d3 in $FFA514 as it does, draws the wider row, returns. */
static uint32_t row_draw(uint32_t pc) {
    (void)pc;
    wr16(0xFFA514, D(3));
    draw_row((int16_t)D(6));
    USE_CYCLES((23 + 2 * ws_cols) * 230);      /* about what the game's own row costs */
    uint32_t ret = rd32(A(7));
    A(7) += 4;
    return ret & 0xFFFFFF;
}

/* ---- Each frame ------------------------------------------------------------------------ */
static int last_cam_x = -100000, last_map = -1;
static bool was_scrolling;
static int settle;                      /* frames left to wait for a new area's set-up */
static uint16_t settle_from;            /* $FFA502 when the map number changed */

void widescreen_frame(void) {
    if (!ws_extra) return;
    bool play = ram16(0xAC00) != 0, scrolling = play && ram16(0xA4DE) == 0 && vdp_screen_width() == 320;
    vdp_opts.mask_sides = !scrolling;
    int cam_x = (int16_t)ram16(0xA4F2), map = ram16(0xFE76);
    /* A door changes the map number first; the plane's address and the camera's blocks stay
     * the old area's until the new one is set up, some 25 frames on ($FFA502 changes then).
     * A redraw in between would draw with the old area's values, into the old area's plane:
     * where the new area draws in the other plane, that is left full of pieces (the town
     * draws in plane B at $E000, the fields beside it in plane A). So after a map change
     * nothing is redrawn until $FFA502 moves, or two seconds have passed. */
    if (map != last_map) { settle = 120; settle_from = ram16(0xA502); }
    bool redraw = scrolling && (!was_scrolling || map != last_map || cam_x - last_cam_x > 15 || last_cam_x - cam_x > 15);
    if (settle) {
        if (ram16(0xA502) != settle_from || --settle == 0) { settle = 0; redraw = scrolling; }
        else redraw = false;
    }
    if (redraw)
        for (int dy = 0; dy < 16; dy++) draw_row(dy);
    was_scrolling = scrolling;
    last_cam_x = cam_x;
    last_map = map;
}

/* Turns widescreen on with `extra` pixels each side (at most 64: the plane holds 32 blocks),
 * or off with 0. */
void widescreen_set(int extra) {
    if (extra > 64) extra = 64;
    if (extra < 0) extra = 0;
    ws_extra = extra;
    ws_cols = extra ? (extra + 15) / 16 : 0;
    vdp_opts.extra_width = extra;
    if (!extra) vdp_opts.mask_sides = false;
    was_scrolling = false;              /* redraw the sides at the next frame */
    settle = 0;
    if (game) machine_override(game->row_draw, extra ? row_draw : NULL);
}
