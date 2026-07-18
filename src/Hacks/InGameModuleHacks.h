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

/**
 * Aim Search : systeme natif "AimingSearch" du jeu.
 * Force le flag PlayerStateBattle::SetIsAimingSeached(true) sur tous les
 * ennemis => le moteur les REVELE a l'ecran (marqueur / silhouette a travers
 * le decor). Execution locale, pas de RPC. C'est le vrai systeme de detection
 * du jeu, on ne fait que le piloter.
 * @param searchTime  duree de detection en secondes (<=0 => 10s)
 * @return nombre d'ennemis reveles ce tick.
 */
int InGameHack_AimSearch(float searchTime = 0.0f);


struct SeasonRankRewardItemOption
{
    int arrayIndex = -1;
    int rank = 0;
    int slot = 0; // 0=FreeItem, 1=PremiumItem
    int code = 0;
    int category = 0;
    int quantity = 0;
    std::string label;
};

struct Ch202Unique3ParamsConfig
{
    float meleeAChaseHeightOffset = 0.0f;
    float meleeAChaseStartSpeedMax = 1.0f;
    float meleeAChaseStartSpeedMin = 1.0f;
    float meleeAChaseLastSpeed = 1.0f;
    float meleeAChaseSpeedSpan = 1.0f;
    float meleeAChaseSpan = 1.0f;
    float meleeADistance = 1.0f;
    float meleeABreakTargetSpeed = 1.0f;
    float meleeABreakTargetSpan = 1.0f;
    float unique1HoldTime = 1.0f;
    float unique2NeedFieldTime = 1.0f;
    float unique2HoldTime = 1.0f;
    float unique2MoveTime = 1.0f;
    float unique2MoveSpeedMin = 1.0f;
    float unique2MoveSpeedMax = 1.0f;
    float unique2QuintupleRiseSlideSpeed = 1.0f;
    float unique2QuintupleRiseSlideSpan = 1.0f;
    float unique2QuintupleFallSlideSpeed = 1.0f;
    float unique2QuintupleFallSlideSpan = 1.0f;
    int32_t unique2AfterLevel = 1;
    bool unique2QuintupleAllAmmoConsumed = false;
    int32_t initTransMissionLevel = 1;
    float endDistanceOfFollowing = 1.0f;
    float moveSpeedStart = 1.0f;
    float moveSpeedEnd = 1.0f;
    float speedChangeTime = 1.0f;
    float exitSpeedStart = 1.0f;
    float exitSpeedEnd = 1.0f;
    float exitChangeTime = 1.0f;
    int32_t limitCount = 1;
    float moveMagazinePercentMin = 1.0f;
    float moveMagazinePercentMax = 1.0f;
    float punchMagazinePercentMin = 1.0f;
    float punchMagazinePercentMax = 1.0f;
    int32_t recoverMagazineBase = 1;
    int32_t recoverMagazineAdd = 1;
    float delayCancelTimer = 1.0f;
    float lockOnSec = 1.0f;
    float lockOnMinSec = 0.1f;
    float lockOnRange = 1000.0f;
    int32_t lockOnAttackTargetCheckType = 0;
    float curveHorizontalDistanceMin = 1.0f;
    float curveHorizontalDistanceMax = 1.0f;
    float middleUpOffset = 1.0f;
    float targetOffset = 1.0f;
    float splitDistance = 1.0f;
    float splitLerpRate = 1.0f;
    float controlPointsRate = 1.0f;
    float minDetroitRange = 1.0f;
    float minDetroitSpeed = 1.0f;
    float maxDetroitRange = 1.0f;
    float maxDetroitSpeed = 1.0f;
    float middleOffsetX = 0.0f;
    float middleOffsetY = 0.0f;
    float middleOffsetZ = 0.0f;
    float middleOffsetAerialX = 0.0f;
    float middleOffsetAerialY = 0.0f;
    float middleOffsetAerialZ = 0.0f;
    float specialHoldTime = 1.0f;
    float specialActivationTime = 1.0f;
    int32_t specialSmokeMagazineCost = 1;
    int32_t specialLegCount = 1;
    float specialJumpVerticalSpan = 1.0f;
    float specialJumpVerticalHeight = 1.0f;
    float specialJumpForwardSpan = 1.0f;
    float specialJumpForwardHeight = 1.0f;
    float specialJumpForwardInitialSpeedH = 1.0f;
    float specialJumpForwardLastSpeedH = 1.0f;
    float specialJumpForwardAccelSpanH = 1.0f;
    float specialWallJumpSpan = 1.0f;
    float specialWallJumpHeight = 1.0f;
    float specialWallJumpInitialSpeed = 1.0f;
    float specialWallJumpLastSpeed = 1.0f;
    float conditionAnimationMultiplyRate = 1.0f;
    float conditionMoveMultiplyRate = 1.0f;
    float conditionJumpAdjustMultiplyRate = 1.0f;
};

