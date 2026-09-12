# 오버레이 반복 선택 정지 — 등록 저장소 격리 조사

## 판정

**미해결이다.** 첫 항목 뒤 대부분의 교체가 막힌다는 사용자 보고를 최신
툴레드 로그로 확인했다. 이번 조사는 동작 코드 수정이나 시험 DLL 배포가 아니다.
사용자가 성공을 확인한 Native 스킨/얼굴 시험 DLL, 오버레이 코드, 설정,
배포 규칙, 세이브를 변경하지 않았다.

## 최신 실행 증거

`Documents/My Games/Skyrim Special Edition/SKSE/BodyChangeNG.log`
(2026-09-11 06:35–06:41 실행):

- Face [Ovl0]: 06:40:34.778, 06:40:36.304 두 선택은 stored/live=true.
  06:40:38.344부터 7회 연속 교체가 ownershipConflict(result=6).
- Body [Ovl1]: 06:40:52.561 첫 선택은 stored/live=true.
  06:40:54.072부터 5회 교체가 같은 결과로 차단.
- Hands [Ovl0]: 06:41:04.929 첫 선택은 stored/live=true.
  06:41:05.684부터 3회 교체가 차단.
- 실패 때 마지막 DDS는 현재 지오메트리에 남아 있다. 반면 저장 DDS 0–7,
  tint/alpha 조회는 모두 absent/false다.
- 이 실행의 `BCNG overlay explicit removal` 기록은 0회.

직접 차단 조건은 `RaceMenuOverlay.cpp`의 이전 슬롯 저장 경로 확인 실패다.
키가 실제 삭제됐는지, 등록 문자열/저장소 조회가 달라졌는지, 어느 실행
주체가 관여하는지는 위 로그만으로 구분할 수 없다. 외부 모드 탓으로
단정하지 않는다. 빈 슬롯으로 옮기거나 저장 키를 다시 만드는 우회는 하지 않았다.

## 설치 DLL 및 독립 검사

- 로드 로그상 UBE 2.0 U.0.7 skee64.dll, Override v1.
- 실제 파일: `D:/TuLED13E/File Mod Skyrim SE/mods/[LED]UBE 2.0/SKSE/Plugins/skee64.dll`
- SHA256: `283EA6F0DF6234B5636D6B03445A57C90369514E61EC3B02DA07F731FCF3469B`
- 고정 MSVC dumpbin으로 AddNodeOverride RVA 0x87C70, GetNodeOverride
  RVA 0x88A40, StringTable::GetString RVA 0xE0540을 다시 읽었다.
  이 DLL의 Add/Get은 액터 FormID를 사용하며 구형 VM handle 추정을 적용하지 않았다.
- 저장 노드 이름은 문자열 내용 hash와 intern된 shared pointer로 조회한다.
  BCNG에서 호출하는 v1 인자 전달/Variant 필드 배치는 이 함수들과 일치한다.

`tools/probe-racemenu-node-registration.py`는 이 **정확한 SHA256 하나만**
허용하는 폐기 가능한 별도 프로세스용 진단이다. Skyrim에 연결하지 않고,
SKSEPlugin_Load나 렌더링/세이브 함수를 호출하지 않는다. OS 로더로 해당 DLL의
전역 컨테이너를 초기화한 뒤, 합성 FormID 참조와 새 문자열 버퍼로 등록/조회를
반복한다. 이 DLL에서 확인한 문자열 intern/refcount 동작만 검사하며 제품 코드에
RVA, 원시 저장소 접근, 수동 shared pointer 참조계수 조작을 추가하지 않는다.

결과: 얼굴·몸·손·발 각각 1,000회, 총 4,000회 교체에서 DDS·tint·alpha 등록과
조회가 유지됐다. 등록에 사용한 임시 문자열 참조를 해제하고 새 이름 버퍼로
다시 조회해도 통과했다. 최초 scalar-only 검사도 통과했다.

한계: 이 검사는 BCNG의 GetNodeProperty 기반 문자열 준비, 실제 게임의 이벤트,
작업 큐, OverlayFix/다른 모드 훅, 렌더링, 저장·로드를 재현하지 않는다.
따라서 제품 통합 회귀 테스트 또는 인게임 해결 증거로 세지 않는다.

## 추가로 확인한 범위와 다음 조사 지점

- 공식 RaceMenu 고정 소스(87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc)의
  OverrideInterface/StringTable 등록·조회·해제 구현을 기존 로컬 감사 저장소에서 대조했다.
- 로드된 OverlayFix의 원본 소스도 조회했지만, 현재 로그만으로 삭제 주체로
  지목할 근거는 없다. skee64-memleak-patch 로그는 이미지 크기 불일치를 보고했다.
