#include "BodyChangeNG/RuntimeAssetCache.h"
#include "BodyChangeNG/ContentSignature.h"
#include <Windows.h>
#include <fstream>
#include <array>

#include "BodyChangeNG/PathText.h"

#include <SKSE/Logger.h>

#include <format>
#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <ranges>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    std::mutex g_registeredSourcesLock;
    std::unordered_map<std::string, std::filesystem::path> g_registeredSources;
    struct SourceHash
    {
        std::uint64_t value{};
        std::string gamePath;
        std::vector<std::filesystem::path> normalCompanions;
    };
    std::unordered_map<std::string, SourceHash> g_sourceHashes;
    // Cache existence is immutable for normal gameplay. Catalog Refresh clears
    // this set so an external MO2 overwrite cleanup is detected without doing
    // one filesystem stat per actor for a shared skin file.
    std::unordered_set<std::string> g_verifiedCachePaths;

    [[nodiscard]] std::optional<std::filesystem::path> FinalSourcePath(
        const std::filesystem::path& path)
    {
        const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return std::nullopt;

        std::wstring buffer(32768U, L'\0');
        const auto length = GetFinalPathNameByHandleW(handle, buffer.data(),
            static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        CloseHandle(handle);
        if (length == 0U || length >= buffer.size()) return std::nullopt;

        buffer.resize(length);
        constexpr std::wstring_view uncPrefix{ LR"(\\?\UNC\)" };
        constexpr std::wstring_view extendedPrefix{ LR"(\\?\)" };
        if (buffer.starts_with(uncPrefix)) buffer = LR"(\\)" + buffer.substr(uncPrefix.size());
        else if (buffer.starts_with(extendedPrefix)) buffer.erase(0U, extendedPrefix.size());
        return std::filesystem::path{ buffer }.lexically_normal();
    }

    [[nodiscard]] std::string SourceIdentity(const std::filesystem::path& source)
    {
        auto identity = bcn::path_text::GenericUtf8(source.lexically_normal());
        std::ranges::transform(identity, identity.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return identity;
    }

    class TexturePreparationWorker final
    {
    public:
        TexturePreparationWorker() : worker_([this](const std::stop_token stop) { Run(stop); }) {}
        ~TexturePreparationWorker()
        {
            worker_.request_stop();
            condition_.notify_all();
        }

        bool Submit(const std::uint32_t key,
            std::vector<bcn::runtime_assets::TexturePreparation> paths,
            std::function<void(bool)> completion, const bool urgent)
        {
            if (!completion || worker_.get_stop_token().stop_requested()) return false;
            std::scoped_lock lock(lock_);
            // Pending replacements for one actor have not started any I/O.
            // Dropping their closure also releases the retained actor lease.
            std::erase_if(jobs_, [key](const Job& job) { return job.key == key; });
            Job job{ key, std::move(paths), std::move(completion) };
            if (urgent) jobs_.push_front(std::move(job));
            else jobs_.push_back(std::move(job));
            condition_.notify_one();
            return true;
        }

        void CancelAll()
        {
            std::scoped_lock lock(lock_);
            jobs_.clear();
        }

    private:
        struct Job final
        {
            std::uint32_t key{};
            std::vector<bcn::runtime_assets::TexturePreparation> paths;
            std::function<void(bool)> completion;
        };

        void Run(const std::stop_token stop)
        {
            while (!stop.stop_requested()) {
                std::optional<Job> job;
                {
                    std::unique_lock lock(lock_);
                    condition_.wait(lock, stop, [&] { return !jobs_.empty(); });
                    if (stop.stop_requested()) return;
                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                }
                bool success = true;
                for (const auto& path : job->paths) {
                    if (stop.stop_requested()) return;
                    if (bcn::runtime_assets::TexturePathFromGameRelative(
                            path.path, path.nameSpace).empty()) success = false;
                }
                if (!stop.stop_requested()) job->completion(success);
            }
        }

        std::mutex lock_;
        std::condition_variable_any condition_;
        std::deque<Job> jobs_;
        std::jthread worker_;
    };

    TexturePreparationWorker& PreparationWorker()
    {
        static TexturePreparationWorker worker;
        return worker;
    }

    [[nodiscard]] std::string NormalizeGamePath(std::string_view path)
    {
        std::string result{ path };
        std::ranges::replace(result, '/', '\\');
        std::ranges::transform(result, result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    [[nodiscard]] std::uint64_t Hash(const std::string_view value) noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (const auto character : value) {
            hash = (hash ^ static_cast<std::uint8_t>(character)) * 1099511628211ULL;
        }
        return hash;
    }

    [[nodiscard]] std::string LowerAscii(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    [[nodiscard]] std::vector<std::filesystem::path> NormalCompanions(
        const std::filesystem::path& source)
    {
        if (LowerAscii(bcn::path_text::Utf8(source.extension())) != ".dds") return {};
        const auto stem = bcn::path_text::Utf8(source.stem());
        const auto lower = LowerAscii(stem);
        std::string base;
        if (lower.ends_with("_msn")) base = stem.substr(0, stem.size() - 4U);
        else if (lower.ends_with("_n") && !lower.ends_with("_n_m") &&
                 !lower.ends_with("_n_ov")) base = stem.substr(0, stem.size() - 2U);
        else return {};

        std::vector<std::filesystem::path> result;
        const auto add = [&](const std::string_view suffix) {
            const auto candidate = source.parent_path() /
                bcn::path_text::FromUtf8(base + std::string{ suffix });
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error) && !error && candidate != source) {
                result.push_back(candidate);
            }
        };
        // Mu Dynamic NormalMap derives these exact sibling names from the
        // active _msn/_n filename. Keep every installed optional component;
        // the user's selected Mu/RSV mask policy remains authoritative.
        if (lower.ends_with("_msn")) add("_n.dds");
        add("_n_m.dds");
        add("_n_ov.dds");
        return result;
    }

    void HashFile(bcn::ContentSignature& hash, const std::filesystem::path& source)
    {
        hash.Text(LowerAscii(bcn::path_text::Utf8(source.filename())));
        std::ifstream stream(source, std::ios::binary);
        std::array<char, 65536> block;
        while (stream) {
            stream.read(block.data(), block.size());
            for (std::streamsize n{}; n < stream.gcount(); ++n) {
                hash.Byte(static_cast<unsigned char>(block[n]));
            }
        }
    }

    [[nodiscard]] std::string CacheFilename(const std::filesystem::path& source,
        const std::string_view identity)
    {
        auto filename = bcn::path_text::Utf8(source.filename());
        const auto safeAscii = !filename.empty() && std::ranges::all_of(filename, [](const unsigned char value) {
            return std::isalnum(value) || value == '.' || value == '_' || value == '-';
        });
        if (safeAscii) return filename;
        auto extension = bcn::path_text::Utf8(source.extension());
        if (extension.empty()) extension = ".dds";
        return std::format("{:016X}{}", Hash(identity), extension);
    }

    [[nodiscard]] bool SameFile(const std::filesystem::path& left,
        const std::filesystem::path& right)
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(left, error) || error) return false;
        const auto leftSize = std::filesystem::file_size(left, error);
        if (error) return false;
        const auto rightSize = std::filesystem::file_size(right, error);
        if (error || leftSize != rightSize) return false;
        const auto leftTime = std::filesystem::last_write_time(left, error);
        if (error) return false;
        const auto rightTime = std::filesystem::last_write_time(right, error);
        return !error && leftTime == rightTime;
    }

    [[nodiscard]] std::optional<std::filesystem::path> ResolveGameRelativeSource(
        const std::string_view path)
    {
        if (path.empty()) return std::nullopt;
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            if (const auto found = g_registeredSources.find(NormalizeGamePath(path));
                found != g_registeredSources.end()) {
                // Registration already validated this provider during the
                // catalog scan. Avoid repeating a source-file stat for every
                // actor application; an explicit Refresh rebuilds the map.
                return found->second;
            }
        }
        return std::filesystem::current_path() / "Data" / bcn::path_text::FromUtf8(path);
    }

    [[nodiscard]] std::string CacheRelativePath(const std::filesystem::path& source,
        const std::string_view nameSpace)
    {
        auto identity = SourceIdentity(source);
        std::error_code error;
        bool registered{};
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            if (const auto found = g_sourceHashes.find(identity); found != g_sourceHashes.end()) {
                identity += "|" + std::to_string(found->second.value);
                registered = true;
            }
        }
        if (!registered) {
            if (!std::filesystem::is_regular_file(source, error) || error) return {};
            // Non-catalog caller: cheap metadata only, never hash a DDS from
            // an actor application callback.
            const auto size = std::filesystem::file_size(source, error);
            const auto stamp = std::filesystem::last_write_time(source, error);
            if (!error) identity += "|" + std::to_string(size) + "|" +
                std::to_string(stamp.time_since_epoch().count());
        }
        const auto cacheKey = std::format("{:016X}", Hash(identity));
        const auto filename = CacheFilename(source, identity);
        const auto relative = std::filesystem::path{ "textures" } / "BodyChangeNG" / "Cache" /
            bcn::path_text::FromUtf8(nameSpace) / cacheKey /
            bcn::path_text::FromUtf8(filename);
        auto result = bcn::path_text::GenericUtf8(relative);
        std::ranges::replace(result, '/', '\\');
        return result;
    }
}

