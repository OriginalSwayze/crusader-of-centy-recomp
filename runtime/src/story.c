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

/* The hero's name (text code $F1) is kept as this mark in the lines read, and filled in with
 * the name the player gave ($FFFE4A, in the text's letters, ending at $FF) when they are shown. */
#define HERO ""

static const char *punct(uint8_t b) {
    switch (b) {
    case 0x3F: return ","; case 0x40: return "."; case 0x41: return "\""; case 0x42: return "!";
    case 0x46: return "'"; case 0x5E: return "?"; case 0x5F: return ":"; case 0x60: return "-";
    case 0x5D: case 0xFE: return " "; case 0xF1: return HERO;
    default: return NULL;
    }
}

/* Reads one message; names the flags it sets that have no name yet. With who_out (32 bytes)
 * and line_out (256), only gives its speaker and line instead (false if it does not read). */
static bool read_message(uint32_t a, int bank, int also, char *who_out, char *line_out) {
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
    if ((!who_out && !nflags) || unknown > 4) return false;
    while (n && text[n - 1] == ' ') text[--n] = 0;
    /* "Name:"line"": the speaker, then the line without its quotes. */
    char *speaker = "", *said = text, *colon = strchr(text, ':');
    if (colon && colon - text < 20 && (colon[1] == '"' || (colon[1] == ' ' && colon[2] == '"'))) {
        *colon = 0;
        speaker = text;
        said = colon + (colon[1] == ' ' ? 2 : 1);
    }
    /* A speaker shown by a portrait, not a name, leaves a bare ':' before the quote. */
    for (char *p = said, *q = said; ; p++) {
        if (*p == ':' && p[1] == '"' && (p == said || p[-1] == ' ')) continue;
        if (!(*q++ = *p)) break;
    }
    if (*said == '"') said++;
    size_t len = strlen(said);
    if (len && said[len - 1] == '"') said[--len] = 0;
    if (len < 4) return false;
    if (who_out) {
        snprintf(who_out, 32, "%s", speaker);
        snprintf(line_out, 256, "%s", said);
        return true;
    }
    if (also >= 0 && strstr(said, "Yes") && strstr(said, "No") && len < 16) return false;   /* only a choice */
    for (int i = 0; i < nflags; i++) {
        int f = flags[i];
        if (f < 0 || f >= FLAGS || line[f]) continue;
        who[f] = strdup(speaker);
        line[f] = strdup(said);
        bank_of[f] = bank;
        named++;
    }
    return true;
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
            for (int m = 0; m < messages; m++) read_message(group + rom16(group + (uint32_t)m * 2), b, -1, NULL, NULL);
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
        read_message(group + rom16(group + (uint32_t)m * 2), b, f, NULL, NULL);
    }
}

static bool is_set(int f) { return (bus.ram[(LIVE + f / 8) & 0xFFFF] >> (f % 8)) & 1; }

void story_gained(int flag) {
    if (flag < 0 || flag >= FLAGS) return;
    for (int i = 0; i < norder; i++) if (order[i] == flag) return;
    if (norder < FLAGS) { order[norder++] = (uint16_t)flag; dirty = true; }
}

/* The player's name for the hero, as the game shows it; "Corona" when none is set. */
static void hero_name(char *out, size_t n) {
    size_t k = 0;
    for (uint32_t a = 0xFE4A; a < 0xFE4A + 8 && k + 1 < n; a++) {
        uint8_t c = bus.ram[a];
        if (c == 0xFF) break;
        char ch = c == 0 ? ' ' : c <= 0x0A ? (char)('0' + c - 1) : c <= 0x24 ? (char)('A' + c - 0x0B) : c <= 0x3E ? (char)('a' + c - 0x25) : 0;
        if (!ch) break;
        out[k++] = ch;
    }
    while (k && out[k - 1] == ' ') k--;
    out[k] = 0;
    if (!k) snprintf(out, n, "Corona");
}

/* `src` with the hero's mark replaced by the name, into `buf`. */
static const char *with_name(const char *src, char *buf, size_t n) {
    if (!strchr(src, HERO[0])) return src;
    char name[24];
    hero_name(name, sizeof name);
    size_t k = 0;
    for (const char *p = src; *p && k + 1 < n; p++) {
        if (*p != HERO[0]) { buf[k++] = *p; continue; }
        for (const char *q = name; *q && k + 1 < n; q++) buf[k++] = *q;
    }
    buf[k] = 0;
    return buf;
}

static char shown_buf[64][2][300];
static int shown_next;

/* An event as shown: its speaker and line with the hero's name filled in. */
static StoryEvent shown(const char *who_s, const char *line_s) {
    char (*b)[300] = shown_buf[shown_next++ % 64];
    return (StoryEvent){ with_name(who_s, b[0], sizeof b[0]), with_name(line_s, b[1], sizeof b[1]) };
}

