#include <cstdint>
#include <sstream>
#include <algorithm>
#include "Main.h"
#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"

#include "../Hacks/HackThread.h"

// Include SDK for accessing UObject, FindObjectFast, etc.
// Using Engine_classes.hpp as recommended in Dumper-7 guide for faster compilation
#include "SDK/Engine_classes.hpp"

namespace Main
{
    static bool g_initialized = false;

    void Initialize()
    {
        if (g_initialized) return;

        // Console completely disabled - no window will appear
        // AllocConsole removed
        // freopen_s removed
        // FILE* pFile;
        // freopen_s(&pFile, "CONOUT$", "w", stdout);

        // All logging is silent - no console output

        // Initialiser le hook D3D11
        if (D3D11Hook::Initialize())
        {
            // D3D11 hook initialized successfully
        }
        else
        {
            // Failed to initialize D3D11 hook!
            return;
        }

        // Initialize the hack thread system
        try
        {
            HackThreadManager::GetInstance().Initialize();
        }
        catch (const std::exception& e)
        {
            // Failed to initialize hack thread manager
        }

        // ESP drawing is now done directly in SDK_TestDeprojectScreenToWorld() using ImGui


        // Initialize game thread hook (VMT shadowing ProcessEvent)
        if (GameThreadHook::Initialize())
        {
            // Game thread hook initialized successfully
        }
        else
        {
            // Game thread hook initialization failed (non-critical)
        }

        g_initialized = true;
    }

    void Shutdown()
    {
        if (!g_initialized) return;

        D3D11Hook::Shutdown();
        HackThreadManager::GetInstance().Shutdown();
        GameThreadHook::Shutdown();
        
        g_initialized = false;
    }

    bool IsInitialized()
    {
        return g_initialized;
    }
}
