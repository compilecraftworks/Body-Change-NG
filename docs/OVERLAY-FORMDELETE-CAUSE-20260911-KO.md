# 오버레이 첫 선택 이후 실패 — 삭제 경로 확정

## 상태

2026-09-11 07:35–07:41 툴레드 시험 로그와 설치 DLL 독립 재현으로
**이번 저장 키 소실의 원인을 확인했다. 아직 수정 DLL은 배포하지 않았다.**
사용자가 성공을 확인한 Native 몸/손/발/얼굴, UI, 배포 규칙, 세이브,
설치된 RaceMenu 파일은 이번 조사에서 변경하지 않았다.

## 확정된 경로

```text
Skyrim → SKSE 삭제 콜백 순회 (skse64_1_5_97.dll + F5D1 CALL)
       → 설치 SKEE의 FormDelete (skee64.dll + 50AA0)
       → 64비트 handle을 하위 32비트로 축소
       → 해당 FormID의 Armor/Node override 저장소 삭제
       → BCNG의 다음 교체에서 이전 슬롯 저장 키가 없어 실패
```

- 설치 SKEE: `[LED]UBE 2.0/SKSE/Plugins/skee64.dll`
- SHA256: `283EA6F0DF6234B5636D6B03445A57C90369514E61EC3B02DA07F731FCF3469B`
- 플레이어 실제 VM handle: `0000FFFF00000014` (선택 직후 조회 로그).
- 삭제 콜백의 실제 입력 예: `0002001500000014`, `0002001400000014`,
  `0002001B00000014`. 모두 **다른 64비트 값**이지만 하위 32비트는 `00000014`다.
- 플레이어 삭제 콜백 **43회 모두**, 같은 스레드의 다음 sequence에서
  `actor-erase actor=00000014`가 이어졌다. tick 차이는 0–1ms였다.
- 실제 플레이어 VM handle 자체로 받은 삭제 통지는 **0회**였다.
- 전체 actor-erase 3,545회 모두 SKSE 콜백 순회의 반환 주소 `+F5D3`를 포함했다.
  이 전체 수치가 모든 NPC의 실제 저장 데이터 소실을 뜻하지는 않는다.
  빈 map에 대한 erase 진입도 추적에 포함된다.

관찰 훅의 `BodyChangeNG.dll` 스택 프레임은 기록용 래퍼다. BCNG가 삭제를
시작했다는 뜻이 아니다. SKEE FormDelete는 마지막에 actor-erase로 tail jump하므로
스택에서 SKEE 프레임이 생략된다. 디스크 코드로 `+50AC5 → +89490`을 확인했다.
SKSE `+F5D1`은 콜백 함수 포인터 CALL이고 그 반환 위치가 `+F5D3`다.

상위 비트의 구체적인 객체 종류/수명 분류는 이번 증거만으로 일반화하지 않는다.
핵심 증거는 **플레이어 자체가 아닌 다른 전체 핸들 통지가 플레이어 저장소를
삭제했다는 것**이다. 모든 RaceMenu 배포본에 동일한 버그가 있다는 결론도 아니다.

## 선택 실패와 대조

| 부위 | 정상 선택 확인 | 저장소 삭제 확인 | 다음 선택 |
| --- | --- | --- | --- |
| 얼굴 | 07:40:27.674 stored/live=true | 07:40:28.869, seq1462→1463 | 07:40:29.523부터 absent, 4회 실패 |
| 몸 | 07:40:39.140 stored/live=true | 07:40:39.182–183, seq1762→1765 | 07:40:39.671 absent, 이후 외부 DDS도 관찰 |
| 손 | 07:40:45.842/46.497/47.218 정상 | 07:40:47.562, seq2012→2013 | 이 실행에서 다음 손 선택 실패는 기록되지 않음 |

몸 슬롯에는 키가 지워진 후 Soulgem Oven의 veins DDS가 다시 나타난다.
이는 별도의 슬롯 재사용 충돌 위험이며 해당 모드가 최초의 actor-map 삭제를
시작했다는 증거가 아니다. 다른 모드의 값을 덮어쓰도록 BCNG 소유권 검사를
제거해서는 안 된다.

