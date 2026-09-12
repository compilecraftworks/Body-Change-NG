# Native Face TXST 재읽기 시험판 — 2026-09-10

## 확인한 증거와 정정

20:12:57–20:14:54 툴레드 로그에서 BCNG의 얼굴 TXST 소유권과 선택 DDS
기록은 정상인데, 실제 얼굴 material/loadedDiffuse는 계속 이전 경로였다.

- 일반 `FemaleHeadNord`: `textures\actors\character\female\FemaleHead.dds` 유지.
- 이후 `HPBodyChangeFace4`: `textures\BodyChange\CustomSet4\FemaleHead.dds` 유지.

CustomSet4만의 충돌로 설명할 수 없다. 기존 BodyChange.esp를 비활성화하거나
공유 HeadPart의 TXST를 바꾸는 수정은 하지 않았다. 앞선 FaceDetailBaseline
수정은 팩 선택 순서에 따른 디테일 선택 문제를 고친 것으로, 이 재읽기 누락과
별개의 수정이다.

## 네이티브 갱신 근거

[SKSE RegenerateHead 작업](https://raw.githubusercontent.com/ianpatt/skse64/master/skse64/InternalTasks.cpp)은
현재 FaceGen 노드, 현재 Face HeadPart, TESNPC를 엔진에 전달한다.
[SKSE 2.0.20 선언](https://raw.githubusercontent.com/ianpatt/skse64/v2.0.20/skse64/GameData.h)의
SE 주소 `0x003D2A60`은 설치된 1.5.97 Address Library의 ID 26259와 일치한다.
기존 고정 CommonLibSSE-NG 6.7.0에서는 같은 함수를
`BSFaceGenManager::PrepareHeadPartForShaders`, SE/AE ID 26259/26838로 노출한다.
새 하드코딩 주소나 RaceMenu 버전별 함수 후킹은 추가하지 않았다.

HeadPart는 엔진에 현재 얼굴을 지정하는 인자다. HeadPart 교체, 공유 HDPT
TXST 변경, DDS 이중 적용을 의미하지 않는다. 일반 스킨은 계속 Native TXST다.

## 변경 파일/동작

- `NativeFaceRefresh.h`: TXST/로드 리소스의 대소문자·슬래시·Textures 접두사
  차이를 고려하는 비교 및 얼굴 갱신 조건. 실제 경로를 변경하지 않는다.
- `NativeSkinBackend.cpp/.h`: 기존 NiNodeUpdate 뒤에 현재 Face HeadPart를
  엔진 함수에 전달해 얼굴 셰이더를 준비한다. 활성 선택은 BCNG 얼굴 TXST를
  여전히 소유하고, 지정 DDS와 현재 재질이 다를 때만 수행한다.
- 기본값 또는 얼굴 없는 팩으로 전환할 때 이전 얼굴을 복원하는 일회성 명령은
  로드된 액터 ID만 보관한다. 외부 TXST 교체 시 폐기하고, 실행 및 세션 초기화 시
  제거한다. 세이브 외형 상태나 Geometry/Material 포인터를 추가 저장하지 않는다.
- `ActorEvents.cpp`, `AppearanceWork.h`: 얼굴 갱신 전용 채널. 반복 이벤트는
  최신 작업 하나로 합치고, 장비/SOS/TNG 작업과 분리한다. 바디 UI의 대화형 작업
  취소가 이미 반영된 TXST의 후속 갱신까지 취소하지 않도록 비대화형 채널로 둔다.
- `SkinArchitectureTests.cpp`, `FrameTaskQueueTests.cpp`: 비교/소유권/기본값/로드
  준비 조건, 일반·커스텀 헤드에 동일 정책, 중복 이벤트, UI 취소 격리,
  CBBE/UNP A→B→A 선택의 몸·손·발·얼굴 및 명시적 발 DDS 분리 테스트.

직접 Material/TextureSet 쓰기, 추가 전체 3D 재구축, 타이머 폴링, 헤드 모프
변경, 틴트/화장 배열 수정은 추가하지 않았다. 엔진 함수 호출 후 이전 재질
포인터를 재사용하지 않는다. 기존 `BCNG face audit`는 갱신 후 실제 DDS를 기록한다.

## 몸·손·발 점검

위 로그의 7개 팩, 총 26회 적용을 부위와 shader index별로 대조했다.
몸 역할의 기록 800개, 손 488개, 발 592개에서 같은 적용 안의 동일 부위/채널이
여러 경로로 갈리는 기록은 없었다. 몸에는 femalebody, 손에는 femalehands,
발에는 같은 팩의 femalebody DDS가 배정됐다. 해당 Legacy 팩의 발은 몸 아틀라스를
공유하므로 정상이다. 명시적 feet DDS가 있는 팩 및 UBE 공유 아틀라스는 별도
기존 정책과 테스트를 유지한다.

이는 TXST 배정/리소스 검사 결과이며, 모든 부위의 실제 화면 결과나 모든 장비,
모든 NPC를 인게임 검증한 것은 아니다. 몸·손·발 적용 경로는 이번에 변경하지 않았다.

## 검증과 배포 범위

- 기존 xmake 3.1.0 / CommonLibSSE-NG 6.7.0 / SE·AE flat 구성 유지.
- Release 빌드 및 전체 17개 회귀 실행 파일 통과.
- 첫 테스트 실행에서 주석의 금지 API 이름을 격리 검사기가 검출했다.
  주석 표현만 수정했고 검사기는 완화하지 않았다. 이후 전체 재실행 통과.
- `git diff --check` 통과. 기존 LF/CRLF 안내만 존재.
- SHA-256: `7F665C7AB90BC0A34EA4D737AE12DB3A6BFD550283875F59C64F90374B3F9F27`.
- 시험 대상: `D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`.
  게임 종료 확인 후 DLL만 교체하고 설치본 SHA-256 일치까지 확인했다.
  설정/규칙/스킨팩/ESP/세이브/스크립트는 변경하지 않았다.
- 공개 GitHub 릴리즈 및 떼껄룩 업데이트 없음.

회귀 테스트는 순수 정책/큐/플래너 및 기존 저장·배포 테스트다. 실제 엔진 함수가
최종 화면의 얼굴을 교체하는지는 아직 게임 검증 전이다. 다음 확인은 일반 헤드와
현재 커스텀 헤드 각각 스킨 A→B→기본값, 얼굴형/화장/흉터 보존, 저장/로드를 포함한다.
`BCNG native face shaders refreshed`와 직후 `BCNG face audit`를 실제 화면과 대조한다.
오버레이의 반복 선택 실패 원인은 이번 수정으로 해결됐다고 주장하지 않는다.
