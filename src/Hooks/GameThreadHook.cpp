#include "GameThreadHook.h"
#include "../SDK/SDKInit.h"
#include "../Hacks/InGameModuleHacks.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <cstdarg>
#include <atomic>
#include <Windows.h>
#include "../Core/UnloadManager.h"

// Forward declarations
namespace UE4 {
    class UObject;
    class UFunction;
    class APlayerController;
}

// Get PlayerController
typedef void* (*GetPlayerControllerFn)();
extern "C" void* SDK_GetPlayerController();

namespace GameThreadHook
{
	static bool g_Initialized = false;
	static std::ofstream g_LogFile;
	static constexpr bool GAME_THREAD_DEBUG_LOGGING = false;
	static std::atomic<int> g_ProcessEventCallDepth(0);

	// Logging helper
	void Log(const char* format, ...)
	{
		if (!GAME_THREAD_DEBUG_LOGGING)
			return;

		if (!g_LogFile.is_open())
		{
			g_LogFile.open("c:\\temp\\gamethread.log", std::ios::app);
		}

		char buffer[1024];
		va_list args;
		va_start(args, format);
		vsnprintf_s(buffer, sizeof(buffer), format, args);
		va_end(args);

		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		char timebuf[32];
		ctime_s(timebuf, sizeof(timebuf), &time);
		timebuf[strlen(timebuf) - 1] = '\0'; // Remove newline

		g_LogFile << "[" << timebuf << "] " << buffer << std::endl;
		g_LogFile.flush();

		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}

	// Utility to check valid pointer
	inline bool ValidPtr(void* ptr)
	{
		return ptr && (uintptr_t)ptr > 0x10000;
	}

	// ProcessEvent Interceptor with VMT Shadowing
	class ProcessEventInterceptor
	{
	public:
		typedef void(*ProcessEventFn)(void*, class UE4::UFunction*, void*);

	private:
		uintptr_t* m_vtable = nullptr;
		uintptr_t* m_vtable_cache = nullptr;
		int m_vtablesize = -1;
		int m_eventindex = -1;
		uintptr_t m_class = 0;
		ProcessEventFn m_original_processevent = nullptr;

		static constexpr int INDEX_PROCESSEVENT = 0x44; // UE4 default index
		static constexpr uintptr_t ProcessEventOffset = 0x019F76A0; // Will be scanned

	public:
		ProcessEventInterceptor() = default;

		// Get VTable size by scanning for null entries
		int GetVTableSize()
		{
			if (!ValidPtr((void*)m_vtable))
				return -1;

			int count = 0;
			for (int i = 0; i < 512; i++)
			{
				auto entry = m_vtable[i];
				if (!ValidPtr((void*)entry))
					break;
				count++;
			}

			Log("[VMT] VTable size: %d", count);
			return count;
		}

		// Find ProcessEvent index by brute-force scanning
		int FindProcessEventIndex()
		{
			if (!ValidPtr((void*)m_vtable) || m_vtablesize == -1)
				return -1;

			// Try default index first
			auto func = m_vtable[INDEX_PROCESSEVENT];
			if (ValidPtr((void*)func))
			{
				Log("[VMT] Found ProcessEvent at default index: %d", INDEX_PROCESSEVENT);
				return INDEX_PROCESSEVENT;
			}

			// Fallback
			Log("[VMT] ProcessEvent not found, using known index: %d", INDEX_PROCESSEVENT);
			return INDEX_PROCESSEVENT;
		}

		// Get object name for debugging
		std::string GetObjectName(uintptr_t pClass)
		{
			try
			{
				// UObject::Name at offset 0x18 (FName structure)
				uintptr_t nameAddr = pClass + 0x18;
				return "Object_0x" + std::to_string(pClass);
			}
			catch (...)
			{
				return "Error";
			}
		}

