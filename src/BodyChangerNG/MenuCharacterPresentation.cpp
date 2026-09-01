#include "BodyChangerNG/MenuCharacterPresentation.h"

#include "BodyChangerNG/NativeImGuiHost.h"
#include "BodyChangerNG/SmoothCamIntegration.h"

#include <SKSE/Logger.h>

#include <imgui.h>

namespace
{
    // Skyrim's third-person shoulder pivot is not screen-space symmetric.
    // Keep the visually correct right framing and pull the left framing
    // inward so the character is not pushed against the left edge.
    constexpr auto kLeftCameraHorizontalOffset = 60.0F;
    constexpr auto kRightCameraHorizontalOffset = -78.0F;
    constexpr auto kCameraVerticalOffset = -30.0F;
    constexpr auto kCameraDistance = 150.0F;
    constexpr auto kMenuWorldFov = 90.0F;
    constexpr auto kPlayerPitch = 0.27F;
    constexpr auto kLeftFacingCorrection = 0.35F;
    constexpr auto kRightFacingCorrection = -0.35F;
    // The Tint tab and its detail popup intentionally share one close-up so
    // opening the picker cannot shift the character. A zero vertical offset
    // raises the character substantially from the former detail value (15).
    constexpr auto kTintCameraDistance = 58.0F;
    constexpr auto kTintWorldFov = 45.0F;
    constexpr auto kTintVerticalOffset = 0.0F;
    // Preserve approximately the same screen-space side placement while the
    // camera changes from distance 150/FOV 90 to distance 58/FOV 45. Without
    // this projection-aware reduction, zooming pushes the actor farther out.
    constexpr auto kTintCameraSideMultiplier = 0.16F;
    constexpr auto kTintPitchZoomOffset = 0.46F;
    constexpr auto kNormalPitchZoomOffset = 0.10F;
    constexpr auto kMouseRotationRadiansPerPixel = 0.003F;
    constexpr auto kMaxMouseRotationRadiansPerFrame = 0.060F;
    std::atomic_bool g_cameraUpdateQueued{};

    void QueueCameraUpdate() noexcept
    {
        if (g_cameraUpdateQueued.exchange(true, std::memory_order_acq_rel)) return;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                g_cameraUpdateQueued.store(false, std::memory_order_release);
                if (auto* camera = RE::PlayerCamera::GetSingleton()) camera->Update();
            });
            return;
        }
        g_cameraUpdateQueued.store(false, std::memory_order_release);
    }

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
        RE::NiPoint2 freeRotation{};
        float actorAngleX{};
        float actorAngleZ{};
        bool actorPitchModified{};
        float targetZoomOffset{};
        float pitchZoomOffset{};
        float worldFov{};
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
        state_->pitchZoomOffset = thirdPersonState->pitchZoomOffset;
        state_->worldFov = camera->GetRuntimeData2().worldFOV;
        state_->freeRotationEnabled = thirdPersonState->freeRotationEnabled;
        state_->toggleAnimCam = thirdPersonState->toggleAnimCam;
        state_->side = side;
        state_->active = true;
        state_->rotating = false;
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
        const auto horizontal = focus == State::TintFocus::tint ?
            normalHorizontal * kTintCameraSideMultiplier : normalHorizontal;
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
        camera->GetRuntimeData2().worldFOV = focus == State::TintFocus::tint ? kTintWorldFov : kMenuWorldFov;
        // Camera::Update can touch the active game render pipeline. Running it
        // inside the native ImGui PostDisplay pass made the whole UI change
        // tone for a frame on DLSS/post-processing setups when switching the
        // left/right presentation. Apply it on the game task instead.
        QueueCameraUpdate();
        presentedActor->Update3DPosition(true);
    }

    void Presentation::Restore()
    {
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
            thirdPersonState->pitchZoomOffset = state_->pitchZoomOffset;
            thirdPersonState->freeRotationEnabled = state_->freeRotationEnabled;
            thirdPersonState->toggleAnimCam = state_->toggleAnimCam;
        }
        for (const auto& [setting, originalValue] : state_->cameraSettings) {
            if (setting) setting->data.f = originalValue;
        }
        if (camera) {
            camera->GetRuntimeData2().worldFOV = state_->worldFov;
            QueueCameraUpdate();
        }
        state_->originalCameraState = nullptr;
        state_->presentedActorHandle.reset();
        state_->originalCameraTarget.reset();
        state_->desiredPosOffset = {};
        state_->cameraSettings = {};
        state_->headTrackingModified = false;
        state_->actorPitchModified = false;
        state_->side = CharacterPosition::disabled;
        state_->rotating = false;
        state_->tintFocus = State::TintFocus::uninitialized;
        state_->active = false;
        smoothcam::ReleaseCameraControl();
        SKSE::log::debug("Body Changer NG restored menu character presentation");
    }

    void Presentation::UpdateRotationInteraction()
    {
        if (!state_ || ImGui::GetCurrentContext() == nullptr) {
            native_ui::EndCharacterRotationUnpause();
            return;
        }
        if (!state_->active && state_->requestedSide != CharacterPosition::disabled) {
            auto requestedActor = state_->requestedActorHandle.get();
            Apply(state_->requestedSide, requestedActor.get());
        }
        if (!state_->active) {
            native_ui::EndCharacterRotationUnpause();
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
        if (!state_->rotating || io.MouseDelta.x == 0.0F) return;
        const auto delta = std::clamp(-io.MouseDelta.x * kMouseRotationRadiansPerPixel,
            -kMaxMouseRotationRadiansPerFrame, kMaxMouseRotationRadiansPerFrame);
        presentedActor->SetHeading(NormalizeAngle(presentedActor->data.angle.z + delta));
        thirdPersonState->freeRotation.x = NormalizeAngle(thirdPersonState->freeRotation.x - delta);
        presentedActor->Update3DPosition(true);
        QueueCameraUpdate();
    }
}
