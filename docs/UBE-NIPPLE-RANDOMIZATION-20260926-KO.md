# UBE/Necoco 여성 유두 랜덤화 — 2026-09-26

## 확정 범위

1.3.4 작업본. 사용자의 최종 지시에 따라 임시로 만들었던 일반형/약한 돌출/봉긋/함몰의 배타적 4분기와 77/15/6/2 확률은 제거했다. `BodyRandomizationPolicy.h::GenerateNippleMorphs`를 그대로 호출한다. OBody NG 4.4.3과 같은 독립 추첨, 호출 순서, 조건부 난수 호출, 함몰 안의 50% 분기를 사용한다. 기존 3BA/BHUNP 수치 정책 파일은 변경하지 않는다.

UBE의 모프 이름과 변형량은 3BA와 다르므로 추첨 결과만 별도 변환한다. **확률·분기는 동일하지만 UBE 수치나 외형까지 3BA와 동일하다는 뜻은 아니다.** 별도 UBE 형태 종류, 임의의 추가 확률, 공유 형태 난수, 함몰 당첨 시 다른 슬라이더를 강제로 지우는 처리는 없다. 봉긋함·돌출 감소·함몰이 함께 당첨될 수 있다.

여성 플레이어와 NPC 모두 바디프리셋 확정 경로에서만 실행한다. 일반 미리보기·의상 보정 갱신에는 추첨하지 않으며, 남성/계열 불명 액터에 확대 적용하지 않는다. 옵션만 켜서 모든 NPC를 순회하거나 새 배포 규칙을 만들지 않는다. 두 옵션은 별개이며 UBE 생식기 Innie/Average/Outie 20/60/20 방식은 변경하지 않는다.

## 근거와 설계값의 구분

- 원본 분기 대조: `tests/reference/OBodyNG443Morphs.inl`의 수정하지 않은 함수와 `tests/OBodyNumericTests.cpp`.
- 설치 자산: 두부팩 UBE 2.0 및 Necoco 2.1의 OSP, OSD, NIF, SliderCategories와 빌드된 몸 TRI를 읽기 전용으로 대조했다.
- UBE 공통 유두 항목은 위치 변형 20개와 UV 6개다. Necoco 프로젝트의 `Necoco_Overall_Nipple`은 현재 두부팩 생성 TRI에는 없다. 카테고리 이름만 존재하는 `NippleFolds` 등은 실제 지원 모프로 간주하지 않는다.
- 활성 모드/overwrite 승리 경로의 일반 여성 프리셋 42개를 참고했다. 예: NippleLength 명시 끝점 0..130, NipplesPerkiness -20..200, NippleDiameter -70..200, AreolaErection -20..100. 이 최대치를 랜덤화 한계로 그대로 사용하지 않는다.
- 아래 범위와 역할 대응은 BCNG 설계다. 제작자가 보증한 환산식이나 인게임 검증된 범용 안전 범위가 아니다. 같은 이름이라도 3BA와 UBE의 정점 이동량은 같지 않다.

## 원본 분기와 UBE 연결

숫자는 BodySlide 퍼센트 단위다. 표의 확률은 원본 `chance` 인수이며 실제 구현도 원본과 같은 실수 0..99 추첨 및 `<=` 비교다. 정수 0..99 추첨으로 바꾸거나 반올림하지 않는다. 범위의 상단 제외 규칙도 원본 난수 함수를 따른다.

| 원본 역할 | 원본 추첨 | UBE 출력 범위/처리 |
|---|---|---|
| AreolaSize | 15: 작은 쪽, 그 외 큰 쪽 | 작은 UV 0..30 또는 큰 UV 0..35; 반대 UV는 0 |
| AreolaPull_v2 | 75일 때만 | 작은 유륜 메시 0..30 또는 큰 메시 0..35; 같은 추첨값의 부호로 결정 |
| NippleLength | 15: 긴 쪽, 그 외 일반 | 긴 쪽 25..40, 일반 5..25 |
| NippleManga | 항상 | Nipples_Fantasy 0..45, 해당 UV도 같은 값 |
| NipplePerkManga | 25일 때만 | NipplesPerkiness -10..40 |
| NipBGone | 15일 때만 | NipplesShowUp 10..0: 원본 감소 강도가 커질수록 작게 |
| NippleSize | 항상 | NippleDiameter n\|p -15..25 |
| NippleDip | 항상 | NippleCreaseCentral 0..30 |
| NippleCrease_v2 | 항상 | NippleCreaseHorizontal 0..30 |
| NipplePuffy_v2 | 6일 때만 | AreolaErection 25..45 |
| NippleThicc_v2 | 35일 때만 | 같은 추첨으로 길이와 직경에 각각 0..10을 더해 한 번만 기록 |
| NippleInvert_v2 | 2일 때, 내부 50 분기 | Nippleinverted 고정 60 / 그 외 35..50 |

