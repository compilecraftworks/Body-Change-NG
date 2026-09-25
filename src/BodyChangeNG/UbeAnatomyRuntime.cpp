#include "BodyChangeNG/UbeAnatomyRuntime.h"
#include "BodyChangeNG/UbeTriSupport.h"

#include <RE/A/Actor.h>
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/B/BSVisit.h>
#include <RE/N/NiStringExtraData.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    static_assert(offsetof(RE::BSResource::StreamBase, totalSize) == 0x08);
    static_assert(offsetof(RE::BSResourceNiBinaryStream, stream) == 0x20);
    // Bounded path-only metadata: no retained actor, mesh or engine pointer.
    std::mutex cacheLock;
    struct CacheEntry { std::uint8_t support; std::uint64_t used; };
    std::unordered_map<std::string, CacheEntry> supportCache;
    std::uint64_t cacheUse{};

    class StreamReader
    {
    public:
        explicit StreamReader(RE::BSResourceNiBinaryStream& stream) : stream_(stream)
        {
            // CommonLib get_info is a no-op. StreamBase::totalSize is at 0x08
            // in both SE and AE (only the later flags member changes offset).
            remaining_ = stream_.stream ? stream_.stream->totalSize : 0U;
        }
        [[nodiscard]] std::uint32_t Remaining() const { return remaining_; }
        bool Read(void* data, const std::uint32_t bytes)
        {
            if (bytes > remaining_ || !stream_.read(static_cast<char*>(data), bytes)) return false;
            remaining_ -= bytes;
            return true;
        }
        bool Skip(const std::uint32_t bytes)
        {
            if (bytes > remaining_ || bytes > 128U * 1024U * 1024U) return false;
            if (!bytes) return true;
            // Read the last skipped byte as well: seeking beyond EOF alone
            // must not certify a truncated payload as valid.
            const auto before = stream_.tell();
            stream_.seek(static_cast<std::int32_t>(bytes - 1U));
            char last{};
            if (stream_.tell() != before + bytes - 1U || !stream_.read(&last, 1U)) return false;
            remaining_ -= bytes;
            return true;
        }
    private:
        RE::BSResourceNiBinaryStream& stream_;
        std::uint32_t remaining_{};
    };

    std::string TriPath(const char* raw)
    {
        if (!raw) return {};
        const auto length = strnlen(raw, 1024U);
        if (!length || length == 1024U) return {};
        std::string path(raw, length);
        for (auto& c : path) {
            if (c == '/') c = '\\';
            else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        }
        if (path.front() == '\\' || path.find(':') != std::string::npos ||
            path.find("..") != std::string::npos || !path.ends_with(".tri")) return {};
        // BODYTRI is relative to meshes, as in RaceMenu CreateTRIPath.
        return "meshes\\" + path;
    }

    std::uint8_t FileSupport(const std::string& path)
    {
        std::scoped_lock lock(cacheLock);
        if (const auto found = supportCache.find(path); found != supportCache.end()) {
            found->second.used = ++cacheUse;
            return found->second.support;
        }
        std::uint8_t support{};
        RE::BSResourceNiBinaryStream stream(path);
        if (stream.good()) {
            StreamReader reader(stream);
            support = bcn::ube_anatomy::ReadExtraSupport(reader).value_or(0U);
        }
        // Evict one least-recently-used path, not the entire cache: a new
        // outfit must not force every other NPC's TRI to be read again.
        if (supportCache.size() >= 64U) {
            const auto oldest = std::min_element(supportCache.begin(), supportCache.end(),
                [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
            supportCache.erase(oldest);
        }
        supportCache.emplace(path, CacheEntry{ support, ++cacheUse });
        return support;
    }
}

namespace bcn::ube_anatomy
{
    std::uint8_t SupportedExtras(RE::Actor* actor)
    {
        auto* root = actor ? actor->Get3D(false) : nullptr;
        if (!root) return 0U;
        std::uint8_t support{};
        std::size_t visited{};
        std::unordered_set<std::string> checked;
        const RE::BSFixedString tag("BODYTRI");
        RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* object) {
            if (++visited > 4096U || checked.size() >= 32U || support == 0x1FU) {
                return RE::BSVisit::BSVisitControl::kStop;
            }
            if (!object) return RE::BSVisit::BSVisitControl::kContinue;
            const auto* extra = netimmerse_cast<RE::NiStringExtraData*>(object->GetExtraData(tag));
            if (extra) {
                const auto path = TriPath(extra->value);
                if (!path.empty() && checked.insert(path).second) support |= FileSupport(path);
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return support;
    }

    void ClearSupportCache()
    {
        std::scoped_lock lock(cacheLock);
        supportCache.clear();
        cacheUse = 0U;
    }
}
