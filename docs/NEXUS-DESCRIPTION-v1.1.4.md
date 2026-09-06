# **BODY CHANGE NG · v1.1.4**
### Change BodySlide presets, actor skins, and player tint masks in real time

**NATIVE IN-GAME GUI  •  GAMEPAD SUPPORT  •  LIVE NPC DISTRIBUTION**
**SUPPORTED BODY TYPES: CBBE 3BA  •  BHUNP / UNP  •  UBE  •  HIMBO  •  SAM  •  VANILLA FEMALE / MALE**

---

## **OVERVIEW**

Body Change NG manages BodySlide body morphs, actor skin textures, and player RaceMenu tint layers from one native in-game interface. Select the player or a loaded NPC, preview changes immediately, edit NPC distribution conditions in game, and either distribute to loaded NPCs now or save the rules for the next launch.

**No MCM. No manual distribution-JSON editing. No runtime body-mesh replacement.**

---

## **WHAT'S NEW IN 1.1.2–1.1.4**

- **Stable NPC skin sequencing** — automatic NPC body and outfit rebuilds now settle before the selected skin is applied, fixing the NPC-only flash followed by an immediate return to the original skin
- **Final-clone multipart repaint** — a skin is marked complete only after the final Biped clone has received the correct body, hand, foot, face, genital, and anal routes; this covers RaceMenu Override v0/v1 and v2
- **Already-dead NPC distribution** — loaded corpses are no longer left on Zeroed Sliders merely because they were dead before BCNG evaluated the cell
- **Persistent rule editing and bulk selection** — closing the NPC Distribution editor saves its next-launch draft, while localized Select all and Clear all controls fill compatible Body or Skin pools without changing this session's active rules
- **Correct conventional hand and foot routing** — CBBE 3BA, BHUNP, and vanilla body, hand, and foot channels target their exact ArmorAddon geometry, so the final foot assignment cannot repaint the hands
- **Hidden equipped-part recovery** — when SFS hides footwear visually while the real feet slot remains occupied, BCNG saves a durable one-bit fallback for only the absent part and protects every visible exact part from cross-part repainting
- **Separated RaceMenu Override generations** — legacy SE v0, UBE's AE-backported v0, official v1, and official v2 are classified independently; v0/v1 use safe Papyrus strings and only audited v2 uses the native wrapper
- **Black and gray interface** — window, panel, card, input, and idle-control surfaces now use a consistent black/charcoal/grayscale foundation, while existing selection, hover, warning, success, progress, favorite, and tint-preview colors remain visible as functional highlights
- **Shorter actor refresh label** — the button now reads **Refresh actors**, with matching Korean and Simplified Chinese labels; its loaded-actor refresh behavior is unchanged
- **Clearer catalogs and rule editor** — Body Skin entries follow the selected actor's detected family, NPC rule pools follow the female/male body types selected in Mod Settings, the redundant per-rule family dropdown and Combat Style target are removed, and detailed target lists open downward
- **Separated male skin families and SOS sets** — explicitly named HIMBO and SAM skin packs stay in their matching family, while each pack's SOS Regular, Muscular, or Smurf textures remain paired with that same pack
- **Localized asset guidance** — the tabs are named **Body Presets**, **Body Skins**, and **Tint Masks**, with Korean, English, and Simplified Chinese empty-list instructions for exact paths and live refresh; tint masks and unsupported overlays are clearly distinguished
- **Stable Default Skin restoration** — clearing a BCNG skin now removes owned keys and already loaded BCNG texture clones, while equipment changes clean stale legacy outfit keys only on the affected actor
- **Portable starter rules** — the eight bundled editable starter exclusions now use English names without renaming rules already created by the user
- **Reliable saved NPC results** — after loading, BCNG verifies each saved body or skin once and reapplies only a missing or broken result
- **Non-stacking BodyMorph application** — repeated preview and distribution reach the preset's absolute target without deleting morph keys owned by other mods
- **Complete multipart skin routing** — body, hands, feet, face, genital, and conditional race/elder parts keep their correct channels through equipment rebuilds and partial packs
- **Clothed and naked skin consistency** — durable one-slot RaceMenu keys preserve a temporarily absent body, hand, or foot target while exact loaded skin and outfit nodes are updated as the primary route
- **Correct UBE and conventional limb atlases** — UBE body, hand, and foot surfaces use the UBE Body atlas; CBBE 3BA/BHUNP hands use their hand atlas while feet use the body atlas unless the pack supplies a dedicated feet set
- **Actor-aware futanari skins** — a conditional catalog supports the active UBE SOS/TNG, UBE/CBBE TRX, or CBBE 3BA ERF genital type, including optional wet companion textures; equipped biped-slot detection keeps the tab stable through NiNode rebuilds, and ordinary BodySkin updates cannot overwrite the selected genital skin
- **Expanded compatibility** — actor-matched CBBE 3BA and UBE 2.0, standard female/male and SOS skins, Argonian/Khajiit, Racial Skin Variance, Mu Dynamic NormalMap, and improved OverlayFix coexistence
- **More responsive processing** — direct selections are prioritized, required skin files are prepared off the game thread, stale work is cancelled, and automatic distribution remains frame-budgeted
- **Faster MO2 skin catalogs** — one backing DDS exposed through physical and virtual Data paths is hashed only once; full DDS reads do not run per NPC event
- **Safer mixed-body anatomy options** — clothed breast/nipple correction and stable NPC nipple/genital randomization are limited to verified CBBE 3BA actors; UBE actors are skipped with clear in-game guidance

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
- NPC body and skin rule pools use the female/male NPC body family selected in Mod Settings; runtime distribution enforces the same family before choosing a candidate
- Repeated preview, confirmation, and NPC distribution target the selected preset's absolute compatible-family shape; omitted XML sliders target zero through BCNG-owned compensation, not a global morph clear
- CBBE 3BA and UBE use the same SliderPresets folder; Preset set and Group metadata inside each XML identify its family
- Body application uses RaceMenu BodyMorph; no body mesh is generated or replaced while the game is running

