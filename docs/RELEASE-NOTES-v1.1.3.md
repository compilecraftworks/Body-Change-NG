# Body Change NG v1.1.3

CBBE 3BA, BHUNP, and vanilla BodySkin hand/foot routing fix — 2026-09-06.

### Fixes

- Fixes the root cause where RaceMenu's single skin-slot operation traversed every ArmorAddon attached to the same Skin Armor, allowing the final foot texture assignment to repaint the hands.
- Conventional body families now apply body, hand, and foot textures only to each part's exact ArmorAddon and currently loaded geometry. CBBE-family feet still use the body atlas by design, while the hands retain the `femalehands_1` texture set.
- If SFS or another display system hides equipped footwear while the real feet slot remains occupied, BCNG stores a durable one-bit fallback for only that absent part. Override v0/v1 repairs all visible exact parts after its required legacy callback; v2 stores the missing key without live broad repainting.
- Player/NPC manual selection, direct NPC assignment, rule-based distribution, and equipment-rebuild recovery all share the corrected backend.
- A restored completion record is verified against the live body, hands, and feet once per new session. An incomplete old result is reapplied; an already-correct actor is skipped.
- UBE keeps its dedicated shared Body-atlas route because its body, hands, and feet intentionally use the same atlas.
- RaceMenu Override compatibility is classified as four explicit routes: legacy SE v0, UBE AE-backport v0, official v1, and official v2. The first three use the safe Papyrus string interface; only official v2 uses the audited native wrapper. Unknown future ABI versions fail closed.

### Performance and compatibility

- Adds no recurring filesystem scan, timer, or all-NPC polling. A missing-part fallback runs only during a requested apply and adds at most one coalesced node rebuild; ordinary exact-part applications retain the shorter path.
- Existing distribution rules, settings, `BodyChangeNGdistribution.json`, co-save data, partial skin packs, Default Skin restoration, and UBE routing remain compatible. A new save is not required.

### Validation and files

- The Release build and all 12 regression test executables passed.
- `Body-Change-NG-v1.1.3.zip` — MO2-ready release archive.
- `Body-Change-NG-v1.1.3-Source.zip` — buildable source from the matching Git revision.
- `SHA256SUMS-v1.1.3.txt` — SHA-256 checksums for both archives.

[한국어 변경 이력 및 업데이트 안내](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.1.3/docs/RELEASE-NOTES-v1.1.3-KO.md)
