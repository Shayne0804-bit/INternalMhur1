#include "StreamOverlay.h"

#include <dxgi.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

// ============================================================================
// IMPORTANT - why IDCompositionSurface and NOT a composition swapchain:
//
// The whole overlay renders from INSIDE the game's hooked Present(). Any DXGI
// swapchain we create shares the same (hooked) IDXGISwapChain vtable, so calling
// Present() on it re-enters our Present hook AND Steam's gameoverlayrenderer64
// Present hook -> infinite recursion -> EXCEPTION_STACK_OVERFLOW.
//
// DirectComposition surfaces are composed directly by the DWM. There is NO
// swapchain and NO Present call, so no hook is ever re-entered. We draw the menu
// into our own offscreen RTV (origin 0,0 - clean for the ImGui DX11 backend),
// then blit it into the surface via BeginDraw/EndDraw and Commit().
// ============================================================================

namespace StreamOverlay
{
    static const wchar_t* kClassName = L"RugirStreamOverlayWnd";

    static HWND                    g_Overlay = nullptr;
    static bool                    g_ClassRegistered = false;
    static DWORD                   g_CreatorThreadId = 0;

    static ID3D11Device*           g_Device = nullptr;   // borrowed, not ref-counted
    static ID3D11DeviceContext*    g_ContextBorrowed = nullptr;   // borrowed

    static IDCompositionDevice*    g_DComp = nullptr;
    static IDCompositionTarget*    g_DCompTarget = nullptr;
    static IDCompositionVisual*    g_DCompVisual = nullptr;
    static IDCompositionSurface*   g_Surface = nullptr;

    static ID3D11Texture2D*        g_MenuTex = nullptr;   // offscreen menu render target
    static ID3D11RenderTargetView* g_MenuRTV = nullptr;

    static UINT g_Width = 0;
    static UINT g_Height = 0;