// CH025 V2 (Nejire Hado rework) tunable SDK params.
// Sources: UBP_Ch025V2_U2_Param_C (flight), UBP_Ch025V2U3_Param_C (wave/barrier),
// UBP_Ch025_ActionAttack_Special_C (dash), UBB_CC_CH025_WAVE_BARRIER_C,
// UBB_CC_Ch025_Continuos_Recover_Health_C.
struct Ch025V2ParamsConfig
{
    // Unique 2 — flight
    float u2MoveSpeedXY = 1.0f;
    float u2MoveSpeedZUp = 1.0f;
    float u2MoveSpeedZDown = 1.0f;
    float u2ReinforceMoveSpeedXY = 1.0f;
    float u2ReinforceMoveSpeedZUp = 1.0f;
    float u2ReinforceMoveSpeedZDown = 1.0f;
    float u2ActivityLimitTime = 1.0f;
    float u2ActivityLowestTime = 1.0f;
    float u2ShockWaveMagRateL1 = 1.0f;
    float u2ShockWaveMagRateL2 = 1.0f;
    float u2ShockWaveMagRateL3 = 1.0f;
    float u2ActionMagRateL1 = 1.0f;
    float u2ActionMagRateL2 = 1.0f;
    float u2ActionMagRateL3 = 1.0f;
    // Unique 3 — wave / barrier
    float u3InitialVerticalSpeed = 1.0f;
    float u3LastVerticalSpeed = 1.0f;
    float u3Span = 1.0f;
    float u3WaitTurnTime = 1.0f;
    float u3ApplyInertiaSpawn = 1.0f;
    float u3ApplyInertiaSpawnHRate = 1.0f;
    float u3ApplyInertiaSpawnVRate = 1.0f;
    float u3ConditionTimeL1 = 1.0f;
    float u3ConditionTimeL2 = 1.0f;
    float u3ConditionTimeL3 = 1.0f;
    float u3BarrierValueL1 = 1.0f;
    float u3BarrierValueL2 = 1.0f;
    float u3BarrierValueL3 = 1.0f;
    float u3BarrierValueAllyL1 = 1.0f;
    float u3BarrierValueAllyL2 = 1.0f;
    float u3BarrierValueAllyL3 = 1.0f;
    float u3TurnAngleDeg = 1.0f;
    float u3TurnTime = 1.0f;
    float u3TurnBlendExp = 1.0f;
    int32_t u3TurnSteps = 1;
    float u3ManyAllyBarrierRate = 1.0f;
    float u3OwnerBarrierListValue = 1.0f;
    float u3AllyBarrierListValue = 1.0f;
    // Special — dash
    float specialLowGravityTime = 1.0f;
    float specialStartSpeed = 1.0f;
    float specialMiddleSpeed = 1.0f;
    float specialEndSpeed = 1.0f;
    float specialStartSpan = 1.0f;
    float specialEndSpan = 1.0f;
    float specialDashTime = 1.0f;
    float specialStartVerticalSpeed = 1.0f;
    float specialMiddleVerticalSpeed = 1.0f;
    float specialEndVerticalSpeed = 1.0f;
    float specialStartVerticalSpan = 1.0f;
    float specialEndVerticalSpan = 1.0f;
    // Wave barrier condition effect
    float barrierMaxTime = 1.0f;
    float barrierDurability = 1.0f;
    float barrierCopyRate = 1.0f;
    // Continuous recover health
    float recoveryHealthValue = 1.0f;
};

