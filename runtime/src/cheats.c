/* Cheats: RAM values held every frame, like a Pro Action Replay. The codes come from
 * --poke on the command line and cheat_codes in settings.ini. Built-in cheats need the
 * game's RAM mapped first (its health, items and so on); none are for this game yet. */
#include "cheats.h"
#include <stdlib.h>
#include <string.h>
#include "genesis.h"
#include "settings.h"

typedef struct { uint16_t addr; uint16_t value; bool word; } Code;
static Code codes[64];                  /* --poke codes */
static int code_count;
static Code user_codes[64];             /* settings.cheat_codes */
static int user_count;
static char user_spec[sizeof settings.cheat_codes];

static int parse(const char *spec, Code *out, int *count, int cap) {
    int added = 0;
    while (*spec && *count < cap) {
        char *end;
        unsigned long a = strtoul(spec, &end, 16);
        if (*end != ':') break;
        const char *v = end + 1;
        unsigned long val = strtoul(v, &end, 16);
        if (end == v) break;
        out[(*count)++] = (Code){ (uint16_t)(a & 0xFFFF), (uint16_t)val, end - v > 2 };
        added++;
        while (*end == ' ') end++;
        spec = *end == ',' ? end + 1 : end;
        while (*spec == ' ') spec++;
    }
    return added;
}

int cheats_add_codes(const char *spec) { return parse(spec, codes, &code_count, 64); }
void cheats_clear_codes(void) { code_count = 0; }

static void apply(const Code *c, int n) {
    for (int i = 0; i < n; i++) {
        if (c[i].word) { bus.ram[c[i].addr] = (uint8_t)(c[i].value >> 8); bus.ram[(uint16_t)(c[i].addr + 1)] = (uint8_t)c[i].value; }
        else bus.ram[c[i].addr] = (uint8_t)c[i].value;
    }
}

void cheats_frame(void) {
    if (strcmp(user_spec, settings.cheat_codes)) {
        memcpy(user_spec, settings.cheat_codes, sizeof user_spec);
        user_count = 0;
        parse(user_spec, user_codes, &user_count, 64);
    }
    apply(user_codes, user_count);
    apply(codes, code_count);
}
