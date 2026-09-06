#pragma once

#include "BodyChangeNG/SkinProfiles.h"

#include <string_view>
#include <vector>

namespace bcn::skin_plan
{
    struct ActorContext final
    {
        bool elder{};
        bool vampire{};
        HumanoidSkinRace humanoidRace{ HumanoidSkinRace::generic };
        std::string_view faceDetailFilename;
    };

    struct ApplicationPlan final
    {
        SkinUvLayout layout{ SkinUvLayout::unknown };
        std::vector<SkinTextureLayer> body;
        std::vector<SkinTextureLayer> hands;
        std::vector<SkinTextureLayer> feet;
        std::vector<SkinTextureLayer> face;
        std::vector<SkinTextureLayer> cbbeGenitalAnal;
        std::vector<SkinTextureLayer> unpGenitalAnal;
        bool requiresFaceGeometry{};
        bool broadSharedAtlas{};
        bool beastTail{};
    };

    void OverlayLayers(
        std::vector<SkinTextureLayer>& base, const std::vector<SkinTextureLayer>& overlay);

    [[nodiscard]] ApplicationPlan Build(
        const SkinProfile& profile, const ActorContext& actor);
}
