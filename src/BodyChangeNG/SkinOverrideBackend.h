#pragma once

#include "BodyChangeNG/RaceMenuOverrideRouting.h"

#include <cstdint>
#include <string>
#include <utility>

namespace RE
{
    class BGSTextureSet;
    class NiAVObject;
    class TESObjectARMA;
    class TESObjectARMO;
    class TESObjectREFR;
}

namespace bcn::skin_backend
{
    class IPluginInterface
    {
    public:
        virtual ~IPluginInterface() = default;
        virtual std::uint32_t GetVersion() = 0;
        virtual void Revert() = 0;
    };

    // RaceMenu's public Override interface v2. The complete virtual surface
    // stays isolated here so feature code cannot accidentally depend on or
    // reorder the external ABI.
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

    class StringVariant final : public IOverrideInterfaceV2::SetVariant
    {
    public:
        explicit StringVariant(std::string value) : value_(std::move(value)) {}
        Type GetType() override { return Type::String; }
        const char* String() override { return value_.c_str(); }

    private:
        std::string value_;
    };

    class StringVisitor final : public IOverrideInterfaceV2::GetVariant
    {
    public:
        void Int(std::int32_t) override {}
        void Float(float) override {}
        void String(const char* value) override { value_ = value ? value : ""; }
        void Bool(bool) override {}
        void TextureSet(const RE::BGSTextureSet*) override {}

        [[nodiscard]] const std::string& Value() const noexcept { return value_; }

    private:
        std::string value_;
    };

    [[nodiscard]] IPluginInterface* Interface() noexcept;
    [[nodiscard]] racemenu_override::Route ActiveRoute() noexcept;
    [[nodiscard]] IOverrideInterfaceV2* NativeV2() noexcept;
    [[nodiscard]] bool UsesPapyrus() noexcept;
}
