#include "BodyChangeNG/FaceSkinNodeAccess.h"
#include "BodyChangeNG/FaceSkinPolicy.h"
#include "BodyChangeNG/RaceMenuOverrideABI.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include <algorithm>

namespace
{
    using namespace bcn::racemenu_abi;
    using bcn::racemenu_compat::NodeOverrideAbi;
    constexpr std::uint16_t textureKey = 9;
    bool Valid(RE::Actor* actor, const std::string& node, unsigned channel)
    {
        auto* base = actor ? actor->GetActorBase() : nullptr;
        const auto* head = base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
        return head && !node.empty() && head->formEditorID.c_str() == node &&
            std::ranges::find(bcn::face_skin::kChannels, channel) != bcn::face_skin::kChannels.end();
    }
    class StringValue final : public IOverrideInterfaceV2::SetVariant
    {
    public:
        explicit StringValue(const std::string& value) : value_(value) {}
        Type GetType() override { return Type::String; }
        const char* String() override { return value_.c_str(); }
    private:
        const std::string& value_;
    };
    class StringResult final : public IOverrideInterfaceV2::GetVariant
    {
    public:
        void Int(std::int32_t) override {}
        void Float(float) override {}
        void String(const char* value) override { text = value ? value : ""; }
        void Bool(bool) override {}
        void TextureSet(const RE::BGSTextureSet*) override {}
        std::string text;
    };
}

namespace bcn::face_skin
{
    NodeAccess NodeAccess::Connect()
    {
        NodeAccess result;
        const auto branch = runtime::ResolveGameBranch(REL::Module::get().version());
        if (branch == runtime::GameBranch::unsupported) return result;
        auto* provider = static_cast<IPluginInterface*>(racemenu::QueryInterface("Override"));
        if (!provider) return result;
        result.abi_ = racemenu_compat::ResolveNodeOverrideAbi(provider->GetVersion(), branch);
        if (result.abi_ != NodeOverrideAbi::papyrus) result.interface_ = provider;
        return result;
    }
    const char* NodeAccess::Label() const noexcept
    {
        switch (abi_) {
        case NodeOverrideAbi::legacyV1: return "Override-v1 immediate";
        case NodeOverrideAbi::publicV2: return "Override-v2 prefix immediate";
        default: return "Papyrus fallback";
        }
    }
    bool NodeAccess::Read(RE::Actor* actor, bool female, const std::string& node,
        unsigned channel, bool saved, std::string& value) const
    {
        value.clear();
        // Saved keys on a previous head must remain readable for cleanup.
        if (!interface_ || !actor || node.empty() ||
            std::ranges::find(kChannels, channel) == kChannels.end() || (!saved && !Valid(actor, node, channel))) return false;
        if (abi_ == NodeOverrideAbi::legacyV1) {
            auto* api = static_cast<IOverrideInterfaceV1*>(interface_);
            const RE::BSFixedString name(node);
            LegacyOverrideVariant property;
            property.key = textureKey;
            property.index = static_cast<std::int8_t>(channel);
            const auto* found = saved ? api->GetNodeOverride(actor, female, name, textureKey,
                static_cast<std::uint8_t>(channel)) : &property;
            if (!saved) api->GetNodeProperty(actor, false, name, &property);
            if (found && found->type == LegacyOverrideVariant::kTypeString && found->string)
                value = found->string->c_str(); // copy before any registry mutation
            return true;
        }
        auto* api = static_cast<IOverrideInterfaceV2*>(interface_);
        StringResult result;
        if (saved) {
            api->GetNodeOverride(actor, female, node.c_str(), textureKey, static_cast<std::uint8_t>(channel), result);
        } else if (!api->GetNodeProperty(actor, false, node.c_str(), textureKey, static_cast<std::uint8_t>(channel), result)) return false;
        value = std::move(result.text);
        return true;
    }
    bool NodeAccess::Write(RE::Actor* actor, bool female, const std::string& node,
        unsigned channel, const std::string& value, bool persist) const
    {
        if (!interface_ || value.empty() || !Valid(actor, node, channel)) return false;
        const auto index = static_cast<std::uint8_t>(channel);
        if (abi_ == NodeOverrideAbi::legacyV1) {
            auto* api = static_cast<IOverrideInterfaceV1*>(interface_);
            const RE::BSFixedString name(node); // owns trivial borrowed LegacyNodeName
            auto write = LegacyOverrideVariant::String(textureKey, index, value);
            // v1 AddNodeOverride copies its argument without interning it.
            // GetNodeProperty supplies RaceMenu's own serializable string.
            LegacyOverrideVariant interned;
            interned.key = textureKey;
            interned.index = static_cast<std::int8_t>(index);
            return WriteLegacyString(value, persist,
                [&] { api->SetNodeProperty(actor, name, &write, true); },
                [&]() -> std::string {
                    api->GetNodeProperty(actor, false, name, &interned);
                    return interned.type == LegacyOverrideVariant::kTypeString && interned.string ?
                        std::string(interned.string->c_str()) : std::string{};
                },
                [&] { api->AddNodeOverride(actor, female, name, interned); });
        }
        auto* api = static_cast<IOverrideInterfaceV2*>(interface_);
        StringValue write(value);
        if (persist) api->AddNodeOverride(actor, female, node.c_str(), textureKey, index, write);
        // v1 handles both views internally; public v2 takes an explicit view.
        // Match the existing Papyrus behavior without applying twice to an
        // object shared by first and third person.
        RE::NiAVObject* last{};
        bool applied{};
        for (const bool firstPerson : { false, true }) {
            auto* root = actor->Get3D(firstPerson);
            auto* object = root ? root->GetObjectByName(RE::BSFixedString(node)) : nullptr;
            if (!object || object == last) continue;
            api->SetNodeProperty(actor, firstPerson, node.c_str(), textureKey, index, write, true);
            last = object;
            applied = true;
        }
        return applied;
    }
    bool NodeAccess::Remove(RE::Actor* actor, bool female, const std::string& node, unsigned channel) const
    {
        if (!interface_ || !actor || node.empty() || std::ranges::find(kChannels, channel) == kChannels.end()) return false;
        if (abi_ == NodeOverrideAbi::legacyV1) {
            const RE::BSFixedString name(node);
            static_cast<IOverrideInterfaceV1*>(interface_)->RemoveNodeOverride(actor, female, name, textureKey, static_cast<std::uint8_t>(channel));
        } else {
            static_cast<IOverrideInterfaceV2*>(interface_)->RemoveNodeOverride(actor, female, node.c_str(), textureKey, static_cast<std::uint8_t>(channel));
        }
        return true;
    }
}
