# Release documentation

Current version: **Body Change NG v1.2.9**. See the [English changelog](../CHANGELOG.md#129), [한국어 변경 내역](../CHANGELOG-KO.md#129), [current usage / removal preparation](../README.md), and [verification](RELEASE-VERIFICATION-v1.2.9-KO.md). Earlier version files remain historical records.

## Current v1.2.9 release documents

| Purpose | English | 한국어 |
| --- | --- | --- |
| Formatted changelog | [Nexus BBCode](NEXUS-CHANGELOG-v1.2.9-EN.bbcode) | [Standalone HTML](NEXUS-CHANGELOG-v1.2.9-KO.html) |
| Nexus changelog field | [Plain text](NEXUS-CHANGELOG-v1.2.9-EN.txt) | [일반 텍스트](NEXUS-CHANGELOG-v1.2.9-KO.txt) |

The detailed v1.2.4 descriptions below remain the base installation guides; the
current README and changelogs document later behavior and removal preparation.

## Base usage documents — v1.2.4

| Purpose | English | 한국어 |
| --- | --- | --- |
| Full description and usage | [Nexus BBCode](NEXUS-DESCRIPTION-v1.2.4-EN.bbcode) · [Markdown](NEXUS-DESCRIPTION-v1.2.4.md) | [Standalone HTML](NEXUS-DESCRIPTION-v1.2.4-KO.html) |
| Formatted changelog | [BBCode](NEXUS-CHANGELOG-v1.2.4-EN.bbcode) | [Standalone HTML](NEXUS-CHANGELOG-v1.2.4-KO.html) |
| Nexus changelog field | [Plain text](NEXUS-CHANGELOG-v1.2.4-EN.txt) | [일반 텍스트](NEXUS-CHANGELOG-v1.2.4-KO.txt) |

The descriptions list OBody NG, Racial Skin Variance - SPID (RSV), and RaceMenu Selector of Skins - Unique Player Character as incompatible simultaneous appearance controllers. RSV/Selector skin reapplication is not blocked by BCNG, and no built-in conflict prevention or separate compatibility patch is provided. The descriptions also cover the separate optional OBody ORefit JSON input, UBE baseline calculation, and improved native skin / custom-NPC detection. Full installation paths and folder-copy examples remain intact.

## Previous version — v1.2.3

| Purpose | English | 한국어 |
| --- | --- | --- |
| Full description and usage | [Nexus BBCode](NEXUS-DESCRIPTION-v1.2.3-EN.bbcode) · [Markdown](NEXUS-DESCRIPTION-v1.2.3.md) | [Standalone HTML](NEXUS-DESCRIPTION-v1.2.3-KO.html) |
| Formatted changelog | [BBCode](NEXUS-CHANGELOG-v1.2.3-EN.bbcode) | [Standalone HTML](NEXUS-CHANGELOG-v1.2.3-KO.html) |
| Nexus changelog field | [Plain text](NEXUS-CHANGELOG-v1.2.3-EN.txt) | [일반 텍스트](NEXUS-CHANGELOG-v1.2.3-KO.txt) |
| Full version history | [Markdown](../CHANGELOG.md) | [Markdown](../CHANGELOG-KO.md) |
| Upgrade notes and verification boundary | [Markdown](RELEASE-NOTES-v1.2.3.md) | [Markdown](RELEASE-NOTES-v1.2.3-KO.md) |
| Existing short description | [Plain text](NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt) | [일반 텍스트](NEXUS-SHORT-DESCRIPTION-v1.1.0-KO.txt) |

Paste the English description into Nexus's **BBCode editor**. Use plain text in Nexus changelog fields that do not accept BBCode. Korean HTML is a self-contained browser document, not a Nexus BBCode input.

Installation paths are written in full, relative to the game's Data folder. Copy whole **installed** skin-mod folders into BodySkin and futanari-skin mod folders into Futanari; preserve their Textures trees. In MO2, the mod root corresponds to Data. The preset directory is Data\CalienteTools\BodySlide\SliderPresets\. Tint masks share BodySkin; the old standalone TintMask root is obsolete.

Prior version files and dated engineering reports remain historical records. In particular, an old v1.2.0 experiment mentioning a Face TXST/HeadPart swap, preview commit on close, or rule auto-save on close does not describe the final release.

Current verification evidence: [v1.2.9 results](RELEASE-VERIFICATION-v1.2.9-KO.md). Historical audits: [v1.2.8 whole-feature results](RELEASE-VERIFICATION-v1.2.8-KO.md) · [compatibility and UI](COMPATIBILITY-UI-20260912-KO.md) · [raw tests](COMPATIBILITY-UI-20260912-TESTS.json) · [full audit](FULL-AUDIT-20260912-KO.md).

Latest documentation includes optional [SFS](https://www.nexusmods.com/skyrimspecialedition/mods/187128) outfit-correction integration, remembered popup positions, per-candidate colors in NPC-distribution checkbox mode, the fitted title-bar rotation hint, and keyboard/gamepad Activate/Cancel bindings. See the [SFS integration audit](SFS-OREFIT-INTEGRATION-20260913-KO.md).

The unchanged short description is retained separately. Documentation updates do not publish a release archive, upload to Nexus, or establish exhaustive in-game compatibility.
