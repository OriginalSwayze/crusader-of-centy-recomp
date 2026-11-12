## Crusader of Centy Recomp 1.1

A full pass over the game, in all three versions.

### Checked in every area
Every one of the game's 136 area numbers was entered from a save and walked around, in the USA, Japanese and Korean versions:
- the recompiled code behaves exactly like the original CPU, frame by frame, and with block-by-block verification
- widescreen draws the map correctly across the whole 16:9 view, from either kind of area you arrive from
- the HD picture matches the original video chip pixel for pixel, in 4:3 and 16:9
- save states and rewind restore the machine exactly
- the sound stays clear of clipping

### Faster
- **Much more of the game runs as native code**: about 10,000 recompiled blocks in each version, up from 6,500-6,800. The Japanese and Korean versions gain the most.
- **With the Sharp scaler, nothing is drawn larger than your screen shows.** 16x on a 1440p screen is now drawn at 6-8x, which looks the same and is about four times faster. On a 144 Hz screen that means 145 pictures a second instead of about 30. The rounding scalers still supersample, and photo mode still saves at the full setting.

### Fixed
- **Widescreen in the Japanese and Korean versions**: two of the checks that keep objects alive in the extra side areas were missing there. They are now in place.
- **The Journal uses your hero's name** instead of always saying "Corona".

Updating keeps your settings, saves and save states.
