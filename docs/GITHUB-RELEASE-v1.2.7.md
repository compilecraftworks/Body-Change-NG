# Body Change NG v1.2.7

## English

- Harden distribution condition-list reads against invalid faction/race/class/keyword objects: verify runtime type and current form identity before reading metadata. Read validated full-name data without invoking the unsafe name virtual; retain editor-ID hooks and use the existing ID label fallback if only the name component is unavailable. No faction is excluded by name or ID.
- Reset distribution checkbox mode, sex, candidate selections, and preview-dirty flags consistently when closing/reopening the main window or changing game sessions. Keep candidates intact when entering the condition popup so adding another rule still works.
- Collect faction/race/keyword/class/plugin options on the game task instead of walking engine arrays during UI drawing. Publish an owned, immutable result; reject late results after close/load and avoid per-frame scans or repeated failure retries. Saved rules and the All NPC/name targets do not depend on the options being ready.
- Add UI cleanup at serialization revert and New Game/PostLoadGame boundaries. Discard old actor/camera presentation references without restoring them into a new world, and invalidate queued camera updates from the previous session.
- Recheck cancellation and session validity at body-preview cleanup/body-apply/native-skin mutation boundaries. These are cooperative checks, not a way to interrupt an engine call already in progress.
- Add an explicit Enter-triggered name/hex RefID search for existing actor references, independent of the nearby dropdown's distance/32-NPC limit. Resolve names in bounded batches and discard stale searches after close/load/replacement; searching does not spawn NPCs or load cells.
- Allow validated manual body/skin/futanari/overlay choices to be stored for actors whose 3D is unloaded. Preview still requires loaded 3D. The existing attach queue rechecks compatibility and applies saved selections when the actor loads; saving the game preserves pending choices in the existing co-save format. Unspawned NPC bases require distribution rules rather than an individual RefID selection.
- Validation: Release build and all 28 regression executables passed, including invalid metadata entries, editor-ID hooks, current production reset helpers, cancellation checkpoints, snapshot lifetime, 400 publication/reset races, and pending-selection co-save round trips. Required relocation coverage was checked against the 12 supported SE/AE Address Library databases. The supplied 1.2.6 crash path is now guarded, but confirmation on the reporter's setup and remote selection/attachment still need in-game testing. The source of the invalid object is not established. Existing RaceMenu contracts, saved rules, and co-save format are unchanged.
- Allow hostile as well as non-hostile NPCs in the actor dropdown, including bandits. Keep the 4,096-unit radius, nearest-32 limit, and existing name/NPC/live-3D safety checks. Distribution rule matching is unchanged.

## 한국어

- 배포 조건의 팩션·종족·클래스·키워드 목록에서 실제 런타임 타입과 현재 폼 등록 상태를 확인한 후 정보를 읽습니다. 이름은 검증된 구성요소에서 위험한 가상 함수 호출 없이 읽으며, Editor ID 제공 훅은 유지합니다. 이름 부분만 사용할 수 없으면 기존 ID 표시로 항목을 유지하고 특정 팩션을 이름·ID로 제외하지 않습니다.
- 본체 창 닫기·재열기·게임 세션 전환 때 배포 체크박스 모드·성별·선택 후보·미리보기 갱신 플래그를 함께 초기화합니다. 조건 팝업으로 넘어갈 때는 후보를 보존하여 추가 규칙에 사용할 수 있도록 유지합니다.
- 종족·팩션·키워드·클래스·플러그인 조건 목록은 UI 그리기 중 엔진 배열을 순회하지 않고 게임 작업에서 수집합니다. 문자열·ID를 소유하는 읽기 전용 결과만 전달하고 닫기·로드 뒤 도착한 결과는 폐기합니다. 매 프레임 재수집이나 실패 시 무한 재시도는 하지 않으며, 저장된 규칙·전체 NPC·이름 조건은 목록 준비와 별개로 유지합니다.
- 직렬화 Revert와 새 게임·로드 완료 경계에도 UI 정리를 연결했습니다. 이전 액터·카메라 참조를 새 게임에 복원하지 않고 정리하며 이전 세션의 대기 카메라 갱신도 무효화합니다.
- 바디 미리보기 복원·바디 적용·네이티브 스킨 변경 직전에 취소 및 세션 유효성을 다시 확인합니다. 이미 실행 중인 엔진 호출을 강제로 중단하는 방식은 아닙니다.
- 액터 검색에 이름·16진수 RefID를 입력하고 Enter를 누르면 근거리 드롭다운의 거리·32명 제한 밖에 있는 기존 개별 액터도 찾습니다. 이름 확인 작업은 분할 처리하고 닫기·로드·새 검색 시 이전 검색을 취소합니다. NPC 생성·소환이나 셀 강제 로드는 하지 않습니다.
- 3D 미로드 액터도 호환성 검사를 통과한 바디·스킨·후타스킨·오버레이를 확정하여 선택값을 저장할 수 있습니다. 미리보기는 3D 로드가 필요하며, 저장한 선택은 기존 액터 로드 큐에서 호환성을 재확인한 뒤 적용합니다. 게임을 저장하면 미적용 선택도 기존 코세이브 형식으로 보존합니다. 아직 생성되지 않은 NPC 원본은 개별 RefID 선택 대신 배포 조건을 사용해야 합니다.
- 검증: Release 빌드와 회귀 테스트 28개 통과. 비정상 목록 항목·Editor ID 훅, 현재 제품 초기화 함수, 취소 경계, 스냅샷 메모리 수명, 게시/초기화 경합 400회, 미적용 선택 코세이브 왕복 검사를 포함합니다. 지원 SE/AE 12개 버전의 Address Library에서 필요한 주소 존재 여부를 확인했습니다. 제공된 1.2.6 충돌 경로는 보강했으며 제보 환경의 해결 여부와 원거리 NPC 선택·접근 적용은 인게임 확인이 필요합니다. 객체가 비정상 상태가 된 최초 원인은 확정하지 못했습니다. 기존 RaceMenu 계약·배포 규칙·코세이브 형식은 변경하지 않았습니다.
- 산적을 포함하여 적대·비적대 NPC 모두 액터 드롭다운에 표시할 수 있도록 적대 필터를 제거했습니다. 거리 4,096·가까운 32명 제한과 이름·NPC 판정·생존·활성·3D 로드 조건은 유지합니다. 배포 규칙 판정은 변경하지 않았습니다.
