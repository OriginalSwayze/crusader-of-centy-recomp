# Roadmap: Crusader of Centy, Enhanced Edition

"Preserve the original. Enhance everything around it."

This sorts the Enhanced Edition wishlist by where each item stands. Every enhancement stays
optional: **MORE > Original experience** in the pause menu puts the cartridge back as it
was in one press.

Status markers:

* **Done**: in the game now.
* **Engine**: the shared runtime can do it; Centy needs a setting or a little tuning.
* **Map**: needs a part of the game's RAM or code located first (see "Mapped so far").
* **Build**: new work in the renderer or menus, no game knowledge needed.
* **Hard**: possible, but large or with open questions.
* **Out**: not possible or not appropriate for this project, with the reason.

## Mapped so far

| What | Where (USA) | Used for |
|---|---|---|
| Player X / Y | `$FFAC12` / `$FFAC16` (object record at `$FFAC00`, 0x54 bytes per object) | camera, effects, map, health bars (next) |
| Health / max health (apples) | `$FFFE62` / `$FFFE60` | infinite health, HUD |
| Hit / heal routine | `$6B36`: adds `$FFB100` to the health at `$6B60` | difficulty, infinite health (`config/patches.json`), tested on a real hit |
| Camera X / Y | `$FFA4F2` / `$FFA4F6` (world pixels, picture's top left) | combat info; next: world effects, map |
| Object list | `$FFC800`, 128 records of 0x40: +0 type, +2 health (sixteenths), +4 X, +8 Y (16.16, middle), +20 hit timer, +24 damage waiting | combat info, stats |
| Object update / damage | `$1C398` loops the list; `$1C45E` applies +24 to +2 at `$1C4E6`, defeat at `$1C4EE` | combat info |
| "In play" | the player record's first word `$FFAC00` is 0 until play starts | effects only during play |
| Current map | `$FFFE76` (6 = the house at the start; a door sets it from its object's +$1A at `$1C7DA`) | auto-map |

Hits come from `subq.w #1,($FFB100)` (one apple) and `sub.w d0,($FFB100)` at `$1C94E`
(bigger hits). The Japanese and Korean addresses are found automatically
(`tools/map-versions.mjs`).

## Stage 1: Core recompilation

| Item | Status |
|---|---|
| Native Windows executable, original game functionality | Done: 99.98% native, checked block by block against the interpreter |
| Stable game timing | Done: exact cycle timing, 60 Hz game logic at any refresh rate |
| Modern resolutions, keyboard and controller, audio | Done |
| Save system improvements | Done: 9 save-state slots with thumbnails, rewind, cartridge saves in `save.srm` |

## Stage 2: Rendering

| Item | Status |
|---|---|
| 4K / 1440p / 1080p, integer scaling, nearest-neighbour | Done: internal resolution up to 6x, Sharp scaler keeps pixels square |
| Optional smooth scaling, texture filtering | Done: Edge and Smooth (xBR) scalers, sharpening |
| Texture replacement, high-resolution textures | Done: texture packs (MORE > Texture pack, Dump tiles) |
| Bloom and glow, colour grading | Done: Lighting, HDR look |
| CRT, scanlines, screen curvature | Done: Filter (Scanlines, CRT with curvature, LCD) |
| Chromatic aberration, film grain | Build: two small passes in `hd.c` |
| Unlocked frame rate | Engine: frames draw at the display's rate; moving sprites between game ticks (smooth motion) needs the sprite-to-object link: Map |
| Widescreen camera | Map, Hard: the game's map streaming and camera clamp must be patched, as Beyond Oasis Recomp does |
| Ultrawide | follows widescreen |
| Original graphics mode, one-click compare | Done: Original / Enhanced experience presets; the menu's preview card shows before/after |

## Stage 3: Visual enhancements

| Item | Status |
|---|---|
| Dynamic lighting (torches, caves, glowing objects, glow around the player) | Engine for glow; light *sources* placed in the world need the map and object types: Map |
| Time-of-day / day-night tint | Build: the camera and "in play" are mapped; indoors and outdoors still need telling apart, or houses would darken at night |
| Enhanced water (ripples, reflections, shoreline foam) | Done: EFFECTS > Water, tuned to Centy's cyan streams; splashes when wading in still to do |
| Falling leaves, fireflies | Done: EFFECTS > Weather (`runtime/src/fx.c`) |
| Grass, flowers and trees swaying; dust | Build: needs the plants found in the scenery |
| Rain, snow | Done: EFFECTS > Weather (rain with an overcast tone and splashes) |
| Fog, wind, weather changing by area | Build: weather is one setting for the whole game for now |
| Soft shadows under characters, ambient occlusion | Build + Map: needs which sprites are characters (the object list) |
| Particles: sword sparks, enemy defeat, impacts, debris | Build: enemy health and defeats are mapped (combat.c), so these can hang off them |
| Configurable particle density, shadow intensity | follows the above |

## Stage 4: Animal companions

| Item | Status |
|---|---|
| Companion status indicators, information screen, ability descriptions, discovered list, showcase | Map: find the companion slots and the "met" flags, then a menu page |
| Quick companion selection | Map, Hard: must go through the game's own selection code to keep it safe |
| Companion particle effects, visual feedback on abilities | Map |
| New companion animations, idle animations, reactions, better following | Out for now: these need new sprite frames and changes to the game's AI, which would alter the original artwork and behaviour; possible later as an optional texture/animation pack |

## Stage 5: Interface and quality of life

| Item | Status |
|---|---|
| Remappable controls, Xbox / PlayStation / most controllers, keyboard | Done |
| Quick-save / quick-load, save anywhere | Done: F2 / F4 save states work anywhere |
| Difficulty: Original, Easy, Hard, Very hard | Done: PLAY > Difficulty (damage taken 100 / 50 / 150 / 200%) |
| Infinite health | Done: PLAY > Infinite health |
| Enemy / boss health bars, damage numbers | Done, to be confirmed in a fight: PLAY > Combat info (`runtime/src/combat.c`) |
| Autosave at progression points | Map: needs the story-progress flags; until then save states |
| Movement speed option | Map: the player's movement code |
| Gameplay statistics, playtime, death counter, defeats | Done: GAME > Stats (`stats.ini`), PLAY > Play timer |
| Improved weapon switching | Hard: the game's inventory code |
| Separate music / sound effects / ambience volume | Hard: the sound driver mixes music and effects on one chip; needs the driver's channel use mapped |
| Configurable vibration | Done: PLAY > Rumble (off, light, strong) on hits, defeats and being hit |
| HUD scaling / positioning | Hard: the HUD is drawn by the game into the same picture |
| Interface in the game's own look (launcher, pause menu, controls) | Done: dialogue-box frames, apple switches, sword cursor, storybook headings, animated launcher (`theme.c`) |
| Enhanced in-game inventory, high-resolution inventory | Hard: the inventory is the game's own screen; would be a full redraw from its RAM |
| Original interface theme | Done: the game's own interface is untouched |
| Multiple controller profiles, Steam Input | Build / works: SDL reads Steam Input's virtual controller |

## Stage 6: Additional features

| Item | Status |
|---|---|
| Interactive world map, discovered locations, completion | Started: GAME > Map fills in each map as you explore (`runtime/src/automap.c`, `automap.bin`); a world overview, markers and completion still to do |
| Bestiary with defeat counts | Map: enemy types |
| Photo mode: pause, hide HUD, resolution, effects | Build: pause and full-resolution screenshots are close; hiding the HUD needs to know which layer it is on |
| Photo mode camera zoom / free camera | Hard: only what the game has loaded in video memory can be shown |
| New Game+, Boss Rush, boss rematch, time attack, challenge modes | Hard: new game logic on top of the original; needs the save format and story flags mapped |
| Steam achievements | Out: needs a Steam release and app ID, which a build from your own ROM cannot have; local achievements (a menu page) are possible after Map |
| Remastered soundtrack, new arrangements, spatial audio | Out for the project itself (no new music can ship with it); a hook for user-supplied music packs is possible later |
| AI-generated art | Out by design: the original pixel art stays |

## Next steps, in order

1. Confirm combat info in a real fight; find the story flags.
2. Smooth motion (needs which sprites belong to which object).
3. Widescreen camera (patch the map streaming and camera clamp).
4. World effects that need the camera: weather, vegetation, shadows, day/night.
5. Companion pages, photo mode, then the larger modes (New Game+, Boss Rush).
