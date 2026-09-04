# Body Change NG 1.1.0 전체 작업 인수인계

작성일: 2026-09-04 (Asia/Seoul)

코드·배포 기준 커밋: `834e007cc3db64fb8024631e3588ea7af86068d7`

런타임 기능 최종 변경: `67607685f7ffae28a93a032811ca54e4cf704955`

대상: 다음 작업자, 릴리스 담당자, 회귀 점검 담당자

> 이 문서는 현재 소스·Git 이력·배포 파일·이 대화에서 확정한 요구사항을 연결하는 기술 인수인계다. “요청됨”, “코드에 구현됨”, “자동 테스트로 확인됨”, “인게임에서 확인됨”을 서로 바꿔 쓰지 않는다. 이전 대화의 제안이나 완료 표현만으로 실제 구현 또는 무결성을 보증하지 않는다.
>
> 본문과 함께 [전체 Git 이력·변경 파일·증거 부록](HANDOVER-EVIDENCE-v1.1.0-KO.md)을 읽는다. 부록에는 기준 커밋까지 **54개 커밋 전체**, 각각의 전체 변경 파일 목록, 현재 소스·테스트·문서 목록, 아티팩트 해시가 있다. Git 기준선 이전의 대화는 결정 이력으로 정리했으며, 존재하지 않는 커밋을 만들어 연결하지 않았다.

## 목차

1. 현재 상태와 다음 작업자가 먼저 알아야 할 사항
2. 경로·파일·배포 상태
3. 사용자 확정 요구사항과 폐기·변경된 결정
4. 전체 개발 단계
5. 코드 구조와 실행 수명주기
6. 바디 프리셋·BodyFamily·모프 중첩
7. 바디스킨·부분 텍스처·종족·UBE·SOS
8. 플레이어 틴트마스크
9. NPC 배포 조건·JSON·저장 데이터
10. 의상 보정·무작위화·OBody JSON
11. 성능·작업 큐·중복 방지
12. 카메라·회전·일시정지·UI
13. 이름 변경·경로 이전·구 BodyChange 얼굴 프리셋
14. 사용자 피드백별 조치와 남은 검증
15. 자동 테스트와 검증의 정확한 범위
16. 빌드·의존성·런타임 ABI
17. 릴리스·소스 ZIP·GitHub·MO2 운영
18. Nexus 소개글·업데이트 이력·라이선스
19. 우선순위별 후속 점검
20. 다음 작업자 실행 순서·금지 사항·참고 자료

## 1. 현재 상태와 먼저 알아야 할 사항

### 1.1 현재 기준

| 항목 | 상태 |
| --- | --- |
| 최종 이름 | **Body Change NG**. Body Changer NG는 이전 이름이다. |
| 제품 버전 | **1.1.0**, DLL FileVersion/ProductVersion **1.1.0.0** |
| 실제 저장소 | `C:\Users\yunha\Desktop\Body Change NG` |
| 이 작업의 도구 기본 cwd | `C:\Users\yunha\Desktop\Obody NPC`. 실제 저장소와 다르므로 상대 경로 주의. |
| Git 브랜치 | `main` |
| 문서 작성 시작 직전 작업 트리 | clean |
| GitHub | https://github.com/compilecraftworks/Body-Change-NG |
| GitHub 릴리스 | https://github.com/compilecraftworks/Body-Change-NG/releases/tag/v1.1.0 |
| Nexus 모드 페이지 | https://www.nexusmods.com/skyrimspecialedition/mods/189962 |
| 런타임 코드 변경 여부 | 이번 인수인계 작성 작업에서는 변경하지 않는다. |
| 이번 문서의 배포 범위 | 개발 문서. MO2 설치 ZIP에 넣지 않는다. |

`v1.1.0` 태그는 `26408f2`를 가리킨다. 그 뒤 최소 패키징 변경은 `834e007` / `v1.1.0-packaging.1`로 분리했다. 원래 `v1.1.0` 태그를 강제 이동하지 않았다. 현재 공개 1.1.0 소스 ZIP은 패키징 후속 커밋 기준이므로 태그 이름만 보고 `26408f2`와 파일이 완전히 같다고 가정하지 않는다.

### 1.2 가장 중요한 경계

- 바디·스킨: 플레이어와 선택한 NPC에 직접 적용, NPC 조건 배포 지원.
- 틴트: **플레이어의 RaceMenu 틴트 레이어만** 지원. NPC 틴트 배포로 소개하면 안 된다.
- 바디 모프 중첩: 전역 `ClearMorphs`로 타 모드 값을 지우는 방식이 아니다. 호환 슬라이더 집합에 대해 BCNG 소유 키에 보정값을 기록한다.
- 프리셋 계열: 슬라이더 이름 점수 판정안은 사용자가 철회했다. **XML의 set → Group → 이름/경로 보조 정보**가 현재 기준이다.
- 피부: 실제 메시·부위·셰이더 채널에 맞춰 존재하는 DDS만 바꾼다. 몸·손·발·얼굴을 서로 대신 넣지 않는다.
- 자동 배포: 성능 모드 OFF도 큐 경로를 사용한다. “OFF이면 이벤트 콜백 안에서 모든 NPC를 즉시 처리”하는 과거 경로를 되살리지 않는다.
- 큐의 “한 번”은 **게임 태스크 처리 차례**다. 실제 렌더 프레임 하나에 NPC 한 명이라는 보장은 없다.
- 카메라: 최종 정상 구도와 틴트 구도를 아래 확정값으로 구분한다. 예전 pause counter 실험은 최종 코드가 아니다.
- **OverlayFix 병용 크래시, 유방 충돌체, 모든 3BA/BHUNP/SOS/UBE 변형의 인게임 정상 여부는 확정 검증하지 못했다.**
- “스터터 0 / 누수 0 / 충돌 0”을 입증하는 인게임 프로파일링은 없다.
- MO2 설치용 ZIP은 현재 **정확히 7파일**이다. 개발 이력·체인지로그·소개글을 다시 넣지 않는다.

## 2. 경로·파일·배포 상태

### 2.1 게임 상대 경로

