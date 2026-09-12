# TXST 슬롯 대응 후속 시험판 — 2026-09-10

## 실제 실행 증거

이전 시험 DLL `129E61D4…`의 03:09:09–03:09:33 툴레드 로그:

```text
BCNG TXST readback mismatch form=FF002F47 index=2
expected='Actors\Character\MaleChild\UpperBodyMale_sk.dds' actual=''
Body Change NG could not clone the native Skin Armor graph for actor 00000014
```

같은 지점에서 스킨 선택이 반복 중단됨. 보라색이 사라진 것은 새 스킨 적용 성공이 아니라, 잘못 기록된 복제 그래프를 붙이지 않고 원래 스킨을 유지한 결과였다. 3D 갱신 이후의 렌더링 실패라고 설명하지 않는다.

첫 자식용 ARMA는 Skin Armor가 가진 여러 종족용 ARMA 중 먼저 복제된 항목이다. 이 로그 자체는 플레이어 종족이 아동 종족으로 변경되었다는 뜻이 아니다.

## 수정

- `NativeTextureData.h`: 내부 TESTexture 배열 순서와 `BSTextureSet::GetTexturePath`의 슬롯 순서를 동일하다고 가정하던 부분 제거.
- 새로 만든 **아직 연결되지 않은 비공개 TXST**의 8개 텍스처 이름에 구분 가능한 임시 문자열을 넣고 실제 엔진 getter가 읽는 배열 항목을 확인. 1:1 대응만 수용한다.
- 임시 문자열은 성공/실패 모두 scope 종료 시 복원한다. 이 단계는 원본/연결된 TXST, Geometry, Material을 수정하거나 DDS를 로딩하지 않는다. 추가 임시 Form도 생성하지 않는다.
- 대응표는 해당 TextureBinding에 값으로 보관. 생성 시 한 번 확인하며 스킨 선택 때마다 다시 조사하지 않는다.
- 복제·UBE 합성·기본 경로 초기화·선택 DDS 기록이 모두 대응표를 거친다. 기록 후 shader getter readback 및 게임 DDS preflight 검사는 유지.
- 정상 지원 게임에서도 내부 슬롯이 SDK 이름과 다를 수 있으므로 특정 게임 버전 또는 임의의 고정 순서를 추정하지 않는다.
- 일반 스킨은 Native TXST 경로 유지. 오버레이/색상/바디모프/배포/세이브 코드 변경 없음.

## 검증/배포

- Release 빌드 통과.
- 전체 17개 테스트 통과.
- 추가 테스트: 8! = 40,320개 배열 순서에서 getter 기반 대응과 DDS 재읽기 확인, 모호한 대응 거부, 성공/실패 시 임시 경로·resource ID 보존.
- 위 테스트는 실제 Skyrim 실행 테스트가 아니다. 다음 실행에서 `BCNG TXST shader-to-record slots` 및 `path verified … engineDDS=true` 로그와 실제 화면을 함께 확인해야 한다.
- `git diff --check` 통과.
- DLL SHA256: `A7F9FE9671BF1475553EC2510DC3B07D5730B6C418C3868ADD40B114BA3D7FB1`.
- 게임 종료 확인 후 툴레드 MO2의 `Body Change NG\SKSE\Plugins\BodyChangeNG.dll`만 교체. 설정/규칙/스킨팩/스크립트 보존, 공개 릴리즈 및 떼껄룩 갱신 안 함.

재시험: 스킨 하나 선택 → 다른 팩 선택 → 기본값 복원 순서로 몸·손·발·얼굴의 실제 변화를 확인. 실패하면 새 로그에서 대응표 검증, DDS preflight, 실제 적용/3D 갱신 중 어디까지 진행했는지 구분한다.
