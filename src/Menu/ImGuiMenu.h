#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <Windows.h>
#include <cstdint>
#include <vector>

// Forward declarations
namespace SDK
{
    class ACharacterBattle;
}

class ACh008;
class APlayer;

namespace ImGuiMenu
{
    // ============================================================================
    // HOTKEY STRUCTURE - Groups Keyboard, Xbox, and PS4 together
    // ============================================================================
    struct HotkeySet
    {
        int Keyboard = 0x00;  // PC keyboard key code
        int Xbox = 0x0000;    // XINPUT gamepad button code (Xbox/PS4 compatible)
        int PS4 = 0x0000;     // Same as Xbox for gamepad buttons
        
        HotkeySet() = default;
        HotkeySet(int kb, int gamepad) : Keyboard(kb), Xbox(gamepad), PS4(gamepad) {}
    };

    // ============================================================================
    // SETTINGS STRUCTURE
    // ============================================================================
    struct MenuSettings
    {
        // Global
        bool EnableGlobal = true;
        bool EnableESP = false;
        bool EnableMenuBackgroundVideo = true;

        // ESP - Display
        bool EnablePlayerESP = false;
        bool Player_Box = false;
        bool Player_Health = false;
        bool Player_Distance = false;
        bool Player_Name = false;
        bool ShowHP = false;         // Display health points
        bool ShowGP = false;         // Display guard points
        bool ShowPU = false;        // Display Plus Ultra (when available)
        bool ShowPlatform = false;   // Display player platform (PS/Xbox/PC)
        bool ShowTeamId = false;     // Display team ID in ESP text
        bool ShowPlayerSkeleton = false;  // Display skeleton bones
        bool ShowServerStatusOverlay = false;  // Display current server and ping overlay

        // Aimbot
        bool EnableAimbot = false;
        bool AimbotSmoothing = false;
        float AimbotSmoothFactor = 0.3f;  // 0.01-1.0 legacy factor; values above 1 add slower smoothing
        bool AimbotDrawLine = false;  // Draw line from center to target
        bool AimbotDrawFOV = false;   // Draw FOV circle on screen
        bool AimbotDrawCrosshair = false;  // Draw the game crosshair at screen center
        float AimbotFOVRadius = 500.0f;  // FOV radius in pixels (adjustable)
        float AimbotMaxDistance = 1000.0f;  // Max 3D distance to target (meters)
        bool AimbotRequireHold = false;  // Require holding key
        bool AimbotIgnoreDownedTargets = true;  // Skip downed/dying targets by default
        
        // Aimbot Hotkey (Keyboard/Xbox/PS4 grouped)
        HotkeySet AimbotHoldKey = HotkeySet(0x02, 0x0100);  // KB: RButton, Gamepad: LB
        
        // Teleport to Kota Hotkey
        bool EnableTeleportToKota = false;
        bool EnableInfiniteObjects = false;
        HotkeySet TeleportToKotaKey = HotkeySet(0x50, 0x0200);  // KB: P, Gamepad: RB

        // Custom Drop (generate N unique request serials, then identify replicated FNames)
        bool EnableCustomDrop = false;
        int  CustomDropQuantity = 1;        // 1-100 passes
        int  CustomDropSelectedIndex = 0;   // index into scanned world item catalog
        int  CustomDropSerialId = 1;        // serial count per pass
        HotkeySet CustomDropKey = HotkeySet(0x4A, 0x0400);  // KB: J, Gamepad: dpad-down

        // Transform Into Random ESP Target Hotkey
        bool EnableTransformIntoRandomESP = false;
        HotkeySet TransformIntoRandomESPKey = HotkeySet(0x4F, 0x1000);  // KB: O, Gamepad: A
        
        // Duplicate Into Imitation Random ESP Target Hotkey
        bool EnableDuplicateIntoImitationRandomESP = false;
        HotkeySet DuplicateIntoImitationRandomESPKey = HotkeySet(0x49, 0x8000);  // KB: I, Gamepad: Y
        float DuplicateImitationLifeTime = 30.0f;  // Lifetime for imitation duplicate
        int DuplicateIntoImitationCount = 1;  // Number of imitation duplicates to spawn per hotkey (1-100)
        
