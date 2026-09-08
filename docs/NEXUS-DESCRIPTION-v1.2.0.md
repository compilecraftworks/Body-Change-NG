# **BODY CHANGE NG · v1.2.0**
### Change the player's and NPCs' BodySlide presets and body skins, edit the player's tint masks in real time, and distribute BodySlide presets and body skins to NPCs using editable conditions

**NATIVE IN-GAME GUI · GAMEPAD SUPPORT · RULE-BASED NPC DISTRIBUTION**

**BODY SUPPORT:** CBBE 3BA · BHUNP / UNP · UBE 2.0 · HIMBO · SAM · VANILLA FEMALE / MALE

---

## **OVERVIEW**

Body Change NG is a native SKSE plugin for changing BodySlide morph presets,
actor skin textures, optional genital-addon skins, and the player's RaceMenu
tint layers from one in-game interface. It also includes an ordered rule editor
for distributing body presets and skins to NPCs, save-specific result recovery,
favorites, outfit correction, and explicit OBody NG JSON import.

Body shape remains RaceMenu BodyMorph data. Version 1.2.0 replaces the old
general-purpose skin repaint pipeline with an actor-native TextureSet →
ArmorAddon → Skin Armor graph. The selected skin becomes the NPC base's normal
skin source, so naked body, hands, feet, and face rebuild through Skyrim's own
system without BCNG tracking which gloves, boots, or outfits are equipped.

---

## **WHAT'S NEW IN 1.2.0**

- **Native Skin Armor/TXST backend** — BCNG deep-clones the actor's current
  provider graph, preserves every original texture channel, overlays only the
  DDS parts supplied by the chosen pack, and attaches the private graph at the
  NPC ActorBase.
- **Equipment-independent base skin** — changing clothes is no longer a trigger
  for repainting the normal body skin. When an outfit reveals the actor's real
  body, Skyrim rebuilds it from the already selected Skin Armor.
- **Exact body-part routing** — body, hands, feet, face, 3BA vagina/anus,
  BHUNP vagina/anus/canal, beast tails, and external genital addons have
  separate roles. A missing or ambiguous role is never redirected to another
  body part.
- **Conventional-body versus UBE catalog** — Legacy means a skin pack using
  Skyrim's conventional `actors\character` texture tree, including CBBE/3BA,
  BHUNP/UNP, vanilla female/male, HIMBO, and SAM. Only an explicit `!UBE\Body`
  or `!UBE\Head` tree is UBE. Exact material routing is derived from the
  selected actor at apply time, not from the pack name.
- **Partial packs remain safe** — every supplied part and material channel is
  optional. Missing DDS files retain the current provider value instead of
  borrowing a body, hand, foot, face, or genital texture.
- **UBE 2.0 graph support** — the verified slot-53 body plus separate hand/foot
  ARMAs use the UBE Body atlas, while the Head atlas targets the live face.
  Private TXST synthesis is limited to verified canonical UBE naked models.
- **RSV-aware ownership** — Racial Skin Variance may remain the provider. BCNG
  rebuilds from current RSV forms when they change and restores only pointers
  it still owns; the bounded face bridge does not create or delete RSV's saved
  keys.
- **Independent appearance features** — body morph, native base skin, player
  tint, male SOS/TNG, female futanari, RSV face, and outfit correction use
  separate state and work channels. Fixing or clearing one does not reset the
  others.
- **Reliable NPC distribution** — BodyMorph stays reference-scoped, while the
  native skin selection is ActorBase-scoped. Stable choices are recorded only
  after the relevant operation succeeds, and unchanged Body or Skin categories
  preserve their prior saved result.
- **Safer rule editing and saving** — starter rules can be edited, reordered,
  and deleted; new IDs do not collide; broad earlier rules show a shadowing
  warning; built-in names follow Korean, English, or Simplified Chinese; JSON
  saves use validated temporary files and atomic replacement.
