# Addon size changes and removal preparation — 2026-09-19

Working-tree investigation following v1.2.7. Not a released or in-game-certified fix.

## Evidence and limits

- User-supplied TNG 4.2.6 archive: inspected its `TNG_MCM.psc`, `TNG_PapyrusUtil.psc`, and extracted DLL identity. DLL SHA-256: `76EBF88CE01372D1E0F9B6D10C86226A4226F7F1F664CED077F8CCDA6ECEBD0A`. No supplied binary was executed or installed.
- In that MCM, `ShowTNGMenu` may call `TNGSetAddon` before presenting the size list. `TNGSetAddon` calls `SetActorAddon` and then `QueueNiNodeUpdate` when not mounted. Therefore a user describing a size change can also have passed through an addon/3D rebuild. Global/race size sliders call `SetActorSize` separately.
- Public TNG source pinned for analysis at [db688f2d408553d6125e110067ac56673a484e8e](https://github.com/ModiLogist/TheNewGentleman/tree/db688f2d408553d6125e110067ac56673a484e8e): `SetActorSize` scales genital/scrotum bones; `SetPlayerInfo` stores addon/size choices. `GetSkinWithAddonForRg` caches a composed armor containing the parent armatures. This is upstream source evidence, not proof that every instruction in the supplied 4.2.6 DLL matches this commit. Its MCM differs from the supplied archive.
- Read the actual `SOS_SetupQuest_Script.pex` functions in TOFU, TuLED, and TuLED's PapyrusUtil & SexLab Tweaks override, using the existing read-only PEX decoder. `SetSchlongSize` sets faction ranks and calls `ScaleSchlongBones`. The originals can dispatch to SOS_SKSE; TOFU's fallback and the TuLED tweak use NetImmerse node scaling (including first-person player scaling). No direct DDS assignment was found in those decoded size functions. This does not establish every installed override's final load order or prove the native SOS DLL internals.
- Both male and female addon selections use BCNG's slot-52 native supply. Neither scale value nor bone transform is a texture selection key. Hair/non-SkinTint materials remain excluded, ARMO/ARMA/node targets remain explicit, and geometry pointers are not retained across rebuilds. No size hook, forced addon switch, continuous scan, or repeated blanket 3D reset was added.

## Confirmed BCNG gaps addressed

1. Default/session reset detached skin pointers but retained selected DDS strings in private cloned TXST forms. A composed armor retaining the private armatures could still see those strings. Restore all private texture channels as well as owned pointers, without editing original provider forms. Attempt all channels even if one write fails.
2. Texture preparation trusted a cached-positive existence result indefinitely. Recheck when preparing a new application and recreate a removed cache alias if its source remains. The native visitor still does not perform disk I/O. Deleting source/cache files while a selected skin is active remains unsafe and is not an uninstall procedure.

These findings do **not** establish that the user's save files themselves were corrupted, nor that either finding fully explains the reported half-purple genital texture. A live sequence using the affected addon/skin pack is still needed to verify visual behavior after size/addon changes, save/reload, and process restart.

## Removal workflow

1. Keep a backup save and the currently installed skin packs/cache.
2. Open settings → **Prepare for mod removal** → confirm **Start cleanup**.
3. Distribution, outfit correction, and normal editing are suspended persistently. Existing rule files remain unchanged. Cleanup removes only BCNG/legacy-BCNG morph keys, not OBody/OClothe or arbitrary foreign keys; it cannot reconstruct foreign morphs erased by an earlier user-requested non-preserving preset commit.
4. Body/far-skin private forms return to original paths and owned pointers detach. Male/futanari runtime selections clear. Exact owned overlay registrations and player tint changes restore. Saved face registrations on unloaded actors restore through NiOverride without loading cells or creating actor 3D. Every reference's face is independent even if its body base is shared.
5. Completion waits for queued work and asynchronous leases/face batches, then checks remaining morphs, private skin paths, face baselines, overlay ownership and original tint values. Missing interfaces, unresolved actors, or failed restoration mean **incomplete**, not safe-to-remove. Evidence remains for retry. A session change invalidates the previous result; verify the newly loaded save again.
6. Only after **Cleanup verified**, close the UI, save to a **new slot**, exit Skyrim completely, and uninstall BCNG. Do not delete older saves or other mods' configuration. Do not remove DDS/cache files first.

**Resume BCNG** ends persistent suspension; it does not undo cleanup or recover erased selections. An already broken save is not guaranteed repairable. No save files are edited by this feature.

## Compatibility and acceptance

The support table remains 1.5.97 and 1.6.317/318/323/342/353/629/640/659/1130/1170/1179, SE/AE-exclusive. No dependency upgrade, new runtime layout, or new RaceMenu virtual slot is introduced. Existing BodyMorph v4/v5 and Overlay/Override v1/v2 adapters remain; unloaded face cleanup uses existing public NiOverride calls.

Automated checks cover cleanup completion gates, shared-base Default generation handling, asynchronous-lease draining, private-path restoration with injected write failures, foreign face-key preservation, persisted removal mode, deleted-cache repair, and the existing addon ownership/runtime policy suite. Automated checks and Address Library coverage are not substitutes for testing every game/RaceMenu combination in-game.

Acceptance run: SE/AE release build succeeded and all 29 test executables passed. No MO2 installation, release archive replacement, or GitHub publication was performed for these working-tree changes.

Read-only Address Library audit: all 9 core relocation IDs selected by `NativeAddonLayout.h` exist with nonzero offsets in each of the 12 supported runtime databases installed in TOFU. This verifies address-table coverage only; startup instruction/callsite validation is still required and no runtime guard was relaxed.
