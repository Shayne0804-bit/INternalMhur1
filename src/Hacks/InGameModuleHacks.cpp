#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "InGameModuleHacks.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
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
#define OFFSET_PLAYERSTATE 0x240          // AActor::PlayerState
#define OFFSET_PLAYERSTATE_IFDYING 1400   // Dying flag at +1400

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
    
    for (auto id : allIds)
    {
        // Get character name and variation from enum value
        const char* charName = nullptr;
        int varIdx = 0;
        
        // Map enum to character name and variation index
        switch (id)
        {
            case SDK::EVariationCharacterId::Ch001_Variation0: charName = "Ch001"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch001_Variation1: charName = "Ch001"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation0: charName = "Ch002"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch002_Variation1: charName = "Ch002"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation2: charName = "Ch002"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch003_Variation0: charName = "Ch003"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch003_Variation1: charName = "Ch003"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch004_Variation0: charName = "Ch004"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch004_Variation1: charName = "Ch004"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch005_Variation0: charName = "Ch005"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch005_Variation1: charName = "Ch005"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch006_Variation0: charName = "Ch006"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch006_Variation1: charName = "Ch006"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch007_Variation0: charName = "Ch007"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch007_Variation1: charName = "Ch007"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch008_Variation0: charName = "Ch008"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch008_Variation1: charName = "Ch008"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch010_Variation0: charName = "Ch010"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch010_Variation1: charName = "Ch010"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch011_Variation0: charName = "Ch011"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch011_Variation1: charName = "Ch011"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch012_Variation0: charName = "Ch012"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch012_Variation1: charName = "Ch012"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch013_Variation0: charName = "Ch013"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch013_Variation1: charName = "Ch013"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation0: charName = "Ch015"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch015_Variation1: charName = "Ch015"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation2: charName = "Ch015"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch016_Variation0: charName = "Ch016"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch016_Variation1: charName = "Ch016"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch017_Variation0: charName = "Ch017"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch017_Variation1: charName = "Ch017"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch018_Variation0: charName = "Ch018"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch018_Variation1: charName = "Ch018"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch023_Variation0: charName = "Ch023"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch023_Variation1: charName = "Ch023"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch024_Variation0: charName = "Ch024"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch024_Variation1: charName = "Ch024"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch025_Variation0: charName = "Ch025"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch025_Variation1: charName = "Ch025"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch026_Variation0: charName = "Ch026"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch026_Variation1: charName = "Ch026"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch034_Variation0: charName = "Ch034"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch034_Variation1: charName = "Ch034"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch037_Variation0: charName = "Ch037"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch037_Variation1: charName = "Ch037"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch038_Variation0: charName = "Ch038"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch038_Variation1: charName = "Ch038"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch043_Variation0: charName = "Ch043"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch043_Variation1: charName = "Ch043"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch046_Variation0: charName = "Ch046"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch046_Variation1: charName = "Ch046"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch100_Variation0: charName = "Ch100"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch100_Variation1: charName = "Ch100"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch101_Variation0: charName = "Ch101"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch101_Variation1: charName = "Ch101"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch102_Variation0: charName = "Ch102"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch102_Variation1: charName = "Ch102"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch103_Variation0: charName = "Ch103"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch103_Variation1: charName = "Ch103"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch104_Variation0: charName = "Ch104"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch104_Variation1: charName = "Ch104"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch105_Variation0: charName = "Ch105"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch105_Variation1: charName = "Ch105"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch109_Variation0: charName = "Ch109"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch109_Variation1: charName = "Ch109"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch114_Variation0: charName = "Ch114"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch114_Variation1: charName = "Ch114"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch115_Variation0: charName = "Ch115"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch115_Variation1: charName = "Ch115"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch200_Variation0: charName = "Ch200"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch200_Variation1: charName = "Ch200"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch201_Variation0: charName = "Ch201"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch201_Variation1: charName = "Ch201"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch202_Variation0: charName = "Ch202"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch202_Variation1: charName = "Ch202"; varIdx = 1; break;
            default: charName = "Unknown"; break;
        }
        
        if (charName)
        {
            names.push_back(std::string(charName) + "_Variation" + std::to_string(varIdx));
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
        SDK::ECharacterId::Ch103, SDK::ECharacterId::Ch104, SDK::ECharacterId::Ch105, SDK::ECharacterId::Ch109,
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
    if (!playerController || !playerCharacter || !targetCharacter)
    {
        return false;
    }

    try
    {
        // Get the player state from player controller
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            return false;
        }
        
        // Cast to APlayerStateBattle to access TransformControlComponent
        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UTransformControlComponent from player state
        SDK::UTransformControlComponent* transformComponent = playerStateBattle->_transformControlComponent;
        if (!transformComponent)
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
        
        // Get possessed pawn (player character)
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
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
    if (!playerController || !playerCharacter || !targetCharacter)
    {
        return false;
    }

    try
    {
        // Get the player state from player controller
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            return false;
        }
        
        // Cast to APlayerStateBattle to access DuplicateControlComponent
        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UDuplicateControlComponent from player state (offset 0x0C68)
        SDK::UDuplicateControlComponent* duplicateComponent = playerStateBattle->_duplicateControlComponent;
        if (!duplicateComponent)
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
        if (!playerController)
        {
            return false;
        }
        
        // Get player state
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            return false;
        }
        
        // Cast to APlayerStateBattle
        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!buffParam)
        {
            return false;
        }
        
        // Set the reload adjust rate
        buffParam->BP_SetAttackAdjustRate(rate);
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
        if (!playerController)
        {
            return false;
        }
        
        // Get player state
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            return false;
        }
        
        // Cast to APlayerStateBattle
        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!buffParam)
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
        if (!playerController)
        {
            return false;
        }
        
        // Get player state
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            return false;
        }
        
        // Cast to APlayerStateBattle
        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = playerStateBattle->_buffParam;
        if (!buffParam)
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
        if (!baseController)
        {
            Logger::LogError("[TRAINING] Could not get PlayerController");
            return false;
        }
        
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!playerController)
        {
            Logger::LogError("[TRAINING] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get current character pawn
        SDK::APawn* currentPawn = playerController->Pawn;
        SDK::ACharacterBattle* currentCharacter = static_cast<SDK::ACharacterBattle*>(currentPawn);

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
        Logger::LogError("[TRAINING] Exception caught");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[TRAINING] Unknown exception");
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
int InGameHack_ApplyToAllControllers(SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        return 0;
    }

    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world || !world->PersistentLevel)
        {
            return 0;
        }

        // Get LOCAL player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!battlePC || !battlePC->Pawn)
        {
            return 0;
        }

        // Create character data once with ALL parameters
        SDK::FInGameBattleCharacterData characterData = {};
        
        // DECODE EVariationCharacterId into (characterId, variationId)
        auto [characterId, variationId] = GetCharacterAndVariationFromVariationCharacterId(variationCharacterId);
        
        // Use decoded ECharacterId and variation index in struct fields
        characterData._characterId = characterId;  // Decoded ECharacterId
        characterData._variationId = variationId;  // Variation index (0, 1, 2, etc)
        characterData._skillVariationCode = variationId;  // Use full enum value for skill code
        characterData._technique1Level = unique1;
        characterData._technique2Level = unique2;
        characterData._technique3Level = unique3;
        characterData._costumeCode = costumeCode;
        characterData._costumeAuraType = costumeAuraType;

        int appliedCount = 0;
        int totalActors = world->PersistentLevel->Actors.Num();

        // LOOP OVER ALL ACTORS - INCLUDING local player
        // LOCAL CONTROLLER calls ChangeCharacter_OnServer for EACH character (including self)
        for (int i = 0; i < totalActors; i++)
        {
            SDK::AActor* actor = world->PersistentLevel->Actors[i];
            
            // Robust type check using IsA()
            if (!actor || actor->IsDefaultObject())
                continue;
                
            if (!actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);

            // Call LOCAL controller's ChangeCharacter_OnServer for THIS character
            // Includes local player and all enemies
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
            return 0;
        }

        return appliedCount;
    }
    catch (const std::exception& e)
    {
        return 0;
    }
    catch (...)
    {
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
bool InGameHack_ApplyToTeam(unsigned char teamId, SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
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
        if (!battlePC || !battlePC->Pawn)
        {
            Logger::LogError("[ApplyToTeam] Invalid battle PC or no pawn");
            return false;
        }

        // Create character data once with ALL parameters
        SDK::FInGameBattleCharacterData characterData = {};
        
        // DECODE EVariationCharacterId into (characterId, variationId)
        auto [characterId, variationId] = GetCharacterAndVariationFromVariationCharacterId(variationCharacterId);
        
        // Use decoded ECharacterId and variation index in struct fields
        characterData._characterId = characterId;
        characterData._variationId = variationId;
        characterData._skillVariationCode = variationId;
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
            
            // Robust type check using IsA()
            if (!actor || actor->IsDefaultObject() || !actor->IsA(SDK::ACharacterBattle::StaticClass()))
                continue;

            SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);

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
        if (!battlePC || !battlePC->Pawn)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get valid battle controller or pawn");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = static_cast<SDK::ACharacterBattle*>(battlePC->Pawn);

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

            enemies.push_back(targetCharacter);
        }

        if (enemies.empty())
        {
            Logger::LogWarning("[CopySkillsFromNearestEnemy] No enemies found to copy skills from");
            return 0;
        }

        // Pick a random enemy
        int randomIndex = rand() % enemies.size();
        SDK::ACharacterBattle* randomEnemy = enemies[randomIndex];

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
        // Get local player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!battlePC || !battlePC->Pawn)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get valid battle controller or pawn");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = static_cast<SDK::ACharacterBattle*>(battlePC->Pawn);
        
        // Validate master character
        if (!masterCharacter || masterCharacter->IsDefaultObject())
        {
            Logger::LogError("[CopySkillsFromCharacter] Invalid master character");
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
        if (localCharacter->_skillManagementComponent)
        {
            skillMgmt = localCharacter->_skillManagementComponent;
        }
        
        if (!skillMgmt)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get SkillManagementComponent from local player");
            return 0;
        }

        // Activate copy mode first, then set the character to copy from
        try
        {
            // Start copy mode with a 30 second duration (using None type)
            skillMgmt->BP_StartCopyMode(300.0f, (SDK::ECopyModeCharacterType)1);
            Logger::LogInfo("[CopySkillsFromCharacter] Started copy mode");

            // Now set the master character to copy from
            skillMgmt->BP_SetCopyCharacter(masterCharacter, bSetCopySkill, bUseOwnerCharacterLevel);
            Logger::LogInfo("[CopySkillsFromCharacter] Successfully copied skills from enemy character");
            return 1;
        }
        catch (...)
        {
            Logger::LogError("[CopySkillsFromCharacter] Failed to execute copy sequence");
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
        if (!basePlayerController || !basePlayerController->PlayerState)
        {
            Logger::LogError("[ChangeMyTeam] Could not get player controller or player state");
            return 0;
        }

        // Cast to HerovsPlayerState to access team functions
        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(basePlayerController->PlayerState);
        if (!playerState)
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
        if (!basePlayerController)
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!battlePC || !battlePC->Pawn)
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get valid battle controller or pawn");
            return 0;
        }

        // Get player state
        SDK::ACharacterBattle* localCharacter = static_cast<SDK::ACharacterBattle*>(battlePC->Pawn);
        if (!localCharacter || !localCharacter->PlayerState)
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get player state");
            return 0;
        }

        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(localCharacter->PlayerState);
        if (!playerState)
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
        if (!playerController || !playerController->PlayerState)
            return false;

        // Get player character
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
            return false;

        // Recover player state
        SDK::APlayerStateBattle* playerState = static_cast<SDK::APlayerStateBattle*>(playerController->PlayerState);
        if (!playerState)
            return false;

        // Check if player is dying FIRST
        if (!playerState->BP_IsDying())
            return false;  // Not dying, no need to recover

        playerCharacter->RecoverDyingAlly_ToServer(playerCharacter, true);
        return true;

        return false;
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
        if (!playerControllerPtr)
        {
            return false;
        }

        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerControllerPtr->PlayerState);
        if (!playerStateBattle)
        {
            return false;
        }

        // Hardcoded parameters - all features enabled
        float fixTime = 10.0f;          // Fixed duration
        float maxTime = 120.0f;         // Maximum duration
        bool enableEffect = true;       // Show effects
        bool projectileThrough = true;  // Projectiles pass through
        bool slipDamageThrough = true;  // Slip damage passes through
        bool activedTransparent = true; // Make player transparent

        // Create empty attack ID array (constructor creates empty array)
        SDK::TArray<SDK::EAttackId> ignoreClearAction;

        // Call SetInvincible_Server
        playerStateBattle->SetInvincible_Server(
            fixTime,
            maxTime,
            enableEffect,
            projectileThrough,
            slipDamageThrough,
            activedTransparent,
            ignoreClearAction
        );

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
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for RebuildMyself");
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
            false                            // bTimeOverwrite
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
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CH202TransMission");
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
            false                            // bTimeOverwrite
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
        if (!baseController)
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for Unbreakable");
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
            false                            // bTimeOverwrite
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
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)62,  // COMPRESSION_REGENERATION = 62
            0,                               // Level
            50.0f,                           // span
            0.0f,                            // value
            0.001f,                          // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false                            // bTimeOverwrite
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
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)90,  // CH024_TRANSPARENT = 65
            0,                               // Level
            50.0f,                           // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false                            // bTimeOverwrite
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
            50,                            // value
            0.1f,                            // interval
            5,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false                            // bTimeOverwrite                           // bTimeOverwrite
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

        // Call SetCondition_ToServer with ability ID, level 1-100, span 50 seconds
        conditionComponent->BP_SetCondition(
            (SDK::ECharacterConditionId)abilityId,  // Ability ID (43-47)
            level,                                   // Level (1-100)
            500.0f,                                   // span: 50 seconds
            1000.0f,                                   // value
            0.1f,                                   // interval
            level,                                       // subLevel
            nullptr,                                 // instigatedPlayer
            0                               // damageActionSerialNo
        );

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
    return SetAbility(43, level);  // ABILITY_ATTACK = 43
}

