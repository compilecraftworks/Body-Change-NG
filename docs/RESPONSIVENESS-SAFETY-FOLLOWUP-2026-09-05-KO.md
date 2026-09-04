# 직접 선택 반응성·콜백 수명 후속 수정 — 2026-09-05

## 기준과 범위

- 사용자 승인 후 `d109c56`을 기준으로 수정했다. 브랜치: `compilecraftworks/lifecycle-safety`, 버전: 1.1.0 유지.
- 목적은 직접 바디/스킨 선택이 불필요하게 기다리는 경로를 줄이는 것이다. 액터별 중복 실행 금지, 로드 취소, 원본 보존 경계는 유지한다.
- xmake 3.1.0, CommonLibSSE-NG 6.7.1 및 의존성 잠금을 변경하지 않았다.
- 아래 검증은 엔진 없는 정책/카탈로그 테스트와 DLL 빌드이다. 제보자 환경의 OverlayFix CTD나 실제 클릭 지연을 재현한 결과는 아니다.

## 1. 직접 선택의 처리 기회

- 기존 urgent와 별개로 interactive 표시를 둔다. 게임 태스크 밖에서 요청한 바디/스킨/틴트 적용 채널과 그 후속 작업이 이를 전달한다.
- Pump의 첫 작업 선택에 직접 입력 처리 기회를 둔다. 새로운 별도 실행 루프나 프레임 예산 초과 실행은 추가하지 않았다.
- 연속 세 배치까지 입력 우선 기회를 주고 네 번째에는 기존 aging 정책을 사용한다. 비싼 native 한 번이 예산을 다 쓰더라도 오래 기다린 자동 작업이 계속 굶지 않도록 한다.
- 같은 액터에서는 기존 FIFO를 유지한다. 앞선 commit 뒤의 preview를 먼저 실행하지 않고, 필요한 앞선 작업을 우선 처리한다.
- 자동 스킨 작업이 이미 시작된 뒤 사용자가 선택하면 그 액터의 활성 lease에 우선권을 전달한다. VM 스레드에서도 읽으므로 atomic으로 관리한다.
- 이미 예약된 후속 작업도 동일 lease를 참조하여 새 우선권을 볼 수 있다. 다른 액터의 자동 작업 전체를 승격하지 않는다.
- continuation은 기존 lease를 유지하는 actor=0/channel=0 작업으로만 허용한다. 같은 액터의 작업권을 다시 얻으려 기다리는 자기 교착을 방지한다.

## 2. 콜백 객체 수명과 실제 작업 수명 분리

기존 `LegacyOverrideCallback`, `LegacyOwnershipQueryCallback`, `NodeUpdateCallback`은 호출이 끝난 뒤에도 VM이 객체를 보관하면 객체 멤버의 batch/lease가 남아 액터 busy 상태를 연장할 수 있었다.

- `CompletionPayload<T>`가 콜백 호출 시 payload를 정확히 한 번 꺼내 준다. 객체가 남아 있어도 완료한 콜백은 더 이상 batch/lease를 보유하지 않는다.
- 아직 필요한 payload는 로컬 완료 처리 또는 실제 후속 작업으로 넘긴다. 후속 작업이 끝나기 전에 lease를 해제하지 않는다.
- Override 및 ownership-query 콜백은 호출되지 않고 폐기된 경우의 완료 카운터 정리도 유지했다. 호출 뒤 소멸, 중복 호출, 동시 호출에서는 중복 완료하지 않는다.
- NodeUpdate의 후속 안전 대기와 일반 lease 해제 후 추가 업데이트 경계는 그대로 유지했다. 시간만 지났다는 이유로 외부 갱신을 완료 처리하지 않는다.
- 취소/로드 시 무효화된 lease를 정상 작업으로 되살리지 않는다. continuation을 실행하는 Pump가 예외 발생 시에도 TLS 소유권/우선권을 복원한다.

## 3. 연속 선택의 오래된 단계 생략

- 같은 채널의 대기 요청을 최신 선택으로 합치는 기존 정책을 유지했다. 예를 들어 스킨 100회 선택은 대기 중 마지막 스킨 하나로 합쳐진다.
- 스킨 batch에 생성 시 actor FormID를 기록해 VM 콜백에서 엔진 객체를 역참조하지 않고 잠금으로 보호된 세대 번호를 검사한다.
- 오래된 ownership-query 결과와 이미 대체된 batch의 완료에서는 불필요한 다음 게임 태스크를 등록하지 않는다.
- 이미 VM에 전달한 native 호출은 강제 취소하지 않는다. 모든 진행 중 콜백이 끝나야 새 스킨/바디 작업이 같은 액터에서 시작할 수 있다.
- 실제 native 호출 전의 ActorHandle/VM identity/session/성별/선택 세대 재검증은 유지했다.

