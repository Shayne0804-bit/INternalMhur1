#pragma once

#include <cstdint>
#include <functional>

namespace GameThreadHook
{
	// Debug console/file-light logging for this hook only.
	void Log(const char* format, ...);

	// Initialize the game thread hook with VMT shadowing ProcessEvent hook
	// Logs all activity to c:\temp\gamethread.log
	bool Initialize();

	// Shutdown the game thread hook and free resources
	void Shutdown();

	// Get if hook is active
	bool IsHookActive();

	// Fallback frame tick when ProcessEvent is quiet.
	void TickFrameHacks();

	// Queue work that must run from the game's ProcessEvent thread.
	bool EnqueueTask(std::function<void()> task);

	// Hook ProcessEvent on a specific object using VMT shadowing
	void HookProcessEvent(uintptr_t pObjectAddress);
}
