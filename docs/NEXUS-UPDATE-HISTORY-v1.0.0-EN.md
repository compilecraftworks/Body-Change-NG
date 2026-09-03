# Body Change NG

## Development & Update History

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
- Finalized symmetric main-camera framing at FOV 70, distance 200, horizontal ±70, vertical -45, and player pitch 0.1.
- Added a separate player-tint close-up that keeps the same FOV while scaling distance and horizontal offset together and positioning the face inside the frame.
- When the game is paused, right-drag now orbits the camera instead of rotating the actor root, avoiding FSMP stretching without manipulating Skyrim's pause counter.
- Menu close now restores the original camera target, facing, FOV, actual projection frustum, and both current and target zoom.
- Moved camera presentation updates outside the ImGui render pass, preventing whole-UI color shifts in DLSS and post-processing environments.
- Cleaned queued work and temporary Body/Skin state when actors unload, and blocked stale tasks after save changes.
- Fixed missing footer buttons in the NPC Distribution window and refreshed the tint color chip after restoring a tint value.
- Reduced automatic-distribution stutter in both normal and performance modes by coalescing actor events, prioritizing newly visible NPCs, deferring RaceMenu partition rebuilds, and skipping empty outfit-morph clears. Performance mode adds a further scheduling interval between actors.
- Completed the v1.0.0 DLL build with all ten regression test suites passing.

v1.0.0 is the first stable public baseline for Body Change NG.
