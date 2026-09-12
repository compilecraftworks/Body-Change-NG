# Native TXST 경로 및 오버레이 소유권 시험판 — 2026-09-10

## 확인한 실패

- 이전 `A7F9FE96…` 시험판은 슬롯 readback/DDS header 검사에 통과했지만 실제 화면에서 다시 보라색 피부가 나타났다. 기존 테스트 통과는 게임 렌더링 성공이 아니었다.
- 실행 중인 툴레드 Skyrim SE 1.5.97의 코드 영역을 읽기 전용으로 확인했다. 프로세스 메모리 수정·주입·엔진 함수 실행은 하지 않았다.
- `BGSTextureSet` secondary vtable slot 0x26, RVA `0x2D1B90`: getter 반환값 앞에 상수 `Data\Textures\`를 붙여 로더를 호출한다. 상수 RVA `0x1578960`도 직접 읽어 확인했다.
- BCNG는 TXST에 `textures\BodyChangeNG\Cache\...`를 기록했다. 실제 요청은 `Data\Textures\textures\BodyChangeNG\Cache\...`가 되어 사전 검사와 다른 파일을 찾았다.
- 이 실행 코드 검증은 SE 1.5.97에 대한 증거이며 AE 전체 바이너리를 실행 검증했다는 의미가 아니다. 시험 DLL에 위 RVA나 프로세스 읽기 코드를 넣지 않았다.

## 수정

- `NativeTexturePath.h`: 캐시 출력에서 Native TXST용 `BodyChangeNG\Cache\...`와 BSResource용 `textures\BodyChangeNG\Cache\...`를 분리한다.
- DDS preflight와 실제 TXST 기록이 같은 변환 결과를 사용한다. 잘못된 루트, 경로 이탈, 절대경로, 빈 구성요소, 로더 버퍼 제한 초과를 거부한다.
- 원본 provider의 TXST 경로, 캐시 파일명, 스킨 ID, 선택 저장 형식은 변경하지 않는다. 일반 스킨은 Native TXST만 사용한다.
- 로그의 `engineDDS=true`를 `native=... resource=... DDSHeader=true`로 변경해 헤더 검사와 렌더링 성공을 혼동하지 않도록 한다.

## 오버레이 조사 및 한계

마지막 게임 로그에서 03:44:09/10에 Body [Ovl1]의 HeartPubeS → HeartPubeM은 stored/live 검증을 통과했다. 03:44:11 이후 선택과 색상 변경은 result=6(ownershipConflict)으로 차단됐다.

- `OverlayPolicy.h`에 비교 전용 경로 identity를 추가했다. 동일한 텍스처의 texture-root-relative / Textures-relative / Data\Textures 경로 표기를 일치시킨다. 실제 provider에 쓰는 문자열 형식은 변경하지 않는다.
- 파일명만 비교하지 않는다. 다른 폴더의 동일 파일명, 예상하지 않은 companion map, 중복 textures 루트는 일치로 취급하지 않는다. 무조건 슬롯을 빼앗거나 새 슬롯으로 우회하지 않는다.
- `RaceMenuOverlay.cpp`의 저장값 및 라이브값 비교에 적용했다. 교체/지연 확정이 막히면 액터, 노드, 예상 packed path, 실제 8개 저장 texture 값, tint/alpha 키 존재 여부를 기록한다.
- 기존 실패 로그는 실제 불일치 값을 기록하지 않았다. 따라서 **이번 경로 비교 수정이 관찰된 오버레이 정지의 원인을 모두 해결했다고 확정하지 않는다.** 재시험으로 확인해야 한다.

## 검증

- Release SE/AE 빌드, 전체 17개 테스트 통과. VR 비활성, 의존성 핀 유지.
- Native 텍스처 경로 조합/DDS 검사와 로더 대상 일치, 부위/맵 종류, 잘못된 경로 거부 테스트 추가.
- 오버레이 접두사 동치, 다른 팩/companion 보호, 반복 교체 경로 비교 테스트 추가. 실제 RaceMenu/게임 렌더러 실행 테스트는 아님.
- 공개 GitHub Release 및 떼껄룩 배포는 갱신하지 않는다. 툴레드용 시험 DLL만 대상.
- Skyrim 종료를 재확인한 뒤 `D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`만 교체했다. 설정·규칙·스킨팩·스크립트 보존.
- 빌드/설치 DLL SHA256 일치: `17506AACC90BDEF0DD9604775F397B991C50F30E771A0C96A547D424C4FC651F`.

재시험: 스킨 A → B → 기본값 복원(몸·손·발·얼굴), 오버레이 A → B → C → A와 색상/불투명도 변경. 오버레이가 다시 멈추면 이번 `BCNG overlay ownership conflict`의 expected/stored 값을 확인한다. 저장/로드 유지와 NPC 실제 화면은 별도 게임 검증이 필요하다.
