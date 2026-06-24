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

        GameThreadHook::Log("[Main] Initialize begin");

        // Initialiser le hook D3D11
        if (D3D11Hook::Initialize())
        {
            GameThreadHook::Log("[Main] D3D11Hook initialized");
        }
        else
        {
            GameThreadHook::Log("[Main] D3D11Hook initialization failed");
            return;
        }

        // Initialize the hack thread system
        try
        {
            HackThreadManager::GetInstance().Initialize();
            GameThreadHook::Log("[Main] HackThread initialized");
        }
        catch (const std::exception& e)
        {
            GameThreadHook::Log("[Main] HackThread std::exception: %s", e.what());
        }

        // ESP drawing is now done directly in SDK_TestDeprojectScreenToWorld() using ImGui


        // Initialize game thread hook (VMT shadowing ProcessEvent)
        if (GameThreadHook::Initialize())
        {
            GameThreadHook::Log("[Main] GameThreadHook active");
        }
        else
        {
            GameThreadHook::Log("[Main] GameThreadHook not active yet");
        }

        g_initialized = true;
        GameThreadHook::Log("[Main] Initialize end");
    }

    void Shutdown()
    {
        if (!g_initialized) return;

        D3D11Hook::Shutdown();
        HackThreadManager::GetInstance().Shutdown();
        GameThreadHook::Shutdown();
        
        g_initialized = false;
        GameThreadHook::Log("[Main] Shutdown end");
    }

    bool IsInitialized()
    {
        return g_initialized;
    }
}
