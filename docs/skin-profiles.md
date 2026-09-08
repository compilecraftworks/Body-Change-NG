# Shared skin profiles

Body Change NG uses one **catalog** of texture profiles, but player selection
and NPC distribution are independent. A profile applied to the player is never
automatically assigned to NPCs. Each NPC distribution rule owns a separate
body-preset pool and skin-profile pool, so different NPC groups may receive
different fixed or stable-randomized results.

A profile changes the actor's private native Skin Armor/TXST graph, with the
legacy RaceMenu override backend retained only for supported older interface
versions. A pack may be partial: only its supplied parts
and channels change, while absent ones retain the actor's underlying textures.
Body, hands, feet, and face files are never guessed across parts. A verified
humanoid layout may intentionally use its body atlas for feet when no separate
feet atlas exists; beast-race and unknown layouts never do. It does
not replace a NIF, inventory, equipment slots, baked FaceGen files, or NPC tint
masks.

Put each skin pack below `BodySkin` at the MO2 mod root (the game's virtual
`Data` root). Copy the source skin mod's `textures` folder contents directly
into that skin's `Textures` folder:

```text
BodySkin\
  MySkinA\
    Textures\actors\character\female\femalebody_1.dds
    Textures\actors\character\female\femalehands_1.dds
  MySkinB\
    Textures\...
```

Argonian and Khajiit packs use the same top-level `BodySkin` convention. Keep
their original Bethesda race folders and file names. One pack may contain any
or all four race/sex folders; each populated race/sex combination becomes its
own compatible catalog row:

```text
BodySkin\
  My Beast Skin\
    Textures\actors\character\argonianfemale\argonianfemalebody.dds
    Textures\actors\character\argonianmale\argonianmalebody.dds
    Textures\actors\character\khajiitfemale\femalebody.dds
    Textures\actors\character\khajiitmale\bodymale.dds
```

Body Change NG detects the selected actor's race and sex separately. Humanoid,
Argonian, and Khajiit skins are not shown, previewed, reapplied, or distributed
across race boundaries. Body, hands, feet, and face channels remain independent;
if a beast-race pack has no dedicated feet files, its current feet textures are
left unchanged rather than borrowing the body map. Argonian and Khajiit tail
NIFs intentionally use their sex-specific body atlas, so that same body atlas
is also applied to the live tail slot for those two races only.

UBE 2.0 keeps its separate UV/topology texture namespace. Preserve both atlas
folders instead of moving their files into the conventional female folder:

```text
BodySkin\
  My UBE Skin\
    Textures\!UBE\Body\femalebody_1_d.dds
    Textures\!UBE\Body\femalebody_1_n.dds
    Textures\!UBE\Body\femalebody_1_sk.dds
    Textures\!UBE\Head\femalehead_d.dds
    Textures\!UBE\Head\femalehead_n.dds
    Textures\!UBE\Head\femalehead_sk.dds
```

The folder name is your skin's display name. With no JSON at all, Body Change
NG scans the standard body/hands/feet names under `Textures` and creates a
female and/or male profile automatically. It also recognizes the UBE Body and
Head d/n/sk atlases as one UBE-only female profile. The UBE body atlas targets
every live UBE body, hand, and foot surface, including outfits that split the
visible body between slots 32 and 53. The head atlas targets the actor's live
face geometry; UBE does not require conventional hand/foot files.

For CBBE 3BA, `femalebody_etc_v2_1` plus its `_msn`, `_sk`, and `_s`
companions is a shared genital/anal atlas and is applied only to matching
`3BA`/`3BBB` vagina and anus geometries. For BHUNP/UNP, preserve
`female\BakaUNP\VaginalAnalCanal2` and the same four channels; that atlas is
applied only to matching vagina, anus, and canal geometries. These atlases are
never substituted for the regular body, hands, feet, or face.

SOS male addons use a separate slot-52 ArmorAddon. Keep the original addon
folder and file names inside the skin pack:

```text
BodySkin\
  My Male Skin\
    Textures\actors\character\male\malebody_1.dds
    Textures\actors\character\male\malehands_1_msn.dds
    Textures\actors\character\SOS\VectorPlexus Regular\malegenitals_1.dds
    Textures\actors\character\SOS\VectorPlexus Regular\malegenitals_1_msn.dds
    Textures\actors\character\SOS\VectorPlexus Muscular\malegenitals_1_msn.dds
```

Body Change NG reads the live slot-52 ArmorAddon model path and selects the
matching Smurf Average, VectorPlexus Regular, or VectorPlexus Muscular atlas.
The optional `malegenitals_argonian_1*`, `malegenitals_khajiit_1*`, and
`malegenitals_old_1*` files override only their matching actor variant. Each
body part and each diffuse, normal, subsurface, or specular channel is
independent: missing files retain the currently loaded material, and a body map
is never copied to hands, feet, or genitals.

Optional UBE PBR, RFAOS, wetness, overlay, and material-specific companion maps
are not assigned to guessed `BSTextureSet` indices. They remain controlled by
the actor's active UBE material/Community Shaders setup, preventing a skin
selection from overwriting an unrelated shader channel.

Skin-pack metadata JSON is not supported. Every conventional humanoid texture
tree is classified as `Legacy`; only an explicit `!UBE` Body/Head tree is
classified as `UBE`. Argonian and Khajiit remain separate race boundaries, not
body-family classifications. A stale metadata file beside `Textures` is
ignored and cannot change the pack's name, identity, sex, race, layout, or DDS
mapping.

The stable catalog ID is always derived from the top-level folder's relative
path and detected sex, for example `auto:MySkinA:female`. UBE adds the existing
`:ube` suffix and beast-race rows add their existing race suffix. Rename a pack
folder only when intentionally creating a new catalog identity. Favorites and
NPC distribution rules continue to store these automatic IDs.

Shader texture indices follow the RaceMenu/NiOverride convention used here:
`0` diffuse, `1` normal (`_msn`), `2` skin/tint (`_sk`), `3` face detail, and
`7` specular (`_s`). A usable profile may provide any recognized body, hands,
feet, or face map. Missing parts and missing diffuse, normal, subsurface, detail,
or specular channels remain controlled by the actor's original skin/material.

The main Skin list compares every profile with the selected actor's race, sex,
and runtime body family. Humanoid, Argonian, and Khajiit profiles never cross
race boundaries. CBBE 3BA and UNP/BHUNP actors both accept Legacy packs, while
UBE actors accept only UBE packs. An NPC distribution rule can still
contain a mixed skin pool; at runtime it stably samples only the compatible
profiles for that NPC. Existing relative-folder automatic IDs do not change
when a pack is classified. Conflicting/unknown runtime actor evidence still
fails closed.

RaceMenu's broad skin-slot call may traverse several ArmorAddons attached to
one Skin Armor. Body Change NG 1.2 permits that route only for UBE's explicitly
shared body atlas. Every other layout writes only to verified live
Armor+ArmorAddon+geometry targets; a hidden or missing part remains pending for
the next 3D/equipment refresh instead of painting another body part.

RaceMenu skin overrides are shared by property slot rather than by mod owner.
If another mod changes the same skin texture slot, the last applied override
wins. Body Change NG therefore does not use a broad “remove all skin
overrides” reset: that could erase another mod's work.
