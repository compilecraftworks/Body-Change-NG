# RSV 3.4.3 원본 확인 및 BCNG 비교

## 확인 범위

2026-09-11, 이미 다운로드되어 있던 RSV 3.4.3 4K 압축파일의 원본 ESP,
동봉 Papyrus 소스, SPID 설정과 현재 BCNG 작업 트리를 비교했다.
게임 통합 테스트, 운영 코드 변경, DLL 빌드/교체, 공개 배포는 하지 않았다.
이 결과를 다른 RSV 버전이나 설치 옵션 전체에 일반화하지 않는다.

- 파일: `C:\Users\yunha\Downloads\Racial Skin Variance - SPID 4K Version-81668-3-4-3-1776836448.7z`
- 압축파일 SHA256: `7593f9ba799ad74ce7d617cb7db1d261d53b9cd2a103c0710ef38560d0d88609`
- Main ESP SHA256: `8b1d6fa333ee59f3000fc53884d4d70c96bc25b61e10aab930c38cbd71b44785`
- 추출 위치: `build/audits/rsv-3.4.3-source/Racial Skin Variance - SPID 3.4.3 - 4K/`
- 읽기 전용 ESP 검사: `build/audits/inspect_rsv_forms.py`
- 공식 배포 페이지: https://www.nexusmods.com/skyrimspecialedition/mods/81668?tab=files

ESP 검사는 GRUP/레코드/서브레코드 경계, 압축 레코드의 확장 크기를 검사하며
FormID 연결을 읽었다. 마스터 플러그인에만 있는 레코드는 external로 표시하고
그 내부를 확인한 것으로 취급하지 않았다. 동봉 소스와 PEX의 완전한 의미적
동등성이나 실제 실행 순서를 런타임 추적으로 검증한 것은 아니다.

## 얼굴: NiOverride 사용 확정

`Distribution/Males and Females/RSVMain_DISTR.ini`는
`RSV_HeadTextureSpell`을 배포한다. 이전 대화에서 인용한
`RSVHeadTexturePatch`라는 과거 설정 이름과 구별해야 한다.

실제 ESP 연결 (아래 번호는 플러그인 내부 local FormID):

- SPEL `0x836`, `RSV_HeadTextureSpell`
- EFID가 MGEF `0x835`, `RSV_HeadTextureEffect`를 참조
- MGEF VMAD의 스크립트: `RSV_HeadTextureEffectScript`

동봉 `Main/scripts/source/` 안의 호출 근거:

- `RSV_HeadTextureEffectScript.psc:330`: NPC는 `ChangeHeadTexture`에 persistence=false 전달.
- `RSV_PlayerAliasScript.psc:532`, `:649`: 플레이어는 persistence=true 전달.
- `RSV_MainQuestScript.psc:904`: `ChangeHeadTexture` 구현.
- 같은 파일 `:930`: 실제 Face HeadPart 이름(type 1)을 얻는다.
- 같은 파일 `:933`: `NiOverride.AddNodeOverrideString`으로 key 9의 DDS 채널을 적용한다.

대상은 diffuse(0), normal(1), subsurface(2), 선택적 detail(3), specular(7)이다.
화장/틴트만 처리하는 코드가 아니며 얼굴 피부 텍스처 자체를 바꾼다.
Face TXST는 detail 파일명 확인에 읽지만, 이 함수에서 Face TXST를 교체하지 않는다.
HeadPart를 찾는 것은 노드 식별용이며 HeadPart/NIF 교체가 아니다.

NPC 효과의 StandBy 상태는 NiNodeUpdate 후 0.1초 지연 업데이트를 예약하여
`TextureSetSelection`을 다시 실행한다 (`RSV_HeadTextureEffectScript.psc:123`, `:137`).
`RSVHeadTexturesReset` 이벤트도 해당 액터/베이스를 확인하여 재적용한다.
NPC는 이 호출의 persistent 플래그가 false이므로 플레이어와 똑같이
영구 NiOverride 키 저장으로 설명하면 부정확하다. 이벤트 기반 재적용도 있다.

`HeadTexturesReset` (`RSV_MainQuestScript.psc:806`)은 해당 노드의 key 9,
채널 0/1/2/3/7을 제거하고 persistence=true인 경우 NiNode 갱신을 요청한다.
Skin Armor 복원만으로 이 별도 얼굴 상태가 제거/복원되는 구조가 아니다.

## 몸·손·발: 미리 만든 네이티브 그래프

Main ESP 레코드 수: ARMO 25, ARMA 156, TXST 839, FLST 115.
이는 Main ESP 전체 수이며 모두 동일 목적의 배포 대상이라는 뜻은 아니다.

배포 설정에는 Breton(0x875), Imperial(0x876), Nord(0x877), Redguard(0x878),
High Elf(0x879), Dark Elf(0x87A), Wood Elf(0x87B), Orc(0x87C) 및
노인/흡혈귀/기타 변형용 별도 Skin 배정이 있다. 전 NPC가 공통 폼 하나를
사용하는 것이 아니다.

실제 `RSV_SkinNakedNord`(0x877)의 RSV 소유 구성:

| 부위 | ARMA | 남성 TXST | 여성 TXST |
|---|---|---|---|
| 몸 | 0x88E | 0x92D | 0x8D7 |
| 손 | 0x88F | 0x938 | 0x8E2 |
| 발 | 0x895 | 몸과 동일 | 몸과 동일 |

