# Body Changer NG

Change BodySlide morphs, actor skin textures, and player RaceMenu tint layers
from one native in-game UI. Body Changer NG supports direct player/NPC editing,
save-specific actor results, top-down NPC distribution rules, RaceMenu rebuild
recovery, and optional OBody NG rule compatibility without replacing meshes or
silently taking over another mod's configuration.

## How this differs from OBody NG

Both mods ultimately deform a built body through RaceMenu BodyMorph and TRI
data. Body Changer NG does not swap in a separate body mesh. The difference is
the selection, distribution, and state-management layer built above that
shared morph pipeline:

- **Native GUI instead of an MCM:** press F7 to manage loaded actors, bodies,
  skins, player tint, and distribution rules in one scalable ImGui window.
- **Gamepad navigation:** move through rows and tabs with the D-pad, then select
  or close with the primary and cancel face buttons.
- **Isolated live preview:** committed body, preview, and outfit correction use
  separate owned morph keys. Superseded rapid selections are discarded so an
  older task cannot overwrite the latest click.
- **Explicit rule pools:** choose exactly which body presets and skin packs each
  rule may distribute instead of automatically treating every installed preset
  as a candidate.
- **Independent Body and Skin decisions:** distribute or exclude either
  category while the other category continues to use its selected pool.
- **Visible top-down priority:** the first matching rule owns both decisions,
  making broad-to-specific ordering straightforward to inspect and edit.
- **In-game distribution editing:** edit targets, sex, priority, Body/Skin
  pools, and each category's distribute/exclude mode in the native GUI.
- **Apply now or next launch:** immediately reevaluate currently loaded NPCs,
  or save the edits for the next game launch without changing this session.
- **Save-specific results without full repeat work:** selected results and
  application signatures are kept in the SKSE co-save so unchanged actors do
  not need a complete redistribution on every load.
- **One integrated workflow:** actor skin overrides, player tint detail,
  RaceMenu rebuild recovery, and body distribution share the same UI and state
  model rather than being separate tools.
- **Favorites:** star Body presets, Skin packs, and Tint packs, then show only
  those favorites in each catalog.

## How this differs from the legacy BodyChange

The legacy **BodyChange - A Multi-Bodyshape System** builds a separate body
mesh for each `CustomSet`, then uses Papyrus to replace predeclared ESP Skin
Armor, HeadPart, and TextureSet records as a unit. A skin slot therefore also
requires the matching built mesh and records.

Body Changer NG is not a port of that ESP/Papyrus system. It uses a different
native SKSE architecture:

- **Skin is independent of mesh sets:** it keeps the actor's existing Skin
  Armor, HeadPart, and NIFs, and changes only supported texture channels on the
  exact loaded body, hand, foot, and face parts through RaceMenu/NiOverride.
- **Body and Skin are freely composable:** body shape is a BodyMorph preset;
  skin is a separate texture override. Either can change without forcing a
  matching copy of the other.
- **No fixed `CustomSet1`-`CustomSet20` slots:** catalogs are built from the
  installed BodySlide XML files and skin/tint folders.
- **Runtime installation and refresh:** add a BodySlide XML file or a skin/tint
  folder while Skyrim is running, press that tab's Refresh button, and select
  it without rebuilding an ESP slot or restarting the game.
- **Player and NPC support:** direct selection, conditional NPC Body/Skin
  distribution, and save-specific evaluated results share one implementation.
- **In-game rules with immediate distribution:** edit conditions and pools
  without leaving Skyrim, then apply them to loaded NPCs immediately.
- **Direct tint editing:** choose player tint assets, immediately edit each
  layer's color and opacity, and restore an individual layer to the RaceMenu
  value captured before Body Changer NG changed it.

## Main features

### Body

- Select a loaded actor and apply compatible BodySlide presets immediately.
- Low/high-weight values are interpolated using the actor's current weight.
- Live preview, committed body, and outfit correction are isolated under
  separate Body Changer NG-owned morph keys.
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
- Adjust color and opacity per layer immediately in game, or restore the exact
  RaceMenu value captured before Body Changer NG's first change.
- Tint is player-only and is rebuilt after leaving RaceMenu.

### NPC distribution

- Rules are evaluated from top to bottom; the first match owns Body and Skin.
- Body and Skin can be distributed or excluded independently.
- Targets include all NPCs, custom followers, elders, exact NPC base FormIDs,
  names, factions, plugins, and races.
- Choose one fixed preset/skin or multiple stable random choices per rule.
- Rules are portable in `BodyChangerNGdistribution.json`; evaluated actor
  results are kept in the SKSE co-save to avoid redistributing unchanged NPCs.
- Edit the rules in game and either distribute to loaded NPCs immediately or
  save the edits for the next game launch only.

### Outfit correction

- Optional breast correction while clothed with a separate nipple toggle.
- Optional stable nipple and genital slider randomization.
- Explicitly register OBody NG exclusions, forced outfits, and outfit-specific
  correction presets from its JSON. The source file is never modified or read
  silently at startup.
- The
  [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  `OBody_presetDistributionConfig.json` is a supported optional import source.
  Its file and assets are not redistributed with Body Changer NG.

### UI and camera

- Korean, English, and Simplified Chinese UI.
- Mouse, keyboard arrows/WASD, and gamepad D-pad navigation with gamepad
  confirm/cancel actions.
- Native IME text input, Backspace, searches, Body/Skin/Tint favorites with
  favorites-only filtering, and resizable dropdowns/panes.
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

If Skyrim is already running, newly added BodySlide XML files and skin/tint
folders are discovered by pressing the corresponding tab's **Refresh** button.

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

Body Changer NG's own code is GPL-3.0. The GitHub repository and every GitHub
release provide the source. A complete source archive with pinned vendored
dependencies and applicable licenses is attached to the release. Referenced
mods and compatible JSON files remain under their respective authors'
copyright and licenses and are not bundled with Body Changer NG.

## Credits

- OBody NG for established BodySlide distribution and ORefit behavior
- [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  by SlickSilk, as an optional JSON-format compatibility and validation target
- Skyrim Fitting System for GPL-3.0 menu-presentation reference work
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json
