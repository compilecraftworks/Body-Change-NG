#include "BodyChangerNG/MenuCharacterPresentation.h"

#include "BodyChangerNG/NativeImGuiHost.h"
#include "BodyChangerNG/SmoothCamIntegration.h"

#include <SKSE/Logger.h>

namespace RE
{
    class ShadowSceneNode;
}

#include <RE/D/DrawWorld.h>

#include <imgui.h>

#include <cmath>

namespace
{
    constexpr auto kLeftCameraHorizontalOffset = 70.0F;
    constexpr auto kRightCameraHorizontalOffset = -70.0F;
    constexpr auto kCameraVerticalOffset = -45.0F;
    constexpr auto kCameraDistance = 140.0F;
    constexpr auto kMenuWorldFov = 70.0F;
    constexpr auto kPlayerPitch = 0.0F;
    constexpr auto kLeftFacingCorrection = 0.35F;
    constexpr auto kRightFacingCorrection = -0.35F;
    // The Tint tab and its detail popup intentionally share one close-up so
    // opening the picker cannot shift the character. A zero vertical offset
    // raises the character substantially from the former detail value (15).
    constexpr auto kTintCameraDistance = 60.0F;
    constexpr auto kTintWorldFov = 45.0F;
    constexpr auto kTintVerticalOffset = 0.0F;
    // Projection-matched to the normal 140/FOV70/+-70 framing. This keeps the
    // actor at the same screen-space side position while the face zooms in.
    constexpr auto kLeftTintCameraHorizontalOffset = 18.0F;
    constexpr auto kRightTintCameraHorizontalOffset = -18.0F;
    constexpr auto kTintPitchZoomOffset = 0.46F;
    constexpr auto kNormalPitchZoomOffset = 0.0F;
    constexpr auto kMouseRotationRadiansPerPixel = 0.003F;
    constexpr auto kMaxMouseRotationRadiansPerFrame = 0.060F;
    constexpr auto kCameraRootPositionSettleEpsilon = 0.05F;
    constexpr auto kCameraRootRotationSettleEpsilon = 0.0005F;
    constexpr auto kCameraZoomSettleEpsilon = 0.01F;
    constexpr std::uint8_t kCameraSettleStableFrames = 3U;
    constexpr std::uint8_t kCameraSettleMaxFrames = 30U;

    enum class CameraZoomUpdate : std::uint8_t
    {
        refresh,
        snapCurrentToTarget,
        restoreSaved
    };

    std::atomic_bool g_cameraUpdateQueued{};
    std::atomic<CameraZoomUpdate> g_pendingCameraZoomUpdate{ CameraZoomUpdate::refresh };
    std::atomic<float> g_savedTargetZoom{};
    std::atomic<float> g_savedCurrentZoom{};
    std::atomic_uint64_t g_nextCameraCompletionRequest{};
    std::atomic_uint64_t g_pendingCameraCompletionRequest{};
    std::atomic_uint64_t g_completedCameraCompletionRequest{};

    [[nodiscard]] float NormalizeAngle(float angle)
    {
        constexpr auto twoPi = std::numbers::pi_v<float> * 2.0F;
        while (angle > std::numbers::pi_v<float>) angle -= twoPi;
        while (angle < -std::numbers::pi_v<float>) angle += twoPi;
        return angle;
    }

    [[nodiscard]] float VectorLength(const RE::NiPoint3& vector)
    {
        return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    }

    [[nodiscard]] float MatrixMaxDelta(const RE::NiMatrix3& left, const RE::NiMatrix3& right)
    {
        float maximum{};
        for (std::size_t row{}; row < 3U; ++row) {
            for (std::size_t column{}; column < 3U; ++column) {
                maximum = (std::max)(maximum,
                    std::abs(left.entry[row][column] - right.entry[row][column]));
            }
        }
        return maximum;
    }

    void ApplyPresentationWorldFov(RE::PlayerCamera* camera, const float worldFov)
    {
        if (camera) camera->GetRuntimeData2().worldFOV = worldFov;
        // PlayerCamera is the gameplay-side source. DrawWorld is consumed when
        // the render camera updates its projection; a pausing menu can freeze
        // that value before it observes PlayerCamera's new FOV.
        RE::DrawWorld::GetSingleton().worldFOV = worldFov;
    }

