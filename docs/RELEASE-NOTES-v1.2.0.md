# Body Change NG v1.2.0 — final release notes

This is the final implementation summary compared with **v1.1.4**, consolidated on 2026-09-13. It supersedes intermediate v1.2.0 experiments; it does not assert that a new archive/tag was published in this documentation update.

- [Nexus description, English BBCode](NEXUS-DESCRIPTION-v1.2.0-EN.bbcode)
- [Description, English Markdown](NEXUS-DESCRIPTION-v1.2.0.md)
- [Description, Korean HTML](NEXUS-DESCRIPTION-v1.2.0-KO.html)
- [Complete v1.1.4 comparison, English](NEXUS-CHANGELOG-v1.2.0-EN.txt)
- [Complete v1.1.4 comparison, Korean](NEXUS-CHANGELOG-v1.2.0-KO.html)

## Important behavior changes

Single-click previews; double-click/confirmation commits. Closing without confirmation restores the last committed selection. NPC conditions now save **only** through either explicit distribution action; closing cancels unsaved edits, reversing v1.1.4's auto-save-on-close behavior.

Body/hands/feet use private native Skin Armor/ARMA/TXST routing. Face skin uses dedicated NiOverride skin channels. Supported male/female SOS/TNG addons use native TXST construction, not the discarded general live-material path. Manual overlays support multiple entries per area; automatic rules select one candidate per area.

Futanari tools are enabled by supported female-addon installation/loading, while manual and NPC eligibility additionally require SOS/TNG registration. Tint masks remain player-only. OBody NPC-rule import and the distribution/exclusion mode are removed; explicit ORefit registration remains.

## Install and upgrade

### Latest UI and optional SFS integration

- The outfit/randomization popup opens centered on first use. Supported popups retain their individual positions. The title-bar rotation hint appears immediately left of X and scales to fit without truncation.
- Keyboard and gamepad independently follow the game's Activate and menu Cancel bindings for confirmation and closing.
- NPC-distribution checkbox mode supports per-overlay candidate color/opacity editing. Either explicit distribution action saves the checked candidates and their individual colors; these batch edits do not change the selected actor's committed overlays.
- [Skyrim Fitting System SE-AE](https://www.nexusmods.com/skyrimspecialedition/mods/187128) separates displayed outfits from actual equipment. With Rendered Outfit API v1, breast/nipple correction and ORefit rules use each visible actual/registered armor's own identity. Hidden items are ignored and hiding all correction-relevant clothing clears correction. Without SFS or its supported API, actual-equipment behavior remains unchanged.

### Installation paths

- Copy an entire **installed skin-mod folder** into Data\BodySkin\, preserving its Textures tree.
- Copy an entire **installed futanari-skin mod folder** into Data\Futanari\, preserving its Textures tree. Install the actual addon normally.
- BodySlide XML directory: Data\CalienteTools\BodySlide\SliderPresets\
- Tint directory example: Data\BodySkin\Your Pack\Textures\actors\character\character assets\tintmasks\
- In MO2, the mod's root is Data: omit the initial Data\ inside that mod.
- Back up saves with their .skse co-saves, settings, user distribution JSON, and packs. Keep customized rules rather than overwriting them with the empty installer starter.
- Review schema-7 positive-rule migration; legacy exclusion entries are discarded. Keep asset names/paths stable. Downgrade compatibility is not guaranteed.

## Requirements and verification boundary

Exact targets: SE 1.5.97; AE 1.6.317 / 1.6.318 / 1.6.323 / 1.6.342 / 1.6.353 / 1.6.629 / 1.6.640 / 1.6.659 / 1.6.1130 / 1.6.1170 / 1.6.1179.

Matching SKSE64, Address Library, and RaceMenu are required. No VR, Epic 1.6.678, Store/Game Pass, or unlisted runtimes including 1.7.x. A newer dependency DLL is not automatically compatible with an older game.

The final SE/AE-only Release build and 21 automated test executables passed in the [2026-09-12 audit](COMPATIBILITY-UI-20260912-KO.md); [raw results](COMPATIBILITY-UI-20260912-TESTS.json). Native genital ownership code was inspected on 1.5.97 and 1.6.1170. Other targets require runtime code checks and are not all play-tested. Known RaceMenu interfaces were audited, not every future or unofficial release. No claim of exhaustive leak-free/in-game coverage is made.

The 2026-09-13 update passed the Release build and all 23 current test executables, including actual SFS-consumer boundary tests and ImGui popup/title-bar geometry and clipping tests. See the [SFS integration audit](SFS-OREFIT-INTEGRATION-20260913-KO.md). New in-game coverage is not claimed.
