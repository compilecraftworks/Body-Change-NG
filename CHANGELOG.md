# Changelog

All notable public changes to Body Change NG are documented here.

## 1.2.0 — final comparison with v1.1.4

This list describes the final v1.2.0 behavior, not the sequence of development experiments. Body presets, skins, tint editing, futanari skins, and NPC body/skin assignment already existed in v1.1.4; the changes below revise or extend those systems.

### Selection and UI
- Standardized single-click/list-navigation preview and explicit double-click/Enter/configured Activate confirmation. Closing without confirmation restores the previous committed selection instead of confirming the last preview. Keyboard/gamepad confirmation and closing now follow the game's Activate and menu Cancel bindings.
- Final tab order: Body Presets, Body Skins, Tint Masks, conditional Futanari Skin, Overlays.
- Added LT + RS horizontal character rotation alongside right-mouse dragging; improved face framing, input consumption, popup alignment, long-row clipping, and stable catalog layout.
- Fixed the outfit/randomization popup jumping to the top of the screen on first open; supported popups remember their individual positions between openings.
- Restored the character-rotation hint on the main title bar immediately left of X. Corrected its clip region and right alignment; long text scales to fit instead of disappearing or truncating.
- Shortened catalog help and fitted it to the available width on one line, including the English overlay hint.
- Moved NPC distribution entry into each supported catalog. Added checkbox selection and category-specific condition popups; selected candidates remain available when adding more rules.
- Body-preset, body-skin, and futanari-skin NPC-distribution lists now preview the last clicked or navigated compatible row, including checkbox clicks and keyboard/gamepad activation. Checkboxes remain distribution candidates, not manual actor assignments. Cancel, Clear selection, popup entry, actor/tab changes, and closing restore the corresponding committed appearance.
- Catalog scans/refreshes use queued work and reusable snapshots instead of rebuilding expensive lists each time a window opens. Performance-mode scheduling remains separate from manual selection.
- Updated the in-game help and complete installation guides. Skin and futanari packs can be installed by copying the entire installed texture-mod folder into BodySkin or Futanari.
- Documented adding or replacing body-preset XML and BodySkin/Futanari packs while the game is running: finish copying, Refresh, then reselect and confirm. New plugin/addon installation and newly enabled MO2 mods still require restarting; this is not an overlay-mod installation workflow.
- Added BodySlide preparation: build the character body and outfits with Zeroed Sliders and Build Morphs enabled so the preset's XML morphs are expressed on the intended baseline.

### Body presets and morph preservation

- Added Preserve other mods' morphs in Mod Settings, enabled by default. Default character placement is Left; Pause game and Performance mode are off. Existing saved preferences are retained.
- Body presets use the XML slider values interpolated by actor weight, including negative values and values above 100%. Removed aggregate-morph compensation from the committed preset path.
- With preservation enabled, preset application replaces only BCNG's own morph keys. With it disabled, confirming a preset clears other RaceMenu body morphs before applying the preset. Unconfirmed previews remain reversible in either mode.
- Default Body and actor reset clear current and legacy BCNG-owned keys without deleting other mods' morphs, even when preservation is disabled.
- Aligned procedural breast/nipple correction with OBody NG's preset-key calculation instead of subtracting the sum of every mod's morphs. Preview and confirmed application now share the same correction policy.
- Removed the obsolete catalog-wide slider-union cache and unused compatibility helpers; switching presets no longer depends on that extra catalog-wide work.

### Skin application and asset routing
- Replaced the general live-material BodySkin pipeline with private native Skin Armor/ARMA/TXST routing for body, hands, and feet. Source provider forms remain separate from BCNG's clones.
- Faces now use a dedicated NiOverride skin-channel path, with adapters for known old/new RaceMenu interfaces and a fallback where direct access is unavailable. The final implementation does not replace the face HeadPart or NIF.
- Reworked face update ordering, stale-request cancellation, baseline restoration, and texture-path/cache handling to address delayed, wrong-pack, and purple-face failures during repeated selection.
- Removed the superseded general skin/live-material code and obsolete face experiments from the runtime path.
- Applies compatible available DDS channels from partial packs. Missing optional genital/anal targets no longer reject an otherwise usable skin pack, including BnP CBBE layouts.
- Uses a matching recognized race/elder/vampire DDS first, then the selected pack's ordinary channel, then the underlying provider where the pack has neither. The same policy applies to male and female packs.
- Corrected UBE skin/subsurface channel routing and kept conventional atlas routing separate. Body/hand/foot/face/addon roles are not treated as interchangeable.
- Improved Unicode pack paths, mod-manager provider discovery, cached texture identity, and provider restoration including RSV-related baselines.
- Removed skin profile.json parsing. Pack identity and supported texture paths drive discovery.

### Futanari and male addon skins
- Moved supported SOS/TNG male and female-addon textures to a native TXST construction path, including refresh of already attached supported addon geometry.
- Fixed a false validation failure that could block supported SOS/TNG male-addon and futanari skin changes when engine imports span adjacent readable memory regions. Code-pattern, ownership, and memory-access checks remain in place.
- Female-addon installation/loading enables the futanari feature globally; merely having an ordinary male addon does not.
- Manual eligibility is based on SOS/TNG registration of a supported female addon, not only currently visible geometry. Unregistered females and males are ineligible.
- Added category-specific futanari distribution restricted to registered female futanari NPCs. Texture selection does not equip/register an addon.
- Preserves an eligible actor's independent skin choice through temporary addon-geometry absence.

