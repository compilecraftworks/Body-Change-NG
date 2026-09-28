# v1.3.5 릴리즈 점검 — 2026-09-29

## 최신 통합본

- 기존 스킨·후타스킨 수정에 배포 조건 편집 UI, 후보 변경, 대상 검색,
  팩션 EditorID 보완, 인간·엘프 자동 미리보기 대상을 포함했다.
- 1.5.97의 확인된 UBE SE 동봉 DLL에 한정한 모프 원본 데이터 정렬 보완을 포함했다.
  프리셋 수치·체중 보간·보정/랜덤화 계산식은 변경하지 않았다.
- 한영 체인지로그를 사용자 관점으로 축약하고 소개글의 편집·저장 절차를 갱신했다.
  1.5.97에서도 UBE의 DLL 없는 AE 옵션 + SE용 RaceMenu + skee64backports
  권장 구성과 Zeroed Sliders / Build Morphs 준비를 안내한다.
- 사용자가 텍스트 입력 중 Delete 차단을 확인했다. 입력 코드는 추가 변경하지 않았다.
- 저장소 고정 XMake 3.1.0 / MSVC 14.51.36231 / flat SE/AE Release 전체 빌드 성공.
- 최신 `*Tests.exe` 47개 전부 통과. 규칙 작성 PowerShell 검사와 문서 대비 108개 통과.
- zlib 1.3.2를 정적으로 사용하며 기존 CommonLib 전이 의존성은 변경하지 않았다.
- DLL 버전 `1.3.5.0`, SHA-256:
  `459E1BDD768BB90D917B7BDB6893AC1F6C810AA54ECDE78A9E6A9AA1DDB14861`.
- 툴레드 설치 DLL과 위 빌드 DLL의 해시 일치를 재확인했다.
  경로: `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`.
- 최신 DLL 교체 전 백업:
  `build/mo2-backups/TuLED-20260929-ui-search-3e3f5ff8133b44b89193258bbeaa9386/`.
- 이번 패키징 작업은 게임을 실행하거나 사용자 설정·규칙·세이브를 변경하지 않는다.
  상세 동작과 검사 범위는 `DISTRIBUTION-EDITOR-AUDIT-20260929.md` 및
  `UBE-SE-MORPH-BASELINE-AUDIT-20260928.md`를 참고한다.

## 아래 내용은 2026-09-28 이전 통합본 기록

## 포함 범위

- 플레이어 얼굴 미리보기의 RaceMenu 임시 키 적용·복원·저장 경계 보호.
- UBE 기본 SOS/TRX UBE 54번 성기 슬롯 및 등록 판정, 별도 TRX UBE 스킨 유형.
- 기존 3BA TRX/ERF 52번 경로, 남성 경로, UBE 몸통·모발 제외 유지.
- UBE 기본 맵 6개만 교체하고 기본 normal의 Head/Body 폴더에 있는 MO2 loose RFAOS·wet 효과만 캐시에 연결.
- 상위 !UBE 폴더의 wet을 대신 가져오는 경로를 제거하고 효과 캐시 서명을 v2로 분리. 기존 캐시 삭제 없음.
- 한영 소개글·체인지로그와 후타스킨 전체 경로 안내.

## 빌드와 검사

- 저장소에 고정된 xmake 3.1.0, CommonLibSSE-NG 6.7.1과 기존 의존성 잠금 유지.
- Visual Studio 2026 / MSVC 14.51.36231 / Windows SDK 10.0.26100.0.
- `release`, SE/AE 활성, VR 비활성. upstream `REL/Common.h`가
  `EXCLUSIVE_SKYRIM_FLAT`을 선택하며 새 얼굴 관리자도 이를 컴파일 검사한다.
- `xmake build -y -a` 성공. 회귀 실행 파일 43/43 통과.
- 휠 입력 필터 147,456개 오프라인 조합과 방향키 차단 양성 대조 통과. 줌 불가 해결을 뜻하지 않으며 카메라/입력 코드는 변경하지 않음.
- 규칙 작성 PowerShell 검사 통과. HTML 밝음/어두움 대비 108개 조합 통과.
- 실제 툴레드 후타스킨 카탈로그 12개: UBE 기본형 3, TRX UBE 1, 3BA TRX 4, ERF 4.
- DLL FileVersion/ProductVersion: `1.3.5.0`.
- DLL SHA-256: `5845926F9A8E14E972D0031FF7F716CAC6C8FD31A3F9CFC8FC7C74F8B531B4C2`.

위 결과는 빌드·오프라인 회귀 및 로컬 에셋 검사다. 이번 배포 작업에서
게임을 실행하거나 인게임 렌더링·전체 엔진 메모리 사용량을 측정하지 않았다.

## 툴레드 MO2 반영

- 대상: `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG`.
- 2026-09-28 17:43 KST, 게임 종료 상태에서 DLL·두 폴더 안내·meta.ini 교체.
- 18:04 KST, UBE 기본 효과 보완의 초기 DLL과 BodySkin 안내문을 재반영했다.
- 20:36 KST, 상위 폴더 wet 탐색을 제거한 후속 DLL과 BodySkin 안내문을 교체했다.
- 최종 설치 DLL 해시가 위 릴리즈 DLL과 일치한다.
- 최종 교체 직전 백업: `build/mo2-backups/TuLED-v1.3.5-wet-fix-20260928-203608`.
- 초기 효과 보완 직전 백업: `build/mo2-backups/TuLED-v1.3.5-effects-20260928-180400`.
- 이전 파일 백업: `build/mo2-backups/TuLED-v1.3.5-20260928-174307`.
- 설정·사용자 배포 규칙·즐겨찾기·스킨팩·캐시·세이브·다른 모드는 변경하지 않았다.
- 이전 조사 문서의 1.3.4 DLL과 미배포 표기는 당시 단계의 기록이며,
  이 문서의 1.3.5 통합본이 후속 배포 대상이다.

## 패키지 정책

`scripts/Package-Release.ps1`로 커밋된 소스와 Release DLL을 묶는다.
배포본은 런타임·도구·폴더/규칙 안내·필수 라이선스만 포함하고 PDB는 제외한다.
시작 배포 규칙은 schema 8의 빈 목록이며, 설치본의 사용자 규칙과 교체하지 않는다.
소스 ZIP은 커밋 ID와 고정된 빌드 의존성을 포함하고 빌드 결과·개인 캐시를 제외한다.
