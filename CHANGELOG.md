# Changelog

All notable public changes to Body Change NG are documented here.

## 1.2.0 — 2026-09-08

### Skin architecture

- Separates catalog `SkinLayout` from runtime `BodyFamily`. Only an explicit `!UBE` atlas tree is UBE; every other conventional humanoid pack is Legacy regardless of folder labels or optional genital/anal DDS files.
- Does not classify packs as CBBE/3BA versus UNP/BHUNP or vanilla/HIMBO/SAM. Runtime BodyFamily still selects the exact material route, UBE accepts only UBE, and optional genital/anal atlases route only through matching TXST/geometry roles.
- Removes the regression that excluded metadata-free conventional packs as `unknown` and preserves every relative-path automatic ID.
- Replaces general BodySkin NiOverride painting with a private deep clone of the current native TXST -> ARMA -> Skin Armor graph and an optional Face TXST. The clone preserves all source channels and overlays only the part/channel pairs declared by the selected profile.
- Requires an exact TXST role only for parts the partial pack actually supplies. Missing body parts or DDS channels retain the current provider value; a declared part with no exact role aborts before attachment instead of being redirected to another part.
- Models the shipped UBE 2.0 graph explicitly: its slot-53 torso and separate hand/foot ARMAs share `!UBE\Body\femalebody_1_{d,n,sk}` while their NAM1 fields are empty. BCNG synthesizes a private TXST only on clones of verified canonical UBE naked models when body-atlas layers are actually declared; face-only profiles, custom paths, and absent channels are never guessed.
- Attaches the complete graph at the NPC ActorBase, so naked body, hand, and foot rebuilds use Skyrim's native skin source without tracking equipment. Outfit-owned hard-coded materials are not guessed or repainted.
- Prevents Argonian and Khajiit feet from borrowing a body DDS when the pack does not supply a dedicated feet atlas.
- Removes skin-pack `profile.json` parsing and the bundled manifest example. A stale manifest is ignored; folder-relative automatic identity and the texture namespace are the only catalog inputs. Settings and distribution-rule JSON are unchanged.

### Ownership, distribution, and compatibility

- Keeps BodyMorph reference-scoped while making native skin distribution ActorBase-scoped. The existing 1.1.4 rule conditions, priorities, pools, JSON schema, and UI remain unchanged; every reference sharing one NPC base now converges on one stable skin-pool choice, including legacy reference-scoped results, and contradictory manual requests fail closed.
- Allows ActorBase ownership to transfer to another reference only while no skin is active. The same active profile may be shared; a different active profile or a forced Default from a non-owner remains a conflict.
- Treats the current RSV Skin Armor as a source provider, rebuilds BCNG's clone when RSV replaces body/far-skin/face pointers, and rebases an unattached face or far-skin component before a later profile starts using it. Only pointers BCNG still owns are restored. The bounded face bridge paints only channels currently owned by RSV as live, non-persistent values and never creates or removes a serialized key.
- Preserves UBE 2.0's upstream requirement to exclude its player race from RSV (`PLAYER VANILLA`); only an already valid UBE/RSV provider graph is cloned.
- Keeps male SOS/TNG slot-52 textures and optional female SOS/ERF/TRX futanari textures in independent reference-scoped adapters. Only those adapters and outfit morph correction observe equipment changes; general native skin does not.
- Reapplies male SOS/TNG, female futanari, and the verified RSV face bridge independently when cell attachment recreates their external geometry; native base skin is not repainted on that path. Genital adapters compare the loaded Armor/ArmorAddon identity before equipment/cell reconciliation, so unrelated OStim/ODF/outfit events cannot repeatedly overwrite live third-party effects; removal clears the identity so the same Form is restored after re-equip.
- Removes only BCNG-owned 1.1.x body/hand/foot/tail/face NiOverride keys once during migration. It excludes RSV, foreign providers, male genital, and futanari ownership.
- Stores an optional language-neutral `nameKey` for built-in samples and untouched generated rule names, so they follow the Korean, English, or Simplified Chinese UI without translating or overwriting a user-edited name.
- Recovers an exact legacy generated label whose saved rule sex was changed without its name, retargeting only that built-in Korean/English/Chinese label to the current sex while leaving arbitrary custom names untouched.
- Makes starter samples editable, movable, and deletable like every other rule, allocates collision-free persistent IDs for new rows, and shows a localized warning when an earlier same-sex all-NPC rule makes the selected row unreachable.
- Saves distribution rules and settings through a flushed, reparsed temporary file followed by an atomic Windows replace, so a failed write cannot delete the last valid file.
- Logs every rejected automatic NPC body/skin submission with the actor, ActorBase, rule/manual source, selected ID, and exact rejection reason. Co-save completion remains recorded only after the BodyMorph or native TXST operation finishes successfully.
- Preserves an automatic actor's serialized body/skin choice when a matched rule leaves that category unchanged; a selected asset and an explicit Default Body remain distinct outcomes. Private native form graphs are reused across save loads after restoring provider pointers and clearing ownership, avoiding per-load duplicate-form growth, and every TXST clone is reset to provider paths before applying the next profile.
- Preserves every installed same-name BodySlide preset ID when importing OBody NG distribution rules, so CBBE/UBE or multi-source catalog order cannot pin an NPC rule to the wrong BodyFamily and leave the actor undistributed.
- Routes OBody/ORefit outfit name, plugin, and resolved FormID exclusions through one tested runtime policy. Registering the list now re-evaluates every loaded actor immediately, so a newly excluded outfit clears an already-applied correction without waiting for another equip or cell event.

