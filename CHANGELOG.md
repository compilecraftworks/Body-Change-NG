# Changelog

All notable public changes to Body Change NG are documented here.

## 1.3.4

### Body presets and outfit correction

- Add separate UBE/Necoco female genital randomization: correlated Innie/Average/Outie recipes (20%/60%/20%). Leave AnusSpread and existing 3BA/BHUNP calculations unchanged. Necoco genital extras require nonzero position morphs in loaded BODYTRI files; the bounded session-only metadata cache clears on game-session reset. Save formats and RaceMenu interfaces are unchanged.
- Add UBE/Necoco nipple randomization using the original OBody NG 4.4.3 independent probability/branch program, including the nested inversion branch and preservation of unselected optional XML values. Translate results to UBE-specific sliders/ranges; do not introduce exclusive shape classes or extra random draws. Existing 3BA/BHUNP numerical recipes are unchanged.
- Include female players as well as NPCs in both anatomy options when a body preset is committed. Rename the controls to Female nipple randomization / Female genital randomization in Korean, English and Chinese. Distribution rules are not required; previews do not reroll anatomy, and disabling an option then reapplying restores the preset values.
- Outfit/breast correction, nipple correction, female genital randomization and female nipple randomization all default to OFF. Enable only the features you want; explicitly saved settings are preserved.
- Align base preset values with OBody NG 4.4.3: interpolate small/big XML endpoints by actor weight, divide by 100, use zero for omitted endpoints, preserve negative/over-100 values, and retain the original UNP reverse-slider and first-nonzero duplicate rules. Remove the extra UBE build-default subtraction and weight clamp.
- Align CBBE 3BA/BHUNP correction and supported anatomy-randomization calculations with the original recipes. Preserve the other-morph option, actor-family separation, SFS integration and cancellable previews.
- Add a separate additive UBE/Necoco outfit recipe derived from Fit to Thicc v1 Nude → Pushup: eight breast offsets, plus six nipple/UV offsets when nipple correction is enabled. Interpolate the offsets by actor weight without subtracting BCNG or RaceMenu morphs. The reference NipplesShowUp default affects this fixed recipe only, not normal preset XML parsing.
- Keep global outfit/nipple switches editable with a UBE player. Turning nipple correction off removes only its offsets; turning outfit correction off or undressing removes the outfit layer. Each actor uses its own body-family recipe.
- Apply ORefit name/plugin/FormID exclusions to both breast and nipple correction for all supported families. Keep explicit force-refit precedence and compatible named-refit priority. With SFS, evaluate the final visible outfit; otherwise use actual equipment. Existing stored body selections and correction layers reconcile once through the existing work queue.

### Skins, overlays and tint masks

- Match skin/futanari/overlay/tint asset IDs, DDS paths and extensions without ASCII case sensitivity; accept either path separator. Keep existing stored spelling and save formats. Favorites, preview colors and restoration now use the same identity rules.
- Fix valid skin texture writes being rejected when Skyrim reuses an interned filename with different capitalization. Avoid repeated skin rebuilding caused only by ID casing. Keep texture ownership, slot reservations and path-safety checks.
- Recognize the exact blank DDS left by SlaveTats / SlaveTats NG after an overlay slot is released, restoring usable Face/Body/Hands/Feet capacity instead of incorrectly showing 0/0. Protect all registered texture/tint/alpha overrides, including invisible tattoos, in both camera views. Unrelated blank textures and other mods' tattoo records remain untouched.

### NPC distribution

- In every supported tab, replace the actor dropdown and Refresh actors controls with "Distribute selected items to world NPCs within a chosen scope" during distribution selection and condition editing. Korean, English and Chinese titles fit on one line; ordinary actor controls return after leaving distribution mode.
- Restore UBE presets, including UBE Zeroed, in the configured distribution list and actual rule-selection pool. Ordinary lists follow the selected actor; distribution candidates follow settings, while previews remain actor-compatible.
- Before recording an automatic preset choice, check the configured family and the receiving NPC's known family without copying all candidate sliders. Preserve sex checks, unknown-family handling, cumulative overlay previews and colors. This does not convert NPC bodies or expand existing skin/futanari distribution restrictions.

### Updating and verification

