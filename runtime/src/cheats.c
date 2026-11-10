/* Cheats and difficulty: RAM values held every frame, like a Pro Action Replay (the codes
 * come from --poke on the command line and cheat_codes in settings.ini), and the damage the
 * player takes (config/patches.json, player_health_change() in recomp_rt.h).
 *
 * RAM used (from the disassembly):
 *   $FFFE60  the player's maximum health, in apples (8 at the start)
 *   $FFFE62  the player's health; the hit/heal routine at $6B36 adds $FFB100 to it at $6B60 */
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

int patch_damage = 100;
int patch_speed = 100;
static int damage_carry;                /* hundredths of a point of damage owed by earlier hits */

int scale_damage(int damage) {
    if (patch_damage <= 0) return 0;
    int total = damage * patch_damage + damage_carry;
    damage_carry = total % 100;
    return total / 100;
}

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

/* Difficulty: the percent of every hit the player takes. */
static const int difficulty_damage[4] = { 100, 50, 150, 200 };

void cheats_frame(void) {
    int d = settings.difficulty >= 0 && settings.difficulty < 4 ? settings.difficulty : 0;
    patch_damage = settings.cheat_health ? 0 : difficulty_damage[d];
    static const int walk[3] = { 100, 125, 150 };
    patch_speed = walk[settings.walk_speed >= 0 && settings.walk_speed < 3 ? settings.walk_speed : 0];
    if (patch_damage == 100) damage_carry = 0;
    if (settings.cheat_health) {
        uint16_t max = (uint16_t)(bus.ram[0xFE60] << 8 | bus.ram[0xFE61]);
        uint16_t now = (uint16_t)(bus.ram[0xFE62] << 8 | bus.ram[0xFE63]);
        if (max > 0 && max <= 0x40 && now < max) { bus.ram[0xFE62] = (uint8_t)(max >> 8); bus.ram[0xFE63] = (uint8_t)max; }
    }
    if (strcmp(user_spec, settings.cheat_codes)) {
        memcpy(user_spec, settings.cheat_codes, sizeof user_spec);
        user_count = 0;
        parse(user_spec, user_codes, &user_count, 64);
    }
    apply(user_codes, user_count);
    apply(codes, code_count);
}