### **Skin**

- Apply independent skin packs to the player or a selected loaded NPC
- NPC manual Skin and Default Skin previews are locked immediately so actor initialization or automatic distribution cannot overwrite the visible choice
- When a body or outfit rebuild is required, BCNG repaints the final Biped clone once before recording success, preventing hands and genital/anal materials from reverting with the old clone
- Persistent RaceMenu/NiOverride keys support body, hands, feet, face, vampire face, diffuse, normal, subsurface, specular, and compatible detail textures
- Body, hands, and feet use separate single-bit skin-slot keys, while exact outfit ArmorAddon nodes are also updated when clothing embeds visible skin; the two routes prevent clothed and naked parts from falling back independently
- Partial skin packs are supported: only supplied parts and channels change, missing values retain the actor's underlying textures, and files are never substituted across body, hands, feet, or face
- Selected skin parts are verified and repaired after equipment or 3D rebuilds, including looting a dead NPC; missing BCNG cache files are rebuilt from the original pack
- Default Skin clears every BCNG-owned body, hand, foot, face, and conditional texture route. A previously managed Default selection is checked only when that actor's equipment changes, so a stale legacy outfit key is removed without scanning unrelated NPCs
- UBE 2.0 Body/Head d-n-sk atlases are detected separately; the Body atlas follows exact UBE body, hand, and foot skin nodes across slots 32/53 while the Head atlas targets the face
- Conventional CBBE 3BA/BHUNP hands use femalehands channels and feet use the body atlas unless an explicit feet atlas exists; exact limb-node matching avoids repainting gloves, boots, or fabric nodes
- The Body Skins tab shows only the selected actor's detected body family; NPC rule pools use the female/male NPC body type selected in Mod Settings
- Argonian and Khajiit female/male folders are detected separately and matched to each actor's race and sex for preview, reapply, and NPC distribution; their body atlas is also applied to the matching live tail geometry
- CBBE 3BA and BHUNP/UNP genital/anal atlases are routed only to matching material geometry, separately from regular body maps
- Optional SOS Smurf Average, VectorPlexus Regular, and VectorPlexus Muscular textures match the actor's live addon, race, and elder variant
- Femaleold and race-specific face normals apply only when matching files exist; Astrid/Afflicted textures and BodySkin tint-mask DDS are excluded
- Ownership-aware cleanup does not delete another mod's texture keys
- All current body, hand, and foot targets are captured before the first live repaint, and transient detached texture sets are rejected instead of being traversed again during the same selection

