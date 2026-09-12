# 얼굴 부착 전 TXST 바인딩 시험 — 2026-09-11

## 상태

부착 전 얼굴 어댑터를 구현하고 **SE 1.5.97용 시험 Release DLL**을 만들었다.
이전의 “미구현” 기록 이후 단계다. 아직 이 DLL의 인게임 검증은 하지 않았으므로
얼굴 교체, 표정, NPC 간 격리, 저장/로드 및 CTD 해결 완료로 보고하지 않는다.

시험 DLL:
`build/v1.2.0/trials/face-attachment/windows/x64/release/BodyChangeNG.dll`

SHA256:
`54C037855D1E3C68E51D3844583144E5BCA41C35149C919DA304746765F04D10`

시험 DLL 생성 당시에는 툴레드 MO2의 설치 파일, 실행 중인 게임, 세이브, GitHub
공개 Release를 변경하지 않았다. 이후 사용자 요청으로 아래의 툴레드 배포를 진행했다.
이번 시험 기능 자체에는 새 ESP·Papyrus 스크립트·설정 JSON이 필요 없다.
다른 버전의 오래된 설치 전체가 DLL만으로 갱신 가능하다는 의미는 아니다.

## 변경 구조

1. 기존 NativeSkinBackend가 선택 스킨의 Face TXST와 선택 상태를 유지한다.
2. 원래 FaceGen 로더가 끝난 뒤, 엔진의 얼굴 부착 함수로 들어가기 직전에
   선택값과 현재 Face HeadPart 식별자를 값으로 복사한다.
3. 로더의 결과를 별도 얼굴 인스턴스로 복제한다. 원본 지오메트리와 포인터가
   겹치지 않는지 확인하고, 해당 Face HeadPart 지오메트리만 대상으로 삼는다.
4. shader property를 복제하고, 엔진 `SetMaterial(..., true)`로 원본과 다른
   재질을 얻은 뒤 새 BSShaderTextureSet에 선택 DDS 경로를 넣는다.
5. 이전 Diffuse가 유효하면 로드를 생략하는 엔진 조건 때문에, **복제 재질에서만**
   기존 텍스처 참조를 비우고 `OnLoadTextureSet`으로 실제 리소스를 읽는다.
   FaceGen tint는 강한 참조로 보존하고, 제공되지 않은 채널의 리소스도 보존한다.
6. 실제 로드된 NiSourceTexture 이름을 선택 경로와 비교한다. SetupGeometry의
   true 반환을 로드 성공으로 간주하지 않으며 설정 후에도 재질을 다시 조회한다.
7. 선택의 세대·콘텐츠 해시·액터·ActorBase·TXST·HeadPart·경로가 여전히 같은지
   선택 잠금 아래 재확인한 후에만 호출자가 가진 얼굴 NiPointer를 교체한다.
8. 잠금을 해제하고 **원래 부착 함수를 정확히 한 번 호출**한다. 엔진이 실제
   부착한 얼굴을 현재 얼굴 등록표에 넣고, 기존 장비 partition 및 정리 흐름을
   그대로 처리한다. 공용 모델 DB나 등록표 삽입/제거 함수는 수정하지 않는다.

이것은 월드에 붙은 얼굴을 따로 다시 칠하는 경로가 아니다. 원본 HeadPart/NIF
교체, 일반 스킨 override API, 추가 QueueNiNodeUpdate, 의상 장착 추적을 넣지
않았다. 시험 어댑터가 설치되면 기존 사후 얼굴 shader helper도 실행하지 않는다.
몸·손·발, 오버레이, 성기 어댑터 및 직렬화 형식의 동작 코드는 변경하지 않았다.

## 소유권·기본값·부분 팩

- BCNG의 새 전역 얼굴/재질 포인터 보관소는 없다. 준비 도중의 참조는 로컬
  NiPointer가 관리하고, 부착 후에는 기존 엔진 소유 관계를 따른다.
- 복제/로드 실패 또는 선택 변경 시 준비한 복제본을 폐기하고 원래 얼굴을 넘긴다.
  선택 mutex 아래에서 원본이 최종 해제되지 않도록 원래 루트의 로컬 참조를
  유지하며, 원래 부착/FixSkinInstances 호출이 끝날 때까지 그 참조를 보존한다.
- 기본값, 얼굴 레이어가 없는 스킨, BCNG를 선택하지 않은 액터는 원래 부착으로
  통과한다. 기존 BCNG 저장 선택 복원 후 발생하는 native 재구축에도 같은 경로를 쓴다.
- 전체 DDS가 필수는 아니다. 스킨팩이 선언한 채널만 교체하고, 원래 얼굴 tint
  채널 6을 스킨 채널로 취급하지 않는다.
- FaceGen RGB Tint 재질에는 FaceGen detail 텍스처 필드가 없다. 이 재질에서는
  엔진이 지원하는 0/1/2/7 채널을 로드 검증하며, 일반 FaceGen은 0/1/2/3/7이다.
- 엔진 CloneMembers가 animationData를 공유하는 기존 의미는 바꾸지 않았다.
  표정 상태까지 완전한 deep copy를 구현했다는 뜻이 아니다.
- 기존 shared-ActorBase 선택 제한을 바꾸지 않았다. 동일 ActorBase의 모든 참조에
  서로 다른 스킨을 허용하는 기능이 새로 생겼다고 해석하지 않는다.

## 런타임 및 빌드 분리

