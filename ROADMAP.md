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
| Hit / heal routine | `$6B36`: adds `$FFB100` to the health at `$6B60` | difficulty, infinite health (`config/patches.json`) |

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
| Time-of-day / day-night tint | Build: a colour grade over the world; needs "is this the world map" (Map) to leave menus alone |
| Enhanced water (ripples, reflections, shoreline, splashes) | Engine: the Beyond Oasis water effect finds water by its colours; Centy's water colours need picking |
| Grass, flowers, trees swaying; falling leaves; dust; fireflies | Build + Map: needs the camera so particles stay put in the world |
| Rain, snow, fog, wind, weather transitions | Build + Map: same |
| Soft shadows under characters, ambient occlusion | Build + Map: needs which sprites are characters (the object list) |
| Particles: sword sparks, enemy defeat, impacts, debris | Map: needs the enemy objects and their health |
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
| Enemy / boss health bars, damage numbers | Map: enemy object health |
| Autosave at progression points | Map: needs the story-progress flags; until then save states |
| Movement speed option | Map: the player's movement code |
| Gameplay statistics, playtime, death counter, defeats | Map: deaths are near (health reaching 0 at `$6B68`) |
| Improved weapon switching | Hard: the game's inventory code |
| Separate music / sound effects / ambience volume | Hard: the sound driver mixes music and effects on one chip; needs the driver's channel use mapped |
| Configurable vibration | Map: needs hits (health hook is there) |
| HUD scaling / positioning | Hard: the HUD is drawn by the game into the same picture |
| Enhanced interface theme, high-resolution inventory | Hard: the inventory is the game's own screen; would be a full redraw from its RAM |
| Original interface theme | Done: the game's own interface is untouched |
| Multiple controller profiles, Steam Input | Build / works: SDL reads Steam Input's virtual controller |

## Stage 6: Additional features

| Item | Status |
|---|---|
| Interactive world map, discovered locations, completion | Map: the Beyond Oasis auto-map needs Centy's map number and camera |
| Bestiary with defeat counts | Map: enemy types |
| Photo mode: pause, hide HUD, resolution, effects | Build: pause and full-resolution screenshots are close; hiding the HUD needs to know which layer it is on |
| Photo mode camera zoom / free camera | Hard: only what the game has loaded in video memory can be shown |
| New Game+, Boss Rush, boss rematch, time attack, challenge modes | Hard: new game logic on top of the original; needs the save format and story flags mapped |
| Steam achievements | Out: needs a Steam release and app ID, which a build from your own ROM cannot have; local achievements (a menu page) are possible after Map |
| Remastered soundtrack, new arrangements, spatial audio | Out for the project itself (no new music can ship with it); a hook for user-supplied music packs is possible later |
| AI-generated art | Out by design: the original pixel art stays |

## Next steps, in order

1. **Map the rest of the core RAM** by playing to the first enemies: the object list's
   enemies and their health, the camera, the map number, story flags. This unlocks most of
   the "Map" rows at once.
2. Bring back the Beyond Oasis features that only needed those addresses: combat info and
   health bars, statistics, the auto-map, smooth motion.
3. Widescreen camera (patch the map streaming and camera clamp).
4. World effects that need the camera: weather, vegetation, shadows, day/night.
5. Companion pages, photo mode, then the larger modes (New Game+, Boss Rush).