### **Futanari Skin**

- The Futanari tab appears only when the selected actor currently has supported UBE SOS/TNG, CBBE 3BA TRX, or CBBE 3BA ERF genital geometry
- Only skin packs matching the actor's live body/addon combination are listed and applied; genital textures remain independent from the full BodySkin profile
- Default futanari skin removes only BCNG genital texture overrides and restores the installed addon's original material
- The selected profile remains saved if the genital addon is temporarily removed and is reapplied when the same supported type returns
- Missing channels are left unchanged, and TRX wet companions such as **wetschlong_110_s.dds** are preserved beside the selected cached skin

### **Player Tint**

- Install multiple tint packs and choose a pack from the Tint tab
- Automatically select a usable DDS for each layer and the player's current race
- Adjust color and opacity per layer immediately in game
- Restore the exact RaceMenu value captured before Body Change NG first changed that layer
- Reapply the confirmed tint selection after RaceMenu rebuilds the player

### **NPC Distribution**

- Rules are evaluated from top to bottom; the first matching rule owns Body and Skin
- Body and Skin can independently distribute selected entries, be excluded from distribution, or be left unchanged
- Targets include all NPCs, custom followers, elders, plugins, races, factions, keywords, classes, names, and exact NPC base FormIDs
- Faction, race, keyword, and class targets use plugin plus local FormID; unnamed forms remain selectable
- One selected preset or skin pack is fixed; multiple choices produce stable per-NPC random results
- Loaded NPCs that are already dead are eligible for the same body and skin rules; disabled, unloaded, non-NPC, and player actors remain excluded
- Eight editable starter exclusions remain: custom followers and elders of both sexes exclude Body only; Argonians and Khajiit of both sexes exclude Skin only
- Use Select all or Clear all for the compatible Body or Skin pool; Select all includes Zeroed Sliders so it can be unchecked when desired
- Closing the rule popup with X or Escape saves the edited next-launch draft. Distribute to loaded NPCs now remains the explicit action that also replaces this session's active rules
- Evaluated actor results are kept in the SKSE co-save to avoid redistributing unchanged NPCs

**Rules file**
`Data\SKSE\Plugins\BodyChangeNGdistribution.json`

The in-game editor writes this file, so manual JSON editing is not required. To preserve your rules when reinstalling the mod, Skyrim, or an MO2 profile, back up the file, copy it back to the same path, and press **Load saved values** in the NPC Distribution window.

### **Outfit Correction and Randomization**

- Optional CBBE 3BA breast correction while clothed, with a separate nipple-correction toggle
- Optional stable CBBE 3BA NPC nipple and genital slider randomization
- UBE uses a materially different slider system, so these anatomy options are not applied to UBE actors; selecting one shows an in-game warning and tooltips
- Global switches remain available in mixed UBE-player/CBBE-NPC installations and are enforced per actor

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

## **HOW TO USE**

