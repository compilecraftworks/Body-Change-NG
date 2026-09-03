#include "BodyChangeNG/RaceMenuPresetMigration.h"

#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/RaceMenuPresetMigrationRules.h"

#include <SKSE/Logger.h>

#include <algorithm>
#include <cwctype>
#include <ranges>

namespace
{
    constexpr std::uint32_t kHighPolyFemaleNordHead = 0x000A06U;
    constexpr std::uint32_t kVanillaFemaleNordHead = 0x051623U;

    [[nodiscard]] bool IsJslot(const std::filesystem::path& path)
    {
        auto extension = path.extension().wstring();
        std::ranges::transform(extension, extension.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return extension == L".jslot";
    }

    [[nodiscard]] std::filesystem::path BackupPathFor(const std::filesystem::path& path)
    {
        auto candidate = path;
        candidate += L".body-change-ng.bak";
        std::error_code error;
        if (!std::filesystem::exists(candidate, error)) return candidate;
        for (std::uint32_t suffix = 1U; suffix < 1000U; ++suffix) {
            candidate = path;
            candidate += std::format(L".body-change-ng.bak.{}", suffix);
            error.clear();
            if (!std::filesystem::exists(candidate, error)) return candidate;
        }
        return {};
    }

    [[nodiscard]] std::optional<bcn::racemenu_preset_migration::HeadPartTarget> ResolveTarget()
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return std::nullopt;

        if (const auto* head = data->LookupForm<RE::BGSHeadPart>(
                kHighPolyFemaleNordHead, "High Poly Head.esm")) {
            return bcn::racemenu_preset_migration::HeadPartTarget{
                .plugin = "High Poly Head.esm",
                .formIdentifier = "High Poly Head.esm|000A06",
                .runtimeFormID = head->GetFormID()
            };
        }
        if (const auto* head = data->LookupForm<RE::BGSHeadPart>(kVanillaFemaleNordHead, "Skyrim.esm")) {
            return bcn::racemenu_preset_migration::HeadPartTarget{
                .plugin = "Skyrim.esm",
                .formIdentifier = "Skyrim.esm|051623",
                .runtimeFormID = head->GetFormID()
            };
        }
        return std::nullopt;
    }

    [[nodiscard]] bool SaveAtomically(const std::filesystem::path& path, const nlohmann::json& preset)
    {
        const auto backup = BackupPathFor(path);
        if (backup.empty()) return false;
        auto temporary = path;
        temporary += L".body-change-ng.tmp";

        std::error_code error;
        if (!std::filesystem::copy_file(path, backup, std::filesystem::copy_options::none, error) || error) {
            SKSE::log::warn("Body Change NG could not back up RaceMenu preset '{}' ({})",
                bcn::path_text::Utf8(path), error.message());
            return false;
        }

        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output << preset.dump(2) << '\n';
            output.flush();
            if (!output) return false;
        }

        try {
            std::ifstream verification(temporary, std::ios::binary);
            [[maybe_unused]] const auto parsed = nlohmann::json::parse(verification);
        } catch (...) {
            std::filesystem::remove(temporary, error);
            return false;
        }

        if (!MoveFileExW(temporary.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            SKSE::log::warn("Body Change NG could not replace migrated RaceMenu preset '{}' (Win32 {})",
                bcn::path_text::Utf8(path), GetLastError());
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }
}

namespace bcn::racemenu_preset_migration
{
    void MigrateVisiblePresets()
    {
        const auto target = ResolveTarget();
        if (!target) {
            SKSE::log::warn("Body Change NG could not resolve a safe RaceMenu head target; legacy presets were not changed");
            return;
        }

        if (auto* data = RE::TESDataHandler::GetSingleton(); data && data->LookupModByName("BodyChange.esp")) {
            SKSE::log::info("Body Change NG left RaceMenu presets unchanged because legacy BodyChange.esp is loaded");
            return;
        }

        const std::filesystem::path root{ L"Data\\SKSE\\Plugins\\CharGen\\Presets" };
        std::error_code error;
        if (!std::filesystem::is_directory(root, error) || error) return;

        std::size_t migrated{};
        std::size_t skippedUBE{};
        std::size_t failed{};
        for (std::filesystem::recursive_directory_iterator it(root,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            if (!it->is_regular_file(error) || error || !IsJslot(it->path())) {
                error.clear();
                continue;
            }

            try {
                std::ifstream input(it->path(), std::ios::binary);
                auto preset = nlohmann::json::parse(input);
                const auto result = TransformLegacyBodyChangeHeadParts(preset, *target);
                if (result.skippedUBE) {
                    ++skippedUBE;
                    SKSE::log::info("Body Change NG preserved UBE RaceMenu preset '{}' with a legacy head reference",
                        path_text::Utf8(it->path()));
                } else if (result.Changed()) {
                    if (SaveAtomically(it->path(), preset)) {
                        ++migrated;
                        SKSE::log::info("Body Change NG migrated legacy RaceMenu head in '{}' to {}",
                            path_text::Utf8(it->path()), target->formIdentifier);
                    } else {
                        ++failed;
                    }
                }
            } catch (const std::exception& exception) {
                ++failed;
                SKSE::log::warn("Body Change NG could not inspect RaceMenu preset '{}' ({})",
                    path_text::Utf8(it->path()), exception.what());
            }
        }
        if (migrated != 0U || skippedUBE != 0U || failed != 0U) {
            SKSE::log::info("Body Change NG RaceMenu preset migration: migrated={} preserved-ube={} failed={}",
                migrated, skippedUBE, failed);
        }
    }
}
