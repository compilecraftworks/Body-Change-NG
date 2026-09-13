# BODY CHANGE NG
v1.2.1 · Your character. Your choices.
Body presets • Skins • Tint masks • Optional futanari skins • Overlays
Native in-game interface • Player and NPC editing • Opt-in NPC distribution

Body Change NG (BCNG) brings appearance selection into one in-game window. Preview installed assets, confirm only what you want to keep, and create NPC rules from the items you select. Body shape and skin are independent; you do not need a prepared mesh set for every combination.

BCNG ships no body meshes, skin packs, tattoo collections, or preset collection. It adds no ESP/ESL and does not require its own MCM. Install the assets and their normal requirements separately.

## REQUIREMENTS & GAME VERSIONS

**Required**
- [SKSE64](https://skse.silverlock.org/) built for your exact Skyrim executable version.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), with the database for that runtime. Choose the appropriate SE/AE package.
- [RaceMenu](https://www.nexusmods.com/skyrimspecialedition/mods/19080), including its matching SKSE plugin and scripts. BCNG uses RaceMenu's BodyMorph, NiOverride, and overlay services.

**Required for the feature you use**
- **Body presets:** a supported morph-enabled body, compatible outfits, and BodySlide preset XMLs. To reproduce the XML's intended shape, build **both the character body and the outfits you use** in [BodySlide](https://www.nexusmods.com/skyrimspecialedition/mods/201) with the matching **Zeroed Sliders** preset and **Build Morphs checked**. Install the generated meshes and TRI files together. Building only the body is not sufficient for outfits to follow correctly.
- **Skins and tint masks:** compatible loose DDS files, arranged as shown below.
- **Overlays:** installed RaceMenu paint registrations and textures, or SlaveTats collection JSONs and textures. Enable the relevant overlays in RaceMenu's configuration; BCNG does not increase its slot limit.
- **Genital-addon skins:** the appropriate SOS or TNG setup, its supported addon, and compatible textures. Female futanari skins require a supported female addon; ordinary male addons do not enable this feature.

**Explicitly supported runtime targets**

```text
Skyrim SE: 1.5.97
Skyrim AE: 1.6.317 / 1.6.318 / 1.6.323 / 1.6.342 / 1.6.353
           1.6.629 / 1.6.640 / 1.6.659 / 1.6.1130
           1.6.1170 / 1.6.1179
```

Here, SE/AE refers to the executable version, not whether you purchased the Anniversary Upgrade. **Skyrim VR, LE, Epic 1.6.678, Microsoft Store/Game Pass, and unlisted runtimes—including 1.7.x—are not supported.**

**Do not blindly install the newest SKSE or RaceMenu file.** Use releases made for your game version, including the correct Steam/GOG variant. BCNG handles the known older and newer RaceMenu interfaces rather than requiring one product version; it cannot make a mismatched RaceMenu DLL load.

## INSTALLATION & FIRST USE

- Install the matching requirements and the bodies/assets you want to use.
- Install BCNG with your mod manager. Its DLL must resolve to **Data\SKSE\Plugins\BodyChangeNG.dll**. Keep only one active BCNG DLL.
- For a normal skin pack, copy its entire installed mod folder into BodySkin, as shown below. MO2's mod root corresponds to the game's Data folder: inside an MO2 mod, start with **BodySkin**, **Futanari**, or **CalienteTools**—do not add another Data folder.
- Launch through SKSE, using the same mod-manager profile.
- Press **F7**. The window opens with the player selected. Use **Refresh actors** and the actor dropdown to choose a loaded NPC.
- Choose a tab, single-click to preview, and **double-click to confirm**. Close without confirming to restore the previously committed selection.

**Tab order:** Body Presets → Body Skins → Tint Masks → Futanari Skin → Overlays.
The Futanari Skin tab is hidden when no supported female addon is installed and loaded.

**Add or replace files without closing the game:** You can add or replace body-preset XML files and body-skin, tint-mask, and futanari-skin packs at the full paths documented below while the game is running. Finish copying the files, press **Refresh** in the corresponding tab, then select the updated entry again to preview it and double-click to apply. Refresh updates the catalog; it does not automatically reapply a replacement to actors. With MO2, use the BCNG mod or a content mod already connected to the running game. The files must be accessible to that game process; restart after enabling a new mod or installing an ESP, DLL, or addon. This does not cover installing overlay mods.

### BodySlide preparation — before launching the game

1. Open BodySlide through the same mod-manager profile and select the **Zeroed Sliders** preset appropriate for your installed body family.
2. Check **Build Morphs**. Build the character body and its matching parts, then use **Batch Build** for the compatible outfits you will wear, keeping the same zeroed base and Build Morphs enabled.
3. Enable the generated mesh and TRI output in that profile. Ensure another mod does not overwrite it with a body or outfit built to a different shape.

**Why this matters:** BCNG applies the selected XML's morph values on top of the installed mesh. If another preset's shape is already baked into that mesh, the result will not match the XML's intended shape. **Zeroed Sliders + Build Morphs is required for both body and outfits**; checking Build Morphs alone does not remove a shape already baked into the mesh. The XML sliders must also match the body/outfit's morph data.

### Controls
- **F7:** open/close; configurable in Mod Settings, including modifier combinations.
- **Single click / Up–Down / W–S / D-pad Up–Down:** move through rows and preview.
- **Double click / Enter / your configured Activate input:** confirm the focused item. Keyboard and gamepad both follow Skyrim's current gameplay Activate binding.
- **Left–Right / A–D / D-pad Left–Right:** switch tabs when not editing text or interacting with a popup.
- **Right-mouse drag over the character side:** rotate the character. On a gamepad, hold **LT** and move **RS left/right**.
- **Your configured menu Cancel key (keyboard) / Cancel button (gamepad):** dismiss the active popup first, or close the main UI if no popup is open. BCNG reads each device's current menu Cancel binding instead of requiring a fixed key/button. Close, X, and Escape also work; closing the main UI restores unconfirmed catalog previews.

The camera and character presentation are restored when the UI closes. Search is available across the catalogs; stars and the Favorites filter are available for body presets, body skins, tint packs, futanari skins, and overlays.

The character-rotation hint is right-aligned on the main title bar, immediately **left of X**: right-mouse drag or **LT+RS left/right**. Long hints scale to fit without shortening or truncation.

## USING EACH TAB

### Body Presets
Select a BodySlide shape for the player or a loaded NPC. The list uses the selected actor's sex and detected body family. XML Preset/set and Group metadata help distinguish CBBE 3BA, BHUNP/UNP, UBE, HIMBO, and SAM; renaming an XML does not convert its sliders.

The first **Default Body** row removes the actor's BCNG body/outfit morphs, including old BCNG keys, plus the exact OBody and OClothe keys. Default Body leaves all other morph keys alone regardless of the preservation option. It does not replace installed meshes or turn a custom body into a vanilla mesh. Your body and outfit must contain the corresponding morph data to show a change.

#### How preset morphs are calculated

BCNG reads the XML's small (weight 0) and big (weight 100) values for each slider. With w = actor weight / 100, the recorded morph is:

```text
morph = (small × (1 - w) + big × w) / 100
```

For example, small 20 and big 80 at weight 25 produce 0.35 (35%). Negative values and values above 100% are retained, not clamped to the 0–100% range. BCNG does not subtract other mods' morphs from this XML value when applying the preset.

UNP-family presets have a small set of reverse-defined base sliders, including Breasts and NippleDistance. BCNG converts those endpoints with 1 - (XML value / 100) before interpolation; it does not reverse every slider. Optional NPC nipple/genital randomization can separately replace its selected anatomical sliders, and does not apply to the player.

With **Preserve other mods' morphs** on, BCNG's previous preset/correction keys and the exact OBody/OClothe keys are removed before application; other morph keys remain preserved; preserved keys can also affect the final shape according to RaceMenu's morph-combination setting. With it off, confirming a preset clears existing RaceMenu body morph keys before writing the new values. Previewing does not permanently delete those keys; closing without confirmation restores the committed state.

Clothed breast/nipple correction is a separate layer. The built-in correction uses either a target minus BCNG's preset value, or a weight-interpolated offset added to it. An explicitly selected outfit-correction preset supplies its own correction values instead. These corrections do not rewrite the source XML. To reproduce the intended baseline, build both the body and outfits with **Zeroed Sliders** and **Build Morphs**.

### Body Skins
Choose a texture pack independently of the body preset.
- Body, hands, and feet use a private native Skin Armor/ARMA/TXST setup. The face uses separate NiOverride skin-texture channels—not a tattoo overlay and not a HeadPart/NIF swap.
- Diffuse, normal, skin/subsurface, specular, and recognized detail channels are used where the pack and target provide them. A partial pack is valid; optional genital/anal textures do not have to exist on the current body for its ordinary skin to apply.
- For recognized race, elder, or vampire variants, a matching file in the selected pack takes priority. If absent, the selected pack's ordinary matching channel is used; if that is also absent, the underlying skin provider's channel is retained.
- Conventional feet may use the body's atlas when that is their native layout; hands and genuinely separate atlases keep their own routes. UBE uses its own atlas layout.
- Male SOS/TNG addon textures can be included in a male BodySkin pack. They require a compatible addon and an available native genital backend.
- The first **Default Skin** row restores the underlying provider instead of another BCNG pack. A provider may be vanilla, a replacer, or RSV.

The catalog separates UBE from conventional skins and filters by sex and applicable race information. A conventional listing is **not a UV conversion**: choose CBBE or UNP textures for the mesh you actually use. Likewise, a male SOS texture pack must fit your HIMBO/SOS mesh and addon.

### Tint Masks — player only
Tint masks share the **BodySkin** pack folder. A pack containing both skin DDS and recognized tint masks can appear in both tabs; applying the skin does not automatically select its makeup.

Choose a pack, then a supported tint layer/entry. Preview and confirm as above; use the detail/color controls to adjust color and intensity. Recognized layers include lips, cheeks, eyeliner, eye sockets, freckles, warpaint, and other supported facial tint types. Existing player tint layers determine what can be edited. Lists are filtered for sex and UBE/conventional compatibility using filenames and pack/path information; recognized COtR packs may be shared.

Use the restore entries/controls to restore captured pre-BCNG tint values. **Tint Masks has no NPC distribution.** Arbitrary DDS files without a recognized tint filename are not automatically turned into new RaceMenu layers.

Color and opacity can be edited during a preview. While the main UI stays open, BCNG remembers edits per pack/layer across item and tab changes. Confirm the catalog selection to keep it on the player; closing the main window without confirmation restores the committed tint and clears temporary edits.

### Futanari Skin — optional addon support
- The tab is enabled by installed, loaded **supported female SOS/TNG addons**, including recognized TRX, ERF, and UBE variants. It does not require an eligible actor to be present just to expose the tab and rule tools.
- Manual editing requires the selected female actor to be registered with a supported futanari addon in SOS/TNG. Male actors and unregistered female actors show **Not eligible**.
- Selecting a skin does not register an actor as futanari, equip an addon, or install its mesh. Set that up in SOS/TNG first.
- The list matches the registered addon type and body layout. A skin selected for an eligible actor remains independent of the ordinary body skin, including across a temporary disappearance of its addon geometry.
- The first default row restores the addon's own textures. NPC rules are locked to eligible female futanari NPCs.

### Overlays — Face / Body / Hands / Feet
One scrolling catalog contains four expandable sections. Open an arrow to see installed RaceMenu paints and SlaveTats entries for that area. You do not need to open RaceMenu to populate BCNG's list.

- Single-click previews. **Double-click adds an overlay; double-click an already applied entry to remove it.** Multiple confirmed overlays can coexist in each area.
- A new preview replaces the temporary preview, not the confirmed stack. Closing without confirmation restores the committed stack.
- Selecting an area's first-row Default restores the normal body-skin camera framing. Confirming it removes only that area's BCNG overlays, not other mods' overlays.
- **Applied 2/14** means two confirmed BCNG overlays, out of 14 slots available to BCNG. The denominator is RaceMenu's normal slot capacity minus slots occupied/reserved by other mods. A preview does not increase the count.
- Select an entry and use the fixed bottom **Adjust color and opacity** controls. Color changes on a preview remain preview changes; changes to an already committed overlay update that overlay. The color popup is not a second Apply/Cancel transaction.
- Preview colors are remembered per actor, area, and entry until the main window closes. While **NPC Distribution** checkboxes are visible, checked entries accumulate as a temporary preview stack across all areas; unchecking an entry removes only its preview. The focused row selects which candidate's color/opacity you edit. An unchecked candidate can retain a draft color, but is not previewed until checked. Other mods' occupied slots are never overwritten; checked candidates beyond the available preview slots remain in the distribution pool. These distribution edits do not change the selected actor's committed overlays. Click **Distribute to loaded NPCs now** or **Distribute next game launch** to save the checked candidates and their individual colors with the rules. Automatic distribution selects one candidate per configured area and applies that candidate's color.
- Star entries to favorite them. Long secondary paths are shortened with an ellipsis.

Slot capacity is read from RaceMenu and depends on your configuration. An invisible or transparent foreign overlay can still reserve a slot. BCNG does not automatically clear it to make room.

## NPC DISTRIBUTION — SELECT FIRST, SAVE EXPLICITLY

Available in **Body Presets, Body Skins, Futanari Skin, and Overlays**. Not available in Tint Masks. New installations start with **no rules**; BCNG does not randomly distribute the entire installed catalog.

- For ordinary NPC body/skin pools, set the female and male **NPC distribution body type** in Mod Settings.
- Open the relevant tab and press **NPC distribution** at the right of the Refresh row. The buttons become **Female · Male · Distribute · Cancel distribution**, starting with Female. Female/Male selects the nearest locally loaded NPC of that sex for preview, or the player if none is available. Catalogs and new rules follow the selected distribution sex regardless of the player's sex; body-preset/skin candidates also follow the configured NPC distribution body type. Changing sex clears previous checkmarks and previews. Futanari Skin is female-only: it selects the nearest SOS/TNG-registered female futanari NPC, falling back to the player if none is available. Incompatible candidates can be checked without being previewed on the current actor.
- Check the entries to use. **Select all** and **Clear selection** operate on the visible eligible list. Default/reset rows are not asset candidates.
- Press **Distribute** to open the conditions popup. This button alone does not save or distribute. The selected IDs stay attached to this editing session; adding another rule uses that selection.
- Choose sex and target: all NPCs, name, NPC base FormID, race, faction, class, keyword, or plugin, as available for the category. For **All NPCs**, **Exclude custom followers** and **Exclude elder NPCs** are checked by default.
- Use **+ Add rule**, **Delete rule**, **Up**, and **Down** above the condition list to manage priority.
- Choose **Distribute to loaded NPCs now** to save, activate, and process loaded NPCs, or **Distribute next game launch** to save without changing this session's active rules.

**Press Distribute to loaded NPCs now or Distribute next game launch to save your conditions.**

Rules are evaluated in order independently for each feature, and independently for each overlay area. A single compatible candidate gives a fixed assignment; several candidates form a stable per-NPC random pool. **Automatic overlay distribution picks one candidate per configured area; it does not apply every checked overlay at once.** Manual overlay editing supports the multi-overlay stack.

Automatic pools use conventional/non-UBE candidates and the appropriate sex. Futanari rules require registered female futanari NPCs and match the addon type. Manual confirmed choices, including Default choices, take priority over automatic distribution. NPCs sharing an ActorBase also share its native body-skin assignment; BCNG does not create a separate ActorBase for every spawned reference.

The two All NPCs exclusion checkboxes are target filters, not separate blacklist rules.

In body-preset, body-skin, and futanari-skin distribution lists, clicking a checkbox or row, or navigating with the keyboard/gamepad, previews the last compatible item on the selected actor. Only one item from each of these categories is previewed at a time; checkbox choices remain the distribution pool. Incompatible candidates may stay checked but are not rendered on that actor. Cancel distribution, Clear selection, entering the conditions popup, switching actors/tabs, or closing restores the committed appearance. The normal Futanari Skin list shows only the actor's registered ERF/TRX/UBE TRX type; the distribution pool can mix conventional ERF and TRX candidates, which are matched to each NPC's registered addon.

## FILE PATHS — KEEP THE ORIGINAL TEXTURE TREE

**All paths below start at the game's Data folder.** In MO2, omit the initial Data\ inside the mod directory. Replace the example pack names with your own. The examples show complete paths; entries ending in a backslash identify a directory.

**The normal method: copy the whole installed skin-mod folder into BodySkin.** You do not need to extract individual DDS files or rebuild the inner folders.

For example, copy the installed **BnP female skin 4k (CBBE Player and Replacer)** folder from MO2's mods directory into **Body Change NG\BodySkin**. Keep its existing Textures directory and all its contents.

```text
Before — relative to your MO2 installation:
mods\BnP female skin 4k (CBBE Player and Replacer)\Textures\actors\character\female\femalehead.dds

After — inside the BCNG mod:
mods\Body Change NG\BodySkin\BnP female skin 4k (CBBE Player and Replacer)\Textures\actors\character\female\femalehead.dds

The game sees:
Data\BodySkin\BnP female skin 4k (CBBE Player and Replacer)\Textures\actors\character\female\femalehead.dds
```

Alternatively, keep your packs in a separate MO2 content mod with BodySkin at its top level, so replacing the BCNG plugin mod does not replace your packs. Keep each skin mod in its own folder.

“Installed mod folder” means the result after choosing FOMOD options, not the unprocessed download archive containing competing installer options. Extra meshes/scripts/plugins nested in BodySkin are not activated as a gameplay mod by BCNG; install any required body/addon plugin normally. If the original skin mod is only a texture replacer, keeping it enabled determines the underlying default skin independently of the copied BCNG pack.

### BodySlide XMLs
**Full directory: Data\CalienteTools\BodySlide\SliderPresets\**
A normal preset mod can stay installed normally: BCNG reads its XML files at this standard location.

```text
Data\CalienteTools\BodySlide\SliderPresets\My Presets.xml
```

UBE and conventional presets use the same location. TRI/mesh build output stays in its normal body/outfit paths; it does not belong under BodySkin.

### Conventional female skin — example

```text
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalebody_1.dds
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalebody_1_msn.dds
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalebody_1_sk.dds
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalebody_1_s.dds
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalehands_1.dds
Data\BodySkin\My CBBE Skin\Textures\actors\character\female\femalehead.dds
```

Keep the matching hand/head maps and any recognized conditional subfolders too. For UNP, use the UNP version in another named pack; do not rename its DDS files to simulate CBBE compatibility.

### Conventional male skin and male SOS/TNG addon textures

```text
Data\BodySkin\My Male Skin\Textures\actors\character\male\malebody_1.dds
Data\BodySkin\My Male Skin\Textures\actors\character\male\malehands_1.dds
Data\BodySkin\My Male Skin\Textures\actors\character\male\malehead.dds
Data\BodySkin\My Male Skin\Textures\actors\character\SOS\SmurfAverage\malegenitals_1.dds
```

Keep the original addon directory and companion maps from your selected variant. SmurfAverage is an example, not a required rename. Ordinary male genital textures belong in **BodySkin**, not the female Futanari catalog.

### UBE body and face — example

```text
Data\BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_d.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_n.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_sk.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Head\femalehead_d.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Head\femalehead_n.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Head\femalehead_sk.dds
```

Keep UBE and conventional atlases in separate top-level packs. Preserve the actual !UBE tree.

### Tint masks — inside the same BodySkin pack

```text
Data\BodySkin\My CBBE Skin\Textures\actors\character\character assets\tintmasks\FemaleHeadLips.dds
```

This is a naming example for a recognized lip tint; preserve your pack's original recognizable filenames and tintmasks directory. A tint-only pack may use the same structure without body DDS files. **Do not use the old Data\TintMask root.** Skin scanning excludes tint masks, while the Tint Masks tab scans them separately.

### Female futanari skins — copy the entire skin-mod folder
**Copy the whole installed futanari-skin mod folder into Data\Futanari\.** Keep its Textures tree intact, exactly as with BodySkin.

```text
Before — relative to your MO2 installation:
mods\My ERF Skin\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds

After — inside the BCNG mod:
mods\Body Change NG\Futanari\My ERF Skin\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds

The game sees:
Data\Futanari\My ERF Skin\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds
```

This copies a skin pack, not the SOS/TNG addon registration. Install the actual addon and its requirements normally. You can use a separate MO2 content mod with Futanari at its top level to keep copied skins separate from the plugin.

**Full DDS path examples for supported layouts:**

```text
Data\Futanari\My UBE Addon Skin\Textures\!UBE\Body\malebody_1_d.dds
Data\Futanari\My TRX Skin\Textures\[TRX] Futa addon\Regular\Default\schlong.dds
Data\Futanari\My ERF Skin\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds
```

Keep each variant's full tree and maps: UBE commonly uses _d, _n, _sk; TRX/ERF commonly use diffuse, _msn, _sk, _s. Keep optional wet companion textures if supplied. An ERF DDS cannot be made into a TRX/UBE skin by changing its filename.

### Overlays — install the original collection normally

```text
Data\Textures\Actors\Character\Overlays\       (a common RaceMenu texture location)
Data\Textures\Actors\Character\SlaveTats\*.json  (SlaveTats collection definitions)
Data\Textures\Actors\Character\SlaveTats\     (collection textures)
```

RaceMenu paints are discovered from their registrations; textures alone are not sufficient. A mod can legitimately register another texture path. SlaveTats entries are read from collection JSON and their referenced DDS files. There is **no BCNG Overlay pack root**; do not move these collections into BodySkin or Futanari.

**Refresh after adding or replacing files:** Once copying finishes, press Refresh in Body Presets, Body Skins, Tint Masks, or Futanari Skin. Reselect the updated entry to preview it and double-click to apply it. The files must be accessible to the running game at the full paths above; Refresh does not load new plugins or newly enabled mod-manager virtual paths.

## SETTINGS, SAVES & RESET

```text
Data\SKSE\Plugins\BodyChangeNG\settings.json
Data\SKSE\Plugins\BodyChangeNGdistribution.json
Data\Textures\BodyChangeNG\Cache\
Data\SKSE\Plugins\OBody_presetDistributionConfig.json  (optional ORefit input)
```

- **settings.json:** UI, hotkey, language, favorites, outfit-correction, and related options.
- **BodyChangeNGdistribution.json:** explicitly saved NPC rules. Keep your customized file when upgrading; the installer starter is empty.

- **Runtime texture cache:** generated DDS copies under Textures\BodyChangeNG\Cache. This is output, not your source-pack folder. Keep it available to the same mod-manager profile; do not delete it while the game is running.
- **OBody ORefit JSON:** use [OBody Next Generation ORefit JSON Master List](https://www.nexusmods.com/skyrimspecialedition/mods/105052) for outfit-correction rules, then click **Register OBody NG outfit-correction rules** in BCNG.
- **Actor selections:** confirmed manual and automatic results are stored with the save through SKSE serialization. Keep the matching .skse co-save beside its .ess save; copying settings.json alone does not copy an actor's selections.
- **Tint saves:** the confirmed pack, per-layer color/opacity, restored layers, and captured original DDS/color are stored in that save's co-save. Original tints are not borrowed from another character's shared settings.

MO2 may place generated files in Overwrite or a configured output mod. Check the winning virtual Data path rather than assuming every generated file is physically inside the BCNG mod.

**Mod Settings** includes the opening hotkey, UI language (English/Korean/Simplified Chinese), text/UI scale, character placement, optional game pause, morph preservation, NPC body types, and performance mode. Defaults are **character on the left**, **game pause off**, **performance mode off**, and **Preserve other mods' morphs on**. Existing saved preferences are kept. Performance mode changes automatic-work spacing, not the intended selection.

**Preserve other mods' morphs** controls which existing morph keys BCNG replaces. On: applying a body preset removes BCNG and OBody/OClothe keys before applying the new preset, while preserving other morph keys. Off: applying a body preset clears all of that actor's RaceMenu body morph keys first. In either mode, BCNG writes the XML values, interpolated for actor weight, directly to its own key without subtracting other mods' values. Preserved morphs can therefore add to the visible result. Single-click previews simulate the chosen policy without deleting persistent keys; cancelling restores the committed state. Changing the option does not immediately reset every actor.

OBody and OClothe are exceptions to preservation: confirmation and Default Body remove both even when preservation is enabled. Previews simulate their removal without deleting persistent keys until confirmation. This does not disable redistribution by a running OBody installation; avoid having both systems continuously assign bodies to the same actors.

**Reset selected actor settings** and **Reset all actor settings** restore BCNG-managed body morphs, body skin including supported male-addon skin, separate futanari skin, overlays, and player tint values. “All” includes BCNG's recorded actor states and the player; it is not a deletion of every other mod's appearance data. Reset is saved as an explicit Default choice, so old automatic assignments do not immediately return. It does not delete asset folders or your NPC rule file.

### Outfit correction & randomization
The **Outfit · randomization** popup provides optional clothed breast/nipple correction and NPC nipple/genital-shape randomization for supported conventional female bodies. UBE does not use these correction/randomization options. This is morph adjustment, not genital-addon registration.
Breast/nipple refit combines target adjustments and fixed offsets: targets subtract only BCNG's own preset value, while fixed offsets remain additive. It never subtracts another mod's combined morph value. Body-preset previews use the same outfit-rule decision and calculate correction against the previewed preset without changing committed morphs.

The outfit popup opens centered on first use instead of jumping to the top of the screen. Outfit, settings, NPC-condition, and color popups remember their individual positions for the next time you open them.

**Register OBody NG outfit-correction rules** reads only the outfit-correction information in the OBody ORefit JSON at the full path above. If several MO2 mods provide that file, BCNG reads the winning file. It does not import NPC distribution rules or modify the source JSON.

#### Skyrim Fitting System integration — correction follows the displayed outfit

[Skyrim Fitting System SE-AE (SFS)](https://www.nexusmods.com/skyrimspecialedition/mods/187128) lets you hide actual equipment or show a separately registered outfit while keeping the actual equipment's stats and effects.

With SFS's Rendered Outfit API v1, BCNG automatically bases breast/nipple correction on the final displayed outfit, including ORefit exclusions, force-refit rules, and outfit-specific presets:

- **Actual equipment visible:** use that equipment's name, FormID, and plugin.
- **Registered appearance visible:** use that appearance's name, FormID, and plugin—not the hidden equipment underneath.
- **Both visible:** evaluate the visible items together. Hidden items never contribute their rules.
- **All correction-relevant clothing hidden:** remove both breast and nipple correction.

SFS is optional. This integration requires an SFS build providing Rendered Outfit API v1; without SFS or that API, BCNG keeps its normal actual-equipment behavior.

## COMPATIBILITY & TROUBLESHOOTING

- **RSV and other skin providers:** a BCNG skin replaces supported channels while selected; Default Skin returns control to the underlying provider. This is not simultaneous blending of two complete skin packs, nor a blanket guarantee against another script continuously reapplying its own overrides.
- **Custom outfits:** native skin routing is used where the mesh draws from that skin. An outfit's own hard-coded textures or incompatible UV layout are not automatically converted.
- **Other body controllers:** avoid having two systems continuously assign the same actor's morphs or texture channels.
- **F7 does nothing:** verify the active DLL, exact game/SKSE/RaceMenu combination, and SKSE logs. Do not install a 1.7.x dependency merely because it is labelled latest.
- **A preset appears but does not change the body/outfit, or its shape differs from the XML:** rebuild both the character body and the outfits with the matching Zeroed Sliders preset and Build Morphs checked. Verify the winning mesh/TRI output and matching preset slider names; a different baked-in base shape remains underneath BCNG's morphs.
- **A skin or tint pack is missing:** check the selected actor's sex/layout, the top-level pack folder, resolved Textures tree, and recognized DDS names. Remove accidental Data\Data or Textures\Textures nesting from the installation layout.
- **Purple face or wrong texture:** check the selected pack, missing/corrupt DDS files, winning mod-manager paths, and conflicting face overrides. Include the selection sequence and versions in a report; a screenshot alone cannot establish the cause.
- **Overlay unavailable or color has little effect:** check the area's RaceMenu enable/count settings, foreign reserved slots, and the texture itself. A colored texture will not necessarily tint like a grayscale mask.
- **Futanari tab missing / Not eligible:** distinguish supported female-addon installation from the actor's SOS/TNG registration. Neither an ordinary male addon nor a texture-only pack satisfies both conditions.

The log is **BodyChangeNG.log** in SKSE's log directory, commonly under Documents\My Games\Skyrim Special Edition\SKSE; the location can differ by game edition/setup. For reports, include game/SKSE/RaceMenu versions, body/addon type, pack path, reproduction steps, and the log. Do not assume all combinations are proven leak-free or regression-free: the v1.2.1 Release build and automated regression tests passed, which is not a substitute for every-runtime gameplay and long-session testing.

## UPDATING FROM EARLIER VERSIONS

- A new game or save cleaning is not required solely for this update. Existing BCNG body records are re-evaluated once under the updated cleanup policy; the co-save format is unchanged. Keep existing settings, rules, asset packs, and favorites.
- Exit Skyrim and back up your saves with matching SKSE co-saves, settings, customized distribution JSON, and asset packs before replacing the plugin.
- Keep your existing BodySkin and Futanari packs. Move old standalone tint packs into **Data\BodySkin\Your Pack\Textures\actors\character\character assets\tintmasks\**, retaining their original inner texture tree. Do not leave them only under the obsolete TintMask root.
- Do not overwrite your customized rule file with the empty installer starter. The schema-7 migration removes legacy exclusion entries and retains eligible positive distribution rules. Review the resulting conditions before explicitly saving or distributing.
- **Changed confirmation behavior:** closing the picker now cancels an unconfirmed preview. Closing the NPC rule popup now cancels unsaved rule edits instead of auto-saving a next-launch draft.
- Do not rely on a renamed/deleted pack retaining its previous selection ID. Keep pack names/paths stable when possible. Downgrading a save written by v1.2.1 is not guaranteed.

## CREDITS & SOURCE
Thanks to the SKSE team, expired6978/RaceMenu, CommonLibSSE-NG contributors, Dear ImGui, pugixml, and the authors of the body, texture, addon, and overlay assets you choose to install.

[Body Change NG source repository](https://github.com/compilecraftworks/Body-Change-NG) · GPL-3.0; see LICENSE and THIRD_PARTY_NOTICES.md.
Third-party assets keep their own licenses and are not bundled with BCNG.