    [[nodiscard]] RE::NiPoint3 ProjectVector(const RE::NiPoint3& vector, const RE::NiPoint3& axis)
    {
        const auto denominator = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (denominator <= 0.0001F) return {};
        const auto scale = (vector.x * axis.x + vector.y * axis.y + vector.z * axis.z) / denominator;
        return axis * scale;
    }

    [[nodiscard]] RE::NiPoint2 RotateVector(const RE::NiPoint2& vector, const float angle)
    {
        const auto sine = std::sin(angle);
        const auto cosine = std::cos(angle);
        return { vector.x * cosine - vector.y * sine, vector.x * sine + vector.y * cosine };
    }

    [[nodiscard]] float GetAngle(const RE::NiPoint2& from, const RE::NiPoint2& to)
    {
        return std::atan2(from.x * to.y - from.y * to.x, from.x * to.x + from.y * to.y);
    }

    [[nodiscard]] float GetCameraAlignedActorYaw(RE::Actor* actor, RE::PlayerCamera* camera)
    {
        if (!actor || !camera || !camera->cameraRoot) return actor ? actor->data.angle.z : 0.0F;
        const auto actorPosition = actor->GetPosition();
        const auto cameraPosition = camera->cameraRoot->world.translate;
        const auto targetPosition = actor->GetLookingAtLocation();
        auto actorDirectionToTarget = targetPosition - actorPosition;
        if (VectorLength(actorDirectionToTarget) <= 0.0001F) return actor->data.angle.z;
        actorDirectionToTarget.Unitize();
        const auto cameraToActor = actorPosition - cameraPosition;
        const auto projected = ProjectVector(cameraToActor, actorDirectionToTarget);
        const auto projectedPosition = cameraPosition + projected;
        auto projectedDirectionToTarget = targetPosition - projectedPosition;
        if (VectorLength(projectedDirectionToTarget) <= 0.0001F) return actor->data.angle.z;
        projectedDirectionToTarget.Unitize();
        const auto currentDirection = RotateVector({ 0.0F, 1.0F }, actor->data.angle.z);
        const RE::NiPoint2 projectedDirection{ -projectedDirectionToTarget.x, projectedDirectionToTarget.y };
        return NormalizeAngle(actor->data.angle.z + GetAngle(currentDirection, projectedDirection));
    }

    [[nodiscard]] RE::ThirdPersonState* GetThirdPersonState(RE::PlayerCamera* camera)
    {
        // SFS itself is SE/AE-only. The menu camera presentation deliberately
        // fails closed on VR until its separate camera-state layout has been
        // validated; the rest of Body Changer NG remains usable on VR.
        if (!camera || REL::Module::IsVR()) return nullptr;
        const auto& state = camera->GetRuntimeData().cameraStates[RE::CameraState::kThirdPerson];
        return state ? static_cast<RE::ThirdPersonState*>(state.get()) : nullptr;
    }

