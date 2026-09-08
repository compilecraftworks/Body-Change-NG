# Body Change NG 1.2.0 릴리스 노트

1.2.0은 별도로 관리한 네이티브 스킨 아키텍처입니다. BodySlide 체형 변경은
계속 RaceMenu BodyMorph를 사용하고, 일반 바디스킨은 액터의 현재 네이티브
TXST → ArmorAddon → Skin Armor 그래프를 전용으로 복제해 NPC ActorBase에
연결합니다.

## 업데이트 주의사항

- 1.1.4와 1.2.0 DLL을 함께 두지 마세요.
- 개인 `BodySkin`, `Futanari`, `TintMask`, BodySlide 프리셋 폴더를 메인 모드
  안에 보관했다면 업데이트 전에 보존하세요.
- 모드를 교체하기 전에 `SKSE\Plugins\BodyChangeNGdistribution.json`을
  보존하세요. 설정·배포 규칙 JSON은 계속 지원합니다.
- 스킨팩별 `profile.json`은 더 이상 지원하거나 읽지 않습니다. 스킨 ID는
  기존 상대 폴더 기반 자동 ID입니다. 명시적인 `!UBE` 텍스처 구조만 UBE이고
  나머지 일반 인간형 텍스처 트리는 Legacy입니다.
- 바디 선택, 스킨 선택, 의상 변경, 저장·불러오기, NPC 배포를 사용할 세이브에서
  확인할 때까지 이전 DLL 백업을 보관하세요.

## 호환 범위

- Skyrim SE 1.5.97과 검증된 Skyrim AE 1.6.x 레이아웃(1.6.1179까지)
- 게임 버전에 맞는 SKSE64, Address Library, RaceMenu
- CBBE 3BA, BHUNP/UNP, UBE 2.0, HIMBO, SAM, 바닐라 인간형 바디
- 아르고니안·카짓 스킨 종족 경계
- RSV 공급자 재기반과 제한된 얼굴 복구
- 독립된 SOS/TNG/TRX/ERF 외부 성기 스킨 어댑터
- Skyrim VR 빌드 없음

## 검증 결과

- Release 플러그인 빌드 통과
- 자동 회귀 테스트 실행 파일 15종 전체 통과
- 한글 스킨팩 폴더와 `FemaleHeadDetail_Age40.dds`가 함께 사용된 실제 툴레드
  크래시 조건을 UTF-8 경로 회귀 테스트로 재현해 통과
- 떼껄룩·툴레드에 설치된 OBody JSON 공급자 6개로 배포/OBody NG ORefit 파서를 점검했고,
  읽힌 의상 이름·플러그인 제외와 강제 보정 항목 전체를 플러그인과 같은 판정
  경로로 확인했습니다. 등록 시 현재 로드된 모든 액터를 즉시 다시 판정합니다.
- 실제 TAKEALOOK 자산 트리에서 BodySkin 11개: Legacy 7개, UBE 4개
- 스킨 DDS 139개 매핑, 무관 DDS 23개 제외, Tint 85개와 Futanari 10개 유지
- 설치 ZIP 승인 항목만 포함, 스킨팩 `profile.json` 0개

## 배포 파일

- `Body Change NG v1.2.0.zip` — MO2 설치용
- `Body Change NG v1.2.0 Source.zip` — 대응하는 빌드 가능 소스, 고정 의존성,
  스크립트, 라이선스, 릴리스 문서
