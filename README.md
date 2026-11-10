# Crusader of Centy Recomp

A native PC port of **Crusader of Centy (Sega Genesis)**, released in Japan as
**Shin Souseiki Ragnacenty** and in Korea as **Ragnacenty**, built from your own ROM. It
plays in every language the game came out in: English, Japanese and Korean. No game data
is included in this repository: every tool reads your ROM and checks its SHA-1 (the list
is in `config/versions.json`).

It is a static recompilation, not a decompilation: at build time the game's 68000 machine
code is translated into C from your ROM, and anything not yet covered runs on an
interpreter. It shares its recompiler and runtime with
[Beyond Oasis Recomp](https://github.com/OriginalSwayze/beyond-oasis-recomp).

## Status

| Part | State |
|---|---|
| ROM verification (`tools/setup.mjs`) | done: USA, Japan, Korea |
| 68000 decoder, code tracer, disassembly listing (`tools/disasm.mjs`) | done: ~90 KB of code in 6,551 blocks (USA), from static tracing plus the code seen running in play (`config/coverage*.txt`) |
| 68000 → C recompiler (`tools/recomp.mjs`, run by CMake) | done: 99.98% of 68000 time runs as native C on the tested route |
| Runtime: memory map, VDP, pads, save RAM, settings | done |
| Z80 + YM2612 + PSG sound | done |
| Save states and rewind | done |
| In-game menu: save/load slots with thumbnails, options, controls, reset, quit | done |
| Internal resolution 2-6x, scalers, display filters, lighting, HDR, sharpening, texture packs | done |
| Slowdown fix (68000 at 150-300%), sprite flicker fix, fast-forward | done |
| Difficulty (damage taken 50-200%), infinite health, Original / Enhanced presets | done: the hit routine is patched (`config/patches.json`), tested on real hits |
| Combat info (damage numbers, enemy health bars), rumble, play timer, stats | done (`runtime/src/combat.c`); numbers and bars not yet seen in a real fight |
| Installer (`setup/setup.c`) and one-click builder | renamed for this game, not yet released |
| Water: streams that swell, foam at the shore and reflect what stands in them (Settings > Effects) | done (`runtime/src/hd.c`): picture only, the game is untouched |
| Auto-map (GAME > Map): each map fills in as you explore, saved in `automap.bin` | done (`runtime/src/automap.c`) |
| Launcher and pause menu (`runtime/src/menu.c`): professional, in the colours of the game's title art and fields (sand, peach, moss, forest green); the launcher shows the real title logo cut from your ROM's title screen, Continue (the newest save state) and New game; Settings in pages (Display, Effects, Gameplay, Sound, Controls, General) with only player-facing options | done |
| Sound levels: the mix brought down to about -19 dBFS with no clipping (it clipped at the Beyond Oasis gain); the volume setting follows the ear (squared: 50% is about -12 dB, 10% about -40 dB) | done |
| Widescreen 16:10 and 16:9 (Settings > Display; `runtime/src/widescreen.c`, `config/patches.json`): the scrolling maps drawn wider by the game's own map code, with the camera stopping at the map edges and objects living in the side areas; other screens keep 4:3 with black sides | done; off is exactly the original |
| Smooth motion | not yet: needs which sprite belongs to which object (`runtime/src/motion.c`); the game updates 60 times a second, so it would only help above 60 Hz |

### Tested

On a scripted route from power-on through the intro, the title screen and the file
select into the hero's house (5,400 frames):

* `--verify`: 21,653,031 block runs checked against the interpreter, 0 mismatches
* `--hash-log`: native and `--interpret` runs identical every frame (RAM, VRAM, PC), for
  all three versions
* `--state-check 4300 5300`: replay identical; `--rewind-check 600`: state identical

Areas past the house have not been played yet: code first reached there runs on the
interpreter until `tools/save-coverage.mjs` adds it (see Usage).

The plan for the Enhanced Edition, item by item, is in [ROADMAP.md](ROADMAP.md).

## Usage

You need:

* Windows 10 or 11 (the menus draw their text with Windows' fonts)
* [Git](https://git-scm.com/), [Node.js](https://nodejs.org/) 18 or newer,
  [CMake](https://cmake.org/) 3.20 or newer, [Ninja](https://ninja-build.org/) and
  [Clang](https://releases.llvm.org/)
* Your own cartridge dump of one or more versions (SHA-1s in `config/versions.json`). It is
  not included and never will be.

Clone with the submodules (the third-party code below), or the build cannot find them:

```
git clone --recursive <this repository's URL>
```

Then, in the repository's folder:

```
node tools/setup.mjs "<path to a ROM>" ["<another version's ROM>" ...]
node tools/disasm.mjs
cmake -S . -B build/cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/cmake
build/cmake/CrusaderOfCenty.exe "<path to ROM>"
```

`setup.mjs` checks each ROM, finds which version it is and remembers where it is
(`local.json`, never committed); the build recompiles every version set up into
`generated/<id>/` (also never committed), all in one game. Started without a ROM, the game
looks in the `rom` folder next to it, then asks for one. The launcher comes first (Play,
Continue, New game, Settings, Language, Choose ROM); `--no-launcher` skips it once, and Settings > General > "Launcher at startup" turns it off. Performance stats, auto quality and the renderer's switches are in `settings.ini` only.

The tracer finds most of the code on its own; the rest is code it has seen running.
`config/coverage.txt` lists those addresses (addresses only, nothing from the ROM), so a
fresh build is as complete as a played one. After playing, `node tools/save-coverage.mjs`
adds what that play reached to it (`node tools/save-coverage.mjs jp` for another version:
its list is `config/coverage-jp.txt`), and the next build recompiles it.

Default controls (all remappable in the pause menu, MORE > Controls):

| Action | Keyboard | Controller (Xbox layout) |
|---|---|---|
| Move | Arrows | D-pad (the left stick always works too) |
| A / B / C | Z / X / C | X / A / B |
| Start | Enter | Start |
| Rewind (hold) | Backspace | LB |
| Fast-forward (hold) | Tab | RB |
| Pause menu | Esc | Back (Guide and Esc always work too) |

Fixed keys: F2 save state, F4 load, F6/F7 previous/next slot (1-9, files in `states/`),
F11 fullscreen. Settings live in `settings.ini` next to the executable, the game's own
saves in `save.srm` (`save-jp.srm`, `save-kr.srm` for the other versions).

## Testing

The native build must behave exactly like the interpreter. Two checks:

```
CrusaderOfCenty.exe --headless 6000 --verify <rom>        # every block vs. the interpreter: registers, flags, RAM, VDP, cycles
CrusaderOfCenty.exe --headless 6000 --hash-log a.log <rom>
CrusaderOfCenty.exe --headless 6000 --hash-log b.log --interpret <rom>
                                                          # a.log and b.log must be identical
```

The route above, through the intro and file select into the house:

```
--press "1000:S,1300:S,1600:S,1900:S,2000:C,2300:S,3400:C,3500:C,3600:C,3700:C,3800:C,3900:C,4000:C,4100:C,4200:C,4400:D:60,4500:L:80,4600:B,4700:U:120,4900:R:90,5000:C,5100:B"
```

Headless runs start from blank save RAM and never write `save.srm`. Other headless
options: `--press`, `--shot F.png`, `--shots EVERY DIR`, `--watch ADDR,...`,
`--trace-vram LO HI`, `--dump F`, `--hide-layers MASK`, `--state-check A B`,
`--rewind-check K`, `--menu-shot F.png W H` (with `--menu-tab TAB ROW`, `--controls-shot`),
`--hd-shot F.png SCALE`, `--filter-shot F.png W H MODE`, `--gpu-shot GPU.png CPU.png`,
`--bench N`, `--pc-profile N`, `--wav F`, `--overclock PERCENT`, `--save-to N`, `--load N`,
`--states-base PREFIX`, `--switch-language F ID`, `--poke FFxxxx:VV[VV],...`, `--difficulty N`,
`--infinite-health`, `--combat N`, `--combat-log` (prints every hit and defeat the combat info sees),
`--water 0|1`, `--widescreen 0|1|2`, `--trace-ram LO HI` (with `--interpret`: the PC of every write to $FFLO-$FFHI), `--launcher-shot F.png W H PAGE ROW T`, `--launcher-test N` (with `--window-test`), `--no-launcher`.

## How it fits together

* `tools/lib/m68k.mjs` decodes 68000 instructions; `tools/lib/trace.mjs` follows the code
  from the vectors, through calls, branches and the game's jump tables (word offsets,
  pointer tables, and the rows of `bra`/`jmp` that Crusader of Centy jumps into with
  `jsr T(pc,dN.w)`), plus the code listed in `config/coverage*.txt`.
* `tools/lib/codegen.mjs` turns each basic block into a C function that works on Musashi's
  CPU state with exact cycle counts, so native code and the interpreter can hand over at
  any instruction.
* `runtime/src/machine.c` runs a frame line by line, calling native blocks where there are
  some and the interpreter elsewhere; `vdp.c`, `bus.c`, `io.c`, `z80bus.c` and `sound.cpp`
  are the rest of the console.
* `config/addresses.json` and `config/patches.json` are where the game's own code is hooked
  (for widescreen, smooth motion and cheats in Beyond Oasis Recomp). They are empty here
  until those features are mapped in this game; `tools/map-versions.mjs` then finds each
  address in the Japanese and Korean versions.

## Third-party code

* [Musashi](https://github.com/kstenerud/Musashi) (MIT): 68000 interpreter fallback
* [SDL2](https://github.com/libsdl-org/SDL) (zlib): window, input, audio
* [z80](https://github.com/superzazu/z80) (MIT): Z80 sound CPU
* [ymfm](https://github.com/aaronsgiles/ymfm) (BSD-3-Clause): YM2612 FM synthesis

## License and legal

The code in this repository (the runtime and tools) is under the MIT License: see
[LICENSE](LICENSE). The third-party code above keeps its own licenses.

This is an unofficial fan project, not affiliated with or endorsed by Sega or Atlus.
Crusader of Centy and Sega are trademarks of their owners. No part of the game (code,
graphics, music or text) is included: you need your own copy, and everything made from it
stays on your computer.
