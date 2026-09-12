# 오버레이 FormDelete 오삭제 방지 시험본

## 결과와 범위

기존 삭제 추적 시험본 대신 **잘못된 삭제를 사전에 거르는 수정**을 구현했다.
일반 몸/손/발/얼굴 Native 스킨, 오버레이 선택/색상/슬롯 소유권, ASTR 저장,
배포 규칙은 변경하지 않았다. 성공 확인된 face attachment trial은 켜 둔다.

시험 DLL SHA256:
`431FC8084E120BCACFD3138F347087AEDDE3E5A1F4EC792ED060180237338DA7`

크기: 2,572,800 bytes.

빌드 경로:
`build/v1.2.0/trials/form-delete-guard/windows/x64/release/BodyChangeNG.dll`

설치 대상:
`D:/TuLED13E/File Mod Skyrim SE/mods/Body Change NG/SKSE/Plugins/BodyChangeNG.dll`

게임 종료를 확인한 뒤 위 DLL만 교체했고, 원본 시험 DLL과 설치 DLL의 SHA256
일치를 확인했다. SKEE 원본 SHA256도 변경되지 않았다. 추가 백업 DLL이나
PDB·INI·스크립트는 MO2 폴더에 넣지 않았다.

Release 모드의 비공개 시험본이다. 공개 Release, 소스 ZIP, GitHub, 떼껄룩은
업데이트하지 않는다. **아직 수정본의 인게임 연속 선택·저장/로드 검증 전이다.**

## 마지막 미확인점의 실제 코드 확인

사용자가 종료하기 전 실행된 툴레드 PID 21300에서, SE 1.5.97
`SkyrimScript::HandlePolicy`의 8개 코드 구간을 읽기 전용으로 확인했다.
프로세스 쓰기, 엔진 함수 원격 호출, 정지, 주입, 액터/세이브 메모리 읽기는 하지 않았다.

`tools/audit-vm-handle-policy.py`는 Address Library SE ID **271953**의 가상 테이블을
사용한다. 디스크/실제 가상 함수 포인터 일치를 확인하고 게임 모듈 내부의 한정된
코드만 읽는다. 결과는 `build/audits/vm-handle-policy-20260911/`에 있다.

- `GetHandleForObject` RVA **0x902860**: 일반 Form은 `0x0000FFFF00000000 | FormID`.
- 같은 함수의 별칭 분기는 `alias ordinal << 32 | parent FormID`.
- unique reference 분기는 `0x0001000000000000 | ordinal << 32 | parent FormID`.
- 활성 효과 분기(VM type 0x8E)는 `0x0002000000000000 | effect ordinal << 32 | target FormID`.
- `HandleIsType` RVA **0x902560**도 상위 태그와 중간 16비트로 이 분기를 구별한다.
- `EmptyHandle`은 `0x0000FFFF00000000`이며, FormID 0을 삭제 대상으로 삼지 않는다.

따라서 로그의 `0002001500000014`는 플레이어 자체의 핸들이 아니라 플레이어를
대상으로 하는 활성 효과의 핸들이다. 이것을 FormID32로 바로 자르면 효과 삭제가
플레이어 전체 override 삭제로 잘못 이어진다.

## 구현

`RaceMenuFormDeletePolicy.h`:

```cpp
(handle >> 32) == 0x0000FFFF && uint32_t(handle) != 0
```

SE 1.5.97에서 확인한 일반 Form 핸들만 원래의 두 처리로 전달한다:

1. 기존 Armor override map의 해당 FormID 삭제.
2. 기존 Node override map의 해당 FormID 삭제.

나머지 핸들은 부모 FormID 저장소를 건드리지 않는다. 해당 효과/별칭 자체에 대한
게임 및 다른 플러그인의 삭제 처리는 계속 실행된다. SKSE 전체 콜백 순회를 바꾸지 않는다.

`RaceMenuFormDeleteGuardCore.cpp`는 다음 조건에서만 시작 시 1회 연결한다:

- 게임 런타임이 정확히 SE 1.5.97.
- 설치된 UBE SKEE의 전체 SHA256이
  `283EA6F0DF6234B5636D6B03445A57C90369514E61EC3B02DA07F731FCF3469B`.
- FormDelete 원본 함수 42 bytes, 기존 두 삭제 함수 진입부, 전역 Override 객체의
  가상 테이블이 감사한 값과 일치.

