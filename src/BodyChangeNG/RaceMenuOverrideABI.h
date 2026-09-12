#pragma once

#include "BodyChangeNG/RaceMenuLegacyStringABI.h"
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace RE { class TESObjectREFR; class TESObjectARMO; class TESObjectARMA; class NiAVObject; class BGSTextureSet; }

// Shared ABI declarations only. Face skin and paint have separate state and policy.
namespace bcn::racemenu_abi
{
    using bcn::racemenu_compat::LegacyNodeName;
    class IPluginInterface
    {
    public:
        virtual ~IPluginInterface() = default;
        virtual std::uint32_t GetVersion() = 0;
        virtual void Revert() = 0;
    };

    class LegacyFixedString final
    {
    public:
        LegacyFixedString() : hash_(HashLower(value_)) {}
        explicit LegacyFixedString(const std::string_view value) : value_(value), hash_(HashLower(value_)) {}
        [[nodiscard]] const char* c_str() const noexcept { return value_.c_str(); }

    private:
        [[nodiscard]] static std::size_t HashLower(const std::string_view value) noexcept
        {
            std::size_t result = 14695981039346656037ULL;
            for (const auto character : value) {
                result ^= static_cast<unsigned char>(std::tolower(
                    static_cast<unsigned char>(character)));
                result *= 1099511628211ULL;
            }
            return result;
        }

        // Exact RaceMenu 0.4.16 SKEEFixedString data layout.
        std::string value_;
        std::size_t hash_{};
    };

    struct LegacyOverrideVariant final
    {
        enum : std::uint8_t
        {
            kTypeNone = 0,
            kTypeIdentifier = 1,
            kTypeString = 2,
            kTypeInt = 3,
            kTypeFloat = 4,
            kTypeBool = 5
        };

        union Value
        {
            std::int32_t i;
            std::uint32_t u;
            float f;
            bool b;
            void* p;
        };

        std::uint16_t key{};
        std::uint8_t type{ kTypeNone };
        std::int8_t index{ -1 };
        Value data{};
        std::shared_ptr<LegacyFixedString> string;

        [[nodiscard]] static LegacyOverrideVariant String(const std::uint16_t key,
            const std::uint8_t index, const std::string_view value)
        {
            LegacyOverrideVariant result;
            result.key = key;
            result.type = kTypeString;
            result.index = static_cast<std::int8_t>(index);
            result.data.p = nullptr;
            result.string = std::make_shared<LegacyFixedString>(value);
            return result;
        }

        [[nodiscard]] static LegacyOverrideVariant Int(const std::uint16_t key,
            const std::uint8_t index, const std::int32_t value) noexcept
        {
            LegacyOverrideVariant result;
            result.key = key;
            result.type = kTypeInt;
            result.index = static_cast<std::int8_t>(index);
            result.data.i = value;
            return result;
        }

        [[nodiscard]] static LegacyOverrideVariant Float(const std::uint16_t key,
            const std::uint8_t index, const float value) noexcept
        {
            LegacyOverrideVariant result;
            result.key = key;
            result.type = kTypeFloat;
            result.index = static_cast<std::int8_t>(index);
            result.data.f = value;
            return result;
        }
    };

    static_assert(sizeof(LegacyFixedString) == 40U);
    static_assert(sizeof(LegacyOverrideVariant) == 32U);

    // RaceMenu v1 exposes the concrete OverrideInterface. The reserved entries
    // below intentionally mirror its exact 0.4.16 virtual order up to the node
    // operations BCNG uses. Names and signatures of unused entries are local;
    // their slots are never invoked.
    class IOverrideInterfaceV1 : public IPluginInterface
    {
    public:
        virtual void ReservedSave() = 0;
        virtual void ReservedLoad() = 0;
        virtual void ReservedLoadOverrides() = 0;
        virtual void ReservedLoadNodeOverrides() = 0;
        virtual void ReservedLoadWeaponOverrides() = 0;
        virtual void ReservedAddRawOverride() = 0;
        virtual void ReservedAddOverride() = 0;
        virtual void ReservedAddRawNodeOverride() = 0;
        virtual void AddNodeOverride(RE::TESObjectREFR*, bool, LegacyNodeName,
            LegacyOverrideVariant&) = 0;
        virtual void ReservedSetArmorAddonProperty() = 0;
        virtual void ReservedGetArmorAddonProperty() = 0;
        virtual void SetNodeProperty(RE::TESObjectREFR*, LegacyNodeName,
            LegacyOverrideVariant*, bool) = 0;
        virtual void GetNodeProperty(RE::TESObjectREFR*, bool, LegacyNodeName,
            LegacyOverrideVariant*) = 0;
        virtual void ReservedHasArmorAddonNode() = 0;
        virtual void ReservedApplyNodeOverrides() = 0;
        virtual void ReservedApplyOverrides() = 0;
        virtual void ReservedRemoveAllOverrides() = 0;
        virtual void ReservedRemoveAllReferenceOverrides() = 0;
        virtual void ReservedRemoveAllArmorOverrides() = 0;
        virtual void ReservedRemoveAllArmorAddonOverrides() = 0;
        virtual void ReservedRemoveAllArmorAddonNodeOverrides() = 0;
        virtual void ReservedRemoveArmorAddonOverride() = 0;
        virtual void ReservedRemoveAllNodeOverrides() = 0;
        virtual void ReservedRemoveAllReferenceNodeOverrides() = 0;
        virtual void ReservedRemoveAllNodeNameOverrides() = 0;
        virtual void RemoveNodeOverride(RE::TESObjectREFR*, bool, LegacyNodeName,
            std::uint16_t, std::uint8_t) = 0;
        virtual void ReservedGetOverride() = 0;
        virtual LegacyOverrideVariant* GetNodeOverride(RE::TESObjectREFR*, bool,
            LegacyNodeName, std::uint16_t, std::uint8_t) = 0;
    };

