#include "BodyChangerNG/NativeImGuiHost.h"

#include "BodyChangerNG/RuntimeLayout.h"
#include "BodyChangerNG/Settings.h"
#include "BodyChangerNG/TextInputFilter.h"
#include "BodyChangerNG/UI.h"

#include <SKSE/Logger.h>
#include <RE/C/ControlMap.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <vector>

// Dear ImGui intentionally keeps this declaration behind an #if 0 in the
// Win32 backend header so consumers that do not use Windows headers do not
// inherit them. This translation unit already uses HWND/WPARAM directly.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace RE
{
    class GFxCharEvent final : public GFxEvent
    {
    public:
        GFxCharEvent() = default;
        explicit GFxCharEvent(const std::uint32_t a_character) :
            GFxEvent(EventType::kCharEvent), wcharCode(a_character)
        {}

        std::uint32_t wcharCode{};
        std::uint32_t keyboardIndex{};
    };

    static_assert(sizeof(GFxCharEvent) == 0x0C);
}

namespace
{
    constexpr std::string_view kMenuName = "BodyChangerNGMenu";
    constexpr auto kFontPixelSize = 20.0F;
    constexpr auto kMinimumUiScale = 0.75F;
    constexpr auto kMaximumUiScale = 1.50F;

    bcn::native_ui::RenderCallback g_renderCallback{};
    std::atomic_bool g_registered{};
    std::atomic_bool g_hookInstalled{};
    std::atomic_bool g_open{};
    std::atomic_bool g_openRequested{};
    std::atomic_bool g_cursorShowPending{};
    std::atomic_bool g_escapeRequested{};
    std::atomic_int g_mouseWheelSteps{};
    std::atomic_int g_rightMouseState{ -1 };
    std::atomic_bool g_wantsTextInput{};
    std::mutex g_rendererLock;
    struct PendingTextInputKey
    {
        std::uint32_t scanCode{};
        bool down{};
    };
    std::mutex g_textInputKeyLock;
    std::vector<PendingTextInputKey> g_pendingTextInputKeys;
    ImGuiContext* g_context{};
    HWND g_window{};
    WNDPROC g_previousWindowProc{};
    bool g_win32Ready{};
    bool g_dx11Ready{};
    bool g_rendererReady{};
    ImGuiStyle g_baseStyle{};
    bool g_baseStyleReady{};
    bool g_skyrimTextInputAllowed{};
    float g_resolutionScale{ 1.0F };

    void ClearPendingTextInputKeys() noexcept
    {
        std::scoped_lock lock(g_textInputKeyLock);
        g_pendingTextInputKeys.clear();
    }

    [[nodiscard]] float ResolutionScaleForHeight(const float height) noexcept
    {
        if (height >= 2000.0F) return 1.50F;  // 4K-class vertical resolution
        if (height >= 1300.0F) return 1.25F;  // 1440p / 2K-class resolution
        return 1.0F;
    }

