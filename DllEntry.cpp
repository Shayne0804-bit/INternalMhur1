#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <fstream>
#include "src/Core/Main.h"
#include "src/Core/UnloadManager.h"
#include "src/Hooks/D3D11Hook.h"
#include "src/Hooks/GameThreadHook.h"
#include "src/Hacks/HackThread.h"
#include <atomic>
#include <cstdint>

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
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

#if RUGIR_ENABLE_UNLOAD
DWORD WINAPI UnloadThread(LPVOID lpParam)
{
    HMODULE module = reinterpret_cast<HMODULE>(lpParam);

    for (int i = 0; i < 4000 && g_MainThreadRunning.load(std::memory_order_acquire); ++i)
        Sleep(1);

    try
    {
        HackThreadManager::GetInstance().Shutdown();
    }
    catch (...)
    {
    }

    try
    {
        GameThreadHook::Shutdown();
    }
    catch (...)
    {
    }

    try
    {
        D3D11Hook::RestoreHookOnly();
    }
    catch (...)
    {
    }

    for (int i = 0; i < 5000 && !D3D11Hook::IsSafeToUnload(); ++i)
        Sleep(1);

    try
    {
        D3D11Hook::ReleaseResourcesForUnload();
    }
    catch (...)
    {
    }

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
    g_MainThreadRunning.store(true, std::memory_order_release);
    g_hModule = Module;
    ApplyLibProtProtection(Module);
    
    /* Console completely disabled - no logging output */
    // AllocConsole removed - console window will not appear
    // All printf statements disabled
    // All file logging disabled
    
    /* Wait for game initialization */
    Sleep(3000);

    if (UnloadManager::IsUnloadRequested())
    {
        g_MainThreadRunning.store(false, std::memory_order_release);
        return 0;
    }

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
        /* Cleanup resources */
        if (!UnloadManager::IsUnloadRequested())
            Main::Shutdown();
        break;
    }
    }
    return TRUE;
}