여기서 `Data\`는 게임의 가상 Data 루트다. MO2 모드 루트 안에는 보통 `Data`를 한 번 더 만들지 않는다.

| 용도 | 게임 기준 경로 |
| --- | --- |
| 플러그인 DLL | `Data\SKSE\Plugins\BodyChangeNG.dll` |
| 자체 조건 JSON | `Data\SKSE\Plugins\BodyChangeNGdistribution.json` |
| 환경 설정 | `Data\SKSE\Plugins\BodyChangeNG\settings.json` |
| 선택적 OBody JSON | `Data\SKSE\Plugins\OBody_presetDistributionConfig.json` |
| 바디 프리셋 | `Data\CalienteTools\BodySlide\SliderPresets\*.xml` |
| 스킨팩 | `Data\BodySkin\<팩 이름>\Textures\...` |
| 틴트팩 | `Data\TintMask\<팩 이름>\textures\...` |
| 런타임 텍스처 별칭 캐시 | `Data\textures\BodyChangeNG\Cache\<namespace>\<경로 해시>.dds` |
| 구 얼굴 프리셋 검사 | `Data\SKSE\Plugins\CharGen\Presets\*.jslot`, 하위 폴더 포함 |
| 플러그인 로그 | `Documents\My Games\Skyrim Special Edition\SKSE\BodyChangeNG.log` |

중요: OBody 파일은 **`OBody_presetDistributionConfig.json`이라는 파일 하나**다. 대화 중 표기되었던 `OBody\_presetDistributionConfig.json`이라는 하위 디렉터리 구조를 현재 구현으로 안내하지 않는다. 자체 JSON과 같은 `SKSE\Plugins` 폴더에 나란히 놓는다. 두 JSON을 합치거나 서로 덮어쓰지 않는다.

MO2에서는 설정/캐시/백업이 Overwrite 또는 기존 제공 모드로 기록될 수 있다. 모드 폴더만 볼 것이 아니라 가상 Data 승자와 Overwrite도 확인한다.

### 2.2 로컬 릴리스 아티팩트

| 파일 | 바이트 | SHA-256 |
| --- | ---: | --- |
| `release\Body Change NG v1.1.0.zip` | 1,098,586 | `821D7FF39EE12AA2F058B4BD60FE6694A5F345283924044659C7741994610499` |
| `release\Body Change NG v1.1.0 Source.zip` | 5,353,027 | `D3953DA061D6C564B648F93D0D258A70163F18219AD632678A280D886F25B121` |
| `build\v1.1.0\windows\x64\release\BodyChangeNG.dll` | 2,258,944 | `8BF779DCEA540EA8B34EF9778DEBABBE4A7CE4A5F6745D3A15EE4F8F95717732` |

GitHub asset명은 공백 대신 하이픈을 사용한다.

- `Body-Change-NG-v1.1.0.zip`
- `Body-Change-NG-v1.1.0-Source.zip`
- `SHA256SUMS-v1.1.0.txt`

로컬 체크섬 파일의 공백 포함 파일명과 GitHub 체크섬 파일의 하이픈 파일명이 다르지만 ZIP 내용 해시는 같다. GitHub가 업로드명 공백을 점으로 바꿨던 문제가 있어 명시적으로 하이픈 이름으로 정리했다.

### 2.3 MO2 설치 상태

대상: `D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG`

- 현재 설치 DLL 해시는 위 빌드 DLL과 동일한 `8BF779DC…717732`임을 인수인계 작성 중 다시 확인했다.
- 1.1.0 배포 당시 사용자 설정과 배포 JSON은 덮지 않도록 전후 해시를 확인했으며 사용자 에셋은 보존했다.
- **최소 ZIP 작업 이후 기존 MO2 폴더 안의 문서까지 청소한 것은 아니다.** MO2 루트에는 README.md, CHANGELOG.md, CHANGELOG-KO.md, LICENSE, 이전 THIRD_PARTY_NOTICES.md, meta.ini 등이 남아 있다.
- 현재 MO2의 THIRD_PARTY_NOTICES.md는 2,469바이트인 이전 버전이다. 새 ZIP의 통합 고지문은 9,456바이트다. DLL 최신 여부와 문서 최신 여부를 구분한다.
- 이번 인수인계 작업은 MO2 설정이나 파일을 다시 덮지 않는다.

## 3. 사용자 확정 요구사항과 폐기·변경된 결정

이 표는 긴 대화에서 뒤집힌 결정을 다시 적용하지 않기 위한 기준이다.

| 주제 | 최종 결정 | 폐기·주의할 과거안 |
| --- | --- | --- |
| 제품 이름 | Body Change NG, 내부 DLL/경로/소스명도 변경 | Body Changer NG를 새 파일명에 사용하지 않음 |
| 버전 | 1.1.0 | 처음 릴리스 준비 때의 1.0.0에 새 기능을 계속 덮는 방식 종료 |
| 탭 | 바디프리셋 / 바디스킨 / 틴트마스크 | 짧은 바디 / 스킨 / 틴트 표기에서 확장 |
| 기본 규칙 | 커스텀 팔로워·노인 남녀 바디만 제외, 아르고니안·카짓 남녀 스킨만 제외 | 커스텀 팔로워·노인 바디+스킨 모두 제외 아님 |
| 인종별 여성 제외 | 앞서 요청한 브레튼·다크엘프·오크·하이엘프·임페리얼·레드가드 제외안 취소 | 이 6개를 기본 규칙에 재등록하지 않음 |
| 커스텀 팔로워 | 모드로 새로 추가된 팔로워 대상 | 바닐라 팔로워 단순 리텍은 이 범주에서 제외 |
| 채널별 제외 | 바디/스킨을 독립적으로 배포·제외·무변경 설정 | 하나 제외하면 다른 채널도 잠기는 UI 금지 |
| 표시 수량 | “1개 바디 · 3개 스킨”, 제외 채널은 “배포 제외” 명시 | 수량 설명에 “스킨팩”을 고집하지 않음 |
| 드롭다운 | 공통 목록 높이 조절, 선택값 표시, 종족·팩션·플러그인 등은 선택형 | 입력으로만 EditorID를 요구하지 않음 |
| 계열 분류 | XML set/Group 우선 | 슬라이더 고유도 점수화·임계값 방식 철회 |
| 복수 계열 | 같은 성별의 명확한 계열 여러 개는 모두 라벨·필터에 반영 | CBBE+BHUNP 또는 CBBE+UBE를 무조건 Unknown 처리하지 않음 |
| UBE Anus | `3BBB Body Amazing UBE Anus`는 CBBE 3BA | 단어 UBE만 보고 UBE 전신 계열로 오분류 금지 |
| 의상 이름 프리셋 | Clothes, Outfit, Bikini, Armor, Cuirass, Dress, Panty, Overalls도 노출 | 의상 관련 단어만으로 숨기는 필터 금지 |
| HIMBO | Clothes 프리셋도 노출 | Clothes 6개 제외안 철회 |
| NPC 계열 필터 | 이후 혼합 UBE/3BA 요청에 따라 런타임 후보 호환성 검사 | 초기 “메인 UI만, NPC 배포 풀은 자동 필터 없음”은 후속 요구로 변경됨. 편집자가 선택한 원래 풀 자체는 보존 |
| 누락 슬라이더 | 호환 슬라이더 집합 안에서 목표 0으로 초기화 후 XML 값 반영 | 새 XML에 적힌 슬라이더만 기존 값 교체하면 안 됨 |
| 타 모드 모프 | BCNG 보정 키로 합계를 맞춤 | 전체 ClearMorphs로 임신·근육 등 외부 소유 값 영구 삭제 금지 |
| 부분 스킨 | 미리보기·확정·배포 모두 있는 부위/채널만 적용 | 전체 세트 필수 조건, 몸 텍스처를 발/손에 복제하는 fallback 금지 |
| 3BA 보조 아틀라스 | 여성 외음부+항문 | “3BA_Vagina에만”이라는 중간 판단은 정정 |
| BHUNP | 외음부+항문+canal 아틀라스 | CBBE 보조 파일을 혼용하지 않음 |
| 특정 NPC 스킨 | Astrid/Afflicted 전용 파일은 일반 스킨 적용에서 제외 | 일반 여성 피부로 뿌리지 않음 |
| 노인/종족 normal | 해당 액터에 해당하는 파일만 추가 반영 | 다른 종족 전용 파일을 대체재로 쓰지 않음 |
| BodySkin tintmasks | 바디스킨/조건 배포에서 제외 | TintMask 전용 카탈로그와 혼동하지 않음 |
| OBody 의상 JSON | 7종 지원 데이터 명시 등록 | 제외 의상만 읽는 초기 축소안 이후 확대 |
| ORefit 명칭 | 사용자 UI에서는 의상 보정·가슴/유두 보정 | 내부 코드·원본 호환 키는 ORefit 명칭 유지 가능 |
| OBody 시작 자동 읽기 | 하지 않음. 버튼으로 명시적 읽기 | 없는 파일로 오류 강요하거나 자체 JSON 덮기 금지 |
| 저장 값 불러오기 | 별도 선택창 없이 자체 JSON + 있으면 OBody 배포 JSON | 둘 중 하나 선택 팝업안 취소 |
| 액터 결과 저장 | SKSE 코세이브, 규칙은 전역 JSON | 모든 액터 결과를 전역 JSON만으로 유지하는 초기 구조에서 변경 |
| 규칙 NPC ID | Plugin + Local Base FormID | 런타임 load-order 전체 FormID만 저장하지 않음 |
| 코세이브 액터 ID | RefID를 저장하고 ResolveFormID | 같은 Base NPC 여러 인스턴스를 한 개로 합치지 않음 |
| 옵션 간소화 | 성능 모드는 유지. 수동 바디 계열·수동 NPC 잠금·OBody 자동 호환 등의 불필요한 노출 제거 | 숨긴 설정 UI를 기능 삭제로 오해하지 않음 |
| 설치 기본값 | 일시정지 OFF, 캐릭터 왼쪽, 본체 UI 오른쪽 | 이전 설정이 있으면 사용자의 저장값을 존중 |
| 카메라 정상 | FOV70 / 거리200 / 좌우+70,-70 / 세로-45 / 플레이어 pitch0.1 | 거리140/±85/±90/pitch0 등 중간값 폐기 |
| 카메라 틴트 | FOV70 유지 / 거리80 / 좌우+28,-28 / 세로+10 | 지나치게 가까움·얼굴 화면 밖·-13.5 등의 중간안 폐기 |
| NPC 배포 하단 | 7버튼 한 줄, 그에 맞는 팝업 폭 | 두 줄 및 하단 버튼이 화면 밖으로 밀리는 회귀 금지 |
| 빈 폴더 | 안내 README.txt + 실제 기본 배포 JSON | .keep만 넣지 않음 |
| MO2 설치 ZIP | 런타임·배치 안내 + 필요한 라이선스 2문서만 | README/체인지로그/Nexus/인수인계 대량 동봉 금지 |
| 소스 ZIP | 빌드 가능한 필수 의존 소스 포함, 캐시·빌드·Git 이력 제외 | 거대한 개발 폴더 통째 압축 금지 |

“Unknown으로 하지 말라”는 복수의 **명확한 동성 계열**을 잃지 말라는 지시다. 현재 코드에는 무증거 및 남녀 계열 충돌용 `Unclassified` fallback은 남아 있다.

## 4. 전체 개발 단계

| 단계 | 대표 커밋 | 주요 내용 |
| --- | --- | --- |
| Git 기준선 | d6957cd | 기존 Body Changer NG 소스·빌드·패키지·테스트 추적 시작 |
| 저장·조건/UI 기반 | cc2a2cf | 액터 상태 코세이브, 배포 UI/조건, 관련 테스트 정리 |
| 입력/UI 회귀 복구 | f5e7c02, 5ee5508 | 드롭다운 빈 선택값, 백스페이스, 분할 패널 크기 조절 |
| 카메라 반복 조정 | f1d20a5 → e81565a | 틴트 확대, 좌우 대칭, FOV, pause, 회전, 복원, 하단 버튼/컬러 동기화 |
| pause 실험 폐기 | b9e9f3f → e3e08aa | settle 실험 후 제거. 최종 구현과 SFS 참고 구현을 구별 |
| 1.0.0 준비·설명 | eab8a94 → accdd25 | 영어 BBCode/한글 HTML, 경로/조건/JSON/크레딧, 문구 간소화 |
| 마크다운 이력 | 4da38a1 | BBCode 없는 영문 업데이트 이력 |
| 제품명 전면 변경 | 9fc852f → 888fae7 | Body Change NG로 소스/설정/패키지/문서/이미지/GitHub 명칭 정리 |
| UBE 혼합 환경 | 5c16dbc | 액터별 UBE/일반 계열, 스킨·틴트 레이아웃, NPC 혼합 풀 호환성 |
| 성능 피드백 | dc5f5ee, e841e04 | 자동 모프 갱신 분산, 정상 모드도 이벤트 즉시 실행 경로 제거 |
| 구 얼굴 프리셋 | 54b7742 | BodyChange.esp 없는 환경의 .jslot HeadPart 안전 이전 |
| 부분 스킨·남성 | ef1734f | 없는 부위·채널 유지, 경로/소유권/미리보기/적용 보강 |
| 탭·수인 | 91c4f0f, d97e168 | 탭 명칭, 남녀 Argonian/Khajiit 피부 |
| 3BA/노인/UNP | d2c37a7, 269fd18, db21427 | 3BA 아틀라스, 노인·종족 normal, CBBE/UNP 부위별 정확한 라우팅 |
| 피드백 종합 반영 | 6760768 | 절대 모프 보정, 남성 손/부위, SOS, 팩션·종족 ID, keyword/class/combat style, 지연·테스트 |
| 1.1.0 릴리스 | 26408f2 | 버전, 한영 문서, 빌드/소스 패키징 |
| 최소 MO2 ZIP | 834e007 | 7파일 allowlist, 라이선스 고지 통합, source 재패키징 |

전체 커밋의 날짜·원문 제목·변경 파일은 부록에 있으며, 여기서는 설명을 중복하지 않는다.

## 5. 코드 구조와 실행 수명주기

### 5.1 모듈 책임 지도

소스 루트: `src/BodyChangeNG/`. 정확한 전체 목록은 부록 참조.

| 모듈 | 역할·확인할 경계 |
| --- | --- |
| `src/main.cpp` | SKSE 로드, 로깅, ABI 초기화, 이벤트, 카탈로그 최초 스캔, 로드 후 초기 배포 예약 |
| `NativeImGuiHost` | DX11/Win32 ImGui 호스트, 입력 훅, 렌더, 텍스트 포커스, 테마 |
| `UI` | 액터/바디/스킨/틴트 목록, 미리보기/확정, 설정·배포·의상·틴트 팝업, 공통 드롭다운 |
| `InputSink / Hotkey / TextInputFilter` | F7·수정키 조합, 입력 캡처/취소, 문자열 편집 키 보호 |
| `ActorCatalog` | 주변 액터 목록 및 선택 대상 |
| `PresetCatalog / BodyFamilyRules / BodyFamily` | XML 파싱·ID·계열 메타데이터, 액터 메시/스킨 증거, 슬라이더 universe 캐시 |
| `CatalogRoots / PathText` | MO2 가상 경로의 실체 공급 폴더 발견, UTF-8/Windows 경로 처리 |
| `SkinProfiles / SkinCatalog` | DDS 부위·채널·성별·종족·계열 및 변형 카탈로그 |
| `SkinOverrides / SkinGeometryRouting / SkinOverrideOwnership` | 정확한 실제 노드/ARMA/채널 적용, 소유권 보호, preview/commit/restore |
| `RuntimeAssetCache` | 런타임 로드 가능한 textures 별칭을 hardlink 또는 copy로 제공 |
| `PlayerTint` | 플레이어 현재 틴트 레이어, DDS 선택, RGBA 조절·복원·코세이브 |
| `RaceMenuBodyMorph` | RaceMenu v4/v5 ABI, 바디 preview/commit/outfit 키, 절대 목표 보정·모델 반영 |
| `Distribution` | 규칙/JSON/OBody 배포 import, 첫 일치, 후보 선택·호환성 |
| `ActorState / ActorRegistry` | 결과·수동 선택·signature, 코세이브 ASTR/TINT, 중복 방지 |
| `ActorWorkQueue` | ActorHandle 기반 자동 배포 큐, 세션 세대, 병합·취소·우선 처리·메트릭 |
| `ActorEvents` | init/cell/equip/RaceSexMenu, 장비·3D 재생성 후 적용 |
| `OutfitRefit / OutfitRefitRules` | 명시 JSON 등록, 제외/강제/매핑, 현재 바디 -Refit 및 절차적 fallback |
| `MenuCharacterPresentation / MenuCameraProjection` | 정상/틴트 구도, 카메라·회전·FOV 캡처 및 복원 |
| `SmoothCamIntegration` | 선택적 외부 카메라 API. 소개글에서 제거했지만 구현은 존재 |
| `Settings / PathMigration` | 환경 설정/즐겨찾기, 새 이름 우선·이전 이름 fallback |
| `RaceMenuPresetMigration / RaceMenuPresetMigrationRules` | .jslot 구 BodyChange 얼굴 의존 제거·백업 |
| `RuntimeLayout` | 확인된 SE/AE 렌더·입력 훅, 미확인 런타임 차단 |

### 5.2 로드 순서

1. SKSE 초기화 후 로깅 초기화. 순서를 바꾸면 SKSE 기본 로거가 기존 로그를 덮거나 끊을 수 있다.
2. 설정 읽기, ActorRegistry 코세이브 콜백 등록, UI/입력/렌더 훅 초기화.
3. `kPostPostLoad`: 선택적 SmoothCam 인터페이스 교환 및 RaceMenu 인터페이스 초기화.
4. `kDataLoaded`: RaceMenu 확인 → 구 .jslot 얼굴 이전 → 바디 프리셋 스캔 → 스킨 스캔 → 자체 배포 JSON 읽기 → 의상 외부 규칙 초기화 → 액터 이벤트 등록.
5. `kInputLoaded`: 게임 입력 이벤트 등록.
6. `kPostLoadGame / kNewGame`: 큐·이벤트·모프·스킨·계열의 세션 캐시/세대를 정리한 뒤 초기 로드 NPC 확인을 2 scheduling hop 뒤 예약.
7. 이후 새 NPC·셀 attach·장비 변경·RaceMenu 종료 이벤트로 필요한 작업만 예약한다.

정적 모드 목록 또는 파일 파싱을 프레임마다 반복하지 않는다. OBody JSON은 4번에서 자동 대체 파일로 읽지 않는다.

### 5.3 소유권과 비동기 수명

- 지연 작업은 raw Actor 포인터를 오래 잡아두지 않고 ActorHandle을 다시 확인한다.
- 세션 세대/액터별 apply generation/preview generation을 통해 구 작업이 새 선택을 덮지 못하게 한다.
- actor detach, 탭/액터 전환, 세이브 로드, 메뉴 종료 시 관련 pending 작업을 무효화한다.
- Registry 선택 결과와 실제 3D가 존재한다는 사실은 다르다. 코세이브가 있어도 3D 재생성/장비 교체/외부 덮어쓰기 시 필요한 복구는 다시 실행된다.
- 프로세스 정적 캐시가 있다고 바로 메모리 누수라고 단정하지 않는다. 반대로 generation/컨테이너 정리만으로 장시간 누수 부재를 입증한 것도 아니다.

## 6. 바디 프리셋·BodyFamily·모프 중첩

### 6.1 파싱·목록·ID

- UBE를 포함해 표준 BodySlide XML 경로는 동일하다.
- **XML 파일 수와 프리셋 수는 다르다.** 한 XML에 여러 Preset이 들어간다. 대화에서 XML 40개에 프리셋 72개로 보인 이유를 파일별 항목 수로 설명했다.
- 당시 `Miggyluv's HIMBO Galore.xml` 20개, `HIMBO.xml` 16개, 나머지 XML 36개 각 1개라는 샘플 구성이 있었다. 설치 파일은 이후 바뀔 수 있으므로 현재 설치 개수로 단정하지 않는다.
- 일반 카탈로그와 이름이 정확히 `-Refit`로 끝나는 카탈로그를 분리한다.
- 파일명에 Clothes/Armor 등이 있다는 이유로 숨기지 않는다.
- `BodyPreset::PersistentId()`는 `source + '\x1F' + name`이다. XML 파일 경로나 내부 프리셋 이름 변경은 저장된 ID 연결에 영향을 줄 수 있다.
- 빈/파싱 실패 프리셋과 “일부 슬라이더 생략”을 구별한다. `ApplyNow`는 슬라이더가 하나도 없는 프리셋을 거부한다. 일부 생략된 프리셋은 아래 보정 규칙을 따른다.