int story_events(StoryEvent *out, int max, int *done, int *total) {
    shown_next = 0;
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
        out[n++] = shown(who[f], line[f]);
    }
    /* Then the rest, latest in the cartridge's text order first. */
    for (int b = 255; b >= 0 && n < max; b--)
        for (int f = FLAGS - 1; f >= 0 && n < max; f--)
            if (line[f] && bank_of[f] == b && is_set(f) && !listed[f]) {
                listed[f] = true;
                out[n++] = shown(who[f], line[f]);
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

/* ---- Animals ---------------------------------------------------------------------------- *
 * $FFFE6A holds the animals that have joined, one bit each; $FFFE72 / $FFFE74 the two taken
 * along (Animal A and B: a bit number, 0 for none); $FFFE6E the others. Animal k is bit k,
 * 5 to 20: bank 1, group 0, message k is that animal's own line about what it does (the
 * game's animal window), so its name is the speaker. Kitty (5) revives Corona once and
 * leaves ($7674); Batty and Edgar (18, 19) leave once used. Read for the USA version only. */
#define ANIMAL_FIRST 5
#define ANIMAL_LAST  20

static char animal_name[ANIMALS][32], animal_line[ANIMALS][256], animal_shown[ANIMALS][256];
static const GameVersion *animals_for;
static bool animals_read;

static void read_animals(void) {
    if (animals_for == game) return;
    animals_for = game;
    animals_read = false;
    if (!game || strcmp(game->id, "us")) return;
    uint32_t bank = TEXT_BASE + rom32(TEXT_BASE + 1 * 4);
    uint32_t group = bank + rom16(bank);
    if (list_len(group, 2, 256) <= ANIMAL_LAST) return;
    for (int k = 0; k < ANIMALS; k++) {
        animal_name[k][0] = animal_line[k][0] = 0;
        if (!read_message(group + rom16(group + (uint32_t)(ANIMAL_FIRST + k) * 2), 1, -1, animal_name[k], animal_line[k]) || !animal_name[k][0])
            return;
    }
    animals_read = true;
}

static uint32_t ram32(uint32_t a) { return (uint32_t)bus.ram[a & 0xFFFF] << 24 | bus.ram[(a + 1) & 0xFFFF] << 16 | bus.ram[(a + 2) & 0xFFFF] << 8 | bus.ram[(a + 3) & 0xFFFF]; }
static int ram16(uint32_t a) { return bus.ram[a & 0xFFFF] << 8 | bus.ram[(a + 1) & 0xFFFF]; }

int story_animals(StoryAnimal out[ANIMALS], int *joined) {
    read_animals();
    *joined = 0;
    if (!animals_read) return 0;
    uint32_t have = ram32(0xFE6A);
    int a = ram16(0xFE72), b = ram16(0xFE74);
    for (int k = 0; k < ANIMALS; k++) {
        int bit = ANIMAL_FIRST + k;
        out[k].name = animal_name[k];
        out[k].line = with_name(animal_line[k], animal_shown[k], sizeof animal_shown[k]);
        out[k].joined = (have >> bit) & 1;
        out[k].slot = a == bit ? 1 : b == bit ? 2 : 0;
        if (out[k].slot) out[k].joined = true;
        *joined += out[k].joined;
    }
    return ANIMALS;
}

/* ---- Places ----------------------------------------------------------------------------- *
 * Text bank 0, group 3: the 21 place names (Soleil Town, Rafflesia School, ...). The world
 * map's entrances are listed at $35A2, $16 bytes each up to $FFFF: +$04 the area one arrives
 * in, +$0A its place. $FFFE7A holds the entrance area of the region the player is in; the
 * game names the place by finding it in that list ($487A), except in region $47, where the
 * area itself ($FFFE76) is looked up when it is $27, $2D, $3C, $3D or $40. */
#define PLACES     21
#define ENTRANCES  0x35A2

static char place_name[PLACES][64];
static const GameVersion *places_for;
static int places;

static void read_places(void) {
    if (places_for == game) return;
    places_for = game;
    places = 0;
    if (!game || strcmp(game->id, "us")) return;
    uint32_t bank = TEXT_BASE + rom32(TEXT_BASE);
    uint32_t group = bank + rom16(bank + 3 * 2);
    int n = list_len(group, 2, PLACES);
    for (int k = 0; k < n; k++) {
        char who[32], line[256];
        if (!read_message(group + rom16(group + (uint32_t)k * 2), 0, -1, who, line) || who[0]) break;
        snprintf(place_name[k], sizeof place_name[k], "%s", line);
        places = k + 1;
    }
}

const char *story_place_name(int index) {
    read_places();
    return index >= 1 && index < places ? place_name[index] : NULL;   /* 0 is the game's title */
}

int story_place_now(void) {
    read_places();
    if (!places) return -1;
    int region = ram16(0xFE7A), area = ram16(0xFE76);
    if (region == 0x47 && (area == 0x27 || area == 0x2D || area == 0x3C || area == 0x3D || area == 0x40)) region = area;
    for (uint32_t a = ENTRANCES; a + 0x16 < ROM_SIZE && rom16(a) != 0xFFFF; a += 0x16)
        if (rom16(a + 4) == region) {
            int p = rom16(a + 0x0A);
            return p >= 1 && p < places ? p : -1;
        }
    return -1;
}
