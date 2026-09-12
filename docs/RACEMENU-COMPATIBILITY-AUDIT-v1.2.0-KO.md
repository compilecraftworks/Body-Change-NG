# Body Change NG 1.2.0 RaceMenu 호환성 점검

점검일: 2026-09-09

> 2026-09-11 정정: 1.6.678은 GOG가 아닌 Epic판이며 SKSE 공식 미지원이다.
> 기존 13개 허용 표에서 이 항목을 제거했다. 아래의 과거 테스트 결과를
> 전 버전 인게임 검증으로 해석하지 않는다. 최신 시험 DLL/검증 범위는
> `APPEARANCE-COMPATIBILITY-20260911-KO.md`를 참조한다.

> 2026-09-10 추가 점검에서 구형 문자열 전달 ABI와 Native ARMA 복제 결함을
> 확인하고 수정했다. 최신 증거/검증 범위는
> `SKIN-FORM-AND-RACEMENU-ABI-20260910-KO.md`를 참조한다.

## 지원 범위

Body Change NG가 명시적으로 허용하는 Skyrim SE/AE 런타임과 그 런타임용
RaceMenu를 대상으로 한다. Skyrim VR은 프로젝트 전역에서 지원하지 않는다.
게임 버전과 맞지 않는 RaceMenu DLL 조합은 RaceMenu 자체가 로드되지 않으므로
호환 대상으로 간주하지 않는다.

지원 런타임:

- SE 1.5.97
- AE 1.6.317, 1.6.318, 1.6.323, 1.6.342, 1.6.353
- AE 1.6.629, 1.6.640, GOG 1.6.659
- AE 1.6.1130, 1.6.1170, GOG 1.6.1179

## RaceMenu 세대별 ABI

| RaceMenu 계열 | BodyMorph | Overlay / Override | BCNG 처리 |
|---|---:|---:|---|
| SE 0.4.12~0.4.16 | v4 | v1 / v1 | 전용 legacy-v1 어댑터 |
| 초기 AE~0.4.19.14 | v4 | v1 / v1 | 전용 legacy-v1 어댑터 |
| 0.4.19.15~0.4.19.16 | v4 | v2 / v2 | 공개 wrapper-v2 어댑터 |
| 최신 공식 1.7.99 대응 소스 | v5 | v2 / v2 | 인터페이스 계약 대조 완료; 게임 1.7.99는 BCNG 런타임 지원과 별개 |
| 공통 접두부가 유지되는 BodyMorph v5 백포트 | v5 | v1/v1 또는 v2/v2만 | BodyMorph v5 접두부만 사용 |

Overlay v1과 v2, Override v1과 v2는 버전 숫자만 다른 같은 인터페이스가
아니다. v1은 구형 concrete object와 `BSFixedString`/`OverrideVariant` ABI를
사용하며, v2는 공개 wrapper와 visitor ABI를 사용한다. BCNG는 두 타입을 서로
캐스팅하지 않는다. v1/v2 세대 혼합은 거부하지만, 사용자 요청에 따라 미확인
상위 인터페이스는 확인된 최상위 하위 계약으로 동작한다. BodyMorph 6 이상은
v5의 공통 접두부, Overlay/Override 각각 2 이상은 v2 공개 접두부만 호출한다.
예를 들어 Overlay 3 + Override 2도 v2로 동작한다. 상위 버전이 하위 ABI를
유지한다는 전제이며 실제 미래 버전의 호환성을 검증했다는 뜻은 아니다.
폴백 시 실제 버전/사용 계약/하위 ABI 유지 전제를 한 번 경고로 기록한다.
파일 버전은 기능 차단 기준이 아니다. 최소 계약 미만, 누락 인터페이스,
미지원 Skyrim 게임 런타임에는 추측 폴백을 적용하지 않는다.

2026-09-10 폴백 변경 후 Release 빌드 및 전체 17개 테스트 통과. BodyMorph
6/7/100/UINT32_MAX, Overlay/Override 3 이상과 2의 조합, 최소 버전 미만,
미지원 게임 런타임을 검사했다. 해당 빌드 완료 시 SkyrimSE 프로세스가 실행
중이었으므로 MO2 DLL은 교체하지 않았다. 직전 시험 DLL과 구분해야 한다.

## 기능별 결과