### 6.2 프리셋 계열 최종 분류

`BodyFamilyRules::ClassifyPreset`:

1. XML `Preset/@set` 내부 명시 계열.
2. set에서 결정할 수 없으면 XML `Group` 이름.
3. 두 항목 모두 결정할 수 없으면 preset 이름 + source 경로 보조 정보.
4. 같은 성별의 복수 계열은 bit mask를 합쳐 모두 라벨로 표시.
5. 무증거 또는 남녀 계열 충돌은 Unclassified 안전 fallback.

대표 기대값:

| XML/텍스트 증거 | 결과 |
| --- | --- |
| `3BBB Body Amazing` | CBBE 3BA |
| `3BBB Body Amazing UBE Anus` | CBBE 3BA |
| CBBE + 별개의 UBE 명시 | CBBE 3BA / UBE |
| CBBE + BHUNP | CBBE 3BA / BHUNP / UNP |
| BHUNP 3BBB | BHUNP / UNP |
| Group = UBE 2.0 | 앞선 set 증거가 없으면 UBE |
| HIMBO / SAM Light | 각 남성 계열 |
| `3BBB` 단독 | 계열 확정하지 않음 |
| cube / samurai / Samutchi | UBE/SAM 단어 부분 일치로 오판정하지 않음 |
| Breasts/Waist/Butt 공통 슬라이더만 | 슬라이더 명칭으로 계열 확정하지 않음 |

고유 슬라이더 다수 점수화·임계값 분류는 **현재 요구사항이 아니다**. 슬라이더 이름은 모프 적용 집합에 쓰지만 계열 추론 근거와 혼동하지 않는다.

### 6.3 액터 계열

- 설치된 모드 이름 하나만으로 전 액터를 UBE 또는 3BA로 처리하지 않는다.
- SFS 참고 원칙대로 Skin/ARMA/실제 3D 메시·텍스처 레이아웃 등 액터 증거를 보수적으로 이용한다.
- 실제 UBE `!UBE` 텍스처 레이아웃은 별도 topology/UV 근거다.
- 표준 female/male 텍스처 경로는 UBE 후보를 제외하는 증거가 될 수 있다.
- CBBE와 UNP가 모두 설치된 상황에서 일반 경로만으로 둘 중 하나를 확정하지 않는다.
- 액터 감지 실패는 모든 항목 차단이 아니라 성별 전체 또는 미분류 안전 노출로 처리한다.
- 텍스처 이름만으로 표준 스킨의 CBBE/UNP UV 호환성을 완벽히 구분할 수는 없다. 부록/후속 점검의 보수적 fallback 한계를 읽는다.

### 6.4 적용 방식과 중첩 수정

기본 보간:

`목표 = lowWeight + (highWeight - lowWeight) × clamp(ActorBaseWeight / 100, 0, 1)`

메시는 BodySlide로 준비된 몸/의상과 TRI를 사용하고, RaceMenu BodyMorph 인터페이스를 통해 실제 메시를 변형한다. 바디 변경이 새로운 NIF 파일을 액터에 통째 갈아끼우는 기능은 아니다.

중첩 수정의 핵심:

- 새로고침 시 호환 성별·계열 프리셋들의 슬라이더 합집합을 미리 만든다.
- 적용 시 이 집합의 목표값을 먼저 0으로 채운다.
- 선택 XML에 있는 슬라이더만 체중 보간값으로 덮는다.
- BCNG 현재 적용 키를 비운 후 남아 있는 총 모프 값을 읽는다.
- BCNG 키에 `desired + preservedOutfit - currentAggregate`를 넣는다.
- 따라서 예전 RaceMenu 3BA MORPHS 값이 남아 있어도 적용 순간의 총합을 선택 프리셋 목표에 맞춘다. 외부 모드의 원래 키 데이터는 직접 삭제하지 않는다.
- 이것은 모든 모프가 항상 독립적으로 유지된다는 뜻은 아니다. **같은 슬라이더에 대한 외부 효과는 BCNG 적용 순간 상쇄될 수 있고**, 외부 모드가 이후 값을 변경하면 총합은 다시 변할 수 있다.
- 집합에 없는 슬라이더까지 모두 찾아 0으로 만드는 전역 clear가 아니다. 현재 카탈로그에 한 번도 나타나지 않는 슬라이더는 남을 수 있다.
- 몸/의상 NIF에 미리 구워진 체형은 별도 문제다. 이미 non-zero 체형으로 빌드된 메시 자체의 형상을 계산만으로 지우지는 못한다.

### 6.5 clear와 적용 시점

| 동작 | 모프 변화 |
| --- | --- |
| UI 열기 / 액터 선택 변경 / 새로고침 | 그 자체만으로 전체 모프 clear 하지 않음 |
| 바디 항목 1회 선택·미리보기 | preview 세대 확인 후 BCNG preview 키 교체, 현재 outfit 보정 유지 |
| 바디 확정 | BCNG preview/outfit/committed 관련 소유 키 정리 후 새 목표 보정 |
| NPC 조건 배포 | 확정과 같은 바디 적용 논리 사용, 자동 갱신 정책은 deferred |
| 미리보기 취소·액터/탭 전환 | 해당 미리보기 소유 액터에 대해서만 취소 |
| 의상 보정 | 별도 outfit 키만 갱신 |
| 기본 바디/모프 초기화 | BCNG 및 명시적으로 다루는 이전 키 제거, 다른 모드 전체 clear 아님 |

정확한 예외: `ApplyNow`는 preview/commit의 **둘 모두**에서 legacy `OBody`/`OClothe` 키가 있으면 제거한다. “미리보기에서는 무조건 BCNGPreview 하나 외에는 전혀 손대지 않는다”라는 예전 설명은 이 예외 때문에 정확하지 않다. 다음 작업자가 OBody 병용/preview 취소 복구를 점검할 때 주의한다.

유두·생식기 무작위화는 현재 여성 CBBE 기반 commit 경로에서 목표값에 추가된다. 모든 남성·UBE·BHUNP에 동일 무작위 슬라이더를 지원한다고 쓰면 안 된다.

## 7. 바디스킨·부분 텍스처·종족·UBE·SOS

### 7.1 접근 방식

구 BodyChange의 HeadPart/Skin Armor/메시 교체 방식과 달리, BCNG는 액터의 현재 geometry/FaceGen와 RaceMenu persistent texture override를 사용한다. 스킨팩에 대체 몸·얼굴 NIF를 반드시 동봉하는 구조가 아니다. 다만 **기존 액터 메시와 DDS의 UV/재질 호환**은 여전히 필요하다.

`SkinProfile`은 성별, BodyFamily mask, humanoid/argonian/khajiit 축, body/hands/feet/face, vampireFace, elder 변형, raceFace, faceDetails, CBBE/UNP 보조 아틀라스, SOS addon별 채널들을 분리해 보관한다.

스캐너는 `BodySkin/<pack>/profile.json` 명시 프로필도 지원하고, 없으면 표준 경로를 자동 감지한다. 일반 사용자에게 JSON 작성을 필수로 요구하지 않는다.

### 7.2 채널과 부분 적용

| 파일 채널 | 셰이더 texture index |
| --- | ---: |
| diffuse / 통상 무접미 기본 DDS, UBE `_d` | 0 |
| normal / `_msn`, UBE `_n` | 1 |
| subsurface / `_sk` | 2 |
| 호환 FaceGen detail | 3 |
| specular / `_s` | 7 |

- body normal만 있으면 body normal만 대체한다.
- hands normal만 있는 팩이 몸 또는 발 diffuse로 들어가면 안 된다.
- 팩에 손/발이 없다고 body DDS를 손/발에 복제하지 않는다.
- 누락 채널은 다른 채널의 텍스처로 채우지 않는다.
- 미리보기/확정/NPC 배포가 같은 부위·채널 규칙을 공유한다.
- 다른 모드가 소유한 NiOverride 값은 소유권 검사로 보호한다. BCNG cache 경로와 이전 BodyChangerNG cache 경로만 BCNG 소유로 인식하는 테스트가 있다.
- “있는 DDS는 무엇이든 적용”이 아니다. 분류 가능한 경로·부위·채널 및 실제 해당 geometry가 있어야 한다.

### 7.3 주요 배치 예

