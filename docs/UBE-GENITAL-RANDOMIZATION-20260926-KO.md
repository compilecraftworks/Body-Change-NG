# UBE/Necoco 여성 생식기 무작위화 — 2026-09-26

## 범위와 기존 동작 보존

1.3.4 작업본. CBBE 3BA/BHUNP의 `BodyRandomizationPolicy.h` 계산은 수정하지 않는다. UBE 전용 분기는 여성 플레이어와 NPC의 바디프리셋 확정 시만 실행한다. 미리보기·의상 보정에서는 다시 추첨하지 않는다. 유두 랜덤화는 별도 `UBE-NIPPLE-RANDOMIZATION-20260926-KO.md`에 정리했다. 직접 확정과 NPC 배포 모두 같은 적용 경로이며 배포 조건은 필수가 아니다. 옵션만 켜서 모든 월드 NPC를 바꾸지는 않는다.

값은 BCNG 프리셋 키의 절대 목표값이며 현재 값에 반복 가산하지 않는다. 기존 다른 모프 보존 옵션·미리보기 상쇄·저장·기본 복원 경로를 사용한다. 옵션 OFF 후 프리셋 재적용 시 XML 값으로 복원한다. 기존 재적용 정책처럼 확정 적용이 다시 실행되면 재추첨할 수 있으며, 별도 영구 난수 시드는 만들지 않는다.

## 자료와 판단의 구분

두부팩 활성 모드와 overwrite 승리 경로를 반영해 XML 81개에서 UBE 기록 64개, 일반 여성 프리셋 42개를 분석했다. 생식기 값 조합으로 중복을 제거하면 24개다. `Vagina_spread`는 40개가 0/생략, Fitness_v4/v5 두 개가 -6이며 두 프리셋의 생식기 조합은 동일하다. 따라서 -6도 독립적인 두 사례가 아니고, 다른 슬라이더와 무관한 안전 한계로 간주하지 않는다.

사용한 자산:

- `D:\TOFU\MO2\mods\UBE 2.0\CalienteTools\BodySlide\ShapeData\UBE SE 2.0 Release Body\UBE SE 2.0 Release Body.osd` — SHA256 `a6ea4a6eaefe7fee78568b34570bc50e0e52d0c269b6ba52b6d8ae643c18cda7`
- `D:\TOFU\MO2\mods\h4 UBE SE - Necoco Body v2.1\CalienteTools\BodySlide\ShapeData\UBE SE 2.0 - Necoco Body\UBE SE 2.0 - Necoco Body.osd` — SHA256 `3c3c13495288c212a3c5fbe7d260aeb5b545c19e1ab7672f1653b816754b31f0`
- `D:\TOFU\MO2\mods\CBBE 3BA\CalienteTools\BodySlide\ShapeData\SE 3BBB Amazing\SE 3BBB Body Amazing v2.osd` — SHA256 `5b30e46af95ea15ff4b4d0e0442e689acef163b1f3111e13802df9ceb7df9f4d`

각 원본 NIF의 몸 Shape/Scene Root/skin 변환은 단위 스케일이며 회전도 동일 또는 부동소수점 오차 수준이다. OSD 100%에서 X축 최대 이동량: UBE/Necoco Vagina_spread 0.664782, 3BA VaginaHole 바깥 0.248447/내부 0.375413. 3BA VaginaHole+Labiaspread를 각각 100으로 합치면 0.368340/0.576709다. UBE는 Z축 이동도 더 커서 이 숫자들은 전체 외형의 환산표가 아니다. 서로 다른 토폴로지·시작 형태·다른 모프와의 합성까지 같다는 뜻이 아니다.