    // Public wrapper ABI used by Overlay/Override v2 releases.
    class IOverrideInterfaceV2 : public IPluginInterface
    {
    public:
        class GetVariant
        {
        public:
            virtual void Int(std::int32_t) = 0;
            virtual void Float(float) = 0;
            virtual void String(const char*) = 0;
            virtual void Bool(bool) = 0;
            virtual void TextureSet(const RE::BGSTextureSet*) = 0;
        };
        class SetVariant
        {
        public:
            enum class Type { None, Int, Float, String, Bool, TextureSet };
            virtual Type GetType() { return Type::None; }
            virtual std::int32_t Int() { return 0; }
            virtual float Float() { return 0.0F; }
            virtual const char* String() { return nullptr; }
            virtual bool Bool() { return false; }
            virtual RE::BGSTextureSet* TextureSet() { return nullptr; }
        };

        virtual bool HasArmorAddonNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, bool) = 0;
        virtual bool HasArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) = 0;
        virtual void AddArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&) = 0;
        virtual bool GetArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void RemoveArmorOverride(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t) = 0;
        virtual void SetArmorProperties(RE::TESObjectREFR*, bool) = 0;
        virtual void SetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
        virtual bool GetArmorProperty(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void ApplyArmorOverrides(RE::TESObjectREFR*, RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool) = 0;
        virtual void RemoveAllArmorOverrides() = 0;
        virtual void RemoveAllArmorOverridesByReference(RE::TESObjectREFR*) = 0;
        virtual void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR*, bool, RE::TESObjectARMO*) = 0;
        virtual void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*) = 0;
        virtual void RemoveAllArmorOverridesByNode(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, const char*) = 0;
        virtual bool HasNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t) = 0;
        virtual void AddNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, SetVariant&) = 0;
        virtual bool GetNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void RemoveNodeOverride(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t) = 0;
        virtual void SetNodeProperties(RE::TESObjectREFR*, bool) = 0;
        virtual void SetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
        virtual bool GetNodeProperty(RE::TESObjectREFR*, bool, const char*, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void ApplyNodeOverrides(RE::TESObjectREFR*, RE::NiAVObject*, bool) = 0;
        virtual void RemoveAllNodeOverrides() = 0;
        virtual void RemoveAllNodeOverridesByReference(RE::TESObjectREFR*) = 0;
        virtual void RemoveAllNodeOverridesByNode(RE::TESObjectREFR*, bool, const char*) = 0;
        virtual bool HasSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t) = 0;
        virtual void AddSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t, SetVariant&) = 0;
        virtual bool GetSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void RemoveSkinOverride(RE::TESObjectREFR*, bool, bool, std::uint32_t, std::uint16_t, std::uint8_t) = 0;
        virtual void SetSkinProperties(RE::TESObjectREFR*, bool) = 0;
        virtual void SetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, SetVariant&, bool) = 0;
        virtual bool GetSkinProperty(RE::TESObjectREFR*, bool, std::uint32_t, std::uint16_t, std::uint8_t, GetVariant&) = 0;
        virtual void ApplySkinOverrides(RE::TESObjectREFR*, bool, RE::TESObjectARMO*, RE::TESObjectARMA*, std::uint32_t, RE::NiAVObject*, bool) = 0;
        virtual void RemoveAllSkinOverrides() = 0;
        virtual void RemoveAllSkinOverridesByReference(RE::TESObjectREFR*) = 0;
        virtual void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR*, bool, bool, std::uint32_t) = 0;
    };

}
