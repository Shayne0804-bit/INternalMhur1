#pragma once
#include <d3d11.h>
#include <Windows.h>

// ============================================================================
// StreamOverlay - capture-excluded transparent overlay
// ----------------------------------------------------------------------------
// Renders the ImGui menu/ESP into a separate top-most, click-through, transparent
// DirectComposition window that sits exactly over the game's client area and is
// flagged WDA_EXCLUDEFROMCAPTURE. The window is excluded from screen/window
// capture (OBS display/window capture, Discord Go Live, Xbox Game Bar, ...),
// so viewers never see the menu while the game window itself stays fully visible.
//
// Input is NOT handled here: the overlay is WS_EX_TRANSPARENT so every mouse/key
// event passes through to the game window, whose existing WndProc hook already
// feeds ImGui. Because the overlay covers the exact same client rectangle, the
// cursor and the drawn menu line up 1:1.
//
// The overlay swapchain is created on the SAME D3D11 device as the game, so all
// existing textures (fonts, icons, background video) remain valid when their
// draw data is rendered here.
// ============================================================================

namespace StreamOverlay
{
    // Ensure the overlay exists, track the game client rect, clear it transparent
    // and return the RTV to bind for this frame. Returns nullptr on failure
    // (caller should fall back to rendering into the game backbuffer).
    ID3D11RenderTargetView* Begin(HWND gameWindow, ID3D11Device* device, ID3D11DeviceContext* context);

    // Present the overlay swapchain. Call after ImGui_ImplDX11_RenderDrawData
    // has drawn into the RTV returned by Begin().
    void Present();

    // Clear + present a fully transparent frame so no stale menu lingers on the
    // overlay when there is nothing to draw this frame.
    void PresentEmpty(HWND gameWindow, ID3D11Device* device, ID3D11DeviceContext* context);

    // Tear down the overlay window and all D3D/DComp resources. Idempotent and
    // cheap when nothing is allocated (safe to call every frame while disabled).
    void Shutdown();
}
