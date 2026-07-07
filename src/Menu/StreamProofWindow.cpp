#include "StreamProofWindow.h"

#include <dxgi.h>
#include <dcomp.h>
#include <imgui.h>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <windowsx.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

// ImGui Win32 backend message handler (defined by imgui_impl_win32).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace StreamProofWindow
{
    static const wchar_t* kClass = L"RugirStreamProofMenuWnd";

    // Custom messages posted to the window thread.
    static const UINT WM_SPW_SHOW = WM_APP + 1;
    static const UINT WM_SPW_HIDE = WM_APP + 2;
    static const UINT WM_SPW_QUIT = WM_APP + 3;

    struct InputMsg { UINT msg; WPARAM w; LPARAM l; };

    static std::thread            g_Thread;
    static std::atomic<HWND>      g_Hwnd{ nullptr };
    static std::atomic<DWORD>     g_ThreadId{ 0 };
    static std::atomic<bool>      g_Running{ false };
    static std::atomic<bool>      g_ToggleRequest{ false };
    static HWND                   g_GameWindow = nullptr;

    static std::mutex             g_InputMutex;
    static std::vector<InputMsg>  g_InputQueue;

    // GPU objects - render thread only.
    static IDCompositionDevice*    g_DComp = nullptr;
    static IDCompositionTarget*    g_Target = nullptr;
    static IDCompositionVisual*    g_Visual = nullptr;
    static IDCompositionSurface*   g_Surface = nullptr;
    static ID3D11Texture2D*        g_Offscreen = nullptr;
    static ID3D11RenderTargetView* g_OffscreenRTV = nullptr;
    static ID3D11Device*           g_Device = nullptr;   // borrowed
    static UINT                    g_W = 0;
    static UINT                    g_H = 0;

    template <typename T> static void Rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

    // ------------------------------------------------------------------ window thread

    static void PlaceOverGame(HWND hwnd)
    {
        if (!g_GameWindow) return;
        RECT rc{};
        if (!GetClientRect(g_GameWindow, &rc)) return;
        POINT tl{ rc.left, rc.top };
        ClientToScreen(g_GameWindow, &tl);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0) w = 800;
        if (h <= 0) h = 600;
        SetWindowPos(hwnd, HWND_TOPMOST, tl.x, tl.y, w, h, SWP_NOACTIVATE);
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_SPW_SHOW:
            PlaceOverGame(hWnd);
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
            SetActiveWindow(hWnd);
            return 0;
        case WM_SPW_HIDE:
            ShowWindow(hWnd, SW_HIDE);
            if (g_GameWindow) SetForegroundWindow(g_GameWindow);
            return 0;
        case WM_CLOSE:
            // Toggling off, never destroy on the X - just request a hide.
            g_ToggleRequest.store(true, std::memory_order_release);
            return 0;
        case WM_DESTROY:
            return 0;
        }

        // Toggle key closes the menu even when this window holds focus.
        if (msg == WM_KEYDOWN && wParam == VK_INSERT)
        {
            g_ToggleRequest.store(true, std::memory_order_release);
            return 0;
        }

        // Queue input for the render thread to replay into the menu ImGui context.
        switch (msg)
        {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:     case WM_KEYUP:
        case WM_SYSKEYDOWN:  case WM_SYSKEYUP:
        case WM_CHAR:
        {
            std::lock_guard<std::mutex> lk(g_InputMutex);
            g_InputQueue.push_back({ msg, wParam, lParam });
            break;
        }
        default:
            break;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    static void ThreadMain()
    {
        g_ThreadId.store(GetCurrentThreadId(), std::memory_order_release);

        HINSTANCE hInst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);

        // WS_EX_NOREDIRECTIONBITMAP: DComp per-pixel alpha. Focusable (no NOACTIVATE)
        // so it takes keyboard/mouse natively. TOOLWINDOW keeps it out of alt-tab.
        HWND hwnd = CreateWindowExW(
            WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kClass, L"", WS_POPUP,
            0, 0, 800, 600,
            nullptr, nullptr, hInst, nullptr);

        if (!hwnd)
        {
            g_Running.store(false, std::memory_order_release);
            return;
        }

        // Hide from capture (Win10 2004+); WDA_MONITOR is the pre-2004 fallback.
        if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
            SetWindowDisplayAffinity(hwnd, WDA_MONITOR);

        PlaceOverGame(hwnd);
        g_Hwnd.store(hwnd, std::memory_order_release);

        MSG m;
        while (GetMessageW(&m, nullptr, 0, 0))
        {
            if (m.hwnd == nullptr && m.message == WM_SPW_QUIT)
                break;
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }

        DestroyWindow(hwnd);
        UnregisterClassW(kClass, hInst);
        g_Hwnd.store(nullptr, std::memory_order_release);
        g_Running.store(false, std::memory_order_release);
    }

    // ------------------------------------------------------------------ public API

    bool Start(HWND gameWindow)
    {
        if (g_Running.load(std::memory_order_acquire))
            return g_Hwnd.load(std::memory_order_acquire) != nullptr;

        g_GameWindow = gameWindow;
        {
            std::lock_guard<std::mutex> lk(g_InputMutex);
            g_InputQueue.clear();
        }
        g_ToggleRequest.store(false, std::memory_order_release);
        g_Running.store(true, std::memory_order_release);
        g_Thread = std::thread(ThreadMain);

        // Wait for the window to exist (bounded).
        for (int i = 0; i < 500 && !g_Hwnd.load(std::memory_order_acquire) &&
                        g_Running.load(std::memory_order_acquire); ++i)
            Sleep(2);

        return g_Hwnd.load(std::memory_order_acquire) != nullptr;
    }

    void Stop()
    {
        if (g_Running.load(std::memory_order_acquire))
        {
            DWORD tid = g_ThreadId.load(std::memory_order_acquire);
            if (tid)
                PostThreadMessageW(tid, WM_SPW_QUIT, 0, 0);
        }
        if (g_Thread.joinable())
            g_Thread.join();

        Rel(g_OffscreenRTV);
        Rel(g_Offscreen);
        Rel(g_Surface);
        Rel(g_Visual);
        Rel(g_Target);
        Rel(g_DComp);
        g_Device = nullptr;
        g_W = g_H = 0;

        {
            std::lock_guard<std::mutex> lk(g_InputMutex);
            g_InputQueue.clear();
        }
        g_ToggleRequest.store(false, std::memory_order_release);
    }

    bool IsRunning() { return g_Running.load(std::memory_order_acquire); }
    HWND GetHwnd()   { return g_Hwnd.load(std::memory_order_acquire); }

    void Show(bool show)
    {
        DWORD tid = g_ThreadId.load(std::memory_order_acquire);
        if (tid)
            PostThreadMessageW(tid, show ? WM_SPW_SHOW : WM_SPW_HIDE, 0, 0);
        // PostThreadMessage may miss if no queue yet; also poke via the hwnd.
        HWND hwnd = g_Hwnd.load(std::memory_order_acquire);
        if (hwnd)
            PostMessageW(hwnd, show ? WM_SPW_SHOW : WM_SPW_HIDE, 0, 0);
    }

    bool ConsumeToggleRequest()
    {
        return g_ToggleRequest.exchange(false, std::memory_order_acq_rel);
    }

    bool GetClientSize(unsigned& w, unsigned& h)
    {
        HWND hwnd = g_Hwnd.load(std::memory_order_acquire);
        if (!hwnd) return false;
        RECT rc{};
        if (!GetClientRect(hwnd, &rc)) return false;
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        if (cw <= 0 || ch <= 0) return false;
        w = (unsigned)cw; h = (unsigned)ch;
        return true;
    }

    // ------------------------------------------------------------------ GPU (render thread)

    static bool EnsureDComp(ID3D11Device* device, HWND hwnd)
    {
        if (g_DComp && g_Target && g_Visual && g_Device == device)
            return true;

        Rel(g_OffscreenRTV); Rel(g_Offscreen); Rel(g_Surface);
        Rel(g_Visual); Rel(g_Target); Rel(g_DComp);
        g_Device = nullptr; g_W = g_H = 0;

        IDXGIDevice* dxgi = nullptr;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi))))
            return false;

        bool ok = false;
        do {
            if (FAILED(DCompositionCreateDevice(dxgi, IID_PPV_ARGS(&g_DComp)))) break;
            if (FAILED(g_DComp->CreateTargetForHwnd(hwnd, TRUE, &g_Target))) break;
            if (FAILED(g_DComp->CreateVisual(&g_Visual))) break;
            if (FAILED(g_Target->SetRoot(g_Visual))) break;
            ok = true;
        } while (false);

        Rel(dxgi);
        if (!ok) { Rel(g_Visual); Rel(g_Target); Rel(g_DComp); return false; }

        g_Device = device;
        return true;
    }

    static bool EnsureSized(ID3D11Device* device, UINT w, UINT h)
    {
        if (g_Surface && g_OffscreenRTV && w == g_W && h == g_H)
            return true;

        Rel(g_OffscreenRTV); Rel(g_Offscreen); Rel(g_Surface);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, &g_Offscreen))) return false;
        if (FAILED(device->CreateRenderTargetView(g_Offscreen, nullptr, &g_OffscreenRTV)))
        { Rel(g_Offscreen); return false; }

        if (FAILED(g_DComp->CreateSurface(w, h, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_ALPHA_MODE_PREMULTIPLIED, &g_Surface)))
        { Rel(g_OffscreenRTV); Rel(g_Offscreen); return false; }

        if (FAILED(g_Visual->SetContent(g_Surface)) || FAILED(g_DComp->Commit()))
        { Rel(g_Surface); Rel(g_OffscreenRTV); Rel(g_Offscreen); return false; }

        g_W = w; g_H = h;
        return true;
    }

    ID3D11RenderTargetView* BeginGpu(ID3D11Device* device)
    {
        HWND hwnd = g_Hwnd.load(std::memory_order_acquire);
        if (!hwnd || !device) return nullptr;

        unsigned w = 0, h = 0;
        if (!GetClientSize(w, h)) return nullptr;

        if (!EnsureDComp(device, hwnd)) return nullptr;
        if (!EnsureSized(device, w, h)) return nullptr;

        return g_OffscreenRTV;
    }

    void PresentGpu(ID3D11DeviceContext* context)
    {
        if (!g_Surface || !g_Offscreen || !g_DComp || !context) return;

        RECT rc{ 0, 0, (LONG)g_W, (LONG)g_H };
        ID3D11Texture2D* dst = nullptr;
        POINT off{ 0, 0 };
        if (FAILED(g_Surface->BeginDraw(&rc, IID_PPV_ARGS(&dst), &off)))
            return;

        D3D11_BOX box{ 0, 0, 0, g_W, g_H, 1 };
        context->CopySubresourceRegion(dst, 0, (UINT)off.x, (UINT)off.y, 0,
                                       g_Offscreen, 0, &box);
        Rel(dst);
        g_Surface->EndDraw();
        g_DComp->Commit();
    }

    void DrainInto()
    {
        std::vector<InputMsg> local;
        {
            std::lock_guard<std::mutex> lk(g_InputMutex);
            local.swap(g_InputQueue);
        }
        HWND hwnd = g_Hwnd.load(std::memory_order_acquire);
        for (const InputMsg& im : local)
            ImGui_ImplWin32_WndProcHandler(hwnd, im.msg, im.w, im.l);
    }
}
