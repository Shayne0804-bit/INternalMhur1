#include "StreamOverlay.h"

#include <dxgi1_2.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

namespace StreamOverlay
{
    static const wchar_t* kClassName = L"RugirStreamOverlayWnd";

    static HWND                    g_Overlay = nullptr;
    static bool                    g_ClassRegistered = false;
    static DWORD                   g_CreatorThreadId = 0;

    static ID3D11Device*           g_Device = nullptr;   // borrowed, not ref-counted
    static IDXGISwapChain1*        g_SwapChain = nullptr;
    static ID3D11RenderTargetView* g_RTV = nullptr;
    static IDCompositionDevice*    g_DComp = nullptr;
    static IDCompositionTarget*    g_DCompTarget = nullptr;
    static IDCompositionVisual*    g_DCompVisual = nullptr;

    static UINT g_Width = 0;
    static UINT g_Height = 0;

    template <typename T>
    static void SafeRelease(T*& p)
    {
        if (p)
        {
            p->Release();
            p = nullptr;
        }
    }

    static void ReleaseRTV()
    {
        SafeRelease(g_RTV);
    }

    static void ReleaseGpu()
    {
        ReleaseRTV();
        SafeRelease(g_DCompVisual);
        SafeRelease(g_DCompTarget);
        SafeRelease(g_DComp);
        SafeRelease(g_SwapChain);
        g_Device = nullptr;
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
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = hInst;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kClassName;
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;
            g_ClassRegistered = true;
        }

        // WS_EX_NOREDIRECTIONBITMAP: no GDI redirection surface, content is supplied
        //   entirely by the DirectComposition visual (true per-pixel alpha).
        // WS_EX_TRANSPARENT + WS_EX_NOACTIVATE: click-through, never steals focus/input.
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

        // Hide from all capture. WDA_EXCLUDEFROMCAPTURE needs Win10 2004+; fall back
        // to WDA_MONITOR on older builds (also blocks most capture paths).
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

    static bool CreatePipeline(ID3D11Device* device, UINT w, UINT h)
    {
        IDXGIDevice*  dxgiDevice = nullptr;
        IDXGIAdapter* adapter    = nullptr;
        IDXGIFactory2* factory   = nullptr;

        bool ok = false;
        do
        {
            if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
                break;
            if (FAILED(dxgiDevice->GetAdapter(&adapter)))
                break;
            if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
                break;

            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = w;
            desc.Height = h;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
            desc.Scaling = DXGI_SCALING_STRETCH;

            if (FAILED(factory->CreateSwapChainForComposition(device, &desc, nullptr, &g_SwapChain)))
                break;

            if (FAILED(DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_DComp))))
                break;
            if (FAILED(g_DComp->CreateTargetForHwnd(g_Overlay, TRUE, &g_DCompTarget)))
                break;
            if (FAILED(g_DComp->CreateVisual(&g_DCompVisual)))
                break;
            if (FAILED(g_DCompVisual->SetContent(g_SwapChain)))
                break;
            if (FAILED(g_DCompTarget->SetRoot(g_DCompVisual)))
                break;
            if (FAILED(g_DComp->Commit()))
                break;

            ok = true;
        } while (false);

        SafeRelease(factory);
        SafeRelease(adapter);
        SafeRelease(dxgiDevice);

        if (!ok)
            ReleaseGpu();

        return ok;
    }

    static bool EnsureSwapChain(ID3D11Device* device, UINT w, UINT h)
    {
        if (g_SwapChain && g_Device == device)
        {
            if (w != g_Width || h != g_Height)
            {
                ReleaseRTV();
                if (FAILED(g_SwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0)))
                    return false;
                g_Width = w;
                g_Height = h;
            }
            return true;
        }

        // Device changed or first use: rebuild the whole pipeline.
        ReleaseGpu();
        if (!CreatePipeline(device, w, h))
            return false;

        g_Device = device;
        g_Width = w;
        g_Height = h;
        return true;
    }

    static bool EnsureRTV()
    {
        if (g_RTV)
            return true;
        if (!g_SwapChain || !g_Device)
            return false;

        ID3D11Texture2D* backBuffer = nullptr;
        if (FAILED(g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
            return false;

        HRESULT hr = g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_RTV);
        backBuffer->Release();
        return SUCCEEDED(hr);
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

        if (!EnsureSwapChain(device, w, h))
            return nullptr;

        if (!EnsureRTV())
            return nullptr;

        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->ClearRenderTargetView(g_RTV, clear);
        return g_RTV;
    }

    void Present()
    {
        if (g_SwapChain)
            g_SwapChain->Present(0, 0);
    }

    void PresentEmpty(HWND gameWindow, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (Begin(gameWindow, device, context))
            Present();
    }

    void Shutdown()
    {
        if (!g_Overlay && !g_SwapChain && !g_DComp)
            return;

        ReleaseGpu();

        if (g_Overlay)
        {
            // ShowWindow is safe cross-thread; DestroyWindow only succeeds on the
            // creating thread. On cross-thread teardown (DLL unload) we at least
            // hide it and drop the handle rather than crash.
            ShowWindow(g_Overlay, SW_HIDE);
            if (GetCurrentThreadId() == g_CreatorThreadId)
                DestroyWindow(g_Overlay);
            g_Overlay = nullptr;
        }

        g_CreatorThreadId = 0;
    }
}