원본 **디스크 파일은 수정하지 않는다.** BCNG가 시작 시 이 SKEE 콜백의 실행 중
진입부 하나만 연결한다. 다른 버전/수정된 코드에서는 이 특정 패치를 적용하지 않으며,
BCNG의 기존 인터페이스 기반 RaceMenu 기능을 함께 비활성화하지 않는다.

정상 경로는 원래 콜백이 호출하던 SKEE 삭제 함수 둘을 같은 순서·인자로 호출한다.
스택을 바꾸는 원본 prologue를 임의 trampoline에 복사하지 않는다. 연결은
14-byte indirect JMP이고 후속 처리는 컴파일러가 호출 규약/unwind 정보를 생성한다.
추가 실행 메모리 할당은 없다. 시작 시 검증에 사용한 파일 버퍼는 즉시 해제된다.

## 누수·회귀 경계

- 삭제 콜백 안에 VM/액터 조회, 로그 출력, 동적 할당, 큐, 잠금, 문자열 보관 없음.
- 액터/Geometry/Material 포인터, 액터별 추적 map, VM PersistHandle 없음.
- 사라진 키 재생성, 재시도/복구 타이머 없음.
- 원래의 개별 키 삭제, 노드 삭제, 액터별 명시적 삭제, Revert는 가로채지 않음.
- 이전의 네 군데 삭제 추적 훅은 시험본에서 **컴파일 비활성화**.
  두 시험 옵션을 동시에 켜면 컴파일 오류로 중복 연결을 방지한다.
- 진단은 고정 크기 atomic 카운터 세 개뿐이며 기존 input tick에서 변화가 있을 때
  최대 5초당 한 번 요약한다. 이는 키 적용/복구를 수행하는 타이머가 아니다.

## 테스트

- Release `xmake build --all` 통과.
- 기존 18개 + 신규 핸들 판정 1개 = **회귀 실행 파일 19개 전부 통과**.
- 핸들 판정은 4개 FormID에서 모든 16비트 ordinal, 효과/별칭/unique reference/
  알 수 없는 상위 태그 경계, EmptyHandle을 검사한다.
- 실제 설치 SKEE 독립 프로세스: 4부위 × 1,000회 DDS/tint/alpha 교체 통과.
- 수정 전 콜백 30회로 재현했던 소실에 대해, 수정 후 **3,900회** 잘못된 통지를
  넣어도 세 액터/남녀/4부위/Armor 등록값이 보존됨.
- 정상 Form 삭제는 해당 액터의 **Armor와 Node map 양쪽**을 삭제하고 다른 두
  액터의 DDS·색상·알파는 변경하지 않음.
- 명시적 키/노드/액터 삭제 및 Revert 후 재등록 정상.
- 4스레드, **2,000회 보존→정상 삭제 순환** 통과.
- 잘못된 런타임/파일 해시/이미 수정된 콜백을 거부하며 원본 코드 불변.
- 중복 설치는 같은 코드로 유지되며 재연결하지 않음.

독립 프로세스는 게임 렌더링과 SKSE 실제 저장/로드를 실행하지 않는다.
Revert 후 재등록 검사를 실제 세이브 호환성 증명으로 표현하지 않는다.

## 빌드 및 인게임 확인

```powershell
xmake f -m release --face_attachment_trial=y --overlay_registry_trace=n --form_delete_guard_trial=y
xmake build --all
```

시험본 생성 후 기본 빌드 설정의 세 trial 옵션은 모두 `n`으로 되돌렸다.
일반 출력의 빌드도 확인하되 **그 DLL은 툴레드에 복사하지 않는다**. 정상 일반
출력과 이번 face+FormDelete 수정 시험 출력을 구분하기 위한 설정이다.

설치 후 다음 실행에서 로그에 `BCNG FormDelete guard true: installed...`가 있어야 한다.
원인과 같은 효과 종료 시 `skipped`는 증가하지만 그 액터 키는 유지돼야 한다.

인게임 확인 대상:

1. 얼굴/몸/손/발 각각 여러 항목 연속 선택 및 색상/알파 변경.
2. 기본값 복원 후 다시 선택, 다른 모드의 슬롯 유지.
3. 플레이어/NPC 각각 선택→저장→종료→로드.
4. NPC unload/reload, 3D rebuild, 실제 삭제 시 정상 정리.

문제가 남으면 이번 카운터와 기존 ownership/verification 로그를 구분해서 조사한다.
모든 남은 증상을 이 한 가지 원인이라고 단정하지 않는다.