| 기능 | RaceMenu 의존성 | 호환성 처리 |
|---|---|---|
| 네이티브 ImGui UI와 단축키 | 없음 | BCNG 런타임 훅 표로 분기 |
| 바디 프리셋, 의상 보정, NPC 바디 배포 | BodyMorph | v4/v5 공통 접두부만 호출 |
| 몸·손·발·얼굴 스킨, NPC 스킨 배포 | 없음 | Native Skin Armor/TXST이며 일반 스킨 NiOverride 없음 |
| 스킨 적용 직후 3D 갱신 | SKSE Actor 함수 | 한 번의 `QueueNiNodeUpdate` 경로만 사용 |
| 오버레이 목록 | RaceMenuBase 데이터 전송 규약 | RaceMenu 편집 UI를 열지 않고 HUD Scaleform 브리지로 수집 |
| 오버레이 적용·복원·NPC 배포 | Overlay + Override | v1과 v2 전용 어댑터로 분리 |
| 플레이어 틴트마스크 | RaceMenu C++ ABI 없음 | 게임 런타임별 PlayerCharacter 배열 오프셋을 명시적으로 선택 |
| SOS/TNG 라이브 성기 스킨 | 없음 | RaceMenu 인터페이스와 격리된 live-material 경로 |
| 세이브 상태 | 없음 | BCNG SKSE serialization이 바디·스킨·오버레이 선택을 저장 |

BodyMorph가 꺼졌거나 호환되지 않아도 인터페이스 맵과 오버레이 기능까지 같이
비활성화되지 않도록 초기화를 분리했다. RaceMenu 인터페이스가 너무 일찍
조회되어 아직 없을 때도 세션 전체 실패로 고정하지 않고 이후 호출에서 다시
조회한다.

## v1 전용 처리

- 오버레이 노드: `Body [Ovl#]`, `Hands [Ovl#]`, `Feet [Ovl#]`, `Face [Ovl#]`
- 슬롯 수: 가상 Data 경로의 `SKSE/Plugins/skee64.ini`에서 읽음
- 기본 슬롯 수: Body 6, Hands 3, Feet 3, Face 3
- `bEnableOverlays=0`이면 슬롯 0개로 처리
- `bEnableFaceOverlays=0`이면 Face 슬롯만 0개로 처리
- 구형 `OverrideVariant` 크기와 문자열 보유 구조를 컴파일 시 검증

툴레드의 RaceMenu 0.4.16과 UBE 2.0 수정 DLL은 실제 바이너리 안에서도 구형
`Face [Ovl%d]` 형식을 사용한다. TAKEALOOK의 RaceMenu 0.4.20.0은 신형
`Face [Ovl{}]` 형식을 사용한다. BCNG는 각 형식을 해당 ABI에서만 사용한다.

## 회귀 검증

- Release DLL 빌드 성공
- 테스트 실행: 17/17 통과
- 당시 13개 허용 표에 대한 UI 훅·입력 훅·틴트 배열 분기 테스트
  (1.6.678 포함은 이후 정정; 실제 13개 게임의 실행 검증이 아님)
- 각 지원 런타임에서 BodyMorph v4/v5와 Overlay/Override v1/v1·v2/v2 판정 확인
- 미확인 상위 인터페이스의 하위 계약 폴백 및 v1/v2 혼합·미지원 게임 런타임 거부 확인
- 일반 스킨 코드에 RaceMenu Override API가 다시 들어오지 않는지 검사
- NativeSkinBackend에 두 번째 `DoReset3D`/`QueueNiNodeUpdate` 경로가 없는지 검사
- BodyMorph 실패가 Overlay 초기화를 막지 않는지 검사

정적 ABI·빌드·회귀 테스트는 완료됐지만, 실제 Skyrim 안에서의 최종 판정은
별도다. 툴레드 0.4.16에서 첫 스킨 클릭과 오버레이 적용을 확인하고,
TAKEALOOK 0.4.20.0에서도 같은 항목을 확인해야 공개 릴리스 검증이 끝난다.

## 대조한 원본

- RaceMenu 공식 소스: <https://github.com/expired6978/SKSE64Plugins>
- RaceMenu 공식 배포/변경 기록: <https://www.nexusmods.com/skyrimspecialedition/mods/19080>
- 주요 공식 소스 기준 커밋: `86c890a`, `7ffff9a`, `87a5cad`, `867458e`,
  `c1b408f`, `7694eab`, `348607e`, `9ebcb73`
# 설치 안전성

`SKSE\Plugins` 바로 아래에 `.dll` 확장자로 둔 백업 파일도 SKSE가 플러그인으로
로드한다. `BodyChangeNG.before-update.dll` 같은 사본이 원본과 함께 로드되면 동일한
UI·이벤트·직렬화·Skin Armor 갱신이 중복 등록되어 충돌한다. v1.2.0은 파일명이
정확히 `BodyChangeNG.dll`인 모듈만 초기화하며, 백업은 `SKSE\Plugins` 밖에 두거나
`.dll.disabled`처럼 확장자를 바꿔 보관해야 한다.
