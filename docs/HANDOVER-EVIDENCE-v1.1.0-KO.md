# Body Change NG 1.1.0 인수인계 증거 부록

작성일: 2026-09-04 (Asia/Seoul)

기준 HEAD: `834e007cc3db64fb8024631e3588ea7af86068d7`

[인수인계 본문](HANDOVER-v1.1.0-KO.md)

이 부록은 문서 작성 직전 저장소의 읽기 전용 스냅샷이다. Git 명령으로 수집한 **54개 전체 커밋과 모든 변경 파일**을 싣는다. 이 인수인계 문서 자체를 추가하는 이후 변경은 기준 스냅샷에 포함되지 않는다. 변경량에는 이름 변경 때의 삭제/추가 쌍이 포함된다. 테스트 통과 범위와 인게임 미검증 사항은 본문 14~16장을 따른다.

## 1. 현재 Git 기준

- 저장소: `C:\Users\yunha\Desktop\Body Change NG`
- 문서 작성 직전 `git status --short`: 출력 없음(clean).
- 로컬/원격 main: `834e007cc3db64fb8024631e3588ea7af86068d7`
- remote: https://github.com/compilecraftworks/Body-Change-NG.git
- GitHub release: https://github.com/compilecraftworks/Body-Change-NG/releases/tag/v1.1.0
- 인수인계 작성 중 remote/ref와 asset digest를 읽기 전용 재확인. GitHub 공개 상태는 draft=false / prerelease=false.

```text
602d7d924f82e2f1ffd88c7e7334edc58e4eeec9 refs/tags/v1.0.0
888fae722fa240f12f61fbf20c5d0779cd8e1c41 refs/tags/v1.0.0^{}
e9204e838406c5be1ce0aaf3e49d2b78d1c5a9ed refs/tags/v1.1.0
26408f21944131f8ccf9957de6cd3139318cf2c0 refs/tags/v1.1.0^{}
c014c0bec2627892b6eb61c730cbabba06ae69da refs/tags/v1.1.0-packaging.1
834e007cc3db64fb8024631e3588ea7af86068d7 refs/tags/v1.1.0-packaging.1^{}
```

`^{}`는 annotated tag가 실제 가리키는 commit이다. v1.0.0은 최종 이름/아트워크 반영 커밋 888fae7, v1.1.0은 26408f2, packaging.1은 834e007을 가리킨다.

## 2. 아티팩트 증거

| 상대 경로 | 바이트 | SHA-256 |
| --- | ---: | --- |
| `build\v1.1.0\windows\x64\release\BodyChangeNG.dll` | 2,258,944 | `8BF779DCEA540EA8B34EF9778DEBABBE4A7CE4A5F6745D3A15EE4F8F95717732` |
| `release\Body Change NG v1.1.0.zip` | 1,098,586 | `821D7FF39EE12AA2F058B4BD60FE6694A5F345283924044659C7741994610499` |
| `release\Body Change NG v1.1.0 Source.zip` | 5,353,027 | `D3953DA061D6C564B648F93D0D258A70163F18219AD632678A280D886F25B121` |

MO2 `D:\TuLED13E\File Mod Skyrim SE\mods\Body Change NG\SKSE\Plugins\BodyChangeNG.dll` SHA-256:

`8BF779DCEA540EA8B34EF9778DEBABBE4A7CE4A5F6745D3A15EE4F8F95717732`

GitHub 업로드명은 `Body-Change-NG-v1.1.0.zip` / `Body-Change-NG-v1.1.0-Source.zip`. 위 두 ZIP과 size/digest 일치. GitHub `SHA256SUMS-v1.1.0.txt`는 하이픈 파일명 기준이며 그 파일의 SHA-256은 `715E742048261BE0934B9266C9B6C474BE40D2FE626A45105E2DDF4CEA2EE84F`이다.

### 2.1 실제 binary ZIP 엔트리

| 경로 | 압축 전 바이트 |
| --- | ---: |
| `LICENSE` | 35,803 |
| `THIRD_PARTY_NOTICES.md` | 9,456 |
| `BodySkin/README.txt` | 7,405 |
| `TintMask/README.txt` | 1,281 |
| `SKSE/Plugins/BodyChangeNG.dll` | 2,258,944 |
| `SKSE/Plugins/BodyChangeNGdistribution.json` | 3,908 |
| `CalienteTools/BodySlide/SliderPresets/README.txt` | 1,039 |

정확히 7개. 기존 MO2 폴더에는 과거 문서가 남아 있으므로 설치 폴더와 새 ZIP의 문서 구성은 다르다.

### 2.2 실제 source ZIP

엔트리 수: 2712. dependency include/source/license가 대부분을 차지한다. 캐시/바이너리까지 포함하는 전체 개발 폴더 압축이 아니다.

`SOURCE-REVISION.txt` 실제 내용:

```text
Body Change NG 1.1.0
Git revision: 834e007cc3db64fb8024631e3588ea7af86068d7
Dependency pins: DEPENDENCIES.md and xmake-requires.lock
```

## 3. 전체 커밋 색인

시간은 Git의 author ISO timestamp이며 +09:00 기준이다. 각 커밋 제목은 원문을 보존한다. 설명상 기능 단계와 뒤집힌 결정은 본문 3~4장을 참조한다.

