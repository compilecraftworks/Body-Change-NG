# Body Change NG v1.2.8 — 전체 기능 배포 전 점검

검사일: 2026-09-19. 기준: v1.2.7 `b8e3342046c7ef500710cdcd34b97f49781a295f` 이후 작업과 기존 기능 전체.

## 결과와 검증 수준

- SE/AE Release 빌드 성공. 자동 회귀 실행 파일 **31개**, 오프라인 보조 검사 **19개** 통과.
- 기존 기능을 기능군별로 분류해 제품 코드와 관련 테스트를 확인했다. 수정 파일만 검사한 것이 아니다.
- 정책/직렬화 검사는 실제 게임 통합 검사와 다르다. 파일 저장 검사는 실제 임시 파일을, UI 검사는 실제 ImGui의 비표시 프레임을 사용한다. 실패 주입 검사는 제품 함수와 가짜 VM/엔진 할당자를 연결한다.
- 이번 배포 정리에서는 게임 실행·메모리 접근·MO2 교체·세이브 변경을 하지 않았다. 앞선 엔진 코드 읽기 결과는 별도 보고서를 참조한다.
- 새로 확인한 삭제 준비 확인창의 중첩 Cancel 누락은 수정하고 회귀 검사를 추가했다. 그 외 아래 검사 범위에서 새 실패는 발견하지 못했다. **모든 실제 모드 조합의 무결점·누수 없음 인증은 아니다.**

## 전체 기능별 검증표

