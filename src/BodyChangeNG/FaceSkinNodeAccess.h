#pragma once

#include "BodyChangeNG/RaceMenuCompatibility.h"
#include "BodyChangeNG/RaceMenuOverrideABI.h"
#include <string>

namespace RE { class Actor; }
namespace bcn::face_skin
{
    // A v1 saved string must retain RaceMenu's own interned object; recreating
    // an equivalent std::string is insufficient for its serialization table.
    struct SavedKey
    {
        bool present{};
        std::string path;
        racemenu_abi::LegacyOverrideVariant legacy;
    };
    // Owned name only; no geometry/material is retained beyond this read.
    [[nodiscard]] std::string ResolveNodeName(RE::Actor* actor);
    // Borrowed plugin interface, valid for a synchronous batch. No actor,
    // geometry, material, registry value or texture pointer is retained.
    class NodeAccess
    {
    public:
        static NodeAccess Connect();
        explicit operator bool() const noexcept { return interface_ != nullptr; }
        const char* Label() const noexcept;
        bool Read(RE::Actor*, bool female, const std::string& node, unsigned channel, bool saved, std::string& value) const;
        bool Write(RE::Actor*, bool female, const std::string& node, unsigned channel, const std::string& value, bool persist) const;
        bool Remove(RE::Actor*, bool female, const std::string& node, unsigned channel) const;
        bool ReadSaved(RE::Actor*, bool female, const std::string& node, unsigned channel, SavedKey&) const;
        // Registry only: no geometry lookup, texture load or shader update.
        bool RestoreSaved(RE::Actor*, bool female, const std::string& node, unsigned channel, const SavedKey&) const;
        bool SaveCurrent(RE::Actor*, bool female, const std::string& node, unsigned channel, const std::string& path) const;
    private:
        void* interface_{};
        racemenu_compat::NodeOverrideAbi abi_{ racemenu_compat::NodeOverrideAbi::papyrus };
    };
}
