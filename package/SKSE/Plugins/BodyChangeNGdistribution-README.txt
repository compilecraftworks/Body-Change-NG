Body Change NG — Offline distribution authoring / 게임 밖에서 배포 규칙 작성
Schema 7 · BCNG v1.3.3+ comment support · 2026-09-24

==================== 한국어 ====================

1. 무엇을 하는 파일인가?

Data\SKSE\Plugins\BodyChangeNGdistribution.json

이 파일은 모든 세이브에서 읽는 공용 NPC 배포 규칙입니다. 게임을 시작할 때
읽고 조건에 맞는 NPC의 3D가 로드되면 적용합니다. 이용자가 BCNG 창이나
배포 버튼을 누를 필요는 없습니다. 플레이어에게 배포하는 기능은 아닙니다.
필요한 바디/프리셋/스킨/플러그인은 따로 설치되어 있어야 합니다.
인게임 액터 드롭다운에서 직접 확정한 선택은 이 파일이 아닌 그 세이브에
저장되며, 해당 기능의 자동 배포보다 우선합니다.

MO2 모드 폴더 안에서는 Data를 빼고 SKSE\Plugins부터 만드세요.
Overwrite나 다른 출력 모드에도 같은 파일이 있으면 그 파일이 우선할 수
있습니다. 게임에서 실제 사용되는 파일 하나를 확인하고 편집하세요.
여러 모드가 같은 JSON을 제공해도 자동으로 병합하지 않습니다.

먼저 Skyrim을 완전히 종료하고 기존 JSON을 백업하세요.
메모장 같은 텍스트 편집기로 열어 UTF-8로 저장합니다.
BCNG 1.3.3부터 이 배포 파일에서 // 한 줄 주석과 /* ... */ 여러 줄 주석을
읽습니다. 1.3.2 이하 DLL에는 주석 파일을 넣지 마세요. 기존 주석 없는
schema 7 파일도 계속 사용할 수 있습니다. settings.json 등 다른 JSON은
이 변경의 대상이 아닙니다. 그 파일에는 주석을 넣지 마세요.
실제 적용되는 것은 rules 배열뿐이며 기본 []는 자동 배포 없음입니다.
인게임에서 규칙을 저장하면 주석은 없어집니다. 이 별도 안내와 원본
템플릿을 보관하세요. 인게임 저장은 활성 rules를 정상 저장합니다.

2. 가장 간단한 시작: 동료 A는 프리셋 A, 동료 B는 프리셋 B

아래는 완전한 JSON 예시입니다. 이름과 숫자는 모두 가상 값입니다.
실제 NPC 플러그인/로컬 BaseID/XML 경로/XML Preset name으로 바꾸세요.
기존 규칙이 있다면 파일 전체를 덮어쓰지 말고 rules 안에 객체를 추가하세요.
종족과 복수 후보를 함께 쓰는 예제는 JSON의 COMPLETE CONFIGURATION에 있습니다.
노드 여성에게 바디 후보 2개 중 하나와 스킨 후보 2개 중 하나를 주는 완성 파일입니다.

{
  "schemaVersion": 7,
  "rules": [
    {
      "id": "follower-a-body",
      "name": "Follower A body",
      "female": true,
      "scope": 1,
      "npcPlugin": "My Followers.esp",
      "npcLocalFormID": 4660,
      "presetIds": ["My Presets.xml\u001fPreset A"]
    },
    {
      "id": "follower-b-body",
      "name": "Follower B body",
      "female": true,
      "scope": 1,
      "npcPlugin": "My Followers.esp",
      "npcLocalFormID": 4661,
      "presetIds": ["My Presets.xml\u001fPreset B"]
    }
  ]
}

동봉 JSON 맨 위 rules 안의 첫 예제는 양쪽 표식 줄
/* EXAMPLE: individual_body 와 END EXAMPLE */ 를 지우면 활성화됩니다.
파일 아래쪽의 다른 예제는 { ... } 객체만 위 rules 배열 안으로 복사하세요.
아래쪽에서 주석만 해제하면 JSON 뒤에 두 번째 객체가 생겨 오류가 납니다.
예제 19개는 모든 대상 조건 0~10과 바디·스킨·후타·오버레이를 다룹니다.
두 객체 사이는 쉼표로 구분하고 마지막 객체 뒤에는 쉼표를 넣지 않습니다.
주석 속 예제는 아무 NPC에게도 적용되지 않습니다. 중첩 /* */ 주석은 쓰지
마세요. 규칙을 주석으로 끌 때도 남은 객체 사이의 쉼표를 다시 확인하세요.

3. NPC의 올바른 ID 찾기 — RefID와 다릅니다