### 정확히 대응하지 않는 항목

- 설치된 3BA의 AreolaSize는 UV 전용이고 AreolaPull은 메시 변형이다. UBE의 유륜 UV 크기와 메시 크기를 각각 이 역할에 연결해 원본의 독립 추첨을 보존한다. 따라서 모든 UBE UV를 메시 값과 무조건 같게 만들지는 않는다. 실제 설치 프리셋도 유륜 UV와 메시 수치를 독립적으로 지정한다.
- UBE에는 3BA NippleThicc와 정확히 같은 단일 항목이 없다. 해당 35 분기의 한 추첨을 길이·직경의 작은 성분으로 나누고, 원래 길이·직경 추첨 결과와 합친 절대 목표값을 한 번 기록한다. 범위는 길이 최대 50, 직경 최대 35다. 기존 액터 모프에 계속 더하는 누적 보정이 아니다.
- Fantasy UV는 Fantasy 변형값을 따르며, 음수 직경 UV 보정은 최종 직경이 음수일 때 그 크기를 따른다. 이 관계 역시 BCNG 설계이며 모든 스킨의 유륜 위치를 보장하는 공식은 아니다.
- NippleInverted_PuffyAreola, NippleFlatten, NippleCircularCrease, 세로 끝 주름, Necoco_Overall_Nipple에는 추가 추첨이나 강제 초기화를 넣지 않는다. 원래 XML 값을 유지한다. 특히 함몰 내부 50 분기를 UBE의 서로 다른 프리셋 선택으로 바꾸지 않는다.
- NippleHeight, 좌우 간격, NippleHorizontal, 유륜 가로·세로 형태, 가슴/생식기/음경 항목도 이 유두 랜덤화에서 건드리지 않는다.

## 상태·성능·회귀

선택적 추첨이 실패하면 그 항목은 기록하지 않아 XML 값이 남는다. 대소문자는 기존 slider_name 맵과 RaceMenu 식별 규칙을 사용한다. 옵션 OFF 후 재적용은 XML부터 BCNG 소유 키를 다시 만들며, 타 모드 보존과 취소 가능한 미리보기 동작은 기존 경로를 따른다. 별도 저장 형식이나 영구 난수 시드를 추가하지 않는다. 기존 확정 재적용 정책에 따라 다시 추첨될 수 있다.

UBE 변환 함수는 스택의 소수 값과 문자열 뷰만 사용하며 별도 파일 판독·캐시·액터 포인터 저장을 하지 않는다. 원본 확률/범위 난수 함수도 기존 것을 그대로 재사용한다. BODYTRI 판독은 생식기 옵션의 Necoco 추가 항목에만 필요하다. 기존 bounded 메타데이터 캐시는 유지한다.

검증:

- 512개 조건 조합 × 101개 범위 표본 = 51,712 UBE 변환 검사: 호출 순서, 수치 범위, 동시 당첨, 조건 실패 시 XML 보존, 대소문자, 비대상 항목, 생식기와의 분리, 비누적 기록.
- 수정하지 않은 OBody 함수와 UBE 추첨 경로의 확률·범위 호출 비교: 1,024개 조건 마스크 × 3개 표본 = 3,072개.
- Release 빌드와 오프라인 회귀 실행 파일 40개 통과. 기존 XML/모프 수치, 미리보기·소유권·배포·저장·입력·런타임 레이아웃 검사 포함.
- 인게임 외형 검증과 전체 게임 메모리 누수 측정은 수행하지 않았다. MO2 교체·릴리즈 ZIP·GitHub 배포는 이번 작업에서 하지 않았다.
