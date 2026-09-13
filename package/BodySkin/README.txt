Body Change NG v1.2.1 — BodySkin 설치 안내

[한국어]
일반적인 스킨팩은 설치된 스킨 모드 폴더를 통째로 이 BodySkin 폴더 안에
복사하면 됩니다. 개별 DDS를 골라 옮길 필요 없이 기존 Textures 구조를 유지하세요.
FOMOD를 사용하는 팩은 모드 매니저에서 원하는 옵션을 설치한 결과 폴더를 사용합니다.

게임 Data 기준 전체 DDS 경로 예시:
Data\BodySkin\BnP female skin 4k (CBBE Player and Replacer)\Textures\actors\character\female\femalehead.dds
Data\BodySkin\My Male Skin\Textures\actors\character\male\malebody_1.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Body\femalebody_1_d.dds
Data\BodySkin\My UBE Skin\Textures\!UBE\Head\femalehead_d.dds

MO2에서는 모드 최상위가 Data에 해당합니다.
mods\Body Change NG\BodySkin\스킨 모드 폴더\Textures
구조로 두며, 모드 안에 Data를 한 겹 더 만들지 마세요.
별도 에셋 모드 최상위에 BodySkin을 두어도 됩니다.

- 여러 스킨의 내용물을 한 팩에 합치지 말고 모드 폴더별로 구분합니다.
- 여성 CBBE/UNP, 남성, UBE는 실제 바디 메시와 맞는 파일을 사용합니다.
- 몸·손·발·얼굴의 normal/skin/specular와 조건부 하위 폴더도 보존합니다.
- 일부 DDS만 있어도 적용 가능한 채널을 사용하며, 없는 채널은 원래 제공자를 유지합니다.
- 종족·노인·흡혈귀용 파일이 없으면 선택 팩의 일반 같은 채널을 먼저 사용합니다.
- 일반 발은 원래 레이아웃에 따라 몸 아틀라스를 사용할 수 있습니다.
- 남성 SOS/TNG 성기 스킨은 BodySkin 팩에 함께 둡니다. 여성 후타스킨은 Futanari입니다.
- 스킨 profile.json은 필요하지 않습니다.

틴트마스크도 같은 스킨팩 안에서 읽습니다. 전체 경로 예시:
Data\BodySkin\My Skin\Textures\actors\character\character assets\tintmasks\FemaleHeadLips.dds
위 이름은 입술 틴트 예시입니다. 원래 팩의 인식 가능한 파일 이름을 유지하세요.
스킨과 틴트는 각 탭에서 독립적으로 선택합니다. 옛 Data\TintMask 루트는 사용하지 않습니다.

각 탭의 새로고침은 게임에 보이는 파일 목록을 다시 읽습니다.
새 ESP/DLL이나 MO2 모드를 활성화한 경우에는 게임을 다시 실행해야 할 수 있습니다.
BodySkin 안에 복사된 ESP·메시·스크립트는 원래 모드로 활성화되지 않습니다.
필요한 바디·애드온 본체는 일반 모드 설치 경로에 정상 설치하세요.

[English]
Copy the entire installed skin-mod folder into BodySkin. Preserve its existing
Textures tree; no need to extract DDS files individually. Use the installed
FOMOD result, not an archive containing several competing installer options.

The complete example paths above are relative to the game installation.
In MO2, the mod root is Data: do not add another Data directory inside the mod.
A separate asset mod with BodySkin at its top level is also valid.

Keep packs separate and choose files compatible with your actual body UVs.
Keep all companion maps and conditional folders. Partial packs apply compatible
available channels; a missing conditional channel falls back to the selected
pack's ordinary channel, then to the underlying provider if absent there too.
Conventional feet can use a native body atlas. Male addon skins belong here;
female futanari skins belong under Futanari. No profile.json is required.

Tint masks share the pack's Textures\actors\character\character assets\tintmasks
directory and are selected independently in the player-only Tint Masks tab.
Do not use the obsolete Data\TintMask root.

Refresh re-reads files visible to the running game. New plugins or mod-manager
providers may require a restart. Copying a plugin/mesh/script inside a BodySkin
pack does not activate it as a normal mod; install required bodies/addons normally.