struct AttackChainConfig
{
    bool useChainComboFlag = true;
    float chainComboTime = 0.25f;
    bool enableShiftAttackActions = true;
    bool clearShiftActionAttackFlags = true;
    bool useAnimationRate = true;
    float animationRate = 2.0f;
    bool animationRateNagara = true;
    bool usePhaseEndCondition = false;
    bool comboCommand = true;
    bool grabbed = false;
    float endTimer = 0.0f;
    bool landing = false;
    bool endAnim = true;
    int32_t endAnimSlot = 0;
    bool finishCurrentPhase = false;
    bool terminateAttackLayer = false;
    bool enableAimingActionCancel = true;
    int32_t actionCancelFlag = 255;
    bool ownerActionOnly = false;
    bool validateNextReservedAction = true;
};

struct DownPowerConfig
{
    bool patchDamageParams = true;
    int32_t damageType = 16;
    bool includeNoActionDamage = true;
    bool overrideRecoverSpan = false;
    float recoverSpan = 0.0f;
    bool applyDurableRates = true;
    float durableRate = 1.0f;
    float durableAttackActionRate = 1.0f;
    float durableTakeDamageRate = 1.0f;
    float durableSpecialActionRate = 1.0f;
    float durableRollSlotRate = 1.0f;
    float durableTakeDamageRollSlotRate = 1.0f;
    bool applyBreakDownSuperArmorRate = true;
    float breakDownSuperArmorRate = 1.0f;
    bool disableTargetSuperArmor = true;
    int32_t targetMode = 1; // 0=Forward ESP, 1=All enemies, 2=All non-local, 3=Selected
    int32_t selectedCharacterIndex = -1;
    float breakDownGaugeRate = 0.0f;
};

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
 * Debug logger for the current player's role slot state.
 * Runs only in valid battle modes and writes to C:\temp\current_roleslot_compare.log.
 */
void InGameHack_AutoLogCurrentRoleSlotState();

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

/**
 * Give/activate Plus Ultra on the local battle character.
 */
bool InGameHack_GivePlusUltra();

/**
 * Fill Plus Ultra gauge on the local battle character without changing active duration.
 */
bool InGameHack_KeepPlusUltraReady();

/**
 * Toggle faster Plus Ultra charge/reload on the local player state.
 */
bool InGameHack_SetPlusUltraFastCharge(bool enable);

/**
 * Toggle wall/player through on the local battle character.
 */
bool InGameHack_SetNoCollision(bool enable);

/**
 * Run camera-driven no collision movement while the hold input is active.
 */
bool InGameHack_UpdateNoCollisionMovement(bool holdActive, float forwardAxis, float rightAxis, float speed);

/**
 * Apply camera FOV through SDK camera/player-controller members.
 */
bool InGameHack_SetCameraFOV(float fov);

/**
 * Apply SDK attack-chain helpers on the local player's current attack.
 */
bool InGameHack_ApplyAttackChainControl(const AttackChainConfig& config);

/**
 * Apply SDK durable/breakdown controls and optionally clear super armor on targets.
 */
bool InGameHack_ApplyDownPowerConfig(const DownPowerConfig& config);
bool InGameHack_TryReadDownPowerConfig(DownPowerConfig& outConfig);

/**
 * Set CV_None CurveFloat key values used by damage attenuation.
 * value <= 1.0 restores the original key values captured before patching.
 */
bool InGameHack_SetCvNoneCurveValue(float value);

