# 얼굴 복원·재적용 후속 수정 (부분 완료)

## 변경

- `FaceSkinPolicy.h`: 얼굴 키 소유권을 대소문자, 경로 구분자, Data/Textures 루트
  표기가 아닌 전체 리소스 경로로 비교한다. 서로 다른 팩/캐시 디렉터리는 여전히 구별한다.
  RaceMenu의 [SKEEFixedString](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/StringTable.h)은
  대소문자 비구분 비교를 사용한다. 종전 byte-exact 비교와 맞지 않았다.
- `FaceSkinOverrides.cpp/.h`: 현재 얼굴 요청의 진행 중 여부를 조회한다.
  비동기 콜백 사이에 Geometry/Material 포인터를 저장하지 않는다.
- `NativeSkinBackend.cpp`: 몸 TXST가 이미 적용됐다는 이유만으로 얼굴 실패/대기까지 전체 성공으로
  표시하지 않는다. 얼굴 완료 시에만 성공 처리하고, 진행 중이면 새 요청으로 덮지 않는다.
  얼굴 요청이 실패/누락된 경우 얼굴만 다시 제출하며 몸 3D 재구축은 추가하지 않는다.
- `FaceSkinPolicyTests.cpp`: 경로 표기 변경 후 Default 복원, 다른 팩 경로 보존,
  얼굴 완료/대기/재적용 분기를 테스트한다.

## 검증

- Release 빌드 통과 (`EXCLUSIVE_SKYRIM_FLAT`, SE/AE 시험 구성 유지).
- 전체 XMake 테스트 21개 재빌드/실행 통과, Python 감사 테스트 18개 통과.
- 시험 DLL SHA-256: `F487930437E54AC5CF386560EEF35553CC985F452CAE02D24ED76B220D7A2174`.
- MO2 설치 파일 / 공개 Release는 교체하지 않았다.

## 남은 문제

이 수정으로 사용자 화면의 보라색 얼굴이나 Default 실패가 모두 해결됐다고 주장하지 않는다.
실제 로드된 얼굴 DDS 및 나머지 채널은 여전히 실기 관측이 필요하다.
ERF의 FormID=0 가정과 이후 baseline-unavailable, BnP CBBE의 NIF 전용 재질에 대한
잘못된 그래프 필수 조건은 아직 수정 완료가 아니다. 검사만 지워 부분 적용을 성공으로 만들지 않았다.

ERF의 생성 폼 등록/해제 확인을 위해 툴레드 메인 메뉴 실행을 요청했다.
기존 캡처에서 다음 하위 함수가 빠져 있다: SE TESBoundObject 생성자 `0x2211F0`,
BGSTextureSet 소멸자 본문 `0x2D1290` (삭제 래퍼 `0x2D2140`의 직접 호출 대상).
종료된 실행 파일의 해당 디스크 코드가 패킹되어 있어 디스크 바이트를 실제 실행 코드로
간주하지 않는다. 프로세스 메모리 쓰기/주입/세이브 로드는 필요하지 않다.