		// Apply Shadow VMT Hook
		void ApplyHook(uintptr_t pClass, ProcessEventFn pFunc)
		{
			if (m_eventindex == -1 || m_vtablesize == -1)
			{
				Log("[VMT] Cannot apply hook: invalid state (eventindex=%d, vtablesize=%d)", m_eventindex, m_vtablesize);
				return;
			}

			if (pClass != m_class)
			{
				this->m_vtable = *reinterpret_cast<uintptr_t**>(pClass);

				if (!ValidPtr((void*)m_vtable))
				{
					Log("[VMT] Invalid vtable pointer: 0x%llX", (uintptr_t)m_vtable);
					return;
				}

				// Allocate new shadow vtable
				this->m_vtable_cache = reinterpret_cast<uintptr_t*>(malloc(m_vtablesize * 0x8));

				if (!m_vtable_cache)
				{
					Log("[VMT] Failed to allocate vtable cache (%d * 8 bytes)", m_vtablesize);
					return;
				}

				Log("[VMT] Allocated shadow vtable: 0x%llX (size: %d entries)", (uintptr_t)m_vtable_cache, m_vtablesize);

				// Copy original vtable
				memcpy(m_vtable_cache, m_vtable, m_vtablesize * 0x8);
				Log("[VMT] Copied original vtable to cache");

				// Save original ProcessEvent function
				m_original_processevent = (ProcessEventFn)m_vtable_cache[m_eventindex];
				Log("[VMT] Original ProcessEvent at index %d: 0x%llX", m_eventindex, (uintptr_t)m_original_processevent);

				// Replace ProcessEvent entry with our hook
				m_vtable_cache[m_eventindex] = reinterpret_cast<uintptr_t>(pFunc);
				Log("[VMT] Replaced ProcessEvent with hook: 0x%llX", reinterpret_cast<uintptr_t>(pFunc));

				// Point object to new shadow vtable
				*(uintptr_t**)pClass = m_vtable_cache;
				Log("[VMT] Redirected object 0x%llX to shadow vtable", pClass);

				this->m_class = pClass;
				Log("[VMT] Hook applied successfully to object: %s (0x%llX)", GetObjectName(pClass).c_str(), pClass);
			}
		}

		void Initialize(uintptr_t pClass)
		{
			Log("[VMT] Initializing ProcessEventInterceptor for object: 0x%llX", pClass);

			if (pClass < 0x10000)
			{
				Log("[VMT] Invalid class pointer: 0x%llX", pClass);
				return;
			}

			this->m_vtable = *(uintptr_t**)pClass;
			this->m_vtablesize = GetVTableSize();
			this->m_eventindex = FindProcessEventIndex();

			Log("[VMT] Initialized: vtable=0x%llX, vtable_size=%d, PE_index=%d", 
				(uintptr_t)m_vtable, m_vtablesize, m_eventindex);
		}

		ProcessEventFn GetOriginalProcessEvent() const
		{
			return m_original_processevent;
		}

		void RestoreHook()
		{
			if (!m_class || !m_vtable || !m_vtable_cache)
				return;

			__try
			{
				uintptr_t** objectVTable = reinterpret_cast<uintptr_t**>(m_class);
				if (objectVTable && *objectVTable == m_vtable_cache)
				{
					*objectVTable = m_vtable;
					Log("[VMT] Restored original vtable for object: 0x%llX", m_class);
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				Log("[VMT] Exception while restoring original vtable");
			}
		}

		void NeutralizeHookNoFree()
		{
			if (m_vtable_cache && m_eventindex >= 0 && m_original_processevent)
			{
				m_vtable_cache[m_eventindex] = reinterpret_cast<uintptr_t>(m_original_processevent);
			}

			RestoreHook();
		}

		void FreeCache()
		{
			RestoreHook();

			if (m_vtable_cache)
			{
				Log("[VMT] Freeing shadow vtable cache: 0x%llX", (uintptr_t)m_vtable_cache);
				free(m_vtable_cache);
				m_vtable_cache = nullptr;
			}

			m_vtable = nullptr;
			m_vtablesize = -1;
			m_eventindex = -1;
			m_class = 0;
			m_original_processevent = nullptr;
		}

		~ProcessEventInterceptor()
		{
			if (UnloadManager::IsUnloadRequested())
			{
				NeutralizeHookNoFree();
				return;
			}

			FreeCache();
		}
	};

	// Global interceptor instance
	static ProcessEventInterceptor g_interceptor;

