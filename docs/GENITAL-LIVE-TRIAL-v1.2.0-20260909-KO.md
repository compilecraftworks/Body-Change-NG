# v1.2.0 성기 스킨 Live Material 시험 빌드 — 2026-09-09

## 구현 구조

| 대상 | 적용 경로 | BCNG 저장 상태 |
|---|---|---|
| 몸·손·발 | 액터별 Native Skin Armor / TXST | 선택한 BodySkin ID |
| 얼굴 | 액터별 Native Face TXST | 같은 BodySkin ID |
| SOS 남성 | 현재 슬롯 52 지오메트리의 독립 Live Material | 선택한 남성 BodySkin ID |
| SOS 여성 후타나리 | 현재 슬롯 52 지오메트리의 독립 Live Material | 독립 FutanariSkin ID |
| TNG 남성 | 현재 슬롯 52 지오메트리의 독립 Live Material | 선택한 남성 BodySkin ID |
| TNG 여성 후타나리 | 현재 슬롯 52 지오메트리의 독립 Live Material | 독립 FutanariSkin ID |

SOS와 TNG 성기 스킨은 하나의 `LiveAddonSkinBackend`를 사용한다. BCNG는 공급자를
분기하지 않고 현재 Biped에 실제 로드된 슬롯 52 지오메트리를 같은 조회 함수로 찾는다.
그 대상이 착용 ARMO 소유인지 Skin Armor 소유인지는 진단용 정보일 뿐 적용 조건이
아니다. ShaderProperty 복제, Material 복제, TextureSet 복제, DDS 적용, 기본값 복원과
재적용까지 완전히 같은 경로다.

성기 스킨은 RaceMenu 텍스처 오버라이드 값을 만들거나 조회하거나 제거하지 않는다.
SOS/TNG 장착·탈착과 애드온 선택은 각 원본 모드가 계속 소유하며 BCNG는 장비를 대신
교체하지 않는다. 이전 시험 DLL이 저장한 RaceMenu 키는 1.2 런타임에서 건드리지 않는다.

일반 BodySkin도 몸·손·발은 Native Skin Armor/TXST, 얼굴은 Native Face TXST만 새로
적용한다. 이전 일반 스킨 RaceMenu 적용·재시도·검증·정리 경로와 RSV 비영구 얼굴
브리지는 모두 제거했다. RSV 액터에서는 현재 Skin Armor/Face TXST를 원본 공급자
그래프로 잡고 BCNG 선택 중에는 그 네이티브 그래프를 대체하며, Default에서 보관한
RSV 그래프를 복원한다.

## Live Material 격리와 수명

BCNG는 원본 ARMO, ARMA, 공유 Material 또는 공유 TextureSet을 수정하지 않는다.
현재 액터에 로드된 정확한 슬롯 52 지오메트리마다 ShaderProperty와 Material,
TextureSet을 별도로 복제한 뒤 복제본에만 DDS를 적용한다. 그러므로 같은 애드온 원본을
사용하는 다른 액터에게 선택값이 전파되지 않는다.

기본 텍스처 경로는 지오메트리의 `NiStringsExtraData`에 기록한다. 프로세스 전역에
지오메트리·Material 포인터를 보관하지 않으므로 탈착이나 3D 재생성으로 기존 객체가
삭제되어도 죽은 포인터를 다시 사용하지 않는다. 새 지오메트리에는 기준 데이터가
없으므로 저장된 액터 선택값을 다시 적용한다. 같은 지오메트리에 같은 상태가 이미
적용되어 있으면 중복 작업하지 않는다.

## TNG와 일반 바디 Skin Armor의 관계

TNG DLL 함수 후킹, 주소 탐색, 호출 규약 의존, 전용 성기 ARMA/TXST 복제 경로는 없다.
일반 몸·손·발 스킨을 적용할 때 현재 Skin Armor가 TNG 조합 결과라면, 그 공개 Form
구성에서 몸 ARMA만 액터 전용 Native Skin Armor 부모로 복제하고 슬롯 52와
`TNG_CustomSkin` 표식은 넣지 않는다. 이후 TNG가 자신의 정상 흐름으로 슬롯 52를
결합한다. BCNG는 생성된 현재 TNG 슬롯 52 지오메트리에 공용 Live Material Adapter를
적용할 뿐이다.

