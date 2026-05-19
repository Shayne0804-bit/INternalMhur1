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

// Declare SDL logging functions from Basic.cpp
extern "C" void SDK_PrintInitLogs();
extern "C" void SDK_RunComprehensiveDiagnostics();

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

        // Print SDK initialization logs now that console is set up
        SDK_PrintInitLogs();

        // === SDK INITIALIZATION ===
        
        try {
            // Get module base address using SDK function (which logs)
            uintptr_t ModuleBase = SDK::InSDKUtils::GetImageBase();
            
            // GObjects offset from compiled SDK
            const uint32_t GObjectsOffset = 0x06B40250;
            uintptr_t GObjectsAddress = ModuleBase + GObjectsOffset;
            
            // Try to access GObjects memory location
            uintptr_t* GObjectsPtr = (uintptr_t*)GObjectsAddress;
            if (*GObjectsPtr != 0) {
                std::stringstream ss;
                ss << "[SDK] Module Base: 0x" << std::hex << ModuleBase;
                
                ss.str("");
                ss << "[SDK] GObjects Address: 0x" << std::hex << GObjectsAddress;
                
                ss.str("");
                ss << "[SDK] First GObject Pointer: 0x" << std::hex << *GObjectsPtr;
                
                // Call comprehensive SDK diagnostics from Basic.cpp
                SDK_RunComprehensiveDiagnostics();
                
                
            } else {
                // GObjects address is null (game might not be loaded yet)
            }
            
        } catch (const std::exception&) {
            // Exception during SDK initialization
        } catch (...) {
            // Unknown exception during SDK initialization
        }

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

        HackThreadManager::GetInstance().Shutdown();
        GameThreadHook::Shutdown();
        D3D11Hook::Shutdown();
        ImGuiMenu::Shutdown();
        
        g_initialized = false;
    }

    bool IsInitialized()
    {
        return g_initialized;
    }
}