NPC 원본 레코드(NPC_)의 최초 정의 플러그인과 로컬 Base FormID를 사용합니다.
NPC 레코드 편집기(예: xEdit)에서 NPC_ 레코드의 원본을 확인할 수 있습니다.
마지막으로 얼굴/의상을 덮어쓴 동료 리텍 플러그인을 무조건 쓰면 안 됩니다.
예를 들어 원본이 Skyrim.esm에 있고 다른 ESP가 외형만 덮어쓴 NPC라면
npcPlugin에는 Skyrim.esm을 사용합니다.
게임에서 소환된 개체의 RefID, ACHR 레코드, NPC 표시 이름을 숫자 대신
넣지 마세요. 같은 BaseID로 생성된 여러 액터는 모두 그 규칙의 대상입니다.

일반 ESP/ESM은 전체 ID에서 앞의 로드 순서 2자리를 뺀 하위 6자리,
ESL 또는 ESL 플래그 플러그인은 FE와 라이트 인덱스를 뺀 하위 3자리를
사용합니다. 확장자는 .esp여도 ESL 플래그가 있을 수 있습니다.
  일반: 05123456 -> 로컬 0x123456 -> JSON 숫자 1193046
  라이트: FE012ABC -> 로컬 0xABC -> JSON 숫자 2748
  예제: 로컬 0x1234 -> JSON 숫자 4660

Windows 계산기 프로그래머 모드에서 HEX 값을 DEC로 바꾸면 됩니다.
JSON에는 10진 정수를 따옴표 없이 씁니다. 0x1234나 "0x1234"는 안 됩니다.
플러그인 파일명은 확장자를 포함합니다. 로드 순서가 바뀌어도 원본 플러그인과
로컬 ID 조합은 유지됩니다. 레코드 자체를 압축/리넘버링하면 다시 확인하세요.
플러그인 파일명·로컬 ID를 찾기 어렵다면 scope 2 이름 규칙을 쓸 수 있지만,
동명이인·번역·이름 변경에 영향을 받으므로 팩 배포에는 scope 1이 적합합니다.

4. 바디프리셋 ID 찾기 — 게임 실행 없이 가능

프리셋 전체 위치:
Data\CalienteTools\BodySlide\SliderPresets\My Presets.xml

XML을 텍스트로 열면 <Preset name="Preset A" set="..."> 항목이 있습니다.
파일 이름과 Preset name은 별개이며 XML 하나에 프리셋이 여러 개일 수 있습니다.
ID는 SliderPresets 기준 상대 파일 경로 + 구분자 \u001f + Preset name입니다.
  My Presets.xml\u001fPreset A
하위 폴더가 있으면 슬래시(/)로 포함합니다.
  My Pack/My Presets.xml\u001fPreset A

JSON 문자열에 \u001f를 그대로 한 번 씁니다. \\u001f로 두 번 이스케이프하면
다른 문자열이 됩니다. 대소문자·공백·하위 경로까지 정확히 맞추세요.
XML 속성에 &amp; 같은 엔티티가 있으면 해석된 이름(&)이 ID에 들어갑니다.
이름이 -Refit으로 끝나는 별도 의상 보정 프리셋은 일반 바디 후보가 아닙니다.

동봉 Tools\BodyChangeNG-RuleId.ps1은 읽기 전용 ID 도구입니다.
아래 경로를 본인의 실제 MO2 경로로 바꿔 PowerShell에서 실행하세요.
게임이나 MO2를 통해 실행할 필요가 없습니다. PowerShell 실행 정책으로
차단되면 조직/PC 정책을 임의로 낮추지 말고 위 수동 작성법을 사용하세요.

& "D:\My MO2\mods\Body Change NG\Tools\BodyChangeNG-RuleId.ps1" -PresetXml "D:\My MO2\mods\My Presets\CalienteTools\BodySlide\SliderPresets\My Presets.xml"

출력의 id 문자열을 따옴표까지 복사해 presetIds 배열에 넣습니다.
XML이 SliderPresets 바깥에 있으면 -PresetRoot로 게임의 SliderPresets에
해당할 실제 폴더를 지정하세요. 상대 경로는 실제 설치 결과와 같아야 합니다.
도구는 파일을 수정하지 않으며 ID 출력만으로 성별·바디 호환성을 검증하지는
않습니다. 프리셋을 사용할 바디·의상은 Zeroed Sliders + Build Morphs로
빌드되어 있어야 합니다.

5. 규칙의 모든 필드와 대상 조건

동봉 JSON의 ALL FIELDS 주석에는 필드 19개를, SCOPE 주석에는 대상 종류
0~10을 한영으로 설명했습니다. OTHER EXAMPLES에서 해당 예제를 찾으세요.
필요 없는 필드는 생략하세요. 핵심은 id, female, scope, 대상 식별자,
그리고 한 가지 기능의 후보 배열입니다.