| 기능 | 확인한 동작 | 검사와 한계 |
| --- | --- | --- |
| 바디프리셋 카탈로그 | XML 끝점 순서, 체중 보간, 음수·100 초과, 대소문자/슬라이더 이름, 빈 UBE Zeroed, UBE 빌드 기준값 | PresetCatalogTests, UbeMorphTests, BodyFamilyTests. 실제 TRI 변형 결과는 인게임 미검증 |
| 프리셋 미리보기·확정·기본값 | 임시 델타, 다른 모프 보존 ON/OFF, OBody/OClothe 제거, 기본값, 언로드 뒤 미리보기 키 정리, 저장된 선택 복구 | BodyMorphKeyTests 4,511 검사, ActorStateTests, CodePatternTests. 엔진 렌더 호출은 정적 확인 |
| 바디스킨 | CBBE/UNP/UBE/남성 경로, 몸·손·발·얼굴·종족/노인/뱀파이어 레이어, 얼굴 전용→몸 포함, NIF 기반 대상, 공유 ActorBase | AssetCatalogTests, SkinArchitectureTests, FaceSkinPolicyTests, SkinSessionStateTests. DDS 픽셀·목선 여부는 검사하지 않음 |
| 스킨 전환·실패·복원 | 원본 보존, 부분 채널 실패 롤백, 기본값·세션 초기화, private TXST 원복, 콜백 포기, 이전 세대 거부 | FailurePathTests, FaceSkinPolicyTests, TexturePathIsolationTests. VM이 여전히 보관 중인 콜백은 강제 취소하지 않음 |
| 후타·남성 성기 스킨 | 여성 애드온 설치 감지와 액터 등록 구분, ERF/TRX/UBE-TRX 호환, 남성 슬롯, 애드온 변경 재선택, 선택 저장, 텍스처 소유권 | NativeAddonPolicyTests, GenitalPersistenceTests, AssetCatalogTests 및 FutanariSupport/SkinApplication 코드 확인. 실제 TNG/SOS 크기 변경 렌더는 미검증 |
| 틴트마스크 | 플레이어 전용, 성별·UBE/legacy 필터, 팩/레이어, RGBA, 미리보기 색상, 원본 백업, 저장/로드 | AssetCatalogTests의 15레이어 직렬화·잘린 레코드·팩 탐색, PlayerTint/UI 코드 확인. 최종 얼굴 합성은 미검증 |
| 오버레이 | 얼굴/몸/손/발, 수용 슬롯에서 외부 점유 제외, 확정 개수와 미리보기 분리, 누적 체크박스 미리보기·해제, 후보별 색상, 기본값·원본 복원 | OverlayPolicyTests, AssetCatalogTests, GenitalPersistenceTests. 실제 RaceMenu/SlaveTats 노드 생성은 미검증 |
| NPC 배포 후보·미리보기 | 여성/남성, 가까운 적합 NPC/플레이어 대체, 커스텀 팔로워·노인 자동 선택 제외, 후타 여성 제약, 체크박스 후보와 미리보기의 독립성 | ActorStateTests, UiCatalogPolicy/AppearancePreviewState 검사 및 UI 코드 확인. 실제 NPC 이동·카메라 전환은 미검증 |
| NPC 배포 조건 | 전체/이름/개별/팩션/종족 등 범위, 기능별 후보 분리, 수동 선택 우선, 현재 조건 팝업 닫기 시 초안 폐기, 즉시/다음 실행 명시 저장 | ActorStateTests, DistributionLifecycleTests, DistributionTargetReadTests 및 Distribution/UI 연결 확인 |
| 배포 파일 저장 | 11개 범위 × 성별 2 × 기능 4 = 88개 행, 안정적인 플러그인+로컬 ID, 네 부위 RGBA, 빈 규칙, 파일 잠금·임시 파일 쓰기 실패·재시도 | 새 DistributionPersistenceTests가 **제품 WriteDistributionFile 함수 그대로** 실행. 기존 파일 보존 확인. 엔진 대상 재해석은 별도 코드 검사 |
| 액터 목록·원거리 검색 | 적대/비적대 동일 취급, 근거리 4,096/32명 제한, Enter 이름/RefID 검색, 미로드 선택 보존, 늦은 검색 폐기 | ActorSearchPolicy/ActorStateTests, ActorCatalog 코드 확인. 월드 스폰·강제 셀 로드는 하지 않음 |
| 가슴·유두보정/ORefit | 바디 계열별 보정, 보존 옵션, 옵션 해제 시 기존 보정 제거, 명시 JSON 등록, 이름/FormID/플러그인 강제·제외 규칙 | BodyMorphKeyTests, OutfitRefitRulesTests 및 OutfitRefit/RaceMenuBodyMorph 코드 확인 |
| SFS 연동 | API 없음/미지원 시 실제 장비, API 준비 중 보류, 최종 표시 외형의 원본 ID, 모두 숨김 시 해제, 오래된 스냅샷 거부, 변경 알림 중복 억제 | RenderedOutfitTests 436 검사. 실제 SFS DLL·씬 알림의 인게임 연동은 미검증 |
| 랜덤화 | NPC 대상·지원 바디 계열 분기, 안정적인 액터/프리셋 시드, 옵션/기본값 복원 시 모프 키 처리 | RaceMenuBodyMorph 생성 경로 정적 확인과 기존 모프 정책 검사. 모든 랜덤 슬라이더의 시각적 결과를 자동 검사한 것은 아님 |
| 세이브·로드·새 게임 | ASTR 기존 버전 읽기, v6 분할 저장, 16,384 초과·문자열 상한, 얼굴 소유권·틴트·오버레이 색상, 미적용 선택, 세션 무효화 | ActorStateTests, FaceSkinPolicyTests, GenitalPersistenceTests, DistributionLifecycleTests. 실제 ESS/SKSE 동시 저장·재로드는 미검증 |
| 전체/선택 초기화·삭제 준비 | 모든 기능의 Default 의도, 공유 몸 한 번·얼굴 각각 복원, 실패 시 복원 정보 유지, 다른 모드 값 보존, 자동 작업 중단·재개 | RemovalPreparationTests, FailurePathTests, ActorStateTests 및 RemovalPreparation/ActorSettingsReset 호출 경로 확인. 삭제 후 실제 세이브 상태는 미검증 |
| 입력 | F7/복합키, 게임 Activate/Cancel 매핑, 텍스트 입력 예외, 누름/해제 소유권, 저프레임 마우스·휠·드래그·더블클릭 | HotkeyTests, MouseInputReplayTests 60/15/5 FPS. NativeImGuiHost 입력 차단 경로 정적 확인 |
| 창·팝업·카메라 | 9개 팝업 위치 기억·중앙 시작·축소 화면 보정, 중첩 Cancel, 클리퍼/체크박스 정렬, 한 줄 안내/타이틀 힌트, 카메라 복원 | PopupPlacementTests의 실제 ImGui 프레임, RuntimeLayoutTests 투영 검사 및 Presentation 복원 경로 확인. 게임 카메라 애니메이션은 미검증 |
| 설정·즐겨찾기·언어 | 좌측/일시정지 OFF/성능 OFF/모프 보존 ON 기본값, 즐겨찾기 UTF-8, 옵션/팝업 위치 저장, 기존 설정 이관 | AssetCatalogTests, HotkeyTests, PathMigrationTests, ActorStateTests의 이름 번역표 검사. 모든 번역의 시각적 줄바꿈은 미검증 |
| 작업 큐·성능·수명 | 작업 합치기·세션/요청 취소·상호 배제, 초기 지연, 미리보기 우선, 새로고침 폭주, 스냅샷 회수 | FrameTaskQueueTests, AsyncWorkGuardTests, CatalogRefreshTests 10,000회, DistributionLifecycleTests 게시/초기화 경합 400회 |
| 런타임/RaceMenu ABI | 명시 SE/AE 12개, 알려진 BodyMorph v4/v5 및 Override/Overlay 어댑터, 미지원 런타임 거부, 버전별 레이아웃/패턴 | RuntimeLayoutTests, CodePatternTests, FormDeletePolicyTests. 12개 Address Library의 배포 메타데이터 11개 주소 각각 확인. 전 버전 실게임 검증 아님 |

