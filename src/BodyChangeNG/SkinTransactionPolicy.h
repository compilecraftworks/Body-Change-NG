#pragma once

namespace bcn::skin_transaction
{
    enum class Mode { commit, preview, restore };
    // Direct choices may select a known pack layout when a standalone actor's
    // BodySlide family is unknown. Automatic selection must never guess it.
    enum class Selection { automatic, direct };

    [[nodiscard]] constexpr bool RecordsApplication(Mode mode) noexcept
    { return mode == Mode::commit || mode == Mode::restore; }

    // Unlike a new unloaded commit (selection intent only), cancelling a
    // preview must undo native forms that were already changed while loaded.
    [[nodiscard]] constexpr bool RestoresPreview(Mode mode) noexcept
    { return mode == Mode::restore; }

    [[nodiscard]] constexpr bool PersistsFace(bool player, Mode mode) noexcept
    { return player && RecordsApplication(mode); }

    // Restoration must visit every channel, even if one engine write fails.
    // Early-returning here would leave all later channels from the failed skin.
    template<class Rows, class Writer>
    bool RestoreRows(const Rows& rows, Writer write)
    {
        bool restored = true;
        for (decltype(rows.size()) row{}; row < rows.size(); ++row)
            for (decltype(rows[row].size()) channel{}; channel < rows[row].size(); ++channel)
                restored = write(row, channel, rows[row][channel]) && restored;
        return restored;
    }

    template<class Bindings, class Writer>
    bool RestoreOriginalTextures(Bindings& bindings, Writer write)
    {
        bool restored = true;
        for (auto& binding : bindings)
            for (decltype(binding.originalPaths.size()) channel{}; channel < binding.originalPaths.size(); ++channel)
                restored = write(binding, channel, binding.originalPaths[channel]) && restored;
        return restored;
    }
}
