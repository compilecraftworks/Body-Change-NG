# 얼굴 디테일 원본 경로 복원 수정

## 실제 재현 증거

툴레드 SE 1.5.97, 진단 DLL C168EB6B…의 09:36 로그와 보라색 얼굴 스크린샷.
원본 로그: `build/audits/face-purple-20260912-0936/BodyChangeNG.log`.

- 09:36:13.098 / gen4 / BnP UNP 적용 전: 얼굴 slot3의 TXST 문자열은 빈 값이지만
  실제 NiSourceTexture.name은 `data\TEXTURES\actors\character\Female\FemaleHeadDetail_Age40.dds`.
- BnP UNP 적용 후: 선택 팩의 age40 DDS가 실제 detailTexture에 연결됨.
- Diamond를 거쳐 니블A(gen12)로 변경. 니블A에는 해당 detail 대체 경로가 없음.
- 09:36:16.166: 복원 호출 뒤 slot3 경로는 빈 값,
  실제 텍스처는 `BSShader_DefNormalMap`(16×16)으로 바뀜.
- 09:36:17.365 settled-late에서도 동일. 0/1/2/7의 이름은 선택 팩 DDS와 일치.

확정한 버그는 **원본 TXST 문자열만 저장하여 실제 원본 디테일 DDS를 잃은 것**이다.
보라색을 렌더링하는 픽셀까지 GPU readback한 것은 아니므로, 수정 후 시각 검증은 필요하다.
실제 파일 텍스처의 진단 width/height=0은 GPU 로딩 실패의 증거로 취급하지 않았다.

RaceMenu 공식 소스의 NIOVTaskUpdateTexture::Run은 빈 문자열도 LoadTexture에 전달한다.
이는 채널 제거 API가 아니다. GetNodePropertyString은 실제 NiTexture가 아닌 TXST 경로를 읽는다.
참고: https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/ShaderUtilities.cpp

## 수정 범위

- FaceSkinOverrides.cpp: 실제 Face HeadPart의 현재 재질을 읽기 전용으로 관찰하고
  채널별 NiSourceTexture 이름에서 원본 DDS 경로를 저장한다. FaceGen과 RGBTint를 구분한다.
- FaceSkinPolicy.h: Data/Textures 표기 정규화, DDS 파일명과 엔진 기본 텍스처 이름 구분.
  로드된 DDS 이름이 있으면 비어 있거나 오래된 TXST 문자열보다 우선한다.
- 기존 세이브의 빈 slot3 baseline은 원본 NPC/root face NPC/race 얼굴 TXST의
  detail 경로로 보완한다. 현재 BCNG 팩의 DDS를 원본으로 재캡처하지 않는다.
- 원래 저장된 NiOverride 값 또는 나중에 다른 모드가 기록한 값은 기존 우선순위를 유지한다.
- 빈 경로의 NiOverride 쓰기는 금지한다. 실제 적용 확인에도 텍스처 이름 대조를 추가한다.
- 원본 디테일 폼에도 경로가 없는 특수한 기존 baseline은 추측한 DDS를 넣지 않는다.
  복원 미완료 경고와 baseline을 남기며, 성공으로 처리하거나 빈 텍스처를 로드하지 않는다.

몸·손·발 Native TXST, 성기 TXST, 오버레이, 틴트마스크 적용 방식은 변경하지 않았다.
새 재질 복제/직접 수정/엔진 후킹/추가 지연/재시도 루프가 없다.
새 관찰 함수는 문자열만 반환하며 geometry/material/texture 포인터를 저장하지 않는다.
baseline 저장 형식은 그대로이고, 세션 초기화 및 액터 정리 수명도 변경하지 않았다.

## 검증

- Release 빌드 성공, EXCLUSIVE_SKYRIM_FLAT / VR 비활성화 유지.
- form_delete_guard_trial / native_addon_txst_trial / face_skin_trace 모두 활성화 유지.
- 전체 C++ 테스트 타깃 21개 빌드 및 실행 통과.
- Python addon 감사 테스트 18개 통과.
- 빈 TXST+실제 age40 DDS, 반복 100회 원본 유지, 외부 provider 우선순위,
  원본 detail 경로 직렬화 왕복, 기본 텍스처 이름/부적합 경로 거부 테스트 추가.
- 소스 감사에 빈 쓰기 방지, 실제 원본 경로 캡처, 재질 직접 수정 부재 검사 추가.
- 위 테스트는 실제 게임 반복 선택/렌더 테스트를 대체하지 않는다.

시험 DLL: `build/v1.2.0/trials/face-texture-trace/windows/x64/release/BodyChangeNG.dll`

SHA256: `2E43BDFE4006F365BAE431FE0349ED829A669C4CA0D6D4EFC857397FAC1D9A6F`

게임 종료를 확인한 뒤 툴레드 MO2의 `Body Change NG/SKSE/Plugins/BodyChangeNG.dll`에
반영하고 위 해시 일치를 확인했다. DLL 이외 설정/팩/세이브와 떼껄룩 및 공개 Release는 변경하지 않았다.

다음 인게임 확인: BnP UNP → 니블A / Diamond → 기본값을 반복하여
얼굴의 디테일 복원과 보라색 재발 여부를 확인한다. BnP CBBE 전체 적용 실패와
ERF 성기 적용 실패는 별도 원인으로, 이 수정에서 해결했다고 주장하지 않는다.
