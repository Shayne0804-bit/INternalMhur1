#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SDK
{
    class APlayerControllerBattle;
    class ACharacterBattle;
    struct FInGameBattleCharacterData;
    struct FVector;
    class AActor;
    enum class ECharacterId : uint8_t;
    enum class EVariationCharacterId : uint8_t;
    enum class ESeasonType : uint8_t;
}

// ============================================================================
// CHARACTER VARIATION FUNCTIONS
// ============================================================================

/**
 * Get available variations for a character
 * @param characterId - The character to get variations for
 * @return Vector of variation IDs (0-based indices)
 */
std::vector<int32_t> GetVariationsForCharacter(SDK::ECharacterId characterId);

/**
 * Get the display name of a variation
 * @param variationId - The variation index (0, 1, 2, etc.)
 * @return String name like "Variation 0", "Variation 1", etc.
 */
std::string GetVariationName(int32_t variationId);

/**
 * Convert combo index to actual variation ID
 * @param characterId - The character ID
 * @param comboIndex - The index from the combo box (0, 1, 2, etc.)
 * @return The actual variation ID (e.g., 0, 1, 2 for Ch000; 1, 3 for Ch001)
 */
int32_t GetVariationIdFromComboIndex(SDK::ECharacterId characterId, int32_t comboIndex);

/**
 * Convert ECharacterId + variation index to EVariationCharacterId
 * @param characterId - The character ID
 * @param variationIndex - The variation index (0, 1, 2, etc.)
 * @return The corresponding EVariationCharacterId that contains both character and variation info
 */
SDK::EVariationCharacterId GetVariationCharacterId(SDK::ECharacterId characterId, int32_t variationIndex);

/**
 * Extract character ID and variation index from EVariationCharacterId
 * Inverse operation of GetVariationCharacterId()
 * @param variationCharacterId - The variation character ID enum
 * @return Pair of (ECharacterId, variation index)
 */
std::pair<SDK::ECharacterId, int32_t> GetCharacterAndVariationFromVariationCharacterId(SDK::EVariationCharacterId variationCharacterId);

/**
 * Get list of all available variation character IDs as enums
 * Returns all EVariationCharacterId enums in order: Ch001_Variation0, Ch001_Variation1, Ch002_Variation0, etc.
 * This is the direct enum list - no conversion needed!
 */
std::vector<SDK::EVariationCharacterId> GetAllVariationCharacterIds();

/**
 * Get list of all available variation names for combo selection
 * Returns all variations in order: Ch001_Variation0, Ch001_Variation1, Ch002_Variation0, etc.
 */
std::vector<std::string> GetAllVariationNames();

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Check if current game mode is a valid battle mode (2-9)
 * Valid modes: SOLO_BATTLE(2), DUO_BATTLE(3), SQUAD_BATTLE(4), LEADERS_BATTLE(5),
 *             DOMINATION_BATTLE(6), SOLOPICK_BATTLE(7), TUTORIAL(8), TRAINING(9)
 * @return true if in valid battle mode, false otherwise
 */
bool IsValidBattleMode();

/**
 * Change character on server with logging
 * @param PlayerController - The APlayerControllerBattle instance
 * @param CurrentCharacter - Current character battle pawn
 * @param NewCharacterData - The new character data to apply
 */
bool TrainingHack_ChangeCharacterOnServer(
    SDK::APlayerControllerBattle* PlayerController,
    SDK::ACharacterBattle* CurrentCharacter,
    const SDK::FInGameBattleCharacterData& NewCharacterData);

/**
 * Transform player into target character on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to transform into
 */
bool InGameHack_TransformInto(
    SDK::APlayerControllerBattle* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter);

/**
 * Transform into random ESP target (hotkey wrapper)
 * Gets player and random target from ESP, then calls InGameHack_TransformInto
 */
bool InGameHack_TransformIntoRandomESP();

