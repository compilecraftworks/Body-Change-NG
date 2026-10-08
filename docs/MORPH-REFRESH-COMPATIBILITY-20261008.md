# BCNG SE/AE morph refresh compatibility — 2026-10-08

## Scope and confirmed findings

The UBE inflation case is a user report, not a reproduction on the owner's AE
installation. OverlayFix is not a dependency of this change and was not changed.

The exact UBE SE 1.5.97 ExtraData comparator defect already covered in
`UBE-SE-MORPH-BASELINE-AUDIT-20260928.md` remains isolated to its verified DLL.
Do not extend that private byte patch to other RaceMenu binaries.

TAKEALOOK's AE 1.6.1170 / RaceMenu 0.4.20.0 DLL has SHA-256
`5225E4E3B185E6FC57C8D31B0CEDBE5A030A951D9A744D33071B64C45A38C208`.
Its BodyMorph v5 prefix matches BCNG's used methods. The original lookup leaf at
RVA 0xC7230 performs a full linear fallback after failed binary search. Running
that hash-checked leaf on private synthetic objects passed 17,527 checks,
including the captured SE pointer-order failure and shuffled ExtraData lists.
This is not a renderer/game reproduction and does not show that the reported AE
inflation has the same cause as the SE comparator defect.

## Product change

- Body, preview, cleanup and outfit key writers request one semantic
  `bodyMorphRefresh` job per actor. A later request replaces the old callback
  and moves it after already queued writes. The existing queue's actor lease,
  priority inheritance, session reset, cancellation and frame budget remain in
  force. This adds no actor map, retained geometry or per-frame mesh scan.
- At execution, resolve the ActorHandle and its current loaded 3D, fetch the
  current public interface, then call `ApplyBodyMorphs(actor, false)` on the
  existing game-task pump. Complete SKEE's public refresh under that actor lease
  instead of leaving partition work queued beyond it. This public v4/v5 flag
  controls update deferral, not the attach path; no attach/mesh hook was added.
- Automatic player restoration checks saved application signature and current
  owned keys through the existing registry. Surviving committed values are
  refreshed without rerolling/replacing them. Missing keys, changed preset XML
  or changed body options retain the normal repair commit. An older queued
  restore cannot overwrite a newer selection or active preview.
- Explicit preset commits and settings-popup option changes retain their normal
  commit/randomization path. Default/reset/removal and outfit policy retain
  their existing key ownership. Foreign keys are not removed by visual refresh.

The shared-prefix design covers the declared game admission table; a RaceMenu
installation must still match its game/SKSE release. It does not make a DLL built
for a different runtime interchangeable.

## Supported admission matrix

| Game runtime | BodyMorph prefix fixture | Overlay/Override routing |
|---|---|---|
| 1.5.97.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.317.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.318.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.323.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.342.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.353.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.629.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.640.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.659.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.1130.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.1170.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |
| 1.6.1179.0 | v4 and v5 pass | paired v1/v1 and v2/v2 policy passes |

These are offline admission/interface-layout tests, not installation of every
historical RaceMenu DLL. Product-file version is not used as a fragile ABI
whitelist. Missing/below-minimum/mixed contracts retain their rejection or
existing fallback policy; unlisted game layouts remain unsupported. Future
interface prefixes are a documented compatibility assumption, not verification.

## Dependency and engine-layout review

Latest stable CommonLibSSE-NG v11.0.0 is pinned at
`94faaed0c60eddd8347767f2d4d29a97c93bde8c`. Reviewed upstream release changes and
the diff from v6.7.1 in Actor/MagicTarget, PlayerCharacter, NiAVObject, BSGeometry,
BSDismemberSkinInstance, BSLightingShaderMaterialBase, BSLightingShaderProperty
and RuntimeDataAccessors. VR-only insertions stay outside the flat declarations;
no VR target or code path was enabled. Changed opaque field names are not direct
BCNG accesses. Geometry uses the versioned runtime accessor; the existing tint,
native-addon and UI adapters retain the explicit game table and boundary tests.

One consumer adjustment was needed: use upstream's GFxCharEvent (same 0x0C size)
and `wCharCode` member rather than the old duplicate local declaration. The new
ABI fixture fails compilation unless EXCLUSIVE_SKYRIM_FLAT is set and VR is off;
it also checks NiAVObject 0x110 and lighting material 0xA0 sizes.

