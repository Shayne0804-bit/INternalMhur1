#include "D3D11Hook.h"
#include "GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"

#include "../SDK/SDKESPFunctions.h"
#include "../Hacks/InGameModuleHacks.h"
#include "../Hacks/HackThread.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <Xinput.h>
#include <mutex>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "xinput.lib")

// Aimbot function declaration
extern "C" void SDK_RunAimbot();
extern "C" void SDK_RunSilentAim();
extern "C" void SDK_RunTeleportToKota();

namespace D3D11Hook
{
    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_Context = nullptr;
    static bool g_ImGuiInit = false;
    static bool g_SDKLogged = false;
    static std::atomic<bool> g_PresentCalled(false);

    static PresentFn g_OriginalPresent = nullptr;
    static thread_local bool g_InHookedPresent = false;

    static HWND g_GameWindow = nullptr;

    static HRESULT WINAPI HookedPresent(
        IDXGISwapChain* pSwapChain,
        UINT SyncInterval,
        UINT Flags)
    {
        if (g_InHookedPresent)
            return g_OriginalPresent(pSwapChain, SyncInterval, Flags);

        g_InHookedPresent = true;
        g_PresentCalled.store(true, std::memory_order_relaxed);

        if (!pSwapChain)
        {
            g_InHookedPresent = false;
            return g_OriginalPresent(pSwapChain, SyncInterval, Flags);
        }

        // Récupérer le device et context lors du premier appel
        if (!g_Device)
        {
            if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_Device))))
            {
                g_Device->GetImmediateContext(&g_Context);
// Get game window - ImGuiMenu will handle WndProc hooking
                if (!g_GameWindow)
                {
                    DXGI_SWAP_CHAIN_DESC desc{};
                    pSwapChain->GetDesc(&desc);
                    g_GameWindow = desc.OutputWindow;
}
            }
        }

        // Initialiser ImGui au premier Present
        if (!g_ImGuiInit && g_Device && g_Context && g_GameWindow && pSwapChain)
        {
            if (ImGuiMenu::Initialize(pSwapChain, g_GameWindow))
            {
                g_ImGuiInit = true;
}
            else
            {
}
        }

        // Test DeprojectScreenToWorld conversion per frame - MUST BE BEFORE ImguiMenu::Render!
        // Run every frame to draw boxes
        static int deprojectCounter = 0;
        deprojectCounter++;
        if (deprojectCounter >= 1)  // Run every frame
        {
            deprojectCounter = 0;
            try
            {
                SDK_TestDeprojectScreenToWorld();
            }
            catch (...)
            {
}
        }

        // ===== RUN AIMBOT EVERY FRAME =====
        // This is called after actor cache is updated, so targets are current
        try
        {
            SDK_RunAimbot();
        }
        catch (...)
        {
}

        // ===== FRAME UPDATE HACKS (via HackThread) =====
        // This calls all continuous frame-based hacks like bullet redirection
        try
        {
            HackThreadManager::GetInstance().FrameUpdateHacks();
        }
        catch (...)
        {
}

        // ===== RUN TELEPORT TO KOTA =====
        try
        {
            SDK_RunTeleportToKota();
        }
        catch (...)
        {
}

        // ===== RUN TRANSFORM INTO RANDOM ESP TARGET =====
        // NOTE: InGameHack_TransformIntoRandomESP is now called from HackThread.cpp::FrameUpdateHacks()

        // ===== RUN DUPLICATE INTO IMITATION RANDOM ESP TARGET =====
        // NOTE: InGameHack_DuplicateIntoImitationRandomESP is now called from HackThread.cpp::FrameUpdateHacks()

        // =====================================================================
        // HOTKEY: SET INVINCIBLE (F11 or B button)
        // =====================================================================
        // NOTE: InGameHack_SetInvincible is now called from HackThread.cpp::FrameUpdateHacks()

        // ===== REBUILD MYSELF HOTKEY =====
        // NOTE: InGameHack_RebuildMyself is now called from HackThread.cpp::FrameUpdateHacks()
        // via EnqueueHack() to avoid duplicate calls and maintain consistency

        // ===== RUN CH202 INIT TRANSMISSION LEVEL 5 AUTO-APPLY =====
        try
        {
            // Auto-apply CH202 init trans level 5 once if enabled
            if (ImGuiMenu::g_Settings.EnableCH202InitTransLevel5)
            {
                if (IsValidBattleMode())
                {
                    InGameHack_SetInitTransMissionLevel(0, 5);
                    // Disable after execution to prevent re-execution
                    ImGuiMenu::g_Settings.EnableCH202InitTransLevel5 = false;
                }
            }
        }
        catch (...)
        {
}

        // ===== RUN RELOAD ADJUST RATE BUFFS =====
        try
        {
            // Only apply reload buffs in valid battle modes
            if (IsValidBattleMode())
            {
                // Apply general reload adjust rate
                if (ImGuiMenu::g_Settings.ReloadAdjustRate != 1.0f)
                {
                    InGameHack_SetReloadAdjustRate(ImGuiMenu::g_Settings.ReloadAdjustRate);
                }
                
                // Apply reload adjust rate for roll slot
                if (ImGuiMenu::g_Settings.ReloadAdjustRate_RollSlot != 1.0f)
                {
                    InGameHack_SetReloadAdjustRate_RollSlot(ImGuiMenu::g_Settings.ReloadAdjustRate_RollSlot);
                }
                
                // Apply reload adjust rate for blue flame
                if (ImGuiMenu::g_Settings.ReloadAdjustRate_WearBlueFlame != 1.0f)
                {
                    InGameHack_SetReloadAdjustRate_WearBlueFlame(ImGuiMenu::g_Settings.ReloadAdjustRate_WearBlueFlame);
                }
            }
        }
        catch (...)
        {
}

        // ===== TRAINING MODE - PLAYER CHARACTER CONFIGURATION =====
        try
        {
            // Get player controller
            SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
            if (playerController)
            {
                // Find and cast to UTrainingMenuWidget to configure training player
                // This needs to be called when "Apply Player Configuration" button is pressed
                // For now, we just ensure the settings are stored for training mode
                
                // Note: To actually apply the training player config, you need to call:
                // trainingWidget->OnSetPlayerCharacter(
                //     (ECharacterId)g_Settings.TrainingPlayerCharacter,
                //     g_Settings.TrainingPlayerUnique1,
                //     g_Settings.TrainingPlayerUnique2,
                //     g_Settings.TrainingPlayerUnique3,
                //     g_Settings.TrainingPlayerSkillCode);
            }
        }
        catch (...)
        {
}

        // NOTE: Recovery functions (RecoverMe, RecoverTeam, RecoverAllESP) are now called
        // from HackThread.cpp::FrameUpdateHacks() via EnqueueHack() to avoid duplicate calls

        // ===== RUN SILENT AIM (BULLET TP) - DISABLED =====
        // BulletTP has been disabled
        // try
        // {
        //     SDK_RunSilentAim();
        // }
        // catch (...)
        // {
        //     Logger::LogWarning("D3D11Hook: Exception during silent aim execution");
        // }

        // Render ImGui (now boxes are queued and will be drawn)
        if (g_ImGuiInit)
        {
            try
            {
                ImGuiMenu::Render(pSwapChain);
            }
            catch (...)
            {
}
        }

        // Drawing is now done in ImGuiMenu::Render() at the right time (after NewFrame, before Render)

        HRESULT hr = g_OriginalPresent(pSwapChain, SyncInterval, Flags);

        g_InHookedPresent = false;
        return hr;
    }

    bool Initialize()
    {
// Créer une fenêtre temporaire
        WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, DefWindowProcW, 0L, 0L,
            GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"D3D11Hook_Tmp", nullptr };

        RegisterClassExW(&wc);

        HWND hwnd = CreateWindowW(
            wc.lpszClassName, L"",
            WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100,
            nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swap = nullptr;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &sd,
            &swap,
            &device,
            nullptr,
            &context);

        if (FAILED(hr))
        {
DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Hooker la vtable du swapchain
        void** vtable = *reinterpret_cast<void***>(swap);
        g_OriginalPresent = reinterpret_cast<PresentFn>(vtable[8]);

        DWORD old;
        VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        vtable[8] = reinterpret_cast<void*>(&HookedPresent);
        VirtualProtect(&vtable[8], sizeof(void*), old, &old);
swap->Release();
        context->Release();
        device->Release();

        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return true;
    }

    void Shutdown()
    {
if (g_ImGuiInit)
        {
            ImGuiMenu::Shutdown();
            g_ImGuiInit = false;
        }

        if (g_Context)
        {
            g_Context->Release();
            g_Context = nullptr;
        }
        if (g_Device)
        {
            g_Device->Release();
            g_Device = nullptr;
        }

        g_OriginalPresent = nullptr;
}

    ID3D11Device* GetDevice() { return g_Device; }
    ID3D11DeviceContext* GetContext() { return g_Context; }
    HWND GetGameWindow() { return g_GameWindow; }
    bool IsHookInstalled() { return g_PresentCalled.load(); }
}
