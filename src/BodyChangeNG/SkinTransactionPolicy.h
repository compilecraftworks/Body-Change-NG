#pragma once

namespace bcn::skin_transaction
{
    enum class Mode { commit, preview };

    [[nodiscard]] constexpr bool RecordsApplication(Mode mode) noexcept
    { return mode == Mode::commit; }

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
}
