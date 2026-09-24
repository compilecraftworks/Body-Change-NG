BODY CHANGE NG — NPC 배포 사전 설정 / NPC distribution authoring
Schema 8 · 한글 / English

[한글]
1. 무엇을 할 수 있나요?
인게임 창을 열지 않고, NPC 조건과 바디 프리셋·바디스킨·후타스킨·오버레이
후보를 텍스트로 지정할 수 있습니다. 예를 들어 "Follower A"에게 "Preset A"를
항상 주거나, 노드 여성에게 여러 프리셋 중 하나를 배포할 수 있습니다.
이것은 BCNG 자체 형식이며 OBody 배포 JSON을 읽는 기능은 아닙니다.
숫자 ID나 별도 도구는 일반적인 이름 지정에 필요하지 않습니다.

2. 어느 파일을 편집하나요?
게임이 읽는 전체 경로:
Data\SKSE\Plugins\BodyChangeNGdistribution.json
MO2 콘텐츠 모드:
mods\Body Change NG\SKSE\Plugins\BodyChangeNGdistribution.json
단, Overwrite나 다른 모드가 같은 파일을 덮으면 최종 승자 파일을 편집하세요.
파일을 자동 병합하지 않습니다. 게임 종료 후 원본을 백업하고 UTF-8로 저장하세요.
이 새 형식은 schema 8 지원 DLL이 필요합니다. 예전 schema 7용 DLL에는
새 JSON만 따로 넣지 마세요.

3. 처음 작성하는 예
동봉 JSON 맨 위의 비어 있는 rules 배열을 다음처럼 바꿉니다.
(샘플 이름은 실제 NPC·프리셋·스킨 이름으로 바꿔야 합니다.)

{
  "schemaVersion": 8,
  "rules": [
    {
      "id": "follower-a-body",
      "name": "A 동료 체형",
      "sex": "female",
      "scope": "name",
      "target": "Follower A",
      "presets": ["Preset A"]
    },
    {
      "id": "follower-b-skin",
      "name": "B 동료 스킨",
      "sex": "female",
      "scope": "name",
      "target": "Follower B",
      "skins": ["My Skin Pack"]
    }
  ]
}

id는 규칙마다 다르게 쓰고 배포 후에는 가급적 유지하세요.
name은 설명 제목입니다. NPC 이름은 target에 씁니다.
이름이 같은 여러 NPC에게는 모두 해당합니다. 특정 NPC Base만 지정하려면
scope: "npc"와 고유 EditorID/이름을 쓰세요. 동명 구분은 아래 6번을 참고하세요.
주석 속 예시는 비활성입니다. 필요한 { ... }만 맨 위 rules 안에 복사해야 합니다.
여러 객체 사이에는 쉼표가 필요하고 마지막 객체 뒤에는 쉼표를 넣지 않습니다.

4. 각 후보의 이름은 어디서 얻나요?
presets: BodySlide Preset 드롭다운/BCNG 바디프리셋 목록 이름.
  XML의 <Preset name="..."> 값이며 XML 파일명이 아닙니다.
  설치 경로: Data\CalienteTools\BodySlide\SliderPresets\<파일>.xml
skins: BCNG 바디스킨 목록의 팩 이름.
  Data\BodySkin\<스킨팩>\textures\... 아래에 모드 폴더의 텍스처 구조를 유지합니다.
futaSkins: BCNG 후타스킨 목록의 팩 이름.
  Data\Futanari\<후타팩>\textures\... 아래에 텍스처 구조를 유지합니다.
overlays: BCNG 오버레이 목록에 등록된 전체 이름.
  face/body/hands/feet 부위별로 넣습니다. RaceMenu/SlaveTats에 등록된
  설치 모드의 항목이어야 합니다. 임의 DDS 파일을 새로 등록하는 기능이 아닙니다.
틴트마스크는 플레이어 전용이므로 NPC 배포 채널이 없습니다.

후보 여러 개:
  "presets": ["Preset A", "Preset B", "Preset C"]
  "skins": ["Skin Pack A", "Skin Pack B"]
  "futaSkins": ["Futa Pack A", "Futa Pack B"]
  "overlays": {
    "body": [
      {"name": "Paint A", "color": "#FF000080"},
      {"name": "Paint B", "color": "#FFFFFFFF"}
    ],
    "hands": ["Hand Paint A"]
  }
