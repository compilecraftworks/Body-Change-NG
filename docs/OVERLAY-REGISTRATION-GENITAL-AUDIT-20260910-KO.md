# 오버레이 등록 유지 · 성기 경로 · RaceMenu 호환 점검

## 이번 시험판의 범위

- 사용자 확인으로 정상화된 일반 Body/Hands/Feet/Face의 Native TXST 알고리즘은 변경하지 않았다.
- 오버레이는 RaceMenu 등록/재질 갱신 API를 사용한다. BCNG가 별도 오버레이 렌더러를 구현하거나 소실된 키를 재생성하는 복구 루프를 두지 않는다.
- SOS/TNG 남성 및 여성 후타나리는 기존의 동일 Live Material Adapter를 유지한다. 원본 ARMO/ARMA 교체, Equip/Unequip, TNG 함수 후킹, 성기용 RaceMenu 텍스처 키 생성을 추가하지 않았다.
- 공통 작업 큐의 취소 누락은 수정했다. 따라서 Native 스킨을 포함한 다른 기능도 **취소된 후속 작업**은 실행되지 않는다. 정상 작업의 스케줄링/예산은 그대로다.

## 확인하고 수정한 결함

1. **등록 전 전체 삭제**: 기존 오버레이의 모든 DDS/색상/알파 키를 삭제하고 새 값을 쓰던 경로를 제거했다. v1/v2 모두 RaceMenu의 키별 교체 기능으로 새 값을 등록한 뒤, 새 팩에서 빠진 이전 보조 DDS 키만 제거한다. 단순 DDS 선택/색상 변경에는 Revert 작업을 예약하지 않는다. 전체 삭제는 명시적 기본값/미리보기 취소에 따른 제거 경로에만 남는다.
2. **적용 전 선택 상태 확정**: 지연된 RaceMenu 등록을 예약한 직후 BCNG의 live/저장 선택을 미리 바꾸던 순서를 수정했다. 등록이 끝난 후의 콜백에서 상태를 공개한다. 이전 추정 경로로 되돌아가며 소유권을 맞추던 `PreviewState.replaced`도 제거했다.
3. **취소된 후속 작업 실행**: actor ID가 0인 continuation은 원래 액터의 lease를 가지고 있었지만, `CancelActor` 이후 `Take`/`Pump`에서 취소 여부를 검사하지 않았다. 이제 Take가 취소된 continuation을 제거하고 Pump도 실행 직전에 다시 검사한다. 추가한 오버레이 취소 테스트는 수정 전 큐에서 실패했고 수정 후 통과했다.
4. **색상 변경 과잉 차단**: RaceMenu 등록 경로로 해당 슬롯 소유권을 확인한 뒤에도 라이브 diffuse 표시까지 일치해야 색상 API를 호출하던 조건을 제거했다. 저장된 다른 모드의 DDS가 발견되면 여전히 덮어쓰지 않는다. 색상만 바꿀 때 DDS를 재등록/재업로드하지 않는다.
5. **성기 식별 근거 소실/오분류**: BCNG 캐시 DDS를 UBE의 근거로 삼던 분기를 제거했다. Live Adapter가 현재 지오메트리에 보관한 원본 diffuse를 남성/여성 성기 분류에 사용한다. 원본 경로가 애드온 식별 근거인 generic-name 메시도 반복 선택 후 동일하게 식별할 수 있다. 지오메트리 원시 포인터를 장기 보관하지 않는다.
6. 이전 `RecoverMissingNodeMetadata`, 관련 허용/색상 비교 함수 및 복구 테스트를 제거했다. 새 변경은 소실 후 복구가 아니다.

## 실제 설치 및 공식 이력 확인

