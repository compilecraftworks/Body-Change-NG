# Shared skin profiles

Body Change NG uses one **catalog** of texture profiles, but player selection
and NPC distribution are independent. A profile applied to the player is never
automatically assigned to NPCs. Each NPC distribution rule owns a separate
body-preset pool and skin-profile pool, so different NPC groups may receive
different fixed or stable-randomized results.

A profile changes RaceMenu skin-texture overrides only. Body, hands, and feet
use skin-slot overrides, while the live face geometry receives matching texture
paths so the full set changes together. It does not replace a NIF, an
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

The folder name is your skin's display name. With no JSON at all, Body Change
NG scans the standard body/hands/feet names under `Textures` and creates a
female and/or male profile automatically. This is the normal setup.

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
`7` specular (`_s`). A usable profile must provide body, hands, and face maps;
when feet are omitted, body maps are used for the feet slot as a legacy
BodyChange-compatible fallback.

RaceMenu skin overrides are shared by property slot rather than by mod owner.
If another mod changes the same skin texture slot, the last applied override
wins. Body Change NG therefore does not use a broad “remove all skin
overrides” reset: that could erase another mod's work.
