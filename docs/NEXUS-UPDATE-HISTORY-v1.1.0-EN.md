# Body Change NG — Update History

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

Validation: Release build and ten regression test executables. File/NIF evidence and automated tests cover routing and state logic; in-game atlas appearance, collision behavior, OverlayFix crash compatibility, and stutter-free gameplay are not certified by these tests.

---

## Previous releases

### v0.1.x — Core Application Foundation

- Established BodySlide preset application through RaceMenu BodyMorph.
- Built the live-actor skin-texture application path and the first native ImGui interface.
- Introduced the F7 menu hotkey, persistent settings, and the initial resolution-aware UI scaling structure.

### v0.2.0–v0.2.3 — Player Tint and NPC Distribution

- Added player RaceMenu tint packs and restoration of captured original layer values.
- Added nearby-actor selection and direct per-NPC Body and Skin application.
- Introduced top-down, first-match NPC distribution rules with independent Body and Skin pools.
- Reworked the catalogs so BodySlide XML files and Skin/Tint folders added while Skyrim is running can be discovered with Refresh.

### v0.2.4–v0.2.10 — UI and Text-Input Stability

- Stabilized Korean, English, and Simplified Chinese UTF-8 UI rendering and native Windows IME input.
- Corrected Backspace, Korean composition, and suppression of game hotkeys while a text field owns focus.
- Added single-click live selection, double-click confirmation, and keyboard/gamepad navigation.
- Added per-tab favorites, search fields, selected-row highlighting, adjustable dropdown height, and resizable popups.

### v0.2.11–v0.2.17 — Skin/Tint Ownership and RaceMenu Recovery

- Mapped body, hands, feet, face, vampire face, and supported texture channels to their correct NiOverride keys.
- Restricted cleanup to Body Change NG-owned keys so another mod's overrides are not deleted.
- After RaceMenu rebuilds meshes, NiOverride data, and tint arrays, restored confirmed state in Body → outfit correction → Skin → Tint order.
- Added generation checks so an older asynchronous task from rapid selection cannot overwrite the newest choice.

### v0.2.18–v0.2.21 — Save State, Distribution, and Outfit Correction

- Introduced the Actor Registry, storing evaluated actor results in the SKSE co-save and resolving actor references after load-order changes.
- Separated global distribution rules from save-specific evaluated results to reduce unnecessary redistribution.
- Completed independent Body/Skin distribute-or-exclude controls and eight starter exclusions: Body exclusion for female/male custom followers and elders, plus Skin exclusion for female/male Argonians and Khajiit.
- Added explicit-button import of supported OBody NG distribution and outfit-correction JSON rules, including exclusions, forced correction, FormIDs, plugins, and outfit-specific presets.
- Added breast and nipple correction while clothed, plus stable nipple and genital shape randomization.
- Added BodyFamily classification that prioritizes actual preset-set information from XML and safely exposes presets that belong to multiple families.

### v1.0.0 — First Stable Public Release

- Added per-actor CBBE 3BA/UBE detection for mixed installations, using the
  live MO2-winning skin/head texture path and XML set/group metadata.
- Added UBE 2.0 skin-pack scanning and slot-53 Body plus live-Head d/n/sk
  application, with family-safe main lists and NPC skin distribution.
- Added Argonian and Khajiit female/male skin scanning under the standard
  BodySkin root, with race/sex-safe preview, reapply, and NPC distribution.
- Added CBBE 3BA `femalebody_etc_v2_1` diffuse, normal, subsurface, and
  specular channels, routed to the matching `3BA`/`3BBB` vagina and anus
  geometries that share the atlas.
- Added BHUNP/UNP `BakaUNP\VaginalAnalCanal2` diffuse, normal, subsurface, and
  specular channels for the matching vagina, anus, and canal geometries.
- Made BodySlide application absolute across the detected compatible family:
  omitted XML sliders resolve to zero and repeated preview, confirmation, or
  NPC distribution converges to the same shape without deleting unrelated
  mod-owned keys.
- Added keyword, class, and combat-style distribution conditions and stored
  faction/race/form targets as plugin plus local FormID. Unnamed forms remain
  selectable in the in-game dropdowns.
- Fixed partial male skin routing and added live SOS slot-52 genital atlas
  selection for Smurf Average, VectorPlexus Regular/Muscular, race variants,
  elders, and independently missing DDS channels.
- Finalized symmetric main-camera framing at FOV 70, distance 200, horizontal ±70, vertical -45, and player pitch 0.1.
- Added a separate player-tint close-up that keeps the same FOV while scaling distance and horizontal offset together and positioning the face inside the frame.
- When the game is paused, right-drag now orbits the camera instead of rotating the actor root, avoiding FSMP stretching without manipulating Skyrim's pause counter.
- Menu close now restores the original camera target, facing, FOV, actual projection frustum, and both current and target zoom.
- Moved camera presentation updates outside the ImGui render pass, preventing whole-UI color shifts in DLSS and post-processing environments.
- Cleaned queued work and temporary Body/Skin state when actors unload, and blocked stale tasks after save changes.
- Fixed missing footer buttons in the NPC Distribution window and refreshed the tint color chip after restoring a tint value.
- Reduced automatic-distribution stutter in both normal and performance modes by coalescing actor events, prioritizing newly visible NPCs, deferring RaceMenu partition rebuilds, and skipping empty outfit-morph clears. Performance mode adds a further scheduling interval between actors.
- Deferred the first loaded-NPC pass beyond the save-load callback burst while
  keeping all catalog/form discovery out of per-frame and per-actor hot paths.
- Completed the v1.0.0 DLL build with all ten regression test suites passing.

v1.0.0 is the first stable public baseline for Body Change NG.