- Preserve customized BodyChangeNGdistribution.json, settings, packs and texture cache. The rule schema remains 8, with no active rules in the starter. No new runtime hook, polling loop, persistent cache, dependency, RaceMenu ABI or co-save-format change is introduced.
- Release build and all 40 offline regression executables passed, including 51,712 UBE nipple mappings, comparison against the unmodified upstream probability/branch code, 9,696 UBE genital recipes and packed/legacy TRI boundary tests. Coverage includes reference morph calculations, distribution candidates, 1,000 list-mode cycles, 36 header layouts and mixed-case texture/identity paths. The earlier 1,200,000-call overlay predicate allocation check retained zero blocks/bytes.
- The mouse/input implementation was left unchanged.

## 1.3.3

- Added human-readable schema 8 for NPC rules: use BodySlide/BCNG preset names, skin and futa pack names, and registered overlay names instead of internal IDs.
- All eleven target scopes are supported using readable scope names. Races, factions, keywords, classes and combat styles accept EditorIDs or unique game-provided names, with plugin qualifiers and a local hexadecimal-ID fallback for records that expose no unique name.
- Overlay candidates support #RRGGBB / #RRGGBBAA colors. Multiple candidates remain independent per feature and overlay area. Existing actor-compatibility and automatic-distribution restrictions are retained.
- Added a bilingual annotated template with 19 inactive examples, 39 copyable vanilla target names, a complete configuration, duplicate-name guidance and a separate authoring guide. A numeric-ID helper is no longer required for ordinary name-based editing.
- Existing schema 3–7 files migrate with an exact, non-overwriting .schemaN.bak backup. Known entries become named references; unresolved old IDs and unresolved new names remain stored. Rule order, candidate colors and existing legacy migrations are preserved.
- Schema-8 enabled:false disables an individual rule. The old ignored enabled flag retains its previous behavior during migration. Ambiguous names are not resolved arbitrarily, and invalid fields/scopes are rejected rather than becoming broad conditions.
- The starter has zero active rules. In-game explicit saves write active rules at the top and retain comments/examples below without accumulating duplicates. Preserve your custom file when updating; schema 8 requires the updated DLL, not just a replacement JSON.
- Asset name indexes are rebuilt when catalogs change, and actor compatibility is checked once per candidate pool. No new per-frame polling, engine hook, supported-runtime change or co-save format change.
- Korean description/changelog HTML retains browser-adaptive light/dark colors.

- Verification: Release build and 36 offline regression executables passed, including production name-conversion functions, schema 3–7 migration, 88 persistence cases, 65,536 color round-trips and comment-preserving repeated saves. Vanilla targets were checked against two Skyrim.esm files. No in-game test was performed.

## 1.3.2

- Restore direct skin selection for standalone actors whose BodySlide family cannot be identified. Show sex/race-compatible packs and an unknown-family hint; clicking previews and double-clicking applies without distribution rules. Keep known incompatible layouts blocked.
- Keep automatic NPC distribution's existing compatibility checks. Preserve direct-choice intent for saved manual assignments, preview rollback, and player RaceMenu restoration. Ordinary body/hand/foot/face routing is retained without guessing CBBE/UNP auxiliary genital/anal atlases.
- Preserve an unknown source classification while BCNG's private skin graph is active, so cached DDS names cannot reclassify the actor after the first choice. No new polling, cache format, co-save schema, or dependency changes.
- Resolve NPC face targets inside the live FaceGen subtree instead of requiring the geometry name to match the HeadPart editor ID. Prefer the exact name, accept only an unambiguous skin-material fallback, and reject collisions with objects outside the face. Body-only packs no longer require a face target when there is no previous face state to restore.
- Verification: Release build, 35 offline regression executables and 19 audit-tool tests passed. No new in-game testing was performed; resolution in the reporters' specific setups is not yet confirmed. See [verification](docs/RELEASE-VERIFICATION-v1.3.2-KO.md).

## 1.3.1

