#include <cstdint>
#include <sstream>
#include <algorithm>
#include <Windows.h>
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

    static int HandleAccessViolation(_EXCEPTION_POINTERS* exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
        return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_CONTINUE_SEARCH;
    }

    static bool InitializeD3D11HookNoThrow()
    {
        __try
        {
            return D3D11Hook::Initialize();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    static void InitializeHackThreadNoThrow()
    {
        __try
        {
            HackThreadManager::GetInstance().Initialize();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static bool InitializeGameThreadHookNoThrow()
    {
        __try
        {
            return GameThreadHook::Initialize();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    static void ShutdownD3D11HookNoThrow()
    {
        __try
        {
            D3D11Hook::Shutdown();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void ShutdownHackThreadNoThrow()
    {
        __try
        {
            HackThreadManager::GetInstance().Shutdown();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void ShutdownGameThreadHookNoThrow()
    {
        __try
        {
            GameThreadHook::Shutdown();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    void Initialize()
    {
        if (g_initialized) return;

        GameThreadHook::Log("[Main] Initialize begin");

        // Initialiser le hook D3D11
        if (InitializeD3D11HookNoThrow())
        {
            GameThreadHook::Log("[Main] D3D11Hook initialized");
        }
        else
        {
            GameThreadHook::Log("[Main] D3D11Hook initialization failed");
            return;
        }

        // Initialize the hack thread system
        InitializeHackThreadNoThrow();
        GameThreadHook::Log("[Main] HackThread initialized");

        // ESP drawing is now done directly in SDK_TestDeprojectScreenToWorld() using ImGui


        // Initialize game thread hook (VMT shadowing ProcessEvent)
        if (InitializeGameThreadHookNoThrow())
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

        ShutdownD3D11HookNoThrow();
        ShutdownHackThreadNoThrow();
        ShutdownGameThreadHookNoThrow();
        
        g_initialized = false;
        GameThreadHook::Log("[Main] Shutdown end");
    }

    bool IsInitialized()
    {
        return g_initialized;
    }
}
