#include "GameThreadHook.h"
#include "../Utils/Logger.h"
#include "../SDK/SDKESPFunctions.h"
#include <cstring>
#include <cstdint>
#include <Windows.h>
#include <fstream>

namespace GameThreadHook
{
	// ProcessEvent signature
	typedef void(__fastcall* ProcessEventFn)(void*, void*, void*);
	static ProcessEventFn g_OriginalProcessEvent = nullptr;

	// VMT Shadow Hook variables
	static uintptr_t* g_ShadowVMT = nullptr;  // Copy of hooked VTable
	static uintptr_t* g_OriginalVMT = nullptr;  // Original VTable
	static bool g_HookActive = false;
	static bool g_HacksInitialized = false;

	// Debug logging
	static void DebugLog(const std::string& msg)
	{
		std::ofstream log("c:\\temp\\GameThreadHook_Debug.log", std::ios::app);
		if (log.is_open()) {
			log << msg << "\n";
			log.close();
		}
	}

	// Hooked ProcessEvent
	static void __fastcall HookedProcessEvent(void* Object, void* Function, void* Parms)
	{
		// Call original ALWAYS
		if (g_OriginalProcessEvent)
			g_OriginalProcessEvent(Object, Function, Parms);

		// Initialize hacks once in game thread
		if (!g_HacksInitialized)
		{
			try
			{
				g_HacksInitialized = true;
				DebugLog("[HookedProcessEvent] ✅ Initializing hacks in game thread...");
				Logger::LogInfo("[GameThreadHook] ✅ Hacks initialized in game thread");
			}
			catch (...)
			{
				DebugLog("[HookedProcessEvent] ❌ Exception during hack initialization");
				Logger::LogError("[GameThreadHook] Exception during hack initialization");
				g_HacksInitialized = false;
			}
		}
	}

	// Implementation: Install VMT Shadow Hook
	bool Initialize()
	{
		DebugLog("[Initialize] Starting initialization...");
		
		try
		{
			// Check if already hooked
			if (g_HookActive)
			{
				DebugLog("[Initialize] Hook already active");
				return true;
			}

			// Get GWorld
			DebugLog("[Initialize] Getting GWorld...");
			SDK::UWorld* GWorld = SDK_GetWorld();
			if (!GWorld)
			{
				DebugLog("[Initialize] ❌ GWorld is NULL - retry next frame");
				Logger::LogWarning("[GameThreadHook] GWorld not ready yet");
				return false;
			}

			DebugLog("[Initialize] ✅ GWorld obtained");

			// Get VTable from GWorld
			uintptr_t** vmt_ptr = reinterpret_cast<uintptr_t**>(GWorld);
			g_OriginalVMT = *vmt_ptr;
			if (!g_OriginalVMT)
			{
				DebugLog("[Initialize] ❌ VTable pointer is NULL");
				Logger::LogWarning("[GameThreadHook] VTable pointer is NULL");
				return false;
			}

			DebugLog("[Initialize] ✅ VTable obtained");

			// Allocate shadow VTable (copy of original)
			// VTable size estimation: typically 200-300 entries
			const size_t VMT_SIZE = 256;  // Safe estimation
			g_ShadowVMT = new uintptr_t[VMT_SIZE];
			memcpy(g_ShadowVMT, g_OriginalVMT, VMT_SIZE * sizeof(uintptr_t));

			DebugLog("[Initialize] ✅ Shadow VMT allocated and copied");

			// Get ProcessEvent function from original VMT
			// ProcessEvent is typically at index 0x44 (68 in decimal)
			const int PROCESSEVENT_INDEX = 0x44;
			g_OriginalProcessEvent = (ProcessEventFn)g_OriginalVMT[PROCESSEVENT_INDEX];

			DebugLog("[Initialize] ✅ Original ProcessEvent at index 0x44 stored");

			// Replace ProcessEvent in shadow VMT with our hook
			g_ShadowVMT[PROCESSEVENT_INDEX] = reinterpret_cast<uintptr_t>(&HookedProcessEvent);

			DebugLog("[Initialize] ✅ Shadow VMT patched with HookedProcessEvent");

			// Replace GWorld's VTable pointer with shadow VTable
			*vmt_ptr = g_ShadowVMT;

			DebugLog("[Initialize] ✅ GWorld VTable pointer replaced with shadow VMT");

			g_HookActive = true;
			DebugLog("[Initialize] ✅✅✅ SUCCESS: Hook installed");
			Logger::LogInfo("[GameThreadHook] ✅ VMT Shadow Hook installed successfully");

			return true;
		}
		catch (const std::exception& e)
		{
			DebugLog(std::string("[Initialize] ❌ Exception: ") + e.what());
			Logger::LogError(std::string("[GameThreadHook] Init exception: ") + e.what());
			return false;
		}
		catch (...)
		{
			DebugLog("[Initialize] ❌ Unknown exception");
			Logger::LogError("[GameThreadHook] Init unknown exception");
			return false;
		}
	}

	void Shutdown()
	{
		DebugLog("[Shutdown] Shutting down...");
		
		if (g_ShadowVMT)
		{
			delete[] g_ShadowVMT;
			g_ShadowVMT = nullptr;
		}

		g_HookActive = false;
		g_HacksInitialized = false;
		g_OriginalProcessEvent = nullptr;

		DebugLog("[Shutdown] Shutdown complete");
		Logger::LogInfo("[GameThreadHook] Shutdown complete");
	}

	bool IsHookActive()
	{
		return g_HookActive;
	}
}
