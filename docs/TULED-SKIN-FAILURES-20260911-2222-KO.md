# 툴레드 22:22 실행: 얼굴 / BnP CBBE / SOS ERF 실패

## 범위와 결과

사용자가 인게임에서 확인한 실패 기록이다. **수정 완료나 실기 통과 기록이 아니다.**
시험 DLL SHA-256: `1DC3260B1A0755C25210076A310F000D3DBBF0679F3AD3ED5BF4FA58638E69BC`.
종료 후 로그를 `build/audits/tulederf-face-20260911/BodyChangeNG.log`에 보존했다.
이번 조사에서 운영 코드 / MO2 DLL / 공개 Release는 바꾸지 않았다.

## SOS ERF: 실제 TXST 공급 전에 차단됨

- 22:28:34.820: `CBBE Schlong` 대상에서 팩토리가 `FF00091B` FormID를 반환했다.
- `NativeAddonSkinBackend.cpp::MakeTexture`는 FormID가 0이어야 한다고 가정하여 반환값을 거절했다.
- 동일 시각 `stage=texture-preparation`. 이후 선택은 `stage=baseline-unavailable`로 중단된다.
- 이후 게시 로그의 `prior-supplies=0`: 선택은 등록되지만 실제 네이티브 방문자에 선택 TXST가 공급되지 않았다.
- `peniscollision`의 `shader-feature-or-flag` 제외는 피부 노드 실패와 구분한다.

팩토리 생성물이 항상 익명 FormID 0이라는 종전 설명은 실제 관측과 맞지 않는다.
그러나 이 검사만 제거하기 전에 생성 폼의 등록 여부와 최종 참조 해제 시 폼 정리를 확인해야 한다.
FormID만 0으로 덮거나 전역 폼 테이블에서 임의 제거하는 변경은 하지 않는다.
첫 실패 후 baseline-unavailable이 계속되는 이유도 별도 확인 대상이다.
같은 백엔드를 쓰는 남성 SOS/TNG에 잠재적으로 해당하지만, 남성/TNG의 동일 실패를 실기로 확인한 것은 아니다.

## BnP CBBE: 부위 요구 조건이 전체 적용을 차단함

- 22:25:02.256부터 BnP CBBE의 `GraphSupportsPlan` 검사 실패가 반복된다.
- BnP UNP는 같은 실행에서 네이티브 바디 적용 단계까지 진행한다.
- CBBE 팩은 `cbbe-genital-anal=4`, UNP 팩은 `unp-genital-anal=4`.
  현재 CBBE 런타임은 전자의 별도 TXST 역할을 필수로 요구한다.
- 검사 대상은 ARMA의 skinTextures / swap-list TXST 그래프다. NIF에만 있는 개별 재질은 포함되지 않는다.

실제 설치 Body Slide Output의 `meshes/actors/character/character assets/femalebody_1.nif`를
`tools/audit-addon-nif-shaders.py`로 파싱했다. SHA-256:
`28f3a89e1810fa0977a30d5bda971554e21a3dd83062980c034619c3b52aea18`.

| 셰이더 블록 | DDS diffuse | type / flags1 | 네이티브 피부 플래그 |
|---|---|---|---|
| 121 | female/femalebody_1.dds | 5 / 82601303 | 있음 |
| 127 | female/femalebody_etc_v2_1.dds | 5 / 82401303 | 없음 |
| 133 | dw/ubeanus/malebody_1.dds | 5 / 82400303 | 없음 |

이는 셰이더 블록 번호이며 Geometry 이름이 아니다. 모든 의상 / 모든 3BA 설치에 일반화하지 않는다.
특히 etc 재질은 본체와 별개이고 같은 Skin Tint 플래그를 갖지 않는다.
검사만 제거하면 해당 DDS를 실제 적용하지 않은 채 팩 전체 성공으로 보고할 수 있다.
해당 NIF 재질의 정확한 네이티브 교체 경로를 해결한 뒤 전체 적용 조건을 정리해야 한다.
UNP 팩이 검사 통과했다는 사실은 UNP 전용 DDS까지 CBBE 메시에서 올바르게 적용됐다는 뜻이 아니다.

## 얼굴 / 기본값 복원: 아직 원인 미확정

사용자 스크린샷에는 일반 몸과 보라색 얼굴이 함께 보인다. 기본값 복원도 잘못된 상태로 보고됐다.
로그에는 `paths/required textures verified` 및 `restored native default Skin Armor`가 있다.
후자는 몸 그래프 포인터 복원 기록이며 얼굴 최종 렌더링 성공을 의미하지 않는다.
현재 얼굴 검사는 경로와 diffuse/normal 객체 및 rendererTexture 존재를 확인하지만,
로드된 리소스 정체성 / 대체 텍스처 / detail·subsurface·specular / 최종 픽셀은 증명하지 못한다.
따라서 이번 결과로 이전 진단 변경의 충분성이 부정되었다. 성공 로그를 근거로 사용자 증상을 부정하지 않는다.

공식 RaceMenu [ShaderUtilities.cpp](https://github.com/expired6978/SKSE64Plugins/blob/master/skee64/ShaderUtilities.cpp)는
경로 읽기와 실제 텍스처 로드를 구분하며, 채널별로 다른 재질 포인터를 갱신한다.
다음 얼굴 조사는 필수 두 채널의 non-null 여부를 반복 확인하는 것이 아니라
실제 로드된 텍스처 이름 / 채널 / 복원 기준을 비교해야 한다.
게임이 종료되어 현재 재질의 메모리 스냅샷은 확보하지 못했다.
