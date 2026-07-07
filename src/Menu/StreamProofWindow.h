#pragma once
#include <Windows.h>
#include <d3d11.h>

// ============================================================================
// StreamProofWindow
//
// A real, focusable, top-most borderless window that hosts the ImGui menu in
// ITS OWN window instead of painting it into the game's backbuffer. Because it
// is a genuine window it receives mouse/keyboard natively (no click-through, no
// input routed through the game) and SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)
// hides ONLY this window from capture (Discord Go Live / OBS / Game Bar) while
// the game window stays fully visible in the stream.
//
// Threading contract:
//   - The window lives on its OWN thread which does nothing but pump messages,
//     queue input, and show/hide. It NEVER touches D3D or ImGui.
//   - All D3D + ImGui work (BeginGpu/PresentGpu/DrainInto) runs on the GAME
//     render thread (inside the Present hook), so the shared immediate context
//     is only ever used from one thread. Input is marshalled from the window
//     thread to the render thread via a lock-protected queue.
//
// Rendering uses an IDCompositionSurface (composed by the DWM) - no swapchain,
// no Present(), so the game's/Steam's Present hooks are never re-entered.
// ============================================================================

namespace StreamProofWindow
{
    // --- window-thread side (callable from any thread) ---
    bool Start(HWND gameWindow);   // spawn window thread + create window (idempotent)
    void Stop();                   // destroy window, join thread, release GPU
    bool IsRunning();
    HWND GetHwnd();
    void Show(bool show);          // marshalled to the window thread
    bool ConsumeToggleRequest();   // true once if the menu window saw the toggle key

    // --- render-thread side (game thread only) ---
    // Ensure DComp + surface + offscreen RTV exist and match the window size,
    // clear the offscreen RTV to transparent and return it for ImGui to draw into.
    ID3D11RenderTargetView* BeginGpu(ID3D11Device* device);
    // Blit the offscreen RTV into the composition surface and Commit.
    void PresentGpu(ID3D11DeviceContext* context);
    // Replay queued window-thread input into the CURRENT ImGui context.
    void DrainInto();
    // Current client size of the menu window (0 if not ready).
    bool GetClientSize(unsigned& w, unsigned& h);
}
