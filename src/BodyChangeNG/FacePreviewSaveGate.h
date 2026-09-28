#pragma once

namespace bcn::face_preview
{
    struct SaveGate
    {
        bool suspended{}, serialized{};
        void Begin() noexcept { suspended = true; serialized = false; }
        void Serialized() noexcept { if (suspended) serialized = true; }
        bool ObserveIdle(bool saving, bool loading, bool workerBusy) noexcept
        {
            if (!suspended || !serialized || saving || loading || workerBusy) return false;
            suspended = serialized = false;
            return true;
        }
    };
}
