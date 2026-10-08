# v1.3.6 릴리즈 검증 — 2026-10-08

## 변경 범위

바디·의상·미리보기 취소의 화면 갱신을 기존 액터 작업 큐에서 합치고,
로드/RaceMenu 복원 시 살아 있는 확정 모프 값을 유지한다. 키·프리셋·옵션이
변경된 경우의 복구 확정과 사용자의 직접 프리셋·설정 적용은 유지했다.
새 영구 액터 컨테이너, 메시 포인터 보관, 전체 메시 재생성 훅은 추가하지 않았다.

CommonLibSSE-NG v11.0.0을 커밋
`94faaed0c60eddd8347767f2d4d29a97c93bde8c`로 고정했다. SE/AE 전용 flat 구성과
잠긴 패키지 버전은 유지하며 업스트림의 고정 MinHook v1.3.4 HDE만 빌드에 포함한다.
OpenVR SDK는 SE/AE 빌드에 필요하지 않아 소스 패키지의 추가 복사 대상에서 제외했다.
원본 CommonLib 코드는 수정하지 않았다.

## 빌드와 검사

- XMake 3.1.0 / VS 2026 MSVC / Release, 전체 대상 빌드 성공.
- DLL 파일 버전: **1.3.6.0**, 3,141,632바이트.
- DLL SHA-256: `DAEA05128EFF14A02927F0667679B4EB8739DD38DDDDF0C8BA4B968FD710D987`.
- 캐시 도구 SHA-256: `BF21E223B326FA6B2F6478C3E70FD1F9E56583A9B1CA176E5AC796A89781F286`.
- **회귀 검사 48/48 통과**. 지원 런타임 12개, BodyMorph v4/v5 공통 ABI,
  모프 키 소유권, 액터 수명·복원·취소, 배포·스킨·오버레이 및 입력 검사 포함.
- 소스 패턴 검사는 주석 문구 대신 실제 화면 갱신 호출을 경계로 사용하도록
  보완했다. 모든 수치가 필터 없이 SetMorph로 전달되는 기존 검사 의미는 유지한다.

| 검사 실행 파일 | 결과 |
|---|---|
| BodyChangeNGActorStateTests.exe | PASS |
| BodyChangeNGAppearanceIntegrationTests.exe | PASS |
| BodyChangeNGAppearanceLifecycleTests.exe | PASS |
| BodyChangeNGAssetCatalogTests.exe | PASS |
| BodyChangeNGAsyncWorkGuardTests.exe | PASS |
| BodyChangeNGBodyFamilyTests.exe | PASS |
| BodyChangeNGBodyMorphAbiTests.exe | PASS |
| BodyChangeNGBodyMorphKeyTests.exe | PASS |
| BodyChangeNGCacheMaintenanceTests.exe | PASS |
| BodyChangeNGCatalogRefreshTests.exe | PASS |
| BodyChangeNGCodePatternTests.exe | PASS |
| BodyChangeNGDistributionAuthoringTests.exe | PASS |
| BodyChangeNGDistributionEditorTests.exe | PASS |
| BodyChangeNGDistributionEditorUITests.exe | PASS |
| BodyChangeNGDistributionLifecycleTests.exe | PASS |
| BodyChangeNGDistributionPersistenceTests.exe | PASS |
| BodyChangeNGDistributionPresetTests.exe | PASS |
| BodyChangeNGDistributionTargetReadTests.exe | PASS |
| BodyChangeNGFacePreviewReplayTests.exe | PASS |
| BodyChangeNGFacePreviewTransactionTests.exe | PASS |
| BodyChangeNGFaceSkinPolicyTests.exe | PASS |
| BodyChangeNGFactionEditorIDTests.exe | PASS |
| BodyChangeNGFailurePathTests.exe | PASS |
| BodyChangeNGFormDeletePolicyTests.exe | PASS |
| BodyChangeNGFrameTaskQueueTests.exe | PASS |
| BodyChangeNGGenitalPersistenceTests.exe | PASS |
| BodyChangeNGHotkeyTests.exe | PASS |
| BodyChangeNGMouseInputReplayTests.exe | PASS |
| BodyChangeNGNativeAddonPolicyTests.exe | PASS |
| BodyChangeNGOBodyNumericTests.exe | PASS |
| BodyChangeNGOutfitRefitRulesTests.exe | PASS |
| BodyChangeNGOverlayPolicyTests.exe | PASS |
| BodyChangeNGPathMigrationTests.exe | PASS |
| BodyChangeNGPopupPlacementTests.exe | PASS |
| BodyChangeNGPresetCatalogTests.exe | PASS |
| BodyChangeNGPreviewRestoreTests.exe | PASS |
| BodyChangeNGRaceMenuExtraDataTests.exe | PASS |
| BodyChangeNGRaceMenuPresetMigrationTests.exe | PASS |
| BodyChangeNGRemovalPreparationTests.exe | PASS |
| BodyChangeNGRenderedOutfitTests.exe | PASS |
| BodyChangeNGRuntimeLayoutTests.exe | PASS |
| BodyChangeNGSkinArchitectureTests.exe | PASS |
| BodyChangeNGSkinSessionStateTests.exe | PASS |
| BodyChangeNGTexturePathIsolationTests.exe | PASS |
| BodyChangeNGUbeGenitalTests.exe | PASS |
| BodyChangeNGUbeMorphTests.exe | PASS |
| BodyChangeNGUbeNippleTests.exe | PASS |
| BodyChangeNGWheelFilterTests.exe | PASS |

## 아카이브 절차와 재현

`scripts/Package-Release.ps1`은 깨끗한 Git 커밋과 DLL 리소스 버전을 확인한다.
릴리즈 ZIP의 11개 파일, opt-in 배포 규칙, 소스 의존성 및 각 ZIP 항목의
SHA-256을 검증하고 `SOURCE-REVISION.txt`와 별도의 SHA256SUMS를 기록한다.
이미 존재하는 이름의 배포 파일은 자동으로 덮어쓰지 않는다.

소스 ZIP에는 Git 추적 프로젝트 파일, CommonLib 소스·헤더·리소스,
ImGui와 pugixml의 빌드 필수 파일 및 라이선스를 포함한다. 패키지와 HDE
다운로드는 고정된 레시피·버전·체크섬을 사용한다. `.git`, 빌드 산출물,
설정 캐시와 테스트 EXE는 포함하지 않는다.

## 인게임 검증 한계

이번 릴리즈 과정에서 게임이나 세이브·MO2 설치본을 수정하지 않았다.
AE 체형 누적 제보의 실제 원인과 모든 RaceMenu 배포본의 인게임 호환성을
확정하지 않는다. 기존 UBE SE DLL 전용 오류 수정의 적용 범위는 그대로다.
상세 근거는 [공통 모프 갱신 호환성 감사](MORPH-REFRESH-COMPATIBILITY-20261008.md)를 따른다.

## 한영 체인지로그

- [한국어 텍스트](NEXUS-CHANGELOG-v1.3.6-KO.txt) / [한국어 HTML](NEXUS-CHANGELOG-v1.3.6-KO.html)
- [English text](NEXUS-CHANGELOG-v1.3.6-EN.txt) / [Nexus BBCode](NEXUS-CHANGELOG-v1.3.6-EN.bbcode)
