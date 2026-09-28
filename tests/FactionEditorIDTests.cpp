#include "BodyChangeNG/FactionEditorIDs.h"
#include "BodyChangeNG/AssetIdentity.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <zlib.h>

namespace {
std::string Lower(std::string_view input) {
    std::string result(input);
    for (auto& c : result) c = static_cast<char>(bcn::asset_identity::FoldCase(static_cast<unsigned char>(c)));
    return result;
}
#include "FactionEditorUI.inl"
using namespace bcn::faction_editor_ids;
void Check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void Put(std::string& out, std::uint32_t value, unsigned n = 4)
{ for (unsigned i{}; i < n; ++i) out += static_cast<char>(value >> (i * 8)); }
std::string Sub(std::string tag, std::string data)
{ Put(tag, static_cast<std::uint32_t>(data.size()), 2); return tag + data; }
std::string Edid(std::string name) { name += '\0'; return Sub("EDID", name); }
std::string Record(std::string type, std::uint32_t id, std::string data, std::uint32_t flags = 0)
{
    Put(type, static_cast<std::uint32_t>(data.size())); Put(type, flags); Put(type, id);
    type.resize(24); return type + data;
}
std::string Group(std::string data, std::string type = "FACT")
{
    std::string out = "GRUP"; Put(out, static_cast<std::uint32_t>(data.size() + 24));
    out += type; out.resize(24); return out + data;
}
std::string Plugin(std::string data, unsigned masters = 0)
{
    std::string header;
    for (unsigned i{}; i < masters; ++i) header += Sub("MAST", std::string("Master.esm\0", 11));
    return Record("TES4", 0, header) + data;
}
std::string Compressed(std::string data)
{
    uLongf length = compressBound(static_cast<uLong>(data.size()));
    std::string buffer(length, '\0');
    Check(compress2(reinterpret_cast<Bytef*>(buffer.data()), &length,
        reinterpret_cast<const Bytef*>(data.data()), static_cast<uLong>(data.size()), 6) == Z_OK, "compression fixture");
    buffer.resize(length); std::string result; Put(result, static_cast<std::uint32_t>(data.size()));
    return result + buffer;
}
Result Parse(std::string data, std::unordered_set<std::uint32_t> ids = {0x100, 0x801},
    const std::function<bool()>& live = {})
{ std::istringstream stream(data, std::ios::binary); return Read(stream, ids, live); }
}
int main(int argc, char** argv)
try {
    auto result = Parse(Plugin(Group(Record("FACT", 0x100, Edid("BanditFaction")))));
    Check(result.valid && result.ids.at(0x100) == "BanditFaction", "plain EDID / low new-runtime form ID");
    result = Parse(Plugin(Group(Record("FACT", 0x100, Edid("WrongMaster")) +
        Record("FACT", 0x02000100, Edid("OwnedFaction")) +
        Record("FACT", 0x02000801, Compressed(Edid("LightOwned")), 0x40000)), 2));
    Check(result.valid && result.ids.size() == 2 && result.ids.at(0x100) == "OwnedFaction" &&
        result.ids.at(0x801) == "LightOwned", "master override collision / compressed / ESL local ID");
    std::string extendedSize; Put(extendedSize, 9);
    result = Parse(Plugin(Group(Record("FACT", 0x100, Sub("XXXX", extendedSize) +
        std::string("EDID\0\0", 6) + std::string("Extended\0", 9)))));
    Check(result.valid && result.ids.at(0x100) == "Extended", "XXXX extended subrecord");
    const auto good = Plugin(Group(Record("FACT", 0x100, Edid("Good"))));
    for (std::size_t length{}; length < good.size(); ++length) {
        const auto cut = Parse(good.substr(0, length));
        Check(cut.ids.empty(), "truncation returned unvalidated label");
    }
    for (const auto& bad : {std::string("junk"), Compressed(Edid("Bad")) + "trailer",
            std::string("\xff\xff\xff\x7f", 4) + "abc"}) {
        result = Parse(Plugin(Group(Record("FACT", 0x100, bad, 0x40000))));
        Check(result.valid && result.ids.empty(), "invalid compressed record accepted");
    }
    for (const auto& bad : {Sub("EDID", "NoNul"), Edid("bad\nlabel"),
            Sub("XXXX", "bad"), Sub("XXXX", extendedSize), Edid("Good") + "x"}) {
        Check(Parse(Plugin(Group(Record("FACT", 0x100, bad)))).ids.empty(), "malformed subrecord accepted");
    }
    result = Parse(Plugin(Group(Record("FACT", 0x100, Edid("First")) + Record("FACT", 0x100, Edid("Last")))));
    Check(result.ids.at(0x100) == "Last", "last own record");
    result = Parse(Plugin(Group(Record("FACT", 0x100, Edid("First")) + Record("FACT", 0x100, "", 0x20))));
    Check(result.ids.empty(), "deleted record retained");
    result = Parse(Plugin(Group(std::string(2 * 1024 * 1024, 'x'), "WRLD") + Group(Record("FACT", 0x100, Edid("Good")))));
    Check(result.valid && result.ids.at(0x100) == "Good" && result.bytesRead < 150, "unrelated large group not skipped");
    result = Parse(good, {0x100}, [] { return false; });
    Check(!result.valid && result.ids.empty(), "cancelled session published data");
    for (const auto name : {"../Skyrim.esm", "C:Skyrim.esm", "dir/Skyrim.esm", "x.txt"})
        Check(!ReadFile(".", name, {0x100}).valid, "unsafe plugin path accepted");
    for (int i{}; i < 2000; ++i) Check(Parse(good).ids.size() == 1, "repeat parser mismatch");
    // Execute the production label completion helper against real file I/O.
    const auto temp = std::filesystem::temp_directory_path() /
        ("BCNG-EditorID-Test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(temp);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{temp};
    { std::ofstream file(temp / "Test.EsP", std::ios::binary); file << Plugin(Group(
        Record("FACT", 0x100, Edid("RecoveredFaction")) + Record("FACT", 0x801, Edid("DontReplaceProvider")))); }
    DistributionTargetSnapshot snapshot;
    snapshot.factions = {
        {"Unnamed", "", "test.esp", 0x100, 0xFE123100, ""},
        {"ProviderLabel", "ProviderEDID", "Test.EsP", 0x801, 0xFE123801, ""},
        {"MissingLabel", "", "Missing.esp", 0x100, 0x12000100, ""},
        {"LocalizedName", "", "Test.EsP", 0x100, 0xFE123100, "LocalizedName"}
    };
    snapshot.races = {{"RaceKeep"}};
    CompleteFactionEditorIDs(snapshot, temp, [] { return false; });
    Check(snapshot.factions[0].editorID.empty(), "cancelled UI scan changed label");
    CompleteFactionEditorIDs(snapshot, temp, [] { return true; });
    Check(snapshot.factions.size() == 4 && snapshot.races[0].display == "RaceKeep", "scan changed target membership");
    for (const auto& option : snapshot.factions) {
        if (option.plugin == "Missing.esp") Check(option.display == "MissingLabel", "missing plugin lost target");
        else if (option.localFormID == 0x801) Check(option.editorID == "ProviderEDID" && option.display == "ProviderLabel", "runtime provider overwritten");
        else Check(option.editorID == "RecoveredFaction" && option.runtimeFormID == 0xFE123100 &&
            option.display.contains("RecoveredFaction") && option.display.contains("000100") &&
            (option.name.empty() || option.display.starts_with("LocalizedName")), "label or identity mismatch");
    }
    if (argc == 2) {
        const auto real = ReadFile(std::filesystem::path(argv[1]), "Skyrim.esm", {0x1BCC0, 0xDB1});
        Check(real.valid && real.ids.size() == 2, "real Skyrim FACT records missing");
        for (const auto& [id, name] : real.ids) std::cout << std::hex << id << " " << name << '\n';
        Check(real.ids.at(0x1BCC0) == "BanditFaction" && real.ids.at(0xDB1) == "PlayerFaction", "real EditorID mismatch");
        std::cout << std::dec << "Real plugin bytes read: " << real.bytesRead << '\n';
        // Independent header-only enumeration supplies the same query set that
        // the game's loaded faction array supplies (benchmark Skyrim.esm).
        std::ifstream file(std::filesystem::path(argv[1]) / "Skyrim.esm", std::ios::binary);
        const auto read32 = [](const std::array<unsigned char, 24>& h, int p) {
            return std::uint32_t(h[p]) | std::uint32_t(h[p+1]) << 8 |
                std::uint32_t(h[p+2]) << 16 | std::uint32_t(h[p+3]) << 24;
        };
        std::unordered_set<std::uint32_t> all;
        std::array<unsigned char, 24> header{};
        while (file.read(reinterpret_cast<char*>(header.data()), 24)) {
            const auto size = read32(header, 4);
            if (std::string_view(reinterpret_cast<char*>(header.data()), 4) == "GRUP") {
                if (std::string_view(reinterpret_cast<char*>(header.data()+8), 4) == "FACT") {
                    const auto end = static_cast<std::streamoff>(file.tellg()) + size - 24;
                    while (static_cast<std::streamoff>(file.tellg()) < end && file.read(reinterpret_cast<char*>(header.data()), 24)) {
                        all.insert(read32(header, 12));
                        file.seekg(read32(header, 4), std::ios::cur);
                    }
                    break;
                }
                file.seekg(size - 24, std::ios::cur);
            } else file.seekg(size, std::ios::cur);
        }
        const auto start = std::chrono::steady_clock::now();
        const auto full = ReadFile(std::filesystem::path(argv[1]), "Skyrim.esm", all);
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        Check(full.valid && !full.ids.empty() && full.ids.size() == all.size(), "all real factions failed");
        std::cout << "All " << full.ids.size() << " factions: " << full.bytesRead << " bytes read, " << elapsed << " ms (offline filesystem)\n";
    }
    std::cout << "FactionEditorIDTests passed (plain/compressed/XXXX/masters/ESL/corrupt/cancel/2000 repeats)\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