무엇을 입력할지 모르겠다면 JSON의 REAL CONDITION VALUES를 먼저 보세요.
기본 게임 종족·팩션·키워드·클래스·전투스타일 39개의 실제 값이 있습니다.
예를 들어 노드는 scope=5, targetPlugin="Skyrim.esm",
targetLocalFormID=79686입니다. 한글 "노드"를 target에 넣는 방식이 아닙니다.
노드 뱀파이어는 다른 레코드이므로 558996으로 별도 규칙을 작성합니다.
기본 게임 조건 예제는 실제 값이고, My... NPC/에셋은 여전히 교체할 가상 값입니다.

모드가 추가한 조건은 xEdit(SSEEdit)에서 실제 설치 플러그인을 읽어 찾습니다.
NPC_의 최종 덮어쓰기에서 종족/팩션/키워드/클래스/전투스타일 참조를 확인하고,
그 참조의 RACE/FACT/KYWD/CLAS/CSTY 원본 플러그인과 로컬 ID를 사용하세요.
게임은 실행할 필요가 없으며, 플러그인을 수정 저장할 필요도 없습니다.
전투스타일은 들고 있는 무기가 아니라 지정된 CSTY 레코드가 일치해야 합니다.
화면상 직업이나 모드 이름으로 내부 식별자를 추측하면 안 됩니다.

scope 0: 전체 NPC. includeCustomFollowers와 includeElderNPCs가 적용됩니다.
scope 1: 개별 NPC 원본. npcPlugin + npcLocalFormID.
scope 2: NPC 원본 이름. target에 이름 완전 일치. 부분 검색이 아닙니다.
         ASCII 대소문자는 구분하지 않지만 표시용 별명과 다를 수 있습니다.
scope 3: NPC 원본의 팩션 소속. targetPlugin + targetLocalFormID.
scope 4: NPC 원본을 최초 정의한 플러그인. target에 파일명.
         리텍 ESP가 덮어쓴 NPC들을 모두 찾는 조건이 아닙니다.
scope 5: 종족. targetPlugin + targetLocalFormID.
scope 6: 커스텀 팔로워 판정. 구형 파일 호환용으로 지원되는 범위.
scope 7: 노인 판정. 구형 파일 호환용으로 지원되는 범위.
scope 8: NPC 원본 키워드. targetPlugin + targetLocalFormID.
scope 9: NPC 클래스. targetPlugin + targetLocalFormID.
scope 10: NPC 전투스타일. targetPlugin + targetLocalFormID. 구형 호환 범위.

3/5/8/9/10의 targetLocalFormID는 NPC가 아닌 해당 조건 레코드의 로컬 ID입니다.
target의 정확한 EditorID도 구형 대체 식별자로 지원하지만, 팩에는 원본
플러그인+로컬 ID를 권장합니다. 인게임에서 읽은 표시 이름과 EditorID는
다릅니다. 잘못된 폼 타입은 일치하지 않습니다.
한 규칙은 성별 + 한 scope + 호환 조건으로 작동합니다. 여러 scope 조건의
AND 조합이나 중첩 조건 문법은 없습니다. 규칙을 두 개 만든다고 AND가
되지 않습니다. 둘 중 하나에 맞는 대상들을 각각 처리하는 방식입니다.
커스텀 팔로워는 공식 본편/DLC 외 플러그인 출신이며 플레이어 팀원이거나
팔로워 팩션에 속한 NPC를 뜻합니다. 노인은 종족 EditorID의 elder 또는
음성 EditorID의 old/elder로 판정하며 화면 외모를 분석하지 않습니다.

female은 true/false 불리언이며 "true", "female" 같은 문자열이 아닙니다.
scope 0의 includeCustomFollowers=false는 커스텀 팔로워 제외,
includeElderNPCs=false는 노인 제외입니다. 둘 다 기본값 false입니다.
이 두 필터는 특정 NPC·이름·팩션 등 다른 scope에는 적용되지 않습니다.
enabled=false는 현재 버전에서 비활성화 수단이 아닙니다. 로드 정규화에서
true가 됩니다. 끄려면 규칙 객체를 별도 파일에 보관하고 rules에서 빼거나,
객체 전체를 /* ... */ 주석으로 감싸세요. 남은 객체 사이 쉼표도 확인하세요.
기존 적용 외형을 자동으로 원복하는 동작과 규칙 삭제는 다릅니다.
규칙을 지워도 기존 세이브의 배정이 남을 수 있습니다.

6. 스킨·후타·오버레이 후보와 색상

몸/얼굴 스킨은 skinProfileIds:
  Data\BodySkin\My Skin Pack\Textures\actors\character\female\femalehead.dds
  -> "auto:My Skin Pack:female"
  남성 팩 -> "auto:My Skin Pack:male"
팩이 실제 인식 가능한 DDS·경로를 갖춰야 합니다. 자동 배포는 인간형의
지원 일반 바디 스킨을 대상으로 하며 UBE·아르고니안·카짓 스킨 후보는
이 자동 스킨 풀에 들어가지 않습니다. 수동 선택 범위와 혼동하지 마세요.

