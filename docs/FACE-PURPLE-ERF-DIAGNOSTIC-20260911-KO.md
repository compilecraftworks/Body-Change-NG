# 툴레드 얼굴 보라색 / ERF 미적용 후속 조사

## 보고 및 확인 범위

사용자는 스킨 반복 선택 중 얼굴이 자주 보라색이 되고 후타나리 스킨이 바뀌지 않는다고
보고했다. 테스트한 후타 애드온은 TRX가 아니라 **SOS ERF**다. 남성 SOS/TNG 및 TRX의
실패를 실기로 확인한 것은 아니다. 남성/여성은 같은 NativeAddonSkinBackend를 사용하므로
공통 경로 문제라면 함께 영향받을 수 있으나, 실패 조건은 아직 확정하지 못했다.

이번 로그: Documents/My Games/Skyrim Special Edition/SKSE/BodyChangeNG.log,
실행 시작 2026-09-11 21:50, 마지막 기록 21:59. 게임은 사용자에 의해 종료된 상태에서 조사했다.

- 네이티브 성기 훅 설치 성공 메시지가 있다.
- 21:58:29 이후 ERF용 후타 스킨 선택마다 대상 1개/DDS 4개 게시 및 QueueNiNodeUpdate 요청이 있다.
- 이 메시지는 **선택 데이터 등록**이다. 네이티브 방문자 진입·TXST 전달·렌더 성공을 입증하지 않는다.
- 21:58:38.439 얼굴 channel 1이 선택 캐시가 아니라 기본 FemaleHead_msn.dds를 반환한 기록이 있다.
  그 뒤 성공 메시지가 다시 있다. 이 불일치 하나를 모든 보라색 현상의 원인으로 단정하지 않는다.
- 얼굴 캐시 DDS 69개는 기본 헤더(128 bytes, DDS magic, header size 124)를 통과했다.
  GPU 디코딩, 인게임 VFS 조회, 모든 mip 데이터까지 검증한 것은 아니다.

## 실제 ERF 파일

활성 TuLED(SL) modlist에서 Body Slide Output이 SOS - Futanari Addon CBBE SSE보다
높은 우선순위다. 같은 상대 경로의 NIF 두 개를 모두 읽었다.

- 상대 경로: `meshes/ERF_Futanari/futanari_schlong_cbbe_1.nif`
- 우선 출력 SHA-256: `00ae0b25fcc1a504b9678850700d9c87ca4f3080cda31b85d1eaf6906985ab5d`
- 원본 모드 SHA-256: `0c65bfb07a222ed4041a31ce4b2a5be9471ad2b31529d1ec17718bf1a736c685`
- 양쪽 모두 피부 shader type 5, flags1 `82601303`, flags2 `02008001`.
- 원본 diffuse: `textures/erf_futanari/fairskincbbe/futanari_schlong.dds`.

정적 파일은 네이티브 Skin Tint 조건에 맞는다. 이것만으로 게임 내 훅의 대상 일치와 실행을
보증할 수 없다. 런타임 노드 변경·다른 모드 개입까지 배제한 결과도 아니다.

## 얼굴 코드 수정

RaceMenu 공식 [ShaderUtilities.cpp](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/ShaderUtilities.cpp)의
GetShaderProperty는 저장된 TextureSet 경로를 읽는다. NIOVTaskUpdateTexture는 그 경로를
기록한 다음 LoadTexture 결과를 재질에 대입한다. 따라서 경로가 같아도 실제 로딩 실패를
기존 BCNG 판정이 놓칠 수 있었다.

- FaceSkinPolicy의 CacheOverridePath로 얼굴 NiOverride에 `textures/BodyChangeNG/Cache/...`
  리소스 경로를 전달한다. BGSTextureSet용 Textures-root 상대 경로와 분리했다.
- 읽기 비교 뒤 현재 실제 HeadPart 지오메트리의 diffuse/normal 및 rendererTexture 존재를 확인한다.
  비어 있으면 완료 처리하지 않고 액터·채널·요청 경로를 기록한다. 픽셀 색상 검사와는 다르다.
- 원본 diffuse/normal 경로를 얻지 못한 상태에서는 복원 기준을 확정하지 않는다.
  Default에서도 필수 채널에 빈 경로를 쓰지 않는다. 선택적 detail 채널의 제거는 허용한다.
- 몸·손·발/성기의 TXST 경로, 일반 오버레이 및 틴트 채널 6은 변경하지 않았다.
- 비동기 작업 사이에 새 Geometry/Material 포인터를 보관하지 않는다.

이 변경은 경로 구분과 거짓 성공 판정을 수정한다. **보고된 모든 보라색 현상을 재현·해결했다고
확정한 상태는 아니다.** 원본 데이터, 텍스처 로딩 및 외부 갱신 간섭의 추가 확인이 필요하다.

## 성기 진단 보강

- 호출 총수, 슬롯52 방문수, TXST 공급 시도수를 다음 선택 게시 시 기록한다.
- 최초 선택 전 진단을 비활성화하고, 이후 단계별 상세 로그를 세션당 64건으로 제한한다.
- 슬롯/셰이더/액터 핸들/대상 불일치/기본값 캡처/팩토리/슬롯 변환 실패를 구별한다.
- 방문자 입력은 NiAVObject로 받고 AsGeometry 확인 후 지오메트리 멤버를 읽는다.
- 구 Live Material 구현을 재도입하거나 장비를 강제 탈착하는 변경은 하지 않았다.
- 이번에는 ERF 적용 실패 조건을 우회한 것이 아니라, 기존 로그로 구분할 수 없었던 구간을
  관측하도록 했다. **ERF 미적용 원인은 아직 미확정이다.**

## 결과

- Release 빌드 통과, 전체 XMake 테스트 21개 통과, Python 감사 테스트 18개 통과.
- 새 얼굴 테스트: Native/리소스 경로 분리, 경로 순회 거부, 빈 필수 복원 거부,
  경로만 존재/renderer 없는 텍스처의 거짓 성공 거부, 선택적 채널 제거 허용.
- DLL: `build/v1.2.0/trials/native-addon-txst/windows/x64/release/BodyChangeNG.dll`
- 크기 2,633,728 bytes.
- SHA-256 `1DC3260B1A0755C25210076A310F000D3DBBF0679F3AD3ED5BF4FA58638E69BC`.
- 후속 사용자 승인으로 게임 종료를 확인한 뒤 **툴레드 MO2의 DLL만 위 진단본으로 교체**했고
  SHA-256 일치를 검증했다. 설정/스킨팩/배포 규칙은 변경하지 않았다.
  떼껄룩 설치 DLL은 이전 시험본 그대로이며 공개 Release도 변경하지 않았다.

다음 실기에는 이 진단 DLL로 ERF 스킨 A → B를 선택한 로그가 필요하다. 재빌드 파일이
생겼다는 이유만으로 미적용 해결 또는 남성/TRX 정상 작동을 선언하지 않는다.
