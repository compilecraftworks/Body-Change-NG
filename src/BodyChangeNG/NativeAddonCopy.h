#pragma once

#include <cstddef>

namespace bcn::native_skin
{
    // TESObjectARMA inherits TESForm's no-op Copy virtual. Copy its data
    // components explicitly, never the form/vtable or ownership-bearing bytes.
    // Containers copy their storage; model components use the engine copier.
    template <class Addon, class CopyModel>
    void CopyAddonData(Addon& destination, Addon& source, CopyModel copyModel)
    {
        destination.race = source.race;
        destination.bipedModelData = source.bipedModelData;
        destination.data = source.data;
        for (std::size_t sex{}; sex < 2U; ++sex) {
            copyModel(destination.bipedModels[sex], source.bipedModels[sex]);
            copyModel(destination.bipedModel1stPersons[sex], source.bipedModel1stPersons[sex]);
            destination.skinTextures[sex] = source.skinTextures[sex];
            destination.skinTextureSwapLists[sex] = source.skinTextureSwapLists[sex];
        }
        destination.additionalRaces = source.additionalRaces;
        destination.footstepSet = source.footstepSet;
        destination.artObject = source.artObject;
    }

    template <class Addon, class SameModel>
    [[nodiscard]] bool SameAddonGeometry(const Addon& copy, const Addon& source,
        SameModel sameModel)
    {
        if (copy.race != source.race || copy.GetSlotMask() != source.GetSlotMask() ||
            copy.additionalRaces.size() != source.additionalRaces.size()) return false;
        for (decltype(source.additionalRaces.size()) i{}; i < source.additionalRaces.size(); ++i) {
            if (copy.additionalRaces[i] != source.additionalRaces[i]) return false;
        }
        for (std::size_t sex{}; sex < 2U; ++sex) {
            if (!sameModel(copy.bipedModels[sex], source.bipedModels[sex]) ||
                !sameModel(copy.bipedModel1stPersons[sex], source.bipedModel1stPersons[sex])) return false;
        }
        return copy.footstepSet == source.footstepSet && copy.artObject == source.artObject;
    }
}
