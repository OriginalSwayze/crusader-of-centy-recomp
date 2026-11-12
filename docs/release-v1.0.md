## Crusader of Centy Recomp 1.0

The first release: a native PC port of **Crusader of Centy** (Sega Genesis), built on your PC from your own cartridge. It also builds the Japanese and Korean releases (Shin Souseiki Ragnacenty, Ragnacenty), each in its own language. The game's 68000 code is translated to C at build time and runs natively, with an interpreter for anything not yet covered. Every frame checked so far matches the original exactly.

### Picture
- Internal resolution from 2x to 16x, with scalers, display filters (scanlines, CRT, LCD), sharpening and texture packs
- Widescreen 16:10 and 16:9, drawn by the game's own map code
- Lighting, an HDR look and living water that swells, foams and reflects
- **Blend dither**: the checkerboard tree shadows and haze shown as the even shade a CRT blurred them into
- **Smooth motion**: on displays faster than 60 Hz, the scenery and every character are drawn between the game's moves
- Sprite flicker fix, slowdown fix, fast-forward and rewind

### Play
- Save states with thumbnails, autosave when the story moves on, photo mode (F9)
- Difficulty (damage taken 50-200%), infinite health, walking speed, Original and Enhanced presets
- Combat info: damage numbers, enemy health bars and a boss bar; hit sparks and defeat bursts. With Inferno or Chilly along, hits throw flames or ice, and Kitty's revive glows
- **New Game+** (Settings > Gameplay): the story again, keeping Corona's maximum health and money

### Pause menu
- **Map**: each area fills in as you explore, named by the game's own places, with the doors you have used and the places found so far
- **Journal**: the story so far, each event named by the game's own line that marks it. Press right for **the animals**: who has joined, who is along, and what each one can do
- **Stats**: play time, enemies defeated and a bestiary. Press right for **18 achievements**, with a short message over the game when you earn one

The Journal, the animals, place names and the achievements that depend on them read the English text, so they need the USA cartridge. Everything else works with all three.

### Not yet
Boss Rush (the boss arenas are mapped, but two of them need the way in that the game sets up) and swaying trees. See ROADMAP.md.
