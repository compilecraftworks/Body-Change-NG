# v1.3.5 릴리즈 점검 — 2026-09-28

## 포함 범위

- 플레이어 얼굴 미리보기의 RaceMenu 임시 키 적용·복원·저장 경계 보호.
- UBE 기본 SOS/TRX UBE 54번 성기 슬롯 및 등록 판정, 별도 TRX UBE 스킨 유형.
- 기존 3BA TRX/ERF 52번 경로, 남성 경로, UBE 몸통·모발 제외 유지.
- UBE 기본 맵 6개만 교체하고 MO2의 loose RFAOS·wet 효과를 normal 캐시 경로에 연결.
- 한영 소개글·체인지로그와 후타스킨 전체 경로 안내.

## 빌드와 검사

- 저장소에 고정된 xmake 3.1.0, CommonLibSSE-NG 6.7.1과 기존 의존성 잠금 유지.
- Visual Studio 2026 / MSVC 14.51.36231 / Windows SDK 10.0.26100.0.
- `release`, SE/AE 활성, VR 비활성. upstream `REL/Common.h`가
  `EXCLUSIVE_SKYRIM_FLAT`을 선택하며 새 얼굴 관리자도 이를 컴파일 검사한다.
- `xmake build -y -a` 성공. 회귀 실행 파일 42/42 통과.
- 규칙 작성 PowerShell 검사 통과. HTML 밝음/어두움 대비 108개 조합 통과.
- 실제 툴레드 후타스킨 카탈로그 12개: UBE 기본형 3, TRX UBE 1, 3BA TRX 4, ERF 4.
- DLL FileVersion/ProductVersion: `1.3.5.0`.
- DLL SHA-256: `9EEEBE73E97112CAF0542A0A2E7887E52B9F74F96988C61BFABB95F3F85B3E42`.

위 결과는 빌드·오프라인 회귀 및 로컬 에셋 검사다. 이번 배포 작업에서
게임을 실행하거나 인게임 렌더링·전체 엔진 메모리 사용량을 측정하지 않았다.

## 툴레드 MO2 반영

- 대상: `D:\TuLED\File Mod Skyrim SE\mods\Body Change NG`.
- 2026-09-28 17:43 KST, 게임 종료 상태에서 DLL·두 폴더 안내·meta.ini 교체.
- 18:04 KST, UBE 기본 효과 보완을 포함한 최종 DLL과 BodySkin 안내문을 재반영했다.
- 최종 설치 DLL 해시가 위 릴리즈 DLL과 일치한다.
- 재반영 직전 백업: `build/mo2-backups/TuLED-v1.3.5-effects-20260928-180400`.
- 이전 파일 백업: `build/mo2-backups/TuLED-v1.3.5-20260928-174307`.
- 설정·사용자 배포 규칙·즐겨찾기·스킨팩·캐시·세이브·다른 모드는 변경하지 않았다.
- 이전 조사 문서의 1.3.4 DLL과 미배포 표기는 당시 단계의 기록이며,
  이 문서의 1.3.5 통합본이 후속 배포 대상이다.

## 패키지 정책

`scripts/Package-Release.ps1`로 커밋된 소스와 Release DLL을 묶는다.
배포본은 런타임·도구·폴더/규칙 안내·필수 라이선스만 포함하고 PDB는 제외한다.
시작 배포 규칙은 schema 8의 빈 목록이며, 설치본의 사용자 규칙과 교체하지 않는다.
소스 ZIP은 커밋 ID와 고정된 빌드 의존성을 포함하고 빌드 결과·개인 캐시를 제외한다.