기본 루트는 항상 `BodySkin\<스킨팩명>\Textures\`이다.

| 대상 | 그 아래 상대 경로/파일 예 |
| --- | --- |
| 일반 여성 몸 | `actors\character\female\femalebody_1.dds`, `_msn`, `_sk`, `_s` |
| 일반 여성 손 | `actors\character\female\femalehands_1.dds` 및 동일 채널 접미사 |
| 일반 여성 발 | `actors\character\female\femalefeet_1.dds` 및 동일 채널 접미사 |
| 일반 여성 얼굴 | `actors\character\female\femalehead.dds` 및 해당 채널 |
| 일반 남성 | `actors\character\male\malebody_1*`, `malehands_1*`, `malefeet_1*`, `malehead*` |
| 노인 여성 | `actors\character\femaleold\...`의 해당 body/hands/head 파일 |
| 종족 여성 얼굴 normal | `actors\character\bretonfemale\femalehead_msn.dds` 등 해당 종족 폴더 |
| 수인 | 해당 `argonianfemale/argonianmale/khajiitfemale/khajiitmale` 경로의 표준 DDS |
| UBE 몸 | `!UBE\Body\femalebody_1_d.dds`, `femalebody_1_n.dds`, `femalebody_1_sk.dds` |
| UBE 얼굴 | `!UBE\Head\femalehead_d.dds`, `femalehead_n.dds`, `femalehead_sk.dds` |
| CBBE 3BA 보조 | 원래 피부 경로의 `femalebody_etc_v2_1.dds`, `_msn`, `_sk`, `_s` |
| UNP/BHUNP 보조 | `actors\character\BakaUNP\VaginalAnalCanal2*` |
| SOS | `actors\character\SOS\<Addon 폴더>\...malegenitals_*...dds` |

팩명은 임의 이름이지만 하위 원래 DDS 경로를 보존한다. 특히 `!UBE`를 억지로 `actors\character` 안으로 옮기지 않는다. 경로 안내의 실제 전체 예시는 `package/BodySkin/README.txt`가 함께 제공한다.

### 7.4 종족·노인·얼굴 우선순위

- humanoid, Argonian, Khajiit은 별도 호환 축이다. 피부 계열이 같은 CBBE여도 사람 피부를 카짓에 대신 배포하지 않는다.
- 수인 흡혈귀 이름 판정도 테스트한다.
- humanoid 하위 얼굴 변형: Nord/Breton/DarkElf/HighElf/Imperial/Orc/Redguard/WoodElf.
- 노인 판정은 race/voice 단서를 사용한다. 임의의 모든 노인 커스텀 NPC를 100% 판정하는 나이 API가 있는 것은 아니다.
- 실제 `EffectiveBodyLayers`: 팩 기본 body → 해당 노인 body의 **존재하는 채널만** 덮기.
- 실제 `EffectiveHandsLayers`: 팩 기본 hands → 해당 노인 hands의 존재 채널.
- 실제 `EffectiveFaceLayers`: 팩 기본 face → 해당 raceFace → vampireFace → elderFace → 가능한 detail.
- 전용 파일이 없으면 다른 종족 전용 파일을 빌리지 않는다. **같은 팩의 일반 채널이 있으면 그 일반 채널을 사용하고, 그것도 없을 때 액터 원래 채널을 유지**한다.
- 따라서 “femaleold가 없으면 아무 스킨도 적용하지 않는다”가 아니다. “전용 노인 추가 덮기가 생략된다”가 정확하다. 사용자의 ‘기본’이 게임 원본을 뜻하는지 같은 팩 기본을 뜻하는지 다시 논의할 때 이 구현 사실을 제시한다.

### 7.5 3BA와 UNP 보조 아틀라스

CBBE 3BA:

- `femalebody_etc_v2_1*`는 vagina와 anus에 공유되는 아틀라스.
- `3BA_Vagina / 3BBB_Vagina / 3BA_Anus / 3BBB_Anus` 노드 지원.
- “외음부만”, “항문만”으로 축소하면 안 된다.

UNP/BHUNP:

- `BakaUNP/VaginalAnalCanal2*`.
- `BaseShapeVagina / BaseShapeAnus / BaseShapeCanal`.

실제 재질에 알려진 아틀라스 경로가 남아 있으면 이를 우선한다. BCNG cache alias로 경로가 변한 뒤에는 제한적인 노드 이름 fallback을 사용한다. 다른 계열의 알려진 재질 경로가 발견되면 노드 이름만으로 잘못 매칭하지 않는다. 일반 body와 특수 아틀라스는 분리한다.

정적 NIF/경로·스캐너·라우팅 테스트는 존재하지만 모든 의상 리빌드/NIF 변형의 인게임 UV 정상 여부는 아직 확인하지 못했다.

### 7.6 UBE

- 사용자 실환경 참고: TAKEALOOK의 플레이어 UBE, NPC CBBE 3BA.
- 액터별로 감지하며 “통팩에 UBE가 설치돼 있으니 NPC도 모두 UBE”라고 판단하지 않는다.
- UBE는 `!UBE/Body`와 `!UBE/Head` 아틀라스, 실제 slot-53 바디 geometry를 대상으로 한다.
- 표준 손·발 텍스처 fallback으로 UBE 아틀라스를 쪼개 넣지 않는다.
- UBE 별도 wet/PBR/RFAOS 등은 현재 재질이 의미를 정의한다. 존재하는 모든 DDS에 임의 shader slot을 붙여 배포하지 않는다.
- 스킨 카탈로그에서 알려진 비호환 일반 피부는 UBE 액터에 숨긴다. 근거가 불확실하면 기존 안전 fallback을 유지한다.
- UBE 프리셋 XML 경로는 일반 BodySlide와 동일하다.

### 7.7 남성·SOS

조사 원본:

`D:\TuLED13E\File Mod Skyrim SE\mods\Schlongs of Skyrim SE`

- `Schlongs of Skyrim.esp`
- `SOS - Smurf Average Addon.esp`
- `SOS - VectorPlexus Regular Addon.esp`
- `SOS - VectorPlexus Muscular Addon.esp`
- 위 모드의 `textures\actors\character\SOS` 및 실제 addon 모델 경로.

현재 대응:

- 남성 표준 body/hands/feet/head 경로·성별 판정을 보강.
- slot에 바로 매칭되지 않는 multi-slot Skin Armor에 한해 제한된 보조 탐색으로 실제 Skin ARMA geometry를 찾음. 모든 착용 geometry에 무차별 적용하지 않음.
- SOS는 **실제 장착된 slot-52 addon의 모델 디렉터리**와 팩의 addon 변형을 맞춘다.
- Smurf Average / VectorPlexus Regular / VectorPlexus Muscular를 서로 구분.
- Muscular의 normal 전용 파일 + 원본에서 공유하는 Regular diffuse/subsurface/specular 구조 반영.
- humanoid/Argonian/Khajiit/elder 변형과 채널 누락을 분리.
- 세 종류 addon이 한 팩에 들어 있어도 현재 addon 이외의 성기 텍스처가 덮이지 않게 한다.
- 테스트 데이터 매칭은 확인했지만 실제 SOS 전체 조합의 인게임 정상은 미검증이다.

### 7.8 의도적으로 제외한 파일

- `astridbody* / astridhands* / astridhead*`: Astrid 특수 상태용.
- `*afflicted.dds`: Afflicted 전용.
- `character assets\tintmasks\*.dds`: BodySkin/NPC 피부 배포에서 제외. 별도 TintMask에 배치해야 함.
- 의미가 확인되지 않은 PBR/재질 확장 DDS: 무작정 일반 채널에 할당하지 않음.

### 7.9 새로고침과 런타임 에셋

`CatalogRoots`는 가상 Data에 보이는 파일/디렉터리의 실제 공급 경로를 Windows handle로 찾고, 발견한 폴더와 가상 루트를 스캔한다. 최초 탐색은 4,096 entry로 제한된다. 논리 가상 루트를 마지막에 처리하여 MO2의 현재 승자 파일을 우선한다.

`RuntimeAssetCache`는 원본 경로 해시 이름으로 textures 하위 별칭을 만든다.

- 같은 파일 여부: 크기 + 수정시각 비교.
- 가능한 경우 hardlink, 안 되면 copy.
- 실제로 선택·적용되는 에셋 materialize 작업은 I/O를 할 수 있다. “모든 파일 작업이 오직 새로고침 때만 발생한다”는 설명은 틀리다.
- 새 팩/파일을 기존 활성 제공 폴더에 추가하고 새로고침하는 기능을 지원한다.
- 게임 실행 후 **새 MO2 모드를 활성화**하는 것까지 USVFS에 즉시 반영된다는 보장은 없다.
- 엔진이 이미 메모리에 캐시한 동일 경로 DDS를 디스크에서 바꿨을 때 모든 경우 자동 재로드된다는 보장도 없다.
- 캐시 hardlink를 직접 편집하면 원본과 같은 파일 실체일 수 있다. 캐시는 사용자 편집 대상으로 안내하지 않는다.

## 8. 플레이어 틴트마스크

### 8.1 지원 범위·경로

`TintMask\<팩 이름>\textures\actors\character\character assets\tintmasks\*.dds`

- 현재 플레이어 RaceMenu 틴트 레이어를 대상으로 한다.
- NPC 선택 시 틴트 탭은 숨긴다. 바디/스킨은 그대로 사용 가능.
- 액터 종족에 맞는 DDS를 선택하며, 다른 종족 파일을 임의 fallback으로 쓰지 않는다.
- 일반/UBE/COtR 등의 카탈로그 호환성 분리를 지원하되 불확실한 경우 안전 노출.
- 색상·불투명도를 실시간 편집한다. 피부 전체 색조 또는 목 경계선 자동 보정 기능은 아니다.
- 첫 BCNG 변경 직전의 원본 텍스처와 RGBA를 백업하여 기본 틴트/레이어별 복원이 가능하다.

### 8.2 회귀 수정

- 월드에서는 복원됐는데 하단 컬러 스와치가 이전 빨강으로 남던 문제: UI 편집 상태도 복원 결과에 맞춰 동기화.
- 틴트 탭과 상세 조절 팝업은 동일 카메라 확대를 사용.
- 선택 팩과 실제 활성 레이어에 usable DDS가 있는 항목만 상세 레이어 목록에 제공.
- RaceMenu 프리셋 변경 후 틴트 배열/FaceGen이 재생성되므로 재적용 시 **팩 → 개별 색/알파 편집 → 원본 복원 지정** 순서를 유지한다.
- 기본 틴트 상태는 RaceMenu가 새로 정한 값을 존중하도록 해야 하며, 오래된 BCNG 오버라이드를 무조건 다시 넣으면 안 된다.

### 8.3 RaceMenu 종료 복구

`ActorEvents`는 RaceSexMenu 열기/닫기에 generation을 사용한다.

- 메뉴를 다시 열면 이전 종료 후 작업을 무효화.
- 종료 후 초기 3 hop 대기, 3D 미준비/메뉴 재열림 시 제한된 재시도.
- 현재 코드 호출 인자는 `3U, 8U, 2U`. 초기 복구 뒤 추가 verification pass 두 번이 예약될 수 있다.
- 현재 확정 body → outfit 처리 요청 → skin → tint를 복원하는 흐름.
- 이것은 제안된 OverlayFix 전용 “3D 완전 안정화 완료 신호”를 구현한 state machine이 아니다.
- 체형/스킨/틴트 유지가 중요한 필수 회귀 시나리오지만, 모든 .jslot·RaceMenu·외부 모드 조합에서 성공했다고 단정하지 않는다.

## 9. NPC 배포 조건·JSON·저장 데이터

### 9.1 편집과 평가

- 인게임에서 규칙 추가/삭제, 우선순위 위/아래 이동, 성별·대상·바디·스킨 풀을 편집한다.
- 규칙 하나에 바디와 스킨 채널을 각각 배포/제외/무변경으로 둘 수 있다.
- **위에서 처음 일치한 규칙 하나가 두 채널을 모두 담당한다.**
- 예: 1번이 “노인 여성 바디 제외, 스킨 빈 목록”이면 스킨은 그대로다. 2번 전체 여성 스킨 규칙까지 내려가 스킨을 받는 구조가 아니다.
- 바디 또는 스킨 풀 1개는 고정 선택, 여러 개는 액터·규칙·채널 기반의 안정적인 선택.
- 이전 선택이 현재 호환 풀에 남아 있으면 유지하는 로직도 있다.
- 안정적인 무작위는 “환경/규칙/풀/ID가 어떻게 바뀌어도 영원히 같은 결과”라는 뜻은 아니다.
- 사용자 수동 직접 지정은 해당 채널의 자동 배포보다 우선한다.
- 수동 잠금 옵션을 UI에서 없앤 것과 수동 선택을 보호하는 데이터 구조를 없앤 것은 다르다.

### 9.2 대상 종류와 저장 enum

| enum 값 | 대상 | 실제 매칭 기준 |
| ---: | --- | --- |
| 0 | 전체 NPC | 적격 NPC 및 지정 성별 |
| 1 | 특정 NPC | Base FormID, 저장은 plugin + local ID |
| 2 | 같은 이름 NPC | ActorBase 이름, 대소문자 무시 정확 일치 |
| 3 | 팩션 | target Form을 찾고 `base->IsInFaction` |
| 4 | 플러그인 | NPC base의 `GetFile(0)` 원본 플러그인명 |
| 5 | 종족 | base race의 실제 FormID |
| 6 | 커스텀 팔로워 | 아래 정의 |
| 7 | 노인 NPC | race/voice 기반 노인 판정 |
| 8 | 키워드 | base의 `HasKeyword` |
| 9 | 클래스 | base의 `IsInClass` |
| 10 | 전투 스타일 | base의 `GetCombatStyle` |

enum 값은 JSON 호환성 때문에 고정한다. 대상 드롭다운의 넓은 범위→좁은 범위 표시 순서를 바꿀 때 enum을 재번호화하지 않는다.

현재 팩션 매칭은 base 기준이며, 런타임 reference에만 추가된 faction 변화까지 모두 동일하게 처리하는지 추가 점검이 필요하다. “플러그인 조건”도 최종 리텍 승자 플러그인이 아니라 base 원본 파일이라는 점을 설명한다.

### 9.3 커스텀 팔로워 정확한 구현

`IsCustomFollower`:

1. NPC base의 원본 파일이 Skyrim.esm / Update.esm / Dawnguard.esm / HearthFires.esm / Dragonborn.esm이면 제외.
2. 그 외 플러그인에서 온 NPC 중 플레이어 teammate이거나 PotentialFollowerFaction `0005C84D` / CurrentFollowerFaction `0005C84E`에 속하면 해당.

이 때문에 일반 바닐라 리디아 리텍을 커스텀 팔로워로 보지 않는다. 반대로 별도 follower framework가 teammate/해당 faction을 전혀 사용하지 않는 커스텀 팔로워는 놓칠 수 있다. 임의 모드 NPC라는 이유만으로 모두 팔로워로 간주하지 않는다.

### 9.4 기본 샘플 8개

| 순서 | 조건 | 바디 | 스킨 |
| ---: | --- | --- | --- |
| 1 | 커스텀 팔로워 여성 | 배포 제외 | 빈 풀·변경하지 않음 |
| 2 | 커스텀 팔로워 남성 | 배포 제외 | 빈 풀·변경하지 않음 |
| 3 | 노인 여성 | 배포 제외 | 빈 풀·변경하지 않음 |
| 4 | 노인 남성 | 배포 제외 | 빈 풀·변경하지 않음 |
| 5 | ArgonianRace 여성 | 빈 풀·변경하지 않음 | 배포 제외 |
| 6 | ArgonianRace 남성 | 빈 풀·변경하지 않음 | 배포 제외 |
| 7 | KhajiitRace 여성 | 빈 풀·변경하지 않음 | 배포 제외 |
| 8 | KhajiitRace 남성 | 빈 풀·변경하지 않음 | 배포 제외 |

기본 JSON은 schema 4. ArgonianRace = Skyrim.esm local `0x013740`, KhajiitRace = `0x013745`. 각각 10진수 79680/79685로 저장되어 있다. 수인 피부 기능을 구현했어도 사용자의 지시대로 **기본 제외 규칙은 유지**했다. 수인 피부를 자동 배포하려면 사용자가 해당 규칙을 수정/끄거나 상위 규칙을 적절히 구성해야 한다.

### 9.5 팩션 빈 드롭다운과 안정 ID

- 이전 EditorID 의존 목록은 게임 런타임에 EditorID가 없으면 항목이 비는 문제가 있었다.
- 현재 `UI::RefreshDistributionTargetOptions` 계열은 TESDataHandler의 typed FormArray에서 faction/race/keyword/class/combat style을 수집한다.
- 이름·EditorID가 없어도 plugin/local ID 등으로 선택 가능한 라벨을 구성한다.
- 저장은 `targetPlugin + targetLocalFormID`, 실행 시 `targetFormID`를 해석한다.
- 이전 schema-3의 EditorID target은 읽을 때 한 번 실제 Form으로 해석하고 안정 ID로 정규화.
- unresolved/타입 불일치는 다른 Form에 억지로 적용하지 않는다.
- 이 구현을 “실제 설치된 모든 팩션에서 게임 내 드롭다운 확인 완료”로 소개하지 않는다. typed array 및 런타임별 구조 검증 범위는 16장 참조.

### 9.6 저장 분리

**전역 규칙 JSON**과 **세이브별 액터 결과**는 다른 데이터다.

`BodyChangeNGdistribution.json`:

- schemaVersion 4, 이전 3 읽기.
- rules; 각 id/name/enabled/female/scope.
- npcPlugin/npcLocalFormID.
- targetPlugin/targetLocalFormID 및 표시·구버전 호환 target.
- bodyFamily(편집기 풀 필터), presetIds, skinProfileIds.
- excluded(이전 통합 제외 호환), bodyExcluded, skinExcluded.
- source = bodychangeng / obody.
- 로드 시 규칙은 최대 256개까지만 수용하는 방어가 있다.

**SKSE 코세이브**:

- 식별자 `0x42434E47` (BCNG).
- 액터 레코드 `0x41535452` (ASTR), 틴트 `0x54494E54` (TINT).
- 레코드 버전 1.
- 액터별 reference FormID, base plugin/local 검증, 수동 body/skin 선택, default 여부, 적용 결과/서명.
- RefID는 `ResolveFormID`로 재해석, 해석 실패 엔트리는 건너뛴다.
- 문자열 테이블을 공유해 중복 ID 저장을 줄인다.
- 방어 한계: actors 16,384, strings 131,072, string length 1,024, tint layers 32.
- 레코드 버전/인덱스가 잘못되면 무조건 신뢰하지 않는다.
- 세이브 새로 로드/revert 시 이전 런타임 상태를 정리한다.

RefID는 액터 **인스턴스**, BaseID는 NPC **정의**다. 둘 다 FormID의 종류이고 load-order prefix를 포함한 런타임 값 자체가 영구 불변은 아니다. 코세이브 결과에 RefID를 쓰는 이유는 같은 Base NPC의 여러 인스턴스를 구별하기 위해서다. 규칙의 plugin/local BaseID와 용도가 다르다.

### 9.7 버튼 의미

- **저장 값 불러오기**: 자체 규칙 JSON을 읽고, OBody 파일이 있으면 지원 배포 규칙도 읽는다. 별도 선택창 없음. 현재 in-memory 규칙 집합을 바꿀 수 있다.
- **로드된 NPC 즉시 배포**: 편집 규칙 저장 성공 후 활성화하고 로드된 적격 NPC를 큐에 등록한다.
- **다음 게임 실행 시 배포**: 편집 규칙을 디스크에만 저장하고 현재 세션 활성 규칙은 유지한다.
- 현재 UI 알림의 NPC 숫자는 요청/큐 등록 수와 연결된다. 모든 사람이 실제로 바뀐 최종 완료 수라고 해석하면 안 된다.
- 기본 규칙 JSON을 백업해 같은 경로로 옮기고 저장 값 불러오기를 누르면 조건을 복원할 수 있다. 팩/프리셋도 같은 ID·경로로 있어야 선택 연결이 유지된다.
- 코세이브의 수동 액터 결과까지 JSON 백업 하나로 옮겨지는 것은 아니다.

## 10. 의상 보정·무작위화·OBody JSON

### 10.1 UI

“의상·랜덤화” 팝업:

- 의상 착용 시 가슴 보정.
- 그 아래 종속된 의상 착용 시 유두 보정.
- OBody NG 의상 보정 규칙 등록.
- 등록 성공 시 옆 안내문구를 등록됨으로 바꾸고 다른 색상 표시.
- 유두 형태 무작위화.
- 생식기 형태 무작위화.
- 불필요한 하단 적용/닫기 버튼과 장문의 안내 두 줄, “ORefit도 같은 JSON에서 읽는다” 중복 줄 삭제.
- 의상 JSON 등록 알림을 본체 바디/스킨 UI 공통 알림에 띄우지 않도록 수정. 등록 팝업 안 상태 표시 사용.

### 10.2 지원 OBody 의상 키 7종

| 원본 JSON key | 의미 |
| --- | --- |
| `blacklistedOutfitsFromORefit` | 의상명 제외 |
| `blacklistedOutfitsFromORefitFormID` | plugin/local FormID 제외 |
| `blacklistedOutfitsFromORefitPlugin` | 플러그인 제외 |
| `outfitsForceRefit` | 의상명 강제 보정 |
| `outfitsForceRefitFormID` | FormID 강제 보정 |
| `refitOutfitPresetsFemale` | 여성 의상명→전용 프리셋 |
| `refitOutfitPresetsMale` | 남성 의상명→전용 프리셋 |

파서는 입력을 제한하고 FormID는 게임 스레드에서 활성 플러그인에 맞춰 해석한다. JSON을 읽기만 하고 원본을 수정하지 않는다.

사용자가 “남성이 왜 필요한가”라고 질문했지만, 원본 호환 데이터는 남성/여성 의상별 프리셋을 별도로 가질 수 있으므로 두 매핑은 유지했다. 이것이 남성 생식기 랜덤화 구현을 의미하지 않는다.

### 10.3 런타임 탐색 순서

1. 보정 옵션 OFF이면 BCNG outfit 보정 제거.
2. 몸/가슴 관련 착용 슬롯에서 제외되지 않은 의상 여부 확인.
3. 강제 보정은 위 슬롯 외의 장착 아이템도 확인. 반지 등의 force entry도 고려.
4. 적격 의상 또는 강제 항목이 없으면 outfit 보정 제거.
5. 현재 착용 의상명에 매핑된 전용 프리셋.
6. 현재 바디 이름 + `-Refit`.
7. `Female-Refit / Male-Refit`.
8. 없으면 절차적 기본 보정.
9. outfit signature가 같고 적용된 상태면 중복 작업 생략.

유두 보정 OFF는 명명된 -Refit 프리셋에서도 nipple/areola 계열 슬라이더를 건너뛰도록 반영한다. 기본 절차적 보정과 무작위화는 지원 슬라이더가 있는 메시에서만 외형 변화가 보인다.

### 10.4 등록 데이터의 수명 — 중요한 미완료 경계

현재 `OutfitRefit::LoadOBodyRules`는 읽은 7종 규칙을 **메모리의 rules_에만** 넣는다. 자체 배포 JSON 또는 코세이브에 의상 import 전체를 저장하는 경로는 없다. `kDataLoaded`에서 `ClearLegacyRules`도 호출한다.

따라서 현재 코드만으로는 **한 번 등록한 OBody 의상 규칙이 게임 완전 재실행 후에도 자동 복원된다**고 말할 수 없다. 현재 세션 명시 등록과 다음 실행 자동 등록은 별개다. 사용자 기대와 다를 수 있는 후속 점검 항목이며, 이번 문서 작성 중 기능을 임의로 추가하지 않았다.

### 10.5 OBody 배포 JSON

지원 import:

- blacklistedNpcs / blacklistedNpcsFormID.
- npcFormID / npc.
- blacklistedNpcsPluginFemale/Male.
- blacklistedRacesFemale/Male.
- factionFemale/Male.
- npcPluginFemale/Male.
- raceFemale/Male.
- blacklist를 반영한 여성/남성 기본 풀.

재가져오기 시 이전 imported rule만 제거하고 사용자가 만든 자체 규칙은 유지한다. 기존 자체 규칙 뒤에 import를 붙이는 구조이므로 자체 상위 규칙이 먼저 매칭하면 OBody 규칙이 가려질 수 있다.

OBody의 모든 UI 옵션/API/기능을 복제한 것은 아니다. 특히 “자동 배포 제외 프리셋을 메뉴에서도 숨길지” 같은 OBody 전용 옵션을 그대로 추가할 필요는 없다고 사용자와 정리했다. BCNG는 사용자가 배포 풀을 직접 선택한다.

초기 OBody 비교에서 논의한 외부 Papyrus/C++ API·변경 이벤트는 별도 항목이다. 현재 확인한 소스에는 다른 모드가 호출하는 BCNG 전용 Papyrus 함수 등록 또는 공개 변경 ModEvent 계약이 없다. RaceMenu의 Papyrus NiOverride를 내부에서 호출하는 것을 BCNG가 외부 API를 제공하는 것으로 오해하지 않는다. 향후 필요하다면 이벤트/ABI/버전/소유권을 별도로 설계해야 한다.

OBody JSON Master List 호환 페이지: https://www.nexusmods.com/skyrimspecialedition/mods/105052

이 URL은 호환 출처이지 해당 모드나 JSON을 BCNG ZIP에 재배포한다는 뜻이 아니다.

## 11. 성능·작업 큐·중복 방지

### 11.1 피드백의 실제 조건

다른 사용자의 피드백: 규칙 약 25개, 바디 프리셋 약 55개, 바디만 배포, Skin/Tint 폴더 비어 있음. OBody 환경에 비해 BCNG에서 스터터가 느껴짐.

이는 이 사용자 본인 통팩의 측정값이 아니다. 피부/틴트를 비웠으므로 그 기능을 먼저 범인으로 확정하면 안 됐다.

### 11.2 적용한 조치

- BodyMorph partition 갱신을 자동 배포/의상에서는 deferred로 사용.
- 수동 미리보기/확정은 빠른 반응과 오래된 선택의 역전 방지를 위해 synchronous 정책 유지.
- 자동 이벤트 normal/performance 모두 큐 경로.
- 같은 액터 요청 병합.
- body/skin/outfit 별 signature로 변하지 않은 결과의 중복 재적용 억제.
- 카탈로그와 대상 Form 목록은 이벤트마다 재스캔하지 않음.
- 파일 로그를 INFO마다 flush하지 않고 warn 기준 flush.
- 초기 로드 콜백 폭주 직후 2 hop 지연.
- 새로 나타난 액터는 bulkLoad보다 앞에 배치.
- 3D 없는 액터는 busy loop 대신 준비 이벤트를 기다림.
- detach/session change에서 stale pending 제거.

### 11.3 성능 모드 ON/OFF

| 항목 | OFF | ON |
| --- | --- | --- |
| 자동 이벤트 콜백에서 즉시 전체 적용 | 하지 않음 | 하지 않음 |
| ActorHandle 큐 | 사용 | 사용 |
| 한 drain에서 처리 | 액터 최대 1개 | 액터 최대 1개 |
| 추가 scheduling hop | 0 | 1 |
| 최초 로드 지연 | 2 hop | 2 hop |
| 최종 선택 결과 | 동일 규칙·동일 후보라면 동일 | 동일 |
| UI 직접 조작 | 자동 대량 작업과 별도 반응 정책 | 동일 |

“다음 태스크”는 반드시 다음 화면 프레임과 일대일 대응하지 않는다. 멀티 NPC의 전체 적용 완료 시간, 화면에 새 NPC가 나타나기 전 적용 완료 여부, 프레임당 최악 지연을 이 코드 구조만으로 보증할 수 없다.

### 11.4 계측

`ActorWorkQueue` 메트릭:

- requests, coalesced, waitingFor3D.
- processed, changed, unchanged.
- processingMicros, maxPending, currentPending.

`RaceMenuBodyMorph`는 자체 setup이 한 60FPS frame(약 16.7ms)을 넘으면 warning을 남긴다. deferred 호출 뒤 RaceMenu/OverlayFix/FSMP/CBPC 내부 전체 비용이나 GPU 부하는 이 값에 모두 포함되지 않을 수 있다.

후속 성능 점검은 사용자가 실제 만나는 셀 규모, 예를 들어 10~30명 및 제보의 25규칙/55프리셋부터 비교한다. 200~500 NPC를 인게임 필수 검증으로 강요하는 계획은 사용자가 원치 않았다.

### 11.5 남는 비용과 한계

- 규칙 평가와 compatible 후보 추림, Registry lookup은 여전히 수행된다.
- OutfitRefit는 스냅샷 복사/프리셋 후보 탐색, 강제 항목을 위한 장착 인벤토리 확인을 한다.
- 에셋 최초 별칭 생성은 파일 I/O를 수반한다.
- 해시 기반 변경 판단은 프리셋 ID·옵션 중심이다. **같은 ID XML 내용만 수정**한 경우 자동 적용 invalidation이 충분한지 별도 확인해야 한다.
- 현재 UI 바디 새로고침 버튼은 PresetCatalog::Refresh를 호출한다. 자동 배포의 NeedsBodyApply는 ID/default/무작위 옵션 signature를 비교하므로, 파일 내용 변경을 자동 배포에 반영하는 동작은 UI 카탈로그에 새 값이 보이는 것과 별도로 검증해야 한다.
- 모프 목표 집합은 카탈로그 기반 캐시라 대용량 프리셋에서 집합 크기에 비례하는 작업이 존재한다.
- .jslot migration은 DataLoaded의 한 번의 스캔이다. 수천 개의 대형 jslot이면 로딩 비용 자체는 증가할 수 있다.
- 장시간 메모리/GPU 리소스/캐시 디스크 증가를 실측하지 않았다. 정적 안전 검사만으로 누수 없음이라고 말하지 않는다.

## 12. 카메라·회전·일시정지·UI

### 12.1 최종 카메라 상수

| 값 | 바디프리셋/바디스킨 | 틴트 탭 및 상세 팝업 |
| --- | ---: | ---: |
| world FOV | 70 | 70 |
| 거리 | 200 | 80 |
| 왼쪽 캐릭터 horizontal | +70 | +28 |
| 오른쪽 캐릭터 horizontal | -70 | -28 |
| vertical | -45 | +10 |
| 플레이어 pitch | 0.1 | 0.1 |
| pitch zoom offset | 0.1 | 0.1 |

틴트 horizontal은 `70 × 80/200`이다. FOV를 바꾸지 않고 거리와 대칭 위치로 확대한다. 상수 이름은 `MenuCharacterPresentation.cpp` 첫 부분에 모여 있다.

코드의 현재 설명상 camera PosZ가 작아지면 캐릭터가 화면 위쪽으로, 커지면 아래쪽으로 이동한다. 월드 카메라 높이와 화면 속 캐릭터 이동 방향을 반대로 설명하지 않는다.

기타 상수:

- 좌우 facing correction: +0.35 / -0.35.
- 우클릭 rotation: 0.003 rad/pixel, 한 프레임 최대 0.060 rad.

### 12.2 캡처·복원

- 메뉴 시작 때 원래 camera target/zoom/current zoom/각도/offset/FOV/프러스텀 등 상태 캡처.
- 엔진이 목표 zoom을 계산한 뒤 현재 zoom을 그 목표로 즉시 맞춤.
- 메뉴 종료 때 **원래 현재 zoom과 목표 zoom 둘 다** 복원.
- 카메라 world FOV와 DrawWorld 쪽 FOV/투영 상태를 함께 관리.
- additive FOV 때문에 구도가 달라지는 경로를 정리.
- 액터 변경 시 이전 액터 표시 상태 복구 후 새 액터 적용.
- unload/메뉴 종료/다른 camera state 이탈에도 복원 경계를 유지.
- 회전 refresh가 예약된 zoom restore/snap 요청을 덮지 않도록 우선순위 유지.
- ImGui 렌더 중 엔진 카메라 갱신을 수행하면 UI 전체 색 변화 등의 문제가 생겼다. 갱신은 render pass 밖 SKSE task로 수행.

### 12.3 일시정지와 회전의 역사

SFS 참고 자료에서는 kShow 시점 pause count 미반영 → 첫 일반 UI frame에서 count 1개 확보 → settle → 복원하는 해결책이 제시됐다. 하지만 BCNG에는 이후 다른 실험·수정이 진행되었고, `e3e08aa`에서 **camera pause settle 실험을 제거**했다.

현재 BCNG에 해당 SFS 프로토콜이 그대로 들어 있다고 설명하지 않는다.

- live pause flag/count를 임의로 줄였다 늘리는 실험을 되살리지 않는다.
- 수동 `UpdateThirdPerson()` 호출은 하지 않는다.
- 일시정지 중 우클릭은 actor root 물리 상태를 흔들지 않는 카메라 측 회전 경로를 사용한다.
- FSMP가 정지한 상태에서 캐릭터 root만 돌려 메시가 늘어지는 현상을 피하려는 설계다.
- pause ON/OFF 동일 구도, 플레이어→NPC A→NPC B→플레이어, 오른쪽→왼쪽→틴트→복귀가 핵심 인게임 회귀 시나리오다.

### 12.4 UI 회귀와 수정

| 제보/요청 | 처리 |
| --- | --- |
| 모든 드롭다운 선택값 공란 | 공통 combo preview 렌더 복구 |
| 백스페이스로 삭제 불가 | text editing key 입력 경로 복구 |
| 캐릭터 좌우 선택 시 전체 UI 색 변화 | camera update를 ImGui render pass 밖으로 분리 |
| 설정창 하단 안 보임 | 기본 크기·배치 조정 |
| 배포 좌/우 경계 조절 | resize 가능한 2열 표, 오른쪽 stretch |
| 왼쪽 규칙 목록 너무 좁음 | 기본 폭을 전체 36%, scale된 280~380 범위로 확대 |
| 배포 footer 창 늘려도 안 보임 | footer 높이 사전 예약, NoHostExtendY, child 높이 계산 수정 |
| footer 두 줄 | 7개 번역 버튼 글자 폭을 계산해 최소 팝업 폭 결정 |
| tint restore 후 색 스와치 잔상 | UI 상태와 world restore 동기화 |
| OBody 등록 알림이 본체에 뜸 | 의상 팝업 내부 상태로 제한 |
| 드롭다운 길이 | 공통 높이 resize 적용, 메인 액터와 조건/설정 목록 포함 |

배포 footer 7개: 규칙 추가 / 삭제 / 위 우선순위 / 아래 우선순위 / 저장 값 불러오기 / 로드된 NPC 즉시 배포 / 다음 게임 실행 시 배포.

배포창 기본 기준 920×520, viewport 최대 약 96%×94%; footer가 넓으면 폭을 늘리지만 viewport 제한은 남는다. 작은 해상도+매우 큰 글자+긴 번역에서는 한 줄 전체가 들어가는지 추가 확인이 필요하다. “모든 화면 크기에서 절대 안 잘림”으로 과장하지 않는다.

### 12.5 설정·조작

새 설치 defaults:

- F7.
- 언어 Windows 자동; 한국어/영어/중국어 간체 지원.
- 캐릭터 왼쪽.
- 게임 일시정지 OFF.
- 성능 모드 ON.
- 가슴 보정 ON / 유두 보정 ON.
- 유두·생식기 무작위화 OFF.
- 저장 위치 없으면 화면 중앙을 시작점으로 오른쪽 UI 배치, 이후 저장 위치 존중.
- UI scale은 해상도와 텍스트 크기에 연동. `textScale=1.0` 구조체 초기값과 해상도별 초기 자동 조정은 별개.

GUI는 native Dear ImGui이며 MCM이 아니다. 키보드·마우스와 ImGui gamepad navigation 사용. 방향키/WASD/D-pad 및 확정/취소 매핑 코드가 있으나 “패드만으로 모든 단축키 설정까지 인게임 검증 완료”라고 쓰지 않는다.

즐겨찾기는 body/skin/tint 각각 독립 저장한다.

## 13. 이름 변경·경로 이전·구 BodyChange 얼굴 프리셋

### 13.1 제품명 이전

- 공개명·UI·소스 namespace 디렉터리·DLL·JSON/설정 경로·문서·패키지·아트워크·GitHub를 Body Change NG로 변경.
- 현재 native 이름은 `BodyChangeNG`, 과거는 `BodyChangerNG`.
- 현재 이름 파일이 우선이며 없으면 이전 이름 설정/배포 경로를 읽는 compatibility 존재.
- 이전 BodyChangerNG의 모프/cache 소유 키도 복원·정리에 필요한 범위에서 인식.
- 새 파일과 구 파일이 공존할 때 새 파일이 우선이다.

초기에 사용자가 “배포도 세이브도 없으므로 distribution.json의 구경로 migration은 불필요”라고 했던 것과, 나중에 “이름을 스키마 migration까지 해서 전부 바꿔라”는 요구는 시점이 다르다. 제품명 변경 후 들어간 compatibility를 초기 발언만으로 임의 삭제하지 않는다.

### 13.2 구 BodyChange.esp 얼굴 문제

문제 맥락:

- 예전 구 BodyChange를 사용해 저장된 RaceMenu 프리셋에 `HPbodychangeface` HeadPart가 남음.
- 구 모드를 제거하면 HeadPart 소유 플러그인이 없어져 경로/참조를 못 찾음.
- 이를 BodySlide 바디 XML 문제와 혼동하면 안 된다. 대상은 RaceMenu **.jslot**이다.

현재 migration:

1. `BodyChange.esp`가 로드되어 있으면 변경하지 않음.
2. `High Poly Head.esm` local `000A06`가 있으면 그 Face HeadPart로 교체.
3. 없으면 `Skyrim.esm` local `051623` (바닐라 Female Nord Head).
4. 안전한 대체 HeadPart도 없으면 변경하지 않음.
5. UBE dependency가 있는 jslot은 보존; 일반/HPH 얼굴로 강제 바꾸지 않음.
6. 해당 구 얼굴 참조만 바꾸고 unrelated HeadParts와 필요한 modNames는 유지.
7. 옆에 `.body-change-ng.bak`, 이미 있으면 숫자 suffix 백업 생성.
8. 임시 JSON 저장 → 다시 파싱 검증 → MoveFileExW replace/write-through.
9. 재실행해도 같은 결과가 또 중복 변환되지 않는 idempotence 테스트.

특정 HPH HeadPart의 UI EditorID `00KLH_FemaleHeadNord`는 설명에 사용되었지만, 현재 구현은 위 plugin/local ID를 해석한다. 모든 커스텀 헤드 또는 모든 얼굴 모양을 자동 호환시키는 일반 migration 기능은 아니다.

### 13.3 RaceMenu 머리 크기 질문

대화에서 특정 .jslot의 head/전체 scale 차이도 조사했었다. 이는 프리셋 자체가 머리 크기에 영향을 줄 수 있다는 설명이지 “BCNG의 어떤 상호작용도 절대 원인이 아니다”라는 최종 인게임 증명은 아니다. 이후 접수된 3BA MORPHS 중첩 수정과 구 HeadPart migration, 머리 크기/얼굴 sculpt 문제를 각각 구분한다.

## 14. 사용자 피드백별 조치와 남은 검증

| 피드백 | 코드에서 확인되는 대응 | 상태/주의 |
| --- | --- | --- |
| NPC 손은 기본 스킨 | 남성 표준 경로, exact body-part/ARMA, multi-slot 제한 탐색, 부분 hands 채널 | 코드/fixture 보강. 모든 NPC 리텍 NIF 실게임 확인 아님 |
| 여성 외음부·항문 텍스처 오류 | 3BA shared atlas와 UNP canal atlas 분리, actual material 우선, 채널 테스트 | 인게임 불가였음. UV 정상 확정 보류 |
| RaceMenu 3BA값과 체형 중첩 | 호환 slider universe 목표0 + XML값, BCNG 보정 delta | 계산/fixture 확인. 외부 모프· baked body 한계 존재 |
| 반복 NPC 배포로 누적 | 현재 키 정리 후 목표 재계산, signature/세대 | 실제 반복 배포 스트레스는 별도 |
| UBE/3BA 동시 환경 | per-actor 판정, 혼합 rule pool runtime 호환 추림 | unknown fallback 보존, 실제 모든 custom race는 미확인 |
| 팩션 목록 공란 | typed form enumeration, plugin/local ID 라벨·저장 | runtime-only faction membership 및 게임별 array layout 검증 필요 |
| Keyword/class/combat style 조건 | scope 8/9/10, 목록·저장·매칭 추가 | 별도 게임 mock 없이 전체 조건 end-to-end 테스트는 부족 |
| Breast Collision이 크기를 따라가지 않음 | body morph partition 및 scene update 경로 보강의 대상 | **독립 CBPC/FSMP collision 갱신 성공은 입증되지 않음** |
| non-zero BodySlide 빌드 위 중첩 | 모프 합산 중첩과 메시 bake 문제 구분 | zero slider+Build Morphs/TRI 준비 필요성은 기술적으로 남음 |
| OverlayFix와 로드 크래시 | 최초 load burst 지연, handle/3D/세대, 자동 deferred·queue | **특정 크래시 root cause 확정/수정 완료 아님** |
| 성능 OFF도 스터터 | normal도 queue, synchronous event fast path 제거 | 프레임타임 측정 전 “스터터 없음” 보장 금지 |
| 캐릭터 왼쪽·틴트 화면 밖 | 대칭 거리/offset 및 최종 tint80/+10 | 사용자 스크린샷 기반 반복 조정 이력. 모든 체형/키 조합 보장 아님 |
| 드롭다운/Backspace/footer/색 | 공통 UI 수리 | UI 변경 시 반드시 재검증 |
| 문서 많은 MO2 ZIP | 정확히 7파일 allowlist | 최소 패키징 확인. 기존 설치폴더 문서 cleanup은 별개 |

### OverlayFix 참고 작업의 활용 범위

참고 작업: `codex://threads/01a0674d-5013-7972-8ff7-6cc106b05833`, “넥서스 Q&A 답변 작성”.

