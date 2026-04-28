#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "InGameModuleHacks.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../Utils/Logger.h"
#include "../Menu/ImGuiMenu.h"
#include <set>
#include <cctype>
#include <string>

// ============================================
// EXTERNAL DECLARATIONS FROM BASIC.CPP
// ============================================

// Forward declarations from Basic.cpp
extern "C" SDK::APlayerController* SDK_GetPlayerController();
extern "C" SDK::AActor* SDK_GetRandomESPTarget();  // Returns random ACharacterBattle as AActor*
extern "C" SDK::AActor* SDK_GetForwardESPTarget();  // Returns closest ACharacterBattle in front as AActor*
extern "C" int SDK_ApplyCharacterToAllPlayers(const SDK::FInGameBattleCharacterData& characterData);  // Apply character to all players

// CachedActor struct definition (mirrors Basic.cpp)
struct CachedActor {
	SDK::AActor* ActorPtr;           // Actor pointer
	std::string ClassName;           // "CharacterOutGame" or "CharacterBattle" or "NPCCitizen"
	SDK::FVector Position;           // Actor world position
	bool IsMySelf;                   // Player's own character
	int CharacterID;                 // Character ID (CharacterOutGame only)
	int CostumeCode;                 // Costume code (CharacterOutGame only)
	
	// Combat properties
	float Health;                    // Current health
	float MaxHealth;                 // Maximum health
	float GuardPoint;                // Current guard points
	float MaxGuardPoint;             // Maximum guard points
	int TeamId;                      // Team identifier (0-7)
	bool IsBot;                      // Is this a bot (AI)?
	bool IsAlly;                     // Is in same team as local player
	uint8 Platform;                  // Platform enum (PlayStation/Xbox/Windows/Switch)
	uint8 CitizenType;               // NPC citizen type (for NPCCitizen only)
	
	// Skeleton and validation
	bool HasValidSkeleton = false;   // Is skeleton valid/extracted?
};

// Access ESP cached actors list from Basic.cpp
extern std::vector<CachedActor> g_ActorsForRendering;  // Vector of cached actors for rendering (ESP list)

// ============================================
// UTILITY FUNCTIONS
// ============================================

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

// ============================================
// TRAINING HACKS - Character Changing
// ============================================

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
        buffParam->BP_SetReloadAdjustRate(rate);
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
 * Start the training match by calling Decide() on UTrainingMenuWidget
 */
bool InGameHack_DecideTraining()
{
    try
    {
        SDK::UTrainingMenuWidget* trainingWidget = SDK::UTrainingMenuWidget::GetDefaultObj();
        if (!trainingWidget)
        {
            Logger::LogError("[TRAINING] Widget is nullptr!");
            return false;
        }

        trainingWidget->Decide();
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

/**
 * Apply training player configuration (player only, no AI)
 */
bool InGameHack_ApplyPlayerConfiguration(int characterId, int unique1, int unique2, int unique3, int skillCode, int costumeCode, int costumeAuraType)
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
        characterData._variationId = (SDK::int32)skillCode;
        characterData._skillVariationCode = skillCode;
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
 * Apply player configuration to ALL PlayerStateBattle instances in the match
 * Calls SDK_ApplyCharacterToAllPlayers from Basic.cpp to apply configuration
 */
bool InGameHack_ApplyToAllControllers(int characterId, int unique1, int unique2, int unique3, int skillCode, int costumeCode, int costumeAuraType)
{
    try
    {
        // Create character data structure
        SDK::FInGameBattleCharacterData characterData = {};
        characterData._characterId = (SDK::ECharacterId)characterId;
        characterData._variationId = (SDK::int32)skillCode;
        characterData._skillVariationCode = skillCode;
        characterData._technique1Level = unique1;
        characterData._technique2Level = unique2;
        characterData._technique3Level = unique3;
        characterData._costumeCode = costumeCode;
        characterData._costumeAuraType = costumeAuraType;

        // Call wrapper function from Basic.cpp
        int appliedCount = SDK_ApplyCharacterToAllPlayers(characterData);
        
        return appliedCount > 0;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CHARACTER] Exception in ApplyToAllControllers");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[CHARACTER] Unknown exception in ApplyToAllControllers");
        return false;
    }
}

// ============================================
// INVINCIBILITY & RECOVERY IMPLEMENTATIONS
// ============================================

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
            (SDK::ECharacterConditionId)24,  // REBUILD_MYSELF = 24
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
            10,                               // Level
            0.0f,                            // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
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
 * Recover player and team allies from dying state
 * Calls BP_RecoverDying on player + all team members with dying flag
 */
