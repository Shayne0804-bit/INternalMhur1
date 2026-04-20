#pragma once

#include <Windows.h>
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CoreUObject_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"

using namespace SDK;

namespace SDKInit
{
	/// <summary>
	/// Initialize the SDK - must be called before any SDK operations
	/// This sets up access to game memory and validates offsets
	/// </summary>
	bool Initialize();

	/// <summary>
	/// Check if SDK is initialized and ready for use
	/// </summary>
	bool IsInitialized();

	/// <summary>
	/// Shutdown SDK systems
	/// </summary>
	void Shutdown();

	/// <summary>
	/// Get the game world instance
	/// Returns nullptr if not found
	/// </summary>
	UWorld* GetGameWorld();

	/// <summary>
	/// Validate that we can access game objects
	/// </summary>
	bool ValidateGameAccess();
}
