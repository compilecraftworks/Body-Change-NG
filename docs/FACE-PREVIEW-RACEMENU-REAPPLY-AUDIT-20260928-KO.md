# UBE 플레이어 스킨 미리보기 얼굴색 차이 조사 — 2026-09-28

## 대상과 범위

- 사용자 확인 환경: 툴레드, Skyrim SE 1.5.97, UBE 플레이어, BCNG 1.3.4.
- 미리보기에서 얼굴이 밝아지고 목선이 나타나지만 같은 스킨을 확정하면 정상.
- 최초 보고의 '잠깐 뒤'는 수 초 지연을 뜻하지 않는다. 사용자는 거의 즉시,
  또는 몸이 바뀌는 순간부터 나타나는 경우도 많다고 추가 확인했다.
- 사용자는 skee64backports 비활성 비교를 별도로 진행 중이다. 그 결과는
  이 조사 시점에 아직 전달받지 않았다.
- 최초 조사 단계에서는 제품 코드, 설치 DLL, 모드 설정, 세이브를 변경하지 않았다.
  실행 중 게임 메모리 접근도 하지 않았다.

## 확인한 실제 호출 경로

### 1. 설치된 RaceMenu 자체가 3D 갱신 뒤 저장값을 다시 적용한다

읽은 파일:

`D:\TuLED\File Mod Skyrim SE\mods\Race Menu\RaceMenu.bsa`

- MO2 메타데이터 버전: 0.4.16.0.
- BSA SHA-256: `FCC46F42731D2B7C3782B96CFF40930FFCF689482CA8E9A5568CF2B76E9A7603`.
- BSA 내부 `scripts/source/racemenu.psc` 226–230행:
  `OnNiNodeUpdate`에서 대상 액터가 일치하면 `InvalidateShaders` 호출.
- 같은 파일 552–558행: SKEE가 있으면 `ApplyOverrides`,
  `ApplyNodeOverrides`, `ApplySkinOverrides`를 순서대로 호출.
- 초기 대상은 플레이어다. 이 이벤트 처리에는 RaceMenu 창이 열려 있어야
  한다는 조건이 없다.
- 동봉된 실행용 `scripts/racemenu.pex`도 끝까지 파싱했다.
  `OnNiNodeUpdate`의 CALLMETHOD `InvalidateShaders`, 해당 함수의
  세 CALLSTATIC 명령이 위 소스와 일치한다. 문자열 존재만으로 판단하지 않았다.
- 설치 모드 폴더 검색에서는 정확한 `racemenu.pex` loose 대체 파일을
  발견하지 못했다. 모든 다른 BSA 내부의 중복 스크립트까지 조사한 것은 아니다.

이것은 RaceMenu Selector나 RSV의 코드가 아니라 기본 RaceMenu의 코드다.
따라서 이 호출 경로가 존재하는 데 skee64backports는 필요하지 않다.
그 모드를 끄면 반드시 해결된다거나 반드시 그대로라는 뜻은 아니다.

공식 C++ 구현도 대조했다(조회만 수행, 의존성 변경 없음).

- 커밋 `87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc`의
  [PapyrusNiOverride.cpp](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/PapyrusNiOverride.cpp):
  Papyrus `ApplyNodeOverrides`가 `SetHandleNodeProperties(handle, false)` 호출.
- 같은 커밋의
  [OverrideInterface.cpp](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/OverrideInterface.cpp):
  저장된 노드 키를 열거하여 현재 노드에 셰이더 값을 적용.
- 같은 커밋의
  [ShaderUtilities.cpp](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/ShaderUtilities.cpp):
  비즉시 텍스처 쓰기는 SKSE 작업으로 예약되며, 즉시 쓰기와 예약 쓰기 모두
  텍스처 교체 뒤 `InitializeShader` 실행.

### 2. 수정 전 BCNG 미리보기와 확정의 차이

- `UI.cpp` 2030/2051행: 각각 preview/commit으로 같은 팩을 요청한다.
- `SkinTransactionPolicy.h`의 `PersistsFace`: 플레이어 commit/restore만
  RaceMenu 저장 키를 갱신한다. preview는 현재 화면만 바꾼다.
- `FaceSkinNodeAccess.cpp`의 legacy-v1 경로: 두 모드 모두
  `SetNodeProperty(..., true)`로 현재 얼굴에 쓰지만 preview에서는
  `AddNodeOverride`를 실행하지 않는다.
- 같은 스킨을 확정할 때 이미 맞는 몸을 다시 생성하지 않아도 얼굴 요청의
  모드가 달라지므로 얼굴 처리는 다시 수행한다.
