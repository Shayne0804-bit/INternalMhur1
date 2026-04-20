#include "D3D11Hook.h"
#include "../Menu/ImGuiMenu.h"
#include "../Utils/Logger.h"
#include "../SDK/SDKESPFunctions.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <mutex>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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