각 기능/오버레이 부위에서 호환 후보 하나를 NPC별로 안정적으로 선택합니다.
전부 겹쳐 적용하거나 프리셋 첫째+스킨 첫째처럼 세트로 묶는 뜻이 아닙니다.
색은 #RRGGBBAA, AA는 00 투명~FF 불투명입니다. #RRGGBB는 불투명.
후보가 하나면 그 항목을 고정 배포합니다.

5. 종족·팩션·클래스 등을 어떻게 쓰나요?
성별 sex는 "female"/"male", scope와 target은 다음처럼 씁니다.
  전체:          "scope": "all"                         (target 없음)
  NPC Base:      "scope": "npc", "target": "MyFollowerEditorID"
  NPC 이름:      "scope": "name", "target": "Follower A"
  소속 팩션:     "scope": "faction", "target": "BanditFaction"
  원본 플러그인: "scope": "plugin", "target": "MyFollowers.esp"
  종족:          "scope": "race", "target": "NordRace"
  커스텀 동료:   "scope": "customFollowers"             (target 없음)
  노인:          "scope": "elders"                      (target 없음)
  NPC 키워드:    "scope": "keyword", "target": "ActorTypeNPC"
  클래스:        "scope": "class", "target": "CombatWarrior1H"
  전투스타일:    "scope": "combatStyle", "target": "csHumanMeleeLvl1"
동봉 JSON에는 그대로 복사 가능한 실제 바닐라 이름 39개를 분류별로 실었습니다.
종족에는 흡혈귀 종족을 따로 지정합니다. NordRace는 NordRaceVampire가 아닙니다.
커스텀 조건은 작성자의 EditorID 또는 게임/BCNG 조건 목록 이름을 씁니다.
번역된 표시 이름은 언어가 바뀌면 달라질 수 있어 EditorID가 더 이식성 있습니다.
팩션은 해당 NPC의 실제 소속, 클래스·전투스타일은 실제 레코드 값을 따릅니다.
plugin은 NPC를 최초 정의한 파일이며 마지막 리텍 패치 이름이 아닙니다.
조건을 여러 개 AND/OR로 합치는 새 문법은 없습니다.

6. 이름이 겹치거나 이름이 없는 경우
같은 대상 이름: "target": {"name": "MyRace", "plugin": "MyRaces.esp"}
같은 프리셋 이름: "presets": [{"name": "Preset A", "file": "Folder/My Presets.xml"}]
  file은 SliderPresets 아래 XML 상대 경로입니다.
같은 후타팩 이름: "futaSkins": [{"name": "My Futa Pack", "type": "TRX"}]
  type: ERF / TRX / UBE-TRX. 자동 UBE 배포 제한을 풀지는 않습니다.
같은 오버레이 이름:
  "overlays": {"body": [{"name": "Paint A", "texture": "Actors/Character/Overlays/A.dds"}]}
  texture는 실제 등록된 경로 그대로 쓰세요.
인게임 저장은 정확히 선택한 항목을 유지하려고 file/type/texture를 붙일 수 있습니다.
이름 후보가 현재 액터와 호환되는 항목 중 하나로 좁혀져야 적용합니다.

일부 커스텀 레코드는 이름/EditorID를 실행 중 제공하지 않습니다.
그때만 다음 고급 표기를 씁니다.
  "scope": "npc",
  "target": {"plugin": "MyFollowers.esp", "formId": "0x1234", "label": "Follower A"}
formId는 원본 플러그인 내부의 로컬 16진 Base ID입니다. 전체 RefID가 아닙니다.
ESL은 로컬 3자리, 일반 ESP/ESM은 로컬 6자리 이내이며 앞의 로드순서는 빼세요.
label은 메모이며 대체 검색어가 아닙니다. 보통은 이름만 쓰면 됩니다.
못 찾거나 중복인 이름은 저장에서 삭제하지 않으며 잘못된 다른 항목을 적용하지 않습니다.

