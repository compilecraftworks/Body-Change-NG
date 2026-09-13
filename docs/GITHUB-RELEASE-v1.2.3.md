## Body Change NG v1.2.3

2026-09-14

### English — changes from v1.2.2

- Separate body-skin previews from committed application metadata and persistent player face overrides, including NPC-distribution checkboxes and Default Skin previews. Previewing the same pack no longer counts as explicit confirmation.
- Add bounded recovery for partially written face channels and native body/hand/foot texture graphs. Retain the last completed body snapshot for face failure recovery; respect later foreign ownership and never load an empty face texture path.
- Distinguish refreshed DDS content and preview/commit mode in request generations. Release completed callbacks and discard recovery snapshots at save-load boundaries while retaining reusable native graphs.
- Preserve partial DDS routing, NPC distribution, save record formats, and the morph fixes from 1.2.1/1.2.2. No new per-frame file scans, timers, or bulk diagnostics.
- Validation: 24 host regression targets, including preview/save codec boundaries and injected native channel-write failures. In-game repeated-switch and save/load validation remains pending.

### 한국어 — v1.2.2 대비 변경

- 바디스킨 미리보기를 확정 적용 기록과 플레이어의 RaceMenu 저장 키에서 분리했습니다. NPC 배포 체크박스와 기본 스킨 미리보기도 포함하며 같은 팩의 미리보기와 확정을 구분합니다.
- 얼굴 채널과 몸·손·발 TXST의 부분 적용 실패 시 복원 처리를 추가했습니다. 얼굴 실패에 대비해 마지막 완료된 몸 상태를 보관하며, 다른 모드의 후속 소유권을 존중하고 빈 얼굴 텍스처 경로는 로드하지 않습니다.
- DDS 새로고침에 따른 내용 변경과 미리보기/확정을 작업 세대에서 구분합니다. 완료 콜백과 로드 경계의 복원 스냅샷을 정리하되 기존 네이티브 그래프 재사용은 유지합니다.
- 부분 DDS 적용, NPC 배포, 기존 세이브 형식과 1.2.1/1.2.2의 모프 수정은 유지합니다. 매 프레임 파일 검사·추가 타이머·대량 진단 로그는 넣지 않았습니다.
- 검증: 미리보기 중 저장·읽기와 채널 쓰기 실패 주입을 포함한 호스트 회귀 테스트 24개. 인게임 반복 전환·저장/로드 확인은 아직 남아 있습니다.

### Installation / 설치

Download **Body Change NG v1.2.3.zip** for MO2. It contains 7 runtime/license/folder-guide files, retaining the asset folders with **no PDB or full documentation bundle**. **Body Change NG v1.2.3 Source.zip** includes matching source and pinned build-required dependencies. `SHA256SUMS v1.2.3.txt` verifies both archives.

Exit Skyrim before updating. Preserve settings, custom distribution rules, asset packs, presets and matching `.ess`/`.skse` saves. Do not overwrite custom rules with the empty starter. This update retains the existing co-save format; a new game or save cleaning is not required solely for this update.

게임을 종료하고 교체하세요. 기존 설정·사용자 배포 규칙·스킨팩·프리셋 및 `.ess`/`.skse` 세이브 쌍을 보존하세요. 배포본의 빈 규칙 파일로 사용자 규칙을 덮어쓰지 마세요. 기존 코세이브 형식을 유지하며 이 업데이트만을 위해 새 게임이나 세이브 정리가 필요하지 않습니다.

Required: runtime-matched **SKSE64, Address Library and RaceMenu**, plus the assets required by each feature. Build body/outfit meshes with **Zeroed Sliders + Build Morphs**. Skyrim SE/AE only; no VR build.

### Guides / 안내

- [English usage](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.3/docs/NEXUS-DESCRIPTION-v1.2.3.md) · [Nexus BBCode](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.3/docs/NEXUS-DESCRIPTION-v1.2.3-EN.bbcode)
- [한국어 HTML](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.3/docs/NEXUS-DESCRIPTION-v1.2.3-KO.html)
- [Build and validation](https://github.com/compilecraftworks/Body-Change-NG/blob/v1.2.3/docs/RELEASE-NOTES-v1.2.3.md)

Release DLL: **1.2.3.0**, 2,776,576 bytes. SHA-256: `DD648BA5E0C07950B05902451ED6A0510AAF0A70E565D27D21D0156277E12EBD`.

All 24 host test executables passed, including 2,000 preview/save codec boundaries, channel-write fault injection, and the retained 4,509 morph-key checks. These are not in-game renderer, timing, or leak measurements.