## 4. UI 상태와 비용

- 기존 탭 안내문 한 줄을 대기/적용 중 상태에 재사용한다. 새 줄, 팝업, 목록 높이 변화를 추가하지 않았다.
- 선택 액터 작업이 1.5초 이상 남아 있으면 갱신 지연 안내로 바뀐다. 이는 실패 판정이나 강제 타임아웃이 아니다.
- 툴팁은 목록의 선택이 요청한 값이며, 이전 갱신 완료 후 최신 요청을 처리한다는 점을 설명한다. 한국어/영어/중국어 간체를 함께 반영했다.
- 큐가 비면 원래 안내문을 복원한다. 큐가 비었다는 이유만으로 외형 적용 성공이나 외부 지오메트리 완료를 표시하지 않는다.
- 상태 조회는 선택 액터의 pending/busy map 두 개만 조회한다. DDS/XML 검사, 전체 NPC 탐색, 새 로그 반복 출력은 넣지 않았다.
- 일반 모드 2ms/액터 작업 4개, 성능 모드 1ms/2개, 전체 후속 작업 최대 64개라는 기존 배치 예산을 유지했다. 분할할 수 없는 native 호출의 실제 시간은 이 예산보다 길 수 있다.

## 5. 검증

- Release DLL 빌드 성공. 새 빌드 경고 없음.
- 12종 재빌드/실행 통과: ActorState, AsyncWorkGuard, FrameTaskQueue, AssetCatalog, BodyFamily, Hotkey, OutfitRefitRules, PathMigration, PresetCatalog, RaceMenuPresetMigration, RuntimeLayout, SkinOverrideOwnership.
- 추가 정책 테스트: 입력 우선권과 aged bulk 공정성, 같은 액터 prerequisite/FIFO/due 경계, 자동 작업 진행 중 직접 입력 우선권 전달, 기존 lease를 가진 continuation, 100회 연속 선택의 마지막 결과.
- 추가 수명 테스트: 완료한 콜백 객체 보관, 실제 후속 작업의 lease 유지, 여러 native 콜백 중 일부만 완료, 마지막 콜백 뒤 다음 선택 재개, 취소 후 payload 이전, 중복/16개 스레드 동시 완료, 폐기 시 자원 정리.
- 상태 테스트: queued/busy/종료/로드 초기화, 1.5초 표시 임계값. 인게임 UI 렌더링 검증을 했다는 뜻은 아니다.
- 테스트 임시 루트: `C:\Users\yunha\Desktop\Obody NPC\bcng-responsiveness-final-c7614c3833b64efc8e9f81e0e7f9b5bc`.
- 변경 없음 확인: 카메라 구현, 바디 모프 계산, SkinGeometryRouting, SkinProfiles 부위/채널 분류, 배포 조건 평가, ActorRegistry 저장 스키마, RuntimeLayout, xmake 설정/의존성 잠금.

DLL: `build/v1.1.0/windows/x64/release/BodyChangeNG.dll`

SHA256: `44A826DD462EA9FEF98E01C3EC7C4F5336EB1BD7C8532CE4ED46D166960E8BCC`

크기: 2,309,120 bytes.

## 남은 한계와 배포

- VM이 아직 완료하지 않은 호출을 영구 보관하는 문제 자체는 강제 타임아웃으로 해결하지 않았다. 그런 경우 같은 액터의 다음 작업은 안전하게 대기하며 UI는 지연을 알린다.
- 완료한 콜백 객체가 나중까지 남는 경우와, 호출 자체가 끝나지 않은 경우를 구분한다. 이번 수정은 전자의 불필요한 잠김을 제거한다.
- 실제 엔진/OverlayFix 작업 완료 신호를 새로 확보한 것은 아니다. 고정 업데이트 간격이 다른 플러그인의 작업 완료를 보장하지 않는다.
- 인게임 반응 시간·외형 결과·CTD 해결·회귀 0을 확정하지 않는다. 자동 검증을 통과한 변경이다.
- MO2 설치본, GitHub, Nexus, Release/Source ZIP은 이번 작업에서 갱신하지 않았다.