- 활성 프로필: `D:\TuLED13E\File Mod Skyrim SE\profiles\TuLED(SL)`.
- 실제 로드된 skee64: `[LED]UBE 2.0\SKSE\Plugins\skee64.dll`, MD5 `f44b9a36364d6547ec625cc617089f3e`. SlaveTatsNG의 로드 기록/주소 표와 일치한다. 일반 Race Menu 폴더의 DLL과 혼동하지 않았다.
- 이 DLL의 Override vtable 및 Add/GetNodeOverride 디스어셈블리 확인: 두 함수 모두 액터 FormID 32비트를 사용하며 호출부 시그니처는 v1 계약과 맞는다. 공식 구버전의 VM handle 내부 구현과 동일하다고 가정하지 않는다.
- 활성 섹랩용 skee64.ini: overlays/face overlays 활성, 슬롯 Body 32 / Hands 12 / Feet 12 / Face 13. 사용자 ini는 수정하지 않았다.
- 설치된 ERF/TRX NIF의 문자열에서 `CBBE Schlong`, `CBBE_Schlong`, 각 애드온 DDS 경로를 대조했다. 이것은 파일의 식별 문자열 확인이며 런타임 Form/Geometry 렌더 테스트를 대체하지 않는다.
- `audit-racemenu-history.py`: 공식 저장소의 11개 고정 커밋을 사용한다. 0.4.12, 0.4.14, 0.4.16 계열, 초기 AE, 중간 리팩터링, 1.6.640/GOG, 공개 v2 도입, 1.6.1170 및 마지막 점검 소스까지 확인했다. 호출하는 Overlay/Override/BodyMorph 함수 순서·인자·반환 형식 및 v2 Variant 계약 모두 일치했다. 정확한 전체 커밋 해시는 스크립트에 고정되어 있다.
- 배포 파일 버전명을 화이트리스트로 사용하지 않는다. BodyMorph v4/v5, Overlay/Override v1/v2를 각각 사용한다. 미확인 상위 인터페이스는 기존 하위 prefix만 사용한다. 이것은 forward-compatibility 정책이며 알 수 없는 DLL의 ABI 보증은 아니다.
- Skyrim 자체의 SE/AE 런타임/레이아웃 지원 범위는 `RuntimeCompatibility.h`의 검증된 표를 유지한다. Skyrim용 버전이 맞지 않는 RaceMenu DLL을 BCNG가 호환되게 만들 수는 없다. VR 대상은 없다.

## 검증

- SE/AE flat Release 빌드 성공.
- 전체 17개 테스트 타깃 통과.
- 오버레이: 기존 등록 유지, 보조 DDS 제거 순서, 64회 반복 교체/불투명도 변경, 취소된 등록 콜백의 선택값 유지.
- 성기: ASTR v4 저장/복원, 미장착 상태에서 후타 선택 유지, 재장착/3D 재구축 작업 정책, NPC별 선택 분리, 원본 DDS 기반 분류 및 캐시의 UBE 오분류 차단.
- 기존 바디/스킨/배포/가져오기/런타임 경계/텍스처 경로 격리 테스트도 통과. 저장 포맷과 UI/규칙 데이터는 변경하지 않았다.
- `git diff --check` 통과.

## 아직 확정할 수 없는 부분

- 기존 로그에서 모든 오버레이 키가 absent가 된 정확한 실행 주체는 확정하지 못했다. 이번에 실제 코드 결함을 제거했지만, 과거 증상이 전부 이 결함 때문이었다고 단정하지 않는다. 외부 모드가 키를 지웠다는 증거도 없다.
- 얼굴의 최종 가시성, 장시간 선택 후 정지 재현 여부, SOS/TNG의 실제 장착 메시 표시/게임 저장·종료·로드는 이번 오프라인 테스트로 검증하지 않았다.
- 11개 **소스 지점의 API 계약 검사**를 모든 배포 DLL의 실행 검증 또는 모든 RaceMenu 버전의 게임 테스트 완료라고 표현하지 않는다.

## 시험 DLL

- 빌드: `build/v1.2.0/windows/x64/release/BodyChangeNG.dll`
- SHA256: `4A692ED664B3CFD9EEAA730DDBECE83BF0C956D393EA4FF6CB3660DCDE871223`
- Skyrim 프로세스가 없는 것을 확인한 뒤 툴레드 `mods/Body Change NG/SKSE/Plugins/BodyChangeNG.dll`만 교체했고, 설치본 SHA256도 위 값과 일치했다. 공개 GitHub Release 및 떼껄룩은 변경하지 않았다. ini/배포 규칙/팩/스크립트도 변경하지 않았다.
