# Body Change NG

Change BodySlide morphs, actor skin textures, and player RaceMenu tint layers
from one native in-game UI. Body Change NG supports direct player/NPC editing,
save-specific actor results, top-down NPC distribution rules, RaceMenu rebuild
recovery, and optional OBody NG rule compatibility without replacing meshes or
silently taking over another mod's configuration.

## How this differs from OBody NG

Both mods ultimately deform a built body through RaceMenu BodyMorph and TRI
data. Body Change NG does not swap in a separate body mesh. The difference is
the selection, distribution, and state-management layer built above that
shared morph pipeline:

- **Native GUI instead of an MCM:** press F7 to manage loaded actors, bodies,
  skins, player tint, and distribution rules in one scalable ImGui window.
- **Actor-matched Body and Skin lists:** mixed CBBE 3BA and UBE installations
  are detected per actor from the live winning skin/head path. The main lists
  show the matching family instead of mixing incompatible assets. If detection
  is uncertain, the safe fallback keeps usable candidates visible.
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

## How this differs from legacy mesh-slot systems

Legacy mesh-slot systems build a separate body mesh for each predefined slot,
then use Papyrus to replace prepared ESP Skin Armor, HeadPart, and TextureSet
records as a unit. A skin slot therefore also requires its matching built mesh
and records.

Body Change NG instead uses a native SKSE architecture without predefined ESP
body slots:

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
  value captured before Body Change NG changed it.

## Main features

### Body

- Select a loaded actor and apply compatible BodySlide presets immediately.
- Low/high-weight values are interpolated using the actor's current weight.
- Live preview, committed body, and outfit correction are isolated under
  separate Body Change NG-owned morph keys.
- Conservative BodyFamily detection keeps the main list relevant without
  blocking uncertain presets or filtering distribution pools.
- CBBE 3BA and UBE use the same SliderPresets folder; `Preset/@set` and XML
  `Group` metadata identify the family.
- Body application uses RaceMenu BodyMorph; no body mesh is generated or
  replaced while the game is running.

### Skin

- Apply independent skin packs to the player or a selected loaded NPC.
- Persistent RaceMenu/NiOverride keys support body, hands, feet, face, vampire
  face, diffuse, normal, subsurface, specular, and detail textures.
- Partial packs are supported: only supplied parts and channels are changed;
  missing values keep the actor's underlying textures without cross-part substitution.
- UBE 2.0 Body/Head d-n-sk atlases are detected separately and applied to the
  live slot-53 body and face. Known-incompatible conventional and UBE skins are
  not shown or distributed to the wrong detected family.
- Cleanup is ownership-aware and does not delete another mod's texture keys.

### Player tint

- Install multiple tint packs and choose a pack from the Tint tab.
- The usable DDS for each layer is selected automatically for the player's race.
- Adjust color and opacity per layer immediately in game, or restore the exact
  RaceMenu value captured before Body Change NG's first change.
- Tint is player-only and is rebuilt after leaving RaceMenu.

### NPC distribution

- Rules are evaluated from top to bottom; the first match owns Body and Skin.
- Body and Skin can be distributed or excluded independently.
- Targets include all NPCs, custom followers, elders, exact NPC base FormIDs,
  names, factions, plugins, and races.
- Choose one fixed preset/skin or multiple stable random choices per rule.
- Rules are written by the in-game editor to
  `Data\SKSE\Plugins\BodyChangeNGdistribution.json`; manual JSON editing is
  not required. Evaluated actor results are kept in the SKSE co-save to avoid
  redistributing unchanged NPCs.
- Edit the rules in game and either distribute to loaded NPCs immediately or
  save the edits for the next game launch only.
- For a mod, Skyrim, or MO2-profile reinstall, back up
  `BodyChangeNGdistribution.json`, copy it back to the same path, and press
  **Load saved values**.

### Outfit correction

- Optional breast correction while clothed with a separate nipple toggle.
- Optional stable nipple and genital slider randomization.

### UI and input

