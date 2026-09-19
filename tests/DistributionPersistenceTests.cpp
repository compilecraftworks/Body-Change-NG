#include "BodyChangeNG/DistributionOverlayColors.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

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
        return nlohmann::json::parse(file);
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
    Sandbox sandbox;
    const auto path = sandbox.root / "Data/SKSE/Plugins/BodyChangeNGdistribution.json";
    Require(WriteDistributionFile(path, {}), "empty rules did not save");
    Require(Read(path)["schemaVersion"] == 7 && Read(path)["rules"].empty(), "empty rule set changed");
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
    for (std::size_t i{}; i < rules.size(); ++i) {
        const auto& source = rules[i]; const auto& value = stored["rules"][i];
        Require(value["id"] == source.id && value["name"] == source.name &&
            value["nameKey"] == source.nameKey && value["enabled"] == source.enabled &&
            value["female"] == source.female && value["scope"] == static_cast<unsigned>(source.scope) &&
            value["includeCustomFollowers"] == source.includeCustomFollowers &&
            value["includeElderNPCs"] == source.includeElderNPCs, "rule flags or identity changed");
        Require(value["npcPlugin"] == source.npcPlugin && value["npcLocalFormID"] == source.npcLocalFormID &&
            value["targetPlugin"] == source.targetPlugin && value["targetLocalFormID"] == source.targetLocalFormID &&
            value["target"] == source.target && !value.contains("npcBaseFormID") && !value.contains("targetFormID"),
            "stable plugin/local IDs replaced with load-order-dependent IDs");
        Require(value["presetIds"] == source.presetIds && value["skinProfileIds"] == source.skinProfileIds &&
            value["futanariSkinIds"] == source.futanariSkinIds && value["overlayIds"] == source.overlayIds &&
            bcn::ReadDistributionOverlayColors(value["overlayColors"], source.overlayIds) == source.overlayColors,
            "feature pools or per-candidate RGBA lost");
    }
    {
        // Deny replacement of the live file, simulating a sharing violation.
        HeldFile held{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr)};
        Require(held.handle != INVALID_HANDLE_VALUE, "could not lock test file");
        Require(!WriteDistributionFile(path, {}), "locked destination reported success");
        Require(Read(path) == stored, "failed replacement discarded saved rules");
    }
    auto temporary = path; temporary += ".new";
    Require(!std::filesystem::exists(temporary), "replace failure left a temporary file");
    std::filesystem::create_directory(temporary); // Deny opening the staging file.
    Require(!WriteDistributionFile(path, {}) && Read(path) == stored, "staging failure discarded saved rules");
    std::filesystem::remove(temporary);
    Require(WriteDistributionFile(path, {}) && Read(path)["rules"].empty(), "retry after failure did not work");
    std::cout << "Distribution persistence passed: " << rules.size()
        << " scope/sex/feature rows, stable IDs, RGBA, real atomic replacement/failure/retry (no engine load)\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
