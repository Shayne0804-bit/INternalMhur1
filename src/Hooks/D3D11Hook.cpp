#include "D3D11Hook.h"
#include "GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"
#include "../Utils/Logger.h"
#include "../SDK/SDKESPFunctions.h"
#include "../Hacks/InGameModuleHacks.h"
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
                Logger::LogInfo("D3D11Hook: Device et context acquis");

                // Get game window - ImGuiMenu will handle WndProc hooking
                if (!g_GameWindow)
                {
                    DXGI_SWAP_CHAIN_DESC desc{};
                    pSwapChain->GetDesc(&desc);
                    g_GameWindow = desc.OutputWindow;
                    Logger::LogInfo("D3D11Hook: Game window detected");
                }
            }
        }

        // Initialiser ImGui au premier Present
        if (!g_ImGuiInit && g_Device && g_Context && g_GameWindow && pSwapChain)
        {
            if (ImGuiMenu::Initialize(pSwapChain, g_GameWindow))
            {
                g_ImGuiInit = true;
                Logger::LogInfo("D3D11Hook: ImGui menu initialized");
            }
            else
            {
                Logger::LogWarning("D3D11Hook: ImGui menu initialization failed - retry next frame");
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
                Logger::LogWarning("D3D11Hook: Exception during deproject test");
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
            Logger::LogWarning("D3D11Hook: Exception during aimbot execution");
        }

        // ===== RUN TELEPORT TO KOTA =====
        try
        {
            SDK_RunTeleportToKota();
        }
        catch (...)
        {
            Logger::LogWarning("D3D11Hook: Exception during teleport to kota execution");
        }

        // ===== RUN TRANSFORM INTO RANDOM ESP TARGET =====
        if (ImGuiMenu::g_Settings.EnableTransformIntoRandomESP)
        {
            try
            {
                // Debounce tracking - static across frames
                static bool lastTransformPressed = false;
                static auto lastTransformTime = std::chrono::high_resolution_clock::now();
                const int TRANSFORM_COOLDOWN_MS = 100;  // 100ms cooldown between presses

                bool shouldTransform = false;
                
                // Check keyboard hotkey
                if ((GetAsyncKeyState(ImGuiMenu::g_Settings.TransformIntoRandomESPKey.Keyboard) & 0x8000) != 0)
                {
                    shouldTransform = true;
                }
                
                // Check gamepad hotkeys (if no keyboard key pressed)
                if (!shouldTransform)
                {
                    XINPUT_STATE xInputState = {};
                    
                    // Check Xbox gamepad
                    if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                    {
                        if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.TransformIntoRandomESPKey.Xbox) != 0)
                        {
                            shouldTransform = true;
                        }
                        else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.TransformIntoRandomESPKey.PS4) != 0)
                        {
                            shouldTransform = true;
                        }
                    }
                }
                
                // Debounce: only execute when transitioning from not-pressed to pressed AND cooldown elapsed
                if (shouldTransform && !lastTransformPressed)
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTransformTime).count();
                    
                    if (elapsed >= TRANSFORM_COOLDOWN_MS)
                    {
                        // CHECK: Only execute in valid battle modes (2-9)
                        if (IsValidBattleMode())
                        {
                            InGameHack_TransformIntoRandomESP();
                            lastTransformTime = now;
                        }
                        else
                        {
                            Logger::LogWarning("[D3D11Hook] TransformIntoRandomESP hotkey pressed but not in valid battle mode");
                        }
                    }
                }
                
                // Update last pressed state
                lastTransformPressed = shouldTransform;
            }
            catch (...)
            {
                Logger::LogWarning("D3D11Hook: Exception during transform into random ESP execution");
            }
        }

        // ===== RUN DUPLICATE INTO IMITATION RANDOM ESP TARGET =====
        if (ImGuiMenu::g_Settings.EnableDuplicateIntoImitationRandomESP)
        {
            try
            {
                // Debounce tracking - static across frames
                static bool lastDuplicateImitationPressed = false;
                static auto lastDuplicateImitationTime = std::chrono::high_resolution_clock::now();
                const int DUPLICATE_IMITATION_COOLDOWN_MS = 100;  // 100ms cooldown between presses

                bool shouldDuplicateImitation = false;
                
                // Check keyboard hotkey
                if ((GetAsyncKeyState(ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.Keyboard) & 0x8000) != 0)
                {
                    shouldDuplicateImitation = true;
                }
                
                // Check gamepad hotkeys (if no keyboard key pressed)
                if (!shouldDuplicateImitation)
                {
                    XINPUT_STATE xInputState = {};
                    
                    // Check Xbox gamepad
                    if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                    {
                        if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.Xbox) != 0)
                        {
                            shouldDuplicateImitation = true;
                        }
                        else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.PS4) != 0)
                        {
                            shouldDuplicateImitation = true;
                        }
                    }
                }
                
                // Debounce: only execute when transitioning from not-pressed to pressed AND cooldown elapsed
                if (shouldDuplicateImitation && !lastDuplicateImitationPressed)
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDuplicateImitationTime).count();
                    
                    if (elapsed >= DUPLICATE_IMITATION_COOLDOWN_MS)
                    {
                        // CHECK: Only execute in valid battle modes (2-9)
                        if (IsValidBattleMode())
                        {
                            InGameHack_DuplicateIntoImitationRandomESP(0.0f, ImGuiMenu::g_Settings.DuplicateImitationLifeTime, ImGuiMenu::g_Settings.DuplicateIntoImitationCount);
                            lastDuplicateImitationTime = now;
                        }
                        else
                        {
                            Logger::LogWarning("[D3D11Hook] DuplicateIntoImitation hotkey pressed but not in valid battle mode");
                        }
                    }
                }
                
                // Update last pressed state
                lastDuplicateImitationPressed = shouldDuplicateImitation;
            }
            catch (...)
            {
                Logger::LogWarning("D3D11Hook: Exception during duplicate into imitation random ESP execution");
            }
        }

        // =====================================================================
        // HOTKEY: SET INVINCIBLE (F11 or B button)
        // =====================================================================
        try
        {
            static auto lastSetInvincibleTime = std::chrono::high_resolution_clock::now();
            static bool lastSetInvinciblePressed = false;
            const int SET_INVINCIBLE_COOLDOWN_MS = 500;  // 500ms cooldown

            bool shouldSetInvincible = false;

            // Check keyboard hotkey
            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.SetInvincibleKey.Keyboard) & 0x8000) != 0)
            {
                shouldSetInvincible = true;
            }

            // Check gamepad hotkeys (if no keyboard key pressed)
            if (!shouldSetInvincible)
            {
                XINPUT_STATE xInputState = {};

                // Check Xbox gamepad
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.SetInvincibleKey.Xbox) != 0)
                    {
                        shouldSetInvincible = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.SetInvincibleKey.PS4) != 0)
                    {
                        shouldSetInvincible = true;
                    }
                }
            }

            // Debounce: only execute when transitioning from not-pressed to pressed AND cooldown elapsed
            if (shouldSetInvincible && !lastSetInvinciblePressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSetInvincibleTime).count();

                if (elapsed >= SET_INVINCIBLE_COOLDOWN_MS)
                {
                    // CHECK: Only execute in valid battle modes (2-9)
                    if (IsValidBattleMode())
                    {
                        InGameHack_SetInvincible();
                        lastSetInvincibleTime = now;
                    }
                    else
                    {
                        Logger::LogWarning("[D3D11Hook] SetInvincible hotkey pressed but not in valid battle mode");
                    }
                }
            }

            // Update last pressed state
            lastSetInvinciblePressed = shouldSetInvincible;
        }
        catch (...)
        {
            Logger::LogWarning("D3D11Hook: Exception during set invincible execution");
        }

        // ===== REBUILD MYSELF HOTKEY ====
        // ===== REBUILD MYSELF HOTKEY =====
        try
        {
            static auto lastRebuildMyselfTime = std::chrono::high_resolution_clock::now();
            static bool lastRebuildMyselfPressed = false;
            const int REBUILD_MYSELF_COOLDOWN_MS = 200;  // 200ms cooldown

            bool shouldRebuildMyself = false;

            // Check keyboard hotkey
            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.RebuildMyselfKey.Keyboard) & 0x8000) != 0)
            {
                shouldRebuildMyself = true;
            }

            // Check gamepad hotkeys (if no keyboard key pressed)
            if (!shouldRebuildMyself)
            {
                XINPUT_STATE xInputState = {};

                // Check Xbox gamepad
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.RebuildMyselfKey.Xbox) != 0)
                    {
                        shouldRebuildMyself = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.RebuildMyselfKey.PS4) != 0)
                    {
                        shouldRebuildMyself = true;
                    }
                }
            }

            // Debounce: only execute when transitioning from not-pressed to pressed AND cooldown elapsed
            if (shouldRebuildMyself && !lastRebuildMyselfPressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRebuildMyselfTime).count();

                if (elapsed >= REBUILD_MYSELF_COOLDOWN_MS)
                {
                    // CHECK: Only execute in valid battle modes (2-9)
                    if (IsValidBattleMode())
                    {
                        InGameHack_RebuildMyself();
                        lastRebuildMyselfTime = now;
                    }
                    else
                    {
                        Logger::LogWarning("[D3D11Hook] RebuildMyself hotkey pressed but not in valid battle mode");
                    }
                }
            }

            // Update last pressed state
            lastRebuildMyselfPressed = shouldRebuildMyself;
        }
        catch (...)
        {
            Logger::LogWarning("D3D11Hook: Exception during rebuild myself hotkey");
        }

        // ===== RUN RELOAD ADJUST RATE BUFFS =====
        try
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
        catch (...)
        {
            Logger::LogWarning("D3D11Hook: Exception during reload adjust rate buff execution");
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
            Logger::LogWarning("D3D11Hook: Exception during training mode player configuration");
        }

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
                Logger::LogError("D3D11Hook: Exception during render");
            }
        }

        // Drawing is now done in ImGuiMenu::Render() at the right time (after NewFrame, before Render)

        HRESULT hr = g_OriginalPresent(pSwapChain, SyncInterval, Flags);

        g_InHookedPresent = false;
        return hr;
    }

    bool Initialize()
    {
        Logger::LogInfo("D3D11Hook: Installation du hook");

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
            Logger::LogError("D3D11Hook: D3D11CreateDeviceAndSwapChain failed");
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

        Logger::LogInfo("D3D11Hook: Present hook installed");

        swap->Release();
        context->Release();
        device->Release();

        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return true;
    }

    void Shutdown()
    {
        Logger::LogInfo("D3D11Hook: Shutdown");

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

        Logger::LogInfo("D3D11Hook: Shutdown complete");
    }

    ID3D11Device* GetDevice() { return g_Device; }
    ID3D11DeviceContext* GetContext() { return g_Context; }
    HWND GetGameWindow() { return g_GameWindow; }
    bool IsHookInstalled() { return g_PresentCalled.load(); }
}