후타스킨은 futanariSkinIds:
  Data\Futanari\My Futa Pack\Textures\[TRX] Futa addon\Regular\Default\schlong.dds
  -> "futanari:My Futa Pack:cbbe-trx"
  Data\Futanari\My Futa Pack\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds
  -> "futanari:My Futa Pack:erf"
female=true인 규칙만 사용하며, 실제 SOS/TNG 등록 유형에 맞는 후보를
선택합니다. 이 규칙으로 후타 등록 자체를 하지는 않습니다.
UBE TRX 자동 후타스킨 배포는 지원하지 않습니다.

오버레이는 overlayIds: [얼굴 후보 배열, 몸 후보 배열, 손 후보 배열, 발 후보 배열].
빈 부위는 []로 둡니다. 해당 부위의 실제 카탈로그 ID가 필요합니다.
DDS 파일명이나 표시 이름만 쓰면 안 됩니다. 등록된 텍스처 경로와 부위로
읽기 전용 도구에서 ID를 계산할 수 있습니다.

& "D:\My MO2\mods\Body Change NG\Tools\BodyChangeNG-RuleId.ps1" -OverlayArea body -OverlayTexture "Actors\Character\Overlays\My Paint.dds"

RaceMenu/SlaveTats가 실제 등록하는 texture 문자열을 넣으세요.
다른 부위 또는 Textures\ 접두사 차이는 ID가 달라집니다.
텍스처를 정상 설치·등록하지 않고 ID만 계산해도 새 오버레이가 생기지 않습니다.
UBE 오버레이는 자동 배포 풀에 넣지 않습니다. 외부 모드의 슬롯은 침범하지
않으므로 슬롯 부족·성별·레이아웃 불일치 시 보이지 않을 수 있습니다.

overlayColors는 동일한 부위 순서의 객체 4개입니다.
각 객체의 키는 그 부위 overlayIds에 있는 정확한 ID,
값은 AARRGGBB를 10진 정수로 바꾼 값입니다.
  흰색 불투명: 0xFFFFFFFF -> 4294967295
  빨강 약 50% 불투명: 0x80FF0000 -> 2164195328
없으면 흰색 불투명입니다. 없는 후보의 색은 정리됩니다.
자동 배포는 부위당 후보 하나를 선택합니다. 수동 복수 오버레이 적용처럼
모든 후보를 동시에 쌓지 않습니다. 틴트마스크는 NPC 배포 대상이 아닙니다.

7. 바디 타입 사전 설정, 우선순위와 한도

여러 후보를 적는 방법은 JSON의 MULTIPLE CANDIDATES에 완전한 예제가 있습니다.
바디프리셋은 presetIds의 문자열 배열, 바디스킨은 skinProfileIds의 문자열 배열,
후타스킨은 futanariSkinIds의 문자열 배열입니다. 각 문자열 사이에 쉼표를 씁니다.
예: "skinProfileIds": ["auto:My Skin A:female", "auto:My Skin B:female"]
후보마다 실제 팩 폴더명이 다릅니다. DDS 파일 경로나 XML 파일 경로만 넣지 마세요.
바디/스킨/후타는 각각 호환 후보 하나를 선택하며 동시에 모두 적용하지 않습니다.
바디와 스킨을 모두 주려면 별도 규칙으로 작성합니다. 후보 A끼리 짝을 맞추거나
NPC마다 순서대로 하나씩 나눠 주는 기능은 없습니다. 여러 NPC가 같은 후보를
선택할 수도 있습니다. 위에 둔 같은 기능의 고정 규칙이 먼저 적용될 수 있습니다.

Data\SKSE\Plugins\BodyChangeNG\settings.json
규칙의 bodyFamily는 옛 필드라 현재는 지워집니다. 여성·남성 배포 타입은
이 설정 파일의 femaleNpcBodyType, maleNpcBodyType 숫자를 사용합니다.
  femaleNpcBodyType: 0=CBBE 3BA(기본), 1=BHUNP/UNP, 2=UBE, 3=바닐라
  maleNpcBodyType:   0=HIMBO(기본), 1=SAM, 2=바닐라
숫자 2 UBE 항목이 존재해도 현재 자동 후보 풀은 UBE 프리셋/스킨을 받지
않습니다. UBE 배포를 켜는 방법으로 안내하지 않습니다.
바닐라는 해당 성별의 자동 바디프리셋 제거 정책이므로 프리셋 지정용으로
선택하지 마세요. 성별당 타입 하나이며 규칙별 타입을 지정할 수 없습니다.
기존 settings.json이 있으면 필요한 필드만 수정하고 나머지는 보존하세요.
설정 파일이 없을 때 CBBE 3BA/HIMBO 기본값이면 새 파일이 필요 없습니다.
BHUNP/SAM 팩 등 기본값과 다를 때의 최소 설정 예:
{"femaleNpcBodyType": 1, "maleNpcBodyType": 1}
설정 파일도 MO2 최종 승리 경로에 있어야 합니다. removalMode=true인
삭제 준비 상태에서는 자동 배포·보정이 중단됩니다.