- **Explicit compatibility boundaries** — verified Skyrim SE 1.5.97 and AE
  1.6.x layouts through 1.6.1179 are supported. RaceMenu BodyMorph/Override
  interface generations are dispatched separately and unknown layouts fail
  closed. Skyrim VR is not built or supported.

---

## **WHY BODY CHANGE NG?**

### **Compared with OBody NG**

Both mods ultimately deform an already built body through RaceMenu BodyMorph
and TRI data. The difference is the selection, distribution, and
state-management layer above that shared morph pipeline.

- **One native interface** — press F7 to manage the player and loaded NPCs,
  body presets, skins, player tint, favorites, and distribution rules.
- **Actor-matched catalogs** — preset compatibility is resolved from the
  actor's runtime BodyFamily. Skin layout, race, and sex are checked separately.
- **Explicit Body and Skin pools** — choose exactly which entries each rule may
  distribute; either category can use a pool, Default, exclusion, or Unchanged.
- **Visible top-down priority** — the first matching rule owns both category
  decisions, so broad and specific conditions can be reordered in game.
- **Immediate or next-launch distribution** — apply edited rules to loaded NPCs
  now, or save a next-launch draft without silently changing the current
  session's active rules.
- **Save-specific results** — direct choices and evaluated NPC results live in
  the SKSE co-save and unchanged actors avoid unnecessary full redistribution.
- **Separated live state** — preview, committed BodyMorph, outfit correction,
  native BodySkin, tint, and genital-addon skin use independent ownership.
- **Optional migration path** — an existing OBody NG JSON can be explicitly
  imported for supported NPC distribution and ORefit data.

### **Compared with legacy mesh-slot systems**

Legacy slot systems require a separately built body mesh and prepared ESP
Skin Armor, HeadPart, and TextureSet records for every predefined slot.
Body Change NG does not use fixed CustomSet slots.

- Body shape is selected from installed BodySlide XML and applied as BodyMorph.
- Version 1.2.0 deep-clones the actor's current native TXST → ArmorAddon → Skin
  Armor provider graph and changes only mapped channels on the private clone.
- Body shape and skin remain independently selectable instead of requiring one
  prepared mesh-and-record set.
- The native base skin is attached at the NPC ActorBase, allowing Skyrim to
  rebuild exposed body, hands, feet, and face without BCNG tracking outfits.
- Direct player/NPC selection, conditional distribution, favorites, live
  Refresh, and player tint editing share the same interface.

---

## **FEATURES**

### **Body Presets**

- Select the player or a loaded NPC and preview compatible BodySlide presets.
- Low/high values interpolate by actor weight.
- Preview, commit, and outfit correction use separate owned BodyMorph keys.
- Repeated application targets the same absolute result without clearing morph
  keys owned by other mods.
- The main list is filtered using conservative runtime BodyFamily evidence;
  ambiguous and multi-family presets retain a safe visible fallback.
- NPC rule pools use the female/male body type selected in Mod Settings, and
  runtime distribution rechecks compatibility before choosing a candidate.
- Body preset XML classification and runtime actor BodyFamily checks remain
  independent from skin-pack classification.

### **Body Skins**

- Apply separate skin packs to the player or loaded NPCs.
- Manual Skin and Default Skin choices take ownership immediately, preventing
  actor initialization or automatic distribution from replacing the preview.
- Native body, hand, foot, far-skin, and optional face TextureSets are cloned
  from the actor's current provider; unrelated channels stay intact.
- Conventional CBBE/3BA, BHUNP/UNP, vanilla, HIMBO, and SAM texture trees are
  grouped as Legacy. UBE is detected only from its explicit `!UBE` atlas
  namespace.
- CBBE 3BA and BHUNP/UNP genital/anal atlases remain part of the normal female
  skin pack and route only to their exact matching geometry.
