# Body Change NG v1.2.2

## Changes from v1.2.1

- Fixed case-only slider-name collisions such as Breasts/breasts, Waist/waist and HipBone/Hipbone. XML low/high endpoints now merge into one morph while retaining the first spelling.
- Fixed previews and breast/nipple outfit correction combining differently cased names incorrectly. Preview cancellation still restores the committed state.
- UNP reverse-defined base sliders now use the same case-insensitive matching. Negative and over-100 XML values remain supported; XML files and the co-save format are unchanged.
- Retained all v1.2.1 features, including OBody/OClothe key cleanup with morph preservation enabled, Futanari Skin favorites and sex-specific NPC distribution controls.
- Includes the automatic checkbox-preview target exclusion for custom followers and elder NPCs added after the initial public v1.2.1 build. Manual actor selection and actual distribution rules are unchanged.

## Updating

1. Exit Skyrim. Back up saves together with matching `.skse` co-saves, settings, custom rules and asset packs.
2. Replace the BCNG DLL with v1.2.2, keeping only one active copy. Preserve `Data\SKSE\Plugins\BodyChangeNG\settings.json`, customized distribution rules and existing asset packs; do not replace your rules with the empty starter file.
3. Load your existing save and verify the selected body. A new game or save cleaning is not required solely for this update. To explicitly clear competing OBody layers, confirm a body preset or Default Body.

This cleanup does not disable a running OBody installation. Prevent both systems from continuously assigning bodies to the same actors. Meshes still need the correct Zeroed Sliders and Build Morphs output; key cleanup cannot change a baked-in base mesh.

## Verification — 2026-09-13

- SE/AE-only Release DLL: `build/v1.2.2/windows/x64/release/BodyChangeNG.dll`, resource version `1.2.2.0`.
- SHA-256: `BBEED2C787B8C275A9533282CC7BAFF6FCF71AC0E9A87A986B47BEF639D3B20B`.
- Nearest-NPC/female/male/futanari/player-fallback policy tests and UI wiring checks passed, including independent rule sex, configured body-type filtering, and stale-frame guards after toolbar changes.
- All 24 automated test executables passed. BodyMorphKeyTests passed 4,509 checks, including preservation on/off, exact-key cleanup, repeated case-variant preview/cancel/commit, OBody-only sliders, Default preview/reset and unrelated-key preservation. PresetCatalogTests also cover mixed-case XML endpoints, reversed endpoint order, +120/-150 values, named refits and UNP inversion.
- No new periodic scan, thread or save-format migration was added. The v1.2.1 replacement policy is unchanged; tests do not measure in-game timing or leaks.
- Deployed this follow-up DLL to both TuLED and TAKEALOOK MO2 installations and verified matching hashes. Settings/rule JSONs and asset packs were preserved; replaced files were backed up. In-game verification remains pending.

See [full usage](NEXUS-DESCRIPTION-v1.2.2.md) and [Nexus changelog](NEXUS-CHANGELOG-v1.2.2-EN.bbcode).