규칙은 위에서 아래로 기능별로 검사합니다. 호환 후보를 얻는 첫 규칙이
해당 기능을 결정합니다. 개별 동료 규칙을 전체 NPC 규칙보다 위에 놓으세요.
프리셋 규칙이 스킨 규칙을 가리지 않으며 오버레이도 부위별로 독립입니다.
배포 후보 하나면 고정, 여러 개면 호환 후보 중 하나입니다. 기존 자동 선택이
현재 후보에 남아 있으면 유지될 수 있으며 매 로드마다 새로 뽑지 않습니다.
복수 후보의 선택 결과는 로드 순서가 다른 구성에서 달라질 수 있습니다.
대상을 찾는 플러그인+로컬 ID는 안정적이지만 고정 프리셋이면 후보를 하나만 쓰세요.
수동 확정·명시적 기본값 선택은 해당 기능의 자동 규칙보다 우선합니다.
근처 액터 목록의 4,096 거리·32명 제한은 규칙 배포 한도가 아닙니다.

각 객체는 바디·스킨·후타·오버레이 중 한 기능으로 작성하는 것이 좋습니다.
여러 기능을 한 객체에 넣으면 내부에서 분리합니다. 활성 규칙은 정규화 후
최대 256개이며 초과분은 버려질 수 있습니다. 대량 팩은 이 한도 안에서
같은 후보를 공유할 수 있는 대상들을 조건으로 묶으세요.
id는 고유하게 유지하고 name은 설명으로만 쓰세요. 한 규칙에 여러
플러그인명을 배열로 넣거나 지원하지 않는 키를 만들어도 조건이 늘지 않습니다.
개별 후보 ID는 최대 1,024바이트, target은 최대 512바이트입니다.
모든 기능 후보가 비어 있는 규칙은 아무 효과가 없어 제거됩니다.
원본 NPC가 같은 개체들의 네이티브 바디스킨은 공유될 수 있습니다.

8. 모드팩에 넣기와 확인

제작자:
1) 설치된 XML/스킨/오버레이와 NPC 원본을 확인해 규칙을 만듭니다.
2) 기존 세이브와 JSON을 백업한 상태에서 최소 한 NPC로 확인하세요.
   이전 수동 확정이 있는 테스트 세이브에서는 그 선택이 우선합니다.
3) 필요한 에셋·플러그인과 JSON을 팩에 포함합니다. 저작권/재배포 권한도
   따로 확인하세요. 저자의 .ess/.skse를 이용자에게 요구할 필요는 없습니다.
4) 파일을 별도 MO2 설정 모드로 제공하면 BCNG 업데이트 시 보존하기 쉽습니다.
   이용자의 기존 JSON을 합칠 때는 rules 배열을 수동 병합하고 ID/우선순위/
   256개 한도를 검토하세요. 파일 두 개를 활성화해도 자동 병합하지 않습니다.
5) 편집 후에는 게임 완전 종료/재실행이 필요합니다. 목록 새로고침은 이
   배포 JSON을 다시 읽는 버튼이 아닙니다.

이용자:
같은 모드 구성에서 SKSE로 실행하고 플레이하면 됩니다. NPC가 아직 생성되지
않았거나 3D가 로드되지 않았으면 나중에 조건을 충족할 때 처리됩니다.
실제 적용된 결과를 다음에도 유지하려면 게임을 정상 저장하세요.
별도 인게임 배포 창을 열 필요는 없습니다.

잘 안 되면 순서대로 확인:
- 1.3.3 이상 DLL인가? rules가 비었거나 예제가 아직 주석 안에만 있는가?
- JSON 문법 오류: 끝 쉼표, 작은따옴표, 문자열 대신 잘못된 숫자/배열/불리언?
- npcPlugin이 원본이 아닌 리텍 ESP인가? RefID나 전체 로드 순서 ID를 썼는가?
- XML 상대 경로, 대소문자, Preset name, \u001f 구분자가 정확한가?
- 모드 매니저에서 다른 JSON/XML이 덮어쓰는가?
- 성별/설정의 배포 바디 타입/설치된 TRI/팩 레이아웃이 맞는가?
- 더 위의 전체 NPC 규칙이 먼저 매칭되는가?
- 이전 세이브 수동 선택 또는 removalMode가 있는가?
- 아직 NPC의 3D가 로드되지 않았는가?
필드 타입 하나가 잘못되면 파일 전체 로드가 실패할 수 있습니다. 일부 규칙만
정상이라고 가정하지 마세요. BodyChangeNG.log는 보통
Documents\My Games\Skyrim Special Edition\SKSE\BodyChangeNG.log에 있습니다.
이 안내는 현재 규칙 형식 설명이며 모든 팩의 인게임 검증을 대신하지 않습니다.

