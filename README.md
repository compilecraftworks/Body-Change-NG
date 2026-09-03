# Body Change NG

Body Change NG is a GPL-3.0 native SKSE manager for BodySlide morphs, actor
skin textures, and the player's RaceMenu tint layers. It combines direct
in-game selection with rule-based NPC distribution in one scalable ImGui UI.
The window opens with F7 by default; modifier chords are supported and the
shortcut is configurable in-game.

Version 1.0.0 supports the verified Skyrim SE 1.5.97 and listed Skyrim AE
1.6.x runtimes through 1.6.1179. The unified binary contains VR-compatible
CommonLib code, but its native renderer/input hooks deliberately fail closed on
VR until VR-specific layouts are verified.

## Requirements

- SKSE64 matching the installed Skyrim runtime
- Address Library for SKSE Plugins matching the installed runtime
- RaceMenu with BodyMorph and NiOverride support
- BodySlide presets built with **Build Morphs** for body changes
- Compatible body/outfit TRI data for visible body and outfit correction

SmoothCam and OBody NG are optional. SmoothCam camera control is requested
through its public API when present. OBody NG is used only when the user
explicitly imports distribution or outfit-correction rules; its JSON is never
silently loaded at startup.

## Compared with OBody NG

OBody NG and Body Change NG both ultimately write BodySlide slider values
through RaceMenu's BodyMorph interface and display them through built TRI data.
Body Change NG does not replace the actor's body mesh. Its distinction is the
native control and ownership layer around that shared morph mechanism:

- a standalone F7 native ImGui interface, not an MCM;
- D-pad list/tab navigation with gamepad confirm and cancel actions;
- separate `BodyChangeNG`, `BodyChangeNGPreview`, and
  `BodyChangeNGOutfit` morph keys for committed, live-preview, and outfit
  states;
- latest-selection generation checks that discard superseded preview/apply
  work;
- explicit, ordered Body and Skin pools with independent distribute/exclude
  decisions;
- native in-game condition/pool/priority editing with separate **Distribute to
  loaded NPCs now** and **Distribute on next game launch** actions;
- persistent Body, Skin, and Tint favorites with per-tab favorites-only views;
- save-specific actor selections and apply signatures that avoid repeating a
  complete unchanged distribution pass; and
- integrated actor skins, player tint detail, and RaceMenu rebuild recovery.

This is a different workflow rather than a claim that RaceMenu receives a new
kind of morph. OBody NG remains optional and is not required for Body Change
NG's body application.

## Compared with the legacy BodyChange

The installed **BodyChange - A Multi-Bodyshape System** architecture defines
fixed `CustomSet` slots. BodySlide writes a separate body NIF into each slot,
and its Papyrus effect calls `SetSkin`, `ChangeHeadPart`, and
`SetFaceTextureSet` to switch the corresponding ESP records together. A skin
choice is therefore coupled to a prepared mesh and record set.

Body Change NG does not use that ESP/Papyrus replacement path. It keeps the
actor's current Skin Armor, HeadPart, and NIFs; body shape is applied through
RaceMenu BodyMorph, while Skin uses ownership-aware RaceMenu/NiOverride texture
overrides against the actor's exact loaded body, hand, foot, and face parts.
Body and Skin can consequently be mixed independently, for the player or NPCs,
without building one mesh copy per skin slot.

There are no fixed `CustomSet1`-`CustomSet20` catalog slots. BodySlide preset
XML files and top-level Skin/Tint pack folders are scanned into the UI. They can
be added while Skyrim is running and discovered with the relevant **Refresh**
button. Player Tint also supports immediate per-layer color and opacity edits,
plus restoration of the pre-change RaceMenu value.

NPC Body/Skin conditions, pool contents, and priority are edited in the same
in-game GUI. The result can be distributed to currently loaded NPCs immediately
or saved for the next launch without changing the active session. Body, Skin,
and Tint catalogs each support persistent stars and a favorites-only view.

## Installation

Install the release archive with MO2 or another mod manager. Keep the included
folder structure intact, enable the mod, and launch through SKSE. Put personal
assets in the included folders and press the corresponding in-game **Refresh**
button after adding files:

- `CalienteTools\BodySlide\SliderPresets` — BodySlide preset XML files
- `BodySkin\<pack name>\Textures\actors\character\...` — conventional skin packs
- Conventional female packs may keep optional `femaleold` body/hand/face
  channels and race-specific `*female\femalehead*_msn.dds` folders under the
  same original `Textures\actors\character` tree. They stay in one pack row
  and are selected for the matching actor.
- CBBE 3BA female packs may also provide `femalebody_etc_v2_1.dds` plus `_msn`,
  `_sk`, and `_s`; these channels target only `3BA_Vagina`/`3BBB_Vagina`
