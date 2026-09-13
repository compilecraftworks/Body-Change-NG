## Body Change NG v1.2.1

Updated 2026-09-13: automatic NPC checkbox preview now excludes custom followers and elder NPCs. / NPC 배포 체크박스 모드 자동 액터 선택에서 커스텀 팔로워·노인 제외를 추가 반영했습니다.

### English — changes from v1.2.0

- **OBody morph cleanup:** preset confirmation, automatic body application and Default Body also remove the exact `OBody` and `OClothe` keys, even with **Preserve other mods' morphs** enabled. Unrelated morph keys follow the preservation option. Preview remains reversible; default preview now scans morph values once instead of four times.
- **Existing saves:** BCNG body states are re-evaluated once under the new cleanup policy. The co-save format remains unchanged; no new game or save cleaning is required solely for this update. This does not disable redistribution by a running OBody installation.
- **Futanari Skin favorites:** stars and the Favorites filter now work in both normal and NPC-distribution lists, with persistent, separate ERF/TRX/UBE TRX entries.
- **NPC distribution controls:** Female / Male / Distribute / Cancel distribution. Female is selected on entry. Sex buttons select the nearest matching loaded NPC, falling back to the player; futanari requires a registered female futanari NPC. Catalogs and new rules retain the selected distribution sex independently of the preview actor, and body/skin candidates follow configured NPC body types. Switching sex clears previous checkmarks and previews. Automatic preview targeting in NPC-distribution checkbox mode skips custom followers and elder NPCs, including futanari targets. Manual actor selection and actual distribution rules are unchanged.
- **Checkbox previews:** body/skin/futanari lists preview the last compatible selection; overlays preview checked entries cumulatively with editable colors. Distribute opens the conditions popup; rules are saved only by the immediate/next-launch distribution buttons.
- Updated English Nexus BBCode and Korean HTML guides/changelogs, with full asset paths and BodySlide preparation.

### 한국어 — v1.2.0 대비 변경

- **OBody 모프 정리:** 프리셋 확정·자동 바디 적용·기본 바디 복원 시 `OBody`·`OClothe` 두 키를 제거합니다. **다른 모드의 모프 보존**을 켜도 두 키는 제거하고, 그 외 모프는 옵션을 따릅니다. 미리보기는 취소 가능하며 기본 복원 미리보기 순회를 4회에서 1회로 줄였습니다.
- **기존 세이브:** BCNG 바디 기록을 새 정리 정책으로 한 번 재평가합니다. 코세이브 형식은 그대로이며 이번 업데이트 때문에 새 게임이나 세이브 정리를 할 필요는 없습니다. 실행 중인 OBody의 자체 재배포를 끄는 기능은 아닙니다.
- **후타스킨 즐겨찾기:** 일반·NPC 배포 목록에 별 버튼과 즐겨찾기 필터를 추가하고, ERF·TRX·UBE TRX 항목을 구분해 설정에 저장합니다.
- **NPC 배포 도구 행:** 여성 / 남성 / 배포 하기 / 배포 취소. 진입 시 여성이 기본 선택됩니다. 가까운 해당 성별 NPC를 선택하고 없으면 플레이어로 돌아갑니다. 후타는 등록된 여성 후타 NPC만 자동 선택합니다. 배포 목록·새 규칙의 성별은 미리보기 액터와 분리하고, 바디·스킨에는 설정된 배포 바디 타입도 반영합니다. 성별 전환 시 이전 체크·미리보기를 정리합니다. NPC 배포 체크박스 모드의 자동 미리보기 액터 선택에서는 후타를 포함해 커스텀 팔로워·노인 NPC를 제외합니다. 일반 액터 직접 선택과 실제 배포 규칙은 바꾸지 않습니다.
- **체크박스 미리보기:** 바디·스킨·후타는 마지막 호환 항목, 오버레이는 체크한 항목들을 색상과 함께 누적 미리보기합니다. 배포 하기는 조건 팝업을 열며, 즉시 배포/다음 게임 배포 버튼을 눌러야 규칙이 저장됩니다.
- 전체 파일 경로와 BodySlide 준비를 포함해 영문 Nexus BBCode·한글 HTML 소개글과 양쪽 체인지로그를 업데이트했습니다.

### Installation / 설치

Download **Body Change NG v1.2.1.zip** for MO2. It contains 7 runtime/license/folder-guide files, retains the asset-folder structure, and includes **no PDB or full documentation bundle**. **Body Change NG v1.2.1 Source.zip** is the matching source and pinned build-required dependency tree. `SHA256SUMS v1.2.1.txt` verifies both ZIPs.

게임을 종료하고 교체하세요. 기존 설정·사용자 배포 규칙·스킨팩·프리셋과 `.ess`/`.skse` 세이브 쌍은 보존하세요. 배포본의 빈 규칙 파일로 사용자 규칙을 덮어쓰지 마세요. 캐릭터 바디와 의상은 **Zeroed Sliders + Build Morphs**로 준비해야 합니다.

Exit Skyrim before updating. Preserve settings, custom distribution rules, asset packs, presets and matching `.ess`/`.skse` saves. Do not overwrite customized rules with the empty starter. Build both body and outfits with **Zeroed Sliders + Build Morphs**.

Required: runtime-matched **SKSE64, Address Library and RaceMenu**, plus the assets needed for each feature. Skyrim SE/AE only; no VR build.

Release build and all **24 automated test executables passed**, including **2,936 morph-key checks**. This build's in-game verification is pending.

### Guides / 안내

- [English usage](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.1/docs/NEXUS-DESCRIPTION-v1.2.1.md) · [English Nexus BBCode](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.1/docs/NEXUS-DESCRIPTION-v1.2.1-EN.bbcode)
- [한국어 HTML 소개글](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.1/docs/NEXUS-DESCRIPTION-v1.2.1-KO.html) · [한국어 체인지로그](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.1/docs/NEXUS-CHANGELOG-v1.2.1-KO.txt)
- [Build and verification notes](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.1/docs/RELEASE-NOTES-v1.2.1.md)
