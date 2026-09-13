## Body Change NG v1.2.2

2026-09-13

### English — changes from v1.2.1

- Fixed case-only slider-name collisions such as Breasts/breasts, Waist/waist and HipBone/Hipbone. XML low/high endpoints now merge into one morph while retaining the first spelling.
- Fixed previews and breast/nipple outfit correction combining differently cased names incorrectly. Preview cancellation still restores the committed state.
- UNP reverse-defined base sliders now use the same case-insensitive matching. Negative and over-100 XML values remain supported; XML files and the co-save format are unchanged.
- Retained all v1.2.1 features, including OBody/OClothe key cleanup with morph preservation enabled, Futanari Skin favorites and sex-specific NPC distribution controls.
- Includes the automatic checkbox-preview target exclusion for custom followers and elder NPCs added after the initial public v1.2.1 build. Manual actor selection and actual distribution rules are unchanged.

### 한국어 — v1.2.1 대비 변경

- Breasts/breasts, Waist/waist, HipBone/Hipbone처럼 대소문자만 다른 슬라이더의 충돌을 수정했습니다. XML의 small/big 값을 하나의 모프로 병합하며 처음 표기된 이름은 유지합니다.
- 미리보기와 가슴·유두 의상 보정에서 대소문자가 다른 이름을 잘못 합성하던 문제를 수정했습니다. 미리보기 취소 시 확정 상태로 복원하는 동작은 유지합니다.
- UNP 역방향 기본 슬라이더에도 같은 이름 비교를 적용했습니다. 음수·100% 초과 값을 유지하며 원본 XML과 코세이브 형식은 바꾸지 않았습니다.
- 모프 보존을 켠 상태의 OBody/OClothe 키 정리, 후타스킨 즐겨찾기, 성별별 NPC 배포 등 1.2.1의 기존 기능은 모두 유지합니다.
- 최초 공개 1.2.1 이후 추가했던 체크박스 미리보기 액터의 커스텀 팔로워·노인 자동 선택 제외도 포함합니다. 일반 액터 직접 선택과 실제 배포 규칙은 바꾸지 않습니다.

### Installation / 설치

Download **Body Change NG v1.2.2.zip** for MO2. It contains 7 runtime/license/folder-guide files, retains the asset-folder structure, and includes **no PDB or full documentation bundle**. **Body Change NG v1.2.2 Source.zip** is the matching source and pinned build-required dependency tree. `SHA256SUMS v1.2.2.txt` verifies both ZIPs.

게임을 종료하고 교체하세요. 기존 설정·사용자 배포 규칙·스킨팩·프리셋과 `.ess`/`.skse` 세이브 쌍은 보존하세요. 배포본의 빈 규칙 파일로 사용자 규칙을 덮어쓰지 마세요. 캐릭터 바디와 의상은 **Zeroed Sliders + Build Morphs**로 준비해야 합니다.

Exit Skyrim before updating. Preserve settings, custom distribution rules, asset packs, presets and matching `.ess`/`.skse` saves. Do not overwrite customized rules with the empty starter. Build both body and outfits with **Zeroed Sliders + Build Morphs**.

Required: runtime-matched **SKSE64, Address Library and RaceMenu**, plus the assets needed for each feature. Skyrim SE/AE only; no VR build.

Release build and all **24 automated test executables passed**, including **4,509 morph-key checks** and case-variant XML endpoint/UNP conversion tests. This build's in-game verification is pending.

### Guides / 안내

- [English usage](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.2/docs/NEXUS-DESCRIPTION-v1.2.2.md) · [English Nexus BBCode](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.2/docs/NEXUS-DESCRIPTION-v1.2.2-EN.bbcode)
- [한국어 HTML 소개글](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.2/docs/NEXUS-DESCRIPTION-v1.2.2-KO.html) · [한국어 체인지로그](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.2/docs/NEXUS-CHANGELOG-v1.2.2-KO.txt)
- [Build and verification notes](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.2/docs/RELEASE-NOTES-v1.2.2.md)