- HIMBO or SAM skin and SOS/TNG slot-52 genital textures may live in one pack.
  Vanilla male skin is also supported; the live addon path chooses Regular,
  Muscular, Smurf, race, and elder variants.
- Argonian and Khajiit packs are filtered by race and sex, and their matching
  body atlas also reaches the native tail role.
- Diffuse, normal, subsurface, specular, compatible detail, elder, vampire,
  and race-specific variants are applied only when matching files exist.
- Completed results are verified after the native rebuild boundary. Missing
  cache files are reconstructed from the original pack without rescanning every
  NPC event.
- A Default Skin action restores the captured provider graph without touching
  BodyMorph, tint, futanari, or foreign override ownership.

### **Futanari Skin**

- The tab appears only for a detected supported female addon: UBE SOS/TNG,
  UBE/CBBE TRX, or CBBE 3BA ERF.
- The genital skin choice is separate from the full BodySkin choice.
- Removing the addon keeps the selection; re-equipping the supported type
  reapplies its matching saved skin.
- Missing channels remain controlled by the addon's original material.

### **Player Tint Masks**

- Install several RaceMenu tint-mask packs and choose per tint layer.
- Edit color and opacity immediately.
- Restore the exact value captured before BCNG first changed that layer.
- Reapply confirmed layers after RaceMenu rebuilds the player.
- Tint editing is player-only. Overlay-based makeup is not a tint mask and is
  not supported by this tab.

### **NPC Distribution**

- Rules are evaluated from top to bottom; the first match owns both category
  decisions.
- Body and Skin can each distribute a pool, explicitly use Default, be excluded,
  or remain unchanged.
- Conditions include sex, custom followers, elders, plugin, race, faction,
  keyword, class, name, and exact NPC base FormID.
- Faction, race, keyword, and class targets retain plugin plus local FormID, so
  unnamed records remain selectable and load-order changes can be resolved.
- One pool entry is fixed; several entries produce a stable per-NPC result.
- Loaded corpses receive the same rules; the player, disabled, unloaded, and
  non-NPC references remain excluded.
- Eight editable starter exclusions cover custom followers, elders, Argonians,
  and Khajiit without locking sample rows against editing or deletion.
- Select all and Clear all populate compatible Body or Skin pools quickly.
- Apply the edited rules to loaded NPCs now, or save the draft for the next
  launch without silently replacing the current session's active rules.
- All sample and user rules use the same edit, reorder, and delete behavior.

**Rules file:** `Data\SKSE\Plugins\BodyChangeNGdistribution.json`

The in-game editor writes this file. Back it up when replacing the mod or MO2
profile, restore it to the same path, and press **Load saved values**.

### **Outfit Correction and Randomization**

- Optional clothed breast correction for supported CBBE 3BA and BHUNP/UNP
  actors, with a separate nipple-correction toggle.
- Optional stable NPC nipple and genital-shape randomization for supported
  CBBE 3BA and BHUNP/UNP NPCs.
- UBE actors are skipped because their slider layout is materially different;
  mixed UBE-player and conventional-NPC installations are evaluated per actor.
- ORefit rules may select an outfit-specific preset first, then the current
  body's exact `-Refit` preset, a `Female-Refit`/`Male-Refit` fallback, and
  finally procedural correction.

### **UI and Input**

- Korean, English, and Simplified Chinese interface text.
- Configurable F7 shortcut with Ctrl, Shift, and Ctrl+Shift combinations.
- Mouse, keyboard/WASD, and gamepad D-pad navigation with confirm/cancel.
- Native IME input, Unicode searches and names, favorites, and per-tab
  favorites-only filters.

---

## **REQUIREMENTS**

- Skyrim SE 1.5.97 or a supported Skyrim AE 1.6.x runtime through 1.6.1179
- Matching SKSE64
- Address Library for SKSE Plugins matching the game runtime
- RaceMenu matching the game runtime, with BodyMorph and NiOverride/Override
- BodySlide presets and compatible TRI files built with **Build Morphs**

