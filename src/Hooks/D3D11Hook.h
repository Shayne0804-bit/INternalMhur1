#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <Windows.h>
#include <atomic>

namespace D3D11Hook
{
    typedef HRESULT(WINAPI* PresentFn)(IDXGISwapChain*, UINT, UINT);

    bool Initialize();
    void Shutdown();
    
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();
    HWND GetGameWindow();
    bool IsHookInstalled();
}
