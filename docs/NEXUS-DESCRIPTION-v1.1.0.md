# **BODY CHANGE NG · v1.1.0**
### Change BodySlide presets, actor skins, and player tint masks in real time

**NATIVE IN-GAME GUI  •  GAMEPAD SUPPORT  •  LIVE NPC DISTRIBUTION**

---

## **OVERVIEW**

Body Change NG manages BodySlide body morphs, actor skin textures, and player RaceMenu tint layers from one native in-game interface. Select the player or a loaded NPC, preview changes immediately, edit NPC distribution conditions in game, and either distribute to loaded NPCs now or save the rules for the next launch.

**No MCM. No manual distribution-JSON editing. No runtime body-mesh replacement.**

---

## **WHAT'S NEW IN 1.1.0**

- **Repeat-safe body application** — absolute preset targets prevent repeated preview or distribution from stacking the same shape; omitted compatible sliders target zero without deleting other mods' morph keys
- **Broader skin support** — improved male hands/feet and partial packs, per-actor UBE and beast-race matching, dedicated 3BA/UNP atlas routing, and optional SOS addon textures
- **More NPC conditions** — keyword, class, and combat style, plus populated faction dropdowns and load-order-safe form targets
- **Bounded automatic work** — coalesced event processing in both modes and a deferred initial load pass, without recurring asset scans

---

## **WHY BODY CHANGE NG?**

### **Compared with OBody NG**

Both mods ultimately deform a built body through RaceMenu BodyMorph and TRI data. Body Change NG does not swap in a separate body mesh. The difference is the selection, distribution, and state-management layer built above that shared morph pipeline.

- **Native GUI instead of an MCM** — press F7 to manage loaded actors, bodies, skins, player tint, and distribution rules in one scalable window
- **Actor-matched Body and Skin lists** — mixed CBBE 3BA and UBE installations are detected per actor from the live winning skin/head path. Skin packs additionally match the actor's humanoid, Argonian, or Khajiit race and female/male sex, so incompatible assets are not mixed; uncertain body-family evidence keeps the safe fallback visible
- **Explicit rule pools** — choose exactly which body presets and skin packs each rule may distribute instead of treating every installed preset as a candidate
- **Independent Body and Skin decisions** — distribute or exclude either category while the other category continues to use its selected pool
- **Visible top-down priority** — the first matching rule owns both decisions, making broad-to-specific ordering easy to inspect and edit
- **Apply now or next launch** — reevaluate loaded NPCs immediately, or save edits for the next launch without changing the active rules in the current session
- **Save-specific results** — selected results and application signatures are stored in the SKSE co-save so unchanged actors do not require full redistribution on every load
- **Isolated live preview** — preview, committed body, and outfit correction use separate owned morph keys; an older queued preview cannot overwrite the latest selection
- **One integrated workflow** — actor skins, player tint detail, RaceMenu rebuild recovery, favorites, and NPC distribution share the same UI and state model

### **Compared with Legacy Mesh-Slot Systems**

Legacy mesh-slot systems build a separate body mesh for each predefined slot, then use Papyrus to replace prepared ESP Skin Armor, HeadPart, and TextureSet records as a unit. A skin slot therefore also requires its matching built mesh and records.

Body Change NG instead uses a native SKSE architecture without predefined ESP body slots.

- **Skin is independent of mesh sets** — the actor keeps its existing Skin Armor, HeadPart, and NIFs while supported texture channels are changed on the loaded body, hands, feet, and face through RaceMenu/NiOverride
- **Body and Skin are freely composable** — body shape is a BodyMorph preset and skin is a separate texture override; either can change without forcing a matching copy of the other
- **No fixed CustomSet1-CustomSet20 slots** — catalogs are built from installed BodySlide XML files and skin/tint folders
- **Runtime installation and refresh** — add BodySlide XML or skin/tint folders while Skyrim is running, press Refresh on that tab, and use them without rebuilding an ESP slot or restarting the game
- **Player and NPC support** — direct selection, conditional NPC Body/Skin distribution, and save-specific evaluated results share one implementation
- **Direct tint editing** — choose player tint assets, edit each layer's color and opacity immediately, or restore its captured RaceMenu value

---

## **FEATURES**

### **Body**

- Select the player or a loaded NPC and apply compatible BodySlide presets immediately
- Low/high-weight values are interpolated using the actor's current weight
- Conservative BodyFamily detection filters the main list while retaining safe fallbacks for uncertain and multi-family presets
- The editor preserves your selected rule pools; runtime distribution checks the matched actor's sex and known family before choosing a candidate
- Repeated preview, confirmation, and NPC distribution target the selected preset's absolute compatible-family shape; omitted XML sliders target zero through BCNG-owned compensation, not a global morph clear
- CBBE 3BA and UBE use the same SliderPresets folder; Preset set and Group metadata inside each XML identify its family
- Body application uses RaceMenu BodyMorph; no body mesh is generated or replaced while the game is running

