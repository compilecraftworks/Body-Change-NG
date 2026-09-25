# Body Change NG — v1.3.4

  Automatic checkbox-mode targeting skips custom followers and elder NPCs;
  manual actor selection and distribution rules remain unchanged.
Native SKSE appearance control for Skyrim SE/AE: BodySlide presets, body and
face skins, player tint masks, supported futanari skins, and Face/Body/Hands/Feet
overlays. Direct player/NPC editing and opt-in NPC rules share one F7 interface.
No BCNG ESP/ESL or MCM is required. Third-party meshes and texture packs are not bundled.

## Current version — v1.3.4

NPC distribution mode now replaces the actor dropdown/refresh controls with
world-NPC scope guidance in every supported tab. UBE body-preset candidates
are restored in both the selection list and automatic rule evaluation. The
ordinary list and previews remain actor-compatible; automatic presets must
also match the receiving NPC's known body family. This does not convert
NPC bodies or change the existing skin/futanari distribution restrictions.

Fix overlay slots remaining unavailable after SlaveTats / SlaveTats NG has
released them but left its blank DDS on the live node. Reuse only the exact
known blank resource with no registered texture, tint or alpha overrides.
Keep actual tattoos, transparent reserved paints, both camera views and other
mods' similarly named textures protected. No blanket overlay reset, polling,
persistent cache or RaceMenu ABI changes are introduced.

