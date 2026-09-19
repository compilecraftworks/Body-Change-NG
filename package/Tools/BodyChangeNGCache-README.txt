Body Change NG 1.3.1 - Safe cache space maintenance

[한국어]
1. Skyrim과 텍스처 편집기를 종료합니다.
2. 이 폴더의 BodyChangeNGCache.exe를 직접 실행합니다. MO2를 통해 실행하지 마세요.
3. MO2 Overwrite 폴더 또는 그 안의 textures\BodyChangeNG\Cache 폴더를 선택합니다.
4. 먼저 변경 없이 검사 결과를 표시합니다. 정리 확인 창에서 Yes를 누르면 실행합니다.
   기본 선택은 No입니다. 파일이 많으면 검사에 시간이 걸릴 수 있습니다.

- 모든 DDS 경로와 내용은 유지합니다. 동일 내용·수정 시각의 복사본을 하드링크로
  합쳐 저장 공간을 줄이므로 예전 세이브의 경로도 사라지지 않습니다.
- 파일 크기와 SHA-256 비교 후 실제 바이트도 다시 비교합니다.
- 현재 캐시 밖에 하드링크가 있는 파일(원본 스킨팩 등), 읽기 전용 파일,
  잠긴 파일, 링크/정션 폴더, 알 수 없는 구조는 건드리지 않습니다.
- 1.3.1이 생성하는 .bcng-prepare-* / .bcng-link-* 준비용 임시 파일은
  게임에 전달되지 않는 파일입니다. 게임 종료 후 남아 있으면 정리합니다.
- 고유 DDS를 '오래됐다/지금 안 쓴다'는 이유만으로 삭제하지 않습니다.
  다른 세이브의 모든 참조를 판별하는 도구가 아니며, 1.2.7의 고유 캐시도 보존합니다.
- 폴더의 표시 용량은 그대로 보일 수 있습니다. 실제 데이터는 공유됩니다.
  회수량은 할당 크기 기반 추정치이며 이미 하드링크인 캐시는 크게 줄지 않을 수 있습니다.
- 원본 스킨팩, 세이브, 배포 규칙과 설정은 수정하지 않습니다.
- 정리한 캐시 DDS를 직접 덮어쓰지 마세요. 원본 스킨팩을 수정하고 게임에서 새로고침하세요.
  원본 파일은 이 도구가 새로 만드는 공유 링크에 포함하지 않습니다.
- Windows 10/11의 NTFS용 도구입니다. 지원되지 않는 파일 작업은 건너뛰거나 실패로 표시합니다.
  관리자 권한은 요구하지 않습니다. 백신의 잠금 등으로 실패하면 잠금 해제 후 다시 실행하세요.

[English]
1. Close Skyrim and texture editors.
2. Run BodyChangeNGCache.exe directly from this folder, not through MO2.
3. Select MO2's Overwrite, or its textures\BodyChangeNG\Cache directory.
4. A read-only scan runs first. Choose Yes in the confirmation to compact;
   No is the default. Large caches can take time to scan.

All DDS resource paths and bytes remain intact. Only identical copies with
matching last-write times are merged into hard links. Size, SHA-256 and actual
bytes are checked. Older saves keep the same resource names.

External hard links (including source packs), read-only/locked files, reparse
points and unknown layouts are excluded. Known .bcng-prepare-* / .bcng-link-*
staging files produced by 1.3.1 are never handed to the game and can be removed
after Skyrim exits. Unique DDS files are never deleted just for being old or
not currently selected; unique 1.2.7 cache entries remain protected too.

This is not an all-save reference scanner. Source packs, saves, distribution
rules and settings are not changed. Reported reclaimed space is an allocation
estimate. Explorer's folder size may stay the same because hard links share
storage. Already linked or unique textures may offer little or no savings.

Do not edit compacted cache DDS files directly. Edit the source pack, then
Refresh in BCNG. This tool does not link new shared cache data to source packs.
Windows 10/11 and NTFS are required for compaction. No administrator rights are
requested. Unsupported/blocked operations are skipped or reported as failures.

Optional command line (quotes are required for paths containing spaces):
BodyChangeNGCache.exe --scan "D:\My MO2\overwrite"
BodyChangeNGCache.exe --compact "D:\My MO2\overwrite\textures\BodyChangeNG\Cache"