- Korean, English, and Simplified Chinese UI.
- The default F7 menu hotkey can be rebound in Mod Settings, including Ctrl,
  Shift, and Ctrl+Shift combinations.
- Mouse, keyboard arrows/WASD, and gamepad D-pad navigation with gamepad
  confirm/cancel actions.
- Native IME text input, Backspace, searches, Body/Skin/Tint favorites with
  favorites-only filtering.

## Requirements

- Skyrim SE 1.5.97 or a verified Skyrim AE 1.6.x runtime through 1.6.1179
- Matching SKSE64
- Address Library for SKSE Plugins
- RaceMenu with BodyMorph and NiOverride

## Installation

1. Install the main archive with MO2.
2. Keep the included folder structure intact and enable the mod.
3. Launch through SKSE and press F7.

## Adding Body Presets, Skin Packs, and Tint Packs

1. **Body preset XML:** place standard BodySlide preset files at
   `CalienteTools\BodySlide\SliderPresets\*.xml` from the MO2 mod root.
2. **Skin packs:** conventional CBBE 3BA/BHUNP/UNP skins use
   `BodySkin\<pack name>\Textures\actors\character\...`. UBE 2.0 skins retain
   `BodySkin\<pack name>\Textures\!UBE\Body\femalebody_1_[d/n/sk].dds` and
   `...\!UBE\Head\femalehead_[d/n/sk].dds`. Preserve the source mod's original
   tree; each `<pack name>` folder becomes one family-matched catalog entry.
3. **Tint packs:** create one folder per pack at
   `TintMask\<pack name>\textures\actors\character\character assets\tintmasks\*.dds`
   and keep the RaceMenu tint-mask filenames and folders intact. Each
   `<pack name>` folder is one entry in the in-game catalog. Include `UBE` in
   the top-level name of UBE-only tint-mask packs; `COtR` packs are treated as
   compatible with both UBE and conventional female heads.

These paths are relative to the MO2 mod root. Assets may be placed inside
the Body Change NG mod or in separate enabled MO2 mods that provide the same
virtual paths.

You may add or replace BodySlide preset XML files, skin-pack folders, and
tint-pack folders while the game is running. Open the corresponding Body,
Skin, or Tint tab and press **Refresh** to rescan the files and update the list
without restarting the game.

The archive includes README files in every user asset folder and a valid
schema-3 distribution JSON with eight editable starter exclusions.

## Optional OBody NG JSON Compatibility

Users who want to reuse OBody NG-format distribution conditions or
outfit-correction rules may optionally use its configuration file. Body Change
NG works without OBody NG or this JSON.

- The
  [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  is a supported optional source. Its JSON and assets are not redistributed
  with Body Change NG.
- An optional OBody NG configuration may be placed beside Body Change NG's
  JSON at `Data\SKSE\Plugins\OBody_presetDistributionConfig.json`. Keep both
  files under `Data\SKSE\Plugins`; do not rename, merge, or replace
  `BodyChangeNGdistribution.json`.
- The OBody file is never read automatically at startup. **Load saved values**
  imports supported OBody distribution rules when the file exists, and
  **Register OBody NG outfit-correction rules** in the Outfit · randomization
  window registers its supported outfit-correction data.
- Use only the distribution import, only the outfit-correction registration, or
  both, depending on which OBody NG rules you want. The original OBody JSON is
  read only and is never modified.

## Compatibility

- Body Change NG does not edit NIFs, Skin Armor records, ArmorAddon records,
  or equipped slots.
- Body, Skin, and Tint selections are reapplied after RaceMenu closes.

## Credits & License

Body Change NG is licensed under
[**GNU GPLv3**](https://www.gnu.org/licenses/gpl-3.0.html). The complete source
code, build scripts, and matching version tags are available on
[**GitHub**](https://github.com/compilecraftworks/Body-Change-NG). Nexus
provides the MO2-ready Release ZIP only.

Credits to the authors of SKSE64, Address Library for SKSE Plugins, and
RaceMenu. Detailed
third-party notices and license texts are included in the distribution ZIP.
All respective rights belong to their original authors.
