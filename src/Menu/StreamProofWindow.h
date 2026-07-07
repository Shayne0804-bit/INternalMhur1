#pragma once
#include <Windows.h>
#include <d3d11.h>

// ============================================================================
// StreamProofWindow
//
// A real, focusable, top-most, per-pixel-alpha window that hosts the ImGui menu
// in ITS OWN window instead of painting it into the game's backbuffer. Because
// it is a genuine window it receives mouse/keyboard natively, and
// SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) hides ONLY this window from
// capture (Discord Go Live / OBS / Game Bar) while the game window stays fully
// visible in the stream.
//
// Threading: EVERYTHING runs on the GAME render thread (inside the Present
// hook) - window creation, the message pump (PeekMessage), input, DComp and
// ImGui. No second thread, no cross-thread D3D/DComp, no input marshalling.
// This is what makes the per-pixel alpha (transparency) and WDA behave.
//
// Rendering uses an IDCompositionSurface (composed by the DWM) - no swapchain,
// no Present(), so the game's/Steam's Present hooks are never re-entered.
// ============================================================================

namespace StreamProofWindow
{
    // All of these must be called from the game render thread.
    bool EnsureCreated(HWND gameWindow, ID3D11Device* device);
    void Stop();
    bool IsReady();
    HWND GetHwnd();

    void Pump();                   // PeekMessage/Dispatch (menu ImGui ctx must be current)
    void Show(bool show);          // show + focus / hide + return focus to game
    bool ConsumeToggleRequest();   // true once if the menu window saw the toggle key

    ID3D11RenderTargetView* BeginGpu(ID3D11Device* device);  // clears + returns offscreen RTV
    void PresentGpu(ID3D11DeviceContext* context);           // blit offscreen -> surface + commit
    bool GetClientSize(unsigned& w, unsigned& h);
}