해당 작업에는 다음 **제안**이 있었다.

`NPC 선택 → 바디 적용 → 3D 안정화 대기 → 스킨 → 의상 → 완료`

- OverlayFix 감지/버전 정책.
- 체형 적용 뒤 실제 geometry 안정 확인.
- 3D 변경 시 재예약.
- 외부 async 완료 신호가 없어 단순 지연만으로 완전 안전을 보장할 수 없다는 경고.

현재 BCNG는 최초 2-hop 지연과 일반 queue/세대 관리 등이 있지만, 위 OverlayFix 전용 단계별 전체 state machine과 외부 작업 완료 fence를 구현했다고 볼 수 없다. 관련 crash log는 사용자가 확보할 수 없다고 했다. 버전별 외부 호환 설명은 해당 작업 당시 조사 결과이며 새 릴리스 시에는 upstream을 다시 확인해야 한다.

## 15. 자동 테스트와 검증의 정확한 범위

### 15.1 현재 10개 target

| target | 주요 검증 |
| --- | --- |
| BodyChangeNGHotkeyTests | F7, 수정키 exact match, invalid modifier-only, key 표시 |
| BodyChangeNGPresetCatalogTests | XML 다중 Preset, 보간 원본값, 계열, refit 구분, compatible slider universe, real directory 인자 |
| BodyChangeNGAssetCatalogTests | live provider root, 경로 Unicode, 부분 body/hands/head, 채널0/1/2/3/7, UBE/수인/노인/종족/3BA/UNP/SOS, 틴트 분리 |
| BodyChangeNGSkinOverrideOwnershipTests | BCNG/옛 cache ownership, 타 모드 texture override 보호 |
| BodyChangeNGBodyFamilyTests | set/Group/metadata, UBE Anus 예외, 복수 계열, 3BBB 모호성, false substring, layout |
| BodyChangeNGOutfitRefitRulesTests | 7키 import 및 malformed input |
| BodyChangeNGActorStateTests | scope enum 안정성, state signatures, normal/performance queue 정책, 절대 보정·반복 계산, 성별 변경 풀 정리 |
| BodyChangeNGRuntimeLayoutTests | SE/AE hook adapter, 미지원 runtime, FOV frustum/반복/near-far 보존 |
| BodyChangeNGPathMigrationTests | 현재 파일 우선·옛 파일 fallback |
| BodyChangeNGRaceMenuPresetMigrationTests | HPH/vanilla 변환, UBE 보존, 중복 제거, unrelated ref 유지, idempotence |

