#include "StreamProofWindow.h"

#include <dxgi.h>
#include <dcomp.h>
#include <imgui.h>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

// ImGui Win32 backend message handler (defined by imgui_impl_win32).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace StreamProofWindow
{
    static const wchar_t* kClass = L"RugirStreamProofMenuWnd";

    static HWND  g_Hwnd = nullptr;
    static bool  g_ClassReg = false;
    static HWND  g_GameWindow = nullptr;
    static std::atomic<bool> g_ToggleRequest{ false };

    // GPU objects (all render-thread).
    static IDCompositionDevice*    g_DComp = nullptr;
    static IDCompositionTarget*    g_Target = nullptr;
    static IDCompositionVisual*    g_Visual = nullptr;
    static IDCompositionSurface*   g_Surface = nullptr;
    static ID3D11Texture2D*        g_Offscreen = nullptr;
    static ID3D11RenderTargetView* g_OffscreenRTV = nullptr;
    static ID3D11Device*           g_Device = nullptr;   // borrowed
    static UINT g_W = 0, g_H = 0;

    template <typename T> static void Rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Runs on the render thread during Pump(), with the menu ImGui context
        // current, so we can feed the backend directly.
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam == VK_INSERT)
            {
                g_ToggleRequest.store(true, std::memory_order_release);
                return 0;
            }
            break;
        case WM_CLOSE:
            g_ToggleRequest.store(true, std::memory_order_release);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    static void PlaceOverGame()
    {
        if (!g_Hwnd || !g_GameWindow) return;
        RECT rc{};
        if (!GetClientRect(g_GameWindow, &rc)) return;
        POINT tl{ rc.left, rc.top };
        ClientToScreen(g_GameWindow, &tl);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0) w = 800;
        if (h <= 0) h = 600;
        SetWindowPos(g_Hwnd, HWND_TOPMOST, tl.x, tl.y, w, h, SWP_NOACTIVATE | SWP_NOREDRAW);
    }

    static bool EnsureDComp(ID3D11Device* device)
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
            if (FAILED(g_DComp->CreateTargetForHwnd(g_Hwnd, TRUE, &g_Target))) break;
            if (FAILED(g_DComp->CreateVisual(&g_Visual))) break;
            if (FAILED(g_Target->SetRoot(g_Visual))) break;
            ok = true;
        } while (false);

        Rel(dxgi);
        if (!ok) { Rel(g_Visual); Rel(g_Target); Rel(g_DComp); return false; }

        g_Device = device;
        return true;
    }

    bool EnsureCreated(HWND gameWindow, ID3D11Device* device)
    {
        g_GameWindow = gameWindow;
        if (!device) return false;

        if (!g_Hwnd)
        {
            HINSTANCE hInst = GetModuleHandleW(nullptr);
            if (!g_ClassReg)
            {
                WNDCLASSEXW wc{};
                wc.cbSize = sizeof(wc);
                wc.style = CS_HREDRAW | CS_VREDRAW;
                wc.lpfnWndProc = WndProc;
                wc.hInstance = hInst;
                wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                wc.lpszClassName = kClass;
                if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                    return false;
                g_ClassReg = true;
            }

            // WS_EX_NOREDIRECTIONBITMAP: content comes from the DComp visual with
            // true per-pixel alpha. Focusable (no NOACTIVATE) so it takes input.
            g_Hwnd = CreateWindowExW(
                WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                kClass, L"", WS_POPUP,
                0, 0, 800, 600,
                nullptr, nullptr, hInst, nullptr);
            if (!g_Hwnd)
                return false;

            if (!SetWindowDisplayAffinity(g_Hwnd, WDA_EXCLUDEFROMCAPTURE))
                SetWindowDisplayAffinity(g_Hwnd, WDA_MONITOR);

            PlaceOverGame();
        }

        return EnsureDComp(device);
    }

    void Stop()
    {
        Rel(g_OffscreenRTV); Rel(g_Offscreen); Rel(g_Surface);
        Rel(g_Visual); Rel(g_Target); Rel(g_DComp);
        g_Device = nullptr; g_W = g_H = 0;

        if (g_Hwnd)
        {
            DestroyWindow(g_Hwnd);
            g_Hwnd = nullptr;
        }
        g_ToggleRequest.store(false, std::memory_order_release);
    }

    bool IsReady() { return g_Hwnd != nullptr && g_DComp != nullptr; }
    HWND GetHwnd() { return g_Hwnd; }

    void Pump()
    {
        if (!g_Hwnd) return;
        MSG m;
        while (PeekMessageW(&m, g_Hwnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    void Show(bool show)
    {
        if (!g_Hwnd) return;
        if (show)
        {
            PlaceOverGame();
            ShowWindow(g_Hwnd, SW_SHOW);
            SetForegroundWindow(g_Hwnd);
            SetActiveWindow(g_Hwnd);
            SetFocus(g_Hwnd);
        }
        else
        {
            ShowWindow(g_Hwnd, SW_HIDE);
            if (g_GameWindow)
                SetForegroundWindow(g_GameWindow);
        }
    }

    bool ConsumeToggleRequest()
    {
        return g_ToggleRequest.exchange(false, std::memory_order_acq_rel);
    }

    bool GetClientSize(unsigned& w, unsigned& h)
    {
        if (!g_Hwnd) return false;
        RECT rc{};
        if (!GetClientRect(g_Hwnd, &rc)) return false;
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        if (cw <= 0 || ch <= 0) return false;
        w = (unsigned)cw; h = (unsigned)ch;
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
        if (!g_Hwnd || !device) return nullptr;

        // Track the game client size so the menu window stays glued on top of it.
        PlaceOverGame();

        unsigned w = 0, h = 0;
        if (!GetClientSize(w, h)) return nullptr;
        if (!EnsureDComp(device)) return nullptr;
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
}