    [[nodiscard]] bool IsImGuiMouseMessage(const UINT message) noexcept
    {
        switch (message) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            return true;
        default:
            return false;
        }
    }

    LRESULT CALLBACK BodyChangerWindowProc(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
    {
        if (message == WM_KILLFOCUS ||
            (message == WM_ACTIVATEAPP && wParam == FALSE)) {
            g_wantsTextInput.store(false, std::memory_order_release);
            ClearPendingTextInputKeys();
            bcn::text_input::Reset();
        }
        // A native IMenu has no Scaleform movie that forwards mouse-button
        // events. Feed them directly to ImGui while this overlay is visible.
        // Horizontal wheel input has no meaning in this UI. Consuming it here
        // prevents touchpads and stale Shift modifier state from pushing
        // catalogs or combo popups sideways.
        if (g_open.load() && message == WM_MOUSEHWHEEL) return 0;
        if (g_open.load() && g_rendererReady && g_context && IsImGuiMouseMessage(message)) {
            const auto previous = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(g_context);
            ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
            ImGui::SetCurrentContext(previous);
            return 0;
        }
        return g_previousWindowProc ?
            CallWindowProc(g_previousWindowProc, window, message, wParam, lParam) :
            DefWindowProc(window, message, wParam, lParam);
    }

    [[nodiscard]] bool CurrentEditableTextInputActive()
    {
        auto* context = ImGui::GetCurrentContext();
        if (!context || !context->PlatformImeData.WantTextInput || context->ActiveId == 0) return false;
        const auto* state = ImGui::GetInputTextState(context->ActiveId);
        return state && (state->Flags & ImGuiInputTextFlags_ReadOnly) == 0;
    }

    void SyncTextInput(const bool afterWidgetDraw)
    {
        if (!g_context || !g_rendererReady) return;
        const auto wantsText = g_open.load() &&
            (afterWidgetDraw ? CurrentEditableTextInputActive() : ImGui::GetIO().WantTextInput);
        if (wantsText != g_skyrimTextInputAllowed) {
            if (!wantsText) {
                // A modifier can be pressed while an edit box owns Win32
                // keyboard input and released on the frame focus leaves it.
                // Clear ImGui's cached keyboard state so Shift cannot turn
                // subsequent vertical wheel input into horizontal scrolling.
                ImGui::GetIO().ClearInputKeys();
                ClearPendingTextInputKeys();
            }
            if (auto* controlMap = RE::ControlMap::GetSingleton()) {
                controlMap->AllowTextInput(wantsText);
                g_skyrimTextInputAllowed = wantsText;
            }
        }
        g_wantsTextInput.store(wantsText, std::memory_order_release);
    }

    [[nodiscard]] bool InstallWindowMessageHook(const HWND window)
    {
        if (!window || g_previousWindowProc) return g_previousWindowProc != nullptr;
        SetLastError(0);
        const auto previous = SetWindowLongPtr(window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&BodyChangerWindowProc));
        if (previous == 0 && GetLastError() != 0) {
            SKSE::log::warn("Body Changer NG could not install the native mouse-wheel fallback");
            return false;
        }
        g_previousWindowProc = reinterpret_cast<WNDPROC>(previous);
        return true;
    }

    void RemoveWindowMessageHook()
    {
        if (!g_window || !g_previousWindowProc) return;
        if (reinterpret_cast<WNDPROC>(GetWindowLongPtr(g_window, GWLP_WNDPROC)) == &BodyChangerWindowProc) {
            SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_previousWindowProc));
        }
        g_previousWindowProc = nullptr;
    }

    class ScopedContext final
    {
    public:
        explicit ScopedContext(ImGuiContext* context) noexcept : old_(ImGui::GetCurrentContext())
        {
            ImGui::SetCurrentContext(context);
        }

        ~ScopedContext()
        {
            ImGui::SetCurrentContext(old_);
        }

        ScopedContext(const ScopedContext&) = delete;
        ScopedContext& operator=(const ScopedContext&) = delete;

    private:
        ImGuiContext* old_{};
    };

    [[nodiscard]] ImGuiKey ToImGuiKey(const RE::GFxKey::Code key) noexcept
    {
        if (key >= RE::GFxKey::kA && key <= RE::GFxKey::kZ) {
            return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + static_cast<int>(key) - static_cast<int>(RE::GFxKey::kA));
        }
        if (key >= RE::GFxKey::kF1 && key <= RE::GFxKey::kF15) {
            return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_F1) + static_cast<int>(key) - static_cast<int>(RE::GFxKey::kF1));
        }
        if (key >= RE::GFxKey::kNum0 && key <= RE::GFxKey::kNum9) {
            return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + static_cast<int>(key) - static_cast<int>(RE::GFxKey::kNum0));
        }
        switch (key) {
        case RE::GFxKey::kAlt: return ImGuiMod_Alt;
        case RE::GFxKey::kControl: return ImGuiMod_Ctrl;
        case RE::GFxKey::kShift: return ImGuiMod_Shift;
        case RE::GFxKey::kTab: return ImGuiKey_Tab;
        case RE::GFxKey::kReturn: return ImGuiKey_Enter;
        case RE::GFxKey::kEscape: return ImGuiKey_Escape;
        case RE::GFxKey::kSpace: return ImGuiKey_Space;
        case RE::GFxKey::kCapsLock: return ImGuiKey_CapsLock;
        case RE::GFxKey::kComma: return ImGuiKey_Comma;
        case RE::GFxKey::kPeriod: return ImGuiKey_Period;
        case RE::GFxKey::kSlash: return ImGuiKey_Slash;
        case RE::GFxKey::kBackslash: return ImGuiKey_Backslash;
        case RE::GFxKey::kQuote: return ImGuiKey_Apostrophe;
        case RE::GFxKey::kBracketLeft: return ImGuiKey_LeftBracket;
        case RE::GFxKey::kBracketRight: return ImGuiKey_RightBracket;
        case RE::GFxKey::kEqual: return ImGuiKey_Equal;
        case RE::GFxKey::kMinus: return ImGuiKey_Minus;
        case RE::GFxKey::kKP_Multiply: return ImGuiKey_KeypadMultiply;
        case RE::GFxKey::kKP_Add: return ImGuiKey_KeypadAdd;
        case RE::GFxKey::kKP_Enter: return ImGuiKey_KeypadEnter;
        case RE::GFxKey::kKP_Subtract: return ImGuiKey_KeypadSubtract;
        case RE::GFxKey::kBackspace: return ImGuiKey_Backspace;
        case RE::GFxKey::kDelete: return ImGuiKey_Delete;
        case RE::GFxKey::kInsert: return ImGuiKey_Insert;
        case RE::GFxKey::kHome: return ImGuiKey_Home;
        case RE::GFxKey::kEnd: return ImGuiKey_End;
        case RE::GFxKey::kPageUp: return ImGuiKey_PageUp;
        case RE::GFxKey::kPageDown: return ImGuiKey_PageDown;
        case RE::GFxKey::kLeft: return ImGuiKey_LeftArrow;
        case RE::GFxKey::kRight: return ImGuiKey_RightArrow;
        case RE::GFxKey::kUp: return ImGuiKey_UpArrow;
        case RE::GFxKey::kDown: return ImGuiKey_DownArrow;
        default: return ImGuiKey_None;
        }
    }

    [[nodiscard]] constexpr ImGuiKey ScanCodeToImGuiKey(const std::uint32_t scanCode) noexcept
    {
        switch (scanCode) {
        case 0x01: return ImGuiKey_Escape;
        case 0x02: return ImGuiKey_1;
        case 0x03: return ImGuiKey_2;
        case 0x04: return ImGuiKey_3;
        case 0x05: return ImGuiKey_4;
        case 0x06: return ImGuiKey_5;
        case 0x07: return ImGuiKey_6;
        case 0x08: return ImGuiKey_7;
        case 0x09: return ImGuiKey_8;
        case 0x0A: return ImGuiKey_9;
        case 0x0B: return ImGuiKey_0;
        case 0x0C: return ImGuiKey_Minus;
        case 0x0D: return ImGuiKey_Equal;
        case 0x0E: return ImGuiKey_Backspace;
        case 0x0F: return ImGuiKey_Tab;
        case 0x10: return ImGuiKey_Q;
        case 0x11: return ImGuiKey_W;
        case 0x12: return ImGuiKey_E;
        case 0x13: return ImGuiKey_R;
        case 0x14: return ImGuiKey_T;
        case 0x15: return ImGuiKey_Y;
        case 0x16: return ImGuiKey_U;
        case 0x17: return ImGuiKey_I;
        case 0x18: return ImGuiKey_O;
        case 0x19: return ImGuiKey_P;
        case 0x1A: return ImGuiKey_LeftBracket;
        case 0x1B: return ImGuiKey_RightBracket;
        case 0x1C: return ImGuiKey_Enter;
        case 0x1D: return ImGuiKey_LeftCtrl;
        case 0x1E: return ImGuiKey_A;
        case 0x1F: return ImGuiKey_S;
        case 0x20: return ImGuiKey_D;
        case 0x21: return ImGuiKey_F;
        case 0x22: return ImGuiKey_G;
        case 0x23: return ImGuiKey_H;
        case 0x24: return ImGuiKey_J;
        case 0x25: return ImGuiKey_K;
        case 0x26: return ImGuiKey_L;
        case 0x27: return ImGuiKey_Semicolon;
        case 0x28: return ImGuiKey_Apostrophe;
        case 0x29: return ImGuiKey_GraveAccent;
        case 0x2A: return ImGuiKey_LeftShift;
        case 0x2B: return ImGuiKey_Backslash;
        case 0x2C: return ImGuiKey_Z;
        case 0x2D: return ImGuiKey_X;
        case 0x2E: return ImGuiKey_C;
        case 0x2F: return ImGuiKey_V;
        case 0x30: return ImGuiKey_B;
        case 0x31: return ImGuiKey_N;
        case 0x32: return ImGuiKey_M;
        case 0x33: return ImGuiKey_Comma;
        case 0x34: return ImGuiKey_Period;
        case 0x35: return ImGuiKey_Slash;
        case 0x36: return ImGuiKey_RightShift;
        case 0x38: return ImGuiKey_LeftAlt;
        case 0x39: return ImGuiKey_Space;
        case 0x3A: return ImGuiKey_CapsLock;
        case 0x9C: return ImGuiKey_KeypadEnter;
        case 0x9D: return ImGuiKey_RightCtrl;
        case 0xB8: return ImGuiKey_RightAlt;
        case 0xC7: return ImGuiKey_Home;
        case 0xC8: return ImGuiKey_UpArrow;
        case 0xC9: return ImGuiKey_PageUp;
        case 0xCB: return ImGuiKey_LeftArrow;
        case 0xCD: return ImGuiKey_RightArrow;
        case 0xCF: return ImGuiKey_End;
        case 0xD0: return ImGuiKey_DownArrow;
        case 0xD1: return ImGuiKey_PageDown;
        case 0xD2: return ImGuiKey_Insert;
        case 0xD3: return ImGuiKey_Delete;
        default: break;
        }
        if (scanCode >= 0x3B && scanCode <= 0x44) {
            return static_cast<ImGuiKey>(ImGuiKey_F1 + (scanCode - 0x3B));
        }
        if (scanCode == 0x57) return ImGuiKey_F11;
        if (scanCode == 0x58) return ImGuiKey_F12;
        return ImGuiKey_None;
    }

    static_assert(ScanCodeToImGuiKey(0x0E) == ImGuiKey_Backspace);
    static_assert(ScanCodeToImGuiKey(0xD3) == ImGuiKey_Delete);

    void DrainTextInputKeys(ImGuiIO& io)
    {
        std::vector<PendingTextInputKey> pending;
        {
            std::scoped_lock lock(g_textInputKeyLock);
            pending.swap(g_pendingTextInputKeys);
        }
        for (const auto& event : pending) {
            if (const auto key = ScanCodeToImGuiKey(event.scanCode); key != ImGuiKey_None) {
                io.AddKeyEvent(key, event.down);
            }
        }
    }

    void ApplyStyle()
    {
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 5.0F;
        style.FrameRounding = 4.0F;
        style.GrabRounding = 4.0F;
        style.ChildRounding = 4.0F;
        style.PopupRounding = 5.0F;
        style.WindowBorderSize = 1.0F;
        style.FrameBorderSize = 1.0F;
        style.FramePadding = ImVec2(9.0F, 5.0F);
        style.ItemSpacing = ImVec2(8.0F, 7.0F);

        auto* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.91F, 0.94F, 0.97F, 1.0F);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.58F, 0.65F, 0.70F, 1.0F);
        const ImVec4 panelBackground{ 0.10F, 0.14F, 0.17F, 1.0F };
        colors[ImGuiCol_WindowBg] = panelBackground;
        colors[ImGuiCol_ChildBg] = panelBackground;
        colors[ImGuiCol_PopupBg] = panelBackground;
        colors[ImGuiCol_Border] = ImVec4(0.29F, 0.37F, 0.43F, 1.0F);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16F, 0.22F, 0.27F, 1.0F);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.21F, 0.31F, 0.38F, 1.0F);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.24F, 0.39F, 0.49F, 1.0F);
        colors[ImGuiCol_TitleBg] = ImVec4(0.12F, 0.17F, 0.21F, 1.0F);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15F, 0.23F, 0.29F, 1.0F);
        colors[ImGuiCol_Button] = ImVec4(0.18F, 0.27F, 0.33F, 1.0F);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.24F, 0.39F, 0.49F, 1.0F);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.30F, 0.49F, 0.61F, 1.0F);
        colors[ImGuiCol_Header] = ImVec4(0.20F, 0.39F, 0.50F, 1.0F);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25F, 0.50F, 0.63F, 1.0F);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.29F, 0.58F, 0.72F, 1.0F);
        colors[ImGuiCol_CheckMark] = ImVec4(0.50F, 0.80F, 0.95F, 1.0F);
        colors[ImGuiCol_Separator] = ImVec4(0.29F, 0.37F, 0.43F, 1.0F);
        g_baseStyle = style;
        g_baseStyleReady = true;
    }

    void ApplyConfiguredScale()
    {
        if (!g_baseStyleReady) return;
        const auto settings = bcn::Settings::Get().Snapshot();
        auto& style = ImGui::GetStyle();
        style = g_baseStyle;
        const auto configuredScale = std::clamp(settings.textScale, kMinimumUiScale, kMaximumUiScale);
        style.ScaleAllSizes(g_resolutionScale * configuredScale);
        ImGui::GetIO().FontGlobalScale = configuredScale;
    }

    void MergeFont(ImGuiIO& io, const char* path, const ImWchar* range, const float pixelSize)
    {
        if (!std::filesystem::exists(path)) return;
        ImFontConfig config{};
        config.MergeMode = true;
        config.PixelSnapH = true;
        config.OversampleH = 1;
        config.OversampleV = 1;
        io.Fonts->AddFontFromFileTTF(path, pixelSize, &config, range);
    }

    [[nodiscard]] bool BuildFonts(ImGuiIO& io, const float pixelSize)
    {
        io.Fonts->Clear();
        ImFontConfig base{};
        base.OversampleH = 1;
        base.OversampleV = 1;
        base.PixelSnapH = true;
        if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(
                "C:/Windows/Fonts/segoeui.ttf", pixelSize, &base, io.Fonts->GetGlyphRangesDefault());
        }
        if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();
        MergeFont(io, "C:/Windows/Fonts/malgun.ttf", io.Fonts->GetGlyphRangesKorean(), pixelSize);
        MergeFont(io, "C:/Windows/Fonts/msyh.ttc", io.Fonts->GetGlyphRangesChineseSimplifiedCommon(), pixelSize);
        if (!io.Fonts->Build()) {
            SKSE::log::error("Body Changer NG could not build the ImGui font atlas");
            return false;
        }
        const auto* texture = io.Fonts->TexData;
        if (!texture || texture->Width <= 0 || texture->Height <= 0 ||
            texture->Width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
            texture->Height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
            SKSE::log::error("Body Changer NG rejected the ImGui font atlas dimensions");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool ValidateFontTexture(ID3D11Device* device, ImGuiIO& io)
    {
        unsigned char* pixels{};
        int width{};
        int height{};
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!device || !pixels || width <= 0 || height <= 0) return false;

        D3D11_TEXTURE2D_DESC description{};
        description.Width = static_cast<UINT>(width);
        description.Height = static_cast<UINT>(height);
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pixels;
        data.SysMemPitch = description.Width * 4U;
        ID3D11Texture2D* texture{};
        const auto result = device->CreateTexture2D(&description, &data, &texture);
        if (texture) texture->Release();
        return SUCCEEDED(result);
    }

    void ResetRenderer()
    {
        bcn::ui::OnClosed();
        g_cursorShowPending = false;
        g_escapeRequested = false;
        g_wantsTextInput = false;
        if (g_skyrimTextInputAllowed) {
            if (auto* controlMap = RE::ControlMap::GetSingleton()) controlMap->AllowTextInput(false);
            g_skyrimTextInputAllowed = false;
        }
        RemoveWindowMessageHook();
        const auto old = ImGui::GetCurrentContext();
        if (g_context) ImGui::SetCurrentContext(g_context);
        if (g_dx11Ready) ImGui_ImplDX11_Shutdown();
        if (g_win32Ready) ImGui_ImplWin32_Shutdown();
        g_dx11Ready = false;
        g_win32Ready = false;
        const auto destroyed = g_context;
        if (destroyed) ImGui::DestroyContext(destroyed);
        g_context = nullptr;
        ImGui::SetCurrentContext(old == destroyed ? nullptr : old);
        g_rendererReady = false;
        g_baseStyleReady = false;
        g_resolutionScale = 1.0F;
        g_window = nullptr;
    }

    [[nodiscard]] bool InitializeRenderer(IDXGISwapChain* swapChain, ID3D11Device* device,
        ID3D11DeviceContext* context)
    {
        std::scoped_lock lock(g_rendererLock);
        if (g_rendererReady) return true;
        if (!swapChain || !device || !context) return false;
        DXGI_SWAP_CHAIN_DESC description{};
        if (FAILED(swapChain->GetDesc(&description)) || !description.OutputWindow) return false;
        auto displayHeight = static_cast<float>(description.BufferDesc.Height);
        if (displayHeight <= 0.0F) {
            RECT client{};
            if (GetClientRect(description.OutputWindow, &client)) {
                displayHeight = static_cast<float>(client.bottom - client.top);
            }
        }
        g_resolutionScale = ResolutionScaleForHeight(displayHeight);

        IMGUI_CHECKVERSION();
        const auto old = ImGui::GetCurrentContext();
        g_context = ImGui::CreateContext();
        if (!g_context) return false;
        ImGui::SetCurrentContext(g_context);
        ApplyStyle();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        if (!BuildFonts(io, kFontPixelSize * g_resolutionScale) || !ValidateFontTexture(device, io)) {
            SKSE::log::error("Body Changer NG did not attach ImGui because D3D11 rejected its font texture");
            ResetRenderer();
            ImGui::SetCurrentContext(old);
            return false;
        }
        g_window = description.OutputWindow;
        g_win32Ready = ImGui_ImplWin32_Init(g_window);
        g_dx11Ready = g_win32Ready && ImGui_ImplDX11_Init(device, context);
        if (!g_win32Ready || !g_dx11Ready || !io.BackendPlatformUserData || !io.BackendRendererUserData) {
            ResetRenderer();
            ImGui::SetCurrentContext(old);
            return false;
        }
        g_rendererReady = true;
        if (!InstallWindowMessageHook(g_window)) {
            SKSE::log::warn("Body Changer NG will rely on Skyrim's normal input route for scrolling");
        }
        ImGui::SetCurrentContext(old);
        SKSE::log::info("Body Changer NG native ImGui D3D11 host initialized");
        return true;
    }

    void UpdateMousePosition()
    {
        auto& io = ImGui::GetIO();
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
            if (const auto* cursor = RE::MenuCursor::GetSingleton()) {
                io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
                io.AddMousePosEvent(cursor->cursorPosX, cursor->cursorPosY);
                return;
            }
        }
        POINT point{};
        if (g_window && GetCursorPos(&point) && ScreenToClient(g_window, &point)) {
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMousePosEvent(static_cast<float>(point.x), static_cast<float>(point.y));
        }
    }

    void FeedScaleformEvent(const RE::BSUIScaleformData* data)
    {
        if (!data || !data->scaleformEvent || !g_context || !g_rendererReady) return;
        ScopedContext context(g_context);
        auto& io = ImGui::GetIO();
        const auto* event = data->scaleformEvent;
        switch (event->type.get()) {
        case RE::GFxEvent::EventType::kMouseDown:
        case RE::GFxEvent::EventType::kMouseUp: {
            const auto* mouse = reinterpret_cast<const RE::GFxMouseEvent*>(event);
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMouseButtonEvent(static_cast<int>(mouse->button), event->type == RE::GFxEvent::EventType::kMouseDown);
            break;
        }
        case RE::GFxEvent::EventType::kMouseWheel: {
            const auto* mouse = reinterpret_cast<const RE::GFxMouseEvent*>(event);
            io.AddMouseWheelEvent(0.0F, mouse->scrollDelta);
            break;
        }
        case RE::GFxEvent::EventType::kKeyDown:
        case RE::GFxEvent::EventType::kKeyUp: {
            const auto* key = reinterpret_cast<const RE::GFxKeyEvent*>(event);
            const auto converted = ToImGuiKey(key->keyCode);
            if (converted != ImGuiKey_None) io.AddKeyEvent(converted, event->type == RE::GFxEvent::EventType::kKeyDown);
            break;
        }
        case RE::GFxEvent::EventType::kCharEvent: {
            const auto* character = reinterpret_cast<const RE::GFxCharEvent*>(event);
            if (g_wantsTextInput.load(std::memory_order_acquire) &&
                character->wcharCode >= 0x20U && character->wcharCode != 0x7FU) {
                io.AddInputCharacter(character->wcharCode);
            }
            break;
        }
        default:
            break;
        }
    }

    void AcquireCursor()
    {
        // CursorMenu is shared by Skyrim UI and other native ImGui mods.  A
        // different menu may hide it while Body Changer NG remains open, so
        // an internal "owned" bit is not enough: verify the real menu state
        // every frame, as Skyrim Fitting System does.
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
            g_cursorShowPending = false;
            return;
        }
        if (g_cursorShowPending.exchange(true)) return;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddUITask([] {
                g_cursorShowPending = false;
                if (!g_open) return;
                if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) return;
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                    queue->AddMessage(RE::CursorMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
                }
            });
        } else {
            g_cursorShowPending = false;
        }
    }

    class NativeMenu final : public RE::IMenu
    {
    public:
        void PostDisplay() override
        {
            if (!g_rendererReady || !g_context || !g_win32Ready || !g_dx11Ready) return;
            ScopedContext context(g_context);
            ImGui_ImplWin32_NewFrame();
            ImGui_ImplDX11_NewFrame();
            ApplyConfiguredScale();
            UpdateMousePosition();
            auto& io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            io.AddKeyEvent(ImGuiMod_Super,
                (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
            const auto wheel = g_mouseWheelSteps.exchange(0);
            if (wheel != 0) io.AddMouseWheelEvent(0.0F, static_cast<float>(wheel));
            const auto rightMouse = g_rightMouseState.exchange(-1);
            if (rightMouse >= 0) {
                io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
                io.AddMouseButtonEvent(ImGuiMouseButton_Right, rightMouse != 0);
            }
            DrainTextInputKeys(io);
            ImGui::NewFrame();
            SyncTextInput(false);
            if (g_renderCallback) g_renderCallback();
            // Match SFS: synchronize immediately after the text widget has
            // gained or released focus. Waiting until after Render() leaves
            // Skyrim's Type Mode one UI frame behind the visible caret.
            SyncTextInput(true);
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            AcquireCursor();
        }

        RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& message) override
        {
            switch (message.type.get()) {
            case RE::UI_MESSAGE_TYPE::kShow:
                g_open = true;
                g_openRequested = true;
                g_cursorShowPending = false;
                g_escapeRequested = false;
                bcn::ui::OnOpened();
                break;
            case RE::UI_MESSAGE_TYPE::kHide:
                bcn::ui::OnClosed();
                g_open = false;
                g_openRequested = false;
                g_cursorShowPending = false;
                g_escapeRequested = false;
                g_mouseWheelSteps = 0;
                g_rightMouseState = -1;
                g_wantsTextInput = false;
                ClearPendingTextInputKeys();
                bcn::text_input::Reset();
                if (g_skyrimTextInputAllowed) {
                    if (auto* controlMap = RE::ControlMap::GetSingleton()) controlMap->AllowTextInput(false);
                    g_skyrimTextInputAllowed = false;
                }
                if (g_context) {
                    ScopedContext context(g_context);
                    auto& io = ImGui::GetIO();
                    io.ClearInputKeys();
                    io.ClearInputMouse();
                }
                break;
            case RE::UI_MESSAGE_TYPE::kScaleformEvent:
                FeedScaleformEvent(reinterpret_cast<const RE::BSUIScaleformData*>(message.data));
                return RE::UI_MESSAGE_RESULTS::kHandled;
            default:
                break;
            }
            return IMenu::ProcessMessage(message);
        }

        static RE::IMenu* Creator()
        {
            using Flags = RE::UI_MENU_FLAGS;
            auto* menu = new NativeMenu();
            menu->menuFlags.set(Flags::kUpdateUsesCursor, Flags::kUsesCursor,
                Flags::kCustomRendering, Flags::kUsesMenuContext);
            if (bcn::Settings::Get().Snapshot().pauseGameWhenOpen) {
                menu->menuFlags.set(Flags::kPausesGame);
            }
            menu->depthPriority = 11;
            menu->inputContext.set(RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
            return menu;
        }
    };

    [[nodiscard]] bool RegisterNativeMenu()
    {
        if (g_registered) return true;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->Register(kMenuName, NativeMenu::Creator);
            g_registered = true;
            return true;
        }
        return false;
    }

    struct D3DInitHook
    {
        static void Thunk()
        {
            func();
            const auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            if (!renderer) return;
            const auto& state = renderer->GetRuntimeData();
            if (!state.renderWindows || !state.renderWindows->swapChain) return;
            auto* swapChain = reinterpret_cast<IDXGISwapChain*>(state.renderWindows->swapChain);
            auto* device = reinterpret_cast<ID3D11Device*>(state.forwarder);
            auto* context = reinterpret_cast<ID3D11DeviceContext*>(state.context);
            if (InitializeRenderer(swapChain, device, context)) {
                if (!RegisterNativeMenu()) SKSE::log::critical("Body Changer NG could not register its native menu");
            }
        }

        static inline REL::Relocation<decltype(Thunk)> func;
    };

    void QueueVisibility(const bool open)
    {
        g_openRequested = open;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddUITask([open] {
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                    queue->AddMessage(kMenuName, open ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide, nullptr);
                }
            });
        }
    }
}

