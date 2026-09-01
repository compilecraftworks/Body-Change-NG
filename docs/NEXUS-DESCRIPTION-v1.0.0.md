# Body Changer NG

Change BodySlide morphs, actor skin textures, and player RaceMenu tint layers
from one native in-game UI. Body Changer NG supports direct player/NPC editing,
save-specific actor results, top-down NPC distribution rules, RaceMenu rebuild
recovery, and optional OBody NG rule compatibility without replacing meshes or
silently taking over another mod's configuration.

## Main features

### Body

- Select a loaded actor and apply compatible BodySlide presets immediately.
- Low/high-weight values are interpolated using the actor's current weight.
- Conservative BodyFamily detection keeps the main list relevant without
  blocking uncertain presets or filtering distribution pools.
- Body application uses RaceMenu BodyMorph; no body mesh is generated or
  replaced while the game is running.

### Skin

- Apply independent skin packs to the player or a selected loaded NPC.
- Persistent RaceMenu/NiOverride keys support body, hands, feet, face, vampire
  face, diffuse, normal, subsurface, specular, and detail textures.
- Cleanup is ownership-aware and does not delete another mod's texture keys.

### Player tint

- Install multiple tint packs and choose a pack from the Tint tab.
- The usable DDS for each layer is selected automatically for the player's race.
- Adjust color and opacity per layer, or restore the exact RaceMenu value
  captured before Body Changer NG's first change.
- Tint is player-only and is rebuilt after leaving RaceMenu.

### NPC distribution

- Rules are evaluated from top to bottom; the first match owns Body and Skin.
- Body and Skin can be distributed or excluded independently.
- Targets include all NPCs, custom followers, elders, exact NPC base FormIDs,
  names, factions, plugins, and races.
- Choose one fixed preset/skin or multiple stable random choices per rule.
- Rules are portable in `BodyChangerNGdistribution.json`; evaluated actor
  results are kept in the SKSE co-save to avoid redistributing unchanged NPCs.

### Outfit correction

- Optional breast correction while clothed with a separate nipple toggle.
- Optional stable nipple and genital slider randomization.
- Explicitly register OBody NG exclusions, forced outfits, and outfit-specific
  correction presets from its JSON. The source file is never modified or read
  silently at startup.

### UI and camera

- Korean, English, and Simplified Chinese UI.
- Mouse, keyboard arrows/WASD, and gamepad D-pad navigation.
- Native IME text input, Backspace, searches, per-tab favorites, and resizable
  dropdowns/panes.
- Scales consistently from 1080p through 4K.
- Optional symmetric left/right character presentation and player tint close-up.
- Paused right-drag orbits the camera without rotating the actor root, avoiding
  FSMP stretching. SmoothCam is optional.
- New installations open unpaused with the character on the left; saved user
  preferences continue to take priority.

## Requirements

- Skyrim SE 1.5.97 or a verified Skyrim AE 1.6.x runtime through 1.6.1179
- Matching SKSE64
- Address Library for SKSE Plugins
- RaceMenu with BodyMorph and NiOverride
- BodySlide presets built with **Build Morphs**
- Compatible TRI data for visible body/outfit morphing

The v1.0.0 native renderer/input hooks are not enabled on Skyrim VR because the
VR-specific layouts have not yet been verified.

## Installation

1. Install the main archive with MO2.
2. Keep the included folder structure intact and enable the mod.
3. Put BodySlide XML files, skin packs, and tint packs in the included folders
   or in separate enabled MO2 mods using the same paths.
4. Launch through SKSE and press F7.

The archive includes README files in every user asset folder and a valid
schema-3 distribution JSON with eight editable starter exclusions.

## Compatibility

- OBody NG is optional. Its distribution and outfit rules are imported only
  through explicit buttons.
- SmoothCam is optional and is accessed through its public camera-control API.
- Body Changer NG does not edit NIFs, Skin Armor records, ArmorAddon records,
  equipped slots, or OBody's JSON.
- Body, Skin, and Tint selections are reapplied after RaceMenu closes.

## Updating

Private 0.2.x testers can install v1.0.0 over the previous main mod. Personal
asset packs are safest in separate MO2 mods. The final 0.2.x schema-3
distribution JSON remains compatible.

## Source and permissions

GPL-3.0. The GitHub repository and every GitHub release provide the source. A
complete source archive with pinned vendored dependencies and applicable
licenses is attached to the release.

## Credits

- OBody NG for established BodySlide distribution and ORefit behavior
- Skyrim Fitting System for GPL-3.0 menu-presentation reference work
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json
