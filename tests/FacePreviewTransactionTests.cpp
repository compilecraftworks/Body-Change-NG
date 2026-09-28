// Exact production journal and saved-key access bodies. Only engine/provider
// boundaries are fakes; this is not a Skyrim render or allocator test.
#include "BodyChangeNG/FacePreviewTransaction.h"
#include "BodyChangeNG/FacePreviewSaveGate.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/RaceMenuOverrideABI.h"
#include <algorithm>
#include <compare>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace RE {
    struct Base { unsigned id{7}; unsigned GetFormID() const { return id; } };
    class Actor;
    struct ActorHandle { std::weak_ptr<Actor> value; auto get() const { return value.lock(); } };
    class Actor : public std::enable_shared_from_this<Actor> {
    public:
        unsigned id{0x14}; Base base; bool player{true};
        std::string node{"Face"};
        bool IsPlayerRef() const { return player; }
        unsigned GetFormID() const { return id; }
        Base* GetActorBase() { return &base; }
        ActorHandle GetHandle() { return {shared_from_this()}; }
    };
    using BSFixedString = std::string;
    struct UI {
        bool editing{};
        static UI* GetSingleton() { static UI value; return &value; }
        bool IsMenuOpen(const char*) const { return editing; }
    };
    struct RaceSexMenu { static constexpr auto MENU_NAME = "RaceSex Menu"; };
    struct BGSSaveLoadGame {
        bool saving{}, loading{};
        static BGSSaveLoadGame* GetSingleton() { static BGSSaveLoadGame value; return &value; }
        bool GetSaveGameSaving() const { return saving; }
        bool GetSaveGameLoading() const { return loading; }
    };
    struct BGSSaveLoadManager {
        struct Thread { bool isBusy{}; };
        struct Data { Thread thread; } data;
        static BGSSaveLoadManager* GetSingleton() { static BGSSaveLoadManager value; return &value; }
        const Data& GetRuntimeData() const { return data; }
    };
}
namespace REL {
    struct Version {
        unsigned minor{}, patch{};
        auto compare(const Version& v) const { return std::pair{minor, patch} <=> std::pair{v.minor, v.patch}; }
    };
    Version loaded{5, 97};
    struct Module {
    static Module& get() { static Module value; return value; }
    Version version() const { return loaded; }
    };
    #include "save_relocate.inc"
}
namespace SKSE { constexpr REL::Version RUNTIME_SSE_1_6_1130{6, 1130}; }
#include "save_accessor_macro.inc"
#include "save_accessor_alias.inc"
struct SaveManagerLayout {
    alignas(8) std::byte storage[0x400]{};
    struct RUNTIME_DATA { std::byte thread[0xC0]; };
    #include "save_manager_accessor.inc"
};
namespace bcn::runtime {
    enum class GameBranch { supported, unsupported };
    bool supported{true};
    GameBranch ResolveGameBranch(REL::Version) { return supported ? GameBranch::supported : GameBranch::unsupported; }
}
namespace bcn::frame_tasks { unsigned owner{0x14}; bool HasPreview(unsigned id) { return id == owner; } }
namespace SKSE::log { template<class... T> void error(T&&...) {} }