/**
 * Damage multiplier (legit path). Instead of rewriting the shared CV_None damage
 * curve asset, drive the player's own attack-adjust buff rate on UBuffParam - the
 * same per-player multiplier the game applies for its own damage buffs. Rate 1.0
 * is normal damage. Must be re-applied periodically as the buff system may reset it.
 */
bool InGameHack_SetAttackDamageMultiplier(float multiplier);

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
int InGameHack_ApplyToAllControllers(int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType);

/**
 * Get all player names on the map
 * Returns vector with format: "PlayerName (CharacterClass)"
 */
std::vector<std::string> InGameHack_GetAllPlayerNames();

/**
 * Apply character change to a specific player by index
 * @param playerIndex - Index in the player list (from GetAllPlayerNames)
 * @param variationCharacterId - The variation character ID enum
 * @param unique1 - Unique skill 1 level (1-9)
 * @param unique2 - Unique skill 2 level (1-9)
 * @param unique3 - Unique skill 3 level (1-9)
 * @param costumeCode - Costume code ID (0-5)
 * @param costumeAuraType - Costume aura type (0-5)
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ApplyToSpecificPlayer(int playerIndex, SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType);

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
bool InGameHack_ApplyToTeam(unsigned char teamId, int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType);

/**
 * Copy skills from the nearest enemy character to the local player
 * Automatically finds the nearest enemy and copies their skills
 * @param bSetCopySkill - Whether to enable skill copy mode
 * @param bUseOwnerCharacterLevel - Use local player's level for copied skills
 * @return 1 if successful, 0 if failed or no enemy found
 */
int InGameHack_CopySkillsFromNearestEnemy(bool bSetCopySkill, bool bUseOwnerCharacterLevel, int copyModeType = 0);

/**
 * Copy skills from ONE specific character to the local player
 * The character to copy from must NOT be the local player
 * @param masterCharacter - The character to copy skills from
 * @param bSetCopySkill - Whether to enable skill copy mode
 * @param bUseOwnerCharacterLevel - Use local player's level for copied skills
 * @return 1 if successful, 0 if failed
 */
// copyModeType: 0 = ECopyModeCharacterType::Ch016 (Copy), 1 = Ch104 (Imitation).
int InGameHack_CopySkillsFromCharacter(SDK::ACharacterBattle* masterCharacter, bool bSetCopySkill, bool bUseOwnerCharacterLevel, int copyModeType = 0);

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

// ============================================================================
// ROLLSLOT SKILL FUNCTIONS
// ============================================================================

/**
 * Test: Launch Ch101 RollSlot Unique Skill
 * Tests the RollSlot skill activation system with detailed logging to C:\temp\rollslot_launch.log
 * @return true if skill was launched successfully
 */
bool InGameHack_LaunchCh101RollSlotSkill();

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
 * Toggle the infinite objects patch from objetos infinitos.ct.
 * Replaces MHUR.exe+3ED21F5 bytes 89 51 08 with NOPs while enabled.
 */
bool InGameHack_SetInfiniteObjectsPatch(bool enabled);

/**
 * Restore original bytes for the infinite objects patch if they are currently applied.
 */
void InGameHack_RestoreInfiniteObjectsPatch();

/**
 * Infinite objects, pure-SDK version (no byte patch / no hardcoded offset).
 * Call TickInfiniteObjectsSDK every frame while enabled; it re-inflates the
 * local player's inventory holder counts (UC::TArray::NumElements) so items
 * are never consumed. Call Reset on disable or match exit to clear tracking.
 */
void InGameHack_TickInfiniteObjectsSDK();
void InGameHack_ResetInfiniteObjectsSDK();

/**
 * Read-only snapshot of one inventory holder (USupplyHolder).
 */
struct InGameInventoryHolder
{
    int32_t inventoryIndex = -1;                     // index in USupplyHolderComponent::_inventory
    int32_t holderIndex = -1;                        // USupplyHolder::_index
    const char* typeName = "?";                      // ESupplyHolderType (INVENTORY/ABILITYSLOT/...)
    bool enabled = false;                            // USupplyHolder::_bEnable
    int32_t supplyCount = 0;                         // USupplyHolder::_supplies.Num()
    int32_t serialCount = 0;                         // USupplyHolder::_serverSerialList.Num()
    std::vector<std::string> supplyClassNames;       // class name of each USupply* slot
};

