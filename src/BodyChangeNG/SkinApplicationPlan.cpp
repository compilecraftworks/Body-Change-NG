#include "BodyChangeNG/SkinApplicationPlan.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

namespace
{
    constexpr std::uint8_t kFaceDetailTextureIndex = 3U;

    [[nodiscard]] std::string LowerFilename(const std::string_view path)
    {
        auto filename = std::filesystem::path{ path }.filename().string();
        std::ranges::transform(filename, filename.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return filename;
    }

    [[nodiscard]] std::optional<bcn::SkinTextureLayer> MatchingFaceDetail(
        const bcn::SkinProfile& profile, const std::string_view currentFilename)
    {
        if (profile.faceDetails.empty()) return std::nullopt;
        const auto normalizedCurrent = LowerFilename(currentFilename);
        if (!normalizedCurrent.empty()) {
            const auto exact = std::ranges::find_if(
                profile.faceDetails, [&](const bcn::SkinTextureLayer& layer) {
                    return LowerFilename(layer.path) == normalizedCurrent;
                });
            if (exact != profile.faceDetails.end()) return *exact;

            for (const auto token : { std::string_view{ "frek" },
                     std::string_view{ "rough" }, std::string_view{ "blank" } }) {
                if (!normalizedCurrent.contains(token)) continue;
                const auto semantic = std::ranges::find_if(
                    profile.faceDetails, [&](const bcn::SkinTextureLayer& layer) {
                        return LowerFilename(layer.path).contains(token);
                    });
                if (semantic != profile.faceDetails.end()) return *semantic;
            }
        }

        // One file is unambiguous. Multiple unmatched alternatives must leave
        // the actor's current FaceGen detail channel untouched.
        return profile.faceDetails.size() == 1U ?
            std::optional{ profile.faceDetails.front() } : std::nullopt;
    }
}

namespace bcn::skin_plan
{
    void OverlayLayers(std::vector<SkinTextureLayer>& base,
        const std::vector<SkinTextureLayer>& overlay)
    {
        for (const auto& layer : overlay) {
            const auto existing = std::ranges::find(
                base, layer.shaderTextureIndex, &SkinTextureLayer::shaderTextureIndex);
            if (existing != base.end()) *existing = layer;
            else base.push_back(layer);
        }
        std::ranges::sort(base, {}, &SkinTextureLayer::shaderTextureIndex);
    }

    ApplicationPlan Build(const SkinProfile& profile, const ActorContext& actor)
    {
        ApplicationPlan plan;
        plan.layout = profile.uvLayout;
        plan.body = profile.body;
        plan.hands = profile.hands;
        plan.cbbeGenitalAnal = profile.cbbeGenitalAnal;
        plan.unpGenitalAnal = profile.unpGenitalAnal;
        plan.broadSharedAtlas = AllowsBroadSkinSlotFallback(profile.uvLayout);
        plan.beastTail = profile.race == SkinRace::argonian ||
            profile.race == SkinRace::khajiit;

        if (actor.elder) {
            OverlayLayers(plan.body, profile.elderBody);
            OverlayLayers(plan.hands, profile.elderHands);
        }

        switch (ResolveFeetLayerSource(profile.uvLayout, profile.race,
                    profile.body.size(), profile.feet.size())) {
        case FeetLayerSource::explicitFeet:
            plan.feet = profile.feet;
            break;
        case FeetLayerSource::bodyAtlas:
            plan.feet = plan.body;
            break;
        default:
            break;
        }

        if (plan.broadSharedAtlas) {
            plan.hands = plan.body;
            plan.feet = plan.body;
        }

        plan.face = profile.face;
        const auto raceIndex = static_cast<std::size_t>(actor.humanoidRace);
        if (raceIndex < profile.raceFace.size()) {
            OverlayLayers(plan.face, profile.raceFace[raceIndex]);
        }
        if (actor.vampire) OverlayLayers(plan.face, profile.vampireFace);
        if (actor.elder) OverlayLayers(plan.face, profile.elderFace);
        if (std::ranges::find(plan.face, kFaceDetailTextureIndex,
                &SkinTextureLayer::shaderTextureIndex) == plan.face.end()) {
            if (const auto detail = MatchingFaceDetail(
                    profile, actor.faceDetailFilename)) {
                plan.face.push_back(*detail);
            }
        }

        plan.requiresFaceGeometry = !profile.face.empty() ||
            !profile.faceDetails.empty() ||
            (actor.vampire && !profile.vampireFace.empty()) ||
            (actor.elder && !profile.elderFace.empty()) ||
            (raceIndex < profile.raceFace.size() && !profile.raceFace[raceIndex].empty());
        return plan;
    }
}
