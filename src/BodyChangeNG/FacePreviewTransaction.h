#pragma once
#include <cstdint>
#include <string>

namespace RE { class Actor; }
namespace bcn::face_skin { class NodeAccess; }
namespace bcn::face_preview
{
    // Publish a reversible RaceMenu key after the live preview DDS is ready.
    // Unsupported provider ABIs retain the existing live-only preview path.
    bool Publish(const face_skin::NodeAccess&, RE::Actor*, bool female,
        const std::string& node, unsigned channel, const std::string& path);
    bool Restore(std::uint32_t actor = 0);
    void OwnerChanged();
    void BeforeSave();
    void OnSerialization();
    void ObserveSaveCompletion();
    void Reset();
}
