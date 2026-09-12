# 얼굴 지연 개선: 버전별 NiOverride 직접 호출

## 근거

툴레드 10:02:31~32 로그: 몸 TXST 반영 31.927, 재구축 이벤트 32.016,
얼굴 시작 32.066, 채널별 확인 32.163/32.263/32.364/32.462.
몸 로그는 화면 표시 시각이 아니지만, 얼굴 채널당 약 0.1초의 순차 Papyrus
왕복이 존재한다. 이번 변경은 이 채널별 VM 왕복을 제거한다.

## 구조

| Override 인터페이스 | 얼굴 처리 |
|---|---|
| v1 | 구형 concrete ABI, SetNodeProperty(immediate=true), 구형 문자열 등록 |
| v2 | 공개 wrapper ABI, SetNodeProperty(immediate=true), Get/SetVariant |
| v2 초과 | 알려진 v2 prefix만 사용. 기존 prefix 유지 전제이며 미래 바이너리 검증은 아님 |
| 직접 인터페이스 없음 또는 v0 | 기존 Papyrus 폴백 |

RaceMenu 파일 버전 allowlist를 만들지 않았다. 얼굴은 Overlay 인터페이스 버전,
오버레이 노드/슬롯/설치 여부에 의존하지 않는다. 지원 Skyrim SE/AE 런타임
안에서만 직접 ABI를 사용하며 VR 및 신규 미검증 게임 버전을 추가하지 않았다.

- FaceSkinNodeAccess.cpp/.h: 실제 Face HeadPart 이름과 채널 0/1/2/3/7만 취급.
  현행 Batch의 Read/Write/Remove를 동기로 실행한다.
- FaceSkinOverrides.cpp: 원본 보존/복원/소유권/저장 구조 및 세대 취소는 그대로,
  transport만 변경한다. 게임 스레드의 동일 batch에서 채널들을 연속 처리한다.
  피부/노멀/디테일/광택이 프레임마다 하나씩 뒤따라오는 VM 대기를 없앤다.
- 원래 노드에서 다른 HeadPart로 바뀐 뒤 과거 노드 키를 정리하는 예외 경로는
  기존 Papyrus 처리를 유지한다. 미사용 잔재가 아니라 실제 폴백이다.
- 구형 v1은 임의 std::shared_ptr 문자열을 영구 저장하지 않는다.
  SetNodeProperty → GetNodeProperty로 RaceMenu 자체 interned 문자열 확보 →
  AddNodeOverride 순서로 처리한다. GetNodeProperty의 PackValue/SetString이
  g_stringTable.GetString을 사용하는 공식 구현을 확인했다.
- 신형 v2는 wrapper가 문자열 변환을 소유한다. 1/3인칭에 같은 얼굴 객체가
  공유되면 중복 적용하지 않는다. 구형은 원래 함수 내부에서 두 뷰를 처리한다.
- 빈 경로 쓰기 금지, 실제 detail 원본 경로 캡처 및 기존 baseline 보완 유지.
  기본값/외부 provider의 원본 복원 우선순위와 플레이어 영구 키/NPC 비영구 정책 유지.
- 재질/텍스처를 직접 수정하거나 새 영구 Geometry/Material 포인터를 저장하지 않는다.
  NodeAccess는 batch 수명 동안 plugin interface만 빌려 사용하며 전역 map을 추가하지 않는다.
- 3D 재구축 완료 대기는 유지한다. 완료 이벤트 뒤에는 입력 틱 기반 actor queue로
  되돌아가지 않고 SKSE 게임 작업 큐에 바로 넘긴다. 세대/epoch 검사는 유지하므로
  반복 이벤트와 이전 선택은 폐기된다. 이 한 번의 게임 스레드 경계와 엔진의 3D
  재구축 자체는 안전하게 없앨 수 없다.

## ABI 공통화 및 감사

RaceMenuOverrideABI.h로 기존 오버레이의 v1/v2 선언과 LegacyVariant를 그대로
이동했다. 오버레이 실행 로직이나 슬롯 정책은 변경하지 않았다.
tools/audit-racemenu-history.py가 공통 헤더를 읽도록 수정했다.

공식 소스 11개 고정 커밋 감사 통과:

```
86c890a7f89a3d31f3f80362b0b44f1d83d76fe3
7ffff9afb35ce9cae5e26f8bdbf86367b75e29b8
87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc
867458e0d6a9f45dea5599bf694d6198332e3b0b
4cf6d628e7a74e1204fc5bdc0fc4885de6481a03
e779c68ce5449f713c7b551c7b7173208e7f83c0
c1b408f363d645d354a8b548d064b3429ba1da5d
8adc4b635ef2c6eef5befef3578ecabfb059a5ee
7694eabfdf9e675cc28a8524afecb96aa5cd8a4b
348607e9ae5f360ccab0d623e8b0d8f42e586fa6
9ebcb733e17be695f994cd2e9cc383043446bc02
```

구형 Override prefix 28개, 신형 36개의 순서와 호출되는 인자/반환 형식,
신형 visitor 계약을 확인했다. 이는 모든 배포 바이너리의 실행 검증은 아니다.

공식 근거:
- https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/OverrideInterface.cpp
- https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/OverrideVariant.h
- https://github.com/expired6978/SKSE64Plugins/blob/7694eabfdf9e675cc28a8524afecb96aa5cd8a4b/skee64/OverrideInterface.cpp

## 빌드 / 테스트

- Release 성공. EXCLUSIVE_SKYRIM_FLAT, VR off.
- form_delete_guard_trial / native_addon_txst_trial 활성, face_skin_trace 비활성.
- 전체 C++ 테스트 실행 파일 22개 통과, Python 감사 19개 통과.
- 신규: v1/v2/상위버전 prefix/인터페이스 없음/지원 외 런타임 분기,
  legacy 문자열 쓰기→intern→저장 순서, NPC 비영구 처리,
  잘못된 readback과 빈 경로의 영구 키 생성 방지.
- 기존 디테일 복원 및 저장 왕복 테스트 유지.

시험 DLL:
`build/v1.2.0/trials/native-addon-txst/windows/x64/release/BodyChangeNG.dll`

SHA256: `D3840A511753521FC9165B6F45FB45AE64021FBA26957676937BCBEE93FE8C7E`

게임 종료를 확인한 뒤 툴레드와 떼껄룩 MO2 Body Change NG의 DLL에 반영하고
해시 일치를 확인했다. 설정/스킨팩/세이브, 공개 GitHub Release는 갱신하지 않았다.

인게임에서 batch finished 로그의 transport와 elapsed-ms, 반복 선택/기본값/저장 후
로드를 확인해야 한다. 아직 개선 후 실측 시간이나 보라색 무재발을 주장하지 않는다.
