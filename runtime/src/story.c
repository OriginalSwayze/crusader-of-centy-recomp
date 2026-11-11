/* The journal: see story.h.
 *
 * From the disassembly (USA addresses):
 *   $0331D0  shows a message: the table at $1C0000 holds a long offset per bank ($B62E),
 *            each bank a word offset per group ($B628 - 1), each group a word offset per
 *            message ($B62A), all from the table they are in
 *   text     $00 space, $01-$0A 0-9, $0B-$24 A-Z, $25-$3E a-z, $3F , $40 . $41 " $42 !
 *            $46 ' $5E ? $5F : $60 -, $5D waits for a button, $FE a new line, $FF the end,
 *            $F1 the hero's name, $E0 hi lo sets a story flag ($030786) and $E1 hi lo clears
 *            one; both skip the rest of their line
 * The tables give no counts: each list ends where the first thing it points to begins. Lines
 * that do not read as text (bytes outside the above) are left out.
 *
 * Objects run scripts: a word per command, the handler from the table at $0308DA (a long a
 * command), the arguments after it. The command handled at $030CC8 gives the object a line
 * to say and a flag (bank, group, message, flag: +$3A, +$3C, +$3E, +$38); when the player has
 * read it ($030A84 shows it, $032C44 sets the flag left in $B63E), the flag is set. So these
 * flags are named by that line too: the ROM is searched for the command with a message that
 * reads as text and a flag below 512. */
#include "story.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "genesis.h"

#define FLAGS     512
#define TEXT_BASE 0x1C0000u
#define LIVE      0xC500                /* the live flags in RAM */
#define SCRIPT_OPS   0x308DA            /* the script commands' handlers */
#define SAY_AND_FLAG 0x30CC8            /* the handler of "this line, then this flag" */

static char *who[FLAGS], *line[FLAGS];  /* NULL: not named */
static int bank_of[FLAGS];
static int named;
static const GameVersion *named_for;

static uint16_t order[FLAGS];           /* journal.bin: flags in the order they were met */
static int norder;
static bool dirty;

static uint32_t rom32(uint32_t a) { return a + 3 < ROM_SIZE ? (uint32_t)bus.rom[a] << 24 | bus.rom[a + 1] << 16 | bus.rom[a + 2] << 8 | bus.rom[a + 3] : 0; }
static uint16_t rom16(uint32_t a) { return a + 1 < ROM_SIZE ? (uint16_t)(bus.rom[a] << 8 | bus.rom[a + 1]) : 0; }

/* How many offsets a list at `a` holds: up to the nearest thing one of them points to. */
static int list_len(uint32_t a, int size, int cap) {
    uint32_t least = 0xFFFFFFFFu;
    int i;
    for (i = 0; i < cap && (uint32_t)(i * size) < least; i++) {
        uint32_t o = size == 4 ? rom32(a + (uint32_t)i * 4) : rom16(a + (uint32_t)i * 2);
        if (o && o < least) least = o;
    }
    return i;
}

static const char *punct(uint8_t b) {
    switch (b) {
    case 0x3F: return ","; case 0x40: return "."; case 0x41: return "\""; case 0x42: return "!";
    case 0x46: return "'"; case 0x5E: return "?"; case 0x5F: return ":"; case 0x60: return "-";
    case 0x5D: case 0xFE: return " "; case 0xF1: return "Corona";
    default: return NULL;
    }
}

/* Reads one message; names the flags it sets that have no name yet. */
static void read_message(uint32_t a, int bank, int also) {
    char text[900];
    int n = 0, unknown = 0, flags[8], nflags = 0;
    if (also >= 0) flags[nflags++] = also;
    for (int k = 0; k < 800 && a < ROM_SIZE; k++) {
        uint8_t c = bus.rom[a++];
        if (c == 0xFF) break;
        if (c == 0xE0 || c == 0xE1) {
            int f = rom16(a);
            a += 2;
            if (c == 0xE0 && nflags < 8) flags[nflags++] = f;
            while (a < ROM_SIZE && bus.rom[a] != 0xFE && bus.rom[a] != 0xFF) a++;
            continue;
        }
        char one[2] = { 0, 0 };
        const char *s = one;
        if (c == 0) one[0] = ' ';
        else if (c <= 0x0A) one[0] = (char)('0' + c - 1);
        else if (c <= 0x24) one[0] = (char)('A' + c - 0x0B);
        else if (c <= 0x3E) one[0] = (char)('a' + c - 0x25);
        else if (!(s = punct(c))) { unknown++; continue; }
        for (; *s && n < (int)sizeof text - 1; s++) {
            if (*s == ' ' && (n == 0 || text[n - 1] == ' ')) continue;  /* one space at a time */
            text[n++] = *s;
        }
    }
    text[n] = 0;
    if (!nflags || unknown > 4) return;
    while (n && text[n - 1] == ' ') text[--n] = 0;
    /* "Name:"line"": the speaker, then the line without its quotes. */
    char *speaker = "", *said = text, *colon = strchr(text, ':');
    if (colon && colon - text < 20 && colon[1] == '"') {
        *colon = 0;
        speaker = text;
        said = colon + 1;
    }
    /* A speaker shown by a portrait, not a name, leaves a bare ':' before the quote. */
    for (char *p = said, *q = said; ; p++) {
        if (*p == ':' && p[1] == '"' && (p == said || p[-1] == ' ')) continue;
        if (!(*q++ = *p)) break;
    }
    if (*said == '"') said++;
    size_t len = strlen(said);
    if (len && said[len - 1] == '"') said[--len] = 0;
    if (len < 4) return;
    if (also >= 0 && strstr(said, "Yes") && strstr(said, "No") && len < 16) return;   /* only a choice */
    for (int i = 0; i < nflags; i++) {
        int f = flags[i];
        if (f < 0 || f >= FLAGS || line[f]) continue;
        who[f] = strdup(speaker);
        line[f] = strdup(said);
        bank_of[f] = bank;
        named++;
    }
}