	// Hooked ProcessEvent function
	void HookedProcessEvent(void* pThis, class UE4::UFunction* pFunction, void* pParams)
	{
		struct ProcessEventScope
		{
			ProcessEventScope()
			{
				g_ProcessEventCallDepth.fetch_add(1, std::memory_order_acq_rel);
			}

			~ProcessEventScope()
			{
				g_ProcessEventCallDepth.fetch_sub(1, std::memory_order_acq_rel);
			}
		} processEventScope;

		// Call original ProcessEvent
		auto original = g_interceptor.GetOriginalProcessEvent();
		if (original)
		{
			original(pThis, pFunction, pParams);
		}
		else
		{
			Log("[HOOK] ERROR: Original ProcessEvent is null!");
			return;
		}

		// ===== CALL SDK FEATURES ON GAME THREAD =====
	}

	// Initialize - Hook ProcessEvent using VMT Shadowing
	bool Initialize()
	{
		if (g_Initialized)
		{
			Log("[Init] Already initialized");
			return true;
		}

		Log("[Init] Starting GameThreadHook initialization...");

		try
		{
			// Initialize SDKInit module first
			Log("[Init] Initializing SDKInit module...");
			if (SDKInit::Initialize())
			{
				Log("[Init] ✅ SDKInit initialized successfully!");
			}
			else
			{
				Log("[Init] ⚠️  SDKInit initialization failed (spawn may not work)");
			}

			// ========== CRITICAL: GET PLAYERCONTROLLER AND HOOK IT ==========
			Log("[Init] Attempting to get PlayerController...");
			void* pPlayerController = SDK_GetPlayerController();

			if (!pPlayerController || (uintptr_t)pPlayerController < 0x10000)
			{
				Log("[Init] ⚠️  PlayerController not available yet (game may still be loading)");
				Log("[Init] VMT hook will be set up on next Initialize() call");
				// Don't mark as initialized - let it retry next frame
				return false;
			}

			Log("[Init] ✅ PlayerController obtained: 0x%llX", (uintptr_t)pPlayerController);
			Log("[Init] 🔧 Applying VMT shadowing hook to PlayerController...");
			
			// NOW ACTUALLY HOOK THE PLAYERCONTROLLER
			HookProcessEvent((uintptr_t)pPlayerController);
			Log("[Init] ✅ PlayerController hooked successfully!");

			// ========== TEST TRAINING HACKS ==========
			DWORD threadId = GetCurrentThreadId();
			Log("[Training] Current Thread ID: 0x%X (%u)", threadId, threadId);
			Log("[Training] Calling TrainingHack_ChangeCharacterOnServer from GameThreadHook (Thread 0x%X)...", threadId);
			
			// Note: This is just for testing/logging. In production, this would be called from the game
			// when needed, not during initialization.

			g_Initialized = true;
			Log("[Init] GameThreadHook initialization complete - VMT shadowing hook ACTIVE");
			return true;
		}
		catch (const std::exception& e)
		{
			Log("[Init] Exception during initialization: %s", e.what());
			return false;
		}
	}

	// Shutdown
	void Shutdown()
	{
		Log("[Shutdown] Shutting down GameThreadHook...");
		SDKInit::Shutdown();
		g_interceptor.NeutralizeHookNoFree();

		for (int i = 0; i < 200 && g_ProcessEventCallDepth.load(std::memory_order_acquire) > 0; ++i)
			Sleep(1);

		if (!UnloadManager::IsUnloadRequested())
			g_interceptor.FreeCache();

		g_Initialized = false;
		
		if (g_LogFile.is_open())
		{
			g_LogFile.close();
		}
		Log("[Shutdown] Done");
	}

	// IsHookActive
	bool IsHookActive()
	{
		return g_Initialized;
	}

	// Hook ProcessEvent on a specific object
	void HookProcessEvent(uintptr_t pObjectAddress)
	{
		Log("[HookPE] Attempting to hook ProcessEvent on object: 0x%llX", pObjectAddress);

		try
		{
			g_interceptor.Initialize(pObjectAddress);
			g_interceptor.ApplyHook(pObjectAddress, &HookedProcessEvent);
			Log("[HookPE] Successfully hooked ProcessEvent");
		}
		catch (const std::exception& e)
		{
			Log("[HookPE] Exception: %s", e.what());
		}
	}
}
