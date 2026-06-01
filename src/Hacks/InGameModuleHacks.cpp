#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "InGameModuleHacks.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/OutGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/BackendSubsystem_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../Utils/Logger.h"
#include "../Menu/ImGuiMenu.h"
#include <set>
#include <map>
#include <cctype>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <ctime>

// ============================================
// EXTERNAL DECLARATIONS FROM BASIC.CPP
// ============================================

// Forward declarations from Basic.cpp
extern "C" SDK::APlayerController* SDK_GetPlayerController();
extern "C" SDK::AActor* SDK_GetRandomESPTarget();  // Returns random ACharacterBattle as AActor*
extern "C" SDK::AActor* SDK_GetForwardESPTarget();  // Returns closest ACharacterBattle in front as AActor*
extern "C" const char* SDK_GetPlayerName(void* PlayerState);  // Get player name from PlayerState

// UTILITY FUNCTIONS
// ============================================

// Offsets for PlayerState (from Basic.cpp)
#define OFFSET_PLAYERSTATE 0x6E0          // AActor::PlayerState
#define OFFSET_PLAYERSTATE_IFDYING 1416   // Dying flag at +1400

/**
 * Check if a character is dying using PlayerState offset
 * Same method as Basic.cpp to avoid crashes
 */
static inline bool IsCharacterDyingOffset(SDK::ACharacterBattle* Character)
{
    if (!Character) return false;
    try
    {
        // Read PlayerState pointer from Character
        void* PlayerState = *(void**)((uintptr_t)Character + OFFSET_PLAYERSTATE);
        if (!PlayerState) return false;

        // Check dying flag at offset 1400 (bit 2)
        bool isDying = (*(uint8_t*)((uintptr_t)PlayerState + OFFSET_PLAYERSTATE_IFDYING) & 2) != 0;
        return isDying;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Check if current game mode is a valid battle mode (2-9)
 * Valid modes: SOLO_BATTLE(2), DUO_BATTLE(3), SQUAD_BATTLE(4), LEADERS_BATTLE(5),
 *             DOMINATION_BATTLE(6), SOLOPICK_BATTLE(7), TUTORIAL(8), TRAINING(9)
 */
bool IsValidBattleMode()
{
    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world)
        {
            Logger::LogError("[IsValidBattleMode] Could not get UWorld");
            return false;
        }

        // Get GameState
        SDK::AHerovsGameState* gameState = static_cast<SDK::AHerovsGameState*>(world->GameState);
        if (!gameState)
        {
            Logger::LogError("[IsValidBattleMode] Could not get AHerovsGameState");
            return false;
        }

        // Get GameModeType using public method (no offsets)
        SDK::EGameModeType modeType = gameState->GetGameModeType();

        // Check if valid battle mode (2-9) - cast to int for comparison
        int modeValue = (int)modeType;
        bool isValid = (modeValue >= 2 && modeValue <= 9);
        
        if (!isValid)
        {
            Logger::LogWarning("[IsValidBattleMode] Not in valid battle mode. Type: " + std::to_string(modeValue));
        }

        return isValid;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[IsValidBattleMode] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[IsValidBattleMode] Unknown exception");
        return false;
    }
}

/**
 * Validate if a pointer is safe to access
 * Checks for NULL and obviously invalid addresses
 */
template<typename T>
static inline bool IsValidPointer(T* ptr)
{
    if (!ptr) return false;
    
    // Check if address is obviously invalid (too low or misaligned)
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    
    // Reject addresses below 0x10000 (user space guard pages)
    if (addr < 0x10000) return false;
    
    // Reject completely misaligned pointers (likely garbage)
    if (addr & 0xF) return false;  // Should be 16-byte aligned for UE4 objects
    
    return true;
}

/**
 * Safe getter for player pawn with validation
 */
static inline SDK::ACharacterBattle* GetPlayerCharacterBattle(SDK::APlayerController* playerController)
{
    if (!IsValidPointer(playerController)) return nullptr;
    
    SDK::APawn* pawn = playerController->Pawn;
    if (!IsValidPointer(pawn)) return nullptr;
    
    SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(pawn);
    if (!IsValidPointer(character)) return nullptr;
    
    return character;
}

/**
 * Safe getter for player state with validation
 */
static inline SDK::APlayerStateBattle* GetPlayerStateBattle(SDK::APlayerController* playerController)
{
    if (!IsValidPointer(playerController)) return nullptr;
    
    SDK::APlayerState* playerState = playerController->PlayerState;
    if (!IsValidPointer(playerState)) return nullptr;
    
    SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
    if (!IsValidPointer(playerStateBattle)) return nullptr;
    
    return playerStateBattle;
}

/**
 * Get list of all available variation character IDs as enums
 * Returns all EVariationCharacterId enums in order: Ch001_Variation0, Ch001_Variation1, Ch002_Variation0, etc.
 * This is the direct enum list - no conversion needed!
 */
std::vector<SDK::EVariationCharacterId> GetAllVariationCharacterIds()
{
    static std::vector<SDK::EVariationCharacterId> allIds = {
        // Ch001: 2 variations
        SDK::EVariationCharacterId::Ch001_Variation0,
        SDK::EVariationCharacterId::Ch001_Variation1,
        
        // Ch002: 3 variations
        SDK::EVariationCharacterId::Ch002_Variation0,
        SDK::EVariationCharacterId::Ch002_Variation1,
        SDK::EVariationCharacterId::Ch002_Variation2,
        
        // Ch003: 2 variations
        SDK::EVariationCharacterId::Ch003_Variation0,
        SDK::EVariationCharacterId::Ch003_Variation1,
        
        // Ch004: 2 variations
        SDK::EVariationCharacterId::Ch004_Variation0,
        SDK::EVariationCharacterId::Ch004_Variation1,
        
        // Ch005: 2 variations
        SDK::EVariationCharacterId::Ch005_Variation0,
        SDK::EVariationCharacterId::Ch005_Variation1,
        
        // Ch006: 2 variations
        SDK::EVariationCharacterId::Ch006_Variation0,
        SDK::EVariationCharacterId::Ch006_Variation1,
        
        // Ch007: 2 variations
        SDK::EVariationCharacterId::Ch007_Variation0,
        SDK::EVariationCharacterId::Ch007_Variation1,
        
        // Ch008: 2 variations
        SDK::EVariationCharacterId::Ch008_Variation0,
        SDK::EVariationCharacterId::Ch008_Variation1,
        
        // Ch010: 2 variations
        SDK::EVariationCharacterId::Ch010_Variation0,
        SDK::EVariationCharacterId::Ch010_Variation1,
        
        // Ch011: 2 variations
        SDK::EVariationCharacterId::Ch011_Variation0,
        SDK::EVariationCharacterId::Ch011_Variation1,
        
        // Ch012: 2 variations
        SDK::EVariationCharacterId::Ch012_Variation0,
        SDK::EVariationCharacterId::Ch012_Variation1,
        
        // Ch013: 2 variations
        SDK::EVariationCharacterId::Ch013_Variation0,
        SDK::EVariationCharacterId::Ch013_Variation1,
        
        // Ch015: 3 variations
        SDK::EVariationCharacterId::Ch015_Variation0,
        SDK::EVariationCharacterId::Ch015_Variation1,
        SDK::EVariationCharacterId::Ch015_Variation2,
        
        // Ch016: 2 variations
        SDK::EVariationCharacterId::Ch016_Variation0,
        SDK::EVariationCharacterId::Ch016_Variation1,
        
        // Ch017: 2 variations
        SDK::EVariationCharacterId::Ch017_Variation0,
        SDK::EVariationCharacterId::Ch017_Variation1,
        
        // Ch018: 2 variations
        SDK::EVariationCharacterId::Ch018_Variation0,
        SDK::EVariationCharacterId::Ch018_Variation1,
        
        // Ch023: 2 variations
        SDK::EVariationCharacterId::Ch023_Variation0,
        SDK::EVariationCharacterId::Ch023_Variation1,
        
        // Ch024: 2 variations
        SDK::EVariationCharacterId::Ch024_Variation0,
        SDK::EVariationCharacterId::Ch024_Variation1,
        
        // Ch025: 2 variations
        SDK::EVariationCharacterId::Ch025_Variation0,
        SDK::EVariationCharacterId::Ch025_Variation1,
        
        // Ch026: 2 variations
        SDK::EVariationCharacterId::Ch026_Variation0,
        SDK::EVariationCharacterId::Ch026_Variation1,
        
        // Ch034: 2 variations
        SDK::EVariationCharacterId::Ch034_Variation0,
        SDK::EVariationCharacterId::Ch034_Variation1,
        
        // Ch037: 2 variations
        SDK::EVariationCharacterId::Ch037_Variation0,
        SDK::EVariationCharacterId::Ch037_Variation1,
        
        // Ch038: 2 variations
        SDK::EVariationCharacterId::Ch038_Variation0,
        SDK::EVariationCharacterId::Ch038_Variation1,
        
        // Ch043: 2 variations
        SDK::EVariationCharacterId::Ch043_Variation0,
        SDK::EVariationCharacterId::Ch043_Variation1,
        
        // Ch046: 2 variations
        SDK::EVariationCharacterId::Ch046_Variation0,
        SDK::EVariationCharacterId::Ch046_Variation1,
        
        // Ch100: 2 variations
        SDK::EVariationCharacterId::Ch100_Variation0,
        SDK::EVariationCharacterId::Ch100_Variation1,
        
        // Ch101: 2 variations
        SDK::EVariationCharacterId::Ch101_Variation0,
        SDK::EVariationCharacterId::Ch101_Variation1,
        
        // Ch102: 2 variations
        SDK::EVariationCharacterId::Ch102_Variation0,
        SDK::EVariationCharacterId::Ch102_Variation1,
        
        // Ch103: 2 variations
        SDK::EVariationCharacterId::Ch103_Variation0,
        SDK::EVariationCharacterId::Ch103_Variation1,
        
        // Ch104: 2 variations
        SDK::EVariationCharacterId::Ch104_Variation0,
        SDK::EVariationCharacterId::Ch104_Variation1,
        
        // Ch105: 2 variations
        SDK::EVariationCharacterId::Ch105_Variation0,
        SDK::EVariationCharacterId::Ch105_Variation1,
        
        // Ch109: 2 variations
        SDK::EVariationCharacterId::Ch109_Variation0,
        SDK::EVariationCharacterId::Ch109_Variation1,
        
        // Ch111: 2 variations
        SDK::EVariationCharacterId::Ch111_Variation0,
        SDK::EVariationCharacterId::Ch111_Variation1,
        
        // Ch114: 2 variations
        SDK::EVariationCharacterId::Ch114_Variation0,
        SDK::EVariationCharacterId::Ch114_Variation1,
        
        // Ch115: 2 variations
        SDK::EVariationCharacterId::Ch115_Variation0,
        SDK::EVariationCharacterId::Ch115_Variation1,
        
        // Ch200: 2 variations
        SDK::EVariationCharacterId::Ch200_Variation0,
        SDK::EVariationCharacterId::Ch200_Variation1,
        
        // Ch201: 2 variations
        SDK::EVariationCharacterId::Ch201_Variation0,
        SDK::EVariationCharacterId::Ch201_Variation1,
        
        // Ch202: 2 variations
        SDK::EVariationCharacterId::Ch202_Variation0,
        SDK::EVariationCharacterId::Ch202_Variation1,
    };
    
    return allIds;
}

// ============================================
// CHARACTER VARIATION MAPPING
// ============================================

/**
 * Convert ECharacterId + variation index to EVariationCharacterId
 * @param characterId - The character ID
 * @param variationIndex - The variation index (0, 1, 2, etc.)
 * @return The corresponding EVariationCharacterId
 */
SDK::EVariationCharacterId GetVariationCharacterId(SDK::ECharacterId characterId, int32_t variationIndex)
{
    // Map (ECharacterId, variationIndex) to EVariationCharacterId
    static std::map<std::pair<SDK::ECharacterId, int32_t>, SDK::EVariationCharacterId> variationMap = {
        // Ch001: 2 variations
        { {SDK::ECharacterId::Ch001, 0}, SDK::EVariationCharacterId::Ch001_Variation0 },
        { {SDK::ECharacterId::Ch001, 1}, SDK::EVariationCharacterId::Ch001_Variation1 },
        
        // Ch002: 3 variations
        { {SDK::ECharacterId::Ch002, 0}, SDK::EVariationCharacterId::Ch002_Variation0 },
        { {SDK::ECharacterId::Ch002, 1}, SDK::EVariationCharacterId::Ch002_Variation1 },
        { {SDK::ECharacterId::Ch002, 2}, SDK::EVariationCharacterId::Ch002_Variation2 },
        
        // Ch003: 2 variations
        { {SDK::ECharacterId::Ch003, 0}, SDK::EVariationCharacterId::Ch003_Variation0 },
        { {SDK::ECharacterId::Ch003, 1}, SDK::EVariationCharacterId::Ch003_Variation1 },
        
        // Ch004: 2 variations
        { {SDK::ECharacterId::Ch004, 0}, SDK::EVariationCharacterId::Ch004_Variation0 },
        { {SDK::ECharacterId::Ch004, 1}, SDK::EVariationCharacterId::Ch004_Variation1 },
        
        // Ch005: 2 variations
        { {SDK::ECharacterId::Ch005, 0}, SDK::EVariationCharacterId::Ch005_Variation0 },
        { {SDK::ECharacterId::Ch005, 1}, SDK::EVariationCharacterId::Ch005_Variation1 },
        
        // Ch006: 2 variations
        { {SDK::ECharacterId::Ch006, 0}, SDK::EVariationCharacterId::Ch006_Variation0 },
        { {SDK::ECharacterId::Ch006, 1}, SDK::EVariationCharacterId::Ch006_Variation1 },
        
        // Ch007: 2 variations
        { {SDK::ECharacterId::Ch007, 0}, SDK::EVariationCharacterId::Ch007_Variation0 },
        { {SDK::ECharacterId::Ch007, 1}, SDK::EVariationCharacterId::Ch007_Variation1 },
        
        // Ch008: 2 variations
        { {SDK::ECharacterId::Ch008, 0}, SDK::EVariationCharacterId::Ch008_Variation0 },
        { {SDK::ECharacterId::Ch008, 1}, SDK::EVariationCharacterId::Ch008_Variation1 },
        
        // Ch010: 2 variations
        { {SDK::ECharacterId::Ch010, 0}, SDK::EVariationCharacterId::Ch010_Variation0 },
        { {SDK::ECharacterId::Ch010, 1}, SDK::EVariationCharacterId::Ch010_Variation1 },
        
        // Ch011: 2 variations
        { {SDK::ECharacterId::Ch011, 0}, SDK::EVariationCharacterId::Ch011_Variation0 },
        { {SDK::ECharacterId::Ch011, 1}, SDK::EVariationCharacterId::Ch011_Variation1 },
        
        // Ch012: 2 variations
        { {SDK::ECharacterId::Ch012, 0}, SDK::EVariationCharacterId::Ch012_Variation0 },
        { {SDK::ECharacterId::Ch012, 1}, SDK::EVariationCharacterId::Ch012_Variation1 },
        
        // Ch013: 2 variations
        { {SDK::ECharacterId::Ch013, 0}, SDK::EVariationCharacterId::Ch013_Variation0 },
        { {SDK::ECharacterId::Ch013, 1}, SDK::EVariationCharacterId::Ch013_Variation1 },
        
        // Ch015: 3 variations
        { {SDK::ECharacterId::Ch015, 0}, SDK::EVariationCharacterId::Ch015_Variation0 },
        { {SDK::ECharacterId::Ch015, 1}, SDK::EVariationCharacterId::Ch015_Variation1 },
        { {SDK::ECharacterId::Ch015, 2}, SDK::EVariationCharacterId::Ch015_Variation2 },
        
        // Ch016: 2 variations
        { {SDK::ECharacterId::Ch016, 0}, SDK::EVariationCharacterId::Ch016_Variation0 },
        { {SDK::ECharacterId::Ch016, 1}, SDK::EVariationCharacterId::Ch016_Variation1 },
        
        // Ch017: 2 variations
        { {SDK::ECharacterId::Ch017, 0}, SDK::EVariationCharacterId::Ch017_Variation0 },
        { {SDK::ECharacterId::Ch017, 1}, SDK::EVariationCharacterId::Ch017_Variation1 },
        
        // Ch018: 2 variations
        { {SDK::ECharacterId::Ch018, 0}, SDK::EVariationCharacterId::Ch018_Variation0 },
        { {SDK::ECharacterId::Ch018, 1}, SDK::EVariationCharacterId::Ch018_Variation1 },
        
        // Ch023: 2 variations
        { {SDK::ECharacterId::Ch023, 0}, SDK::EVariationCharacterId::Ch023_Variation0 },
        { {SDK::ECharacterId::Ch023, 1}, SDK::EVariationCharacterId::Ch023_Variation1 },
        
        // Ch024: 2 variations
        { {SDK::ECharacterId::Ch024, 0}, SDK::EVariationCharacterId::Ch024_Variation0 },
        { {SDK::ECharacterId::Ch024, 1}, SDK::EVariationCharacterId::Ch024_Variation1 },
        
        // Ch025: 2 variations
        { {SDK::ECharacterId::Ch025, 0}, SDK::EVariationCharacterId::Ch025_Variation0 },
        { {SDK::ECharacterId::Ch025, 1}, SDK::EVariationCharacterId::Ch025_Variation1 },
        
        // Ch026: 2 variations
        { {SDK::ECharacterId::Ch026, 0}, SDK::EVariationCharacterId::Ch026_Variation0 },
        { {SDK::ECharacterId::Ch026, 1}, SDK::EVariationCharacterId::Ch026_Variation1 },
        
        // Ch034: 2 variations
        { {SDK::ECharacterId::Ch034, 0}, SDK::EVariationCharacterId::Ch034_Variation0 },
        { {SDK::ECharacterId::Ch034, 1}, SDK::EVariationCharacterId::Ch034_Variation1 },
        
        // Ch037: 2 variations
        { {SDK::ECharacterId::Ch037, 0}, SDK::EVariationCharacterId::Ch037_Variation0 },
        { {SDK::ECharacterId::Ch037, 1}, SDK::EVariationCharacterId::Ch037_Variation1 },
        
        // Ch038: 2 variations
        { {SDK::ECharacterId::Ch038, 0}, SDK::EVariationCharacterId::Ch038_Variation0 },
        { {SDK::ECharacterId::Ch038, 1}, SDK::EVariationCharacterId::Ch038_Variation1 },
        
        // Ch043: 2 variations
        { {SDK::ECharacterId::Ch043, 0}, SDK::EVariationCharacterId::Ch043_Variation0 },
        { {SDK::ECharacterId::Ch043, 1}, SDK::EVariationCharacterId::Ch043_Variation1 },
        
        // Ch046: 2 variations
        { {SDK::ECharacterId::Ch046, 0}, SDK::EVariationCharacterId::Ch046_Variation0 },
        { {SDK::ECharacterId::Ch046, 1}, SDK::EVariationCharacterId::Ch046_Variation1 },
        
        // Ch100: 2 variations
        { {SDK::ECharacterId::Ch100, 0}, SDK::EVariationCharacterId::Ch100_Variation0 },
        { {SDK::ECharacterId::Ch100, 1}, SDK::EVariationCharacterId::Ch100_Variation1 },
        
        // Ch101: 2 variations
        { {SDK::ECharacterId::Ch101, 0}, SDK::EVariationCharacterId::Ch101_Variation0 },
        { {SDK::ECharacterId::Ch101, 1}, SDK::EVariationCharacterId::Ch101_Variation1 },
        
        // Ch102: 2 variations
        { {SDK::ECharacterId::Ch102, 0}, SDK::EVariationCharacterId::Ch102_Variation0 },
        { {SDK::ECharacterId::Ch102, 1}, SDK::EVariationCharacterId::Ch102_Variation1 },
        
        // Ch103: 2 variations
        { {SDK::ECharacterId::Ch103, 0}, SDK::EVariationCharacterId::Ch103_Variation0 },
        { {SDK::ECharacterId::Ch103, 1}, SDK::EVariationCharacterId::Ch103_Variation1 },
        
        // Ch104: 2 variations
        { {SDK::ECharacterId::Ch104, 0}, SDK::EVariationCharacterId::Ch104_Variation0 },
        { {SDK::ECharacterId::Ch104, 1}, SDK::EVariationCharacterId::Ch104_Variation1 },
        
        // Ch105: 2 variations
        { {SDK::ECharacterId::Ch105, 0}, SDK::EVariationCharacterId::Ch105_Variation0 },
        { {SDK::ECharacterId::Ch105, 1}, SDK::EVariationCharacterId::Ch105_Variation1 },
        
        // Ch109: 2 variations
        { {SDK::ECharacterId::Ch109, 0}, SDK::EVariationCharacterId::Ch109_Variation0 },
        { {SDK::ECharacterId::Ch109, 1}, SDK::EVariationCharacterId::Ch109_Variation1 },

        { {SDK::ECharacterId::Ch111, 0}, SDK::EVariationCharacterId::Ch109_Variation0 },
        { {SDK::ECharacterId::Ch111, 1}, SDK::EVariationCharacterId::Ch109_Variation1 },

        // Ch114: 2 variations
        { {SDK::ECharacterId::Ch114, 0}, SDK::EVariationCharacterId::Ch114_Variation0 },
        { {SDK::ECharacterId::Ch114, 1}, SDK::EVariationCharacterId::Ch114_Variation1 },
        
        // Ch115: 2 variations
        { {SDK::ECharacterId::Ch115, 0}, SDK::EVariationCharacterId::Ch115_Variation0 },
        { {SDK::ECharacterId::Ch115, 1}, SDK::EVariationCharacterId::Ch115_Variation1 },
        
        // Ch200: 2 variations
        { {SDK::ECharacterId::Ch200, 0}, SDK::EVariationCharacterId::Ch200_Variation0 },
        { {SDK::ECharacterId::Ch200, 1}, SDK::EVariationCharacterId::Ch200_Variation1 },
        
        // Ch201: 2 variations
        { {SDK::ECharacterId::Ch201, 0}, SDK::EVariationCharacterId::Ch201_Variation0 },
        { {SDK::ECharacterId::Ch201, 1}, SDK::EVariationCharacterId::Ch201_Variation1 },
        
        // Ch202: 2 variations
        { {SDK::ECharacterId::Ch202, 0}, SDK::EVariationCharacterId::Ch202_Variation0 },
        { {SDK::ECharacterId::Ch202, 1}, SDK::EVariationCharacterId::Ch202_Variation1 },
    };

    auto it = variationMap.find({characterId, variationIndex});
    if (it != variationMap.end())
    {
        return it->second;
    }
    
    // Default fallback
    return SDK::EVariationCharacterId::UNDEF;
}

