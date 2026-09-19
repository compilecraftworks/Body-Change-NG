# Body Change NG 1.3.0 — 전체 기능 재점검

검사일: 2026-09-19. 대상은 커밋 전 1.3.0 작업 트리이며, 기준 HEAD는 `747de9b06bac5a7c74525cd5bf9a554cd932e6cd`이다.

> 후속 상태: 아래는 **수정 전 검사 기록**이다. F1/F2/F3 및 D1은 이후 1.3.0 수정에 반영했고, 정상 결과를 확인하는 `tests/AppearanceLifecycleTests.cpp`와 전체 검사로 재검증했다. `Run-PrecisionAudit130.ps1`도 이 정상 회귀 실행기로 전환했으며 중복된 옛 진단 C++ 파일은 남기지 않았다. 최종 결과는 [1.3.0 수정·검증 기록](RELEASE-VERIFICATION-v1.3.0-KO.md)을 따른다.

## 결론

**기존 회귀 검사는 통과했지만, 추가한 기능 간 전환 진단에서 수정이 필요한 세 경로를 확인했다. 따라서 현재 상태를 결함 없는 배포 승인으로 판단하지 않는다.**

1. 플레이어의 오버레이 미리보기 취소 작업이 RaceMenu 진입 때 삭제되어, 미리보기 기록이 남고 저장 외형 복원을 막을 수 있다.
2. 바디 미리보기 취소 작업은 반대로 RaceMenu 편집 중에도 실행되어 플레이어 모프·지오메트리를 갱신할 수 있다.
3. 기본 바디를 선택한 플레이어의 로드/RaceMenu 종료 복원은 의상 보정 키를 지운 뒤 보정 재평가를 예약하지 않는다.

이 세 가지는 실제 제품 함수를 추출한 오프라인 진단에서 실행 순서와 호출 유무를 확인했다. 실제 게임의 외형 이상·CTD를 재현한 것은 아니다. 별도로 README의 검사 개수 안내가 33개/31개로 불일치한다.

이번 요청에서는 **검사 도구와 이 보고서만 추가**했다. 제품 소스, DLL, 설정, 세이브, MO2, ZIP, GitHub는 변경하지 않았다. 기존 1.3.0 수정 사항과 미커밋 변경을 유지했다.

## 방법과 범위

- 게임을 실행하거나 실행 중 메모리를 읽고 쓰지 않았다. 인게임 검사 및 세이브 조작은 하지 않았다.
- 기존 고정 도구 xmake 3.1.0, CommonLibSSE-NG 6.7.1(`70c1acd5261210982bd52f6d4468a082fe04d798`) 및 저장소 잠금을 유지했다. 의존성 업그레이드 작업이 아니다.
- SE/AE 전용 `EXCLUSIVE_SKYRIM_FLAT` 구성과 VR 비활성화를 확인했다.
- 기존 테스트 재실행뿐 아니라 UI 취소 → 작업 큐 → RaceMenu 이벤트 → 저장 선택 복원의 연결을 검토했다.
- 카탈로그·입력·선택·적용·미리보기·취소·저장·복원·배포·초기화·삭제 준비·외부 연동을 기능군별로 대조했다. 아래 표에서 실행 검사와 소스 검토를 구분한다.
- 모든 코드 줄/외부 모드 조합/엔진 실행 순서를 완전 탐색한 검사는 아니다. 특히 가짜 엔진 경계의 통과를 실제 렌더링 또는 전체 게임 누수 없음으로 확대하지 않는다.

## 추가 발견 사항

### F1 · P2 — 오버레이 취소가 RaceMenu 진입 때 유실됨

관련 위치:

- `src/BodyChangeNG/RaceMenuOverlay.cpp:1404` — `QueueCancelPreviews`
- `src/BodyChangeNG/RaceMenuOverlay.cpp:1437` — `QueueReapplySaved`
- `src/BodyChangeNG/ActorEvents.cpp:363` — RaceMenu 열림 시 플레이어 작업 취소
- `src/BodyChangeNG/FrameTasks.cpp:20` — `Pump`

조건과 흐름:

