# UBE 프리셋·스킨 점검 — 2026-09-15

이 문서는 제작자 설명, 설치 파일, BCNG 코드에서 확인한 내용을 구분하는 내부 점검 기록이다. 특정 사용자의 실제 액터 상태나 모든 게임 버전의 인게임 결과를 대신하지 않는다.

**현재 상태:** 아래의 초기 조사 기록 이후 액터 계열 분류와 NIF 내장 피부 경로를 추가 수정했다. 최신 결과는 문서 끝의 **1.2.4 추가 수정 및 재검증**을 기준으로 한다. Selector 협조 패치는 사용자 결정으로 진행하지 않으며 동시 스킨 제어 비호환으로 안내한다.

## 제작자 설명에서 확인한 내용

- UBE는 별도 종족·메시·UV를 사용하며 CBBE/UNP 스킨 및 오버레이를 그대로 공유하지 않는다.
- 의상도 UBE 전용 ArmorAddon과 모델 경로를 사용한다. 종족용 경로를 만드는 패치가 일반 의상 지오메트리·모프까지 UBE로 변환한다는 뜻은 아니다.
- SE용 RaceMenu에서 표면 노멀 갱신 문제가 있으며 UBE에 AE 기능을 역이식한 DLL이 제공된다고 설명한다. 체중 변경만으로 노멀이 갱신되지 않는 문제도 별도로 언급한다.
- 제작자는 Unique Player 계열과의 비호환을 명시한다. 따라서 RaceMenu Selector 설치가 특정 제보 환경에서 증상을 가렸다는 것과 UBE에 권장되는 해결책이라는 것은 구분해야 한다.