/**
 * Read-only snapshot of the local player's whole inventory.
 */
struct InGameInventorySnapshot
{
    bool valid = false;
    int32_t totalSupplies = 0;
    std::vector<InGameInventoryHolder> holders;
};

/**
 * Read the local player's inventory (USupplyHolderComponent) into `out`.
 * Pure SDK reads, no writes. Returns false if not in battle or no component.
 */
bool InGameHack_ReadInventory(InGameInventorySnapshot& out);

/**
 * Convenience: read the inventory and dump it to the log (one line per holder).
 */
void InGameHack_LogInventory();

/**
 * Drop every supply in the local player's inventory via OnDrop_ToServer (by
 * server serial). With Infinite Objects active the inventory count does not
 * decrement, so dropped supplies are duplicated on the ground.
 * Returns the number of serials sent to the server.
 */
int32_t InGameHack_DropInventorySupplies(bool longDropDistance);

/**
 * Convenience: drop all inventory supplies and append the result to the log.
 */
void InGameHack_LogDropInventorySupplies();

/**
 * Scan the world for droppable item actors (AItemBase) and rebuild the internal
 * catalog used by the custom drop menu. Returns the number of distinct items.
 */
int InGameHack_ScanWorldItemCatalog();

/**
 * Copy the current world item catalog display names (for the UI combo).
 */
void InGameHack_GetWorldItemCatalogNames(std::vector<std::string>& out);

/**
 * Drop `serialId` unique request serials per quantity pass. The replicated
 * ASupplyActorBase actors are identified afterward by _netSupplyId/_supplyId.
 * Logs to C:\temp\rugir_inventory.log. Returns the number of serials sent.
 */
int InGameHack_DropCatalogItem(int catalogIndex, int quantity, int serialId, bool longDrop);

/**
 * Custom drop placement updater. Tracks custom-drop serials and moves the
 * replicated ASupplyActorBase instances into FName groups around the player.
 */
bool InGameHack_HasPendingCustomDropPlacement();
bool InGameHack_UpdateCustomDropPlacement();

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

/**
 * Apply a configurable character condition to the local player.
 * applyMode: 0=SetCondition_ToServer, 1=BP_SetCondition, 2=BP_SetConditionLocal.
 */
bool InGameHack_ApplyCustomCharacterCondition(int conditionId, int applyMode, int level, float duration, float value, float interval, int subLevel, bool timeOverwrite);

/**
 * Clear a character condition from the local player using TIME_UP end type.
 */
bool InGameHack_ClearCustomCharacterCondition(int conditionId);

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

// Keeps enabled ability conditions applied (throttled re-apply, ~1/min) and clears
// stale ability conditions at match entry. Call every frame update.
void InGameHack_TickAbilityConditions();

// Legacy name for the frame-update call site; delegates to the tick above.
void InGameHack_AutoClearConditionOnModeChange();

// Damage multiplier via RPC parameter scaling (no condition injection, no crash).
// GetLocalDamageComponentObject: the object GameThreadHook must hook so its
// SendDamageToClient RPC passes through HookedProcessEvent. TryScaleDamageRPC:
// called for each intercepted ProcessEvent; scales _damageValue when the function
// is the damage RPC and the multiplier slider is > 1.
namespace SDK { class UObject; class UFunction; }
SDK::UObject* InGameHack_GetLocalDamageComponentObject();
bool InGameHack_TryScaleDamageRPC(const SDK::UObject* object, SDK::UFunction* function, void* params);
// Installs a direct vtable hook on the local damage component's ProcessEvent slot so
// the SendDamageToClient RPC routes through our scaler. Call every frame update.
void InGameHack_InstallDamageProcessEventHook();
// Restores ProcessEvent's original bytes. MUST be called before DLL unload or the
// game crashes calling into freed detour memory.
void InGameHack_RemoveDamageProcessEventHook();
// Dedicated damage-multiplier diagnostic log (C:\temp\rugir_dmgmult.log).
void DmgMultLog(const std::string& line);

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
std::vector<std::string> InGameHack_GetClearInvincibleTargetNames();

