/* Achievements: see achieve.h. */
#include "achieve.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "automap.h"
#include "combat.h"
#include "genesis.h"
#include "ngplus.h"
#include "story.h"

typedef enum { ANIMALS_N, PLACES_N, STORY_PCT, DEFEATED_N, KINDS_N, BIG_FOES_N, MAX_HEALTH, MONEY, NGPLUS, HOURS } Measure;

typedef struct { const char *key, *title, *about; Measure what; int goal; } Goal;

static const Goal goals[] = {
    { "first_friend",   "First Friend",        "An animal joins the group.",                          ANIMALS_N, 1 },
    { "menagerie",      "Menagerie",           "Eight animals have joined.",                       ANIMALS_N, 8 },
    { "every_animal",   "Friend to All",       "All sixteen animals have joined.",                 ANIMALS_N, 16 },
    { "setting_out",    "Setting Out",         "Visit three places.",                              PLACES_N, 3 },
    { "well_travelled", "Well Travelled",      "Visit ten places.",                                PLACES_N, 10 },
    { "every_place",    "Seen It All",         "Visit every place on the island.",                 PLACES_N, 20 },
    { "story_quarter",  "The Tale Begins",     "A quarter of the story events done.",              STORY_PCT, 25 },
    { "story_half",     "Halfway There",       "Half of the story events done.",                   STORY_PCT, 50 },
    { "story_most",     "Nearly a Legend",     "Nine in ten story events done.",                   STORY_PCT, 90 },
    { "hunter",         "Monster Hunter",      "Defeat 100 enemies.",                              DEFEATED_N, 100 },
    { "slayer",         "Monster Slayer",      "Defeat 1,000 enemies.",                            DEFEATED_N, 1000 },
    { "naturalist",     "Naturalist",          "Defeat twenty kinds of enemy.",                    KINDS_N, 20 },
    { "giant_slayer",   "Giant Slayer",        "Defeat a foe of 16 points or more.",               BIG_FOES_N, 1 },
    { "giants",         "Bane of Giants",      "Defeat ten foes of 16 points or more.",            BIG_FOES_N, 10 },
    { "hearty",         "Hearty",              "Reach 32 points of health.",                       MAX_HEALTH, 32 },
    { "rich",           "Well Off",            "Carry 1,000 malins.",                              MONEY, 1000 },
    { "again",          "Once More",           "Start a New Game+.",                               NGPLUS, 1 },
    { "devoted",        "Devoted",             "Play for five hours.",                             HOURS, 5 },
};
#define GOALS ((int)(sizeof goals / sizeof goals[0]))

static Achievement list[GOALS];
static bool ready, dirty;
static int tick;
static int queue[GOALS], queued;        /* met, waiting to be shown in turn */
static Uint64 shown_at;                 /* when queue[0] began to show */

static void init(void) {
    if (ready) return;
    for (int i = 0; i < GOALS; i++) list[i] = (Achievement){ goals[i].title, goals[i].about, 0 };
    ready = true;
}

static int ram16(uint32_t a) { return bus.ram[a & 0xFFFF] << 8 | bus.ram[(a + 1) & 0xFFFF]; }

static long measure(Measure m) {
    switch (m) {
    case ANIMALS_N: { StoryAnimal an[ANIMALS]; int joined = 0; story_animals(an, &joined); return joined; }
    case PLACES_N: return automap_places();
    case STORY_PCT: { StoryEvent ev[1]; int done = 0, total = 0; story_events(ev, 0, &done, &total); return total ? (long)done * 100 / total : 0; }
    case DEFEATED_N: { CombatTotals t; combat_totals(&t); return (long)t.defeated; }
    case KINDS_N: return combat_beast_count();
    case BIG_FOES_N: { CombatTotals t; combat_totals(&t); return (long)t.big_foes; }
    case MAX_HEALTH: return ram16(0xFE60);
    case MONEY: return ram16(0xFE66);
    case NGPLUS: return ngplus_runs();
    case HOURS: { CombatTotals t; combat_totals(&t); return (long)(t.play_s / 3600); }
    }
    return 0;
}

void achieve_frame(void) {
    init();
    if (++tick % 60) return;
    if (ram16(0xAC00) == 0) return;     /* not in play */
    for (int i = 0; i < GOALS; i++) {
        if (list[i].unlocked || measure(goals[i].what) < goals[i].goal) continue;
        list[i].unlocked = time(NULL);
        if (!queued) shown_at = SDL_GetPerformanceCounter();
        if (queued < GOALS) queue[queued++] = i;
        dirty = true;
    }
}

int achieve_count(void) { init(); return GOALS; }
const Achievement *achieve_get(int i) { init(); return i >= 0 && i < GOALS ? &list[i] : NULL; }

const Achievement *achieve_recent(double seconds, double *age) {
    while (queued) {
        Uint64 now = SDL_GetPerformanceCounter();
        double a = (double)(now - shown_at) / SDL_GetPerformanceFrequency();
        if (a <= seconds) { if (age) *age = a; return &list[queue[0]]; }
        memmove(queue, queue + 1, sizeof queue[0] * (size_t)--queued);   /* its turn is over: the next one */
        shown_at = now;
    }
    return NULL;
}

/* ---- achievements.ini: key=unix time --------------------------------------------------- */
void achieve_load(const char *base_dir) {
    init();
    char path[1100], line[128];
    snprintf(path, sizeof path, "%sachievements.ini", base_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = 0;
        for (int i = 0; i < GOALS; i++)
            if (!strcmp(line, goals[i].key)) list[i].unlocked = (time_t)strtoll(eq + 1, NULL, 10);
    }
    fclose(f);
}

void achieve_save(const char *base_dir) {
    if (!dirty) return;
    char path[1100];
    snprintf(path, sizeof path, "%sachievements.ini", base_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Achievements met (achieve.c): when, as seconds since 1970\n");
    for (int i = 0; i < GOALS; i++)
        if (list[i].unlocked) fprintf(f, "%s=%lld\n", goals[i].key, (long long)list[i].unlocked);
    fclose(f);
    dirty = false;
}