- `BodySkin\<pack name>\Textures\actors\character\argonianfemale|argonianmale|khajiitfemale|khajiitmale` — beast-race skin packs
- `BodySkin\<pack name>\Textures\!UBE\Body` and `...\Head` — UBE 2.0 skin packs
- `TintMask\<pack name>\textures\...\tintmasks` — player tint DDS packs

## Runtime UI

- The first opening is centered; later openings restore the last saved window
  position, including after a game restart.
- The runtime detects 1080p, 1440p, and 4K. The single **Text size** setting
  scales text, controls, spacing, lists, popups, and the default window size
  together.
- Korean, English, and Simplified Chinese are built and rendered as UTF-8.
  Automatic language selection follows the Windows display language and does
  not require Windows' optional “Beta: Use Unicode UTF-8” system-locale mode.
- In SE/AE third-person view, **Character position while open** can frame the
  selected actor on the left or right without moving their world position.
  The normal framing uses FOV 70, distance 200, symmetric horizontal offsets
  ±70, vertical offset -45, and pitch 0.1. Right-drag the outer character area
  to rotate. While paused, the menu orbits only its camera so FSMP bones are not
  separated from the actor root; while unpaused, actor and camera rotate
  together. Camera target, facing, offsets, FOV, projection frustum, and both
  current and target zoom are restored on target change or close.
- The actor selector searches every currently loaded, rendered NPC by name or
  hexadecimal FormID and labels entries as `Name, Sex (FormID)`.
- Body, skin, and tint-pack rows update the live actor on one click. A
  double-click confirms that row and keeps the picker open; closing the picker
  confirms its last live row. Every main window and popup supports Escape, and
  the configured F7 chord always toggles the main window.
- Keyboard Up/Down or W/S and gamepad D-pad Up/Down move the live catalog row.
  Keyboard Left/Right or A/D and gamepad D-pad Left/Right switch tabs. Enter,
  Space, or the gamepad primary face button confirms a row; Escape or the
  gamepad cancel face button closes and confirms the picker. Catalog navigation
  is disabled while a text field owns focus.
- While an ImGui text field is active, Skyrim's standard text-input state is
  enabled so compatible hotkey mods suppress their shortcuts. The state is
  unconditionally released when the field or menu closes.
- While the window remains open, the actual Skyrim `CursorMenu` state is
  checked every frame. If another mod closes its own UI and hides the shared
  cursor, Body Change NG restores it without claiming or forcibly hiding a
  cursor that belongs to another open menu.

## Runtime asset folders

- BodySlide presets: `CalienteTools\BodySlide\SliderPresets\*.xml`
- Conventional skin packs: `BodySkin\<pack name>\Textures\actors\character\...`
- Optional conventional female variants: original `femaleold`, `nordfemale`,
  `bretonfemale`, `darkelffemale`, `highelffemale`, `imperialfemale`,
  `femaleorc`, `redguardfemale`, and `woodelffemale` directories in that same
  tree. Missing variant channels fall back to the pack's base female channel,
  then to the actor's original texture.
- Argonian/Khajiit skin packs: the same `BodySkin\<pack name>` root with the
  original `argonianfemale`, `argonianmale`, `khajiitfemale`, or `khajiitmale`
  folders below `Textures\actors\character`
- UBE 2.0 skin packs: `BodySkin\<pack name>\Textures\!UBE\Body` and
  `BodySkin\<pack name>\Textures\!UBE\Head`
- Player tint packs: `TintMask\<pack name>\textures\...\tintmasks\*.dds`
- Distribution rules: `SKSE\Plugins\BodyChangeNGdistribution.json`

The release includes a `README.txt` in each asset folder and a valid schema-3
distribution file containing the eight initial body/skin exclusion rows. The
in-game distribution editor updates this JSON; OBody's JSON remains a separate,
explicit import source.

The **Refresh** button in each tab rescans that tab's catalog, including files
added while the game is running. Skin and tint pack names may contain Unicode
characters. CBBE and CBBE 3BA presets are presented as one `CBBE 3BA`
family. Skin catalogs additionally match the selected actor's humanoid,
Argonian, or Khajiit race and female/male sex before preview, reapply, or NPC
distribution. Mixed CBBE 3BA/UBE installations are supported: the selected actor's
live skin/head texture namespace resolves its family, and BodySlide
`Preset/@set` plus XML `Group` metadata classifies the preset. UBE and CBBE
3BA continue to share the standard SliderPresets XML folder.

### Legacy BodyChange RaceMenu presets

