#pragma once

#include <cstdint>

namespace GameThreadHook
{
	// Initialize the game thread hook (call from Main.cpp)
	// Hooks ProcessEvent to initialize hacks safely in game thread
	bool Initialize();

	// Shutdown the game thread hook
	void Shutdown();

	// Get if hook is active
	bool IsHookActive();
}
