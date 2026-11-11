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
| Player X / Y | `$FFAC12` / `$FFAC16` (object record at `$FFAC00`, 0x54 bytes per object) | camera, effects, map |
| Health / max health (apples) | `$FFFE62` / `$FFFE60` | infinite health, HUD |
| Hit / heal routine | `$6B36`: adds `$FFB100` to the health at `$6B60` | difficulty, infinite health (`config/patches.json`), tested on a real hit |
| Camera X / Y | `$FFA4F2` / `$FFA4F6` (world pixels, picture's top left); limits `$FFA4FA`-`$FFA500`, held while `$FFA4DE` is set | combat info, map, widescreen |
| Map scrolling | `$188D2` moves the camera; `$624A` draws a column and `$62DE` a row of 16x16 blocks from the map at `$FF2C04` (blocks' tiles at `$FF0000`) | widescreen |
| Object list | `$FFC800`, 128 records of 0x40: +0 type, +2 health (sixteenths), +4 X, +8 Y (16.16, middle), +20 hit timer, +24 damage waiting | combat info, stats |
| Object update / damage | `$1C398` loops the list; `$1C45E` applies +24 to +2 at `$1C4E6`, defeat at `$1C4EE` | combat info |
| Sprite list | Built once a tick: entries (8 bytes, as the VDP's, x/y + $80) at the address in `$FFA000`, sort keys at `$FFA004`, count `$FFA008`; every entry added by `$019524` (`$019504` first drops off-screen pieces) with the drawing object in a0: an object of `$FFC800`, or `$FFB350` for the player (at `$FFAC12`/`$FFAC16`); the HUD has neither | smooth motion |
| Enemy types | `$1B2D8`, 32 bytes a type (type - 1): +0 set-up routine (writes the type, then the starting health), +$0C hitbox offset and +$10 size; spawned at `$19D3E`, health doubled when `$FFC01C` is set. Most foes start with 1-4 points; the 64-pixel ones ($0E, $1E, $36, $47, $81) and the late guardians ($60-$6A) 6-32 | boss bar |
| "In play" | the player record's first word `$FFAC00` is 0 until play starts | effects only during play |
| Current map | `$FFFE76` (6 = the house at the start; a door sets it from its object's +$1A at `$1C7DA`) | auto-map |
| Animals | `$FFFE6A` (long) the animals that have joined, bit k for animal k (5-20: Kitty, Mac, Moa, Inferno, Chilly, Flash, Dippy, Dodo, Leviathan, Wong, Cecil, Pieces, Rio, Batty, Edgar, Monarchy); `$FFFE72` / `$FFFE74` the two taken along (Animal A / B, 0 for none); `$FFFE6E` the joined ones not taken along. Script command `$030C78` adds one. Kitty is used up when Corona dies (`$7674`), Batty and Edgar after use (`$CB02`, `$CC92`). Text bank 1 group 0, message k: animal k's own line (the game's animal window) | journal |
| Place names | Text bank 0 group 3: 21 names (Soleil Town, Rafflesia School, Dahlia Valley, ... Black Night); read at `$32F6` / `$48D0` by an index. The world map's entrances are at `$35A2` ($16 bytes each: +$04 the area one arrives in, +$0A its place); `$FFFE7A` is the entrance area of the region the player is in, looked up there (`$487A`; in region $47 the area itself when $27/$2D/$3C/$3D/$40) | map: each area is tagged with its place as it is visited (`automap.bin` v4) |
| Story flags | 512 bits, live at `$FFC500`, copied into the game's save at `$FFFE0A` when it saves (`$30B98`); event scripts set them at `$3079E` (both copies) and `$30794` (live only), clear at `$307BE`. Dialogue sets most: text code `$E0 hi lo` sets a flag, `$E1 hi lo` clears one (`$03329E`); the text table is at `$1C0000` (bank, group, message; read at `$0331D0`), letters `$0B`-`$3E`. Objects run scripts (a word per command, handlers at `$0308DA`, position at +$30; `$FFFF` ends a tick): `$030CC8` gives an object a line and a flag (+$3A/+$3C/+$3E message, +$38 flag), set by `$032C44` once the line is read; `$030AC6` / `$030ADA` set / clear a flag; `$030B22` sets +$38 alone (items: `$02F2B4` sets it when picked up). 102 flags are named from dialogue and these lines (Paused > Journal). The save itself is `$FFFE00`-`$FFFEAB` with a checksum at `$FEAA` | autosave, journal |

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
| 4K / 1440p / 1080p, integer scaling, nearest-neighbour | Done: internal resolution up to 16x (3584 lines), Sharp scaler keeps pixels square |
| Optional smooth scaling, texture filtering | Done: Edge and Smooth (xBR) scalers, sharpening |
| Texture replacement, high-resolution textures | Done: texture packs (MORE > Texture pack, Dump tiles) |
| Bloom and glow, colour grading | Done: Lighting, HDR look |
| CRT, scanlines, screen curvature | Done: Filter (Scanlines, CRT with curvature, LCD) |
| Chromatic aberration, film grain | Not wanted |
| Unlocked frame rate | Done: Settings > Display > Smooth motion (above 70 Hz); the planes and each object's sprites drawn between ticks (`runtime/src/motion.c`) |
| Widescreen camera | Done: 16:10 and 16:9 on the scrolling maps; the field camera ($188D2) keeps the wider view inside the map, its column and row draws reach the sides, 18 near-the-screen checks widened (one is not found in the Japanese and Korean code, `$22E3E`) |
| Ultrawide | Hard: the plane holds 32 blocks (512 pixels), so the view can grow by 64 pixels a side at most (about 2:1 would need 80) |
| Original graphics mode, one-click compare | Done: Original / Enhanced experience presets; the menu's preview card shows before/after |

## Stage 3: Visual enhancements

| Item | Status |
|---|---|
| Dynamic lighting (torches, caves, glowing objects, glow around the player) | Engine for glow; light *sources* placed in the world need the map and object types: Map |
| Time-of-day / day-night tint | Build: the camera and "in play" are mapped; indoors and outdoors still need telling apart, or houses would darken at night |
| Enhanced water (ripples, reflections, shoreline foam) | Done: EFFECTS > Water, tuned to Centy's cyan streams, with droplets and ripples when wading (the feet's ground read from the frame's planes) |
| Falling leaves, fireflies, rain, snow | Removed: they were a Beyond Oasis-style overlay, not part of this game's look; may come back per area once areas are mapped |
| Grass, flowers and trees swaying; dust | Blocked: outdoors, plane B holds the tree tops together with the lawns, hedges and trunks, and plane A only the trees' checkerboard shadows; the sprites are characters and items. Telling a tree top from a lawn needs each area's plant tiles listed (a table of tile numbers per area), not a guess from colour |
| Fog, wind, weather by area | Build: needs the areas mapped (the map number is, `$FFFE76`) |
| Soft shadows under characters, ambient occlusion | Not needed: the characters' sprites carry their own shadow. The scenery's shadows (trees) are checkerboard dither: Settings > Display > Blend dither shows them as an even, see-through shade (`hd.c`) |
| Particles: sword sparks, enemy defeat, impacts, debris | Done: Effects > Hit effects: sparks from every hit; a flash, ring, embers and leaves from every defeat |
| Configurable particle density, shadow intensity | Density done (Hit effects: On or Lots); shadows not started |

## Stage 4: Animal companions

| Item | Status |
|---|---|
| Companion status indicators, information screen, ability descriptions, discovered list, showcase | Done: Paused > Journal, then right: joined count, Animal A / B, each animal's own line, the unmet ones hidden |
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
| Enemy / boss health bars, damage numbers | Done: PLAY > Combat info (`runtime/src/combat.c`); foes of 16 points or more get a wide boss bar at the bottom (seen with `--test-boss`; no boss save state yet) |
| Autosave at progression points | Done: GAMEPLAY > Autosave (on by default): a save state in its own slot two seconds after a story flag is newly set; Paused > Load autosave, and the launcher's Continue offers it when it is the newest |
| Movement speed option | Done: GAMEPLAY > Walking speed (Original, Faster 125%, Fastest 150%): the player's step each frame (`$9256` X, `$9280` Y) is scaled while a direction is held, so scenes keep their pace; walls still stop you |
| Gameplay statistics, playtime, death counter, defeats | Done: GAME > Stats (`stats.ini`), PLAY > Play timer |
| Improved weapon switching | Hard: the game's inventory code |
| Separate music / sound effects / ambience volume | Hard: the sound driver mixes music and effects on one chip; needs the driver's channel use mapped |
| Configurable vibration | Done: PLAY > Rumble (off, light, strong) on hits, defeats and being hit |
| HUD scaling / positioning | Hard: the HUD is drawn by the game into the same picture |
| Modern settings interface (launcher, pause menu, controls) | Done: clean and minimal, pages instead of tabs, the picture large beside the menu (`menu.c`) |
| Enhanced in-game inventory, high-resolution inventory | Hard: the inventory is the game's own screen; would be a full redraw from its RAM |
| Original interface theme | Done: the game's own interface is untouched |
| Multiple controller profiles, Steam Input | Build / works: SDL reads Steam Input's virtual controller |

## Stage 6: Additional features

| Item | Status |
|---|---|
| Interactive world map, discovered locations, completion | Started: Paused > Map fills in each area as you explore (`runtime/src/automap.c`, `automap.bin`) and shows how much of the area is explored (its size from the camera's limits); left / right go through every area visited, and each door used is marked with the area it leads to (a door is where the player was when the map number changed). Still to do: one stitched world picture, which needs the areas' places in the world (the game keeps none we have found) |
| Bestiary with defeat counts | Done: Paused > Stats shows every enemy type defeated, its picture (its own sprites, kept while it is on screen) and how many (`bestiary.bin`); names would need the types matched to the manual |
| Photo mode: pause, hide HUD, resolution, effects | Done: Paused > Photo mode or F9: the game freezes, H / Y hides the HUD (its sprites), Enter / A saves a PNG at the internal resolution with every effect to `screenshots/` |
| Photo mode camera zoom / free camera | Hard: only what the game has loaded in video memory can be shown |
| New Game+ | Done: Settings > Gameplay > New Game+ keeps max health and money (`runtime/src/ngplus.c`); a new game is set up at `$019188`; `$0058A2` (max health past 64 set to 32) patched to 64 once used. Animals are not carried: the scenes where they join check whether they already have |
| Boss Rush, boss rematch, time attack, challenge modes | Hard: needs the boss rooms and how to enter them mapped |
| Steam achievements | Out: needs a Steam release and app ID, which a build from your own ROM cannot have; local achievements (a menu page) are possible after Map |
| Remastered soundtrack, new arrangements, spatial audio | Out for the project itself (no new music can ship with it); a hook for user-supplied music packs is possible later |
| AI-generated art | Out by design: the original pixel art stays |

## Next steps, in order

1. Story flags: 102 named (Paused > Journal); the rest are items picked up (`$030B22`), script commands with no line (`$030AC6`), first visits (the table at `$38B8`) and code. Naming them needs the areas named too.
2. Vegetation swaying: needs each area's plant tiles listed (tree tops share plane B with the lawns).
3. Boss Rush (needs the boss rooms mapped).
