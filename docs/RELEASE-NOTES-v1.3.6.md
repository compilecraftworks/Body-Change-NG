# Body Change NG v1.3.6

## English

### Morph refresh and restoration

- Reduced duplicate visual updates by coalescing body, outfit, preview and cleanup refreshes into one actor job.
- Save-load and RaceMenu restoration now preserve surviving committed morph values instead of repeatedly recommitting the preset. Missing keys or changed preset/options still use the normal repair path.
- Explicit preset selection, preview cancellation, Default/reset and settings-popup reapplication retain their existing behavior. No additional OverlayFix dependency is required.

### Compatibility and validation

- Updated the pinned build dependency to CommonLibSSE-NG v11.0.0 in the SE/AE-only flat configuration.
- Release build and all 48 offline regression executables passed, including the 12 declared game runtimes and RaceMenu BodyMorph v4/v5 prefix fixtures.
- The existing targeted UBE SE RaceMenu fix remains limited to its verified DLL. These checks do not confirm the reported AE inflation's root cause or in-game compatibility with every RaceMenu binary.

### Updating

- Fully exit the game before replacing the mod, then restart. Preserve settings, customized distribution rules, favorites, skin packs and texture caches. Supported game versions and save formats are unchanged.

## 한국어

### 모프 갱신·복원

- 바디·의상·미리보기·취소 뒤의 화면 갱신을 액터당 하나의 작업으로 합쳐 중복 갱신을 줄였습니다.
- 세이브 로드와 RaceMenu 복원 시 기존 확정 모프 값을 보존합니다. 모프 키가 없거나 프리셋·설정이 변경된 경우에는 정상 확정 경로로 복구합니다.
- 직접 프리셋 적용, 미리보기 취소, Default·초기화와 설정 창의 즉시 재적용은 기존 동작을 유지합니다. OverlayFix를 추가 의존성으로 요구하지 않습니다.

### 호환성·검증

- 빌드 의존성을 CommonLibSSE-NG v11.0.0으로 고정하고 SE/AE 전용 flat 구성을 사용합니다.
- Release 빌드와 오프라인 회귀 검사 48개가 모두 통과했습니다. 지원 게임 12개 버전과 RaceMenu BodyMorph v4/v5 공통 ABI 검사도 포함합니다.
- 기존 UBE SE RaceMenu 호환 수정은 확인된 특정 DLL에만 적용합니다. 이번 검사로 AE 체형 누적 제보의 실제 원인이나 모든 RaceMenu 배포본의 인게임 호환성을 확정한 것은 아닙니다.

### 업데이트 안내

- 게임을 완전히 종료한 뒤 교체하고 다시 실행하세요. 설정·사용자 배포 규칙·즐겨찾기·스킨팩·텍스처 캐시는 보존하세요. 지원 게임 버전과 저장 형식은 그대로입니다.