/**
 * Get available variations for a character
 * @param characterId - The character to get variations for
 * @return Vector of variation indices (0, 1, 2, etc.)
 */
std::vector<int32_t> GetVariationsForCharacter(SDK::ECharacterId characterId)
{
    // Map each character to their available variations
    // Variation ID is 0 = Variation0, 1 = Variation1, 2 = Variation2, etc.
    static std::map<SDK::ECharacterId, std::vector<int32_t>> variationMap = {
        { SDK::ECharacterId::Ch001, {0, 1} },
        { SDK::ECharacterId::Ch002, {0, 1, 2} },
        { SDK::ECharacterId::Ch003, {0, 1} },
        { SDK::ECharacterId::Ch004, {0, 1} },
        { SDK::ECharacterId::Ch005, {0, 1} },
        { SDK::ECharacterId::Ch006, {0, 1} },
        { SDK::ECharacterId::Ch007, {0, 1} },
        { SDK::ECharacterId::Ch008, {0, 1} },
        { SDK::ECharacterId::Ch010, {0, 1} },
        { SDK::ECharacterId::Ch011, {0, 1} },
        { SDK::ECharacterId::Ch012, {0, 1} },
        { SDK::ECharacterId::Ch013, {0, 1} },
        { SDK::ECharacterId::Ch015, {0, 1, 2} },
        { SDK::ECharacterId::Ch016, {0, 1} },
        { SDK::ECharacterId::Ch017, {0, 1} },
        { SDK::ECharacterId::Ch018, {0, 1} },
        { SDK::ECharacterId::Ch023, {0, 1} },
        { SDK::ECharacterId::Ch024, {0, 1} },
        { SDK::ECharacterId::Ch025, {0, 1} },
        { SDK::ECharacterId::Ch026, {0, 1} },
        { SDK::ECharacterId::Ch034, {0, 1} },
        { SDK::ECharacterId::Ch037, {0, 1} },
        { SDK::ECharacterId::Ch038, {0, 1} },
        { SDK::ECharacterId::Ch043, {0, 1} },
        { SDK::ECharacterId::Ch046, {0, 1} },
        { SDK::ECharacterId::Ch100, {0, 1} },
        { SDK::ECharacterId::Ch101, {0, 1} },
        { SDK::ECharacterId::Ch102, {0, 1} },
        { SDK::ECharacterId::Ch103, {0, 1} },
        { SDK::ECharacterId::Ch104, {0, 1} },
        { SDK::ECharacterId::Ch105, {0, 1} },
        { SDK::ECharacterId::Ch109, {0, 1} },
        { SDK::ECharacterId::Ch111, {0, 1} },
        { SDK::ECharacterId::Ch114, {0, 1} },
        { SDK::ECharacterId::Ch115, {0, 1} },
        { SDK::ECharacterId::Ch200, {0, 1} },
        { SDK::ECharacterId::Ch201, {0, 1} },
        { SDK::ECharacterId::Ch202, {0, 1} },
    };

    auto it = variationMap.find(characterId);
    if (it != variationMap.end())
    {
        return it->second;
    }
    
    // Default: if not found, return just variation 0
    return {0};
}

/**
 * Get the name of a variation for display
 * @param variationId - The variation index (0, 1, 2, etc.)
 * @return String name like "Variation 0", "Variation 1", etc.
 */
std::string GetVariationName(int32_t variationId)
{
    return "Variation " + std::to_string(variationId);
}

/**
 * Convert combo index to actual variation ID
 * @param characterId - The character ID  
 * @param comboIndex - The index from the combo box (0, 1, 2, etc.)
 * @return The actual variation ID
 */
int32_t GetVariationIdFromComboIndex(SDK::ECharacterId characterId, int32_t comboIndex)
{
    auto variations = GetVariationsForCharacter(characterId);
    if (comboIndex < 0 || comboIndex >= static_cast<int32_t>(variations.size()))
    {
        return variations.empty() ? 0 : variations[0];
    }
    return variations[comboIndex];
}

/**
 * Get list of all available variation names for combo display
 * Generates names from enums: Ch001_Variation0, Ch001_Variation1, etc.
 */
std::vector<std::string> GetAllVariationNames()
{
    auto allIds = GetAllVariationCharacterIds();
    std::vector<std::string> names;
    
    // Character code to name mapping
    auto getCharacterName = [](const char* charCode) -> std::string {
        if (strcmp(charCode, "Ch001") == 0) return "Izuku Midoriya";
        if (strcmp(charCode, "Ch002") == 0) return "Katsuki Bakugo";
        if (strcmp(charCode, "Ch003") == 0) return "Ochaco Uraraka";
        if (strcmp(charCode, "Ch004") == 0) return "Shoto Todoroki";
        if (strcmp(charCode, "Ch005") == 0) return "Tenya Iida";
        if (strcmp(charCode, "Ch006") == 0) return "Tsuyu Asui";
        if (strcmp(charCode, "Ch007") == 0) return "Denki Kaminari";
        if (strcmp(charCode, "Ch008") == 0) return "Eijiro Kirishima";
        if (strcmp(charCode, "Ch010") == 0) return "Momo Yaoyorozu";
        if (strcmp(charCode, "Ch011") == 0) return "Fumikage Tokoyami";
        if (strcmp(charCode, "Ch012") == 0) return "All Might";
        if (strcmp(charCode, "Ch013") == 0) return "Shota Aizawa";
        if (strcmp(charCode, "Ch015") == 0) return "Tomura Shigaraki";
        if (strcmp(charCode, "Ch016") == 0) return "All For One";
        if (strcmp(charCode, "Ch017") == 0) return "Dabi";
        if (strcmp(charCode, "Ch018") == 0) return "Himiko Toga";
        if (strcmp(charCode, "Ch023") == 0) return "Endeavor";
        if (strcmp(charCode, "Ch024") == 0) return "Mirio Togata";
        if (strcmp(charCode, "Ch025") == 0) return "Nejire Hado";
        if (strcmp(charCode, "Ch026") == 0) return "Tamaki Amajiki";
        if (strcmp(charCode, "Ch034") == 0) return "Overhaul";
        if (strcmp(charCode, "Ch037") == 0) return "Twice";
        if (strcmp(charCode, "Ch038") == 0) return "Mr Compress";
        if (strcmp(charCode, "Ch043") == 0) return "Hawks";
        if (strcmp(charCode, "Ch046") == 0) return "Itsuka Kendo";
        if (strcmp(charCode, "Ch100") == 0) return "Mt. Lady";
        if (strcmp(charCode, "Ch101") == 0) return "Cementoss";
        if (strcmp(charCode, "Ch102") == 0) return "Ibara Shiozaki";
        if (strcmp(charCode, "Ch103") == 0) return "Kurogiri";
        if (strcmp(charCode, "Ch104") == 0) return "Neito Monoma";
        if (strcmp(charCode, "Ch105") == 0) return "Hitoshi Shinso";
        if (strcmp(charCode, "Ch109") == 0) return "Present Mic";
        if (strcmp(charCode, "Ch111") == 0) return "Mirko";
        if (strcmp(charCode, "Ch114") == 0) return "Star and Stripe";
        if (strcmp(charCode, "Ch115") == 0) return "Lady Nagant";
        if (strcmp(charCode, "Ch200") == 0) return "Armored All Might";
        if (strcmp(charCode, "Ch201") == 0) return "All For One (Young)";
        if (strcmp(charCode, "Ch202") == 0) return "Midoriya (OFA)";
        return charCode;  // Fallback to code
    };
    
    for (auto id : allIds)
    {
        // Get character name and variation from enum value
        const char* charCode = nullptr;
        int varIdx = 0;
        
        // Map enum to character code and variation index
        switch (id)
        {
            case SDK::EVariationCharacterId::Ch001_Variation0: charCode = "Ch001"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch001_Variation1: charCode = "Ch001"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation0: charCode = "Ch002"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch002_Variation1: charCode = "Ch002"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation2: charCode = "Ch002"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch003_Variation0: charCode = "Ch003"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch003_Variation1: charCode = "Ch003"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch004_Variation0: charCode = "Ch004"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch004_Variation1: charCode = "Ch004"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch005_Variation0: charCode = "Ch005"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch005_Variation1: charCode = "Ch005"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch006_Variation0: charCode = "Ch006"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch006_Variation1: charCode = "Ch006"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch007_Variation0: charCode = "Ch007"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch007_Variation1: charCode = "Ch007"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch008_Variation0: charCode = "Ch008"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch008_Variation1: charCode = "Ch008"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch010_Variation0: charCode = "Ch010"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch010_Variation1: charCode = "Ch010"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch011_Variation0: charCode = "Ch011"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch011_Variation1: charCode = "Ch011"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch012_Variation0: charCode = "Ch012"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch012_Variation1: charCode = "Ch012"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch013_Variation0: charCode = "Ch013"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch013_Variation1: charCode = "Ch013"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation0: charCode = "Ch015"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch015_Variation1: charCode = "Ch015"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation2: charCode = "Ch015"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch016_Variation0: charCode = "Ch016"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch016_Variation1: charCode = "Ch016"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch017_Variation0: charCode = "Ch017"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch017_Variation1: charCode = "Ch017"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch018_Variation0: charCode = "Ch018"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch018_Variation1: charCode = "Ch018"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch023_Variation0: charCode = "Ch023"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch023_Variation1: charCode = "Ch023"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch024_Variation0: charCode = "Ch024"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch024_Variation1: charCode = "Ch024"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch025_Variation0: charCode = "Ch025"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch025_Variation1: charCode = "Ch025"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch026_Variation0: charCode = "Ch026"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch026_Variation1: charCode = "Ch026"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch034_Variation0: charCode = "Ch034"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch034_Variation1: charCode = "Ch034"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch037_Variation0: charCode = "Ch037"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch037_Variation1: charCode = "Ch037"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch038_Variation0: charCode = "Ch038"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch038_Variation1: charCode = "Ch038"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch043_Variation0: charCode = "Ch043"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch043_Variation1: charCode = "Ch043"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch046_Variation0: charCode = "Ch046"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch046_Variation1: charCode = "Ch046"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch100_Variation0: charCode = "Ch100"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch100_Variation1: charCode = "Ch100"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch101_Variation0: charCode = "Ch101"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch101_Variation1: charCode = "Ch101"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch102_Variation0: charCode = "Ch102"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch102_Variation1: charCode = "Ch102"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch103_Variation0: charCode = "Ch103"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch103_Variation1: charCode = "Ch103"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch104_Variation0: charCode = "Ch104"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch104_Variation1: charCode = "Ch104"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch105_Variation0: charCode = "Ch105"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch105_Variation1: charCode = "Ch105"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch109_Variation0: charCode = "Ch109"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch109_Variation1: charCode = "Ch109"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch111_Variation0: charCode = "Ch111"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch111_Variation1: charCode = "Ch111"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch114_Variation0: charCode = "Ch114"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch114_Variation1: charCode = "Ch114"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch115_Variation0: charCode = "Ch115"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch115_Variation1: charCode = "Ch115"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch200_Variation0: charCode = "Ch200"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch200_Variation1: charCode = "Ch200"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch201_Variation0: charCode = "Ch201"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch201_Variation1: charCode = "Ch201"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch202_Variation0: charCode = "Ch202"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch202_Variation1: charCode = "Ch202"; varIdx = 1; break;
            default: charCode = "Unknown"; break;
        }
        
        if (charCode)
        {
            std::string displayName = getCharacterName(charCode) + " (Variation " + std::to_string(varIdx) + ")";
            names.push_back(displayName);
        }
    }
    
    return names;
}

/**
 * Convert EVariationCharacterId back to (ECharacterId, variationId)
 * @param variationCharacterId - The combined variation character ID
 * @return Pair of (ECharacterId, variationId) or (UNDEF, -1) if invalid
 */