/**
 * Duplicate player into target character for imitation on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to duplicate into
 * @param spawnLocation - Where to spawn the duplicate
 * @param lifeTime - How long the imitation lasts
 */
bool InGameHack_DuplicateIntoImitation(
    SDK::APlayerControllerBattle* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter,
    const SDK::FVector& spawnLocation,
    float lifeTime);

/**
 * Duplicate into random ESP target for imitation (hotkey wrapper)
 * Gets player and forward target from ESP, then calls InGameHack_DuplicateIntoImitation N times
 */
bool InGameHack_DuplicateIntoImitationRandomESP(float spawnOffsetZ, float lifeTime, int count);

// ============================================
// BUFF/STAT ADJUSTMENT HACKS
// ============================================

/**
 * Set general reload adjust rate
 * @param rate - Reload rate multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
 */
bool InGameHack_SetReloadAdjustRate(float rate);

/**
 * Set reload adjust rate for roll slot skills
 * @param rate - Reload rate multiplier for roll slot
 */
bool InGameHack_SetReloadAdjustRate_RollSlot(float rate);

/**
 * Set reload adjust rate while wearing blue flame
 * @param rate - Reload rate multiplier while wearing blue flame
 */
bool InGameHack_SetReloadAdjustRate_WearBlueFlame(float rate);

// ============================================
// TRAINING MODE EXECUTION
// ============================================

/**
 * Apply training player configuration (player character setup only, no AI)
 * @param characterId - Character ID (0-50)
 * @param unique1 - Unique skill 1 level (0-10)
 * @param unique2 - Unique skill 2 level (0-10)
 * @param unique3 - Unique skill 3 level (0-10)
 * @param skillCode - Skill variation code (0-5)
 * @param costumeCode - Costume code ID (0=default)
 * @param costumeAuraType - Costume aura type (0-5)
 */
/**
 * Apply player configuration for training mode
 */
bool InGameHack_ApplyPlayerConfiguration(int characterId, int variationId, int unique1, int unique2, int unique3, int skillCode, int costumeCode, int costumeAuraType);

/**
 * Apply player configuration to ALL PlayerControllerBattle instances in the match
 * @param variationCharacterId - The combined EVariationCharacterId enum (e.g., Ch001_Variation0)
 * @param unique1 - Unique skill 1 level (1-9)
 * @param unique2 - Unique skill 2 level (1-9)
 * @param unique3 - Unique skill 3 level (1-9)
 * @param costumeCode - Costume code ID (0-5)
 * @param costumeAuraType - Costume aura type (0-5)
 * @return Number of characters successfully changed (0 if failed or no characters found)
 */
int InGameHack_ApplyToAllControllers(SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType);

/**
 * Get ALL UNIQUE TEAM IDs present in the current match
 * Scans all characters and collects their unique team IDs (no hardcoding)
 * @return Vector of unique team IDs found in the match
 */
std::vector<unsigned char> InGameHack_GetAllTeamIds();

/**
 * Get all characters in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of ACharacterBattle pointers for that team
 */
std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharactersByTeamId(unsigned char teamId);

/**
 * Get all player names in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of player names for that team
 */
std::vector<std::string> InGameHack_GetTeamNamesByTeamId(unsigned char teamId);

/**
 * Get my current team ID
 * @return Team ID of the local player, or -1 if not found
 */
int InGameHack_GetMyTeamId();

/**
 * Get all characters belonging to a specific TEAM ID
 * @param teamId - The team ID to filter by
 * @return Vector of all ACharacterBattle pointers in that team
 */
std::vector<SDK::ACharacterBattle*> InGameHack_GetCharactersByTeamId(unsigned char teamId);