## 이번 추가 확인·수정

`DrawRemovalControls`의 확인창은 부모 설정창이나 삭제 준비 본체보다 나중에 그려졌다. 부모가 먼저 BCNG의 일회성 Cancel 입력을 소비할 수 있었다. 현재 팝업 깊이에 자식 팝업이 열려 있으면 부모는 Cancel을 소비하지 않게 했으며 확인창은 공통 위치 관리에 연결했다. 기존 팝업 키 순서는 유지하고 새 키만 마지막에 추가했다.

비표시 ImGui 테스트에서 본체→확인창과 본체→설정→확인창을 각각 열고 닫아, 가장 안쪽만 Cancel을 처리할 수 있는지 확인했다. 팝업 위치 설정의 실제 파일 왕복 검사에도 새 키가 포함된다.

배포 조건 저장은 기존 정책 검사만으로 파일 교체 실패를 증명하지 못하므로 별도 실행 파일을 추가했다. 테스트의 진단 로그만 대체했으며 제품 저장 함수와 Windows 파일 교체는 실제 구현이다. 기존 JSON을 열어 교체를 막은 상태와 `.new` 위치에 디렉터리가 있는 상태 모두 실패를 반환하고 기존 JSON을 보존했다.

## 메모리·성능 결과

- 실패 경로 할당 검사: 6개 그룹 × 100회, 총 2,977,400회 호스트 C++ 할당. 그룹 종료 시 추적된 잔여 바이트/블록은 0. 양성 대조군은 4,096바이트 보유를 정상 검출했다.
- 콜백 인프라 증분은 검사 모델에서 동시 생존 콜백당 176바이트. 그래프 생성 가드 본체 48바이트, 배열 개수 헤더 8바이트. 엔진/GPU 메모리와 힙 관리 비용은 포함하지 않는다.
- **성공한 장기 유지 가짜 그래프는 테스트가 직접 정리한다.** 제품이 이미 공개한 모든 그래프를 즉시 회수한다는 뜻이 아니다. TNG 등의 raw-pointer 참조 때문에 유지하는 그래프는 보존되며 원본 교체 반복 시 증가 가능성이 남는다.
- 합성 1,000팩 메타데이터: 기존 스킨 복사+경로 집계 약 15.16 ms, 공유 카탈로그의 캐시된 개수 순회 약 0.0004 ms. 실제 게임 FPS 비교가 아니다.
- 합성 4,600행 목록은 12개 행을 그렸으며 화면 밖 포커스 행도 별도 클리퍼 테스트로 확인했다. DDS 이미지·GPU 캐시를 매 프레임 복사한 테스트가 아니다.
- 정상 팩 전환은 기존 그래프를 재사용한다. 실패한 비공개 생성물은 정리하되, 독립 저장소가 검증되지 않은 비정상 복제물을 무조건 파괴하지 않는다.