1. 플레이어에게 오버레이 미리보기 기록이 있다.
2. BCNG를 닫아 미리보기 복원을 예약한다. 이 작업은 플레이어 FormID에 귀속된다.
3. 복원 실행 전에 RaceMenu를 열면 `CancelActor(player)`가 해당 작업을 제거한다. RaceMenu 편집 중 일반 플레이어 작업을 폐기하는 Pump 경로도 있다.
4. 미리보기 기록 삭제는 아직 실행되지 않은 `RestorePreviewNow` 안에 있으므로 기록이 남는다.
5. RaceMenu를 닫아 저장 오버레이를 복원하려 해도 `HasActivePreview(actor)`에서 반환한다. 같은 미리보기 보호 조건을 사용하는 자동 배포도 영향을 받을 수 있다.

새 진단은 얼굴·몸·손·발 모두에서 기록 잔존과 저장 복원 건너뜀을 확인했다. RaceMenu 없이 취소를 실행하는 대조군에서는 복원되고 기록이 지워진다.

권장 수정 방향: 언로드/메뉴 취소에 살아남되 RaceMenu 편집 중에는 기다리는 **대상·세션·최신 선택 식별이 있는 복원**을 사용한다. 단순히 기록만 지우거나 무조건 ActorID 0 작업으로 바꾸면, 원상 복원 누락 또는 F2와 같은 편집 중 변경 문제가 생긴다.

### F2 · P2 — 바디 미리보기 취소가 RaceMenu 편집 보호를 우회함

관련 위치:

- `src/BodyChangeNG/RaceMenuBodyMorph.cpp:291` — `ClearPreviewNow`
- `src/BodyChangeNG/RaceMenuBodyMorph.cpp:950` — `QueueCancelPreview`
- `src/BodyChangeNG/RaceMenuBodyMorph.cpp:964` — `QueueClearInactivePreview`
- `src/BodyChangeNG/FrameTasks.cpp:20` — `Pump`

바디 취소는 언로드 취소에서 살아남도록 `Queue(0, ...)`으로 예약한다. 이 작업에는 새 스킨 취소 복원처럼 대상 플레이어를 식별하는 복원 lease가 없다. 따라서 `CancelActor(player)`에도 남고, Pump의 플레이어 편집 보호에도 걸리지 않는다. `ClearPreviewNow` 자체도 RaceMenu 열림을 확인하지 않는다.

실제 `QueueCancelPreview`, `Pump`, `ClearPreviewNow`를 연결한 진단 결과:

| 조건 | 미리보기 키 정리 호출 | 지오메트리 갱신 호출 |
| --- | ---: | ---: |
| RaceMenu 닫힘 | 1 | 1 |
| RaceMenu 열림 | 1 | 1 |

즉 편집 중 변경이 코드상 확인된다. 다만 그것이 특정 환경에서 깜빡임이나 CTD를 일으켰다는 증거는 없다. `QueueClearInactivePreview`도 같은 예약 구조이므로 함께 검토해야 한다.

권장 수정 방향: F1과 같은 복원 수명/보류 규칙으로 처리하되, 새 미리보기·새 선택·세션 변경 검사를 유지한다. 일반 액터 작업으로 되돌리기만 하면 기존 언로드 복구가 다시 깨진다.

### F3 · P2 — 기본 바디 플레이어 복원에 의상 보정 재평가가 없음

관련 위치:

- `src/BodyChangeNG/ActorEvents.cpp:126` — `ReapplyPlayerSelectionsAfterRaceMenu`
- `src/BodyChangeNG/ActorEvents.cpp:192` — 로드 후 같은 복원 함수 호출
- `src/BodyChangeNG/RaceMenuBodyMorph.cpp:934` — 이름 있는 프리셋 복원의 보정 재평가
- `src/BodyChangeNG/RaceMenuBodyMorph.cpp:1030` — 기본 바디 정리

기본 바디의 수동 선택이 저장되어 있으면 플레이어 복원 함수는 `QueueClearBodyChangeMorphs`만 호출한다. 이 정리는 기본/미리보기 키뿐 아니라 BCNG 의상 보정 키도 제거한다. 보정 서명은 무효화하지만 `OutfitRefit::ProcessActor`를 예약하지 않는다.

반면 이름 있는 프리셋은 `QueueReapplyCurrent`를 통해 보정도 재평가한다. SFS가 없는 대조 조건에서 종료 후 재확인 2회를 포함해 실행하면:

| 저장된 바디 | 바디 복원 예약 | 의상 보정 재평가 호출 |
| --- | ---: | ---: |
| 이름 있는 프리셋 | 3 | 3 |
| 기본 바디 | 3 | 0 |

