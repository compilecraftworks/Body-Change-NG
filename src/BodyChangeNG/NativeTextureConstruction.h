#pragma once
#include <memory>

namespace bcn::native_skin
{
    // Only for BGSTextureSet factories with the verified initial reference=1.
    // Never adopt an existing provider or a published ARMA's borrowed pointer.
    // release() transfers that construction reference to a persistent graph;
    // failure releases it through NiRefObject, never through TESForm delete.
    template<class Texture>
    struct ReleaseTextureConstruction
    {
        void operator()(Texture* value) const noexcept { if (value) value->DecRefCount(); }
    };
    template<class Texture>
    using TextureConstruction = std::unique_ptr<Texture, ReleaseTextureConstruction<Texture>>;
}