### **Skin**

- Apply independent skin packs to the player or a selected loaded NPC
- Persistent RaceMenu/NiOverride keys support body, hands, feet, face, vampire face, diffuse, normal, subsurface, specular, and compatible detail textures
- Partial skin packs are supported: only supplied parts and channels change, missing values retain the actor's underlying textures, and files are never substituted across body, hands, feet, or face
- UBE 2.0 Body/Head d-n-sk atlases are detected separately and applied to the live slot-53 body and face; known-incompatible conventional and UBE skins are never shown or distributed to the wrong detected family
- Argonian and Khajiit female/male folders are detected separately and matched to each actor's race and sex for preview, reapply, and NPC distribution; their body atlas is also applied to the matching live tail geometry
- CBBE 3BA and BHUNP/UNP genital/anal atlases are routed only to matching material geometry, separately from regular body maps
- Optional SOS Smurf Average, VectorPlexus Regular, and VectorPlexus Muscular textures match the actor's live addon, race, and elder variant
- Femaleold and race-specific face normals apply only when matching files exist; Astrid/Afflicted textures and BodySkin tint-mask DDS are excluded
- Ownership-aware cleanup does not delete another mod's texture keys

### **Player Tint**

- Install multiple tint packs and choose a pack from the Tint tab
- Automatically select a usable DDS for each layer and the player's current race
- Adjust color and opacity per layer immediately in game
- Restore the exact RaceMenu value captured before Body Change NG first changed that layer
- Reapply the confirmed tint selection after RaceMenu rebuilds the player

### **NPC Distribution**

- Rules are evaluated from top to bottom; the first matching rule owns Body and Skin
- Body and Skin can independently distribute selected entries, be excluded from distribution, or be left unchanged
- Targets include all NPCs, custom followers, elders, plugins, races, factions, keywords, classes, combat styles, names, and exact NPC base FormIDs
- Faction, race, keyword, class, and combat-style targets use plugin plus local FormID; unnamed forms remain selectable
- One selected preset or skin pack is fixed; multiple choices produce stable per-NPC random results
- Eight editable starter exclusions remain: custom followers and elders of both sexes exclude Body only; Argonians and Khajiit of both sexes exclude Skin only
- Edit every condition and pool in game, then distribute to loaded NPCs immediately or save the edits for the next launch
- Evaluated actor results are kept in the SKSE co-save to avoid redistributing unchanged NPCs

**Rules file**
`Data\SKSE\Plugins\BodyChangeNGdistribution.json`

The in-game editor writes this file, so manual JSON editing is not required. To preserve your rules when reinstalling the mod, Skyrim, or an MO2 profile, back up the file, copy it back to the same path, and press **Load saved values** in the NPC Distribution window.

### **Outfit Correction and Randomization**

- Optional breast correction while clothed
- Separate nipple-correction toggle
- Optional stable nipple and genital slider randomization

### **UI and Input**

- Korean, English, and Simplified Chinese UI
- Default F7 menu hotkey can be rebound in Mod Settings, including Ctrl, Shift, and Ctrl+Shift combinations
- Mouse, keyboard arrows/WASD, and gamepad D-pad navigation
- Gamepad confirm and cancel actions
- Native IME text input, Backspace, search fields, favorites, and favorites-only filtering

---

## **REQUIREMENTS**

- Skyrim SE 1.5.97 or a verified Skyrim AE 1.6.x runtime through 1.6.1179
- Matching SKSE64
- Address Library for SKSE Plugins
- RaceMenu with BodyMorph and NiOverride

---

## **INSTALLATION**

1. Install the main archive with MO2
2. Keep the included folder structure intact and enable the mod
3. Launch Skyrim through SKSE and press F7

---

## **ADDING BODY PRESETS, SKIN PACKS, AND TINT PACKS**

All paths below are relative to an MO2 mod root. You may place these assets inside Body Change NG or in separate enabled MO2 mods that provide the same virtual paths.

### **Body Preset XML**
`CalienteTools\BodySlide\SliderPresets\*.xml`

Place standard BodySlide preset XML files in this folder.