std::pair<SDK::ECharacterId, int32_t> GetCharacterAndVariationFromVariationCharacterId(SDK::EVariationCharacterId variationCharacterId)
{
    // Map EVariationCharacterId enum values back to (ECharacterId, variationId)
    // This is the inverse of GetVariationCharacterId()
    
    switch (variationCharacterId)
    {
        // Ch001: variations 0, 1
        case SDK::EVariationCharacterId::Ch001_Variation0: return {SDK::ECharacterId::Ch001, 0};
        case SDK::EVariationCharacterId::Ch001_Variation1: return {SDK::ECharacterId::Ch001, 1};
        
        // Ch002: variations 0, 1, 2
        case SDK::EVariationCharacterId::Ch002_Variation0: return {SDK::ECharacterId::Ch002, 0};
        case SDK::EVariationCharacterId::Ch002_Variation1: return {SDK::ECharacterId::Ch002, 1};
        case SDK::EVariationCharacterId::Ch002_Variation2: return {SDK::ECharacterId::Ch002, 2};
        
        // Ch003: variations 0, 1
        case SDK::EVariationCharacterId::Ch003_Variation0: return {SDK::ECharacterId::Ch003, 0};
        case SDK::EVariationCharacterId::Ch003_Variation1: return {SDK::ECharacterId::Ch003, 1};
        
        // Ch004: variations 0, 1
        case SDK::EVariationCharacterId::Ch004_Variation0: return {SDK::ECharacterId::Ch004, 0};
        case SDK::EVariationCharacterId::Ch004_Variation1: return {SDK::ECharacterId::Ch004, 1};
        
        // Ch005: variations 0, 1
        case SDK::EVariationCharacterId::Ch005_Variation0: return {SDK::ECharacterId::Ch005, 0};
        case SDK::EVariationCharacterId::Ch005_Variation1: return {SDK::ECharacterId::Ch005, 1};
        
        // Ch006: variations 0, 1
        case SDK::EVariationCharacterId::Ch006_Variation0: return {SDK::ECharacterId::Ch006, 0};
        case SDK::EVariationCharacterId::Ch006_Variation1: return {SDK::ECharacterId::Ch006, 1};
        
        // Ch007: variations 0, 1
        case SDK::EVariationCharacterId::Ch007_Variation0: return {SDK::ECharacterId::Ch007, 0};
        case SDK::EVariationCharacterId::Ch007_Variation1: return {SDK::ECharacterId::Ch007, 1};
        
        // Ch008: variations 0, 1
        case SDK::EVariationCharacterId::Ch008_Variation0: return {SDK::ECharacterId::Ch008, 0};
        case SDK::EVariationCharacterId::Ch008_Variation1: return {SDK::ECharacterId::Ch008, 1};
        
        // Ch010: variations 0, 1
        case SDK::EVariationCharacterId::Ch010_Variation0: return {SDK::ECharacterId::Ch010, 0};
        case SDK::EVariationCharacterId::Ch010_Variation1: return {SDK::ECharacterId::Ch010, 1};
        
        // Ch011: variations 0, 1
        case SDK::EVariationCharacterId::Ch011_Variation0: return {SDK::ECharacterId::Ch011, 0};
        case SDK::EVariationCharacterId::Ch011_Variation1: return {SDK::ECharacterId::Ch011, 1};
        
        // Ch012: variations 0, 1
        case SDK::EVariationCharacterId::Ch012_Variation0: return {SDK::ECharacterId::Ch012, 0};
        case SDK::EVariationCharacterId::Ch012_Variation1: return {SDK::ECharacterId::Ch012, 1};
        
        // Ch013: variations 0, 1
        case SDK::EVariationCharacterId::Ch013_Variation0: return {SDK::ECharacterId::Ch013, 0};
        case SDK::EVariationCharacterId::Ch013_Variation1: return {SDK::ECharacterId::Ch013, 1};
        
        // Ch015: variations 0, 1, 2
        case SDK::EVariationCharacterId::Ch015_Variation0: return {SDK::ECharacterId::Ch015, 0};
        case SDK::EVariationCharacterId::Ch015_Variation1: return {SDK::ECharacterId::Ch015, 1};
        case SDK::EVariationCharacterId::Ch015_Variation2: return {SDK::ECharacterId::Ch015, 2};
        
        // Ch016: variations 0, 1
        case SDK::EVariationCharacterId::Ch016_Variation0: return {SDK::ECharacterId::Ch016, 0};
        case SDK::EVariationCharacterId::Ch016_Variation1: return {SDK::ECharacterId::Ch016, 1};
        
        // Ch017: variations 0, 1
        case SDK::EVariationCharacterId::Ch017_Variation0: return {SDK::ECharacterId::Ch017, 0};
        case SDK::EVariationCharacterId::Ch017_Variation1: return {SDK::ECharacterId::Ch017, 1};
        
        // Ch018: variations 0, 1
        case SDK::EVariationCharacterId::Ch018_Variation0: return {SDK::ECharacterId::Ch018, 0};
        case SDK::EVariationCharacterId::Ch018_Variation1: return {SDK::ECharacterId::Ch018, 1};
        
        // Ch023: variations 0, 1
        case SDK::EVariationCharacterId::Ch023_Variation0: return {SDK::ECharacterId::Ch023, 0};
        case SDK::EVariationCharacterId::Ch023_Variation1: return {SDK::ECharacterId::Ch023, 1};
        
        // Ch024: variations 0, 1
        case SDK::EVariationCharacterId::Ch024_Variation0: return {SDK::ECharacterId::Ch024, 0};
        case SDK::EVariationCharacterId::Ch024_Variation1: return {SDK::ECharacterId::Ch024, 1};
        
        // Ch025: variations 0, 1
        case SDK::EVariationCharacterId::Ch025_Variation0: return {SDK::ECharacterId::Ch025, 0};
        case SDK::EVariationCharacterId::Ch025_Variation1: return {SDK::ECharacterId::Ch025, 1};
        
        // Ch026: variations 0, 1
        case SDK::EVariationCharacterId::Ch026_Variation0: return {SDK::ECharacterId::Ch026, 0};
        case SDK::EVariationCharacterId::Ch026_Variation1: return {SDK::ECharacterId::Ch026, 1};
        
        // Ch034: variations 0, 1
        case SDK::EVariationCharacterId::Ch034_Variation0: return {SDK::ECharacterId::Ch034, 0};
        case SDK::EVariationCharacterId::Ch034_Variation1: return {SDK::ECharacterId::Ch034, 1};
        
        // Ch037: variations 0, 1
        case SDK::EVariationCharacterId::Ch037_Variation0: return {SDK::ECharacterId::Ch037, 0};
        case SDK::EVariationCharacterId::Ch037_Variation1: return {SDK::ECharacterId::Ch037, 1};
        
        // Ch038: variations 0, 1
        case SDK::EVariationCharacterId::Ch038_Variation0: return {SDK::ECharacterId::Ch038, 0};
        case SDK::EVariationCharacterId::Ch038_Variation1: return {SDK::ECharacterId::Ch038, 1};
        
        // Ch043: variations 0, 1
        case SDK::EVariationCharacterId::Ch043_Variation0: return {SDK::ECharacterId::Ch043, 0};
        case SDK::EVariationCharacterId::Ch043_Variation1: return {SDK::ECharacterId::Ch043, 1};
        
        // Ch046: variations 0, 1
        case SDK::EVariationCharacterId::Ch046_Variation0: return {SDK::ECharacterId::Ch046, 0};
        case SDK::EVariationCharacterId::Ch046_Variation1: return {SDK::ECharacterId::Ch046, 1};
        
        // Ch100: variations 0, 1
        case SDK::EVariationCharacterId::Ch100_Variation0: return {SDK::ECharacterId::Ch100, 0};
        case SDK::EVariationCharacterId::Ch100_Variation1: return {SDK::ECharacterId::Ch100, 1};
        
        // Ch101: variations 0, 1
        case SDK::EVariationCharacterId::Ch101_Variation0: return {SDK::ECharacterId::Ch101, 0};
        case SDK::EVariationCharacterId::Ch101_Variation1: return {SDK::ECharacterId::Ch101, 1};
        
        // Ch102: variations 0, 1
        case SDK::EVariationCharacterId::Ch102_Variation0: return {SDK::ECharacterId::Ch102, 0};
        case SDK::EVariationCharacterId::Ch102_Variation1: return {SDK::ECharacterId::Ch102, 1};
        
        // Ch103: variations 0, 1
        case SDK::EVariationCharacterId::Ch103_Variation0: return {SDK::ECharacterId::Ch103, 0};
        case SDK::EVariationCharacterId::Ch103_Variation1: return {SDK::ECharacterId::Ch103, 1};
        
        // Ch104: variations 0, 1
        case SDK::EVariationCharacterId::Ch104_Variation0: return {SDK::ECharacterId::Ch104, 0};
        case SDK::EVariationCharacterId::Ch104_Variation1: return {SDK::ECharacterId::Ch104, 1};
        
        // Ch105: variations 0, 1
        case SDK::EVariationCharacterId::Ch105_Variation0: return {SDK::ECharacterId::Ch105, 0};
        case SDK::EVariationCharacterId::Ch105_Variation1: return {SDK::ECharacterId::Ch105, 1};
        
        // Ch109: variations 0, 1
        case SDK::EVariationCharacterId::Ch109_Variation0: return {SDK::ECharacterId::Ch109, 0};
        case SDK::EVariationCharacterId::Ch109_Variation1: return {SDK::ECharacterId::Ch109, 1};

        case SDK::EVariationCharacterId::Ch111_Variation0: return {SDK::ECharacterId::Ch111, 0};
        case SDK::EVariationCharacterId::Ch111_Variation1: return {SDK::ECharacterId::Ch111, 1};
        
        // Ch114: variations 0, 1
        case SDK::EVariationCharacterId::Ch114_Variation0: return {SDK::ECharacterId::Ch114, 0};
        case SDK::EVariationCharacterId::Ch114_Variation1: return {SDK::ECharacterId::Ch114, 1};
        
        // Ch115: variations 0, 1
        case SDK::EVariationCharacterId::Ch115_Variation0: return {SDK::ECharacterId::Ch115, 0};
        case SDK::EVariationCharacterId::Ch115_Variation1: return {SDK::ECharacterId::Ch115, 1};
        
        // Ch200: variations 0, 1
        case SDK::EVariationCharacterId::Ch200_Variation0: return {SDK::ECharacterId::Ch200, 0};
        case SDK::EVariationCharacterId::Ch200_Variation1: return {SDK::ECharacterId::Ch200, 1};
        
        // Ch201: variations 0, 1
        case SDK::EVariationCharacterId::Ch201_Variation0: return {SDK::ECharacterId::Ch201, 0};
        case SDK::EVariationCharacterId::Ch201_Variation1: return {SDK::ECharacterId::Ch201, 1};
        
        // Ch202: variations 0, 1
        case SDK::EVariationCharacterId::Ch202_Variation0: return {SDK::ECharacterId::Ch202, 0};
        case SDK::EVariationCharacterId::Ch202_Variation1: return {SDK::ECharacterId::Ch202, 1};
        
        default: return {SDK::ECharacterId::UNDEF, -1};
    }
}

/**
 * Get character ID and variation index from combo index
 * Maps combo selection index back to (ECharacterId, variationIndex)
 */
std::pair<SDK::ECharacterId, int32_t> GetCharacterAndVariationFromIndex(int32_t comboIndex)
{
    static SDK::ECharacterId characterList[] = {
        SDK::ECharacterId::Ch001, SDK::ECharacterId::Ch002, SDK::ECharacterId::Ch003, SDK::ECharacterId::Ch004,
        SDK::ECharacterId::Ch005, SDK::ECharacterId::Ch006, SDK::ECharacterId::Ch007, SDK::ECharacterId::Ch008,
        SDK::ECharacterId::Ch010, SDK::ECharacterId::Ch011, SDK::ECharacterId::Ch012, SDK::ECharacterId::Ch013,
        SDK::ECharacterId::Ch015, SDK::ECharacterId::Ch016, SDK::ECharacterId::Ch017, SDK::ECharacterId::Ch018,
        SDK::ECharacterId::Ch023, SDK::ECharacterId::Ch024, SDK::ECharacterId::Ch025, SDK::ECharacterId::Ch026,
        SDK::ECharacterId::Ch034, SDK::ECharacterId::Ch037, SDK::ECharacterId::Ch038, SDK::ECharacterId::Ch043,
        SDK::ECharacterId::Ch046, SDK::ECharacterId::Ch100, SDK::ECharacterId::Ch101, SDK::ECharacterId::Ch102,
        SDK::ECharacterId::Ch103, SDK::ECharacterId::Ch104, SDK::ECharacterId::Ch105, SDK::ECharacterId::Ch109, SDK::ECharacterId::Ch111,
        SDK::ECharacterId::Ch114, SDK::ECharacterId::Ch115, SDK::ECharacterId::Ch200, SDK::ECharacterId::Ch201,
        SDK::ECharacterId::Ch202
    };
    
    int currentIndex = 0;
    
    // Find the character and variation at the given combo index
    for (const auto& charId : characterList)
    {
        auto variations = GetVariationsForCharacter(charId);
        if (comboIndex < currentIndex + static_cast<int32_t>(variations.size()))
        {
            int varIdx = variations[comboIndex - currentIndex];
            return {charId, varIdx};
        }
        currentIndex += static_cast<int32_t>(variations.size());
    }
    
    // Invalid index
    return {SDK::ECharacterId::UNDEF, -1};
}

/**
 * Change character on server with logging
 * @param PlayerController - The APlayerControllerBattle instance
 * @param CurrentCharacter - Current character battle pawn
 * @param NewCharacterData - The new character data to apply
 */
bool TrainingHack_ChangeCharacterOnServer(
    SDK::APlayerControllerBattle* PlayerController,
    SDK::ACharacterBattle* CurrentCharacter,
    const SDK::FInGameBattleCharacterData& NewCharacterData)
{
    if (!PlayerController || !CurrentCharacter)
    {
        Logger::LogError("[CHARACTER] Invalid controller or character");
        return false;
    }

    try
    {
        PlayerController->ChangeCharacter_OnServer(CurrentCharacter, NewCharacterData);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CHARACTER] Exception: " + std::string(e.what()));
        return false;
    }
}

// ============================================
// TRANSFORM HACKS - Character Transformation
// ============================================

/**
 * Transform player into target character on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to transform into
 */
bool InGameHack_TransformInto(
    SDK::APlayerController* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter)
{

    // Validate inputs
    if (!IsValidPointer(playerController) || !IsValidPointer(playerCharacter) || !IsValidPointer(targetCharacter))
    {
        return false;
    }

    try
    {
        // Get the player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UTransformControlComponent from player state
        SDK::UTransformControlComponent* transformComponent = playerStateBattle->_transformControlComponent;
        if (!IsValidPointer(transformComponent))
        {
            return false;
        }
        
        // Call the RPC function on the component
        transformComponent->TransformInto_RPC_ToServer(targetCharacter);
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Transform into random ESP target (hotkey wrapper)
 * Gets player and random target from ESP, then calls InGameHack_TransformInto
 */
bool InGameHack_TransformIntoRandomESP()
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }
        
        // Get possessed pawn (player character) with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            return false;
        }
        
        // Get random target from ESP using SDK function
        SDK::AActor* targetActorPtr = SDK_GetRandomESPTarget();
        if (!targetActorPtr)
        {
            return false;
        }
        
        // Cast to ACharacterBattle
        SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(targetActorPtr);
        if (!targetCharacter)
        {
            return false;
        }
        
        // Call the transform hack
        return InGameHack_TransformInto(playerController, playerCharacter, targetCharacter);
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// DUPLICATE HACKS - Character Duplication
// ============================================

/**
 * Duplicate player into target character on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to duplicate into
 * @param spawnLocation - Where to spawn the duplicate
 */
/**
 * Duplicate player into target character for imitation on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to duplicate into
 * @param spawnLocation - Where to spawn the duplicate
 * @param lifeTime - How long the imitation lasts
 */
