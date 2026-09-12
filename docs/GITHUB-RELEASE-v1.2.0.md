# Body Change NG v1.2.0

Final v1.2.0 implementation, consolidated on September 13, 2026, with the changes below compared with v1.1.4.

## Downloads

- **Body Change NG v1.2.0.zip** — the MO2-ready installer. Seven files only: the DLL, an empty opt-in distribution JSON, three asset-folder guides, and two required license files. The complete installation folder structure is retained. No PDB, debug binaries, release documentation, source code, or third-party skins are included.
- **Body Change NG v1.2.0 Source.zip** — corresponding source with pinned build dependencies, tests, scripts, licenses, and documentation. `SOURCE-REVISION.txt` records the exact source commit. Generated binaries, PDBs, and Python caches are excluded.
- **SHA256SUMS v1.2.0.txt** — checksums for both ZIPs.

## Highlights

- Single-click or list navigation previews; double-click or the configured Activate binding confirms. Keyboard/gamepad menu Cancel closes the current popup or main UI. Unconfirmed previews are restored on close.
- Body, hands, and feet use private native Skin Armor/ARMA/TXST routing. Faces use separate NiOverride skin channels. Supported SOS/TNG male and female addons use native TXST construction.
- Reworked face update ordering, stale requests, restoration, texture paths, and cache identity to address delayed updates and purple-face failures during repeated selection. Compatible DDS channels from partial packs can apply independently.
- Tint masks are discovered inside BodySkin packs. Overlays include installed RaceMenu and SlaveTats collections, multiple manual entries per area, favorites, and per-item color/opacity.
- NPC conditions are configured from each supported catalog and saved only by an explicit immediate/next-launch distribution action. Overlay candidates retain their individual colors, including edits made in the checkbox-selection list. New installations have no preset rules.
- Manual appearance choices and defaults are saved with the matching SKSE co-save. Selected/all-actor reset covers every BCNG appearance category.
- Popups retain individual positions. The outfit popup opens centered on first use. The main title-bar rotation hint is fitted immediately left of X; right-mouse dragging and LT + RS rotate the character.
- Optional [Skyrim Fitting System SE-AE](https://www.nexusmods.com/skyrimspecialedition/mods/187128) integration uses Rendered Outfit API v1 for breast/nipple correction and ORefit decisions. Visible actual and registered armor use their own identities; hidden armor is ignored. If all correction-relevant clothing is hidden, correction is removed. Without the supported API, actual-equipment behavior is unchanged.

## Requirements

Use **SKSE64, Address Library for SKSE Plugins, and RaceMenu** matching your Skyrim runtime. Body presets require compatible body/outfit meshes built with morph support. Install the assets and addons required by each feature separately.

- Skyrim SE: **1.5.97**
- Skyrim AE: **1.6.317 / 1.6.318 / 1.6.323 / 1.6.342 / 1.6.353 / 1.6.629 / 1.6.640 / 1.6.659 / 1.6.1130 / 1.6.1170 / 1.6.1179**
- SE/AE-only build; no VR support. SFS is optional and its supported API is required only for the integration above.

## Installation and upgrade

In MO2, the mod root corresponds to the game's `Data` folder. Install the runtime ZIP normally; do not install the source ZIP as a mod.

```text
Data\SKSE\Plugins\BodyChangeNG.dll
Data\SKSE\Plugins\BodyChangeNGdistribution.json
Data\CalienteTools\BodySlide\SliderPresets\
Data\BodySkin\
Data\Futanari\
```

- Copy each entire **installed skin-mod folder** into `Data\BodySkin\` and preserve its `Textures` tree.
- Copy each entire **installed futanari-skin mod folder** into `Data\Futanari\`; install the actual SOS/TNG addon normally.
- Tint example: `Data\BodySkin\Your Pack\Textures\actors\character\character assets\tintmasks\`. The old separate `TintMask` root is no longer used.
- Back up `.ess` saves with their matching `.skse` co-saves, settings, distribution JSON, and asset packs. Keep customized rules rather than overwriting them with the empty installer starter.
- Do not load old and new BCNG DLLs together. Review migrated NPC conditions before saving; the old exclusion-rule model has been removed.

## Verification

The SE/AE Release build and all **23 automated test executables** passed, including SFS-consumer boundaries and popup/title-bar geometry and clipping. This is not exhaustive in-game or leak-free certification. Native addon ownership code was directly inspected on 1.5.97 and 1.6.1170; other supported targets still require runtime code verification.

[Full English usage and paths](https://github.com/compilecraftworks/Body-Change-NG/blob/main/docs/NEXUS-DESCRIPTION-v1.2.0.md) · [Complete English changelog](https://github.com/compilecraftworks/Body-Change-NG/blob/main/docs/NEXUS-CHANGELOG-v1.2.0-EN.txt)

---

## 한국어 요약

- 릴리즈 ZIP은 DLL·빈 배포 설정·폴더별 안내 3개·필수 라이선스 2개만 포함합니다. 설치 폴더 구조는 유지하고 PDB·디버그 파일·소개글·체인지로그·소스·스킨 DDS는 넣지 않았습니다.
- 한 번 클릭은 미리보기, 더블클릭 또는 게임에 지정된 Activate는 적용입니다. 키보드·게임패드의 메뉴 Cancel로 팝업이나 본창을 닫습니다.
- 몸·손·발과 지원 SOS/TNG 애드온은 네이티브 TXST, 얼굴은 별도 NiOverride 피부 채널을 사용합니다. 빠른 스킨 전환의 얼굴 갱신 순서·복원·텍스처 캐시 처리를 개선했습니다.
- 오버레이는 부위별 복수 적용·즐겨찾기·개별 색상을 지원합니다. NPC 배포 체크박스 목록에서도 후보마다 색상·투명도를 편집할 수 있으며, 즉시 배포 또는 다음 게임 배포를 눌러야 조건과 색상이 저장됩니다. 기본 배포 조건은 없습니다.
- 틴트마스크는 BodySkin 안의 팩에서 인식합니다. 설치된 스킨 모드 폴더 전체는 `Data\BodySkin\`, 후타스킨 모드 폴더 전체는 `Data\Futanari\` 아래에 넣고 `Textures` 구조를 유지하세요. 바디프리셋은 `Data\CalienteTools\BodySlide\SliderPresets\`입니다.
- Skyrim Fitting System의 Rendered Outfit API v1이 있으면 가슴·유두 보정과 ORefit 규칙이 화면에 보이는 실제/등록 의상 각각의 정보를 사용합니다. 보정 대상 의상이 모두 숨겨지면 보정을 해제하고, 지원 API가 없으면 기존 실제 장비 기준을 유지합니다.
- 수동 선택·색상·기본값은 세이브별로 저장합니다. 기존 설정·배포 JSON·팩과 세이브/코세이브는 업데이트 전에 백업하고 유지하세요.

[한글 소개글 HTML](https://github.com/compilecraftworks/Body-Change-NG/blob/main/docs/NEXUS-DESCRIPTION-v1.2.0-KO.html) · [전체 한글 변경 내역](https://github.com/compilecraftworks/Body-Change-NG/blob/main/docs/NEXUS-CHANGELOG-v1.2.0-KO.txt)
