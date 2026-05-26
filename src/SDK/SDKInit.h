#pragma once

#include <Windows.h>
#include <vector>
#include <string>
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

	/// <summary>
	/// Log all existing pawns to c:/temp/pawns.log
	/// </summary>
	void LogExistingPawns();

	/// <summary>
	/// Spawn a new CharacterBattle pawn and possess it with the player controller
	/// Returns the spawned APawn, or nullptr if spawn failed
	/// </summary>
	APawn* SpawnCharacterBattle(const FVector& SpawnLocation);

	/// <summary>
	/// Enumerate all available character classes (Ch001, Ch008, Ch046, etc)
	/// Fills the internal class list
	/// </summary>
	void EnumerateAvailableCharacterClasses();

	/// <summary>
	/// Get list of available character class names
	/// Must call EnumerateAvailableCharacterClasses first
	/// </summary>
	const std::vector<std::string>& GetAvailableCharacterClasses();

	/// <summary>
	/// Find existing CharacterBattle pawn in the game world
	/// </summary>
	APawn* FindExistingCharacterBattle();
	/// <summary>
	/// Create a new PlayerController for possessing pawns
	/// </summary>
	APlayerController* CreateNewPlayerController(UWorld* World);
	/// <summary>
	/// Spawn a pawn of specified class and possess it
	/// ClassName should be from GetAvailableCharacterClasses()
	/// </summary>
	APawn* SpawnCharacterByClass(const std::string& ClassName, const FVector& SpawnLocation);
}