- `CanKeepVisibleTexture`는 저장 키가 다르면 플레이어 확정 쓰기를
  생략하지 않는다. 이 쓰기에는 RaceMenu의 셰이더 초기화도 포함된다.

그러므로 '확정하면 정상'만으로 최초 미리보기 DDS 로드가 실패했다거나,
새 오버레이가 한 겹 추가됐다고 결론 내릴 수 없다.

### 3. 완료 뒤 변경을 놓칠 수 있는 조건

- BCNG는 native 재생성 반환 뒤 준비 상태를 확인하고 얼굴 배치를 실행한다.
- 배치가 완료되면 Request::complete가 true가 된다.
- 동일 재생성 이벤트에서 예약된 BCNG 작업이 뒤늦게 실행되면 `Pump`는
  complete를 보고 생략할 수 있다. 그 사이 RaceMenu Papyrus/예약 작업이
  저장 값을 다시 쓰더라도 그 쓰기 자체는 새 NiNodeUpdate가 아니다.
- `Matches`도 complete/프로필/모드를 확인할 뿐 현재 얼굴을 다시 읽지 않는다.
- 반대로 후속 변경 **이후에 새로운** NiNodeUpdate가 오면 complete가
  무효화되므로 재적용할 수 있다. 모든 재생성이 실패한다는 뜻은 아니다.

## 오프라인 재현

진단 전용 파일(배포 대상 아님):

- `build/diagnostics/face-preview-order-20260928.cpp`
- `build/diagnostics/read-racemenu-bsa-20260928.py`

전자는 실제 `FaceSkinPolicy.h`/`SkinTransactionPolicy.h`를 포함하여
설치된 MSVC 19.51 계열로 빌드·실행했고 다음 조건을 확인했다.

1. 배치 완료 후 다른 쓰기를 주입하면 기존 완료 상태의 Pump는 생략한다.
2. 그 변경 이후 새 이벤트로 완료 상태를 무효화하면 다시 적용할 수 있다.
3. 현재 경로와 GPU 리소스가 맞지만 저장 키가 다르면 preview는 쓰기를
   생략하고 플레이어 commit은 다시 쓴다.
4. 저장 키도 이미 같으면 commit 역시 생략할 수 있다. NPC commit은
   플레이어의 저장 키 승격 조건과 다르므로 결과를 일반화하지 않는다.

이는 처리 순서와 정책의 재현이며 Skyrim 렌더링/화면 밝기의 재현은 아니다.
후자는 BSA/LZ4/PEX를 읽기만 하며 원본이나 Data 폴더에 추출하지 않는다.

## 현재 결론과 남은 구분

**기본 RaceMenu의 후속 재적용과 BCNG의 임시 얼굴 적용이 충돌할 수 있는
구체적인 경로는 확인했다.** 최초 적용이 정상이고 확정에서는 정상이라는
플레이어 보고와 부합한다. 수 초의 고정 지연을 가정할 필요도 없다.

다만 해당 게임 순간의 저장 키, 얼굴 DDS, 틴트/재질 값 전후를 확보한 것은
아니므로 '실제 밝기 변화가 어느 채널의 어느 쓰기로 발생했다'까지 확정하지
않는다. skee64backports의 단독 결함이나 UBE 스킨 파일 손상으로도 단정하지
않는다. 밝아지는 현상만으로 새 오버레이 생성 여부를 판정하지 않는다.

수정 검토 시에는 미리보기의 취소·저장 분리를 유지해야 한다. preview를
그냥 영구 저장으로 바꾸거나 타 모드의 얼굴 키를 일괄 삭제하면 안 된다.
고정 수 초 대기 또는 무제한 덮어쓰기도 이 조사로 정당화되지 않는다.

## 1차 수정 — 사용자 요청에 따른 미리보기 복구

- 얼굴 배치가 성공한 뒤에도 UI가 소유한 미리보기 액터 한 명의 실제 얼굴을
  두 입력 틱마다 확인한다. 이미 준비되어 적용된 얼굴 DDS 채널만 대상이다.
- 저장 키를 임시 스킨으로 바꾸거나 제거하지 않는다. 기본값 미리보기도
  마지막으로 정상 적용된 원래 얼굴 경로를 관찰한다. 확정·복원·세이브의
  기존 영구 저장 정책 및 직렬화 형식은 바꾸지 않았다.
- 실제 텍스처·셰이더 경로가 바뀌었을 때만 기존 얼굴 트랜잭션으로 복구한다.
  이미 일치하는 채널은 기존 즉시 API 경로에서 다시 로드하지 않는다.
  몸 재생성, 전체 NPC 스캔, 별도 스레드 또는 매번 Papyrus 조회는 추가하지 않았다.
