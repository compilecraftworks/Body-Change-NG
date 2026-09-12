# SOS/TNG 성기 Native TXST 시험 빌드 — 2026-09-11

후속 실기에서 얼굴 보라색 및 ERF 후타 스킨 미적용이 보고되었다.
[후속 수정·진단 기록](FACE-PURPLE-ERF-DIAGNOSTIC-20260911-KO.md)을 참고한다.
아래 해시는 최초 설치본 기준이며, 같은 빌드 경로의 최신 DLL은 후속 기록의 해시다.

## 상태와 범위

구 성기 Live Material Adapter를 제거하고 네이티브 피부 방문자에 선택 TXST를 공급하는
코드를 연결했다. Release 빌드와 오프라인 테스트는 통과했다. 선택·재장착·저장/로드·
NPC 간 표시 격리의 실기 검증은 아직 하지 않았다.

- 실행 코드 확인: TuLED SE 1.5.97 / TAKEALOOK AE 1.6.1170.
- 사용자 실행 게임에서 query/read 권한으로 코드만 캡처했다. 조사 중 게임 메모리 쓰기,
  원격 함수 호출, 세이브 로드 및 DLL 교체는 하지 않았다.
- 시험 DLL은 이 두 버전의 코드 지문, 호출 지점, 소유권 가상 함수가 일치해야 활성화된다.
  다른 SE/AE 버전의 성기 TXST 지원을 검증한 결과가 아니다. VR은 빌드하지 않는다.
- 일반 빌드에서는 현재 이 기능이 비활성화된다. 이 상태를 공개 릴리즈로 배포하면 안 된다.
- 후속 사용자 요청으로 게임 종료를 확인한 뒤 TuLED/TAKEALOOK MO2의
  `Body Change NG/SKSE/Plugins/BodyChangeNG.dll`만 아래 시험 DLL로 교체했다.
  두 설치본의 SHA-256 일치를 확인했다. 별도 백업은 사용자 지시에 따라 만들지 않았다.
  설정·스킨팩·스크립트·배포 규칙 및 GitHub 공개 Release는 변경하지 않았다.

## 처리 구조

1. 기존 BCNG ActorState/ASTR v4가 액터별 선택을 저장한다. 남성 성기는 선택한 남성
   바디스킨의 성기 변형, 여성 후타나리는 별도 선택값을 사용한다.
2. 준비된 DDS 경로와 정확한 ARMO/ARMA ID, 피부 노드 이름을 값으로만 게시한다.
   전역 상태는 Actor/Geometry/Material/TXST 생 포인터를 보관하지 않는다.
3. 선택이 달라지면 일반 바디와 같은 Actor.QueueNiNodeUpdate를 요청한다.
   동일 소스/선택 재게시에는 별도 갱신을 요청하지 않는다.
4. 게임의 최초 구성 및 기존 부위 재사용 과정에서 슬롯 52 피부 방문자에 연결한다.
   정확한 액터·ARMO·ARMA·노드와 Skin Tint 셰이더 조건을 확인한다.
5. 해당 호출에만 빌린 방문자 컨텍스트/BIPOBJECT 사본으로 전용 TXST를 공급한다.
   실제 현재/버퍼 BIPOBJECT, 공급 모드의 Skin Armor/ARMO/ARMA/TXST는 수정하지 않는다.
6. 텍스처 로딩과 재질 갱신은 원본 네이티브 방문자가 수행한다. BCNG가 Material을
   직접 복제하거나 성기 NiOverride 값을 작성하거나 Equip/Unequip하지 않는다.

SOS의 착용 ARMO와 TNG의 Skin Armor 결합 ARMA는 대상 검색의 입력이 다를 수 있지만,
선택 DDS 적용 백엔드는 동일하다. 몸·손·발 Native Skin Armor, 얼굴 NiOverride,
페인트 오버레이를 이 성기 경로로 대체하지 않았다.

## TXST 수명

작은 반환 함수는 BGSTextureSet의 BSTextureSet 하위 객체(+0x30)를 반환한다.
피부 재질은 그 주소를 +0x78에 보관하고 참조 계수를 증가시킨다. 따라서 네이티브
방문자가 반환했다고 TESForm을 직접 파기하면 안 된다.

| 확인 항목 | SE RVA | AE RVA |
|---|---|---|
| 최종 피부 방문자 | `1CCD50` | `219510` |
| TXST 하위 객체 반환 | `2D1980` | `3270A0` |
| 재질의 TXST 참조 증가 | `12CF4FC` | `14B799C` |
| 생성자 최초 참조 1 설정 | `2D126E` | `3268EE` |
| this 보정 소멸 진입 | `2D2074` | `327784` |
| 마지막 참조 DeleteThis | `C61A30` | `D27520` |

팩토리 생성 참조 1을 NiPointer로 인수한 뒤 생성 참조를 해제한다. 원본 방문자가
재질에 남긴 참조는 호출 반환 후에도 유지된다. 엔진의 마지막 참조가 없어지면
this 보정을 거쳐 전체 BGSTextureSet이 파기된다. 과거 선택 TXST를 무한 보관하지 않는다.

