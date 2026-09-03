Body Change NG - 스킨팩 설치 안내

이 폴더 아래에 스킨팩마다 별도의 폴더를 만드세요.

일반 CBBE 3BA/BHUNP/UNP 구조:
BodySkin\<스킨팩 이름>\Textures\actors\character\...

UBE 2.0 구조:
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_d.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_n.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Body\femalebody_1_sk.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_d.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_n.dds
BodySkin\<스킨팩 이름>\Textures\!UBE\Head\femalehead_sk.dds

스킨 모드의 textures 폴더 내용을 <스킨팩 이름>\Textures 아래에 원래
폴더 구조 그대로 넣으면 됩니다. 얼굴까지 함께 바꾸려면 body, hands,
head에 대응하는 DDS 파일이 모두 필요합니다.

!UBE\Body 및 !UBE\Head 구조는 UBE 스킨으로 자동 분류하며 UBE의 슬롯 53
바디와 얼굴에 적용합니다. 일반 스킨과 UBE 스킨은 선택한 액터의 실제
바디 계열에 맞는 것만 목록에 표시됩니다.

게임 실행 중 폴더를 추가했다면 Body Change NG의 스킨 탭에서
새로고침을 누르세요. 각 최상위 스킨팩 폴더가 목록의 한 항목이 됩니다.

English:
Create one folder per skin pack and keep the original texture tree under
BodySkin\<pack name>\Textures. Press Refresh on the Skin tab after adding a
pack while the game is running. Conventional skins use
Textures\actors\character\..., while UBE 2.0 skins keep their
Textures\!UBE\Body and Textures\!UBE\Head d/n/sk atlases. The catalog shows
only skin packs compatible with the selected actor's detected body family.