bool InGameHack_DuplicateIntoImitation(
    SDK::APlayerController* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter,
    const SDK::FVector& spawnLocation,
    float lifeTime)
{
    // Validate inputs
    if (!IsValidPointer(playerController) || !IsValidPointer(playerCharacter) || !IsValidPointer(targetCharacter))
    {
        return false;
    }

    try
    {
        // Get the player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UDuplicateControlComponent from player state (offset 0x0C68)
        SDK::UDuplicateControlComponent* duplicateComponent = playerStateBattle->_duplicateControlComponent;
        if (!IsValidPointer(duplicateComponent))
        {
            return false;
        }
        
        // Call the RPC function on the component with attackId = 0 (default)
        // EAttackId is an enum, using 0 as default
        duplicateComponent->DuplicateIntoForImitation_RPC_ToServer(targetCharacter, spawnLocation, (SDK::EAttackId)1, lifeTime);
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Duplicate into random ESP target for imitation (hotkey wrapper)
 * Gets player and forward target from ESP, then calls InGameHack_DuplicateIntoImitation N times
 */
bool InGameHack_DuplicateIntoImitationRandomESP(float spawnOffsetZ, float lifeTime, int count)
{
    try
    {
        // Clamp count to valid range
        if (count < 1 || count > 100)
        {
            count = 1;
        }

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }
        
        // Get possessed pawn (player character)
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
        {
            return false;
        }
        
        // Get player position and forward vector
        SDK::FVector playerPos = static_cast<SDK::AActor*>(playerCharacter)->K2_GetActorLocation();
        SDK::FVector forwardVector = static_cast<SDK::AActor*>(playerCharacter)->GetActorForwardVector();
        
        // Spawn N imitation duplicates in front of player
        bool anySuccess = false;
        for (int i = 0; i < count; i++)
        {
            // Get forward target from ESP using SDK function
            SDK::AActor* targetActorPtr = SDK_GetForwardESPTarget();
            if (!targetActorPtr)
            {
                continue;  // Try next duplicate
            }
            
            // Cast to ACharacterBattle
            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(targetActorPtr);
            if (!targetCharacter)
            {
                continue;  // Try next duplicate
            }
            
            // Calculate spawn location: player position + forward * 200 (no Z offset)
            SDK::FVector spawnLocation = playerPos + (forwardVector * 200.0f);
            
            // Call the duplicate imitation hack
            if (InGameHack_DuplicateIntoImitation(playerController, playerCharacter, targetCharacter, spawnLocation, lifeTime))
            {
                anySuccess = true;
            }
        }
        
        return anySuccess;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// BUFF/STAT ADJUSTMENT IMPLEMENTATIONS
// ============================================

/**
 * Set general reload adjust rate via BuffParam
 */
bool InGameHack_SetReloadAdjustRate(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate
        buffParam->BP_SetAttackAdjustRate_Unique1(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set reload adjust rate for roll slot
 */
bool InGameHack_SetReloadAdjustRate_RollSlot(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate_RollSlot] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate for roll slot
        buffParam->BP_SetReloadAdjustRate_RollSlot(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set reload adjust rate while wearing blue flame
 */
bool InGameHack_SetReloadAdjustRate_WearBlueFlame(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate_WearBlueFlame] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate for blue flame
        buffParam->BP_SetReloadAdjustRate_WearBlueFlame(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}


/**
 * Apply training player configuration (player only, no AI)
 */
bool InGameHack_ApplyPlayerConfiguration(int characterId, int variationId, int unique1, int unique2, int unique3, int skillCode, int costumeCode, int costumeAuraType)
{
    try
    {
        // Get PlayerController
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[ApplyPlayerConfiguration] Could not get valid PlayerController");
            return false;
        }
        
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[ApplyPlayerConfiguration] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get current character pawn with validation
        SDK::ACharacterBattle* currentCharacter = GetPlayerCharacterBattle(playerController);
        if (!currentCharacter)
        {
            Logger::LogError("[ApplyPlayerConfiguration] Could not get player character");
            return false;
        }

        // Create character data structure
        SDK::FInGameBattleCharacterData characterData = {};
        characterData._characterId = (SDK::ECharacterId)characterId;
        characterData._variationId = (SDK::int32)variationId;  // Variation ID (0-2 depending on character)
        characterData._skillVariationCode = skillCode;         // Skill variation (0-5)
        characterData._technique1Level = unique1;
        characterData._technique2Level = unique2;
        characterData._technique3Level = unique3;
        characterData._costumeCode = costumeCode;
        characterData._costumeAuraType = costumeAuraType;

        // Call ChangeCharacter_OnServer
        playerController->ChangeCharacter_OnServer(currentCharacter, characterData);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyPlayerConfiguration] Exception caught");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ApplyPlayerConfiguration] Unknown exception");
        return false;
    }
}

// ============================================
// APPLY TO ALL CONTROLLERS
// ============================================

/**
 * Apply player configuration to ALL enemy characters in the match
 * Same exploit method as Dll.cpp: local player controller calls ChangeCharacter_OnServer for each enemy
 * Uses SDK method call directly (not ProcessEvent)
 * Returns: number of characters changed (0 if failed or no characters found)
 */
int InGameHack_ApplyToAllControllers(int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToAll] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            Logger::LogError("[ApplyToAll] Could not get world");
            return 0;
        }

        // Get LOCAL player controller
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[ApplyToAll] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ApplyToAll] Invalid battle PC");
            return 0;
        }

        // Get player pawn with validation
        SDK::ACharacterBattle* playerPawn = GetPlayerCharacterBattle(battlePC);
        if (!playerPawn)
        {
            Logger::LogError("[ApplyToAll] No valid player pawn");
            return 0;
        }

        // Open log file for getter data
        std::ofstream debugFile("C:\\temp\\ApplyToAll_GetterData.txt", std::ios::trunc);
        debugFile << "=== ApplyToAll Getter Data Log ===" << std::endl;
        debugFile << "Requested Change - CharId: " << characterId << ", Variation: " << variationId << std::endl;
        debugFile << "Technique Levels: " << unique1 << ", " << unique2 << ", " << unique3 << std::endl;
        debugFile << "Costume: " << costumeCode << ", Aura: " << costumeAuraType << std::endl;
        debugFile << "\n--- Player Data Retrieved via Getters ---\n" << std::endl;

        int appliedCount = 0;

        // LOOP OVER ALL ACTORS - NO FILTERING, APPLY TO EVERYONE
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            // Robust type check with pointer validation
            if (!IsValidPointer(actor) || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
            if (!IsValidPointer(targetCharacter))
                continue;

            try
            {
                // ✅ RETRIEVE ACTUAL DATA from character via getters
                int32_t currentVariationNo = targetCharacter->BP_GetVariationNo();
                int32_t currentCostumeCode = targetCharacter->BP_GetCostumeCode();
                SDK::ECharacterId currentCharacterId = targetCharacter->BP_GetCharacterId();

                // Log to file
                debugFile << "Actor " << i << ": CharId=" << (int)currentCharacterId 
                          << ", Variation=" << currentVariationNo 
                          << ", Costume=" << currentCostumeCode << std::endl;

                // Set the variation on the target character BEFORE RPC so server sees it
                int32_t originalVariation = targetCharacter->_variationNo;
                targetCharacter->_variationNo = variationId;

                // Create character data
                SDK::FInGameBattleCharacterData changeData = {};
                changeData._characterId = (SDK::ECharacterId)characterId;
                changeData._variationId = variationId;
                changeData._skillVariationCode = ((characterId + 1) * 100) + variationId;
                changeData._technique1Level = unique1;
                changeData._technique2Level = unique2;
                changeData._technique3Level = unique3;
                changeData._costumeCode = costumeCode;
                changeData._costumeAuraType = costumeAuraType;

                // Call LOCAL controller's ChangeCharacter_OnServer for THIS character
                battlePC->ChangeCharacter_OnServer(targetCharacter, changeData);

                // Restore original (server will have synced the new variant)
                targetCharacter->_variationNo = originalVariation;
                appliedCount++;
            }
            catch (...)
            {
                debugFile << "Actor " << i << ": ERROR retrieving data" << std::endl;
                continue;
            }
        }

        debugFile << "\n--- Summary ---" << std::endl;
        debugFile << "Total applied: " << appliedCount << std::endl;
        debugFile.close();

        if (appliedCount == 0)
        {
            Logger::LogWarning("[ApplyToAll] No characters found to modify");
            return 0;
        }

        Logger::LogInfo("[ApplyToAll] Applied character change to " + std::to_string(appliedCount) + " characters");
        return appliedCount;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToAll] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToAll] Unknown exception");
        return 0;
    }
}

// ============================================
// GET ALL PLAYER NAMES
// ============================================

/**
 * Get all player names on the map
 * Returns vector with format: "PlayerName (CharacterClass)"
 */
std::vector<std::string> InGameHack_GetAllPlayerNames()
{
    std::vector<std::string> playerNames;
    try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
            return playerNames;

        // Loop over all actors to find characters with PlayerState
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            if (!IsValidPointer(actor) || actor->IsDefaultObject())
                continue;

            if (!actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(actor);
            if (!IsValidPointer(character) || !IsValidPointer(character->PlayerState))
                continue;

            // Get player name
            std::string playerName = "Unknown";
            try
            {
                const char* namePtr = SDK_GetPlayerName(character->PlayerState);
                if (namePtr && namePtr[0] != '\0')
                    playerName = namePtr;
            }
            catch (...)
            {
            }

            // Get character class name
            std::string className = "Unknown";
            try
            {
                if (character->Class)
                    className = character->Class->GetName();
            }
            catch (...)
            {
            }

            playerNames.push_back(playerName + " (" + className + ")");
        }
    }
    catch (...)
    {
    }

    return playerNames;
}

// ============================================
// APPLY TO SPECIFIC PLAYER
// ============================================

/**
 * Apply character change to a specific player by index
 * @param playerIndex - Index in the player list (from GetAllPlayerNames)
 * @param variationCharacterId - The variation character ID enum
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_ApplyToSpecificPlayer(int playerIndex, SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToSpecificPlayer] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Decode the variation character ID
        auto [characterId, variationId] = GetCharacterAndVariationFromVariationCharacterId(variationCharacterId);

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            Logger::LogError("[ApplyToSpecificPlayer] Could not get world");
            return 0;
        }

        // Get LOCAL player controller
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[ApplyToSpecificPlayer] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ApplyToSpecificPlayer] Invalid battle PC");
            return 0;
        }

        // Verify player pawn exists
        SDK::ACharacterBattle* playerPawn = GetPlayerCharacterBattle(battlePC);
        if (!playerPawn)
        {
            Logger::LogError("[ApplyToSpecificPlayer] No valid player pawn");
            return 0;
        }

        // Create character data
        SDK::FInGameBattleCharacterData changeData = {};
        changeData._characterId = characterId;
        changeData._variationId = variationId;
        changeData._skillVariationCode = (((int)characterId + 1) * 100) + variationId;
        changeData._technique1Level = unique1;
        changeData._technique2Level = unique2;
        changeData._technique3Level = unique3;
        changeData._costumeCode = costumeCode;
        changeData._costumeAuraType = costumeAuraType;

        int currentIndex = 0;

        // Loop over all actors to find the target player
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            if (!IsValidPointer(actor) || actor->IsDefaultObject())
                continue;

            if (!actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
            if (!IsValidPointer(targetCharacter) || !IsValidPointer(targetCharacter->PlayerState))
                continue;

            // Check if this is the target player
            if (currentIndex == playerIndex)
            {
                try
                {
                    // Apply character change
                    battlePC->ChangeCharacter_OnServer(targetCharacter, changeData);
                    Logger::LogInfo("[ApplyToSpecificPlayer] Applied character change to player at index " + std::to_string(playerIndex));
                    return 1;
                }
                catch (const std::exception& e)
                {
                    Logger::LogError("[ApplyToSpecificPlayer] Exception: " + std::string(e.what()));
                    return 0;
                }
                catch (...)
                {
                    Logger::LogError("[ApplyToSpecificPlayer] Unknown exception");
                    return 0;
                }
            }

            currentIndex++;
        }

        Logger::LogWarning("[ApplyToSpecificPlayer] Player at index " + std::to_string(playerIndex) + " not found");
        return 0;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToSpecificPlayer] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToSpecificPlayer] Unknown exception");
        return 0;
    }
}

// ============================================
// APPLY TO TEAM
// ============================================

/**
 * Apply player configuration to ALL characters in a specific team
 * Uses same exploit as ApplyToAllControllers but filters by team ID
 */
bool InGameHack_ApplyToTeam(unsigned char teamId, int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToTeam] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            Logger::LogError("[ApplyToTeam] Could not get world");
            return false;
        }

        // Get LOCAL player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[ApplyToTeam] Could not get player controller");
            return false;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ApplyToTeam] Invalid battle PC");
            return false;
        }

        // Verify player pawn exists
        SDK::ACharacterBattle* playerPawn = GetPlayerCharacterBattle(battlePC);
        if (!playerPawn)
        {
            Logger::LogError("[ApplyToTeam] No valid player pawn");
            return false;
        }

        // Create character data once with ALL parameters FILLED
        SDK::FInGameBattleCharacterData characterData = {};
        characterData._characterId = (SDK::ECharacterId)characterId;
        characterData._variationId = variationId;
        characterData._skillVariationCode = ((characterId + 1) * 100) + variationId;
        characterData._technique1Level = unique1;
        characterData._technique2Level = unique2;
        characterData._technique3Level = unique3;
        characterData._costumeCode = costumeCode;
        characterData._costumeAuraType = costumeAuraType;

        int appliedCount = 0;

        // LOOP OVER ALL ACTORS - FILTER BY TEAM ID
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            // Robust type check with pointer validation
            if (!IsValidPointer(actor) || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
            if (!IsValidPointer(targetCharacter))
                continue;

            // GET TARGET CHARACTER'S TEAM ID AND FILTER
            unsigned char characterTeamId = 255;
            try {
                characterTeamId = targetCharacter->BP_GetTeamId();
            }
            catch (...) {
                continue;
            }
            
            if (characterTeamId != teamId)
                continue;  // Skip if not in target team

            // Call LOCAL controller's ChangeCharacter_OnServer for THIS character (only if team matches)
            try
            {
                battlePC->ChangeCharacter_OnServer(targetCharacter, characterData);
                appliedCount++;
            }
            catch (...)
            {
                continue;
            }
        }

        if (appliedCount == 0)
        {
            Logger::LogWarning("[ApplyToTeam] No characters found in team " + std::to_string(teamId));
            return false;
        }

        Logger::LogInfo("[ApplyToTeam] Applied character change to " + std::to_string(appliedCount) + " characters in team " + std::to_string(teamId));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToTeam] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToTeam] Unknown exception");
        return false;
    }
}

// ============================================
// COPY SKILLS FROM CHARACTER
// ============================================

/**
 * Copy skills from a RANDOM enemy character to the local player
 * Automatically finds a random enemy and copies their skills
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_CopySkillsFromNearestEnemy(bool bSetCopySkill, bool bUseOwnerCharacterLevel)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[CopySkillsFromNearestEnemy] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Get local player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get valid battle controller");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get player character");
            return 0;
        }

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get world");
            return 0;
        }

        // Find ALL enemies
        std::vector<SDK::ACharacterBattle*> enemies;

        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            if (!actor || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
            
            // Skip if it's the local player
            if (targetCharacter == localCharacter)
                continue;

            // Skip if character has no SkillManagementComponent
            if (!targetCharacter->_skillManagementComponent)
                continue;

            enemies.push_back(targetCharacter);
        }

        if (enemies.empty())
        {
            Logger::LogWarning("[CopySkillsFromNearestEnemy] No valid enemies found to copy skills from");
            return 0;
        }

        // Pick a random enemy
        int randomIndex = rand() % enemies.size();
        SDK::ACharacterBattle* randomEnemy = enemies[randomIndex];

        // Final safety check
        if (!randomEnemy || randomEnemy->IsDefaultObject() || !randomEnemy->_skillManagementComponent)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Selected random enemy became invalid");
            return 0;
        }

        // Now copy skills from the random enemy
        return InGameHack_CopySkillsFromCharacter(randomEnemy, bSetCopySkill, bUseOwnerCharacterLevel);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CopySkillsFromNearestEnemy] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[CopySkillsFromNearestEnemy] Unknown exception");
        return 0;
    }
}

/**
 * Copy skills from ONE specific character to the local player
 * The character to copy from must NOT be the local player
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_CopySkillsFromCharacter(SDK::ACharacterBattle* masterCharacter, bool bSetCopySkill, bool bUseOwnerCharacterLevel)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[CopySkillsFromCharacter] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Validate master character early
        if (!IsValidPointer(masterCharacter) || masterCharacter->IsDefaultObject())
        {
            Logger::LogError("[CopySkillsFromCharacter] Invalid master character");
            return 0;
        }

        // Get local player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get valid battle controller");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get player character");
            return 0;
        }

        // IMPORTANT: Do NOT copy from yourself!
        if (masterCharacter == localCharacter)
        {
            Logger::LogWarning("[CopySkillsFromCharacter] Cannot copy skills from yourself");
            return 0;
        }

        // Get the SkillManagementComponent from the LOCAL player
        SDK::USkillManagementComponent* skillMgmt = nullptr;
        if (IsValidPointer(localCharacter->_skillManagementComponent))
        {
            skillMgmt = localCharacter->_skillManagementComponent;
        }
        
        if (!IsValidPointer(skillMgmt))
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get SkillManagementComponent from local player");
            return 0;
        }

        // Verify master character has a SkillManagementComponent before attempting to copy
        if (!masterCharacter->_skillManagementComponent)
        {
            Logger::LogError("[CopySkillsFromCharacter] Master character has no SkillManagementComponent");
            return 0;
        }

        // Verify master character is still valid and not default object
        if (masterCharacter->IsDefaultObject())
        {
            Logger::LogError("[CopySkillsFromCharacter] Master character became invalid (default object)");
            return 0;
        }

        // Activate copy mode first, then set the character to copy from
        try
        {
            // Start copy mode with a 30 second duration (using None type)
            skillMgmt->BP_StartCopyMode(300.0f, (SDK::ECopyModeCharacterType)1);
            Logger::LogInfo("[CopySkillsFromCharacter] Started copy mode");

            // Verify master character is STILL valid after starting copy mode
            if (masterCharacter->IsDefaultObject())
            {
                Logger::LogError("[CopySkillsFromCharacter] Master character became invalid after starting copy mode");
                return 0;
            }

            // Now set the master character to copy from
            skillMgmt->BP_SetCopyCharacter(masterCharacter, bSetCopySkill, bUseOwnerCharacterLevel);
            Logger::LogInfo("[CopySkillsFromCharacter] Successfully copied skills from enemy character");
            return 1;
        }
        catch (const std::exception& ex)
        {
            Logger::LogError("[CopySkillsFromCharacter] Failed to execute copy sequence: " + std::string(ex.what()));
            return 0;
        }
        catch (...)
        {
            Logger::LogError("[CopySkillsFromCharacter] Failed to execute copy sequence (unknown error)");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CopySkillsFromCharacter] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[CopySkillsFromCharacter] Unknown exception");
        return 0;
    }
}

/**
 * Change my team ID to a random available team (excluding current team)
 * Gets all available teams, excludes current team, and switches to a random one
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeam()
{
    try
    {
        if (!IsValidBattleMode())
        {
            Logger::LogWarning("[ChangeMyTeam] Not in valid battle mode");
            return 0;
        }

        // Get player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[ChangeMyTeam] Could not get valid player controller");
            return 0;
        }

        // Get player state with validation
        if (!IsValidPointer(basePlayerController->PlayerState))
        {
            Logger::LogError("[ChangeMyTeam] Could not get player state");
            return 0;
        }

        // Cast to HerovsPlayerState to access team functions
        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(basePlayerController->PlayerState);
        if (!IsValidPointer(playerState))
        {
            Logger::LogError("[ChangeMyTeam] Could not get HerovsPlayerState");
            return 0;
        }

        // Get my current team ID
        int myCurrentTeamId = -1;
        try
        {
            myCurrentTeamId = playerState->BP_GetTeamId();
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeam] Failed to get current team ID");
            return 0;
        }

        Logger::LogInfo("[ChangeMyTeam] Current team ID: " + std::to_string(myCurrentTeamId));

        // Get world to scan all characters and find available teams
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            Logger::LogError("[ChangeMyTeam] Could not get world");
            return 0;
        }

        // Collect all unique team IDs in the match
        std::vector<int> availableTeams;
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            if (!IsValidPointer(actor))
                continue;
            if (!actor || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(actor);
            
            try
            {
                int teamId = character->BP_GetTeamId();
                
                // Check if this team ID is already in our list
                bool teamFound = false;
                for (int existingTeamId : availableTeams)
                {
                    if (existingTeamId == teamId)
                    {
                        teamFound = true;
                        break;
                    }
                }
                
                // Add new team ID if not already in list and valid
                if (!teamFound && teamId >= 0 && teamId != myCurrentTeamId)
                {
                    availableTeams.push_back(teamId);
                }
            }
            catch (...)
            {
                continue;
            }
        }

        // Check if we found any other teams
        if (availableTeams.empty())
        {
            Logger::LogWarning("[ChangeMyTeam] No other teams found to join");
            return 0;
        }

        // Select a random team from available teams
        int randomIndex = rand() % availableTeams.size();
        int newTeamId = availableTeams[randomIndex];

        Logger::LogInfo("[ChangeMyTeam] Switching to team ID: " + std::to_string(newTeamId));

        // Change team via BP_SetTeamId
        try
        {
            playerState->BP_SetTeamId(newTeamId);
            Logger::LogInfo("[ChangeMyTeam] Successfully changed team to: " + std::to_string(newTeamId));
            return 1;
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeam] Failed to call BP_SetTeamId");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeMyTeam] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ChangeMyTeam] Unknown exception");
        return 0;
    }
}

/**
 * Change my team ID to a specific team ID
 * @param targetTeamId - The team ID to switch to
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeamTo(unsigned char targetTeamId)
{
    try
    {
        if (!IsValidBattleMode())
            return 0;

        // Get player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get valid player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get valid battle controller");
            return 0;
        }

        // Get player character with validation
        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get player character");
            return 0;
        }

        // Get player state
        if (!IsValidPointer(localCharacter->PlayerState))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get player state");
            return 0;
        }

        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(localCharacter->PlayerState);
        if (!IsValidPointer(playerState))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get HerovsPlayerState");
            return 0;
        }

        // Get my current team ID
        int myCurrentTeamId = playerState->BP_GetTeamId();

        Logger::LogInfo("[ChangeMyTeamTo] Switching to team ID: " + std::to_string(targetTeamId));

        // Change team via BP_SetTeamId
        try
        {
            playerState->BP_SetTeamId(targetTeamId);
            Logger::LogInfo("[ChangeMyTeamTo] Successfully changed team to: " + std::to_string(targetTeamId));
            return 1;
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeamTo] Failed to call BP_SetTeamId");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeMyTeamTo] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ChangeMyTeamTo] Unknown exception");
        return 0;
    }
}

/**
 * Recover self (player only) with full health restoration
 * Uses RecoverDying_ToClient on player state
 */