SE/AE의 Skin Tint(feature 5) 재질 복제 경로도 각각 읽었다. 이 경로의 인스턴스
분리를 전제로 하며, 음모·일반 조명 셰이더를 피부로 추측해 공급하지 않는다.
실제 두 NPC 화면과 장시간 메모리 사용량 검증은 별도로 필요하다.

## 기본값·저장·구 경로 정리

- Default는 공급 모드의 현재 TXST를 우선한다. TXST 없이 NIF 경로만 있는 부위는
  지오메트리에 값으로 기록한 원본 8개 채널에서 복원한다.
- A → B에서 B에 없는 채널은 공급 원본을 사용한다. A의 DDS를 B에 이어 쓰지 않는다.
  준비 실패 시 Default 복원용 원본 기록을 유지한다.
- 선택과 장착 상태는 별개다. 후타 장비 탈착 자체는 저장된 선택을 지우지 않는다.
- 세션 Reset/액터 Forget은 런타임 캐시를 정리하며 ASTR 영구 선택은 별도 관리한다.
- `LiveAddonSkinBackend.cpp/.h`, `AddonLiveMaterialPolicy.h`를 제거했다.
  구 지오메트리 ExtraData는 런타임 자료이며 세이브 저장 키가 아니었다. 기존 얼굴·
  오버레이 및 영구 상태 직렬화 코드는 구 성기 경로로 오인하여 삭제하지 않았다.

## 실제 설치 팩의 남은 제한

TAKEALOOK의 `LDD - TNG - Racial Penis Variances - BNP 'n TRX - 3D Pubic Hair`에 있는
Altmer `malegenitals_1.nif`는 성기 shader type 5 / flags1 `82601303`을 사용한다.
피부 diffuse는 `Textures\LDDAltmer\LDDAltmer.dds`, 별도 음모는 type 6이다.
파일 SHA-256: `bc7622ad54c2eb39e6607e0b95913cd076cb8a851f8629d4c7ab5dff944aef93`.

기존 남성 스킨팩 변형 선택은 `\sos\<variant>\` 모델/TXST 경로를 근거로 한다.
LDD 전용 경로는 여기에 매칭되지 않는다. 이 경우 원본을 유지하고 경고를 기록한다.
TNG라는 이유만으로 SOS DDS를 LDD/TRX에 대입하지 않았다. LDD의 UV와 선택 팩 DDS를
확인한 명시적 매핑은 아직 미완료다. **이 DLL로 TAKEALOOK의 모든 LDD 남성 성기
스킨까지 바뀐다고 보고할 수 없다.**

## 검증 결과

- Release `BodyChangeNG` 빌드 통과.
- XMake 전체 테스트 타깃 21개 빌드/실행 통과.
- `test-addon-audits.py`: 18개 테스트 통과.
- 소유권/코드 지문 검사: SE 15개, AE 15개 통과.
- 일반 갱신 → 네이티브 방문자 연결 검사: SE 27개, AE 27개 통과.
- DLL 구 문자열 `BCNG_ADDON_BASELINE_`, `live_addon_skin`, `Live Material skin` 없음.

새 정책 테스트는 생산 코드의 참조 인수 도우미로 1,000회 모형 수명 주기를 검사한다.
동일 선택 무변경, 부분 채널 복원, 공유 ARMO의 서로 다른 액터 선택, 동시 게시/조회,
Reset/Forget 후 마지막 독자 해제도 검사했다. 기존 실제 ASTR codec/session 테스트도
통과했다. 실제 Skyrim 렌더러/공급 모드 또는 실게임 누수 검사를 실행한 것은 아니다.

## 결과 파일 및 변경

- DLL: `build/v1.2.0/trials/native-addon-txst/windows/x64/release/BodyChangeNG.dll`
- 크기: 2,625,024 bytes
- SHA-256: `DE4F5B488AECC054CEE84164AE011B5721E05FDDCB553338361DD16ED56A5FA4`
- 구성: Release, `native_addon_txst_trial=y`, 기존 `form_delete_guard_trial=y`,
  `skyrim_vr=n`, `EXCLUSIVE_SKYRIM_FLAT`.

주요 변경: `NativeAddonSkinBackend.*`, `NativeAddonPolicy.h`, `NativeAddonRuntime.h`,
`SkinApplication.*`, `SkinTargetResolver.cpp`, `ActorRegistry.cpp`, `main.cpp`,
`NativeAddonPolicyTests.cpp`, 성기 persistence/경로 격리 테스트, 감사 도구, `xmake.lua`.
UI는 후타 Default 설명의 구 Live Material 문구만 변경했다.

## 남은 게임 검증

1. SOS/TNG 각각 이미 장착된 상태에서 A → B → Default 즉시 반영.
2. 남성 선택 → 저장 → 게임 종료 → 로드 후 동일 스킨.
3. 여성 선택 → 탈착 → 저장/종료 → 로드 → 재장착 후 마지막 선택.
4. Cell/3D 재로드, 반복 QueueNiNodeUpdate 후 유지.
5. 동일 애드온의 두 NPC에 서로 다른 스킨 및 Default 복원의 상호 간섭 여부.
6. 선택/Default 반복과 장비 교체를 장시간 수행할 때 메모리와 CTD 여부.

이 항목을 실제 검증하기 전에는 공개 Release를 갱신하지 않는다.
