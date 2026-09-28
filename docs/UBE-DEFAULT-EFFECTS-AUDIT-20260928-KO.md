# UBE 기본 RFAOS·wet 유지 — 2026-09-28

## 요청과 적용 범위

UBE 바디스킨의 직접 교체 대상은 Head/femalehead 및 Body/femalebody_1의
`_d.dds`, `_n.dds`, `_sk.dds` 6개다. `AutoUbePart`의 화이트리스트를 유지했다.
스킨팩의 `_RFAOS.dds`, `_wet.dds`, `_s.dds`는 직접 선택 채널에 추가하지 않는다.

사용자가 제시한 툴레드 `ube-rfaos`는 다음 파일을 loose DDS로 제공한다.

- `Data\textures\!UBE\Head\femalehead_RFAOS.dds`
- `Data\textures\!UBE\Body\femalebody_1_RFAOS.dds`
- `Data\textures\!UBE\femalehead_wet.dds`
- `Data\textures\!UBE\femalebody_1_wet.dds`

같은 모드의 ESP는 TES4 헤더만 있으며 BSA에도 같은 4개 DDS만 들어 있다.
이번 경로 보완은 게임의 가상 Data에 보이는 loose DDS를 대상으로 한다.
BSA에만 존재하는 효과 파일을 추출하거나 다른 모드의 설치 파일을 변경하지 않는다.

## 원인과 수정

[Community Shaders의 Skin::SetupExtraTexture](https://github.com/doodlum/skyrim-community-shaders/blob/main/src/Features/Skin.cpp)는
현재 specular 또는 normal 경로의 접미사를 바꾸어 `_rfaos.dds`와 `_wet.dds`를 찾는다.
UBE는 기본 TXST에 specular가 없는 구조를 확인했다. 따라서 BCNG가 normal을
전용 ASCII 캐시 경로로 변경하면 기본 효과가 원래 경로에만 있는 것으로는 부족하다.
로컬 CS DLL 1.4.6.0에도 해당 함수명과 두 접미사 문자열이 있다. 공개 소스 확인은
그 로컬 바이너리 전체의 역어셈블리 또는 인게임 렌더링 검증을 대신하지 않는다.

`RuntimeAssetCache`는 UBE Head/Body의 해당 normal 두 종류에 한해 전역
`Data\textures\!UBE`에서 MO2가 선택한 실제 파일을 읽는다. RFAOS는 해당 부위 폴더,
wet은 해당 부위 폴더를 먼저 보고 없으면 위 공통 루트의 파일을 사용한다.
선택 스킨팩 옆에 있는 동명 효과 파일은 사용하지 않는다. 기본 파일을 normal 캐시
옆에 하드링크/복사하여 CS의 파생 경로를 충족하며 DDS 내용이나 효과 수치는 바꾸지 않는다.

효과 내용도 캐시 서명에 포함하므로 기본 효과 파일을 교체하고 새로고침하면 새 경로가
선택된다. 예전 세이브가 참조할 수 있는 기존 캐시를 삭제하지 않는다. 원본·캐시 내용에
색상 합성이나 인플레이스 쓰기를 하지 않는다. 생성은 기존 원자적 게시 함수를 재사용한다.

## 범위·비용·검사

- 별도 엔진/CS 훅, 새 RaceMenu ABI, 렌더 콜백의 디스크 접근은 추가하지 않았다.
- 기본 파일 탐색·해시는 카탈로그 새로고침당 Head/Body 2개 메모에 한정된다.
- 기존 경로와 같이 준비 작업에서만 파일을 게시한다. 같은 볼륨에서는 기존 하드링크
  경로를 재사용하고 지원되지 않으면 복사한다. GPU 텍스처 캐시와 효과 렌더링은 CS 소관이다.
- 카탈로그 테스트: 정확히 기본 6개만 사용, 대소문자 혼합, 팩 효과 배제,
  MO2 기본 효과 내용 보존, 루트 wet과 부위별 우선순위, 같은 크기/시간 파일 갱신,
  캐시 삭제 복구, 게시 실패·재시도, 효과 미설치와 3BA/TRX 분리.
- Release 빌드와 오프라인 테스트 42/42 통과. 실제 게임의 효과 표시와 VRAM 수치는
  이번 오프라인 테스트로 확정하지 않았다.