/**
 * Apply player configuration to ALL characters in a SPECIFIC TEAM
 * Filters characters by TeamID and applies changes only to matching team members
 * @param teamId - Target team ID (actual value from the match, not hardcoded)
 * @param variationCharacterId - Character variation to apply
 * @param unique1 - Unique skill 1 level (1-9)
 * @param unique2 - Unique skill 2 level (1-9)
 * @param unique3 - Unique skill 3 level (1-9)
 * @param costumeCode - Costume code ID (0-5)
 * @param costumeAuraType - Costume aura type (0-5)
 * @return true if at least one character was modified, false otherwise
 */
bool InGameHack_ApplyToTeam(unsigned char teamId, SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType);

/**
 * Copy skills from the nearest enemy character to the local player
 * Automatically finds the nearest enemy and copies their skills
 * @param bSetCopySkill - Whether to enable skill copy mode
 * @param bUseOwnerCharacterLevel - Use local player's level for copied skills
 * @return 1 if successful, 0 if failed or no enemy found
 */
int InGameHack_CopySkillsFromNearestEnemy(bool bSetCopySkill, bool bUseOwnerCharacterLevel);

/**
 * Copy skills from ONE specific character to the local player
 * The character to copy from must NOT be the local player
 * @param masterCharacter - The character to copy skills from
 * @param bSetCopySkill - Whether to enable skill copy mode
 * @param bUseOwnerCharacterLevel - Use local player's level for copied skills
 * @return 1 if successful, 0 if failed
 */
int InGameHack_CopySkillsFromCharacter(SDK::ACharacterBattle* masterCharacter, bool bSetCopySkill, bool bUseOwnerCharacterLevel);

/**
 * Change my team ID to a random available team (excluding current team)
 * Gets all available teams in the match, excludes current team, and switches to a random one
 * Uses BP_SetTeamId to change the player's team
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeam();

/**
 * Change my team ID to a specific team ID
 * @param targetTeamId - The team ID to switch to
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeamTo(unsigned char targetTeamId);

// ============================================================================
// BULLET REDIRECTION FUNCTIONS
// ============================================================================

/**
 * Redirect all bullets to nearest enemy
 * Filters bullets by skill type (Alpha/Beta/Gamma/Special)
 * @param bIncludeAlpha - Include Alpha (Unique1) skills
 * @param bIncludeBeta - Include Beta (Unique2) skills
 * @param bIncludeGamma - Include Gamma (Unique3) skills
 * @param bIncludeSpecial - Include Special skills
 * @return true if bullets were redirected
 */
bool InGameHack_RedirectBulletsToNearestEnemy(bool bIncludeAlpha, bool bIncludeBeta, bool bIncludeGamma, bool bIncludeSpecial);

/**
 * Iterate all players via GameStateTraining method (Method 3)
 * Access PlayerControllerTraining -> GameStateTraining -> UWorld
 * Log all players and their info to temp/gamestate_training_players.log
 */
bool InGameHack_IteratePlayersViaGameStateTraining();

/**
 * Start the training match by calling Decide() on UTrainingMenuWidget
 * Must be called after configuring player via OnSetPlayerCharacter
 */
bool InGameHack_DecideTraining();

// ============================================
// RECOVERY & INVINCIBILITY HACKS
// ============================================

/**
 * Apply invincibility to player
 * @param fixTime - Time to fix (seconds)
 * @param maxTime - Max invincibility time (seconds)
 * @param enableEffect - Show visual effect
/**
 * Set invincibility (hardcoded: all true, fixTime=10, maxTime=120)
 * No parameters needed - all settings are predetermined
 */
bool InGameHack_SetInvincible();

/**
 * Rebuild/reset the player character state (REBUILD_MYSELF)
 * Resets character dynamics and condition
 */
bool InGameHack_RebuildMyself();

/**
 * Apply CH202_TRANS_MISSION condition to player character
 * Enables Ch202 transformation/mission state (ECharacterConditionId = 85)
 */
bool InGameHack_CH202TransMission();

/**
 * Apply UNBREAKABLE condition to player character
 * Makes player unbreakable/invulnerable (ECharacterConditionId = 35)
 */