- Add an optional offline cache-maintenance utility. Scan MO2 Overwrite without changing files, then explicitly compact byte-identical DDS copies with matching timestamps into hard links. Keep every DDS resource path, including paths used by older saves and v1.2.7 caches.
- Exclude files linked outside the selected cache, read-only/locked files, reparse points and unknown layouts. Do not delete unique DDS files based on age or the current actor's selection. Savings depend on actual eligible duplicates; folder size can still look unchanged.
- Remove only recognized, unpublished preparation/link staging files after the game exits. The utility refuses a running Skyrim process and does not modify source packs, saves or distribution rules.
- Publish new runtime DDS files through a private staging path and atomic replacement. Preparation failure preserves an existing good alias and cleans up its own temporary file; read-only source attributes are preserved. Body, face, genital and companion-map paths continue to use the existing cache identity.
- Preserve the v1.3.0 restoration fixes, existing appearance features, supported runtimes, RaceMenu contracts and dependency pins. The maintenance utility does not run during gameplay or add frame-time scanning.
- Verification: Release build, 35 regression executables, 19 audit-tool tests and the cache AddressSanitizer run passed. Tests cover byte/path preservation, external-link isolation, locked-file failure/retry and temporary-file cleanup. No new in-game testing or installed-cache compaction was performed. See [verification](docs/RELEASE-VERIFICATION-v1.3.1-KO.md).


## 1.3.0

- Reserve separate slots when restoring multiple saved overlays before their deferred node writes finish. Restore unique existing slots first, allocate unassigned/out-of-range or duplicate saved slots afterward, preserve colors, and leave other mods' occupied slots untouched.
- Treat the Default body and its independent outfit correction as separate layers during automatic validation. Keep valid breast/nipple corrections; if the base really needs clearing, invalidate the old correction signature and reevaluate the outfit through the normal policy. Explicit reset and mod-removal cleanup still clear their owned layers.
- Defer player skin-preview undo while RaceMenu is open instead of running it during editing or discarding it. Keep undo across actor unload, carry the same gate into texture-preparation continuations, and reject work from a previous session. Newer choices still invalidate older undo.
- Keep body-preview cancellation and inactive-preview cleanup pending while RaceMenu edits the player, then restore after it closes. Preserve unload cleanup and newer active previews.
- Prevent overlay-preview cancellation from being lost on RaceMenu entry. Keep per-area latest-choice coalescing and actor ordering, including deferred writes, so old undo cannot replace a newer preview, color edit, commit or reset.
- Reevaluate outfit correction after restoring a player's saved Default body on load or after RaceMenu. Use the existing disabled/nude/SFS/named/procedural policies; explicit reset and mod-removal cleanup still do not reapply corrections.
- Preserve distribution rules, co-save formats, supported SE/AE runtimes, RaceMenu interface contracts and pinned dependencies. No new permanent cache, engine-form destruction path or full-actor scan.
- Verification: Release build, 34 regression executables and 19 audit-tool tests passed. Product-function regressions cover restoration boundaries, with 1,000,000 mixed queue operations and an AddressSanitizer run. The eight-group host-allocation probe covers 3,029,300 allocations with no retained growth, including channelled undo and 2,000,000 parked-queue iterations. These are offline checks, not a whole-game leak or rendering guarantee. See [verification](docs/RELEASE-VERIFICATION-v1.3.0-KO.md).

## 1.2.9

- Restore an already-applied skin preview when its NPC unloads before cancellation. Distinguish undo from a new unloaded selection; restore owned native skin state without forcing 3D to load. Keep session and newer-selection guards, and recheck committed face state after detach.
- Clean up detached single and checkbox-batch overlay previews even when the actor has no committed BCNG record. Preserve newer previews, committed paints/colors, and foreign textures instead of allowing old cleanup to erase them.
- Record Default skin completion only when both private-body restoration and face cleanup succeed. Preserve restoration evidence after failure and do not describe a failed recovery as success.
- Keep normal application, compatibility checks, distribution rules, co-save schemas, dependencies, and supported SE/AE versions unchanged. No new permanent cache, polling loop, or engine-form destruction path was introduced.
- Verification: Release build, 32 regression executables, 19 audit-tool tests, and 12-runtime metadata relocation checks passed. Product-function tests cover partial failures/retry and 10,000 preview-detach cycles; queue tests cover detach, newer choices and session reset. These are offline checks, not whole-game leak or rendering guarantees. See [verification](docs/RELEASE-VERIFICATION-v1.2.9-KO.md).

## 1.2.8