여성 몸은 `Actors\Character\RSV\NordFemale\FemaleBody_1.dds`,
손은 같은 폴더의 `FemaleHands_1.dds`를 참조한다. 남성도 NordMale 안에서
동일 역할로 분리된다. ARMA에는 남녀별 NIF 경로와 TXST가 있고,
각 성별 몸/손의 FLST에 11개 TXST 변형도 연결되어 있다.
노인 Skin(0x870)의 몸/손/발은 MaleOld/FemaleOld TXST를 연결하며
발이 몸 TXST를 쓰는 구성도 확인했다.

SPID의 실제 배정은 NPC 베이스의 `skin`을 선택 ARMO로 바꾼다.
DDS 파일을 NPC마다 복사하는 방식이 아니다.
근거: https://github.com/powerof3/Spell-Perk-Item-Distributor/blob/master/SPID/src/Distribute.cpp
의 `for_first_form<RE::TESObjectARMO>` / `npc->skin = a_skin`.

## BCNG와의 차이

| 기준 | RSV | 현재 BCNG |
|---|---|---|
| 최종 몸 표시 경로 | Skin Armor → ARMA → TXST | 동일한 네이티브 경로 |
| 그래프 준비 | ESP에 미리 작성 | 현재 액터의 원본 그래프를 실행 중 복제 |
| 스킨 지정 | 조건별 완성 ARMO를 SPID로 배정 | 복제 TXST에 선택 팩 DDS를 쓰고 베이스에 배정 |
| 메시/종족 구조 | 배포된 ARMA 구성 | 원본 ARMA의 모델/종족/부위 구성을 보존하는 방향 |
| 얼굴 | Papyrus → NiOverride DDS 적용 | Face TXST + 얼굴 부착 시 선택 채널 공급 |

BCNG 근거: `NativeSkinBackend.cpp:243` CloneArmorGraph,
`:664` 원본 GetSkin 및 복제 준비, `:839` Face TXST 배정,
`:1292` FaceSnapshotLocked. `NativeFaceAttachment.cpp`는 그 스냅샷을 받아
얼굴 부착 시 선택 채널을 공급하고 FaceGen tint 자원을 보존한다.

중요한 제한: 현재 BCNG 네이티브 인스턴스는 ActorBase FormID 기준이다.
같은 베이스를 공유하는 서로 다른 액터의 상충하는 선택은
`ResolveSharedBaseAction`으로 거절한다 (`NativeSkinBackend.cpp:976`).
이를 모든 레퍼런스에 대해 완전히 독립적인 Skin Armor로 설명하면 안 된다.
SPID의 베이스 skin 배정 역시 베이스 공유 자체를 없애지 않는다.

## '폼 하나'로 단순화할 수 있는 범위

스킨/호환 구조별 완성 ARMO를 미리 만들고 같은 구성을 쓰는 NPC끼리
읽기 전용으로 공유하는 설계는 가능하다. 런타임 복제 폼과 수명 관리 부담을
줄일 수 있지만, 성능/메모리 효과는 아직 측정하지 않았다.

단 하나의 공통 ARMO/TXST를 계속 수정하면서 NPC마다 서로 다른 DDS를 주는
방식은 안 된다. 공유 폼을 변경하면 그것을 참조하는 다른 NPC에도 영향을 준다.
남녀별 텍스처 필드나 기존 FLST가 있다는 사실도 임의의 액터별 선택 상태를
공통 폼 하나로 자동 격리해 주지는 않는다.

BCNG가 임의의 폴더형 팩과 커스텀 바디/종족/원본 Skin Armor를 유지하려면
미리 준비한 폼 방식에서도 소스 그래프 및 호환 조합별 변형이 필요하다.
같은 스킨 이름만으로 그래프를 공유하면 원본 메시나 추가 부위가 다른 NPC를
동일 구성으로 바꿔 버릴 수 있다. 새 팩별 ESP 생성 또는 런타임 폼 생성도 필요하다.
기본값 원본 보관, 선택 저장/로드, 현재 3D 갱신도 여전히 별개로 처리해야 한다.

현재 기능 범위에서는 원본을 보존하는 네이티브 복제 경로를 유지하는 편이
적합하다는 판단이다. 추후 동일 원본/동일 최종 텍스처/동일 구조의 불변 그래프
공유는 별도 최적화로 검토할 수 있으나, 이번 조사에서 구현하지 않았다.

## RSV 호환에 대한 정정

기존 `FEMALE-SKIN-RSV-AUDIT-20260911-KO.md`의 원본 미확인 상태를 이번
3.4.3 조사로 보완한다. RSV 얼굴 처리는 별도 NiOverride 경로임을 확인했다.

현재 BCNG는 RSV 네이티브 텍스처 경로 감지/원본 보존/소유권 기반 복원은 있으나,
RSV 얼굴 효과를 선택 중 중지하고 기본값에서 재개하는 전용 조율은 확인되지 않았다.
따라서 몸 원본을 RSV에서 복제했다고 얼굴까지 완전히 대체/복원된다고 할 수 없다.

코드상 RSV의 지연 재적용이 BCNG 얼굴 부착 이후 값을 다시 바꿀 가능성이 있다.
이는 이번에 확인한 호출 구조에서 도출한 충돌 위험이며, 실제 게임에서 발생 순서나
현재 사용자 증상의 원인을 재현한 결과는 아니다.
요청한 'BCNG 미선택=RSV, 선택=BCNG, 기본값=RSV' 보장을 위해서는
얼굴 효과/override의 적용 권한과 재개를 별도로 설계하고 검증해야 한다.
RSV 스킨/얼굴 전부를 무조건 제거하거나 다른 모드의 같은 노드 키까지 지우는
방식을 이번 조사 결과만으로 안전하다고 판단해서는 안 된다.