==================== ENGLISH ====================

1. Purpose and installation

Data\SKSE\Plugins\BodyChangeNGdistribution.json

This is a shared NPC-distribution configuration, not a particular save.
BCNG reads it at game startup and applies compatible rules as NPC 3D loads.
Recipients do not need to open BCNG or press Distribute. It does not target
the player. Required presets, bodies, textures and plugins must be installed.
Manual actor-dropdown choices are stored in the save's SKSE co-save instead;
those choices, including explicit Default, take priority for their feature.

Inside an MO2 mod, start at SKSE\Plugins; do not add another Data directory.
Check Overwrite/output mods for a winning copy. Multiple providers of this
same JSON are NOT merged. Close Skyrim, back up the winning JSON, then edit
with a plain-text editor and save as UTF-8.

BCNG 1.3.3+ accepts // line comments and /* ... */ block comments in this
distribution file. Do not give a commented file to a 1.3.2-or-older DLL.
Existing plain schema-7 JSON remains supported. Other JSON files, including
settings.json, have NOT gained comment support; keep those files comment-free.
Only the rules array is active. The default [] applies nothing.
In-game rule saving rewrites active rules and drops comments. Keep the original
template and this separate guide.

2. Quick start: one named preset for each follower

The complete JSON example in Korean section 2 is language-independent:
copy it, replace My Followers.esp and each local NPC base ID, then replace
each preset ID with your actual XML relative path and Preset name.
The example maps local 0x1234 (4660) to Preset A, and 0x1235 (4661) to Preset B.
These are fictitious examples, not installed NPCs or presets.
For race targeting with multiple candidates, see COMPLETE CONFIGURATION in
the JSON: a complete file assigning female Nords one of two bodies and one of
two skins independently. Replace its fictional asset IDs with installed ones.

The first example is already inside rules. Delete its two marker lines,
/* EXAMPLE: individual_body and END EXAMPLE */, to activate it.
For examples BELOW the root object, copy only { ... } into rules ABOVE.
Simply uncommenting a below-root example creates an invalid second root.
Separate rule objects with commas; do not put a comma after the last one.
Preserve existing active rules rather than overwriting the whole file.
All 19 examples cover scopes 0..10 and body/skin/futa/overlay channels.
Commented examples do nothing. Block comments cannot nest. When commenting
out a rule, also check the commas between the remaining active objects.

3. NPC IDs: use the originating plugin and local BASE ID, not RefID

Find the NPC_ base record in a plugin record editor such as xEdit.
Use the plugin that first defines it, not the last appearance-replacer
override. A Skyrim.esm NPC overridden by a replacer still uses Skyrim.esm.
An ACHR reference, spawned actor RefID or display name is not a local base ID.
All references sharing that base can match the rule.

Ordinary ESP/ESM: remove the leading two load-order hex digits (keep six).
Light/ESL-flagged plugin: keep the final three hex digits. A .esp can be light.
  05123456 -> local 0x123456 -> decimal JSON number 1193046
  FE012ABC -> local 0xABC -> decimal JSON number 2748
  local 0x1234 -> decimal JSON number 4660
Windows Calculator's Programmer mode can convert HEX to DEC.
Write an unquoted decimal integer; neither 0x1234 nor "0x1234" is valid here.
Include the plugin extension. Load-order changes do not change this identity;
compacting/renumbering the underlying records does.
Exact-name scope 2 is an alternative, but name collisions/translations/renames
make plugin + local base ID preferable for a distributed modpack.

4. Preset IDs without launching the game

Full example: Data\CalienteTools\BodySlide\SliderPresets\My Presets.xml
Open the XML and find <Preset name="Preset A" set="...">.
ID = path relative to SliderPresets + \u001f separator + decoded Preset name.
  "My Presets.xml\u001fPreset A"
  "My Pack/My Presets.xml\u001fPreset A"
Keep exact case, spaces and subfolders; use / for path separators.
Write \u001f ONCE in JSON. \\u001f produces the wrong literal string.
XML entities must be decoded: &amp; in XML becomes & in the name.
One XML may contain multiple Preset elements. Names ending in -Refit belong
to the separate outfit-correction catalog, not the normal body pool.

Optional read-only helper (replace paths with your physical MO2 paths):
& "D:\My MO2\mods\Body Change NG\Tools\BodyChangeNG-RuleId.ps1" -PresetXml "D:\My MO2\mods\My Presets\CalienteTools\BodySlide\SliderPresets\My Presets.xml"