/**
 * Clear invincibility on selected battle targets through APlayerStateBattle SDK server RPCs.
 * targetMode: 0=Forward ESP, 1=All enemies, 2=All non-local characters, 3=Selected character index
 * method: 0=ClearInvincible_Server, 1=ClearInvincibleFromAttack_Server, 2=ClearInvincibleWithTag_Server
 * Returns number of successful RPC sends.
 */
int InGameHack_ClearInvincibleTargets(
    int targetMode,
    int method,
    bool ignoreFixed,
    int attackId,
    const char* tag,
    int selectedCharacterIndex);

std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharacterBattles();
std::vector<std::string> InGameHack_GetTeamCharacterNames();
bool InGameHack_RecoverDyingTeamMember(SDK::ACharacterBattle* target);

/**
 * Log all damage attenuation curves found in game
 * Scans all UANS_Attack objects and logs their _damageAttenuation curves with their properties
 */
void InGameHack_LogAllDamageAttenuationCurves();

/**
 * Scan loaded CurveFloat objects for CV_none and dump curve keys to C:/Temp/cv_none_curve_scan.log.
 */
bool InGameHack_DumpCvNoneCurveScan();

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
 * Hide Kills: empties the battle kill-log manager's replicated lists locally each
 * frame, so kill entries never render in the kill feed and the team/leader KO
 * tallies read as empty. Must be called continuously while enabled.
 */
void InGameHack_ApplyHideKills();

/**
 * Infinite Skills: refills the local character's skill magazines (unique alpha/
 * beta/gamma + special) to max via the MagazineManagementComponent SDK methods,
 * so skills never run out. Uses SDK members/functions only (no raw offsets).
 * Must be called continuously while enabled.
 */
bool InGameHack_ApplyInfiniteSkills();

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

bool InGameHack_ApplyCh202Unique3Params(const Ch202Unique3ParamsConfig& config);
bool InGameHack_TryReadCh202Unique3Params(Ch202Unique3ParamsConfig& outConfig);

bool InGameHack_ApplyCh025V2Params(const Ch025V2ParamsConfig& config);
bool InGameHack_TryReadCh025V2Params(Ch025V2ParamsConfig& outConfig);

/**
 * Set skill level for a character
 * @param skillIndex - The skill index (0-8 for levels 1-9)
 * @param level - Level to set (1-9)
 */
bool InGameHack_SetSkillLevel(int skillIndex, int level);

/**
 * Test the server-side item-use attack action path.
 * Calls CharacterActionControlComponent::SetAttackAction_ToServer(USEITEM).
 */
bool InGameHack_TestUseItemAction(int uniqueLevel = 1);

/**
 * Test the server-side ally respawn action path.
 * Calls CharacterActionControlComponent::SetAttackAction_ToServer(USERESPAWN).
 */
bool InGameHack_TestUserRespawnAction();
bool InGameHack_TestUserRespawnSelectedTeamMember(int teamMemberIndex);

/**
 * Test the real respawn-card supply path.
 * Finds a respawn supply in inventory and calls SupplyHolderComponent::OnUseSupply_ToServer.
 */
bool InGameHack_TestUseRespawnCardSupply();

/**
 * Get current skill level for a character
 * @param skillIndex - The skill index (0-8)
 * @return - Current skill level (1-9) or -1 on error
 */
int InGameHack_GetSkillLevel(int skillIndex);

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
    bool enableTokoyamiMode,
    bool enableGenericCondition,
    int conditionId,
    int applyMode,
    int level,
    float duration,
    float value,
    float interval,
    int subLevel,
    bool timeOverwrite);
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
 * Set backend player platform enum.
 * EPlatform: 0=Invalid, 1=PlayStation, 2=Xbox, 3=Windows, 4=Switch, 5=None.
 */
