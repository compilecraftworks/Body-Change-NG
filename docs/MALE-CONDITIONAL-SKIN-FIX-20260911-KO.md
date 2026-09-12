# 남성 노인·종족별 스킨 분류 수정

2026-09-11, 현재 v1.2.0 작업 트리 기준.

## 변경

- `SkinProfiles.cpp`: 여성만 처리하던 조건부 DDS 분류를 남성에도 연결했다.
  `maleold`의 몸·손·얼굴, `nordmale`, `bretonmale`, `darkelfmale`,
  `highelfmale`, `imperialmale`, `orcmale`, `redguardmale`, `woodelfmale` 얼굴을 인식한다.
- 남성 오크 얼굴은 `maleheadorc` 우선, `malehead` 별칭도 지원한다.
- 조건부 디렉터리가 일반 프로필 병합에 참여하지 못하도록 양쪽 성별 모두 제외한다.
  남녀 혼합 팩에서도 성별이 일치하는 디렉터리만 조건부 채널로 사용한다.
- ID 생성·저장 형식·런타임 적용 방식은 변경하지 않았다.
- 기존 공통 적용 계획을 그대로 사용한다. 전용 채널이 없으면 선택한 팩의 일반 채널을
  사용하고, 일반 채널도 없으면 원본 유지 정책이 적용된다.
  얼굴 채널 우선순위는 일반 → 종족별 → 흡혈귀 → 노인용이다.
- 발은 명시적 발 파일이 있으면 그대로 사용하고, 몸 아틀라스 폴백이면 노인용 몸 채널도 따라간다.
- 종족/노인 액터 판정 방식, RSV 연동 및 SOS/TNG Live Material Adapter는 변경하지 않았다.

## 검증

1. 혼합 성별 카탈로그 테스트를 먼저 추가하여 수정 전 실패를 재현했다.
2. 수정 후 노인 몸·손·얼굴 분리, 남성 8종족 얼굴, 성별 격리, 일반 얼굴 오염 방지,
   전용 파일 추가 전후 자동 ID 유지 테스트가 통과했다.
3. 기본 남성/HIMBO/SAM의 전용 파일 유무·노인·흡혈귀 24조합에서 같은 팩 일반 DDS 폴백과
   몸·손·발·얼굴 채널을 검증했다.
4. 전체 테스트 실행 파일 20개 재빌드/실행 통과, Release 시험 DLL 빌드 통과.
5. 설치된 `Tempered Skins for Males - SOS Full`의 상대 파일명만 작업 폴더에 복제하여
   생산 카탈로그를 실행했다. 실제 DDS 내용은 복사하지 않고 게임 파일도 변경하지 않았다.
   일반 얼굴 4채널, 노인 몸·손·얼굴 노멀 각 1개, 실제 포함된 종족 얼굴 7개를 분리했다.
   팩에 없는 nordmale 변형은 생성하지 않는다.
6. `git diff --check` 통과. 인게임 표시 및 저장/로드 실측은 수행하지 않았다.

## 산출물

현재 비공개 appearance-compat 설정의 시험 DLL:

`build/v1.2.0/trials/appearance-compat/windows/x64/release/BodyChangeNG.dll`

SHA-256: `6275A31FA237762F4C1F473DB222A593F3EAE02739675DE5A263FDD60CED22E9`

MO2 설치 폴더, GitHub 공개 Release 및 배포 ZIP은 갱신하지 않았다.
기존 Face attachment/FormDelete guard 시험 옵션이 유지된 빌드이며,
이 변경으로 미검증 게임 버전의 호환성이 새로 입증된 것은 아니다.