1. Press **F7**. Choose the player from the actor box, or press **Refresh actors** and select a currently loaded NPC.
2. Open **Body Presets** or **Body Skins**. A single click applies a live preview. Double-click to confirm while keeping the picker open; closing the picker confirms the last live selection. The Default row removes BCNG's selection for that category.
3. Open **Tint Masks** to change the player's existing RaceMenu tint layers. Choose a pack and layer, then use **Adjust tint values** for color and opacity. **Restore tint values** restores the value captured before BCNG first changed that layer. Tint editing is player-only.
4. When the selected actor has supported female genital geometry, the **Futanari** tab appears automatically. Choose only a UBE SOS/TNG, CBBE 3BA TRX, or CBBE 3BA ERF skin matching the detected addon. This choice is independent from Body Skin and remains saved if the addon is temporarily removed.
5. For automatic NPC assignment, first choose the female and male NPC body types in **Mod Settings**. Open **NPC Distribution**, edit conditions, use the exact Body/Skin pools or their Select all controls, then choose **Distribute to loaded NPCs now** to activate them immediately. Closing the popup also saves the current draft for the next launch without replacing the active rules in this session.
6. Use **Outfit · randomization** only for verified CBBE 3BA actors. Breast/nipple correction and NPC nipple/genital randomization do not run on UBE or an uncertain female body family.
7. Files added while Skyrim is running become available after pressing **Refresh** on the matching tab.

**Save scope:** Direct actor choices and evaluated NPC results belong to the current SKSE co-save and return when that same save is loaded. A completely new game does not inherit direct actor choices from another save. Distribution rules are stored separately in `BodyChangeNGdistribution.json`.

---

## **ADDING BODY PRESETS, SKIN PACKS, FUTANARI SKINS, AND TINT PACKS**

All paths below are relative to an MO2 mod root. You may place these assets inside Body Change NG or in separate enabled MO2 mods that provide the same virtual paths.

### **Body Preset XML**
`CalienteTools\BodySlide\SliderPresets\*.xml`

Place standard BodySlide preset XML files in this folder.

### **Skin Packs**

```text
CBBE 3BA:
BodySkin\CBBE 3BA - Skin A\Textures\actors\character\female\...

BHUNP / UNP:
BodySkin\BHUNP UNP - Skin B\Textures\actors\character\female\...

UBE 2.0:
BodySkin\UBE - Skin C\Textures\!UBE\Body\femalebody_1_[d/n/sk].dds
BodySkin\UBE - Skin C\Textures\!UBE\Head\femalehead_[d/n/sk].dds

HIMBO with its matching SOS textures:
BodySkin\HIMBO - Skin D\Textures\actors\character\male\...
BodySkin\HIMBO - Skin D\Textures\actors\character\SOS\<addon name>\...

SAM with its matching SOS textures:
BodySkin\SAM - Skin E\Textures\actors\character\male\...
BodySkin\SAM - Skin E\Textures\actors\character\SOS\<addon name>\...

Vanilla or beast-race packs use their original actors\character folders
inside another separately named BodySkin folder.
```

Create one top-level folder for each selectable skin **and body family**. Do not put CBBE 3BA, BHUNP/UNP, UBE, HIMBO, SAM, and Vanilla assets together under one skin-pack name. Inside that family-specific folder, keep only the matching body, hand, foot, face, optional race/elder, and—on a male pack—SOS files from the same skin. BCNG never combines assets from another skin-pack folder. Preserve the source mod's Textures tree and DDS names.

- **CBBE 3BA genital/anal atlas:** keep `femalebody_etc_v2_1.dds` plus `_msn`, `_sk`, and `_s` in the conventional female directory. They target only matching 3BA/3BBB vagina and anus geometry.
- **BHUNP/UNP genital/anal atlas:** keep `BakaUNP\VaginalAnalCanal2.dds` plus `_msn`, `_sk`, and `_s` under the conventional female directory. They target only matching vagina, anus, and canal geometry.
- **Argonian/Khajiit:** preserve `argonianfemale`, `argonianmale`, `khajiitfemale`, and `khajiitmale` inside the same pack. Each populated race/sex combination is detected separately.
- **Male plus SOS:** place male body/hand/foot/face files and SOS addon folders under the same `BodySkin\Your Skin Pack`. The live slot-52 addon chooses Regular, Muscular, or Smurf and the actor's humanoid, Argonian, Khajiit, or elder atlas. If the top-level pack name explicitly contains **HIMBO** or **SAM**, it is restricted to that family; an unlabelled general male skin keeps the compatible fallback.
- **UBE 2.0:** keep Body and Head in the `!UBE` namespace. Do not move UBE atlases into `actors\character\female`. The UBE Body atlas supplies live body, hand, and foot surfaces; the Head atlas supplies the face.
- **Partial packs:** missing parts or material channels keep the actor's underlying texture. A body file is never copied into hands, feet, face, or genital geometry.

