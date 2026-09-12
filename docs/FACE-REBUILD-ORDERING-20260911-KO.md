# 얼굴 적용 / 3D 갱신 순서 시험본 (2026-09-11)

## 범위와 확인 수준

사용자 재현: 같은 팩도 처음에는 정상이며 여러 스킨을 선택하다 보면 얼굴만
보라색이 된다. 특정 팩의 DDS 손상으로 단정하지 않는다. 직전 실행의 얼굴
캐시 DDS 69개는 Pillow 디코딩을 통과했지만, 이는 게임의 GPU 로딩 성공을
증명하지 않는다.

기존 코드는 얼굴 Papyrus 호출 체인을 시작한 직후 몸의 QueueNiNodeUpdate를
요청하고, NiNodeUpdate 이벤트에서 얼굴 체인을 다시 제출했다. 이번 변경은
이 BCNG 내부 순서 문제를 제거하는 시험 수정이며 보라색 현상의 확정 원인이나
실제 게임 해결을 입증한 결과가 아니다.

## 변경

- NativeSkinBackend는 표시 중인 액터의 얼굴 선택/기본값 복원을 먼저 대기
  상태로 제출한다. 몸·손·발 Native TXST 변경 방식은 유지한다.
- SkinApplication의 기존 단일 QueueNiNodeUpdate 경로에 액터별 순서 제어를
  연결한다. 이미 제출된 얼굴 호출의 콜백이 돌아온 다음 재구축을 요청한다.
- 요청 수락을 재구축 완료로 취급하지 않는다. NiNodeUpdate 이벤트를 받아야
  얼굴 작업이 진행된다. 무조건 몇 프레임 뒤 적용하는 fallback은 추가하지 않는다.
- 이벤트 수신 시 즉시 이전 얼굴 generation을 무효화한다. 실제 얼굴 접근은
  게임 스레드에서 최신 선택값으로 수행한다. 기존 지연 Reapply 제출은 삭제했다.
- 재구축 중 선택 변경은 최신 요청 하나로 합친다. 이미 진행 중인 재구축이
  끝나면 필요한 다음 재구축을 진행하고 그 뒤 얼굴을 적용한다.
- 기본값 복원도 동일 순서로 진행한다. 얼굴 선택이 없는 성기 전용 갱신은
  빈 얼굴 선택을 만들지 않으며 완료 후 임시 상태를 제거한다.
- 같은 몸의 얼굴만 재시도할 때는 불필요한 새 재구축을 요청하지 않는다.
- 세이브 데이터 형식은 변경하지 않았다. 대기 중인 호출/순서 제어 상태는
  직렬화하지 않고 세션 초기화/액터 해제 시 제거한다. 얼굴 원본 복원 정보와
  선택값은 기존 FCNI/ASTR 경로를 유지한다.
- 새 상태에는 Geometry/Material 포인터를 저장하지 않는다. ActorHandle,
  generation, 값 타입 상태와 한 개의 대기 dispatch만 보관한다.
- 앞서 요청한 스킨 목록의 Humanoid 등 종족 분류 문구 제거도 빌드에 포함된다.

변경 파일: FaceSkinOverrides.cpp/.h, FaceSkinPolicy.h, NativeSkinBackend.cpp,
SkinApplication.cpp, ActorEvents.cpp, FaceSkinPolicyTests.cpp,
TexturePathIsolationTests.cpp. UI.cpp의 문구 삭제는 직전 작업이다.

## 검증

- Release 빌드 성공. 저장소에 고정된 xmake 3.1.0 / CommonLibSSE-NG 6.7.1,
  EXCLUSIVE_SKYRIM_FLAT SE/AE 설정을 그대로 사용했다.
- 전체 xmake 테스트 실행 파일 21개 빌드/실행 통과.
- tools/test-addon-audits.py: 18개 통과.
- 새 테스트: 진행 중 얼굴 호출과 재구축의 배타성, dispatch 이전 적용 금지,
  재구축 중 100회 요청 합치기, 이전 완료 이벤트 이후에도 최신 재구축 대기,
  기본값 복원, 다른 액터 독립성, 완료한 얼굴 중복 실행 방지, 세션 초기화.
- 소스 경로 감사는 단일 facade 갱신 경로와 새 순서 제어 연결을 확인한다.
  최초 실행에서 이 감사가 이전 raw pointer 호출 문자열을 기대하여 실패했으며,
  ActorHandle을 해석한 actor.get() 호출 및 순서 제어까지 검증하도록 수정 후
  전체 테스트를 다시 통과했다.

## 아직 필요한 게임 검증

- 위 테스트는 엔진/Papyrus/렌더 통합 테스트가 아니다. 모든 RaceMenu/게임
  버전에서의 표시 성공이나 타 모드가 만드는 모든 이벤트 순서를 증명하지 않는다.
- SKSE 이벤트는 개별 Papyrus 요청의 ID를 전달하지 않는다. 외부 모드의 갱신이
  동시에 들어오는 상황은 실제 실행 순서 확인이 필요하다.
- 재구축 요청 실패는 오류로 남기며 얼굴 성공으로 표시하지 않는다. 엔진 이벤트가
  오지 않으면 시간만으로 대기를 해제하지 않는다. 무응답 여부도 시험 대상이다.
- 로그의 `rebuild barrier dispatch` → `rebuild barrier event` → `face batch start`
  및 최신 profile을 확인해야 한다. 기존 `paths/required textures verified`도
  최종 화면이나 모든 텍스처 채널의 GPU 상태를 보장하지 않는다.
- 플레이어/NPC 반복 선택, 기본값 복원, 저장/로드를 실제 게임에서 확인해야 한다.
- ERF 미적용과 BnP CBBE 그래프 거부 문제를 해결했다고 주장하지 않는다.

## 산출물

- DLL: build/v1.2.0/trials/native-addon-txst/windows/x64/release/BodyChangeNG.dll
- SHA-256: A6FDB4A2629BC6DB836DF6DB088567EDAE413E20581AEA1AEFB8E147A3C66BD6
- 이번 작업에서는 두 MO2 설치본과 공개 Release를 변경하지 않았다.

참조: [RaceMenu ShaderUtilities.cpp](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/ShaderUtilities.cpp),
[SKSE 2.0.20 PapyrusActor.cpp](https://github.com/ianpatt/skse64/blob/v2.0.20/skse64/PapyrusActor.cpp).
로컬 CommonLibSSE-NG AIProcess::Update3DModel은 엔진 함수 반환 후
NiNodeUpdateEvent를 전송하는 경로를 명시한다.