Run in PowerShell, not through the game/MO2. Copy a quoted id from its JSON
output into presetIds. If there is no SliderPresets ancestor, specify
-PresetRoot as the physical directory corresponding to that game folder.
The installed relative path must remain the same. The helper changes no files
and does not validate sex/body compatibility merely by listing an ID.
If PC policy blocks scripts, use the manual method instead of weakening policy.
Build both body and outfits with matching Zeroed Sliders + Build Morphs.

5. Complete rule fields and target scopes

The shipped JSON documents 19 fields in its ALL FIELDS comments and all scopes
in SCOPE; find copyable objects under OTHER EXAMPLES. Omit irrelevant fields.
A typical rule needs id, female, scope,
its target identity and one feature's candidate pool.

Start with REAL CONDITION VALUES in the JSON if you do not know what to type.
It lists 39 verified base-game race/faction/keyword/class/combat-style records.
For a Nord target use scope=5, targetPlugin="Skyrim.esm", targetLocalFormID=79686,
not the translated name "Nord" in target. NordRaceVampire is a separate record
(558996) and needs its own rule. Skyrim.esm condition examples use real IDs;
My... NPC/asset names remain fictional values to replace.

For mod-added records, inspect installed plugins in xEdit (SSEEdit). Find the
NPC_'s winning override and its race/faction/keyword/class/combat-style reference,
then use the originating plugin/local ID of that RACE/FACT/KYWD/CLAS/CSTY record.
No game launch or plugin edits are required. Do not guess IDs from a mod title,
visible occupation or equipment. Combat styles match assigned CSTY records,
not everyone carrying a particular weapon.

0 = all NPCs; includeCustomFollowers/includeElderNPCs apply.
1 = NPC base; npcPlugin + npcLocalFormID.
2 = exact NPC base name in target (ASCII-case-insensitive, not substring).
3 = NPC base faction membership; targetPlugin + targetLocalFormID.
4 = NPC originating plugin filename in target. NOT all NPCs a replacer overrides.
5 = race; targetPlugin + targetLocalFormID.
6 = custom-follower classification; legacy supported scope.
7 = elder classification; legacy supported scope.
8 = NPC base keyword; targetPlugin + targetLocalFormID.
9 = NPC class; targetPlugin + targetLocalFormID.
10 = NPC combat style; targetPlugin + targetLocalFormID; legacy supported scope.

In 3/5/8/9/10 the local ID belongs to the CONDITION form, not the NPC.
An exact EditorID in target is also a legacy fallback, but stable plugin/local
IDs are preferable for packs. Display names are not EditorIDs. Wrong form
types do not match. Each rule means sex AND one scope AND compatibility;
there is no nested condition or multiple-scope AND syntax. Two rules match
their respective targets, not the intersection of the two scopes.
Custom followers have non-base-game/DLC origins and player-teammate or follower-
faction status. Elders are classified by elder in race EditorID or old/elder
in voice EditorID, not their rendered appearance.

female must be true or false, not "true" or "female".
For scope 0, false in includeCustomFollowers/includeElderNPCs means skip those
actors (both defaults are false). These flags do not veto explicit NPC/name/
faction/etc. targets. Currently normalization forces enabled=true:
enabled=false DOES NOT disable a rule. Remove the object from rules and keep
it in a separate file, or wrap the object in /* ... */ and recheck commas.
Removing a rule is not an undo of existing appearance; a save can retain
previous automatic assignments even when no current rule matches.

6. Skin, futa and overlay IDs

skinProfileIds example:
  Data\BodySkin\My Skin Pack\Textures\actors\character\female\femalehead.dds
  -> "auto:My Skin Pack:female"; male packs use "auto:My Skin Pack:male".
Recognized DDS/path structure is still required. Automatic skin pools accept
supported conventional humanoid skins, not UBE/Argonian/Khajiit skin entries.
Do not confuse manual selection eligibility with automatic distribution.

futanariSkinIds:
  Data\Futanari\My Futa Pack\Textures\[TRX] Futa addon\Regular\Default\schlong.dds
  -> "futanari:My Futa Pack:cbbe-trx"
  Data\Futanari\My Futa Pack\Textures\ERF_Futanari\FairSkinCBBE\futanari_schlong.dds
  -> "futanari:My Futa Pack:erf"
Use female=true. The NPC must already be SOS/TNG-registered with a matching
female addon; a rule does not perform that registration. UBE TRX automatic
futa-skin distribution is not supported.

overlayIds has four arrays in face/body/hands/feet order; keep unused ones [].
Use exact catalog IDs, not display names or bare DDS filenames. To calculate
an ID offline from the exact registered texture string:
& "D:\My MO2\mods\Body Change NG\Tools\BodyChangeNG-RuleId.ps1" -OverlayArea body -OverlayTexture "Actors\Character\Overlays\My Paint.dds"

Use the actual RaceMenu/SlaveTats registration texture string. Changing area
or a Textures\ prefix changes the ID. Calculating an ID does not register a
new paint or install its DDS. UBE overlays are not automatic candidates.
Foreign slots are respected; unavailable slots/sex/layout may prevent display.