출처: [UBE 2.0 제작자 소개](https://www.nexusmods.com/skyrimspecialedition/mods/92989), 로그인 후 본문 확인. 소개글의 과거 RaceMenu 관련 주의사항을 모든 최신 DLL에서 재현된 사실로 취급하지 않는다. 제작자도 권장하지 않은 Engine Fixes 설정 변경은 수행하지 않았다.

## 두부 설치 파일 확인

대상은 `D:\TOFU\MO2\mods\UBE 2.0`의 SliderSets 및 `BODYSLIDE OUTPUT (UBE)\meshes\!UBE`의 출력이다.

| 부위 | OSP 슬라이더 | 게임용 TRI | 빌드 시만 처리하는 항목 |
| --- | ---: | ---: | --- |
| 몸 | 202 | 위치 196 + UV 6 | 없음 |
| 손 | 36 | 위치 35 | HandNailsZap |
| 발 | 26 | 위치 25 | ToeNailsZap |
| Preview | 227 | 개별 부위 출력 사용 | 손·발 손톱 Zap |

몸·손·발의 실행용 슬라이더 이름은 해당 출력 TRI와 모두 일치했다. 일반 Zap은 메시를 만드는 단계의 기능이며 런타임 숫자 모프로 정점 삭제를 구현할 수 없다. UV 슬라이더는 별도 구분한다.

기본 몸·Preview에서 0이 아닌 빌드 기본값은 다음 두 개다. 몸·손·발의 남성 SliderSet도 같은 기본값 정책임을 확인했다.

| 이름 | 체중 0 기본값 | 체중 100 기본값 |
| --- | ---: | ---: |
| NipplesShowUp | 100% | 100% |
| SkinnyMorph | 100% | 0% |

`-Zeroed Sliders-` XML은 SetSlider가 없는 유효한 UBE 프리셋이다. 이 경우 BodySlide의 OSP 기본값을 사용한다. 모든 슬라이더의 절대값이 숫자 0이라는 뜻이 아니다.

설치된 몸 NIF와 원본 ShapeData/TRI를 대조했을 때, 체중 0은 위 두 기본값, 체중 100은 NipplesShowUp 기본값이 들어간 결과와 부동소수점 오차 범위에서 일치했다. 현재 출력이 잘못 빌드됐다고 단정할 근거는 없다.

### 재현용 해시

- Body OSP SHA-256: `dcf288ff4777c22a0662aa0f9d896ab6b882025408db0fd03fdf291f6d529093`
- Body TRI SHA-256: `f4f291ec9601eaf4156eb9a66d4ed6ffecab2cdbf76882e6b6697524f09c2583`
- Hands TRI SHA-256: `7e3c86dbd29129c06e14be40818ec2809ed8db311ccb7f3c3efe9f8625c67be4`
- Feet TRI SHA-256: `e6db1fc23fb971b3d307c2f34b65109a1f1b929e16eb5f0de1a95345b8282fbe`

## BCNG에서 확인한 문제와 수정

1. 슬라이더 목록이 비었다는 이유로 유효한 UBE Zeroed 프리셋을 거절했다. UBE 기본값 프리셋만 허용하고 다른 계열의 임의 빈 프리셋까지 예외를 넓히지 않았다.
2. UBE의 명시된 절대 XML 값에 빌드 기본값이 중복될 수 있었다. 일반 UBE 프리셋은 `XML 값 / 100 - 해당 체중 끝점의 빌드 기본값`을 저장한다. 예를 들어 NipplesShowUp 64%는 이미 들어간 100% 위에 +64%가 아니라 -36%를 적용한다.
3. XML에 없는 끝점은 델타 0으로 두어 원래 OSP 기본값을 보존한다. 음수·100% 초과·슬라이더 이름의 `n|p`는 임의 제한하거나 반전하지 않는다. 이름이 `-Refit`인 별도 가산 보정 프리셋은 이 정규화에서 제외한다.
4. Zeroed 선택은 적용 모프 키가 없어도 저장된 프리셋 선택으로 인정한다. 기존 OBody/OClothe 키 정리와 다른 모드 모프 보존 옵션은 유지한다.
5. 얼굴은 NiNode 완료 알림만 기다리던 경로를 보완했다. 게임의 직접 재생성 호출이 끝난 뒤 실제 pending 상태와 현재 얼굴 재질을 확인한다. 준비되면 같은 게임 작업에서 얼굴 처리를 진행하고, 아직 준비되지 않았으면 프레임별로 확인한다. 시간 초과를 성공으로 처리하지 않으며 실패 콜백은 한 번만 소비한다.

일반적인 사용자 제작 UBE 변형 메시·의상·충돌용 프로젝트가 표준 본체와 다른 기본값으로 빌드된 경우까지 검증한 것은 아니다. 다른 모드의 UBE RaceMenu 모프 키도 보존 옵션이 켜져 있으면 의도대로 유지된다.

## 추가로 구분해야 할 사항

- 샘플 23개 프리셋 중 15개에 기본 몸·손·발/Preview 슬라이더 집합 밖의 0이 아닌 이름이 있었다. UBE 3.0, 의상 전용 압박·변형 등의 이름이 포함된다. 이것만으로 해당 프리셋 전체가 잘못됐다고 판정하거나 목록에서 거절하지 않는다. 해당 이름을 가진 메시가 없으면 그 모프만 표현되지 않는다.
- 두부의 확인된 두 `skee64.ini` 제공 파일 모두 `bEnableBodyNormalRecalculate=1`이다. 노멀 설정이 꺼져 있다는 증거는 없다.
- 공개 RaceMenu 소스에서는 `ApplyBodyMorphs`가 모프 갱신을 거쳐 활성화된 노멀 재계산까지 수행한다. BCNG도 이 인터페이스를 호출하므로 별도 전체 재생성을 반복 추가하지 않았다. 실제 설치 DLL의 모든 세대가 같은 동작을 하는지는 별도 런타임 문제다.
- NPC의 몸 스킨이 NIF에만 정의되어 있고 일반 Skin TXST가 없는 경우의 네이티브 연결 공백은 별도 미완료 조사 항목이다. 이번 얼굴 대기 수정만으로 AE 1.6.1179의 몸·얼굴 비대칭 제보 전체가 해결됐다고 주장하지 않는다.

RaceMenu 근거: [BodyMorphInterface.cpp](https://raw.githubusercontent.com/expired6978/SKSE64Plugins/master/skee64/BodyMorphInterface.cpp), [BodyMorphInterface.h](https://raw.githubusercontent.com/expired6978/SKSE64Plugins/master/skee64/BodyMorphInterface.h). BodySlide 기본값/빌드 근거: [BodySlideApp.cpp](https://raw.githubusercontent.com/ousnius/BodySlide-and-Outfit-Studio/dev/src/program/BodySlideApp.cpp), [TRI 형식](https://raw.githubusercontent.com/ousnius/BodySlide-and-Outfit-Studio/dev/src/files/TriFile.cpp). 소스 조회용 링크이며 의존성 버전을 변경하지 않았다.

## 검증 기록

- 읽기 전용 `tools/audit-ube-morphs.py`: 표준 OSP/출력 TRI 확인 및 계산 17,045건 통과.
- 새 `BodyChangeNGUbeMorphTests`: 실제 PresetCatalog를 사용해 빈 Zeroed, 기본값, 누락 끝점, 범위, 대소문자, 가산 Refit, 기존 계열 및 외부 키 보존을 검사했다. 설치 OSP를 입력한 C++ 계산 17,045건 통과.
- 새 Visual Studio 설치 후 기존 PCH 버전 불일치를 전체 재빌드로 해소했다. 새 MiddleHighProcessData 사용에 필요한 BGSBodyPartDefs 헤더 의존성을 명시했다. Zeroed 저장 상태 예외를 반영하도록 기존 소스 패턴 회귀 검사를 갱신했다.
- **1.2.4 Release 빌드 성공, 테스트 실행 파일 25개 전부 통과.** 설치 OSP를 넣은 C++ 계산 17,045건도 재실행 통과.
- DLL: `build/v1.2.4/windows/x64/release/BodyChangeNG.dll`, 파일 버전 `1.2.4.0`, 2,780,160 bytes, SHA-256 `CAA79436DFEE3AEA44F7457A1036C39B5C23768E1DF125A1C51B08F9EDA7550D`.
- MO2, 게임 데이터, 세이브, 공개 릴리즈는 아직 교체하지 않았다.

### 현재 사용한 빌드 도구

- 공유 보관본 XMake 3.1.0, commit `96ad28edb71dc4e9c8193924a491629c656e8e8c` 사용. 새 다운로드 없음.
- 재설치된 MSVC `cl.exe` 파일 버전 `19.51.36257.0`, 제품 버전 `14.51.36257.0`.
- 경로: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`.
- SHA-256: `315a654ea116864516a1674858e587e535e3bc3045ff32ed2f2739a2c1ec5640`.
- SE/AE 전용 설정 유지. VR 대상 추가 없음. 공유 도구 등록부의 이전 컴파일러 해시와 달라 파일 버전을 다시 확인했다.

## 추가 사용자 피드백 — 수정 전 조사 기록

### 커스텀 NPC의 CBBE 3BA 프리셋 누락

Immersive Wenches의 Eila 등 추가·리플레이서 NPC가 기본 설치 몸을 사용하는데 CBBE 3BA 프리셋이 보이지 않는다는 제보다. 두부에는 Immersive Wenches와 Basic Wenches가 있으나 제보자와 같은 최종 덮어쓰기 레코드/메시인지는 알 수 없다.

현재 `UI::BodyItems`는 일반 목록에서 ResolveActor 결과로 필터링한다. ResolveActor의 알 수 없음(0)은 목록을 숨기지 않는다. 반면 Skin Armor/Addon의 이름·플러그인·모델 경로가 한 계열을 명시하면 그 판정이 실제 로드된 메시 증거보다 먼저 사용된다. 따라서 단순 미분류와 잘못된 명시 판정을 구분해야 한다. NPC 템플릿·최종 Skin Armor·실제 모델/모프까지 추적하기 전 특정 Eila 구성의 원인이라고 단정하지 않는다.

#### 제공된 Immersive Wenches 원본 확인

- 원본: `C:\Users\yunha\Downloads\Immersive Wenches-595-2-0-2-1761580809.7z`.
- 아카이브 SHA-256: `4525F4EBD26534AA3E2166F3F4846C534771F9844A25D2445CE1CAA91DD4DCD9`.
- 내부 `Immersive Wenches.esp`: 2,030,462 bytes, SHA-256 `6fd02cafe933703d77ba1165cc7ecbe3607c5f533237099c6f731da7bc59944a`.
- Eila 실제 NPC `IW_INN_Whiterun_Jarlhouse_M_Eila`는 파일 내 ID `040140CE`, 외형 템플릿 `IW_FACEPRESET_nord45_necro`는 `04001884`다. 두 레코드 모두 여성, NordRace `00013746`, 체중 80이며 Skin은 `042B48B5`다. 실제 NPC는 Template Flags `181B`로 Use Traits를 사용하고, 외형 템플릿은 `171A`로 Use Traits를 사용하지 않는다. 이 ID의 `04`는 원본 파일의 마스터 순서이며 게임의 런타임 로드 인덱스가 아니다.
- Skin `IW_SkinNaked_Wench`는 전용 ARMO이지만, 포함된 25개 ARMA는 전부 Skyrim.esm을 참조한다. 여러 종족용 부품이 함께 있으며 Nord에 맞는 것은 기본 `NakedTorso` (`00000D67`), `NakedHands` (`00000D6C`), `NakedFeet` (`00000D6E`)다.
- 위 세 ARMA의 원본 여성 모델은 각각 `Actors\Character\Character Assets\FemaleBody_1.nif`, `FemaleHands_1.nif`, `FemaleFeet_1.nif`다. 따라서 최종 덮어쓰기가 이 연결을 바꾸지 않았다면 설치된 기본 여성 몸 메시를 사용한다. 전용 Skin Armor라는 이유만으로 독립 UNP/UBE 몸이라고 판정할 수 없다.
- 제공 아카이브에는 별도의 여성 몸·손·발 교체 메시가 없었다. 얼굴·머리카락·마스크 등의 외형 자원은 있다. 원본 플러그인 안의 ARMA 4개도 마스크/마법 형상용이며 독립 여성 본체가 아니다.
- 두부의 설치된 Immersive Wenches ESP 해시는 원본과 다르다. 이 설치본의 두 Skin Armor 연결은 위와 같았지만, 모든 레코드가 원본과 동일하다는 뜻은 아니다. `BasicWenches.esp`는 별개의 마스터 구성이고 Eila 레코드가 없었다. 현재 두부의 `Botox SE - Immersive Wenches.esp`는 헤더만 있는 203-byte 파일이다.

**판정:** 원본에서 CBBE 프리셋을 배제할 이유는 발견하지 못했다. 기본 경로의 CBBE 메시가 최종 적용된 환경이라면 CBBE 3BA 목록이 나와야 한다. 현재 분류 정책에서 미분류는 전체 표시이며, CBBE 설치 + 표준 텍스처 근거도 CBBE로 분류된다. 제보 환경의 최종 NPC/Skin/ARMA 덮어쓰기·로드된 지오메트리·목록 모드를 확보하지 않은 채 특정 잘못된 분류를 재현했다고 주장하지 않는다. 메타데이터의 모든 ARMA를 종족 구분 없이 모으고 실제 로드된 몸보다 우선하는 점은 별도 개선 후보이지, 이 원본 제보의 확정 원인은 아니다.

검사 도구 `tools/audit-plugin-records.py`는 압축본의 ESP를 메모리로 읽고 레코드 경계, 압축 해제 크기 및 확장 서브레코드 길이를 검사한다. 게임/MO2에는 추출하지 않았다. 필드 해석은 [xEdit의 Skyrim 정의](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES5.pas)의 NPC ACBS/TPLT/RNAM/WNAM 및 ARMO/ARMA 구조와 대조했다. 이 도구는 최종 로드 오더 해석기나 xEdit 대체품이 아니다.

#### 추가 확인: 두부 최종 플러그인/메시와 분류 반례

`tools/audit-npc-compatibility.py`로 TOFU NSFW의 활성 플러그인 및 필수 마스터 1,060개를 읽었다. MO2 overwrite → modlist 우선순위 → Stock Game Data 순으로 실제 loose 플러그인 제공 파일을 해석하고, 원본 마스터 이름 + local FormID로 동일 레코드의 덮어쓰기를 비교했다. 검사 대상 플러그인/모드 디렉터리 누락은 없었다. 이 검사는 런타임 패처·세이브의 동적 변경·게임 메모리의 최종 상태까지 읽는 것은 아니다.

- Eila, 외형 템플릿 및 `IW_SkinNaked_Wench`의 최종 플러그인 제공자는 두부의 Immersive Wenches.esp 그대로였다.
- NordRace는 최종 Synthesis.esp가 제공하지만 기본 SkinNaked 참조를 유지했다. Eila는 이 종족 기본 스킨이 아니라 자신의 위 Skin Armor를 참조한다.
- `NakedTorso`, `NakedHands`, `NakedFeet`는 Skyrim.esm의 기본 모델 경로가 유지됐다. Skin에 포함된 25개 ARMA 전체도 따로 추적했으며, 최종 EDID 및 여성 모델 경로에서 CBBE/3BA/UNP/BHUNP/UBE 계열명에 해당하는 신호가 없었다.
- 위 세 기본 모델 경로의 실제 loose 파일 제공자는 `BODYSLIDE OUTPUT (CBBE)`다. 몸 NIF의 본체 형상 이름은 `3BA`, 손은 `Hands`, 발은 `Feet`이며 모두 표준 여성 텍스처 경로였다. 몸에는 `3BA_Vagina`와 `3BA_Anus`도 있다.
- 따라서 두부의 파일 상태에서 Eila를 독립 UBE/UNP로 단정할 근거는 없다. 현재 UI의 실시간 분류 결과를 읽거나 제보자의 게임 화면을 재현한 것은 아니다.

`tools/audit-body-family-policy.cpp`를 현재 `BodyFamilyRules.cpp`와 함께 컴파일하여 `ResolveActor`의 함수 호출 순서만 재현했다. 게임 액터 모형이나 바람직한 동작을 검증하는 통과 테스트가 아니라 **현재 정책의 반례를 드러내는 진단 프로그램**이다.

| 입력 조건 | 현행 결과 | 해석 |
| --- | --- | --- |
| Eila의 일반 Skin/ARMA 이름과 기본 여성 모델 경로 | metadata=0 | 이 이름 자체는 CBBE 목록을 배제하지 않는다. |
| CBBE + UBE 설치 감지, 표준 스킨 근거 | CBBE, 목록 표시 | 두부 기본 경로에 부합한다. |
| 아무 계열도 확정하지 못함 | unknown, 목록 표시 | 단순 미분류만으로 목록이 없어지지는 않는다. |
| 설치 감지는 UBE만 남고 표준 스킨 근거는 있음 | UBE, CBBE 목록 숨김 | 첫 해석은 UBE를 제외해 unknown을 반환하지만, 두 번째 unknown-layout fallback이 UBE를 다시 선택한다. 표준 근거를 잃는 코드상 문제다. |
| UNP 메타데이터 + 로드된 CBBE 형상 신호 | UNP, CBBE 목록 숨김 | 메타데이터가 실제 형상 신호보다 무조건 우선한다. |
| UBE 메타데이터 + 표준 CBBE 형상 신호 | UBE, CBBE 목록 숨김 | 같은 우선순위 문제다. |

분류 개선은 액터에 실제로 맞는 ARMA/본체 모델 근거를 먼저 사용하고, 설치 여부만으로 이미 확인한 표준/UBE 구분을 뒤집지 않도록 해야 한다. 이름만 충돌하는 경우와 실제 독립 UV/메시가 다른 경우를 구분해야 하므로, 모든 커스텀 NPC를 CBBE로 강제하거나 호환 검사를 없애는 해결책은 아니다. 위 문제는 조건부 코드 반례로 확인됐지만 제보자의 Eila가 정확히 어떤 조건을 충족했는지는 미확인이다. **이번 추가 확인에서는 제품 코드/필터를 수정하지 않았다.**

### 커스텀 A → B 전환이 안 되고 Default → B만 동작

같은 원본 DDS를 색만 바꾼 세 팩에서 첫 팩이 남으며, Default를 거치면 다른 팩을 적용할 수 있다는 제보다. 정상 사용법이 아니다. "어두워짐"은 첫 팩의 원래 색이었다는 제보자의 정정을 반영하며, PC 사양이나 저장 파일 손상으로 단정하지 않는다.

직접 전환은 기존 private Skin Armor/TXST 그래프를 재사용하고, Default는 소유한 포인터를 복원한다. 두 경로 사이의 엔진 캐시·외부 포인터 소유권·RaceMenu 덮어쓰기 차이를 재현해야 한다. 현재 수정의 테스트 통과를 이 인게임 제보 해결 증거로 대신하지 않는다.

### RaceMenu Selector of Skins - Unique Player Character

이 전체 문자열은 **한 모드의 이름**이다. 확인 대상은 이 모드와 BCNG의 동시 사용이다.

- [원본 모드 소개](https://www.nexusmods.com/skyrimspecialedition/mods/126721): 버전 0.1.6. 플레이어의 몸 메시·텍스처를 선택하며, 다른 실시간 텍스처 교체 모드와의 간섭을 주의사항으로 명시한다.
- [원본 파일 목록](https://www.nexusmods.com/skyrimspecialedition/mods/126721?tab=files): AixBodySelector.esp, AixBodySelectorScript.pex 및 두 위치의 PSC, AixChangeBodySpellScript와 AixUpdateScript가 확인된다. 현재 두부의 mod 디렉터리 이름 검사에서는 해당 셀렉터 설치가 확인되지 않았다. 이것은 플러그인/스크립트 전수 설치 판정은 아니다.

#### 제공된 Selector 원본 스크립트/플러그인 확인

- 원본: `C:\Users\yunha\Downloads\Selector of Skins-126721-0-1-6-1724178303.zip`.
- 아카이브 SHA-256: `7A8F9C50AA26A9F635CD04B25FC49AC2B9043E5270D26811B778A9571142DBE5`.
- `AixBodySelector.esp`: SHA-256 `b368a623dbd6ef986fd9cfcafd6844e8bd7d923d4abe2270c1e1bfb48df203f1`, 8개 Skin Armor, 12개 ARMA, 16개 TXST가 있다.
- `Scripts/AixBodySelectorScript.pex`: SHA-256 `27b0b91e4c70209b3b7519391c944973b5aaf30b6bc3361b31245fc5c90e7368`.
- `source/scripts/AixBodySelectorScript.psc`: SHA-256 `e4bc620efb7e39a4f253b0f7aa8d3d8e2c2e9d96d1903f4af37e422bb08fc3ea`.
- 별도로 동봉된 `Scripts/AixBodySelectorScript.psc`는 디컴파일 표기가 있으며 SHA-256 `b08844fa2ad949d12147d6bfbd42eca3a4dd07c4b8a3324ecf29c554cd750459`다. 두 PSC의 아래 핵심 동작은 일치했다. 이번에 PEX를 새로 디컴파일하거나 PSC/PEX 전체 동등성을 검증한 것은 아니다.

| 원본 PSC 동작 | BCNG와 만나는 지점 |
| --- | --- |
| `On3DLoaded` → `OnReloadSettings` → `UpdateBody` | bodyvalue가 1~4이면 플레이어 ActorBase에 `SetSkin(Skin0xNaked[F])`를 다시 호출한다. BCNG가 연결한 개인용 Skin Armor 포인터가 다른 포인터로 바뀔 수 있다. |
| `OnReloadSettings`는 skinvalue가 0이 아니면 `UpdateSkin` 호출 | 얼굴 노드에 `NiOverride.AddNodeOverrideString(..., 9, 0/1/2/7, ..., true)`를 기록한다. BCNG도 같은 키/채널을 사용하므로 독립 저장 공간이 아니다. |
| 얼굴 슬라이더 0 선택 시 `UpdateSkin` | 같은 네 채널을 `RemoveNodeOverride`로 제거한다. 기록한 모드별 소유자를 구분하지 않는다. |
| 몸/얼굴 슬라이더 변경 후 `QueueNiNodeUpdate` | 다시 생성된 3D에 위 재적용 경로가 연결된다. |

몸 값이 0일 때 `UpdateBody` 자체는 SetSkin을 반복하지 않는다. 몸 슬라이더를 명시적으로 0으로 바꿀 때만 SkinNaked를 지정한다. 얼굴 값도 0이면 OnReloadSettings에서 UpdateSkin을 생략하므로, 단순 설치만으로 항상 위 덮어쓰기가 발생한다고 표현하면 안 된다. 탑승 상태를 기다리는 단발 업데이트는 정상 상태에서 계속 실행되는 무한 5초 타이머가 아니다. `AixUpdateScript`의 로드 처리는 저장된 스크립트 버전 조건부이며 매 로드마다 무조건 초기화한다고 해석하지 않는다.

BCNG의 `OwnsCurrentPointers`, QueueApply, `ResolveGraphAction`은 활성 선택 도중 다른 제공자가 포인터를 바꾸면 `ownershipConflict`/`rejectForeignOwner`로 차단한다. TNG의 알려진 자식 그래프와 RSV 재설정 예외는 있으나 Selector의 Skin Armor는 그 예외가 아니다. 따라서 **Selector의 비기본 몸 재적용 → BCNG 연결 상실 → 다음 스킨 적용 차단**은 실제 소스상 연결되는 충돌 경로다. Default는 BCNG 적용 식별자를 비우고 다음 선택이 새 기준으로 그래프를 만들 수 있도록 하므로, 제보의 Default 경유 차이와 부합한다. 다만 제보자가 당시 Selector 비기본 값을 사용했는지와 이 경로가 실제 발생했는지는 아직 확인하지 못했다. 모든 A→B 실패나 저장 파일 손상을 이것으로 단정하지 않는다.

얼굴은 두 모드가 같은 persistent node override를 기록해 최종 실행 순서에 영향을 받는다. 저장 후 재적용되는 상태 충돌 가능성과 세이브 데이터 손상은 다른 문제다. Selector 소스의 대상은 플레이어이며 Immersive Wenches NPC의 프리셋 목록 누락을 직접 설명하지 않는다.

**판정:** 정적 분석에서 몸/얼굴 쓰기 충돌을 확인했으며 동시 제어 호환을 보장할 수 없다. BCNG에서 단순 보호 검사 제거 또는 매 프레임 재덮어쓰기로 해결하지 않는다. 외부 스킨을 기본값으로 복원하는 계약, BCNG 활성 선택 우선권, 얼굴 채널 저장/복원 정책을 함께 정해야 한다. 호환 수정·동시 실행 검증은 아직 하지 않았고, 셀렉터 설치/비활성화·외부 override 삭제·게임/MO2 교체도 하지 않았다.

#### 추가 확인: 이벤트 출처와 협조 패치 가능성

원본 ESP의 `RaceMenuSkinSelector` 퀘스트(local `000801`) VMAD에 `AixBodySelectorScript`와 `RaceMenuLoad`가 연결되어 있다. 동봉된 RaceMenuLoad.psc는 ReferenceAlias의 `OnLoad`에서 소유 퀘스트의 `On3DLoaded`를 호출한다. RaceMenuBase.psc는 `RSM_LoadPlugins` 이벤트를 `OnReloadSettings`에 연결한다. **따라서 Selector의 On3DLoaded를 SKSE NiNodeUpdate 알림과 동일한 이벤트로 취급하거나, 모든 BCNG 클릭마다 반드시 발생한다고 단정하면 안 된다.** 최신 설치 RaceMenu의 PEX를 디컴파일하여 동봉 소스와 전체 동등성을 검증한 것은 아니다.

[SKSE ActorBase 소스](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusActorBase.cpp)의 SetSkin은 ActorBase의 skin 포인터에 직접 대입한다. [RaceMenu NiOverride 소스](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/PapyrusNiOverride.cpp)의 AddNodeOverride는 persist=true일 때 같은 맵을 기록하고 즉시 같은 노드 재질에도 반영한다. 별도 제공자 우선권 토큰이나 완료 알림을 통해 두 호출을 조율하는 구조가 아니다. 조회 소스는 런타임에 새로 설치한 라이브러리가 아니다.

Selector의 공개 함수 및 이벤트에는 BCNG가 재적용을 일시 중지하거나 소유권을 협상하는 API가 없었다. 현재 CommonLib의 FindBoundObject/GetVariable/DispatchMethodCall로 퀘스트의 선택값을 읽거나 함수를 호출하는 수단은 있으나, 읽기/호출 수단의 존재가 외부 함수의 실행 중단이나 동시 쓰기 배제를 보장하지는 않는다.

안전한 협조 패치의 설계 후보:

1. Selector의 몸/얼굴 선택값은 Selector가 그대로 저장한다. BCNG가 그 값을 0으로 강제하거나 private clone FormID를 Selector의 저장 프로퍼티에 집어넣지 않는다.
2. `UpdateBody`의 SetSkin 직전과 `UpdateSkin`의 Add/RemoveNodeOverride 직전에 BCNG 활성 여부를 확인해, 활성 동안에는 BCNG가 해당 변경을 처리하도록 협조한다. BCNG가 없거나 비활성이라면 원본 동작을 그대로 실행한다.
3. BCNG 일반 스킨은 선택된 Selector 메시를 복사한 개인용 Skin Armor의 텍스처만 바꾼다. BCNG 스킨 선택 중 Selector의 몸/메시 선택이 바뀌면 새 메시를 기준으로 안전하게 재구성해야 한다.
4. Default 시에는 과거 snapshot만 복원하지 않고 Selector가 **현재** 저장한 몸/얼굴 선택을 복원한다. 미리보기 취소는 직전 확정 BCNG 선택으로, BCNG 적용이 없던 미리보기의 취소는 현재 Selector 선택으로 돌아가야 한다.
5. 소유권 변경·세이브 로드·게임 종료·RaceMenu 재적용·마운트 지연 콜백과 BCNG 비활성 상태에서도 검증해야 한다. 콜백은 세대별로 무효화하고 오래된 작업이 새 선택을 덮지 않도록 한다.

이는 아직 구현한 API가 아니라 검토 결과다. 원본 스크립트를 그대로 둔 채 BCNG만 감시/재적용하면 관찰 시점 이전의 쓰기와 Papyrus 지연 실행을 막을 수 없다. 전역 SetSkin/NiOverride/VM 함수를 가로채는 BCNG-only 대안은 다른 모드 및 RaceMenu 세대 전체를 건드리는 위험이 크다. **현재 확인한 경계에서는 작은 선택적 Selector 협조 패치 + BCNG 연결이 가장 명확한 해결 방향**이며, 패치 작성/배포와 인게임 검증은 별도 작업이다.

## 1.2.4 추가 수정 및 재검증

### 커스텀 NPC 바디 계열

- 실제 로드된 피부 형상 이름을 폼·폴더 이름보다 우선한다. 폴더 이름과 임의 FormID 문자열을 형상 이름의 근거와 합치지 않는다.
- Skin Armor에 여러 종족이나 별도 성기 부품이 있어도 현재 종족의 몸·손·발 ARMA만 계열 근거로 사용한다. CBBE/UNP/UBE/HIMBO/SAM 기존 성별 구분을 유지한다.
- 확인된 표준 텍스처 레이아웃을 버린 뒤 설치된 UBE만으로 재추정하던 두 번째 fallback을 제거했다. 근거가 없거나 최상위 근거끼리 충돌하면 기존 unknown/no-filter 정책을 유지하며 모든 커스텀 NPC를 CBBE로 강제하지 않는다.
- 기존 NiNode 이벤트에서 해당 액터의 분류 캐시만 지우고 필요할 때 재계산한다. 이미 확보한 BCNG 원래 제공자 계열은 유지한다. 분류 로그는 debug로 낮춰 재생성 때마다 일반 로그가 늘어나지 않게 했다.
- 진단 프로그램을 새 제품 함수에 연결했다. 이전 세 반례는 각각 unknown(목록 보존), CBBE, CBBE로 바뀌었고 CBBE 프리셋이 표시되는 결과를 확인했다. 총 4,374가지 근거 조합을 테스트했다.
- 제보자의 Eila 최종 환경은 확인할 수 없다. 원본 IW와 두부 파일의 연결 및 재현 가능한 코드상 오판을 고친 것이며 제보자의 인게임 재현 성공으로 표현하지 않는다.

### NIF 내장 피부 및 얼굴 전용 → 몸 포함 팩

확인한 누락은 두 가지다. 일반 Skin TXST/NAM 또는 FLST가 없으면 기존 코드는 정해진 UBE 폴더에만 고정 DDS 기본값을 만들었다. 다른 경로의 UBE와 같은 방식의 CBBE/UNP/남성 커스텀 몸은 일반 피부 대상이 없는 상태로 남을 수 있었다. 또한 UBE 예외 생성 여부가 **처음 팩의 body 목록이 비어 있는가**에 의존해 얼굴 전용 팩으로 시작한 재사용 그래프는 나중에 몸 팩을 선택해도 부족한 대상을 보충하지 않았다.

두부 UBE 원본의 `00UBE_NakedTorso/Hands/Feet` ARMA에는 여성 NAM1이 없고 모델 경로만 있다. 설치된 출력 세 NIF의 실제 본체·손·발 형상은 SkinTint(shader type 5, flags1 `82600303`)이며 `!UBE\Body\femalebody_1_d.dds`, `_n.dds`, `_sk.dds`를 공유한다. 스킨 채널은 0/1/2이며 tangent normal이다.

| 설치 출력 | SHA-256 |
| --- | --- |
| Body/femalebody_tangent_1.nif | b79fbe0be532158508651ca524e7f1f598d4c4f05cffe2e236c513afd853c25b |
| Hands/femalehands_tangent_1.nif | 6d2dd0597b7254829f964d99aff61f6be2b681fb804f25036e11e5132e19bc43 |
| Feet/femalefeet_tangent_1.nif | 8b507fd1652f8319fad086ff91482239ecebde2a50fa5aec9d39748e9dc8bf53 |

수정 내용:

1. 기존 NAM/FLST가 없는 몸·손·발 부품은 실제 원본 모델을 읽어 피부 재질의 모든 지원 텍스처 채널과 노멀 방식을 가져온다. 특정 UBE 폴더를 요구하거나 고정 여성 DDS를 임의 생성하지 않는다.
2. 1·3인칭의 피부 아틀라스·부위·제공자·노멀 방식이 같으면 복제 ARMA의 NAM에 개인용 TXST를 연결한다. 서로 다르면 안전하게 확인한 형상만 index3D와 이름을 키로 복제 모델의 MODS에 연결한다. 다른 형상까지 같은 아틀라스로 덮지 않는다.
3. 원래 MODS가 있으면 해당 TXST의 경로·flags를 보존하고 그 제공자를 복제한다. 원본 ARMA/MODS/TXST와 로드된 원본 NIF에는 쓰지 않는다. 복제본 안의 기존 MODS 별칭도 새 전용 TXST와 일치시킨다.
4. 읽을 수 없는 한 모델이나 잘못된 자식 하나 때문에 다른 정상 형상까지 모두 차단하지 않는다. 전체 공용 NAM을 추측하는 대신 확인된 형상별 적용을 사용하고 나머지는 원래 제공자를 유지한다.
5. 내부 그래프를 첫 팩의 내용과 무관하게 준비한다. 얼굴 전용 적용은 여전히 몸 그래프를 연결하지 않지만 이후 몸 팩은 준비된 대상에 바로 적용할 수 있다. 반복 선택 중 모델을 매번 읽거나 타이머를 추가하지 않는다.
6. 실제 종족에 맞는 ARMA만 모델 검사하므로 공용 Skin Armor의 다른 종족용 NIF까지 모두 불러오지 않는다. 부품 복제와 소유권·미리보기·세이브 형식은 유지한다. 같은 몸 ARMA에 별도 SOS/TNG 성기나 다른 UV의 질·항문 형상이 있어도 일반 피부로 취급하지 않으며, UBE 전용 슬롯 53도 계열 판정 대상에 포함한다.

### 문서와 검증

- 1.2.4 영문 Nexus BBCode·Markdown, 한글 HTML 소개글 및 영·한 체인지로그를 작성했다. 전체 설치 경로와 폴더 통째 복사 안내를 유지했다.
- OBody NG와 RaceMenu Selector의 동시 외형 제어를 비호환으로 안내한다. 독립 OBody ORefit JSON 입력과 필수 RaceMenu 본체는 별개다. Selector 패치는 작성하지 않았다.
- SE/AE Release 빌드와 자동 테스트 실행 파일 25개 통과. UBE 실제 OSP 재구성 계산 17,045건도 통과했다. 새로운 검사에는 기존 NAM/FLST 보존, 각 바디 계열, UBE 슬롯 53, 성기 전용 슬롯 제외, 8채널 보존, 복수 아틀라스, 1·3인칭 차이, 노멀 방식, 원래 MODS 제공자, 얼굴 전용 이후 몸 대상 재사용이 포함된다.
- 최신 DLL: `build/v1.2.4/windows/x64/release/BodyChangeNG.dll`, 1.2.4.0, 2,783,744 bytes, SHA-256 `68833DE5DC03ADF14E7F5821161CEB87DA7D3DBA5EA0F3C64612062DB0132B00`.
- 이 시점의 결과는 정적 자료·코드와 호스트 테스트 결과다. AE 1.6.1179 제보자의 실제 원인이 동일했는지, 최신 수정으로 해당 게임에서 재현이 사라졌는지는 아직 확인하지 않았다. 이후 확인 결과는 아래 2026-09-16 기록을 따른다.

## 2026-09-16 — v1.2.4 릴리즈 확정

- 두부 AE 1.6.1170의 제한된 읽기 전용 진단에서 NPC 얼굴의 BCNG DDS가 약 1~2초 뒤 RSV DDS로 교체되는 것을 확인했다. 숫자 피부색과 FaceTint 경로는 유지됐다. 같은 스킨의 확정 직후에는 정상으로 보였지만, 이후 다른 재생성 이벤트까지 안전하다는 의미는 아니다.
- 사용자는 같은 MO2 환경에서 RSV를 끄고 정상 작동을 확인했다. 이 결과를 모든 게임 버전·액터·세이브 조합의 검증으로 일반화하지 않는다. Selector는 이번 인게임 검증 대상이 아니다.
- RSV와 Selector는 비호환 안내로 정리했다. 외부 모드의 변경을 차단하는 훅, 자동 재도색, 별도 모드 패치는 구현하지 않았다.
- 임시 얼굴 진단 블록 6개와 전용 빌드 옵션을 제거했다. 원래의 얼굴 상태 확인·복원 코드는 유지한다. 진단 로그·임시 DLL·개인 MO2 자료는 배포 및 소스 ZIP에 넣지 않는다.
- 실제 버튼·알림·레이아웃 테스트 문구와 영문/한글 소개글을 ORefit 의상 보정 규칙 등록 / Register ORefit outfit-correction rules로 통일했다. ORefit JSON Master List 링크는 유지하고 RSV·Selector 소개 항목의 링크는 제거했다. JSON 파일 경로와 읽기 로직은 바꾸지 않았다.
- 최종 SE/AE Release 빌드 및 자동 테스트 실행 파일 25개 통과. `EXCLUSIVE_SKYRIM_FLAT` 구성과 기존 의존성 핀을 유지했다.
- 최종 DLL: `build/v1.2.4/windows/x64/release/BodyChangeNG.dll`, 1.2.4.0, 2,783,744 bytes, SHA-256 `93C1BE2FD452EF875401D80FD9A259156C145BE1FA4E03C31908C455197A37DD`.