공식 구현 참고: [BodySlide DiffData.cpp](https://github.com/ousnius/BodySlide-and-Outfit-Studio/blob/dev/src/components/DiffData.cpp), [BodySlide TriFile.cpp](https://github.com/ousnius/BodySlide-and-Outfit-Studio/blob/dev/src/files/TriFile.cpp), [RaceMenu BodyMorphInterface.cpp](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/BodyMorphInterface.cpp). 조사 참고일 뿐 빌드 의존성은 추가하지 않는다.

## 확정한 설계 범위

아래는 제작자 권장값이나 인게임 검증된 안전 범위가 아니라, 자료를 참고해 선택한 BCNG 설계값이다. 표는 BodySlide 퍼센트 단위이며 적용 시 100으로 나눈다. 분기는 정수 0..99 추첨으로 정확히 20/60/20%다. 같은 분기에서는 난수 하나를 공유하여 각 행의 시작→끝을 함께 보간한다. 개별 슬라이더의 최대치를 독립 추첨하지 않는다.

| 슬라이더 | Innie 20% | Average 60% | Outie 20% |
|---|---:|---:|---:|
| PubicAreaSize p\|n | 10→25 | 5→30 | 5→25 |
| PubicAreaHilly | 25→45 | 10→35 | 10→30 |
| PussyCute | 50→80 | 50→15 | 20→0 |
| Vagina_shape | 0→10 | 5→25 | 20→45 |
| Vagina_shape_wider | 0→5 | 0→10 | 5→20 |
| Vagina_Fantasy | 0→20 | 20→50 | 40→70 |
| ClitorisErection | 0→15 | 10→30 | 20→45 |
| Vagina_spread | -6→2 | -6→15 | 0→35 |
| AnusSize | 15→30 | 20→40 | 25→50 |
| BiggerAnus | 10→20 | 15→30 | 20→35 |
| AnusTriangular | 20→40 | 30→60 | 40→80 |

`Vagina_spread` 양수 상한은 3BA VaginaHole의 -20..5/-20..40/0..100 분기와 바깥 메시 이동량 비율 약 0.374를 참고해 2/15/35로 잡았다. 음수 하한은 설치 프리셋에서 관측한 -6을 사용했다. 단순 동일 외형 환산이나 검증된 충돌 방지 공식이 아니다. 나머지 범위도 관측 최댓값을 그대로 쓰지 않고 분기별로 구성한 설계값이다.

Necoco 추가 5개(실제 로드된 BODYTRI에 비영 위치 모프가 있을 때만):

| 슬라이더 | Innie | Average | Outie |
|---|---:|---:|---:|
| Anus_creases | 5→0 | 5→0 | 5→0 |
| Anus_creases_1 | 0→5 | 0→5 | 0→5 |
| BiggerAnus_2 | 0→3 | 0→5 | 5→10 |
| Shy_Pussy | 5→10 | 5→0 | 0→0 |
| Necoco_Body_Addon_Lewd_Zone | 0→5 | 5→10 | 10→20 |

추가 항목은 프리셋 표본이 부족하므로 약하게 시작하는 설계값이다. 두 주름 모프는 합계 5를 공유한다. `AnusSpread`는 제외(0 강제 기록도 안 함)하고 XML·타 모드의 원래 값은 기존 소유권 정책에 따른다. 음경/고환 슬라이더는 추가하지 않는다.

## TRI 판독·수명·성능

로드된 3D에서 타입 확인한 NiStringExtraData의 BODYTRI 경로를 찾는다. 게임 리소스 스트림을 사용해 MO2/BSA 승리 파일을 읽으며 RaceMenu의 비공개 ABI는 호출하지 않는다. PIRT와 구형 body TRI의 위치 모프만 허용하고, UV에만 있는 이름·빈 모프·0배율·0변형은 추가 항목 지원으로 보지 않는다. 모드 설치명이나 XML 문자열만으로 Necoco 지원을 추정하지 않는다.

파일/카운트/누락 데이터의 경계를 검사한다. 정점 전체 배열을 보관하지 않으며, 최대 4 KiB 묶음 버퍼로 필요한 항목만 판독한다. 세션 캐시는 경로 최대 64개와 5비트 결과만 보관하고 게임 세션 초기화에서 비운다. 액터·3D·엔진 포인터를 캐시에 남기지 않는다. 프레임별 검사나 디스크 쓰기, 새 저장 형식은 없다. 지원 정보를 못 읽어도 기본 프리셋과 UBE 공통 11개 처리는 막지 않는다.

두부팩 생성 TRI `h5 BODYSLIDE OUTPUT (UBE)\Meshes\!UBE\Body\femalebody_tangent.tri`(SHA256 `f4f291ec9601eaf4156eb9a66d4ed6ffecab2cdbf76882e6b6697524f09c2583`)에는 추가 5개가 없으며 0비트 결과다. Necoco 프로젝트가 설치되어 있어도 생성된 TRI가 기본 UBE이면 추가 항목은 적용하지 않는다. 다른 TRI로 교체한 뒤에는 재시작하고 프리셋을 재적용해야 한다.

## 검증

- 순수 계산: 3분기×101보간점×32지원마스크 = 9,696개 조합, 20/60/20 분기 개수, 유한값·범위·연동·제외 항목 보존.
- TRI: packed/legacy, 513정점 배치 경계, 모든 바이트 절단 지점, UV 전용·대소문자·빈 모프·0변형·0/무한 배율·과대 카운트와 실제 두부팩 파일.
- Release 전체 빌드 및 오프라인 회귀 실행 파일 40개 통과. 유두 랜덤화 시험 외에 원본 OBody 수치, 미리보기 복원, 모프 키, 저장·배포·런타임 레이아웃·스킨·입력 시험을 포함한다. 기존 3BA/BHUNP 수치 정책 파일은 변경하지 않았다.
- 인게임 시각 검사 및 엔진 전체 누수 측정은 수행하지 않았다. 최대 이동량 비교나 코드 시험만으로 모든 프리셋 조합의 시각적 안전성을 보장하지 않는다.
