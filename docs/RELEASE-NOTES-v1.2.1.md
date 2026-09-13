# Body Change NG v1.2.1

## Changes from v1.2.0

- Distribution toolbar: Female / Male / Distribute / Cancel distribution; defaults to Female. Nearest locally loaded matching NPC is selected, falling back to the player. Futanari requires a registered female futanari NPC. Requested catalog/rule sex is independent of the preview actor; body/skin lists follow configured distribution body types. Switching sex clears previous selections and previews. Automatic preview targeting in NPC-distribution checkbox mode skips custom followers and elder NPCs, including futanari targets. Manual actor selection and actual distribution rules are unchanged.

- Futanari Skin favorites now work in normal and NPC-distribution lists and persist in settings. ERF, TRX and UBE TRX retain separate identities.
- Preset confirmation, automatic body application and Default Body clear the exact `OBody` and `OClothe` keys, even when **Preserve other mods' morphs** is on. Other keys still follow the preservation option. Existing BCNG-managed actors follow the same cleanup scope during a full reset.
- Preview simulates cleanup without deleting persistent keys; cancellation restores them. Default preview visits morph values once instead of four times.
- Existing BCNG body states are re-evaluated once under the updated policy. The ASTR co-save format remains version 6; no per-save legacy calculation mode was introduced.

## Updating

1. Exit Skyrim. Back up saves together with matching `.skse` co-saves, settings, custom rules and asset packs.
2. Replace the BCNG DLL with v1.2.1, keeping only one active copy. Preserve `Data\SKSE\Plugins\BodyChangeNG\settings.json`, customized distribution rules and existing asset packs; do not replace your rules with the empty starter file.
3. Load your existing save and verify the selected body. A new game or save cleaning is not required solely for this update. To explicitly clear competing OBody layers, confirm a body preset or Default Body.

This cleanup does not disable a running OBody installation. Prevent both systems from continuously assigning bodies to the same actors. Meshes still need the correct Zeroed Sliders and Build Morphs output; key cleanup cannot change a baked-in base mesh.

## Verification — 2026-09-13

- SE/AE-only Release DLL: `build/v1.2.1/windows/x64/release/BodyChangeNG.dll`, resource version `1.2.1.0`.
- SHA-256: `61379E22BBA9F341BC5C827C816167FCBD44A90E4CA78B848B2DF89B107733B9`.
- Nearest-NPC/female/male/futanari/player-fallback policy tests and UI wiring checks passed, including independent rule sex, configured body-type filtering, and stale-frame guards after toolbar changes.
- All 24 automated test executables passed. BodyMorphKeyTests passed 2,936 checks, including preservation on/off, exact-key cleanup, repeated preview/cancel/commit, OBody-only sliders, Default preview/reset and unrelated-key preservation.
- No new periodic scan or thread. First-load reconciliation is a one-time policy transition; no in-game timing or leak claim is made by the unit tests.
- Deployed this follow-up DLL to both TuLED and TAKEALOOK MO2 installations and verified matching hashes. Settings/rule JSONs and asset packs were preserved; replaced files were backed up. In-game verification remains pending.

See [full usage](NEXUS-DESCRIPTION-v1.2.1.md) and [Nexus changelog](NEXUS-CHANGELOG-v1.2.1-EN.bbcode).