When the old `BodyChange.esp` is no longer loaded, Body Change NG inspects the
currently visible `Data\SKSE\Plugins\CharGen\Presets\*.jslot` files once at
startup. A stale `BodyChange.esp` face HeadPart is replaced with
`00KLH_FemaleHeadNord` when `High Poly Head.esm` is available, or Skyrim's
vanilla `FemaleHeadNord` otherwise. The original preset is saved beside it as
`.body-change-ng.bak` (or the next numbered backup) before an atomic rewrite.
Malformed and unrelated presets are never rewritten.

This repair concerns RaceMenu `.jslot` HeadParts, not BodySlide XML body
presets. UBE is a separate custom-race/head system, so any `.jslot` carrying an
UBE plugin dependency is preserved instead of being redirected to a vanilla or
High Poly Head face.

Each top-level `BodySkin` or `TintMask` folder appears as one list row. Skin
rows show sex and mapped texture count; tint rows show sex and DDS count. The
first tint entry restores the original layers captured immediately before Body
Change NG first changes each layer in that save. The tint-detail footer lists
only active layers for which the selected pack has a usable DDS. Its DDS is
chosen automatically for the player's current race; race-specific files for a
different race are not offered as fallbacks. Each available layer can be color
and opacity adjusted or restored independently to its captured RaceMenu value.

The direct body and skin lists work for both the player and a selected NPC.
NPC skin packs can also be selected independently inside each NPC distribution
rule. Body and skin pools are evaluated top-down; the first matching rule owns
both pools, and an empty pool leaves that category unchanged. Body and skin
state is isolated per actor. Distribution targets can use an NPC base FormID,
name, faction EditorID, plugin file, or race EditorID. The body- and skin-pool
editors have independent name filters and vertical scrolling. Tint remains
player-only, so only the Tint tab is hidden when an NPC is selected.

Skin application uses the selected actor's live FaceGen subtree and RaceMenu's
persistent texture overrides. Standard body, hands, feet, face, vampire-face,
normal (`_msn`), subsurface (`_sk`), specular (`_s`), and compatible FaceGen
detail DDS channels are handled without changing NIFs or Skin Armor records.
Partial packs change only the supplied part/channel pairs; missing values retain
the actor's underlying textures, and body, hands, feet, and face files are never
substituted for one another.
UBE 2.0 profiles are detected from `Textures\!UBE\Body` and `Head`, map their
diffuse/normal/skin atlases to the actor's UBE slot-53 body and live face, and
do not invent shader slots for optional PBR/RFAOS/wet maps owned by the active
material setup. The Skin and player Tint catalogs hide known-incompatible
families while preserving the existing show-all fallback when actor evidence
is uncertain. NPC distribution rules may keep mixed skin pools; runtime
selection samples only candidates compatible with each matched actor.

When the player leaves RaceMenu, Body Change NG waits for RaceMenu's final
geometry and tint-array rebuild, then restores the currently confirmed body,
skin, and tint selections. Tint reconstruction applies the selected pack first,
then its per-layer detail edits and original-value restores. A default selection
leaves that category under RaceMenu's ownership instead of reintroducing an old
override.

Actor results are stored in the SKSE co-save with resolvable actor references,
so unchanged NPCs are not fully redistributed every time a save loads. Rules
remain global in `BodyChangeNGdistribution.json`; evaluated actor results are
save-specific. New or changed actors are coalesced through a handle-based work
queue, and detached actors do not leave stale preview or apply generations.

The outfit popup can explicitly register OBody NG's complete ORefit rule set
from `Data\SKSE\Plugins\OBody_presetDistributionConfig.json`. Outfit-name,
plugin and FormID exclusions, name and FormID force-refit entries, and the
female/male outfit-to-refit-preset mappings are imported without modifying the
OBody source file. An outfit-specific mapping is evaluated before the current
body's `-Refit` preset, the sex-wide fallback, and the procedural fallback.
The
[OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
by SlickSilk is explicitly supported as an optional import source. Its JSON and
assets are not redistributed by Body Change NG.

## Source and license

Body Change NG is released under GPL-3.0. The Git repository uses pinned
submodules; each GitHub release also provides a complete source archive with
the vendored dependency sources and applicable licenses needed to reproduce
the release build. Exact versions are listed in `DEPENDENCIES.md`. Referenced
mods and compatible JSON files retain their respective authors' copyright and
licenses and are not bundled with Body Change NG.

## Credits

- OBody NG — established BodySlide distribution and ORefit JSON compatibility
  behavior.
- [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
  by SlickSilk — optional JSON-format compatibility and validation target.
- Skyrim Fitting System — public GPL-3.0 reference for the optional menu
  character presentation and its safe pause/rotation lifecycle.
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json — see their bundled
  upstream licenses in the source dependencies.
