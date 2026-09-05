# Changelog

All notable public changes to Body Change NG are documented here.

## 1.1.2 — 2026-09-05

### Interface theme

- Changes the native UI foundation to black, charcoal, and grayscale across windows, child panels, popups, title bars, idle controls, inputs, cards, tables, scrollbars, resize grips, and navigation surfaces.
- Keeps the existing blue selection, hover, and pressed states plus warning, success, progress, favorite, incompatibility, and live tint-preview colors as functional highlights.
- Shortens the nearby-actor refresh button to `Refresh actors`, with matching Korean and Simplified Chinese labels; its loaded-actor refresh behavior is unchanged.
- Preserves UI layout and scaling, keyboard/gamepad behavior, camera presentation, catalogs, distribution rules, settings, JSON and co-save compatibility, and all body, skin, tint, and outfit-correction paths.

### CBBE 3BA anatomy safety boundary

- Limits clothed breast/nipple correction to verified CBBE 3BA actors. UBE and ambiguous female body families are skipped instead of receiving guessed 3BA-compatible or experimental UBE slider values.
- Limits stable nipple/genital randomization to CBBE 3BA NPCs and labels both controls as NPC options in Korean, English, and Simplified Chinese.
- Shows an in-game UBE warning and per-control tooltips. The global controls remain available in mixed installations so an UBE player can be skipped while CBBE 3BA NPCs continue to use the enabled options.
- Clears a previously owned outfit-correction layer once when an unsupported female family is encountered, then caches the no-op result so later equipment events add no repeated work.

### Catalog guidance and UBE BodySkin routing

- Falls back from UBE body slot 53 to the standard body slot 32 only when the live actor has no usable slot-53 body target. This fixes UBE profiles that changed the face but not the body without applying one body profile to two geometry routes.
- Restores the full `Body Presets`, `Body Skins`, and `Tint Masks` tab names.
- Adds Korean, English, and Simplified Chinese empty-list guidance for each catalog, including exact placement paths and the Refresh action.
- Clarifies in the UI, packaged TintMask guide, and Nexus descriptions that tint-mask-based facial tints are supported while overlay-based tints are not.
- Uses English names for the eight bundled starter rules and for runtime regeneration of those defaults. Existing saved user rule names are not rewritten.

### Validation

Release build and all 12 regression test executables passed. The theme and family-policy checks add no per-frame scanning or file work; unsupported UBE anatomy paths now stop before morph application.

## 1.1.1 — 2026-09-05

### NPC distribution and saved-state recovery

- Verifies a manually assigned or rule-distributed NPC's live body and skin once after loading. Missing results are reapplied, while already-correct actors are skipped.
- Coalesces overlapping actor initialization, cell-attach, and equipment work, invalidates stale session work on load, and bounds retries for unavailable 3D.
- Records an application as complete only after it succeeds.

### Body presets and outfit correction

- Prevents presets from accumulating on top of RaceMenu 3BA MORPHS values or repeated distribution. XML-omitted compatible sliders target zero, while morph keys owned by other mods remain intact.
- Uses the same absolute-result path for preview, confirmation, and NPC distribution and prevents stale work from replacing the current selection.
- Recalculates outfit correction after body changes against the complete evaluated morph result.
- Separates CBBE 3BA and UBE 2.0 breast/nipple correction and stable nipple/genital randomization by the actor's live body family.

### BodySkin application and compatibility

- Routes body, hand, and foot geometry separately in multi-part Skin Armor and prevents body textures from being copied onto hands or feet.
- Partial packs replace only supplied parts and diffuse, normal, subsurface, or specular channels; absent content keeps the actor's current textures.
- Repairs selected skin parts after equipment rebuilds, including looting dead NPCs, and rebuilds missing BCNG cache files from the source pack.
- Supports actor-matched CBBE 3BA and UBE 2.0 profiles, standard female and male skins, SOS male genital textures, Argonian/Khajiit skins, and optional elder/race face variants.
- Adds bounded compatibility for Racial Skin Variance and Mu Dynamic NormalMap companion files.
- Improves compatibility with OverlayFix by tightening actor-update and asynchronous-work boundaries.

### Responsiveness, performance, and stability

- Prioritizes direct body, skin, and tint selections while preserving fair automatic-distribution service within the frame budget.
- Prepares only the selected actor's required skin files on one background worker, keeps the latest request, and reuses prepared cache files.
- Deduplicates full DDS hashing when MO2 exposes one backing file through both physical and virtual Data paths; large per-file diagnostics are debug-only.
- Limits full DDS content reads to initial catalog loading or explicit Refresh and detects changed XML/DDS content under the same ID.
- Releases completed callback ownership immediately, times out a RaceMenu SE callback batch that never returns, and rejects late or superseded follow-up work.
- Avoids equipment-event work for actors using neither BodySkin nor outfit correction and reduces unnecessary hot-path settings/rule copies.