overlayColors has four objects in the same area order. Each key must be an
exact candidate ID from that area's overlayIds. Values are decimal AARRGGBB:
  opaque white 0xFFFFFFFF = 4294967295 (default when omitted)
  roughly half-opaque red 0x80FF0000 = 2164195328
Colors for absent candidates are discarded. Automatic distribution picks
ONE candidate per area, not the whole pool as a simultaneous stack.
Player tint masks are not an NPC-distribution feature.

7. Body settings, priority and limits

MULTIPLE CANDIDATES in the JSON contains complete multi-preset/skin/futa examples.
Use arrays of quoted IDs, separated by commas: presetIds for body presets,
skinProfileIds for body skins and futanariSkinIds for futa skins. For example:
"skinProfileIds": ["auto:My Skin A:female", "auto:My Skin B:female"]
Use real pack folder names; bare DDS/XML paths are not candidate IDs. Each
feature chooses one compatible entry, not a simultaneous stack. To give body
AND skin, add separate rules; there is no body-A-to-skin-A pairing or round-robin
assignment across NPCs. Several NPCs may receive the same candidate. An earlier
fixed rule for the same feature can take priority over the new pool.

Data\SKSE\Plugins\BodyChangeNG\settings.json
Per-rule bodyFamily is obsolete and cleared. Distribution uses:
  femaleNpcBodyType: 0=CBBE 3BA (default), 1=BHUNP/UNP, 2=UBE, 3=Vanilla
  maleNpcBodyType: 0=HIMBO (default), 1=SAM, 2=Vanilla
The UBE enum exists, but current automatic candidate pools reject UBE presets/
skins; it is not a way to enable UBE distribution. Vanilla selects the
automatic body-preset removal policy, not a preset-assignment body type.
There is one distribution type per sex, not a separate type per rule.
Change only needed fields in existing settings.json, preserving other values.
No new settings file is needed for default CBBE 3BA/HIMBO. A minimal BHUNP/SAM
configuration, if no settings file exists, is:
{"femaleNpcBodyType": 1, "maleNpcBodyType": 1}
Check MO2's winning file. removalMode=true suspends automatic distribution/
corrections during mod-removal preparation.

Rules are evaluated top-to-bottom independently per feature and overlay area.
The first matching rule yielding a compatible candidate wins. Put specific
followers above all-NPC fallbacks. Body rules do not shadow skin rules.
One candidate means fixed assignment; multiple mean one compatible selection.
An existing automatic choice may be retained while still in the pool rather
than rerolled on every load. Manual/Default choices take precedence.
Multi-candidate results can differ in a setup with a different load order.
Plugin/local IDs still identify targets, but use one candidate for a fixed preset.
The nearby dropdown's 4,096-unit/32-NPC limits do not bound rule matching.

Prefer one feature (body/skin/futa/overlay) per object. Mixed-feature objects
are split internally. There is a maximum of 256 rules AFTER normalization;
excess rules can be dropped. Group targets that can share pools where possible.
Keep id unique; name is only a label. Plugin arrays and invented fields do not
add condition semantics. Candidate IDs have a 1,024-byte limit and target a
512-byte limit. Rules with no candidates have no effect and are removed.
References sharing an NPC base can share its native body-skin assignment.

8. Ship and troubleshoot

Authors: prepare the JSON using installed records/assets, keep backups and
check at least one NPC. Prior manual choices can override a test rule.
Ship the configuration and required assets/plugins with appropriate
redistribution permission; recipients do not need your .ess/.skse save.
A separate MO2 configuration mod helps preserve rules during BCNG updates.
Merge rules arrays deliberately (IDs, priority and 256-rule limit); simply
enabling two JSON providers does not merge them.
Fully restart Skyrim after external edits. Catalog Refresh does not reload
this distribution JSON. Recipients launch through SKSE and play; eligible
NPCs are processed when their 3D loads, including NPCs spawned later.
Normal game saving retains applied actor state. No BCNG window is required.

If nothing happens, check:
- Is the DLL 1.3.3+? Are rules empty or your examples still commented out?
- JSON syntax: double quotes, no trailing commas, correct number/bool/array types?
- Origin plugin/local BaseID rather than replacer ESP, RefID or full runtime ID?
- Exact XML relative path/name/case and a single \u001f escape?
- A different file winning in MO2?
- Sex, configured body type, matching TRI and asset layout?
- A higher-priority general rule winning first?
- Prior manual choice/Default or active removalMode?
- Has the actor's 3D actually loaded?
A wrong field type can fail the whole file load, not just that rule.
The log is commonly:
Documents\My Games\Skyrim Special Edition\SKSE\BodyChangeNG.log
This guide describes the current format; it does not replace testing your pack.