### **Skin Packs**
**Conventional:** BodySkin\YourSkinPack\Textures\actors\character\...
**CBBE 3BA female genital/anal atlas:** femalebody_etc_v2_1.dds plus _msn, _sk, and _s in the conventional female directory. These files target the matching 3BA/3BBB vagina and anus geometries that share the atlas, never another body part.
**BHUNP/UNP female genital/anal atlas:** BakaUNP\VaginalAnalCanal2.dds plus _msn, _sk, and _s under the conventional female directory. These files target the matching vagina, anus, and canal geometries only.
**Argonian:** BodySkin\YourSkinPack\Textures\actors\character\argonianfemale\... and argonianmale\...
**Khajiit:** BodySkin\YourSkinPack\Textures\actors\character\khajiitfemale\... and khajiitmale\...
**Male:** BodySkin\YourSkinPack\Textures\actors\character\male\...
**SOS addons:** BodySkin\YourSkinPack\Textures\actors\character\SOS\... — preserve the original Smurf Average, VectorPlexus Regular, or VectorPlexus Muscular folder and DDS names; install the corresponding addon separately
**Elder/race variants:** preserve femaleold and humanoid race folders inside the same pack; missing variant files keep the actor's underlying texture
**UBE 2.0:** BodySkin\YourSkinPack\Textures\!UBE\Body\femalebody_1_[d/n/sk].dds
BodySkin\YourSkinPack\Textures\!UBE\Head\femalehead_[d/n/sk].dds

Create one folder per skin pack and preserve the skin mod's original Textures tree and DDS files. One pack may contain all four Argonian/Khajiit race-sex folders; each populated combination is detected separately. Do not move UBE atlases into the conventional female folder. Catalog rows are matched to the selected actor's race, sex, and detected body family.

### **Tint Packs**
`TintMask\YourTintPack\textures\actors\character\character assets\tintmasks\*.dds`

Create one folder per tint pack and preserve its RaceMenu tint-mask filenames and folder structure. Each folder directly under **TintMask** becomes one entry in the in-game catalog.
For a UBE-only tint-mask pack, include **UBE** in the top-level pack name. Packs marked **COtR** are treated as compatible with both UBE and conventional female heads.

**Adding files while Skyrim is running**

BodySlide preset XML files, skin-pack folders, and tint-pack folders may be added or replaced while the game is running. Open the corresponding **Body Presets**, **Body Skins**, or **Tint Masks** tab and press **Refresh** to rescan the files and update the list without restarting Skyrim.

The archive includes a README in every user-asset folder and a valid schema-4 distribution JSON with eight editable starter exclusions. Existing schema-3 files remain readable and are upgraded when saved.

---

## **OPTIONAL OBODY NG JSON COMPATIBILITY**

**This is optional.** Body Change NG works without OBody NG or its JSON. Use this only if you want to reuse OBody NG-format distribution conditions, outfit-correction rules, or both.

**Optional OBody NG file**
`Data\SKSE\Plugins\OBody_presetDistributionConfig.json`

Place it beside `BodyChangeNGdistribution.json[/b] under [b]Data\SKSE\Plugins`. Do not rename either file, merge the two JSON files, or replace Body Change NG's own JSON.

- **Distribution conditions** — press **Load saved values** in the NPC Distribution window to import supported OBody distribution rules when the file exists
- **Outfit-correction rules** — press **Register OBody NG outfit-correction rules** in the Outfit · randomization window
- **Independent choices** — import only distribution conditions, register only outfit-correction rules, or use both
- **Read only** — the OBody JSON is never read automatically at startup and is never modified by Body Change NG

The [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052) is a supported optional source.

---

## **COMPATIBILITY NOTES**

- Body Change NG does not edit NIFs, Skin Armor records, ArmorAddon records, or equipped slots
- Confirmed Body, Skin, and Tint selections are reapplied after RaceMenu closes
- After removing legacy BodyChange.esp, obsolete non-UBE RaceMenu face references can be repaired with an adjacent backup and available High Poly Head/vanilla fallback; UBE custom-head presets are preserved
- Skin support replaces matching texture channels; it does not create missing geometry or change its UV mapping
- **Racial Skin Variance (RSV)** — Body Preset and TintMask features are independent of RSV. When a BCNG BodySkin is selected for an RSV actor, BCNG targets that actor's live RSV Skin Armor and applies every available body, hands, feet, face, and conditional channel. RSV keeps ownership of its serialized FaceGen keys; BCNG paints its selected face on the live node and performs one coalesced reconciliation after RSV's deferred node update. Clearing the BCNG skin naturally reveals RSV again. Missing BCNG channels continue to use the RSV/base texture

---

## **CREDITS & LICENSE**

Body Change NG is licensed under [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html). The complete source code, build scripts, and matching version tags are available on [GitHub](https://github.com/compilecraftworks/Body-Change-NG). Nexus provides the MO2-ready Release ZIP only.

Credits to the authors of CommonLibSSE-NG, Dear ImGui, pugixml, nlohmann/json, SKSE64, Address Library for SKSE Plugins, and RaceMenu. Detailed third-party notices and license texts are included in the distribution ZIP. All respective rights belong to their original authors.
