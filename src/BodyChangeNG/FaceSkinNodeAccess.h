#pragma once

#include "BodyChangeNG/RaceMenuCompatibility.h"
#include <string>

namespace RE { class Actor; }
namespace bcn::face_skin
{
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
    private:
        void* interface_{};
        racemenu_compat::NodeOverrideAbi abi_{ racemenu_compat::NodeOverrideAbi::papyrus };
    };
}