using LegacyOverrideVariant = bcn::racemenu_abi::LegacyOverrideVariant;
enum class NodeOverrideAbi { legacyV1, publicV2 };
constexpr unsigned textureKey = 9;
struct StringResult { std::string text; bool stringReceived{}; };
struct StringValue { const std::string& text; explicit StringValue(const std::string& s) : text(s) {} };
using Key = std::pair<std::string, unsigned>;
struct Storage {
    std::map<Key, LegacyOverrideVariant> saved, live;
    unsigned reads{}, writes{};
} provider;
struct IOverrideInterfaceV1 {
    LegacyOverrideVariant* GetNodeOverride(RE::Actor*, bool, const std::string& node, unsigned, unsigned ch) {
        ++provider.reads;
        const auto it = provider.saved.find({node, ch});
        return it == provider.saved.end() ? nullptr : &it->second;
    }
    void GetNodeProperty(RE::Actor*, bool, const std::string& node, LegacyOverrideVariant* v) {
        *v = provider.live[{node, static_cast<unsigned>(v->index)}];
    }
    void AddNodeOverride(RE::Actor*, bool, const std::string& node, LegacyOverrideVariant& v) {
        ++provider.writes; provider.saved[{node, static_cast<unsigned>(v.index)}] = v;
    }
} v1;
struct IOverrideInterfaceV2 {
    bool HasNodeOverride(RE::Actor*, bool, const char* node, unsigned, unsigned ch) {
        ++provider.reads; return provider.saved.contains({node, ch});
    }
    bool GetNodeOverride(RE::Actor*, bool, const char* node, unsigned, unsigned ch, StringResult& result) {
        const auto it = provider.saved.find({node, ch});
        if (it == provider.saved.end() || !it->second.string) return false;
        result.text = it->second.string->c_str(); result.stringReceived = true; return true;
    }
    void AddNodeOverride(RE::Actor*, bool, const char* node, unsigned key, unsigned ch, StringValue& value) {
        ++provider.writes;
        provider.saved[{node, ch}] = LegacyOverrideVariant::String(static_cast<std::uint16_t>(key),
            static_cast<std::uint8_t>(ch), value.text);
    }
} v2;
bool Valid(RE::Actor* actor, const std::string& node, unsigned ch) {
    return actor && actor->node == node && std::ranges::find(bcn::face_skin::kChannels, ch) != bcn::face_skin::kChannels.end();
}
namespace bcn::face_skin {
    #include "preview_saved_key.inc"
    class NodeAccess {
    public:
        void* interface_{&v1}; NodeOverrideAbi abi_{NodeOverrideAbi::legacyV1};
        explicit operator bool() const { return interface_ != nullptr; }
        bool ReadSaved(RE::Actor*, bool, const std::string&, unsigned, SavedKey&) const;
        bool RestoreSaved(RE::Actor*, bool, const std::string&, unsigned, const SavedKey&) const;
        bool SaveCurrent(RE::Actor*, bool, const std::string&, unsigned, const std::string&) const;
        bool Remove(RE::Actor*, bool, const std::string& node, unsigned ch) const {
            ++provider.writes; provider.saved.erase({node, ch}); return true;
        }
        bool Read(RE::Actor*, bool, const std::string& node, unsigned ch, bool, std::string& out) const {
            const auto it = provider.live.find({node, ch});
            if (it == provider.live.end() || !it->second.string) return false;
            out = it->second.string->c_str(); return true;
        }
    };
    // Includes the closing production namespace brace.
    #include "preview_key_access.inc"

#include "preview_transaction.inc"

namespace {
    using namespace bcn::face_preview;
    using namespace bcn::face_skin;
    void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    std::string Saved(const std::string& node, unsigned ch) {
        const auto it = provider.saved.find({node, ch});
        return it != provider.saved.end() && it->second.string ? it->second.string->c_str() : "<absent>";
    }
    void Live(RE::Actor* actor, unsigned ch, const std::string& path) {
        provider.live[{actor->node, ch}] = LegacyOverrideVariant::String(9, static_cast<std::uint8_t>(ch), path);
    }
    void Original(unsigned ch, const std::string& path) {
        provider.saved[{"Face", ch}] = LegacyOverrideVariant::String(9, static_cast<std::uint8_t>(ch), path);
    }
    void Clean() {
        Reset(); provider = {}; g_transaction = {}; g_save = {};
        bcn::runtime::supported = true; bcn::frame_tasks::owner = 0x14;
        *RE::BGSSaveLoadGame::GetSingleton() = {};
        *RE::BGSSaveLoadManager::GetSingleton() = {};
        RE::UI::GetSingleton()->editing = false;
    }
    bool PublishPath(const NodeAccess& api, RE::Actor* actor, unsigned ch, const std::string& path) {
        Live(actor, ch, path);
        return Publish(api, actor, true, actor->node, ch, path);
    }
}
int main() try {
    using namespace bcn::face_preview;
    using namespace bcn::face_skin;
    for (bool legacy : {true, false}) {
        NodeAccess api;
        if (!legacy) { api.interface_ = &v2; api.abi_ = NodeOverrideAbi::publicV2; }
        auto actor = std::make_shared<RE::Actor>();
        Clean(); Original(0, "committed.dds");
        const auto interned = provider.saved.at({"Face", 0}).string;
        Check(PublishPath(api, actor.get(), 0, "preview.dds"), "publish failed");
        Check(Saved("Face", 0) == "preview.dds", "preview did not change saved key");
        const auto reads = provider.reads, writes = provider.writes;
        for (unsigned i{}; i < 10000; ++i) Check(Publish(api, actor.get(), true, "Face", 0, "preview.dds"), "no-op failed");
        Check(provider.reads == reads && provider.writes == writes, "unchanged observer churned saved keys");
        Check(Restore(actor->id) && Saved("Face", 0) == "committed.dds", "cancel lost committed key");
        if (legacy) Check(provider.saved.at({"Face", 0}).string == interned, "v1 original was not the interned object");
        Check(!g_transaction.actor, "journal retained completed snapshot");
        Original(1, "");
        Check(PublishPath(api, actor.get(), 1, "preview.dds") && Restore(), "empty stored string setup");
        Check(Saved("Face", 1).empty() && provider.saved.contains({"Face", 1}), "empty key was confused with absent key");
        provider.saved.erase({"Face", 1});

        for (unsigned i{}; i < 10000; ++i) {
            for (auto ch : kChannels) Check(PublishPath(api, actor.get(), ch, "preview.dds"), "repeated publish failed");
            Check(Restore(), "repeated restore failed");
            Check(provider.saved.size() == 1 && !g_transaction.actor, "cancel grew saved entries or retained journal");
        }
        for (const auto path : {"A.dds", "B.dds", "C.dds", "default.dds"}) {
            Check(Restore() && PublishPath(api, actor.get(), 0, path), "A/B/default switch failed");
        }
        Check(Restore() && Saved("Face", 0) == "committed.dds", "multi-switch overwrote original");

        Check(PublishPath(api, actor.get(), 0, "applied.dds"), "commit setup");
        Check(Restore() && api.SaveCurrent(actor.get(), true, "Face", 0, "applied.dds"), "promotion failed");
        bcn::frame_tasks::owner = 0;
        OwnerChanged();
        Check(Saved("Face", 0) == "applied.dds", "owner release reverted committed key");
        bcn::frame_tasks::owner = actor->id;
        Check(PublishPath(api, actor.get(), 0, "preview.dds"), "foreign setup");
        Original(0, "other-mod.dds");
        Check(Restore() && Saved("Face", 0) == "other-mod.dds", "undo overwrote newer provider");
        Check(PublishPath(api, actor.get(), 0, "preview.dds"), "removed provider setup");
        provider.saved.erase({"Face", 0});
        Check(Restore() && Saved("Face", 0) == "<absent>", "undo recreated provider-removed key");

        Check(PublishPath(api, actor.get(), 0, "preview.dds"), "failed second publish setup");
        Check(!Publish(api, actor.get(), true, "Face", 0, "missing.dds"), "accepted non-live DDS");
        Check(Restore() && Saved("Face", 0) == "<absent>", "failure lost ownership of first publication");

        for (bool raceMenuFirst : {true, false}) {
            Original(0, "committed.dds");
            Check(PublishPath(api, actor.get(), 0, "preview.dds"), "save setup");
            BeforeSave();
            auto* state = RE::BGSSaveLoadGame::GetSingleton();
            auto* manager = RE::BGSSaveLoadManager::GetSingleton();
            state->saving = true; manager->data.thread.isBusy = true;
            Check(Saved("Face", 0) == "committed.dds", "pre-save did not restore before any plugin");
            if (!raceMenuFirst) OnSerialization();
            Check(PublishPath(api, actor.get(), 0, "during-save.dds"), "save suspension should keep live preview");
            Check(Saved("Face", 0) == "committed.dds", "RaceMenu serialized temporary key");
            if (raceMenuFirst) OnSerialization();
            state->saving = false;
            Check(PublishPath(api, actor.get(), 0, "worker.dds") && Saved("Face", 0) == "committed.dds", "unblocked before worker idle");
            manager->data.thread.isBusy = false; state->loading = true;
            Check(PublishPath(api, actor.get(), 0, "loading.dds") && Saved("Face", 0) == "committed.dds", "published during load");
            state->loading = false;
            Check(PublishPath(api, actor.get(), 0, "after-save.dds") && Saved("Face", 0) == "after-save.dds", "preview failed to resume");
            Check(Restore() && Saved("Face", 0) == "committed.dds", "post-save cancel changed commitment");
        }
        BeforeSave(); // No callback: failure must not silently reopen the gate.
        Check(PublishPath(api, actor.get(), 0, "failed-save.dds") && Saved("Face", 0) == "committed.dds", "premature save resume");
        Reset();
        for (std::size_t cut{}; cut <= kChannels.size(); ++cut) {
            for (std::size_t i{}; i <= kChannels.size(); ++i) {
                if (i == cut) BeforeSave();
                if (i < kChannels.size()) Check(PublishPath(api, actor.get(), kChannels[i], "partial.dds"), "interleaved publication failed");
            }
            Check(Saved("Face", 0) == "committed.dds" && provider.saved.size() == 1,
                "save boundary inside a five-channel batch leaked a temporary key");
            Reset();
        }
        Check(PublishPath(api, actor.get(), 0, "active.dds"), "late owner setup");
        OwnerChanged(); // Delayed older notification, current owner still valid.
        Check(Saved("Face", 0) == "active.dds", "stale notification reverted active owner");
        Check(Restore(), "late owner cleanup");
        Check(PublishPath(api, actor.get(), 0, "old-head.dds"), "head setup");
        actor->node = "NewFace";
        Check(PublishPath(api, actor.get(), 0, "new-head.dds"), "head change failed");
        Check(Saved("Face", 0) == "committed.dds" && Saved("NewFace", 0) == "new-head.dds", "old head was left temporary");
        bcn::frame_tasks::owner = 999;
        OwnerChanged();
        Check(Saved("NewFace", 0) == "<absent>" && !g_transaction.actor, "owner switch retained temporary key");
        actor->node = "Face";
        for (unsigned excluded{}; excluded < 4; ++excluded) {
            actor->player = excluded != 0;
            bcn::frame_tasks::owner = excluded == 1 ? 0 : actor->id;
            RE::UI::GetSingleton()->editing = excluded == 2;
            auto unavailable = api; if (excluded == 3) unavailable.interface_ = nullptr;
            const auto previousWrites = provider.writes;
            Check(PublishPath(unavailable, actor.get(), 0, "excluded.dds"), "live-only fallback failed");
            Check(provider.writes == previousWrites, "NPC/owner/RaceMenu/unknown-ABI guard changed keys");
        }
        Clean();
        Check(PublishPath(api, actor.get(), 0, "unload.dds"), "unload setup");
        actor.reset();
        Check(Restore() && !g_transaction.actor, "journal retained a deleted actor or original strings");
    }
    for (unsigned mask{}; mask < 8; ++mask) {
        SaveGate gate; gate.Begin(); gate.Serialized();
        Check(gate.ObserveIdle(mask & 1, mask & 2, mask & 4) == (mask == 0), "save state combination wrong");
        gate.Begin();
        Check(!gate.ObserveIdle(false, false, false), "second save reused old callback marker");
    }
    // SE 1.5.97, last pre-boundary AE, first post-boundary AE and current
    // supported endpoints use the upstream versioned member accessor itself.
    for (const auto version : {REL::Version{5, 97}, {6, 659}, {6, 1130}, {6, 1170}, {6, 1179}}) {
        REL::loaded = version;
        SaveManagerLayout layout;
        const auto offset = reinterpret_cast<const std::byte*>(&layout.GetRuntimeData()) - layout.storage;
        Check(offset == (version.minor == 5 || version.patch < 1130 ? 0x2B0 : 0x2F8), "save worker runtime offset wrong");
        const auto& immutable = layout;
        Check(&immutable.GetRuntimeData() == &layout.GetRuntimeData(), "const save accessor selected a different layout");
    }
    std::cout << "Face preview transaction: v1/v2, intern identity, absence, ownership, default/commit/cancel, save ordering, 20000 journal lifecycles passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
