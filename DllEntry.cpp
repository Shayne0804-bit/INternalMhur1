#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <fstream>
#include "src/Core/Main.h"

// MainThread following Dumper-7 recommended pattern
DWORD WINAPI MainThread(HMODULE Module)
{
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