namespace bcn::native_ui
{
    bool Register(const RenderCallback callback)
    {
        if (!callback) return false;
        g_renderCallback = callback;
        return true;
    }

    bool InstallRendererHook()
    {
        if (g_hookInstalled) return true;
        const auto version = REL::Module::get().version();
        const auto layout = runtime::ResolveRendererHook(version);
        if (!layout) {
            SKSE::log::critical("Body Changer NG native ImGui is disabled: Skyrim {} has no verified renderer hook layout", version.string());
            return false;
        }
        const auto callSite = REL::ID(layout->relocationID).address() + layout->callOffset;
        if (*reinterpret_cast<const std::uint8_t*>(callSite) != 0xE8) {
            SKSE::log::critical("Body Changer NG native ImGui is disabled: renderer hook signature mismatch");
            return false;
        }
        D3DInitHook::func = SKSE::GetTrampoline().write_call<5>(callSite, D3DInitHook::Thunk);
        g_hookInstalled = true;
        SKSE::log::info("Body Changer NG installed a verified renderer hook for Skyrim {}", version.string());
        return true;
    }

    bool Open()
    {
        if (!IsReady()) return false;
        // `g_openRequested` is a queued UI-thread request, not proof that the
        // native menu has actually been shown.  Retrying an open while that
        // request is still pending must remain an open operation.
        if (!g_open) QueueVisibility(true);
        return true;
    }