        // Copy Skills From Nearest Enemy Hotkey
        bool EnableCopySkillsFromNearestEnemy = false;
        HotkeySet CopySkillsFromNearestEnemyKey = HotkeySet(0x4B, 0x0200);  // KB: K, Gamepad: RB
        bool CopySkillsSetCopySkill = false;  // Set copy skill flag
        bool CopySkillsUseOwnerCharacterLevel = false;  // Use owner character level
        
        // Generate Projectile Hotkey
        bool EnableGenerateProjectile = false;
        HotkeySet GenerateProjectileKey = HotkeySet(0x47, 0x4000);  // KB: G, Gamepad: X
        
        // Reload Adjust Rates
        float ReloadAdjustRate = 1.0f;                    // General reload rate (1.0 = normal)
        float ReloadAdjustRate_RollSlot = 1.0f;           // Reload rate for roll slot
        float ReloadAdjustRate_WearBlueFlame = 1.0f;      // Reload rate while wearing blue flame
        float CvNoneDamageCurveValue = 1.0f;              // CV_None damage attenuation curve key value
        
        // Training Mode - Player Character Setup
        int TrainingPlayerCharacter = 0;                  // Character ID (0=UNDEF, 1=Ch000, 2=Ch001... 44=Ch999)
        int TrainingPlayerVariationId = 0;                // Variation ID (0-100)
        int TrainingPlayerUnique1 = 1;                    // Unique 1 skill level (0-10)
        int TrainingPlayerUnique2 = 1;                    // Unique 2 skill level (0-10)
        int TrainingPlayerUnique3 = 1;                    // Unique 3 skill level (0-10)
        int TrainingPlayerSkillCode = 0;                  // Skill variation code (0-5)
        int TrainingPlayerCostumeCode = 0;                // Costume code ID (0=default)
        int TrainingPlayerCostumeAuraType = 0;            // Costume aura type (0-5)
        
        // Invincibility Hotkey (unified)
        HotkeySet SetInvincibleKey = HotkeySet(0x7A, 0x2000);  // KB: F11, Gamepad: B
        
        // Recovery Settings
        bool EnableRecoveryMe = false;                  // Recover self only
        bool EnableRecoveryTeam = false;                // Recover self + team members
        bool EnableRecoverySelectedTeam = false;        // Recover selected team member only
        bool EnableRecoveryAllESP = false;              // Recover all ESP targets with dying flag
        
        // Teleport Items (Level Up Cards)
        bool EnableTeleportLevelUpCards = false;
        
        // BulletTP (Silent Aim) - Bullet Redirection - Uses same settings as Aimbot
        bool EnableBulletTP = false;
        bool BulletTP_IncludeAlpha = false;      // Include Alpha (Unique1) skills
        bool BulletTP_IncludeBeta = false;      // Include Beta (Unique2) skills
        bool BulletTP_IncludeGamma = false;     // Include Gamma (Unique3) skills
        bool BulletTP_IncludeSpecial = false;   // Include Special skills
        float BulletTP_FOVRadius = 500.0f;      // FOV radius in pixels for targeting
        float BulletTP_MaxDistance = 5000.0f;   // Maximum distance to search for targets
        bool BulletTPIgnoreDownedTargets = true; // Skip downed/dying targets by default

        // Camera FOV (SDK PlayerController::FOV / PlayerCameraManager::DefaultFOV)
        bool EnableCameraFOV = false;
        float CameraFOV = 90.0f;

        // Rebuild Myself Hotkey
        bool EnableRebuildMyself = false;
        HotkeySet RebuildMyselfKey = HotkeySet(0x4C, 0x1F00);  // KB: L, Gamepad: LT

        // ESP - Filters
        bool ShowEnemies = false;
        bool ShowTeam = false;
        bool ShowLobbyCharacters = false;
        bool ShowKota = false;          // Display KOTA (special NPC)
        float Player_DrawDistance = 5000.0f;
        