    std::uint64_t QueueCameraUpdate(const CameraZoomUpdate zoomUpdate = CameraZoomUpdate::refresh,
        const float savedTargetZoom = 0.0F, const float savedCurrentZoom = 0.0F,
        const bool trackCompletion = false) noexcept
    {
        const auto completionRequest = trackCompletion ?
            g_nextCameraCompletionRequest.fetch_add(1U, std::memory_order_acq_rel) + 1U : 0U;
        if (completionRequest != 0U) {
            g_pendingCameraCompletionRequest.store(completionRequest, std::memory_order_release);
        }
        if (zoomUpdate == CameraZoomUpdate::restoreSaved) {
            g_savedTargetZoom.store(savedTargetZoom, std::memory_order_release);
            g_savedCurrentZoom.store(savedCurrentZoom, std::memory_order_release);
        }
        // Rotation-only refreshes must not erase a pending zoom snap/restore.
        // A newer substantive request represents a newer menu state and wins.
        if (zoomUpdate != CameraZoomUpdate::refresh) {
            g_pendingCameraZoomUpdate.store(zoomUpdate, std::memory_order_release);
        }
        if (g_cameraUpdateQueued.exchange(true, std::memory_order_acq_rel)) return completionRequest;

        const auto update = [] {
            g_cameraUpdateQueued.store(false, std::memory_order_release);
            const auto completionRequest =
                g_pendingCameraCompletionRequest.exchange(0U, std::memory_order_acq_rel);
            const auto complete = [completionRequest] {
                if (completionRequest != 0U) {
                    g_completedCameraCompletionRequest.store(completionRequest, std::memory_order_release);
                }
            };
            const auto requestedZoomUpdate =
                g_pendingCameraZoomUpdate.exchange(CameraZoomUpdate::refresh, std::memory_order_acq_rel);
            auto* camera = RE::PlayerCamera::GetSingleton();
            if (!camera) {
                complete();
                return;
            }

            // Let Skyrim derive targetZoomOffset from the temporary distance
            // first. A paused menu otherwise freezes interpolation at the old
            // currentZoomOffset and frames differently from an unpaused menu.
            camera->Update();
            auto* thirdPersonState = GetThirdPersonState(camera);
            if (!thirdPersonState) {
                complete();
                return;
            }
            if (requestedZoomUpdate == CameraZoomUpdate::snapCurrentToTarget) {
                thirdPersonState->currentZoomOffset = thirdPersonState->targetZoomOffset;
                camera->Update();
            } else if (requestedZoomUpdate == CameraZoomUpdate::restoreSaved) {
                // Reassert both original values after refresh so closing the
                // menu cannot retain either Body Changer NG zoom value.
                thirdPersonState->targetZoomOffset = g_savedTargetZoom.load(std::memory_order_acquire);
                thirdPersonState->currentZoomOffset = g_savedCurrentZoom.load(std::memory_order_acquire);
            }
            complete();
        };
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask(update);
            return completionRequest;
        }
        update();
        return completionRequest;
    }

    [[nodiscard]] bool CameraUpdateCompleted(const std::uint64_t request) noexcept
    {
        return request != 0U &&
            g_completedCameraCompletionRequest.load(std::memory_order_acquire) >= request;
    }

    void SetCameraHandle(RE::ThirdPersonState* state, RE::RefHandle& handle)
    {
        if (!state || REL::Module::IsVR()) return;
        // Verified CommonLibSSE-NG slots: SE/AE 0x09; VR has the extra
        // TESCameraState slot at 0x0A. Do not emit an unconditional virtual call.
        using SetCameraHandle = void (RE::ThirdPersonState::*)(RE::RefHandle&);
        REL::RelocateVirtual<SetCameraHandle>(0x09, 0x0A, state, handle);
    }

    [[nodiscard]] bool CanPresentActor(RE::PlayerCharacter* player, RE::Actor* actor,
        RE::PlayerCamera* camera, RE::ThirdPersonState* thirdPersonState)
    {
        if (REL::Module::IsVR() || !player || !actor || !camera || !thirdPersonState || !player->Is3DLoaded() ||
            !actor->Is3DLoaded() || player->IsOnMount() || camera->IsInFreeCameraMode()) {
            return false;
        }
        const auto* actorState = player->AsActorState();
        if (!actorState || actorState->GetSitSleepState() >= RE::SIT_SLEEP_STATE::kWantToSit) return false;
        return camera->currentState.get() == thirdPersonState;
    }
}

namespace bcn::menu_character
{
    struct Presentation::State
    {
        enum class TintFocus : std::uint8_t
        {
            uninitialized,
            normal,
            tint
        };

        struct SavedSetting
        {
            RE::Setting* setting{};
            float originalValue{};
        };

        bool active{};
        bool rotating{};
        bool cameraPausePending{};
        bool cameraPauseDeferred{};
        bool cameraPauseVerifyPending{};
        bool hasCameraSettleSample{};
        std::uint8_t cameraSettleFrames{};
        std::uint8_t cameraSettleStableFrames{};
        std::uint64_t cameraSettleCompletionRequest{};
        TintFocus tintFocus{ TintFocus::uninitialized };
        CharacterPosition side{ CharacterPosition::disabled };
        CharacterPosition requestedSide{ CharacterPosition::disabled };
        RE::ActorHandle presentedActorHandle{};
        RE::ActorHandle requestedActorHandle{};
        RE::ActorHandle originalCameraTarget{};
        RE::TESCameraState* originalCameraState{};
        RE::NiPoint3 posOffsetExpected{};
        RE::NiPoint3 posOffsetActual{};
        RE::NiPoint3 desiredPosOffset{};
        RE::NiPoint3 lastCameraRootTranslation{};
        RE::NiMatrix3 lastCameraRootRotation{};
        float lastCameraSettleZoomOffset{};
        RE::NiPoint3 settledCameraRootTranslation{};
        RE::NiMatrix3 settledCameraRootRotation{};
        RE::NiPoint3 settledStateTranslation{};
        float settledCurrentZoomOffset{};
        float settledTargetZoomOffset{};
        RE::NiPoint2 freeRotation{};
        float actorAngleX{};
        float actorAngleZ{};
        bool actorPitchModified{};
        float targetZoomOffset{};
        float currentZoomOffset{};
        float pitchZoomOffset{};
        float worldFov{};
        float drawWorldFov{};
        bool hasDrawWorldFov{};
        bool freeRotationEnabled{};
        bool toggleAnimCam{};
        bool headTrackingEnabled{};
        bool headTrackingModified{};
        std::array<SavedSetting, 9> cameraSettings{};
    };