static void name_flags(void) {
    if (named_for == game) return;
    for (int f = 0; f < FLAGS; f++) { free(who[f]); free(line[f]); who[f] = line[f] = NULL; }
    named = 0;
    named_for = game;
    if (!game || strcmp(game->id, "us")) return;
    int banks = list_len(TEXT_BASE, 4, 256);
    for (int b = 0; b < banks; b++) {
        uint32_t bank = TEXT_BASE + rom32(TEXT_BASE + (uint32_t)b * 4);
        int groups = list_len(bank, 2, 64);
        for (int g = 0; g < groups; g++) {
            uint32_t group = bank + rom16(bank + (uint32_t)g * 2);
            int messages = list_len(group, 2, 256);
            for (int m = 0; m < messages; m++) read_message(group + rom16(group + (uint32_t)m * 2), b, -1);
        }
    }
    /* Scripts' "say this, then set that flag" (see the top). */
    int op = -1;
    for (int i = 0; i < 128 && op < 0; i++) if (rom32(SCRIPT_OPS + (uint32_t)i * 4) == SAY_AND_FLAG) op = i;
    if (op < 0) return;
    /* A flag that many scripts use this way is a scratch flag (an answer), not an event. */
    static uint8_t uses[FLAGS];
    memset(uses, 0, sizeof uses);
    for (uint32_t a = 0; a + 10 <= ROM_SIZE; a += 2)
        if (rom16(a) == op && rom16(a + 8) < FLAGS && uses[rom16(a + 8)] < 255) uses[rom16(a + 8)]++;
    for (uint32_t a = 0; a + 10 <= ROM_SIZE; a += 2) {
        if (rom16(a) != op) continue;
        int b = rom16(a + 2), g = rom16(a + 4), m = rom16(a + 6), f = rom16(a + 8);
        if (f >= FLAGS || line[f] || b >= banks || uses[f] > 2) continue;
        uint32_t bank = TEXT_BASE + rom32(TEXT_BASE + (uint32_t)b * 4);
        if (g >= list_len(bank, 2, 64)) continue;
        uint32_t group = bank + rom16(bank + (uint32_t)g * 2);
        if (m >= list_len(group, 2, 256)) continue;
        read_message(group + rom16(group + (uint32_t)m * 2), b, f);
    }
}

static bool is_set(int f) { return (bus.ram[(LIVE + f / 8) & 0xFFFF] >> (f % 8)) & 1; }

void story_gained(int flag) {
    if (flag < 0 || flag >= FLAGS) return;
    for (int i = 0; i < norder; i++) if (order[i] == flag) return;
    if (norder < FLAGS) { order[norder++] = (uint16_t)flag; dirty = true; }
}

int story_events(StoryEvent *out, int max, int *done, int *total) {
    name_flags();
    *total = named;
    *done = 0;
    for (int f = 0; f < FLAGS; f++) if (line[f] && is_set(f)) (*done)++;
    int n = 0;
    bool listed[FLAGS] = { false };
    for (int i = norder - 1; i >= 0 && n < max; i--) {      /* met while playing, newest first */
        int f = order[i];
        if (!line[f] || !is_set(f) || listed[f]) continue;
        listed[f] = true;
        out[n++] = (StoryEvent){ who[f], line[f] };
    }
    /* Then the rest, latest in the cartridge's text order first. */
    for (int b = 255; b >= 0 && n < max; b--)
        for (int f = FLAGS - 1; f >= 0 && n < max; f--)
            if (line[f] && bank_of[f] == b && is_set(f) && !listed[f]) {
                listed[f] = true;
                out[n++] = (StoryEvent){ who[f], line[f] };
            }
    return n;
}

/* ---- journal.bin: "CCJR", count, then the flags as 16-bit numbers ------------------------- */
#define MAGIC 0x524A4343u

void story_load(const char *base_dir) {
    char path[1100];
    snprintf(path, sizeof path, "%sjournal.bin", base_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    uint32_t head[2];
    if (fread(head, 4, 2, f) == 2 && head[0] == MAGIC && head[1] <= FLAGS)
        norder = (int)fread(order, sizeof order[0], head[1], f);
    fclose(f);
}

void story_save(const char *base_dir) {
    if (!dirty) return;
    char path[1100];
    snprintf(path, sizeof path, "%sjournal.bin", base_dir);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t head[2] = { MAGIC, (uint32_t)norder };
    fwrite(head, 4, 2, f);
    fwrite(order, sizeof order[0], (size_t)norder, f);
    fclose(f);
    dirty = false;
}
