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
        bool EnableGlobal = false;
        bool EnableESP = false;

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
        bool ShowPlayerSkeleton = false;  // Display skeleton bones (like zero1)

        // Aimbot
        bool EnableAimbot = false;
        bool AimbotSmoothing = false;
        float AimbotSmoothFactor = 0.3f;  // Default: 30% (faster targeting)
        bool AimbotDrawLine = false;  // Draw line from center to target
        bool AimbotDrawFOV = false;   // Draw FOV circle on screen
        float AimbotFOVRadius = 500.0f;  // FOV radius in pixels (adjustable)
        bool AimbotRequireHold = false;  // Require holding key (Zero1 exact - reduces detection)
        
        // Aimbot Hotkey (Keyboard/Xbox/PS4 grouped)
        HotkeySet AimbotHoldKey = HotkeySet(0x02, 0x0100);  // KB: RButton, Gamepad: LB
        
        // Teleport to Kota Hotkey
        bool EnableTeleportToKota = false;
        HotkeySet TeleportToKotaKey = HotkeySet(0x50, 0x0080);  // KB: P, Gamepad: RB

        // Transform Into Random ESP Target Hotkey
        bool EnableTransformIntoRandomESP = false;
        HotkeySet TransformIntoRandomESPKey = HotkeySet(0x4F, 0x0040);  // KB: O, Gamepad: A
        
        // Duplicate Into Imitation Random ESP Target Hotkey
        bool EnableDuplicateIntoImitationRandomESP = false;
        HotkeySet DuplicateIntoImitationRandomESPKey = HotkeySet(0x49, 0x8000);  // KB: I, Gamepad: Y
        float DuplicateImitationLifeTime = 30.0f;  // Lifetime for imitation duplicate
        int DuplicateIntoImitationCount = 1;  // Number of imitation duplicates to spawn per hotkey (1-100)
        
        // Copy Skills From Nearest Enemy Hotkey
        bool EnableCopySkillsFromNearestEnemy = false;
        HotkeySet CopySkillsFromNearestEnemyKey = HotkeySet(0x4B, 0x2000);  // KB: K, Gamepad: RB
        bool CopySkillsSetCopySkill = false;  // Set copy skill flag
        bool CopySkillsUseOwnerCharacterLevel = false;  // Use owner character level
        
        // Reload Adjust Rates
        float ReloadAdjustRate = 1.0f;                    // General reload rate (1.0 = normal)
        float ReloadAdjustRate_RollSlot = 1.0f;           // Reload rate for roll slot
        float ReloadAdjustRate_WearBlueFlame = 1.0f;      // Reload rate while wearing blue flame
        
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
        HotkeySet SetInvincibleKey = HotkeySet(0x00, 0x0000);  // KB: F11, Gamepad: B
        
        // Recovery Settings
        bool EnableRecoveryMe = false;                  // Recover self only
        bool EnableRecoveryTeam = false;                // Recover self + team members
        bool EnableRecoverySelectedTeam = false;        // Recover selected team member only
        bool EnableRecoveryAllESP = false;              // Recover all ESP targets with dying flag
        
        // Team Condition Settings
        bool EnableApplyCondition23Team = false;        // Apply REBUILD_MYSELF (ID 23) to all team members
        
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

        // Rebuild Myself Hotkey
        bool EnableRebuildMyself = false;
        HotkeySet RebuildMyselfKey = HotkeySet(0x4C, 0x4000);  // KB: L, Gamepad: LT

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

        // Ability Hack Levels (1-100)
        int AbilityAttackLevel = 1;
        int AbilityDurableLevel = 1;
        int AbilityMovespeedLevel = 1;
        int AbilityHealLevel = 1;
        int AbilityTechniqueLevel = 1;

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

        // Player Name Change
        char ChangePlayerNameBuffer[256] = {0};              // Input buffer for new player name
        
        // Backend - License Exp Purchase
        int BuyLicenseExpCount = 100;                        // Amount of License Exp to buy (1-10000)
    };

    // ============================================================================
    // PUBLIC API
    // ============================================================================
    bool Initialize(IDXGISwapChain* pSwapChain, HWND hWnd);
    void Shutdown();
    void Render(IDXGISwapChain* pSwapChain);

    bool IsInitialized();
    bool IsVisible();
    void SetVisible(bool visible);

    // Global settings access
    extern MenuSettings g_Settings;
    extern HackSettings g_HackSettings;
    extern std::vector<SDK::ACharacterBattle*> g_CurrentTeamCharacters;
}