namespace bcn::runtime_assets
{
    void ClearGameRelativeSources(const std::string_view prefix)
    {
        const auto normalized = NormalizeGamePath(prefix);
        std::scoped_lock lock(g_registeredSourcesLock);
        std::erase_if(g_registeredSources, [&normalized](const auto& entry) {
            return entry.first.starts_with(normalized);
        });
        // Include losing MO2 providers as well as the final winning mapping.
        std::erase_if(g_sourceHashes, [&](const auto& entry) { return entry.second.gamePath.starts_with(normalized); });
        g_verifiedCachePaths.clear();
    }

    void RegisterGameRelativeSource(const std::string_view path,
        const std::filesystem::path& source)
    {
        if (path.empty() || source.empty()) return;
        std::error_code error;
        if (!std::filesystem::is_regular_file(source, error) || error) return;
        // MO2 exposes the winning file through the logical Data tree as well
        // as through its physical provider. Resolve both spellings to the
        // same backing file so a multi-gigabyte skin pack is hashed once per
        // refresh instead of once for every catalog root.
        const auto stableSource = FinalSourcePath(source).value_or(source.lexically_normal());
        std::scoped_lock lock(g_registeredSourcesLock);
        const auto identity = SourceIdentity(stableSource);
        if (!g_sourceHashes.contains(identity)) {
            // Once per distinct source at catalog scan/refresh, not once per
            // actor. Full bytes also detect replacements preserving size/time.
            // Optional Mu companions participate in the same signature so a
            // changed mask gets a fresh cache path and one normal-map refresh.
            ContentSignature hash;
            HashFile(hash, stableSource);
            auto companions = NormalCompanions(stableSource);
            for (const auto& companion : companions) HashFile(hash, companion);
            g_sourceHashes[identity] = {
                .value = hash.value,
                .gamePath = NormalizeGamePath(path),
                .normalCompanions = std::move(companions)
            };
        }
        g_registeredSources.insert_or_assign(NormalizeGamePath(path), stableSource);
    }

