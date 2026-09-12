# 얼굴 보라색 재현: 읽기 전용 진단 시험본

## 직전 시험 결과

2026-09-12 08:52~09:03 툴레드 로그: 순서 수정 시험본 A6FDB4A...에서
갱신 dispatch/event 각 64회, 얼굴 batch 시작 63회, 완료 로그 53회.
얼굴 경로 불일치/필수 텍스처 없음/트랜잭션 실패 경고는 모두 0건이다.
사용자는 반복 선택 약 10회 중 1~2회 보라색을 보고했다. 중단된 batch 10개와
보라색 발생이 같은 사건이라는 증거는 없으므로 이를 원인으로 단정하지 않는다.
직전 로그는 build/audits/face-purple-20260912/BodyChangeNG.log에 보존했다.

## 이번 변경은 진단만

- FaceSkinDiagnostics.cpp/.h: 플레이어의 실제 Face HeadPart 이름으로 현재
  3D의 얼굴을 매번 새로 찾고, shader feature/주소 및 채널 0/1/2/3/6/7을 관찰한다.
- 각 채널에 desired DDS, 실제 TextureSet 경로, NiTexture 이름/타입/주소,
  NiSourceTexture인 경우 renderer/resource/SRV 포인터, 크기/mip/format을 기록한다.
- FaceGen(4)의 tint/detail/subsurface 포인터와 FaceGenRGBTint(5)의 RGB 색상은
  별도 분기한다. RGB 값을 텍스처 포인터로 해석하지 않는다.
- NiRenderedTexture 등 소스 텍스처가 아닌 객체는 확인되지 않은 renderer
  레이아웃을 읽지 않는다. `gpu-inspected=false`는 GPU 고장이 아니라 미검사다.
- GPU API 호출, 픽셀 readback, 텍스처 로딩/생성/교체, 셰이더 초기화가 없다.
  따라서 resource/SRV가 있어도 실제 색상이나 렌더 정상임을 증명하지는 않는다.
- before-face, 채널별 readback, complete/incomplete/superseded를 기록한다.
  완료한 최신 선택만 2 input tick 뒤, 그 후 30 tick 뒤 두 번 더 관찰한다.
  시간 초 단위 보장이 아니며, 선택 변경/세션 변경/재구축/액터 해제 시 건너뛴다.
- Geometry/Material/Texture/GPU 포인터는 콜백 안에서 읽기만 하고 저장하지 않는다.
  지연 작업은 actor ID/epoch/generation 값만 캡처한다. 영구 polling/map을 추가하지 않는다.
- 진단은 `face_skin_trace` 빌드 옵션일 때만 활성화한다. 별도 비대화형 큐 채널
  skinFaceDiagnostic=215를 사용하며 스킨/오버레이 작업을 대체하지 않는다.
- 얼굴 적용 조건, 성공 판정, 기본값 복원, 저장 형식, 일반 스킨/성기/오버레이
  적용 방식을 이번 진단 작업에서 바꾸지 않았다. 로깅 자체의 타이밍 영향은 가능하다.

## 빌드와 검사

기존 고정 의존성, Release, SE/AE FLAT, VR 비활성화를 유지한다.
최종 구성은 아래 세 옵션을 모두 명시한다. xmake 재설정 때 기존 trial 옵션이
빠지지 않도록 최종 설정 및 생성 DLL의 진단/guard/addon 문자열도 확인했다.

```
xmake f -m release --form_delete_guard_trial=y --native_addon_txst_trial=y --face_skin_trace=y
xmake build BodyChangeNG
```

- 최종 진단 활성 빌드 성공. 비활성 빌드도 컴파일 확인.
- 전체 C++ 테스트 실행 파일 21개 빌드/실행 통과.
- tools/test-addon-audits.py 18개 통과.
- 진단 필드 분리, 모르는 shader 제외, 오래된 세션/선택의 지연 관찰 제외 테스트 추가.
- 소스 감사에 진단 모듈의 타입 확인/플레이어 제한 및 쓰기 API 부재 검사 추가.
- 이는 실제 엔진/렌더 검증이나 보라색 해결 결과가 아니다.

## 설치와 다음 재현

- 산출물: build/v1.2.0/trials/face-texture-trace/windows/x64/release/BodyChangeNG.dll
- SHA-256: C168EB6BD4126F20FC756B83B223BE26A31C380E0FC62F2944724C27093E8E40
- 툴레드 MO2의 Body Change NG/SKSE/Plugins/BodyChangeNG.dll에 복사 후 해시 일치 확인.
- 설정, 스킨팩, 세이브, 떼껄룩 설치본, 공개 GitHub Release는 변경하지 않았다.

보라색 재현 시 클릭을 멈추고 몇 초 유지한 후 스크린샷/마지막 선택 항목을 남긴다.
`FaceGPU` 로그에서 동일 generation의 정상/실패 상태와 시간 경과를 비교한다.
특히 `name-matches-desired=false`도 중간 채널 적용 전이나 원래 경로 없는 생성
틴트에서는 정상일 수 있으므로 단독으로 실패 판정하지 않는다.
