#include <cstdint>
#include <sstream>
#include <algorithm>
#include "Main.h"
#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"
#include "../Utils/Logger.h"

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

        // Print diagnostic info about logging
        Logger::LogInfo("[SDK] Diagnostic log files location:");
        Logger::LogInfo("[SDK]   - C:\\temp\\RUGIR_Diagnostic.log (DLL load test)");
        Logger::LogInfo("[SDK]   - C:\\temp\\RUGIR_AddComponent.log (Function calls)");
        Logger::LogInfo("[SDK]   - C:\\temp\\RUGIR_GetFunction.log (Function resolution)");
        Logger::LogInfo("[SDK]   - C:\\temp\\RUGIR_FindObject.log (Object searches)");

        // Print SDK initialization logs now that console is set up
        SDK_PrintInitLogs();

        // === SDK INITIALIZATION ===
        Logger::LogInfo("[SDK] Initializing UE4 SDK...");
        
        try {
            // Get module base address using SDK function (which logs)
            uintptr_t ModuleBase = SDK::InSDKUtils::GetImageBase();
            
            // GObjects offset from compiled SDK
            const uint32_t GObjectsOffset = 0x06B40250;
            uintptr_t GObjectsAddress = ModuleBase + GObjectsOffset;
            
            // Try to access GObjects memory location
            uintptr_t* GObjectsPtr = (uintptr_t*)GObjectsAddress;
            if (*GObjectsPtr != 0) {
                Logger::LogInfo("[SDK] ✅ GObjects Successfully Accessed!");
                
                std::stringstream ss;
                ss << "[SDK] Module Base: 0x" << std::hex << ModuleBase;
                Logger::LogInfo(ss.str());
                
                ss.str("");
                ss << "[SDK] GObjects Address: 0x" << std::hex << GObjectsAddress;
                Logger::LogInfo(ss.str());
                
                ss.str("");
                ss << "[SDK] First GObject Pointer: 0x" << std::hex << *GObjectsPtr;
                Logger::LogInfo(ss.str());
                
                Logger::LogInfo("[SDK] ✅ SDK is ready for use!");
                
                // Call comprehensive SDK diagnostics from Basic.cpp
                SDK_RunComprehensiveDiagnostics();
                
                
            } else {
                Logger::LogWarning("[SDK] ⚠️ GObjects address is null (game might not be loaded yet)");
            }
            
        } catch (const std::exception& e) {
            Logger::LogError("[SDK] ❌ Exception during SDK initialization");
        } catch (...) {
            Logger::LogError("[SDK] ❌ Unknown exception during SDK initialization");
        }

        // Initialiser le hook D3D11
        if (D3D11Hook::Initialize())
        {
            Logger::LogInfo("D3D11 hook initialized successfully");
            Logger::LogInfo("MenuUI will initialize on first frame");
        }
        else
        {
            Logger::LogError("Failed to initialize D3D11 hook!");
            return;
        }

        // ESP drawing is now done directly in SDK_TestDeprojectScreenToWorld() using ImGui
        Logger::LogInfo("✅ ESP drawing system initialized (ImGui overlay)");


        // Initialize game thread hook (Tick monitoring)
        // DISABLED - GameThreadHook disabled
        // if (GameThreadHook::Initialize())
        // {
        //     Logger::LogInfo("Game thread hook initialized successfully");
        //     Logger::LogInfo("Monitoring Tick calls in game thread");
        // }
        // else
        // {
        //     Logger::LogWarning("Game thread hook initialization failed (non-critical)");
        // }

        g_initialized = true;
        Logger::LogInfo("Initialization complete! Press INSERT to toggle menu");
    }

    void Shutdown()
    {
        if (!g_initialized) return;

        Logger::LogInfo("Shutdown in progress...");
        
        // GameThreadHook::Shutdown(); // DISABLED
        D3D11Hook::Shutdown();
        ImGuiMenu::Shutdown();
        
        g_initialized = false;
        Logger::LogInfo("Shutdown complete");
    }

    bool IsInitialized()
    {
        return g_initialized;
    }
}
