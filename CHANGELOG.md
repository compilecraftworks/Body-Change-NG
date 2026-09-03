# Changelog

All notable public changes to Body Change NG are documented here.

## 1.0.0 — 2026-09-02

First stable public release.

### Body

- Applies BodySlide presets through RaceMenu BodyMorph without replacing body
  meshes at runtime.
- Interpolates low/high-weight preset values using the actor's current weight.
- Detects the selected actor's body family conservatively and filters only the
  main Body tab. Uncertain or multi-family presets remain visible through safe
  fallbacks; NPC distribution pools are never auto-filtered.
- Supports mixed CBBE 3BA/UBE installations by resolving each actor from its
  live winning skin/head texture path and classifying UBE presets from XML
  `Preset/@set` and `Group` metadata in the shared SliderPresets folder.
- Spreads automatic NPC morph work through the performance queue, uses
  RaceMenu's deferred partition update for distribution and outfit correction,
  and skips mesh rebuilds when there is no outfit morph to clear.
- Keeps outfit-named presets such as Clothes, Outfit, Bikini, Armor, Cuirass,
  Dress, Panty, and Overalls visible. Only exact `-Refit` presets are reserved
  for outfit correction.

### Skin

- Applies persistent RaceMenu/NiOverride texture overrides independently per
  actor without editing NIF, Skin Armor, or equipment records.
- Supports body, hands, feet, face, vampire face, diffuse, normal, subsurface,
  specular, and compatible FaceGen detail textures.
- Detects UBE 2.0 `Textures\!UBE\Body` and `Head` d/n/sk atlases, applies them
  to the live slot-53 body and face, and separates conventional and UBE skin
  catalogs and runtime distribution candidates per detected actor family.
- Tracks ownership precisely so cleanup removes only Body Change NG's keys.

### Player tint

- Adds player-only tint packs with automatic race-appropriate DDS selection.
- Separates known UBE-only, conventional, and COtR-compatible tint packs while
  preserving the show-all fallback when actor-family evidence is uncertain.
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
- Coalesces equipment and actor work in both normal and performance modes,
  with performance mode adding an extra scheduling interval; stores ActorHandles instead of raw actor
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