- 설치 NiOverrideCleanScript의 전체 노드 삭제는 명시적인 MCM 항목 선택 경로다.
  자동 반복 삭제가 확인된 것이 아니다.

다음에는 실제 게임에서 **등록 직후부터 첫 실패까지 저장소 변경/조회 분기**를
관찰해야 한다. 전 슬롯 저장 키 삭제, 원본 스킨 변경, 다른 모드 비활성화,
소유권 검사 제거를 추측으로 적용하지 않는다. 이번 턴에서는 새 DLL을 빌드하거나
교체하지 않았고 전체 기존 테스트를 재실행하지 않았다(제품 소스 변경 없음).

## 후속: 실행 중 저장소 직접 관찰 (07:05 실행, PID 12408)

사용자가 실패 상태로 둔 툴레드에 `QUERY_INFORMATION | VM_READ`만 사용했다.
게임 메모리 쓰기, 함수 원격 호출, DLL 주입, 스레드 정지, 장비/스킨 조작은
하지 않았다. `tools/read-racemenu-registry-live.py`는 위 SKEE SHA256과
SE 1.5.97, 설치 BCNG 시험 DLL SHA256을 모두 검사하는 한정 진단이다.
이 도구의 내부 저장소 오프셋은 일반 버전 지원 코드가 아니다.

이번 로그:

- 07:05:39.126 Body [Ovl0] HeartPubeL: stored=true, live=true.
- 07:05:39.711 같은 슬롯 HeartPubeM: stored=false, live=true.
- 07:05:40.128 및 .589 이후 선택은 이전 경로 저장 키 부재로 result=6.
- 그 뒤의 저장소 관찰 동안 추가 BCNG 선택 로그나 세이브 재로드 로그는 없었다.

직접 읽은 결과:

1. 실제 Override 전역 객체는 SKEE base + 0x1DC730이다.
   BCNG가 캐시한 Override 포인터도 **같은 객체**였고, Add/Get 가상 함수
   포인터도 각각 감사한 0x87C70/0x88A40을 가리켰다.
   따라서 이번 실행에서 잘못된 별도 인터페이스 객체를 읽는다는 가설은 배제했다.
2. 첫 스냅샷에는 플레이어 노드 등록 1개, 여성 Body [Ovl0]의
   `textures\\dse-soulgem-oven\\veins_cbbe.dds`, alpha=0이 존재했다.
   노드 이름과 StringTable의 intern 포인터/참조계수도 일치했다.
3. 뒤이은 스냅샷에는 actor map 자체가 0개였다. 07:14:22 및 07:15:58에도
   0개였으며, 각 읽기 앞뒤의 actor map 헤더는 동일했다.
   이는 BCNG 문자열 조회만 실패한 것이 아니라 **해당 시점에 등록 자체가
   없었다**는 증거다. 무잠금 외부 스냅샷이므로 삭제 호출자나 정확한 삭제
   시각까지 증명하는 것은 아니다.
4. Add/Get/Remove/Revert 진입부와 registry 코드 RVA 0x87000–0x8F1FF는
   설치 DLL의 디스크 코드와 같았다. 직접 삭제 호출 후보의 CALL/JMP도
   일치했다. 이 범위 밖의 훅, 가상 호출, 다른 모드의 정상 API 호출까지
   없다는 뜻은 아니다.

Soulgem Oven의 설치 소스는 저장한 슬롯 이름을 재사용하고 자기 슬롯의
`RemoveAllNodeNameOverrides`를 호출하는 경로가 있다. 그러나 그것이
actor map 전체가 없어지는 이번 현상의 원인이라는 증거는 없다.
이 모드를 끄거나 그 예약 키를 지우지 않았다.

정적 호출 조사로 RaceMenu의 프리셋 적용, Papyrus 전체/액터별 삭제,
SKSE FormDelete 콜백 경로가 남았다. 설치 SKEE의 FormDelete 콜백
RVA 0x50AA0은 입력의 하위 32비트로 액터 등록을 지우지만, **그 콜백이
이번 소실 때 실행됐다는 관찰은 없다**. 이를 원인으로 단정하거나 핸들을
무기한 유지하는 우회는 하지 않는다.

현재 결론: 저장 키가 없는 상태에서 재선택/색상 변경을 막는 직접 조건은
확인했고, 실제 저장소 소실도 확인했다. **삭제 주체와 근본 수정은 아직
미확정**이다. 읽기 전용 스냅샷에는 이미 지나간 삭제 호출 스택이 남지
않으므로 다음 단계는 삭제 순간의 호출자를 기록하는 한정 시험 진단이다.
사용자가 정상 동작을 확인한 스킨/얼굴 코드와 설치 DLL은 변경하지 않았다.
