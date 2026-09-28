# UBE / 3BA 후타스킨 분리 점검 — 2026-09-28

## 재현 조건과 원인

툴레드 `TuLED(SL)`의 Papyrus 로그에는 플레이어의 SOS 애드온을 `SOS UBE`로 바꾼 기록이 있다.
기존 BCNG 로그의 후타스킨 카탈로그는 11개였다. 목록 파일 자체를 읽지 못한 상황은 아니다.

설치된 ESP를 레코드 경계에 따라 읽어 다음 슬롯/모델을 확인했다.

| 애드온 | ARMO | ARMA | 여성 모델 | 등록 팩션 EDID |
|---|---|---|---|---|
| UBE 기본 SOS | 52, 54 | 53, 54 | `!UBE\SOS_Addon\ube_penis_1.nif` | `SOS_Addon_UBE_Faction` |
| TRX UBE | 52, 54 | 53, 54 | `[TRX] Futa addon UBE\trx_schlong_1.nif` | `SOS_Addon_TRX_FutaU_Faction` |
| CBBE 3BA TRX | 52 | 52 | `[TRX] Futa addon\Regular\trx_schlong_1.nif` | `SOS_Addon_TRX_Futa_Faction` |
| CBBE 3BA ERF | 52 | 52 | `ERF_Futanari\futanari_schlong_cbbe_1.nif` | `SOS_Addon_ERF_Futa1_Faction` |

기존 코드에는 설치 검색, 현재 장착 대상 검색, 적용 대상 필터, 네이티브 텍스처 방문자에
각각 52번 전용 조건이 있었다. UBE의 실제 공통 슬롯 54번이 누락됐다.
또한 UBE 기본 팩션은 이름에 `futa`가 없어 미장착 상태의 등록 판정에서도 제외됐다.
TRX UBE DDS 경로는 카탈로그에 없었고, UBE 몸의 TRX를 UBE 기본 텍스처 유형으로 매핑했다.

## 수정 범위

- 3BA TRX/ERF와 기존 남성 경로는 공통 52번을 그대로 사용한다.
- 모델로 확인된 UBE 기본형/TRX UBE만 공통 54번을 추가 허용한다.
  ARMO의 52번 비트, ARMO/ARMA 공통 슬롯 및 모델 계열을 함께 확인한다.
  UBE 일반 몸통에 쓰이는 53번은 성기 텍스처 대상으로 허용하지 않는다.
- 네이티브 방문자는 22/24 인덱스만 읽고, 24번은 여성·UBE 애드온·정확한 대상과
  SkinTint 재질일 때만 처리한다. 남성 경로, 모발 재질, 다른 액터/애드온은 확장하지 않는다.
- UBE 등록 팩션은 같은 플러그인에서 확인된 지원 SOS 애드온과 연결된 정확한
  EDID/표시 이름만 추가 허용한다. 다른 모든 팩션을 받아들이는 변경은 아니다.
- UBE 기본형 `malebody_1`은 기존 enum 값과 `:ube-trx` ID를 유지한다.
  TRX UBE `schlong`에는 별도 `ubeTrxAddon` / `:ube-trx-addon` 유형을 추가한다.
  경로: `Futanari\<팩>\Textures\[TRX] Futa addon UBE\schlong.dds`.
- UI, 적용 전 검사, 재적용 검사에서 두 UBE 유형과 3BA TRX/ERF를 분리한다.
  기존 자동 배포의 UBE 제외 정책은 두 UBE 유형 모두에 유지한다.
- 스크립트, 다른 모드의 ESP/NIF/DDS, SOS/SexLab 설정, 세이브 형식은 수정하지 않는다.

## 로컬 실물 대조

- UBE 기본형의 활성 BodySlide 출력: `!UBE Bodyslide Output\meshes\!UBE\SOS_Addon\ube_penis_1.nif`.
  `Penis` 노드, SkinTint(5), `!UBE\Body\malebody_1_d.dds`.
- TRX UBE: `TRX UBE Futanari addon_102`의 `FemaleDick` 노드, SkinTint(5),
  `[TRX] Futa addon UBE\Schlong.dds`.
- 두 UBE 메시에서 성기 분할 54번도 확인했다. 원본 파일은 변경하지 않았다.
- 3BA `Body Slide Output`의 ERF는 `CBBE Schlong`, TRX는 `CBBE_Schlong` 노드다.
  원본 TRX의 `CBBE_Shlong` 철자도 지원한다. 모두 SkinTint(5)와 기존 DDS 경로로 일치한다.
  원본 TRX의 `PUBIC_1` 모발 재질(6)은 네이티브 SkinTint 대상이 아니다.
- 현재 프로필에서 ERF는 활성, 일반 3BA TRX 플러그인은 비활성이다.
  비활성 TRX 원본과 활성 BodySlide 출력도 읽어서 대조했지만 모드 활성 상태는 바꾸지 않았다.
- 수정된 실제 스캐너가 `Body Changer NG Skin_Tint\Futanari`에서 총 12개를 읽는다:
  UBE 기본형 3, TRX UBE 1, 3BA TRX 4, ERF 4. 기존 11개 ID는 유지한다.

## 검증 경계

SE/AE flat releasedbg 빌드와 42개 회귀 테스트를 사용한다.
슬롯/성별/계열 조합, 기존 등록 팩션, 모델·노드·DDS 대소문자,
새 유형의 저장 ID 분리, 선택 스냅샷/기본 복원, 빌린 BIPOBJECT 무변경을 검사한다.
새 슬롯에 별도 수명 관리나 캐시를 추가하지 않으며 기존 값 스냅샷과 TXST 참조 수명을 유지한다.
이 기록은 파일·오프라인 검사이며 이번 수정 DLL의 인게임 렌더 확인을 뜻하지 않는다.
MO2나 GitHub에는 이 수정 작업만으로 배포하지 않는다.

최종 결과: 전체 빌드 성공, 42/42 테스트 통과, `git diff --check` 통과.
수정 DLL: `build\v1.3.4\windows\x64\releasedbg\BodyChangeNG.dll`
(SHA-256 `01D366C90C877723A9752C06940FB7177E65F8AE5C2933A01F2D3DE8428A722D`).
툴레드 설치 DLL은 교체하지 않았으며 기존 SHA-256
`354009D11CF839B04A2755D955F4C4ADDA59361683B46D284150D8E26B9494C8`을 유지한다.