| 번호 | 날짜·시간 | 커밋 | 원문 제목 | 변경 경로 수 |
| ---: | --- | --- | --- | ---: |
| 1 | 2026-09-01 16:44:54+09:00 | [d6957cd](https://github.com/compilecraftworks/Body-Change-NG/commit/d6957cd304ad58d28ca0541660ab175d10396e7e) | Establish Body Changer NG source baseline | 72 |
| 2 | 2026-09-01 17:28:14+09:00 | [cc2a2cf](https://github.com/compilecraftworks/Body-Change-NG/commit/cc2a2cfdb25dccd35dedfa51650df8bba7b7851d) | Persist actor selections and harden distribution UI | 29 |
| 3 | 2026-09-01 18:50:44+09:00 | [f5e7c02](https://github.com/compilecraftworks/Body-Change-NG/commit/f5e7c028472bf6e8cd0d7ad7a35acff5b54657a4) | Restore combo previews and text editing | 6 |
| 4 | 2026-09-01 18:54:05+09:00 | [5ee5508](https://github.com/compilecraftworks/Body-Change-NG/commit/5ee5508eea7965d59676fd310e1c98eca0a98d01) | Make distribution panes resizable | 1 |
| 5 | 2026-09-01 19:02:17+09:00 | [f1d20a5](https://github.com/compilecraftworks/Body-Change-NG/commit/f1d20a5dcb4f4ad7defc2851ef5c0b2a95f36358) | Add closer tint detail camera framing | 3 |
| 6 | 2026-09-01 19:27:11+09:00 | [7f4b920](https://github.com/compilecraftworks/Body-Change-NG/commit/7f4b920363cef0983e5505c00515501ca235ed67) | Unify tint framing and balance character sides | 3 |
| 7 | 2026-09-01 19:35:27+09:00 | [fa38474](https://github.com/compilecraftworks/Body-Change-NG/commit/fa38474cd7c118f8d1eef315545d654091b15eee) | Keep tint zoom inside screen bounds | 1 |
| 8 | 2026-09-01 19:37:10+09:00 | [6273fc4](https://github.com/compilecraftworks/Body-Change-NG/commit/6273fc447cbe7b370295349fdbfb22e2ea3bd429) | Preserve right tint framing | 1 |
| 9 | 2026-09-01 19:48:25+09:00 | [b8f2c8a](https://github.com/compilecraftworks/Body-Change-NG/commit/b8f2c8aa5538b321272eb081a0d1ad299e39c02b) | Reframe normal and tint cameras | 1 |
| 10 | 2026-09-01 20:20:45+09:00 | [9e57dfc](https://github.com/compilecraftworks/Body-Change-NG/commit/9e57dfc72433992385a0cc36dff3bd09b262d894) | Finalize menu camera and release layout | 6 |
| 11 | 2026-09-01 21:00:57+09:00 | [85f6677](https://github.com/compilecraftworks/Body-Change-NG/commit/85f667774c0b3bed8f1ec9e7f9084b84a52a9adf) | Keep outfit-named BodySlide presets visible | 2 |
| 12 | 2026-09-01 21:22:55+09:00 | [c8c1332](https://github.com/compilecraftworks/Body-Change-NG/commit/c8c1332f14824f84ee6d9c83735909957f4a1923) | Set final menu camera framing | 1 |
| 13 | 2026-09-01 21:35:02+09:00 | [b9e9f3f](https://github.com/compilecraftworks/Body-Change-NG/commit/b9e9f3f7b32b53d650b8f54d3c1a6a975ba7e985) | Stabilize camera before restoring menu pause | 3 |
| 14 | 2026-09-01 21:46:10+09:00 | [c5847e1](https://github.com/compilecraftworks/Body-Change-NG/commit/c5847e1abd8a94f86c44771ba03cc85e9aec5728) | Synchronize render world FOV in menu | 1 |
| 15 | 2026-09-01 21:52:06+09:00 | [e3e08aa](https://github.com/compilecraftworks/Body-Change-NG/commit/e3e08aa54d2993ee19740f34fdb75edd4a15b801) | Remove menu camera pause settle experiment | 3 |
| 16 | 2026-09-01 22:56:19+09:00 | [832219d](https://github.com/compilecraftworks/Body-Change-NG/commit/832219def5dc7e90c945a904b2701a7d75b9a679) | Neutralize additive FOV in menu camera | 4 |
| 17 | 2026-09-02 00:10:51+09:00 | [d135cbf](https://github.com/compilecraftworks/Body-Change-NG/commit/d135cbf075eacc78ec89d0acd773dd0abc13129d) | Harden camera state and distribution UI | 13 |
| 18 | 2026-09-02 00:15:27+09:00 | [e81565a](https://github.com/compilecraftworks/Body-Change-NG/commit/e81565adf89054220c960f9052dda4ed4052f76c) | Sync tint preview and final camera pitch | 4 |
| 19 | 2026-09-02 00:29:32+09:00 | [eab8a94](https://github.com/compilecraftworks/Body-Change-NG/commit/eab8a9455c5cbe62bb25740b4293cb16be993c8c) | Release Body Changer NG v1.0.0 | 11 |
| 20 | 2026-09-02 01:02:31+09:00 | [3ab178d](https://github.com/compilecraftworks/Body-Change-NG/commit/3ab178dd9cc9438edb32ae2f8a49b9ee1ab73338) | Document OBody JSON compatibility and native UI advantages | 3 |
| 21 | 2026-09-02 01:09:40+09:00 | [d81164e](https://github.com/compilecraftworks/Body-Change-NG/commit/d81164e2f1c790dd89f524f5c56a4a9b8eff2233) | Keep distribution actions on one row | 4 |
| 22 | 2026-09-02 01:20:05+09:00 | [0092e8e](https://github.com/compilecraftworks/Body-Change-NG/commit/0092e8e845bd948df7de0956d8d0a4aa34baad9d) | Add Nexus upload copy and update history | 5 |
| 23 | 2026-09-02 01:31:01+09:00 | [bfa805e](https://github.com/compilecraftworks/Body-Change-NG/commit/bfa805e925b80b95fd5ac3b04cbd42acf18895d3) | Refine tint framing and Nexus feature copy | 10 |
| 24 | 2026-09-02 01:38:40+09:00 | [89b6081](https://github.com/compilecraftworks/Body-Change-NG/commit/89b6081e01a383ac62e192f0028c652573068cbf) | Simplify Nexus short descriptions | 2 |
| 25 | 2026-09-02 01:41:45+09:00 | [8b902cf](https://github.com/compilecraftworks/Body-Change-NG/commit/8b902cfe810c2aa1dd356f9256618482e8499236) | Fine tune tint camera framing | 1 |
| 26 | 2026-09-02 01:44:13+09:00 | [784a983](https://github.com/compilecraftworks/Body-Change-NG/commit/784a9839bcaab976c43f0cc9768d16d98ba0ce6a) | Finalize Nexus short description | 1 |
| 27 | 2026-09-02 01:51:40+09:00 | [0b0bb72](https://github.com/compilecraftworks/Body-Change-NG/commit/0b0bb7213877f075372f5e7c45896530e2593b5a) | Streamline Nexus description and installation paths | 4 |
| 28 | 2026-09-02 01:55:33+09:00 | [1b5488d](https://github.com/compilecraftworks/Body-Change-NG/commit/1b5488d1c215a9f8d5e2d60f29802a450e5895d7) | Refine Nexus credits and rights notice | 4 |
| 29 | 2026-09-02 02:03:08+09:00 | [5956f1d](https://github.com/compilecraftworks/Body-Change-NG/commit/5956f1de67472933e8f86f99f7d3926a41ee60ff) | Align Nexus license copy with release policy | 4 |
| 30 | 2026-09-02 02:07:34+09:00 | [3490101](https://github.com/compilecraftworks/Body-Change-NG/commit/3490101da8cca622469a4cfff23a7d8f9666a659) | Remove menu framework wording from Nexus copy | 4 |
| 31 | 2026-09-02 02:08:25+09:00 | [895cd75](https://github.com/compilecraftworks/Body-Change-NG/commit/895cd758607478167dac4b351a0893ab7c2acef9) | Simplify Nexus release ZIP wording | 4 |
| 32 | 2026-09-02 02:22:44+09:00 | [dca40c2](https://github.com/compilecraftworks/Body-Change-NG/commit/dca40c28c9bdd98e97618cfcc2204951c2e7fe03) | Finalize Nexus description and distribution guidance | 6 |
| 33 | 2026-09-02 02:25:26+09:00 | [e68f63b](https://github.com/compilecraftworks/Body-Change-NG/commit/e68f63b6900317ba7ea781ca67e7fad4633c835b) | Separate optional OBody JSON compatibility | 3 |
| 34 | 2026-09-02 02:28:13+09:00 | [acbe044](https://github.com/compilecraftworks/Body-Change-NG/commit/acbe044d65a4a226c84a13e8d906daddb935fac9) | Remove duplicate Nexus feature guidance | 3 |
| 35 | 2026-09-02 02:36:03+09:00 | [fdd0bde](https://github.com/compilecraftworks/Body-Change-NG/commit/fdd0bde0002cf17cefb9cbc9b8cbcd0d7cc4a1d1) | Polish Nexus BBCode and add English update history | 3 |
| 36 | 2026-09-02 02:38:55+09:00 | [7c39933](https://github.com/compilecraftworks/Body-Change-NG/commit/7c39933704aada45ab2b02743cfdcbff24813d9c) | Add color hierarchy to Nexus BBCode | 3 |
| 37 | 2026-09-02 02:42:49+09:00 | [accdd25](https://github.com/compilecraftworks/Body-Change-NG/commit/accdd25e7a63b976b4131c9c1efb09c785a24bd6) | Generalize legacy system comparison | 3 |
| 38 | 2026-09-02 02:50:54+09:00 | [4da38a1](https://github.com/compilecraftworks/Body-Change-NG/commit/4da38a120bfd550718e5af79144ffd53fe20d066) | Add Markdown update history | 1 |
| 39 | 2026-09-02 08:26:45+09:00 | [9fc852f](https://github.com/compilecraftworks/Body-Change-NG/commit/9fc852fc4a55446481c799c1033df732f8178c72) | Rename project to Body Change NG | 146 |
| 40 | 2026-09-02 08:34:39+09:00 | [ef61c43](https://github.com/compilecraftworks/Body-Change-NG/commit/ef61c43153d2f509acb97f6ee3f12431985e0a20) | Update release artwork for Body Change NG | 2 |
| 41 | 2026-09-02 08:36:19+09:00 | [888fae7](https://github.com/compilecraftworks/Body-Change-NG/commit/888fae722fa240f12f61fbf20c5d0779cd8e1c41) | Optimize Body Change NG header artwork | 1 |
| 42 | 2026-09-03 20:02:23+09:00 | [5c16dbc](https://github.com/compilecraftworks/Body-Change-NG/commit/5c16dbcb6428fb9c709aa69a9370e0d3972c9fb6) | Add per-actor UBE asset compatibility | 28 |
| 43 | 2026-09-03 20:21:56+09:00 | [dc5f5ee](https://github.com/compilecraftworks/Body-Change-NG/commit/dc5f5ee9da97ce59a1495e4755e9db19653ab74d) | Reduce automatic morph distribution stutter | 12 |
| 44 | 2026-09-03 20:38:10+09:00 | [e841e04](https://github.com/compilecraftworks/Body-Change-NG/commit/e841e041df529ca5ff1d56e926c585e1e986a003) | Keep normal distribution off the event fast path | 8 |
| 45 | 2026-09-03 21:18:53+09:00 | [54b7742](https://github.com/compilecraftworks/Body-Change-NG/commit/54b7742e10caca4178bdd4e1ffe9c3249dca0a7c) | Migrate legacy RaceMenu head presets safely | 10 |
| 46 | 2026-09-03 22:40:15+09:00 | [ef1734f](https://github.com/compilecraftworks/Body-Change-NG/commit/ef1734f7e684f23f748a7ca805f4517d7397cc00) | fix: support safe partial skin overrides | 29 |
| 47 | 2026-09-03 22:51:28+09:00 | [91c4f0f](https://github.com/compilecraftworks/Body-Change-NG/commit/91c4f0f30e053afdde64ce8e04dceefb77485d01) | ui: clarify catalog tab names | 1 |
| 48 | 2026-09-03 23:21:43+09:00 | [d97e168](https://github.com/compilecraftworks/Body-Change-NG/commit/d97e168711168a69e7253b4b02dc44082d4c33db) | feat: support Argonian and Khajiit skins | 19 |
| 49 | 2026-09-04 00:24:50+09:00 | [d2c37a7](https://github.com/compilecraftworks/Body-Change-NG/commit/d2c37a7a2f643c82fb6bd54f6d3095b3e7640a87) | feat: apply 3BA female genital skin textures | 12 |
| 50 | 2026-09-04 00:57:30+09:00 | [269fd18](https://github.com/compilecraftworks/Body-Change-NG/commit/269fd18cac39167f0fdbab1e52ec2c322447fd0a) | feat: select elder and race skin variants | 9 |
| 51 | 2026-09-04 01:46:14+09:00 | [db21427](https://github.com/compilecraftworks/Body-Change-NG/commit/db21427eea208eda1a2937e8e47db29fdeff9b74) | feat: route CBBE and UNP skin atlases | 14 |
| 52 | 2026-09-04 03:17:14+09:00 | [6760768](https://github.com/compilecraftworks/Body-Change-NG/commit/67607685f7ffae28a93a032811ca54e4cf704955) | fix: harden distribution morphs and male skins | 29 |
| 53 | 2026-09-04 18:18:25+09:00 | [26408f2](https://github.com/compilecraftworks/Body-Change-NG/commit/26408f21944131f8ccf9957de6cd3139318cf2c0) | release: prepare 1.1.0 and bilingual Nexus documentation | 20 |
| 54 | 2026-09-04 19:14:49+09:00 | [834e007](https://github.com/compilecraftworks/Body-Change-NG/commit/834e007cc3db64fb8024631e3588ea7af86068d7) | packaging: minimize MO2 archive and consolidate license notices | 3 |

## 4. 커밋별 변경 파일 전체

Git `diff-tree --root --no-commit-id --name-status -r` 결과. A=추가, M=수정, D=삭제. Rename을 별도로 탐지하지 않은 명령 결과이므로 이름 변경은 D/A로 표시될 수 있다. 이 목록은 파일 단위 이력이며 상세 line diff는 각 커밋 링크 또는 `git show <hash>`로 확인한다.

### 1. d6957cd — Establish Body Changer NG source baseline

- 날짜: 2026-09-01T16:44:54+09:00
- 전체 SHA: `d6957cd304ad58d28ca0541660ab175d10396e7e`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/d6957cd304ad58d28ca0541660ab175d10396e7e)

```text
A	.gitignore
A	.gitmodules
A	DEPENDENCIES.md
A	LICENSE
A	README.md
A	docs/Skin Profile Folder.txt
A	docs/Skin Texture Folder.txt
A	docs/SliderPresets Folder.txt
A	docs/skin-profiles.md
A	examples/SkinProfiles/Example Female Skin/profile.json
A	nexus-header-body-changer-ng-3480x996.jpg
A	nexus-representative-body-changer-ng-3840x2160.jpg
A	src/BodyChangerNG/ActorCatalog.cpp
A	src/BodyChangerNG/ActorCatalog.h
A	src/BodyChangerNG/ActorEvents.cpp
A	src/BodyChangerNG/ActorEvents.h
A	src/BodyChangerNG/BodyFamily.cpp
A	src/BodyChangerNG/BodyFamily.h
A	src/BodyChangerNG/BodyFamilyRules.cpp
A	src/BodyChangerNG/CatalogRoots.cpp
A	src/BodyChangerNG/CatalogRoots.h
A	src/BodyChangerNG/Distribution.cpp
A	src/BodyChangerNG/Distribution.h
A	src/BodyChangerNG/Hotkey.cpp
A	src/BodyChangerNG/Hotkey.h
A	src/BodyChangerNG/InputSink.cpp
A	src/BodyChangerNG/InputSink.h
A	src/BodyChangerNG/MenuCharacterPresentation.cpp
A	src/BodyChangerNG/MenuCharacterPresentation.h
A	src/BodyChangerNG/NativeImGuiHost.cpp
A	src/BodyChangerNG/NativeImGuiHost.h
A	src/BodyChangerNG/OutfitRefit.cpp
A	src/BodyChangerNG/OutfitRefit.h
A	src/BodyChangerNG/OutfitRefitRules.h
A	src/BodyChangerNG/PathText.h
A	src/BodyChangerNG/PlayerTint.cpp
A	src/BodyChangerNG/PlayerTint.h
A	src/BodyChangerNG/PresetCatalog.cpp
A	src/BodyChangerNG/PresetCatalog.h
A	src/BodyChangerNG/RaceMenuBodyMorph.cpp
A	src/BodyChangerNG/RaceMenuBodyMorph.h
A	src/BodyChangerNG/RuntimeAssetCache.cpp
A	src/BodyChangerNG/RuntimeAssetCache.h
A	src/BodyChangerNG/RuntimeLayout.h
A	src/BodyChangerNG/Settings.cpp
A	src/BodyChangerNG/Settings.h
A	src/BodyChangerNG/SkinCatalog.cpp
A	src/BodyChangerNG/SkinCatalog.h
A	src/BodyChangerNG/SkinOverrideOwnership.h
A	src/BodyChangerNG/SkinOverrides.cpp
A	src/BodyChangerNG/SkinOverrides.h
A	src/BodyChangerNG/SkinProfiles.cpp
A	src/BodyChangerNG/SkinProfiles.h
A	src/BodyChangerNG/SmoothCamIntegration.cpp
A	src/BodyChangerNG/SmoothCamIntegration.h
A	src/BodyChangerNG/TextInputFilter.cpp
A	src/BodyChangerNG/TextInputFilter.h
A	src/BodyChangerNG/UI.cpp
A	src/BodyChangerNG/UI.h
A	src/PCH.h
A	src/main.cpp
A	tests/AssetCatalogTests.cpp
A	tests/BodyFamilyTests.cpp
A	tests/HotkeyTests.cpp
A	tests/OutfitRefitRulesTests.cpp
A	tests/PresetCatalogTests.cpp
A	tests/SkinOverrideOwnershipTests.cpp
A	third_party/CommonLibSSE-NG
A	third_party/imgui
A	third_party/pugixml
A	xmake-requires.lock
A	xmake.lua
```

### 2. cc2a2cf — Persist actor selections and harden distribution UI

- 날짜: 2026-09-01T17:28:14+09:00
- 전체 SHA: `cc2a2cfdb25dccd35dedfa51650df8bba7b7851d`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/cc2a2cfdb25dccd35dedfa51650df8bba7b7851d)

```text
M	src/BodyChangerNG/ActorEvents.cpp
M	src/BodyChangerNG/ActorEvents.h
A	src/BodyChangerNG/ActorRegistry.cpp
A	src/BodyChangerNG/ActorRegistry.h
A	src/BodyChangerNG/ActorState.h
A	src/BodyChangerNG/ActorWorkQueue.cpp
A	src/BodyChangerNG/ActorWorkQueue.h
M	src/BodyChangerNG/BodyFamilyRules.cpp
M	src/BodyChangerNG/CatalogRoots.cpp
M	src/BodyChangerNG/Distribution.cpp
M	src/BodyChangerNG/Distribution.h
M	src/BodyChangerNG/OutfitRefit.cpp
M	src/BodyChangerNG/OutfitRefit.h
M	src/BodyChangerNG/PlayerTint.cpp
M	src/BodyChangerNG/PlayerTint.h
M	src/BodyChangerNG/RaceMenuBodyMorph.cpp
M	src/BodyChangerNG/RaceMenuBodyMorph.h
M	src/BodyChangerNG/RuntimeAssetCache.cpp
M	src/BodyChangerNG/RuntimeAssetCache.h
M	src/BodyChangerNG/SkinOverrides.cpp
M	src/BodyChangerNG/SkinOverrides.h
M	src/BodyChangerNG/SkinProfiles.cpp
M	src/BodyChangerNG/UI.cpp
M	src/main.cpp
A	tests/ActorStateTests.cpp
M	tests/AssetCatalogTests.cpp
M	tests/BodyFamilyTests.cpp
M	tests/PresetCatalogTests.cpp
M	xmake.lua
```

### 3. f5e7c02 — Restore combo previews and text editing

- 날짜: 2026-09-01T18:50:44+09:00
- 전체 SHA: `f5e7c028472bf6e8cd0d7ad7a35acff5b54657a4`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/f5e7c028472bf6e8cd0d7ad7a35acff5b54657a4)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/NativeImGuiHost.cpp
M	src/BodyChangerNG/NativeImGuiHost.h
M	src/BodyChangerNG/TextInputFilter.cpp
M	src/BodyChangerNG/UI.cpp
M	third_party/imgui
```

### 4. 5ee5508 — Make distribution panes resizable

- 날짜: 2026-09-01T18:54:05+09:00
- 전체 SHA: `5ee5508eea7965d59676fd310e1c98eca0a98d01`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/5ee5508eea7965d59676fd310e1c98eca0a98d01)

```text
M	src/BodyChangerNG/UI.cpp
```

### 5. f1d20a5 — Add closer tint detail camera framing

- 날짜: 2026-09-01T19:02:17+09:00
- 전체 SHA: `f1d20a5dcb4f4ad7defc2851ef5c0b2a95f36358`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/f1d20a5dcb4f4ad7defc2851ef5c0b2a95f36358)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/MenuCharacterPresentation.h
M	src/BodyChangerNG/UI.cpp
```

### 6. 7f4b920 — Unify tint framing and balance character sides

- 날짜: 2026-09-01T19:27:11+09:00
- 전체 SHA: `7f4b920363cef0983e5505c00515501ca235ed67`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/7f4b920363cef0983e5505c00515501ca235ed67)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/MenuCharacterPresentation.h
M	src/BodyChangerNG/UI.cpp
```

### 7. fa38474 — Keep tint zoom inside screen bounds

- 날짜: 2026-09-01T19:35:27+09:00
- 전체 SHA: `fa38474cd7c118f8d1eef315545d654091b15eee`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/fa38474cd7c118f8d1eef315545d654091b15eee)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 8. 6273fc4 — Preserve right tint framing

- 날짜: 2026-09-01T19:37:10+09:00
- 전체 SHA: `6273fc447cbe7b370295349fdbfb22e2ea3bd429`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/6273fc447cbe7b370295349fdbfb22e2ea3bd429)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 9. b8f2c8a — Reframe normal and tint cameras

- 날짜: 2026-09-01T19:48:25+09:00
- 전체 SHA: `b8f2c8aa5538b321272eb081a0d1ad299e39c02b`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/b8f2c8aa5538b321272eb081a0d1ad299e39c02b)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 10. 9e57dfc — Finalize menu camera and release layout

- 날짜: 2026-09-01T20:20:45+09:00
- 전체 SHA: `9e57dfc72433992385a0cc36dff3bd09b262d894`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/9e57dfc72433992385a0cc36dff3bd09b262d894)

```text
M	README.md
A	package/BodySkin/README.txt
A	package/CalienteTools/BodySlide/SliderPresets/README.txt
A	package/SKSE/Plugins/BodyChangerNGdistribution.json
A	package/TintMask/README.txt
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 11. 85f6677 — Keep outfit-named BodySlide presets visible

- 날짜: 2026-09-01T21:00:57+09:00
- 전체 SHA: `85f667774c0b3bed8f1ec9e7f9084b84a52a9adf`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/85f667774c0b3bed8f1ec9e7f9084b84a52a9adf)

```text
M	src/BodyChangerNG/PresetCatalog.cpp
M	tests/PresetCatalogTests.cpp
```

### 12. c8c1332 — Set final menu camera framing

- 날짜: 2026-09-01T21:22:55+09:00
- 전체 SHA: `c8c1332f14824f84ee6d9c83735909957f4a1923`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/c8c1332f14824f84ee6d9c83735909957f4a1923)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 13. b9e9f3f — Stabilize camera before restoring menu pause

- 날짜: 2026-09-01T21:35:02+09:00
- 전체 SHA: `b9e9f3f7b32b53d650b8f54d3c1a6a975ba7e985`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/b9e9f3f7b32b53d650b8f54d3c1a6a975ba7e985)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/NativeImGuiHost.cpp
M	src/BodyChangerNG/NativeImGuiHost.h
```

### 14. c5847e1 — Synchronize render world FOV in menu

- 날짜: 2026-09-01T21:46:10+09:00
- 전체 SHA: `c5847e1abd8a94f86c44771ba03cc85e9aec5728`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/c5847e1abd8a94f86c44771ba03cc85e9aec5728)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 15. e3e08aa — Remove menu camera pause settle experiment

- 날짜: 2026-09-01T21:52:06+09:00
- 전체 SHA: `e3e08aa54d2993ee19740f34fdb75edd4a15b801`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/e3e08aa54d2993ee19740f34fdb75edd4a15b801)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/NativeImGuiHost.cpp
M	src/BodyChangerNG/NativeImGuiHost.h
```

### 16. 832219d — Neutralize additive FOV in menu camera

- 날짜: 2026-09-01T22:56:19+09:00
- 전체 SHA: `832219def5dc7e90c945a904b2701a7d75b9a679`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/832219def5dc7e90c945a904b2701a7d75b9a679)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/RuntimeLayout.h
A	tests/RuntimeLayoutTests.cpp
M	xmake.lua
```

### 17. d135cbf — Harden camera state and distribution UI

- 날짜: 2026-09-02T00:10:51+09:00
- 전체 SHA: `d135cbf075eacc78ec89d0acd773dd0abc13129d`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/d135cbf075eacc78ec89d0acd773dd0abc13129d)

```text
M	src/BodyChangerNG/ActorEvents.cpp
M	src/BodyChangerNG/ActorWorkQueue.cpp
A	src/BodyChangerNG/MenuCameraProjection.h
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/NativeImGuiHost.cpp
M	src/BodyChangerNG/NativeImGuiHost.h
M	src/BodyChangerNG/RaceMenuBodyMorph.cpp
M	src/BodyChangerNG/RuntimeLayout.h
M	src/BodyChangerNG/SkinCatalog.cpp
M	src/BodyChangerNG/SkinOverrides.cpp
M	src/BodyChangerNG/SmoothCamIntegration.cpp
M	src/BodyChangerNG/UI.cpp
M	tests/RuntimeLayoutTests.cpp
```

### 18. e81565a — Sync tint preview and final camera pitch

- 날짜: 2026-09-02T00:15:27+09:00
- 전체 SHA: `e81565adf89054220c960f9052dda4ed4052f76c`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/e81565adf89054220c960f9052dda4ed4052f76c)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
M	src/BodyChangerNG/PlayerTint.cpp
M	src/BodyChangerNG/PlayerTint.h
M	src/BodyChangerNG/UI.cpp
```

### 19. eab8a94 — Release Body Changer NG v1.0.0

- 날짜: 2026-09-02T00:29:32+09:00
- 전체 SHA: `eab8a9455c5cbe62bb25740b4293cb16be993c8c`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/eab8a9455c5cbe62bb25740b4293cb16be993c8c)

```text
A	CHANGELOG.md
M	DEPENDENCIES.md
M	README.md
A	docs/NEXUS-CHANGELOG-v1.0.0-KO.txt
A	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
A	docs/NEXUS-DESCRIPTION-v1.0.0.md
A	docs/RELEASE-NOTES-v1.0.0.md
A	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	src/BodyChangerNG/Settings.h
M	src/BodyChangerNG/UI.cpp
M	xmake.lua
```

### 20. 3ab178d — Document OBody JSON compatibility and native UI advantages

- 날짜: 2026-09-02T01:02:31+09:00
- 전체 SHA: `3ab178dd9cc9438edb32ae2f8a49b9ee1ab73338`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/3ab178dd9cc9438edb32ae2f8a49b9ee1ab73338)

```text
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 21. d81164e — Keep distribution actions on one row

- 날짜: 2026-09-02T01:09:40+09:00
- 전체 SHA: `d81164e2f1c790dd89f524f5c56a4a9b8eff2233`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/d81164e2f1c790dd89f524f5c56a4a9b8eff2233)

```text
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	src/BodyChangerNG/UI.cpp
```

### 22. 0092e8e — Add Nexus upload copy and update history

- 날짜: 2026-09-02T01:20:05+09:00
- 전체 SHA: `0092e8e845bd948df7de0956d8d0a4aa34baad9d`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/0092e8e845bd948df7de0956d8d0a4aa34baad9d)

```text
A	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
A	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
A	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt
A	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode
A	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html
```

### 23. bfa805e — Refine tint framing and Nexus feature copy

- 날짜: 2026-09-02T01:31:01+09:00
- 전체 SHA: `bfa805e925b80b95fd5ac3b04cbd42acf18895d3`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/bfa805e925b80b95fd5ac3b04cbd42acf18895d3)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt
A	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 24. 89b6081 — Simplify Nexus short descriptions

- 날짜: 2026-09-02T01:38:40+09:00
- 전체 SHA: `89b6081e01a383ac62e192f0028c652573068cbf`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/89b6081e01a383ac62e192f0028c652573068cbf)

```text
M	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt
M	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-KO.txt
```

### 25. 8b902cf — Fine tune tint camera framing

- 날짜: 2026-09-02T01:41:45+09:00
- 전체 SHA: `8b902cfe810c2aa1dd356f9256618482e8499236`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/8b902cfe810c2aa1dd356f9256618482e8499236)

```text
M	src/BodyChangerNG/MenuCharacterPresentation.cpp
```

### 26. 784a983 — Finalize Nexus short description

- 날짜: 2026-09-02T01:44:13+09:00
- 전체 SHA: `784a9839bcaab976c43f0cc9768d16d98ba0ce6a`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/784a9839bcaab976c43f0cc9768d16d98ba0ce6a)

```text
M	docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt
```

### 27. 0b0bb72 — Streamline Nexus description and installation paths

- 날짜: 2026-09-02T01:51:40+09:00
- 전체 SHA: `0b0bb7213877f075372f5e7c45896530e2593b5a`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/0b0bb7213877f075372f5e7c45896530e2593b5a)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 28. 1b5488d — Refine Nexus credits and rights notice

- 날짜: 2026-09-02T01:55:33+09:00
- 전체 SHA: `1b5488d1c215a9f8d5e2d60f29802a450e5895d7`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/1b5488d1c215a9f8d5e2d60f29802a450e5895d7)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 29. 5956f1d — Align Nexus license copy with release policy

- 날짜: 2026-09-02T02:03:08+09:00
- 전체 SHA: `5956f1de67472933e8f86f99f7d3926a41ee60ff`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/5956f1de67472933e8f86f99f7d3926a41ee60ff)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 30. 3490101 — Remove menu framework wording from Nexus copy

- 날짜: 2026-09-02T02:07:34+09:00
- 전체 SHA: `3490101da8cca622469a4cfff23a7d8f9666a659`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/3490101da8cca622469a4cfff23a7d8f9666a659)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 31. 895cd75 — Simplify Nexus release ZIP wording

- 날짜: 2026-09-02T02:08:25+09:00
- 전체 SHA: `895cd758607478167dac4b351a0893ab7c2acef9`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/895cd758607478167dac4b351a0893ab7c2acef9)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 32. dca40c2 — Finalize Nexus description and distribution guidance

- 날짜: 2026-09-02T02:22:44+09:00
- 전체 SHA: `dca40c28c9bdd98e97618cfcc2204951c2e7fe03`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/dca40c28c9bdd98e97618cfcc2204951c2e7fe03)

```text
A	THIRD_PARTY_NOTICES.md
A	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
D	docs/NEXUS-DESCRIPTION-v1.0.0-KO.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
D	docs/NEXUS-DESCRIPTION-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 33. e68f63b — Separate optional OBody JSON compatibility

- 날짜: 2026-09-02T02:25:26+09:00
- 전체 SHA: `e68f63b6900317ba7ea781ca67e7fad4633c835b`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/e68f63b6900317ba7ea781ca67e7fad4633c835b)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 34. acbe044 — Remove duplicate Nexus feature guidance

- 날짜: 2026-09-02T02:28:13+09:00
- 전체 SHA: `acbe044d65a4a226c84a13e8d906daddb935fac9`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/acbe044d65a4a226c84a13e8d906daddb935fac9)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 35. fdd0bde — Polish Nexus BBCode and add English update history

- 날짜: 2026-09-02T02:36:03+09:00
- 전체 SHA: `fdd0bde0002cf17cefb9cbc9b8cbcd0d7cc4a1d1`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/fdd0bde0002cf17cefb9cbc9b8cbcd0d7cc4a1d1)

```text
A	docs/NEXUS-CHANGELOG-v1.0.0-EN.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
A	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode
```

### 36. 7c39933 — Add color hierarchy to Nexus BBCode

- 날짜: 2026-09-02T02:38:55+09:00
- 전체 SHA: `7c39933704aada45ab2b02743cfdcbff24813d9c`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/7c39933704aada45ab2b02743cfdcbff24813d9c)

```text
M	docs/NEXUS-CHANGELOG-v1.0.0-EN.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode
```

### 37. accdd25 — Generalize legacy system comparison

- 날짜: 2026-09-02T02:42:49+09:00
- 전체 SHA: `accdd25e7a63b976b4131c9c1efb09c785a24bd6`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/accdd25e7a63b976b4131c9c1efb09c785a24bd6)

```text
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
```

### 38. 4da38a1 — Add Markdown update history

- 날짜: 2026-09-02T02:50:54+09:00
- 전체 SHA: `4da38a120bfd550718e5af79144ffd53fe20d066`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/4da38a120bfd550718e5af79144ffd53fe20d066)

```text
A	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
```

### 39. 9fc852f — Rename project to Body Change NG

- 날짜: 2026-09-02T08:26:45+09:00
- 전체 SHA: `9fc852fc4a55446481c799c1033df732f8178c72`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/9fc852fc4a55446481c799c1033df732f8178c72)

```text
M	CHANGELOG.md
M	DEPENDENCIES.md
M	README.md
M	THIRD_PARTY_NOTICES.md
M	docs/NEXUS-CHANGELOG-v1.0.0-EN.txt
M	docs/NEXUS-CHANGELOG-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/Skin Profile Folder.txt
M	docs/Skin Texture Folder.txt
M	docs/SliderPresets Folder.txt
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	docs/skin-profiles.md
A	nexus-header-body-change-ng-3480x996.jpg
D	nexus-header-body-changer-ng-3480x996.jpg
A	nexus-representative-body-change-ng-3840x2160.jpg
D	nexus-representative-body-changer-ng-3840x2160.jpg
M	package/BodySkin/README.txt
M	package/CalienteTools/BodySlide/SliderPresets/README.txt
A	package/SKSE/Plugins/BodyChangeNGdistribution.json
D	package/SKSE/Plugins/BodyChangerNGdistribution.json
M	package/TintMask/README.txt
A	src/BodyChangeNG/ActorCatalog.cpp
A	src/BodyChangeNG/ActorCatalog.h
A	src/BodyChangeNG/ActorEvents.cpp
A	src/BodyChangeNG/ActorEvents.h
A	src/BodyChangeNG/ActorRegistry.cpp
A	src/BodyChangeNG/ActorRegistry.h
A	src/BodyChangeNG/ActorState.h
A	src/BodyChangeNG/ActorWorkQueue.cpp
A	src/BodyChangeNG/ActorWorkQueue.h
A	src/BodyChangeNG/BodyFamily.cpp
A	src/BodyChangeNG/BodyFamily.h
A	src/BodyChangeNG/BodyFamilyRules.cpp
A	src/BodyChangeNG/CatalogRoots.cpp
A	src/BodyChangeNG/CatalogRoots.h
A	src/BodyChangeNG/Distribution.cpp
A	src/BodyChangeNG/Distribution.h
A	src/BodyChangeNG/Hotkey.cpp
A	src/BodyChangeNG/Hotkey.h
A	src/BodyChangeNG/InputSink.cpp
A	src/BodyChangeNG/InputSink.h
A	src/BodyChangeNG/MenuCameraProjection.h
A	src/BodyChangeNG/MenuCharacterPresentation.cpp
A	src/BodyChangeNG/MenuCharacterPresentation.h
A	src/BodyChangeNG/NativeImGuiHost.cpp
A	src/BodyChangeNG/NativeImGuiHost.h
A	src/BodyChangeNG/OutfitRefit.cpp
A	src/BodyChangeNG/OutfitRefit.h
A	src/BodyChangeNG/OutfitRefitRules.h
A	src/BodyChangeNG/PathMigration.h
A	src/BodyChangeNG/PathText.h
A	src/BodyChangeNG/PlayerTint.cpp
A	src/BodyChangeNG/PlayerTint.h
A	src/BodyChangeNG/PresetCatalog.cpp
A	src/BodyChangeNG/PresetCatalog.h
A	src/BodyChangeNG/RaceMenuBodyMorph.cpp
A	src/BodyChangeNG/RaceMenuBodyMorph.h
A	src/BodyChangeNG/RuntimeAssetCache.cpp
A	src/BodyChangeNG/RuntimeAssetCache.h
A	src/BodyChangeNG/RuntimeLayout.h
A	src/BodyChangeNG/Settings.cpp
A	src/BodyChangeNG/Settings.h
A	src/BodyChangeNG/SkinCatalog.cpp
A	src/BodyChangeNG/SkinCatalog.h
A	src/BodyChangeNG/SkinOverrideOwnership.h
A	src/BodyChangeNG/SkinOverrides.cpp
A	src/BodyChangeNG/SkinOverrides.h
A	src/BodyChangeNG/SkinProfiles.cpp
A	src/BodyChangeNG/SkinProfiles.h
A	src/BodyChangeNG/SmoothCamIntegration.cpp
A	src/BodyChangeNG/SmoothCamIntegration.h
A	src/BodyChangeNG/TextInputFilter.cpp
A	src/BodyChangeNG/TextInputFilter.h
A	src/BodyChangeNG/UI.cpp
A	src/BodyChangeNG/UI.h
D	src/BodyChangerNG/ActorCatalog.cpp
D	src/BodyChangerNG/ActorCatalog.h
D	src/BodyChangerNG/ActorEvents.cpp
D	src/BodyChangerNG/ActorEvents.h
D	src/BodyChangerNG/ActorRegistry.cpp
D	src/BodyChangerNG/ActorRegistry.h
D	src/BodyChangerNG/ActorState.h
D	src/BodyChangerNG/ActorWorkQueue.cpp
D	src/BodyChangerNG/ActorWorkQueue.h
D	src/BodyChangerNG/BodyFamily.cpp
D	src/BodyChangerNG/BodyFamily.h
D	src/BodyChangerNG/BodyFamilyRules.cpp
D	src/BodyChangerNG/CatalogRoots.cpp
D	src/BodyChangerNG/CatalogRoots.h
D	src/BodyChangerNG/Distribution.cpp
D	src/BodyChangerNG/Distribution.h
D	src/BodyChangerNG/Hotkey.cpp
D	src/BodyChangerNG/Hotkey.h
D	src/BodyChangerNG/InputSink.cpp
D	src/BodyChangerNG/InputSink.h
D	src/BodyChangerNG/MenuCameraProjection.h
D	src/BodyChangerNG/MenuCharacterPresentation.cpp
D	src/BodyChangerNG/MenuCharacterPresentation.h
D	src/BodyChangerNG/NativeImGuiHost.cpp
D	src/BodyChangerNG/NativeImGuiHost.h
D	src/BodyChangerNG/OutfitRefit.cpp
D	src/BodyChangerNG/OutfitRefit.h
D	src/BodyChangerNG/OutfitRefitRules.h
D	src/BodyChangerNG/PathText.h
D	src/BodyChangerNG/PlayerTint.cpp
D	src/BodyChangerNG/PlayerTint.h
D	src/BodyChangerNG/PresetCatalog.cpp
D	src/BodyChangerNG/PresetCatalog.h
D	src/BodyChangerNG/RaceMenuBodyMorph.cpp
D	src/BodyChangerNG/RaceMenuBodyMorph.h
D	src/BodyChangerNG/RuntimeAssetCache.cpp
D	src/BodyChangerNG/RuntimeAssetCache.h
D	src/BodyChangerNG/RuntimeLayout.h
D	src/BodyChangerNG/Settings.cpp
D	src/BodyChangerNG/Settings.h
D	src/BodyChangerNG/SkinCatalog.cpp
D	src/BodyChangerNG/SkinCatalog.h
D	src/BodyChangerNG/SkinOverrideOwnership.h
D	src/BodyChangerNG/SkinOverrides.cpp
D	src/BodyChangerNG/SkinOverrides.h
D	src/BodyChangerNG/SkinProfiles.cpp
D	src/BodyChangerNG/SkinProfiles.h
D	src/BodyChangerNG/SmoothCamIntegration.cpp
D	src/BodyChangerNG/SmoothCamIntegration.h
D	src/BodyChangerNG/TextInputFilter.cpp
D	src/BodyChangerNG/TextInputFilter.h
D	src/BodyChangerNG/UI.cpp
D	src/BodyChangerNG/UI.h
M	src/main.cpp
M	tests/ActorStateTests.cpp
M	tests/AssetCatalogTests.cpp
M	tests/BodyFamilyTests.cpp
M	tests/HotkeyTests.cpp
M	tests/OutfitRefitRulesTests.cpp
A	tests/PathMigrationTests.cpp
M	tests/PresetCatalogTests.cpp
M	tests/RuntimeLayoutTests.cpp
M	tests/SkinOverrideOwnershipTests.cpp
M	xmake.lua
```

### 40. ef61c43 — Update release artwork for Body Change NG

- 날짜: 2026-09-02T08:34:39+09:00
- 전체 SHA: `ef61c43153d2f509acb97f6ee3f12431985e0a20`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/ef61c43153d2f509acb97f6ee3f12431985e0a20)

```text
M	nexus-header-body-change-ng-3480x996.jpg
M	nexus-representative-body-change-ng-3840x2160.jpg
```

### 41. 888fae7 — Optimize Body Change NG header artwork

- 날짜: 2026-09-02T08:36:19+09:00
- 전체 SHA: `888fae722fa240f12f61fbf20c5d0779cd8e1c41`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/888fae722fa240f12f61fbf20c5d0779cd8e1c41)

```text
M	nexus-header-body-change-ng-3480x996.jpg
```

### 42. 5c16dbc — Add per-actor UBE asset compatibility

- 날짜: 2026-09-03T20:02:23+09:00
- 전체 SHA: `5c16dbcb6428fb9c709aa69a9370e0d3972c9fb6`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/5c16dbcb6428fb9c709aa69a9370e0d3972c9fb6)

```text
M	CHANGELOG.md
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	docs/skin-profiles.md
M	package/BodySkin/README.txt
M	package/CalienteTools/BodySlide/SliderPresets/README.txt
M	package/TintMask/README.txt
M	src/BodyChangeNG/BodyFamily.cpp
M	src/BodyChangeNG/BodyFamily.h
M	src/BodyChangeNG/BodyFamilyRules.cpp
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/PlayerTint.cpp
M	src/BodyChangeNG/PlayerTint.h
M	src/BodyChangeNG/PresetCatalog.cpp
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinOverrides.h
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/AssetCatalogTests.cpp
M	tests/BodyFamilyTests.cpp
M	tests/PresetCatalogTests.cpp
M	xmake.lua
```

### 43. dc5f5ee — Reduce automatic morph distribution stutter

- 날짜: 2026-09-03T20:21:56+09:00
- 전체 SHA: `dc5f5ee9da97ce59a1495e4755e9db19653ab74d`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/dc5f5ee9da97ce59a1495e4755e9db19653ab74d)

```text
M	CHANGELOG.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	src/BodyChangeNG/ActorWorkQueue.cpp
M	src/BodyChangeNG/ActorWorkQueue.h
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/RaceMenuBodyMorph.cpp
M	src/BodyChangeNG/RaceMenuBodyMorph.h
M	src/BodyChangeNG/UI.cpp
M	src/main.cpp
M	tests/ActorStateTests.cpp
```

### 44. e841e04 — Keep normal distribution off the event fast path

- 날짜: 2026-09-03T20:38:10+09:00
- 전체 SHA: `e841e041df529ca5ff1d56e926c585e1e986a003`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/e841e041df529ca5ff1d56e926c585e1e986a003)

```text
M	CHANGELOG.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	src/BodyChangeNG/ActorWorkQueue.cpp
M	src/BodyChangeNG/ActorWorkQueue.h
M	src/BodyChangeNG/UI.cpp
M	tests/ActorStateTests.cpp
```

### 45. 54b7742 — Migrate legacy RaceMenu head presets safely

- 날짜: 2026-09-03T21:18:53+09:00
- 전체 SHA: `54b7742e10caca4178bdd4e1ffe9c3249dca0a7c`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/54b7742e10caca4178bdd4e1ffe9c3249dca0a7c)

```text
M	CHANGELOG.md
M	README.md
M	docs/RELEASE-NOTES-v1.0.0.md
A	src/BodyChangeNG/RaceMenuPresetMigration.cpp
A	src/BodyChangeNG/RaceMenuPresetMigration.h
A	src/BodyChangeNG/RaceMenuPresetMigrationRules.cpp
A	src/BodyChangeNG/RaceMenuPresetMigrationRules.h
M	src/main.cpp
A	tests/RaceMenuPresetMigrationTests.cpp
M	xmake.lua
```

### 46. ef1734f — fix: support safe partial skin overrides

- 날짜: 2026-09-03T22:40:15+09:00
- 전체 SHA: `ef1734f7e684f23f748a7ca805f4517d7397cc00`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/ef1734f7e684f23f748a7ca805f4517d7397cc00)

```text
M	README.md
M	docs/NEXUS-CHANGELOG-v1.0.0-EN.txt
M	docs/NEXUS-CHANGELOG-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html
M	docs/Skin Profile Folder.txt
M	docs/Skin Texture Folder.txt
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	docs/skin-profiles.md
M	package/BodySkin/README.txt
M	src/BodyChangeNG/ActorRegistry.cpp
M	src/BodyChangeNG/ActorRegistry.h
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/Distribution.h
M	src/BodyChangeNG/PresetCatalog.cpp
M	src/BodyChangeNG/PresetCatalog.h
M	src/BodyChangeNG/RaceMenuBodyMorph.cpp
M	src/BodyChangeNG/RaceMenuBodyMorph.h
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/ActorStateTests.cpp
M	tests/AssetCatalogTests.cpp
```

### 47. 91c4f0f — ui: clarify catalog tab names

- 날짜: 2026-09-03T22:51:28+09:00
- 전체 SHA: `91c4f0f30e053afdde64ce8e04dceefb77485d01`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/91c4f0f30e053afdde64ce8e04dceefb77485d01)

```text
M	src/BodyChangeNG/UI.cpp
```

### 48. d97e168 — feat: support Argonian and Khajiit skins

- 날짜: 2026-09-03T23:21:43+09:00
- 전체 SHA: `d97e168711168a69e7253b4b02dc44082d4c33db`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/d97e168711168a69e7253b4b02dc44082d4c33db)

```text
M	CHANGELOG.md
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/Skin Profile Folder.txt
M	docs/Skin Texture Folder.txt
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	docs/skin-profiles.md
M	package/BodySkin/README.txt
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinOverrides.h
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/AssetCatalogTests.cpp
```

### 49. d2c37a7 — feat: apply 3BA female genital skin textures

- 날짜: 2026-09-04T00:24:50+09:00
- 전체 SHA: `d2c37a7a2f643c82fb6bd54f6d3095b3e7640a87`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/d2c37a7a2f643c82fb6bd54f6d3095b3e7640a87)

```text
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	examples/SkinProfiles/Example Female Skin/profile.json
M	package/BodySkin/README.txt
A	src/BodyChangeNG/SkinGeometryRouting.h
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/AssetCatalogTests.cpp
```

### 50. 269fd18 — feat: select elder and race skin variants

- 날짜: 2026-09-04T00:57:30+09:00
- 전체 SHA: `269fd18cac39167f0fdbab1e52ec2c322447fd0a`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/269fd18cac39167f0fdbab1e52ec2c322447fd0a)

```text
M	CHANGELOG.md
M	README.md
M	package/BodySkin/README.txt
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/AssetCatalogTests.cpp
```

### 51. db21427 — feat: route CBBE and UNP skin atlases

- 날짜: 2026-09-04T01:46:14+09:00
- 전체 SHA: `db21427eea208eda1a2937e8e47db29fdeff9b74`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/db21427eea208eda1a2937e8e47db29fdeff9b74)

```text
M	CHANGELOG.md
M	README.md
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/skin-profiles.md
M	examples/SkinProfiles/Example Female Skin/profile.json
M	package/BodySkin/README.txt
M	src/BodyChangeNG/SkinGeometryRouting.h
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	tests/AssetCatalogTests.cpp
```

### 52. 6760768 — fix: harden distribution morphs and male skins

- 날짜: 2026-09-04T03:17:14+09:00
- 전체 SHA: `67607685f7ffae28a93a032811ca54e4cf704955`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/67607685f7ffae28a93a032811ca54e4cf704955)

```text
M	CHANGELOG.md
M	README.md
M	docs/NEXUS-CHANGELOG-v1.0.0-EN.txt
M	docs/NEXUS-CHANGELOG-v1.0.0-KO.txt
M	docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode
M	docs/NEXUS-DESCRIPTION-v1.0.0-KO.html
M	docs/NEXUS-DESCRIPTION-v1.0.0.md
M	docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md
M	docs/RELEASE-NOTES-v1.0.0.md
M	docs/UPDATE-HISTORY-v1.0.0-KO.md
M	docs/skin-profiles.md
M	package/BodySkin/README.txt
M	package/SKSE/Plugins/BodyChangeNGdistribution.json
M	src/BodyChangeNG/ActorWorkQueue.h
M	src/BodyChangeNG/Distribution.cpp
M	src/BodyChangeNG/Distribution.h
M	src/BodyChangeNG/PresetCatalog.cpp
M	src/BodyChangeNG/PresetCatalog.h
M	src/BodyChangeNG/RaceMenuBodyMorph.cpp
M	src/BodyChangeNG/RaceMenuBodyMorph.h
M	src/BodyChangeNG/SkinGeometryRouting.h
M	src/BodyChangeNG/SkinOverrides.cpp
M	src/BodyChangeNG/SkinProfiles.cpp
M	src/BodyChangeNG/SkinProfiles.h
M	src/BodyChangeNG/UI.cpp
M	src/main.cpp
M	tests/ActorStateTests.cpp
M	tests/AssetCatalogTests.cpp
M	tests/PresetCatalogTests.cpp
```

### 53. 26408f2 — release: prepare 1.1.0 and bilingual Nexus documentation

- 날짜: 2026-09-04T18:18:25+09:00
- 전체 SHA: `26408f21944131f8ccf9957de6cd3139318cf2c0`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/26408f21944131f8ccf9957de6cd3139318cf2c0)

```text
A	CHANGELOG-KO.md
M	CHANGELOG.md
M	DEPENDENCIES.md
M	README.md
M	THIRD_PARTY_NOTICES.md
A	docs/NEXUS-CHANGELOG-v1.1.0-EN.txt
A	docs/NEXUS-CHANGELOG-v1.1.0-KO.txt
A	docs/NEXUS-DESCRIPTION-v1.1.0-EN.bbcode
A	docs/NEXUS-DESCRIPTION-v1.1.0-KO.html
A	docs/NEXUS-DESCRIPTION-v1.1.0.md
A	docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt
A	docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-KO.txt
A	docs/NEXUS-UPDATE-HISTORY-v1.1.0-EN.md
A	docs/NEXUS-UPDATE-HISTORY-v1.1.0-KO.html
A	docs/README.md
A	docs/RELEASE-NOTES-v1.1.0-KO.md
A	docs/RELEASE-NOTES-v1.1.0.md
A	docs/UPDATE-HISTORY-v1.1.0-KO.md
A	scripts/Package-Release.ps1
M	xmake.lua
```

### 54. 834e007 — packaging: minimize MO2 archive and consolidate license notices

- 날짜: 2026-09-04T19:14:49+09:00
- 전체 SHA: `834e007cc3db64fb8024631e3588ea7af86068d7`
- [변경 diff](https://github.com/compilecraftworks/Body-Change-NG/commit/834e007cc3db64fb8024631e3588ea7af86068d7)

```text
M	README.md
M	THIRD_PARTY_NOTICES.md
M	scripts/Package-Release.ps1
```

## 5. 현재 코드·테스트·스크립트·패키지·문서 목록

기준 커밋의 `rg --files src tests scripts package docs` 결과. 각 링크는 저장소 상대 경로다. 과거 v1.0.0 문서도 이력 보존을 위해 남겨 두었다. 아래 목록은 dependency vendor 전체 목록이 아니라 BCNG 작업 파일 목록이다.

### src (61)

- [src/BodyChangeNG/ActorCatalog.cpp](<../src/BodyChangeNG/ActorCatalog.cpp>)
- [src/BodyChangeNG/ActorCatalog.h](<../src/BodyChangeNG/ActorCatalog.h>)
- [src/BodyChangeNG/ActorEvents.cpp](<../src/BodyChangeNG/ActorEvents.cpp>)
- [src/BodyChangeNG/ActorEvents.h](<../src/BodyChangeNG/ActorEvents.h>)
- [src/BodyChangeNG/ActorRegistry.cpp](<../src/BodyChangeNG/ActorRegistry.cpp>)
- [src/BodyChangeNG/ActorRegistry.h](<../src/BodyChangeNG/ActorRegistry.h>)
- [src/BodyChangeNG/ActorState.h](<../src/BodyChangeNG/ActorState.h>)
- [src/BodyChangeNG/ActorWorkQueue.cpp](<../src/BodyChangeNG/ActorWorkQueue.cpp>)
- [src/BodyChangeNG/ActorWorkQueue.h](<../src/BodyChangeNG/ActorWorkQueue.h>)
- [src/BodyChangeNG/BodyFamily.cpp](<../src/BodyChangeNG/BodyFamily.cpp>)
- [src/BodyChangeNG/BodyFamily.h](<../src/BodyChangeNG/BodyFamily.h>)
- [src/BodyChangeNG/BodyFamilyRules.cpp](<../src/BodyChangeNG/BodyFamilyRules.cpp>)
- [src/BodyChangeNG/CatalogRoots.cpp](<../src/BodyChangeNG/CatalogRoots.cpp>)
- [src/BodyChangeNG/CatalogRoots.h](<../src/BodyChangeNG/CatalogRoots.h>)
- [src/BodyChangeNG/Distribution.cpp](<../src/BodyChangeNG/Distribution.cpp>)
- [src/BodyChangeNG/Distribution.h](<../src/BodyChangeNG/Distribution.h>)
- [src/BodyChangeNG/Hotkey.cpp](<../src/BodyChangeNG/Hotkey.cpp>)
- [src/BodyChangeNG/Hotkey.h](<../src/BodyChangeNG/Hotkey.h>)
- [src/BodyChangeNG/InputSink.cpp](<../src/BodyChangeNG/InputSink.cpp>)
- [src/BodyChangeNG/InputSink.h](<../src/BodyChangeNG/InputSink.h>)
- [src/BodyChangeNG/MenuCameraProjection.h](<../src/BodyChangeNG/MenuCameraProjection.h>)
- [src/BodyChangeNG/MenuCharacterPresentation.cpp](<../src/BodyChangeNG/MenuCharacterPresentation.cpp>)
- [src/BodyChangeNG/MenuCharacterPresentation.h](<../src/BodyChangeNG/MenuCharacterPresentation.h>)
- [src/BodyChangeNG/NativeImGuiHost.cpp](<../src/BodyChangeNG/NativeImGuiHost.cpp>)
- [src/BodyChangeNG/NativeImGuiHost.h](<../src/BodyChangeNG/NativeImGuiHost.h>)
- [src/BodyChangeNG/OutfitRefit.cpp](<../src/BodyChangeNG/OutfitRefit.cpp>)
- [src/BodyChangeNG/OutfitRefit.h](<../src/BodyChangeNG/OutfitRefit.h>)
- [src/BodyChangeNG/OutfitRefitRules.h](<../src/BodyChangeNG/OutfitRefitRules.h>)
- [src/BodyChangeNG/PathMigration.h](<../src/BodyChangeNG/PathMigration.h>)
- [src/BodyChangeNG/PathText.h](<../src/BodyChangeNG/PathText.h>)
- [src/BodyChangeNG/PlayerTint.cpp](<../src/BodyChangeNG/PlayerTint.cpp>)
- [src/BodyChangeNG/PlayerTint.h](<../src/BodyChangeNG/PlayerTint.h>)
- [src/BodyChangeNG/PresetCatalog.cpp](<../src/BodyChangeNG/PresetCatalog.cpp>)
- [src/BodyChangeNG/PresetCatalog.h](<../src/BodyChangeNG/PresetCatalog.h>)
- [src/BodyChangeNG/RaceMenuBodyMorph.cpp](<../src/BodyChangeNG/RaceMenuBodyMorph.cpp>)
- [src/BodyChangeNG/RaceMenuBodyMorph.h](<../src/BodyChangeNG/RaceMenuBodyMorph.h>)
- [src/BodyChangeNG/RaceMenuPresetMigration.cpp](<../src/BodyChangeNG/RaceMenuPresetMigration.cpp>)
- [src/BodyChangeNG/RaceMenuPresetMigration.h](<../src/BodyChangeNG/RaceMenuPresetMigration.h>)
- [src/BodyChangeNG/RaceMenuPresetMigrationRules.cpp](<../src/BodyChangeNG/RaceMenuPresetMigrationRules.cpp>)
- [src/BodyChangeNG/RaceMenuPresetMigrationRules.h](<../src/BodyChangeNG/RaceMenuPresetMigrationRules.h>)
- [src/BodyChangeNG/RuntimeAssetCache.cpp](<../src/BodyChangeNG/RuntimeAssetCache.cpp>)
- [src/BodyChangeNG/RuntimeAssetCache.h](<../src/BodyChangeNG/RuntimeAssetCache.h>)
- [src/BodyChangeNG/RuntimeLayout.h](<../src/BodyChangeNG/RuntimeLayout.h>)
- [src/BodyChangeNG/Settings.cpp](<../src/BodyChangeNG/Settings.cpp>)
- [src/BodyChangeNG/Settings.h](<../src/BodyChangeNG/Settings.h>)
- [src/BodyChangeNG/SkinCatalog.cpp](<../src/BodyChangeNG/SkinCatalog.cpp>)
- [src/BodyChangeNG/SkinCatalog.h](<../src/BodyChangeNG/SkinCatalog.h>)
- [src/BodyChangeNG/SkinGeometryRouting.h](<../src/BodyChangeNG/SkinGeometryRouting.h>)
- [src/BodyChangeNG/SkinOverrideOwnership.h](<../src/BodyChangeNG/SkinOverrideOwnership.h>)
- [src/BodyChangeNG/SkinOverrides.cpp](<../src/BodyChangeNG/SkinOverrides.cpp>)
- [src/BodyChangeNG/SkinOverrides.h](<../src/BodyChangeNG/SkinOverrides.h>)
- [src/BodyChangeNG/SkinProfiles.cpp](<../src/BodyChangeNG/SkinProfiles.cpp>)
- [src/BodyChangeNG/SkinProfiles.h](<../src/BodyChangeNG/SkinProfiles.h>)
- [src/BodyChangeNG/SmoothCamIntegration.cpp](<../src/BodyChangeNG/SmoothCamIntegration.cpp>)
- [src/BodyChangeNG/SmoothCamIntegration.h](<../src/BodyChangeNG/SmoothCamIntegration.h>)
- [src/BodyChangeNG/TextInputFilter.cpp](<../src/BodyChangeNG/TextInputFilter.cpp>)
- [src/BodyChangeNG/TextInputFilter.h](<../src/BodyChangeNG/TextInputFilter.h>)
- [src/BodyChangeNG/UI.cpp](<../src/BodyChangeNG/UI.cpp>)
- [src/BodyChangeNG/UI.h](<../src/BodyChangeNG/UI.h>)
- [src/PCH.h](<../src/PCH.h>)
- [src/main.cpp](<../src/main.cpp>)

### tests (10)

- [tests/ActorStateTests.cpp](<../tests/ActorStateTests.cpp>)
- [tests/AssetCatalogTests.cpp](<../tests/AssetCatalogTests.cpp>)
- [tests/BodyFamilyTests.cpp](<../tests/BodyFamilyTests.cpp>)
- [tests/HotkeyTests.cpp](<../tests/HotkeyTests.cpp>)
- [tests/OutfitRefitRulesTests.cpp](<../tests/OutfitRefitRulesTests.cpp>)
- [tests/PathMigrationTests.cpp](<../tests/PathMigrationTests.cpp>)
- [tests/PresetCatalogTests.cpp](<../tests/PresetCatalogTests.cpp>)
- [tests/RaceMenuPresetMigrationTests.cpp](<../tests/RaceMenuPresetMigrationTests.cpp>)
- [tests/RuntimeLayoutTests.cpp](<../tests/RuntimeLayoutTests.cpp>)
- [tests/SkinOverrideOwnershipTests.cpp](<../tests/SkinOverrideOwnershipTests.cpp>)

### scripts (1)

- [scripts/Package-Release.ps1](<../scripts/Package-Release.ps1>)

### package (4)

- [package/BodySkin/README.txt](<../package/BodySkin/README.txt>)
- [package/CalienteTools/BodySlide/SliderPresets/README.txt](<../package/CalienteTools/BodySlide/SliderPresets/README.txt>)
- [package/SKSE/Plugins/BodyChangeNGdistribution.json](<../package/SKSE/Plugins/BodyChangeNGdistribution.json>)
- [package/TintMask/README.txt](<../package/TintMask/README.txt>)

### docs (30)

- [docs/NEXUS-CHANGELOG-v1.0.0-EN.txt](<../docs/NEXUS-CHANGELOG-v1.0.0-EN.txt>)
- [docs/NEXUS-CHANGELOG-v1.0.0-KO.txt](<../docs/NEXUS-CHANGELOG-v1.0.0-KO.txt>)
- [docs/NEXUS-CHANGELOG-v1.1.0-EN.txt](<../docs/NEXUS-CHANGELOG-v1.1.0-EN.txt>)
- [docs/NEXUS-CHANGELOG-v1.1.0-KO.txt](<../docs/NEXUS-CHANGELOG-v1.1.0-KO.txt>)
- [docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode](<../docs/NEXUS-DESCRIPTION-v1.0.0-EN.bbcode>)
- [docs/NEXUS-DESCRIPTION-v1.0.0-KO.html](<../docs/NEXUS-DESCRIPTION-v1.0.0-KO.html>)
- [docs/NEXUS-DESCRIPTION-v1.0.0.md](<../docs/NEXUS-DESCRIPTION-v1.0.0.md>)
- [docs/NEXUS-DESCRIPTION-v1.1.0-EN.bbcode](<../docs/NEXUS-DESCRIPTION-v1.1.0-EN.bbcode>)
- [docs/NEXUS-DESCRIPTION-v1.1.0-KO.html](<../docs/NEXUS-DESCRIPTION-v1.1.0-KO.html>)
- [docs/NEXUS-DESCRIPTION-v1.1.0.md](<../docs/NEXUS-DESCRIPTION-v1.1.0.md>)
- [docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt](<../docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-EN.txt>)
- [docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-KO.txt](<../docs/NEXUS-SHORT-DESCRIPTION-v1.0.0-KO.txt>)
- [docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt](<../docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-EN.txt>)
- [docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-KO.txt](<../docs/NEXUS-SHORT-DESCRIPTION-v1.1.0-KO.txt>)
- [docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode](<../docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.bbcode>)
- [docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md](<../docs/NEXUS-UPDATE-HISTORY-v1.0.0-EN.md>)
- [docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode](<../docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.bbcode>)
- [docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html](<../docs/NEXUS-UPDATE-HISTORY-v1.0.0-KO.html>)
- [docs/NEXUS-UPDATE-HISTORY-v1.1.0-EN.md](<../docs/NEXUS-UPDATE-HISTORY-v1.1.0-EN.md>)
- [docs/NEXUS-UPDATE-HISTORY-v1.1.0-KO.html](<../docs/NEXUS-UPDATE-HISTORY-v1.1.0-KO.html>)
- [docs/README.md](<../docs/README.md>)
- [docs/RELEASE-NOTES-v1.0.0.md](<../docs/RELEASE-NOTES-v1.0.0.md>)
- [docs/RELEASE-NOTES-v1.1.0-KO.md](<../docs/RELEASE-NOTES-v1.1.0-KO.md>)
- [docs/RELEASE-NOTES-v1.1.0.md](<../docs/RELEASE-NOTES-v1.1.0.md>)
- [docs/Skin Profile Folder.txt](<../docs/Skin Profile Folder.txt>)
- [docs/Skin Texture Folder.txt](<../docs/Skin Texture Folder.txt>)
- [docs/SliderPresets Folder.txt](<../docs/SliderPresets Folder.txt>)
- [docs/UPDATE-HISTORY-v1.0.0-KO.md](<../docs/UPDATE-HISTORY-v1.0.0-KO.md>)
- [docs/UPDATE-HISTORY-v1.1.0-KO.md](<../docs/UPDATE-HISTORY-v1.1.0-KO.md>)
- [docs/skin-profiles.md](<../docs/skin-profiles.md>)

루트 기준 파일: [xmake.lua](../xmake.lua), [xmake-requires.lock](../xmake-requires.lock), [DEPENDENCIES.md](../DEPENDENCIES.md), [README.md](../README.md), [CHANGELOG.md](../CHANGELOG.md), [CHANGELOG-KO.md](../CHANGELOG-KO.md), [LICENSE](../LICENSE), [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## 6. 검증 증거의 의미

| 항목 | 근거 | 한계 |
| --- | --- | --- |
| 현재 릴리스 ZIP과 DLL | 실제 SHA-256·크기 확인 | 게임 동작 증거는 아님 |
| 현재 MO2 DLL | 실제 SHA-256 동일 | 사용자 설정과 외부 모드 조합의 정상 보장 아님 |
| 공개 GitHub | remote/ref 및 release API | Nexus/Drive 업로드 증거 아님 |
| 10 tests exit 0 | 이전 빌드 작업 기록 | 일부 assert가 NDEBUG로 빠질 수 있음 |
| 현재 테스트 소스 | tests 디렉터리 | 실제 engine hook/물리/외부 async를 mock한 통합 테스트가 아님 |
| 첫 1.1.0 source rebuild | 이전 작업의 별도 extract/build 성공 기록 | packaging.1 이후 clean rebuild를 재실행한 것은 아님 |
| 7파일 최소 ZIP | 실제 archive entries + Package-Release allowlist | 기존 MO2 문서 cleanup은 별개 |
| 사용자 스크린샷 | UI/camera 문제의 재현 맥락 | 모든 현재 코드 조합에 대한 검증 아님 |

### 6.1 NDEBUG 확인 기록

현재 로컬 dependency 빌드 메타데이터의 다음 두 경로에서 `-DNDEBUG`가 확인됐다.

```text
build/.deps/BodyChangeNGSkinOverrideOwnershipTests/windows/x64/releasedbg/tests/SkinOverrideOwnershipTests.cpp.obj.d
build/.deps/BodyChangeNGOutfitRefitRulesTests/windows/x64/releasedbg/tests/OutfitRefitRulesTests.cpp.obj.d
```

runtime assert가 사라질 수 있으므로 항상 동작하는 Require/Expect 또는 assert-enabled debug로 검증을 보충해야 한다. 이 파일들은 빌드 산출물이라 source ZIP/Git에 포함되지 않는다. 증거를 위해 이 위치와 의미를 기록했을 뿐 이 인수인계에서 런타임/테스트 코드를 변경하지 않았다.

## 7. 업데이트 방법

1. 기준 HEAD, 태그, DLL/ZIP 해시를 다시 읽는다.
2. 새 작업자의 변경 파일과 테스트 결과를 추가한다.
3. 기존 제안/추정 항목은 증거가 생기기 전 완료로 바꾸지 않는다.
4. Git 전체 이력을 다시 생성할 때 이 문서의 자기 참조를 피하기 위해 스냅샷 기준 커밋을 명시한다.
5. 이 부록과 본문은 개발/소스 문서이며 최소 MO2 ZIP allowlist에 추가하지 않는다.
