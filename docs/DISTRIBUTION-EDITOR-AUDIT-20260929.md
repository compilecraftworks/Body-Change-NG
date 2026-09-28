# Distribution editor UI revision — 2026-09-29

## Scope

- Each existing distribution-capable catalog has both NPC distribution and Distribution conditions buttons.
- Direct conditions entry opens the originating feature tab without selecting assets or creating a rule. Catalog-first distribution retains the originating tab and its chosen candidates.
- The conditions popup has Body Presets, Body Skins, Futanari Skin and Overlays tabs in the former explanatory-text row.
- Edit items is immediately beside the item count. Its nested modal has a search, checkboxes, select-all/clear controls, and Save/Close footer. It does not apply or preview assets on an actor.
- Nested Save updates only the corresponding rule's candidate pool. Close, X and Cancel discard that nested edit. The parent Save is beside Close at the bottom right (revised per user request), persists through the existing atomic writer, and activates rules without forcing the immediate loaded-NPC pass. Footer actions wrap when the localized labels do not fit; the rule list reserves the required footer height.
- Existing immediate distribution and next-launch save actions remain. Catalog-first Add retains the original candidate-copy workflow for the same feature/sex. Direct-entry Add starts empty.
- Display prefixes identify each rule's feature without changing its persistent ID or user-authored name.
- Missing/filtered existing candidates remain visible and selected; retained overlay IDs keep their existing RGBA values. No schema, runtime eligibility, automatic UBE distribution policy, skin/morph application, or engine input-hook changes.
- Empty draft feature ownership and nested item selections are UI-local and cleared on editor/session reset. The existing writer continues to omit rules with no candidate pools; this change does not introduce empty-rule persistence.

## Checks

Release plugin build passed using the repository-pinned XMake 3.1.0 and the existing flat SE/AE configuration.

17 test executables passed:

- DistributionEditorTests
- DistributionEditorUITests
- DistributionLifecycleTests
- DistributionPresetTests
- DistributionPersistenceTests
- DistributionAuthoringTests
- DistributionTargetReadTests
- CodePatternTests
- PopupPlacementTests
- HotkeyTests
- MouseInputReplayTests
- WheelFilterTests
- OverlayPolicyTests
- PreviewRestoreTests
- AppearanceLifecycleTests
- FrameTaskQueueTests
- RuntimeLayoutTests

The new UI replay compiles the production drawing/entry/Save functions, sends actual ImGui mouse/keyboard events, and uses stand-ins only at engine/catalog/persistence boundaries. It covers both entry routes for all four feature tabs, nested cancel/save, failed writes, the original seeded Add path, empty tab ownership, retained parent drafts, source-tab selection, and 20,000-row clipping. Layout checks use the host's 20px Segoe UI/Malgun/Microsoft YaHei font combination for Korean, English and Chinese at 0.75/1.0/1.5 scale. The production editable-text detection and Escape handler are exercised for the new search and existing rule-name fields.

These are offline tests, not an in-game test or a measurement of engine-wide memory usage. The UI build is included in the subsequent EditorID/MO2 update below; no release ZIP replacement, Git commit or push is part of that update.

## Faction EditorID fallback (2026-09-29)

Runtime target acquisition and its validated form/component reads remain unchanged. When a faction's runtime EditorID is empty, a file-only worker reads its originating plugin's FACT/EDID record through the game's Data path (MO2 VFS). Runtime/provider-supplied IDs win. Localized names, plugin/local ID identity, runtime IDs and stored rule values are preserved; unavailable files leave the original label usable.

The reader supports compressed records, XXXX subrecords, ordinary and light plugins. TES4 master count distinguishes newly owned records from overrides sharing the same low 24-bit ID. It validates boundaries and stops stale sessions. Unrelated groups are skipped using file seeks, not loaded into memory. Per-record compressed/uncompressed buffers have 16 MiB limits; normal faction records are much smaller. There is no whole-plugin cache or detached thread.

The existing catalog worker now has a latest-only submission option for this metadata scan: one running request and at most one replaceable successor. Ordinary catalog refresh suppression is unchanged. Session snapshots reuse results until the existing UI/session reset. Game forms are collected on the game task; file scans only receive owned strings and numbers.

### Verification