### Appearance work coordination

- Splits actor state, event ownership, and latest-wins work channels across body, native skin, tint, male genital, futanari, RSV face, and outfit features. Resetting or superseding one feature cannot clear or cancel another.
- Removes the 1.1.x distribution delay that coupled native skin to completion of a BodyMorph rebuild. A face-only profile owns only its Face TXST and never attaches the cloned body/far-skin graph.
- Admits only the exact SE/AE runtime table and RaceMenu BodyMorph v4/v5 plus Override v0/v1/v2 routes. Unknown game patches and future ABIs fail closed; the build remains `EXCLUSIVE_SKYRIM_FLAT` with no VR target.
- Adds dedicated skin architecture and OBody/ORefit import regression suites plus feedback-audit coverage for distribution tri-state persistence and independent external-addon identities. The Release DLL and all 15 test executables pass.

## 1.1.4 — 2026-09-06

### NPC skin sequencing and persistence

- Rapid Body catalog input now supersedes obsolete interactive body/skin work while a legacy v0/v1 Papyrus skin change is pending. The latest body is applied once, then the selected BodySkin and futanari skin are repainted once; automatic distribution and equipment work are preserved.
- Applies an automatically distributed NPC skin only after that actor's deferred body and outfit rebuild work has settled. This removes the NPC-only flash followed by an immediate return to the original skin.
- Records a skin as complete only after `QueueNiNodeUpdate` finishes and the final Biped clone has been repainted. The final pass uses the same exact body, hand, foot, face, genital, and anal routes on RaceMenu Override v0/v1 and v2.
- Locks an NPC's manual Skin or Default Skin choice as soon as its live preview is accepted, so an attach or initialization event cannot replace the visible selection with an automatic rule result.

### Dead NPC distribution and rule editing

- Includes already-dead, loaded NPCs in automatic distribution while preserving the player, disabled-actor, unloaded-3D, and non-NPC exclusions. Applied signatures and actor-work coalescing still suppress duplicate work.
- Saves the current NPC Distribution editor draft when the popup closes by X or Escape and when the main menu closes. This updates the next-launch JSON without silently replacing the rules active in the current session.
- Keeps the same-session draft in memory when the popup is reopened, but clears it at a save-load boundary so an old save's unfinished editor state cannot leak into a newly loaded save.
- Adds localized `Select all` and `Clear all` controls to Body and Skin pools. Selection follows the rule sex and the NPC body family configured in Mod Settings; Zeroed Sliders remains selectable and can be unchecked afterward.

### Performance and validation

- Adds no polling, catalog rescan, or per-frame JSON write. Only actors that need both a body change and a skin change receive one bounded delayed skin evaluation, and only an actual Biped rebuild receives one final repaint.
- The Release build and all 12 regression test executables passed. Existing settings, distribution schema, user rules, co-save identities, body morph ownership, partial skin packs, UBE routing, and futanari isolation remain compatible.

## 1.1.3 — 2026-09-06

### CBBE 3BA hand and foot BodySkin routing

- Fixes a RaceMenu broad skin-slot call that traversed every ArmorAddon in the same Skin Armor, allowing the final foot assignment to repaint the hands.
- Applies CBBE 3BA, BHUNP, and vanilla body, hand, and foot textures only to each part's exact ArmorAddon and loaded geometry. CBBE-family feet continue to use the intended body atlas while hands retain their dedicated hand textures.
- When visually hidden equipment still occupies a biped slot, stores one durable one-bit fallback for only the missing conventional part. Override v0/v1 then repairs every currently visible exact part after the legacy broad callback; v2 stores the missing key without a live broad repaint.
- Uses the same corrected path for player/NPC manual selection, direct NPC assignment, rule-based distribution, and recovery after equipment changes or looting a dead NPC.
- Rechecks restored NPC body, hand, and foot state once in a new session and reapplies an existing selection when the old live result is incomplete. No recurring filesystem scan or all-NPC polling was added.
- Keeps UBE's intentional shared body atlas route and removes only the unnecessary broad calls from conventional layouts.
- Distinguishes legacy SE Override v0, UBE's AE-backported Override v0, official Override v1, and official Override v2. Both v0 variants and v1 use the serialization-safe Papyrus string route; only audited v2 uses the native interface, and unknown future versions fail closed.

### Futanari and BodySkin ownership isolation

- Keeps the Futanari tab available through BCNG's own NiNode rebuild by detecting the equipped ArmorAddon from the actor's biped slots instead of treating a temporarily detached geometry clone as an unequip.
- Recognizes the live TRX `CBBE_Schlong` node spelling in addition to earlier variants, so one click reaches the correct UBE/CBBE TRX target.
- Excludes only verified external futanari genital nodes from ordinary BodySkin body apply and cleanup. Normal CBBE 3BA/BHUNP/UBE body, hands, feet, internal genital/anal atlases, and male HIMBO/SAM plus SOS routes remain independently owned.
- Leaves the BodyPreset/BodyMorph pipeline unchanged; preview, confirmation, NPC distribution, Default Body, and outfit correction retain their existing absolute-preset behavior.

### Validation

The Release build and all 12 regression test executables passed. Existing distribution rules, settings, JSON, co-save data, partial skin packs, Default Skin restoration, UBE routing, and normal equipment refresh remain compatible.

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