기존 1.1.0 작업 기록에서 10개 executable exit 0을 확인했다. 이번 문서 작성은 테스트 코드를 변경하거나 게임을 실행하지 않았다.

### 15.2 이번 점검에서 확인한 테스트 공백

기존 dependency 빌드 기록:

- `build/.deps/BodyChangeNGSkinOverrideOwnershipTests/windows/x64/releasedbg/tests/SkinOverrideOwnershipTests.cpp.obj.d`
- `build/.deps/BodyChangeNGOutfitRefitRulesTests/windows/x64/releasedbg/tests/OutfitRefitRulesTests.cpp.obj.d`

두 파일에 `-DNDEBUG`가 있다. 두 테스트 소스는 runtime `assert`를 사용한다.

- NDEBUG 빌드의 runtime assert는 컴파일에서 제거될 수 있다.
- SkinOverrideOwnership의 `static_assert`는 계속 검증되지만 runtime MayReplace/MayRemove assert와 OutfitRefitRules runtime assert를 exit 0만으로 검증 완료라고 할 수 없다.
- 후속 작업에서는 assert가 살아 있는 debug 테스트를 실행하거나 Require/Expect 형태의 항상 동작하는 검사를 별도 변경으로 추가해야 한다.
- 이번 요청은 인수인계 작성이므로 이 발견을 숨기지 않고 기록하되 테스트 구현 자체를 몰래 바꾸지는 않았다.