bool InGameHack_SetBackendPlayerPlatform(int platform);

/**
 * Force the backend fake platform string.
 */
bool InGameHack_ForceFakePlatform(const char* platformName);

/**
 * Generate projectile in front of player (hardcoded: Ch010 PUSH_SPECIAL)
 * Creates projectile at player location facing forward
 * @return true if successful, false otherwise
 */
bool InGameHack_GenerateProjectileInFront();

/**
 * Dump the replicated projectile generation slots from the local player.
 * Use after firing a normal projectile in-game to capture runtime generator IDs.
 * @return true if dump was written, false otherwise
 */
bool InGameHack_DumpLastProjectileGenerateRep();

/**
 * Dump active projectile generators, bullets, projectile notifies and data assets.
 * Use after firing a normal projectile in-game when _createGenerateRep stays empty.
 * @return true if dump was written, false otherwise.
 */
bool InGameHack_DumpProjectileRuntimeDebug();

/**
 * Buy License Exp from backend subsystem
 * @param count - Number of License Exp to buy
 * @return true if successful, false otherwise
 */
bool InGameHack_BuyLicenseExp(int32_t count);

/**
 * Add Special License EXP locally by updating the runtime season data.
 * @param exp - Raw EXP to add to the Special License progress.
 * @return true if the runtime data was updated or the native local add call succeeded.
 */
bool InGameHack_AddSpecialLicenseExpLocal(int32_t exp);

/**
 * Send ReceiveLicense requests through BackendSubsystem for authorized backend idempotency testing.
 * @param seasonCode - SeasonCode argument sent to ReceiveLicense.
 * @param freeRank - Free reward rank to claim, or <=0 to leave the free list empty.
 * @param premiumRank - Limited/premium reward rank to claim, or <=0 to leave the premium list empty.
 * @param repeatCount - Number of immediate requests to send.
 * @return true if at least one request id was returned.
 */
bool InGameHack_ReceiveLicenseClaimTest(int32_t seasonCode, int32_t freeRank, int32_t premiumRank, int32_t repeatCount);

/**
 * Send ReceiveSpecialLicense requests through BackendSubsystem for authorized backend idempotency testing.
 * @param rank - Special license rank to claim.
 * @param repeatCount - Number of immediate requests to send.
 * @return true if at least one request id was returned.
 */
bool InGameHack_ReceiveSpecialLicenseClaimTest(int32_t rank, int32_t repeatCount);

/**
 * Dump season pass and special license getter results to C:/Temp/season_license_getters.log.
 * @return true if the dump file was written, false otherwise.
 */
bool InGameHack_DumpSeasonLicenseGetters();

/**
 * Read existing reward items from DatabaseParams.season.ranks for menu selection.
 */
std::vector<SeasonRankRewardItemOption> InGameHack_GetSeasonRankRewardItemOptions();

/**
 * Replace DatabaseParams.season.ranks rewards with one existing reward item.
 * targetSlotMask: bit 0=FreeItem, bit 1=PremiumItem.
 * @return number of reward slots modified, or -1 on failure.
 */
int InGameHack_ReplaceSeasonRankRewardsFromExistingReward(
    int sourceArrayIndex,
    int sourceSlot,
    int targetRank,
    int quantity,
    int targetSlotMask,
    bool applyAllRanks);

// ============================================================================
// RENTAL TICKET BYPASS
// ============================================================================

/**
 * Bypass rental ticket amount check by writing max ticket value (3)
 * Navigates UWorld->OwningGameInstance->+0xF0->+0x8->+0x6B0 and writes byte at +0x1038
 * Must be called every frame while enabled (one-frame write, no persistence)
 * @return true if write succeeded, false otherwise
 */
bool InGameHack_BypassRentalTickets();
