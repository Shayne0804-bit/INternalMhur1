#include <Windows.h>
#include <fstream>
#include "src/Core/Main.h"
#include "src/Core/UnloadManager.h"
#include "src/Hooks/D3D11Hook.h"
#include "src/Hooks/GameThreadHook.h"
#include "src/Hacks/HackThread.h"
#include "src/Menu/ImGuiMenu.h"
#include "src/Utils/Logger.h"
#include <atomic>
#include <cstdint>
#include <exception>

#ifndef RUGIR_ENABLE_UNLOAD
#define RUGIR_ENABLE_UNLOAD 1
#endif

#ifndef RUGIR_FULL_PROTECTION
#define RUGIR_FULL_PROTECTION 0
#endif

#ifndef DO_NOT_INCLUDE_STR_CRYPTOR
#define DO_NOT_INCLUDE_STR_CRYPTOR 0
#endif
#include "LibProt-main/LibProt-main/LibProt.h"

// Global module handle for unload
static HMODULE g_hModule = nullptr;
static std::atomic_bool g_MainThreadRunning{ false };
static std::atomic_bool g_LibProtInitialized{ false };
static PVOID g_ExceptionHandler = nullptr;

static int HandleAccessViolation(_EXCEPTION_POINTERS* exceptionInfo)
{
    if (!exceptionInfo || !exceptionInfo->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
    return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI RugirVectoredExceptionHandler(_EXCEPTION_POINTERS* exceptionInfo)
{
    return EXCEPTION_CONTINUE_SEARCH;
}

static void ApplyLibProtProtection(HMODULE module)
{
    if (!module)
        return;

    bool expected = false;
    if (!g_LibProtInitialized.compare_exchange_strong(
        expected,
        true,
        std::memory_order_acq_rel,
        std::memory_order_acquire))
    {
        return;
    }

    __try
    {
        void* moduleBase = reinterpret_cast<void*>(module);

#if RUGIR_FULL_PROTECTION
        LibProt::Initialize(moduleBase, true, true, true);
#else
        // Keep exports/TLS and loader DllBase intact so the in-menu unload and
        // FreeLibraryAndExitThread path continue to work after protection.
        LibProt::CleanImportsAndExports(moduleBase, false, false);
        LibProt::CleanPE(moduleBase);
        LibProt::CleanImportsAndExports(moduleBase, false, false);
        LibProt::DestroyEntryPoint(moduleBase, true);
#endif
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

static void RestoreD3D11HookNoThrow()
{
    __try
    {
        D3D11Hook::RestoreHookOnly();
    }
    __except (HandleAccessViolation(GetExceptionInformation()))
    {
    }
}

static void ReleaseD3D11ResourcesNoThrow()
{
    __try
    {
        D3D11Hook::ReleaseResourcesForUnload();
    }
    __except (HandleAccessViolation(GetExceptionInformation()))
    {
    }
}

static void InitializeMainNoThrow()
{
    __try
    {
        Main::Initialize();
    }
    __except (HandleAccessViolation(GetExceptionInformation()))
    {
    }
}

static void ShutdownMainNoThrow()
{
    __try
    {
        Main::Shutdown();
    }
    __except (HandleAccessViolation(GetExceptionInformation()))
    {
    }
}

#if RUGIR_ENABLE_UNLOAD
DWORD WINAPI UnloadThread(LPVOID lpParam)
{
    HMODULE module = reinterpret_cast<HMODULE>(lpParam);

    for (int i = 0; i < 4000 && g_MainThreadRunning.load(std::memory_order_acquire); ++i)
        Sleep(1);

    ShutdownHackThreadNoThrow();
    ShutdownGameThreadHookNoThrow();
    RestoreD3D11HookNoThrow();

    for (int i = 0; i < 5000 && !D3D11Hook::IsSafeToUnload(); ++i)
        Sleep(1);

    ReleaseD3D11ResourcesNoThrow();

    for (int i = 0; i < 1000 && !D3D11Hook::IsSafeToUnload(); ++i)
        Sleep(1);

    Sleep(1000);

    if (module)
        FreeLibraryAndExitThread(module, 0);

    ExitThread(0);
    return 0;
}
#endif

// MainThread following Dumper-7 recommended pattern
DWORD WINAPI MainThread(HMODULE Module)
{
    Logger::InitializeFileLogging();
    Logger::LogInfo("[DllEntry] MainThread start");

    g_MainThreadRunning.store(true, std::memory_order_release);
    g_hModule = Module;
    ImGuiMenu::SetModuleHandle(Module);
    ImGuiMenu::PreloadEmbeddedVideoResource(Module);
    GameThreadHook::Log("[DllEntry] MainThread start module=0x%p", Module);
    ApplyLibProtProtection(Module);
    GameThreadHook::Log("[DllEntry] LibProt protection applied");

    /* Wait for game initialization */
    GameThreadHook::Log("[DllEntry] waiting 3000ms before Main::Initialize");
    Sleep(3000);

    if (UnloadManager::IsUnloadRequested())
    {
        GameThreadHook::Log("[DllEntry] unload requested before init");
        g_MainThreadRunning.store(false, std::memory_order_release);
        return 0;
    }

    /* Initialize the SDK and menu silently */
    GameThreadHook::Log("[DllEntry] Main::Initialize begin");
    InitializeMainNoThrow();
    GameThreadHook::Log("[DllEntry] Main::Initialize end");

    GameThreadHook::Log("[DllEntry] MainThread end");
    Logger::LogInfo("[DllEntry] MainThread end");
    g_MainThreadRunning.store(false, std::memory_order_release);
    return 0;
}

// Extern function to get unload status
extern "C" __declspec(dllexport) bool DLL_IsUnloadRequested()
{
    return UnloadManager::IsUnloadRequested();
}

#if RUGIR_ENABLE_UNLOAD
// Extern function to request DLL unload - starts unload thread
extern "C" __declspec(dllexport) void DLL_Unload()
{
    if (!UnloadManager::RequestUnload())
        return;

    HANDLE hThread = CreateThread(nullptr, 0, UnloadThread, g_hModule, 0, nullptr);
    if (hThread)
        CloseHandle(hThread);
}
#endif

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hModule = hModule;
        Logger::InitializeFileLogging();
        Logger::LogInfo("[DllEntry] DLL_PROCESS_ATTACH");

        if (!g_ExceptionHandler)
        {
            g_ExceptionHandler = AddVectoredExceptionHandler(1, RugirVectoredExceptionHandler);
            Logger::LogInfo(g_ExceptionHandler
                ? "[DllEntry] vectored exception handler installed"
                : "[DllEntry] failed to install vectored exception handler");
        }

        /* Disable thread library calls for performance */
        DisableThreadLibraryCalls(hModule);

        /* Create thread following Dumper-7 pattern */
        HANDLE hThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        if (hThread)
        {
            CloseHandle(hThread);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
    {
        Logger::LogInfo("[DllEntry] DLL_PROCESS_DETACH");

        /* Cleanup resources */
        if (!UnloadManager::IsUnloadRequested())
            ShutdownMainNoThrow();

        if (g_ExceptionHandler)
        {
            RemoveVectoredExceptionHandler(g_ExceptionHandler);
            g_ExceptionHandler = nullptr;
        }
        break;
    }
    }
    return TRUE;
}
