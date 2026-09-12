# Release documentation

Current documentation: **Body Change NG v1.2.0**, consolidated 2026-09-13 against **v1.1.4**. The public files below describe the final implementation, not intermediate experiments.

| Purpose | English | 한국어 |
| --- | --- | --- |
| Full description and usage | [Nexus BBCode](NEXUS-DESCRIPTION-v1.2.0-EN.bbcode) · [Markdown](NEXUS-DESCRIPTION-v1.2.0.md) | [Standalone HTML](NEXUS-DESCRIPTION-v1.2.0-KO.html) |
| Formatted changelog | [BBCode](NEXUS-CHANGELOG-v1.2.0-EN.bbcode) | [Standalone HTML](NEXUS-CHANGELOG-v1.2.0-KO.html) |
| Nexus changelog field | [Plain text](NEXUS-CHANGELOG-v1.2.0-EN.txt) | [일반 텍스트](NEXUS-CHANGELOG-v1.2.0-KO.txt) |
| Full version history | [Markdown](../CHANGELOG.md) | [Markdown](../CHANGELOG-KO.md) |
| Upgrade notes and verification boundary | [Markdown](RELEASE-NOTES-v1.2.0.md) | [Markdown](RELEASE-NOTES-v1.2.0-KO.md) |
| Existing short description | [Plain text](NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt) | [일반 텍스트](NEXUS-SHORT-DESCRIPTION-v1.1.0-KO.txt) |

Paste the English description into Nexus's **BBCode editor**. Use plain text in Nexus changelog fields that do not accept BBCode. Korean HTML is a self-contained browser document, not a Nexus BBCode input.

Installation paths are written in full, relative to the game's Data folder. Copy whole **installed** skin-mod folders into BodySkin and futanari-skin mod folders into Futanari; preserve their Textures trees. In MO2, the mod root corresponds to Data. The preset directory is Data\CalienteTools\BodySlide\SliderPresets\. Tint masks share BodySkin; the old standalone TintMask root is obsolete.

Prior version files and dated engineering reports remain historical records. In particular, an old v1.2.0 experiment mentioning a Face TXST/HeadPart swap, preview commit on close, or rule auto-save on close does not describe the final release.

Current verification evidence: [compatibility and UI audit](COMPATIBILITY-UI-20260912-KO.md) · [raw tests](COMPATIBILITY-UI-20260912-TESTS.json) · [full audit](FULL-AUDIT-20260912-KO.md).

Latest documentation includes optional [SFS](https://www.nexusmods.com/skyrimspecialedition/mods/187128) outfit-correction integration, remembered popup positions, per-candidate colors in NPC-distribution checkbox mode, the fitted title-bar rotation hint, and keyboard/gamepad Activate/Cancel bindings. See the [SFS integration audit](SFS-OREFIT-INTEGRATION-20260913-KO.md).

The unchanged short description is retained separately. Documentation updates do not publish a release archive, upload to Nexus, or establish exhaustive in-game compatibility.