로드 후 플레이어 복원도 같은 함수를 사용한다. 이후 장비/SFS 이벤트가 오면 다시 평가될 수 있으므로, 모든 환경에서 영구적으로 보정이 꺼진다고 주장하지 않는다. 일반 NPC 처리 큐는 별도로 의상 재평가를 연결하므로 이 누락을 NPC 전체 문제로 일반화하지 않는다.

권장 수정 방향: 기본 바디를 복원하는 이 호출 경로에서도 기존 정상 의상 정책에 재평가를 연결한다. **명시적 전체 초기화/삭제 준비에 무조건 보정을 다시 넣는 변경은 하지 않아야 한다.** 보정 수식 자체의 문제는 아니다.

### D1 · P3 — 문서의 검사 개수 불일치

`README.md` 상단은 33개 테스트, Requirements 하단은 31개로 적혀 있다. 실제 이번 실행 대상은 33개다. 배포 문서를 최종 정리할 때 중복 안내를 맞춰야 한다. 이번 검사에서는 기존 문서를 수정하지 않았다.

## 기능별 점검표

“추가 결함 미확인”은 표에 적힌 오프라인 범위에서 새 오류를 확인하지 못했다는 뜻이며, 인게임 보증이 아니다.

| 기능군 | 대조한 내용 / 근거 | 결과 또는 한계 |
| --- | --- | --- |
| 바디 XML 카탈로그 | PresetCatalog, UbeMorph, BodyFamily, 소스의 목록/적용 경로 | 빈 UBE Zeroed, 계열 분류, 슬라이더 이름·범위 정책 검사 통과 |
| 바디 모프 값/키 | BodyMorphKey 4,511개 확인, 보존 옵션 ON/OFF, OBody/OClothe 정리 | 기존 계산·소유 키 정책 유지; 실제 TRI 결과는 미측정 |
| 바디 미리보기/취소 | AppearanceIntegration, PreviewRestore, 추가 제품 함수 진단 | 기존 단독 경로 통과, **F2 발견** |
| 바디 기본값/자동 복구 | 저장 선택·실제 키 검증, 기본값과 의상 키 분리 | 기존 1.3.0 수정 통과, 플레이어 복원 연결에서 **F3 발견** |
| 가슴/유두 보정 | BodyMorphPolicies, OutfitRefitRules, 적용 세대/서명/장비 이벤트 | 기존 수식과 CBBE/BHUNP 분기 유지; UBE에 해당 해부학 보정 금지 |
| NPC 해부학 랜덤화 | BodyMorphPolicies, 설정 변경 후 재적용 코드 | 플레이어/UBE 제외, 계열별 슬라이더 정책 대조; 실제 외형 미검증 |
| ORefit 규칙 | OutfitRefitRules, 이름/FormID/플러그인 평가 경로 | 가져오기·제외·강제 규칙 및 원본 파일 비변경 정책 대조 |
| SFS 유무/최종 의상 | RenderedOutfit 436개 확인, 실제/등록/모두 숨김, 적용 직전 stamp | 기존 분기 및 뒤늦은 상태 거부 통과; 실제 SFS 렌더 미실행 |
| 바디/얼굴 스킨 | SkinArchitecture, SkinSessionState, FaceSkinPolicy, TexturePathIsolation | A→B/기본값/부분 실패/대상 추가 정책 검사 통과 |
| 얼굴 비동기 완료 | FailurePath, 콜백 상태 및 재생성 확인 소스 | 실패·응답 누락·세션 변경 처리 검사; VM/렌더러 자체는 가짜 경계 |
| 스킨 미리보기 복원 | PreviewRestore, AppearanceIntegration, FrameTaskQueue | 새 RaceMenu 보류/언로드 생존/후속 작업 세션 거부 통과 |
| 남성 성기/후타 스킨 | NativeAddonPolicy, GenitalPersistence, 공급자 원본 분리 코드 | 원본 TXST/형상 불변·부분 텍스처 보존·동일 크기 변경 재생성 경로 대조 |
| ERF/TRX/UBE TRX | FutanariSupport, SkinTargetResolver, 호환성/배포 후보 분리 | 설치 여부와 액터 등록 판정 분리 유지; 실제 애드온별 렌더 미실행 |
| SOS/TNG 크기/애드온 변경 | 이벤트 재조정·source identity·기존 baseline 교체 및 intrusive 참조 | 단위/패턴 검사 통과; 실제 크기 변경 시각 검증 아님 |
| 오버레이 목록/소유 슬롯 | OverlayPolicy, AppearanceIntegration, 적용/제거/슬롯 예약 | 기존 외부 슬롯 보호·색상·중복/미배정 슬롯 검사 통과 |
| 오버레이 미리보기/복원 | 추가 제품 함수 진단, 네 부위, 새 배포 작업 우선권 대조 | **F1 발견**. 새 배포가 저장 복원에 덮이는 가설은 반례 검사로 배제 |
| 배포 체크/누적 색상 초안 | AppearanceColorDrafts, DistributionOverlayColors, UI 연결 | 체크/해제·초안/확정·색상 전달 정책 대조; 실제 화면 미검증 |
| 플레이어 틴트 마스크 | AssetCatalog, ActorState, PlayerTintSerialization 및 적용/복원 소스 | 성별/계열 필터, 기본값 백업, 세이브별 상태, 잘못된 숫자 거부 대조 |
| NPC 액터 선택/검색 | ActorCatalog, ActorSearchPolicy, UiCatalogPolicy | 적대 제외 없음, 가까운 32명/거리와 명시 검색 분리, 미로드 참조 처리 대조 |
| 배포 성별/가까운 대상 | UI 및 UiCatalogPolicy, 커스텀/노인 자동 대상 제외 | 성별 기본/플레이어 fallback·후타 조건 대조; 수동 대상 정책 유지 |
| 배포 조건 팝업 | DistributionTargetRead, DistributionLifecycle | 엔진 메타데이터 읽기와 UI 값 분리, 초기화·취소·stale 결과 거부 통과 |
| 배포 명시 저장 | DistributionPersistence 88개 범위/성별/기능 조합 | 실제 JSON 교체 실패·재시도, 저장 초안과 활성 규칙 분리 통과 |
| NPC 자동 배포 | Distribution, ActorWorkQueue, GenitalPersistence | 독립 기능별 규칙·수동 선택 우선·후타 재평가 대조 |
| 세이브/로드/새 게임 | ActorState, SessionSnapshot, DistributionLifecycle, main.cpp | ASTR/TINT/얼굴 데이터·ID 해석·구버전·초기화 대조; **F3는 복원 연결 누락** |
| 선택/전체 초기화 | ActorSettingsReset, FailurePath, RemovalPreparation | 바디/스킨/성기/후타/오버레이/틴트 대상과 소유권 정리 대조 |
| 삭제 준비 | RemovalPreparation, 대기/시간초과/실패/검증 완료 조건 소스 | 미완료를 완료로 표시하지 않는 조건 검사; 실제 세이브 정상화 보증 아님 |
| 마우스/저프레임 | MouseInputReplay, MouseInputQueue, NativeImGuiHost | 60/15/5 FPS 모형 각각 클릭 20/20; Windows/게임 입력 이중 경로 대조 |
| 키보드/게임패드 | Hotkey, MenuActionInput, InputSink 및 게임 입력 차단 코드 | Activate/Cancel 매핑, press/release, 포커스/캡처/중복 처리 대조 |
| UI/팝업/카메라 | PopupPlacement, FittedTextUI, UI 및 MenuCharacterPresentation | 위치 저장·복원·오버레이 기본 카메라·회전/닫기 대조; DPI별 실화면 미검증 |
| 설정/즐겨찾기/언어 | Settings, PathMigration, RaceMenuPresetMigration | 보존 ON, 왼쪽, 일시정지 OFF, 성능 OFF 및 설정 검증·정규화 대조 |
| DDS/XML 새로고침 | CatalogRefresh, RuntimeAssetCache, 각 카탈로그/UI | 공유 목록·세대 캐시·작업 병합·소유 worker 종료 순서 대조 |
| 큐/취소/세션 | FrameTaskQueue, AsyncWorkGuard, 추가 100만 동작 모델 | 기존 회귀 검사 및 추가 모델 통과; 실제 연결의 예외는 F1/F2 |
| 런타임/RaceMenu ABI | RuntimeLayout, CodePattern, 주소 DB·저장된 코드 캡처 | 12개 SE/AE 명시 버전, 알려진 ABI 세대 대조; 미래 ABI 보증 없음 |
| 외부 동시 스킨/바디 제어 | README와 적용 소유권/자동 재적용 경로 | OBody/RSV/Selector 비호환 정책 유지; 자동 충돌 해결 기능 없음 |