    Presentation& Presentation::Get()
    {
        static Presentation presentation;
        static State state;
        presentation.state_ = std::addressof(state);
        return presentation;
    }

    void Presentation::Apply(const CharacterPosition side, RE::Actor* actor)
    {
        if (!state_) return;
        if (side == CharacterPosition::disabled) {
            Restore();
            return;
        }
        // A stale temporary FormID must not silently turn into the player.
        // The UI owns the selected actor and will explicitly select Player
        // when that reference expires.
        if (!actor) {
            Restore();
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* presentedActor = actor;
        const auto requestedHandle = presentedActor ? presentedActor->GetHandle() : RE::ActorHandle{};
        if (state_->active) {
            if (state_->side == side && state_->presentedActorHandle == requestedHandle) {
                state_->requestedSide = side;
                state_->requestedActorHandle = requestedHandle;
                return;
            }
            Restore();
        }
        state_->requestedSide = side;
        state_->requestedActorHandle = requestedHandle;

        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* thirdPersonState = GetThirdPersonState(camera);
        if (!CanPresentActor(player, presentedActor, camera, thirdPersonState)) {
            SKSE::log::debug("Body Changer NG deferred character presentation: actor/camera state is not ready");
            return;
        }
        if (!smoothcam::AcquireCameraControl()) return;

        state_->originalCameraState = camera->currentState.get();
        state_->originalCameraTarget = camera->cameraTarget;
        state_->presentedActorHandle = requestedHandle;
        state_->posOffsetExpected = thirdPersonState->posOffsetExpected;
        state_->posOffsetActual = thirdPersonState->posOffsetActual;
        state_->freeRotation = thirdPersonState->freeRotation;
        state_->actorAngleX = presentedActor->data.angle.x;
        state_->actorAngleZ = presentedActor->data.angle.z;
        state_->actorPitchModified = presentedActor == player;
        state_->targetZoomOffset = thirdPersonState->targetZoomOffset;
        state_->currentZoomOffset = thirdPersonState->currentZoomOffset;
        state_->pitchZoomOffset = thirdPersonState->pitchZoomOffset;
        state_->worldFov = camera->GetRuntimeData2().worldFOV;
        state_->drawWorldFov = RE::DrawWorld::GetSingleton().worldFOV;
        state_->hasDrawWorldFov = true;
        state_->freeRotationEnabled = thirdPersonState->freeRotationEnabled;
        state_->toggleAnimCam = thirdPersonState->toggleAnimCam;
        state_->side = side;
        state_->active = true;
        state_->rotating = false;
        state_->cameraPausePending = false;
        state_->cameraPauseDeferred = false;
        state_->cameraPauseVerifyPending = false;
        state_->hasCameraSettleSample = false;
        state_->cameraSettleFrames = 0U;
        state_->cameraSettleStableFrames = 0U;
        state_->cameraSettleCompletionRequest = 0U;
        // Force the normal framing branch below on the first activation.
        state_->tintFocus = State::TintFocus::uninitialized;

        camera->cameraTarget = requestedHandle;
        auto cameraTargetHandle = requestedHandle.native_handle();
        SetCameraHandle(thirdPersonState, cameraTargetHandle);
        camera->SetState(thirdPersonState);
        thirdPersonState->freeRotationEnabled = true;
        thirdPersonState->toggleAnimCam = true;

        if (presentedActor == player && player->GetGraphVariableBool("IsNPC", state_->headTrackingEnabled)) {
            player->SetGraphVariableBool("IsNPC", false);
            state_->headTrackingModified = true;
        }

        const auto correction = side == CharacterPosition::left ? kLeftFacingCorrection : kRightFacingCorrection;
        const auto angleChange = std::numbers::pi_v<float> + correction;
        presentedActor->SetHeading(NormalizeAngle(GetCameraAlignedActorYaw(presentedActor, camera) - angleChange));
        if (state_->actorPitchModified) presentedActor->data.angle.x = kPlayerPitch;
        thirdPersonState->freeRotation = { NormalizeAngle(angleChange), 0.0F };
        SetTintFocus(false);
        // ProcessMessage(kShow) precedes Skyrim's pause-count accounting. Do
        // not decrement here: a non-zero value could belong to another menu.
        // The first ordinary ImGui frame acquires our contribution instead.
        state_->cameraPausePending = native_ui::IsCameraPresentationPauseConfigured();
        SKSE::log::debug("Body Changer NG applied menu character presentation to {:08X}", presentedActor->GetFormID());
    }

    void Presentation::SetTintFocus(const bool tintTab)
    {
        if (!state_ || !state_->active) return;
        const auto focus = tintTab ? State::TintFocus::tint : State::TintFocus::normal;
        if (state_->tintFocus == focus) return;
        state_->tintFocus = focus;
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* thirdPersonState = GetThirdPersonState(camera);
        auto presentedActor = state_->presentedActorHandle.get();
        if (!camera || !thirdPersonState || !presentedActor) return;

        const auto normalHorizontal = state_->side == CharacterPosition::left ? kLeftCameraHorizontalOffset : kRightCameraHorizontalOffset;
        const auto tintHorizontal = state_->side == CharacterPosition::left ?
            kLeftTintCameraHorizontalOffset : kRightTintCameraHorizontalOffset;
        const auto horizontal = focus == State::TintFocus::tint ? tintHorizontal : normalHorizontal;
        const auto vertical = focus == State::TintFocus::tint ? kTintVerticalOffset : kCameraVerticalOffset;
        const auto distance = focus == State::TintFocus::tint ? kTintCameraDistance : kCameraDistance;
        state_->desiredPosOffset = { horizontal, 0.0F, vertical };
        if (auto* ini = RE::INISettingCollection::GetSingleton()) {
            const std::array settings{
                std::pair{ "fOverShoulderCombatPosX:Camera", horizontal },
                std::pair{ "fOverShoulderCombatAddY:Camera", 0.0F },
                std::pair{ "fOverShoulderCombatPosZ:Camera", vertical },
                std::pair{ "fOverShoulderPosX:Camera", horizontal },
                std::pair{ "fOverShoulderPosZ:Camera", vertical },
                std::pair{ "fAutoVanityModeDelay:Camera", 10800.0F },
                std::pair{ "fVanityModeMinDist:Camera", distance },
                std::pair{ "fVanityModeMaxDist:Camera", distance },
                std::pair{ "fMouseWheelZoomSpeed:Camera", 10000.0F }
            };
            for (std::size_t index{}; index < settings.size(); ++index) {
                if (auto* setting = ini->GetSetting(settings[index].first)) {
                    if (!state_->cameraSettings[index].setting) state_->cameraSettings[index] = { setting, setting->GetFloat() };
                    setting->data.f = settings[index].second;
                }
            }
        }
        thirdPersonState->posOffsetExpected = state_->desiredPosOffset;
        thirdPersonState->posOffsetActual = state_->desiredPosOffset;
        thirdPersonState->pitchZoomOffset = focus == State::TintFocus::tint ? kTintPitchZoomOffset : kNormalPitchZoomOffset;
        ApplyPresentationWorldFov(camera,
            focus == State::TintFocus::tint ? kTintWorldFov : kMenuWorldFov);
        // Camera::Update can touch the active game render pipeline. Running it
        // inside the native ImGui PostDisplay pass made the whole UI change
        // tone for a frame on DLSS/post-processing setups when switching the
        // left/right presentation. Apply it on the game task instead.
        QueueCameraUpdate(CameraZoomUpdate::snapCurrentToTarget);
        presentedActor->Update3DPosition(true);
    }

    void Presentation::Restore()
    {
        native_ui::EndCameraPresentationUnpause();
        native_ui::EndCharacterRotationUnpause();
        if (!state_) return;
        state_->requestedSide = CharacterPosition::disabled;
        state_->requestedActorHandle.reset();
        if (!state_->active) {
            smoothcam::ReleaseCameraControl();
            return;
        }
        auto presentedActor = state_->presentedActorHandle.get();
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* thirdPersonState = GetThirdPersonState(camera);
        if (camera) camera->cameraTarget = state_->originalCameraTarget;
        if (thirdPersonState) {
            auto originalHandle = state_->originalCameraTarget.native_handle();
            SetCameraHandle(thirdPersonState, originalHandle);
        }
        if (camera && state_->originalCameraState && camera->currentState.get() != state_->originalCameraState) {
            camera->SetState(state_->originalCameraState);
        }
        if (presentedActor) {
            if (state_->actorPitchModified) presentedActor->data.angle.x = state_->actorAngleX;
            presentedActor->SetHeading(state_->actorAngleZ);
            if (state_->headTrackingModified) {
                presentedActor->SetGraphVariableBool("IsNPC", state_->headTrackingEnabled);
            }
            if (presentedActor->Is3DLoaded()) presentedActor->Update3DPosition(true);
        }
        if (thirdPersonState) {
            thirdPersonState->posOffsetExpected = state_->posOffsetExpected;
            thirdPersonState->posOffsetActual = state_->posOffsetActual;
            thirdPersonState->freeRotation = state_->freeRotation;
            thirdPersonState->targetZoomOffset = state_->targetZoomOffset;
            thirdPersonState->currentZoomOffset = state_->currentZoomOffset;
            thirdPersonState->pitchZoomOffset = state_->pitchZoomOffset;
            thirdPersonState->freeRotationEnabled = state_->freeRotationEnabled;
            thirdPersonState->toggleAnimCam = state_->toggleAnimCam;
        }
        for (const auto& [setting, originalValue] : state_->cameraSettings) {
            if (setting) setting->data.f = originalValue;
        }
        if (state_->hasDrawWorldFov) {
            RE::DrawWorld::GetSingleton().worldFOV = state_->drawWorldFov;
        }
        if (camera) {
            camera->GetRuntimeData2().worldFOV = state_->worldFov;
            QueueCameraUpdate(CameraZoomUpdate::restoreSaved,
                state_->targetZoomOffset, state_->currentZoomOffset);
        }
        state_->originalCameraState = nullptr;
        state_->presentedActorHandle.reset();
        state_->originalCameraTarget.reset();
        state_->desiredPosOffset = {};
        state_->lastCameraRootTranslation = {};
        state_->lastCameraRootRotation = {};
        state_->lastCameraSettleZoomOffset = 0.0F;
        state_->settledCameraRootTranslation = {};
        state_->settledCameraRootRotation = {};
        state_->settledStateTranslation = {};
        state_->settledCurrentZoomOffset = 0.0F;
        state_->settledTargetZoomOffset = 0.0F;
        state_->cameraSettings = {};
        state_->headTrackingModified = false;
        state_->actorPitchModified = false;
        state_->side = CharacterPosition::disabled;
        state_->rotating = false;
        state_->cameraPausePending = false;
        state_->cameraPauseDeferred = false;
        state_->cameraPauseVerifyPending = false;
        state_->hasCameraSettleSample = false;
        state_->cameraSettleFrames = 0U;
        state_->cameraSettleStableFrames = 0U;
        state_->cameraSettleCompletionRequest = 0U;
        state_->drawWorldFov = 0.0F;
        state_->hasDrawWorldFov = false;
        state_->tintFocus = State::TintFocus::uninitialized;
        state_->active = false;
        smoothcam::ReleaseCameraControl();
        SKSE::log::debug("Body Changer NG restored menu character presentation");
    }

    void Presentation::UpdateRotationInteraction()
    {
        if (!state_ || ImGui::GetCurrentContext() == nullptr) {
            if (state_) {
                state_->cameraPausePending = false;
                state_->cameraPauseDeferred = false;
                state_->cameraPauseVerifyPending = false;
                state_->hasCameraSettleSample = false;
                state_->cameraSettleFrames = 0U;
                state_->cameraSettleStableFrames = 0U;
                state_->cameraSettleCompletionRequest = 0U;
                state_->rotating = false;
            }
            native_ui::EndCameraPresentationUnpause();
            native_ui::EndCharacterRotationUnpause();
            return;
        }
        if (!state_->active && state_->requestedSide != CharacterPosition::disabled) {
            auto requestedActor = state_->requestedActorHandle.get();
            Apply(state_->requestedSide, requestedActor.get());
        }
        if (!state_->active) {
            native_ui::EndCameraPresentationUnpause();
            native_ui::EndCharacterRotationUnpause();
            return;
        }
        if (state_->cameraPauseVerifyPending) {
            auto* camera = RE::PlayerCamera::GetSingleton();
            auto* thirdPersonState = GetThirdPersonState(camera);
            if (camera && camera->cameraRoot && thirdPersonState) {
                const auto rootPositionDelta = VectorLength(
                    camera->cameraRoot->world.translate - state_->settledCameraRootTranslation);
                const auto rootRotationDelta = MatrixMaxDelta(
                    camera->cameraRoot->world.rotate, state_->settledCameraRootRotation);
                const auto stateTranslationDelta = VectorLength(
                    thirdPersonState->translation - state_->settledStateTranslation);
                SKSE::log::info(
                    "Post-pause Body Changer NG camera delta: rootPos={:.4f}, rootRot={:.6f}, "
                    "stateTranslation={:.4f}, zoom={:.4f}->{:.4f} (settled {:.4f}->{:.4f}), "
                    "playerFov={:.2f}, drawWorldFov={:.2f}",
                    rootPositionDelta, rootRotationDelta, stateTranslationDelta,
                    thirdPersonState->currentZoomOffset, thirdPersonState->targetZoomOffset,
                    state_->settledCurrentZoomOffset, state_->settledTargetZoomOffset,
                    camera->GetRuntimeData2().worldFOV, RE::DrawWorld::GetSingleton().worldFOV);
            }
            state_->cameraPauseVerifyPending = false;
        }
        if (state_->cameraPausePending) {
            if (!native_ui::IsCameraPresentationPauseConfigured()) {
                state_->cameraPausePending = false;
            } else if (native_ui::BeginCameraPresentationUnpause()) {
                state_->cameraPausePending = false;
                state_->cameraPauseDeferred = true;
                state_->cameraPauseVerifyPending = false;
                state_->hasCameraSettleSample = false;
                state_->cameraSettleFrames = 0U;
                state_->cameraSettleStableFrames = 0U;
                state_->cameraSettleCompletionRequest = 0U;
                SKSE::log::info("Released Body Changer NG pause contribution after menu show");
            } else {
                // Skyrim has not accounted for this menu yet. Retry without
                // borrowing a pause contribution during the next UI frame.
                return;
            }
        }
        if (state_->cameraPauseDeferred) {
            auto presentedActor = state_->presentedActorHandle.get();
            auto* camera = RE::PlayerCamera::GetSingleton();
            auto* thirdPersonState = GetThirdPersonState(camera);
            if (!presentedActor || !camera || !thirdPersonState || camera->currentState.get() != thirdPersonState) {
                state_->cameraPauseDeferred = false;
                state_->cameraPauseVerifyPending = false;
                state_->hasCameraSettleSample = false;
                state_->cameraSettleFrames = 0U;
                state_->cameraSettleStableFrames = 0U;
                state_->cameraSettleCompletionRequest = 0U;
                native_ui::EndCameraPresentationUnpause();
                return;
            }
            if (state_->cameraSettleCompletionRequest != 0U) {
                if (!CameraUpdateCompleted(state_->cameraSettleCompletionRequest)) return;
                state_->cameraSettleCompletionRequest = 0U;

                ++state_->cameraSettleFrames;
                const bool hasCameraRoot = camera->cameraRoot != nullptr;
                if (state_->hasCameraSettleSample && hasCameraRoot) {
                    const auto rootPositionDelta = VectorLength(
                        camera->cameraRoot->world.translate - state_->lastCameraRootTranslation);
                    const auto rootRotationDelta = MatrixMaxDelta(
                        camera->cameraRoot->world.rotate, state_->lastCameraRootRotation);
                    const auto zoomDelta = std::abs(
                        thirdPersonState->currentZoomOffset - state_->lastCameraSettleZoomOffset);
                    const auto targetZoomDelta = std::abs(
                        thirdPersonState->currentZoomOffset - thirdPersonState->targetZoomOffset);
                    if (rootPositionDelta <= kCameraRootPositionSettleEpsilon &&
                        rootRotationDelta <= kCameraRootRotationSettleEpsilon &&
                        zoomDelta <= kCameraZoomSettleEpsilon &&
                        targetZoomDelta <= kCameraZoomSettleEpsilon) {
                        ++state_->cameraSettleStableFrames;
                    } else {
                        state_->cameraSettleStableFrames = 0U;
                    }
                } else if (hasCameraRoot) {
                    state_->hasCameraSettleSample = true;
                }
                if (hasCameraRoot) {
                    state_->lastCameraRootTranslation = camera->cameraRoot->world.translate;
                    state_->lastCameraRootRotation = camera->cameraRoot->world.rotate;
                    state_->lastCameraSettleZoomOffset = thirdPersonState->currentZoomOffset;
                }

                if (state_->cameraSettleStableFrames >= kCameraSettleStableFrames ||
                    state_->cameraSettleFrames >= kCameraSettleMaxFrames) {
                    const auto settledFrames = state_->cameraSettleFrames;
                    if (hasCameraRoot) {
                        state_->settledCameraRootTranslation = camera->cameraRoot->world.translate;
                        state_->settledCameraRootRotation = camera->cameraRoot->world.rotate;
                    }
                    state_->settledStateTranslation = thirdPersonState->translation;
                    state_->settledCurrentZoomOffset = thirdPersonState->currentZoomOffset;
                    state_->settledTargetZoomOffset = thirdPersonState->targetZoomOffset;
                    state_->cameraPauseDeferred = false;
                    state_->cameraPauseVerifyPending = true;
                    state_->hasCameraSettleSample = false;
                    state_->cameraSettleFrames = 0U;
                    state_->cameraSettleStableFrames = 0U;
                    native_ui::EndCameraPresentationUnpause();
                    SKSE::log::info(
                        "Settled Body Changer NG menu camera before pausing in {} frame(s)", settledFrames);
                    return;
                }
            }

            thirdPersonState->posOffsetExpected = state_->desiredPosOffset;
            thirdPersonState->posOffsetActual = state_->desiredPosOffset;
            if (camera->cameraTarget != state_->presentedActorHandle) {
                camera->cameraTarget = state_->presentedActorHandle;
                auto handle = state_->presentedActorHandle.native_handle();
                SetCameraHandle(thirdPersonState, handle);
            }
            ApplyPresentationWorldFov(camera, state_->tintFocus == State::TintFocus::tint ?
                kTintWorldFov : kMenuWorldFov);
            // Camera::Update stays on the game task so DLSS/post-processing
            // cannot tint the native ImGui pass. Sample only after that exact
            // snap/update request has completed.
            state_->cameraSettleCompletionRequest = QueueCameraUpdate(
                CameraZoomUpdate::snapCurrentToTarget, 0.0F, 0.0F, true);
            return;
        }
        auto& io = ImGui::GetIO();
        const auto popupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
        const auto overUi = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const auto characterSide = state_->side == CharacterPosition::right ? io.MousePos.x >= io.DisplaySize.x * 0.55F :
            io.MousePos.x <= io.DisplaySize.x * 0.45F;
        if (state_->rotating && (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || popupOpen || io.AppFocusLost)) {
            state_->rotating = false;
            native_ui::EndCharacterRotationUnpause();
        }
        if (!state_->rotating && !popupOpen && !overUi && characterSide && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            state_->rotating = true;
            static_cast<void>(native_ui::BeginCharacterRotationUnpause());
        }
        auto presentedActor = state_->presentedActorHandle.get();
        auto* camera = RE::PlayerCamera::GetSingleton();
        auto* thirdPersonState = GetThirdPersonState(camera);
        if (!presentedActor || !camera || !thirdPersonState || camera->currentState.get() != thirdPersonState) {
            state_->rotating = false;
            native_ui::EndCharacterRotationUnpause();
            return;
        }
        thirdPersonState->posOffsetExpected = state_->desiredPosOffset;
        thirdPersonState->posOffsetActual = state_->desiredPosOffset;
        if (camera->cameraTarget != state_->presentedActorHandle) {
            camera->cameraTarget = state_->presentedActorHandle;
            auto handle = state_->presentedActorHandle.native_handle();
            SetCameraHandle(thirdPersonState, handle);
        }
        ApplyPresentationWorldFov(camera, state_->tintFocus == State::TintFocus::tint ?
            kTintWorldFov : kMenuWorldFov);
        if (!state_->rotating || io.MouseDelta.x == 0.0F) return;
        const auto delta = std::clamp(-io.MouseDelta.x * kMouseRotationRadiansPerPixel,
            -kMaxMouseRotationRadiansPerFrame, kMaxMouseRotationRadiansPerFrame);
        presentedActor->SetHeading(NormalizeAngle(presentedActor->data.angle.z + delta));
        thirdPersonState->freeRotation.x = NormalizeAngle(thirdPersonState->freeRotation.x - delta);
        presentedActor->Update3DPosition(true);
        QueueCameraUpdate();
    }
}