- 고정 시간 뒤 관찰을 끝내지 않으므로 늦은 Papyrus 재적용도 관찰할 수 있다.
  단, 같은 미리보기 세대에서 복구는 최대 8회다. 다른 모드가 지속적으로
  덮는 경우 중단하여 무한 경쟁을 방지한다. 이는 RSV/Selector 호환 패치가 아니다.
- 취소·창 닫기·액터 변경·새 선택·세션 리셋·액터 해제 후 오래된 작업은 쓰지 않는다.
  RaceMenu 편집, 엔진 재생성 및 다른 외형 작업 중에는 대기한다.
- 보관하는 것은 단일 관찰 상태의 ID/문자열뿐이며 엔진 메시·텍스처 참조를
  보관하지 않는다. 정상 상태에서는 읽기만 하며 파일 I/O가 없다.

### 추가 자동 검사

`FacePreviewReplayTests.cpp`는 제품의 관찰/예약 함수 본문을 직접 컴파일하고
게임·큐 경계만 대체한다. 즉시 적용 이후 10,000회의 확인을 거친 뒤의 재적용,
다섯 채널의 개별 지연 재적용, 기본값 미리보기, 저장 키 유지, 취소·확정·복원,
액터 전환, 로드 경계, 재생성 대기, 새 선택과 오래된 예약 작업의 교차,
읽기/예약/복구 실패 및 반복 덮어쓰기 제한을 검사한다. 10,000회 복구·닫기
반복에서도 예약 작업과 관찰 경로가 남지 않는지 확인한다.

이 변경은 확인한 DDS 재적용 경로의 복구다. 실행 중 셰이더/틴트/법선의
모든 변화를 가로채는 기능은 아니며, 해당 툴레드 화면 증상의 소실 여부는
새 DLL로 인게임 재확인이 필요하다. 이 작업은 설치된 모드나 세이브를 바꾸지 않는다.

### 실행 결과

- 현재 SE/AE 전용 `releasedbg` DLL 빌드 성공. 버전은 1.3.4 유지.
- 기존 검사와 새 검사를 포함한 회귀 실행 파일 41개 통과. 이후 최종 수정한
  얼굴 미리보기 검사와 실패 경로 검사는 다시 빌드하여 통과했다.
- 관찰 구조체는 MSVC x64에서 232바이트 + 최대 6개 문자열의 내용이다.
  10,000회 복구·닫기 반복에서 관찰 경로/예약 작업이 모두 정리됐다.
  이는 오프라인 상태/수명 검증이며 Skyrim 전체 메모리 누수 측정은 아니다.
- 최종 DLL SHA-256:
  `03649E9A8E0FE145A48750E2CEEF7E5FF06AF5410CFD565B4333A924F7561469`.
- MO2 설치본, 릴리즈 ZIP 및 GitHub는 이 요청에서 교체하지 않았다.

### 1차 수정본의 후속 설치

이후 별도 사용자 요청으로 위 해시의 DLL을 툴레드 MO2의 Body Change NG에
교체했다. 이전 DLL은 프로젝트 `build/mo2-backups` 안에 보관했다.

## 2차 수정 — RaceMenu 키까지 임시 적용하고 복원

사용자는 1차 수정 이후 미리보기에서도 확정 때처럼 RaceMenu 저장 키를
맞추되, 취소하면 원래 값으로 돌아가는 구조를 요청했다. 이 절이 현재 코드다.

- 플레이어 얼굴 미리보기는 현재 DDS 적용 후 같은 경로를 RaceMenu의
  노드 저장 키에도 임시로 넣는다. UBE 전용 분기가 아니며 3BA에도 적용된다.
- 기존 NPC 확정 정책은 유지한다. NPC에 새 영구 노드 키를 만들지 않으며,
  몸·손·발의 네이티브 Skin Armor/텍스처 교체와 바디 모프 계산도 변경하지 않는다.
- 원래 키의 존재 여부와 값을 최대 5개만 보관한다. RaceMenu v1 문자열은
  자체 intern 객체도 함께 유지하여 복원 뒤 직렬화에서 유효한 값이 되게 한다.
  엔진 메시나 GPU 텍스처 객체를 보관하지 않는다.
- 새 얼굴 배치 전에 임시 키만 원복한다. 이때 DDS를 다시 로드하지 않는다.
  따라서 baseline은 항상 확정된 원래 키를 읽으며, 미리보기 A→B→C,
  기본값 미리보기, 동일 스킨 확정에서 임시 키가 원본으로 굳지 않는다.