## 실행 검사 기록

### 기존 검사

- Release 빌드 명령 재실행 성공.
- `build/v1.3.0/tests/*Tests.exe`: **33개 모두 종료 코드 0**.
- `tools/test-addon-audits.py`: **19개 성공**.
- DistributionLifecycle: 초기화 16조합, 취소 경계 5개, 경합 400회.
- PreviewRestore: 언로드/재시도 10,000회.
- CatalogRefresh: 연속 새로고침 10,000회 병합 검사.

실행 대상 33개:

```text
ActorState AppearanceIntegration AssetCatalog AsyncWorkGuard BodyFamily
BodyMorphKey CatalogRefresh CodePattern DistributionLifecycle
DistributionPersistence DistributionTargetRead FaceSkinPolicy FailurePath
FormDeletePolicy FrameTaskQueue GenitalPersistence Hotkey MouseInputReplay
NativeAddonPolicy OutfitRefitRules OverlayPolicy PathMigration PopupPlacement
PresetCatalog PreviewRestore RaceMenuPresetMigration RemovalPreparation
RenderedOutfit RuntimeLayout SkinArchitecture SkinSessionState
TexturePathIsolation UbeMorph
```

### 새 진단

`tools/audits/Run-PrecisionAudit130.ps1`은 현재 작업 트리의 제품 함수 7개를 수정 없이 추출하고 `PrecisionAudit130.cpp`의 가짜 엔진 경계와 연결한다. 실제 FrameTaskQueue 헤더도 사용한다.

