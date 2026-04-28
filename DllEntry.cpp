#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <fstream>
#include "src/Core/Main.h"

// Global module handle for unload
static HMODULE g_hModule = nullptr;

// MainThread following Dumper-7 recommended pattern
DWORD WINAPI MainThread(HMODULE Module)
{
    g_hModule = Module;
    
    /* Console completely disabled - no logging output */
    // AllocConsole removed - console window will not appear
    // All printf statements disabled
    // All file logging disabled
    
    /* Wait for game initialization */
    Sleep(3000);

    /* Initialize the SDK and menu silently */
    try
    {
        Main::Initialize();
    }
    catch (const std::exception& e)
    {
        // Silent fail - no error output
        (void)e;
    }
    catch (...)
    {
        // Silent fail - no error output
    }

    return 0;
}

// Thread function to completely unload the DLL
DWORD WINAPI UnloadThread(LPVOID lpParam)
{
    // Wait for current render operations to finish
    Sleep(500);
    
    // Complete cleanup - remove all hooks and restore original state
    try
    {
        Main::Shutdown();
    }
    catch (...)
    {
    }
    
    // Wait a bit more to ensure all cleanup is done
    Sleep(100);
    
    // Now unload the DLL completely
    if (g_hModule)
    {
        FreeLibraryAndExitThread(g_hModule, 0);
    }
    
    return 0;
}

// Extern function to unload DLL from menu - starts unload thread
extern "C" __declspec(dllexport) void DLL_Unload()
{
    // Create a dedicated thread for unloading to avoid blocking game
    HANDLE hThread = CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
    if (hThread)
    {
        CloseHandle(hThread);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        /* Disable thread library calls for performance */
        DisableThreadLibraryCalls(hModule);
        
        /* Create thread following Dumper-7 pattern */
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
    {
        /* Cleanup resources */
        Main::Shutdown();
        break;
    }
    }
    return TRUE;
}