The existing locked package recipes/versions remain unchanged. Upstream v11's
patch-safety build closure adds only MinHook v1.3.4 HDE, downloaded with its
upstream-pinned SHA-256. The full upstream license is retained in notices.
Release was built from source, not CommonLib's all-runtime prebuilt library.

## Validation

- All targets built successfully with XMake 3.1.0, VS 2026 MSVC, Release SE/AE.
- **48/48 regression executables passed**. Results are in
  `build/bcng-ae-test-results.json`.
- The BodyMorph ABI test calls BCNG's extracted production interface declaration
  through independent public v4/v5 provider layouts, including the unused
  UInt32/UInt64 cache-method difference. It exercises Set/Get, visitors, owned
  key clearing with foreign-key retention and nondeferred refresh: 12 admissions
  x 2 layouts x 1,000 cycles. This proves host-side prefix dispatch, not full
  DLL/game compatibility.
- The lifecycle fixture extracts the actual refresh/restore/cleanup functions.
  It covers 10,000 coalesced requests, final body/outfit composition, detached
  actors, cancellation, session changes, stale saved choices, active previews,
  explicit settings commits and normal Default/reset/removal/SFS controls.
  Its existing deterministic queue model passed 1,000,000 operations with bounded
  pending jobs and matching final outcomes. This is not a GPU/native-game leak
  measurement.
- The compiled refresh callback uses virtual slot 13 (`[vtable+0x68]`) and passes
  false in R8, matching the verified public BodyMorph prefix. Disassembly is
  retained in `build/bcng-morph-refresh-disasm.txt`.
- All 12 installed Address Libraries contain the existing distribution
  metadata guard's 11 relocations under the new SDK headers. See
  `build/bcng-ae-relocation-results.json`. Coverage is not executable-code proof.

## Remaining verification limits

No game was launched or injected into; saves, RaceMenu/UBE/OverlayFix DLLs and
MO2 installations were not changed. AE renderer attach ordering, interactions
with other morph writers and every historical RaceMenu binary remain untested
in game. A single native refresh is indivisible and can exceed the queue's frame
budget on a heavy mesh; coalescing bounds requests, not engine-call duration.

This fixes BCNG's observed redundant restoration/refresh scheduling. It does not
establish a confirmed AE root cause or promise that every third-party setup is
free of inflation. The existing SE guard still targets only its verified file.

Sources: [RaceMenu public morph implementation](https://github.com/expired6978/SKSE64Plugins/blob/9ebcb733e17be695f994cd2e9cc383043446bc02/skee64/BodyMorphInterface.cpp),
[CommonLib v11.0.0 release](https://github.com/alandtse/CommonLibSSE-NG/releases/tag/v11.0.0).

## 한국어 요약

- 지원 게임 12개 버전에서 사용하는 RaceMenu BodyMorph v4/v5 공통 경로를
  보완했다. 게임 버전에 맞는 RaceMenu 설치는 여전히 필요하다.
- 복원 시 기존 확정 모프를 보존하고 바디·의상·취소 뒤의 화면 갱신을 액터당
  하나로 합쳤다. 직접 프리셋 확정과 설정 창의 즉시 재적용은 유지했다.
- SE/AE 전용 Release 빌드, 48개 회귀 테스트, 12개 런타임 ABI/주소 검사 통과.
- 떼껄룩 AE DLL의 기준 메시 검색에서는 SE와 같은 오류가 확인되지 않았다.
  AE 제보의 실제 인게임 원인과 모든 조합의 무문제 여부를 확정한 것은 아니다.

## Final local output

- DLL: build/v1.3.5/windows/x64/release/BodyChangeNG.dll
- SHA-256: D4F9D995002F5BFEE57D66FCB8C213F403152F020B63362F24E22BC4D4181E3A
- This is a local pending compatibility build; version bump, release archives and publication were not performed in this task.

## v1.3.6 release build

The local output above is the pre-release v1.3.5 snapshot. The compatibility
change is now versioned as v1.3.6; the Release DLL resource is 1.3.6.0 and its
SHA-256 is `DAEA05128EFF14A02927F0667679B4EB8739DD38DDDDF0C8BA4B968FD710D987`.
All 48 v1.3.6 regression executables passed. Bilingual notes and release
verification are in `RELEASE-NOTES-v1.3.6.md` and
`RELEASE-VERIFICATION-v1.3.6-KO.md`. The in-game limits above still apply.