- [English changelog](CHANGELOG.md#134) · [한국어 변경 내역](CHANGELOG-KO.md#134)
- [v1.3.4 Nexus BBCode](docs/NEXUS-CHANGELOG-v1.3.4-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.3.4-KO.html)
- [v1.3.4 English text](docs/NEXUS-CHANGELOG-v1.3.4-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.3.4-KO.txt)

## Preserved v1.3.3 distribution authoring

Distribution schema 8 uses readable names: BodySlide/BCNG preset names,
skin/futa pack names, registered overlay names and named targets such as
`NordRace` / `BanditFaction`. All eleven existing target scopes are supported.
Use the [annotated JSON](package/SKSE/Plugins/BodyChangeNGdistribution.json)
and [English/Korean guide](package/SKSE/Plugins/BodyChangeNGdistribution-README.txt):
19 inactive examples cover all features, multiple candidates, overlay colors,
duplicate names and 39 copyable vanilla condition names. Ordinary name editing
does not need an ID helper or an in-game rule-creation session.

**Use the updated schema-8 DLL, not only a new JSON with an older schema-7 DLL.**
Schemas 3–7 migrate with a byte-exact, non-overwriting backup beside the source.
Unresolved old IDs and new names are retained. A name must resolve to one
compatible item; qualifiers disambiguate duplicates. Some custom records expose
no unique runtime name/EditorID and need the documented plugin/local-ID fallback.
Existing automatic-distribution compatibility limits still apply.

`//` and `/* ... */` comments are supported. In-game explicit saves put active
rules first and preserve comments/examples below without enabling or duplicating
them. Other JSON inputs remain unchanged. The shipped rules array is empty;
preserve your customized winning MO2 file. No new per-frame polling or co-save
format change was introduced.

- [English changelog](CHANGELOG.md#133) · [한국어 변경 내역](CHANGELOG-KO.md#133)
- [v1.3.3 Nexus BBCode](docs/NEXUS-CHANGELOG-v1.3.3-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.3.3-KO.html)
- [v1.3.3 English text](docs/NEXUS-CHANGELOG-v1.3.3-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.3.3-KO.txt)

## Preserved v1.3.2 skin selection

Version 1.3.2 restores direct skin selection for standalone actors whose body
family cannot be identified. Select the actor, open Body Skin, click to preview
and double-click to apply. **NPC distribution rules are not required.** An
unknown-family hint asks you to choose the correct 3BA/UBE skin for that actor;
known incompatible layouts, sex and race restrictions remain enforced. Automatic
distribution keeps its existing compatibility checks.

NPC faces are resolved within the actual FaceGen subtree, allowing an unambiguous
face whose exported mesh name differs from the HeadPart editor ID. Body-only skin
packs no longer wait for a face target unless an earlier face change needs cleanup.
Manual-choice restoration, hand/foot routing, existing cache maintenance and
co-save formats are preserved. No new polling or dependencies are added.

The Release build, 35 offline regression executables and 19 audit-tool tests passed.
No new in-game testing was performed; the reporters' specific setups have not been
reproduced. See the [verification and limits](docs/RELEASE-VERIFICATION-v1.3.2-KO.md).

- [English changelog](CHANGELOG.md#132) · [한국어 변경 내역](CHANGELOG-KO.md#132)
- [Full v1.3.2 English Nexus description](docs/NEXUS-DESCRIPTION-v1.3.2-EN.bbcode) · [한국어 HTML 소개글](docs/NEXUS-DESCRIPTION-v1.3.2-KO.html) · [English Markdown](docs/NEXUS-DESCRIPTION-v1.3.2.md)
- [English Nexus BBCode](docs/NEXUS-CHANGELOG-v1.3.2-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.3.2-KO.html)
- [English plain text](docs/NEXUS-CHANGELOG-v1.3.2-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.3.2-KO.txt)

## Preserved v1.3.1 cache maintenance

Version 1.3.1 adds optional **offline cache maintenance**. Close Skyrim, run
`Tools\BodyChangeNGCache.exe` directly (not through MO2), and select MO2 Overwrite
or its `textures\BodyChangeNG\Cache` folder. A read-only scan runs first; choose
Yes to compact eligible duplicates and remove recognized unpublished staging files.
The default confirmation is No. See the [bilingual utility guide](package/Tools/BodyChangeNGCache-README.txt).

DDS paths and bytes are preserved, including older saves' paths. Only identical
copies with matching timestamps and cache-local ownership are merged into hard
links. Unique textures, external hard links, read-only/locked files and reparse
points are retained. This is not an all-save reference scanner or a bulk cache
deletion feature. Savings vary; folder-size totals may look unchanged. Do not edit
compacted cache DDS files directly—edit the source pack and Refresh in BCNG.

Runtime texture preparation now stages a complete DDS before atomic publication;
failures keep the previous good alias and clean up private temporary files.
The utility runs only on demand outside the game; no frame-time cache scan is added.
The Release build, 35 offline regression executables and 19 audit-tool tests passed.
See the [verification and limits](docs/RELEASE-VERIFICATION-v1.3.1-KO.md).

- [English changelog](CHANGELOG.md#131) · [한국어 변경 내역](CHANGELOG-KO.md#131)
- [English Nexus BBCode](docs/NEXUS-CHANGELOG-v1.3.1-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.3.1-KO.html)
- [English plain text](docs/NEXUS-CHANGELOG-v1.3.1-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.3.1-KO.txt)

## Preserved v1.3.0 restoration fixes

Version 1.3.0 fixes cross-feature restoration issues: multiple saved overlays
now reserve distinct slots before deferred writes, automatic Default-body checks
preserve valid outfit corrections, and player skin/body/overlay preview undo waits
until RaceMenu closes. Default-body player restoration also reevaluates clothing
after load or RaceMenu. Unload-safe undo, newer-selection guards, explicit reset/removal behavior,
distribution rules and saved formats are preserved.

The audit includes existing body presets, skins, addons, tint, overlays, distribution,
input, settings, persistence and SFS outfit correction—not only the changed paths.
The Release build, 34 regression executables and 19 audit-tool tests passed.
The added lifecycle tests include 1,000,000 mixed queue operations and an
AddressSanitizer run across the extracted product functions.
The offline allocation probe includes the new restoration queue and reports no
retained allocation growth; parked undo does not allocate again on each frame.
See the [verification and limits](docs/RELEASE-VERIFICATION-v1.3.0-KO.md).
No new in-game testing was performed. Whole-game zero leaks and resolution of every
reported TNG/SOS rendering issue are not claimed.

- [English changelog](CHANGELOG.md#130) · [한국어 변경 내역](CHANGELOG-KO.md#130)
- [English Nexus BBCode](docs/NEXUS-CHANGELOG-v1.3.0-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.3.0-KO.html)
- [English plain text](docs/NEXUS-CHANGELOG-v1.3.0-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.3.0-KO.txt)

### Preparing to uninstall

Back up your save. Open **Mod settings → Prepare for mod removal → Start cleanup**.
Wait for **Cleanup verified**. Then close BCNG, save to a **new slot**, fully exit
Skyrim, and uninstall BCNG. If cleanup is incomplete, do not uninstall; let updates
finish/load remaining actors and retry. This is ownership-scoped cleanup, not a
general damaged-save repair tool.

Removal mode persists in `Data\SKSE\Plugins\BodyChangeNG\settings.json` and suspends
automatic distribution/corrections. Rules and other mods' morph keys are preserved.
**Resume BCNG** exits this mode but does not restore selections erased by cleanup.
Preserve your customized rule JSON and asset packs when updating the mod.

## Previous version — v1.2.7

Version 1.2.7 resets distribution/UI drafts consistently across close, load, and
new-game boundaries. Distribution target metadata is collected on a game task
and handed to the UI as owned values, with stale results discarded. Body/skin
work rechecks cancellation before mutation. This is lifecycle hardening, not an
in-game confirmation of the reported distribution-popup CTD.

Hostile NPCs can appear in the nearby actor dropdown. Explicit name/hex RefID
search also finds existing references outside its radius/32-NPC limit; compatible
manual choices can be saved for unloaded actors and applied when they load.
Searching does not spawn actors or force cells to load.

See [English changelog](CHANGELOG.md#127), [한국어 변경 내역](CHANGELOG-KO.md#127),
and the [investigation and validation scope](docs/DISTRIBUTION-POPUP-CTD-AUDIT-20260918-KO.md).

- [v1.2.7 English Nexus BBCode](docs/NEXUS-CHANGELOG-v1.2.7-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.2.7-KO.html)
- [English plain text](docs/NEXUS-CHANGELOG-v1.2.7-EN.txt) · [한국어 텍스트](docs/NEXUS-CHANGELOG-v1.2.7-KO.txt)

## Previous version — v1.2.6

Version 1.2.6 corrects the 1.2.5 mouse delivery regression: button and wheel
edges come from Skyrim's input hook, independently of Windows mouse messages,
while coordinates still use Skyrim's cursor. Repeated 3D notifications no
longer reload an already-correct face DDS through the immediate API when the
live texture, renderer, and required saved override all match.
The 1.2.5 preset-list optimizations and saved-morph recovery are retained.
The Release build and 26 automated regression executables passed. Reporter
logs and in-game reproduction are unavailable; the reported skin flashing is
not claimed fully resolved. See the [investigation](docs/INPUT-SKIN-AUDIT-v1.2.6-KO.md).
CommonLibSSE-NG remains pinned to v6.7.1. Skyrim 1.7.x is not supported.

- [v1.2.6 English changelog](docs/NEXUS-CHANGELOG-v1.2.6-EN.txt) · [한국어 변경 내역](docs/NEXUS-CHANGELOG-v1.2.6-KO.txt)
- [English Nexus BBCode](docs/NEXUS-CHANGELOG-v1.2.6-EN.bbcode) · [한국어 HTML](docs/NEXUS-CHANGELOG-v1.2.6-KO.html)

## Previous version — v1.2.4

Version 1.2.4 fixes valid empty UBE Zeroed presets, accounts for nonzero standard
UBE build defaults, removes the face refresh's sole dependence on a NiNode
completion event, and builds missing native skin targets from actual NIF
baselines. Custom-NPC classification now prioritizes live body evidence over
stale metadata. Existing 1.2.3 behavior is retained. See the
[English changes](CHANGELOG.md#124),
[한국어 변경 내역](CHANGELOG-KO.md#124), and
[UBE investigation and verification](docs/UBE-PRESET-SKIN-AUDIT-20260915-KO.md).
The SE/AE Release build and 25 automated regression executables passed. In local
TOFU gameplay, RSV face-texture reapplication was identified and the user
confirmed normal skin switching with RSV disabled. This does not establish
compatibility with RSV/Selector or gameplay verification of every runtime.

- [v1.2.4 English Nexus description](docs/NEXUS-DESCRIPTION-v1.2.4-EN.bbcode) · [한국어 HTML 소개글](docs/NEXUS-DESCRIPTION-v1.2.4-KO.html)
- [v1.2.4 English changelog](docs/NEXUS-CHANGELOG-v1.2.4-EN.txt) · [한국어 변경 내역](docs/NEXUS-CHANGELOG-v1.2.4-KO.txt)

### Incompatible simultaneous appearance controllers

- **OBody NG:** body distribution and breast/nipple outfit correction overlap with BCNG. Clearing OBody/OClothe keys does not stop a running OBody instance from reapplying them.
- **Racial Skin Variance - SPID (RSV):** incompatible with BCNG skin control. Its face-texture reapplication after character 3D rebuilds can overwrite the BCNG face and cause a darker face or visible neck seam.
- **RaceMenu Selector of Skins - Unique Player Character:** incompatible with BCNG skin control. Reapplying its player Skin Armor and face textures can overwrite BCNG selections or disrupt later skin switches.

Do not use RSV or Selector to control skins alongside BCNG. BCNG does not suspend
their reapplication and includes neither built-in conflict prevention nor a
separate compatibility patch for them. A successful preview or confirmation can
be overwritten later; Default Skin is not a compatibility workaround.

Use one controller for the affected features. The outfit-rule JSON from
**[ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)**
can be imported with **Register ORefit outfit-correction rules**. **RaceMenu itself
is required** and is not the separate skin Selector mod.

## Previous version guides — v1.2.3

Version 1.2.3 separates body-skin previews from persistent state and adds recovery
from partial texture-write failures. It retains the 1.2.2 morph-name fixes and
1.2.1 OBody/OClothe cleanup, favorites, and sex-specific NPC distribution.
See the [English changes](CHANGELOG.md#123) or [한국어 변경 내역](CHANGELOG-KO.md#123).
The v1.2.3 guides below document the current body-morph policy:
BCNG preset confirmation and Default body remove `OBody` and `OClothe` even when
**Preserve other mods' morphs** is enabled. Other morph keys still follow that
option. Previews remain reversible. This does not turn off a running OBody mod;
avoid having both systems automatically assign bodies to the same actors.

- [English description and complete usage](docs/NEXUS-DESCRIPTION-v1.2.3.md)
- [English Nexus BBCode](docs/NEXUS-DESCRIPTION-v1.2.3-EN.bbcode) · [한국어 HTML 소개글](docs/NEXUS-DESCRIPTION-v1.2.3-KO.html)
- [Changes from v1.2.2, English](docs/NEXUS-CHANGELOG-v1.2.3-EN.txt) · [한국어 변경 내역](docs/NEXUS-CHANGELOG-v1.2.3-KO.txt)
- [Upload formats](docs/README.md) · [Full history](CHANGELOG.md) · [전체 이력](CHANGELOG-KO.md)
- [Upgrade notes](docs/RELEASE-NOTES-v1.2.3.md) · [한국어 업데이트 안내](docs/RELEASE-NOTES-v1.2.3-KO.md)
- [Architecture](docs/ARCHITECTURE-v1.2.0-KO.md) · [Verification limits](docs/COMPATIBILITY-UI-20260912-KO.md)

## Requirements and compatibility

Matching **SKSE64, Address Library, and RaceMenu** are required.
Body changes also need compatible XML presets and body/outfit meshes built with
**Zeroed Sliders + Build Morphs** TRI output. Other features need their corresponding assets/addons.

Explicit targets:

- SE **1.5.97**
- AE **1.6.317 / 1.6.318 / 1.6.323 / 1.6.342 / 1.6.353 / 1.6.629 / 1.6.640 / 1.6.659 / 1.6.1130 / 1.6.1170 / 1.6.1179**

No VR, LE, Epic 1.6.678, Store/Game Pass, or unlisted runtimes including 1.7.x.
Do not assume the newest dependency download matches an older game.

The current Release build and 34 automated test executables passed. This is not
complete in-game/leak verification; see the verification report above.

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
female/male/UBE/TRX/ERF DDS paths. Tint masks share
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
- NPC distribution starts with Female. Female/Male selects the nearest matching
  loaded NPC, falling back to the player. Catalog/rule sex stays independent
  of the preview actor, and body/skin candidates follow configured NPC body types.
  Switching sex clears previous checks/previews. Distribute opens the rule popup;
  it does not save or apply rules by itself. Futanari remains female-only.
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

The optional OBody ORefit JSON reader is only for explicit Outfit Correction
registration. Conventional-female outfit correction and NPC
shape randomization remain in Outfit · randomization. SmoothCam cooperation,
when installed, uses its public camera API.

## Source and license

Body Change NG is released under GPL-3.0. The Git repository uses pinned
submodules; each GitHub release also provides a complete source archive with
the vendored dependency sources and applicable licenses needed to reproduce
the release build. Exact versions are listed in `DEPENDENCIES.md`.
Build with the pinned xmake 3.1.0 (`xmake f -m release`, then `xmake build
BodyChangeNG`); output is `build/v1.3.0/windows/x64/release/BodyChangeNG.dll`.
The checked-in `scripts/Package-Release.ps1` creates versioned binary/source
archives from a clean Git revision and verifies the archive contents. Referenced
mods and compatible JSON files retain their respective authors' copyright and
licenses and are not bundled with Body Change NG.

## Credits

- OBody NG — established ORefit data and behavior used by the optional Outfit
  Correction compatibility path.
- [ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  by SlickSilk — optional JSON-format compatibility and validation target.
- Skyrim Fitting System — public GPL-3.0 reference for the optional menu
  character presentation and its safe pause/rotation lifecycle.
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json — see their bundled
  upstream licenses in the source dependencies.
