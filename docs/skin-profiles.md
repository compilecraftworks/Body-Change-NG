# Shared skin profiles

Body Change NG uses one **catalog** of texture profiles, but player selection
and NPC distribution are independent. A profile applied to the player is never
automatically assigned to NPCs. Each NPC distribution rule owns a separate
body-preset pool and skin-profile pool, so different NPC groups may receive
different fixed or stable-randomized results.

A profile changes RaceMenu skin-texture overrides only. Body, hands, and feet
use their corresponding skin geometry overrides, while supplied face textures
target the live face geometry. A pack may be partial: only its supplied parts
and channels change, while absent ones retain the actor's underlying textures.
Body, hands, feet, and face files are never substituted across parts. It does
not replace a NIF, an
ActorBase's Skin Armor, inventory, equipment slots, baked FaceGen files, or NPC
tint masks.

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
the live slot-53 Skin Armor geometry, and the head atlas targets the actor's
live face geometry; it does not require conventional hand/foot files.

Optional UBE PBR, RFAOS, wetness, overlay, and material-specific companion maps
are not assigned to guessed `BSTextureSet` indices. They remain controlled by
the actor's active UBE material/Community Shaders setup, preventing a skin
selection from overwriting an unrelated shader channel.

`profile.json` is optional and is only needed for custom texture selection.
When it exists, it is placed directly beside `Textures` and overrides automatic
detection for that skin folder. Its `id` is the stable value used by favorites
and distribution rules, so do not change it after assigning the profile to NPC
rules.

```json
{
  "schemaVersion": 1,
  "id": "my-skin-a",
  "name": "My Skin A",
  "sex": "female",
  "body": [
    { "index": 0, "path": "Textures\\actors\\character\\female\\femalebody_1.dds" },
    { "index": 1, "path": "Textures\\actors\\character\\female\\femalebody_1_msn.dds" },
    { "index": 2, "path": "Textures\\actors\\character\\female\\femalebody_1_sk.dds" },
    { "index": 7, "path": "Textures\\actors\\character\\female\\femalebody_1_s.dds" }
  ],
  "hands": [
    { "index": 0, "path": "Textures\\actors\\character\\female\\femalehands_1.dds" }
  ],
  "feet": [
    { "index": 0, "path": "Textures\\actors\\character\\female\\femalefeet_1.dds" }
  ],
  "face": [
    { "index": 0, "path": "Textures\\actors\\character\\female\\femalehead.dds" },
    { "index": 1, "path": "Textures\\actors\\character\\female\\femalehead_msn.dds" },
    { "index": 2, "path": "Textures\\actors\\character\\female\\femalehead_sk.dds" },
    { "index": 7, "path": "Textures\\actors\\character\\female\\femalehead_s.dds" }
  ]
}
```

`sex` accepts `female` or `male`. `Textures\\...` is relative to the
skin folder. Explicit paths may also start with `BodySkin\\` or `textures\\`.
Every path must end in `.dds` and must not include `..` or a drive letter.

Shader texture indices follow the RaceMenu/NiOverride convention used here:
`0` diffuse, `1` normal (`_msn`), `2` skin/tint (`_sk`), `3` face detail, and
`7` specular (`_s`). A usable profile may provide any recognized body, hands,
feet, or face map. Missing parts and missing diffuse, normal, subsurface, detail,
or specular channels remain controlled by the actor's original skin/material.

The main Skin list compares every profile with the selected actor's detected
body family. Conventional CBBE 3BA/BHUNP/UNP profiles and UBE profiles are not
shown to the wrong known family. An NPC distribution rule can still contain a
mixed skin pool; at runtime it stably samples only the compatible profiles for
that NPC. Unknown actor evidence keeps the safe fallback and does not hide
profiles.

RaceMenu skin overrides are shared by property slot rather than by mod owner.
If another mod changes the same skin texture slot, the last applied override
wins. Body Change NG therefore does not use a broad “remove all skin
overrides” reset: that could erase another mod's work.
