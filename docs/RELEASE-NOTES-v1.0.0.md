# Body Change NG v1.0.0

Body Change NG v1.0.0 is the first stable public release of the native SKSE
body, skin, and player tint manager.

## Highlights

- Change BodySlide morphs and persistent skin textures for the player or a
  selected loaded NPC from one in-game UI.
- Detect CBBE 3BA and UBE per actor in mixed installations, show matching
  Body/Skin/Tint assets, and apply UBE Body/Head skin atlases to the correct
  live slot-53 body and face targets.
- Detect Argonian and Khajiit female/male skin folders and match direct
  preview, committed reapply, and NPC distribution to the actor's race and sex.
- Apply player RaceMenu tint packs, edit color/opacity by layer, and restore the
  captured original values.
- Build top-down body/skin NPC distribution rules with independent distribute
  and exclude modes.
- Coalesce automatic NPC events in both normal and performance modes and defer
  RaceMenu partition rebuilds so dense cell loads do not synchronously morph
  every NPC in one frame. Performance mode adds another scheduling interval.
- Keep evaluated actor results in the SKSE co-save while rules remain in a
  portable global JSON.
- Reapply committed body, skin, and tint state after RaceMenu closes.
- Repair legacy non-UBE RaceMenu `.jslot` head references after
  `BodyChange.esp` is removed, with an adjacent backup and High Poly
  Head/vanilla fallback.
- Register OBody NG distribution and complete outfit-correction compatibility
  only when requested.
- Use Korean, English, or Simplified Chinese UI with mouse, keyboard, and
  gamepad navigation at resolutions through 4K.

## Camera and input

The optional third-person presentation uses symmetric left/right framing and a
separate tint close-up. It restores the original camera target, position,
facing, FOV, projection, and current/target zoom on actor change or menu close.
Paused right-drag orbits the camera rather than rotating the actor root, which
avoids FSMP stretching without modifying Skyrim's pause counter. SmoothCam is
optional and accessed only through its public API.

A new installation opens unpaused and frames the character on the left by
default. Existing saved preferences continue to take priority.

## Files

- `Body Change NG v1.0.0.zip` — installable MO2 archive
- `Body Change NG v1.0.0 Source.zip` — complete corresponding source with
  pinned vendored dependencies and licenses

## Requirements

- Skyrim SE 1.5.97 or a verified Skyrim AE 1.6.x runtime through 1.6.1179
- Matching SKSE64 and Address Library
- RaceMenu with BodyMorph/NiOverride
- BodySlide Build Morphs and compatible TRI data for body/outfit morphing
