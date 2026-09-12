# 얼굴 팩 기준 수정 / 오버레이 미해결 조사 — 2026-09-10

## 상태

부분 수정 및 진단용 시험 빌드다. 얼굴의 시각적 결과와 오버레이 간헐 실패가
게임에서 해결됐다고 판정한 빌드가 아니다. 공개 Release는 변경하지 않는다.

사용자 피드백: 몸 스킨은 동작하지만 얼굴이 다른 팩처럼 보이며, 오버레이는
선택에 따라 동작하다가 더 이상 교체되지 않는다.

## 확인한 증거

실행 로그: `Documents/My Games/Skyrim Special Edition/SKSE/BodyChangeNG.log`,
2026-09-10 19:40:50–19:47:44 실행. 설치 DLL은 이전 시험본
`4A692ED664B3CFD9EEAA730DDBECE83BF0C956D393EA4FF6CB3660DCDE871223`이었다.

최근 선택된 6개 팩의 일반 얼굴 diffuse를 직접 SHA-256 비교했다.
모두 아래 원본과 overwrite/캐시 내용이 일치했다. 이 비교는 일반 얼굴
diffuse만 대상으로 하며, 모든 변형/맵 또는 화면의 실제 재질 검증은 아니다.

원본 루트:
`D:/TuLED13E/File Mod Skyrim SE/mods/Body Changer NG Skin_Tint/BodySkin`

캐시 루트:
`D:/TuLED13E/File Mod Skyrim SE/overwrite/textures/BodyChangeNG/Cache/skin-face`

| 팩 | femalehead.dds 캐시 디렉터리 | 원본과 일치 |
|---|---|---|
| DiamondZhizhenGlass | 8266471BC5F5723E | 예 |
| 니블A | 5F50C4E8FD97E4DF | 예 |
| 다이아 (뷰지스 기본) | A101169EFD665C83 | 예 |
| 다이아 Ni 밝은톤 | A9518B288C8DF4B0 | 예 |
| 다이아 보추 | 279C2C7E4AB9D4A7 | 예 |
| 다이아+지젠+툴리소스+리얼걸+NI+다렌 | 86D9E00E78567398 | 예 |

얼굴 TXST 기록에는 해당 선택별로 다른 캐시가 반영된다. 이 로그만으로
FaceGen/실제 렌더링이 새 TXST를 소비했는지는 입증할 수 없다.

오버레이 로그에서는 Face 19:46:45.072, Body 19:46:58.588,
Hands 19:47:18.175–20.549에 stored/live=true였다. 이후 같은 노드와
마지막 DDS가 화면 재질에 남아 있는데 저장 키 조회가 대부분 absent로
나오면서 ownershipConflict로 다음 선택과 색 변경이 차단된다.
키 삭제, 조회 문제, 외부 모드 개입 중 어느 것인지는 아직 확정되지 않았다.
앞선 upsert/continuation 취소 수정으로 이 증상이 해결됐다는 주장은 철회한다.

## 이번 수정

- `SkinApplicationPlan.h`, `NativeSkinBackend.cpp`: BCNG가 현재 얼굴 TXST를
  소유할 때 얼굴 디테일 선택 기준은 복제 전 원본 경로를 사용한다.
  이전 팩의 frek/rough 등이 다음 팩의 선택 기준이 되는 순서 의존성을 제거했다.
  외부 공급자가 현재 얼굴 TXST를 소유하는 경우에는 그 현재 경로를 존중한다.
- `SkinArchitectureTests.cpp`: A의 단일 frek → B의 frek/rough 선택이
  B 직접 선택과 같아야 하는 회귀 테스트, 외부 공급자 및 원본 빈 경로 테스트.
- `NativeSkinBackend.h/.cpp`, `ActorEvents.cpp`: 기존 NiNode 이벤트 후
  읽기 전용 얼굴 로그. 현재 얼굴 TXST 소유 여부, 각 슬롯의 예상/실제 재질
  경로, 실제 diffuse 리소스 이름을 기록한다. Geometry/Material 포인터를
  저장하지 않으며 추가 3D 갱신이나 텍스처 쓰기를 하지 않는다.
- `RaceMenuOverlay.cpp`: BCNG가 키를 명시적으로 제거하는 호출에 로그를
  추가했다. 키 복구, 소유권 우회, 다른 슬롯 재할당, 외부 키 삭제는 추가하지 않았다.

일반 스킨은 Native TXST 방식 그대로다. 오버레이 간헐 실패 자체를 고쳤다고
볼 수 없으며, 추가 실행 로그가 필요하다. 얼굴 기준 수정이 이번 화면 증상
전체를 설명하는지도 아직 검증하지 못했다.

## 검증

- 고정 xmake 3.1.0, 기존 CommonLibSSE-NG 6.7.0, SE/AE flat 설정 유지.
- Release 빌드 성공.
- 전체 17개 회귀 실행 파일 통과, 추가 얼굴 기준 테스트 포함.
- `git diff --check` 통과 (기존 파일의 LF/CRLF 알림만 존재).
- 게임 실행/자동 조작은 하지 않았다.
- 시험 DLL SHA-256:
  `F57C51311108E2698073789B83CE65A6639D5012D65B6B3935447B544055C33A`

다음 게임 확인 시 `BCNG face audit`로 네이티브 TXST와 실제 얼굴 재질을
대조하고, `BCNG overlay explicit removal`과 첫 조회 실패 시각을 대조한다.
조회 실패를 곧바로 외부 모드의 삭제로 단정하면 안 된다.