bool InGameHack_AbilityDurable(int level)
{
    return SetAbility(44, level);  // ABILITY_DURABLE = 44
}

bool InGameHack_AbilityMovespeed(int level)
{
    return SetAbility(45, level);  // ABILITY_MOVESPEED = 45
}

bool InGameHack_AbilityHeal(int level)
{
    return SetAbility(46, level);  // ABILITY_HEAL = 46
}

bool InGameHack_AbilityTechnique(int level)
{
    return SetAbility(47, level);  // ABILITY_TECHNIQUE = 47
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

bool InGameHack_UpgradeSupply(int supplyIndex, int level)
{
    try
    {
        if (level < 1 || level > 99)
        {
            
            return false;
        }
        
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

        // Create manipulation data for upgrade
        SDK::FNetSupplyHolderData manipData;
        manipData._manipulation = (SDK::ESupplyManipulationType)0; // DEFAULT
        manipData._bEnable = true;
        manipData._index = 0;
        
        // Determine supply type based on index
        // supplyIndex 6 = ABILITY, 7 = SHOULDER, or any inventory index
        if (supplyIndex == 6) // ABILITY
        {
            manipData._type = SDK::ESupplyHolderType::ABILITYSLOT; // Type 2
        
        }
        else if (supplyIndex == 7) // SHOULDER
        {
            manipData._type = SDK::ESupplyHolderType::ABILITYSLOT; // Type 2
        }   
        else // INVENTORY
        {
            manipData._type = SDK::ESupplyHolderType::INVENTORY; // Type 1
            manipData._index = supplyIndex;
        
        }
        
        // Add level to level list
        manipData._levelList.Add(level);
        
        // Create array with manipulation data
        SDK::TArray<SDK::FNetSupplyHolderData> manipList;
        manipList.Add(manipData);
        
        // Send upgrade command
        supplyHolderComp->OnManipulation_ToServer(manipList);
        
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
        return false;
    }
    catch (...)
    {
        return false;
    }
}
