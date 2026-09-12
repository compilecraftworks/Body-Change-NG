# 스킨·오버레이 SE/AE 호환성 점검 — 2026-09-11

## 결론과 지원 범위 정정

이 결과는 **비공개 시험 DLL의 소스·빌드·격리 시험 결과**다. 모든 게임
버전에서 실제 플레이어/NPC 표시, 저장/로드가 통과했다는 보고가 아니다.

요청된 13개 중 **1.6.678은 Epic판**이다. 기존 문서의 GOG 표기는 오류였다.
SKSE는 Epic판을 공식 지원하지 않으므로 BCNG만 수정해서 전체 선행 모드까지
지원할 수 없다. 이 버전을 게임/틴트/UI/얼굴 훅 허용 표에서 제외하고 거부
테스트를 추가했다. [SKSE 공식 안내](https://skse.silverlock.org/),
[공식 버전 정의](https://github.com/ianpatt/skse64/blob/master/skse64_common/skse_version.h).

| 게임 | 현재 확인한 범위 | 남은 범위 |
|---|---|---|
| SE 1.5.97 | ID 24228, 이전 메인 메뉴 실행 코드의 얼굴 호출 4곳을 새 실제 C++ 검사기로 재검증 | 이번 새 DLL의 인게임 표시·수명주기 |
| AE 1.6.317 / 318 / 323 / 342 / 353 | 각 설치 Address Library에서 ID 24732 존재, 명시적 런타임 분기 테스트 | 각 실행 코드의 호출 형태·재질 로딩·인게임 표시 |
| AE 1.6.629 / 640 / 659 / 1130 / 1170 / 1179 | 각 설치 Address Library에서 ID 24732 존재, 1.6.629 전후 레이아웃 분기 테스트 | 각 실행 코드의 호출 형태·재질 로딩·인게임 표시 |
| 1.6.678 Epic | SKSE 공식 미지원 확인, 추측 레이아웃 제거 | BCNG 범위 밖의 선행 모드 지원이 필요 |

AE 1.6.1170의 디스크 EXE는 패킹된 명령어이므로 이를 실행 코드의 증거로
사용하지 않았다. 사용자 동의하에 메인 메뉴 프로세스 읽기를 시도했지만
Windows 접근 거부(5)로 코드 캡처를 얻지 못했다. 이후 사용자가 게임을 종료하고
오프라인 점검을 요청하여 추가 실행/재실행 요청 없이 점검했다. 게임 메모리에
쓰거나 엔진 함수를 실행한 적은 없다. Address Library 주소 존재는 ABI나 표시
성공의 증명이 아니다.

## 수정 내용

### 네이티브 얼굴

- SE 전용 함수 RVA 및 호출 위치 4개를 고정하는 부분을 제거했다.
- `NativeAppearanceRuntime.h`에서 런타임별 ID(24228/24732)를 선택한다.
  AE의 24732는 공식 RaceMenu `UpdateHeadState_Target1`과 대조했다.
  [RaceMenu 1.6.1170 소스](https://github.com/expired6978/SKSE64Plugins/blob/348607e9ae5f360ccab0d623e8b0d8f42e586fa6/skee64/SKEEHooks.cpp).
- 실제 로드된 실행 코드에서 해당 함수를 부르는 `E8` 호출과 인자 준비를
  검증한다. 로컬 인자 패턴 검사는 범용 디스어셈블러가 아니며, 현재 원본
  명령어로 검증된 패턴은 SE 캡처에 기반한다. AE에서 모두 일치함은 미확인이다.
- 함수 조각 경계 앞 14바이트를 무조건 요구하던 조건을 제거했다. 실제 인자
  준비 길이(10~14바이트)를 사용하며, 이어진 unwind 정보가 같은 원래 함수에
  속하는 경우만 조각 경계를 허용한다. 빈 구간·다른 함수·순환/손상 정보는
  거부한다. [Microsoft x64 unwind 규약](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170).
- 네 호출 모두 검증되기 전에 변경하지 않는다. 알 수 없는 형태나 타 모드의
  변경으로 검증이 실패하면 그 이유를 로그에 남기고 이 얼굴 연결만 설치하지
  않는다. 미확인 AE에 SE 오프셋을 대입하는 대체 처리는 없다.
- 기존 선택 TXST 상태, 부착 전 private face 준비, 원본 네이티브 부착 함수,
  tint 보존/참조 소유권 코드는 유지한다. 일반 스킨 NiOverride, 부착 후 덧칠,
  헤드 NIF 교체, 전역 모델 캐시 변경을 추가하지 않았다.

### RaceMenu 오버레이

- 공식 소스 11개 커밋에서 BCNG가 쓰는 BodyMorph/Overlay/Override 접두부의
  순서·호출 인수·반환형과 v2 visitor 계약을 재대조했다.
- 파일 버전으로 기능을 막지 않는다. v1/v1과 v2/v2는 별도 ABI를 사용한다.
  미확인 상위 인터페이스는 기존 정책대로 알려진 하위 접두부만 사용한다.
  미래 구현이 그 접두부를 보존한다는 전제이지 미래 ABI 검증 완료가 아니다.
- 툴레드 UBE 백포트의 문제는 64비트 삭제 핸들을 32비트 FormID로 잘라
  살아 있는 부모 액터의 오버레이 키를 지우는 콜백이었다.
- 특정 파일 SHA나 고정 주소 대신 **전체 잘못된 콜백 명령어 구조**, 두 대상
  함수, 동일한 Override 객체 및 RTTI, 실제 메모리/파일 일치를 검사한다.
- DataLoaded에서 실행 중인 VM의 빈 핸들·플레이어·NPC Base 핸들을 비영구
  조회하여 direct-form 형식을 확인한다. 콜백 중에는 VM/액터를 조회하지 않는다.
- 실제 Form 삭제는 원래 두 삭제 함수로 전달하고, 부모 ID 충돌 핸들은 전달하지
  않는다. 사라진 키를 나중에 복구하는 타이머/재등록 루프가 아니다.
- 공식 소스는 이후 해당 콜백을 제거했다.
  [제거 커밋 e779c68](https://github.com/expired6978/SKSE64Plugins/commit/e779c68ce5449f713c7b551c7b7173208e7f83c0).
  구형 전체 64비트 핸들 콜백이나 제거된 콜백은 이 문제 패턴과 일치하지 않으며
  수정하지 않는다. 보정 불일치가 오버레이 정상 API를 비활성화하지 않는다.
- 새 Geometry/Material 캐시, per-actor 포인터 저장, PersistHandle,
  실행 코드 복제용 트램펄린, 상주 스레드를 이 보정에 추가하지 않았다.
  진단 스택 추적 빌드는 이번 DLL에 포함하지 않는다.

## 검증 결과

- Release 모드 SE/AE flat 빌드 성공. CommonLibSSE-NG v6.7.1과 기존 잠금
  의존성 유지. upstream `/Zc:templateScope` 자동 무시 경고는 남아 있다.
- 현재 xmake에 등록된 전체 테스트 실행 파일 **20/20 통과**.
- 새 `CodePatternTests`: 상대 주소 변경, 잘못된 콜백/전체 폭 콜백 거부,
  PE 경계/BSS, 인자 준비, chained unwind, 빈 구간·다른 함수·순환·손상 거부.
- `RuntimeLayoutTests`: 12개 허용 버전과 인터페이스 세대, 미래 하위 계약 폴백,
  1.6.629 경계, Epic/VR/미확인 게임 거부.
- 실제 UBE SKEE 격리 시험: 네 부위 각각 1,000회 교체 등록, 잘못된 부모 삭제
  3,900건 차단, 3액터·양성별·네 부위 및 Armor 키 유지, 실제 Form 삭제와
  명시적 초기화/Revert 정상 전달, 2,000회 동시 보존/삭제, 중복 설치 방지 통과.
- 공식 SE/AE SKEE 두 파일을 별도 시험 프로세스에서 읽고 새 guard가 설치되지
  않으며 실행 코드 해시가 그대로임을 확인했다. SKEE의 SKSEPlugin_Load나
  렌더링 함수는 이 시험에서 호출하지 않았다.
- SE 캡처 네 호출 `14A033 / 31A551 / 3652A7 / 65100E`를 실제 C++ 인자·함수
  경계 검사기에 입력해 모두 통과. 같은 EXE SHA와 unwind 정보를 대조했다.
- 설치 Address Library에서 12개 허용 버전의 얼굴 ID 매핑 확인.
  기록: `build/audits/appearance-compat-20260911/offline-coverage.json`.

격리 등록 시험은 게임 렌더링/셀 재로드/세이브 복원을 대신하지 않는다.
기존 선택 상태 직렬화·NPC 배포·성기 persistence 테스트는 통과했지만 이번
게임 안의 해당 수명주기를 직접 실행한 것은 아니다. 누수가 없거나 CTD가
절대 없다고 단정하지 않는다.

## 산출물과 변경 파일

비공개 시험 DLL:

`build/v1.2.0/trials/appearance-compat/windows/x64/release/BodyChangeNG.dll`

- 크기: 2,583,040 bytes
- SHA-256: `3C2EB2438BEDFCC61DEADA8BC656AE5B3214773B8355581218DC259AA99C265E`
- 빌드 옵션: `face_attachment_trial=y`, `form_delete_guard_trial=y`,
  `overlay_registry_trace=n`, `mode=release`, VR off.
- 일반 빌드의 두 시험 옵션 기본값은 false다. 이 수정의 시험 코드가 일반
  공개 DLL로 승격된 상태가 아니다.
- 툴레드·떼껄룩 MO2 및 원본 SKEE DLL, 공개 Release, 소스 ZIP, GitHub는
  변경하지 않았다. 툴레드 설치 BCNG는 기존 SHA
  `431FC8084E120BCACFD3138F347087AEDDE3E5A1F4EC792ED060180237338DA7` 그대로다.

주요 파일 그룹:

- `NativeFaceAttachment.cpp`, `NativeFaceAttachment.h`, `NativeFaceCallPattern.h`,
  `NativeFunctionBoundary.h`, `NativeAppearanceRuntime.h`: 얼굴 연결/검증.
- `RaceMenuFormDeleteGuard*.cpp/.h`, `RaceMenuFormDeletePolicy.h`,
  `FormDeleteCallbackPattern.h`, `PeImageFile.h`, `main.cpp`: 삭제 보정과 초기화.
- `RuntimeCompatibility.h`, `RuntimeLayout.h`: Epic 허용 오류 정정.
- `CodePatternTests.cpp`, `FormDeletePolicyTests.cpp`, `FormDeleteGuardProbeBridge.cpp`,
  `RuntimeLayoutTests.cpp`: 정책·실제 C++ 검사기 시험.
- `tools/audit-racemenu-history.py`, `tools/audit-appearance-runtime.py`,
  `tools/verify-appearance-compat.py`, `tools/probe-racemenu-guard-compat.py`: 재현 도구.
- `xmake.lua`, `DEPENDENCIES.md`, 이 문서와 기존 호환성/구조 문서: 별도 산출물과
  지원 범위 정정. 기존 작업의 UI·분배 규칙·사용자 설정은 덮어쓰지 않았다.
