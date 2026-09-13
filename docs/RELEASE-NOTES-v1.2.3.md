# Body Change NG v1.2.3

## Changes from v1.2.2

- Separate body-skin previews from committed application metadata and persistent player face overrides, including NPC-distribution checkboxes and Default Skin previews. Previewing the same pack no longer counts as explicit confirmation.
- Add bounded recovery for partially written face channels and native body/hand/foot texture graphs. Retain the last completed body snapshot for face failure recovery; respect later foreign ownership and never load an empty face texture path.
- Distinguish refreshed DDS content and preview/commit mode in request generations. Release completed callbacks and discard recovery snapshots at save-load boundaries while retaining reusable native graphs.
- Preserve partial DDS routing, NPC distribution, save record formats, and the morph fixes from 1.2.1/1.2.2. No new per-frame file scans, timers, or bulk diagnostics.
- Validation: 24 host regression targets, including preview/save codec boundaries and injected native channel-write failures. In-game repeated-switch and save/load validation remains pending.

## Updating

Exit Skyrim before updating. Preserve settings, custom distribution rules, asset packs, presets and matching `.ess`/`.skse` saves. Do not overwrite custom rules with the empty starter. This update retains the existing co-save format; a new game or save cleaning is not required solely for this update.

## Build and verification

Release DLL: **1.2.3.0**, 2,776,576 bytes. SHA-256: `DD648BA5E0C07950B05902451ED6A0510AAF0A70E565D27D21D0156277E12EBD`.

All 24 host test executables passed, including 2,000 preview/save codec boundaries, channel-write fault injection, and the retained 4,509 morph-key checks. These are not in-game renderer, timing, or leak measurements.

[Full usage](NEXUS-DESCRIPTION-v1.2.3.md) · [Detailed skin audit](SKIN-TRANSACTION-v1.2.3-KO.md)