## 남는 검증 경계

1. 제보된 TNG 크기 변경 후 반보라색과 간헐적 스킨 깜빡임은 이 배포 과정에서 재현하지 않았다. 관련 복원·실패 처리 보강을 모든 원인의 해결로 표현하지 않는다.
2. 실제 세이브 로드·삭제 준비 후 모드 제거·RaceMenu/SOS/TNG 버전 조합은 인게임 미검증이다.
3. VM이 콜백을 해제하지 않고 계속 보관하는 경우 임의로 작업을 완료시키지 않는다. 삭제 준비는 미완료로 남을 수 있다.
4. 피부를 재주입하는 RSV/Selector 및 중복 외형 제어 OBody NG와의 기존 비호환 안내는 유지한다.
5. 테스트는 알려진 조건을 검증한다. 외부 모드의 임의 메모리 변경, 깨진 DDS/TRI/NIF, 미래 ABI를 보장하지 않는다.
6. ASTR 액터 선택 분할 저장과 얼굴 복원 표의 안전 상한은 별개다. 얼굴 표는 기존 16,384개 제한에서 새 항목 수락을 거부하며 복원 정보를 무한히 할당하지 않는다. 이번 변경이 모든 기능의 상한을 제거한 것은 아니다.

## 재실행과 배포 구성

- 저장소 고정 xmake 3.1.0, CommonLibSSE-NG 6.7.1 (`70c1acd5261210982bd52f6d4468a082fe04d798`), 기존 잠금 파일 유지. 의존성 업그레이드 없음.
- 최종 DLL: 파일 버전 `1.2.8.0`, 2,855,424바이트, SHA-256 `26392044CF7EB914FC993C833EE9EE20AF6F86ACB7E20DB19081AF44222C6FA3`. 내보내기는 SKSE의 Load/Query/Version 3개이며 디버그 CRT·테스트 DLL 의존성 없음. `skyrim_vr=false` 유지.
- `xmake build -a` 뒤 `build/v1.2.8/tests/*Tests.exe` 31개를 각각 실행. 성능 probe는 `--appearance-audit` 포함 두 모드, 메모리 probe는 `build/v1.2.8/trials/offline-memory/`에서 별도 실행한다.
- `tools/test-addon-audits.py`: 게임 접근 없는 19개 보조 검사.
- `tools/audit-distribution-target-relocations.py <Address Library 디렉터리>`: 오프라인 12개 버전 메타데이터 주소 검사.
- `scripts/Package-Release.ps1`: 커밋된 소스만 패키징. 릴리즈 ZIP은 DLL·빈 배포 규칙·3개 에셋 폴더 안내·2개 라이선스 문서의 **7개 파일**로 제한한다. PDB·테스트 DLL·진단 로그·엔진 캡처는 배포하지 않는다.
- 소스 ZIP은 프로젝트·테스트·문서·고정 의존성의 빌드 필수 소스 및 소스 커밋 정보 포함. 두 ZIP 내부 파일은 SHA-256으로 원본과 대조한다.

관련 근거: [전체 최초 점검](FULL-AUDIT-20260919-KO.md), [A–F 수정](FULL-AUDIT-FIXES-20260919-KO.md), [G/H 후속 검증](NATIVE-LIFETIME-FOLLOWUP-20260919-KO.md), [오프라인 메모리 범위](OFFLINE-MEMORY-COST-20260919-KO.md), [삭제 준비·크기 변경](REMOVAL-AND-ADDON-SIZE-AUDIT-20260919.md).