    bool Toggle()
    {
        if (!IsReady()) return false;
        // Only an actually visible menu may be closed by the shortcut.  Using
        // the queued-request flag here allowed a duplicate/late key event to
        // change a pending show into a hide before the UI thread drew a frame.
        // Multiple opening events now coalesce into harmless show requests.
        QueueVisibility(!g_open);
        return true;
    }

    bool Close()
    {
        if (!g_registered || (!g_open && !g_openRequested)) return false;
        QueueVisibility(false);
        return true;
    }

    void SubmitMouseWheel(const float delta) noexcept
    {
        if (delta > 0.0F) g_mouseWheelSteps.fetch_add(1);
        if (delta < 0.0F) g_mouseWheelSteps.fetch_sub(1);
    }

    void SubmitRightMouseButton(const bool down) noexcept
    {
        if (g_open) g_rightMouseState.store(down ? 1 : 0, std::memory_order_release);
    }

    void SubmitTextInputKey(const std::uint32_t scanCode, const bool down) noexcept
    {
        if (!g_open || !g_wantsTextInput.load(std::memory_order_acquire)) return;
        std::scoped_lock lock(g_textInputKeyLock);
        g_pendingTextInputKeys.push_back({ scanCode, down });
    }

    void SubmitEscape() noexcept
    {
        if (g_open) g_escapeRequested = true;
    }

    bool ConsumeEscape() noexcept
    {
        return g_escapeRequested.exchange(false);
    }

    float GetResolutionScale() noexcept
    {
        return g_resolutionScale;
    }

    bool IsOpen() noexcept
    {
        return g_open || g_openRequested;
    }

    bool IsReady() noexcept
    {
        return g_registered && g_rendererReady && g_context && g_win32Ready && g_dx11Ready;
    }

    bool WantsTextInput() noexcept
    {
        return g_wantsTextInput.load(std::memory_order_acquire);
    }
}
