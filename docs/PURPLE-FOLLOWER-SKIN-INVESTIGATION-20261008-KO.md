# 커스텀 동료의 보라색 스킨: 소스 결함 수정 — 2026-10-09

2026-10-17 갱신: 아래 개발 변경은 1.3.7 릴리즈에 포함한다. 본문의 개발 DLL 해시와 공개 배포 미반영 설명은 당시 기록이며, 최종 배포 검사는 [1.3.7 검증 기록](RELEASE-VERIFICATION-v1.3.7-KO.md)을 따른다.

## 현재 개발 수정

제보자 로그를 전제로 작업을 보류하지 않고, 실제 production 함수를 가짜 엔진 경계와 연결해 결함과 복구를 검사했다. **확인한 결함은 새 몸/손/발 material의 필수 리소스가 없거나 이전 BCNG 캐시 경로가 남아도 동일 폼/프로필이라는 이유로 적용을 성공 처리하고 복구를 생략하는 경로다.** 이 지속/복구 결함은 개발 소스에서 수정했다. 제보자 환경에서 최초 DDS 로딩 실패를 일으킨 특정 공급자·실행 순서까지 확정한 것은 아니며, 실제 보라색 화면 재현/완치 검증과 구분한다.

- ActorRegistry의 기존 이벤트 기반 스킨 재확인에서 세션 성공 표식만으로 live 검증을 생략하지 않는다. 같은 프로필 빠른 경로와 얼굴 완료 콜백도 body 리소스 누락을 전체 성공으로 기록하지 않는다.
- 소유한 Skin Armor의 private ARMA/TXST와 현재 BOD2 슬롯 32/33/37/53만 읽는다. 모든 지원 flat 런타임에서 무기 삽입 인덱스 전의 공통 슬롯이다. 1인칭/버퍼 슬롯·원본 공유 폼·의복·별도 성기/항문 경로를 새 공급 대상으로 확장하지 않았다.
- 지원 런타임 확인 뒤 MiddleHighProcessData의 기존 검증된 update3DModel(0x311)이 비워진 상태에서 shader 경로와 diffuse/normal NiSourceTexture 및 rendererTexture 포인터를 확인한다. 이 검사는 D3D 리소스 내용/렌더 화면의 성공 보증은 아니다.
- 같은 private 부분의 이전 BCNG 캐시 경로는 stale 상태로 검출한다. shader/TXST가 외부 공급자 경로로 바뀌었다면 다른 부위의 BCNG 리소스가 실패했어도 전체 actor refresh를 하지 않는다.
- 적용 후 및 기존 NiNodeUpdate 경계에서 같은 세대의 actor handle로만 검사한다. 기존 FrameTasks에 별도 의미 채널 skinBodyVerify를 사용해 다른 장비 검사를 교체하지 않는다. 진행 중 재생성/얼굴 작업에는 최초 작업 lease를 유지한 최대 30회의 지연 확인만 수행한다.
- 실패한 actor/선택 세대당 최대 두 번 기존 facade/face-barrier의 DoReset3D(false) 경로를 요청한다. 복구가 만드는 이벤트로 횟수가 다시 초기화되지 않는다. 일시적인 partClone 부재도 소진한 횟수를 초기화하지 않는다. 누락 리소스는 엔진 BSResource에서 DDS 선두 4바이트를 읽을 수 있어야 재시도한다.
- 복구 표에는 ID/세대/횟수만 보관하며 최대 4,096개다. 실제 정상 리소스 확인, cell detach, session reset에서 해제한다. 새 폼 복제·렌더 포인터 보관·재질 직접 쓰기·상시 폴링·추가 엔진 후킹·세이브 스키마 변경이 없다.

## 최종 검증

SE/AE EXCLUSIVE_SKYRIM_FLAT, VR 끔, 기존 CommonLibSSE-NG v11.0.0/commit 및 XMake 3.1.0 핀으로 전체 빌드 성공. 실제 ReadBodyTextures/VerifyBodyTexturesNow/LiveStateMatches/NeedsSkinApply 함수를 수정 없이 추출하는 새 검사에서 100,271개 조건 및 50,000회 복구 수명을 검사했다. SE 1.5.97 및 AE 1.6.629/1.6.1170 경계, diffuse/normal/renderer 부재, 이전 캐시, 혼합 외부 shader/TXST, 숨겨진 부위, 세대/ActorBase/카탈로그 변경, 제거 모드, 대기 시간 만료, 정원 제한을 포함한다. 기존 런타임 표/ABI 검사까지 **50/50 실행 파일이 최종 통과**했다. 반복 후 활성 복구 표와 작업 큐가 비어 있는 것을 검사했으며, 실제 엔진/GPU의 할당 누수가 전혀 없다는 실게임 보증은 아니다.

