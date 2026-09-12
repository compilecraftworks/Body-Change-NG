# 여성 노인·종족별 스킨 및 RSV 점검 (2026-09-11)

## 범위와 검증 수준

현재 작업 트리 소스, 실제 BCNG BodySkin 폴더, 독립 실행 회귀 테스트를 점검했다.
게임 실행·DLL 교체·공개 Release 갱신은 하지 않았다. 운영 코드 변경도 없다.
RSV의 실제 동작을 게임에서 검증한 보고서는 아니다.

## 여성 DDS 선택

`SkinProfiles.cpp::AttachConditionalHumanoidLayers`는 여성·인간형·비UBE에만 적용된다.

- 기본 몸: `femalebody_1`, 손: `femalehands_1`, 얼굴: `femalehead` 계열.
- 노인: 같은 최상위 팩 내부의 `femaleold`에서 몸·손·얼굴을 각각 읽는다.
- 종족별 얼굴: `nordfemale`, `bretonfemale`, `darkelffemale`, `highelffemale`,
  `imperialfemale`, `femaleorc`, `redguardfemale`, `woodelffemale` 디렉터리.
- 오크 얼굴은 `femaleheadorc` 및 `femalehead` 별칭을 받는다.
- 종족별 몸 변형을 별도 선택하는 `raceBody` 구조는 없다.
- UBE는 위 Legacy 여성 조건부 디렉터리 처리에서 제외된다.

`SkinApplicationPlan.cpp::Build`의 채널 우선순위:

1. 몸·손은 기본값 위에 노인용 채널을 대입한다.
2. 별도 발이 없으면 최종 몸 아틀라스를 발에 사용한다. 별도 발이 있으면 그 파일을 사용한다.
3. 얼굴은 기본 → 종족별 → 흡혈귀 → 노인용 순서다. 같은 채널이 겹칠 때만 뒤쪽이 이긴다.
4. 얼굴 디테일은 원래 액터 디테일의 파일명과 맞는 팩 내 파일을 우선하며,
   여러 후보 중 매칭되지 않는 경우 기존 디테일을 유지한다.

노인 판정은 Race EditorID의 `elder`, Voice EditorID의 `old`/`elder` 문자열이다.
종족 판정은 Race EditorID 문자열이다. RSV 키워드·배포 그룹·현재 DDS의 노인 여부를
판정 근거로 읽지 않는다. 따라서 RSV 분류와 BCNG 분류가 항상 같다고 보장할 수 없다.

## 실제 설치 팩

생산 카탈로그 `ScanDirectory`로 다음 두 폴더를 직접 읽었다.

- `D:\TuLED13E\File Mod Skyrim SE\mods\Body Changer NG Skin_Tint\BodySkin`
- `C:\TAKEALOOK\mods\Body Changer NG Skin_Tint\BodySkin`

각각 여성 11개: Legacy 7개 + UBE 4개. 두 폴더의 Legacy 7개는 모두
몸 4채널·손 4채널·얼굴 4채널, 별도 발 0개로 판정됐다.
노인 몸·손·얼굴 및 종족별 얼굴은 모두 0개였다. 흡혈귀 얼굴은 팩별 2~4채널이다.

따라서 현재 이 Legacy 팩을 노인/다른 인간형 종족에게 선택해도 팩의 일반 채널을 쓴다.
RSV의 노인/종족별 노멀맵을 자동으로 존중하거나 팩에 없는 노인 질감을 생성하지 않는다.
팩이 제공하는 일반 노멀맵으로 기존 노인 노멀맵이 대체될 수 있다.
이는 DDS 누락으로 목록을 차단하는 문제와는 별개다.

## 현재 RSV 호환의 실제 범위

`NativeSkinBackend.cpp`는 선택 당시 Skin Armor/Far Skin/Face TXST를 원본으로 보관하고
복제한 그래프의 선택 채널만 변경한다. 매 선택마다 먼저 원본 채널로 되돌린 뒤
새 팩 채널을 쓰므로 이전 BCNG 팩의 누락 채널을 그대로 이어쓰는 설계는 아니다.

- 선택 전 원본이 RSV이면 복원 기준도 그 RSV 네이티브 그래프다.
- 선택 팩에 있는 채널은 대체한다. 없는 채널은 원본을 유지하므로
  BCNG diffuse + RSV normal/detail 같은 부분 혼합은 가능한 정책이다.
- 기본값은 BCNG가 아직 소유한 포인터만 원본으로 돌린다.
  다른 공급자가 이후 교체한 포인터는 강제로 덮어쓰지 않는다.
- 적용 중 다른 포인터로 바뀌면 TXST 경로에 `actors\character\rsv\`가 있는 경우
  새 원본으로 받아 다시 구성하는 예외가 있다. RSV 플러그인/키워드 기반 식별이 아니다.
- 같은 폼 포인터의 내부 DDS만 외부에서 바꾸는 경우는 이 포인터 변경 감지만으로
  새 원본임을 알아내지 못한다.
- 얼굴 부착 경로는 선택한 채널만 공급하며 FaceGen tint 자원은 명시적으로 보존한다.
- RSV 전용 NiOverride 키를 발견·중지·대체·복원하는 연동은 현재 소스에서 확인되지 않는다.
  따라서 'RSV가 별도 override도 공급하는 경우 전체 세트 교체/복원'은 검증된 기능이 아니다.
  이는 모든 RSV 버전이 실제로 해당 override를 쓴다는 주장이 아니다.

확인한 양쪽 MO2의 현재 `modlist.txt`/`plugins.txt`에서는 RSV/Racial Skin Variance
이름의 항목이 발견되지 않았다. 별도 이름으로 합쳐진 구성까지 부재를 증명한 것은 아니다.
RSV 공식 Nexus 페이지(81668)는 비로그인 성인 콘텐츠 제한으로 본문 확인이 불가능했다.
그러므로 특정 RSV 버전의 실제 원본 스크립트·폼 동작과의 완전 호환은 미확인이다.

## 테스트

- 기존 카탈로그 테스트: 여성 노인 몸·손·얼굴 노멀맵 분리와 8종족 얼굴 분리 통과.
- 추가 테스트: CBBE/UNP 양쪽에서 종족·흡혈귀·노인 얼굴 우선순위 8조합,
  몸·손·발 채널 분리 통과. 현재 동작을 고정하는 특성 테스트이며 RSV 엔진 통합 테스트가 아니다.
- 진단 모드: 남성뿐 아니라 여성 프로필과 조건부 DDS도 출력하도록 확장.
- 변경한 테스트 2개 Release 빌드 통과. 전체 기존 테스트 실행 파일 20개 모두 통과.

## 결론

여성 기본/노인/종족별 채널 분리는 코드와 테스트에서 확인된다. 하지만 설치 팩에
해당 변형이 없고, RSV 분류를 직접 읽지도 않는다. 현재 RSV 호환은
네이티브 그래프 원본 보존·선택 채널 대체·소유권을 확인한 기본값 복원 범위이며,
RSV 외형 전체를 원자적으로 대체하는 완성된 공급자 어댑터라고 표현하면 안 된다.