### Validation

Release build and all 12 regression test executables passed, including restored actor state, task/callback lifetime, body-family isolation, partial-skin routing, catalog refresh, and MO2 path-alias deduplication. Existing rules, JSON, settings, co-save identifiers, starter exclusions, favorites, and camera values are preserved.

## 1.1.0 — 2026-09-04

### Body presets and mixed-body installations

- Body presets now target an absolute shape across the compatible family's slider set. Omitted XML sliders target zero; repeated preview, confirmation, and NPC distribution no longer add the same shape on top of an existing one.
- Uses BCNG-owned compensation instead of globally clearing morphs. Other mods' morph keys are not deleted, and the preview and committed/NPC paths keep separate ownership.
- Detects CBBE 3BA and UBE per actor from the loaded skin/head evidence. Preset set and Group metadata drive XML classification; ambiguous and multi-family presets retain safe display fallbacks.
- Keeps user-selected rule pools intact; runtime body/skin selection checks the matched actor's sex and known family. Outfit-related names are not blanket-excluded from the body catalog.

### Skin coverage and material routing

- Corrects male skin-path handling and hand/foot targeting, including multi-slot skin armor whose geometry is not found through the first biped slot.
- Partial skin packs work in preview, confirmation, and NPC distribution. Only supplied body/hand/foot/face parts and diffuse, normal, subsurface, or specular channels are replaced; missing values keep the underlying texture without cross-part or cross-channel substitution.
- Adds per-actor UBE 2.0 Body/Head atlas routing and female/male Argonian and Khajiit skin matching, including matching tail geometry.
- Applies femaleold and humanoid race-specific face-normal files only to matching actors and only where files exist. Astrid/Afflicted-specific textures and tint-mask DDS inside BodySkin are excluded from body-skin application.
- Routes CBBE 3BA femalebody_etc_v2_1 to its shared vagina/anal atlas and BHUNP/UNP BakaUNP/VaginalAnalCanal2 to matching vagina/anal/canal geometry, separately from regular body textures.
- Supports optional SOS Smurf Average, VectorPlexus Regular, and VectorPlexus Muscular slot-52 textures from the skin pack's original SOS directory. Addon/race/elder variants follow the live material; Muscular uses the shipped Regular-channel inheritance where appropriate.
- Separates known UBE/conventional player tint packs while retaining safe fallbacks for uncertain family detection. Tint remains a player-only feature.

### NPC distribution and performance

- Adds keyword, class, and combat-style rule targets alongside existing conditions. Faction dropdowns include unnamed forms using EditorID/plugin/local-ID labels.
- Stores faction, race, keyword, class, and combat-style targets as plugin plus local FormID in schema 4. Existing schema-3 rules remain readable and are migrated on save.
- Preserves the eight starter exclusions: custom followers and elders of both sexes remain body-only exclusions; Argonians and Khajiit of both sexes remain skin-only exclusions.
- Coalesces automatic actor/equipment work in both normal and performance modes, defers RaceMenu partition updates, and skips unnecessary outfit-morph rebuilds. Performance mode adds an extra scheduling interval.
- Moves the first loaded-NPC pass out of the serialization/RaceMenu/overlay load-callback burst by two game-task turns. No recurring file scan or timer is added; catalog-derived slider sets are cached.

### Migration, UI, and release

- Repairs visible non-UBE RaceMenu .jslot files with obsolete BodyChange.esp face HeadParts after the legacy plugin is removed. Creates an adjacent backup and prefers the available High Poly Head target, otherwise the vanilla target; UBE custom-head presets are preserved.
- Renames the main catalog tabs to Body Presets, Body Skins, and Tint Masks; refreshes Korean and English release documentation.
- Retains existing settings, co-save identities, ownership namespaces, and the BodyChangeNGdistribution.json filename. Version 1.1.0 does not rename or reset them.

### Validation

Validation: Release build and ten regression test executables. File/NIF evidence and automated tests cover routing and state logic; in-game atlas appearance, collision behavior, OverlayFix crash compatibility, and stutter-free gameplay are not certified by these tests.

## 1.0.0 — 2026-09-02

First stable public release.

### Body

- Applies BodySlide presets through RaceMenu BodyMorph without replacing body
  meshes at runtime.