Skyrim VR is not supported.

---

## **INSTALLATION AND UPDATE**

1. Install the Release ZIP with MO2 and keep its folder structure intact.
2. When updating, preserve your own asset folders and
   `SKSE\Plugins\BodyChangeNGdistribution.json` if they are stored inside the
   main mod.
3. Enable the mod, launch through SKSE, and press F7.

Version 1.2.0 is a separate architectural line from 1.1.4. Do not combine DLLs
from the two versions. Existing settings, distribution rules, favorites, and
co-save identifiers are migrated in place; the old DLL backup is still the
safest rollback point for a save that has not yet been continued extensively
under 1.2.0.

---

## **HOW TO USE**

1. Press **F7**, choose the player, or press **Refresh actors** and select a
   currently loaded NPC.
2. Open **Body Presets** or **Body Skins**. A single click previews; double-click
   confirms while keeping the picker open; closing the picker confirms the last
   preview. The Default row clears BCNG's selection for that category.
3. Open **Tint Masks** for the player's existing RaceMenu tint layers. Select a
   pack and layer, adjust color/opacity, or restore the value captured before
   BCNG first changed it.
4. If supported female genital geometry is detected, choose a matching
   SOS/TNG, TRX, or ERF skin from the independent **Futanari** tab.
5. Choose female and male NPC body types in **Mod Settings**, open
   **NPC Distribution**, edit conditions and pools, then apply to loaded NPCs
   now or save the draft for the next launch.
6. Open **Outfit · randomization** to configure supported clothed correction,
   NPC shape randomization, or explicitly register OBody NG ORefit rules.
7. Press **Refresh** on a catalog tab after adding or replacing assets while the
   game is running.

Direct actor choices and evaluated results belong to the current SKSE co-save.
Distribution rules remain in the separate JSON shown above.

---

## **ADDING ASSETS**

All paths below are relative to an MO2 mod root. Assets may be stored inside
Body Change NG or in separate enabled MO2 mods that expose the same paths.

### **BodySlide presets**

`CalienteTools\BodySlide\SliderPresets\*.xml`

Use standard BodySlide preset XML and build the matching body/outfit TRI data
with Build Morphs.

### **CBBE 3BA and BHUNP/UNP female BodySkin packs**

```text
BodySkin\My Skin\Textures\actors\character\female\femalebody_1.dds
BodySkin\My Skin\Textures\actors\character\female\femalehands_1.dds
BodySkin\My Skin\Textures\actors\character\female\femalehead.dds
```

Copy the source mod's original texture tree. The top-level pack folder is the
display name and stable-ID source.

For CBBE 3BA, `femalebody_etc_v2_1` plus `_msn`, `_sk`, and `_s` is the
vagina/anus atlas. For BHUNP/UNP, preserve
`BakaUNP\VaginalAnalCanal2` plus the same channel suffixes for the
vagina/anus/canal atlas. These assets do not classify the pack and are never
used as replacements for regular body, hands, feet, or face.

### **UBE 2.0 BodySkin packs**

```text
BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_d.dds
BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_n.dds
BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_sk.dds
BodySkin\My UBE Skin\Textures\!UBE\Head\femalehead_d.dds
```

Keep Legacy and UBE assets in separate top-level pack folders.

### **HIMBO and SAM BodySkin plus SOS/TNG**

Keep the HIMBO or SAM body/hand/foot/face tree and its SOS/TNG addon folders
under the same top-level skin pack. Vanilla male skins use the same conventional
texture layout. BCNG never joins textures across packs.

`BodySkin\My Male Skin\Textures\actors\character\SOS\<addon name>\malegenitals_1*`

### **Futanari packs**

- UBE SOS/TNG: `Futanari\<pack>\Textures\!UBE\Body\malebody_1_[d/n/sk].dds`
- TRX: `Futanari\<pack>\Textures\[TRX] Futa addon\Regular\Default\schlong*`
- ERF: `Futanari\<pack>\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong*`