        // Character Control
        int SelectedCharacterIndex = -1;  // Index of selected character for dying/killing
        int SelectedRecoveryTeamIndex = -1;  // Index of selected team member for recovery
        bool EnableCH202InitTransLevel5 = false;  // Enable CH202 init trans level 5 auto-apply each frame
        bool EnableSupplyMaxStackTo100 = false;  // Enable auto-set supply max stack to 100 each frame
        bool EnableFastPlusUltraCharge = false;  // Enable server fast reload for Plus Ultra charge
        bool EnableNoCollision = false;  // Enable camera-driven no collision movement
        float NoCollisionSpeed = 100.0f;
        HotkeySet NoCollisionHoldKey = HotkeySet(0x54, 0);
        bool EnableClearInvincibleAuto = false;
        int ClearInvincibleTargetMode = 1;  // 0=Forward ESP, 1=All enemies, 2=All characters, 3=Selected
        int ClearInvincibleMethod = 0;      // 0=Clear all, 1=From attack, 2=With tag
        bool ClearInvincibleIgnoreFixed = true;
        int ClearInvincibleAttackId = 4;    // EAttackId::MELEE
        int ClearInvincibleIntervalMs = 250;
        int ClearInvincibleSelectedCharacterIndex = -1;
        char ClearInvincibleTagBuffer[64] = "";
        bool EnableAttackChainAuto = false;
        int AttackChainIntervalMs = 50;
        bool AttackChainUseChainComboFlag = true;
        float AttackChainComboFlagTime = 0.25f;
        bool AttackChainEnableShiftAttackActions = true;
        bool AttackChainClearShiftActionAttackFlags = true;
        bool AttackChainUseAnimationRate = true;
        float AttackChainAnimationRate = 2.0f;
        bool AttackChainAnimationRateNagara = true;
        bool AttackChainUsePhaseEndCondition = false;
        bool AttackChainComboCommand = true;
        bool AttackChainGrabbed = false;
        float AttackChainEndTimer = 0.0f;
        bool AttackChainLanding = false;
        bool AttackChainEndAnim = true;
        int AttackChainEndAnimSlot = 0;
        bool AttackChainFinishCurrentPhase = false;
        bool AttackChainTerminateAttackLayer = false;
        bool AttackChainEnableAimingActionCancel = true;
        int AttackChainActionCancelFlag = 255;
        bool AttackChainOwnerActionOnly = false;
        bool AttackChainValidateNextReservedAction = true;
        bool EnableDownPowerAuto = false;
        int DownPowerIntervalMs = 100;
        int DownPowerTargetMode = 1;  // 0=Forward ESP, 1=All enemies, 2=All non-local, 3=Selected
        int DownPowerSelectedCharacterIndex = -1;
        bool DownPowerPatchDamageParams = true;
        int DownPowerDamageType = 16;
        bool DownPowerIncludeNoActionDamage = true;
        bool DownPowerOverrideRecoverSpan = false;
        float DownPowerRecoverSpan = 0.0f;
        bool DownPowerApplyDurableRates = true;
        float DownPowerDurableRate = 1.0f;
        float DownPowerDurableAttackActionRate = 1.0f;
        float DownPowerDurableTakeDamageRate = 1.0f;
        float DownPowerDurableSpecialActionRate = 1.0f;
        float DownPowerDurableRollSlotRate = 1.0f;
        float DownPowerDurableTakeDamageRollSlotRate = 1.0f;
        bool DownPowerApplyBreakDownSuperArmorRate = true;
        float DownPowerBreakDownSuperArmorRate = 1.0f;
        bool DownPowerDisableTargetSuperArmor = true;