7. 옵션·순서·기존 제한
all의 excludeCustomFollowers와 excludeElders는 기본 true(제외)입니다.
다른 scope에는 숨은 제외 조건으로 작용하지 않습니다.
"enabled": false는 schema 8에서 그 규칙을 잠시 끕니다.
각 기능/부위는 위쪽의 일치하고 호환되는 규칙을 우선합니다.
하나의 객체에 여러 기능을 써도 되며 저장하면 기능별 규칙으로 분리될 수 있습니다.
최대 256개(분리 후). 일반 자동 배포의 성별·바디타입·비UBE 제한은 그대로입니다.
후타스킨은 이미 등록된 여성 후타에게 애드온 타입이 맞을 때만 적용합니다.
이름 지정을 추가했다고 액터 호환성·게임 지원 범위가 넓어진 것은 아닙니다.
직접 확정한 수동 선택은 자동 배포보다 우선합니다. 규칙을 끄거나 지웠다고
세이브에 이미 기록된 선택을 전부 초기화하지는 않습니다.

8. 언제 적용되나요? 다른 모드팩으로 옮겨도 되나요?
다음 게임 실행 때 JSON을 읽고 조건에 맞는 NPC가 로드될 때 처리합니다.
인게임 창에서 배포 버튼을 다시 누를 필요는 없습니다(삭제 준비 모드 제외).
인게임 편집은 즉시 배포 또는 다음 게임 배포를 눌러야 저장됩니다.
같은 파일을 다른 인스턴스에 넣을 수 있지만 참조하는 플러그인·프리셋·팩도
있어야 합니다. 이름/플러그인+로컬 ID는 전체 로드순서 ID를 직접 쓰지 않습니다.
플러그인의 로컬 레코드 자체 변경/ESL 압축, 표시 이름 번역, 에셋 이름 변경은
자동 추적할 수 없습니다. 관련 모드도 함께 맞춰 주세요.

9. 기존 JSON과 주석은 어떻게 되나요?
schema 3~7을 읽어 schema 8로 자동 변환합니다. 먼저 원본 옆에
BodyChangeNGdistribution.json.schema7.bak 같은 백업을 만듭니다.
기존 백업이 있으면 .schema7.1.bak 등으로 번호를 붙이고 덮어쓰지 않습니다.
백업/변환/저장 실패 시 원본을 보존합니다.
확인 가능한 에셋은 이름+구분자로, 미설치/모호한 옛 에셋은 {"id":"기존 ID"}로
유지합니다. 규칙 순서와 후보·색상을 유지하며 기존 옛 제외 규칙 정리는 그대로입니다.
옛 버전은 enabled를 무시했으므로 기존 파일의 false는 동작 유지상 true로 옮깁니다.
인게임 저장 후 활성 JSON은 위쪽, 기존 주석·예시는 아래쪽에 남습니다.
옛 주석을 최신 안내로 자동 바꾸지는 않으니 형식은 동봉 새 가이드를 참고하세요.
활성 사용자 JSON을 빈 시작 파일로 덮어쓰지 마세요.

10. 적용되지 않으면
BodyChangeNG.log의 rules[번호] 문법 오류부터 확인하세요(번호는 0부터).
scope/sex 오타, 마지막 쉼표, 잘못된 색상은 고쳐야 합니다.
그 다음 MO2 승자 파일, 실제 설치 목록의 이름·공백·대소문자, 중복 이름,
대상 NPC의 성별/몸체/후타 등록, 수동 확정 선택, 삭제 준비 모드를 확인하세요.
이름을 못 찾았다는 이유로 전체 NPC 조건으로 확대해서 적용하지 않습니다.

[English]
1. Edit Data\SKSE\Plugins\BodyChangeNGdistribution.json with Skyrim closed.
   In MO2, edit the winning file (possibly Overwrite), not a shadowed copy.
   Back it up and save as UTF-8. Schema 8 requires the updated DLL; do not
   distribute only this new JSON to an older schema-7 DLL.
2. Copy a commented example's object into the active rules array at the top.
   The shipped array is empty. Separate objects with commas, without a trailing
   comma. Comments below remain inactive and are preserved on in-game saves.