- Restore each reference's face during ordinary all-actor reset, including references sharing an ActorBase. Coalesce equal Default requests and refresh the shared body once; an intervening skin selection invalidates older reset requests.
- Clear abandoned body-preview keys across NPC detach/cancel boundaries without loading 3D or deleting active previews, committed morphs, or foreign keys.
- Let the overlay color popup consume Esc/mapped Cancel without closing the main UI.
- Keep Cancel in the innermost removal-confirmation popup rather than closing its parent. Remember that confirmation's position like the other popups.
- Share immutable skin/futanari/tint catalogs, cache texture counts and tint-pack grouping, and clip off-screen rows while preserving keyboard focus and distribution checkboxes.
- Save actor choices in multiple bounded, existing-format ASTR records instead of truncating at 16,384 actors. Avoid empty records for unchanged distribution outcomes; discard only transient outfit-cache records on detach, preserving selections, Default intent, and restoration ownership.
- Recover face batches when the VM releases a callback without a response or its queued delivery is discarded. Keep rollback and request-generation checks; do not time out callbacks still held by the VM. Use the existing native reader/remover for old-node cleanup where available.
- Fix the allocation/deallocation contract for model alternate-texture arrays, including the engine's element-count header. Retain the previous array on allocation failure and release unpublished texture construction references. The relevant engine code was read on SE 1.5.97 and AE 1.6.1170; no live game memory was modified.
- Reclaim validated, unpublished ARMO/ARMA/FLST clones and TXST construction references if body/far-skin graph construction fails. Do not destroy completed persistent graphs that TNG may cache. Preserve restoration evidence if any private texture channel fails to restore before a graph replacement. Expanded fault tests cover callback loss, rollback routing, texture failures, array replacement and body/far-skin construction; these are not live leak measurements or a reproduction of the reported TNG texture issue.
- Restore the DDS paths in BCNG's retained private skin graphs on Default and session reset, not only the ActorBase pointers. A provider such as TNG can retain a composed armor referencing those private armatures; it must not retain the last BCNG selection through that graph. Original provider forms are not edited.
- Revalidate a previously prepared cache file when preparing a new texture application, and recreate a deleted alias from its still-available source. No filesystem checks were added to the native addon visitor or per-frame rendering.
- Add **Prepare for mod removal** to settings, with confirmation, persistent suspension of distribution/corrections, ownership-scoped cleanup, and a verified/incomplete result. Keep rule files and foreign morph keys. Restore saved face registrations for unloaded references through the existing public NiOverride path; clear each reference separately even when several actors share one body base. Retain restoration records if cleanup cannot be verified. After verification, save to a new slot, exit completely, then uninstall. This is not a general damaged-save repair tool.
- Keep the existing 12 SE/AE runtime targets and RaceMenu BodyMorph v4/v5, Override/Overlay v1/v2 adapters. No 1.7.x or VR support is added. The TNG/SOS size-change investigation found a TNG menu path that reselects an addon and rebuilds 3D before choosing size; the reported half-purple rendering has not been reproduced in-game.

- Validation covers existing features as well as these changes: Release build, 31 regression executables, 19 offline audit-tool tests, 12-runtime metadata relocation coverage, and separate allocation/catalog probes. The production distribution writer passed 88 scope/sex/feature cases and real file-replacement failure/retry tests. See [full feature verification](docs/RELEASE-VERIFICATION-v1.2.8-KO.md). No new in-game tests were performed; this is not a guarantee of zero bugs or whole-game leaks.

## 1.2.7

- Harden distribution condition-list reads against invalid faction/race/class/keyword objects: verify runtime type and current form identity before reading metadata. Read validated full-name data without invoking the unsafe name virtual; retain editor-ID hooks and use the existing ID label fallback if only the name component is unavailable. No faction is excluded by name or ID.

- Reset distribution checkbox mode, sex, candidate selections, and preview-dirty flags consistently when closing/reopening the main window or changing game sessions. Keep candidates intact when entering the condition popup so adding another rule still works.
- Collect faction/race/keyword/class/plugin options on the game task instead of walking engine arrays during UI drawing. Publish an owned, immutable result; reject late results after close/load and avoid per-frame scans or repeated failure retries. Saved rules and the All NPC/name targets do not depend on the options being ready.
- Add UI cleanup at serialization revert and New Game/PostLoadGame boundaries. Discard old actor/camera presentation references without restoring them into a new world, and invalidate queued camera updates from the previous session.
- Recheck cancellation and session validity at body-preview cleanup/body-apply/native-skin mutation boundaries. These are cooperative checks, not a way to interrupt an engine call already in progress.