### 15.3 빌드/소스 재현 검증 기록

- Release 빌드 성공.
- DLL 리소스 1.1.0.0, 생성된 SKSE 버전 배열 1/1/0/0 확인.
- 최초 1.1.0 Source ZIP을 별도 폴더에 풀고 Release 재빌드 성공 기록: 약 137.703초.
- 이후 `834e007`은 패키징/고지문만 변경. 새 source ZIP은 stage↔archive 모든 파일 해시/항목 수 검증했지만 전체 clean build를 한 번 더 실행한 것은 아니다.
- build DLL / MO2 DLL / 현재 runtime ZIP DLL 동일성 확인.
- 인수인계에서 이미 제거된 verify 임시 폴더를 현재 검증 산출물로 링크하지 않는다.

### 15.4 아직 하지 않은 검증

- OverlayFix 버전별 크래시 재현/스택 분석.
- CBPC/FSMP breast collision 위치·크기 실측.
- 실제 3BA/BHUNP/UBE/SOS 모든 UV·다중 ARMA 장비 조합.
- 실제 게임 세이브 A→B→A 및 load-order 변경 전후 결과 보존.
- 긴 실행 시간의 memory/GPU leak profile.
- 실제 셀에서 normal/performance frame-time p95/p99/max.
- 모든 gamepad UI 경로, 작은 해상도와 큰 폰트 footer.
- 종족/팩션/키워드/class/combat style 조건 전체 game-runtime end-to-end.
- 같은 파일 ID의 XML/DDS 내용 수정 후 cache/signature invalidation 전 경로.
- 현재 10 tests가 위 실게임 검증을 대체하지 않는다.

## 16. 빌드·의존성·런타임 ABI

### 16.1 핀

`DEPENDENCIES.md`와 `xmake-requires.lock`가 기준이다.

| 의존성 | 고정 값 |
| --- | --- |
| xmake | 3.1.0 |
| CommonLibSSE-NG | v6.7.1, `70c1acd5261210982bd52f6d4468a082fe04d798` |
| OpenVR headers | v1.0.15, `60eb187801956ad277f1cae6680e3a410ee0873b` |
| Dear ImGui | 1.92.9b, imgui.cpp SHA `01CD8AFB847FE33F6D3386ECA9FA057BB87783364BF3BD13B0FC582D1F3FB5FF` |
| pugixml | 1.16, cpp SHA `04CDC6BDE588039E7E3F2AF195A6CDAD33303B2DB39568067FA5CE55E0B723C9` |
| nlohmann/json | v3.12.0 |
| xmake-repo closure | `e36e822129b0fcbdfb51633a7fcee8c76af344bf` |
| transitive | DirectXMath 2024.02, DirectXTK 24.2.0, rapidcsv 8.92, spdlog 1.16.0, Xbyak 7.06, CMake 4.3.4, Ninja 1.13.2 |

도구를 찾거나 다운로드하기 전 읽을 파일:

- `C:\Users\yunha\.codex\external-tools\TOOLS.md`
- `C:\Users\yunha\.codex\external-tools\tools-manifest.json`

사용 경로:

- xmake: `C:\Users\yunha\.codex\external-tools\xmake\3.1.0\xmake\xmake.exe`
- Git: `C:\Program Files\Git\cmd\git.exe`
- gh: `C:\Program Files\GitHub CLI\gh.exe`
- shared xmake cache: `C:\Users\yunha\AppData\Local\.xmake`

xmake exe SHA-256: `2457b663e937093d47eb050dcea4fe74232649aa1cba3b5cf58d4d837af98c15`.

문서 정리 작업을 이유로 CommonLib/ImGui/transitive dependency를 새 버전으로 올리지 않았다. 신규 업그레이드 작업 때는 최신 stable changelog/API/ABI를 확인하고 exact tag/commit을 핀한다. floating branch와 공유 cache 삭제를 금지한다.

### 16.2 빌드 명령 예

저장소에서 PowerShell 7, 검증된 MSVC/Windows SDK 환경을 사용한다.

```powershell
Set-Location -LiteralPath 'C:\Users\yunha\Desktop\Body Change NG'
$xmakeTool = 'C:\Users\yunha\.codex\external-tools\xmake\3.1.0\xmake\xmake.exe'
& $xmakeTool f -m release
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
& $xmakeTool build BodyChangeNG
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
```

버전의 단일 기준은 `xmake.lua`의 `local version = "1.1.0"`. output은 `build\v1.1.0\windows\x64\release\BodyChangeNG.dll`. Windows resource compiler를 억지로 별도 경로로 바꾸지 말고 xmake가 잡은 MSVC/SDK 환경을 유지한다.

테스트는 xmake target 이름으로 각각 빌드하고 `build\v1.1.0\tests\<target>.exe`를 직접 실행해 exit code를 확인한다. 모든 target 실행 후 configure 모드가 release로 돌아왔는지 확인하고 배포 DLL 해시를 다시 확인한다. 특히 assert 기반 두 target은 debug 또는 assert-enabled 설정으로 별도 검증한다.

### 16.3 RaceMenu ABI

- SE 1.5.97 / RaceMenu 0.4.14~0.4.16: BodyMorph v4, Override v1.
- AE 1.6.1170 / RaceMenu 0.4.20.0: BodyMorph v5, Override v2.
- v5는 v4 표면 뒤 callback 추가 구조를 확인한 typed view.
- Override v1 string은 RaceMenu 자체 NiOverride Papyrus native를 통해 넣어 내부 string table/serialization을 지킨다. private representation을 BCNG 쪽 임의 포인터로 만들지 않는다.
- Override v2는 공개 wrapper.
- 미확인 BodyMorph/Override 버전은 fail closed.

### 16.4 게임 런타임 훅

`RuntimeLayout.h`:

| 대상 | 렌더 ID/offset | 입력 ID/offset |
| --- | --- | --- |
| SE 1.5.97.0 | 75595 / 0x50 | 67315 / 0x7B |
| 명시 지원 AE 1.6.x.0 | 77226 / 0x2BC | 68617 / 0x7B |

현재 whitelist의 AE revision: 317, 318, 323, 342, 353, 629, 640, 659, 678, 1130, 1170, 1179.

- 다른 revision/VR는 flat UI 훅을 임의 활성화하지 않는다.
- xmake의 skyrim_vr=true는 CommonLib multi-runtime build 설정이지 BCNG VR UI 인게임 검증 완료가 아니다.
- RuntimeLayoutTests는 주로 SE/AE hook adapter와 projection 테스트다. 모든 CommonLib TESDataHandler typed array index·상속 virtual slot·SE/AE 경계까지 포괄했다고 해석하면 안 된다.
- CommonLib 업그레이드 시 Runtime-exclusive virtual은 SE/AE, VR, unified 3-way layout을 지켜야 한다.
- 게임 버전마다 달라지는 enum/count/member offset은 compile-time 최댓값으로 순회하지 않는다. 특히 SE 마지막 버전과 AE 구조 변경 경계를 따로 테스트한다.
- 이번 인수인계는 ABI 패치를 새로 넣는 작업이 아니다.

## 17. 릴리스·소스 ZIP·GitHub·MO2 운영

### 17.1 현재 최소 MO2 ZIP — 정확히 7개

```text
LICENSE
THIRD_PARTY_NOTICES.md
BodySkin/README.txt
TintMask/README.txt
CalienteTools/BodySlide/SliderPresets/README.txt
SKSE/Plugins/BodyChangeNG.dll
SKSE/Plugins/BodyChangeNGdistribution.json
```

이 구성이 가장 최근 사용자 요구다. README.md, CHANGELOG, 소개글, 인수인계, 소스, individual licenses 폴더를 다시 넣지 않는다. 필요한 GPL 본문과 통합 third-party 고지는 유지한다.

### 17.2 source ZIP

- Git HEAD의 추적 파일 + 빌드에 필요한 vendored/submodule 소스.
- git archive만 사용하면 submodule 자리가 비므로 Package-Release에서 필요한 부분을 넣는다.
- CommonLib include/src/res/cmake 및 필수 루트 파일/라이선스, OpenVR headers, ImGui 코어/DX11/Win32/std::string adapter, pugixml src 등.
- 빌드 산출물, .git, .xmake/cache, DLL/EXE/LIB/OBJ/PDB/ILK/EXP 제외.
- `SOURCE-REVISION.txt` 기록.
- 완전 오프라인 dependency cache 전체를 묶는 ZIP은 아니다. xmake locked package 해석에 네트워크/기존 cache가 필요할 수 있다.

### 17.3 Package-Release.ps1 안전 장치

`scripts/Package-Release.ps1`:

- PowerShell 7 이상.
- clean Git 요구. untracked 파일도 있으므로 handover 문서를 미커밋 상태로 둔 채 packaging하면 거부된다.
- xmake 버전값과 DLL resource 버전 일치 검사.
- output은 저장소 하위만 허용.
- 기존 ZIP을 조용히 덮어쓰지 않음.
- binary 정확히 7파일 allowlist.
- starter schema4/rule8 검사.
- source의 금지된 바이너리/캐시 경로 검사.
- GUID 임시 staging 폴더 사용, 삭제 전 temp-root 아래 실제 경로 검증.
- .NET ZipFile로 dotfile 포함 처리.
- archive의 전체 entry 수 + **각 파일 SHA**를 stage와 비교.
- checksum manifest 생성.

패키징 예:

```powershell
.\scripts\Package-Release.ps1 -OutputDirectory 'C:\Users\yunha\Desktop\Body Change NG\release\handoff-next-stage'
```

예시 출력 디렉터리는 새로 비어 있어야 한다. 같은 이름으로 이미 있는 릴리스 파일을 덮으려다 실패하면 기존 것을 지우지 말고 별도 stage에서 만든 뒤 백업/비교/승인을 거쳐 교체한다.

### 17.4 현재 GitHub 확인

인수인계 작성 중 읽기 전용으로 다시 확인:

- remote main = `834e007cc3db64fb8024631e3588ea7af86068d7`.
- v1.1.0 릴리스는 draft=false, prerelease=false.
- 두 ZIP size/digest가 로컬 해시와 일치.
- 이번 인수인계 문서 자체는 작성 당시 공개 릴리스 asset에 포함되어 있지 않다.
- Nexus 실제 업로드나 Google Drive 완료는 이번 확인으로 입증하지 않았다. 과거 요청이 있었던 것과 현재 업로드 성공을 혼동하지 않는다.

### 17.5 MO2 갱신 절차

1. 게임 종료 여부와 정확한 모드 대상 디렉터리 확인.
2. 현재 DLL/설정/규칙/사용자 팩/Overwrite 중 무엇을 바꾸는지 구분.
3. 기존 DLL 및 수정 대상 파일만 안전한 백업으로 보존.
4. DLL 교체 후 빌드·ZIP·MO2 DLL 해시 확인.
5. 사용자의 `settings.json`과 `BodyChangeNGdistribution.json`을 기본 샘플로 덮지 않음.
6. source ZIP 내용을 MO2에 설치하지 않음.
7. 모드 폴더 전체를 비우라는 **새 명시 지시가 없는 한** 과거 청소 요청을 재사용해 삭제하지 않음.
8. 문서만 업데이트할 때 runtime·사용자 에셋을 건드리지 않음.

과거 사용자가 “아직 배포/세이브가 없으므로 깨끗이 다시 설치”를 허용한 시점은 있었지만 현재 모든 상황에 통용되는 삭제 허가가 아니다.

## 18. Nexus 소개글·업데이트 이력·라이선스

### 18.1 현재 문서 세트

`docs/README.md`가 인덱스다.

- NEXUS-DESCRIPTION-v1.1.0-EN.bbcode: Nexus 업로드용 영어 BBCode.
- NEXUS-DESCRIPTION-v1.1.0-KO.html: 같은 내용을 읽기 쉬운 한글 HTML.
- NEXUS-DESCRIPTION-v1.1.0.md: 영문 Markdown 원고.
- NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt / KO.txt.
- NEXUS-CHANGELOG-v1.1.0-EN.txt / KO.txt.
- NEXUS-UPDATE-HISTORY-v1.1.0-EN.md: BBCode 태그 없는 영문 이력.
- UPDATE-HISTORY-v1.1.0-KO.md / NEXUS-UPDATE-HISTORY-v1.1.0-KO.html.
- RELEASE-NOTES-v1.1.0.md / RELEASE-NOTES-v1.1.0-KO.md.
- 루트 CHANGELOG.md / CHANGELOG-KO.md.
- 1.0.0 문서는 역사 보존, 새 업로드에 사용하지 않음.

### 18.2 확정 영어 short description

