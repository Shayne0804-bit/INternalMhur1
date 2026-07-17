#pragma once

namespace Main
{
    void Initialize();

    // Render-only init for the game-update (incompatible) case: brings up ONLY
    // the D3D11 hook + ImGui so the self-update toast can draw. No HackThread, no
    // GameThreadHook, no SDK access — safe when the game offsets are invalid.
    void InitializeRenderOnly();

    void Shutdown();
    bool IsInitialized();
}