- Add an explicit Enter-triggered name/hex RefID search for existing actor references, independent of the nearby dropdown's distance/32-NPC limit. Resolve names in bounded batches and discard stale searches after close/load/replacement; searching does not spawn NPCs or load cells.
- Allow validated manual body/skin/futanari/overlay choices to be stored for actors whose 3D is unloaded. Preview still requires loaded 3D. The existing attach queue rechecks compatibility and applies saved selections when the actor loads; saving the game preserves pending choices in the existing co-save format. Unspawned NPC bases require distribution rules rather than an individual RefID selection.
- Validation: Release build and all 28 regression executables passed, including invalid metadata entries, editor-ID hooks, current production reset helpers, cancellation checkpoints, snapshot lifetime, 400 publication/reset races, and pending-selection co-save round trips. Required relocation coverage was checked against the 12 supported SE/AE Address Library databases. The supplied 1.2.6 crash path is now guarded, but confirmation on the reporter's setup and remote selection/attachment still need in-game testing. The source of the invalid object is not established. Existing RaceMenu contracts, saved rules, and co-save format are unchanged.
- Allow hostile as well as non-hostile NPCs in the actor dropdown, including bandits. Keep the 4,096-unit radius, nearest-32 limit, and existing name/NPC/live-3D safety checks. Distribution rule matching is unchanged.

## 1.2.6

- Fix the 1.2.5 mouse delivery regression by taking button and wheel edges from Skyrim's input hook instead of relying on Windows mouse messages. Keep game-cursor coordinates and avoid duplicate Windows/Scaleform delivery.
- Preserve quick right-button clicks between render frames; ignore held/duplicate button samples and clear queued mouse state on focus loss, close, and reopen.
- Avoid redundant face DDS reloads on repeated 3D notifications when the immediate API confirms matching live properties, actual texture resources, renderers, and any required saved override. Different skins, broken textures, preview confirmation, Default restoration, and distinct first-person heads retain their application paths.
- Keep the preset-list optimizations, saved body-morph recovery, co-save format, distribution selections, and supported runtime table unchanged. No RSV/Selector compatibility patch or Skyrim 1.7.x support is added.
- Validation: Release build and 26 automated regression executables passed, including game-only input replay at 60/15/5 FPS. The reporter's skin flashing has not been reproduced; the redundant reload fix is not a claim that every flashing cause is resolved.

## 1.2.5

- Use one game-cursor coordinate source for native UI mouse input. Buffer Windows button edges until rendering instead of mixing OS cursor positions with Skyrim's cursor; avoid duplicate Scaleform mouse delivery.
- Share immutable preset-list metadata built on refresh instead of copying every preset's slider data each UI frame. Clip off-screen body-preset rows while retaining keyboard focus/navigation.
- Recheck saved body morph layers at actor reconciliation and 3D-update boundaries instead of trusting an earlier session-local success. Recover missing layers from the existing selection without rerolling distribution or interrupting previews.
- Generic-NPC selection filters are unchanged pending clarification of the report. In-game verification of the input and recovery changes remains pending.

## 1.2.4

