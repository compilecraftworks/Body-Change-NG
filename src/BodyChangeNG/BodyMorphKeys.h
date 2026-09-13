#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bcn::racemenu::keys
{
    inline constexpr auto body = "BodyChangeNG";
    inline constexpr auto preview = "BodyChangeNGPreview";
    inline constexpr auto outfit = "BodyChangeNGOutfit";
    inline constexpr auto legacyBody = "BodyChangerNG";
    inline constexpr auto legacyPreview = "BodyChangerNGPreview";
    inline constexpr auto legacyOutfit = "BodyChangerNGOutfit";
    inline constexpr std::array owned{ body, preview, outfit, legacyBody, legacyPreview, legacyOutfit };

    [[nodiscard]] constexpr bool IsOwned(const std::string_view key) noexcept
    {
        for (const auto* candidate : owned) if (key == candidate) return true;
        return false;
    }

    template <class API, class Actor>
    void ClearOwned(API& api, Actor* actor)
    {
        for (const auto* key : owned) api.ClearBodyMorphKeys(actor, key);
    }

    // The destructive branch is used only for an accepted preset commit, not
    // preview, cancellation, Default, or outfit-only updates (OBody semantics).
    template <class API, class Actor>
    void BeginPresetCommit(API& api, Actor* actor, const bool preserveOtherMorphs)
    {
        if (preserveOtherMorphs) ClearOwned(api, actor);
        else api.ClearMorphs(actor);
    }

    // Preview cancels the values that a real commit would replace using one
    // temporary delta key. Foreign keys themselves are never changed. Preserve
    // the current BCNG clothing layer only when SFS cannot yet decide its
    // final render state; otherwise replace it with the newly planned refit.
    class PreviewBase final
    {
    public:
        explicit PreviewBase(const bool preserveOtherMorphs, const bool replaceOutfit = true) :
            preserve_(preserveOtherMorphs), replaceOutfit_(replaceOutfit) {}
        void Visit(const char* name, const char* key, const float value)
        {
            if (!name || !*name || !key) return;
            const std::string_view source(key);
            if (source == preview || source == legacyPreview) return;
            if (!replaceOutfit_ && (source == outfit || source == legacyOutfit)) return;
            if (!preserve_ || IsOwned(source)) values[name] += value;
        }
        std::unordered_map<std::string, float> values;
    private:
        bool preserve_;
        bool replaceOutfit_;
    };
}
