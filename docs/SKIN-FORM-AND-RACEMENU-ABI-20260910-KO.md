# 2026-09-10 Native 스킨 복제 및 RaceMenu ABI 재점검

## 재현과 원인 구분

이전 시험본 AFC170E...는 실패했다. 툴레드 02:09 로그에서 DLL 한 개만 로드된
상태로 최초 스킨 선택 후 QueueNiNodeUpdate 한 번에 몸 소실이 재현됐다.
중복 DLL이나 연속 클릭을 이번 증상의 확정 원인으로 설명한 판단은 철회한다.
플레이어만 문제가 있고 NPC는 정상이라는 게임 검증 결과도 없다.

### Native 스킨: 불완전한 ARMA 복제

CommonLib 헤더와 실제 설치된 Skyrim SE 1.5.97 실행 파일의 가상함수 표를 대조했다.
ARMA, TXST, FLST의 Copy(0x2F)는 모두 TESForm의 기본 Copy와 동일한 주소
(RVA 0x195370)다. ARMO는 별도 Copy(RVA 0x2285F0)를 갖는다.
기존 코드는 모든 종류에서 CreateDuplicateForm만 호출해 내용까지 복사됐다고
가정했다. ARMA의 메시/슬롯/종족 구성이 빠진 채 3D를 재구축할 수 있었다.

- ARMA: 양 성별의 1/3인칭 모델, 모델 교체 목록, 기본/추가 종족, 슬롯,
  DNAM 우선순위/범위/무기 조절, 발소리/아트, TXST/FLST 참조를 명시적으로 복사.
- 모델은 엔진 CopyComponent, 배열은 BSTArray의 소유 저장소 복사를 사용.
- 부착 전 모델 경로·교체 항목·종족·슬롯 비교. ARMO의 공유 배열도 거부.
- TXST: 플래그(모델 공간 노멀 포함), 원본 경로, bounds, 독립 decal data 복사.
- FLST: 원본 항목과 현재 스크립트 추가 항목을 독립 배열로 실체화한 뒤 TXST 복제.
- 액터별 그래프는 기존 인스턴스를 재사용한다. Geometry 포인터는 보관하지 않는다.
- 임시 3D 부재나 캐시 DDS 이름 때문에 목록 분류가 바뀌지 않도록 원본 바디 계열을
  참조한다. 종족/성별/실제 Skin Armor 소유권이 달라지면 이 증거를 사용하지 않는다.

일반 Body/Hands/Feet/Face는 Native TXST 경로를 유지한다. 일반 스킨용
NiOverride 또는 라이브 재질 덧칠 경로는 추가하지 않았다.

### Overlay v1: 같은 크기지만 다른 문자열 전달 규약

