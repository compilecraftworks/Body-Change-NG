#include "BodyChangeNG/FactionEditorIDs.h"
#include "BodyChangeNG/AssetIdentity.h"
#include "BodyChangeNG/PathText.h"

#include <array>
#include <algorithm>
#include <fstream>
#include <optional>
#include <span>
#include <vector>
#include <zlib.h>

namespace bcn::faction_editor_ids
{
    namespace
    {
        using Bytes = std::span<const std::uint8_t>;
        constexpr std::uint32_t maxRecordBytes = 16U * 1024U * 1024U;
        [[nodiscard]] std::uint32_t U32(Bytes data, std::size_t at)
        {
            return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8 |
                std::uint32_t(data[at + 2]) << 16 | std::uint32_t(data[at + 3]) << 24;
        }
        [[nodiscard]] bool Tag(Bytes data, std::size_t at, std::string_view tag)
        {
            return data.size() >= at + 4 &&
                std::string_view(reinterpret_cast<const char*>(data.data() + at), 4) == tag;
        }
        template<class Consume>
        bool Subrecords(Bytes data, Consume consume)
        {
            std::size_t pos{};
            std::optional<std::uint32_t> extended;
            while (pos < data.size()) {
                if (data.size() - pos < 6) return false;
                const auto tag = data.subspan(pos, 4);
                std::uint32_t size = std::uint32_t(data[pos + 4]) | std::uint32_t(data[pos + 5]) << 8;
                pos += 6;
                if (Tag(tag, 0, "XXXX")) {
                    if (extended || size != 4 || data.size() - pos < 4) return false;
                    extended = U32(data, pos); pos += 4;
                    continue;
                }
                if (extended) { size = *extended; extended.reset(); }
                if (size > data.size() - pos || !consume(tag, data.subspan(pos, size))) return false;
                pos += size;
            }
            return !extended;
        }

        class Reader
        {
        public:
            Reader(std::istream& source, const std::unordered_set<std::uint32_t>& query,
                const std::function<bool()>& live) : stream(source), wanted(query), current(live) {}
            Result Run()
            {
                if (wanted.empty()) { result.valid = true; return std::move(result); }
                stream.seekg(0, std::ios::end);
                const auto end = stream.tellg();
                if (end < std::streampos(24)) return std::move(result);
                length = static_cast<std::uint64_t>(end);
                std::array<std::uint8_t, 24> header{};
                if (!At(0, header) || !Tag(header, 0, "TES4")) return std::move(result);
                const auto size = U32(header, 4);
                if (size > maxRecordBytes || size > length - 24 || (U32(header, 8) & 0x40000))
                    return std::move(result);
                std::vector<std::uint8_t> data(size);
                if (!At(24, data) || !Subrecords(data, [&](Bytes tag, Bytes) {
                        if (Tag(tag, 0, "MAST")) ++masters;
                        return masters <= 255;
                    })) return std::move(result);
                result.valid = Groups(24ULL + size, length, false, 0) && (!current || current());
                if (!result.valid) result.ids.clear();
                return std::move(result);
            }
        private:
            bool At(std::uint64_t offset, std::span<std::uint8_t> data)
            {
                if ((current && !current()) || offset > length || data.size() > length - offset) return false;
                stream.seekg(static_cast<std::streamoff>(offset));
                if (!stream) return false;
                if (data.empty()) return true;
                stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                result.bytesRead += static_cast<std::uint64_t>(stream.gcount());
                return stream.good();
            }
            bool Groups(std::uint64_t pos, std::uint64_t end, bool factions, unsigned depth)
            {
                if (depth > 16) return false;
                std::array<std::uint8_t, 24> header{};
                while (pos < end) {
                    if (end - pos < 24 || !At(pos, header)) return false;
                    const auto size = U32(header, 4);
                    if (Tag(header, 0, "GRUP")) {
                        if (size < 24 || size > end - pos) return false;
                        const bool factGroup = U32(header, 12) == 0 && Tag(header, 8, "FACT");
                        if ((factions || factGroup) && !Groups(pos + 24, pos + size, true, depth + 1)) return false;
                        pos += size; // seek past every unrelated group, including CELL/WRLD
                    } else {
                        if (size > end - pos - 24) return false;
                        const auto raw = U32(header, 12);
                        const auto local = raw & 0xFFFFFFU;
                        if (factions && Tag(header, 0, "FACT") && (raw >> 24) == masters && wanted.contains(local)) {
                            result.ids.erase(local);
                            if (!(U32(header, 8) & 0x20U) && size <= maxRecordBytes) Record(pos + 24, size, U32(header, 8), local);
                        }
                        pos += 24ULL + size;
                    }
                }
                return true;
            }
            void Record(std::uint64_t pos, std::uint32_t size, std::uint32_t flags, std::uint32_t local)
            {
                std::vector<std::uint8_t> stored(size), unpacked;
                if (!At(pos, stored)) return;
                Bytes body = stored;
                if (flags & 0x40000) {
                    if (size < 4) return;
                    const auto expanded = U32(stored, 0);
                    if (!expanded || expanded > maxRecordBytes) return;
                    unpacked.resize(expanded);
                    uLongf output = expanded;
                    uLong input = size - 4;
                    if (uncompress2(unpacked.data(), &output, stored.data() + 4, &input) != Z_OK ||
                        output != expanded || input != size - 4) return;
                    body = unpacked;
                }
                std::string editor;
                const bool valid = Subrecords(body, [&](Bytes tag, Bytes value) {
                    if (!Tag(tag, 0, "EDID")) return true;
                    if (value.empty() || value.size() > 4096 || value.back() != 0) return false;
                    // Editor IDs are labels, not paths/commands. Reject embedded
                    // control characters rather than letting them alter the UI.
                    const auto text = value.first(value.size() - 1);
                    if (std::ranges::any_of(text, [](auto c) {return c < 0x20 || c == 0x7F;})) return false;
                    editor.assign(reinterpret_cast<const char*>(text.data()), text.size());
                    return true;
                });
                if (valid && !editor.empty()) result.ids.emplace(local, std::move(editor));
            }
            std::istream& stream;
            const std::unordered_set<std::uint32_t>& wanted;
            const std::function<bool()>& current;
            std::uint64_t length{};
            std::uint32_t masters{};
            Result result;
        };
    }

    Result Read(std::istream& stream, const std::unordered_set<std::uint32_t>& wanted,
        const std::function<bool()>& current)
    {
        return Reader(stream, wanted, current).Run();
    }

    Result ReadFile(const std::filesystem::path& dataDirectory, std::string_view plugin,
        const std::unordered_set<std::uint32_t>& wanted, const std::function<bool()>& current)
    {
        // Only the engine's basename is accepted, never an arbitrary path.
        if (plugin.empty() || plugin.find_first_of("/\\:") != std::string_view::npos ||
            plugin.find('\0') != std::string_view::npos ||
            !(asset_identity::EndsWith(plugin, ".esm") || asset_identity::EndsWith(plugin, ".esp") ||
                asset_identity::EndsWith(plugin, ".esl"))) return {};
        std::ifstream stream(dataDirectory / path_text::FromUtf8(plugin), std::ios::binary);
        if (!stream) return {};
        return Read(stream, wanted, current);
    }
}