All 17 tests above were rerun successfully. Four additional tests passed: FactionEditorIDTests, CatalogRefreshTests, OBodyNumericTests and RaceMenuExtraDataTests (the latter's private-DLL image test was not part of this run).

- Production faction label helper exercised with real filesystem reads: case-insensitive plugin names, runtime-provider precedence, missing plugin fallback, localized name preservation, unchanged IDs, cancellation and unrelated category preservation.
- Parser fixtures: plain/zlib/XXXX, master override collision, light local IDs, low new-runtime IDs, deleted/duplicate records, malformed/truncated/oversized input, cancelled work, 2,000 repeat reads.
- Queue: 10,000 ordinary duplicate submissions and 10,000 latest replacements, superseded closure release, exception/retry; existing lifecycle test also covers 400 publish/reset races.
- Real TuLED Skyrim.esm (249,753,573 bytes): verified `0001BCC0 = BanditFaction` and `00000DB1 = PlayerFaction`. Both requested records required 30,874 bytes of I/O.
- Header-enumerated all 1,084 factions: all recovered, 151,958 bytes read, 1.2276 ms on the offline filesystem with OS cache already warm. This is not an in-game/MO2 frame-time measurement and does not claim the same time for every load order.
- Release DLL builds in the existing SE/AE flat configuration. Import inspection confirms no additional zlib runtime DLL is required.

### Dependency pin

Added static zlib **1.3.2**, upstream tag `v1.3.2`, GitHub tag archive SHA-256 `b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1`. XMake recipe commit: `e36e822129b0fcbdfb51633a7fcee8c76af344bf`. Other pinned packages/CommonLib closure unchanged. Upstream 1.3.2 changelog and `uncompress2` API reviewed: uses stable decompression interface, no engine ABI/layout dependency. No zlib source modifications. Full copyright/license added to THIRD_PARTY_NOTICES.md.

Sources: https://zlib.net/ ; https://github.com/madler/zlib/tree/v1.3.2 ; https://github.com/madler/zlib/blob/v1.3.2/ChangeLog

## Follow-up UI changes after the deployment recorded below

These later changes were built/tested locally and then copied to TuLED MO2 at the user's request (2026-09-29). Skyrim and its loader were stopped; the active profile's only enabled BCNG DLL provider was the Body Change NG mod.

- Latest DLL SHA-256: `459E1BDD768BB90D917B7BDB6893AC1F6C810AA54ECDE78A9E6A9AA1DDB14861`, 3,117,056 bytes. Installed/source hashes match.
- Previous DLL backed up to `build/mo2-backups/TuLED-20260929-ui-search-3e3f5ff8133b44b89193258bbeaa9386/BodyChangeNG.dll`.
- Only the DLL was replaced; installed config hash unchanged. No assets, rules, profile settings, release ZIPs or Git remote changes.

- Parent Save moved beside footer Close; localized footer actions wrap without overlapping. Distribution item-count text is aligned to the Edit items button's frame padding.
- Save success/failure appears temporarily inside the conditions modal, to the right of its feature tabs, instead of posting to the main UI. Text fits the available line; hover shows the full message. Closing/resetting the editor clears this status.
- Plugin/race/faction/keyword/class dropdowns have a fixed search field and separately scrolling, clipped result list. Search covers displayed names, EditorID, plugin and hex runtime/local FormID, case-insensitively. One current dropdown cache is built on opening; filtering reruns only when the query changes. Saved selections are not changed by typing, and missing saved targets remain available. Row selection explicitly closes the combo; Escape uses the existing input-consumption path, dismissing text editing before the combo, without dismissing the parent modal.
- Actual ImGui replay tests exercise all five dropdown types with 20,001 options each: mixed-case names/EditorIDs/plugins/runtime IDs, filtered selection identity, no-result queries, keyboard-input ownership, Escape, clipping and cache release. Save-status routing and nine Korean/English/Chinese layout/scale combinations pass.
- Automatic nearby distribution-preview selection now requires ActorTypeNPC and the human/elf race families: Nord, Breton, Imperial, Redguard, High Elf, Wood Elf, Dark Elf, Orc. Standard and vampire FormIDs plus matching custom race EditorIDs (e.g. UBE NordRace) are recognized. Khajiit, Argonian, creatures and unidentified race families are not automatic candidates. Existing sex, follower, elder and futanari checks and player fallback remain. Direct actor selection and real distribution matching are unchanged.
- The body card says “현재 액터로 미리보기 불가” for incompatible previews. A reversible distribution preview on a nearby NPC, whose current backend selection matches that row, says “가까운 액터로 배포항목 미리보기 중”. The repeated incompatible-actor notification is removed; compatibility gating/rollback remain.
- Release build and DistributionEditorUITests, DistributionLifecycleTests, DistributionPresetTests, SkinArchitectureTests and CodePatternTests passed after these changes. Latest alignment edit reran the UI replay and release build. These are offline tests, not a new in-game verification.

### TuLED MO2 deployment

Deployed while Skyrim/skse64_loader were stopped. Active MO2: `D:\TuLED\MO2\ModOrganizer.exe`, profile `TuLED(SL)`. Checked enabled mod providers, overwrite and stock Data: only the enabled Body Change NG mod supplied this DLL.

- Destination: `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`
- SHA-256: `2324CBFE4AFD76DCE063F047D065B6E6CBE4B92F8DC1D5490F09826A030B49AF`, 3,105,280 bytes; source/destination hashes matched.
- Backup: `build/mo2-backups/TuLED-20260929-editorid-278f5ec20ec247cfb0834fd845466aee/` (previous DLL and notices).
- Previous DLL SHA-256: `202AE96735F0A17D7F7BE6801DC37C3CA1DDE0858107BA62670582EC3BF4D9A6`.
- Only DLL and THIRD_PARTY_NOTICES.md copied. Existing installed config hash unchanged. No rules, assets or MO2/profile settings overwritten. Version remains 1.3.5. No game launch or in-game test performed.