bool InGameHack_Unbreakable();

/**
 * Recover self (player only) with full health restoration
 * Calls BP_ClearDyingState(true) on self
 */
bool InGameHack_RecoverMe();

/**
 * Recover player + all team members with full health restoration
 * Calls BP_ClearDyingState(true) on all team characters
 */
bool InGameHack_RecoverDyingTeam();

/**
 * Recover a specific team member by index
 * Calls RecoverDyingAlly_ToServer on selected team character
 */
bool InGameHack_RecoverDyingSpecificTeamMember(int teamMemberIndex);

/**
 * Recover ALL characters on map with full health restoration
 * Calls BP_ClearDyingState(true) on all characters regardless of team
 */
bool InGameHack_RecoverDyingAllESP();

/**
 * Apply COMPRESSION_REGENERATION condition to player character
 * Regenerates and applies compression state (ECharacterConditionId = 62)
 */
bool InGameHack_CompressionRegeneration();

/**
 * Apply CH024_TRANSPARENT condition to player character
 * Enables CH024 transparency effect (ECharacterConditionId = 65)
 */
bool InGameHack_CH024Transparent();

/**
 * Apply CH011_ABYSS_DARK_BODY condition to player character
 * Applies abyss dark body state (ECharacterConditionId = 95)
 */
bool InGameHack_CH011AbyssDarkBody();

// ============================================
// ABILITY HACKS
// ============================================

/**
 * Apply ABILITY_ATTACK buff to player character
 * @param level - Ability level (1-100)
 * Span: 50 seconds
 */
bool InGameHack_AbilityAttack(int level);

/**
 * Apply ABILITY_DURABLE buff to player character
 * @param level - Ability level (1-100)
 * Span: 50 seconds
 */
bool InGameHack_AbilityDurable(int level);

/**
 * Apply ABILITY_MOVESPEED buff to player character
 * @param level - Ability level (1-100)
 * Span: 50 seconds
 */
bool InGameHack_AbilityMovespeed(int level);

/**
 * Apply ABILITY_HEAL buff to player character
 * @param level - Ability level (1-100)
 * Span: 50 seconds
 */
bool InGameHack_AbilityHeal(int level);

/**
 * Apply ABILITY_TECHNIQUE buff to player character
 * @param level - Ability level (1-100)
 * Span: 50 seconds
 */
bool InGameHack_AbilityTechnique(int level);

// ============================================
// CHARACTER CONTROL FUNCTIONS
// ============================================

/**
 * Get all ACharacterBattle instances visible in the world
 * Returns vector of character pointers for menu selection
 */
std::vector<class SDK::ACharacterBattle*> InGameHack_GetAllCharacterBattles();

/**
 * Get character names for UI display
 * Returns vector of character names (from PlayerState)
 */
std::vector<std::string> InGameHack_GetCharacterNames();
std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharacterBattles();
SDK::ACharacterBattle* InGameHack_GetTeamCharacterByIndex(int teamMemberIndex);
std::vector<std::string> InGameHack_GetTeamCharacterNames();
bool InGameHack_RecoverDyingTeamMember(SDK::ACharacterBattle* target);
bool InGameHack_ApplyConditionToTeam(int conditionId, int level, float span, float value, float interval, int subLevel);
bool InGameHack_ApplyCondition23ToTeam();

/**
 * Log all damage attenuation curves found in game
 * Scans all UANS_Attack objects and logs their _damageAttenuation curves with their properties
 */
void InGameHack_LogAllDamageAttenuationCurves();

/**
 * Set a character to dying state
 * target: The ACharacterBattle to set dying
 */
bool InGameHack_SetCharacterDying(class SDK::ACharacterBattle* target);

/**
 * Kill a character
 * @param victim: The ACharacterBattle to kill
 * @param killer: The ACharacterBattle doing the killing (nullptr = use player)
 */
