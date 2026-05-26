#pragma once

#include <cstdint>

namespace GameThreadHook
{
	// Initialize the game thread hook with VMT shadowing ProcessEvent hook
	// Logs all activity to c:\temp\gamethread.log
	bool Initialize();

	// Shutdown the game thread hook and free resources
	void Shutdown();

	// Get if hook is active
	bool IsHookActive();

	// Hook ProcessEvent on a specific object using VMT shadowing
	void HookProcessEvent(uintptr_t pObjectAddress);
}