3. Write preset names from the BodySlide Preset dropdown / XML Preset name,
   skin/futa pack names from BCNG, and complete registered overlay names.
   Preserve asset spelling, spaces and case. No numeric-ID helper is required.
   Paths:
   Data\CalienteTools\BodySlide\SliderPresets\<file>.xml
   Data\BodySkin\<Skin pack>\textures\...
   Data\Futanari\<Futa pack>\textures\...
4. Supply one or several candidates in presets, skins, futaSkins and overlays.
   See the bilingual JSON's 19 examples and complete two-rule configuration.
   Pools select one compatible candidate independently per feature/overlay area,
   not a stack of all candidates or paired body/skin sets.
   Overlay areas: face/body/hands/feet. color uses #RRGGBBAA (AA=opacity);
   six digits mean opaque, omitted color defaults to #FFFFFFFF.
   Player Tint Masks do not have an NPC distribution channel.
5. sex is female/male. All eleven scopes are supported:
   all, npc, name, faction, plugin, race, customFollowers, elders, keyword,
   class, combatStyle. Omit target for all/customFollowers/elders.
   Other targets use names: NordRace, BanditFaction, ActorTypeNPC,
   CombatWarrior1H, csHumanMeleeLvl1, an NPC name or a plugin filename.
   The JSON guide lists 39 copyable vanilla EditorIDs, including vampire races.
   Custom targets prefer EditorID, then unique game-provided display name.
   Scope name matches every NPC base with that display name; it is affected by
   translations. Scope npc targets one ActorBase, not an individual placed RefID.
   Scope plugin uses the original defining plugin, not the last replacer.
6. Qualify duplicates with target: {"name":"MyRace","plugin":"MyRaces.esp"};
   preset: {"name":"Preset A","file":"Folder/My Presets.xml"};
   futa: {"name":"My Futa Pack","type":"TRX"} (ERF/TRX/UBE-TRX);
   overlay: {"name":"Paint A","texture":"exact/registered/path.dds"}.
   In-game saves may include file/type/texture to retain the exact selected item.
   Some custom records expose no runtime name/EditorID. Only then use the
   advanced target: {"plugin":"MyFollowers.esp","formId":"0x1234","label":"Note"}.
   This is a hexadecimal local Base ID (regular: up to 6 digits; light: 3),
   not a full runtime ID or placed RefID. label is not a fallback identity.
   Unknown/ambiguous names are retained but are not applied arbitrarily.
7. For all only, excludeCustomFollowers/excludeElders default to true.
   enabled:false pauses one schema-8 rule. Earlier matching compatible rules
   win per feature/area. Mixed-feature rows may split on save (256-row limit).
   Existing automatic sex/body-family/non-UBE restrictions remain. Futa rules
   require an already registered compatible female addon; they do not register
   or replace it. This format does not introduce arbitrary AND/OR conditions.
8. Valid rules load on the next game launch and process eligible NPCs as they
   load, unless removal mode is active. No in-game distribution click is needed.
   Manual actor choices in the co-save take priority. Disabling/deleting a rule
   does not reset every previously saved actor choice.
   Files can be moved to another instance if referenced mods/assets also exist.
   Names or plugin/local IDs avoid full load-order IDs, but renamed records,
   translated display names, renamed assets or ESL-compacted local IDs may need
   edits. This is BCNG's format, not an OBody distribution-schema importer.
9. Schemas 3..7 automatically migrate with a byte-exact .schemaN.bak backup
   beside the source, or numbered backups if already present. Failures leave
   the source intact. Known assets become named references; unresolved old IDs
   remain {"id":"original ID"}. Rule order/candidates/colors remain; previous
   legacy exclusion migrations still apply. Old enabled flags were ignored, so
   migrate as true. Existing comments remain below active rules; consult the
   new guide rather than outdated comments. Never overwrite your custom rules
   with the empty installer starter.
10. Check BodyChangeNG.log for rules[index] syntax/unknown-field errors
    (zero-based). Then check MO2 priority, actual asset names, duplicates, actor
    compatibility, manual overrides and removal mode. In-game edits are saved
    only with Distribute now or Distribute next game launch.
