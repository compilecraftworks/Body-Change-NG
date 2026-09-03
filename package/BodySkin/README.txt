Body Change NG - 스킨팩 설치 안내

이 폴더 아래에 스킨팩마다 별도의 폴더를 만드세요.

일반 CBBE 3BA/BHUNP/UNP 구조:
BodySkin\<스킨팩 이름>\Textures\actors\character\...

일반 여성 스킨팩의 선택적 노인·종족 얼굴 구조(원본 경로 그대로):
BodySkin\<스킨팩 이름>\Textures\actors\character\femaleold\FemaleBody_1_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\femaleold\FemaleHands_1_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\femaleold\FemaleHead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\nordfemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\bretonfemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\darkelffemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\highelffemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\imperialfemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\femaleorc\femaleheadorc_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\redguardfemale\femalehead_msn.dds
BodySkin\<스킨팩 이름>\Textures\actors\character\woodelffemale\femalehead_msn.dds

아르고니안·카짓 구조(한 팩에 필요한 폴더를 함께 넣어도 됩니다):
BodySkin\<스킨팩 이름>\Textures\actors\character\argonianfemale\...
BodySkin\<스킨팩 이름>\Textures\actors\character\argonianmale\...
BodySkin\<스킨팩 이름>\Textures\actors\character\khajiitfemale\...
BodySkin\<스킨팩 이름>\Textures\actors\character\khajiitmale\...

UBE 2.0 구조:
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_d.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_n.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_sk.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_d.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_n.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_sk.dds

스킨 모드의 textures 폴더 내용을 <스킨팩 이름>\Textures 아래에 원래
폴더 구조 그대로 넣으면 됩니다. 완전한 세트일 필요는 없습니다. 몸·손·발·
얼굴 중 실제로 들어 있는 부위만 바뀌고, 없는 부위는 액터의 기존 텍스처를
유지합니다. diffuse·normal·subsurface·specular 채널도 같은 원칙이며,
몸 파일을 발이나 손에 대신 쓰지 않습니다.

노인·종족 폴더의 파일은 별도 스킨 항목이 아니라 같은 스킨팩의 조건부
레이어입니다. 선택한 NPC의 종족·성별·노인 여부에 맞을 때만 실제 존재하는
채널을 적용합니다. 해당 폴더나 파일이 없으면 같은 팩의 기본 female 채널을
사용하고, 기본 채널도 없으면 액터가 원래 쓰던 텍스처를 유지합니다.
Astrid 전용 astridbody/astridhands/astridhead 파일과 Afflicted 전용 파일은
일반 NPC 스킨으로 등록하지 않습니다. character assets\tintmasks 아래 DDS도
바디스킨 목록·미리보기·NPC 배포에 포함하지 않으며 TintMask 팩으로 따로
설치해야 합니다.

CBBE 3BA 여성 스킨팩에 아래 파일이 있으면 여성 성기·항문 공용 채널로 함께
등록합니다. 네 파일은 같은 아틀라스를 참조하는 3BA/3BBB의 Vagina와 Anus
지오메트리에만 적용되며 일반 몸, 손, 발, 얼굴에는 적용하지 않습니다.
femalebody_etc_v2_1.dds
femalebody_etc_v2_1_msn.dds
femalebody_etc_v2_1_sk.dds
femalebody_etc_v2_1_s.dds

BHUNP/UNP 여성 스킨팩은 원본 경로의 아래 네 파일을 함께 등록합니다.
이 아틀라스는 BaseShapeVagina, BaseShapeAnus, BaseShapeCanal에만 적용됩니다.
BakaUNP\VaginalAnalCanal2.dds
BakaUNP\VaginalAnalCanal2_msn.dds
BakaUNP\VaginalAnalCanal2_sk.dds
BakaUNP\VaginalAnalCanal2_s.dds

SOS 남성 성기 스킨도 원본 애드온 폴더 구조를 그대로 유지합니다.
BodySkin\<스킨팩 이름>\Textures\actors\character\SOS\<애드온 이름>\malegenitals_1.dds
같은 폴더의 _msn, _sk, _s 및 malegenitals_argonian_1*,
malegenitals_khajiit_1*, malegenitals_old_1*도 존재하는 파일만 등록됩니다.
현재 액터가 슬롯 52에 착용한 Smurf Average, VectorPlexus Regular 또는
VectorPlexus Muscular ArmorAddon 경로와 종족·노인 상태에 맞는 채널만
성기 지오메트리에 적용합니다. 빠진 채널은 현재 텍스처를 유지하며 몸·손·
발 텍스처를 대신 복제하지 않습니다.

아르고니안·카짓은 액터별 종족과 성별을 함께 감지하며 맞는 스킨만
표시·미리보기·재적용·NPC 배포합니다. 두 종족의 꼬리 NIF는 원래 같은
성별의 몸 아틀라스를 사용하므로 몸 텍스처 채널을 꼬리 슬롯에도 적용합니다.

!UBE\Body 및 !UBE\Head 구조는 UBE 스킨으로 자동 분류하며 UBE의 슬롯 53
바디와 얼굴에 적용합니다. 일반 스킨과 UBE 스킨은 선택한 액터의 실제
바디 계열에 맞는 것만 목록에 표시됩니다.

게임 실행 중 폴더를 추가했다면 Body Change NG의 바디스킨 탭에서
새로고침을 누르세요. 아르고니안·카짓의 각 종족·성별 조합은 별도로
감지하며, 인간형 여성의 노인·종족 얼굴은 같은 팩 안에서 자동 선택합니다.

English:
Create one folder per skin pack and keep the original texture tree under
BodySkin\<pack name>\Textures. Press Refresh on the Body Skins tab after adding a
pack while the game is running. Conventional skins use
Textures\actors\character\..., while UBE 2.0 skins keep their
Textures\!UBE\Body and Textures\!UBE\Head d/n/sk atlases. The catalog shows
only skin packs compatible with the selected actor's race, sex, and detected
body family. Argonian and Khajiit packs preserve their argonianfemale,
argonianmale, khajiitfemale, and khajiitmale folders; their body atlas also
targets the live tail geometry.
Skin packs may be partial: only supplied body parts and material channels are
overridden, while missing ones retain the actor's underlying textures. Files
are never substituted across body, hands, feet, or face slots.
Optional femaleold body/hand/face channels and the original nordfemale,
bretonfemale, darkelffemale, highelffemale, imperialfemale, femaleorc,
redguardfemale, and woodelffemale face-normal paths remain part of the same
skin-pack row. They are selected per actor; a missing conditional channel falls
back to the pack's base female channel, then to the actor's original texture.
Astrid/Afflicted-specific files and DDS files under character assets\tintmasks
are not registered as Body Skin assets.
For CBBE 3BA female packs, femalebody_etc_v2_1 with its _msn, _sk, and _s
companions is mapped to the matching 3BA/3BBB vagina and anus geometries that
share this atlas, never to another body part. BHUNP/UNP packs keep the
BakaUNP\VaginalAnalCanal2 atlas for their vagina, anus, and canal geometries.
SOS male-genital packs preserve
Textures\actors\character\SOS\<addon name>\malegenitals_1* plus any supplied
malegenitals_argonian_1*, malegenitals_khajiit_1*, or malegenitals_old_1*
files. The live slot-52 ArmorAddon selects the matching Smurf Average or
VectorPlexus Regular/Muscular material. Missing variants and channels retain
the currently loaded texture and never borrow a body, hand, or foot map.
