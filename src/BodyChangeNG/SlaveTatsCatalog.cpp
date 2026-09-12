#include "BodyChangeNG/SlaveTatsCatalog.h"

#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/OverlayPolicy.h"
#include "BodyChangeNG/PathText.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>

namespace
{
    constexpr std::size_t kMaxJsonBytes = 8U * 1024U * 1024U;
    constexpr std::size_t kMaxEntries = 32768U;

    [[nodiscard]] std::string Lower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    [[nodiscard]] std::optional<bcn::overlay::Area> ParseArea(const std::string_view value)
    {
        const auto lower = Lower(std::string{ value });
        if (lower == "face") return bcn::overlay::Area::face;
        if (lower == "body") return bcn::overlay::Area::body;
        if (lower == "hands" || lower == "hand") return bcn::overlay::Area::hands;
        if (lower == "feet" || lower == "foot") return bcn::overlay::Area::feet;
        return std::nullopt;
    }

    [[nodiscard]] bool SafeRelativeTexture(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_path()) return false;
        for (const auto& component : path) {
            if (component == "..") return false;
        }
        return Lower(bcn::path_text::Utf8(path.extension())) == ".dds";
    }
}

namespace bcn::overlay
{
    std::vector<Entry> ScanSlaveTatsDirectory(const std::filesystem::path& root)
    {
        std::vector<Entry> result;
        std::error_code error;
        if (!std::filesystem::is_directory(root, error) || error) return result;
        for (std::filesystem::recursive_directory_iterator it(root,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end && result.size() < kMaxEntries; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code statusError;
            if (!it->is_regular_file(statusError) || statusError ||
                Lower(path_text::Utf8(it->path().extension())) != ".json") continue;
            const auto bytes = std::filesystem::file_size(it->path(), statusError);
            if (statusError || bytes == 0U || bytes > kMaxJsonBytes) continue;
            try {
                std::ifstream stream(it->path(), std::ios::binary);
                const auto json = nlohmann::json::parse(stream);
                if (!json.is_array()) continue;
                for (const auto& row : json) {
                    if (!row.is_object() || result.size() >= kMaxEntries) continue;
                    const auto name = row.value("name", std::string{});
                    const auto section = row.value("section", std::string{});
                    const auto relativeText = row.value("texture", std::string{});
                    const auto area = ParseArea(row.value("area", std::string{}));
                    const auto relative = path_text::FromUtf8(relativeText);
                    if (!area || name.empty() || name.size() > 512U ||
                        section.size() > 512U || relativeText.size() > 1024U ||
                        !SafeRelativeTexture(relative)) continue;

                    auto texture = std::filesystem::path{ "textures" } / "actors" /
                        "character" / "slavetats" / relative;
                    auto texturePath = path_text::GenericUtf8(texture);
                    std::ranges::replace(texturePath, '/', '\\');
                    const auto evidence = section + ' ' + name;
                    // A virtual Data path often omits the mod name that tells
                    // UBE replacements apart from their Legacy counterpart.
                    // Resolve the winning loose-file provider when MO2 exposes
                    // one; a BSA-only or ordinary filesystem source safely
                    // falls back to the JSON path itself.
                    const auto providerPath = catalog_roots::ResolveProviderPath(it->path());
                    const auto provider = path_text::GenericUtf8(
                        providerPath.value_or(it->path()));
                    result.push_back({
                        .id = StableId(*area, texturePath),
                        .name = section.empty() ? "SlaveTats · " + name :
                            "SlaveTats · " + section + " · " + name,
                        .texturePath = std::move(texturePath),
                        .area = *area,
                        .layout = ClassifyLayout(evidence, relativeText, provider),
                        .sex = ClassifySex(evidence, relativeText, provider),
                        .source = Source::slaveTats
                    });
                }
            } catch (...) {
                // One malformed third-party pack must not suppress the other
                // installed SlaveTats collections or the RaceMenu catalog.
            }
        }
        std::ranges::sort(result, {}, [](const Entry& entry) {
            return std::tuple{ entry.area, entry.name, entry.id };
        });
        return result;
    }
}