### **Tint packs**

`TintMask\<pack>\textures\actors\character\character assets\tintmasks\*.dds`

Files added while Skyrim is running appear after pressing **Refresh** on the
matching tab.

---

## **OPTIONAL OBODY NG JSON IMPORT**

Body Change NG works without OBody NG or its JSON. This optional path reuses
supported rules from:

`Data\SKSE\Plugins\OBody_presetDistributionConfig.json`

Keep it beside `BodyChangeNGdistribution.json`; do not rename or merge the two
files. BCNG reads the OBody file only through an explicit UI action and never
modifies it.

MO2 does not merge several mods that provide this same OBody filename. BCNG
reads the one file that wins at the virtual `Data` path. If a distribution
config and an ORefit master list must be used together, install an OBody-format
file in which those OBody rules have already been combined; never merge it with
BCNG's separate `BodyChangeNGdistribution.json`.

- In **NPC Distribution**, press **Load saved values** to load BCNG's saved
  rules and import supported OBody NPC distribution data when the file exists.
- Distribution import supports preset blacklists, NPC name/FormID exclusions
  and assignments, plugin/race exclusions, faction/plugin/race assignments,
  and female/male default pools. Preset names must match installed catalog
  entries; missing names are skipped and logged.
- A repeated import replaces only earlier OBody-imported rows and preserves
  BCNG sample and user rules. Imported rows remain visible and editable before
  saving or distributing.
- Imported rows use the same top-to-bottom priority as every BCNG rule. An
  earlier same-sex **All NPCs** rule can shadow a more specific imported row;
  the editor warns about this so the specific row can be moved upward or the
  earlier rule narrowed.
- In **Outfit · randomization**, press **Register OBody NG outfit-correction
  rules** to load outfit name/plugin/FormID exclusions, name/FormID force-refit
  entries, and female/male outfit-to-preset mappings.
- Registration immediately re-evaluates every loaded actor. A blacklisted
  torso item is treated as absent for correction, while a force-refit item in
  any worn slot retains OBody NG's override behavior.
- Distribution import and ORefit registration are independent; use either or
  both.

The [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052)
by SlickSilk is supported as an optional import source. Install its JSON and
referenced preset assets separately; Body Change NG does not redistribute them.

---

## **COMPATIBILITY NOTES**

- **Racial Skin Variance:** BCNG treats the current RSV graph as a provider and
  maintains a bounded face bridge. UBE 2.0's own requirement to exclude its
  player race from RSV (`PLAYER VANILLA`) still applies.
- **SOS/TNG/TRX/ERF:** external genital geometry remains reference-scoped and
  equipment-aware, separate from ActorBase-scoped body skin.
- **Mu Dynamic NormalMap:** supported companion normal bundles are preserved
  with the selected cached normal.
- **OverlayFix:** actor-update and asynchronous-work boundaries are isolated to
  improve coexistence.

---

## **SAVE AND OWNERSHIP SCOPE**

- BodyMorph and external genital addons are actor-reference scoped.
- Native BodySkin is NPC ActorBase scoped. References sharing the same base
  therefore share one native skin selection.
- Direct choices and evaluated NPC results belong to the current SKSE co-save.
- Distribution rules are stored separately in
  `Data\SKSE\Plugins\BodyChangeNGdistribution.json`.
- The editor writes settings and rules as validated JSON.

---

## **CREDITS AND LICENSE**

Body Change NG is licensed under [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html).
The corresponding source, build scripts, and version history are available on
[GitHub](https://github.com/compilecraftworks/Body-Change-NG).

Thanks to the authors and maintainers of CommonLibSSE-NG, SKSE64, Address
Library, RaceMenu, Dear ImGui, pugixml, nlohmann/json, BodySlide, and the body,
skin, and compatibility projects used during testing. See the included
`THIRD_PARTY_NOTICES.md` for details.
