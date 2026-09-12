# 2026-09-10: 보라색 스킨·오버레이 누적·색상 UI 시험판

공개 릴리즈 아님. 게임 화면 검증 전이다.

## 확인한 사실

- 직전 툴레드 DLL `1C1D08E5…`에서 몸은 생성되지만 스킨 선택 후 보라색으로 표시됨.
- 실제 overwrite/texture cache의 skin DDS 112개는 DDS magic 및 124-byte header 검사에 통과. 파일이 없다는 결론이나 GPU 로딩 성공이라는 결론은 내리지 않음.
- 02:44 로그에서 얼굴 페인트 선택이 `Face [Ovl0]`, `Ovl1`, `Ovl2` 등으로 이동. stored/live 성공 로그는 새 페인트 적용만 확인했으며 이전 페인트 제거를 보증하지 않았음.
- 코드가 이전 미리보기 제거/비동기 Revert를 예약한 직후 빈 슬롯을 검색하고 있었음. RaceMenu 초기화가 완료되기 전의 기존 Geometry는 사용 중으로 판정되어 새 슬롯이 선택될 수 있었음.

## 스킨 수정

- 일반 Body/Hands/Feet/Face는 Native Skin Armor/Face TXST만 사용. 일반 스킨에 오버레이 API나 Live Material 재칠을 추가하지 않음.
- `NativeTextureData.h`: TXST의 `TESTexture::textureName`에 경로를 기록하고 해당 `BSResource::ID`도 새 경로로 생성. 비어 있는 선택적 맵은 이름과 ID를 함께 초기화.
- 공식 SKSE [PapyrusTextureSet.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusTextureSet.cpp)의 form texture member 직접 기록 경로를 대조함. 종전의 shader-level SetTexturePath 호출이 form 갱신까지 보장한다고 가정하지 않음.
- getter readback까지 확인하고 다르면 적용 성공으로 보고하지 않음.
- 현재/원경/얼굴 그래프를 변경하기 전에 모든 선택 DDS를 게임의 `BSResourceNiBinaryStream`으로 열어 magic을 확인. 같은 캐시 경로는 이번 적용에서 한 번만 검사.
- 새 로그 `BCNG TXST path verified … engineDDS=true`, `TXST DDS unavailable to engine`, `TXST readback mismatch`로 경로 기록과 MO2/게임 리소스 로딩을 구분.
- EXE의 가상 함수 포인터 주소는 확인했으나, 설치 EXE 코드가 패킹되어 있어 종전 setter의 실제 함수 본문이 no-op이라고 확정하지는 않음. 이번 스킨 변경의 화면 결과는 재시험 필요.

## 오버레이 수정

- 미리보기 A → B → C에서 현재 미리보기의 동일 소유 슬롯을 교체. 삭제 예약 뒤 다른 빈 슬롯을 찾지 않음.
- 지연 적용 전에 다음 클릭이 들어오면 아직 화면에 남아 있는 이전 소유값을 확인. 확인되지 않는 슬롯은 다른 슬롯으로 누적시키지 않고 ownership conflict로 처리.
- 다른 모드의 페인트는 삭제하지 않음. 과거 실패 시험판이 이미 남긴 소유권 불명 페인트를 일괄 삭제하는 migration도 하지 않음.
- 목록 하단 색상 미리보기, 색상·투명도 조절, 색상 복원 추가. 한/영/중 UI.
- 팝업은 선택 당시 액터/부위에 한정. ESC/완료/닫기 처리, 메뉴 종료·탭 변경 시 상태 초기화. 기존 틴트 팝업의 완료 버튼도 실제 popup close 수행.
- 정상 적용된 페인트의 색상 조절은 tint/alpha 키만 갱신. DDS 재로딩이나 새 노드 할당 없이 100ms 간격으로 합침. 투명도 0도 유효한 선택.
- ASTR v4는 v3 104-byte 데이터 뒤에 부위별 AARRGGBB 16-byte를 추가한 120-byte 레코드. v1/v2/v3 읽기 유지, 구버전 색상은 흰색/불투명. 바디/스킨/후타/NPC 상태는 기존 prefix 보존.

## 검증 및 설치

- SE/AE Release build 통과, VR 없음.
- 전체 17개 테스트 실행 통과. TXST 슬롯 기록/빈 맵/원본 격리, 동일 미리보기 슬롯 교체, 색상/투명도 0, 부위별 v4 저장·복원, v3 migration 및 잘못된 문자열 인덱스 검사 추가.
- 공식 RaceMenu 이력 8개 commit의 사용하는 interface prefix/인자/반환형 감사 재통과. 이는 실제 모든 게임 버전의 렌더링 검증과 다름.
- `git diff --check` 통과.
- 시험 DLL SHA256: `129E61D4E66183AE69AED66AD90A6D7C48D88D941D0855A0FD3E9ABE1A683E24`.
- 툴레드 `D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll`만 교체. 사용자 허용대로 별도 MO2 백업 생성하지 않음. 설정/규칙/DDS/스크립트는 그대로.
- 떼껄룩 설치본과 GitHub 공개 릴리즈는 이번 변경으로 갱신하지 않음.

## 게임 재시험

1. 가능하면 누적 문제가 발생하기 전 세이브를 로드하고 스킨 하나 선택: 보라색 여부, 몸/손/발/얼굴을 각각 확인.
2. 다른 스킨 → 기본값 복원 → 저장/종료/로드. 새 로그의 경로 readback과 engineDDS 검사 확인.
3. 같은 부위 오버레이 A → B → C: 교체되는지, 빠르게 선택해도 추가 누적되지 않는지 확인.
4. 색상/투명도 0 및 중간값 → 다른 페인트 → 저장/종료/로드: 동일 부위 값 유지 확인. 다른 액터·부위 값은 독립이어야 함.
5. 색상 팝업 완료/ESC/X 후 메인 UI 입력, 틴트 팝업 입력과 일반 스킨 기능이 독립인지 확인.

오프라인 테스트는 실제 Skyrim 3D·GPU·MO2를 실행하지 않는다. 재시험 결과 없이 스킨/오버레이 문제가 완전히 해결되었다고 판단하지 않는다.