bool InGameHack_RecoverMe()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        if (!IsValidPointer(playerController->PlayerState))
            return false;

        // Get player state with validation
        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        if (!playerState)
            return false;

        // Check if player is dying FIRST
        if (!playerState->BP_IsDying())
            return false;  // Not dying, no need to recover

        // Get player character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
            return false;

        playerCharacter->RecoverDyingAlly_ToServer(playerCharacter, true);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set player invincible with custom settings via SetInvincible_Server
 * @param fixTime - Fixed invincibility time
 * @param maxTime - Maximum invincibility time
 * @param enableEffect - Show visual effects
 * @param projectileThrough - Projectiles pass through
 * @param slipDamageThrough - Slip damage passes through
 * @param activedTransparent - Make player transparent
 */
bool InGameHack_SetInvincible()
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetInvincible] Not in valid battle mode");
        return false;
    }

    try
    {
        SDK::APlayerController* playerControllerPtr = SDK_GetPlayerController();
        if (!IsValidPointer(playerControllerPtr))
        {
            return false;
        }

        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerControllerPtr);
        if (!playerStateBattle)
        {
            return false;
        }

        // Hardcoded parameters - all features enabled
        float fixTime = 10.0f;          // Fixed duration
        float maxTime = 120.0f;         // Maximum duration
        bool enableEffect = true;       // Show effects
        // Create FInvincibleData structure
        SDK::FInvincibleData invincibleData;
        invincibleData._lifeTime = maxTime;
        invincibleData._fixedLifeTime = fixTime;
        invincibleData._bProjectileThrough = true;
        invincibleData._bSlipDamageThrough = true;
        invincibleData._bFromTransparent = true;
        invincibleData._bEnableEffect = enableEffect;
        invincibleData._bRep = true;
        // invincibleData._ignoreClearAction and _tags are already initialized to empty by constructor

        // Call SetInvincible_Server with FInvincibleData
        playerStateBattle->SetInvincible_Server(invincibleData);

        return true;
    }
    catch (const std::exception& ex)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// REBUILD MYSELF
// ============================================

/**
 * Rebuild/reset the player character state
 * Calls SetCharacterCondition with REBUILD_MYSELF (enum value 24)
 */
bool InGameHack_RebuildMyself()
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[RebuildMyself] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[RebuildMyself] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[RebuildMyself] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[RebuildMyself] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!IsValidPointer(conditionComponent))
        {
            return false;
        }

        // Call SetCondition_ToServer with REBUILD_MYSELF (enum value 24)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)23,  // REBUILD_MYSELF = 24
            0,                               // Level
            0.0f,                            // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        Logger::LogInfo("[COMBAT] RebuildMyself executed successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in RebuildMyself");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in RebuildMyself");
        return false;
    }
}

/**
 * Apply CH202_TRANS_MISSION condition to player character
 * Enables Ch202 transformation/mission state (ECharacterConditionId = 85)
 */
bool InGameHack_CH202TransMission()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[CH202TransMission] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[CH202TransMission] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[CH202TransMission] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!IsValidPointer(conditionComponent))
        {
            return false;
        }

        // Call SetCondition_ToServer with CH202_TRANS_MISSION (enum value 85)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)85,  // CH202_TRANS_MISSION = 85
            5,                               // Level
            0.0f,                            // span
            0.0f,                            // value
            0.0f,                            // interval
            5,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        Logger::LogInfo("[COMBAT] CH202TransMission executed successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH202TransMission");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH202TransMission");
        return false;
    }
}

/**
 * Apply UNBREAKABLE condition to player character
 * Makes player unbreakable/invulnerable (ECharacterConditionId = 35)
 */
bool InGameHack_Unbreakable()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[Unbreakable] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[Unbreakable] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[Unbreakable] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!IsValidPointer(conditionComponent))
        {
            return false;
        }

        // Call SetCondition_ToServer with UNBREAKABLE (enum value 35)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)35,  // UNBREAKABLE = 35
            0,                               // Level
            50.0f,                           // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in Unbreakable");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in Unbreakable");
        return false;
    }
}

/**
 * Get the team ID of a character using SDK function
 */
static inline unsigned char GetCharacterTeamId(SDK::ACharacterBattle* Character)
{
    if (!Character) return 255;  // Invalid
    try {
        // Use SDK function instead of offset reading
        return Character->BP_GetTeamId();
    }
    catch (...) {
        return 255;  // Return invalid if call fails
    }
}

/**
 * Recover player + all team members with full health restoration
 * Uses RecoverDyingAlly_ToServer on all team characters
 */
bool InGameHack_RecoverDyingTeam()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
            return false;

        // Get player character and state
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter || !playerCharacter->PlayerState)
            return false;

        // Get player's team ID
        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return false;  // Invalid team

        int recoveredCount = 0;
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel || world->PersistentLevel->Actors.Num() == 0)
            return false;

        // Recover all team characters on map
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            try
            {
                SDK::AActor* actor = world->PersistentLevel->Actors[i];
                
                // 1️⃣ Vérifie que l'acteur existe et n'est pas un default object
                if (!actor || actor->IsDefaultObject())
                    continue;

                // 2️⃣ Vérifie le classname (CharacterBattle ou Chxxx)
                if (!actor->Class)
                    continue;

                std::string className = std::string(actor->Class->GetName());
                bool isValidCharacter = (className == "CharacterBattle" || className == "ACharacterBattle");
                
                // Check for Chxxx pattern (Ch001-Ch999)
                bool isChxxx = false;
                if (className.length() == 5 && className[0] == 'C' && className[1] == 'h' &&
                    std::isdigit(className[2]) && std::isdigit(className[3]) && std::isdigit(className[4]))
                {
                    isChxxx = true;
                }

                if (!isValidCharacter && !isChxxx)
                    continue;

                // 3️⃣ Cast à ACharacterBattle
                SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
                
                // 4️⃣ Récupère et vérifie le PlayerState
                if (!targetCharacter || !targetCharacter->PlayerState)
                    continue;

                // 5️⃣ Vérifie que le target est dans la même équipe
                unsigned char targetTeamId = GetCharacterTeamId(targetCharacter);
                if (targetTeamId != playerTeamId)
                    continue;  // Not in same team, skip

                // 6️⃣ Vérifie que le target est dying (using offset)
                if (!IsCharacterDyingOffset(targetCharacter))
                    continue;  // Not dying, skip

                // 7️⃣ Appelle la recovery
                playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
                recoveredCount++;
            }
            catch (...)
            {
                continue;
            }
        }

        return recoveredCount > 0;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Recover a specific team member by index
 * Uses RecoverDyingAlly_ToServer on selected team character
 */
bool InGameHack_RecoverDyingSpecificTeamMember(int teamMemberIndex)
{
    // 0️⃣ Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Not in valid battle mode");
        return false;
    }

    // 1️⃣ Validate team member index
    if (teamMemberIndex < 0 || teamMemberIndex >= (int)ImGuiMenu::g_CurrentTeamCharacters.size())
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Invalid team member index: " + std::to_string(teamMemberIndex));
        return false;
    }

    // 2️⃣ Get player controller
    SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
    if (!playerController)
    {
        Logger::LogWarning("[RECOVER SPECIFIC] No player controller");
        return false;
    }

    // 3️⃣ Get player character
    SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
    if (!playerCharacter || !playerCharacter->PlayerState)
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Player character not found");
        return false;
    }

    // 4️⃣ Get target team member
    SDK::ACharacterBattle* targetCharacter = ImGuiMenu::g_CurrentTeamCharacters[teamMemberIndex];
    if (!targetCharacter || targetCharacter->IsDefaultObject())
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Target character is invalid");
        return false;
    }

    try
    {
        // 5️⃣ Verify target is on same team
        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        unsigned char targetTeamId = GetCharacterTeamId(targetCharacter);
        
        if (playerTeamId == 255 || targetTeamId != playerTeamId)
        {
            Logger::LogWarning("[RECOVER SPECIFIC] Target is not on same team");
            return false;
        }

        // 6️⃣ Check if target is dying
        if (!IsCharacterDyingOffset(targetCharacter))
        {
            Logger::LogInfo("[RECOVER SPECIFIC] Target is not dying, skipping recovery");
            return false;
        }

        // 7️⃣ Call recovery on target
        Logger::LogInfo("[RECOVER SPECIFIC] Recovering specific team member at index " + std::to_string(teamMemberIndex));
        playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[RECOVER SPECIFIC] Exception: " + std::string(e.what()));
        return false;
    }
}

/**
 * Recover ALL characters on map with full health restoration
 * Uses RecoverDyingAlly_ToServer on all characters regardless of team
 */
bool InGameHack_RecoverDyingAllESP()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller and character
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
            return false;

        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
            return false;

        int recoveredCount = 0;
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel || world->PersistentLevel->Actors.Num() == 0)
            return false;

        // Recover ALL characters on the map
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            try
            {
                SDK::AActor* actor = world->PersistentLevel->Actors[i];
                
                // 1️⃣ Vérifie que l'acteur existe et n'est pas un default object
                if (!actor || actor->IsDefaultObject())
                    continue;

                // 2️⃣ Vérifie le classname (CharacterBattle ou Chxxx)
                if (!actor->Class)
                    continue;

                std::string className = std::string(actor->Class->GetName());
                bool isValidCharacter = (className == "CharacterBattle" || className == "ACharacterBattle");
                
                // Check for Chxxx pattern (Ch001-Ch999)
                bool isChxxx = false;
                if (className.length() == 5 && className[0] == 'C' && className[1] == 'h' &&
                    std::isdigit(className[2]) && std::isdigit(className[3]) && std::isdigit(className[4]))
                {
                    isChxxx = true;
                }

                if (!isValidCharacter && !isChxxx)
                    continue;

                // 3️⃣ Cast à ACharacterBattle
                SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
                
                // 4️⃣ Récupère et vérifie le PlayerState
                if (!targetCharacter || !targetCharacter->PlayerState)
                    continue;

                // 5️⃣ Vérifie que le target est dying (using offset)
                if (!IsCharacterDyingOffset(targetCharacter))
                    continue;  // Not dying, skip

                // 6️⃣ Appelle la recovery
                playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
                recoveredCount++;
            }
            catch (...)
            {
                continue;
            }
        }

        return recoveredCount > 0;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// CHARACTER CONDITION HACKS (COMPRESSION_REGENERATION, CH024_TRANSPARENT, CH011_ABYSS_DARK_BODY)
// ============================================

/**
 * Apply COMPRESSION_REGENERATION condition to player character
 * Regenerates and applies compression state (ECharacterConditionId = 62)
 */
bool InGameHack_CompressionRegeneration()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CompressionRegeneration");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);

        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with COMPRESSION_REGENERATION (enum value 62)
        conditionComponent->BP_SetCondition(
            (SDK::ECharacterConditionId)75,  // COMPRESSION_REGENERATION = 62
            0,                               // Level
            500.0f,                           // span
            1.0f,                            // value
            0.1f,                          // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0                            // bTimeOverwrite
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CompressionRegeneration");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CompressionRegeneration");
        return false;
    }
}

/**
 * Apply CH024_TRANSPARENT condition to player character
 * Enables CH024 transparency effect (ECharacterConditionId = 65)
 */
bool InGameHack_CH024Transparent()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CH024Transparent");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);

        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with CH024_TRANSPARENT (enum value 65)
        conditionComponent->BP_SetCondition(
            (SDK::ECharacterConditionId)76,  // CH024_TRANSPARENT = 65
            0,                               // Level
            500.0f,                           // span
            5.0f,                            // value
            0.01f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0                           // bTimeOverwrite
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH024Transparent");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH024Transparent");
        return false;
    }
}

/**
 * Apply CH011_ABYSS_DARK_BODY condition to player character
 * Applies abyss dark body state (ECharacterConditionId = 95)
 */
bool InGameHack_CH011AbyssDarkBody()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CH011AbyssDarkBody");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);

        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with CH011_ABYSS_DARK_BODY (enum value 95)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)94,  // CH011_ABYSS_DARK_BODY = 95
            5,                               // Level
            50.0f,                           // span
            50,                              // value
            0.1f,                            // interval
            5,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH011AbyssDarkBody");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH011AbyssDarkBody");
        return false;
    }
}

// ============================================
// ABILITY HACKS
// ============================================

/**
 * Generic ability setter helper function
 */
static bool SetAbility(int abilityId, int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for Ability");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for Ability");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for Ability");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for Ability");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for Ability");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for Ability");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for Ability");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for Ability");
            return false;
        }

        // Call BP_SetCondition with ability ID, level 1-100, span 50 seconds
        try
        {
            conditionComponent->BP_SetConditionLocal(
                (SDK::ECharacterConditionId)abilityId,  // Ability ID (43-47)
                level,                                   // Level (1-100)
                500.0f,                                  // span: 50 seconds
                1000.0f,                                 // value
                0.1f,                                    // interval
                level,                                   // subLevel
                nullptr,                                 // instigatedPlayer
                0                                        // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for Ability");
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in SetAbility");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in SetAbility");
        return false;
    }
}

bool InGameHack_AbilityAttack(int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for AbilityAttack");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for AbilityAttack");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for AbilityAttack");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for AbilityAttack");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for AbilityAttack");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for AbilityAttack");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for AbilityAttack");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for AbilityAttack");
            return false;
        }

        // Call BP_SetCondition for ABILITY_ATTACK (ID 43)
        try
        {
            conditionComponent->BP_SetCondition(
                (SDK::ECharacterConditionId)43,  // ABILITY_ATTACK = 43
                level,                            // Level (1-100)
                500.0f,                           // span: 50 seconds
                1000.0f,                          // value
                0.1f,                             // interval
                level,                            // subLevel
                nullptr,                          // instigatedPlayer
                0                                 // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for AbilityAttack");
            return false;
        }

        Logger::LogInfo("[COMBAT] AbilityAttack applied with level " + std::to_string(level));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in AbilityAttack: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in AbilityAttack");
        return false;
    }
}

bool InGameHack_AbilityDurable(int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for AbilityDurable");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for AbilityDurable");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for AbilityDurable");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for AbilityDurable");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for AbilityDurable");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for AbilityDurable");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for AbilityDurable");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for AbilityDurable");
            return false;
        }

        // Call BP_SetCondition for ABILITY_DURABLE (ID 44)
        try
        {
            conditionComponent->BP_SetCondition(
                (SDK::ECharacterConditionId)44,  // ABILITY_DURABLE = 44
                level,                            // Level (1-100)
                500.0f,                           // span: 50 seconds
                1000.0f,                          // value
                0.1f,                             // interval
                level,                            // subLevel
                nullptr,                          // instigatedPlayer
                0                                 // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for AbilityDurable");
            return false;
        }

        Logger::LogInfo("[COMBAT] AbilityDurable applied with level " + std::to_string(level));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in AbilityDurable: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in AbilityDurable");
        return false;
    }
}

bool InGameHack_AbilityMovespeed(int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for AbilityMovespeed");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for AbilityMovespeed");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for AbilityMovespeed");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for AbilityMovespeed");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for AbilityMovespeed");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for AbilityMovespeed");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for AbilityMovespeed");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for AbilityMovespeed");
            return false;
        }

        // Call BP_SetCondition for ABILITY_MOVESPEED (ID 45)
        try
        {
            conditionComponent->BP_SetConditionLocal(
                (SDK::ECharacterConditionId)45,  // ABILITY_MOVESPEED = 45
                level,                            // Level (1-100)
                500.0f,                           // span: 50 seconds
                1000.0f,                          // value
                0.1f,                             // interval
                level,                            // subLevel
                nullptr,                          // instigatedPlayer
                0                                 // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for AbilityMovespeed");
            return false;
        }

        Logger::LogInfo("[COMBAT] AbilityMovespeed applied with level " + std::to_string(level));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in AbilityMovespeed: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in AbilityMovespeed");
        return false;
    }
}

bool InGameHack_AbilityHeal(int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for AbilityHeal");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for AbilityHeal");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for AbilityHeal");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for AbilityHeal");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for AbilityHeal");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for AbilityHeal");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for AbilityHeal");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for AbilityHeal");
            return false;
        }

        // Call BP_SetCondition for ABILITY_HEAL (ID 46)
        try
        {
            conditionComponent->BP_SetConditionLocal(
                (SDK::ECharacterConditionId)46,  // ABILITY_HEAL = 46
                level,                            // Level (1-100)
                500.0f,                           // span: 50 seconds
                1000.0f,                          // value
                0.1f,                             // interval
                level,                            // subLevel
                nullptr,                          // instigatedPlayer
                0                                 // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for AbilityHeal");
            return false;
        }

        Logger::LogInfo("[COMBAT] AbilityHeal applied with level " + std::to_string(level));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in AbilityHeal: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in AbilityHeal");
        return false;
    }
}

