# 모프 수치 일치 및 첨부 UBE 패치 분석 — 2026-09-25

## 적용 범위

사용자 요청에 따라 OBody NG 4.4.3의 수치 계산·조건부 쓰기·난수 범위를 기준으로 했다. 3BA도 포함하며 UBE만 별도 수정한 것이 아니다.

- XML 값 / 100과 체중 보간을 사용한다. UBE의 OSP 기본값 추가 차감은 제거했다.
- 명시된 0도 SetMorph에 전달한다. 음수·100 초과 값은 제한하지 않는다. 끝점이 없으면 0이고 슬라이더 자체가 없으면 생성하지 않는다.
- 중복된 동일 끝점은 원본처럼 첫 비영 값을 유지한다. UNP 계열의 지정된 역방향 이름은 원본의 정확한 이름 목록에 따라 `1 - 값 / 100`을 사용한다. 원본의 `coco` 부분 문자열 분류도 그대로여서 Necoco라는 set 이름에도 해당 역방향 이름이 있으면 같은 분기가 적용된다.
- 다른 모프 보존 ON/OFF의 삭제 범위와 의상 보정의 대상값/가산값 구분을 유지했다. 기본 복원은 다른 모드의 모든 키를 지우는 동작이 아니다. OBody/OClothe 교체 예외는 기존 요청대로 유지한다.
- BCNG 자체의 BHUNP용 별도 보정식·랜덤화식·고정 시드 난수를 제거했다. 원본의 조건부 쓰기, 분기 확률, 난수 범위 및 난수 생성 방식을 사용한다. 난수 옵션 ON 상태에서 프리셋을 새로 확정하면 원본처럼 새 난수를 뽑는다.
- 신체 무작위화 NPC 전용, SFS 의상 판정, 키 소유권, 미리보기 취소, 작업 큐와 저장 형식은 유지했다. 이후 사용자 요청으로 UBE는 별도 Nude → Pushup 가산 보정으로 구성했다(맨 아래 최종 결정 참조). 중간에 시험한 UBE 패치의 27개 상쇄 방식은 폐기했다. BCNG의 대소문자 비구분 RaceMenu 이름 결합도 유지한다. 따라서 OBody의 UI·배포·작업 스케줄러까지 복제했다는 뜻은 아니다.
- UBE 플레이어 때문에 전역 가슴·유두 옵션을 강제 ON/잠금 처리하던 부분은 제거했다. 지원 NPC에 영향을 주는 옵션을 사용자가 켜거나 끌 수 있다.
- 기존 비기본 선택의 적용 서명을 v4로, 내장 의상 보정 서명을 v4로 바꾸어 기존 큐에서 한 번 재평가한다. 기본 복원 서명은 유지한다. 새 캐시나 주기 검사, 엔진 인터페이스는 추가하지 않았다.

## 기준 소스와 검증

