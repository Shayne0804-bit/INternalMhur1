#include "D3D11Hook.h"
#include "GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"

#include "../SDK/SDKESPFunctions.h"
#include "../Core/UnloadManager.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <Xinput.h>
#include <atomic>
#include <mutex>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "xinput.lib")

// Aimbot function declaration
extern "C" void SDK_RunAimbot();
extern "C" void SDK_RunSilentAim();
void InGameHack_RestoreInfiniteObjectsPatch();

namespace D3D11Hook
{
    typedef HRESULT(WINAPI* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_Context = nullptr;
    static bool g_ImGuiInit = false;
    static bool g_SDKLogged = false;
    static std::atomic<bool> g_PresentCalled(false);
    static std::atomic<int> g_PresentCallDepth(0);
    static std::atomic<int> g_ResizeBuffersCallDepth(0);
    static std::atomic_bool g_PresentHookRestored(false);
    static std::mutex g_HookStateMutex;

    static PresentFn g_OriginalPresent = nullptr;
    static ResizeBuffersFn g_OriginalResizeBuffers = nullptr;
    static void** g_PresentVTableSlot = nullptr;
    static void** g_ResizeBuffersVTableSlot = nullptr;
    static thread_local bool g_InHookedPresent = false;

    static HWND g_GameWindow = nullptr;

    static HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    static HRESULT WINAPI HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

    static void RestorePresentHook()
    {
        std::lock_guard<std::mutex> lock(g_HookStateMutex);

        if (!g_PresentVTableSlot && !g_ResizeBuffersVTableSlot)
        {
            g_PresentHookRestored.store(true, std::memory_order_release);
            return;
        }

        DWORD oldProtect = 0;
        if (g_PresentVTableSlot && g_OriginalPresent &&
            VirtualProtect(g_PresentVTableSlot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            if (*g_PresentVTableSlot == reinterpret_cast<void*>(&HookedPresent))
                *g_PresentVTableSlot = reinterpret_cast<void*>(g_OriginalPresent);

            DWORD unused = 0;
            VirtualProtect(g_PresentVTableSlot, sizeof(void*), oldProtect, &unused);
        }

        oldProtect = 0;
        if (g_ResizeBuffersVTableSlot && g_OriginalResizeBuffers &&
            VirtualProtect(g_ResizeBuffersVTableSlot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            if (*g_ResizeBuffersVTableSlot == reinterpret_cast<void*>(&HookedResizeBuffers))
                *g_ResizeBuffersVTableSlot = reinterpret_cast<void*>(g_OriginalResizeBuffers);

            DWORD unused = 0;
            VirtualProtect(g_ResizeBuffersVTableSlot, sizeof(void*), oldProtect, &unused);
        }

        g_PresentVTableSlot = nullptr;
        g_ResizeBuffersVTableSlot = nullptr;
        g_PresentHookRestored.store(true, std::memory_order_release);
    }

    static bool NeedsActorCache()
    {
        const auto& settings = ImGuiMenu::g_Settings;
        return settings.EnablePlayerESP ||
            settings.ShowPlayerSkeleton ||
            settings.EnableAimbot ||
            settings.EnableTeleportToKota ||
            settings.EnableBulletTP ||
            settings.EnableTeleportLevelUpCards ||
            settings.EnableTransformIntoRandomESP ||
            settings.EnableDuplicateIntoImitationRandomESP ||
            settings.EnableCopySkillsFromNearestEnemy;
    }

    static HRESULT WINAPI HookedPresent(
        IDXGISwapChain* pSwapChain,
        UINT SyncInterval,
        UINT Flags)
    {
        PresentFn originalPresent = g_OriginalPresent;
        if (!originalPresent)
            return DXGI_ERROR_INVALID_CALL;

        struct PresentCallScope
        {
            PresentCallScope()
            {
                g_PresentCallDepth.fetch_add(1, std::memory_order_acq_rel);
            }

            ~PresentCallScope()
            {
                g_PresentCallDepth.fetch_sub(1, std::memory_order_acq_rel);
            }
        } presentCallScope;

        if (UnloadManager::IsUnloadRequested())
        {
            RestorePresentHook();
            return originalPresent(pSwapChain, SyncInterval, Flags);
        }

        if (g_InHookedPresent)
            return originalPresent(pSwapChain, SyncInterval, Flags);

        struct PresentScope
        {
            PresentScope()
            {
                g_InHookedPresent = true;
            }

            ~PresentScope()
            {
                g_InHookedPresent = false;
            }
        } presentScope;

        g_PresentCalled.store(true, std::memory_order_relaxed);

        if (!pSwapChain)
            return originalPresent(pSwapChain, SyncInterval, Flags);

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

        if (ImGuiMenu::g_Settings.EnableGlobal)
        {
        // Update the actor cache only when a feature actually consumes it.
        if (NeedsActorCache())
        {
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
        if (ImGuiMenu::g_Settings.EnableAimbot)
        {
            try
            {
                SDK_RunAimbot();
            }
            catch (...)
            {
}
        }

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
}
        }

        // Drawing is now done in ImGuiMenu::Render() at the right time (after NewFrame, before Render)

        HRESULT hr = originalPresent(pSwapChain, SyncInterval, Flags);

        return hr;
    }

    static HRESULT WINAPI HookedResizeBuffers(
        IDXGISwapChain* pSwapChain,
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags)
    {
        ResizeBuffersFn originalResizeBuffers = g_OriginalResizeBuffers;
        if (!originalResizeBuffers)
            return DXGI_ERROR_INVALID_CALL;

        struct ResizeBuffersCallScope
        {
            ResizeBuffersCallScope()
            {
                g_ResizeBuffersCallDepth.fetch_add(1, std::memory_order_acq_rel);
            }

            ~ResizeBuffersCallScope()
            {
                g_ResizeBuffersCallDepth.fetch_sub(1, std::memory_order_acq_rel);
            }
        } resizeBuffersCallScope;

        ImGuiMenu::InvalidateRenderTarget();

        if (UnloadManager::IsUnloadRequested())
            RestorePresentHook();

        return originalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    bool Initialize()
    {
        if (UnloadManager::IsUnloadRequested())
            return false;

        if (g_OriginalPresent && g_PresentVTableSlot)
            return true;

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
        g_OriginalResizeBuffers = reinterpret_cast<ResizeBuffersFn>(vtable[13]);
        g_PresentVTableSlot = &vtable[8];
        g_ResizeBuffersVTableSlot = &vtable[13];
        g_PresentHookRestored.store(false, std::memory_order_release);

        DWORD old;
        VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        vtable[8] = reinterpret_cast<void*>(&HookedPresent);
        VirtualProtect(&vtable[8], sizeof(void*), old, &old);
        VirtualProtect(&vtable[13], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
        vtable[13] = reinterpret_cast<void*>(&HookedResizeBuffers);
        VirtualProtect(&vtable[13], sizeof(void*), old, &old);
swap->Release();
        context->Release();
        device->Release();

        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return true;
    }

    void Shutdown()
    {
        InGameHack_RestoreInfiniteObjectsPatch();
        RestorePresentHook();

        for (int i = 0; i < 200 && g_PresentCallDepth.load(std::memory_order_acquire) > 0; ++i)
            Sleep(1);

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
        g_OriginalResizeBuffers = nullptr;
        g_GameWindow = nullptr;
        g_PresentCalled.store(false, std::memory_order_relaxed);
}

    void RestoreHookOnly()
    {
        ImGuiMenu::RestoreWindowProc();
        RestorePresentHook();
    }

    ID3D11Device* GetDevice() { return g_Device; }
    ID3D11DeviceContext* GetContext() { return g_Context; }
    HWND GetGameWindow() { return g_GameWindow; }
    bool IsHookInstalled() { return g_PresentCalled.load(); }
    bool IsPresentHookRestored() { return g_PresentHookRestored.load(std::memory_order_acquire); }
    bool HasActivePresentCall() { return g_PresentCallDepth.load(std::memory_order_acquire) > 0; }
    bool IsSafeToUnload()
    {
        return IsPresentHookRestored() &&
            g_PresentCallDepth.load(std::memory_order_acquire) == 0 &&
            g_ResizeBuffersCallDepth.load(std::memory_order_acquire) == 0 &&
            !ImGuiMenu::HasActiveWindowProc();
    }

    void ReleaseResourcesForUnload()
    {
        ImGuiMenu::InvalidateRenderTarget();

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
    }
}