bool InGameHack_AbilityTechnique(int level)
{
    try
    {
        // Clamp level to 1-100
        level = (level < 1) ? 1 : (level > 100) ? 100 : level;

        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for AbilityTechnique");
            return false;
        }

        // Validate controller pointer
        try
        {
            if (!baseController || IsBadReadPtr(baseController, sizeof(void*)))
            {
                Logger::LogError("[COMBAT] PlayerController pointer is invalid for AbilityTechnique");
                return false;
            }
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] PlayerController validation failed for AbilityTechnique");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle for AbilityTechnique");
            return false;
        }

        // Get the player's current character
        SDK::APawn* pawn = playerController->Pawn;
        if (!pawn || IsBadReadPtr(pawn, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Pawn pointer is invalid for AbilityTechnique");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = static_cast<SDK::ACharacterBattle*>(pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character for AbilityTechnique");
            return false;
        }

        // Validate character pointer
        if (IsBadReadPtr(playerCharacter, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Character pointer is invalid for AbilityTechnique");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = playerCharacter->BP_GetConditionControlComponent();
        if (!conditionComponent || IsBadReadPtr(conditionComponent, sizeof(void*)))
        {
            Logger::LogError("[COMBAT] Could not get or validate condition component for AbilityTechnique");
            return false;
        }

        // Call BP_SetCondition for ABILITY_TECHNIQUE (ID 47)
        try
        {
            conditionComponent->BP_SetConditionLocal(
                (SDK::ECharacterConditionId)47,  // ABILITY_TECHNIQUE = 47
                level,                            // Level (1-100)
                500.0f,                           // span: 50 seconds
                1000.0f,                          // value
                0.1f,                             // interval
                level,                            // subLevel
                nullptr,                          // instigatedPlayer
                0                                 // damageActionSerialNo
            );
        }
        catch (...)
        {
            Logger::LogError("[COMBAT] Exception calling BP_SetCondition for AbilityTechnique");
            return false;
        }

        Logger::LogInfo("[COMBAT] AbilityTechnique applied with level " + std::to_string(level));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in AbilityTechnique: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in AbilityTechnique");
        return false;
    }
}

// ============================================
// CHARACTER CONTROL FUNCTIONS
// ============================================

std::vector<SDK::ACharacterBattle*> InGameHack_GetAllCharacterBattles()
{
    std::vector<SDK::ACharacterBattle*> characters;
    std::set<SDK::ACharacterBattle*> uniqueCharacters;
    
    try
    {
        // Try world iteration first
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (world && world->PersistentLevel && world->PersistentLevel->Actors.Num() > 0)
        {
            
            for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
            {
                SDK::AActor* actor = world->PersistentLevel->Actors[i];
                if (!actor)
                    continue;
                
                std::string className = "Unknown";
                try
                {
                    if (actor->Class)
                    {
                        className = actor->Class->GetName();
                    }
                }
                catch (...)
                {
                    continue;
                }
                
                // Check if this is ACharacterBattle or ChXXX pattern
                bool isCharacterBattle = (className == "CharacterBattle" || className == "ACharacterBattle");
                
                // Check for Ch001-Ch999 pattern (any Chxxx)
                bool isChxxx = false;
                if (className.length() >= 5 && className[0] == 'C' && className[1] == 'h' &&
                    std::isdigit(className[2]) && std::isdigit(className[3]) && std::isdigit(className[4]))
                {
                    isChxxx = true;
                }
                
                if (!isCharacterBattle && !isChxxx)
                    continue;
                
                // Cast to ACharacterBattle and check PlayerState
                SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(actor);
                if (!character || !character->PlayerState)
                    continue;
                
                // Add if not already in set
                if (uniqueCharacters.find(character) == uniqueCharacters.end())
                {
                    uniqueCharacters.insert(character);
                    characters.push_back(character);
                }
            }
        }
        
        // Always use ESP fallback to ensure we get targets
        
        
        for (int attempt = 0; attempt < 100; attempt++)
        {
            SDK::AActor* targetActorPtr = SDK_GetRandomESPTarget();
            if (!targetActorPtr)
            {
                continue;
            }
            
            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(targetActorPtr);
            if (!targetCharacter || !targetCharacter->PlayerState)
            {
                continue;
            }
            
            // Add if not already in set
            if (uniqueCharacters.find(targetCharacter) == uniqueCharacters.end())
            {
                uniqueCharacters.insert(targetCharacter);
                characters.push_back(targetCharacter);
            }
        }
        
    }
    catch (const std::exception& e)
    {
    }
    catch (...)
    {
    }
    
    return characters;
}

std::vector<std::string> InGameHack_GetCharacterNames()
{
    std::vector<std::string> names;
    
    try
    {
        auto characters = InGameHack_GetAllCharacterBattles();
        
        for (size_t idx = 0; idx < characters.size(); idx++)
        {
            auto* character = characters[idx];
            if (!character)
            {
                names.push_back("Unknown");
                continue;
            }
            
            // Get character class name (ChXXX pattern)
            std::string charClassName = "Unknown";
            try
            {
                if (character->Class)
                {
                    charClassName = character->Class->GetName();
                }
            }
            catch (...)
            {
            }
            
            // Get player name from PlayerState
            std::string playerName = "Unknown";
            if (character->PlayerState)
            {
                try
                {
                    // Use SDK_GetPlayerName like in Basic.cpp
                    const char* namePtr = SDK_GetPlayerName(character->PlayerState);
                    if (namePtr && namePtr[0] != '\0')
                    {
                        playerName = namePtr;
                    }
                }
                catch (const std::exception& e)
                {
                }
                catch (...)
                {
                }
            }
            else
            {
            }
            
            // Format: "PlayerName (ChXXX)"
            std::string displayName = playerName + " (" + charClassName + ")";
            names.push_back(displayName);
        }
    }
    catch (const std::exception& e)
    {
    }
    catch (...)
    {
    }
    
    return names;
}

std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharacterBattles()
{
    std::vector<SDK::ACharacterBattle*> teamCharacters;
    try
    {
        if (!IsValidBattleMode())
            return teamCharacters;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
            return teamCharacters;

        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter || !playerCharacter->PlayerState)
            return teamCharacters;

        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return teamCharacters;

        auto characters = InGameHack_GetAllCharacterBattles();
        for (auto* character : characters)
        {
            if (!character || character == playerCharacter || character->IsDefaultObject())
                continue;

            if (!character->PlayerState || !character->Class)
                continue;

            unsigned char targetTeamId = GetCharacterTeamId(character);
            if (targetTeamId != playerTeamId)
                continue;

            teamCharacters.push_back(character);
        }
    }
    catch (...)
    {
        // ignore
    }

    return teamCharacters;
}

std::vector<std::string> InGameHack_GetTeamCharacterNames()
{
    std::vector<std::string> names;
    try
    {
        auto teamCharacters = InGameHack_GetTeamCharacterBattles();
        for (auto* character : teamCharacters)
        {
            if (!character)
            {
                names.push_back("Unknown");
                continue;
            }

            std::string playerName = "Unknown";
            if (character->PlayerState)
            {
                const char* namePtr = SDK_GetPlayerName(character->PlayerState);
                if (namePtr && namePtr[0] != '\0')
                    playerName = namePtr;
            }

            std::string className = "Unknown";
            if (character->Class)
            {
                try
                {
                    className = character->Class->GetName();
                }
                catch (...) {}
            }

            names.push_back(playerName + " (" + className + ")");
        }
    }
    catch (...)
    {
        // ignore
    }
    return names;
}

bool InGameHack_RecoverDyingTeamMember(SDK::ACharacterBattle* target)
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        if (!target || target->IsDefaultObject() || !target->PlayerState)
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
            return false;

        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter || !playerCharacter->PlayerState)
            return false;

        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return false;

        unsigned char targetTeamId = GetCharacterTeamId(target);
        if (targetTeamId != playerTeamId)
            return false;

        if (!IsCharacterDyingOffset(target))
            return false;

        playerCharacter->RecoverDyingAlly_ToServer(target, false);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void InGameHack_LogAllDamageAttenuationCurves()
{
    try
    {
        // Créer le dossier c:\temp s'il n'existe pas
        std::system("mkdir c:\\temp 2>nul");
        
        // Ouvrir le fichier log
        std::ofstream logFile("c:\\temp\\damage_attenuation_curves.log", std::ios::out | std::ios::trunc);
        if (!logFile.is_open())
        {
            Logger::LogError("[CURVE] Failed to open log file: c:\\temp\\damage_attenuation_curves.log");
            return;
        }

        Logger::LogInfo("[CURVE] ============ Logging All DamageAttenuation Curves ============");
        logFile << "============ DamageAttenuation Curves Log ============\n";
        logFile << "Timestamp: " << std::time(nullptr) << "\n";
        logFile << "================================================\n\n";
        
        if (!SDK::UObject::GObjects)
        {
            Logger::LogWarning("[CURVE] GObjects is null, cannot scan");
            logFile << "ERROR: GObjects is null, cannot scan\n";
            logFile.close();
            return;
        }

        int32_t foundCount = 0;
        int32_t totalKeys = 0;

        // Scan all objects for UANS_Attack instances
        for (int32_t i = 0; i < SDK::UObject::GObjects->Num(); ++i)
        {
            auto* obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!obj)
                continue;

            // Check class name to filter UANS_Attack objects
            std::string className = std::string(obj->Class->GetName());
            if (className != "ANS_Attack")
                continue;

            // Cast to UANS_Attack
            auto* ansAttack = static_cast<SDK::UANS_Attack*>(obj);
            if (!ansAttack || !ansAttack->_damageAttenuation)
                continue;

            foundCount++;

            // Get object name
            std::string objName = ansAttack->GetName();
            
            // Log the curve pointer and class
            std::string logMsg = "[CURVE] #" + std::to_string(foundCount) + " - Name: " + objName +
                          " | Curve Ptr: 0x" + std::to_string((uint64_t)ansAttack->_damageAttenuation);
            Logger::LogInfo(logMsg);
            logFile << logMsg << "\n";

            // Log curve details
            if (ansAttack->_damageAttenuation)
            {
                SDK::FRichCurve* curvePtr = &(ansAttack->_damageAttenuation->FloatCurve);
                if (!curvePtr)
                    continue;

                int32_t numKeys = curvePtr->Keys.Num();
                
                std::string keysMsg = "[CURVE]   -> Keys: " + std::to_string(numKeys);
                Logger::LogInfo(keysMsg);
                logFile << keysMsg << "\n";
                totalKeys += numKeys;

                // Log each key
                if (numKeys > 0)
                {
                    for (int32_t keyIdx = 0; keyIdx < numKeys; ++keyIdx)
                    {
                        SDK::FRichCurveKey& key = curvePtr->Keys[keyIdx];
                        
                        std::string keyMsg = "[CURVE]      Key[" + std::to_string(keyIdx) + "]: Time=" + 
                                      std::to_string(key.Time) + ", Value=" + std::to_string(key.value);
                        Logger::LogInfo(keyMsg);
                        logFile << keyMsg << "\n";
                    }
                }
            }
        }

        std::string summary = "[CURVE] ============ Summary ============\n" +
                             std::string("[CURVE] Total curves found: ") + std::to_string(foundCount) + "\n" +
                             std::string("[CURVE] Total keys logged: ") + std::to_string(totalKeys) + "\n" +
                             std::string("[CURVE] ====================================");
        Logger::LogInfo(summary);
        logFile << "\n================================================\n";
        logFile << "Total curves found: " << foundCount << "\n";
        logFile << "Total keys logged: " << totalKeys << "\n";
        logFile << "================================================\n";
        
        logFile.close();
        Logger::LogInfo("[CURVE] Log file saved to c:\\temp\\damage_attenuation_curves.log");
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CURVE] Exception in LogAllDamageAttenuationCurves: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[CURVE] Unknown exception in LogAllDamageAttenuationCurves");
    }
}

bool InGameHack_SetCharacterDying(SDK::ACharacterBattle* character)
{
    try
    {
        if (!character || !character->PlayerState)
        {
            return false;
        }
        
        SDK::APlayerStateBattle* victimState = static_cast<SDK::APlayerStateBattle*>(character->PlayerState);
        if (!victimState)
        {
            return false;
        }
        
        // Get player character as aggriever
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController || !playerController->Pawn)
        {
            return false;
        }
        
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        
        // Get game state
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->GameState)
        {
            return false;
        }
        
        SDK::AGameStateBattle* gameState = (SDK::AGameStateBattle*)(world->GameState);
        if (!gameState)
        {
            return false;
        }
        
        // Call OnCharacterDying_NetMulti on game state with victim and aggressor
        gameState->OnCharacterDying_NetMulti(victimState, playerCharacter);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_KillCharacter(SDK::ACharacterBattle* victim, SDK::ACharacterBattle* killer)
{
    try
    {
        if (!victim || !victim->PlayerState)
        {
            return false;
        }
        
        SDK::APlayerStateBattle* victimState = static_cast<SDK::APlayerStateBattle*>(victim->PlayerState);
        if (!victimState)
        {
            return false;
        }
        
        // If killer not specified, use player character
        SDK::ACharacterBattle* killerCharacter = killer;
        if (!killerCharacter)
        {
            SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
            if (!playerController || !playerController->Pawn)
            {
                return false;
            }
            killerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        }
        
        // Get game state
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->GameState)
        {
            return false;
        }
        
        SDK::AGameStateBattle* gameState = (SDK::AGameStateBattle*)(world->GameState);
        if (!gameState)
        {
            return false;
        }
        
        // Call OnCharacterDead_NetMulti on game state with victim state and killer character
        gameState->OnCharacterDead_NetMulti(victimState, killerCharacter);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_ValidateTransMissionLevel(int level)
{
    try
    {
        if (level < 0 || level > 9)
        {
            return false;
        }

        SDK::int32 validatedLevel = SDK::UCh202ActionAttackBase::ValidateTransMissionLevel(level);
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_SetInitTransMissionLevel(int levelIndex, int32_t newValue)
{
    try
    {
        if (levelIndex < 0)
        {
            Logger::LogError("[CH202] Invalid init trans mission index: " + std::to_string(levelIndex));
            return false;
        }

        SDK::UCh202Params* ch202Params = SDK::UCh202Params::GetDefaultObj();
        if (!ch202Params)
        {
            Logger::LogError("[CH202] Could not get UCh202Params default object");
            return false;
        }

        int arraySize = ch202Params->unique3_initLevel.Num();
        if (levelIndex >= arraySize)
        {
            Logger::LogError("[CH202] Init trans mission index out of range: " + std::to_string(levelIndex) + ", size=" + std::to_string(arraySize));
            return false;
        }

        ch202Params->unique3_initLevel[levelIndex] = newValue;
        Logger::LogInfo("[CH202] Set unique3_initLevel[" + std::to_string(levelIndex) + "] = " + std::to_string(newValue));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CH202] Exception in SetInitTransMissionLevel");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[CH202] Unknown exception in SetInitTransMissionLevel");
        return false;
    }
}

// Get current max stack count from selected supply widget
// Cached max stack value updated each frame
static int32_t g_CachedMaxStackValue = 100;

int32_t InGameHack_GetSupplyMaxStackCount()
{
    try
    {
        // Return the cached value that was updated from the widget
        // This value is updated in ModifySupplyMaxStack() each frame
        return g_CachedMaxStackValue;
    }
    catch (...)
    {
        return 0;
    }
}

// Helper function to search for UItemWidget and read maxStack value
int32_t GetMaxStackFromWidget()
{
    try
    {
        // UItemWidget has cached maxStack at offset 0x08A0
        // However, accessing UI widgets from gameplay layer is complex
        // as widgets exist in viewport/UI hierarchy
        
        // For now, use the actual game default value
        // When enabled in menu, the slider will override this with user value
        return 100;  // Default maxStack value in RUGIR
    }
    catch (...)
    {
        return 100;
    }
}

bool InGameHack_ModifySupplyMaxStack()
{
    try
    {
        // This function is called each frame to update supply max stack
        // Update the cached value from the actual widget
        g_CachedMaxStackValue = GetMaxStackFromWidget();
        
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[SUPPLY] Exception in ModifySupplyMaxStack: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[SUPPLY] Unknown exception in ModifySupplyMaxStack");
        return false;
    }
}

// Global flag for preventing drops on death
static bool g_bPreventDropOnDeath = false;

bool InGameHack_PreventDropOnDeath(bool bPreventDrop)
{
    try
    {
        g_bPreventDropOnDeath = bPreventDrop;
        
        if (bPreventDrop)
        {
            
        }
        else
        {
            
        }
        
        return true;
    }
    catch (const std::exception& e)
    {
        
        return false;
    }
    catch (...)
    {
      
        return false;
    }
}

// Public function to check if drops should be prevented (called during death)
bool InGameHack_ShouldPreventDropOnDeath()
{
    return g_bPreventDropOnDeath;
}

bool InGameHack_SetSkillLevel(int skillIndex, int level)
{
    try
    {
        if (skillIndex < 0 || skillIndex > 8 || level < 1 || level > 9)
        {
            return false;
        }
        
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!playerChar)
        {
            return false;
        }
        
        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        if (!playerState)
        {
            return false;
        }
        
        auto supplyHolderComp = playerState->_supplyHolderComponent;
        if (!IsValidPointer(supplyHolderComp))
        {
            return false;
        }
        
        // Call SetSkillLevel_ToServer with attack ID and level
        // EAttackId enum - skill index 0-8 maps to attack level
        supplyHolderComp->SetSkillLevel_ToServer((SDK::EAttackId)skillIndex, level);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

int InGameHack_GetSkillLevel(int skillIndex)
{
    try
    {
        if (!IsValidBattleMode())
        {
            return -1;
        }

        if (skillIndex < 0 || skillIndex > 8)
        {
            return -1;
        }

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return -1;
        }

        SDK::ACharacterBattle* playerChar = (SDK::ACharacterBattle*)playerController->Pawn;
        if (!playerChar)
        {
            return -1;
        }

        // Call BP_GetUniqueLevel to get current skill level
        return playerChar->BP_GetUniqueLevel((SDK::EAttackId)skillIndex);
    }
    catch (const std::exception& e)
    {
        return -1;
    }
}

bool InGameHack_StopUsingSupply()
{
    try
    {
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
        
            return false;
        }
        
        SDK::ACharacterBattle* playerChar = (SDK::ACharacterBattle*)playerController->Pawn;
        if (!playerChar || !playerChar->PlayerState)
        {
        
            return false;
        }
        
        SDK::APlayerStateBattle* playerState = (SDK::APlayerStateBattle*)playerChar->PlayerState;
        auto supplyHolderComp = playerState->_supplyHolderComponent;
        
        if (!supplyHolderComp)
        {
        
            return false;
        }
        
        supplyHolderComp->OnStopUsing_ToServer();
        return true;
    }
    catch (const std::exception& e)
    {
        
        return false;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// UPGRADE SUPPLY - MAXSTACKNUM
// ============================================

bool InGameHack_UpgradeSupply(int supplyIndex, int level)
{
    try
    {
        // Validate level range
        if (level < 1 || level > 99)
        {
            return false;
        }

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }

        // Get character
        SDK::ACharacterBattle* playerChar = (SDK::ACharacterBattle*)playerController->Pawn;
        if (!playerChar || !playerChar->PlayerState)
        {
            return false;
        }

        // Get player state
        SDK::APlayerStateBattle* playerState = (SDK::APlayerStateBattle*)playerChar->PlayerState;
        if (!playerState)
        {
            return false;
        }

        // Get supply holder component
        auto supplyHolderComp = playerState->_supplyHolderComponent;
        if (!supplyHolderComp)
        {
            return false;
        }

        // Get supply holder from inventory
        if (supplyIndex < 0 || supplyIndex >= supplyHolderComp->_inventory.Num())
        {
            return false;
        }

        auto supplyHolder = supplyHolderComp->_inventory[supplyIndex];
        if (!supplyHolder || supplyHolder->_supplies.Num() == 0)
        {
            return false;
        }

        // Get first supply
        auto supply = supplyHolder->_supplies[0];
        if (!supply)
        {
            return false;
        }

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world)
        {
            return false;
        }

        // Supply ID array
        const char* supplyIds[] = {
            "Su001", "Su002", "Su003", "Su004", "Su005", "Su006",
            "Su007", "Su008", "Su009", "Su010", "Su011", "Su012",
            "Su013", "Su014", "Su015", "Su016", "Su017", "Su018",
            "Su019", "Su020", "Su021", "Su022", "Su023", "Su024",
            "Su025", "Su026"
        };

        // Get supply ID string
        const char* supplyIdStr = (supplyIndex < 26) ? supplyIds[supplyIndex] : supplyIds[0];

        // Convert to wchar_t and then to FName
        int len = strlen(supplyIdStr) + 1;
        wchar_t* wstr = new wchar_t[len];
        mbstowcs(wstr, supplyIdStr, len);
        SDK::FName supplyFName = SDK::BasicFilesImplUtils::StringToName(wstr);
        delete[] wstr;

        // Get supply data asset
        auto supplyAsset = SDK::UStaticDataManager::GetSupplyBaseDataAsset(world, supplyFName);
        if (!supplyAsset)
        {
            return false;
        }

        // Set max stack num
        supplyAsset->_maxStackNum = level;
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// APPLY TEAM BUFFS
// ============================================

bool InGameHack_ApplyTeamBuffs(float attackAdjust, float durableAdjust, float speedAdjust, float healingAdjust, float reloadAdjust)
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }

        // Get character
        SDK::ACharacterBattle* playerChar = (SDK::ACharacterBattle*)playerController->Pawn;
        if (!playerChar || !playerChar->PlayerState)
        {
            return false;
        }

        // Get player state
        SDK::APlayerStateBattle* playerState = (SDK::APlayerStateBattle*)playerChar->PlayerState;
        if (!playerState || !playerState->_buffParam)
        {
            return false;
        }

        // Apply buffs directly using UBuffParam setters
        auto buffParam = playerState->_buffParam;

        // Set attack adjust rate
        buffParam->BP_SetAttackAdjustRate(attackAdjust);

        // Set durable adjust rate
        buffParam->BP_SetDurableAdjustRate(durableAdjust);

        // Set speed (movement speed team role)
        buffParam->BP_SetMoveSpeedAdjustRate_TeamRole(speedAdjust);

        // Set healing adjust rate (direct modification)
        buffParam->_healingAdjustRate_TeamRole = healingAdjust;

        // Set reload adjust rate
        buffParam->BP_SetReloadAdjustRate(reloadAdjust);

        // Send request to buff control component to notify server
        auto buffComponent = playerChar->_buffControlComponent;
        if (buffComponent)
        {
            buffComponent->RequestApplyTeamBuffs_ToServer();
        }

        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

void InGameHack_ValidateTransMissionLevel()
{
    try
    {
        SDK::int32 validatedLevel = SDK::UCh202ActionAttackBase::ValidateTransMissionLevel(5);
      
    }
    catch (const std::exception& e)
    {
        // Error occurred
    }
}

// ============================================
// GET ALL TEAM IDS
// ============================================

std::vector<unsigned char> InGameHack_GetAllTeamIds()
{
    std::vector<unsigned char> teamIds;
    
    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            return teamIds;  // Empty
        }

        // Iterate through all actors
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            // Check if actor is a character
            if (!actor || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(actor);
            
            // Get team ID
            try
            {
                unsigned char teamId = character->BP_GetTeamId();
                
                // Add to vector if not already present
                bool found = false;
                for (unsigned char id : teamIds)
                {
                    if (id == teamId)
                    {
                        found = true;
                        break;
                    }
                }
                
                if (!found && teamId != 255)  // 255 = invalid
                {
                    teamIds.push_back(teamId);
                }
            }
            catch (...)
            {
                continue;
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[GetAllTeamIds] Exception: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[GetAllTeamIds] Unknown exception");
    }

    return teamIds;
}

/**
 * Get all characters in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of ACharacterBattle pointers for that team
 */
std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharactersByTeamId(unsigned char teamId)
{
    std::vector<SDK::ACharacterBattle*> teamCharacters;
    try
    {
        if (!IsValidBattleMode())
            return teamCharacters;

        auto allCharacters = InGameHack_GetAllCharacterBattles();
        for (auto* character : allCharacters)
        {
            if (!character || character->IsDefaultObject())
                continue;

            if (!character->PlayerState)
                continue;

            unsigned char characterTeamId = GetCharacterTeamId(character);
            if (characterTeamId == teamId)
            {
                teamCharacters.push_back(character);
            }
        }
    }
    catch (...)
    {
        Logger::LogError("[GetTeamCharactersByTeamId] Exception occurred");
    }

    return teamCharacters;
}

/**
 * Get all player names in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of player names for that team
 */
std::vector<std::string> InGameHack_GetTeamNamesByTeamId(unsigned char teamId)
{
    std::vector<std::string> names;
    try
    {
        auto teamCharacters = InGameHack_GetTeamCharactersByTeamId(teamId);
        for (auto* character : teamCharacters)
        {
            if (!character)
            {
                names.push_back("Unknown");
                continue;
            }

            std::string playerName = "Unknown";
            if (character->PlayerState)
            {
                const char* namePtr = SDK_GetPlayerName(character->PlayerState);
                if (namePtr && namePtr[0] != '\0')
                    playerName = namePtr;
            }

            names.push_back(playerName);
        }
    }
    catch (...)
    {
        Logger::LogError("[GetTeamNamesByTeamId] Exception occurred");
    }

    return names;
}

/**
 * Get my current team ID
 * @return Team ID of the local player, or -1 if not found
 */
int InGameHack_GetMyTeamId()
{
    try
    {
        SDK::APlayerController* basePC = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePC || !basePC->PlayerState)
            return -1;

        SDK::AHerovsPlayerState* myPlayerState = static_cast<SDK::AHerovsPlayerState*>(basePC->PlayerState);
        if (!myPlayerState)
            return -1;

        return myPlayerState->BP_GetTeamId();
    }
    catch (...)
    {
        Logger::LogError("[GetMyTeamId] Exception occurred");
        return -1;
    }
}