> Change BodySlide presets and skins on the player or NPCs, edit player tint masks in real time, instantly distribute bodies and skins to NPCs using conditions in game.

사용자가 최종 확정한 문장이다. 임의로 기능을 더 넣거나 “tint on NPC”로 넓히지 않는다.

### 18.3 강조할 장점

- MCM이 아닌 인게임 GUI.
- 플레이어/NPC 바디 프리셋과 스킨 실시간 변경.
- 플레이어 틴트마스크의 DDS·색·불투명도 실시간 조절.
- 인게임 조건 편집과 현재 로드된 NPC에 배포 요청.
- 게임 실행 중 기존 제공 폴더에 프리셋/스킨/틴트를 넣고 새로고침.
- 액터별 바디 계열에 맞는 알려진 호환 카탈로그.
- 즐겨찾기, 키 변경, gamepad navigation.
- 스킨팩을 위해 별도 NIF/HeadPart/Skin Armor 교체를 필수로 요구하지 않는 접근.
- 새 계열·남성·수인·부분팩·SOS 지원은 검증 범위에 맞춰 설명.

OBody가 나쁘다/모든 성능에서 우월하다/모든 바디 혼합이 절대 없다라는 식의 근거 없는 비교는 하지 않는다. 양쪽 모두 RaceMenu BodyMorph 기반인 공통점과 BCNG의 GUI·통합 편집·조건 풀·텍스처 접근의 차이를 구분한다.

### 18.4 사용자가 삭제한 소개글 항목

다음은 runtime 기능 제거 지시가 아니라 **Nexus 원고에서 빼달라는 지시**였다.

- SmoothCam 선택 호환.
- 상세 dropdown resize/1080p~4K scaling/camera 좌우/tint zoom/paused FSMP 설명.
- 새 설치 pause OFF/캐릭터 왼쪽 기본값 설명.
- Build Morphs·TRI 요구의 장황한 별도 bullet.
- VR 전용 렌더/입력 미검증 장문.
- 비공개 0.2.x 업데이트 안내.
- “A Multi-Bodyshape System” 제목.
- 메뉴 프레임워크 설명.
- 크레딧의 OBody NG / MasterList / SFS 상세 나열.
- “v1.0.0부터 Nexus에는…”의 시작 버전 표현.
- 중복된 NPC 조건·의상 기능 설명.

기술 README에 필요한 제약·출처가 남아 있는 것은 문서 대상 차이다. 사용자가 크레딧 원고에서 뺐다고 필요한 라이선스나 소스 attribution까지 삭제하지 않는다.

설치 절차와 에셋 폴더 배치 설명은 별도 섹션. 조건/의상 설명, 자체 JSON 백업, 선택적 OBody JSON 호환도 분리해 중복을 피한다. 제목은 상단 중앙, 일관된 대·소제목 컬러, Nexus에서 지원되는 BBCode 사용.

### 18.5 라이선스 최소화

- 프로젝트 GPL-3.0: LICENSE 유지.
- 통합 THIRD_PARTY_NOTICES.md에 필요한 upstream 저작권/조건 유지.
- CommonLib6.7.1 GPL additional permissions, shared MIT 본문과 저작권 목록, OpenVR/Xbyak/rapidcsv BSD 조건 등.
- MIT 계열: ImGui, stb 선택 라이선스, pugixml, nlohmann/json, 원 CommonLib, spdlog, Microsoft DirectX 구성요소 등의 원문 고지.
- Source ZIP에는 각 dependency 원본 license/예외 텍스트 유지.
- Nexus 크레딧 문장에는 GNU GPLv3와 실제 Body-Change-NG GitHub 링크.
- 한글 권리 문구: **“모든 저작권과 권리는 각 원작자에게 있습니다.”**
- pugixml은 BodySlide XML 파싱, nlohmann/json은 설정·조건·jslot 등 실제 사용 라이브러리이므로 라이선스 고지 제거 대상이 아니다.
- 설치 ZIP 문서를 줄이는 것과 필요한 고지를 생략하는 것을 혼동하지 않는다.

## 19. 우선순위별 후속 점검

이번 인수인계는 아래 항목을 자동으로 구현하라는 새 승인이 아니다. 다음 기능 수정 작업 범위를 정할 때 근거로 사용한다.

### P1 — 완료로 오인하면 안 되는 항목

1. **OverlayFix 병용 크래시**: 특정 root cause 불명. 현재 코드에 별도 OverlayFix 검출·단계별 완료 fence는 없음. 외부 모드 비동기 완료와 3D 수명 확인 없이 단순 delay를 완전 해결로 홍보하지 말 것.
2. **Breast collision**: CBPC/FSMP 실제 collision update 검증 없음. 바디 눈에 보이는 모프와 collision mesh/설정 갱신은 구분.
3. **assert 테스트 사각지대**: 위 두 target은 assert-enabled 실행 또는 항상 동작하는 검사로 확인 필요.
4. **OBody 의상 등록 지속성**: 메모리 저장뿐. 재실행 후 등록 유지 기대와 현재 동작 간 차이 검토.
5. **실게임 피부 아틀라스**: CBBE/UNP/UBE/SOS 각 실제 NIF/ARMA/채널 매칭 및 default 복원 확인.

### P2 — 기존 기능과 연결된 회귀 위험

6. preview에서도 legacy OBody/OClothe 키가 지워짐. 취소 시 기대 상태가 맞는지 검증.
7. 같은 ID의 XML 내용 변경 후 Registry signature가 같아 재배포가 생략되지 않는지, 새로고침에서 invalidation이 충분한지 확인.
8. 같은 경로 DDS 내용 변경 후 엔진 texture cache 갱신 확인.
9. 팩션 조건이 ActorBase만 봄. reference에만 추가된 faction을 포함할지 요구 명확화.
10. custom follower 전용 framework 누락 가능성, custom race/elder 판정 보수적 fallback.
11. typed FormArray 및 CommonLib runtime 경계의 실제 테스트 확장. 현재 projection 테스트를 전체 ABI 증명으로 사용하지 않음.
12. 세이브 A/B 전환, load-order 변경, reference 재생성, race change 후 Registry 상태/3D cache 충돌 검사.
13. 외부 모프가 나중에 변경될 때 BCNG 보정 합계, 메뉴 재진입/기본 바디 restore의 의미 확인.
14. 노인/종족 전용 파일 누락 시 “팩의 일반값”과 “게임 원본” 중 사용자 기대를 다시 확인할 경우 실제 현재 merge 우선순위를 제시.

### P3 — 운영·성능·UI

15. 실제 제보 규모 normal/performance 프레임타임 비교. task hop을 frame budget처럼 설명하지 않음.
16. 장기 session pending/세대 map 크기, detach 정리, disk cache 증가 계측.
17. 작은 해상도·큰 UI scale·긴 영문 footer/gamepad 실제 사용 확인.
18. 기존 MO2 폴더 문서와 현재 최소 ZIP 문서 차이. 사용자 설정을 보존한 별도 문서 정리 여부 결정.
19. 새 handover는 source/Git 문서에만 넣고 minimal installer 7파일을 유지.
20. Nexus/Drive 배포 상태는 실제 페이지/파일을 다시 읽고 보고. GitHub 업로드 성공만으로 다른 사이트까지 완료라 하지 않음.

## 20. 다음 작업자 실행 순서·금지 사항·참고 자료

### 20.1 첫 15분 체크리스트

1. 실제 cwd가 Body Change NG인지 확인. Obody NPC/SFS와 혼동하지 않음.
2. 이 문서 1~3장, 14~16장과 부록 해시 확인.
3. `git status --short`, `git log -5 --oneline`, submodule 상태, 정확한 HEAD 확인.
4. 사용자 수정/다른 세션 작업이 있으면 건드리지 않고 먼저 경계 확인.
5. `DEPENDENCIES.md`, xmake lock, shared tool registry 읽기.
6. 다음 작업이 진단인지 수정인지 릴리스인지 구분.
7. 의심 기능의 UI→backend→Registry→이벤트→복원 경로를 모두 추적.
8. 바뀐 모듈의 작은 회귀 테스트부터 실행. 테스트가 실제 assert를 실행하는지 확인.
9. DLL 업데이트가 필요하면 게임 종료·정확한 MO2 경로·설정 보존부터 확인.
10. 완료 보고에 코드 수정/자동 검증/인게임 검증/미검증을 분리.

### 20.2 금지·주의

- `git reset --hard`, 다른 사람 변경 checkout, 모드 폴더 전체 삭제를 묵시적으로 하지 않는다.
- 사용자 설치 팩·설정·배포 JSON·jslot 원본을 백업 없이 덮지 않는다.
- 처음에 “세이브 없음”이라고 했던 발언을 영구적 데이터 삭제 허가로 쓰지 않는다.
- 타 모드 모프 전체 clear, 임의 skin slot/channel 대체, 알 수 없는 ABI 호출 금지.
- 카탈로그 scan을 액터 이벤트/매 프레임 hot path에 넣지 않는다.
- CBBE/UNP/UBE 이름 판정을 소문자 substring 한두 개로 단순화하지 않는다.
- “3BBB Body Amazing UBE Anus” 예외와 의상명 프리셋 허용을 유지.
- 기존 enum/schema를 UI 정렬 때문에 재배열하지 않는다.
- 배포 first-match의 빈 채널을 다음 규칙으로 통과시키는 변경은 사양 변경이다.
- NPC 틴트 지원·모든 UBE 확장 맵·OverlayFix 완전 호환·stutter zero를 근거 없이 소개하지 않는다.
- 카메라 render pass 안 엔진 update, 수동 UpdateThirdPerson, 제거한 pause settle 실험 복원 금지.
- 재현 도구/라이브러리를 임의 최신판으로 바꾸거나 shared dependency cache를 지우지 않는다.
- 문서에 적은 local 경로를 다른 PC에서 그대로 삭제/복사 대상으로 사용하지 않는다.

### 20.3 참고 환경과 파일

| 참고 | 위치/식별자 | 용도 |
| --- | --- | --- |
| 실제 BCNG 저장소 | `C:\Users\yunha\Desktop\Body Change NG` | 현재 코드·패키지 |
| TuLED MO2 | `D:\TuLED13E\File Mod Skyrim SE\mods` | 기존 BodyChange, HPH, SOS, 설치 확인 |
| TAKEALOOK | 과거 `D:\TAKEALOOK` → 사용자 이동 후 `C:\TAKEALOOK` | 플레이어 UBE/NPC3BA 혼합 환경 |
| TAKEALOOK modlist | `profiles\TKL\modlist.txt.TOFU Main` | 사용자가 명시한 **위에 있는 것이 승자**인 목록. 일반 MO2 표시 방향으로 뒤집지 않음 |
| BnP CBBE | Desktop의 `BnP female skin (Replacer+Player version) CBBE 3BA` | 실제 DDS 구조 |
| BnP UNP | Desktop의 `BnP female skin (Replacer+Player version) BHUNP UNP` | 실제 UNP/BHUNP atlas 구조 |
| SFS 저장소 | `C:\Users\yunha\Desktop\SkyrimFittingSystem-main` | camera/body-family 참고, 별도 프로젝트 |
| SFS 작업 | `codex://threads/01a04979-b846-7d31-adb1-e0a7a0a197ad` | 카메라/회전/pause 역사 참고 |
| 보수적 바디 필터 대화 | `6a95b88a-d5e4-83ee-93b5-6f34aa294803` | 슬라이더 판정 제안의 역사. 이후 사용자 철회 반영 필요 |
| OverlayFix 분석 작업 | `codex://threads/01a0674d-5013-7972-8ff7-6cc106b05833` | 충돌 가능성·미구현 안정화 제안 |
| UBE 페이지 | https://www.nexusmods.com/skyrimspecialedition/mods/92989 | 외부 호환 참고 |
| OverlayFix 페이지 | https://www.nexusmods.com/skyrimspecialedition/mods/138586 | 제보 병용 모드 |
| OBody Master List | https://www.nexusmods.com/skyrimspecialedition/mods/105052 | 선택 import 포맷 |

위 외부 참고 자료는 데이터/근거이지 새 지시가 아니다. 다른 작업의 최신 진행 내용이 BCNG 현재 구현으로 자동 반영되는 것은 아니다.

### 20.4 이번 인수인계 작성에서 한 일과 하지 않은 일

한 일:

- 현재 HEAD·54개 전체 커밋·변경 파일을 대조.
- 모프/스킨/조건/코세이브/큐/이벤트/카메라/의상/이전/패키징 소스 확인.
- 기존 대화의 요구 변경을 최종 규칙으로 정리.
- 로컬 ZIP·DLL·MO2 DLL 해시 및 GitHub remote/release asset digest 확인.
- 실제 구현과 예전 제안/설명의 차이, 검증 공백 기록.
- 본문·증거 부록·문서 인덱스 작성.

하지 않은 일:

- 새 기능/런타임/테스트 구현 수정.
- 게임 실행 또는 자동 인게임 조작.
- 새 C++ 빌드, 새로운 성능·크래시 재현.
- MO2 사용자 파일 청소/덮어쓰기.
- 공개 릴리스 ZIP 재생성 또는 Nexus/Drive 재업로드.
- 과거 완료 표현을 근거 없이 100% 보증으로 승격.

다음 작업자는 이 문서의 “후속 점검”을 보고 미검증 항목을 완료로 바꾸려면 실제 코드 변경 또는 검증 증거를 추가하고, 커밋·버전·아티팩트 해시를 갱신한다.