- 제품 함수: Pump, ClearPreviewNow, QueueCancelPreview, QueueReapplyCurrent, QueueCancelPreviews, QueueReapplySaved, ReapplyPlayerSelectionsAfterRaceMenu.
- F1/F2/F3를 재현하고 정상 대조 조건과 비교했다.
- 새 배포 선택이 같은 부위의 저장 복원에 덮이는지 검사했다. 기존 `HasActorChannelWork` 보호가 작동하므로 이 가설은 결함으로 보고하지 않는다.
- 10개 고정 seed × 100,000동작: 32액터 × 8채널에서 최신 작업 교체·지연·우선순위·액터 취소·세션 초기화·최종 배출 결과를 별도 모델과 비교했다. 활성 큐 길이 256 이하와 최종 결과 일치를 확인했다.
- 같은 진단을 MSVC AddressSanitizer(`/fsanitize=address /O2`)로도 실행했다. 검사된 경로에서 sanitizer 오류가 보고되지 않았다. 이는 전체 플러그인/엔진 ASan 실행이 아니다.

**주의:** 이 진단은 현재 결함을 재현하면 종료 코드 0이다. 따라서 기존 회귀 검사와 달리 정상 동작 승인으로 세면 안 된다. 추후 수정 후에는 의도한 정상 결과를 확인하는 회귀 검사로 전환해야 한다.

### 주소/저장된 엔진 코드

- 배포 대상 읽기: 12개 런타임 각각 11개 relocation 확인.
- 성기 TXST 공급 경로: 12개 런타임 각각 32개 주소 ID 확인.
- 기존 1.5.97/1.6.1170 코드 캡처의 소유권 패턴 14개와 호출 지점 4개 확인.
- 원래 검사기는 삭제/이동된 게임 경로 때문에 처음 실패했다. 현재 `D:\TuLED\STOCK GAME\Skyrim Special Edition\SkyrimSE.exe` 및 `D:\TOFU\Stock Game\SkyrimSE.exe`가 과거 캡처와 SHA-256까지 일치하는 것을 확인한 후 경로만 메모리 내에서 치환하여 재실행했다.
- 재실행 도구: `tools/audits/Verify-RelocatedAddonCaptures.py`. 원본 캡처/제품 패턴을 변경하지 않으며 해시가 다른 실행 파일은 거부한다.
- 나머지 10개 버전의 실제 엔진 코드나 현재 인게임 동작을 새로 검증한 것은 아니다.

## 메모리와 성능

### 오프라인 할당 계측