- 취소·액터 변경·세션 초기화·RaceMenu 열기·배치 실패에서 키를 되돌린다.
  복원 시 다른 모드가 나중에 쓴 값이나 삭제한 키는 덮어쓰지 않는다.
- 확정 시 임시 키를 먼저 원복한 뒤 기존 commit 경로로 저장한다. BCNG의
  액터 저장 형식, 선택 기록 및 얼굴 baseline 직렬화 형식은 그대로다.
- 1차 관찰/최대 8회 복구는 이미 예약돼 있던 예전 RaceMenu DDS 작업에 대한
  보조 처리로 남긴다. 정상 상태에서는 저장 키를 반복 읽거나 쓰지 않는다.

### 저장 순서 보호

SKSE의 [2.0.20 SaveGame_Hook](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/Hooks_SaveLoad.cpp)과
[2.2.6 SaveGame_Hook](https://github.com/ianpatt/skse64/blob/v2.2.6/skse64/Hooks_SaveLoad.cpp)은
실제 저장 처리 전에 `kMessage_SaveGame`을 보낸다. 개별 플러그인의 save callback은
로드 순서에 좌우되므로 BCNG의 callback만을 임시 키 복원 경계로 사용하면 안 된다.

- `kSaveGame`에서 동기적으로 임시 키를 복원하고 새 임시 키 게시를 막는다.
- BCNG 직렬화 callback을 관찰한 뒤에도, 게임의 saving/loading 플래그 또는
  저장 작업 스레드의 busy 상태가 남아 있으면 게시를 재개하지 않는다.
- 저장 worker 접근은 고정 오프셋이 아닌 pinned CommonLib 6.7.1의
  `GetRuntimeData()`를 사용한다. AE 1.6.1130 경계 전후의 0x2B0/0x2F8을
  실제 로드 버전에 따라 선택한다. 지원 외 런타임에서는 재개하지 않는다.
- callback까지 도달하지 못한 저장 실패는 세션 초기화/후속 정상 저장 전까지
  live-only 미리보기를 유지한다. 시간만 지났다고 임시 키를 다시 기록하지 않는다.

### 추가 검증 범위

`FacePreviewTransactionTests.cpp`는 제품의 임시 키 관리자와 saved-key 함수
본문을 그대로 컴파일하고 RaceMenu·게임 경계만 대체한다. v1/v2, 원래 키 없음과
빈 문자열의 구분, intern 객체 복원, 기본값/전환/확정/취소, 외부 키 소유권,
5채널 중간의 저장, 두 플러그인 저장 순서, 헤드 변경과 액터 수명을 검사한다.
v1/v2 합계 20,000회 journal 생성·복원 후 잔여 상태가 없는지도 확인한다.
upstream의 versioned accessor/macro 본문 자체를 이용해 SE 1.5.97과
AE 1.6.659/1130/1170/1179의 저장 worker 접근 위치도 검사한다.

이 검사는 키·순서·수명 검증이다. 실제 얼굴의 밝기/목선이 사라지는지는 새 DLL로
게임 확인이 필요하며, 틴트 채널이나 다른 스킨 변경 모드까지 호환시키는 변경은 아니다.
2차 수정본은 아래 후속 요청에 따라 MO2에 교체했다. 릴리즈 ZIP/GitHub에는
이 작업으로 반영하지 않았다.

### 2차 실행 결과

- SE/AE 전용 `releasedbg` DLL 빌드 성공, 버전 1.3.4 유지.
- 회귀 테스트 대상 42개를 모두 다시 빌드하고 실행해 통과했다.
  마지막 수정이 들어간 트랜잭션/카탈로그/코드 경계 검사는 추가로 재빌드했다.
- `git diff --check` 통과.
- DLL SHA-256:
  `354009D11CF839B04A2755D955F4C4ADDA59361683B46D284150D8E26B9494C8`.

### 2차 수정본의 MO2 반영

- 2026-09-28 13:43 KST, 사용자 요청으로 툴레드 MO2의
  `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`만 교체했다.
- 교체 직전 Skyrim 실행 프로세스가 없음을 재확인했고 설치 DLL의 해시가
  위 빌드 해시와 일치함을 확인했다. 버전은 1.3.4.0이다.
- 이전 DLL(1차 수정본)의 백업:
  `build/mo2-backups/TuLED-face-preview-transaction-1.3.4-20260928-134304/BodyChangeNG.dll`.
- 설정·배포 규칙·스킨 파일·세이브는 변경하지 않았다. 임시 교체 파일도 남지 않았다.
