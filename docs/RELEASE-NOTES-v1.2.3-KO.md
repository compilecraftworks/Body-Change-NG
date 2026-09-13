# Body Change NG v1.2.3

## v1.2.2 대비 변경

- 바디스킨 미리보기를 확정 적용 기록과 플레이어의 RaceMenu 저장 키에서 분리했습니다. NPC 배포 체크박스와 기본 스킨 미리보기도 포함하며 같은 팩의 미리보기와 확정을 구분합니다.
- 얼굴 채널과 몸·손·발 TXST의 부분 적용 실패 시 복원 처리를 추가했습니다. 얼굴 실패에 대비해 마지막 완료된 몸 상태를 보관하며, 다른 모드의 후속 소유권을 존중하고 빈 얼굴 텍스처 경로는 로드하지 않습니다.
- DDS 새로고침에 따른 내용 변경과 미리보기/확정을 작업 세대에서 구분합니다. 완료 콜백과 로드 경계의 복원 스냅샷을 정리하되 기존 네이티브 그래프 재사용은 유지합니다.
- 부분 DDS 적용, NPC 배포, 기존 세이브 형식과 1.2.1/1.2.2의 모프 수정은 유지합니다. 매 프레임 파일 검사·추가 타이머·대량 진단 로그는 넣지 않았습니다.
- 검증: 미리보기 중 저장·읽기와 채널 쓰기 실패 주입을 포함한 호스트 회귀 테스트 24개. 인게임 반복 전환·저장/로드 확인은 아직 남아 있습니다.

## 업데이트

게임을 종료하고 교체하세요. 기존 설정·사용자 배포 규칙·스킨팩·프리셋 및 `.ess`/`.skse` 세이브 쌍을 보존하세요. 배포본의 빈 규칙 파일로 사용자 규칙을 덮어쓰지 마세요. 기존 코세이브 형식을 유지하며 이 업데이트만을 위해 새 게임이나 세이브 정리가 필요하지 않습니다.

## 빌드와 검증

Release DLL: **1.2.3.0**, 2,776,576 bytes. SHA-256: `DD648BA5E0C07950B05902451ED6A0510AAF0A70E565D27D21D0156277E12EBD`.

All 24 host test executables passed, including 2,000 preview/save codec boundaries, channel-write fault injection, and the retained 4,509 morph-key checks. These are not in-game renderer, timing, or leak measurements.

[전체 사용 안내](NEXUS-DESCRIPTION-v1.2.3-KO.html) · [상세 점검 기록](SKIN-TRANSACTION-v1.2.3-KO.md)
