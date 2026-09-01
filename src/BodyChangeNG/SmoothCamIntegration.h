#pragma once

namespace bcn::smoothcam
{
    // SmoothCam is optional. These functions use its public SKSE messaging API
    // only when it is already present, so Body Change NG has no DLL dependency.
    void RegisterInterfaceListener();
    void RequestInterface();
    [[nodiscard]] bool AcquireCameraControl();
    void ReleaseCameraControl();
}