        // CH202 SDK params
        float Ch202MeleeAChaseHeightOffset = 0.0f;
        float Ch202MeleeAChaseStartSpeedMax = 1.0f;
        float Ch202MeleeAChaseStartSpeedMin = 1.0f;
        float Ch202MeleeAChaseLastSpeed = 1.0f;
        float Ch202MeleeAChaseSpeedSpan = 1.0f;
        float Ch202MeleeAChaseSpan = 1.0f;
        float Ch202MeleeADistance = 1.0f;
        float Ch202MeleeABreakTargetSpeed = 1.0f;
        float Ch202MeleeABreakTargetSpan = 1.0f;
        float Ch202Unique1HoldTime = 1.0f;
        float Ch202Unique2NeedFieldTime = 1.0f;
        float Ch202Unique2HoldTime = 1.0f;
        float Ch202Unique2MoveTime = 1.0f;
        float Ch202Unique2MoveSpeedMin = 1.0f;
        float Ch202Unique2MoveSpeedMax = 1.0f;
        float Ch202Unique2QuintupleRiseSlideSpeed = 1.0f;
        float Ch202Unique2QuintupleRiseSlideSpan = 1.0f;
        float Ch202Unique2QuintupleFallSlideSpeed = 1.0f;
        float Ch202Unique2QuintupleFallSlideSpan = 1.0f;
        int Ch202Unique2AfterLevel = 1;
        bool Ch202Unique2QuintupleAllAmmoConsumed = false;
        int Ch202U3InitTransMissionLevel = 1;
        float Ch202U3EndDistanceOfFollowing = 1.0f;
        float Ch202U3MoveSpeedStart = 1.0f;
        float Ch202U3MoveSpeedEnd = 1.0f;
        float Ch202U3SpeedChangeTime = 1.0f;
        float Ch202U3ExitSpeedStart = 1.0f;
        float Ch202U3ExitSpeedEnd = 1.0f;
        float Ch202U3ExitChangeTime = 1.0f;
        int Ch202U3LimitCount = 1;
        float Ch202U3MoveMagazinePercentMin = 1.0f;
        float Ch202U3MoveMagazinePercentMax = 1.0f;
        float Ch202U3PunchMagazinePercentMin = 1.0f;
        float Ch202U3PunchMagazinePercentMax = 1.0f;
        int Ch202U3RecoverMagazineBase = 1;
        int Ch202U3RecoverMagazineAdd = 1;
        float Ch202U3DelayCancelTimer = 1.0f;
        float Ch202U3LockOnSec = 1.0f;
        float Ch202U3LockOnMinSec = 0.1f;
        float Ch202U3LockOnRange = 1000.0f;
        int Ch202U3LockOnAttackTargetCheckType = 0;
        float Ch202U3CurveHorizontalDistanceMin = 1.0f;
        float Ch202U3CurveHorizontalDistanceMax = 1.0f;
        float Ch202U3MiddleUpOffset = 1.0f;
        float Ch202U3TargetOffset = 1.0f;
        float Ch202U3SplitDistance = 1.0f;
        float Ch202U3SplitLerpRate = 1.0f;
        float Ch202U3ControlPointsRate = 1.0f;
        float Ch202U3MinDetroitRange = 1.0f;
        float Ch202U3MinDetroitSpeed = 1.0f;
        float Ch202U3MaxDetroitRange = 1.0f;
        float Ch202U3MaxDetroitSpeed = 1.0f;
        float Ch202U3MiddleOffsetX = 0.0f;
        float Ch202U3MiddleOffsetY = 0.0f;
        float Ch202U3MiddleOffsetZ = 0.0f;
        float Ch202U3MiddleOffsetAerialX = 0.0f;
        float Ch202U3MiddleOffsetAerialY = 0.0f;
        float Ch202U3MiddleOffsetAerialZ = 0.0f;
        float Ch202SpecialHoldTime = 1.0f;
        float Ch202SpecialActivationTime = 1.0f;
        int Ch202SpecialSmokeMagazineCost = 1;
        int Ch202SpecialLegCount = 1;
        float Ch202SpecialJumpVerticalSpan = 1.0f;
        float Ch202SpecialJumpVerticalHeight = 1.0f;
        float Ch202SpecialJumpForwardSpan = 1.0f;
        float Ch202SpecialJumpForwardHeight = 1.0f;
        float Ch202SpecialJumpForwardInitialSpeedH = 1.0f;
        float Ch202SpecialJumpForwardLastSpeedH = 1.0f;
        float Ch202SpecialJumpForwardAccelSpanH = 1.0f;
        float Ch202SpecialWallJumpSpan = 1.0f;
        float Ch202SpecialWallJumpHeight = 1.0f;
        float Ch202SpecialWallJumpInitialSpeed = 1.0f;
        float Ch202SpecialWallJumpLastSpeed = 1.0f;
        float Ch202ConditionAnimationMultiplyRate = 1.0f;
        float Ch202ConditionMoveMultiplyRate = 1.0f;
        float Ch202ConditionJumpAdjustMultiplyRate = 1.0f;
    };

    // ============================================================================
    // HACK SETTINGS STRUCTURE
    // ============================================================================
    struct HackSettings
    {
        bool EnableInvincible = false;
        bool EnableInfiniteHealth = false;
        bool EnableInfiniteBarrier = false;
        bool EnableInfinitePlusUltra = false;
        bool EnableFullBuff = false;
        bool EnableHideKills = false;

        // Ability Hack Levels (1-100)
        int AbilityAttackLevel = 1;
        int AbilityDurableLevel = 1;
        int AbilityMovespeedLevel = 1;
        int AbilityHealLevel = 1;
        int AbilityTechniqueLevel = 1;

        bool Hack_BypassRentalTickets = false;