- Interpolates low/high-weight preset values using the actor's current weight.
- Detects the selected actor's body family conservatively and filters only the
  main Body tab. Uncertain or multi-family presets remain visible through safe
  fallbacks; NPC distribution pools are never auto-filtered.
- Keeps outfit-named presets such as Clothes, Outfit, Bikini, Armor, Cuirass,
  Dress, Panty, and Overalls visible. Only exact `-Refit` presets are reserved
  for outfit correction.

### Skin

- Applies persistent RaceMenu/NiOverride texture overrides independently per
  actor without editing NIF, Skin Armor, or equipment records.
- Supports body, hands, feet, face, vampire face, diffuse, normal, subsurface,
  specular, and compatible FaceGen detail textures.
- Tracks ownership precisely so cleanup removes only Body Change NG's keys.

### Player tint

- Adds player-only tint packs with automatic race-appropriate DDS selection.
- Supports independent color/opacity adjustment and restoration for every
  active supported tint layer.
- Restores the color swatch and detail picker together with the world tint.
- Reapplies the selected pack, detail edits, and restored original layers after
  RaceMenu rebuilds the player's tint arrays.

### NPC distribution and persistence

- Adds top-down, first-match body/skin rules for all NPCs, custom followers,
  elders, names, NPC base FormIDs, factions, plugins, and races.
- Allows Body and Skin to be distributed or excluded independently in one rule.
- Includes eight editable starter exclusions: custom followers and elders are
  body-only exclusions for both sexes; Argonians and Khajiit are skin-only
  exclusions for both sexes.
- Uses stable per-actor pool selection and stores evaluated actor results in the
  SKSE co-save. Global rules remain in
  `Data\SKSE\Plugins\BodyChangeNGdistribution.json`.
- Resolves persistent NPC identities across load-order changes and avoids full
  redistribution of unchanged actors on every load.
- Imports OBody distribution rules only through the explicit editor action.

### Outfit correction and randomization

- Corrects supported breast sliders while clothed, with a separate nipple
  correction toggle.
- Imports OBody NG outfit-name/plugin/FormID exclusions, forced corrections,
  and female/male outfit-specific preset mappings without modifying the source
  JSON.
- Resolves an outfit-specific mapping before current-body `-Refit`, sex-wide
  fallback, and procedural correction.
- Adds stable optional nipple and genital shape randomization.

### UI, input, and camera

- Adds scalable Korean, English, and Simplified Chinese ImGui UI for 1080p,
  1440p, 2K, and 4K displays.
- Adds mouse, keyboard arrows/WASD, and gamepad D-pad navigation with confirm and
  cancel actions; shortcuts are suspended while a text field owns focus.
- Supports native Korean IME input, Backspace, configurable modifier hotkeys,
  per-tab favorites, searches, resizable dropdowns, and resizable distribution
  panes.
- Uses one-click live selection, double-click confirmation without closing, and
  confirmation of the final live row when the window closes.
- Adds optional left/right third-person character presentation and tint close-up
  with deterministic FOV/zoom restoration.
- Opens unpaused with the character framed on the left and the main window
  anchored from screen center toward the right by default on a new
  installation; existing saved preferences remain unchanged.
- Keeps paused right-drag rotation camera-only to avoid FSMP stretching, uses
  SmoothCam's public camera-control API when available, and avoids additive FOV
  and pause-counter manipulation.
- Fixes post-processing/DLSS-dependent UI tone changes by moving engine camera
  refreshes out of the ImGui render pass.

### Reliability

- Uses `BodyChangeNG` consistently for the DLL, log, settings directory,
  distribution JSON, morph keys, texture-cache namespace, source targets, and
  release archives. Valid legacy `BodyChangerNG` settings and distribution
  files are migrated to the new paths, while old morph and texture-override
  ownership remains recognized so existing saves do not stack or leak state.
- Reapplies committed Body, Skin, and Tint state after RaceMenu closes, with
  generation checks that prevent superseded asynchronous work from winning.
- Coalesces equipment and actor work, stores ActorHandles instead of raw actor
  pointers, clears detached actor preview/apply state, and resets all transient
  state on a new save session.
- Ships a valid schema-3 starter JSON and README files in every user asset
  folder.
- Verified by the release build and nine regression test executables covering
  actor state, asset catalogs, body-family classification, hotkeys, outfit
  rules, path migration, preset parsing, runtime layouts, and skin-override
  ownership.

## Pre-release development history

The 0.1.x and 0.2.x archives were private validation builds used to stabilize
runtime application, persistence, Unicode input, distribution editing, camera
presentation, and cleanup behavior. Version 1.0.0 is the first supported public
baseline; no pre-release archive is required when installing it.