검사 초기에는 잘못된 가짜 노드 이름(3BBBAnus 대신 실제 정책의 3BBB_Anus)이 검사 실패를 일으켰다. 테스트 픽스처를 정정하고 예외를 텍스트로 반환하도록 바꿨다. 초기 테스트 EXE가 Windows 오류 대기/접근 거부로 잠겨 있어 최종 검사는 BodyChangeNGBodyTextureRecoveryChecks.exe를 사용한다. 이어진 재검사에서 기존 CacheMaintenanceTests의 전체 프로세스 handle-count 동등성 검사가 한 번 실패했다. 해당 제품 코드를 변경하지 않았으며 최종 실행에서는 통과했다. 이 중간 실패를 게임 DLL 오류나 통과 결과로 간주하지 않았다.

개발 DLL SHA-256: F654523BD8CA516DC905E11606B443A83B347BB041FE5A54417F448DB7BD6096.
결과: build/purple-recovery-test-results.json, build/purple-recovery-verification.json, build/purple-recovery-build.log.
공개 1.3.6 배포/소스 ZIP, GitHub, MO2 설치본은 교체하지 않았다. 아래는 2026-10-08 조사 당시의 증거와 진단 빌드 기록이다.

## 결론과 증거 수준

**확정한 코드 결함은 몸·손·발의 렌더 텍스처를 검증하지 않고 적용 성공으로 취급하는 확인 누락이다.** 보라색을 최초로 만드는 리소스 실패/외부 덮어쓰기의 직접 원인은 아직 실패 순간의 게임 상태로 확정하지 않았다. 이 둘을 같은 의미로 쓰거나 이 제보가 해결됐다고 표시하지 않는다.

실제 production `OwnsCurrentPointers`와 `LiveStateMatches` 함수를 수정 없이 추출해 실행했다. 동일한 ActorBase 스킨 포인터, 프로필 ID/해시, 정상 얼굴 조건에서 몸의 diffuse 리소스가 없는 상태를 표현해도 `LiveStateMatches=true`였다. 외부 스킨 포인터 및 얼굴 실패 대조군은 false였다. 이는 가짜 엔진 경계의 소스 반례이며 실제 GPU나 보라색 화면 재현이 아니다.

`ActorRegistry::NeedsSkinApply`에는 `verifiedThisSession`이면 live matcher 자체를 호출하지 않는 빠른 경로도 있다. `ApplyNow`는 같은 프로필/해시와 소유한 폼 포인터가 유지되면 몸 갱신 없이 얼굴만 처리할 수 있다. 세 경로 모두 실제 body/hands/feet 재질의 diffuse/normal 존재를 성공 조건에 포함하지 않는다. 얼굴에는 별도 NiOverride 완료·실패 확인 경로가 있어, '머리는 정상이고 몸/손/발이 간헐적으로 보라색'이라는 관측과 일치하는 차이는 있다. 일치한다고 직접 원인이 증명되는 것은 아니다.

NPC cell detach에서는 `ForgetActorState`를 통해 피부 적용 상태가 무효화된다. 따라서 세션 검증 빠른 경로만으로 모든 셀 이동 실패를 설명할 수는 없다. attach 이후 native 폼 기반 빠른 경로 및 렌더 리소스 상태를 함께 확인해야 한다.

## 설치 자료 확인

- TAKEALOOK TKL 활성 플러그인 4,146개를 읽기 전용으로 추적했다. 누락 플러그인/모드 디렉터리는 0개였다. 이는 이 로컬 환경의 정적 레코드 검사이며 제보자의 모드 구성 검사가 아니다.
- Sofia의 최종 NPC 레코드는 `RDF_Sofia_patch.esp`, Skin은 원본 `SofiaFollower.esp|00A4F5`다. Feris의 최종 NPC 레코드는 `TKL_FerisOutfit_Crossroads.esp`이며 Skin은 `Nero_Follower.esp|000808`이다. 원본 Feris만으로 현재 리플레이서 스킨을 설명하지 않았다.
- 두 원본과 Nero/Yovanna 자료 모두 몸/손/발 ARMA 및 명시적인 NAM1 TXST를 갖는다. 몸/발은 같은 TXST를 공유하고 손은 별도다. NIF-only 스킨의 빈 TXST 문제라고 일반화할 근거가 없다. 로컬 Sofia NPC는 Yovanna의 독립 NPC가 아니므로 그 독립 NPC 스킨을 winning Sofia Skin으로 간주하지 않았다.
- 원본 Feris BSA 1,091개, Sofia BSA 322개 PEX의 문자열 테이블을 확인했다. 이 자료에서는 `SetSkin`, `SetNthTexturePath`, `AddSkinOverride`, `ApplySkinOverrides`, `SetSkinProperty` 참조를 찾지 못했다. Sofia MCM에는 `QueueNiNodeUpdate`가 있다. 문자열 참조 검사는 전체 실행 경로 분석이 아니며 외부 모드/다른 버전/저장 데이터의 적용을 배제하지 않는다. 리플레이서 두 BSA에는 PEX가 없었다.
- 원본 Sofia body/hand DDS와 Feris의 커스텀 body/hand DDS 항목은 각각의 BSA에 있다. DDS 이름 존재만으로 내용/디코드/GPU 로딩 성공을 보증하지 않았다. Recorder/Aniya의 해당 설치 자료는 찾지 못했다.