    std::uint64_t SourceContentHash(std::string_view path)
    {
        std::scoped_lock lock(g_registeredSourcesLock);
        const auto source = g_registeredSources.find(NormalizeGamePath(path));
        if (source == g_registeredSources.end()) return 0;
        const auto found = g_sourceHashes.find(SourceIdentity(source->second));
        return found == g_sourceHashes.end() ? 0 : found->second.value;
    }

    std::string TexturePath(const std::filesystem::path& source,
        const std::string_view nameSpace)
    {
        std::error_code error;
        const auto result = CacheRelativePath(source, nameSpace);
        if (result.empty()) {
            SKSE::log::error("Body Change NG texture source is missing: {}", path_text::Utf8(source));
            return {};
        }
        const auto normalizedResult = NormalizeGamePath(result);
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            if (g_verifiedCachePaths.contains(normalizedResult)) return result;
        }
        const auto destination = std::filesystem::current_path() / "Data" /
            path_text::FromUtf8(result);

        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            SKSE::log::error("Body Change NG could not create texture cache directory {}: {}",
                path_text::Utf8(destination.parent_path()), error.message());
            return {};
        }
        if (!SameFile(destination, source)) {
            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::create_hard_link(source, destination, error);
            if (error) {
                error.clear();
                std::filesystem::copy_file(source, destination,
                    std::filesystem::copy_options::overwrite_existing, error);
                if (!error) {
                    const auto sourceTime = std::filesystem::last_write_time(source, error);
                    if (!error) std::filesystem::last_write_time(destination, sourceTime, error);
                }
            }
            if (error) {
                SKSE::log::error("Body Change NG could not materialize texture cache {} from {}: {}",
                    path_text::Utf8(destination), path_text::Utf8(source), error.message());
                return {};
            }
        }

        std::vector<std::filesystem::path> companions;
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            const auto found = g_sourceHashes.find(SourceIdentity(source));
            if (found != g_sourceHashes.end()) companions = found->second.normalCompanions;
        }
        for (const auto& companion : companions) {
            const auto companionDestination = destination.parent_path() / companion.filename();
            if (SameFile(companionDestination, companion)) continue;
            error.clear();
            std::filesystem::remove(companionDestination, error);
            error.clear();
            std::filesystem::create_hard_link(companion, companionDestination, error);
            if (error) {
                error.clear();
                std::filesystem::copy_file(companion, companionDestination,
                    std::filesystem::copy_options::overwrite_existing, error);
                if (!error) {
                    const auto sourceTime = std::filesystem::last_write_time(companion, error);
                    if (!error) std::filesystem::last_write_time(companionDestination, sourceTime, error);
                }
            }
            if (error) {
                // The primary normal remains valid even if an optional Mu
                // detail/mask/overlay companion cannot be cached.
                SKSE::log::warn("Body Change NG could not materialize optional normal-map companion {}: {}",
                    path_text::Utf8(companion), error.message());
            }
        }

        {
            std::scoped_lock lock(g_registeredSourcesLock);
            g_verifiedCachePaths.insert(normalizedResult);
        }

        return result;
    }

    std::string ExpectedTexturePathFromGameRelative(const std::string_view path,
        const std::string_view nameSpace)
    {
        const auto source = ResolveGameRelativeSource(path);
        return source ? CacheRelativePath(*source, nameSpace) : std::string{};
    }

    bool CachedTextureExists(const std::string_view path)
    {
        if (path.empty()) return false;
        auto normalized = NormalizeGamePath(path);
        while (normalized.starts_with(".\\")) normalized.erase(0, 2);
        constexpr std::string_view prefix = "textures\\bodychangeng\\cache\\";
        if (!normalized.starts_with(prefix) || normalized.starts_with('\\') ||
            normalized.find(':') != std::string::npos) return false;

        const auto untrustedRelative = std::filesystem::path{ path_text::FromUtf8(normalized) };
        for (const auto& component : untrustedRelative) {
            if (component == "..") return false;
        }
        const auto relative = untrustedRelative.lexically_normal();
        {
            std::scoped_lock lock(g_registeredSourcesLock);
            if (g_verifiedCachePaths.contains(normalized)) return true;
        }
        std::error_code error;
        const auto exists = std::filesystem::is_regular_file(
            std::filesystem::current_path() / "Data" / relative, error) && !error;
        if (exists) {
            std::scoped_lock lock(g_registeredSourcesLock);
            g_verifiedCachePaths.insert(std::move(normalized));
        }
        return exists;
    }

    std::string TexturePathFromGameRelative(const std::string_view path,
        const std::string_view nameSpace)
    {
        const auto source = ResolveGameRelativeSource(path);
        return source ? TexturePath(*source, nameSpace) : std::string{};
    }

    bool PrepareTexturePathsAsync(const std::uint32_t key,
        std::vector<TexturePreparation> paths, std::function<void(bool)> completion,
        const bool urgent)
    {
        std::unordered_set<std::string> seen;
        std::erase_if(paths, [&](const TexturePreparation& item) {
            if (item.path.empty() || item.nameSpace.empty()) return true;
            return !seen.insert(item.nameSpace + ':' + NormalizeGamePath(item.path)).second;
        });
        return PreparationWorker().Submit(
            key, std::move(paths), std::move(completion), urgent);
    }

    void InitializeTexturePreparation()
    {
        static_cast<void>(PreparationWorker());
    }

    void CancelTexturePreparations()
    {
        PreparationWorker().CancelAll();
    }
}