설치된 두 구형 DLL 모두 AddNodeOverride에서 R9를 문자열 주소로 직접 읽는다.
CommonLib RE::BSFixedString은 비 trivial 소멸자 때문에 MSVC x64에서 객체 주소로
간접 전달된다. sizeof가 8이라는 검사만으로는 이 불일치를 잡을 수 없다.
잘못된 이름으로 저장하고 같은 잘못된 이름으로 조회하면 stored=true여도 실제
Body [Ovl#] 노드에는 적용되지 않을 수 있다.

- v1 경계의 이름 인자를 trivial pointer-sized LegacyNodeName으로 분리했다.
  소유 RE::BSFixedString은 호출이 끝날 때까지 살아 있으며 raw 포인터를 지연 보관하지 않는다.
- Add/Get/Remove/Revert/GetNodeProperty의 정확한 인자 전달 형식을 수정했다.
- 구형 AddNodeOverride는 전달된 문자열을 자체 저장 테이블에 intern하지 않는다.
  BCNG의 make_shared 문자열을 그대로 등록하면 GetStringID가 -1을 반환한다.
  v1은 정확한 페인트 노드에 SetNodeProperty 후 GetNodeProperty로 RaceMenu가
  intern한 값을 받아 등록한다. BCNG 선택 직렬화와 RaceMenu 문자열 저장 모두 유지한다.
- 지연 작업은 lease/액터 핸들/노드 이름/선택 문자열만 보유한다. 취소된 지연 작업이
  alpha/tint 키만 남기지 않도록 v1의 값들은 후속 작업에서 함께 등록한다.
- v1 setter가 양 시점을 처리하므로 텍스처를 다시 ApplyNodeOverrides하지 않는다.
- 다른 모드의 live diffuse는 alpha=0이어도 빈 슬롯으로 간주하지 않는다.
- v2는 공식 const char* + SetVariant/GetVariant wrapper를 그대로 사용한다.
  wrapper의 SetValueVariant가 자체 문자열 저장을 처리한다.

## 공식 소스 이력 대조

원본: [expired6978/SKSE64Plugins](https://github.com/expired6978/SKSE64Plugins).
아래는 확인한 주요 ABI 이력 지점이며 모든 배포 파일을 게임 실행한 목록이 아니다.

| 지점 | 불변 커밋 | Overlay/Override |
|---|---|---|
| 0.4.12 | 86c890a7f89a3d31f3f80362b0b44f1d83d76fe3 | v1 |
| 0.4.14 | 7ffff9afb35ce9cae5e26f8bdbf86367b75e29b8 | v1 |
| 0.4.16 시점 | 87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc | v1 |
| 초기 AE 1.6.318 | 867458e0d6a9f45dea5599bf694d6198332e3b0b | v1 |
| 1.6.640 대응 | c1b408f363d645d354a8b548d064b3429ba1da5d | v1 |
| 공개 wrapper 도입 | 7694eabfdf9e675cc28a8524afecb96aa5cd8a4b | v2 |
| 1.6.1170 대응 | 348607e9ae5f360ccab0d623e8b0d8f42e586fa6 | v2 |
| 1.7.99 대응 | 9ebcb733e17be695f994cd2e9cc383043446bc02 | v2 |

tools/audit-racemenu-history.py로 각 지점의 가상함수 순서, 호출 인자/반환 형식,
v2 visitor 계약을 비교했다. Reserved 슬롯은 순서만 비교하며 호출하지 않는다.
구형 BSFixedString의 기계어 전달 형식은 아래 설치 바이너리와 별도로 대조했다.
BodyMorph의 실제 공통 접두부는 VisitActors까지다. 사용하지 않는 ClearMorphCache
선언을 제거하고, 옛 UInt32/새 skee_u64 차이가 있는 SetCacheLimit은 미호출 슬롯으로
격리했다. 사용하는 바디모프 함수의 위치와 형식은 모두 일치했다.

## 설치 바이너리

- 툴레드 Race Menu/skee64.dll:
  255C0DB0BA5FF14640CC6D75CCDDFB24474B498D35B77F3140CCB05EB80E7273
  - Override vtable RVA 0x172B30, AddNodeOverride RVA 0x84320.
- 툴레드 [LED]UBE 2.0/skee64.dll:
  283EA6F0DF6234B5636D6B03445A57C90369514E61EC3B02DA07F731FCF3469B
  - Override vtable RVA 0x180490, AddNodeOverride RVA 0x87C70.
- TAKEALOOK RaceMenu/skee64.dll:
  5225E4E3B185E6FC57C8D31B0CEDBE5A030A951D9A744D33071B64C45A38C208
  - Override vtable RVA 0x1E6290, 공개 v2 구조.

tools/audit-native-form-vtables.py, tools/audit-racemenu-vtable.py는 바이너리를
읽기만 하는 보조 도구다. 엔진 파일 수정/후킹 주소 추가는 하지 않았다.

## 검증 및 한계

- Release 빌드, 기존 테스트 17개 전체 통과.
- 시험 DLL SHA-256:
  `1C1D08E59DEAE8D6637B5608A780CFC150869118847A34E92A84AEF77E1E1331`
  툴레드 D:/TuLED13E 및 C:/TAKEALOOK의 기존 BCNG DLL만 백업 없이 교체하고
  설치 파일 해시를 확인했다. 다른 설정/에셋 파일은 이번에 교체하지 않았다.
- 추가 검사: 빈 ARMA 거부, 모델/종족/슬롯/추가종족 보존, 복제 후 원본 배열 격리,
  legacy 문자열 trivial ABI, 외부 오버레이 슬롯 보호.
- 공식 소스 이력 8개 지점의 인터페이스 계약 대조 통과.
- Native 동적 Form의 실제 엔진 렌더링/세이브 수명과 각 게임 버전별 동작은
  이 단위 테스트로 증명되지 않는다. 인게임 메모리 누수 없음/CTD 없음도 단정하지 않는다.
- 1.7.99 RaceMenu의 C++ 인터페이스 대조는 BCNG의 게임 1.7.99 지원을 뜻하지 않는다.
  BCNG의 13개 기존 SE/AE 런타임 허용 표는 변경하지 않았다. VR 빌드는 없다.
- 공개 Release, 세이브, 사용자 규칙/설정, RaceMenu 원본 DLL은 변경하지 않는다.

## 시험 순서

1. 툴레드 재시작 후 기존 세이브에서 스킨 최초 1회 선택: 몸/손/발/얼굴 표시와 변경.
2. 다른 스킨/기본값 왕복: 목록 유지, 의상 착탈 후 표시 유지.
3. 오버레이 최초 선택/교체/기본값: 실제 표시와 타 모드 페인트 보호.
4. 다른 슬롯에 저장 → 게임 종료 → 로드: 선택과 화면 일치.
5. NPC에서도 동일 항목과 NPC 배포 상태 확인. 플레이어 검증으로 NPC 성공을 추정하지 않는다.

새 로그의 ARMA clone verified와 overlay verification의 stored/live/alpha/diffuse를
화면 결과와 대조해야 한다. 공개 배포는 이 시험이 끝난 뒤 판단한다.