`BodyChangeNGOfflineMemoryProbe.exe`의 7개 그룹을 각각 100회 실행했다. 워밍업 이후 총 **2,988,300회 할당**, 그룹별 종료 시 잔여 증가 **0바이트/0블록**이었다.

| 그룹 | 할당 수 | 순간 추가 요청 바이트 최대 | 종료 시 잔여 증가 |
| --- | ---: | ---: | ---: |
| 얼굴 콜백 | 6,700 | 624 | 0 |
| 얼굴 완료/실패 | 600 | 24 | 0 |
| TXST 생성 | 310,100 | 29,496 | 0 |
| 언로드/재시도 | 0 | 0 | 0 |
| 모델 배열 | 960,000 | 3,992 | 0 |
| 몸/원거리 그래프 | 1,700,000 | 1,496 | 0 |
| 복원 큐 | 10,900 | 632 | 0 |

4,096바이트 양성 대조군도 검출했고 해제 후 복귀했다. 복원 큐의 100만 회 보류 동안 추가 할당이 늘지 않았다.

단, 가짜 VM/엔진 할당자를 사용하며 성공한 장기 보유 그래프는 검사기가 정리한다. 외부 제공자가 참조할 수 있는 공개된 private ARMO/ARMA/TXST의 실제 수명, 엔진/GPU 메모리, VM이 이미 실행 중인 호출의 보유량은 이 결과로 증명할 수 없다. 새 무한 누수는 확인하지 못했지만, 장기 유지 정책의 실제 엔진 보유량은 미측정이다.

### 목록 처리 CPU 표본

합성 데이터·Release·CPU 중앙값이며 실제 프레임률이 아니다.

| 표본 | 전체 복사/처리 | 비교 경로 |
| --- | ---: | ---: |
| 긴 슬라이더명, 프리셋 4,600개 × 100슬라이더 | 깊은 복사 38.8533ms | 메타데이터 복사 0.4167ms |
| 같은 프리셋 4,600행 그리기 | 전체 행 0.8454ms | 화면 행 12개 0.0056ms |
| 스킨팩 1,000개 | 복사+개수 집계 20.6516ms | 공유 목록+캐시 개수 0.0004ms |

현재 UI는 바디 목록에 ListSnapshot, 스킨/후타/틴트에 공유 snapshot, 오버레이에는 카탈로그 세대/액터 호환성 캐시를 사용하며 행을 clip한다. 위 느린 전체 복사 수치는 비교용이며 매 프레임 현재 경로가 그 비용을 낸다는 뜻이 아니다. GPU DDS 업로드 시간도 아니다.

남는 비용/검증 한계:

- 검색/즐겨찾기 필터는 항목 수에 비례하고, Settings snapshot은 즐겨찾기 벡터를 포함한다. 매우 큰 즐겨찾기 집합의 실제 게임 지연은 측정하지 않았다.
- 전역 액터 검색은 상세 처리를 배치로 나누지만 최초 FormID 수집은 전체 폼 맵을 한 번 읽는다.
- 큐의 1/2ms 예산은 작업 사이에서만 적용된다. 이미 시작한 단일 RaceMenu/엔진 호출을 중간에 끊을 수는 없다.
- 파일 카탈로그/텍스처 준비 worker의 병합과 종료 수명은 확인했다. 별도의 기존 단축키 fallback은 프로세스 수명 동안 동작하는 단일 detached thread이며, 반복 입력마다 스레드를 새로 만드는 구조는 아니다.

## 산출물 및 다음 수정 경계

제품 DLL은 검사 전과 동일하다.

- 경로: `build/v1.3.0/windows/x64/release/BodyChangeNG.dll`
- 파일/제품 버전: `1.3.0.0`
- 크기: 2,860,544바이트
- SHA-256: `75276C6AD7369A7D8AC9BF331ABA91A28EA57A09973F0B8F3860211A0FFFF142`

다음 수정은 F1/F2의 **미리보기 복원 수명과 RaceMenu 보류**, F3의 **기본 바디 플레이어 복원 호출 연결**에 한정하는 것이 적절하다. 엔진 폼 해제 범위, 모프 계산, 배포 저장 형식, 지원 런타임을 함께 바꾸지 않아도 되는 결함들이다. 수정 후에는 현재 진단을 정상 결과 기반 회귀 검사로 바꾸고 전체 검사/메모리 대조를 다시 수행해야 한다.