TNG가 BCNG 부모에서 새 Skin Armor를 만들었는지는 비성기 ARMA 포인터가 그대로
포함되어 있는지와 `TNG_CustomSkin` 키워드로 제한해서 확인한다. 이는 BCNG가 소유한
일반 바디 Native Skin Armor를 안전하게 복원하기 위한 확인이며 성기 DDS 적용 경로를
분기하거나 TNG 내부 함수를 호출하기 위한 것이 아니다.

## 영구 선택과 자동 재적용

Live Material 객체 자체는 세이브에 저장하지 않는다. 대신 ActorRegistry에 액터별
마지막 남성 BodySkin ID와 FutanariSkin ID를 저장한다. 다음에 공급자가 새 슬롯 52
지오메트리를 만들면 그 선택을 자동으로 다시 적용한다.

- 세이브 로드 및 액터 3D 로드
- SOS/TNG 애드온 탈착·재장착
- 셀 재로드
- `QueueNiNodeUpdate` 등으로 인한 3D 재생성
- RaceMenu 종료 뒤 외형 재생성

여성 후타나리의 선택 상태와 장착 상태는 서로 독립이다. 장비를 벗거나 미장착 상태로
저장해도 마지막 FutanariSkin ID를 삭제하지 않는다. 사용자가 BCNG에서 명시적으로
기본값을 선택했을 때만 저장 선택을 제거한다.

## 자동 회귀 범위

- ActorState ASTR v3의 배포 NPC Body, Skin, 부위별 Overlay, 적용 서명, 의상 보정 서명,
  FutanariSkin 선택 저장/로드 왕복
- 구 ASTR v1 상태의 호환 읽기와 손상된 문자열 인덱스 거부
- 후타 미장착 저장 → 게임 세션 초기화 → 로드 → 재장착 선택 유지
- 명시적 기본값 선택 후 이전 후타 선택이 되살아나지 않음
- SOS/TNG가 소유 형태와 무관하게 동일한 정확한 슬롯 52 승인 규칙을 사용함
- 같은 장비 이벤트 무작업과 1,000회 3D 이벤트의 작업 병합
- 새 지오메트리는 동일 주소여도 기준 데이터 없이는 재적용 대상으로 처리
- 두 액터가 같은 원본 애드온을 사용해도 저장·세션 적용 상태가 분리됨
- 몸·손·발·얼굴 Native TXST, BodySlide, NPC 배포, 규칙 UI 등 기존 전체 회귀 테스트

자동 테스트는 Skyrim 렌더러를 실행하는 end-to-end 검증이 아니다. 실제 SOS/TNG의
지오메트리 노드 선택, 렌더링, 탈착/재조합, 장시간 Material 수명과 CTD 여부는 아래
게임 시험으로 최종 확인해야 한다.

## 게임 시험 체크리스트

1. 툴레드 SOS 남성: 선택 → 저장 → 종료 → 로드 → 같은 성기 DDS 확인.
2. 툴레드 SOS 남성: 탈착/재장착 및 반복 `QueueNiNodeUpdate` 뒤 같은 DDS 확인.
3. 툴레드 SOS 여성 후타: 선택 → 탈착 → 저장/로드 → 재장착 후 마지막 DDS 확인.
4. 떼껄룩 TNG 남성·후타: 선택 → 저장/로드 및 탈착/재장착 뒤 같은 DDS 확인.
5. 로그에서 `addon male Live Material` 또는 `addon futanari Live Material` 적용 확인.
6. 같은 SOS/TNG 원본을 쓰는 NPC 둘에게 서로 다른 스킨을 적용해 상호 전파 확인.
7. 몸·손·발·얼굴, BodySlide 프리셋, NPC 배포 및 틴트마스크 회귀 확인.
8. 선택/기본값/탈착/3D 재생성을 반복해 CTD, 메모리 증가 및 슬롯 52 중복 확인.

## 시험 파일

`build/trials/GENITAL-LIVE-20260909/SKSE/Plugins/BodyChangeNG.dll`

SHA-256: `5AF09A6558947EA0ED68A64269DD9CF2354F19BA27A373A039F36D74FFDEF674`

공개 GitHub v1.2.0 Release는 게임 검증 전에는 변경하지 않는다.