- Accept valid UBE Zeroed presets without SetSlider entries in preview, confirmation, and saved-selection restoration.
- Apply standard UBE XML endpoints relative to their actual zeroed build defaults. Preserve omitted endpoints, negative and over-100 values, and the existing calculations for other body families.
- Check native body-rebuild completion directly so face refresh no longer depends solely on a NiNode notification. Preserve readiness checks, stale-request cancellation, and bounded failure handling.
- Build missing body/hand/foot Skin TXSTs from the source NIF's actual skin materials instead of hard-coded UBE paths. Preserve full channel baselines and normal-map conventions; route distinct atlases through private per-shape alternate textures instead of forcing one shared texture set. Existing native providers remain authoritative.
- Prepare reusable body targets independently of the first selected pack. Selecting a face-only pack first no longer leaves later body-containing packs without their native targets; face-only application still leaves the body detached.
- Improve custom-NPC body-family detection: live skin evidence takes priority over stale form/folder labels, addon evidence is limited to race-compatible body parts, and installed UBE cannot override a confirmed conventional layout. This addresses code-level causes of missing CBBE/UNP presets without forcing every custom NPC into one family.
- Invalidate the affected actor's cached family on existing 3D-update events and reduce routine classification logging. Reuse constructed skin graphs during pack switching; keep OBody/OClothe cleanup, morph-preservation options, partial DDS support, preview cancellation, and the co-save format.
- Update English and Korean guides: list OBody NG, Racial Skin Variance - SPID (RSV), and RaceMenu Selector of Skins - Unique Player Character as incompatible simultaneous appearance controllers. Rename the button to Register ORefit outfit-correction rules and link the ORefit JSON Master List.
- Remove the temporary face-preview diagnostic capture from the release source and build. No RSV/Selector compatibility hooks or automatic conflict-suppression patch are included.
- Validation: 25 automated test executables, 4,374 actor-family evidence combinations, and 17,045 reconstruction cases using installed UBE OSP data. Reporter-specific gameplay reproduction, including the AE 1.6.1179 body/face asymmetry, is not yet verified.

## 1.2.3

- Separate body-skin previews from committed application metadata and persistent player face overrides, including NPC-distribution checkboxes and Default Skin previews. Previewing the same pack no longer counts as explicit confirmation.
- Add bounded recovery for partially written face channels and native body/hand/foot texture graphs. Retain the last completed body snapshot for face failure recovery; respect later foreign ownership and never load an empty face texture path.
- Distinguish refreshed DDS content and preview/commit mode in request generations. Release completed callbacks and discard recovery snapshots at save-load boundaries while retaining reusable native graphs.
- Preserve partial DDS routing, NPC distribution, save record formats, and the morph fixes from 1.2.1/1.2.2. No new per-frame file scans, timers, or bulk diagnostics.
- Validation: 24 host regression targets, including preview/save codec boundaries and injected native channel-write failures. In-game repeated-switch and save/load validation remains pending.

## 1.2.2

- Fixed case-only slider-name collisions such as Breasts/breasts, Waist/waist and HipBone/Hipbone. XML low/high endpoints now merge into one morph while retaining the first spelling.
- Fixed previews and breast/nipple outfit correction combining differently cased names incorrectly. Preview cancellation still restores the committed state.
- UNP reverse-defined base sliders now use the same case-insensitive matching. Negative and over-100 XML values remain supported; XML files and the co-save format are unchanged.
- Retained all v1.2.1 features, including OBody/OClothe key cleanup with morph preservation enabled, Futanari Skin favorites and sex-specific NPC distribution controls.
- Includes the automatic checkbox-preview target exclusion for custom followers and elder NPCs added after the initial public v1.2.1 build. Manual actor selection and actual distribution rules are unchanged.

## 1.2.1

- NPC-distribution checkbox-mode automatic preview targeting skips custom followers and elder NPCs, including futanari targets. If no eligible nearby NPC remains, the player is selected. Manual actor selection and actual distribution rules are unchanged.
- NPC distribution now provides Female · Male · Distribute · Cancel distribution. It starts with Female and selects the nearest matching loaded NPC, falling back to the player. Futanari preview targets must be registered female futanari NPCs. Catalogs/new rules retain the requested sex independently of the preview actor; body/skin candidates also follow the configured distribution body type. Switching sex clears previous checkmarks/previews.
- Preset confirmation and Default body now also clear the exact `OBody` and `OClothe` keys, preventing their base-body/refit layers from stacking with BCNG. These two keys are removed even with Preserve other mods' morphs enabled; unrelated keys remain preserved.
- Previews show the post-cleanup result without deleting persistent keys until confirmation; cancellation restores the original state. Saved BCNG body states are re-evaluated once under the updated replacement policy. This does not disable redistribution by a running OBody installation.
- Added favorite stars and the Favorites filter to Futanari Skin, matching the other catalogs in both normal and NPC-distribution selection modes.
- Futanari favorites are saved in settings and restored after restart. ERF, TRX, and UBE TRX entries keep separate IDs; existing favorites and preferences are preserved.
- Favorite-star clicks do not preview/apply skins or toggle NPC-distribution checkboxes. Long row text leaves space for the star.

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