map-clear 1회는 07:40:08.032 세이브 로드/Revert 경로다. 발 키 10개 삭제는
07:40:51.563–564의 BCNG 개별 키 해제 경로다. 위 얼굴/몸 소실보다 뒤이며
콜백에 의한 actor-map 소실과 구별된다. 이 정상 해제 경로를 차단하면 안 된다.

## 독립 재현

`tools/probe-racemenu-node-registration.py`에 `--form-delete-repro`를 추가했다.

- Skyrim에 연결하거나 실행하지 않는다. BCNG, 다른 모드, SKSEPlugin_Load,
  렌더링 및 세이브/로드 함수를 실행하지 않는다.
- 정확한 SHA256의 SKEE만 OS 로더로 별도 폐기 가능한 프로세스에 로드한다.
- 기존 4부위 각 1,000회 DDS/tint/alpha 등록·교체 검사는 그대로 통과한다.
- 설치 코드의 FormDelete가 두 로컬 override-map eraser만 호출함을 확인한 뒤
  로그에서 관찰한 10개 상위 필드를 재생한다.
- 합성 플레이어 및 NPC FormID 3개 × 10개 입력 = **30회 모두** 해당 low32
  액터의 4부위/남녀 키가 소실되고, 다른 액터 키는 유지되는 것을 재현했다.
- 이는 **버그를 재현하는 검사**이며 해결/인게임 성공 테스트가 아니다.
- 기존 삭제 추적 probe도 재실행하여 600회 삭제 경로, 300회 버퍼 포화,
  2,000회 다중 스레드 삭제 검사 통과. 기존 빌드된 회귀 테스트 실행 파일
  18개도 모두 통과했다. 이번에는 제품 C++ 변경이 없어 재빌드하지 않았다.

명령:

```powershell
& 'C:\Users\yunha\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  'tools\probe-racemenu-node-registration.py' `
  'D:\TuLED13E\File Mod Skyrim SE\mods\[LED]UBE 2.0\SKSE\Plugins\skee64.dll' `
  --form-delete-repro
```

## 수정의 경계

필요한 것은 슬롯 재등록/복구 타이머가 아니라 **잘못된 삭제 통지를 액터
전체 저장소 삭제로 연결하지 않는 호환 처리**다.

- 실제 reference 삭제, 명시적인 키/노드 해제, Revert는 계속 가능해야 한다.
- 다른 플러그인의 SKSE 삭제 콜백을 막거나 교체하면 안 된다.
- VM handle을 무기한 Persist하여 삭제 통지를 억제하지 않는다.
- 저장소/문자열/슬롯 소유권 검사를 우회하지 않는다.
- 원본 SKEE 파일 교체나 UBE 기능 제거 없이 BCNG 측의 한정 호환 처리 가능성을
  검토하되, 입력 핸들 분류와 정상 삭제 조건을 확인한 후 시험 DLL로 검증해야 한다.
- 해당 기계 코드에 대한 한정 패치는 인터페이스 버전만으로 다른 SKEE에
  적용하지 않는다. 일반 Overlay API 지원 여부와 이 특정 결함의 패치 여부는 별개다.

## 증거 보존 및 참고

로컬 로그 사본: `build/diagnostics/overlay-formdelete-20260911/BodyChangeNG.log`

SHA256: `E06996BE7C9010883BC568BF40479E8043CEAACA1BC721558081B6B0AD7D9F8A`

진단 버퍼 drop 34회는 07:37:19에 있었다. 위 핵심 선택/삭제 쌍은 모두
실제로 기록된 항목이다. 로그에 없는 전체 실행을 완전 추적했다고 주장하지 않는다.

- [SKSE 공식 Hooks_Papyrus.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Papyrus.cpp):
  OnFormDelete_Hook → Serialization::HandleDeletedForm 연결.
- [SKSE 공식 Serialization.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/Serialization.cpp):
  플러그인별 삭제 콜백 순회. 이 실행의 실제 경로는 설치 SKSE 바이너리와 대조했다.
- 기존 로컬 RaceMenu 감사 소스 `87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc`,
  `skee/main.cpp`: UInt64 FormDelete를 override 삭제에 전달하는 구형 구현.
  현재 설치 backport는 FormID32 저장소이므로 구형의 handle 기반 전제와 혼동하면 안 된다.
