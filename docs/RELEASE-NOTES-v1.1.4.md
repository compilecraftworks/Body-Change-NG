# Body Change NG 1.1.4

Released: 6 September 2026  
Previous public release: 1.1.3

[한국어 릴리즈 노트](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.1.4/docs/RELEASE-NOTES-v1.1.4-KO.md)

## Fixes

- Fixed NPC skins that briefly appeared and then reverted after body, outfit, or 3D rebuilds.
- Automatic skin distribution now waits for the actor rebuild to settle and repaints the final Biped clone once.
- Manual NPC Skin and Default Skin choices lock immediately and are not overwritten by automatic distribution.
- Kept body, hands, feet, face, genital, and anal texture routes isolated. Missing files retain the actor's existing texture channel.
- Already-dead loaded NPCs are now eligible for body and skin rules instead of remaining on Zeroed Sliders.

## NPC Distribution editor

- Closing the panel with X, Escape, or the main window automatically saves the edited next-launch draft.
- Reopening the panel in the same session retains that draft; a save-load boundary clears the in-memory draft so it cannot leak into another save.
- Added localized **Select all** and **Clear all** controls for body-preset and skin candidate pools.
- Saved edits become active in the current session only through **Load Saved Values** or **Distribute to Loaded NPCs Immediately**.

## Performance and compatibility

- No per-frame JSON writes, directory rescans, or continuous NPC polling were added.
- Skin repaint is bounded to the affected actor and its final rebuilt clone.
- Existing CBBE 3BA, BHUNP/UNP, UBE, HIMBO, SAM, Vanilla, SOS/TNG, RaceMenu version routing, RSV/Mu Dynamic NormalMap handling, and OverlayFix compatibility paths remain intact.

## Validation and files

- Release build completed successfully and all 12 automated regression tests passed.
- `Body-Change-NG-v1.1.4.zip` — MO2-ready release package.
- `Body-Change-NG-v1.1.4-Source.zip` — source archive matching tag `v1.1.4`.
- `SHA256SUMS-v1.1.4.txt` — archive checksums.