bool InGameHack_RecoverDyingTeam()
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            Logger::LogError("[RECOVER_TEAM] Failed to get player controller");
            return false;
        }
        
        // Get possessed pawn (player character)
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[RECOVER_TEAM] Failed to get player character");
            return false;
        }

        // Get player state for recovery
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            Logger::LogError("[RECOVER_TEAM] Failed to get player state");
            return false;
        }

        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            Logger::LogError("[RECOVER_TEAM] Failed to cast to APlayerStateBattle");
            return false;
        }

        // Recover the player if dying
        SDK::APlayerState* playerStatePtr = playerController->PlayerState;
        if (playerStatePtr)
        {
            SDK::APlayerStateBattle* playerStateDying = static_cast<SDK::APlayerStateBattle*>(playerStatePtr);
            if (playerStateDying && playerStateDying->BP_IsDying())
            {
                playerStateBattle->BP_RecoverDying(playerCharacter);
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[RECOVER_TEAM] Exception caught");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[RECOVER_TEAM] Unknown exception");
        return false;
    }
}

/**
 * Recover all dying characters visible in ESP
 * Calls BP_RecoverDying on all ESP targets with dying flag
 */
bool InGameHack_RecoverDyingAllESP()
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            Logger::LogError("[RECOVER_ESP] Failed to get player controller");
            return false;
        }
        
        // Get possessed pawn (player character)
        SDK::ACharacterBattle* playerCharacter = (SDK::ACharacterBattle*)(playerController->Pawn);
        if (!playerCharacter)
        {
            Logger::LogError("[RECOVER_ESP] Failed to get player character");
            return false;
        }

        int recoveredCount = 0;

        // Get player state for recovery
        SDK::APlayerState* playerState = playerController->PlayerState;
        if (!playerState)
        {
            Logger::LogError("[RECOVER_ESP] Failed to get player state");
            return false;
        }

        SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
        if (!playerStateBattle)
        {
            Logger::LogError("[RECOVER_ESP] Failed to cast to APlayerStateBattle");
            return false;
        }
        
        // Iterate ESP targets by calling SDK_GetRandomESPTarget() multiple times
        // This samples different random targets from ESP list
        std::set<SDK::ACharacterBattle*> processedTargets;  // Track processed to avoid duplicates
        
        // Try up to 50 random targets from ESP
        for (int attempt = 0; attempt < 50; attempt++)
        {
            try
            {
                SDK::AActor* targetActorPtr = SDK_GetRandomESPTarget();
                if (!targetActorPtr)
                {
                    continue;  // No target available, try next
                }
                
                SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(targetActorPtr);
                if (!targetCharacter || targetCharacter == playerCharacter)
                {
                    continue;  // Skip nullptr and self
                }
                
                // Skip if already processed
                if (processedTargets.count(targetCharacter) > 0)
                {
                    continue;
                }
                processedTargets.insert(targetCharacter);
                
                // Check if this character is dying via player state
                if (targetCharacter->PlayerState)
                {
                    SDK::APlayerStateBattle* targetStateDying = static_cast<SDK::APlayerStateBattle*>(targetCharacter->PlayerState);
                    if (targetStateDying && targetStateDying->BP_IsDying())
                    {
                        playerStateBattle->BP_RecoverDying(targetCharacter);
                        recoveredCount++;
                    }
                }
            }
            catch (...)
            {
                // Skip this attempt and continue
                continue;
            }
        }

        return recoveredCount > 0;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[RECOVER_ESP] Exception caught");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[RECOVER_ESP] Unknown exception");
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
            (SDK::ECharacterConditionId)65,  // CH024_TRANSPARENT = 65
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
            (SDK::ECharacterConditionId)100,  // CH011_ABYSS_DARK_BODY = 95
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
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)abilityId,  // Ability ID (43-47)
            level,                                   // Level (1-100)
            50.0f,                                   // span: 50 seconds
            50.0f,                                   // value
            0.01f,                                   // interval
            0,                                       // subLevel
            nullptr,                                 // instigatedPlayer
            0,                                       // damageActionSerialNo
            false                                    // bTimeOverwrite
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
    return SetAbility(47, level);  // ABILITY_ATTACK = 43
}

bool InGameHack_AbilityDurable(int level)
{
    return SetAbility(48, level);  // ABILITY_DURABLE = 44
}

bool InGameHack_AbilityMovespeed(int level)
{
    return SetAbility(49, level);  // ABILITY_MOVESPEED = 45
}

bool InGameHack_AbilityHeal(int level)
{
    return SetAbility(51, level);  // ABILITY_HEAL = 46
}

bool InGameHack_AbilityTechnique(int level)
{
    return SetAbility(52, level);  // ABILITY_TECHNIQUE = 47
}