        // Character Settings for ApplyToAllControllers
        int CharacterId = 1;                  // Character ID (1-44 for Ch000-Ch999)
        int CharacterVariationId = 0;         // Variation ID (0-100)
        int CharacterUnique1 = 1;             // Unique 1 skill level (0-100)
        int CharacterUnique2 = 1;             // Unique 2 skill level (0-100)
        int CharacterUnique3 = 1;             // Unique 3 skill level (0-100)
        int CharacterSkillCode = 0;           // Skill variation code (0-5)
        int CharacterCostumeCode = 0;         // Costume code ID (0=default)
        int CharacterCostumeAuraType = 0;     // Costume aura type (0-5)

        // Supply Management - Unique Skill Levels (for Supply Management section)
        int SupplyUniqueSkill1Level = 1;     // Unique Skill 1 level (1-9)
        int SupplyUniqueSkill2Level = 1;     // Unique Skill 2 level (1-9)
        int SupplyUniqueSkill3Level = 1;     // Unique Skill 3 level (1-9)

        // Character Condition Auto-Execution Toggles
        // These are executed automatically when entering a valid battle mode
        bool CharCondition_EnableDekuMode = false;            // Enable DEKU MODE on battle entry
        bool CharCondition_EnableUnbreakable = false;         // Enable UNBREAKABLE on battle entry
        bool CharCondition_EnableCompressionRegen = false;    // Enable MR COMPRESSE MODE on battle entry
        bool CharCondition_EnableMirioMode = false;           // Enable MIRIO MODE on battle entry
        bool CharCondition_EnableTokoyamiMode = false;        // Enable TOKOYAMI DARK MODE on battle entry

        // Generic character condition editor
        bool CharCondition_AutoExecute = false;
        int CharCondition_SelectedConditionId = 35;            // UNBREAKABLE
        int CharCondition_ApplyMode = 0;                       // 0=Server, 1=BP, 2=Local
        int CharCondition_Level = 0;
        float CharCondition_Duration = 50.0f;
        float CharCondition_Value = 0.0f;
        float CharCondition_Interval = 0.0f;
        int CharCondition_SubLevel = 0;
        bool CharCondition_TimeOverwrite = false;

        // Player Name Change
        char ChangePlayerNameBuffer[256] = {0};              // Input buffer for new player name

        // Backend Platform Test
        int BackendPlatformCode = 3;                         // 0=Invalid, 1=PlayStation, 2=Xbox, 3=Windows, 4=Switch, 5=None
        char FakePlatformBuffer[64] = "Windows";             // Input buffer for ForceFakePlatform
        
        // Special License EXP
        int BuyLicenseExpCount = 30000;                      // Raw Special License EXP to add locally (1-300000)
        int LicenseClaimSeasonCode = 1;                      // SeasonCode for ReceiveLicense backend tests
        int LicenseClaimFreeRank = 1;                        // Free reward rank for ReceiveLicense backend tests
        int LicenseClaimPremiumRank = 0;                     // Premium reward rank for ReceiveLicense backend tests
        int LicenseClaimSpecialRank = 1;                     // Rank for ReceiveSpecialLicense backend tests
        int LicenseClaimRepeatCount = 1;                     // Immediate repeat count for idempotency tests
        int LicenseClaimDelayMs = 1000;                      // Delay between timed backend claim requests
        int SeasonRewardSourceIndex = 0;                     // Cached reward item selection
        int SeasonRewardTargetRank = 1;                       // Season rank to edit when not applying all
        int SeasonRewardQuantity = 1;                         // Replacement quantity
        int SeasonRewardTargetSlot = 2;                       // 0=Free, 1=Premium, 2=Both
        bool SeasonRewardApplyAllRanks = false;               // Replace all season ranks
    };

    // ============================================================================
    // PUBLIC API
    // ============================================================================
    void SetModuleHandle(HMODULE module);
    bool PreloadEmbeddedVideoResource(HMODULE module);
    bool Initialize(IDXGISwapChain* pSwapChain, HWND hWnd);
    void Shutdown();
    void InvalidateRenderTarget();
    void RestoreWindowProc();
    bool HasActiveWindowProc();
    void Render(IDXGISwapChain* pSwapChain);

    bool IsInitialized();
    bool IsVisible();
    void SetVisible(bool visible);

    // Global settings access
    extern MenuSettings g_Settings;
    extern HackSettings g_HackSettings;
    extern std::vector<SDK::ACharacterBattle*> g_CurrentTeamCharacters;
}
