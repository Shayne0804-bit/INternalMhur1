#include <cstdint>
#include <sstream>
#include <algorithm>
#include <Windows.h>
#include "Main.h"
#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Menu/ImGuiMenu.h"

#include "../Hacks/HackThread.h"
#include "../Hacks/InGameModuleHacks.h"

// Include SDK for accessing UObject, FindObjectFast, etc.
// Using Engine_classes.hpp as recommended in Dumper-7 guide for faster compilation
#include "SDK/Engine_classes.hpp"

namespace Main
{
    static bool g_initialized = false;

    // ===== CRASH DIAGNOSTIC (VEH) =====
    // Captures the exact game-code location of the recurring next-match crash
    // (AV reading 0x...00BA: null base + small field offset). Logs RIP with
    // module+offset plus MHUR-module return addresses found on the stack, then
    // lets the exception continue unhandled so game behavior is unchanged.
    // Output: C:\temp\rugir_crash.log — this is the data that pinpoints which
    // game function dereferences the stale pointer.
    static volatile LONG g_crashLogCount = 0;

    static LONG CALLBACK CrashDiagnosticHandler(PEXCEPTION_POINTERS info)
    {
        if (!info || !info->ExceptionRecord || !info->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;
        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
            return EXCEPTION_CONTINUE_SEARCH;
        if (info->ExceptionRecord->NumberParameters < 2)
            return EXCEPTION_CONTINUE_SEARCH;

        // Only the signature crash: near-null dereference (base==null + field offset).
        const ULONG_PTR faultAddr = info->ExceptionRecord->ExceptionInformation[1];
        if (faultAddr >= 0x10000)
            return EXCEPTION_CONTINUE_SEARCH;

        // Cap the log volume; VEH sees handled AVs too (our SEH probes), so only
        // keep the first few near-null hits per session.
        if (InterlockedIncrement(&g_crashLogCount) > 8)
            return EXCEPTION_CONTINUE_SEARCH;

        char buf[2048];
        int len = 0;

        const ULONG_PTR rip = (ULONG_PTR)info->ContextRecord->Rip;
        const ULONG_PTR accessType = info->ExceptionRecord->ExceptionInformation[0];

        HMODULE ripModule = nullptr;
        char ripModName[MAX_PATH] = "?";
        ULONG_PTR ripOffset = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)rip, &ripModule) && ripModule)
        {
            GetModuleFileNameA(ripModule, ripModName, MAX_PATH);
            ripOffset = rip - (ULONG_PTR)ripModule;
        }

        len += sprintf_s(buf + len, sizeof(buf) - len,
                         "[CRASH] AV %s addr=0x%llX RIP=%s+0x%llX (abs=0x%llX) tid=%lu\n",
                         accessType ? "WRITE" : "READ",
                         (unsigned long long)faultAddr,
                         ripModName, (unsigned long long)ripOffset,
                         (unsigned long long)rip, GetCurrentThreadId());

        // Walk the raw stack for plausible return addresses (module code pointers).
        const ULONG_PTR* stack = (const ULONG_PTR*)info->ContextRecord->Rsp;
        for (int i = 0; i < 96 && len < (int)sizeof(buf) - 128; ++i)
        {
            ULONG_PTR candidate = 0;
            __try { candidate = stack[i]; }
            __except (EXCEPTION_EXECUTE_HANDLER) { break; }

            if (candidate < 0x10000)
                continue;

            HMODULE mod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)candidate, &mod) && mod)
            {
                char modName[MAX_PATH] = "?";
                GetModuleFileNameA(mod, modName, MAX_PATH);
                const char* base = strrchr(modName, '\\');
                len += sprintf_s(buf + len, sizeof(buf) - len,
                                 "[CRASH]   stack[%02d] %s+0x%llX\n",
                                 i, base ? base + 1 : modName,
                                 (unsigned long long)(candidate - (ULONG_PTR)mod));
            }
        }

        HANDLE file = CreateFileA("C:\\temp\\rugir_crash.log", FILE_APPEND_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, buf, (DWORD)len, &written, nullptr);
            CloseHandle(file);
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }


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

        // Crash diagnostic first so it observes everything from the start.
        CreateDirectoryA("C:\\temp", nullptr);
        AddVectoredExceptionHandler(1, CrashDiagnosticHandler);
        GameThreadHook::Log("[Main] Crash diagnostic VEH installed");

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

    void InitializeRenderOnly()
    {
        if (g_initialized) return;

        GameThreadHook::Log("[Main] InitializeRenderOnly begin (game incompatible)");

        // Tell the menu to skip ALL SDK/menu rendering and draw only the
        // self-update overlay — no SDK reads happen in this mode.
        ImGuiMenu::SetCompatRenderOnly(true);

        // Only the D3D11 hook (swapchain vtable — independent of SDK offsets).
        if (InitializeD3D11HookNoThrow())
        {
            GameThreadHook::Log("[Main] D3D11Hook initialized (render-only)");
        }
        else
        {
            GameThreadHook::Log("[Main] D3D11Hook init failed (render-only)");
            return;
        }

        // Deliberately NO HackThread and NO GameThreadHook: both touch the SDK.
        g_initialized = true;
        GameThreadHook::Log("[Main] InitializeRenderOnly end");
    }

    static void RemoveDamageProcessEventHookNoThrow()
    {
        __try
        {
            InGameHack_RemoveDamageProcessEventHook();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    void Shutdown()
    {
        if (!g_initialized) return;

        // Restore the inline ProcessEvent hook FIRST: once the module unloads, the
        // patched jmp would point into freed detour code and crash the game on the
        // next ProcessEvent. Must happen before anything else is torn down.
        RemoveDamageProcessEventHookNoThrow();

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