Catalog rows are matched to the selected actor's body family, race, and sex. NPC distribution uses the female/male body type chosen in Mod Settings.

### **Futanari Skin Packs**
**UBE with UBE SOS/TNG:** `Futanari\YourSkinPack\Textures\!UBE\Body\malebody_1_[d/n/sk].dds`
**CBBE 3BA with TRX:** `Futanari\YourSkinPack\Textures\[TRX] Futa addon\Regular\Default\schlong.dds` plus `_msn`, `_sk`, and `_s`
**CBBE 3BA with ERF:** `Futanari\YourSkinPack\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds` plus `_msn`, `_sk`, and `_s`

Create one folder per pack and preserve the original DDS names. This is an independent genital-only catalog, not a full BodySkin pack. The tab appears only while the selected actor has supported genital geometry and shows only the detected UBE/TRX/ERF type. Applying a pack does not replace body, hand, foot, or face textures. Default restores the addon's original material. Press **Refresh** after adding or replacing a pack while Skyrim is running.

### **Tint Packs**
`TintMask\YourTintPack\textures\actors\character\character assets\tintmasks\*.dds`

Create one folder per tint pack and preserve its RaceMenu tint-mask filenames and folder structure. Each folder directly under **TintMask** becomes one entry in the player-only catalog. BCNG changes existing RaceMenu skin-tint layers; it does not create RaceMenu overlay slots.

Only tint-mask-based tints are supported; overlay-based tints are not supported.
For a UBE-only tint-mask pack, include **UBE** in the top-level pack name so it is shown to a detected UBE player. Packs marked **COtR** are treated as compatible with both UBE and conventional female heads. Ordinary unmarked female tint packs remain conventional CBBE 3BA/BHUNP/UNP entries.

**Adding files while Skyrim is running**

BodySlide preset XML files and BodySkin, Futanari, or TintMask pack folders may be added or replaced while the game is running. Open the corresponding tab and press **Refresh** to rescan the files and update the list without restarting Skyrim.

The archive includes a README in every user-asset folder and a valid schema-4 distribution JSON with eight editable starter exclusions. Existing schema-3 files remain readable and are upgraded when saved.

---

## **OPTIONAL OBODY NG JSON COMPATIBILITY**

**This is optional.** Body Change NG works without OBody NG or its JSON. Use this only if you want to reuse OBody NG-format distribution conditions, outfit-correction rules, or both.

**Optional OBody NG file**
`Data\SKSE\Plugins\OBody_presetDistributionConfig.json`

Place it beside `BodyChangeNGdistribution.json` under `Data\SKSE\Plugins`. Do not rename either file, merge the two JSON files, or replace Body Change NG's own JSON.

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
- **Mu Dynamic NormalMap** — supported normal-detail, mask, and overlay companion files remain beside the selected cached normal map so the full material bundle updates together
- **OverlayFix** — actor-update and asynchronous-work boundaries were tightened to improve coexistence

---

## **CREDITS & LICENSE**

Body Change NG is licensed under [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html). The complete source code, build scripts, and matching version tags are available on [GitHub](https://github.com/compilecraftworks/Body-Change-NG). Nexus provides the MO2-ready Release ZIP only.

Credits to the authors of CommonLibSSE-NG, Dear ImGui, pugixml, nlohmann/json, SKSE64, Address Library for SKSE Plugins, and RaceMenu. Detailed third-party notices and license texts are included in the distribution ZIP. All respective rights belong to their original authors.
