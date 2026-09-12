# 얼굴 생성 경로 읽기 전용 검증 — 2026-09-10

후속 실측: [얼굴 캐시·재생성 경로 검증](FACE-CACHE-REFRESH-VERIFICATION-20260910-KO.md).
아래 추가 조사에서 미확인이었던 DB 저장과 얼굴 기존 항목 정리 경로를 확인했고,
QueueNiNodeUpdate가 몸만 갱신한다는 기존 설명을 정정했다. 단순 FTST 교체만으로
얼굴 Diffuse가 바뀐다는 가설을 검증 완료한 것은 아니다.

## 결론과 검증 범위

툴레드 SE 1.5.97의 실행 중인 코드에서 **플레이어와 NPC를 받는 공통 얼굴
로딩·부착 경로**를 확인했다. 그러나 이것은 “ActorBase 얼굴 TXST만 변경하면
모든 얼굴 DDS가 자동 교체된다”는 증거가 아니다. 기존 BCNG가 호출하는
`PrepareHeadPartForShaders`의 역할을 너무 넓게 해석했던 점을 정정한다.

이번에는 메인 메뉴에서 코드만 읽었다. 게임 함수 실행, 메모리 쓰기, 후킹,
일시정지, 세이브 로드/저장, DLL 교체, 공개 릴리즈 변경은 하지 않았다.
인게임 스킨 선택 결과나 모든 재생성 이벤트를 실행 검증한 것이 아니다.

추가한 파일은 읽기 전용 조사 도구 `tools/audit-native-face-code.py`와 이 보고서다.
기존 플러그인 소스, 몸·손·발, 오버레이, 저장·배포 로직은 이번 조사에서
수정하지 않았다. 기존 dirty worktree는 유지했다.

## 재현 정보

- 실행 파일: `D:\TuLED13E\STOCK GAME\Skyrim Special Edition\SkyrimSE.exe`
- FileVersion 문자열: `1.5.97.0` (고정 숫자 리소스는 `1.0.0.0`이므로 구별)
- 실행 파일 SHA-256: `693E5A51EA2680119A68620BCF5080E81745549872B5D06BBD3F51131B67ABAB`
- 접근 권한: `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`만 요청.
- 실행 코드 범위는 PE 예외 테이블과 실행 섹션 경계로 검증했다.
  chained unwind fragment를 합쳤고, 예외 테이블에 없는 확인 대상 leaf 함수만
  별도의 64바이트 창으로 읽었다. CALL 후보 검색은 NPC 코드 20 KiB로 제한했다.
- 산출물: `build/audits/native-face-code.json`, `.disasm.txt`, 진단용 `.bin`.
  게임 코드 캡처이므로 배포 패키지/공개 소스 ZIP에 넣지 않는다.
- 아래 주소는 모두 **SE 1.5.97 모듈 상대 주소(RVA)** 다. AE 주소로 재사용 금지.

공식 소스와의 함수명 대조:

- [RaceMenu 0.4.16 SKEEHooks.cpp](https://github.com/expired6978/SKSE64Plugins/blob/87a5cadd5c282e790ea6e9cf104bb7aa551fc4dc/skee/SKEEHooks.cpp):
  `RegenerateHead`, `UpdateHeadState`, preprocessed-head 분기 위치.
- [현행 RaceMenu SKEEHooks.cpp](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/SKEEHooks.cpp):
  같은 역할의 AE 위치/호출 지점이 SE와 다름.
- [Face Discoloration Fix Offsets.h](https://github.com/Exit-9B/Face-Discoloration-Fix/blob/9051804c52bbe4eca300e2678d25b55c94c38704/src/Offsets.h),
  [FaceGenManager.cpp](https://github.com/Exit-9B/Face-Discoloration-Fix/blob/9051804c52bbe4eca300e2678d25b55c94c38704/src/FaceGenManager.cpp):
  생성/사전제작 FaceGen 분기가 별도이며, 해당 모드가 캐시 우회를 위해 여러 지점을
  다루는 것을 확인했다. 그 우회 패치를 BCNG에 적용하지 않았다.

## 1. 현재 얼굴 갱신이 Diffuse 교체를 보장하지 않는 이유

`src/BodyChangeNG/NativeSkinBackend.cpp`의 `RefreshLoadedFace`는 선택 TXST를
ActorBase에 둔 뒤 `PrepareHeadPartForShaders`를 호출한다.
SE ID 26259의 RVA `0x3D2A60`이며 공식 SKSE/RaceMenu의 `RegenerateHead`와 같다.

확인한 native 본문에서 Face HeadPart일 때 다음 순서로 TXST를 얻는다.

1. `TESNPC.headRelatedData->faceDetails`.
2. 없으면 해당 종족·성별의 기본 얼굴 TXST.
3. 마지막으로 HeadPart의 TXST.

그러나 실제 FaceGen(feature 4) 분기가 그 TXST에서 갱신하는 것은 다음과 같다.

| 호출 지점 / helper RVA | 확인한 대상 |
|---|---|
| `0x3D2D52` → `0x3D31C0` | 얼굴 subsurface 텍스처, material `+0xB0` |
| `0x3D2D61` → `0x3D3100` | 얼굴 detail 텍스처, material `+0xA8` |
| `0x3D2D6F` → `0x3D2FC0` | normal 텍스처 `+0x58` 및 관련 셰이더 플래그 |
| 그 뒤의 tint 처리 | 얼굴 tint 텍스처 `+0xA0` 등 |

이 경로에서 선택 TXST의 **Diffuse를 새로 로드해 교체하는 작업은 확인되지
않았다**. feature 5 분기도 피부색/normal 처리가 있으며, 전체 TXST를 그대로
재바인딩하는 함수로 볼 수 없다. 설치된 진입부에는 이미 detour가 있으므로,
이 결론은 읽은 native 본문과 공식 RaceMenu wrapper의 역할을 구분한 것이다.

기존 로그의 “선택한 nativeTXST 경로는 정상, loadedDiffuse는 이전 경로”와
일치하는 구체적인 불일치다. 이전 보고서의 함수 이름만으로 전체 얼굴 DDS가
재읽힌다고 기대했던 판단은 충분하지 않았다. 또한 현재 `refreshed` 로그는
함수 호출 사실이며, 화면 교체 성공 판정이 아니다.

## 2. 새 얼굴과 사전제작 NPC 얼굴의 차이

`GetHeadModel` (`0x363DE0`)에는 다음 두 경로가 있다.

- 사전제작 경로: `0x363E54` → `0x3632D0`. NPC/원본 faceNPC를 기준으로
  FaceGen 리소스를 찾아 캐시된 모델/자식을 복제한다.
- 생성 경로: `0x363EF5` → `0x388480` → `0x3D24F0` → `0x3D2600`.
  HeadPart 기반 얼굴을 만들며 플레이어에 관한 명시적 분기도 존재한다.

생성 helper `0x3D2600`은 HeadPart `+0x88`의 TXST에서 index 0을 읽어 로드한다.
호출 `0x3D27DD` → leaf `0x3D3E90`은 shader `+0x78`의 material에 있는
**diffuseTexture `+0x48`** 을 참조 카운트를 관리하며 바꾼다.
이것은 ActorBase의 FTST를 전체 스킨 공급자로 쓰는 경로와 다르다.

따라서 다음 둘은 구별해야 한다.

- 같은 기본 경로의 DDS 파일 자체를 교체: 그 경로를 읽는 얼굴들이 새 파일을 사용.
- 특정 액터의 FTST만 다른 경로로 지정: 해당 얼굴 로딩 코드가 그 값을 선택해야 적용.

기본 경로 파일 교체가 잘 된다는 사실만으로 액터별 FTST 교체의 동작을 증명할 수 없다.
NPC용 FaceGen NIF를 무시하고 재생성하도록 강제하면 외형 모드의 조형을 잃을 수
있으므로 이번 조사에서 그 방법을 선택하지 않았다.

## 3. 액터를 알 수 있는 공통 지점

`0x364FF0`의 로딩 함수는 TESNPC와 실제 reference를 받는다. reference의
Character 타입 검사 및 플레이어 전용 1인칭/3인칭 관련 분기를 포함한다.
공통 얼굴 생성 구간에서는 다음 호출이 연속된다.

1. `0x365298`: TESNPC로 `GetHeadModel` 호출, 얼굴 노드 출력 획득.
2. `0x3652A7`: 같은 TESNPC, 실제 액터, 얼굴 노드 출력 주소로
   부착 함수 `0x363F20` 호출.

`0x363F20`은 액터의 노드를 구하고 얼굴을 붙인다. 이 위치에는 액터 문맥이 있다.
반면 더 아래의 DDS 경로 로더나 `GetHeadModel` 인자만으로는 실제 Actor reference를
항상 구별할 수 없다. TESNPC와 Actor는 같은 식별자가 아니다.

**주의:** `0x364FF0`에는 TESNPC를 키로 한 기존 얼굴 항목 검사와 생성을 건너뛰는
분기도 있다. 위 지점이 모든 QueueNiNodeUpdate/기존 얼굴 갱신 때 반드시 호출된다고
확정할 수 없다. AE의 인라인된 분기까지 이번 실행 파일 조사로 검증한 것도 아니다.

또한 모델 출력이 돌아오는 시점에는 이미 텍스처가 연결돼 있다. 단순히 이 지점에서
출력 재질을 바꿔 놓고 “게임이 TXST를 처음부터 자동으로 읽었다”고 표현하면 안 된다.
사용자가 거부한 사후 live 재질 적용과 생성 단계의 공급 경로를 명확히 구분해야 한다.

## 4. 복제해도 공유될 수 있음 — 실제 코드 확인

`BSLightingShaderProperty::CreateClone` (`0x12C4980`)은 새 shader property를
만들지만, 그 하위 복제는 다음 경로를 탄다.

`0x12C4A50` → `0x12910F0` → material manager `0x130BB80`.

material manager에는 원래 재질을 반환하거나 동등한 재질을 캐시에서 재사용하는
분기와 새 재질을 만드는 분기가 함께 있다. feature 5는 별도 복제를 요청하는
분기가 있지만, 이것을 모든 얼굴/모든 feature의 독립 소유 보장으로 확대할 수 없다.

더 중요한 것은 material의 복제 자체도 deep copy가 아니라는 점이다.

- `BSLightingShaderMaterialBase::CopyMembers` (`0x12CEF40`)는 diffuse/normal 등
  NiTexture 참조를 복사하며, TextureSet(`+0x78`)도 참조 카운트를 증가시켜 공유한다.
- `BSLightingShaderMaterialFacegen::CopyMembers` (`0x12D2160`)는 위 복제 이후
  tint/detail/subsurface 텍스처도 참조로 복사한다.

따라서 **새 얼굴 노드 / 새 shader property / 새 material** 중 하나를 만들었다는
사실만으로 원본 TextureSet에 쓰기가 안전해지는 것이 아니다. 공통 캐시, 원본
HeadPart/TXST, 공유 TextureSet을 직접 수정해서는 안 된다. 읽기 전용 DDS 리소스의
참조 공유는 가능하지만, 선택별 경로 테이블과 변경 가능한 재질 상태는 분리해야 한다.

## 5. 구현 전에 남은 검증

아래 사항이 남아 있으므로, 현재 상태를 “플레이어/NPC 공통 해결 완료”로 보고하지 않는다.

1. 양쪽 얼굴 생성 분기에서 선택 TXST를 **초기 텍스처 연결 입력**으로 사용할 지점.
   이미 연결된 결과에 사후 DDS를 쓰는 구현으로 요구사항을 바꾸지 않는다.
2. NPC 캐시 hit, FaceGen 원본 faceNPC, 동일 ActorBase를 쓰는 여러 액터의 구별.
3. 기존 얼굴을 갱신할 때 해당 생성 경로로 진입시키는 액터 한정 native 요청.
   전역 FaceGen 캐시 삭제나 전체 NPC 얼굴 재생성으로 우회하지 않는다.
4. 재질/TextureSet 독립화와 엔진 캐시 등록·해제 수명, 다른 모드 detour와 실행 순서.
5. SE/AE별 함수 경계·인라인 경로·정확한 인자. RaceMenu 인터페이스 버전만 같다고
   Skyrim 내부 함수의 주소/호출 규약까지 같아지는 것은 아니다.
6. 인게임 플레이어/NPC A→B→기본값, 저장/로드, 재생성, 동일 모델의 두 액터 격리,
   얼굴 조형·화장·흉터 보존. 이 읽기 전용 검증은 이를 대신하지 않는다.

현재 유지할 원칙은 “BCNG의 액터별 저장 선택을 원본으로 삼되, 원본 FaceGen 조형은
보존하고 생성 시점에 선택된 텍스처를 공급한다”다. **원칙을 지킬 수 있는 구체적
연결 구현은 추가 검증 대상**이며, 동작을 증명한 새 시험 DLL은 아직 없다.

## 조사 도구 검증 결과

- Python 문법 검사 통과.
- 캡처한 24개 코드 범위의 길이 및 BSLightingShaderProperty NiRTTI 주소 대조 통과.
- 주요 direct CALL 5개의 opcode/상대 목적지를 디스어셈블리와 별도로 대조, 통과.
- 초기 예외 테이블에 없는 leaf 요청은 중단됐다. 이후 해당 직접 호출 대상에 한해
  실행 섹션 안의 명시적 64바이트 창을 추가하여 조사했으며, 일반 범위 검사는 유지했다.
- `git diff --check` 통과. 기존 LF/CRLF 안내는 남아 있다.
- `build/audits` 산출물의 Git ignore 확인.
- 플러그인 변경이 없는 조사 작업이므로 Release 빌드/전체 회귀 테스트는 재실행하지
  않았다. 위 검사 결과를 게임 기능 테스트 통과로 표현하지 않는다.

## 추가 조사 — 캐시 재사용과 공개 API 대조

같은 날 추가 요청에 따라 기존 캡처, 현재 BCNG 코드, 공개 소스를 더 대조했다.
추가 확인 시 SkyrimSE 프로세스는 실행 중이 아니었다. 게임을 다시 실행하거나
메모리를 수정하지 않았으며, 이번 추가 조사의 변경 파일은 이 문서뿐이다.

### A. 최초 NIF 로드 한 곳만 가로채는 방법으로는 부족하다

사전제작 얼굴 로더 `0x3632D0` 안에서 복제 호출 두 곳을 구분했다.

- `0x3634D5` → `0xC52820`: 모델 루트 복제.
- `0x36366A` → `0xC52820`: 별도로 만든 얼굴 루트에 붙일 자식 복제.

설치된 SE Address Library에서 이 복제 함수는 ID 68836이다. 모델 DB의
이미 로드된 노드를 복제하는 경로와 앞서 확인한 재질 참조 복사가 있으므로,
**NIF 파일을 처음 읽는 순간에만 텍스처 입력을 바꾸는 후크로 캐시 재사용까지
포괄했다고 볼 수 없다.** 캐시에서 가져온 얼굴도 처음 읽을 때와 같은 선택을
보장해야 한다. 이것은 실제 선택 반복 테스트 결과가 아니라 코드 경로의 제약이다.

더 중요한 추가 데이터 흐름은 다음과 같다.

1. `0x363792`: 출력 얼굴 포인터 `[r14]`를 가져온다.
2. 해당 노드의 참조를 증가시키고 로컬 NiPointer 형태로 둔다.
3. `0x3637AD`: 그 로컬 포인터의 주소를 두 번째 인자로 둔다.
4. `0x3637B2`: 앞서 구성한 별도 모델 경로 버퍼를 첫 번째 인자로 둔다.
5. `0x3637B9` → `0xD2F440` 호출. SE Address Library ID 74043이다.

즉, **만든 얼굴 결과가 함수 반환 전에 다시 경로와 함께 DB 인근 함수에
전달된다.** 가공 모델의 캐시 등록일 가능성이 높지만, `0xD2F440`의 본문은
현재 캡처에 없으므로 등록 방식·소유권 이전·캐시 키를 확정하지 않는다.
다만 이 구간을 무시한 채 “복제 중에 변경하면 캐시에 영향을 줄 수 없다”고
설계하는 것은 근거가 없다. 반환 직전에는 안전하다는 보장도 아직 없다.

로컬 CommonLib의 `BSResource::Entry`는 `ctrl +0x0C`, `data +0x28`을 가지며,
캡처의 엔트리 잠금/루트 접근과 맞는다. `BSModelDB::Demand`의 알려진 SE ID는
74040이다. 이 인접 함수의 이름을 ID 74043에 그대로 붙이지 않았다.
공식 SKSE의 공개 선언에서도 ID 74043의 역할을 확정할 선언은 찾지 못했다.
[SKSE BSModelDB.h](https://github.com/ianpatt/skse64/blob/25b72352adb6543fa6d0bd3795780672b2e238e0/skse64/BSModelDB.h)

### B. 공개 얼굴 교체 API는 요구한 생성 입력 경로가 아니다

Papyrus Extender 공식 소스의 두 함수를 직접 확인했다.

| 후보 | 실제 처리 | BCNG 요구사항과의 관계 |
|---|---|---|
| `ReplaceFaceTextureSet` | 로드된 실제 Face 노드의 재질을 새 재질로 교체 | 사후 live 재질 적용이므로 채택하지 않음 |
| `SetHeadPartTextureSet` | 현재 HeadPart 레코드의 `textureSet` 포인터 변경 | 공유 HeadPart를 직접 바꾸며 액터별 생성 입력의 대안이 아님 |
| 기존 `PrepareHeadPartForShaders` | 얼굴 normal/detail/subsurface/tint 등 갱신 | Diffuse 전체 재바인딩 API라는 기존 가정은 성립하지 않음 |
| NIF 최초 로드만 후킹 | 처음 리소스에 텍스처를 연결하는 경우를 다룸 | 이미 만들어진 모델의 캐시 복제 경로까지 보장하지 못함 |
| `GetHeadModel` 반환 후 출력 수정 | 생성된 얼굴 결과에 텍스처를 다시 설정 | 원래부터 TXST를 읽은 것으로 표현하면 안 됨 |

`ReplaceFaceTextureSet`의 내부 `ActorApplier::SkinTXST`는 재질 생성/복사,
`ClearTextures`, `OnLoadTextureSet`, `SetMaterial` 순서다. 함수명에 TXST가
있다고 네이티브 얼굴 원본에 영구 연결하는 기능인 것은 아니다. 또한 FaceGen
경로의 전체 텍스처 교체에서도 기존 `kMultilayer` 얼굴 tint 슬롯을 따로 보존한다.
이는 스킨 DDS 선택과 NPC의 FaceGen 틴트·화장을 구분해야 한다는 대조 근거다.
해당 live 교체 구현을 BCNG에 가져오지 않았다.
[Graphics.cpp](https://github.com/powerof3/PapyrusExtenderSSE/blob/81e4c6b7b4912a3dea6e6be22c31c5e805f69503/src/Papyrus/Functions/Graphics.cpp),
[ActorGraphics.cpp](https://github.com/powerof3/PapyrusExtenderSSE/blob/81e4c6b7b4912a3dea6e6be22c31c5e805f69503/src/Papyrus/Util/Graphics/ActorGraphics.cpp)

위 커밋들은 조사 대상을 고정하기 위한 참조이며, 프로젝트 의존성을 업그레이드한
것이 아니다. 검토한 API에 적합한 함수가 없다는 결론이지 모든 모드/비공개 엔진
함수를 다 조사해 그런 방법이 절대 없다고 증명한 것은 아니다.

### C. 현재 BCNG에는 같은 ActorBase 공유에 관한 명시적 제약이 있다

`NativeSkinBackend.cpp`의 `g_instances`는 ActorBase FormID가 키다.
`QueueApply`는 `ResolveSharedBaseAction`이 `rejectConflict`이면
`sharedActorBaseConflict`로 반환한다. 현재 정책은 서로 다른 액터라도 같은
ActorBase에서 동일 프로필을 요청하면 허용하지만, 기존 선택과 다른 프로필이면
거절한다. `NativeSkinOwnership.h`와 기존 `SkinArchitectureTests.cpp`에서도
이 정책이 명시되어 있다.

따라서 **“NPC에도 적용된다”와 “같은 베이스를 공유하는 두 NPC가 각자 다른
스킨을 쓴다”는 현재 코드에서 동일한 보장이 아니다.** 이를 플레이어 얼굴 미교체의
원인으로 단정하지 않는다. 후자의 요구를 충족하려면 실제 Actor reference와
ActorBase를 구별해야 하며, 충돌 방어문만 지워 해결해서는 안 된다.

### D. 남은 구현 후보의 조건과 확인 한계

유효성을 검증할 후보는 다음 조건을 모두 갖춰야 한다.

- 실제 액터와 선택 세대 정보를 가진 생성 문맥으로, 생성 얼굴과 사전제작 FaceGen
  얼굴 양쪽에 선택 TXST를 공급한다. 비동기 작업이면 문맥 전달 방식도 검증한다.
- 원본 NIF/HeadPart와 공통 모델 캐시는 읽기 전용으로 두고, 액터에 귀속되는
  텍스처 연결 단계에서만 선택을 반영한다. 캐시 결과로 재등록되는 경계도 포함한다.
- 이미 존재하는 얼굴은 액터 한정 native 갱신으로 이 경로에 다시 들어와야 한다.
  생성 후의 live DDS 쓰기나 전역 FaceGen 캐시 삭제로 대체하지 않는다.
- 기본값 복원은 원래 공급 경로를 사용하며, FaceGen 조형과 해당 액터의 tint는
  별도 정책 없이 선택 스킨의 다른 슬롯으로 덮어쓰지 않는다.

이 조건을 만족하는 단일 후크/완성 구현은 아직 입증하지 못했다. 다음 기계어 확인
대상은 막연한 추가 검색이 아니라 **ID 74043의 DB 처리 본문, 공통 부모의 얼굴
기존 항목 생략 분기와 그 항목의 수명, 액터 한정 얼굴 재진입 경로**다.

설치 EXE의 디스크 코드와 기존 실행 중 캡처를 세 함수의 같은 위치에서 대조했을 때
일치하지 않았다. 그러므로 꺼진 게임의 디스크 바이트를 실행 중 코드 대신
디스어셈블해 위 미확인 함수의 동작을 단정하지 않았다. 이 비교만으로 파일 변조나
그 정확한 원인을 진단하지도 않는다. ID 74043은 디스크의 예외 테이블상
`0xD2F440..0xD2F4CF`의 143바이트 범위이지만 실제 본문 확인은 별개다.

### 추가 조사 검증 결과

- 캡처의 runtime/EXE 해시를 확인하고, 위 두 복제 호출 및 DB 방향 호출을 포함한
  direct CALL 11개의 opcode와 상대 목적지를 원시 바이트로 재검산했다. 모두 일치.
- 공개 API는 이름/문서가 아니라 고정 커밋의 함수 본문을 대조했다.
- `git diff --check` 통과. 기존 LF/CRLF 안내는 유지된다.
- 플러그인 코드/DLL/세이브/설치 모드/공개 릴리즈 변경 없음. Release 빌드와
  인게임 테스트는 하지 않았으며, 이 결과는 기능 동작이나 SE/AE 전체 호환성의
  합격 판정이 아니다.