[OBody NG 4.4.3 Body.cpp](https://github.com/Aietos/OBody-NG/blob/4.4.3/src/Body/Body.cpp), [PresetManager.cpp](https://github.com/Aietos/OBody-NG/blob/4.4.3/src/PresetManager/PresetManager.cpp), [STL.h](https://github.com/Aietos/OBody-NG/blob/4.4.3/include/STL.h)를 기준으로 했다. 수식만 다시 만들어 비교하지 않고 원본 함수 본문을 `tests/reference`에 보관하여 독립 참조로 실행한다. 엔진·난수 입력 경계만 테스트 대역으로 제공한다.

- `OBodyNumericTests`: XML 2,400건. CBBE 3BA·BHUNP·기본 UBE·Necoco set, 양 끝점 생략/단일/양쪽, 0·음수·100 초과·중복·태그 대소문자를 원본 파서와 비교했다. 정확한 슬라이더 이름의 정상 XML이 직접 비교 대상이고 기존 혼합 대소문자 결합은 별도 회귀 테스트로 유지한다.
- 같은 테스트의 수치 6,224건: 원본 가슴/유두 보정과 랜덤화의 이름·조건부 쓰기·확률 호출 순서·구간·결과를 비교했다. 두 독립 난수 실행이 우연히 같은 값을 뽑는지를 비교하는 테스트는 아니다. 같은 입력 난수/분기를 주었을 때 결과가 같은지 검사한다.
- `UbeMorphTests`: 3BA/UBE XML 보간 7,200건 및 설치된 UBE OSP 491개 항목의 보간 17,045건 통과.
- `BodyMorphKeyTests`: 0 포함 직접 재적용, 음수·100 초과, 보존 ON/OFF, 기본 복원, 미리보기 A/B/취소 및 SFS 대기 중 레이어 보존 통과.
- Release DLL 빌드와 오프라인 회귀 실행 파일 38개 모두 통과. 새 함수 경계로 바뀐 소스 패턴 테스트도 갱신하고 기존 미리보기 안전 조건을 계속 검사한다.
- [RaceMenu 공개 구현](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/BodyMorphInterface.cpp)의 `Impl_SetMorph`는 0도 키에 기록하고 `Impl_HasBodyMorphKey`는 키 존재를 검사하는 것을 확인했다. 임의로 0 쓰기를 생략하지 않는다.

참조 파일 SHA-256:

```text
OBodyNG443Morphs.inl CD8C1DC7A15E81DF5AFA7F56A663C07D16AD15AAC4E01FC86C0971E1293D9E3B
OBodyNG443Parser.inl A4898C35EACB6921BCB4A7D80587F43B5DAA32AD3A7F6FFA6C20E8053F45E497
```

이는 수치·오프라인 회귀 검증이다. 실제 엔진 누수 전체, 모든 게임/RaceMenu 조합 및 특정 캐릭터의 인게임 외형을 검증했다는 의미가 아니다. 메시·TRI에 없는 Necoco 모프는 계산을 같게 해도 표현할 수 없다. MO2 교체·릴리즈 ZIP·GitHub 반영은 수행하지 않았다.

## 첨부 UBE ORefit 패치: 정적 분석

압축에는 README와 OBody.dll만 있다. README 표기는 1.1.1이며 OBody NG 4.4.3 교체 DLL이라고 설명한다. 소스·PDB는 포함되어 있지 않다. DLL은 실행하지 않았고 MSVC dumpbin으로 정적 분석했다.

```text
archive SHA256 4A45D15EAD9B79DCB455CC6FD015470F7EB7F0892C554800F140523338700C20
OBody.dll SHA256 2BF89C895A031D9D8A9F459596ABE8F7A71CA13AEB98C98EDD43C3519E3BE1B0
```

RVA 0x26EF0–0x281F3의 보정 생성 함수에서 추가된 27개 유두·유륜 항목을 확인했다. RVA 0x27FF0–0x28078은 항목마다 OBody와 RSMLegacy 값을 읽고 더한 뒤 부호 비트를 반전하여 양쪽 체중 끝점에 같은 값을 넣는다. 즉 `-(OBody + RSMLegacy)`이며, 해당 값들을 더하는 일반 합산 모드에서는 두 키의 효과가 상쇄된다. NIF 자체 형태나 다른 모드의 키까지 자동으로 평평하게 만드는 계산은 아니다.

같은 함수 앞부분에 기존 BreastSideShape·BreastUnderDepth 등 일반 ORefit 가슴 보정 생성 코드가 남아 있다. 그러나 아래 실제 UBE/Necoco OSP·TRI 대조에서 기존 23개 이름은 모두 없었다. 코드가 남아 있다는 사실만으로 UBE 가슴 형태도 바뀐다고 해석하면 안 된다. 추가된 27개는 소개문대로 유두·유륜 전용이고, UBE 가슴 전체에 대한 별도 수식이나 기존 가슴 슬라이더를 UBE 이름으로 옮기는 변환은 확인되지 않았다. 또한 RVA 0x27A6D의 기존 유두 옵션 분기가 OFF이면 RVA 0x27E6F의 27개 배열 초기화로 바로 이동하므로, 이 추가 블록은 기존 유두 옵션 밖에서 실행된다. 해당 함수 내부에는 추가 27개 블록을 UBE에만 제한하는 판정도 없다.

추가 목록:

```text
AreolaErection
AreolaeSizeBig
AreolaeSizeBig_UV_fix
AreolaeSizeSmall
AreolaeSizeSmall_UV_fix
AreolaHorizontal
AreolaVertical
Necoco_Overall_Nipple
NEW_NippleCircularCrease_Uv_fix
NEW_NippleDiameter_neg_value_Uv_fix
NEW_Nipples_Fantasy_UV_fix
NippleCircularCrease
NippleCreaseCentral
NippleCreaseHorizontal
NippleCreaseVertical
NippleDiameter n|p
NippleDispertionSides n|p
NippleFlatten
NippleHeight n|p
NippleHorizontal n|p
Nippleinverted
NippleInverted_PuffyAreola
NippleInverted_PuffyAreola_UV_fix
NippleLength
Nipples_Fantasy
NipplesPerkiness
NipplesShowUp
```

착의 상태에서 OClothe에 적용하고 탈의 시 제거한다는 생명주기 설명은 첨부 README에 명시되어 있다. 이번 분석으로 모든 호출자와 실제 탈착 이벤트를 인게임 검증한 것은 아니다.

## 중간 구현 기록: UBE/Necoco 전용 유두·유륜 상쇄 보정 (이후 폐기)

이 절의 구현·검증 기록은 중간 단계의 이력이다. 사용자의 후속 요청으로 해당 배열·수식·RSMLegacy 조회 콜백은 제거했으며, 현재 구현은 아래 Nude → Pushup 가산 방식이다.

사용자는 추가 27개 보정을 BCNG의 유두 보정 체크에 연결하고 CBBE 3BA/BHUNP 보정과 구분하기로 선택했다. UBE일 때만 `-(BodyChangeNG + RSMLegacy)`를 BCNG 의상 키에 쓰며, 기존 23개 3BA/BHUNP 항목을 함께 쓰거나 UBE 가슴 형태에 임의의 수치를 추가하지 않는다. 두 체크 중 하나라도 OFF이면 해제한다. 플레이어도 대상이며 UBE 신체 무작위화 제외는 그대로다. 기존의 명시적 호환 -Refit 프리셋 우선순위는 유지하므로 해당 사용자 프리셋의 내용까지 유두 전용으로 바꾸지는 않는다.

RSMLegacy는 읽기 전용 입력이며 BCNG 소유/교체 키 목록에 넣지 않았다. 다른 모프 보존 OFF의 미리보기에서는 확정 시 지워질 RSMLegacy를 0으로 간주하여 두 번 상쇄하지 않는다. 미리보기 취소·착의/탈의·SFS 대기/최종 숨김 및 세이브 정리는 기존 키·큐 경로를 사용한다. 공백·극성 접미사·UV 이름은 그대로 유지하며 대소문자만 비구분한다. 새 캐시·폴링·엔진 API는 추가하지 않고, 고정 이름 배열과 기존 임시 맵/모프 키만 사용한다.

### 설치 자산 대조 (읽기 전용)

`D:/TOFU/MO2/mods` 아래:

- `UBE 2.0/CalienteTools/Bodyslide/SliderSets/UBE SE 2.0 Release Body.osp`: 기존 23개 0개, 추가 27개 중 26개. 없는 항목은 Necoco_Overall_Nipple뿐이다.
- `h4 UBE SE - Necoco Body v2.1/CalienteTools/BodySlide/SliderSets/UBE SE 2.0 - Necoco Body.osp`: 기존 23개 0개, 추가 27개 모두 포함.
- `h4 BODYSLIDE OUTPUT (UBE)` 및 `BODYSLIDE OUTPUT (UBE)`의 `meshes/!UBE/Body/femalebody_tangent.tri`: 기존 23개 0개, 추가 26개(그중 UV 6개). SHA256 `f4f291ec9601eaf4156eb9a66d4ed6ffecab2cdbf76882e6b6697524f09c2583`.
- `h4 GT - Softbody v3.37`의 동일 상대 TRI 경로: 기존 23개 0개, 추가 27개(그중 UV 6개). SHA256 `6a7601497a30bffdec71092f27eebad4fa6d352f3815cc6c50ed4c36502e5ba9`.

OSP XML과 `tools/audit-ube-morphs.py`의 PIRT 파서를 사용해 대소문자 비구분으로 비교했다. 이 결과는 위 실제 바디 파일에 대한 결과이며 모든 배포판·의상 TRI에 이름이 있다고 보장하지 않는다.

### 후속 회귀 검사

`BodyMorphKeyTests`에 DLL 배열의 독립 전사본과 수치/키 테스트를 추가했다. 유두 ON/OFF, 체중 범위 밖·0·음수·100 초과, 27개 전체의 혼합 대소문자, 보존 ON/OFF, 미리보기/확정/취소, SFS 준비 대기, 탈의 해제, 반복 64회 적용의 비누적을 검사한다. `OBodyNumericTests`에서는 3BA/BHUNP의 원본 수치와 쓰기 개수가 그대로이고 RSMLegacy를 추가로 읽지 않는 것을 계속 대조한다.

후속 Release 빌드 및 오프라인 실행 파일 38개 통과. 모프 키 검사는 9,449건, 원본 XML 대조 2,400건 및 수치 대조 6,224건 통과. DLL SHA256: `9F4BAF694D9B93D8B5EE506088776B8A38BDB1DFF4028E79B0B261C642D38C48`. 인게임 검증·MO2 교체·배포는 하지 않았다.

## UBE 가슴 보정 참고 자료: Fit to Thicc (초기 분석, 이후 차이값 구현)

사용자가 자동 착탈의 모드가 아니라 슬라이더 보정 수치의 참고 자료를 요청했다. [Fit to Thicc - UBE Bodyslide Preset](https://www.nexusmods.com/skyrimspecialedition/mods/118826)의 제작자 설명은 Nude, No Nips, Pushup, Pushup Cleavage 네 변형을 명시한다. 공개 v1 ZIP(file ID 498926, 2024-05-08)의 SHA256은 `BA381E22983311F151C590B8A4C44C270C23E49357D0AE9F0610CB690BD8225E`이다. XML만 읽었으며 설치/실행하지 않았다.

사용자가 제공한 `D:/TOFU/MO2/mods/Fit to Thicc - UBE Bodyslide Preset/CalienteTools/BodySlide/SliderPresets` 안의 XML 네 개는 공개 ZIP과 바이트 단위로 동일했다. 아래는 대소문자를 무시하여 대조한 원본 XML 수치(낮은 체중 / 높은 체중, 생략 끝점은 OBody 수치 정책대로 0)다. 런타임 모프 값이 아니라 BodySlide 백분율 단위다.

| 가슴 슬라이더 | Nude | Pushup | Pushup Cleavage |
| --- | --- | --- | --- |
| Big_SaggyBreasts | 5 / 15 | 5 / 10 | 5 / 10 |
| BreastCenterGapLowerHeight n\|p | 15 / 0 | 0 / 0 | 0 / 0 |
| BreastCenterGapWidth n\|p | 0 / 0 | -80 / -80 | -60 / -60 |
| BreastsCupSag n\|p | -20 / 0 | -60 / -20 | -60 / -50 |
| BreastsPositionWidth n\|p | 0 / 10 | -30 / -30 | -100 / -80 |
| BreastsRotate_Y | 10 / 15 | 0 / 20 | 10 / 10 |
| BreastsTBD | 10 / 20 | 15 / 25 | 15 / 25 |
| BreastUpperCurve n\|p | 30 / 10 | 30 / 0 | 30 / 0 |

위 8개 이름은 설치된 기본 UBE와 Necoco 바디 OSP에 모두 있다. 네 XML에는 대소문자 비구분으로도 중복 끝점이 없었다. Pushup 변형에는 유두/UV 변경도 함께 있으므로 가슴 변화만 보려면 해당 항목을 분리해야 한다. 이 차이는 이 프리셋 제작자가 정한 특정 체형용 결과이며 모든 UBE 프리셋에서 같은 시각적 강도를 보장하는 공식은 아니다. 초기에는 참고 분석만 했으나 이후 사용자가 아래 차이값 적용 방식을 선택했다.

### Nude 기준 나머지 세 프리셋 전체 차이

No Nips는 Nude와 비교하여 `NipplesShowUp`의 small/big 0 두 항목만 추가한다. 다른 수치는 모두 같다. 아래 유두/유륜/UV 항목과 위 가슴 8개 표가 세 변형의 수치 차이 전체다. 아래 표의 0은 생략된 0 기본값도 포함하며, 기본값이 다른 NipplesShowUp은 생략 여부를 명시했다.

| 유두/UV 슬라이더 | Nude | No Nips | Pushup | Pushup Cleavage |
| --- | --- | --- | --- | --- |
| NEW_NippleCircularCrease_Uv_fix | 20 / 20 | 동일 | 0 / 0 | 0 / 0 |
| NEW_Nipples_Fantasy_UV_fix | 100 / 100 | 동일 | 100 / 100 | 0 / 0 |
| NippleCircularCrease | 20 / 20 | 동일 | 0 / 0 | 0 / 0 |
| NippleDispertionSides n\|p | 0 / -10 | 동일 | 0 / 0 | 40 / 40 |
| NippleHeight n\|p | 0 / 0 | 동일 | 20 / 30 | 0 / 30 |
| NipplesPerkiness | 30 / 60 | 동일 | 20 / 30 | 20 / 30 |
| NipplesShowUp | 양쪽 생략 | 0 / 0 | 0 / 0 | 20 / 30 |

설치된 UBE Body OSP에서 NipplesShowUp 기본값은 small/big 모두 100이다. [BodySlide SliderManager::InitializeSliders](https://github.com/ousnius/BodySlide-and-Outfit-Studio/blob/dev/src/components/SliderManager.cpp)는 프리셋에 해당 끝점이 없으면 OSP에서 받은 defValue를 사용한다. 따라서 BodySlide 프로젝트에서는 위 행이 Nude 100/100 → No Nips·Pushup 0/0 → Cleavage 20/30으로 해석된다. 반면 OBody 방식의 런타임 XML 처리는 생략된 프로젝트 기본값을 합성하지 않는다. BCNG도 사용자 요청대로 그 정책을 유지하므로 Nude와 No Nips가 추가하는 이 모프의 수치 차이는 0이다. 메시의 이미 구워진 형태를 빼는 기능을 임의로 추가하지 않았다.

기록 유무까지 비교하면 No Nips는 1개 이름/2개 끝점, Pushup은 14개 이름/24개 끝점, Pushup Cleavage는 15개 이름/25개 끝점이 다르다. 생략을 0으로 취급하는 런타임 수치만 비교하면 각각 0, 13개/22개, 15개/25개다. Pushup Cleavage는 Pushup 전체를 일정 배수로 증폭한 것이 아니라 폭·간격·회전·유두 위치 등을 서로 다르게 조합한다.

## 최종 결정: UBE Nude → Pushup 차이값 적용

사용자는 기존 OBody UBE 패치의 방식을 폐기하고, 기본 XML은 계속 OBody NG 방식으로 계산하되 착의 보정만 Nude → Pushup 차이값으로 더하도록 요청했다. `BodyMorphPolicies.h`에 아래 14개 고정 가산값을 기록했다. 수치는 BodySlide 백분율이며 `/100`한 뒤 낮은·높은 체중 사이를 보간한다. 현재 액터/프리셋/RSMLegacy의 값을 빼지 않고, 강도 배수·클램프·기본 프리셋 덮어쓰기도 추가하지 않았다.

| 분류 | 슬라이더 | 낮은 / 높은 체중 차이 |
| --- | --- | --- |
| 가슴 | Big_SaggyBreasts | 0 / -5 |
| 가슴 | BreastCenterGapLowerHeight n\|p | -15 / 0 |
| 가슴 | BreastCenterGapWidth n\|p | -80 / -80 |
| 가슴 | BreastsCupSag n\|p | -40 / -20 |
| 가슴 | BreastsPositionWidth n\|p | -30 / -40 |
| 가슴 | BreastsRotate_Y | -10 / +5 |
| 가슴 | BreastsTBD | +5 / +5 |
| 가슴 | BreastUpperCurve n\|p | 0 / -10 |
| 유두/UV | NEW_NippleCircularCrease_Uv_fix | -20 / -20 |
| 유두/UV | NippleCircularCrease | -20 / -20 |
| 유두/UV | NippleDispertionSides n\|p | 0 / +10 |
| 유두/UV | NippleHeight n\|p | +20 / +30 |
| 유두/UV | NipplesPerkiness | -10 / -30 |
| 유두/UV | NipplesShowUp | -100 / -100 |

`NEW_Nipples_Fantasy_UV_fix`는 Nude와 Pushup에서 동일하므로 적용하지 않는다. NipplesShowUp의 -100은 이 참고 자료를 해석할 때만 OSP 기본값 100을 사용하여 산출한 고정값이며, 일반 XML 파서가 생략값에 100을 채워 넣는 변경은 아니다. 다른 부위/유두 27개 전체를 상쇄하던 방식은 완전히 제거했다.

기존 상위 의상 보정 옵션 ON이면 가슴 8개, 하위 유두 옵션도 ON이면 추가 6개가 적용된다. 유두만 OFF이면 가슴은 유지한다. 상위 OFF·탈의·SFS 양쪽 몸통 숨김·보정 대상 의상이 없음이면 기존 의상 키를 제거하고 기본 프리셋 + 보존된 다른 모프의 결과로 돌아간다. SFS NotReady는 기존 레이어를 유지하고 새 Ready 판정을 기다린다. 사용자 정의 호환 -Refit/의상별 프리셋 우선순위, 남성 경로와 CBBE 3BA/BHUNP 계산은 유지한다.

ORefit JSON의 의상 이름·플러그인·FormID 제외는 바디 계열별 계산 이전에 공통 검사한다. UBE만 제외를 우회하는 경로는 없다. 실제 장비 경로와 SFS 최종 표시 경로 모두 같은 규칙을 쓰며, 기존의 any-slot 강제 보정 우선순위는 변경하지 않았다. 따라서 별도 강제 항목이 함께 표시/착용되거나 다른 보정 가능 몸통 항목이 있으면 기존 ORefit 규칙에 따라 보정이 켜질 수 있다.

`BodyMorphKeyTests`는 독립적으로 기록한 Nude·Pushup 원본 끝점에서 차이를 계산하여 결과를 대조한다. 가슴/유두 개수, 0·음수·100 초과 프리셋, 여러 체중, 켜기/끄기·탈의, 보존 ON/OFF, 혼합 대소문자, 미리보기/취소/확정, SFS 대기, 기존 상쇄 레이어 제거, 반복 64회 비누적을 검사한다. ORefit 규칙 및 SFS 테스트는 3BA·BHUNP·UBE별 이름/플러그인/FormID 제외 행렬을 포함한다. 실제 엔진 외형·TRI 호환성과 모든 체형에서의 시각적 강도는 인게임 미검증이다.

모프 변경 단계 검증: SE/AE Release 빌드 성공, 오프라인 실행 파일 38개 모두 통과. BodyMorphKeyTests 8,929건, 원본 OBody XML 2,400건·수치 6,224건, UBE XML 보간 7,200건, SFS 484건 통과. `git diff --check` 통과. 당시 DLL SHA256: `0CD57EE5F021AFE9C1DCC0D9E8A00575C9BE7B5AC7125D3B3DB0305CD8B09A7A`. 새 엔진 호출·캐시·폴링·동적 소유 객체는 추가하지 않았으며 기존 의상 레이어와 변경 서명을 사용한다. 이 단계에서는 버전 변경·MO2 반영·배포 ZIP·GitHub 반영 전이었다.

## 후속 에셋 식별 수정과 최종 통합

이후 스킨·후타·오버레이·틴트의 대소문자 식별 및 정상 TXST 쓰기 오판을 수정했다. 최종 1.3.4.0 DLL SHA-256은 `D15C3CB71B4085848F381A57E23C3A59A6567CBA916068C3EB1C9B2A431E6C69`이며 두부팩 MO2 설치 DLL과 일치한다. 사용자는 최신 빌드의 인게임 정상 동작을 보고했다. 최종 트리에서도 Release 빌드와 38개 회귀 실행 파일을 모두 재검증했다. 배포·문서 통합 범위와 검증 한계는 `RELEASE-VERIFICATION-v1.3.4-KO.md` 마지막 절에 정리했다. 기존 문단의 인게임 미검증·배포 전 표기는 해당 중간 단계의 기록이다.