근거는 [메인 메뉴 코드 조사](FACE-ATTACHMENT-BOUNDARY-AUDIT-20260911-KO.md)다.
네 CALL RVA `14A033`, `31A551`, `3652A7`, `65100E`의 원래 목적지가 모두
`363F20`인지 먼저 검사한다. 한 곳이라도 다르면 아무 CALL도 바꾸지 않는다.
callee prologue나 기존 FixSkinInstances/다른 모드의 내부 후킹은 덮어쓰지 않는다.

이 주소는 **SE 1.5.97에서만 검증**했다. AE에서는 새 시험 후킹을 설치하지 않으며,
기존 기능은 그대로 둔다. SE/AE-exclusive flat 빌드 이외에는 컴파일 오류로 차단한다.
RaceMenu 버전 번호를 이유로 게임 내부 주소를 재사용하지 않는다.

고정 XMake 3.1.0 및 저장소의 기존 고정 의존성을 사용한다. 신규 의존성은 없다.

```powershell
$faceTrialXmake = 'C:\Users\yunha\.codex\external-tools\xmake\3.1.0\xmake\xmake.exe'
& $faceTrialXmake f -m release --face_attachment_trial=y -y
& $faceTrialXmake build -a
# 작업 후 기본 구성을 복구한다. 시험 DLL은 별도 디렉터리에 남는다.
& $faceTrialXmake f -m release --face_attachment_trial=n -y
& $faceTrialXmake build BodyChangeNG
```

옵션 기본값은 false다. 일반 출력 DLL에는 시험 후킹 코드가 포함되지 않는다.
설치 시에는 기존 `SKSE/Plugins/BodyChangeNG.dll`을 교체하는 방식이며, 시험 DLL을
다른 이름의 DLL로 나란히 설치하면 안 된다. 후속 설치 내역은 아래와 같다.

## 후속 툴레드 배포

사용자의 “툴레드에 반영해바” 요청에 따라 게임 종료 상태를 확인하고 다음 한 파일을
위 시험 DLL로 교체했다.

`D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`

- 교체 직전 SkyrimSE 프로세스 없음 재확인.
- 설치 파일 2,562,048바이트, SHA256이 위 시험 DLL과 일치함을 확인.
- 기존 요청대로 MO2 내 별도 백업 DLL은 만들지 않음.
- ESP, 스크립트, 설정/배포 규칙, 스킨팩, 세이브, 떼껄룩 설치, 공개 Release는 변경하지 않음.
- 이 배포 확인은 실제 인게임 적용 성공 확인이 아니다.

## 테스트

- 시험 Release 및 기본 옵션을 끈 일반 Release 빌드 통과. 시험 전용 로그 표식이
  시험 DLL에만 들어 있고 일반 DLL에는 없는지 바이너리로 대조했다.
- 기존 17개 + 새 `NativeFaceAttachmentTests` = 18개 테스트 실행 파일 통과.
- 새 테스트: 부분 채널·틴트 보존, 잘못된/누락 채널 거절, TXST만 바뀌고 로드된
  Diffuse는 그대로인 관측의 실패 판정, RGB/일반 FaceGen 채널 구분, 오래된 선택
  식별자 거절, null/shared 준비본 거절, 폐기 시 참조 해제, 1,000회 반복 교체.
- 테스트의 counted stand-in은 BCNG의 값/commit 정책만 검증한다. Skyrim의
  NiObject 복제, 재질 매니저, 현재 얼굴 등록표, 실제 세이브/로드를 모사해
  성공한 것처럼 보고하지 않는다.
- 기존 경로 격리 테스트의 첫 실행은 새 주석에 금지 API 이름 문자열을 적은
  탓에 실패했다. 주석을 수정했고, 테스트 규칙은 완화하지 않았다. 재실행 통과.
- 생성된 오브젝트에서 `ClearTextures`의 vtable +0x48, `OnLoadTextureSet`의
  +0x40 호출과 0인 두 번째 인자를 확인했다. 독립적인 인게임 ABI 검증은 아니다.

## 변경 파일

- `NativeFaceAttachment.cpp/.h`: 시험 부착 어댑터 및 설치.
- `NativeFaceAttachmentPolicy.h`: 선택 스냅샷, 채널 병합/로드 관측 검증, 조건부 commit.
- `NativeSkinBackend.cpp`: 기존 선택 소유자에서 스냅샷 제공/최종 검증. 시험 설치 시
  사후 얼굴 shader helper 생략.
- `main.cpp`, `xmake.lua`: 시험 전용 진입과 별도 빌드 출력, 기본값 off.
- `tests/NativeFaceAttachmentTests.cpp`: 새 정책 회귀 테스트.

## 아직 필요한 실제 게임 확인

플레이어와 서로 다른 ActorBase의 NPC에서 A → B → 기본값, 반복 선택,
저장/게임 종료/로드, 셀 재로드, native 3D 재구축, 같은 얼굴 모델을 쓰는 두 NPC의
서로 다른 선택, 화장/표정/흉터/장비로 가리는 얼굴 부위를 확인해야 한다.

로그 `privateBeforeAttach=true`는 **부착 직전** 복제본이 준비됐다는 뜻이다.
후속 다른 모드나 엔진 처리까지 끝난 화면의 성공 증거는 아니다. 기존
`BCNG face audit`의 `loadedDiffuse` 및 실제 화면과 함께 비교해야 한다.
실제 실패 가능성과 엔진 수명 문제를 이 오프라인 테스트 통과로 배제하지 않는다.