// ============================================
// BULLET REDIRECTION FUNCTIONS
// ============================================

/**
 * Helper: Check if bullet is a melee attack (should NOT be redirected)
 */
static inline bool IsBulletMeleeAttack(const std::string& bulletName)
{
    // Check for melee/punch/slash patterns
    if (bulletName.find("Melee") != std::string::npos) return true;
    if (bulletName.find("Punch") != std::string::npos) return true;
    if (bulletName.find("Slash") != std::string::npos) return true;
    if (bulletName.find("Strike") != std::string::npos) return true;
    if (bulletName.find("Attack") != std::string::npos && bulletName.find("Skill") == std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Alpha (Unique1) skill
 */
static inline bool IsBulletAlphaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique1") != std::string::npos) return true;
    if (bulletName.find("BPB_CementShot") != std::string::npos) return true;
    if (bulletName.find("BPB_Cement_L") != std::string::npos) return true;
    if (bulletName.find("BPB_Cracks_L") != std::string::npos) return true;
    if (bulletName.find("BPB_FogShot_") != std::string::npos) return true;
    if (bulletName.find("BPB202_U1_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Beta (Unique2) skill
 */
static inline bool IsBulletBetaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique2") != std::string::npos) return true;
    if (bulletName.find("BPB_ThunderBackGroundHit_C") != std::string::npos) return true;
    if (bulletName.find("BPB_CracksRange_L") != std::string::npos) return true;
    if (bulletName.find("BPB_RiseCement_L") != std::string::npos) return true;
    if (bulletName.find("BPB_PortalA_") != std::string::npos) return true;
    if (bulletName.find("BPB_PortalShotA_") != std::string::npos) return true;
    if (bulletName.find("BPB202_U2_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Gamma (Unique3) skill
 */
static inline bool IsBulletGammaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique3") != std::string::npos) return true;
    if (bulletName.find("Cement3Shot") != std::string::npos) return true;
    if (bulletName.find("BPB202_U3_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Special skill
 */
static inline bool IsBulletSpecialSkill(const std::string& bulletName)
{
    if (bulletName.find("BPB_Special_C") != std::string::npos) return true;
    if (bulletName.find("BPB_UniqueSpecial_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet cannot be teleported (causes crash)
 * Some special balls like rebuild balls crash when teleported
 */
static inline bool IsBulletNonTeleportable(const std::string& bulletName)
{
    // Rebuild skill special ball - DO NOT teleport
    if (bulletName.find("Rebuild") != std::string::npos) return true;
    if (bulletName.find("BPB_Rebuild") != std::string::npos) return true;
    // Add more non-teleportable ball patterns here as needed
    return false;
}

/**
 * Redirect bullets to nearest enemy in front
 * Filters bullets by skill type (Alpha/Beta/Gamma/Special)
 * Uses K2_TeleportTo to redirect each bullet to enemy position
 * 
 * @param bIncludeAlpha - Include Alpha (Unique1) skills
 * @param bIncludeBeta - Include Beta (Unique2) skills
 * @param bIncludeGamma - Include Gamma (Unique3) skills
 * @param bIncludeSpecial - Include Special skills
 * @return true if bullets were redirected
 */
bool InGameHack_RedirectBulletsToNearestEnemy(bool bIncludeAlpha, bool bIncludeBeta, bool bIncludeGamma, bool bIncludeSpecial)
{
    try
    {
        // Validate we're in battle
        if (!IsValidBattleMode())
        {
            return false;
        }

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
            return false;

        // Get player character
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
            return false;

        // Get nearest forward target (closest enemy in front)
        SDK::AActor* nearestEnemyPtr = SDK_GetForwardESPTarget();
        if (!nearestEnemyPtr)
            return false;

        SDK::ACharacterBattle* nearestEnemy = static_cast<SDK::ACharacterBattle*>(nearestEnemyPtr);
        if (!nearestEnemy)
            return false;

        // Get nearest enemy position
        SDK::FVector targetPosition = nearestEnemy->K2_GetActorLocation();

        int redirectedCount = 0;
        int totalBulletsFound = 0;
        int bulletsFiltered = 0;

        // Iterate through all actors to find and redirect bullets
        for (int i = 0; i < world->PersistentLevel->Actors.Num(); i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            // Check if actor is a bullet
            if (!actor || actor->IsDefaultObject() || !actor->IsA(SDK::ABullet::StaticClass()))
                continue;

            SDK::ABullet* bullet = static_cast<SDK::ABullet*>(actor);
            if (!bullet)
                continue;

            totalBulletsFound++;

            // Check if bullet belongs to player character
            if (bullet->_ownerChr != playerCharacter)
                continue;

            // Get bullet class name
            std::string bulletClassName = "";
            try
            {
                if (bullet->Class)
                {
                    bulletClassName = bullet->Class->GetName();
                    if (bulletClassName.empty())
                        continue;
                }
                else
                {
                    continue;
                }
            }
            catch (...)
            {
                continue;  // Skip if we can't get class name
            }

            // CRITICAL: Skip melee attacks to prevent crash
            if (IsBulletMeleeAttack(bulletClassName))
                continue;

            // CRITICAL: Skip non-teleportable bullets (rebuild, etc) to prevent crash
            if (IsBulletNonTeleportable(bulletClassName))
                continue;

            // Check if bullet matches skill filter
            bool matchesFilter = false;
            if (bIncludeAlpha && IsBulletAlphaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeBeta && IsBulletBetaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeGamma && IsBulletGammaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeSpecial && IsBulletSpecialSkill(bulletClassName))
                matchesFilter = true;

            if (!matchesFilter)
                continue;  // Skip this bullet if it doesn't match filter

            bulletsFiltered++;

            // Redirect bullet to target position using K2_TeleportTo
            try
            {
                bullet->K2_TeleportTo(targetPosition, SDK::FRotator(0, 0, 0));
                redirectedCount++;
            }
            catch (...)
            {
                continue;  // Skip if teleport fails
            }
        }

        // Log results for debugging
        if (totalBulletsFound > 0 || redirectedCount > 0)
        {
            // Write debug log to C:\temp\bullet_redirect_debug.log
            try
            {
                std::ofstream logFile("C:\\temp\\bullet_redirect_debug.log", std::ios::app);
                if (logFile.is_open())
                {
                    logFile << "[BulletRedirect] Total Bullets: " << totalBulletsFound 
                           << " | Filtered: " << bulletsFiltered 
                           << " | Redirected: " << redirectedCount 
                           << " | Filters(A:" << bIncludeAlpha 
                           << " B:" << bIncludeBeta 
                           << " G:" << bIncludeGamma 
                           << " S:" << bIncludeSpecial << ")\n";
                    logFile.close();
                }
            }
            catch (...)
            {
            }
        }

        return redirectedCount > 0;
    }
    catch (const std::exception& e)
    {
        try
        {
            std::ofstream logFile("C:\\temp\\bullet_redirect_debug.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << "[BulletRedirect] Exception: " << e.what() << "\n";
                logFile.close();
            }
        }
        catch (...)
        {
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Test: Launch Ch101 RollSlot Unique Skill
 * Uses verified public API: BP_GetObject() + OnBroadcastEvent()
 * @return true if skill was launched successfully
 */
bool InGameHack_LaunchCh101RollSlotSkill()
{
    std::stringstream logBuffer;
    
    try
    {
        // Log start
        logBuffer << "[CH101_ROLLSLOT] ===== LAUNCH ATTEMPT START =====\n";
        
        // Step 1: Validate battle mode
        logBuffer << "[CH101_ROLLSLOT] Step 1: Validating battle mode...\n";
        if (!IsValidBattleMode())
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Not in valid battle mode\n";
            
            std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
            if (logFile.is_open()) { logFile << logBuffer.str(); logFile.close(); }
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Valid battle mode\n";

        // Step 2: Get player controller
        logBuffer << "[CH101_ROLLSLOT] Step 2: Getting player controller...\n";
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get player controller\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << logBuffer.str();
                logFile.close();
            }
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Player controller obtained\n";

        // Step 3: Get player character
        logBuffer << "[CH101_ROLLSLOT] Step 3: Getting player character...\n";
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get player character from Pawn\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << logBuffer.str();
                logFile.close();
            }
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Player character obtained at 0x" 
                 << std::hex << (uintptr_t)playerCharacter << std::dec << "\n";

        // Step 4: Get PlayerStateBattle
        logBuffer << "[CH101_ROLLSLOT] Step 4: Getting PlayerStateBattle...\n";
        SDK::APlayerStateBattle* playerState = playerCharacter->BP_GetPlayerStateBattle();
        if (!playerState)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get PlayerStateBattle\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << logBuffer.str();
                logFile.close();
            }
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: PlayerStateBattle obtained at 0x" 
                 << std::hex << (uintptr_t)playerState << std::dec << "\n";

        // Step 5: Get RollSlot Control Component
        logBuffer << "[CH101_ROLLSLOT] Step 5: Getting RollSlot Control Component...\n";
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl = 
            playerState->BP_GetCharacterRollSlotUniqueSkillControlComponent();
        if (!rollSlotCtrl)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get RollSlot Control Component\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << logBuffer.str();
                logFile.close();
            }
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: RollSlot Control Component obtained at 0x" 
                 << std::hex << (uintptr_t)rollSlotCtrl << std::dec << "\n";

        // Step 6: Try different approaches to activate RollSlot
        logBuffer << "[CH101_ROLLSLOT] Step 6: Attempting multiple activation methods...\n";
        
        // Offset of _registerRollSlotUniqueSkillObjectList is 0x00D0
        SDK::uint8* rollSlotCtrlPtr = (SDK::uint8*)rollSlotCtrl;
        SDK::uint8* tMapPtr = rollSlotCtrlPtr + 0x00D0;
        
        logBuffer << "[CH101_ROLLSLOT] TMap address: 0x" << std::hex << (uintptr_t)tMapPtr << std::dec << "\n";
        
        // Method 1: Try BP_BroadcastByCharacterId with SetParam first
        logBuffer << "[CH101_ROLLSLOT] Step 7a: Method 1 - SetParam + Broadcast\n";
        SDK::EVariationCharacterId ch101Var0 = SDK::EVariationCharacterId::Ch101_Variation0;
        float refParam = 1.0f;  // Try with 1.0 instead of 0
        SDK::int32 broadcastIndex = 0;
        
        // Set parameters first
        SDK::FRollSlotUniqueSkillArgumentParam skillParam;
        memset(&skillParam, 0, sizeof(skillParam));
        skillParam._broadcastIndex = 0;
        rollSlotCtrl->BP_SetParamByCharacterId(ch101Var0, skillParam);
        
        // Then broadcast
        rollSlotCtrl->BP_BroadcastByCharacterId(ch101Var0, refParam, broadcastIndex);
        logBuffer << "[CH101_ROLLSLOT] BP_BroadcastByCharacterId completed\n";
        
        // Method 2: Try BP_BroadcastByTriggerType with StartBattleWithCooldown
        logBuffer << "[CH101_ROLLSLOT] Step 7b: Method 2 - BroadcastByTriggerType\n";
        SDK::ERollSlotTriggerType triggerType = SDK::ERollSlotTriggerType::StartBattleWithCooldown;
        rollSlotCtrl->BP_BroadcastByTriggerType(triggerType, refParam, 0);
        logBuffer << "[CH101_ROLLSLOT] BP_BroadcastByTriggerType completed\n";
        
        // Method 3: Try with DealDamage trigger
        logBuffer << "[CH101_ROLLSLOT] Step 7c: Method 3 - DealDamage trigger\n";
        rollSlotCtrl->BP_BroadcastByTriggerType(SDK::ERollSlotTriggerType::DealDamage, refParam, 0);
        logBuffer << "[CH101_ROLLSLOT] DealDamage broadcast completed\n";
        logBuffer << "[CH101_ROLLSLOT] ===== SUCCESS =====\n";

        std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
        if (logFile.is_open()) { logFile << logBuffer.str(); logFile.close(); }
        return true;
    }
    catch (const std::exception& e)
    {
        logBuffer << "[CH101_ROLLSLOT] ERROR: Exception caught: " << e.what() << "\n";
        logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
        
        std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
        if (logFile.is_open())
        {
            logFile << logBuffer.str();
            logFile.close();
        }
        return false;
    }
    catch (...)
    {
        logBuffer << "[CH101_ROLLSLOT] ERROR: Unknown exception\n";
        logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
        
        std::ofstream logFile("C:\\temp\\rollslot_launch.log", std::ios::app);
        if (logFile.is_open())
        {
            logFile << logBuffer.str();
            logFile.close();
        }
        return false;
    }
}

// ============================================
// GET CURRENT SEASON PASS RANK
// ============================================

/**
 * Helper: Write timestamped log to rank file
 */
static void LogToSeasonPassRankFile(const std::string& message)
{
    try
    {
        std::ofstream logFile("C:/Temp/SeasonPassRank.log", std::ios::app);
        if (logFile.is_open())
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::stringstream timestamp;
            timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                     << "." << std::setfill('0') << std::setw(3) << ms.count();

            logFile << "[" << timestamp.str() << "] " << message << "\n";
            logFile.close();
        }
    }
    catch (...) { }
}

/**
 * Get the current season pass rank
 * All logs written to C:/Temp/SeasonPassRank.log
 * @return Current season pass rank (int32), or -1 if error
 */
int InGameHack_GetCurrentSeasonPassRank()
{
    try
    {
        // Direct access to BackendSubsystem singleton
        SDK::UBackendSubsystem* backendSubsystem = SDK::UBackendSubsystem::GetDefaultObj();
        
        // Try to get the rank directly
        if (backendSubsystem)
        {
            SDK::UDatabaseParams* dbParams = backendSubsystem->GetDatabaseParams();
            if (dbParams)
            {
                SDK::UDbpResult* dbpResult = dbParams->GetResultData();
                if (dbpResult)
                {
                    int32_t currentRank = dbpResult->GetCurrentSeasonPassRank();
                    
                    // Log the result to file
                    LogToSeasonPassRankFile("[SUCCESS] Current Season Pass Rank: " + std::to_string(currentRank));
                    return currentRank;
                }
            }
        }

        // If we couldn't get rank through normal methods, return -1
        LogToSeasonPassRankFile("[ERROR] Could not retrieve rank from BackendSubsystem");
        return -1;
    }
    catch (const std::exception& e)
    {
        LogToSeasonPassRankFile("[ERROR] C++ Exception: " + std::string(e.what()));
        return -1;
    }
    catch (...)
    {
        LogToSeasonPassRankFile("[ERROR] Unknown exception occurred");
        return -1;
    }
}

// ============================================
// TEST SEASON PASS EXP INCREASE
// ============================================

/**
 * Helper: Write timestamped log to file
 */
static void LogToSeasonPassFile(const std::string& message)
{
    try
    {
        std::ofstream logFile("C:/Temp/SeasonPassExp.log", std::ios::app);
        if (logFile.is_open())
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            std::stringstream timestamp;
            timestamp << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                     << "." << std::setfill('0') << std::setw(3) << ms.count();

            logFile << "[" << timestamp.str() << "] " << message << "\n";
            logFile.close();
        }
    }
    catch (...) { }
}

/**
 * Add real EXP to Season Pass (modify memory directly)
 * Modifies _prevSeasonExp and triggers rewards unlocking
 * Does NOT use BackendSubsystem functions
 * All logs go to C:/Temp/SeasonPassExp.log
 * SAFE: No function calls that could crash
 */
bool InGameHack_IncreaseSeasonPassExp(SDK::ESeasonType type, int32_t count)
{
    try
    {
        // Get widget instance
        SDK::USeasonPassLicensePurchaseWindow* purchaseWindow = nullptr;
        
        try
        {
            purchaseWindow = SDK::USeasonPassLicensePurchaseWindow::GetDefaultObj();
        }
        catch (...)
        {
            LogToSeasonPassFile("[ERROR] Exception getting widget instance");
            return false;
        }
        
        // Validate pointer is not null
        if (!purchaseWindow)
        {
            LogToSeasonPassFile("[ERROR] Widget GetDefaultObj() returned nullptr");
            return false;
        }

        // Additional validation: check pointer range (basic sanity check)
        uintptr_t addr = reinterpret_cast<uintptr_t>(purchaseWindow);
        if (addr < 0x10000 || addr > 0x7FFFFFFF0000)
        {
            LogToSeasonPassFile("[ERROR] Widget pointer invalid: 0x" + 
                              std::to_string(addr));
            return false;
        }

        // Access widget members safely - direct memory modification only
        try
        {
            // Read current EXP value (offset 0x03AC)
            int32_t currentExp = purchaseWindow->_prevSeasonExp;
            int32_t addedExp = count * 100;
            int32_t newExp = currentExp + addedExp;

            // Write new EXP value directly (SAFE: no function calls)
            purchaseWindow->_prevSeasonExp = newExp;
            
            // Update display value
            purchaseWindow->_purchaseExp = addedExp;

            // Log modification to file
            LogToSeasonPassFile("[SUCCESS] EXP modified: " + std::to_string(currentExp) + 
                              " -> " + std::to_string(newExp) + " (+= " + std::to_string(addedExp) + ")");
            
            LogToSeasonPassFile("[INFO] Season Type: " + std::to_string(static_cast<int32_t>(type)));
            
            return true;
        }
        catch (const std::exception& e)
        {
            LogToSeasonPassFile("[ERROR] Exception accessing widget members: " + 
                              std::string(e.what()));
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LogToSeasonPassFile("[ERROR] C++ Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        LogToSeasonPassFile("[ERROR] Unknown exception occurred");
        return false;
    }
}

// ============================================================================
// CHARACTER CONDITION AUTO-EXECUTION
// ============================================================================

/**
 * Process character condition auto-execution on battle mode entry
 * Detects transitions from invalid to valid battle mode and executes active toggles once
 */
void InGameHack_ProcessCharacterConditionAutoExecution(
    bool enableDekuMode,
    bool enableUnbreakable,
    bool enableCompressionRegen,
    bool enableMirioMode,
    bool enableTokoyamiMode)
{
    // Static tracking of battle session state
    static bool wasInValidBattle = false;
    static bool hasExecutedInCurrentSession = false;  // Track if we already executed in this battle session
    
    try
    {
        // Check if currently in valid battle mode
        bool isCurrentlyInValidBattle = IsValidBattleMode();
        
        // Detect transition: from invalid to valid battle mode
        bool enteredValidBattle = (!wasInValidBattle && isCurrentlyInValidBattle);
        
        // Detect exit from battle: from valid to invalid
        bool exitedBattle = (wasInValidBattle && !isCurrentlyInValidBattle);
        
        // Update state for next frame
        wasInValidBattle = isCurrentlyInValidBattle;
        
        // If we exited battle, reset the execution flag for the next battle session
        if (exitedBattle)
        {
            hasExecutedInCurrentSession = false;
            Logger::LogInfo("[AUTO-EXECUTE] Exited battle mode - reset execution flag");
            return;
        }
        
        // If not in valid battle mode, don't do anything
        if (!isCurrentlyInValidBattle)
        {
            return;
        }
        
        // If we already executed in this session, don't execute again
        if (hasExecutedInCurrentSession)
        {
            return;
        }
        
        // If we just entered valid battle mode, execute active toggles ONCE
        if (enteredValidBattle)
        {
            Logger::LogInfo("[AUTO-EXECUTE] Entered valid battle mode - executing active conditions ONCE");
            
            // Mark that we've executed so we won't do it again this session
            hasExecutedInCurrentSession = true;
            
            // Execute DEKU MODE (CH202 Trans Mission - enum 85)
            if (enableDekuMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing DEKU MODE");
                InGameHack_CH202TransMission();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableDekuMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] DEKU MODE disabled after execution");
            }
            
            // Execute UNBREAKABLE
            if (enableUnbreakable)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing UNBREAKABLE");
                InGameHack_Unbreakable();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableUnbreakable = false;
                Logger::LogInfo("[AUTO-EXECUTE] UNBREAKABLE disabled after execution");
            }
            
            // Execute MR COMPRESSE MODE (Compression Regeneration)
            if (enableCompressionRegen)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing MR COMPRESSE MODE");
                InGameHack_CompressionRegeneration();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableCompressionRegen = false;
                Logger::LogInfo("[AUTO-EXECUTE] MR COMPRESSE MODE disabled after execution");
            }
            
            // Execute MIRIO MODE (CH024 Transparent)
            if (enableMirioMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing MIRIO MODE");
                InGameHack_CH024Transparent();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableMirioMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] MIRIO MODE disabled after execution");
            }
            
            // Execute TOKOYAMI DARK MODE (CH011 Abyss Dark Body)
            if (enableTokoyamiMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing TOKOYAMI DARK MODE");
                InGameHack_CH011AbyssDarkBody();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableTokoyamiMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] TOKOYAMI DARK MODE disabled after execution");
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[AUTO-EXECUTE] Exception: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[AUTO-EXECUTE] Unknown exception");
    }
}

// ============================================================================
// PLAYER NAME CHANGE
// ============================================================================

/**
 * Change player name in the game
 * Calls AGameModeBase::ChangeName on server
 * @param newName - The new player name (max 255 characters)
 * @return true if successful, false otherwise
 */
bool InGameHack_ChangePlayerName(const char* newName)
{
    try
    {
        // Validate input
        if (!newName || newName[0] == '\0' || strlen(newName) > 255)
        {
            Logger::LogError("[ChangeName] Invalid player name");
            return false;
        }

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            Logger::LogError("[ChangeName] Could not get player controller");
            return false;
        }

        // Validate controller pointer
        if (IsBadReadPtr(playerController, sizeof(void*)))
        {
            Logger::LogError("[ChangeName] PlayerController pointer is invalid");
            return false;
        }

        // Convert C string (const char*) to wide string (wchar_t*)
        int requiredSize = MultiByteToWideChar(CP_UTF8, 0, newName, -1, NULL, 0);
        if (requiredSize <= 0)
        {
            Logger::LogError("[ChangeName] Failed to convert player name to wide string");
            return false;
        }

        wchar_t* wideNameBuffer = new wchar_t[requiredSize];
        MultiByteToWideChar(CP_UTF8, 0, newName, -1, wideNameBuffer, requiredSize);

        // Create FString from wide string
        SDK::FString newNameFString(wideNameBuffer);

        // Call ServerChangeName RPC on player controller
        playerController->ServerChangeName(newNameFString);

        // Clean up
        delete[] wideNameBuffer;

        Logger::LogInfo("[ChangeName] Successfully changed player name to: " + std::string(newName));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeName] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ChangeName] Unknown exception");
        return false;
    }
}

bool InGameHack_BuyLicenseExp(int32_t count)
{
    try
    {
        // Validate input
        if (count <= 0 || count > 10000)
        {
            Logger::LogError("[BuyLicenseExp] Invalid count: " + std::to_string(count) + " (must be 1-10000)");
            return false;
        }

        // Get BackendSubsystem singleton via direct pointer access
        SDK::UBackendSubsystem* backendSubsystem = SDK::UBackendSubsystem::GetDefaultObj();
        if (!backendSubsystem)
        {
            Logger::LogError("[BuyLicenseExp] Could not get BackendSubsystem");
            return false;
        }

        if (IsBadReadPtr(backendSubsystem, sizeof(void*)))
        {
            Logger::LogError("[BuyLicenseExp] BackendSubsystem pointer is invalid");
            return false;
        }

        try
        {
            // Call BuyLicenseExp with conversion to SDK int32 type
            SDK::int32 requestId = backendSubsystem->BuyLicenseExp((SDK::int32)count);

            Logger::LogInfo("[BuyLicenseExp] Purchase request sent: count=" + std::to_string(count) + ", requestId=" + std::to_string(requestId));
            return true;
        }
        catch (...)
        {
            Logger::LogError("[BuyLicenseExp] Exception calling BuyLicenseExp method");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[BuyLicenseExp] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[BuyLicenseExp] Unknown exception");
        return false;
    }
}

bool InGameHack_GenerateProjectileInFront()
{
    try
    {
        // Open debug log file
        FILE* debugLog = nullptr;
        fopen_s(&debugLog, "C:\\temp\\projectile_debug.log", "a");
        if (debugLog)
        {
            fprintf(debugLog, "[%lld] === GenerateProjectile function called ===\n", (long long)time(nullptr));
            fflush(debugLog);
        }

        // Get player controller
        SDK::APlayerController* playerController = SDK_GetPlayerController();
        if (debugLog) fprintf(debugLog, "[%lld] playerController = %p\n", (long long)time(nullptr), playerController);
        if (!playerController)
        {
            Logger::LogError("[GenerateProjectile] No player controller found");
            if (debugLog) {
                fprintf(debugLog, "[%lld] ERROR: No player controller found\n", (long long)time(nullptr));
                fclose(debugLog);
            }
            return false;
        }

        // Get player character
        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (debugLog) fprintf(debugLog, "[%lld] playerChar = %p\n", (long long)time(nullptr), playerChar);
        if (!playerChar)
        {
            Logger::LogError("[GenerateProjectile] No player character found");
            if (debugLog) {
                fprintf(debugLog, "[%lld] ERROR: No player character found\n", (long long)time(nullptr));
                fclose(debugLog);
            }
            return false;
        }

        // Get the projectile replicator component from the character
        SDK::UProjectileReplicateBattleComponent* projectileComp = playerChar->_projectileReplicator;
        if (debugLog) fprintf(debugLog, "[%lld] projectileComp = %p\n", (long long)time(nullptr), projectileComp);
        
        if (!IsValidPointer(projectileComp))
        {
            Logger::LogError("[GenerateProjectile] No projectile replicator component found on player character");
            if (debugLog) {
                fprintf(debugLog, "[%lld] ERROR: No projectile replicator component found\n", (long long)time(nullptr));
                fclose(debugLog);
            }
            return false;
        }

        // Get player location
        SDK::FVector playerLocation = playerChar->K2_GetActorLocation();
        if (debugLog) fprintf(debugLog, "[%lld] playerLocation = (%.2f, %.2f, %.2f)\n", (long long)time(nullptr), playerLocation.X, playerLocation.Y, playerLocation.Z);

        // Build FProjectileGenerateRep structure with hardcoded Ch010 PUSH_SPECIAL parameters
        SDK::FProjectileGenerateRep rep;
        rep.Parent = playerChar;
        rep.ParentComponent = nullptr;
        rep.generatorDataID = 1;        // Try ID 1 instead of 0
        rep.generatorDataIndexID = 0;
        rep.targetLocate.X = playerLocation.X;
        rep.targetLocate.Y = playerLocation.Y;
        rep.targetLocate.Z = playerLocation.Z;
        rep.initLocate.X = playerLocation.X;
        rep.initLocate.Y = playerLocation.Y;
        rep.initLocate.Z = playerLocation.Z;
        rep.initScale.X = 100;
        rep.initScale.Y = 100;
        rep.initScale.Z = 100;
        rep.initQuat.Pitch = 0;
        rep.initQuat.Yaw = 0;
        rep.initQuat.Roll = 0;
        rep.genID = 1;                  // Try genID 1 instead of 0
        rep.attachTime = 0.0f;
        rep.socketIdx = -1;             // -1 for no socket
        rep.serialID = 0;
        rep.charaId = 10;               // Ch010
        rep.AttachType = 0;
        rep.Level = 0;                  // Reset to 0
        rep.variationNo = 0;            // Default variation
        rep.commandId = 19;             // PUSH_SPECIAL
        rep.attackId = 6;               // SPECIAL attack type
        rep.conditionID = 0;            // Normal condition
        rep.attackSerial = 0;
        rep.overrideDamageAdjustAdd = 0;
        rep.overrideJsonIDX = -1;       // -1 for default JSON

        if (debugLog) {
            fprintf(debugLog, "[%lld] FProjectileGenerateRep initialized: charaId=%d, commandId=%d, attackId=%d\n",
                (long long)time(nullptr), (int)rep.charaId, (int)rep.commandId, (int)rep.attackId);
        }

        // Call the RPC to send projectile generation to server
        if (debugLog) fprintf(debugLog, "[%lld] Calling CreateGenerate_RPC...\n", (long long)time(nullptr));
        projectileComp->CreateGenerate_RPC(rep);
        if (debugLog) fprintf(debugLog, "[%lld] CreateGenerate_RPC returned\n", (long long)time(nullptr));
        
        Logger::LogInfo("[GenerateProjectile] Projectile generated successfully at Ch010 PUSH_SPECIAL");
        if (debugLog) {
            fprintf(debugLog, "[%lld] Projectile generation SUCCESS\n", (long long)time(nullptr));
            fclose(debugLog);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[GenerateProjectile] Exception: " + std::string(e.what()));
        FILE* debugLog = nullptr;
        fopen_s(&debugLog, "C:\\temp\\projectile_debug.log", "a");
        if (debugLog) {
            fprintf(debugLog, "[%lld] Exception: %s\n", (long long)time(nullptr), e.what());
            fclose(debugLog);
        }
        return false;
    }
    catch (...)
    {
        Logger::LogError("[GenerateProjectile] Unknown exception");
        FILE* debugLog = nullptr;
        fopen_s(&debugLog, "C:\\temp\\projectile_debug.log", "a");
        if (debugLog) {
            fprintf(debugLog, "[%lld] Unknown exception caught\n", (long long)time(nullptr));
            fclose(debugLog);
        }
        return false;
    }
}
