# Body Changer NG

GPL-3.0-licensed native SKSE body, skin, and player tint manager for Skyrim
SE, AE, and VR. Its ImGui window is opened with F7 by default (the shortcut
supports modifier chords and is configurable in-game).

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
  Right-drag the outer character area to rotate them. Camera, facing, and the
  exact pause contribution owned by this menu are restored on target change or
  close. The presentation safely disables itself on VR until a VR-specific
  camera-state layout is verified.
- With **Pause game while open**, right-drag briefly releases only this menu's
  pause count while dragging so SMP can advance; it restores immediately when
  dragging stops, focus is lost, or the menu closes.
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
  cursor, Body Changer NG restores it without claiming or forcibly hiding a
  cursor that belongs to another open menu.

## Runtime asset folders

- BodySlide presets: `CalienteTools\BodySlide\SliderPresets\*.xml`
- Skin packs: `BodySkin\<pack name>\Textures\...`
- Player tint packs: `TintMask\<pack name>\textures\...\tintmasks\*.dds`

The **Refresh** button in each tab rescans that tab's catalog, including files
added while the game is running. Skin and tint pack
names may contain Unicode characters. CBBE and CBBE 3BA presets are presented
as one `CBBE 3BA` family.

Each top-level `BodySkin` or `TintMask` folder appears as one list row. Skin
rows show sex and mapped texture count; tint rows show sex and DDS count. The
first tint entry restores the original layers captured immediately before Body
Changer NG first changes each layer in that save. The tint-detail footer lists
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
detail DDS channels are handled without changing NIFs, Skin Armor records, or
equipment slots.

When the player leaves RaceMenu, Body Changer NG waits for RaceMenu's final
geometry and tint-array rebuild, then restores the currently confirmed body,
skin, and tint selections. Tint reconstruction applies the selected pack first,
then its per-layer detail edits and original-value restores. A default selection
leaves that category under RaceMenu's ownership instead of reintroducing an old
override.

The outfit popup can explicitly register OBody NG's complete ORefit rule set
from `Data\SKSE\Plugins\OBody_presetDistributionConfig.json`. Outfit-name,
plugin and FormID exclusions, name and FormID force-refit entries, and the
female/male outfit-to-refit-preset mappings are imported without modifying the
OBody source file. An outfit-specific mapping is evaluated before the current
body's `-Refit` preset, the sex-wide fallback, and the procedural fallback.

## Credits

- OBody NG — established BodySlide distribution and ORefit JSON compatibility
  behavior.
- Skyrim Fitting System — public GPL-3.0 reference for the optional menu
  character presentation and its safe pause/rotation lifecycle.
- CommonLibSSE-NG, Dear ImGui, pugixml, and nlohmann/json — see their bundled
  upstream licenses in the source dependencies.
