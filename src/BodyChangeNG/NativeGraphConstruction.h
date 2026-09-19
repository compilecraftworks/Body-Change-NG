#pragma once

#include "BodyChangeNG/NativeTextureConstruction.h"
#include <vector>

namespace bcn::native_skin
{
    // Construction transaction only: never own provider forms or any graph
    // already handed to an ActorBase/TNG. Referencing forms die before TXSTs.
    template<class Form, class Texture>
    class GraphConstruction final
    {
    public:
        GraphConstruction() = default;
        GraphConstruction(const GraphConstruction&) = delete;
        GraphConstruction& operator=(const GraphConstruction&) = delete;
        ~GraphConstruction()
        {
            for (auto& form : forms_) form.reset();
            textures_.clear();
        }
        void OwnForm(Form* form)
        {
            std::unique_ptr<Form> pending{ form };
            forms_.push_back(std::move(pending));
        }
        void OwnTexture(Texture* texture)
        {
            TextureConstruction<Texture> pending{ texture };
            textures_.push_back(std::move(pending));
        }
        void Release()
        {
            for (auto& form : forms_) static_cast<void>(form.release());
            for (auto& texture : textures_) static_cast<void>(texture.release());
            forms_.clear();
            textures_.clear();
        }
    private:
        std::vector<std::unique_ptr<Form>> forms_;
        std::vector<TextureConstruction<Texture>> textures_;
    };
}