### Tint masks and overlays
- Moved tint discovery from the old standalone TintMask root into each BodySkin pack's Textures tree. Skin scanning and tint scanning are independent; tint-only and combined skin/tint packs are supported.
- Added sex and UBE/conventional tint filtering. Tint editing remains player-only, with no NPC distribution.
- Added RaceMenu paint and SlaveTats collection discovery in one expandable Face/Body/Hands/Feet list.
- Added multiple manual overlays per area; double-click toggles a committed item on/off. Per-area Default removes only BCNG-owned overlays.
- Added overlay favorites and per-item color/opacity controls outside the scrolling list.
- Remember preview RGBA per item while the main UI is open, and copy each selected overlay candidate's RGBA into NPC rules.
- NPC-distribution checkboxes now drive a cumulative per-area preview stack: checking adds an entry, unchecking removes its preview. The focused row controls color/opacity editing; checked candidates display those edits. Select all, Clear selection, and keyboard/gamepad confirmation follow the same checkbox state. Excess candidates remain in the distribution pool without overwriting foreign slots. Cancelling distribution or closing restores temporary changes without altering committed selections, colors, or counts.
- Previewing an already applied BCNG overlay reuses its owned slot and restores its committed color on cancel, instead of stacking a duplicate. Selecting an area's Default row restores normal body-skin camera framing.
- Store original tint DDS/RGBA in the save-specific TINT v2 record. Keep committed tint intent separate from previews and batch pack/layer restoration into one composite refresh.
- Reworked repeated selection, ownership, restore, and provider-key lifetime handling. Preview slots no longer count as committed selections.
- The header now shows Applied X/Y: X is BCNG's confirmed count; Y is RaceMenu capacity minus foreign occupied/reserved slots. Removed the separate BCNG counter.

### Outfit correction and SFS

- Added optional SFS Rendered Outfit API v1 integration for breast/nipple correction and ORefit exclusions, force-refit entries, and outfit-specific presets.
- Visible actual equipment is evaluated by its own name/FormID/plugin; visible registered appearances use their own identity. Hidden armor is ignored, and hiding all correction-relevant clothing clears breast/nipple correction.
- Re-evaluates changed actors from SFS notifications and discards stale queued corrections. Without SFS or its supported API, or for an actor SFS does not manage, the original actual-equipment path remains unchanged.
- Added support for SFS's game-task query entry point to prevent live breast/nipple updates from stalling after task-worker thread changes. Retained the older API fallback and stale-result checks.

### NPC rules and persistence
- New installations ship an empty opt-in rule set. Removed the distribution/exclusion mode and legacy exclusion-only rules during schema-7 migration; retain eligible positive assignments.
- All NPCs rules default to excluding custom followers and elder NPCs through checked target filters.
- Restrict custom-follower and elder exclusion checkboxes to All NPCs rules; they no longer silently block explicit name, individual NPC, or other target scopes.
- Reevaluate automatic futanari skin candidates after late registration or addon-family changes, without rerolling body/skin/overlay assignments or overriding manual choices. Track the selected pack and texture content as well as addon geometry so a changed candidate is actually reapplied.
- Removed OBody NPC-rule import. The optional OBody ORefit JSON reader remains only for explicit outfit-correction registration.
- Changed v1.1.4's auto-save-on-close behavior: Close/X/Escape/main-window close now discards unsaved rule edits in every distribution-enabled tab.
- Only Distribute to loaded NPCs now or Distribute next game launch saves conditions. Immediate activation occurs only after a successful save; next-launch saves leave active session rules unchanged.
- Rule priority is evaluated independently per feature and per overlay area. Automatic overlays choose one candidate per configured area; manual stacks can contain multiple overlays.
- Expanded save-state handling for confirmed manual selections, multi-overlay stacks/colors, defaults, and addon choices. Manual choices retain priority over automatic assignments.
- Expanded Reset selected actor settings / Reset all actor settings to body, body skin plus supported male-addon skin, futanari skin, overlays, and player tint. Reset persists explicit Default intent and does not delete assets or the rule file.

### Compatibility and verification
- Explicit runtime targets: SE 1.5.97; AE 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353, 1.6.629, 1.6.640, 1.6.659, 1.6.1130, 1.6.1170, 1.6.1179. No VR, Epic 1.6.678, or unlisted runtime support.
- Audited known RaceMenu BodyMorph and Override/Overlay interface generations, including legacy ABI and provider slot counts. A matching RaceMenu/SKSE build is still required.
- Replaced a two-runtime genital-backend address gate with version-aware Address Library resolution and runtime code/ownership verification; corrected the GOG import-resolution assumption.
- Reduced redundant scans, temporary diagnostics, and obsolete code while retaining actionable failure reporting and ownership/lifetime checks.
- The SE/AE-only Release build and automated regression tests passed, including morph-key preservation, reversible previews, overlay selection, SFS-consumer handling, and native-addon validation.

### Upgrade notes
- Back up .ess saves with their matching .skse co-saves, settings, custom distribution JSON, and asset packs before updating.
- Keep your custom BodyChangeNGdistribution.json; do not replace it with the empty installer starter.
- Keep installed skin-mod folders under Data\BodySkin\ and futanari-skin mod folders under Data\Futanari\. Preserve their full Textures trees.
- Move old tint files to Data\BodySkin\Your Pack\Textures\actors\character\character assets\tintmasks\ and review migrated rule conditions before saving.

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
