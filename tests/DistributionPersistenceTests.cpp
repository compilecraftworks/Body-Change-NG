#include "BodyChangeNG/DistributionOverlayColors.h"
#include "BodyChangeNG/DistributionJson.h"
#include "BodyChangeNG/DistributionAuthoring.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <tuple>

namespace SKSE::log {
    // Only the diagnostic sink is stubbed; the writer and atomic replacement
    // below are extracted verbatim from production, and use real temp files.
    template<class... Args> void error(const char*, Args&&...) {}
}
namespace {
#include "DistributionWriter.inl"
    void Require(bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    }
    nlohmann::json Read(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return bcn::distribution_json::Parse(file);
    }
    std::string ReadText(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return {(std::istreambuf_iterator<char>(file)), {}};
    }
    void WriteText(const std::filesystem::path& path, std::string_view text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        file.flush();
        Require(file.good(), "could not create annotated save fixture");
    }
    struct Sandbox {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
            ("BCNG-distribution-writer-" + std::to_string(GetCurrentProcessId()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Sandbox() { Require(std::filesystem::create_directory(root), "sandbox already exists"); }
        ~Sandbox() { std::error_code error; std::filesystem::remove_all(root, error); }
    };
    struct HeldFile {
        HANDLE handle;
        ~HeldFile() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };
}
int main() try {
    const auto parse = [](std::string_view text) {
        std::istringstream stream{std::string(text)};
        return bcn::distribution_json::Parse(stream);
    };
    const auto plain = parse(R"({"schemaVersion":7,"rules":[]})");
    for (const auto text : {
        R"(// before
            {"schemaVersion":7, /* between */ "rules":[]} // after)",
        R"(/* before */ {"schemaVersion":7,"rules":[/* disabled example {"scope":0} */]})",
        R"({"schemaVersion":7,"rules":[]}// end without newline)",
        "\xEF\xBB\xBF// UTF-8 BOM\r\n{\"schemaVersion\":7,\"rules\":[]}"}) {
        Require(parse(text) == plain, "comment/BOM parsing changed active data");
        const auto comments = bcn::distribution_json::Comments(text);
        const auto saved = plain.dump(2) + "\n\n" + comments;
        Require(parse(saved) == plain && bcn::distribution_json::Comments(saved) == comments,
            "moving comments activated data or duplicated the guide");
    }
    const auto strings = parse(R"({"value":"url://host/*literal*/","escaped":"quote\"//still text","preset":"file.xml\u001fName"})");
    Require(strings["value"] == "url://host/*literal*/" && strings["escaped"] == "quote\"//still text" &&
        strings["preset"] == std::string("file.xml") + '\x1F' + "Name", "string content damaged by comment handling");
    Require(bcn::distribution_json::Comments(strings.dump()).empty(),
        "comment markers inside strings became real comments");
    const auto mixed = R"(/* header */ {"odd":"escaped\"//quoted","even":"slash\\", // inline
        "rule":"not a /* comment */"} /* {"disabled":true,"text":"// untouched"} */ // final)";
    const auto mixedComments = bcn::distribution_json::Comments(mixed);
    Require(mixedComments == "/* header */\n// inline\n/* {\"disabled\":true,\"text\":\"// untouched\"} */\n// final\n",
        "comment lexer mishandled escapes or disabled example text");
    Require(parse(parse(mixed).dump() + '\n' + mixedComments) == parse(mixed),
        "comment relocation changed escaped string values");
    Require(bcn::distribution_json::Comments("// one\r// two\r\n/**//* three */") ==
        "// one\n// two\n/**/\n/* three */\n", "CR/LF or adjacent comments were lost");
    for (const auto text : {
        R"({"schemaVersion":7,"rules":[],})", R"({"rules":[{},]})",
        R"({"rules":[]} /* unfinished)", R"({"rules":[]} // comment
        {})", R"({unquoted:1})", R"({"id":0x1234})", R"({"rules":[]} / broken)"}) {
        bool failed{};
        try { (void)parse(text); } catch (const nlohmann::json::parse_error&) { failed = true; }
        Require(failed, "malformed JSON became accepted");
    }
    const auto starter = Read("package/SKSE/Plugins/BodyChangeNGdistribution.json");
    Require(starter.at("schemaVersion") == 8 && starter.at("rules").is_array() &&
        starter.at("rules").empty() && starter.size() == 2,
        "commented starter must contain only schema and empty active rules");
    std::ifstream templateFile("package/SKSE/Plugins/BodyChangeNGdistribution.json", std::ios::binary);
    const std::string templateText((std::istreambuf_iterator<char>(templateFile)), {});
    std::array<bool, 11> scopes{};
    auto examples = nlohmann::json::array();
    std::size_t position{}, exampleCount{};
    while ((position = templateText.find("/* EXAMPLE: ", position)) != std::string::npos) {
        // Marker must start its own line, not the header's explanatory sentence.
        if (position != 0 && templateText[position - 1] != '\n') { ++position; continue; }
        const auto begin = templateText.find('\n', position);
        const auto end = templateText.find("END EXAMPLE */", begin);
        Require(begin != std::string::npos && end != std::string::npos, "unfinished example block");
        const auto value = parse(std::string_view(templateText).substr(begin, end - begin));
        const auto decoded = bcn::distribution_authoring::DecodeRule(value, 8);
        Require(value.at("id").is_string() && value.at("sex").is_string() &&
            value.at("scope").is_string(), "sample rule field types are wrong");
        const auto scope = decoded.at("scope").get<unsigned>();
        Require(scope < scopes.size(), "sample scope out of range");
        scopes[scope] = true;
        if (exampleCount == 0) {
            Require(value.at("target") == "Follower A" && value.at("presets").at(0) == "Preset A",
                "sample display names are wrong");
            auto activated = starter;
            activated["rules"].push_back(value);
            const auto activeRoot = parse(activated.dump(2) + '\n' +
                bcn::distribution_json::Comments(templateText));
            Require(activeRoot.at("rules") == nlohmann::json::array({value}),
                "copying an example did not activate exactly one rule");
            (void)bcn::distribution_authoring::Decode(activeRoot);
        }
        if (decoded.contains("overlayIds")) {
            const auto pools = decoded.at("overlayIds").get<std::array<std::vector<std::string>, 4>>();
            const auto colors = bcn::ReadDistributionOverlayColors(decoded.at("overlayColors"), pools);
            Require(colors[1].at(pools[1][0]) == 0x80FF0000U, "sample overlay alpha/color is wrong");
        }
        examples.push_back(value);
        ++exampleCount;
        position = end + std::string_view("END EXAMPLE */").size();
    }
    Require(exampleCount == 19 && std::ranges::all_of(scopes, [](bool covered) { return covered; }),
        "template must demonstrate all eleven scopes");
    const auto findExample = [&](std::string_view id) -> const nlohmann::json& {
        const auto found = std::ranges::find_if(examples, [&](const nlohmann::json& row) {
            return row.at("id").get_ref<const std::string&>() == id;
        });
        Require(found != examples.end(), "required example is missing");
        return *found;
    };
    for (const auto& [id, scope, target] : std::array{
        std::tuple{"faction-body", "faction", "BanditFaction"}, std::tuple{"race-body", "race", "NordRace"},
        std::tuple{"keyword-body", "keyword", "ActorTypeNPC"}, std::tuple{"class-body", "class", "CombatWarrior1H"},
        std::tuple{"combat_style_body", "combatStyle", "csHumanMeleeLvl1"}}) {
        const auto& row = findExample(id);
        Require(row.at("scope") == scope && row.at("target") == target, "named target example is wrong");
    }
    Require(findExample("follower-a-body-pool").at("presets").size() == 3 &&
        findExample("follower-a-skin-pool").at("skins").size() == 2 &&
        findExample("follower-a-futa-pool").at("futaSkins").size() == 3,
        "multiple-candidate examples must keep all candidates");
    const auto& overlayExample = findExample("follower-a-overlay").at("overlays");
    Require(overlayExample.at("body").size() == 2 &&
        overlayExample.at("body").at(1).at("color") == "#FFFFFFFF",
        "overlay candidates/colors lost");
    auto combined = starter;
    combined["rules"] = examples;
    Require(parse(combined.dump(2)) == combined, "combined example rules did not round-trip");
    const auto configMarker = templateText.find("/* COMPLETE CONFIG: nord_body_and_skin_pools");
    Require(configMarker != std::string::npos, "complete configuration example missing");
    const auto configBegin = templateText.find('\n', configMarker);
    const auto configEnd = templateText.find("END CONFIG */", configBegin);
    Require(configEnd != std::string::npos, "complete configuration example is unfinished");
    const auto complete = parse(std::string_view(templateText).substr(configBegin, configEnd - configBegin));
    Require(complete.at("schemaVersion") == 8 && complete.at("rules").size() == 2,
        "complete configuration must contain two independent rules");
    for (const auto& row : complete.at("rules")) {
        Require(row.at("sex") == "female" && row.at("scope") == "race" && row.at("target") == "NordRace",
            "complete configuration must target female Nords");
    }
    Require(complete.at("rules").at(0).at("presets").size() == 2 &&
        complete.at("rules").at(1).at("skins").size() == 2,
        "complete body/skin pools are incomplete");
    Sandbox sandbox;
    const auto path = sandbox.root / "Data/SKSE/Plugins/BodyChangeNGdistribution.json";
    Require(WriteDistributionFile(path, {}), "empty rules did not save");
    Require(Read(path)["schemaVersion"] == 8 && Read(path)["rules"].empty(), "empty rule set changed");
    Require(Read(path).size() == 2, "writer unexpectedly changed the schema");
    std::vector<bcn::DistributionRule> rules;
    for (unsigned scope{}; scope <= static_cast<unsigned>(bcn::DistributionScope::combatStyle); ++scope) {
        for (bool female : {false, true}) for (unsigned feature{}; feature < 4; ++feature) {
            bcn::DistributionRule rule;
            rule.id = "rule-" + std::to_string(rules.size());
            rule.name = "조건 한국어 中文 " + rule.id;
            rule.nameKey = "test-name-key";
            rule.scope = static_cast<bcn::DistributionScope>(scope);
            rule.female = female;
            rule.enabled = (scope % 2) != 0;
            rule.includeCustomFollowers = (feature % 2) != 0;
            rule.includeElderNPCs = (feature % 3) != 0;
            rule.npcPlugin = "NPC.esp"; rule.npcLocalFormID = 0x123;
            rule.targetPlugin = "Target.esl"; rule.targetLocalFormID = 0x456;
            rule.target = "Target name";
            rule.npcBaseFormID = 0xFE123456; rule.targetFormID = 0x12000345;
            if (feature == 0) rule.presetIds = {"body:A", "body:B"};
            if (feature == 1) rule.skinProfileIds = {"auto:skin:female"};
            if (feature == 2) rule.futanariSkinIds = {"futa:ERF", "futa:TRX", "futa:UBE-TRX"};
            if (feature == 3) for (unsigned area{}; area < 4; ++area) {
                rule.overlayIds[area] = {"overlay:" + std::to_string(area)};
                rule.overlayColors[area][rule.overlayIds[area][0]] =
                    std::array<std::uint32_t, 4>{0U, 0xFFFFFFFFU, 0x7FABCDEFU, 0xFF123456U}[area];
            }
            rules.push_back(std::move(rule));
        }
    }
    Require(WriteDistributionFile(path, rules), "rule matrix write failed");
    const auto stored = Read(path);
    Require(stored["rules"].size() == rules.size(), "lost a rule");
    const auto decodedStored = bcn::distribution_authoring::Decode(stored);
    for (const auto& row : stored.at("rules")) for (const auto& [key, value] : row.items()) {
        (void)value;
        Require(templateText.contains("// " + key + "\n") ||
            templateText.contains("// " + key + "\r\n"), "persisted field lacks documentation");
    }
    std::vector<std::string> exampleIds;
    for (const auto& example : examples) {
        const auto id = example.at("id").get<std::string>();
        Require(!id.starts_with("obody-import-") &&
            std::ranges::find(exampleIds, id) == exampleIds.end(), "reserved/duplicated example ID");
        exampleIds.push_back(id);
    }
    for (std::size_t i{}; i < rules.size(); ++i) {
        const auto& source = rules[i]; const auto& value = decodedStored["rules"][i];
        Require(value["id"] == source.id && value["name"] == source.name &&
            value["nameKey"] == source.nameKey && value["enabled"] == source.enabled &&
            value["female"] == source.female && value["scope"] == static_cast<unsigned>(source.scope) &&
            value["includeCustomFollowers"] == source.includeCustomFollowers &&
            value["includeElderNPCs"] == source.includeElderNPCs, "rule flags or identity changed");
        const auto scope = static_cast<unsigned>(source.scope);
        if (scope == 1) Require(value["npcPlugin"] == source.npcPlugin &&
            value["npcLocalFormID"] == source.npcLocalFormID, "NPC stable ID changed");
        else if (scope == 2 || scope == 4) Require(value["target"] == source.target, "name/plugin changed");
        else if (scope != 0 && scope != 6 && scope != 7) Require(value["targetPlugin"] == source.targetPlugin &&
            value["targetLocalFormID"] == source.targetLocalFormID, "form stable ID changed");
        Require(!value.contains("npcBaseFormID") && !value.contains("targetFormID"),
            "runtime IDs written to disk");
        const auto emptyPool = std::vector<std::string>{};
        const auto pools = value.value("overlayIds", decltype(bcn::DistributionRule::overlayIds){});
        Require(value.value("presetIds", emptyPool) == source.presetIds &&
            value.value("skinProfileIds", emptyPool) == source.skinProfileIds &&
            value.value("futanariSkinIds", emptyPool) == source.futanariSkinIds && pools == source.overlayIds &&
            bcn::ReadDistributionOverlayColors(value.value("overlayColors", nlohmann::json::array()), pools) == source.overlayColors,
            "feature pools or per-candidate RGBA lost");
    }
    WriteText(path, "// original schema-seven guide\n{\"schemaVersion\":7,\"rules\":[]}\n");
    const auto original = ReadText(path);
    Require(bcn::distribution_json::BackupForMigration(path, 7), "migration backup failed");
    Require(ReadText(path.string() + ".schema7.bak") == original, "backup was not byte-exact");
    WriteText(path, original + "// second version\n");
    {
        HeldFile held{CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr)};
        Require(held.handle != INVALID_HANDLE_VALUE, "could not lock migration source");
        Require(!bcn::distribution_json::BackupForMigration(path, 7), "unreadable migration source was backed up");
    }
    Require(bcn::distribution_json::BackupForMigration(path, 7), "numbered backup failed");
    Require(ReadText(path.string() + ".schema7.bak") == original &&
        ReadText(path.string() + ".schema7.1.bak") == original + "// second version\n",
        "backup overwrote an older original");
    Require(WriteDistributionFile(path, rules) && Read(path) == stored, "migration save failed");
    const auto annotated = std::string("// hand-authored rules / 사용자 주석\n") +
        stored.dump(2) + "\n/* keep this on failed save */\n";
    WriteText(path, annotated);
    Require(Read(path) == stored, "annotating saved rules changed their data");
    {
        // Deny replacement of the live file, simulating a sharing violation.
        HeldFile held{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr)};
        Require(held.handle != INVALID_HANDLE_VALUE, "could not lock test file");
        Require(!WriteDistributionFile(path, {}), "locked destination reported success");
        Require(Read(path) == stored, "failed replacement discarded saved rules");
        Require(ReadText(path) == annotated, "failed save changed the original comment bytes");
    }
    auto temporary = path; temporary += ".new";
    Require(!std::filesystem::exists(temporary), "replace failure left a temporary file");
    std::filesystem::create_directory(temporary); // Deny opening the staging file.
    Require(!WriteDistributionFile(path, {}) && Read(path) == stored, "staging failure discarded saved rules");
    Require(ReadText(path) == annotated, "staging failure changed the commented file");
    std::filesystem::remove(temporary);
    Require(WriteDistributionFile(path, rules) && Read(path) == stored,
        "re-save of commented rules changed active data");
    Require(ReadText(path) == stored.dump(2) + "\n\n" + bcn::distribution_json::Comments(annotated),
        "successful save did not place active rules above preserved comments");
    Require(WriteDistributionFile(path, {}) && Read(path)["rules"].empty(), "retry after failure did not work");
    Require(bcn::distribution_json::Comments(ReadText(path)) == bcn::distribution_json::Comments(annotated),
        "deleting every active rule lost the comments");
    WriteText(path, templateText);
    const auto templateComments = bcn::distribution_json::Comments(templateText);
    Require(!templateComments.empty() && templateText.starts_with("{"), "starter must place active JSON first");
    Require(WriteDistributionFile(path, rules) && Read(path) == stored,
        "first in-game save of the full guide changed active rules");
    const auto fullSaved = ReadText(path);
    Require(fullSaved == stored.dump(2) + "\n\n" + templateComments,
        "full guide or disabled examples lost during save");
    for (unsigned repeat{}; repeat < 25; ++repeat) {
        Require(WriteDistributionFile(path, rules) && ReadText(path) == fullSaved,
            "repeated save grew, reordered or damaged the guide");
    }
    WriteText(path, fullSaved + "// user-added note / 사용자 메모\n");
    Require(WriteDistributionFile(path, rules) &&
        ReadText(path) == fullSaved + "// user-added note / 사용자 메모\n",
        "save failed to retain a newly edited note");
    const auto unreadableOriginal = ReadText(path);
    {
        HeldFile held{CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr)};
        Require(held.handle != INVALID_HANDLE_VALUE, "could not deny reading the source");
        Require(!WriteDistributionFile(path, {}), "unreadable comments were silently discarded");
    }
    Require(ReadText(path) == unreadableOriginal && !std::filesystem::exists(temporary),
        "source-read failure changed the original or created staging data");
    const auto brokenComment = fullSaved + "/* incomplete user edit";
    WriteText(path, brokenComment);
    Require(!WriteDistributionFile(path, {}) && ReadText(path) == brokenComment &&
        !std::filesystem::exists(temporary), "unfinished comment was lost or replaced");
    WriteText(path, stored.dump(2));
    Require(WriteDistributionFile(path, rules) && ReadText(path) == stored.dump(2) + '\n',
        "plain file compatibility changed or a guide was unexpectedly injected");
    std::cout << "Distribution persistence passed: " << rules.size()
        << " scope/sex/feature rows, stable IDs, RGBA, preserved guide/examples, 25 stable re-saves, "
           "atomic replacement/failure/retry (no engine load)\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
