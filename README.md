# Body Change NG — v1.2.0

Native SKSE appearance control for Skyrim SE/AE: BodySlide presets, body and
face skins, player tint masks, supported futanari skins, and Face/Body/Hands/Feet
overlays. Direct player/NPC editing and opt-in NPC rules share one F7 interface.
No BCNG ESP/ESL or MCM is required. Third-party meshes and texture packs are not bundled.

## Final user guides and release comparison

Use these final v1.2.0 guides rather than intermediate implementation reports:

- [English description and complete usage](docs/NEXUS-DESCRIPTION-v1.2.0.md)
- [English Nexus BBCode](docs/NEXUS-DESCRIPTION-v1.2.0-EN.bbcode) · [한국어 HTML 소개글](docs/NEXUS-DESCRIPTION-v1.2.0-KO.html)
- [Final v1.1.4 comparison, English](docs/NEXUS-CHANGELOG-v1.2.0-EN.txt) · [한국어 변경 내역](docs/NEXUS-CHANGELOG-v1.2.0-KO.txt)
- [Upload formats](docs/README.md) · [Full history](CHANGELOG.md) · [전체 이력](CHANGELOG-KO.md)
- [Upgrade notes](docs/RELEASE-NOTES-v1.2.0.md) · [한국어 업데이트 안내](docs/RELEASE-NOTES-v1.2.0-KO.md)
- [Architecture](docs/ARCHITECTURE-v1.2.0-KO.md) · [Verification limits](docs/COMPATIBILITY-UI-20260912-KO.md)

## Requirements and compatibility

Matching **SKSE64, Address Library, and RaceMenu** are required.
Body changes also need compatible XML presets and body/outfit meshes with
**Build Morphs** TRI output. Other features need their corresponding assets/addons.

Explicit targets:

- SE **1.5.97**
- AE **1.6.317 / 1.6.318 / 1.6.323 / 1.6.342 / 1.6.353 / 1.6.629 / 1.6.640 / 1.6.659 / 1.6.1130 / 1.6.1170 / 1.6.1179**

No VR, LE, Epic 1.6.678, Store/Game Pass, or unlisted runtimes including 1.7.x.
Do not assume the newest dependency download matches an older game.

The final Release build and 23 automated test executables passed. This is not
complete in-game/leak verification. Native genital ownership code was inspected
on **1.5.97 and 1.6.1170**; the other ten targets must pass runtime code verification
and the backend may remain unavailable. Known RaceMenu interface generations
are audited, not every future ABI-breaking release or unofficial fork.

## Installation

Copy the entire **installed skin-mod folder** into BodySkin, or the entire
**installed futanari-skin mod folder** into Futanari. Keep its Textures tree.
Use the resolved FOMOD installation, not an archive of competing options.
Install required body/addon plugins normally; copying them below an asset-pack
root does not activate them as normal mods.

Complete paths relative to the game installation:

    Data\SKSE\Plugins\BodyChangeNG.dll
    Data\CalienteTools\BodySlide\SliderPresets\
    Data\BodySkin\
    Data\Futanari\
    Data\BodySkin\My Skin\Textures\actors\character\character assets\tintmasks\
    Data\SKSE\Plugins\BodyChangeNG\settings.json
    Data\SKSE\Plugins\BodyChangeNGdistribution.json
    Data\Textures\BodyChangeNG\Cache\
    Data\SKSE\Plugins\OBody_presetDistributionConfig.json

In MO2, the mod root corresponds to Data: omit the initial Data directory
inside a mod. The full guides provide before/after copy examples and complete
female/male/UBE/TRX/ERF DDS paths. No profile.json is required. Tint masks share
BodySkin; the old standalone TintMask root is obsolete.

The runtime installer has the DLL, empty opt-in rule JSON, **three** asset-folder
guides, LICENSE, and THIRD_PARTY_NOTICES.md. Preserve custom rules and packs
during upgrades rather than overwriting them with the empty starter.

## Key final behavior

Tab order: **Body Presets → Body Skins → Tint Masks → Futanari Skin → Overlays**.

- Single-click/list movement previews; double-click or confirmation commits.
  Closing without confirmation restores the prior committed selection.
- Keyboard/gamepad confirmation follows the game's Activate binding; Enter
  also confirms. Closing follows the menu Cancel binding (Escape also works).
  Cancel closes the active popup first, then the main UI.
- NPC rules save only through an explicit immediate/next-launch distribution
  action. Closing cancels unsaved edits, unlike v1.1.4.
- Manual overlays support multiple items per area. Automatic rules pick one
  candidate per configured area. Applied X/Y counts BCNG confirmations against
  RaceMenu capacity minus foreign reserved slots.
- Futanari feature visibility depends on installed/loaded supported female
  addons; manual/NPC eligibility additionally requires SOS/TNG registration.
- Tint masks remain player-only. No tint NPC distribution is provided.
- Manual selections, including Default, override automatic assignments.
  Selected/all actor reset covers all BCNG appearance categories and persists
  Default intent; it does not delete assets, other mods' data, or rule files.
- Keep each .ess save together with its matching .skse co-save.
- With Skyrim Fitting System's supported Rendered Outfit API v1, breast/nipple
  correction and ORefit rules follow the visible actual/registered outfit.
  Without that optional API, correction keeps using actual worn equipment.

## Architecture summary

Body/hands/feet use private native TXST → ARMA → Skin Armor clones at the
ActorBase. References sharing an ActorBase share that body-skin assignment.
Face skin uses separate NiOverride skin channels, not a Face TXST/HeadPart/NIF
replacement or tattoo overlay. Supported male/female addon skins use verified
native TXST construction paths.

Partial packs apply compatible available channels. A matching conditional DDS
takes priority, followed by the selected pack's ordinary channel, then the
underlying provider if both are absent. UBE and conventional atlas routes are
separate; catalog recognition is not CBBE/UNP UV conversion.

OBody NG is optional. Its JSON reader remains only for explicit Outfit Correction
registration, not NPC-rule import. Conventional-female outfit correction and NPC
shape randomization remain in Outfit · randomization. SmoothCam cooperation,
when installed, uses its public camera API.

## Source and license

Body Change NG is released under GPL-3.0. The Git repository uses pinned
submodules; each GitHub release also provides a complete source archive with
the vendored dependency sources and applicable licenses needed to reproduce
the release build. Exact versions are listed in `DEPENDENCIES.md`.
Build with the pinned xmake 3.1.0 (`xmake f -m release`, then `xmake build
BodyChangeNG`); output is `build/v1.2.0/windows/x64/release/BodyChangeNG.dll`.
The checked-in `scripts/Package-Release.ps1` creates versioned binary/source
archives from a clean Git revision and verifies the archive contents. Referenced
mods and compatible JSON files retain their respective authors' copyright and
licenses and are not bundled with Body Change NG.

## Credits

- OBody NG — established ORefit data and behavior used by the optional Outfit
  Correction compatibility path.
- [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  by SlickSilk — optional JSON-format compatibility and validation target.
- Skyrim Fitting System — public GPL-3.0 reference for the optional menu
  character presentation and its safe pause/rotation lifecycle.
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json — see their bundled
  upstream licenses in the source dependencies.