bool InGameHack_KillCharacter(class SDK::ACharacterBattle* victim, class SDK::ACharacterBattle* killer);

/**
 * Validate transmission mission level
 * @param level - Level to validate (0-9)
 */
bool InGameHack_ValidateTransMissionLevel(int level);

/**
 * Set CH202 init transmission mission level in Ch202Params
 * @param levelIndex - index in unique3_initLevel array
 * @param newValue - new value to store
 */
bool InGameHack_SetInitTransMissionLevel(int levelIndex, int32_t newValue);

/**
 * Prevent dropping supplies when character dies
 * @param bPreventDrop - true to prevent dropping
 */
bool InGameHack_PreventDropOnDeath(bool bPreventDrop);

/**
 * Set skill level for a character
 * @param skillIndex - The skill index (0-8 for levels 1-9)
 * @param level - Level to set (1-9)
 */
bool InGameHack_SetSkillLevel(int skillIndex, int level);

/**
 * Upgrade a supply item
 * @param supplyIndex - The supply index in inventory
 * @param level - Level to set (1-99)
 */
bool InGameHack_UpgradeSupply(int supplyIndex, int level);

/**
 * Apply team buffs
 * @param attackAdjust - Attack bonus (e.g. 1.5f = +50%)
 * @param durableAdjust - Durability bonus
 * @param speedAdjust - Speed bonus
 * @param healingAdjust - Healing bonus
 * @param reloadAdjust - Reload speed bonus
 */
bool InGameHack_ApplyTeamBuffs(float attackAdjust, float durableAdjust, float speedAdjust, float healingAdjust, float reloadAdjust);

/**
 * Stop using current supply
 */
bool InGameHack_StopUsingSupply();

/**
 * Validate transmission mission level (calls ValidateTransMissionLevel with level 5)
 */
void InGameHack_ValidateTransMissionLevel();

/**
 * Get the current season pass rank and log it to C:/Temp/SeasonPassRank.log
 * @return Current season pass rank (int32), or -1 if error
 */
int InGameHack_GetCurrentSeasonPassRank();

/**
 * Test BP_SetPurchaseTotalExpValue to increase season pass EXP preview
 * @param type - Season type (0=SeasonLicense, 1=SpecialLicense)
 * @param count - Number of items to purchase
 * @return true if successful, false if widget not found or error
 */
bool InGameHack_IncreaseSeasonPassExp(SDK::ESeasonType type, int32_t count);

// ============================================================================
// CHARACTER CONDITION AUTO-EXECUTION (Battle Mode Transition Detection)
// ============================================================================

/**
 * Process character condition auto-execution on battle mode entry
 * Called every frame from menu render loop to detect battle mode transitions
 * When transitioning from invalid to valid battle mode, executes active toggles once
 * 
 * This function should be called from ImGuiMenu::Render() every frame
 * @param enableDekuMode - Execute DEKU MODE when entering battle?
 * @param enableUnbreakable - Execute UNBREAKABLE when entering battle?
 * @param enableCompressionRegen - Execute MR COMPRESSE MODE when entering battle?
 * @param enableMirioMode - Execute MIRIO MODE when entering battle?
 * @param enableTokoyamiMode - Execute TOKOYAMI DARK MODE when entering battle?
 */
void InGameHack_ProcessCharacterConditionAutoExecution(
    bool enableDekuMode,
    bool enableUnbreakable,
    bool enableCompressionRegen,
    bool enableMirioMode,
    bool enableTokoyamiMode);
// ============================================================================
// PLAYER NAME CHANGE
// ============================================================================

/**
 * Change player name in the game
 * @param newName - The new player name (max 255 characters)
 * @return true if successful, false otherwise
 */
bool InGameHack_ChangePlayerName(const char* newName);

/**
 * Buy License Exp from backend subsystem
 * @param count - Number of License Exp to buy
 * @return true if successful, false otherwise
 */
bool InGameHack_BuyLicenseExp(int32_t count);