## 읽기 전용 진단 빌드

공개 `Body Change NG v1.3.6 Source.zip`을 별도 디렉터리에 추출하여 로그 패치만 적용한 DLL이다. 진행 중인 슬라이더/오버레이 수정은 포함하지 않는다. SHA-256 기준 소스 ZIP은 `FB0E9E323672901F947ABF9AF4F46A65DA7BA5370383658DD879BF77B4A9C4D9`다.

XMake `skin_diagnostics`는 기본 false다. 일반 빌드는 진단 호출이 inline no-op이므로 진단용 geometry 순회/리소스 읽기/로그가 없다. 별도 진단 DLL은 다음만 읽는다.

- 기존 NiNodeUpdate의 지연 작업 및 다음 스킨 적용 **전** 상태. 새 후킹, 작업 채널, 타이머, 폼 복제, 세이브 레코드, 지속 캐시는 없다.
- 이 ActorBase의 private ARMA에 연결된 현재 3인칭 body/hands/feet/UBE body 슬롯 32/33/37/53. 해당 BOD2 인덱스는 지원 flat 런타임에서 동일하다. 버전별로 삽입되는 무기 인덱스나 SDK 최대값을 순회 경계로 쓰지 않는다.
- 형상별 native TXST 경로, shader texture-set 경로, material의 diffuse/normal NiTexture 존재와 이름, 현재 폼/텍스처 소유 여부. 형상 포인터는 현재 호출 동안만 사용하고 비동기 보관하지 않는다. 부위당 최대 64개 geometry만 읽는다. 의복 ARMA와 별도 성기/항문 재질은 제외한다.
- 필수 texture 객체가 없을 때만 선택한 캐시 파일의 선두 `DDS ` 4바이트를 BSResource로 읽는다. `dds-magic-present`는 DDS 시그니처가 읽혔다는 뜻이다. 전체 파일 무결성, 지원 포맷/UV, GPU 업로드, 실제 화면 성공을 뜻하지 않는다. diffuse/normal을 별도로 기록한다.

필요 로그는 `BodyChangeNG.log`의 `BCNG skin diagnostic` 줄이다. `pointerOwned=true`, `txstOwned=true`인데 `diffusePresent=false`라면 폼 성공 판정과 실제 material 리소스가 분리됐음을 관측할 수 있다. `shaderDiffuse`, `visibleDiffuse`가 선택 native 경로와 다르면 별도 재질 적용/재생성 경로를 추적할 근거가 된다. 이 값만으로 덮어쓴 특정 모드의 이름을 확정하지 않는다. 늦은 3D 로딩 중의 일시적인 absent도 있으므로 실제 보라색 관측과 시점을 맞춰야 한다.

## 2026-10-08 당시 한계

당시에는 진단 DLL만 마련했으며 확인 누락의 수정과 복구 정책은 미구현이었다. 현재 개발 수정과 검증은 문서 맨 위에 정리했다. 최초 발생을 일으킨 특정 외부 공급자의 식별은 아직 미확정이며, 제보자에게 로그를 요청하는 것을 개발 수정의 전제 조건으로 삼지 않는다.

## 검증

공개 1.3.6 기반 진단 SE/AE DLL 빌드와 production-matcher 반례/대조군 실행을 완료했다. 고정 도구는 XMake 3.1.0, CommonLibSSE-NG v11.0.0 `94faaed0c60eddd8347767f2d4d29a97c93bde8c`, MSVC 14.51.36231, Windows SDK 10.0.26100.0이다. SE/AE `EXCLUSIVE_SKYRIM_FLAT`, VR 끔을 유지했다. 게임에서 실행한 진단 로그는 아직 없으며 이 빌드를 해결 릴리즈로 발표하지 않는다.

공식 RaceMenu `ShaderUtilities.cpp`도 texture-set 경로 갱신과 실제 `LoadTexture`/material texture 할당을 별도로 수행한다. 경로가 저장됐다고 실제 리소스 성공을 보증할 수 없다는 공개 구현 근거다: https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/ShaderUtilities.cpp

최종 검사: 일반 빌드 `skin_diagnostics=false` 성공, 49/49 회귀 실행 파일 성공, 일반 DLL에서 진단 시작/상태 문자열 제외 확인, 진단 ZIP CRC 및 DLL 해시 검증, 의존성 lock 원문 보존, `git diff --check` 성공, 공개 1.3.6 배포/소스 ZIP 해시 불변.
