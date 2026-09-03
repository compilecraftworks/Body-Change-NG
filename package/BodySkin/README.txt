Body Change NG - 스킨팩 설치 안내

이 폴더 아래에 스킨팩마다 별도의 폴더를 만드세요.

일반 CBBE 3BA/BHUNP/UNP 구조:
BodySkin\<스킨팩 이름>\Textures\actors\character\...

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

CBBE 3BA 여성 스킨팩에 아래 파일이 있으면 여성 성기 전용 채널로 함께
등록합니다. 네 파일은 3BA_Vagina(구형 BodySlide의 3BBB_Vagina) 지오메트리에만
적용되며 3BA_Anus, 일반 몸, 손, 발, 얼굴에는 적용하지 않습니다.
femalebody_etc_v2_1.dds
femalebody_etc_v2_1_msn.dds
femalebody_etc_v2_1_sk.dds
femalebody_etc_v2_1_s.dds

아르고니안·카짓은 액터별 종족과 성별을 함께 감지하며 맞는 스킨만
표시·미리보기·재적용·NPC 배포합니다. 두 종족의 꼬리 NIF는 원래 같은
성별의 몸 아틀라스를 사용하므로 몸 텍스처 채널을 꼬리 슬롯에도 적용합니다.

!UBE\Body 및 !UBE\Head 구조는 UBE 스킨으로 자동 분류하며 UBE의 슬롯 53
바디와 얼굴에 적용합니다. 일반 스킨과 UBE 스킨은 선택한 액터의 실제
바디 계열에 맞는 것만 목록에 표시됩니다.

게임 실행 중 폴더를 추가했다면 Body Change NG의 바디스킨 탭에서
새로고침을 누르세요. 한 팩 안의 각 종족·성별 조합은 별도로 감지됩니다.

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
For CBBE 3BA female packs, femalebody_etc_v2_1 with its _msn, _sk, and _s
companions is mapped only to 3BA_Vagina/3BBB_Vagina, never to 3BA_Anus or any
other body part.
