# 얼굴 NiOverride 전환 시험본 — 2026-09-11

## 적용 범위

사용자가 확정한 범위는 **몸·손·발 유지, 얼굴만 RSV 방식의 NiOverride**다.
ESP, 새 Papyrus 스크립트, 스킨팩 profile.json을 추가하지 않는다.

- 몸·손·발: 기존 Native Skin Armor/ARMA/TXST 유지.
- 얼굴: 실제 Face HeadPart 이름을 대상으로 공개 NiOverride 노드 텍스처 API 사용.
- 성기 SOS/TNG: 기존 Live Material Adapter 유지.
- 페인트 오버레이 및 기존 플레이어 틴트마스크 기능: 별도 상태/백엔드 유지.
- 얼굴 키 9의 채널 0·1·2·3·7만 처리. 틴트 채널 6 및 팩의 tintmasks 제외.

RSV 3.4.3의 `RSV_MainQuestScript.ChangeHeadTexture` 및 플레이어/NPC 호출부와
설치된 `NiOverride.psc`를 기준으로 했다. 플레이어는 persist=true,
NPC는 persist=false이며, NPC 선택값 자체는 BCNG의 기존 ASTR v4에 저장한다.
API 이름은 AddNodeOverrideString, GetNodeOverrideString,
GetNodePropertyString, RemoveNodeOverride다. 페인트 노드를 추가하는 방식이 아니다.

## 비동기 호출과 수명 관리

C++에서 공개 Papyrus 함수를 VM에 요청하고 완료 응답을 받는다. 별도 작업
스레드에서 엔진 재질 포인터를 직접 쓰는 구현은 아니다. 새 private SKEE
C++ vtable ABI나 게임 얼굴 함수 주소 후킹은 사용하지 않는다.

- 액터별 진행 중 배치 하나와 최신 요청을 관리한다.
- 이전 세션 epoch 또는 이전 활성 generation의 콜백은 새 작업을 종료하지 못한다.
- 저장소는 ActorHandle, ID, 문자열을 보유하며 Actor/Geometry/Material raw pointer를 보관하지 않는다.
- 콜백의 배치 참조는 호출 완료 시 해제된다. 액터 언로드 시 런타임 요청을 제거한다.
- 성공한 기본값 복원 후 해당 얼굴 복원 기록을 제거한다.
- 새 세션 초기화에서 작업 저장소와 불필요한 복원 저장소의 할당까지 반환한다.
- 로드 직후 기능 초기화는 방금 불러온 복원 기록을 보존한다.
- 저장/런타임 항목 수 상한은 기존 액터 레지스트리와 동일한 16,384개다.
- 적용 후 실제 DDS 경로를 읽어 비교한다. 응답 불일치 시 완료로 표시하지 않는다.

복원 정보는 별도 **FCNI v1** 레코드다. 원래 화면 DDS, 기존 저장 키,
이전 BCNG 키 및 실행 중인 다음 키를 함께 보관한다. 저장이 쓰기 전후 어느
쪽에서 일어나도 두 BCNG 키를 식별하도록 했다. 기본값은 다른 공급자가
나중에 넣은 별도 키를 삭제하지 않는다. 빈 원래 detail 채널도 복원 대상이다.

## 제거한 이전 구현

- NativeFaceAttachment.cpp/.h 및 NativeFaceAttachmentPolicy.h
- NativeFaceRefresh.h 및 이전 얼굴 갱신/감사 호출
- NativeAppearanceRuntime.h, NativeFaceCallPattern.h, NativeFunctionBoundary.h
- NativeFaceAttachmentTests.cpp 및 위 후킹에만 필요한 테스트/프로브 함수
- verify-appearance-compat.py의 폐기된 얼굴 후킹 검증 도구
- Face TXST 복제·대입·복원 포인터와 이를 이유로 몸 그래프를 재복제하는 추적
- FaceDetailBaseline 및 face_attachment_trial 빌드 분기

FormDelete/오버레이에 쓰이는 공용 코드와 조사 도구는 유지했다. 기존 조사
문서는 과거 결과로 남으며 현재 구현 설명은 이 문서를 따른다. 개발용 조사
스크립트는 게임 DLL에 포함되거나 자동 실행되지 않는다.
삭제된 새 얼굴 후킹 파일들은 미추적 상태였으므로 Git만으로 직접 복원할 수
없다. 별도 소스 백업을 만들지 않았으며 기존 조사 문서는 보존했다.

## 실제 BnP 폴더 확인

읽기 전용 검사 대상:
`D:/TuLED13E/File Mod Skyrim SE/mods/Body Changer NG Skin_Tint/BodySkin`

| 팩 | 일반 얼굴 채널 수 | 흡혈귀 채널 수 | detail 후보 수 |
|---|---:|---:|---:|
| BnP female skin 4k (CBBE Player and Replacer) | 4 | 4 | 6 |
| BnP female skin 4k (UNP Player and Replacer) | 4 | 4 | 6 |
| BnP male skin 4k (SOS full Player Replacer) | 4 | 2 | 6 |

세 팩 각각 인간형 종족 8종 × 노인 여부 × 흡혈귀 여부를 검사했다.
총 **96개 조합 통과**. 종족/노인/흡혈귀 전용 채널이 없으면 선택팩의 일반
DDS가 남으며 다른 팩에서 가져오지 않는다. 발 전용 DDS가 없는 이 팩들은
검증된 Legacy 몸 아틀라스를 발에 사용한다. 틴트마스크 폴더 및 채널 6은
얼굴·몸·손·발 적용 계획에서 제외된다. 기존 자동 ID는 변경하지 않았다.

## 검증 결과와 남은 한계

- SE/AE 전용 Release DLL 빌드 통과. VR 빌드 없음.
- 현재 xmake에 등록된 전체 20개 테스트 실행 통과.
- 얼굴 정책/FCNI 저장·읽기, 잘린 레코드와 과대 개수 거부, 반복 선택의 원본
  유지, 이전 세션/언로드 콜백 구분 테스트 포함.
- 몸·성기 경로의 NiOverride 금지, 얼굴에서만 공개 API 허용, 틴트 제외,
  폐기 파일 재등장 방지 검사 통과.
- 실제 BnP 폴더 검사는 카탈로그/계획 검증이다. 렌더링·VM 실행 검증이 아니다.

인게임 실행, 장시간 메모리 계측, 저장 후 완전 종료/재로드 및 모든 RaceMenu
버전의 실행 검증은 아직 하지 않았다. 특히 활성 RSV의 지연 얼굴 재적용이
BCNG 뒤에 실행되는 경우의 우선순위는 이 변경만으로 보장하지 않는다.
RSV와 같은 API 방식이라는 것과 두 공급자의 동시 실행이 무충돌이라는 것은
별도 검증 사항이다. 이미 VM에 전달된 호출 자체는 취소할 수 없으며 완료 후
후속 동작을 세대 검사로 차단한다.

시험 DLL: `build/v1.2.0/trials/face-ni-override/windows/x64/release/BodyChangeNG.dll`.
SHA-256: `26273345881635595D95DBA9361C18A1FAAD13F113258FC71880BFE9B4AA3833`.
툴레드/떼껄룩 MO2 파일 교체 및 공개 Release/GitHub 갱신은 하지 않았다.