    // Overlay window proc. WS_EX_NOREDIRECTIONBITMAP (needed for DComp per-pixel
    // alpha) makes WS_EX_TRANSPARENT insufficient on its own: the OS hit-tests the
    // composed window as opaque and SWALLOWS the click, so neither the overlay nor
    // the game beneath it ever sees the mouse. Returning HTTRANSPARENT on
    // WM_NCHITTEST forces every click to fall THROUGH to the game window, where the
    // existing HookedWndProc already feeds ImGui. MA_NOACTIVATE keeps focus on the
    // game so nothing is stolen.
    static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    }

    template <typename T>
    static void SafeRelease(T*& p)
    {
        if (p)
        {
            p->Release();
            p = nullptr;
        }
    }

    static void ReleaseSizedResources()
    {
        SafeRelease(g_MenuRTV);
        SafeRelease(g_MenuTex);
        SafeRelease(g_Surface);   // visual content is recreated on next EnsureSurface
    }

    static void ReleaseGpu()
    {
        ReleaseSizedResources();
        SafeRelease(g_DCompVisual);
        SafeRelease(g_DCompTarget);
        SafeRelease(g_DComp);
        g_Device = nullptr;
        g_ContextBorrowed = nullptr;
        g_Width = 0;
        g_Height = 0;
    }

    static bool EnsureWindow(HWND gameWindow)
    {
        if (g_Overlay && IsWindow(g_Overlay))
            return true;

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        if (!g_ClassRegistered)
        {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = OverlayWndProc;
            wc.hInstance = hInst;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kClassName;
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;
            g_ClassRegistered = true;
        }

        // WS_EX_NOREDIRECTIONBITMAP: no GDI redirection surface; content comes
        //   entirely from the DirectComposition visual (true per-pixel alpha).
        // WS_EX_TRANSPARENT + WS_EX_NOACTIVATE: click-through, never steals input.
        // WS_EX_TOPMOST: stays above the (borderless) game window.
        const DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT |
                              WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;

        RECT rc{};
        POINT tl{ 0, 0 };
        int w = 100, h = 100;
        if (GetClientRect(gameWindow, &rc))
        {
            tl.x = rc.left;
            tl.y = rc.top;
            ClientToScreen(gameWindow, &tl);
            w = rc.right - rc.left;
            h = rc.bottom - rc.top;
            if (w <= 0) w = 100;
            if (h <= 0) h = 100;
        }

        g_Overlay = CreateWindowExW(
            exStyle,
            kClassName,
            L"",
            WS_POPUP,
            tl.x, tl.y, w, h,
            nullptr, nullptr, hInst, nullptr);

        if (!g_Overlay)
            return false;

        g_CreatorThreadId = GetCurrentThreadId();

        // Hide from all capture. WDA_EXCLUDEFROMCAPTURE needs Win10 2004+; fall
        // back to WDA_MONITOR on older builds (also blocks most capture paths).
        if (!SetWindowDisplayAffinity(g_Overlay, WDA_EXCLUDEFROMCAPTURE))
            SetWindowDisplayAffinity(g_Overlay, WDA_MONITOR);

        ShowWindow(g_Overlay, SW_SHOWNOACTIVATE);
        return true;
    }

    // Keep the overlay glued to the game's client rectangle (screen space).
    static bool SyncToClient(HWND gameWindow, UINT& outW, UINT& outH)
    {
        RECT rc{};
        if (!GetClientRect(gameWindow, &rc))
            return false;

        POINT tl{ rc.left, rc.top };
        ClientToScreen(gameWindow, &tl);

        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0)
            return false;

        SetWindowPos(g_Overlay, HWND_TOPMOST, tl.x, tl.y, w, h,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

        outW = static_cast<UINT>(w);
        outH = static_cast<UINT>(h);
        return true;
    }

    static bool EnsureDComp(ID3D11Device* device)
    {
        if (g_DComp && g_DCompTarget && g_DCompVisual && g_Device == device)
            return true;

        // Device changed (or first use): rebuild the DComp pipeline.
        ReleaseGpu();

        IDXGIDevice* dxgiDevice = nullptr;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
            return false;

        bool ok = false;
        do
        {
            if (FAILED(DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_DComp))))
                break;
            if (FAILED(g_DComp->CreateTargetForHwnd(g_Overlay, TRUE, &g_DCompTarget)))
                break;
            if (FAILED(g_DComp->CreateVisual(&g_DCompVisual)))
                break;
            if (FAILED(g_DCompTarget->SetRoot(g_DCompVisual)))
                break;
            ok = true;
        } while (false);

        SafeRelease(dxgiDevice);

        if (!ok)
        {
            ReleaseGpu();
            return false;
        }

        g_Device = device;
        return true;
    }

    static bool EnsureSizedResources(ID3D11Device* device, UINT w, UINT h)
    {
        if (g_Surface && g_MenuRTV && w == g_Width && h == g_Height)
            return true;

        ReleaseSizedResources();

        // Offscreen menu render target: ImGui draws here at origin (0,0), which the
        // ImGui DX11 backend requires (it forces a 0,0 viewport).
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&td, nullptr, &g_MenuTex)))
            return false;
        if (FAILED(device->CreateRenderTargetView(g_MenuTex, nullptr, &g_MenuRTV)))
        {
            ReleaseSizedResources();
            return false;
        }

        // DComp surface (composed by the DWM, no swapchain / no Present).
        if (FAILED(g_DComp->CreateSurface(w, h, DXGI_FORMAT_B8G8R8A8_UNORM,
                                          DXGI_ALPHA_MODE_PREMULTIPLIED, &g_Surface)))
        {
            ReleaseSizedResources();
            return false;
        }

        if (FAILED(g_DCompVisual->SetContent(g_Surface)) || FAILED(g_DComp->Commit()))
        {
            ReleaseSizedResources();
            return false;
        }

        g_Width = w;
        g_Height = h;
        return true;
    }

    ID3D11RenderTargetView* Begin(HWND gameWindow, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (!gameWindow || !device || !context)
            return nullptr;

        if (!EnsureWindow(gameWindow))
            return nullptr;

        UINT w = 0, h = 0;
        if (!SyncToClient(gameWindow, w, h))
            return nullptr;

        if (!EnsureDComp(device))
            return nullptr;

        if (!EnsureSizedResources(device, w, h))
            return nullptr;

        g_ContextBorrowed = context;

        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->ClearRenderTargetView(g_MenuRTV, clear);
        return g_MenuRTV;
    }

    void Present()
    {
        if (!g_Surface || !g_MenuTex || !g_DComp || !g_ContextBorrowed)
            return;

        RECT updateRect{ 0, 0, static_cast<LONG>(g_Width), static_cast<LONG>(g_Height) };
        ID3D11Texture2D* surfaceTex = nullptr;
        POINT offset{ 0, 0 };

        if (FAILED(g_Surface->BeginDraw(&updateRect, IID_PPV_ARGS(&surfaceTex), &offset)))
            return;

        // The surface texture may be an atlas: copy our menu into it at the
        // offset the surface handed back.
        D3D11_BOX box{};
        box.left = 0;
        box.top = 0;
        box.front = 0;
        box.right = g_Width;
        box.bottom = g_Height;
        box.back = 1;

        g_ContextBorrowed->CopySubresourceRegion(
            surfaceTex, 0,
            static_cast<UINT>(offset.x), static_cast<UINT>(offset.y), 0,
            g_MenuTex, 0, &box);

        SafeRelease(surfaceTex);

        g_Surface->EndDraw();
        g_DComp->Commit();
    }

    void PresentEmpty(HWND gameWindow, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        // Begin() already cleared the menu RTV to fully transparent; blitting it
        // wipes any stale menu still shown on the surface.
        if (Begin(gameWindow, device, context))
            Present();
    }

    void Shutdown()
    {
        if (!g_Overlay && !g_DComp && !g_Surface)
            return;

        ReleaseGpu();

        if (g_Overlay)
        {
            // ShowWindow is safe cross-thread; DestroyWindow only succeeds on the
            // creating thread. On cross-thread teardown (DLL unload) at least hide
            // it and drop the handle rather than risk a bad-thread destroy.
            ShowWindow(g_Overlay, SW_HIDE);
            if (GetCurrentThreadId() == g_CreatorThreadId)
                DestroyWindow(g_Overlay);
            g_Overlay = nullptr;
        }

        g_CreatorThreadId = 0;
    }
}
