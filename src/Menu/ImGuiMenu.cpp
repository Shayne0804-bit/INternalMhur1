#include "ImGuiMenu.h"

#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Hacks/InGameModuleHacks.h"
#include "../Hacks/Character_Changer.h"
#include "../Hacks/Character_Data.h"
#include "../Utils/Logger.h"
#include "../Utils/CostumeHelper.h"
#include "../Utils/CharacterIconLoader.h"
#include "../Utils/SVGLoader.h"
#include "../Assets/MenuResources.h"
#include "../Core/UnloadManager.h"
#include "../Auth/LicenseAuth.h"
#include <algorithm>
#include <climits>
#include <unordered_map>
#include "../SDK/SDKInit.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include <cmath>
#include <cstring>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <objbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#ifndef RUGIR_ENABLE_UNLOAD
#define RUGIR_ENABLE_UNLOAD 1
#endif

#ifndef RUGIR_MENU_CHEAT_FACTORY
#define RUGIR_MENU_CHEAT_FACTORY 0
#endif

// ============================================================================
// CHARACTER CHANGER GLOBAL STATE (Used by Character_Changer.cpp)
// ============================================================================
int s_techAlpha = 9;           // Alpha skill level (1-9)
int s_techBeta = 9;            // Beta skill level (1-9)
int s_techGamma = 9;           // Gamma skill level (1-9)
int s_costumeCode = 0;         // Costume code
int s_emoteSlot[8] = { 0 };    // Emote slots
int s_lastInGameChar = 0;      // Last character ID

// Logo texture
ID3D11ShaderResourceView* g_RugirLogo = nullptr;
#include <imgui.h>
#include <addons/imgui_addons.h>
#include "ImGuiHelperExt.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <windowsx.h>
#include <Xinput.h>
#include <string>
#include <fstream>
#include "ico_font.h"
#include "segue_font.h"
#include "ImGuiSliderHelper.h"
#include "SettingsManager.h"
#pragma comment(lib, "xinput.lib")

#include "../../Free ImGui/examples/example_win32_directx11/Font.h"

// Forward declare ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare SDK canvas resolution function

// Forward declare unload function
#if RUGIR_ENABLE_UNLOAD
extern "C" __declspec(dllimport) void DLL_Unload();
#endif

// Forward declare SDK drawing functions
extern "C" void SDK_DrawAllRectangles();
extern "C" void SDK_DrawAllTextLabels();
extern "C" void SDK_DrawAllSkeletons();
extern "C" void SDK_DrawAimbotInfo();
extern "C" void SDK_DrawAimbotFOV();
extern "C" void SDK_DrawCrosshair();
extern "C" void SDK_DrawBulletRedirectionFOV();

// Forward declare SDK aimbot functions
extern "C" void SDK_RunAimbot();
extern "C" void SDK_RunSilentAim();
extern "C" void SDK_RunTeleportLevelUpCards();
extern "C" void SDK_TeleportItem_LevelUpCard();
extern "C" void SDK_TeleportItem_BagLarge();
extern "C" void SDK_TeleportItem_BagMedium();
extern "C" void SDK_TeleportItem_BagSmall();
extern "C" void SDK_TeleportItem_BoxLarge();
extern "C" void SDK_TeleportItem_BoxSmall();
extern "C" void SDK_TeleportItem_Box();
extern "C" void SDK_TeleportItem_FullSupport();
extern "C" void SDK_TeleportItem_TeamSupport();
extern "C" void SDK_TeleportItem_Revive1_3();
extern "C" void SDK_TeleportItem_ReviveFull();
extern "C" void SDK_TeleportItem_BoxGold();
extern "C" void SDK_TeleportItem_HPDrink();
extern "C" void SDK_TeleportItem_HPDrinkMedium();
extern "C" void SDK_TeleportItem_HPDrinkSmall();
extern "C" void SDK_TeleportItem_GPDrink();
extern "C" void SDK_TeleportItem_GPDrinkMedium();
extern "C" void SDK_TeleportItem_GPDrinkSmall();
extern "C" void SDK_TeleportItem_TeamEnhancementKit();
extern "C" void SDK_TeleportItem_AbilitySupplyAlpha();
extern "C" void SDK_TeleportItem_AbilitySupplyBeta();
extern "C" void SDK_TeleportItem_AbilitySupplyGamma();

namespace ImGuiMenu
{
    // ============================================================================
    // GLOBAL STATE
    // ============================================================================
    MenuSettings g_Settings;
    HackSettings g_HackSettings;
    std::vector<SDK::ACharacterBattle*> g_AllCharactersList;
    std::vector<SDK::ACharacterBattle*> g_CurrentTeamCharacters;
    ImFont* g_SymbolFont = nullptr;  // Font for menu icons

    // ============================================================================
    // GLOBAL MENU STATE
    // ============================================================================
    static int g_SelectedTab = 0;  // 0=ESP, 1=CHARACTER, 2=AIMBOT, 3=COMBAT, 4=HACKS, 5=LOBBY EXPLOIT, 6=SETTINGS
    
    static bool g_Initialized = false;
    static bool g_Visible = true;
    static bool g_LicenseSectionUnlocked = false;
    static bool g_LicenseUnlockFailed = false;
    static char g_LicenseUnlockBuffer[64] = {};
    static char g_LicenseKeyBuffer[64] = {};
    static bool g_MenuHotkeyDown = false;
    static bool g_PlayerNetworkTableVisible = false;
    static bool g_PlayerNetworkTableHotkeyDown = false;
    static constexpr int PLAYER_NETWORK_TABLE_HOTKEY = VK_F8;
    static bool g_PlayerListHotkeyHintVisible = false;
    static bool g_WasInValidBattleModeForHint = false;
    static DWORD g_LastBattleModeHintCheckTick = 0;
    static DWORD g_PlayerListHotkeyHintStartTick = 0;
    static constexpr DWORD PLAYER_LIST_HINT_CHECK_INTERVAL_MS = 3000;
    static constexpr DWORD PLAYER_LIST_HINT_DURATION_MS = 7000;
    static constexpr int PLATFORM_ICON_COUNT = 6;
    static ID3D11ShaderResourceView* g_PlatformIcons[PLATFORM_ICON_COUNT]{};
    static ImGuiContext* g_Context = nullptr;

    struct CharacterConditionOption
    {
        int id;
        const char* name;
        int defaultApplyMode;
        int defaultLevel;
        float defaultDuration;
        float defaultValue;
        float defaultInterval;
        int defaultSubLevel;
        bool defaultTimeOverwrite;
    };

    static constexpr CharacterConditionOption g_CharacterConditionOptions[] = {
        { 1, "POISON_MIST", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 2, "BURN", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 3, "FREEZE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 4, "DECAY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 5, "BLUE_FLAME", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 6, "THUNDER", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 7, "UNIQUE_SEAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 8, "NOISE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 9, "GRAB", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 10, "DECREASE_HEALTH", 1, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 11, "RECOVER_HEALTH", 1, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 12, "CONTINUOUS_RECOVER_HEALTH", 1, 0, 50.0f, 1.0f, 0.1f, 0, false },
        { 13, "INFLUENCE_OF_ALLY_ABILITY_HEAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 14, "INFLUENCE_OF_ALLY_ROLLSOLT_ABILITY_HEAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 15, "TRANSFORM_STOCKING", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 16, "TRANSFORM", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 17, "COPY_STOCKING", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 18, "COPY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 19, "DUPLICATE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 20, "DYING", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 21, "GUARD", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 22, "UNIQUE_LEVEL_UP", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 23, "REBUILD", 0, 0, 0.0f, 0.0f, 0.0f, 0, false },
        { 24, "REBUILD_MYSELF", 0, 0, 0.0f, 0.0f, 0.0f, 0, false },
        { 25, "STONE_BIND", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 26, "SPEED_DOWN", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 27, "SKILL_PIGGYBACK", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 28, "SKILL_PIGGYBACK_CH001", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 29, "SKILL_PIGGYBACK_CH005", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 30, "SKILL_LOWGRAVITY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 31, "SKILL_LOWGRAVITY_TO_ENEMY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 32, "SKILL_SPEEDUP", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 33, "SKILL_SPEEDDOWN", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 34, "SKILL_AHO", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 35, "UNBREAKABLE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 36, "SKILL_WEAR_BLUE_FLAME", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 37, "SKILL_PIGGYBACK_CH012", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 38, "GIANT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 39, "INFECTIONDECAY_SORCE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 40, "INFECTIONDECAY_BEFORE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 41, "INFECTIONDECAY_AFTER", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 42, "SKILL_STEAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 43, "ABILITY_ATTACK", 1, 1, 500.0f, 1000.0f, 0.1f, 1, false },
        { 44, "ABILITY_DURABLE", 1, 1, 500.0f, 1000.0f, 0.1f, 1, false },
        { 45, "ABILITY_MOVESPEED", 1, 1, 500.0f, 1000.0f, 0.1f, 1, false },
        { 46, "ABILITY_HEAL", 1, 1, 500.0f, 1000.0f, 0.1f, 1, false },
        { 47, "ABILITY_TECHNIQUE", 1, 1, 500.0f, 1000.0f, 0.1f, 1, false },
        { 48, "ROLLSLOT_ABILITY_ATTACK", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 49, "ROLLSLOT_ABILITY_DURABLE", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 50, "ROLLSLOT_ABILITY_MOVESPEED", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 51, "ROLLSLOT_ABILITY_HEAL", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 52, "ROLLSLOT_ABILITY_TECHNIQUE", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 53, "ROLLSLOT_WALLDASH", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 54, "ROLLSLOT_DECAY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 55, "SUPERARMOR_FOR_ATTACK", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 56, "SUPERARMOR_FOR_DAMAGE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 57, "SUPERARMOR_FOR_PLUS", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 58, "SUPERARMOR_FOR_ROLLSLOT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 59, "PLUS_ULTRA", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 60, "SHOW_INVINCIBLE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 61, "COMPRESSION", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 62, "COMPRESSION_REGENERATION", 1, 0, 500.0f, 1.0f, 0.1f, 0, false },
        { 63, "OPTICALDAZZLEPAINT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 64, "ROLLSLOT_OPTICALDAZZLEPAINT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 65, "CH024_TRANSPARENT", 1, 0, 500.0f, 5.0f, 0.01f, 0, false },
        { 66, "ROLLSLOT_INVINCIBLE", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 67, "ROLLSLOT_SPACTION_RELOAD_SPEED_ACCEL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 68, "TELEPORT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 69, "DISP_HEALTH_ON_HIT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 70, "DISP_GUARDPOINT_ON_HIT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 71, "DISP_LOCATION_ON_HIT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 72, "KING", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 73, "ORB_PENALTY", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 74, "LEADER", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 75, "RULE_HP_REGENERATION", 1, 0, 500.0f, 1.0f, 0.1f, 0, false },
        { 76, "RULE_GP_REGENERATION", 1, 0, 500.0f, 5.0f, 0.01f, 0, false },
        { 77, "SUPPROTROLE_REGENERATION", 1, 0, 500.0f, 1.0f, 0.1f, 0, false },
        { 78, "RULE_RESPAWNED", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 79, "BRAINWASH_SPEED_DOWN", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 80, "BRAINWASH_SPECIAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 81, "THUNDER_SPECIAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 82, "CH046_ARMOR", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 83, "CH104_STEEL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 84, "CH026_OPTICALDAZZLEPAINT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 85, "CH202_TRANS_MISSION", 0, 5, 0.0f, 0.0f, 0.0f, 5, false },
        { 86, "CH202_HAKKEI", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 87, "CH202_EFFECT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 88, "SPECIAL_SEAL", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 89, "ROLLSLOT_ACCELERATION", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 90, "CH024_TRANSPARENT_PERFECT", 1, 0, 500.0f, 5.0f, 0.01f, 0, false },
        { 91, "CH015_RECOVERY_HEALTH_GUARDPOINT", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 92, "CH015_DISP_LOCATION_ON_HIT_AIMSEARCH", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 93, "RollSlot_Ability_Ch015_RECOVERY_HEALTH_GUARDPOINT", 0, 1, 50.0f, 0.0f, 0.0f, 0, false },
        { 94, "CH025_WAVE_BARRIER", 0, 5, 50.0f, 50.0f, 0.1f, 5, false },
        { 95, "CH011_ABYSS_DARK_BODY", 0, 5, 50.0f, 50.0f, 0.1f, 5, false },
        { 96, "CH025_CONTINUOUS_RECOVER_HEALTH", 1, 0, 500.0f, 1.0f, 0.1f, 0, false },
        { 97, "STONE_BIND_V2", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 98, "REVERSE_POISON_MIST_RECOVER_HEALTH", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 99, "CH002V3_SWEAT_BOMB", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 100, "ROLLSLOT_THUNDER", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
        { 101, "CH111_SUPERARMOR", 0, 0, 50.0f, 0.0f, 0.0f, 0, false },
    };

    static bool FullWidthButton(const char* label, float height = 0.0f);
    static void SeparatorLabel(const char* label);
    static void DrawPlayerNameInlineControls();
    static void RenderPlayerNameSettingsCard(float width);

    static const CharacterConditionOption* FindCharacterConditionOption(int conditionId)
    {
        for (const auto& option : g_CharacterConditionOptions)
        {
            if (option.id == conditionId)
                return &option;
        }
        return nullptr;
    }

    static const CharacterConditionOption& GetDefaultCharacterConditionOption()
    {
        return *FindCharacterConditionOption(35);
    }

    static void LoadCharacterConditionDefaults(const CharacterConditionOption& option)
    {
        g_HackSettings.CharCondition_SelectedConditionId = option.id;
        g_HackSettings.CharCondition_ApplyMode = option.defaultApplyMode;
        g_HackSettings.CharCondition_Level = option.defaultLevel;
        g_HackSettings.CharCondition_Duration = option.defaultDuration;
        g_HackSettings.CharCondition_Value = option.defaultValue;
        g_HackSettings.CharCondition_Interval = option.defaultInterval;
        g_HackSettings.CharCondition_SubLevel = option.defaultSubLevel;
        g_HackSettings.CharCondition_TimeOverwrite = option.defaultTimeOverwrite;
    }

    static void NormalizeCharacterConditionEditor()
    {
        if (!FindCharacterConditionOption(g_HackSettings.CharCondition_SelectedConditionId))
            LoadCharacterConditionDefaults(GetDefaultCharacterConditionOption());

        g_HackSettings.CharCondition_ApplyMode = std::clamp(g_HackSettings.CharCondition_ApplyMode, 0, 2);
        if (g_HackSettings.CharCondition_Level < 0)
            g_HackSettings.CharCondition_Level = 0;
        if (g_HackSettings.CharCondition_Duration < 0.0f)
            g_HackSettings.CharCondition_Duration = 0.0f;
        if (g_HackSettings.CharCondition_Interval < 0.0f)
            g_HackSettings.CharCondition_Interval = 0.0f;
        if (g_HackSettings.CharCondition_SubLevel < 0)
            g_HackSettings.CharCondition_SubLevel = 0;
    }

    static void FormatCharacterConditionLabel(const CharacterConditionOption& option, char* buffer, size_t bufferSize)
    {
        if (!buffer || bufferSize == 0)
            return;

        snprintf(buffer, bufferSize, "%d - %s", option.id, option.name);
    }

    static void PrepareCharacterConditionField(const char* label)
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float labelWidth = availableWidth < 380.0f ? 120.0f : 180.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        if (availableWidth > labelWidth + spacing + 140.0f)
        {
            const float startX = ImGui::GetCursorPosX();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(startX + labelWidth);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            return;
        }

        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    }

    static bool EnqueueGameThreadMenuTask(std::function<void()> task, const char* label)
    {
        if (GameThreadHook::EnqueueTask(std::move(task)))
            return true;

        Logger::LogError(std::string("[Menu] Game thread hook not ready for ") + label);
        return false;
    }

    static void DrawCharacterConditionCombo()
    {
        const CharacterConditionOption* selectedOption = FindCharacterConditionOption(g_HackSettings.CharCondition_SelectedConditionId);
        if (!selectedOption)
            selectedOption = &GetDefaultCharacterConditionOption();

        char currentLabel[128] = {};
        FormatCharacterConditionLabel(*selectedOption, currentLabel, sizeof(currentLabel));
        PrepareCharacterConditionField("Condition");
        if (ImGui::BeginCombo("##CharacterCondition", currentLabel, ImGuiComboFlags_HeightLarge))
        {
            for (const auto& option : g_CharacterConditionOptions)
            {
                const bool selected = g_HackSettings.CharCondition_SelectedConditionId == option.id;
                char label[128] = {};
                FormatCharacterConditionLabel(option, label, sizeof(label));
                if (ImGui::Selectable(label, selected))
                    LoadCharacterConditionDefaults(option);

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void DrawCharacterConditionApplyModeCombo()
    {
        static const char* kApplyModes[] = {
            "Server - SetCondition_ToServer",
            "BP - BP_SetCondition",
            "Local - BP_SetConditionLocal",
        };

        g_HackSettings.CharCondition_ApplyMode = std::clamp(g_HackSettings.CharCondition_ApplyMode, 0, 2);
        const char* currentMode = kApplyModes[g_HackSettings.CharCondition_ApplyMode];

        PrepareCharacterConditionField("Apply Mode");
        if (ImGui::BeginCombo("##CharacterConditionApplyMode", currentMode))
        {
            for (int i = 0; i < 3; i++)
            {
                const bool selected = g_HackSettings.CharCondition_ApplyMode == i;
                if (ImGui::Selectable(kApplyModes[i], selected))
                    g_HackSettings.CharCondition_ApplyMode = i;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    static void RenderCharacterConditionEditor()
    {
        NormalizeCharacterConditionEditor();

        ImAdd::CheckBox("Auto Execute On Battle Entry", &g_HackSettings.CharCondition_AutoExecute);
        DrawCharacterConditionCombo();
        DrawCharacterConditionApplyModeCombo();

        PrepareCharacterConditionField("Level");
        ImGui::InputInt("##CharacterConditionLevel", &g_HackSettings.CharCondition_Level);
        PrepareCharacterConditionField("Duration");
        ImGui::InputFloat("##CharacterConditionDuration", &g_HackSettings.CharCondition_Duration, 1.0f, 10.0f, "%.2f");
        PrepareCharacterConditionField("Value");
        ImGui::InputFloat("##CharacterConditionValue", &g_HackSettings.CharCondition_Value, 1.0f, 10.0f, "%.2f");
        PrepareCharacterConditionField("Interval");
        ImGui::InputFloat("##CharacterConditionInterval", &g_HackSettings.CharCondition_Interval, 0.01f, 0.1f, "%.3f");
        PrepareCharacterConditionField("Sub Level");
        ImGui::InputInt("##CharacterConditionSubLevel", &g_HackSettings.CharCondition_SubLevel);

        if (g_HackSettings.CharCondition_ApplyMode == 0)
            ImAdd::CheckBox("Overwrite Time", &g_HackSettings.CharCondition_TimeOverwrite);

        NormalizeCharacterConditionEditor();

        if (FullWidthButton("APPLY CONDITION"))
        {
            const int conditionId = g_HackSettings.CharCondition_SelectedConditionId;
            const int applyMode = g_HackSettings.CharCondition_ApplyMode;
            const int level = g_HackSettings.CharCondition_Level;
            const float duration = g_HackSettings.CharCondition_Duration;
            const float value = g_HackSettings.CharCondition_Value;
            const float interval = g_HackSettings.CharCondition_Interval;
            const int subLevel = g_HackSettings.CharCondition_SubLevel;
            const bool timeOverwrite = g_HackSettings.CharCondition_TimeOverwrite;
            EnqueueGameThreadMenuTask([conditionId, applyMode, level, duration, value, interval, subLevel, timeOverwrite]() {
                InGameHack_ApplyCustomCharacterCondition(conditionId, applyMode, level, duration, value, interval, subLevel, timeOverwrite);
            }, "Apply Condition");
        }

        if (FullWidthButton("CLEAR CONDITION (TIME_UP)"))
        {
            const int conditionId = g_HackSettings.CharCondition_SelectedConditionId;
            EnqueueGameThreadMenuTask([conditionId]() {
                InGameHack_ClearCustomCharacterCondition(conditionId);
            }, "Clear Condition");
        }
    }
    static HWND g_GameWindow = nullptr;
    static HMODULE g_DllModule = nullptr;
    static WNDPROC g_OriginalWndProc = nullptr;
    static bool g_WndProcRestored = false;
    static std::atomic<int> g_WndProcCallDepth(0);
    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_DeviceContext = nullptr;
    static ID3D11RenderTargetView* g_RenderTargetView = nullptr;
    static IMFSourceReader* g_MenuVideoReader = nullptr;
    static ID3D11Texture2D* g_MenuVideoTexture = nullptr;
    static ID3D11ShaderResourceView* g_MenuVideoSRV = nullptr;
    static UINT g_MenuVideoWidth = 0;
    static UINT g_MenuVideoHeight = 0;
    static UINT g_MenuVideoSourceWidth = 0;
    static UINT g_MenuVideoSourceHeight = 0;
    static LONG g_MenuVideoStride = 0;
    static LONGLONG g_MenuVideoDuration = 0;
    static double g_MenuVideoLastFrameTime = 0.0;
    static HRESULT g_MenuVideoLastHr = S_OK;
    static DWORD g_MenuVideoLastFlags = 0;
    static DWORD g_MenuVideoLastWin32Error = ERROR_SUCCESS;
    static LONG g_MenuVideoTempSerial = 0;
    static UINT g_MenuVideoFrameCount = 0;
    static UINT g_MenuVideoNoSampleCount = 0;
    static UINT g_MenuVideoBufferBytes = 0;
    static bool g_MenuVideoFrameCopied = false;
    static bool g_MenuVideoComInitialized = false;
    static bool g_MenuVideoMfStarted = false;
    static bool g_MenuVideoReady = false;
    static bool g_MenuVideoRotateToLandscape = false;
    static const char* g_MenuVideoStatus = "idle";
    static std::wstring g_MenuVideoTempPath;
    static std::vector<unsigned char> g_MenuVideoEmbeddedBytes;
    static constexpr double MENU_VIDEO_FRAME_INTERVAL = 1.0 / 30.0;
    static ImFont* g_FreeFont = nullptr;
    static ImFont* g_FreeFontLarge = nullptr;
    static ImFont* g_FreeFontTitle = nullptr;
    static ImFont* g_FreeFontSmall = nullptr;
    static ImFont* g_FreeFontBrand = nullptr;
    static ImFont* g_FreeIconFont = nullptr;

    // Profile Management
    static std::string g_CurrentProfileName = "Default";
    static char g_ProfileNameBuffer[256] = "Default";
    static std::vector<std::string> g_ProfilesList;

    // Section open/close states
    static bool g_ESP_DisplayOpen = true;
    static bool g_ESP_FiltersOpen = true;
    
    // Hotkey listening state
    static bool g_ListeningForHotkey = false;
    static bool g_HotkeyWaitingForRelease = false;
    static ULONGLONG g_HotkeyListenStartTime = 0;
    static int g_CurrentHotkeyValue = 0;  // Temporary value being set
    static constexpr ULONGLONG HOTKEY_LISTEN_TIMEOUT_MS = 5000;
    static constexpr int XINPUT_LT_HOTKEY = 0x1F00;
    static constexpr int XINPUT_RT_HOTKEY = 0x2F00;
    static constexpr int XINPUT_LEFT_STICK_UP_HOTKEY = 0x3001;
    static constexpr int XINPUT_LEFT_STICK_DOWN_HOTKEY = 0x3002;
    static constexpr int XINPUT_LEFT_STICK_LEFT_HOTKEY = 0x3003;
    static constexpr int XINPUT_LEFT_STICK_RIGHT_HOTKEY = 0x3004;
    static constexpr int XINPUT_RIGHT_STICK_UP_HOTKEY = 0x4001;
    static constexpr int XINPUT_RIGHT_STICK_DOWN_HOTKEY = 0x4002;
    static constexpr int XINPUT_RIGHT_STICK_LEFT_HOTKEY = 0x4003;
    static constexpr int XINPUT_RIGHT_STICK_RIGHT_HOTKEY = 0x4004;
    static constexpr BYTE XINPUT_HOTKEY_TRIGGER_THRESHOLD = 128;
    static constexpr SHORT XINPUT_HOTKEY_STICK_THRESHOLD = 20000;

    struct GamepadButtonHotkey
    {
        int code;
        WORD mask;
        const char* xboxName;
        const char* psName;
    };

    static constexpr GamepadButtonHotkey GAMEPAD_BUTTON_HOTKEYS[] = {
        { XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP, "D-Pad Up", "D-Pad Up" },
        { XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN, "D-Pad Down", "D-Pad Down" },
        { XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT, "D-Pad Left", "D-Pad Left" },
        { XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT, "D-Pad Right", "D-Pad Right" },
        { XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_START, "Start", "Options" },
        { XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_BACK, "Back", "Share" },
        { XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_LEFT_THUMB, "L3", "L3" },
        { XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB, "R3", "R3" },
        { XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER, "LB", "L1" },
        { XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB", "R1" },
        { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_A, "A", "Cross" },
        { XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_B, "B", "Circle" },
        { XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_X, "X", "Square" },
        { XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_Y, "Y", "Triangle" }
    };

    // ============================================================================
    // HOTKEY LISTENER FUNCTION
    // ============================================================================
    /**
     * @brief Listen for hotkey input (keyboard or gamepad) for 5 seconds
     * Returns 0 if timeout or ESC pressed, otherwise returns the key code
     */
    static bool IsKnownHotkeyId(int hotkeyId)
    {
        return hotkeyId >= 100 && hotkeyId <= 109;
    }

    static std::string GetWindowsKeyName(int keyCode)
    {
        if (keyCode <= 0 || keyCode > 0xFF)
            return {};

        switch (keyCode)
        {
            case VK_LBUTTON: return "Left Mouse";
            case VK_RBUTTON: return "Right Mouse";
            case VK_MBUTTON: return "Middle Mouse";
            case VK_XBUTTON1: return "Mouse 4";
            case VK_XBUTTON2: return "Mouse 5";
            default: break;
        }

        UINT scanCode = MapVirtualKeyA(static_cast<UINT>(keyCode), MAPVK_VK_TO_VSC_EX);
        if (scanCode == 0)
            return {};

        switch (keyCode)
        {
            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_PRIOR:
            case VK_NEXT:
            case VK_END:
            case VK_HOME:
            case VK_INSERT:
            case VK_DELETE:
            case VK_DIVIDE:
            case VK_NUMLOCK:
                scanCode |= 0x100;
                break;
            default:
                break;
        }

        char name[128] = {};
        LONG lParam = static_cast<LONG>(scanCode << 16);
        if (GetKeyNameTextA(lParam, name, sizeof(name)) <= 0)
            return {};

        return name;
    }

    static bool IsValidKeyboardHotkey(int keyCode)
    {
        return keyCode >= 0x01 &&
               keyCode <= 0xFF &&
               keyCode != VK_ESCAPE &&
               !GetWindowsKeyName(keyCode).empty();
    }

    static bool IsValidGamepadHotkey(int keyCode)
    {
        for (const GamepadButtonHotkey& hotkey : GAMEPAD_BUTTON_HOTKEYS)
        {
            if (keyCode == hotkey.code)
                return true;
        }

        switch (keyCode)
        {
            case XINPUT_LT_HOTKEY:
            case XINPUT_RT_HOTKEY:
            case XINPUT_LEFT_STICK_UP_HOTKEY:
            case XINPUT_LEFT_STICK_DOWN_HOTKEY:
            case XINPUT_LEFT_STICK_LEFT_HOTKEY:
            case XINPUT_LEFT_STICK_RIGHT_HOTKEY:
            case XINPUT_RIGHT_STICK_UP_HOTKEY:
            case XINPUT_RIGHT_STICK_DOWN_HOTKEY:
            case XINPUT_RIGHT_STICK_LEFT_HOTKEY:
            case XINPUT_RIGHT_STICK_RIGHT_HOTKEY:
                return true;
            default:
                return false;
        }
    }

    static void StopHotkeyListening()
    {
        g_ListeningForHotkey = false;
        g_HotkeyWaitingForRelease = false;
        g_CurrentHotkeyValue = 0;
        g_HotkeyListenStartTime = 0;
    }

    static void StartHotkeyListening(int hotkeyId)
    {
        if (!IsKnownHotkeyId(hotkeyId))
        {
            StopHotkeyListening();
            return;
        }

        g_ListeningForHotkey = true;
        g_HotkeyWaitingForRelease = true;
        g_HotkeyListenStartTime = GetTickCount64();
        g_CurrentHotkeyValue = hotkeyId;
    }

    static int GetHotkey(int inputType)
    {
        // Keyboard/Mouse input type (0)
        if (inputType == 0)
        {
            // Scan all virtual key codes (0x01-0xFF)
            for (int vk = 0x01; vk <= 0xFF; vk++)
            {
                // Skip ESC (27) - reserved for cancel
                if (vk == 0x1B) continue;
                
                if ((GetAsyncKeyState(vk) & 0x8000) != 0)
                {
                    return vk;  // Return the pressed key code
                }
            }
        }
        // Gamepad input type (1)
        else
        {
            XINPUT_STATE xInputState = {};
            
            // Check all 4 gamepad slots
            for (DWORD i = 0; i < 4; i++)
            {
                if (XInputGetState(i, &xInputState) == ERROR_SUCCESS)
                {
                    for (const GamepadButtonHotkey& hotkey : GAMEPAD_BUTTON_HOTKEYS)
                    {
                        if ((xInputState.Gamepad.wButtons & hotkey.mask) != 0)
                        {
                            return hotkey.code;
                        }
                    }

                    if (xInputState.Gamepad.bLeftTrigger > XINPUT_HOTKEY_TRIGGER_THRESHOLD)
                    {
                        return XINPUT_LT_HOTKEY;
                    }
                    
                    if (xInputState.Gamepad.bRightTrigger > XINPUT_HOTKEY_TRIGGER_THRESHOLD)
                    {
                        return XINPUT_RT_HOTKEY;
                    }

                    if (xInputState.Gamepad.sThumbLY > XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_LEFT_STICK_UP_HOTKEY;
                    if (xInputState.Gamepad.sThumbLY < -XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_LEFT_STICK_DOWN_HOTKEY;
                    if (xInputState.Gamepad.sThumbLX < -XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_LEFT_STICK_LEFT_HOTKEY;
                    if (xInputState.Gamepad.sThumbLX > XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_LEFT_STICK_RIGHT_HOTKEY;

                    if (xInputState.Gamepad.sThumbRY > XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_RIGHT_STICK_UP_HOTKEY;
                    if (xInputState.Gamepad.sThumbRY < -XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_RIGHT_STICK_DOWN_HOTKEY;
                    if (xInputState.Gamepad.sThumbRX < -XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_RIGHT_STICK_LEFT_HOTKEY;
                    if (xInputState.Gamepad.sThumbRX > XINPUT_HOTKEY_STICK_THRESHOLD)
                        return XINPUT_RIGHT_STICK_RIGHT_HOTKEY;
                }
            }
        }
        
        return 0;  // No input detected
    }

    /**
     * @brief Convert key code to human-readable name
     * Supports keyboard and gamepad input types
     */
    static std::string GetKeyName(int keyCode, int inputType)
    {
        if (inputType == 0)  // HotKeyType::KeyboardMouse
        {
            switch (keyCode)
            {
                case 0: return "None";
                case VK_LBUTTON: return "Left Mouse";
                case VK_RBUTTON: return "Right Mouse";
                case VK_MBUTTON: return "Middle Mouse";
                case VK_XBUTTON1: return "Mouse 4";
                case VK_XBUTTON2: return "Mouse 5";
                
                // Function keys
                case 0x70: return "F1";
                case 0x71: return "F2";
                case 0x72: return "F3";
                case 0x73: return "F4";
                case 0x74: return "F5";
                case 0x75: return "F6";
                case 0x76: return "F7";
                case 0x77: return "F8";
                case 0x78: return "F9";
                case 0x79: return "F10";
                case 0x7A: return "F11";
                case 0x7B: return "F12";
                
                // Letters
                case 'A': return "A";
                case 'B': return "B";
                case 'C': return "C";
                case 'D': return "D";
                case 'E': return "E";
                case 'F': return "F";
                case 'G': return "G";
                case 'H': return "H";
                case 'I': return "I";
                case 'J': return "J";
                case 'K': return "K";
                case 'L': return "L";
                case 'M': return "M";
                case 'N': return "N";
                case 'O': return "O";
                case 'P': return "P";
                case 'Q': return "Q";
                case 'R': return "R";
                case 'S': return "S";
                case 'T': return "T";
                case 'U': return "U";
                case 'V': return "V";
                case 'W': return "W";
                case 'X': return "X";
                case 'Y': return "Y";
                case 'Z': return "Z";
                
                // Numbers
                case '0': return "0";
                case '1': return "1";
                case '2': return "2";
                case '3': return "3";
                case '4': return "4";
                case '5': return "5";
                case '6': return "6";
                case '7': return "7";
                case '8': return "8";
                case '9': return "9";
                
                // Special keys
                case 0x20: return "Space";
                case 0x1B: return "ESC";
                case 0x09: return "Tab";
                case 0x10: return "Shift";
                case 0x11: return "Ctrl";
                case 0x12: return "Alt";
                case 0x0D: return "Enter";
                case 0x2E: return "Delete";
                case 0x2D: return "Insert";
                case 0x23: return "End";
                case 0x24: return "Home";
                case 0x21: return "PgUp";
                case 0x22: return "PgDn";
                case 0x25: return "Left Arrow";
                case 0x26: return "Up Arrow";
                case 0x27: return "Right Arrow";
                case 0x28: return "Down Arrow";
                
                default:
                {
                    std::string keyName = GetWindowsKeyName(keyCode);
                    return keyName.empty() ? "Unassigned" : keyName;
                }
            }
        }
        else if (inputType == 1 || inputType == 2)  // HotKeyType::Gamepad
        {
            const bool usePlayStationNames = (inputType == 2);

            if (keyCode == 0)
                return "None";

            for (const GamepadButtonHotkey& hotkey : GAMEPAD_BUTTON_HOTKEYS)
            {
                if (keyCode == hotkey.code)
                    return usePlayStationNames ? hotkey.psName : hotkey.xboxName;
            }

            switch (keyCode)
            {
                case XINPUT_LT_HOTKEY: return usePlayStationNames ? "L2" : "LT";
                case XINPUT_RT_HOTKEY: return usePlayStationNames ? "R2" : "RT";
                case XINPUT_LEFT_STICK_UP_HOTKEY: return "Left Stick Up";
                case XINPUT_LEFT_STICK_DOWN_HOTKEY: return "Left Stick Down";
                case XINPUT_LEFT_STICK_LEFT_HOTKEY: return "Left Stick Left";
                case XINPUT_LEFT_STICK_RIGHT_HOTKEY: return "Left Stick Right";
                case XINPUT_RIGHT_STICK_UP_HOTKEY: return "Right Stick Up";
                case XINPUT_RIGHT_STICK_DOWN_HOTKEY: return "Right Stick Down";
                case XINPUT_RIGHT_STICK_LEFT_HOTKEY: return "Right Stick Left";
                case XINPUT_RIGHT_STICK_RIGHT_HOTKEY: return "Right Stick Right";
                default: return "Unassigned";
            }
        }
        
        return "Unknown";
    }
    
    // ============================================================================
    // COLOR PALETTE
    // ============================================================================
    struct ColorPalette
    {
        // Backgrounds
        ImVec4 bgDarkest = ImVec4(0.012f, 0.010f, 0.020f, 1.0f);
        ImVec4 bgDark = ImVec4(0.026f, 0.022f, 0.040f, 0.98f);
        ImVec4 bgMedium = ImVec4(0.050f, 0.042f, 0.082f, 1.0f);
        ImVec4 bgLight = ImVec4(0.095f, 0.078f, 0.145f, 1.0f);

        // Accent colors
        ImVec4 accentColor = ImVec4(0.615f, 0.300f, 1.000f, 1.0f);
        ImVec4 accentColorHover = ImVec4(0.735f, 0.420f, 1.000f, 1.0f);
        ImVec4 accentColorActive = ImVec4(0.435f, 0.145f, 0.850f, 1.0f);

        // Status colors
        ImVec4 success = ImVec4(0.250f, 1.000f, 0.520f, 1.0f);
        ImVec4 warning = ImVec4(1.000f, 0.760f, 0.220f, 1.0f);
        ImVec4 danger = ImVec4(1.000f, 0.170f, 0.420f, 1.0f);

        // Text colors
        ImVec4 textPrimary = ImVec4(0.955f, 0.940f, 1.000f, 1.0f);
        ImVec4 textSecondary = ImVec4(0.690f, 0.635f, 0.785f, 1.0f);
        ImVec4 textDisabled = ImVec4(0.405f, 0.365f, 0.490f, 1.0f);

        // Legacy accents (for compatibility)
        ImVec4 primaryBlue = ImVec4(0.220f, 0.430f, 1.000f, 1.0f);
        ImVec4 accentCyan = accentColor;
        ImVec4 accentMagenta = ImVec4(1.000f, 0.180f, 0.760f, 1.0f);
        ImVec4 accentYellow = warning;
        ImVec4 accentOrange = ImVec4(1.000f, 0.430f, 0.160f, 1.0f);
        ImVec4 accentPurple = accentColor;
        ImVec4 accentGreen = success;
        ImVec4 accentAqua = ImVec4(0.110f, 1.000f, 0.820f, 1.0f);
    };
    static ColorPalette g_Colors;

    struct PreviewMenuTheme
    {
        const char* name;
        ImVec4 windowBg;
        ImVec4 cardBg;
        ImVec4 surfaceBg;
        ImVec4 surfaceHover;
        ImVec4 surfaceActive;
        ImVec4 border;
        ImVec4 accent;
        ImVec4 accentHover;
        ImVec4 accentActive;
        ImVec4 accentSecondary;
        ImVec4 text;
        ImVec4 textSecondary;
        ImVec4 textDisabled;
    };

    static ImVec4 AlphaColor(ImVec4 color, float alpha)
    {
        color.w = alpha;
        return color;
    }

    static ImVec4 LerpColor(const ImVec4& from, const ImVec4& to, float t, float alpha)
    {
        return ImVec4(
            from.x + (to.x - from.x) * t,
            from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t,
            alpha
        );
    }

    static const PreviewMenuTheme g_PreviewThemes[] =
    {
        {
            "Factory Blue",
            ImVec4(0.039f, 0.027f, 0.031f, 1.0f),
            ImVec4(0.071f, 0.059f, 0.067f, 1.0f),
            ImVec4(0.102f, 0.086f, 0.090f, 1.0f),
            ImVec4(0.134f, 0.118f, 0.122f, 1.0f),
            ImVec4(0.082f, 0.071f, 0.075f, 1.0f),
            ImVec4(0.134f, 0.110f, 0.118f, 1.0f),
            ImVec4(0.341f, 0.745f, 0.918f, 1.0f),
            ImVec4(0.169f, 0.659f, 0.878f, 1.0f),
            ImVec4(0.000f, 0.478f, 0.784f, 1.0f),
            ImVec4(0.565f, 0.365f, 1.000f, 1.0f),
            ImVec4(0.855f, 0.855f, 0.855f, 1.0f),
            ImVec4(0.660f, 0.690f, 0.710f, 1.0f),
            ImVec4(0.333f, 0.333f, 0.333f, 1.0f)
        },
        {
            "Rugir Violet",
            ImVec4(0.031f, 0.027f, 0.067f, 1.0f),
            ImVec4(0.071f, 0.059f, 0.114f, 1.0f),
            ImVec4(0.102f, 0.086f, 0.157f, 1.0f),
            ImVec4(0.149f, 0.122f, 0.212f, 1.0f),
            ImVec4(0.086f, 0.071f, 0.129f, 1.0f),
            ImVec4(0.173f, 0.130f, 0.285f, 1.0f),
            ImVec4(0.615f, 0.302f, 1.000f, 1.0f),
            ImVec4(0.745f, 0.471f, 1.000f, 1.0f),
            ImVec4(0.435f, 0.145f, 0.850f, 1.0f),
            ImVec4(1.000f, 0.180f, 0.760f, 1.0f),
            ImVec4(0.955f, 0.940f, 1.000f, 1.0f),
            ImVec4(0.700f, 0.650f, 0.820f, 1.0f),
            ImVec4(0.405f, 0.365f, 0.490f, 1.0f)
        },
        {
            "Viridian",
            ImVec4(0.024f, 0.055f, 0.051f, 1.0f),
            ImVec4(0.051f, 0.094f, 0.086f, 1.0f),
            ImVec4(0.063f, 0.133f, 0.122f, 1.0f),
            ImVec4(0.090f, 0.188f, 0.169f, 1.0f),
            ImVec4(0.051f, 0.110f, 0.098f, 1.0f),
            ImVec4(0.086f, 0.220f, 0.196f, 1.0f),
            ImVec4(0.224f, 0.902f, 0.694f, 1.0f),
            ImVec4(0.396f, 1.000f, 0.816f, 1.0f),
            ImVec4(0.067f, 0.663f, 0.506f, 1.0f),
            ImVec4(0.325f, 0.647f, 1.000f, 1.0f),
            ImVec4(0.900f, 1.000f, 0.965f, 1.0f),
            ImVec4(0.640f, 0.775f, 0.735f, 1.0f),
            ImVec4(0.365f, 0.480f, 0.445f, 1.0f)
        },
        {
            "Crimson",
            ImVec4(0.071f, 0.024f, 0.027f, 1.0f),
            ImVec4(0.102f, 0.051f, 0.059f, 1.0f),
            ImVec4(0.141f, 0.075f, 0.086f, 1.0f),
            ImVec4(0.196f, 0.102f, 0.114f, 1.0f),
            ImVec4(0.110f, 0.055f, 0.067f, 1.0f),
            ImVec4(0.240f, 0.105f, 0.130f, 1.0f),
            ImVec4(1.000f, 0.302f, 0.451f, 1.0f),
            ImVec4(1.000f, 0.478f, 0.596f, 1.0f),
            ImVec4(0.780f, 0.122f, 0.282f, 1.0f),
            ImVec4(1.000f, 0.710f, 0.278f, 1.0f),
            ImVec4(1.000f, 0.930f, 0.940f, 1.0f),
            ImVec4(0.805f, 0.640f, 0.665f, 1.0f),
            ImVec4(0.520f, 0.360f, 0.385f, 1.0f)
        },
        {
            "Solar",
            ImVec4(0.063f, 0.047f, 0.020f, 1.0f),
            ImVec4(0.094f, 0.071f, 0.035f, 1.0f),
            ImVec4(0.141f, 0.106f, 0.051f, 1.0f),
            ImVec4(0.200f, 0.145f, 0.075f, 1.0f),
            ImVec4(0.114f, 0.086f, 0.039f, 1.0f),
            ImVec4(0.255f, 0.188f, 0.082f, 1.0f),
            ImVec4(1.000f, 0.784f, 0.341f, 1.0f),
            ImVec4(1.000f, 0.871f, 0.490f, 1.0f),
            ImVec4(0.812f, 0.561f, 0.122f, 1.0f),
            ImVec4(0.255f, 0.839f, 1.000f, 1.0f),
            ImVec4(1.000f, 0.965f, 0.875f, 1.0f),
            ImVec4(0.810f, 0.725f, 0.590f, 1.0f),
            ImVec4(0.520f, 0.440f, 0.315f, 1.0f)
        }
    };

    static int g_SelectedPreviewTheme = 0;

    static int GetPreviewThemeCount()
    {
        return (int)(sizeof(g_PreviewThemes) / sizeof(g_PreviewThemes[0]));
    }

    static const PreviewMenuTheme& GetActivePreviewTheme()
    {
        int themeCount = GetPreviewThemeCount();
        if (g_SelectedPreviewTheme < 0 || g_SelectedPreviewTheme >= themeCount)
            g_SelectedPreviewTheme = 0;

        return g_PreviewThemes[g_SelectedPreviewTheme];
    }

    static void ApplyPreviewThemeToMenuColors(const PreviewMenuTheme& theme)
    {
        g_Colors.bgDarkest = theme.windowBg;
        g_Colors.bgDark = AlphaColor(theme.cardBg, 0.98f);
        g_Colors.bgMedium = theme.surfaceBg;
        g_Colors.bgLight = theme.surfaceHover;

        g_Colors.accentColor = theme.accent;
        g_Colors.accentColorHover = theme.accentHover;
        g_Colors.accentColorActive = theme.accentActive;

        g_Colors.textPrimary = theme.text;
        g_Colors.textSecondary = theme.textSecondary;
        g_Colors.textDisabled = theme.textDisabled;

        g_Colors.primaryBlue = theme.accentActive;
        g_Colors.accentCyan = theme.accentHover;
        g_Colors.accentMagenta = theme.accentSecondary;
        g_Colors.accentPurple = theme.accent;
        g_Colors.accentAqua = theme.accentHover;
    }

    static void ReleaseRenderTargetView()
    {
        if (g_RenderTargetView)
        {
            g_RenderTargetView->Release();
            g_RenderTargetView = nullptr;
        }
    }

    static bool EnsureRenderTargetView(IDXGISwapChain* pSwapChain, ID3D11Device* device)
    {
        if (g_RenderTargetView)
            return true;

        ID3D11Texture2D* backbuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))))
            return false;

        HRESULT hr = device->CreateRenderTargetView(backbuffer, nullptr, &g_RenderTargetView);
        backbuffer->Release();

        if (FAILED(hr))
        {
            g_RenderTargetView = nullptr;
            return false;
        }

        return true;
    }

    // ============================================================================
    // WINDOW PROCEDURE - INPUT HANDLING
    // ============================================================================
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        struct WndProcScope
        {
            WndProcScope()
            {
                g_WndProcCallDepth.fetch_add(1, std::memory_order_acq_rel);
            }

            ~WndProcScope()
            {
                g_WndProcCallDepth.fetch_sub(1, std::memory_order_acq_rel);
            }
        } wndProcScope;

        if (UnloadManager::IsUnloadRequested())
        {
            if (g_OriginalWndProc)
                return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);

            return DefWindowProc(hWnd, msg, wParam, lParam);
        }

        // Toggle menu with INSERT key
        if (msg == WM_KEYDOWN && wParam == VK_INSERT)
        {
            if (!g_MenuHotkeyDown)
            {
                g_Visible = !g_Visible;
                g_MenuHotkeyDown = true;
            }
            return true;
        }

        if (msg == WM_KEYUP && wParam == VK_INSERT)
        {
            g_MenuHotkeyDown = false;
            return true;
        }

        if (msg == WM_KEYDOWN && wParam == PLAYER_NETWORK_TABLE_HOTKEY)
        {
            if (!g_ListeningForHotkey && !g_PlayerNetworkTableHotkeyDown)
            {
                g_PlayerNetworkTableVisible = !g_PlayerNetworkTableVisible;
                g_PlayerNetworkTableHotkeyDown = true;
                g_PlayerListHotkeyHintVisible = false;
            }
            return true;
        }

        if (msg == WM_KEYUP && wParam == PLAYER_NETWORK_TABLE_HOTKEY)
        {
            g_PlayerNetworkTableHotkeyDown = false;
            return true;
        }

        // Handle ImGui input
        if (ImGui::GetCurrentContext() != nullptr && (g_Visible || g_PlayerNetworkTableVisible))
        {
            // First pass the message to ImGui_ImplWin32_WndProcHandler for proper input handling
            ImGuiIO& io = ImGui::GetIO();

            // Let ImGui process the message first (for text input, keyboard, etc.)
            if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
                return true;

            // Then handle our custom logic
            switch (msg)
            {
            case WM_MOUSEMOVE:
                io.MousePos = ImVec2((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
                break;
            case WM_LBUTTONDOWN:
                io.MouseDown[0] = true;
                break;
            case WM_LBUTTONUP:
                io.MouseDown[0] = false;
                break;
            case WM_RBUTTONDOWN:
                io.MouseDown[1] = true;
                break;
            case WM_RBUTTONUP:
                io.MouseDown[1] = false;
                break;
            case WM_MOUSEWHEEL:
                io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1.0f : -1.0f;
                break;
            case WM_KEYDOWN:
            case WM_KEYUP:
                if (io.WantCaptureKeyboard)
                {
                    return true;
                }
                break;
            }

            if (io.WantCaptureMouse || io.WantCaptureKeyboard)
            {
                return true;
            }
        }

        if (g_OriginalWndProc)
            return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // ============================================================================
    // STYLING & COLOR SETUP - MODERNE MAUVE THEME
    // ============================================================================

    static std::string GetDllDirectoryPath()
    {
        char dllPath[MAX_PATH] = { 0 };
        HMODULE hModule = NULL;

        if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetDllDirectoryPath,
            &hModule))
        {
            return {};
        }

        if (!GetModuleFileNameA(hModule, dllPath, MAX_PATH))
            return {};

        char* lastSlash = strrchr(dllPath, '\\');
        if (!lastSlash)
            return {};

        *lastSlash = '\0';
        return dllPath;
    }

    static std::string JoinPath(const std::string& base, const char* relativePath)
    {
        if (base.empty())
            return relativePath ? relativePath : "";

        std::string result = base;
        if (!result.empty() && result.back() != '\\' && relativePath && relativePath[0] != '\\')
            result += "\\";
        result += relativePath ? relativePath : "";
        return result;
    }

    static const char* GetAssetFileName(const char* relativePath)
    {
        if (!relativePath)
            return "";

        const char* lastBackslash = strrchr(relativePath, '\\');
        const char* lastSlash = strrchr(relativePath, '/');
        const char* lastSeparator = lastBackslash > lastSlash ? lastBackslash : lastSlash;
        return lastSeparator ? lastSeparator + 1 : relativePath;
    }

    static std::string BuildProjectAssetPath(const char* relativePath)
    {
        std::string root = IconLoader::CharacterIconCache::GetProjectRootPath();
        if (root.empty())
            return relativePath ? relativePath : "";

        return JoinPath(root, relativePath);
    }

    static std::string BuildDllAssetPath(const char* relativePath)
    {
        return JoinPath(JoinPath(GetDllDirectoryPath(), "Assets"), GetAssetFileName(relativePath));
    }

    static std::string BuildDllDirectAssetPath(const char* relativePath)
    {
        return JoinPath(GetDllDirectoryPath(), GetAssetFileName(relativePath));
    }

    static void LoadRugirLogo()
    {
        if (g_RugirLogo)
            return;

        g_RugirLogo = IconLoader::CharacterIconCache::LoadEmbeddedImageExternal(
            EmbeddedImages::FindAsset(EmbeddedImages::AssetId::Rugir));
        if (g_RugirLogo)
        {
            Logger::LogInfo("[Menu] Loaded embedded logo bytes.");
            return;
        }

        const char* logoAssetPath = "src\\Assets\\rugir.png";
        std::string dllLogo = BuildDllAssetPath(logoAssetPath);
        std::string dllDirectLogo = BuildDllDirectAssetPath(logoAssetPath);
        std::string projectLogo = BuildProjectAssetPath(logoAssetPath);
        const std::string logoPaths[] = {
            dllLogo,
            dllDirectLogo,
            projectLogo,
            logoAssetPath,
            "C:\\Users\\Shayne\\Downloads\\rugir.png"
        };

        for (const std::string& path : logoPaths)
        {
            if (path.empty())
                continue;

            g_RugirLogo = IconLoader::CharacterIconCache::LoadPNGFromFileExternal(path.c_str());
            if (g_RugirLogo)
            {
                Logger::LogInfo("[Menu] Loaded logo: " + path);
                return;
            }
        }

        Logger::LogWarning("[Menu] Logo image not found or not decodable. Using drawn fallback.");
    }

    static ID3D11ShaderResourceView* LoadAssetTexture(
        const char* relativePath,
        EmbeddedImages::AssetId embeddedAssetId = EmbeddedImages::AssetId::None)
    {
        if (!relativePath || !relativePath[0])
            return nullptr;

        if (embeddedAssetId != EmbeddedImages::AssetId::None)
        {
            ID3D11ShaderResourceView* embeddedTexture = IconLoader::CharacterIconCache::LoadEmbeddedImageExternal(
                EmbeddedImages::FindAsset(embeddedAssetId));
            if (embeddedTexture)
            {
                Logger::LogInfo(std::string("[Menu] Loaded embedded asset bytes: ") + relativePath);
                return embeddedTexture;
            }
        }

        std::string dllAssetPath = BuildDllAssetPath(relativePath);
        std::string dllDirectPath = BuildDllDirectAssetPath(relativePath);
        std::string projectPath = BuildProjectAssetPath(relativePath);
        const std::string paths[] = {
            dllAssetPath,
            dllDirectPath,
            projectPath,
            relativePath
        };

        for (const std::string& path : paths)
        {
            if (path.empty())
                continue;

            ID3D11ShaderResourceView* texture = IconLoader::CharacterIconCache::LoadPNGFromFileExternal(path.c_str());
            if (texture)
            {
                Logger::LogInfo("[Menu] Loaded asset texture: " + path);
                return texture;
            }
        }

        Logger::LogWarning(std::string("[Menu] Asset texture not found or not decodable: ") + relativePath);
        return nullptr;
    }

    static void LoadPlatformIcons()
    {
        struct PlatformIconAsset
        {
            int platform;
            const char* path;
            EmbeddedImages::AssetId embeddedAssetId;
        };

        const PlatformIconAsset assets[] = {
            { 1, "src\\Assets\\Psn-Logo-PNG-Photos.png", EmbeddedImages::AssetId::PlatformPsn },
            { 2, "src\\Assets\\xbox.png", EmbeddedImages::AssetId::PlatformXbox },
            { 3, "src\\Assets\\Steam_icon_logo.svg.png", EmbeddedImages::AssetId::PlatformSteam },
            { 4, "src\\Assets\\Nintendo-switch-icon.png", EmbeddedImages::AssetId::PlatformSwitch }
        };

        for (const PlatformIconAsset& asset : assets)
        {
            if (asset.platform <= 0 || asset.platform >= PLATFORM_ICON_COUNT || g_PlatformIcons[asset.platform])
                continue;

            g_PlatformIcons[asset.platform] = LoadAssetTexture(asset.path, asset.embeddedAssetId);
        }
    }

    static void ReleasePlatformIcons()
    {
        for (int i = 0; i < PLATFORM_ICON_COUNT; i++)
        {
            if (g_PlatformIcons[i])
            {
                g_PlatformIcons[i]->Release();
                g_PlatformIcons[i] = nullptr;
            }
        }
    }

    static HMODULE GetCurrentDllModuleHandle()
    {
        if (g_DllModule)
            return g_DllModule;

        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetCurrentDllModuleHandle),
            &module))
        {
            return nullptr;
        }

        return module;
    }

    static bool WriteMenuVideoResourceToTempFile(std::wstring& outPath)
    {
        if (g_MenuVideoEmbeddedBytes.empty())
        {
            HMODULE module = GetCurrentDllModuleHandle();
            if (!module)
            {
                g_MenuVideoStatus = "module fail";
                return false;
            }

            if (!PreloadEmbeddedVideoResource(module))
                return false;
        }

        if (g_MenuVideoEmbeddedBytes.empty())
        {
            g_MenuVideoStatus = "resource empty";
            return false;
        }

        wchar_t tempDir[MAX_PATH] = {};
        DWORD tempDirLength = GetTempPathW(MAX_PATH, tempDir);
        if (tempDirLength == 0 || tempDirLength >= MAX_PATH)
        {
            g_MenuVideoStatus = "temp path fail";
            return false;
        }

        const LONG tempSerial = InterlockedIncrement(&g_MenuVideoTempSerial);
        wchar_t tempFile[MAX_PATH] = {};
        if (swprintf_s(
            tempFile,
            L"%srugir_menu_background_%lu_%lu_%ld_%llu.mp4",
            tempDir,
            GetCurrentProcessId(),
            GetCurrentThreadId(),
            tempSerial,
            (unsigned long long)GetTickCount64()) < 0)
        {
            g_MenuVideoLastWin32Error = ERROR_BUFFER_OVERFLOW;
            g_MenuVideoStatus = "temp name fail";
            return false;
        }

        HANDLE file = CreateFileW(
            tempFile,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            g_MenuVideoLastWin32Error = GetLastError();
            g_MenuVideoStatus = "temp open fail";
            return false;
        }

        DWORD written = 0;
        const DWORD resourceSize = (DWORD)g_MenuVideoEmbeddedBytes.size();
        const BOOL ok = WriteFile(file, g_MenuVideoEmbeddedBytes.data(), resourceSize, &written, nullptr);
        g_MenuVideoLastWin32Error = ok ? ERROR_SUCCESS : GetLastError();
        CloseHandle(file);
        if (!ok || written != resourceSize)
        {
            DeleteFileW(tempFile);
            g_MenuVideoStatus = "temp write fail";
            return false;
        }

        outPath = tempFile;
        g_MenuVideoLastWin32Error = ERROR_SUCCESS;
        g_MenuVideoStatus = "temp ready";
        return true;
    }

    static bool CreateMenuVideoReaderFromEmbeddedResource(IMFAttributes* attributes)
    {
        if (g_MenuVideoTempPath.empty() && !WriteMenuVideoResourceToTempFile(g_MenuVideoTempPath))
            return false;

        HRESULT hr = MFCreateSourceReaderFromURL(g_MenuVideoTempPath.c_str(), attributes, &g_MenuVideoReader);
        g_MenuVideoLastHr = hr;
        g_MenuVideoStatus = SUCCEEDED(hr) && g_MenuVideoReader ? "reader ready" : "reader fail";
        return SUCCEEDED(hr) && g_MenuVideoReader != nullptr;
    }

    static void ReleaseMenuBackgroundVideo()
    {
        if (g_MenuVideoSRV)
        {
            g_MenuVideoSRV->Release();
            g_MenuVideoSRV = nullptr;
        }
        if (g_MenuVideoTexture)
        {
            g_MenuVideoTexture->Release();
            g_MenuVideoTexture = nullptr;
        }
        if (g_MenuVideoReader)
        {
            g_MenuVideoReader->Release();
            g_MenuVideoReader = nullptr;
        }
        if (g_MenuVideoMfStarted)
        {
            MFShutdown();
            g_MenuVideoMfStarted = false;
        }
        if (g_MenuVideoComInitialized)
        {
            CoUninitialize();
            g_MenuVideoComInitialized = false;
        }
        if (!g_MenuVideoTempPath.empty())
        {
            DeleteFileW(g_MenuVideoTempPath.c_str());
            g_MenuVideoTempPath.clear();
        }

        g_MenuVideoWidth = 0;
        g_MenuVideoHeight = 0;
        g_MenuVideoSourceWidth = 0;
        g_MenuVideoSourceHeight = 0;
        g_MenuVideoStride = 0;
        g_MenuVideoDuration = 0;
        g_MenuVideoLastFrameTime = 0.0;
        g_MenuVideoLastHr = S_OK;
        g_MenuVideoLastWin32Error = ERROR_SUCCESS;
        g_MenuVideoLastFlags = 0;
        g_MenuVideoFrameCount = 0;
        g_MenuVideoNoSampleCount = 0;
        g_MenuVideoBufferBytes = 0;
        g_MenuVideoFrameCopied = false;
        g_MenuVideoReady = false;
        g_MenuVideoRotateToLandscape = false;
        g_MenuVideoStatus = "released";
    }

    static bool CreateMenuVideoTexture(UINT width, UINT height)
    {
        if (!g_Device || !width || !height)
        {
            g_MenuVideoStatus = "texture args fail";
            return false;
        }

        if (g_MenuVideoTexture && g_MenuVideoSRV && g_MenuVideoWidth == width && g_MenuVideoHeight == height)
            return true;

        if (g_MenuVideoSRV)
        {
            g_MenuVideoSRV->Release();
            g_MenuVideoSRV = nullptr;
        }
        if (g_MenuVideoTexture)
        {
            g_MenuVideoTexture->Release();
            g_MenuVideoTexture = nullptr;
        }
        g_MenuVideoFrameCopied = false;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = g_Device->CreateTexture2D(&texDesc, nullptr, &g_MenuVideoTexture);
        g_MenuVideoLastHr = hr;
        if (FAILED(hr) || !g_MenuVideoTexture)
        {
            g_MenuVideoStatus = "texture fail";
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = g_Device->CreateShaderResourceView(g_MenuVideoTexture, &srvDesc, &g_MenuVideoSRV);
        g_MenuVideoLastHr = hr;
        if (FAILED(hr) || !g_MenuVideoSRV)
        {
            g_MenuVideoTexture->Release();
            g_MenuVideoTexture = nullptr;
            g_MenuVideoStatus = "srv fail";
            return false;
        }

        g_MenuVideoWidth = width;
        g_MenuVideoHeight = height;
        g_MenuVideoStatus = "texture ready";
        return true;
    }

    static bool InitializeMenuBackgroundVideo()
    {
        if (g_MenuVideoReady)
            return true;
        if (!g_Device || !g_DeviceContext)
        {
            g_MenuVideoStatus = "d3d not ready";
            return false;
        }

        HRESULT hr = S_OK;
        if (!g_MenuVideoMfStarted)
        {
            hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            g_MenuVideoComInitialized = SUCCEEDED(hr);

            hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
            g_MenuVideoLastHr = hr;
            if (FAILED(hr))
            {
                g_MenuVideoStatus = "mf startup fail";
                return false;
            }
            g_MenuVideoMfStarted = true;
        }

        IMFAttributes* attributes = nullptr;
        hr = MFCreateAttributes(&attributes, 3);
        g_MenuVideoLastHr = hr;
        if (SUCCEEDED(hr))
        {
            attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
            attributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
            attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        }

        const bool readerCreated = CreateMenuVideoReaderFromEmbeddedResource(attributes);
        if (attributes)
            attributes->Release();
        if (!readerCreated || !g_MenuVideoReader)
            return false;

        IMFMediaType* mediaType = nullptr;
        hr = MFCreateMediaType(&mediaType);
        g_MenuVideoLastHr = hr;
        if (SUCCEEDED(hr))
        {
            mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            hr = g_MenuVideoReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType);
            g_MenuVideoLastHr = hr;
        }
        if (mediaType)
            mediaType->Release();
        if (FAILED(hr))
        {
            g_MenuVideoStatus = "set type fail";
            return false;
        }

        IMFMediaType* currentType = nullptr;
        hr = g_MenuVideoReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &currentType);
        g_MenuVideoLastHr = hr;
        if (FAILED(hr) || !currentType)
        {
            g_MenuVideoStatus = "get type fail";
            return false;
        }

        UINT32 width = 0;
        UINT32 height = 0;
        hr = MFGetAttributeSize(currentType, MF_MT_FRAME_SIZE, &width, &height);
        g_MenuVideoLastHr = hr;
        UINT32 strideValue = 0;
        LONG stride = 0;
        if (FAILED(currentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideValue)) || strideValue == 0)
            stride = (LONG)width * 4;
        else
            stride = (LONG)strideValue;
        currentType->Release();
        if (FAILED(hr) || width == 0 || height == 0)
        {
            g_MenuVideoStatus = "size fail";
            return false;
        }

        g_MenuVideoStride = stride;

        PROPVARIANT duration;
        PropVariantInit(&duration);
        if (SUCCEEDED(g_MenuVideoReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration)) &&
            duration.vt == VT_UI8)
        {
            g_MenuVideoDuration = (LONGLONG)duration.uhVal.QuadPart;
        }
        PropVariantClear(&duration);

        g_MenuVideoSourceWidth = width;
        g_MenuVideoSourceHeight = height;
        g_MenuVideoRotateToLandscape = height > width;

        const UINT textureWidth = g_MenuVideoRotateToLandscape ? height : width;
        const UINT textureHeight = g_MenuVideoRotateToLandscape ? width : height;

        if (!CreateMenuVideoTexture(textureWidth, textureHeight))
            return false;

        g_MenuVideoReady = true;
        g_MenuVideoStatus = "ready";
        return true;
    }

    static void RestartMenuBackgroundVideo()
    {
        if (!g_MenuVideoReader)
            return;

        PROPVARIANT position;
        PropVariantInit(&position);
        position.vt = VT_I8;
        position.hVal.QuadPart = 0;
        g_MenuVideoReader->SetCurrentPosition(GUID_NULL, position);
        PropVariantClear(&position);
    }

    static bool UpdateMenuBackgroundVideoFrame()
    {
        if (!InitializeMenuBackgroundVideo() || !g_MenuVideoReader || !g_MenuVideoTexture || !g_DeviceContext)
            return false;

        const double now = ImGui::GetTime();
        if ((now - g_MenuVideoLastFrameTime) < MENU_VIDEO_FRAME_INTERVAL)
            return g_MenuVideoSRV != nullptr && g_MenuVideoFrameCopied;

        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* sample = nullptr;
        HRESULT hr = g_MenuVideoReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &sample);
        g_MenuVideoLastHr = hr;
        g_MenuVideoLastFlags = flags;

        if (SUCCEEDED(hr) && (flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (sample)
            {
                sample->Release();
                sample = nullptr;
            }
            RestartMenuBackgroundVideo();
            hr = g_MenuVideoReader->ReadSample(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                &streamIndex,
                &flags,
                &timestamp,
                &sample);
            g_MenuVideoLastHr = hr;
            g_MenuVideoLastFlags = flags;
        }

        if (FAILED(hr) || !sample)
        {
            if (sample)
                sample->Release();
            g_MenuVideoNoSampleCount++;
            g_MenuVideoStatus = FAILED(hr) ? "read fail" : "no sample";
            return g_MenuVideoSRV != nullptr && g_MenuVideoFrameCopied;
        }

        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        sample->Release();
        if (FAILED(hr) || !buffer)
        {
            g_MenuVideoLastHr = hr;
            g_MenuVideoStatus = "buffer fail";
            return g_MenuVideoSRV != nullptr && g_MenuVideoFrameCopied;
        }

        BYTE* data = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;
        hr = buffer->Lock(&data, &maxLength, &currentLength);
        g_MenuVideoLastHr = hr;
        g_MenuVideoBufferBytes = currentLength;
        if (SUCCEEDED(hr) && data && currentLength > 0)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            hr = g_DeviceContext->Map(g_MenuVideoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            g_MenuVideoLastHr = hr;
            if (SUCCEEDED(hr))
            {
                const UINT sourceWidth = g_MenuVideoSourceWidth != 0 ? g_MenuVideoSourceWidth : g_MenuVideoWidth;
                const UINT sourceHeight = g_MenuVideoSourceHeight != 0 ? g_MenuVideoSourceHeight : g_MenuVideoHeight;
                const LONG sourceStride = g_MenuVideoStride != 0 ? g_MenuVideoStride : (LONG)sourceWidth * 4;
                const UINT srcPitch = sourceStride < 0 ? (UINT)-sourceStride : (UINT)sourceStride;
                const UINT sourceRowBytes = sourceWidth * 4;
                const UINT maxRows = srcPitch > 0 ? currentLength / srcPitch : 0;
                const UINT rows = (std::min)(sourceHeight, maxRows);

                if (g_MenuVideoRotateToLandscape)
                {
                    const UINT cols = (std::min)(sourceWidth, srcPitch / 4);
                    for (UINT y = 0; y < rows; ++y)
                    {
                        const UINT srcY = sourceStride < 0 ? (rows - 1 - y) : y;
                        const BYTE* srcRow = data + (size_t)srcY * srcPitch;
                        const UINT dstX = y;
                        for (UINT x = 0; x < cols; ++x)
                        {
                            const UINT dstY = sourceWidth - 1 - x;
                            if (dstX >= g_MenuVideoWidth || dstY >= g_MenuVideoHeight)
                                continue;
                            memcpy(
                                (BYTE*)mapped.pData + (size_t)dstY * mapped.RowPitch + (size_t)dstX * 4,
                                srcRow + (size_t)x * 4,
                                4);
                        }
                    }
                }
                else
                {
                    const UINT copyPitch = (std::min)(sourceRowBytes, (UINT)mapped.RowPitch);
                    for (UINT y = 0; y < rows; ++y)
                    {
                        const UINT srcY = sourceStride < 0 ? (rows - 1 - y) : y;
                        memcpy(
                            (BYTE*)mapped.pData + (size_t)y * mapped.RowPitch,
                            data + (size_t)srcY * srcPitch,
                            copyPitch);
                    }
                }
                g_DeviceContext->Unmap(g_MenuVideoTexture, 0);
                g_MenuVideoLastFrameTime = now;
                if (rows > 0)
                {
                    g_MenuVideoFrameCopied = true;
                    g_MenuVideoFrameCount++;
                    g_MenuVideoStatus = "frame copied";
                }
                else
                {
                    g_MenuVideoStatus = "zero rows";
                }
            }
            else
            {
                g_MenuVideoStatus = "map fail";
            }
        }
        else
        {
            g_MenuVideoStatus = "lock fail";
        }

        if (data)
            buffer->Unlock();
        buffer->Release();
        return g_MenuVideoSRV != nullptr && g_MenuVideoFrameCopied;
    }

    static bool DrawMenuBackgroundVideo(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float rounding)
    {
        if (!g_Settings.EnableMenuBackgroundVideo)
        {
            if (g_MenuVideoReady || g_MenuVideoReader || g_MenuVideoTexture || g_MenuVideoSRV)
                ReleaseMenuBackgroundVideo();
            return false;
        }

        if (!drawList || !UpdateMenuBackgroundVideoFrame() || !g_MenuVideoSRV || g_MenuVideoWidth == 0 || g_MenuVideoHeight == 0)
            return false;

        const float dstW = max.x - min.x;
        const float dstH = max.y - min.y;
        if (dstW <= 1.0f || dstH <= 1.0f)
            return false;

        const float srcAspect = (float)g_MenuVideoWidth / (float)g_MenuVideoHeight;
        float drawW = dstW;
        float drawH = drawW / srcAspect;
        if (drawH < dstH)
        {
            drawH = dstH;
            drawW = drawH * srcAspect;
        }

        const ImVec2 imageMin(
            min.x + (dstW - drawW) * 0.5f,
            min.y + (dstH - drawH) * 0.5f);
        const ImVec2 imageMax(imageMin.x + drawW, imageMin.y + drawH);

        drawList->PushClipRect(min, max, true);
        drawList->AddImageRounded(
            (ImTextureID)g_MenuVideoSRV,
            imageMin,
            imageMax,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32_WHITE,
            rounding);
        drawList->PopClipRect();
        return true;
    }

    static ID3D11ShaderResourceView* GetPlatformIcon(int platform)
    {
        if (platform <= 0 || platform >= PLATFORM_ICON_COUNT)
            return nullptr;

        return g_PlatformIcons[platform];
    }

    static const char* GetPlatformShortName(const Cheats::PlayerNetworkInfo& player)
    {
        switch (player.platform)
        {
        case 1: return "PS";
        case 2: return "XB";
        case 3: return "PC";
        case 4: return "NS";
        default: return player.platformName[0] != '\0' ? player.platformName : "N/A";
        }
    }

    static void DrawPlatformIconCell(const Cheats::PlayerNetworkInfo& player)
    {
        const float iconSize = 16.0f;
        ID3D11ShaderResourceView* icon = GetPlatformIcon(player.platform);

        const float cellWidth = ImGui::GetContentRegionAvail().x;
        const float offsetX = (std::max)(0.0f, (cellWidth - iconSize) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        if (icon)
        {
            ImGui::Image((ImTextureID)icon, ImVec2(iconSize, iconSize));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", player.platformName);
            return;
        }

        ImGui::TextUnformatted(GetPlatformShortName(player));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", player.platformName);
    }

    static int GetTeamSortKey(int teamId)
    {
        return teamId >= 0 ? teamId : 9999;
    }

    static void SetupFutureTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        // SPACING & SIZING
        style.WindowPadding     = ImVec2(0, 0);
        style.FramePadding      = ImVec2(11, 7);
        style.ItemSpacing       = ImVec2(10, 9);
        style.ItemInnerSpacing  = ImVec2(8, 6);
        style.IndentSpacing     = 22.0f;
        style.ScrollbarSize     = 11.0f;
        style.GrabMinSize       = 10.0f;

        // ROUNDING
        style.WindowRounding    = 8.0f;
        style.ChildRounding     = 6.0f;
        style.FrameRounding     = 5.0f;
        style.PopupRounding     = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding      = 5.0f;
        style.TabRounding       = 5.0f;

        // BORDERS
        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 0.0f;
        style.FrameBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.0f;

        ImVec4* colors = style.Colors;

        ImVec4 PRIMARY           = g_Colors.accentColor;
        ImVec4 SECONDARY         = g_Colors.accentMagenta;
        ImVec4 TEXT_WHITE        = g_Colors.textPrimary;
        ImVec4 WINDOW_BG         = g_Colors.bgDarkest;
        ImVec4 CARD_BG           = g_Colors.bgDark;
        ImVec4 SECONDARY_SURFACE = g_Colors.bgMedium;
        ImVec4 BORDER            = ImVec4(0.175f, 0.130f, 0.285f, 1.0f);

        // WINDOW & BACKGROUND
        colors[ImGuiCol_WindowBg]           = WINDOW_BG;
        colors[ImGuiCol_ChildBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg]            = CARD_BG;

        // BORDERS & SEPARATORS
        colors[ImGuiCol_Border]             = BORDER;
        colors[ImGuiCol_BorderShadow]       = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_Separator]          = BORDER;
        colors[ImGuiCol_SeparatorHovered]   = PRIMARY;
        colors[ImGuiCol_SeparatorActive]    = SECONDARY;

        // TEXT
        colors[ImGuiCol_Text]               = TEXT_WHITE;
        colors[ImGuiCol_TextDisabled]       = ImVec4(0.5f, 0.5f, 0.6f, 0.6f);
        colors[ImGuiCol_TextSelectedBg]     = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.3f);

        // BUTTONS
        colors[ImGuiCol_Button]             = SECONDARY_SURFACE;
        colors[ImGuiCol_ButtonHovered]      = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.35f);
        colors[ImGuiCol_ButtonActive]       = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.55f);

        // FRAMES (Input fields, sliders background)
        colors[ImGuiCol_FrameBg]            = ImVec4(0.042f, 0.034f, 0.066f, 1.0f);
        colors[ImGuiCol_FrameBgHovered]     = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.18f);
        colors[ImGuiCol_FrameBgActive]      = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.28f);

        // HEADERS (Tree nodes, CollapsingHeader)
        colors[ImGuiCol_Header]             = SECONDARY_SURFACE;
        colors[ImGuiCol_HeaderHovered]      = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.7f);
        colors[ImGuiCol_HeaderActive]       = PRIMARY;

        // TITLE BAR
        colors[ImGuiCol_TitleBg]            = WINDOW_BG;
        colors[ImGuiCol_TitleBgActive]      = g_Colors.bgMedium;
        colors[ImGuiCol_TitleBgCollapsed]   = CARD_BG;

        // MENU BAR
        colors[ImGuiCol_MenuBarBg]          = CARD_BG;

        // SCROLLBAR
        colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.015f, 0.020f, 0.032f, 0.35f);
        colors[ImGuiCol_ScrollbarGrab]      = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.45f);
        colors[ImGuiCol_ScrollbarGrabHovered] = PRIMARY;
        colors[ImGuiCol_ScrollbarGrabActive] = SECONDARY;

        // CHECKMARK
        colors[ImGuiCol_CheckMark]          = PRIMARY;

        // SLIDER
        colors[ImGuiCol_SliderGrab]         = PRIMARY;
        colors[ImGuiCol_SliderGrabActive]   = SECONDARY;

        // TABS
        colors[ImGuiCol_Tab]                = SECONDARY_SURFACE;
        colors[ImGuiCol_TabHovered]         = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.7f);
        colors[ImGuiCol_TabActive]          = PRIMARY;
        colors[ImGuiCol_TabUnfocused]       = SECONDARY_SURFACE;
        colors[ImGuiCol_TabUnfocusedActive] = PRIMARY;

        // PLOT
        colors[ImGuiCol_PlotLines]          = PRIMARY;
        colors[ImGuiCol_PlotLinesHovered]   = SECONDARY;
        colors[ImGuiCol_PlotHistogram]      = PRIMARY;
        colors[ImGuiCol_PlotHistogramHovered] = SECONDARY;

        // DRAG & DROP
        colors[ImGuiCol_DragDropTarget]     = ImVec4(PRIMARY.x, PRIMARY.y, PRIMARY.z, 0.9f);
    }

    static void SetupFreeImGuiTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const ImVec4 accent = ImVec4(121.0f / 255.0f, 106.0f / 255.0f, 231.0f / 255.0f, 1.0f);
        const ImVec4 accentHover = ImVec4(151.0f / 255.0f, 136.0f / 255.0f, 255.0f / 255.0f, 1.0f);
        const ImVec4 window = ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 12.0f / 255.0f, 0.36f);
        const ImVec4 child = ImVec4(9.0f / 255.0f, 10.0f / 255.0f, 16.0f / 255.0f, 0.42f);
        const ImVec4 widget = ImVec4(23.0f / 255.0f, 24.0f / 255.0f, 33.0f / 255.0f, 0.66f);

        g_Colors.bgDarkest = window;
        g_Colors.bgDark = child;
        g_Colors.bgMedium = widget;
        g_Colors.bgLight = ImVec4(42.0f / 255.0f, 39.0f / 255.0f, 55.0f / 255.0f, 0.58f);
        g_Colors.accentColor = accent;
        g_Colors.accentColorHover = accentHover;
        g_Colors.accentColorActive = ImVec4(129.0f / 255.0f, 99.0f / 255.0f, 255.0f / 255.0f, 1.0f);
        g_Colors.textPrimary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        g_Colors.textSecondary = ImVec4(0.72f, 0.72f, 0.76f, 1.0f);
        g_Colors.textDisabled = ImVec4(0.50f, 0.54f, 0.64f, 1.0f);
        g_Colors.primaryBlue = g_Colors.accentColorActive;
        g_Colors.accentCyan = accentHover;
        g_Colors.accentMagenta = ImVec4(1.0f, 1.0f, 1.0f, 0.55f);
        g_Colors.accentPurple = accent;
        g_Colors.accentAqua = accentHover;

        style.WindowPadding = ImVec2(0.0f, 0.0f);
        style.WindowRounding = 12.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildRounding = 6.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.FramePadding = ImVec2(9.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(5.0f, 3.0f);
        style.ChildPadding = ImVec2(10.0f, 8.0f);
        style.ScrollbarSize = 6.0f;
        style.ScrollbarRounding = 5.0f;
        style.GrabRounding = 4.0f;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = window;
        colors[ImGuiCol_ChildBg] = child;
        colors[ImGuiCol_PopupBg] = ImVec4(7.0f / 255.0f, 8.0f / 255.0f, 18.0f / 255.0f, 0.88f);
        colors[ImGuiCol_Border] = ImVec4(132.0f / 255.0f, 121.0f / 255.0f, 220.0f / 255.0f, 0.24f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_Text] = g_Colors.textPrimary;
        colors[ImGuiCol_TextDisabled] = g_Colors.textDisabled;
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_FrameBg] = widget;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(50.0f / 255.0f, 51.0f / 255.0f, 68.0f / 255.0f, 0.76f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(66.0f / 255.0f, 62.0f / 255.0f, 88.0f / 255.0f, 0.86f);
        colors[ImGuiCol_Button] = widget;
        colors[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.56f);
        colors[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.82f);
        colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.80f);
        colors[ImGuiCol_HeaderActive] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentHover;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(9.0f / 255.0f, 9.0f / 255.0f, 9.0f / 255.0f, 0.28f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
        colors[ImGuiCol_ScrollbarGrabHovered] = accentHover;
        colors[ImGuiCol_ScrollbarGrabActive] = accent;
        colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered] = accentHover;
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        colors[ImGuiCol_ButtonShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.08f);
        colors[ImGuiCol_FrameBgShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.07f);
    }

    static void SetPreviewTheme(int themeIndex)
    {
        int themeCount = GetPreviewThemeCount();
        if (themeCount <= 0)
            return;

        if (themeIndex < 0)
            themeIndex = themeCount - 1;
        else if (themeIndex >= themeCount)
            themeIndex = 0;

        if (g_SelectedPreviewTheme == themeIndex)
            return;

        g_SelectedPreviewTheme = themeIndex;
        SetupFreeImGuiTheme();
    }

    // ============================================================================
    // HELPER FUNCTIONS - MODERN UI COMPONENTS
    // ============================================================================
    static ImU32 ColorWithAlpha(const ImVec4& color, float alpha)
    {
        ImVec4 adjusted = color;
        adjusted.w = alpha;
        return ImGui::GetColorU32(adjusted);
    }

    static void DrawCyberLine(ImDrawList* drawList, const ImVec2& start, float width, ImU32 color)
    {
        drawList->AddLine(start, ImVec2(start.x + width, start.y), color, 1.0f);
        drawList->AddLine(ImVec2(start.x + width + 4.0f, start.y), ImVec2(start.x + width + 20.0f, start.y), color, 2.0f);
    }

    static void DrawLogoFallback(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        drawList->AddRectFilledMultiColor(
            min,
            max,
            ImGui::GetColorU32(g_Colors.accentPurple),
            ImGui::GetColorU32(g_Colors.accentMagenta),
            ImGui::GetColorU32(g_Colors.accentColor),
            ImGui::GetColorU32(g_Colors.primaryBlue)
        );
        drawList->AddRect(min, max, ColorWithAlpha(g_Colors.accentColor, 0.85f), 8.0f, 0, 1.5f);

        const char* logoText = "R";
        float size = 34.0f;
        ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, logoText);
        ImVec2 pos = ImVec2(
            min.x + ((max.x - min.x) - textSize.x) * 0.5f,
            min.y + ((max.y - min.y) - textSize.y) * 0.5f - 1.0f
        );
        drawList->AddText(ImGui::GetFont(), size, pos, IM_COL32(255, 255, 255, 255), logoText);
    }

    static void DrawRugirLogo(const ImVec2& size)
    {
        ImGui::InvisibleButton("##RugirLogo", size);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(min, max, ColorWithAlpha(g_Colors.bgMedium, 0.55f), 8.0f);
        if (g_RugirLogo)
        {
            drawList->AddImageRounded(
                (ImTextureID)g_RugirLogo,
                min,
                max,
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32_WHITE,
                8.0f
            );
            drawList->AddRect(min, max, ColorWithAlpha(g_Colors.accentColor, 0.50f), 8.0f, 0, 1.0f);
        }
        else
        {
            DrawLogoFallback(drawList, min, max);
        }
    }

    static bool ModernButton(const char* label, ImVec2 size, bool selected = false)
    {
        ImGui::PushID(label);
        ImGui::InvisibleButton("##nav", size);
        ImVec2 buttonPos = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();
        bool isHovered = ImGui::IsItemHovered();
        bool isClicked = ImGui::IsItemClicked();
        ImGui::PopID();
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImU32 bgColor = ImGui::GetColorU32(selected ? ImVec4(0.380f, 0.155f, 0.740f, 0.96f) : ImVec4(0.028f, 0.024f, 0.043f, 0.18f));
        if (isHovered && !selected)
            bgColor = ImGui::GetColorU32(ImVec4(0.085f, 0.058f, 0.135f, 0.78f));

        if (selected)
        {
            drawList->AddRectFilledMultiColor(
                buttonPos,
                buttonMax,
                ImGui::GetColorU32(ImVec4(0.540f, 0.240f, 1.000f, 0.98f)),
                ImGui::GetColorU32(ImVec4(0.365f, 0.135f, 0.720f, 0.98f)),
                ImGui::GetColorU32(ImVec4(0.245f, 0.075f, 0.520f, 0.98f)),
                ImGui::GetColorU32(ImVec4(0.455f, 0.165f, 0.900f, 0.98f))
            );
        }
        else
        {
            drawList->AddRectFilled(buttonPos, buttonMax, bgColor, 6.0f);
        }

        drawList->AddRect(buttonPos, buttonMax, ColorWithAlpha(selected ? g_Colors.accentColorHover : g_Colors.bgLight, selected ? 0.90f : 0.20f), 5.0f, 0, selected ? 1.1f : 1.0f);

        if (selected)
        {
            drawList->AddRectFilled(
                buttonPos,
                ImVec2(buttonPos.x + 2.0f, buttonMax.y),
                ImGui::GetColorU32(g_Colors.accentMagenta),
                2.0f
            );
        }
        
        // Parse label: "P   ESP" -> icon="P", text="ESP"
        std::string fullLabel = label;
        std::string iconChar = "";
        std::string labelText = "";
        
        size_t spacePos = fullLabel.find("   ");
        if (spacePos != std::string::npos && spacePos > 0)
        {
            iconChar = fullLabel.substr(0, spacePos);
            labelText = fullLabel.substr(spacePos + 3);
        }
        
        ImVec2 buttonSize = ImVec2(buttonMax.x - buttonPos.x, buttonMax.y - buttonPos.y);
        ImU32 textColor = ImGui::GetColorU32(selected ? g_Colors.textPrimary : g_Colors.textSecondary);
        ImU32 iconColor = ImGui::GetColorU32(selected ? g_Colors.textPrimary : g_Colors.accentColorHover);
        
        if (!iconChar.empty() && !labelText.empty() && g_SymbolFont)
        {
            // Draw icon with symbol font
            ImGui::PushFont(g_SymbolFont);
            ImVec2 iconSize = ImGui::CalcTextSize(iconChar.c_str());
            ImVec2 iconPos = ImVec2(
                buttonPos.x + 7.0f,
                buttonPos.y + (buttonSize.y - iconSize.y) / 2.0f
            );
            drawList->AddText(iconPos, iconColor, iconChar.c_str());
            ImGui::PopFont();
            
            // Draw label text with default font
            ImVec2 textSize = ImGui::CalcTextSize(labelText.c_str());
            ImVec2 textPos = ImVec2(
                buttonPos.x + 28.0f,
                buttonPos.y + (buttonSize.y - textSize.y) / 2.0f
            );
            drawList->AddText(textPos, textColor, labelText.c_str());
        }
        else
        {
            // Fallback: center text
            ImVec2 textSize = ImGui::CalcTextSize(fullLabel.c_str());
            ImVec2 textPos = ImVec2(
                buttonPos.x + (buttonSize.x - textSize.x) / 2.0f,
                buttonPos.y + (buttonSize.y - textSize.y) / 2.0f
            );
            drawList->AddText(textPos, textColor, fullLabel.c_str());
        }
        
        return isClicked;
    }

    // Button with icon from symbolfont
    // icon: single Unicode character from symbolfont, label: text label
    static bool ModernButtonWithIcon(const char* icon, const char* label, ImVec2 size, bool selected = false)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? g_Colors.accentColor : g_Colors.bgLight);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? g_Colors.accentColorHover : g_Colors.accentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Colors.accentColorActive);
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.textPrimary);
        
        // Create combined label: icon + label
        std::string displayLabel = icon;
        displayLabel += "   ";
        displayLabel += label;
        
        // Push symbol font if available for icon rendering
        if (g_SymbolFont)
        {
            ImGui::PushFont(g_SymbolFont);
        }
        
        bool result = ImGui::Button(displayLabel.c_str(), size);
        
        if (g_SymbolFont)
        {
            ImGui::PopFont();
        }
        
        ImGui::PopStyleColor(4);
        return result;
    }

    static void SectionHeader(const char* title)
    {
        ImGui::Spacing();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentColor);
        ImGui::Text(title);
        ImGui::PopStyleColor();
        DrawCyberLine(drawList, ImVec2(cursor.x, cursor.y + ImGui::GetTextLineHeight() + 4.0f), 72.0f, ColorWithAlpha(g_Colors.accentColor, 0.70f));
        ImGui::Spacing();
    }

    static void DrawSidebarInfoPanel(float width)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(width, 126.0f);
        ImVec2 panelMax(pos.x + size.x, pos.y + size.y);

        drawList->AddRectFilled(pos, panelMax, ImGui::GetColorU32(ImVec4(0.020f, 0.017f, 0.035f, 0.94f)), 6.0f);
        drawList->AddRect(pos, panelMax, ColorWithAlpha(g_Colors.bgLight, 0.70f), 6.0f, 0, 1.0f);
        drawList->AddRectFilled(pos, ImVec2(panelMax.x, pos.y + 24.0f), ImGui::GetColorU32(ImVec4(0.075f, 0.047f, 0.125f, 0.92f)), 6.0f, ImDrawFlags_RoundCornersTop);

        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(1.0f, 4.0f));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(g_Colors.accentColorHover, "MENU INFO");
        ImGui::Dummy(ImVec2(1.0f, 8.0f));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(g_Colors.textDisabled, "User");
        ImGui::SameLine(78.0f);
        ImGui::TextColored(g_Colors.textPrimary, "Private");

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(g_Colors.textDisabled, "Profile");
        ImGui::SameLine(78.0f);
        ImGui::TextColored(g_Colors.textPrimary, "%s", g_CurrentProfileName.c_str());

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextColored(g_Colors.textDisabled, "Core");
        ImGui::SameLine(78.0f);
        ImGui::TextColored(g_Settings.EnableGlobal ? g_Colors.success : g_Colors.danger, "%s", g_Settings.EnableGlobal ? "ON" : "OFF");

        ImGui::EndGroup();
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y));
    }

    static void StatusIndicator(bool enabled)
    {
        ImVec4 color = (enabled ? g_Colors.success : g_Colors.danger);
        const char* status = (enabled ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", status);
        ImGui::PopStyleColor();
    }

    static void DrawServerStatusOverlay()
    {
        static Cheats::ServerConnectionInfo cachedInfo{};
        static DWORD lastUpdateTick = 0;
        static bool hasCachedInfo = false;
        static bool waitingForServerInfo = true;

        DWORD now = GetTickCount();
        const DWORD refreshInterval = hasCachedInfo ? 15000 : 1000;
        if (lastUpdateTick == 0 || (now - lastUpdateTick) >= refreshInterval)
        {
            if (Cheats::CanReadServerConnectionInfo())
            {
                Cheats::ServerConnectionInfo nextInfo{};
                if (Cheats::GetCurrentServerConnectionInfo(&nextInfo))
                {
                    cachedInfo = nextInfo;
                    hasCachedInfo = true;
                    waitingForServerInfo = false;
                }
            }
            else
            {
                waitingForServerInfo = !hasCachedInfo;
            }

            lastUpdateTick = now;
        }

        char serverText[512] = {};
        const char* region = hasCachedInfo && cachedInfo.hasRegion ? cachedInfo.region : "Unknown";
        if (hasCachedInfo)
            snprintf(serverText, sizeof(serverText), "Server: %s", region);
        else if (waitingForServerInfo)
            snprintf(serverText, sizeof(serverText), "Server: waiting...");
        else
            snprintf(serverText, sizeof(serverText), "Server: N/A");

        char pingText[64] = {};
        if (hasCachedInfo && cachedInfo.hasPing)
            snprintf(pingText, sizeof(pingText), "Ping: %d ms", cachedInfo.pingMs);
        else
            snprintf(pingText, sizeof(pingText), "Ping: N/A");

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return;

        ImFont* font = ImGui::GetFont();
        float fontSize = 16.0f;
        ImVec2 pos(14.0f, 14.0f);
        ImVec2 serverSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, serverText);
        ImVec2 pingSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, pingText);
        float width = (std::max)(serverSize.x, pingSize.x);
        float height = serverSize.y + pingSize.y + 14.0f;

        drawList->AddRectFilled(
            ImVec2(pos.x - 8.0f, pos.y - 6.0f),
            ImVec2(pos.x + width + 8.0f, pos.y + height),
            IM_COL32(0, 0, 0, 165),
            5.0f
        );

        drawList->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, 255), serverText);
        drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y + serverSize.y + 4.0f), IM_COL32(120, 220, 255, 255), pingText);
    }

    static void UpdatePlayerListHotkeyHintState()
    {
        DWORD now = GetTickCount();
        if ((now - g_LastBattleModeHintCheckTick) < PLAYER_LIST_HINT_CHECK_INTERVAL_MS)
            return;

        g_LastBattleModeHintCheckTick = now;

        bool isInValidBattleMode = IsValidBattleMode();
        bool enteredValidBattleMode = !g_WasInValidBattleModeForHint && isInValidBattleMode;
        g_WasInValidBattleModeForHint = isInValidBattleMode;

        if (enteredValidBattleMode && !g_PlayerNetworkTableVisible)
        {
            g_PlayerListHotkeyHintVisible = true;
            g_PlayerListHotkeyHintStartTick = now;
        }

        if (!isInValidBattleMode || g_PlayerNetworkTableVisible)
            g_PlayerListHotkeyHintVisible = false;
    }

    static void DrawPlayerListHotkeyHint()
    {
        if (!g_PlayerListHotkeyHintVisible)
            return;

        DWORD now = GetTickCount();
        if ((now - g_PlayerListHotkeyHintStartTick) >= PLAYER_LIST_HINT_DURATION_MS)
        {
            g_PlayerListHotkeyHintVisible = false;
            return;
        }

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList)
            return;

        ImGuiIO& io = ImGui::GetIO();
        const char* message = "f8 to display player liste";
        ImFont* font = ImGui::GetFont();
        float fontSize = 18.0f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, message);
        ImVec2 padding(16.0f, 10.0f);
        ImVec2 boxMin(
            (io.DisplaySize.x - textSize.x) * 0.5f - padding.x,
            74.0f
        );
        ImVec2 boxMax(
            boxMin.x + textSize.x + padding.x * 2.0f,
            boxMin.y + textSize.y + padding.y * 2.0f
        );

        drawList->AddRectFilled(boxMin, boxMax, IM_COL32(10, 12, 18, 205), 6.0f);
        drawList->AddRect(boxMin, boxMax, ColorWithAlpha(g_Colors.accentColorHover, 0.90f), 6.0f, 0, 1.2f);
        drawList->AddText(font, fontSize, ImVec2(boxMin.x + padding.x, boxMin.y + padding.y), IM_COL32(255, 255, 255, 255), message);
    }

    static void DrawPlayerNetworkTableWindow()
    {
        if (!g_PlayerNetworkTableVisible)
            return;

        static Cheats::PlayerNetworkInfo players[128]{};
        static int playerCount = 0;
        static DWORD lastUpdateTick = 0;

        DWORD now = GetTickCount();
        if ((now - lastUpdateTick) >= 1000)
        {
            playerCount = Cheats::CollectPlayerNetworkInfo(players, (int)(sizeof(players) / sizeof(players[0])));
            lastUpdateTick = now;
        }

        ImGuiIO& io = ImGui::GetIO();
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
        const float headerAndPaddingHeight = 92.0f;
        const float minWindowHeight = 142.0f;
        const float desiredWindowHeight = headerAndPaddingHeight + rowHeight * (float)((std::max)(playerCount, 1));
        const float maxWindowHeight = (std::max)(220.0f, io.DisplaySize.y - 80.0f);
        const float windowHeight = (std::min)((std::max)(desiredWindowHeight, minWindowHeight), maxWindowHeight);
        const float windowWidth = (std::min)(960.0f, (std::max)(760.0f, io.DisplaySize.x - 40.0f));

        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(100.0f, 120.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Player Network Info", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        ImGui::TextColored(g_Colors.accentColorHover, "OTHER PLAYERS");
        ImGui::SameLine();
        ImGui::TextColored(g_Colors.textDisabled, "| %d found | refresh 1s", playerCount);
        ImGui::Separator();

        if (playerCount <= 0)
        {
            ImGui::TextColored(g_Colors.warning, "No other players found.");
            ImGui::End();
            return;
        }

        ImGuiTableFlags flags =
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp;

        const float tableHeight = (std::max)(64.0f, ImGui::GetContentRegionAvail().y);
        if (ImGui::BeginTable("##PlayerNetworkInfoTable", 8, flags, ImVec2(0.0f, tableHeight)))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
            ImGui::TableSetupColumn("Disconnect", ImGuiTableColumnFlags_WidthFixed, 76.0f);
            ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthStretch, 1.35f);
            ImGui::TableSetupColumn("Public", ImGuiTableColumnFlags_WidthStretch, 1.10f);
            ImGui::TableSetupColumn("Private", ImGuiTableColumnFlags_WidthStretch, 1.10f);
            ImGui::TableSetupColumn("Platform", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 66.0f);
            ImGui::TableSetupColumn("Team", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableHeadersRow();

            int sortedIndices[128]{};
            for (int i = 0; i < playerCount; i++)
                sortedIndices[i] = i;

            std::sort(sortedIndices, sortedIndices + playerCount, [&](int lhs, int rhs)
                {
                    const Cheats::PlayerNetworkInfo& a = players[lhs];
                    const Cheats::PlayerNetworkInfo& b = players[rhs];
                    int teamA = GetTeamSortKey(a.teamId);
                    int teamB = GetTeamSortKey(b.teamId);

                    if (teamA != teamB)
                        return teamA < teamB;
                    if (a.pingMs != b.pingMs)
                        return a.pingMs < b.pingMs;
                    return lhs < rhs;
                });

            int currentTeam = INT_MIN;
            int displayIndex = 0;
            for (int sortedPos = 0; sortedPos < playerCount; sortedPos++)
            {
                const Cheats::PlayerNetworkInfo& player = players[sortedIndices[sortedPos]];
                if (player.teamId != currentTeam)
                {
                    currentTeam = player.teamId;
                    int teamCount = 0;
                    for (int j = 0; j < playerCount; j++)
                    {
                        if (players[j].teamId == currentTeam)
                            teamCount++;
                    }

                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ColorWithAlpha(g_Colors.accentColor, 0.22f));
                    ImGui::TableSetColumnIndex(0);
                    if (currentTeam >= 0)
                        ImGui::Text("TEAM %d", currentTeam);
                    else
                        ImGui::TextUnformatted("TEAM N/A");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextColored(g_Colors.textDisabled, "%d player%s", teamCount, teamCount > 1 ? "s" : "");
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", displayIndex++);

                ImGui::TableSetColumnIndex(1);
                if (player.playerState)
                    ImGui::PushID(player.playerState);
                else
                    ImGui::PushID(sortedPos);
                const bool canChange = player.hasActor && !player.isLocal && player.playerState != nullptr;
                if (!canChange)
                    ImGui::BeginDisabled(true);
                if (ImGui::Button("Kick", ImVec2(66.0f, 0.0f)))
                    Cheats::ChangePlayerNetworkTargetToCh001(player.playerState);
                if (!canChange)
                    ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    if (player.isLocal)
                        ImGui::SetTooltip("Local player is not changed from this table.");
                    else if (!player.hasActor)
                        ImGui::SetTooltip("No active actor found for this player.");
                    else
                        ImGui::SetTooltip("Change this player to Ch001 variation 0.");
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(player.displayName);

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(player.publicName);

                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(player.privateName);

                ImGui::TableSetColumnIndex(5);
                DrawPlatformIconCell(player);

                ImGui::TableSetColumnIndex(6);
                if (player.pingMs >= 0)
                    ImGui::Text("%d ms", player.pingMs);
                else
                    ImGui::TextUnformatted("N/A");

                ImGui::TableSetColumnIndex(7);
                if (player.teamId >= 0)
                    ImGui::Text("%d", player.teamId);
                else
                    ImGui::TextUnformatted("N/A");
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }

    // ============================================================================
    // CHARACTER DATA HELPER FUNCTIONS (Using Character_Data.h)
    // ============================================================================
    struct CharacterVariationOption {
        int characterId;        // Real ID from Character_Data.h (1-46)
        int variationId;        // 0, 1, 2, etc.
        const char* displayName;
    };

    static std::vector<CharacterVariationOption> GetCharacterVariationOptionsFromCharacterData()
    {
        std::vector<CharacterVariationOption> result;
        for (int i = 0; i < Cheats::CharacterCount; i++) {
            const auto& entry = Cheats::Characters[i];
            for (int v = 0; v <= entry.maxVariation; v++) {
                result.push_back({entry.id, v, entry.name});
            }
        }
        return result;
    }

    static std::pair<int, int> GetCharacterAndVariationFromIndex(int index)
    {
        static auto options = GetCharacterVariationOptionsFromCharacterData();
        if (index >= 0 && index < (int)options.size()) {
            return {options[index].characterId, options[index].variationId};
        }
        return {-1, -1};
    }

    static const char* GetCharacterVariationDisplayName(int index)
    {
        static auto options = GetCharacterVariationOptionsFromCharacterData();
        if (index >= 0 && index < (int)options.size()) {
            static char displayName[128] = {};
            if (options[index].variationId > 0)
                snprintf(displayName, sizeof(displayName), "%s (V%d)", options[index].displayName, options[index].variationId);
            else
                snprintf(displayName, sizeof(displayName), "%s", options[index].displayName);

            return displayName;
        }
        return "Unknown";
    }

    static std::unordered_map<int, ID3D11ShaderResourceView*> g_CharacterIconGridCache;
    static size_t g_CharacterIconWarmCursor = 0;

    static ID3D11ShaderResourceView* GetCachedCharacterGridIcon(int characterId)
    {
        auto it = g_CharacterIconGridCache.find(characterId);
        return it != g_CharacterIconGridCache.end() ? it->second : nullptr;
    }

    static bool HasCachedCharacterGridIcon(int characterId)
    {
        return g_CharacterIconGridCache.find(characterId) != g_CharacterIconGridCache.end();
    }

    static void WarmCharacterGridIcons(const std::vector<CharacterVariationOption>& options, int selectedVariationIndex)
    {
        if (options.empty())
            return;

        constexpr int maxLoadsPerFrame = 2;
        int loadedThisFrame = 0;

        if (selectedVariationIndex >= 0 && selectedVariationIndex < (int)options.size())
        {
            int selectedCharacterId = options[selectedVariationIndex].characterId;
            if (!HasCachedCharacterGridIcon(selectedCharacterId))
            {
                g_CharacterIconGridCache[selectedCharacterId] =
                    IconLoader::CharacterIconCache::GetCharacterIcon(selectedCharacterId);
                loadedThisFrame++;
            }
        }

        for (size_t checked = 0; checked < options.size() && loadedThisFrame < maxLoadsPerFrame; checked++)
        {
            const CharacterVariationOption& option = options[g_CharacterIconWarmCursor % options.size()];
            g_CharacterIconWarmCursor = (g_CharacterIconWarmCursor + 1) % options.size();

            if (HasCachedCharacterGridIcon(option.characterId))
                continue;

            g_CharacterIconGridCache[option.characterId] =
                IconLoader::CharacterIconCache::GetCharacterIcon(option.characterId);
            loadedThisFrame++;
        }
    }

    // ============================================================================
    // CHARACTER ICON GRID SELECTOR
    // ============================================================================
    static bool CharacterIconGridSelector(int& selectedVariationIndex, float iconSize = 40.0f, bool reserveScrollbarSpace = false)
    {
        static auto all_options = GetCharacterVariationOptionsFromCharacterData();
        if (all_options.empty())
            return false;

        if (selectedVariationIndex < 0 || selectedVariationIndex >= (int)all_options.size())
            selectedVariationIndex = 0;

        WarmCharacterGridIcons(all_options, selectedVariationIndex);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

        bool changed = false;
        ImGuiStyle& style = ImGui::GetStyle();
        float itemSpacing = 6.0f;
        float buttonWidth = iconSize + 4.0f;
        float availableWidth = ImGui::GetContentRegionAvail().x;
        if (reserveScrollbarSpace)
            availableWidth -= style.ScrollbarSize + itemSpacing;

        int cols = (int)((availableWidth + itemSpacing) / (buttonWidth + itemSpacing));
        if (cols < 1) cols = 1;
        
        for (size_t i = 0; i < all_options.size(); i++)
        {
            const auto& option = all_options[i];
            
            ID3D11ShaderResourceView* iconSRV = GetCachedCharacterGridIcon(option.characterId);
            
            // Create unique ID for button
            ImGui::PushID((int)i);
            
            if (iconSRV)
            {
                // Display icon as image button
                bool pressed = ImGui::ImageButton("##character-icon", iconSRV, ImVec2(iconSize, iconSize));
                
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s (Var %d)", option.displayName, option.variationId);
                }
                
                if (pressed)
                {
                    selectedVariationIndex = (int)i;
                    changed = true;
                }
                
                // Highlight selected
                if (selectedVariationIndex == (int)i)
                {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    draw_list->AddRect(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Header), 3.0f, 0, 2.0f);
                }
            }
            else
            {
                char fallback[16] = {};
                snprintf(fallback, sizeof(fallback), "CH%d", option.characterId);
                if (ImAdd::Button(fallback, ImVec2(iconSize + 4.0f, iconSize + 4.0f)))
                {
                    selectedVariationIndex = (int)i;
                    changed = true;
                }
                
                if (selectedVariationIndex == (int)i)
                {
                        ImGui::GetWindowDrawList()->AddRect(
                        ImGui::GetItemRectMin(),
                        ImGui::GetItemRectMax(),
                        ImGui::GetColorU32(ImGuiCol_Header),
                        3.0f,
                        0,
                        2.0f);
                }
            }
            
            ImGui::PopID();
            
            // Next column logic
            if ((i + 1) % cols != 0 && i + 1 < all_options.size())
                ImGui::SameLine();
        }
        
        ImGui::PopStyleVar(2);
        return changed;
    }

    static bool CharacterIconGridPanel(const char* panelId, int& selectedVariationIndex, float height = 0.0f, float iconSize = 42.0f, bool reserveScrollbarSpace = false)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::BeginChild(panelId, ImVec2(0.0f, height), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        bool changed = CharacterIconGridSelector(selectedVariationIndex, iconSize, reserveScrollbarSpace);
        ImGui::EndChild();

        ImGui::PopStyleVar();
        return changed;
    }

    static int g_SelectedTeamId = -1;
    static std::vector<unsigned char> g_AvailableTeamIds;

    struct TeamListEntry
    {
        unsigned char teamId = 255;
        std::vector<std::string> playerNames;
    };

    static std::vector<std::string> g_CachedRecoveryTeamNames;
    static DWORD g_LastRecoveryTeamRefreshTick = 0;
    static std::vector<TeamListEntry> g_CachedLobbyTeams;
    static int g_CachedLobbyMyTeamId = -1;
    static DWORD g_LastLobbyTeamsRefreshTick = 0;
    static constexpr DWORD GAME_LIST_CACHE_INTERVAL_MS = 7500;
    static std::vector<SeasonRankRewardItemOption> g_CachedSeasonRewardItems;
    static bool g_SeasonRewardItemsLoaded = false;

    struct TimedLicenseClaimTestState
    {
        bool active = false;
        bool specialLicense = false;
        int seasonCode = 1;
        int freeRank = 1;
        int premiumRank = 0;
        int specialRank = 1;
        int repeatTotal = 1;
        int sentCount = 0;
        int delayMs = 1000;
        DWORD lastSendTick = 0;
    };

    static TimedLicenseClaimTestState g_TimedLicenseClaimTest;

    static void StartTimedLicenseClaimTest(bool specialLicense)
    {
        g_TimedLicenseClaimTest.active = true;
        g_TimedLicenseClaimTest.specialLicense = specialLicense;
        g_TimedLicenseClaimTest.seasonCode = g_HackSettings.LicenseClaimSeasonCode;
        g_TimedLicenseClaimTest.freeRank = g_HackSettings.LicenseClaimFreeRank;
        g_TimedLicenseClaimTest.premiumRank = g_HackSettings.LicenseClaimPremiumRank;
        g_TimedLicenseClaimTest.specialRank = g_HackSettings.LicenseClaimSpecialRank;
        g_TimedLicenseClaimTest.repeatTotal = (std::max)(1, (std::min)(g_HackSettings.LicenseClaimRepeatCount, 20));
        g_TimedLicenseClaimTest.sentCount = 0;
        g_TimedLicenseClaimTest.delayMs = (std::max)(50, (std::min)(g_HackSettings.LicenseClaimDelayMs, 10000));
        g_TimedLicenseClaimTest.lastSendTick = 0;
    }

    static void ProcessTimedLicenseClaimTest()
    {
        if (!g_TimedLicenseClaimTest.active)
            return;

        if (g_TimedLicenseClaimTest.sentCount >= g_TimedLicenseClaimTest.repeatTotal)
        {
            g_TimedLicenseClaimTest.active = false;
            return;
        }

        const DWORD now = GetTickCount();
        if (g_TimedLicenseClaimTest.lastSendTick != 0 &&
            now - g_TimedLicenseClaimTest.lastSendTick < static_cast<DWORD>(g_TimedLicenseClaimTest.delayMs))
        {
            return;
        }

        if (g_TimedLicenseClaimTest.specialLicense)
        {
            InGameHack_ReceiveSpecialLicenseClaimTest(g_TimedLicenseClaimTest.specialRank, 1);
        }
        else
        {
            InGameHack_ReceiveLicenseClaimTest(
                g_TimedLicenseClaimTest.seasonCode,
                g_TimedLicenseClaimTest.freeRank,
                g_TimedLicenseClaimTest.premiumRank,
                1);
        }

        ++g_TimedLicenseClaimTest.sentCount;
        g_TimedLicenseClaimTest.lastSendTick = now;

        if (g_TimedLicenseClaimTest.sentCount >= g_TimedLicenseClaimTest.repeatTotal)
            g_TimedLicenseClaimTest.active = false;
    }

    static void RefreshSeasonRewardItemCache()
    {
        g_CachedSeasonRewardItems = InGameHack_GetSeasonRankRewardItemOptions();
        if (g_HackSettings.SeasonRewardSourceIndex < 0 ||
            g_HackSettings.SeasonRewardSourceIndex >= (int)g_CachedSeasonRewardItems.size())
        {
            g_HackSettings.SeasonRewardSourceIndex = 0;
        }
        g_SeasonRewardItemsLoaded = true;
    }

    static void DrawSeasonRankRewardReplacementControls()
    {
        SeparatorLabel("Season rank rewards");

        if (FullWidthButton("REFRESH SEASON REWARDS"))
            RefreshSeasonRewardItemCache();

        if (!g_SeasonRewardItemsLoaded)
            RefreshSeasonRewardItemCache();

        if (g_CachedSeasonRewardItems.empty())
        {
            ImGui::TextColored(g_Colors.warning, "No season rank rewards found.");
            return;
        }

        if (g_HackSettings.SeasonRewardSourceIndex < 0 ||
            g_HackSettings.SeasonRewardSourceIndex >= (int)g_CachedSeasonRewardItems.size())
        {
            g_HackSettings.SeasonRewardSourceIndex = 0;
        }

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const bool wideLayout = availableWidth >= 620.0f;
        const float controlsWidth = wideLayout ? (std::min)(360.0f, (std::max)(250.0f, availableWidth * 0.34f)) : availableWidth;
        const float sourceListWidth = wideLayout ? availableWidth - controlsWidth - ImGui::GetStyle().ItemSpacing.x : availableWidth;
        const float sourceListHeight = (std::max)(180.0f, wideLayout ? availableHeight : 220.0f);

        ImGui::BeginGroup();
        ImGui::TextColored(g_Colors.textDisabled, "Source reward");
        ImGui::BeginChild("##SeasonRewardSourceList", ImVec2(sourceListWidth, sourceListHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for (int i = 0; i < (int)g_CachedSeasonRewardItems.size(); ++i)
        {
            const SeasonRankRewardItemOption& option = g_CachedSeasonRewardItems[i];
            const bool selected = i == g_HackSettings.SeasonRewardSourceIndex;
            if (ImGui::Selectable(option.label.c_str(), selected))
                g_HackSettings.SeasonRewardSourceIndex = i;

            if (selected && ImGui::IsWindowAppearing())
                ImGui::SetScrollHereY(0.5f);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", option.label.c_str());
        }
        ImGui::EndChild();
        ImGui::EndGroup();

        if (wideLayout)
            ImGui::SameLine();
        else
            ImGui::Spacing();

        ImGui::BeginGroup();
        const SeasonRankRewardItemOption& source = g_CachedSeasonRewardItems[g_HackSettings.SeasonRewardSourceIndex];
        ImGui::TextColored(g_Colors.textDisabled, "Selected source");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + controlsWidth);
        ImGui::TextWrapped("%s", source.label.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Checkbox("Apply to all ranks", &g_HackSettings.SeasonRewardApplyAllRanks);
        if (!g_HackSettings.SeasonRewardApplyAllRanks)
            ImAdd::SliderInt("Target rank", &g_HackSettings.SeasonRewardTargetRank, 1, 200, "%d");

        ImAdd::SliderInt("Reward quantity", &g_HackSettings.SeasonRewardQuantity, 1, 999999, "%d");

        static const char* targetSlots[] = { "Free", "Premium", "Both" };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::Combo("Target slot", &g_HackSettings.SeasonRewardTargetSlot, targetSlots, IM_ARRAYSIZE(targetSlots));

        int slotMask = 0;
        if (g_HackSettings.SeasonRewardTargetSlot == 0)
            slotMask = 1;
        else if (g_HackSettings.SeasonRewardTargetSlot == 1)
            slotMask = 2;
        else
            slotMask = 3;

        if (FullWidthButton("REPLACE SEASON RANK REWARD"))
        {
            InGameHack_ReplaceSeasonRankRewardsFromExistingReward(
                source.arrayIndex,
                source.slot,
                g_HackSettings.SeasonRewardTargetRank,
                g_HackSettings.SeasonRewardQuantity,
                slotMask,
                g_HackSettings.SeasonRewardApplyAllRanks);
        }
        ImGui::EndGroup();
    }

    static void DrawLicenseBackendClaimTestControls()
    {
        SeparatorLabel("Backend claim tests");

        ImAdd::SliderInt("Repeat count", &g_HackSettings.LicenseClaimRepeatCount, 1, 20, "%d");
        ImAdd::SliderInt("Delay ms", &g_HackSettings.LicenseClaimDelayMs, 50, 10000, "%d");

        ImAdd::SliderInt("Season code", &g_HackSettings.LicenseClaimSeasonCode, 1, 1000, "%d");
        ImAdd::SliderInt("Free rank", &g_HackSettings.LicenseClaimFreeRank, 0, 200, "%d");
        ImAdd::SliderInt("Premium rank", &g_HackSettings.LicenseClaimPremiumRank, 0, 200, "%d");
        if (FullWidthButton("SEND RECEIVE LICENSE NOW"))
        {
            InGameHack_ReceiveLicenseClaimTest(
                g_HackSettings.LicenseClaimSeasonCode,
                g_HackSettings.LicenseClaimFreeRank,
                g_HackSettings.LicenseClaimPremiumRank,
                g_HackSettings.LicenseClaimRepeatCount);
        }
        if (FullWidthButton("START TIMED RECEIVE LICENSE"))
            StartTimedLicenseClaimTest(false);

        ImGui::Spacing();
        ImAdd::SliderInt("Special rank", &g_HackSettings.LicenseClaimSpecialRank, 1, 200, "%d");
        if (FullWidthButton("SEND RECEIVE SPECIAL LICENSE NOW"))
        {
            InGameHack_ReceiveSpecialLicenseClaimTest(
                g_HackSettings.LicenseClaimSpecialRank,
                g_HackSettings.LicenseClaimRepeatCount);
        }
        if (FullWidthButton("START TIMED RECEIVE SPECIAL LICENSE"))
            StartTimedLicenseClaimTest(true);

        if (g_TimedLicenseClaimTest.active)
        {
            ImGui::Spacing();
            ImGui::TextColored(
                g_Colors.warning,
                "Timed test: %d/%d sent, delay %d ms",
                g_TimedLicenseClaimTest.sentCount,
                g_TimedLicenseClaimTest.repeatTotal,
                g_TimedLicenseClaimTest.delayMs);
            if (FullWidthButton("STOP TIMED CLAIM TEST"))
                g_TimedLicenseClaimTest.active = false;
        }
    }

    static void RefreshRecoveryTeamCache(bool force = false)
    {
        DWORD now = GetTickCount();
        if (!force && g_LastRecoveryTeamRefreshTick != 0 &&
            (now - g_LastRecoveryTeamRefreshTick) < GAME_LIST_CACHE_INTERVAL_MS)
            return;

        g_CurrentTeamCharacters = InGameHack_GetTeamCharacterBattles();
        g_CachedRecoveryTeamNames = InGameHack_GetTeamCharacterNames();

        if (g_Settings.SelectedRecoveryTeamIndex >= (int)g_CachedRecoveryTeamNames.size())
            g_Settings.SelectedRecoveryTeamIndex = -1;

        g_LastRecoveryTeamRefreshTick = now;
    }

    static void RefreshLobbyTeamsCache(bool force = false)
    {
        DWORD now = GetTickCount();
        if (!force && g_LastLobbyTeamsRefreshTick != 0 &&
            (now - g_LastLobbyTeamsRefreshTick) < GAME_LIST_CACHE_INTERVAL_MS)
            return;

        g_CachedLobbyMyTeamId = InGameHack_GetMyTeamId();
        g_CachedLobbyTeams.clear();
        g_AvailableTeamIds.clear();

        Cheats::PlayerInfo players[128]{};
        const int playerCount = Cheats::CollectLobbyPlayerList(players, (int)(sizeof(players) / sizeof(players[0])));
        g_CachedLobbyTeams.reserve((std::max)(playerCount, 0));

        for (int i = 0; i < playerCount; i++)
        {
            const int teamId = players[i].teamId;
            if (teamId < 0 || teamId >= 255)
                continue;

            const unsigned char teamByte = static_cast<unsigned char>(teamId);
            TeamListEntry* teamEntry = nullptr;
            for (TeamListEntry& entry : g_CachedLobbyTeams)
            {
                if (entry.teamId == teamByte)
                {
                    teamEntry = &entry;
                    break;
                }
            }

            if (!teamEntry)
            {
                TeamListEntry entry;
                entry.teamId = teamByte;
                g_CachedLobbyTeams.push_back(entry);
                g_AvailableTeamIds.push_back(teamByte);
                teamEntry = &g_CachedLobbyTeams.back();
            }

            teamEntry->playerNames.push_back(players[i].name[0] ? players[i].name : "Unknown");
        }

        std::sort(g_AvailableTeamIds.begin(), g_AvailableTeamIds.end());
        std::sort(g_CachedLobbyTeams.begin(), g_CachedLobbyTeams.end(), [](const TeamListEntry& lhs, const TeamListEntry& rhs)
            {
                return lhs.teamId < rhs.teamId;
            });

        g_LastLobbyTeamsRefreshTick = now;
    }

    static const TeamListEntry* FindCachedLobbyTeam(unsigned char teamId)
    {
        for (const TeamListEntry& entry : g_CachedLobbyTeams)
        {
            if (entry.teamId == teamId)
                return &entry;
        }

        return nullptr;
    }

    struct CharacterEditorState
    {
        int selectedVariationIndex = 0;
        int techniqueLevel = 9;
        int selectedCostumeIndex = 0;
        int lastVariationIndex = -1;
        std::vector<int> availableCostumes;
        std::vector<std::string> costumeNames;
    };

    static void RugirHeaderText(const char* title)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.Fonts->Fonts.Size > 1)
            ImGui::PushFont(io.Fonts->Fonts[1]);
        ImGui::TextUnformatted(title);
        if (io.Fonts->Fonts.Size > 1)
            ImGui::PopFont();
    }

    static void RugirHeaderToggle(const char* title, bool* enabled)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, style.ChildBorderSize);
        if (io.Fonts->Fonts.Size > 1)
            ImGui::PushFont(io.Fonts->Fonts[1]);
        ImAdd::ToggleButton(title, enabled);
        if (io.Fonts->Fonts.Size > 1)
            ImGui::PopFont();
        ImGui::PopStyleVar();
    }

    static bool BeginRugirCard(const char* id, const char* title, ImVec2 size, bool* headerToggle = nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(8.0f / 255.0f, 9.0f / 255.0f, 14.0f / 255.0f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.20f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        bool visible = ImAdd::BeginChild(id, size);
        if (visible)
        {
            if (headerToggle)
                RugirHeaderToggle(title, headerToggle);
            else
                RugirHeaderText(title);
        }

        return visible;
    }

    static void EndRugirCard()
    {
        ImAdd::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    static void SeparatorLabel(const char* label)
    {
        ImAdd::SeparatorText(label);
    }

    static bool FullWidthButton(const char* label, float height)
    {
        if (height <= 0.0f)
            height = 28.0f;
        return ImAdd::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, height));
    }

    static unsigned int HashLicenseUnlockText(const char* text)
    {
        unsigned int hash = 2166136261u;
        if (!text)
            return hash;

        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
        {
            hash ^= *p;
            hash *= 16777619u;
        }

        return hash;
    }

    static bool ValidateLicenseUnlockPassword()
    {
        constexpr unsigned int kHashPartA = 0x71A80B48u;
        constexpr unsigned int kHashPartB = 0x54455255u;
        return HashLicenseUnlockText(g_LicenseUnlockBuffer) == (kHashPartA ^ kHashPartB);
    }

    static void RenderProtectedAccessCard(float width)
    {
        if (BeginRugirCard("license-activation-page", "ACTIVATION", ImVec2(width, 0.0f)))
        {
            const Auth::State state = Auth::GetState();
            const bool authorized = Auth::IsAuthorized();

            if (authorized)
            {
                const std::string tier = Auth::GetTier();
                ImGui::TextColored(g_Colors.success, "Licence active%s%s.",
                    tier.empty() ? "" : " - ", tier.c_str());
                ImGui::Spacing();
                if (FullWidthButton("DECONNEXION"))
                {
                    Auth::Clear();
                    std::memset(g_LicenseKeyBuffer, 0, sizeof(g_LicenseKeyBuffer));
                }
            }
            else
            {
                const bool busy = (state == Auth::State::Checking);

                ImGui::BeginDisabled(busy);
                const bool submitted = ImGui::InputTextWithHint(
                    "##LicenseKey",
                    "RUGIR-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX",
                    g_LicenseKeyBuffer,
                    sizeof(g_LicenseKeyBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue);

                const bool activateClicked = FullWidthButton(busy ? "VERIFICATION..." : "ACTIVER");
                ImGui::EndDisabled();

                if ((submitted || activateClicked) && !busy)
                    Auth::ActivateAsync(g_LicenseKeyBuffer);

                // Status line: green while checking, red on refusal.
                const std::string status = Auth::GetStatusText();
                if (!status.empty())
                {
                    const ImVec4 color = (state == Auth::State::Denied) ? g_Colors.danger
                        : (busy ? g_Colors.warning : g_Colors.textSecondary);
                    ImGui::TextColored(color, "%s", status.c_str());
                }
            }

            ImGui::Spacing();
            ImGui::TextColored(g_Colors.textSecondary, "HWID: %s", Auth::GetHwidShort().c_str());
        }
        EndRugirCard();
    }

    static void DrawInventoryDebugSettingsSection()
    {
        SeparatorLabel("Inventory Debug");
        if (FullWidthButton("DUMP INVENTORY -> C:/temp"))
        {
            InGameHack_LogInventory();
        }
        if (FullWidthButton("DROP ALL SUPPLIES (DUPE)"))
        {
            InGameHack_LogDropInventorySupplies();
        }
        ImGui::TextDisabled("Writes C:/temp/rugir_inventory.log (inventory + map supply serials)");
    }

    static void DrawDllControlSettingsSection()
    {
#if RUGIR_ENABLE_UNLOAD
        SeparatorLabel("DLL Control");
        ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.danger);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.35f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.08f, 0.25f, 1.0f));

        if (FullWidthButton("SAFE UNLOAD", 32.0f))
        {
            g_Settings.EnableGlobal = false;
            g_Visible = false;
            g_PlayerNetworkTableVisible = false;
            DLL_Unload();
        }

        ImGui::PopStyleColor(3);
#endif
    }

    static bool SelectableRow(const char* label, bool selected)
    {
        return ImAdd::SelectableLabel(label, selected, false, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
    }

    static void DrawHotkeyConfigButton(const char* label, const HotkeySet& hotkey, int hotkeyId)
    {
        SeparatorLabel(label);
        std::string hotkeyLabel = "[KB] " + GetKeyName(hotkey.Keyboard, 0) +
                                  " | [X] " + GetKeyName(hotkey.Xbox, 1) +
                                  " | [PS] " + GetKeyName(hotkey.PS4, 2);
        ImGui::PushID(label);
        ImGui::PushID(hotkeyId);
        if (ImAdd::Button(hotkeyLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 28.0f)))
        {
            StartHotkeyListening(hotkeyId);
        }
        ImGui::PopID();
        ImGui::PopID();
    }

    static void DrawCompactButtonRow(const char* first, const char* second = nullptr, const char* third = nullptr)
    {
        const char* labels[3] = { first, second, third };
        int count = 1;
        if (second) count++;
        if (third) count++;

        const float spacing = 6.0f;
        float width = std::floor((ImGui::GetContentRegionAvail().x - spacing * (float)(count - 1)) / (float)count);

        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, spacing);
            ImAdd::Button(labels[i], ImVec2(width, 26.0f));
        }
    }

    using ButtonAction = void (*)();

    static void DrawActionButtonRow(const char* first, ButtonAction firstAction, const char* second = nullptr, ButtonAction secondAction = nullptr, const char* third = nullptr, ButtonAction thirdAction = nullptr)
    {
        const char* labels[3] = { first, second, third };
        ButtonAction actions[3] = { firstAction, secondAction, thirdAction };
        int count = 1;
        if (second) count++;
        if (third) count++;

        const float spacing = 6.0f;
        float width = std::floor((ImGui::GetContentRegionAvail().x - spacing * (float)(count - 1)) / (float)count);

        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, spacing);
            if (ImAdd::Button(labels[i], ImVec2(width, 26.0f)) && actions[i])
                actions[i]();
        }
    }

    static void RefreshCharacterEditorCostumes(CharacterEditorState& state)
    {
        if (state.selectedVariationIndex == state.lastVariationIndex)
            return;

        state.lastVariationIndex = state.selectedVariationIndex;
        auto [characterId, variationId] = GetCharacterAndVariationFromIndex(state.selectedVariationIndex);
        state.availableCostumes = CostumeHelper::GetCostumesForCharacter((int)characterId);
        state.costumeNames.clear();
        state.costumeNames.reserve(state.availableCostumes.size());

        for (size_t i = 0; i < state.availableCostumes.size(); i++)
            state.costumeNames.push_back(CostumeHelper::FormatCostumeName(state.availableCostumes[i], i));

        if (state.selectedCostumeIndex >= (int)state.costumeNames.size())
            state.selectedCostumeIndex = 0;
    }

    static int GetSelectedCostumeCode(CharacterEditorState& state)
    {
        RefreshCharacterEditorCostumes(state);
        if (state.selectedCostumeIndex >= 0 && state.selectedCostumeIndex < (int)state.availableCostumes.size())
            return state.availableCostumes[state.selectedCostumeIndex];

        return 0;
    }

    static void DrawCostumeSelector(CharacterEditorState& state)
    {
        RefreshCharacterEditorCostumes(state);
        SeparatorLabel("Costume");

        if (state.costumeNames.empty())
        {
            ImGui::TextColored(g_Colors.textDisabled, "No costume list available");
            return;
        }

        if (state.selectedCostumeIndex < 0 || state.selectedCostumeIndex >= (int)state.costumeNames.size())
            state.selectedCostumeIndex = 0;

        const char* currentCostume = state.costumeNames[state.selectedCostumeIndex].c_str();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, 280.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 3.0f));
        if (ImGui::BeginCombo("##CostumeSelector", currentCostume, ImGuiComboFlags_HeightLarge))
        {
            for (int i = 0; i < (int)state.costumeNames.size(); i++)
            {
                const bool selected = state.selectedCostumeIndex == i;
                if (ImGui::Selectable(state.costumeNames[i].c_str(), selected))
                    state.selectedCostumeIndex = i;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar();
    }

    static void SelectedCharacterSummary(int selectedVariationIndex)
    {
        auto [characterId, variationId] = GetCharacterAndVariationFromIndex(selectedVariationIndex);
        SeparatorLabel("Selected Character");
        ImGui::TextWrapped("%s", GetCharacterVariationDisplayName(selectedVariationIndex));
        ImGui::TextColored(g_Colors.textDisabled, "Character ID: %d | Variation: %d", characterId, variationId);
    }

    template<typename ApplyFn>
    static void RenderCharacterConfigCard(const char* id, const char* title, const char* actionLabel, CharacterEditorState& state, ApplyFn applyFn, bool actionEnabled = true, const char* disabledLabel = "Select a target first", bool showIconGrid = false)
    {
        if (BeginRugirCard(id, title, ImVec2(0.0f, 0.0f)))
        {
            ImGui::PushID(id);
            SelectedCharacterSummary(state.selectedVariationIndex);

            if (ImAdd::SliderInt("Technique Level", &state.techniqueLevel, 1, 9))
            {
                s_techAlpha = state.techniqueLevel;
                s_techBeta = state.techniqueLevel;
                s_techGamma = state.techniqueLevel;
            }

            DrawCostumeSelector(state);

            if (showIconGrid)
            {
                SeparatorLabel("Character icons");
                CharacterIconGridPanel("lobby-character-icons", state.selectedVariationIndex, 0.0f, 38.0f, true);
            }

            const char* buttonLabel = actionEnabled ? actionLabel : disabledLabel;
            if (!actionEnabled)
                ImGui::BeginDisabled(true);

            if (FullWidthButton(buttonLabel) && actionEnabled)
            {
                auto [characterId, variationId] = GetCharacterAndVariationFromIndex(state.selectedVariationIndex);
                int costumeCode = GetSelectedCostumeCode(state);
                s_costumeCode = costumeCode;
                applyFn(characterId, variationId, costumeCode);
            }

            if (!actionEnabled)
                ImGui::EndDisabled();

            ImGui::PopID();
        }
        EndRugirCard();
    }

    static void RenderPreviewEspPage(int subtab, float groupWidth)
    {
        (void)subtab;
        if (BeginRugirCard("esp-page", "ESP", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "esp-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            RugirHeaderToggle("ESP CONTROL", &g_Settings.EnablePlayerESP);
            if (g_Settings.EnablePlayerESP)
            {
                ImAdd::CheckBox("Show Enemy", &g_Settings.ShowEnemies);
                ImAdd::CheckBox("Show Team", &g_Settings.ShowTeam);
                ImAdd::CheckBox("LobbyCharacter", &g_Settings.ShowLobbyCharacters);
                ImAdd::CheckBox("KOTA", &g_Settings.ShowKota);
            }

            SeparatorLabel("Render");
            ImAdd::CheckBox("Show Skeleton", &g_Settings.ShowPlayerSkeleton);
            ImAdd::SliderFloat("Max Draw Distance", &g_Settings.Player_DrawDistance, 100.0f, 5000.0f, "%.0f");

            ImGui::NextColumn();

            SeparatorLabel("Display Info");
            ImAdd::CheckBox("HP", &g_Settings.ShowHP);
            ImAdd::CheckBox("GP", &g_Settings.ShowGP);
            ImAdd::CheckBox("PU", &g_Settings.ShowPU);
            ImAdd::CheckBox("Platform", &g_Settings.ShowPlatform);
            ImAdd::CheckBox("Team ID", &g_Settings.ShowTeamId);
            ImAdd::CheckBox("Server / Ping", &g_Settings.ShowServerStatusOverlay);

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static std::mutex g_Ch202Unique3DefaultsMutex;
    static Ch202Unique3ParamsConfig g_Ch202Unique3PendingDefaults{};
    static Ch202Unique3ParamsConfig g_Ch202Unique3LoadedDefaults{};
    static bool g_Ch202Unique3DefaultsPending = false;
    static bool g_Ch202Unique3DefaultsLoaded = false;
    static bool g_Ch202Unique3LoadedDefaultsValid = false;
    static bool g_Ch202Unique3DefaultsRequestInFlight = false;
    static DWORD g_Ch202Unique3LastDefaultRequestTick = 0;
    static Ch202Unique3ParamsConfig g_Ch202Unique3PendingApply{};
    static bool g_Ch202Unique3ApplyPending = false;
    static DWORD g_Ch202Unique3LastApplyTick = 0;
    static DWORD g_Ch202Unique3LastChangeTick = 0;
    static std::atomic_bool g_Ch202Unique3ApplyInFlight = false;
    static std::vector<std::string> g_ClearInvincibleTargetNames;
    static DWORD g_ClearInvincibleLastTargetRefreshTick = 0;

    static void ApplyCh202Unique3ConfigToSettings(const Ch202Unique3ParamsConfig& config)
    {
        g_Settings.Ch202MeleeAChaseHeightOffset = config.meleeAChaseHeightOffset;
        g_Settings.Ch202MeleeAChaseStartSpeedMax = config.meleeAChaseStartSpeedMax;
        g_Settings.Ch202MeleeAChaseStartSpeedMin = config.meleeAChaseStartSpeedMin;
        g_Settings.Ch202MeleeAChaseLastSpeed = config.meleeAChaseLastSpeed;
        g_Settings.Ch202MeleeAChaseSpeedSpan = config.meleeAChaseSpeedSpan;
        g_Settings.Ch202MeleeAChaseSpan = config.meleeAChaseSpan;
        g_Settings.Ch202MeleeADistance = config.meleeADistance;
        g_Settings.Ch202MeleeABreakTargetSpeed = config.meleeABreakTargetSpeed;
        g_Settings.Ch202MeleeABreakTargetSpan = config.meleeABreakTargetSpan;
        g_Settings.Ch202Unique1HoldTime = config.unique1HoldTime;
        g_Settings.Ch202Unique2NeedFieldTime = config.unique2NeedFieldTime;
        g_Settings.Ch202Unique2HoldTime = config.unique2HoldTime;
        g_Settings.Ch202Unique2MoveTime = config.unique2MoveTime;
        g_Settings.Ch202Unique2MoveSpeedMin = config.unique2MoveSpeedMin;
        g_Settings.Ch202Unique2MoveSpeedMax = config.unique2MoveSpeedMax;
        g_Settings.Ch202Unique2QuintupleRiseSlideSpeed = config.unique2QuintupleRiseSlideSpeed;
        g_Settings.Ch202Unique2QuintupleRiseSlideSpan = config.unique2QuintupleRiseSlideSpan;
        g_Settings.Ch202Unique2QuintupleFallSlideSpeed = config.unique2QuintupleFallSlideSpeed;
        g_Settings.Ch202Unique2QuintupleFallSlideSpan = config.unique2QuintupleFallSlideSpan;
        g_Settings.Ch202Unique2AfterLevel = config.unique2AfterLevel;
        g_Settings.Ch202Unique2QuintupleAllAmmoConsumed = config.unique2QuintupleAllAmmoConsumed;
        g_Settings.Ch202U3InitTransMissionLevel = std::clamp(config.initTransMissionLevel, 1, 5);
        g_Settings.Ch202U3EndDistanceOfFollowing = config.endDistanceOfFollowing;
        g_Settings.Ch202U3MoveSpeedStart = config.moveSpeedStart;
        g_Settings.Ch202U3MoveSpeedEnd = config.moveSpeedEnd;
        g_Settings.Ch202U3SpeedChangeTime = config.speedChangeTime;
        g_Settings.Ch202U3ExitSpeedStart = config.exitSpeedStart;
        g_Settings.Ch202U3ExitSpeedEnd = config.exitSpeedEnd;
        g_Settings.Ch202U3ExitChangeTime = config.exitChangeTime;
        g_Settings.Ch202U3LimitCount = config.limitCount;
        g_Settings.Ch202U3MoveMagazinePercentMin = config.moveMagazinePercentMin;
        g_Settings.Ch202U3MoveMagazinePercentMax = config.moveMagazinePercentMax;
        g_Settings.Ch202U3PunchMagazinePercentMin = config.punchMagazinePercentMin;
        g_Settings.Ch202U3PunchMagazinePercentMax = config.punchMagazinePercentMax;
        g_Settings.Ch202U3RecoverMagazineBase = config.recoverMagazineBase;
        g_Settings.Ch202U3RecoverMagazineAdd = config.recoverMagazineAdd;
        g_Settings.Ch202U3DelayCancelTimer = config.delayCancelTimer;
        g_Settings.Ch202U3LockOnSec = config.lockOnSec;
        g_Settings.Ch202U3LockOnMinSec = config.lockOnMinSec;
        g_Settings.Ch202U3LockOnRange = config.lockOnRange;
        g_Settings.Ch202U3LockOnAttackTargetCheckType = std::clamp(config.lockOnAttackTargetCheckType, 0, 2);
        g_Settings.Ch202U3CurveHorizontalDistanceMin = config.curveHorizontalDistanceMin;
        g_Settings.Ch202U3CurveHorizontalDistanceMax = config.curveHorizontalDistanceMax;
        g_Settings.Ch202U3MiddleUpOffset = config.middleUpOffset;
        g_Settings.Ch202U3TargetOffset = config.targetOffset;
        g_Settings.Ch202U3SplitDistance = config.splitDistance;
        g_Settings.Ch202U3SplitLerpRate = config.splitLerpRate;
        g_Settings.Ch202U3ControlPointsRate = config.controlPointsRate;
        g_Settings.Ch202U3MinDetroitRange = config.minDetroitRange;
        g_Settings.Ch202U3MinDetroitSpeed = config.minDetroitSpeed;
        g_Settings.Ch202U3MaxDetroitRange = config.maxDetroitRange;
        g_Settings.Ch202U3MaxDetroitSpeed = config.maxDetroitSpeed;
        g_Settings.Ch202U3MiddleOffsetX = config.middleOffsetX;
        g_Settings.Ch202U3MiddleOffsetY = config.middleOffsetY;
        g_Settings.Ch202U3MiddleOffsetZ = config.middleOffsetZ;
        g_Settings.Ch202U3MiddleOffsetAerialX = config.middleOffsetAerialX;
        g_Settings.Ch202U3MiddleOffsetAerialY = config.middleOffsetAerialY;
        g_Settings.Ch202U3MiddleOffsetAerialZ = config.middleOffsetAerialZ;
        g_Settings.Ch202SpecialHoldTime = config.specialHoldTime;
        g_Settings.Ch202SpecialActivationTime = config.specialActivationTime;
        g_Settings.Ch202SpecialSmokeMagazineCost = config.specialSmokeMagazineCost;
        g_Settings.Ch202SpecialLegCount = config.specialLegCount;
        g_Settings.Ch202SpecialJumpVerticalSpan = config.specialJumpVerticalSpan;
        g_Settings.Ch202SpecialJumpVerticalHeight = config.specialJumpVerticalHeight;
        g_Settings.Ch202SpecialJumpForwardSpan = config.specialJumpForwardSpan;
        g_Settings.Ch202SpecialJumpForwardHeight = config.specialJumpForwardHeight;
        g_Settings.Ch202SpecialJumpForwardInitialSpeedH = config.specialJumpForwardInitialSpeedH;
        g_Settings.Ch202SpecialJumpForwardLastSpeedH = config.specialJumpForwardLastSpeedH;
        g_Settings.Ch202SpecialJumpForwardAccelSpanH = config.specialJumpForwardAccelSpanH;
        g_Settings.Ch202SpecialWallJumpSpan = config.specialWallJumpSpan;
        g_Settings.Ch202SpecialWallJumpHeight = config.specialWallJumpHeight;
        g_Settings.Ch202SpecialWallJumpInitialSpeed = config.specialWallJumpInitialSpeed;
        g_Settings.Ch202SpecialWallJumpLastSpeed = config.specialWallJumpLastSpeed;
        g_Settings.Ch202ConditionAnimationMultiplyRate = config.conditionAnimationMultiplyRate;
        g_Settings.Ch202ConditionMoveMultiplyRate = config.conditionMoveMultiplyRate;
        g_Settings.Ch202ConditionJumpAdjustMultiplyRate = config.conditionJumpAdjustMultiplyRate;
    }

    static void PublishCh202Unique3Defaults(const Ch202Unique3ParamsConfig& config)
    {
        std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
        g_Ch202Unique3PendingDefaults = config;
        if (!g_Ch202Unique3LoadedDefaultsValid)
        {
            g_Ch202Unique3LoadedDefaults = config;
            g_Ch202Unique3LoadedDefaultsValid = true;
        }
        g_Ch202Unique3DefaultsPending = true;
        g_Ch202Unique3DefaultsLoaded = true;
        g_Ch202Unique3DefaultsRequestInFlight = false;
    }

    static void FinishCh202Unique3DefaultsRequest()
    {
        std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
        g_Ch202Unique3DefaultsRequestInFlight = false;
    }

    static bool ConsumeCh202Unique3Defaults()
    {
        Ch202Unique3ParamsConfig config{};
        {
            std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
            if (!g_Ch202Unique3DefaultsPending)
                return false;

            config = g_Ch202Unique3PendingDefaults;
            g_Ch202Unique3DefaultsPending = false;
        }

        ApplyCh202Unique3ConfigToSettings(config);
        return true;
    }

    static bool HasCh202Unique3DefaultsLoaded()
    {
        std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
        return g_Ch202Unique3DefaultsLoaded;
    }

    static bool TryGetCh202Unique3LoadedDefaults(Ch202Unique3ParamsConfig& outConfig)
    {
        std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
        if (!g_Ch202Unique3LoadedDefaultsValid)
            return false;

        outConfig = g_Ch202Unique3LoadedDefaults;
        return true;
    }

    static void RequestCh202Unique3DefaultsFromSdk(bool force)
    {
        {
            std::lock_guard<std::mutex> lock(g_Ch202Unique3DefaultsMutex);
            const DWORD now = GetTickCount();
            if (!force && (g_Ch202Unique3DefaultsLoaded || g_Ch202Unique3DefaultsRequestInFlight))
                return;

            if (!force && g_Ch202Unique3LastDefaultRequestTick != 0 && now - g_Ch202Unique3LastDefaultRequestTick < 1000)
                return;

            g_Ch202Unique3DefaultsRequestInFlight = true;
            g_Ch202Unique3LastDefaultRequestTick = now;
        }

        if (!EnqueueGameThreadMenuTask([]() {
            Ch202Unique3ParamsConfig config{};
            if (InGameHack_TryReadCh202Unique3Params(config))
                PublishCh202Unique3Defaults(config);
            else
                FinishCh202Unique3DefaultsRequest();
        }, "Load CH202 Unique3 SDK Params"))
        {
            FinishCh202Unique3DefaultsRequest();
        }
    }

    static Ch202Unique3ParamsConfig BuildCh202Unique3ParamsConfig()
    {
        Ch202Unique3ParamsConfig config{};
        config.meleeAChaseHeightOffset = g_Settings.Ch202MeleeAChaseHeightOffset;
        config.meleeAChaseStartSpeedMax = g_Settings.Ch202MeleeAChaseStartSpeedMax;
        config.meleeAChaseStartSpeedMin = g_Settings.Ch202MeleeAChaseStartSpeedMin;
        config.meleeAChaseLastSpeed = g_Settings.Ch202MeleeAChaseLastSpeed;
        config.meleeAChaseSpeedSpan = g_Settings.Ch202MeleeAChaseSpeedSpan;
        config.meleeAChaseSpan = g_Settings.Ch202MeleeAChaseSpan;
        config.meleeADistance = g_Settings.Ch202MeleeADistance;
        config.meleeABreakTargetSpeed = g_Settings.Ch202MeleeABreakTargetSpeed;
        config.meleeABreakTargetSpan = g_Settings.Ch202MeleeABreakTargetSpan;
        config.unique1HoldTime = g_Settings.Ch202Unique1HoldTime;
        config.unique2NeedFieldTime = g_Settings.Ch202Unique2NeedFieldTime;
        config.unique2HoldTime = g_Settings.Ch202Unique2HoldTime;
        config.unique2MoveTime = g_Settings.Ch202Unique2MoveTime;
        config.unique2MoveSpeedMin = g_Settings.Ch202Unique2MoveSpeedMin;
        config.unique2MoveSpeedMax = g_Settings.Ch202Unique2MoveSpeedMax;
        config.unique2QuintupleRiseSlideSpeed = g_Settings.Ch202Unique2QuintupleRiseSlideSpeed;
        config.unique2QuintupleRiseSlideSpan = g_Settings.Ch202Unique2QuintupleRiseSlideSpan;
        config.unique2QuintupleFallSlideSpeed = g_Settings.Ch202Unique2QuintupleFallSlideSpeed;
        config.unique2QuintupleFallSlideSpan = g_Settings.Ch202Unique2QuintupleFallSlideSpan;
        config.unique2AfterLevel = g_Settings.Ch202Unique2AfterLevel;
        config.unique2QuintupleAllAmmoConsumed = g_Settings.Ch202Unique2QuintupleAllAmmoConsumed;
        config.initTransMissionLevel = std::clamp(g_Settings.Ch202U3InitTransMissionLevel, 1, 5);
        config.endDistanceOfFollowing = g_Settings.Ch202U3EndDistanceOfFollowing;
        config.moveSpeedStart = g_Settings.Ch202U3MoveSpeedStart;
        config.moveSpeedEnd = g_Settings.Ch202U3MoveSpeedEnd;
        config.speedChangeTime = g_Settings.Ch202U3SpeedChangeTime;
        config.exitSpeedStart = g_Settings.Ch202U3ExitSpeedStart;
        config.exitSpeedEnd = g_Settings.Ch202U3ExitSpeedEnd;
        config.exitChangeTime = g_Settings.Ch202U3ExitChangeTime;
        config.limitCount = g_Settings.Ch202U3LimitCount;
        config.moveMagazinePercentMin = g_Settings.Ch202U3MoveMagazinePercentMin;
        config.moveMagazinePercentMax = g_Settings.Ch202U3MoveMagazinePercentMax;
        config.punchMagazinePercentMin = g_Settings.Ch202U3PunchMagazinePercentMin;
        config.punchMagazinePercentMax = g_Settings.Ch202U3PunchMagazinePercentMax;
        config.recoverMagazineBase = g_Settings.Ch202U3RecoverMagazineBase;
        config.recoverMagazineAdd = g_Settings.Ch202U3RecoverMagazineAdd;
        config.delayCancelTimer = g_Settings.Ch202U3DelayCancelTimer;
        config.lockOnSec = g_Settings.Ch202U3LockOnSec;
        config.lockOnMinSec = g_Settings.Ch202U3LockOnMinSec;
        config.lockOnRange = g_Settings.Ch202U3LockOnRange;
        config.lockOnAttackTargetCheckType = g_Settings.Ch202U3LockOnAttackTargetCheckType;
        config.curveHorizontalDistanceMin = g_Settings.Ch202U3CurveHorizontalDistanceMin;
        config.curveHorizontalDistanceMax = g_Settings.Ch202U3CurveHorizontalDistanceMax;
        config.middleUpOffset = g_Settings.Ch202U3MiddleUpOffset;
        config.targetOffset = g_Settings.Ch202U3TargetOffset;
        config.splitDistance = g_Settings.Ch202U3SplitDistance;
        config.splitLerpRate = g_Settings.Ch202U3SplitLerpRate;
        config.controlPointsRate = g_Settings.Ch202U3ControlPointsRate;
        config.minDetroitRange = g_Settings.Ch202U3MinDetroitRange;
        config.minDetroitSpeed = g_Settings.Ch202U3MinDetroitSpeed;
        config.maxDetroitRange = g_Settings.Ch202U3MaxDetroitRange;
        config.maxDetroitSpeed = g_Settings.Ch202U3MaxDetroitSpeed;
        config.middleOffsetX = g_Settings.Ch202U3MiddleOffsetX;
        config.middleOffsetY = g_Settings.Ch202U3MiddleOffsetY;
        config.middleOffsetZ = g_Settings.Ch202U3MiddleOffsetZ;
        config.middleOffsetAerialX = g_Settings.Ch202U3MiddleOffsetAerialX;
        config.middleOffsetAerialY = g_Settings.Ch202U3MiddleOffsetAerialY;
        config.middleOffsetAerialZ = g_Settings.Ch202U3MiddleOffsetAerialZ;
        config.specialHoldTime = g_Settings.Ch202SpecialHoldTime;
        config.specialActivationTime = g_Settings.Ch202SpecialActivationTime;
        config.specialSmokeMagazineCost = g_Settings.Ch202SpecialSmokeMagazineCost;
        config.specialLegCount = g_Settings.Ch202SpecialLegCount;
        config.specialJumpVerticalSpan = g_Settings.Ch202SpecialJumpVerticalSpan;
        config.specialJumpVerticalHeight = g_Settings.Ch202SpecialJumpVerticalHeight;
        config.specialJumpForwardSpan = g_Settings.Ch202SpecialJumpForwardSpan;
        config.specialJumpForwardHeight = g_Settings.Ch202SpecialJumpForwardHeight;
        config.specialJumpForwardInitialSpeedH = g_Settings.Ch202SpecialJumpForwardInitialSpeedH;
        config.specialJumpForwardLastSpeedH = g_Settings.Ch202SpecialJumpForwardLastSpeedH;
        config.specialJumpForwardAccelSpanH = g_Settings.Ch202SpecialJumpForwardAccelSpanH;
        config.specialWallJumpSpan = g_Settings.Ch202SpecialWallJumpSpan;
        config.specialWallJumpHeight = g_Settings.Ch202SpecialWallJumpHeight;
        config.specialWallJumpInitialSpeed = g_Settings.Ch202SpecialWallJumpInitialSpeed;
        config.specialWallJumpLastSpeed = g_Settings.Ch202SpecialWallJumpLastSpeed;
        config.conditionAnimationMultiplyRate = g_Settings.Ch202ConditionAnimationMultiplyRate;
        config.conditionMoveMultiplyRate = g_Settings.Ch202ConditionMoveMultiplyRate;
        config.conditionJumpAdjustMultiplyRate = g_Settings.Ch202ConditionJumpAdjustMultiplyRate;
        return config;
    }

    static void QueueCh202Unique3Apply(const Ch202Unique3ParamsConfig& config)
    {
        g_Ch202Unique3PendingApply = config;
        g_Ch202Unique3ApplyPending = true;
        g_Ch202Unique3LastChangeTick = GetTickCount();
    }

    static void FlushCh202Unique3Apply(bool force)
    {
        if (!g_Ch202Unique3ApplyPending)
            return;

        const DWORD now = GetTickCount();
        constexpr DWORD ApplyDebounceMs = 220;
        if (!force && g_Ch202Unique3LastChangeTick != 0 && now - g_Ch202Unique3LastChangeTick < ApplyDebounceMs)
            return;

        if (g_Ch202Unique3ApplyInFlight.load())
            return;

        if (!force && g_Ch202Unique3LastApplyTick != 0 && now - g_Ch202Unique3LastApplyTick < ApplyDebounceMs)
            return;

        const Ch202Unique3ParamsConfig config = g_Ch202Unique3PendingApply;
        g_Ch202Unique3ApplyInFlight.store(true);
        if (EnqueueGameThreadMenuTask([config]() {
            InGameHack_ApplyCh202Unique3Params(config);
            g_Ch202Unique3ApplyInFlight.store(false);
        }, "CH202 Params Auto Apply"))
        {
            g_Ch202Unique3ApplyPending = false;
            g_Ch202Unique3LastApplyTick = now;
        }
        else
        {
            g_Ch202Unique3ApplyInFlight.store(false);
        }
    }

    static void RenderCh202Unique3ParamsPage(float groupWidth)
    {
        RequestCh202Unique3DefaultsFromSdk(false);
        ConsumeCh202Unique3Defaults();

        if (BeginRugirCard("character-ch202-u3-page", "CH202 SDK PARAMS", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "character-ch202-u3-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            if (!HasCh202Unique3DefaultsLoaded())
            {
                ImGui::TextUnformatted("Waiting for loaded SDK values...");
                if (FullWidthButton("RETRY LOAD SDK VALUES"))
                    RequestCh202Unique3DefaultsFromSdk(true);

                ImGui::Columns(1);
                ImGui::PopStyleVar();
                EndRugirCard();
                return;
            }

            bool changed = false;

            SeparatorLabel("Melee A");
            changed |= ImAdd::SliderFloat("Chase Height Offset", &g_Settings.Ch202MeleeAChaseHeightOffset, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Chase Start Speed Max", &g_Settings.Ch202MeleeAChaseStartSpeedMax, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Chase Start Speed Min", &g_Settings.Ch202MeleeAChaseStartSpeedMin, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Chase Last Speed", &g_Settings.Ch202MeleeAChaseLastSpeed, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Chase Speed Span", &g_Settings.Ch202MeleeAChaseSpeedSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Chase Span", &g_Settings.Ch202MeleeAChaseSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Melee Distance", &g_Settings.Ch202MeleeADistance, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Break Target Speed", &g_Settings.Ch202MeleeABreakTargetSpeed, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Break Target Span", &g_Settings.Ch202MeleeABreakTargetSpan, 0.0f, 30.0f, "%.2f");

            SeparatorLabel("Unique 1");
            changed |= ImAdd::SliderFloat("Unique 1 Hold Time", &g_Settings.Ch202Unique1HoldTime, 0.0f, 30.0f, "%.2f");

            SeparatorLabel("Unique 2");
            changed |= ImAdd::SliderFloat("Need Field Time", &g_Settings.Ch202Unique2NeedFieldTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Unique 2 Hold Time", &g_Settings.Ch202Unique2HoldTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Unique 2 Move Time", &g_Settings.Ch202Unique2MoveTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Unique 2 Move Speed Min", &g_Settings.Ch202Unique2MoveSpeedMin, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Unique 2 Move Speed Max", &g_Settings.Ch202Unique2MoveSpeedMax, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Rise Slide Speed", &g_Settings.Ch202Unique2QuintupleRiseSlideSpeed, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Rise Slide Span", &g_Settings.Ch202Unique2QuintupleRiseSlideSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Fall Slide Speed", &g_Settings.Ch202Unique2QuintupleFallSlideSpeed, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Fall Slide Span", &g_Settings.Ch202Unique2QuintupleFallSlideSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderInt("Unique 2 After Level", &g_Settings.Ch202Unique2AfterLevel, 1, 9);
            {
                const bool before = g_Settings.Ch202Unique2QuintupleAllAmmoConsumed;
                ImAdd::CheckBox("Quintuple All Ammo", &g_Settings.Ch202Unique2QuintupleAllAmmoConsumed);
                changed |= before != g_Settings.Ch202Unique2QuintupleAllAmmoConsumed;
            }

            ImGui::NextColumn();

            SeparatorLabel("Unique 3 Transmission");
            changed |= ImAdd::SliderInt("Init TransMission Level", &g_Settings.Ch202U3InitTransMissionLevel, 1, 5);
            changed |= ImAdd::SliderInt("Limit Count", &g_Settings.Ch202U3LimitCount, 1, 100);

            SeparatorLabel("Unique 3 Movement");
            changed |= ImAdd::SliderFloat("End Distance Following", &g_Settings.Ch202U3EndDistanceOfFollowing, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Move Speed Start", &g_Settings.Ch202U3MoveSpeedStart, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Move Speed End", &g_Settings.Ch202U3MoveSpeedEnd, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Speed Change Time", &g_Settings.Ch202U3SpeedChangeTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Exit Speed Start", &g_Settings.Ch202U3ExitSpeedStart, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Exit Speed End", &g_Settings.Ch202U3ExitSpeedEnd, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Exit Change Time", &g_Settings.Ch202U3ExitChangeTime, 0.0f, 30.0f, "%.2f");

            SeparatorLabel("Unique 3 Magazine");
            changed |= ImAdd::SliderFloat("Move Magazine Min", &g_Settings.Ch202U3MoveMagazinePercentMin, 0.0f, 100.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Move Magazine Max", &g_Settings.Ch202U3MoveMagazinePercentMax, 0.0f, 100.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Punch Magazine Min", &g_Settings.Ch202U3PunchMagazinePercentMin, 0.0f, 100.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Punch Magazine Max", &g_Settings.Ch202U3PunchMagazinePercentMax, 0.0f, 100.0f, "%.2f");
            changed |= ImAdd::SliderInt("Recover Magazine Base", &g_Settings.Ch202U3RecoverMagazineBase, 0, 1000);
            changed |= ImAdd::SliderInt("Recover Magazine Add", &g_Settings.Ch202U3RecoverMagazineAdd, 0, 1000);
            changed |= ImAdd::SliderFloat("Delay Cancel Timer", &g_Settings.Ch202U3DelayCancelTimer, 0.0f, 30.0f, "%.2f");

            SeparatorLabel("Lock On");
            changed |= ImAdd::SliderFloat("Lock On Sec", &g_Settings.Ch202U3LockOnSec, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Lock On Min Sec", &g_Settings.Ch202U3LockOnMinSec, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Lock On Range", &g_Settings.Ch202U3LockOnRange, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderInt("Target Check Type", &g_Settings.Ch202U3LockOnAttackTargetCheckType, 0, 2);

            SeparatorLabel("Curve");
            changed |= ImAdd::SliderFloat("Curve Distance Min", &g_Settings.Ch202U3CurveHorizontalDistanceMin, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Curve Distance Max", &g_Settings.Ch202U3CurveHorizontalDistanceMax, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Middle Up Offset", &g_Settings.Ch202U3MiddleUpOffset, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Target Offset", &g_Settings.Ch202U3TargetOffset, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Split Distance", &g_Settings.Ch202U3SplitDistance, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Split Lerp Rate", &g_Settings.Ch202U3SplitLerpRate, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Control Points Rate", &g_Settings.Ch202U3ControlPointsRate, 0.0f, 30.0f, "%.2f");

            SeparatorLabel("Detroit");
            changed |= ImAdd::SliderFloat("Min Detroit Range", &g_Settings.Ch202U3MinDetroitRange, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Min Detroit Speed", &g_Settings.Ch202U3MinDetroitSpeed, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Max Detroit Range", &g_Settings.Ch202U3MaxDetroitRange, 0.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Max Detroit Speed", &g_Settings.Ch202U3MaxDetroitSpeed, 0.0f, 10000.0f, "%.2f");

            SeparatorLabel("Offsets");
            changed |= ImAdd::SliderFloat("Middle Offset X", &g_Settings.Ch202U3MiddleOffsetX, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Middle Offset Y", &g_Settings.Ch202U3MiddleOffsetY, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Middle Offset Z", &g_Settings.Ch202U3MiddleOffsetZ, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Aerial Offset X", &g_Settings.Ch202U3MiddleOffsetAerialX, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Aerial Offset Y", &g_Settings.Ch202U3MiddleOffsetAerialY, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Aerial Offset Z", &g_Settings.Ch202U3MiddleOffsetAerialZ, -10000.0f, 10000.0f, "%.2f");

            SeparatorLabel("Special");
            changed |= ImAdd::SliderFloat("Special Hold Time", &g_Settings.Ch202SpecialHoldTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Activation Time", &g_Settings.Ch202SpecialActivationTime, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderInt("Smoke Magazine Cost", &g_Settings.Ch202SpecialSmokeMagazineCost, 0, 1000);
            changed |= ImAdd::SliderInt("Leg Count", &g_Settings.Ch202SpecialLegCount, 0, 100);
            changed |= ImAdd::SliderFloat("Jump Vertical Span", &g_Settings.Ch202SpecialJumpVerticalSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Vertical Height", &g_Settings.Ch202SpecialJumpVerticalHeight, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Forward Span", &g_Settings.Ch202SpecialJumpForwardSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Forward Height", &g_Settings.Ch202SpecialJumpForwardHeight, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Forward Initial Speed H", &g_Settings.Ch202SpecialJumpForwardInitialSpeedH, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Forward Last Speed H", &g_Settings.Ch202SpecialJumpForwardLastSpeedH, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Forward Accel Span H", &g_Settings.Ch202SpecialJumpForwardAccelSpanH, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Wall Jump Span", &g_Settings.Ch202SpecialWallJumpSpan, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Wall Jump Height", &g_Settings.Ch202SpecialWallJumpHeight, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Wall Jump Initial Speed", &g_Settings.Ch202SpecialWallJumpInitialSpeed, -10000.0f, 10000.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Wall Jump Last Speed", &g_Settings.Ch202SpecialWallJumpLastSpeed, -10000.0f, 10000.0f, "%.2f");

            SeparatorLabel("Condition");
            changed |= ImAdd::SliderFloat("Animation Multiply Rate", &g_Settings.Ch202ConditionAnimationMultiplyRate, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Move Multiply Rate", &g_Settings.Ch202ConditionMoveMultiplyRate, 0.0f, 30.0f, "%.2f");
            changed |= ImAdd::SliderFloat("Jump Adjust Multiply Rate", &g_Settings.Ch202ConditionJumpAdjustMultiplyRate, 0.0f, 30.0f, "%.2f");

            ImGui::Columns(1);

            g_Settings.Ch202U3InitTransMissionLevel = std::clamp(g_Settings.Ch202U3InitTransMissionLevel, 1, 5);
            g_Settings.Ch202U3LockOnAttackTargetCheckType = std::clamp(g_Settings.Ch202U3LockOnAttackTargetCheckType, 0, 2);
            g_Settings.Ch202Unique2AfterLevel = std::clamp(g_Settings.Ch202Unique2AfterLevel, 1, 9);

            if (changed)
            {
                const Ch202Unique3ParamsConfig config = BuildCh202Unique3ParamsConfig();
                QueueCh202Unique3Apply(config);
            }

            if (FullWidthButton("RESET DEFAULTS"))
            {
                Ch202Unique3ParamsConfig defaults{};
                if (TryGetCh202Unique3LoadedDefaults(defaults))
                {
                    ApplyCh202Unique3ConfigToSettings(defaults);
                    QueueCh202Unique3Apply(defaults);
                    FlushCh202Unique3Apply(true);
                }
            }

            FlushCh202Unique3Apply(false);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static void RefreshClearInvincibleTargetNames(bool force)
    {
        const DWORD now = GetTickCount();
        if (!force &&
            g_ClearInvincibleLastTargetRefreshTick != 0 &&
            now - g_ClearInvincibleLastTargetRefreshTick < 1000)
        {
            return;
        }

        g_ClearInvincibleTargetNames = InGameHack_GetClearInvincibleTargetNames();
        g_ClearInvincibleLastTargetRefreshTick = now;

        if (g_ClearInvincibleTargetNames.empty())
        {
            g_Settings.ClearInvincibleSelectedCharacterIndex = -1;
        }
        else if (g_Settings.ClearInvincibleSelectedCharacterIndex < 0 ||
            g_Settings.ClearInvincibleSelectedCharacterIndex >= static_cast<int>(g_ClearInvincibleTargetNames.size()))
        {
            g_Settings.ClearInvincibleSelectedCharacterIndex = 0;
        }
    }

    static void QueueClearInvincibleRequest(const char* label)
    {
        g_Settings.ClearInvincibleTargetMode = std::clamp(g_Settings.ClearInvincibleTargetMode, 0, 3);
        g_Settings.ClearInvincibleMethod = std::clamp(g_Settings.ClearInvincibleMethod, 0, 2);
        g_Settings.ClearInvincibleAttackId = std::clamp(g_Settings.ClearInvincibleAttackId, 0, 8);
        g_Settings.ClearInvincibleIntervalMs = std::clamp(g_Settings.ClearInvincibleIntervalMs, 50, 2000);

        const int targetMode = g_Settings.ClearInvincibleTargetMode;
        const int method = g_Settings.ClearInvincibleMethod;
        const bool ignoreFixed = g_Settings.ClearInvincibleIgnoreFixed;
        const int attackId = g_Settings.ClearInvincibleAttackId;
        const int selectedIndex = g_Settings.ClearInvincibleSelectedCharacterIndex;
        const std::string tag = g_Settings.ClearInvincibleTagBuffer;

        EnqueueGameThreadMenuTask([targetMode, method, ignoreFixed, attackId, tag, selectedIndex]() {
            InGameHack_ClearInvincibleTargets(targetMode, method, ignoreFixed, attackId, tag.c_str(), selectedIndex);
        }, label);
    }

    static AttackChainConfig BuildAttackChainConfig()
    {
        AttackChainConfig config{};
        config.useChainComboFlag = g_Settings.AttackChainUseChainComboFlag;
        config.chainComboTime = std::clamp(g_Settings.AttackChainComboFlagTime, 0.0f, 5.0f);
        config.enableShiftAttackActions = g_Settings.AttackChainEnableShiftAttackActions;
        config.clearShiftActionAttackFlags = g_Settings.AttackChainClearShiftActionAttackFlags;
        config.useAnimationRate = g_Settings.AttackChainUseAnimationRate;
        config.animationRate = std::clamp(g_Settings.AttackChainAnimationRate, 0.1f, 30.0f);
        config.animationRateNagara = g_Settings.AttackChainAnimationRateNagara;
        config.usePhaseEndCondition = g_Settings.AttackChainUsePhaseEndCondition;
        config.comboCommand = g_Settings.AttackChainComboCommand;
        config.grabbed = g_Settings.AttackChainGrabbed;
        config.endTimer = std::clamp(g_Settings.AttackChainEndTimer, 0.0f, 10.0f);
        config.landing = g_Settings.AttackChainLanding;
        config.endAnim = g_Settings.AttackChainEndAnim;
        config.endAnimSlot = std::clamp(g_Settings.AttackChainEndAnimSlot, 0, 6);
        config.finishCurrentPhase = g_Settings.AttackChainFinishCurrentPhase;
        config.terminateAttackLayer = g_Settings.AttackChainTerminateAttackLayer;
        config.enableAimingActionCancel = g_Settings.AttackChainEnableAimingActionCancel;
        config.actionCancelFlag = std::clamp(g_Settings.AttackChainActionCancelFlag, 0, 255);
        config.ownerActionOnly = g_Settings.AttackChainOwnerActionOnly;
        config.validateNextReservedAction = g_Settings.AttackChainValidateNextReservedAction;
        return config;
    }

    static void QueueAttackChainRequest(const char* label)
    {
        g_Settings.AttackChainIntervalMs = std::clamp(g_Settings.AttackChainIntervalMs, 16, 1000);
        const AttackChainConfig config = BuildAttackChainConfig();
        EnqueueGameThreadMenuTask([config]() {
            InGameHack_ApplyAttackChainControl(config);
        }, label);
    }

    static void RefreshClearInvincibleTargetNames(bool force);

    static DownPowerConfig BuildDownPowerConfig()
    {
        DownPowerConfig config{};
        config.patchDamageParams = g_Settings.DownPowerPatchDamageParams;
        config.damageType = g_Settings.DownPowerDamageType;
        config.includeNoActionDamage = g_Settings.DownPowerIncludeNoActionDamage;
        config.overrideRecoverSpan = g_Settings.DownPowerOverrideRecoverSpan;
        config.recoverSpan = std::clamp(g_Settings.DownPowerRecoverSpan, 0.0f, 30.0f);
        config.applyDurableRates = g_Settings.DownPowerApplyDurableRates;
        config.durableRate = std::clamp(g_Settings.DownPowerDurableRate, 0.0f, 100.0f);
        config.durableAttackActionRate = std::clamp(g_Settings.DownPowerDurableAttackActionRate, 0.0f, 100.0f);
        config.durableTakeDamageRate = std::clamp(g_Settings.DownPowerDurableTakeDamageRate, 0.0f, 100.0f);
        config.durableSpecialActionRate = std::clamp(g_Settings.DownPowerDurableSpecialActionRate, 0.0f, 100.0f);
        config.durableRollSlotRate = std::clamp(g_Settings.DownPowerDurableRollSlotRate, 0.0f, 100.0f);
        config.durableTakeDamageRollSlotRate = std::clamp(g_Settings.DownPowerDurableTakeDamageRollSlotRate, 0.0f, 100.0f);
        config.applyBreakDownSuperArmorRate = g_Settings.DownPowerApplyBreakDownSuperArmorRate;
        config.breakDownSuperArmorRate = std::clamp(g_Settings.DownPowerBreakDownSuperArmorRate, 0.0f, 100.0f);
        config.disableTargetSuperArmor = g_Settings.DownPowerDisableTargetSuperArmor;
        config.targetMode = std::clamp(g_Settings.DownPowerTargetMode, 0, 3);
        config.selectedCharacterIndex = g_Settings.DownPowerSelectedCharacterIndex;
        return config;
    }

    static void ApplyDownPowerConfigToSettings(const DownPowerConfig& config)
    {
        g_Settings.DownPowerDurableRate = config.durableRate;
        g_Settings.DownPowerDurableAttackActionRate = config.durableAttackActionRate;
        g_Settings.DownPowerDurableTakeDamageRate = config.durableTakeDamageRate;
        g_Settings.DownPowerDurableSpecialActionRate = config.durableSpecialActionRate;
        g_Settings.DownPowerDurableRollSlotRate = config.durableRollSlotRate;
        g_Settings.DownPowerDurableTakeDamageRollSlotRate = config.durableTakeDamageRollSlotRate;
        g_Settings.DownPowerBreakDownSuperArmorRate = config.breakDownSuperArmorRate;
    }

    static void ResetDownPowerSettingsToDefault()
    {
        g_Settings.DownPowerPatchDamageParams = false;
        g_Settings.DownPowerDamageType = 16;
        g_Settings.DownPowerIncludeNoActionDamage = true;
        g_Settings.DownPowerOverrideRecoverSpan = false;
        g_Settings.DownPowerRecoverSpan = 0.0f;
        g_Settings.DownPowerDurableRate = 1.0f;
        g_Settings.DownPowerDurableAttackActionRate = 1.0f;
        g_Settings.DownPowerDurableTakeDamageRate = 1.0f;
        g_Settings.DownPowerDurableSpecialActionRate = 1.0f;
        g_Settings.DownPowerDurableRollSlotRate = 1.0f;
        g_Settings.DownPowerDurableTakeDamageRollSlotRate = 1.0f;
        g_Settings.DownPowerBreakDownSuperArmorRate = 1.0f;
    }

    static void QueueDownPowerRequest(const char* label)
    {
        g_Settings.DownPowerIntervalMs = std::clamp(g_Settings.DownPowerIntervalMs, 50, 2000);
        const DownPowerConfig config = BuildDownPowerConfig();
        EnqueueGameThreadMenuTask([config]() {
            InGameHack_ApplyDownPowerConfig(config);
        }, label);
    }

    static void QueueDownPowerLoadCurrent()
    {
        EnqueueGameThreadMenuTask([]() {
            DownPowerConfig config{};
            if (InGameHack_TryReadDownPowerConfig(config))
                ApplyDownPowerConfigToSettings(config);
        }, "Load DownPower Current");
    }

    static void RenderDownPowerPage(float groupWidth)
    {
        if (BeginRugirCard("character-downpower-page", "DOWNPOWER", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "character-downpower-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            static const char* kTargetModes[] = {
                "Forward ESP",
                "All Enemies",
                "All Non-Local",
                "Selected"
            };

            g_Settings.DownPowerTargetMode = std::clamp(g_Settings.DownPowerTargetMode, 0, 3);
            g_Settings.DownPowerIntervalMs = std::clamp(g_Settings.DownPowerIntervalMs, 50, 2000);

            SeparatorLabel("Damage reaction");
            ImAdd::CheckBox("Patch Damage Params", &g_Settings.DownPowerPatchDamageParams);
            if (g_Settings.DownPowerPatchDamageParams)
            {
                static const char* kDamageTypeLabels[] = {
                    "Weak",
                    "Always Weak",
                    "Strong",
                    "Straight",
                    "Strong Launch",
                    "Strong Bound",
                    "Strong Blow Off",
                    "Strong Add Hit",
                    "Strong Carry Front",
                    "Straight Non Bound"
                };
                static const int kDamageTypeValues[] = {
                    1,
                    2,
                    3,
                    4,
                    11,
                    13,
                    16,
                    17,
                    18,
                    19
                };

                int damageTypeIndex = 6;
                for (int i = 0; i < IM_ARRAYSIZE(kDamageTypeValues); ++i)
                {
                    if (g_Settings.DownPowerDamageType == kDamageTypeValues[i])
                    {
                        damageTypeIndex = i;
                        break;
                    }
                }

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::Combo("Damage Type", &damageTypeIndex, kDamageTypeLabels, IM_ARRAYSIZE(kDamageTypeLabels)))
                    g_Settings.DownPowerDamageType = kDamageTypeValues[damageTypeIndex];

                ImAdd::CheckBox("Include No Action Params", &g_Settings.DownPowerIncludeNoActionDamage);
                ImAdd::CheckBox("Override Recover Span", &g_Settings.DownPowerOverrideRecoverSpan);
                if (g_Settings.DownPowerOverrideRecoverSpan)
                    ImAdd::SliderFloat("Recover Span", &g_Settings.DownPowerRecoverSpan, 0.0f, 30.0f, "%.2f");
            }

            SeparatorLabel("Target armor");
            ImAdd::CheckBox("Disable Target Super Armor", &g_Settings.DownPowerDisableTargetSuperArmor);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("Target Mode", &g_Settings.DownPowerTargetMode, kTargetModes, IM_ARRAYSIZE(kTargetModes));
            if (g_Settings.DownPowerTargetMode == 3)
            {
                RefreshClearInvincibleTargetNames(false);
                if (FullWidthButton("REFRESH TARGETS##DownPower"))
                    RefreshClearInvincibleTargetNames(true);

                if (!g_ClearInvincibleTargetNames.empty())
                {
                    int selectedIndex = std::clamp(
                        g_Settings.DownPowerSelectedCharacterIndex,
                        0,
                        static_cast<int>(g_ClearInvincibleTargetNames.size()) - 1);
                    g_Settings.DownPowerSelectedCharacterIndex = selectedIndex;
                    const char* preview = g_ClearInvincibleTargetNames[static_cast<size_t>(selectedIndex)].c_str();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("Target", preview, ImGuiComboFlags_HeightLarge))
                    {
                        for (int i = 0; i < static_cast<int>(g_ClearInvincibleTargetNames.size()); ++i)
                        {
                            const bool selected = (i == g_Settings.DownPowerSelectedCharacterIndex);
                            if (ImGui::Selectable(g_ClearInvincibleTargetNames[static_cast<size_t>(i)].c_str(), selected))
                                g_Settings.DownPowerSelectedCharacterIndex = i;

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }

            SeparatorLabel("Durable rates");
            ImAdd::CheckBox("Apply Durable Rates", &g_Settings.DownPowerApplyDurableRates);
            if (g_Settings.DownPowerApplyDurableRates)
            {
                ImAdd::SliderFloat("Durable", &g_Settings.DownPowerDurableRate, 0.0f, 100.0f, "%.2f");
                ImAdd::SliderFloat("Attack Action", &g_Settings.DownPowerDurableAttackActionRate, 0.0f, 100.0f, "%.2f");
                ImAdd::SliderFloat("Take Damage", &g_Settings.DownPowerDurableTakeDamageRate, 0.0f, 100.0f, "%.2f");
                ImAdd::SliderFloat("Special Action", &g_Settings.DownPowerDurableSpecialActionRate, 0.0f, 100.0f, "%.2f");
            }

            ImGui::NextColumn();

            SeparatorLabel("Roll slot");
            ImAdd::SliderFloat("Roll Slot", &g_Settings.DownPowerDurableRollSlotRate, 0.0f, 100.0f, "%.2f");
            ImAdd::SliderFloat("Take Damage Roll Slot", &g_Settings.DownPowerDurableTakeDamageRollSlotRate, 0.0f, 100.0f, "%.2f");

            SeparatorLabel("Breakdown");
            ImAdd::CheckBox("Apply Super Armor Breakdown", &g_Settings.DownPowerApplyBreakDownSuperArmorRate);
            if (g_Settings.DownPowerApplyBreakDownSuperArmorRate)
                ImAdd::SliderFloat("Super Armor Rate", &g_Settings.DownPowerBreakDownSuperArmorRate, 0.0f, 100.0f, "%.2f");

            SeparatorLabel("Execution");
            ImAdd::CheckBox("Auto Apply", &g_Settings.EnableDownPowerAuto);
            if (g_Settings.EnableDownPowerAuto)
                ImAdd::SliderInt("Interval", &g_Settings.DownPowerIntervalMs, 50, 2000, "%d ms");

            if (FullWidthButton("APPLY DOWNPOWER NOW"))
                QueueDownPowerRequest("DownPower");
            if (FullWidthButton("RESTORE ORIGINAL DAMAGE PARAMS"))
            {
                g_Settings.DownPowerPatchDamageParams = false;
                QueueDownPowerRequest("DownPower Restore");
            }
            if (FullWidthButton("LOAD CURRENT SDK VALUES"))
                QueueDownPowerLoadCurrent();
            if (FullWidthButton("RESET DEFAULTS##DownPower"))
            {
                ResetDownPowerSettingsToDefault();
                QueueDownPowerRequest("DownPower Reset");
            }

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static void RenderAttackChainPage(float groupWidth)
    {
        if (BeginRugirCard("character-attack-chain-page", "ACTION CANCEL", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "character-attack-chain-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            SeparatorLabel("Chain combo");
            ImAdd::CheckBox("Enable Chain Combo Flag", &g_Settings.AttackChainUseChainComboFlag);
            if (g_Settings.AttackChainUseChainComboFlag)
                ImAdd::SliderFloat("Combo Window", &g_Settings.AttackChainComboFlagTime, 0.0f, 5.0f, "%.2f s");
            ImAdd::CheckBox("Enable Shift Attack Actions", &g_Settings.AttackChainEnableShiftAttackActions);
            ImAdd::CheckBox("Clear Shift Action Flags", &g_Settings.AttackChainClearShiftActionAttackFlags);

            SeparatorLabel("Cancel window");
            ImAdd::CheckBox("Enable Aiming Action Cancel", &g_Settings.AttackChainEnableAimingActionCancel);
            if (g_Settings.AttackChainEnableAimingActionCancel)
            {
                ImAdd::SliderInt("Cancel Flag", &g_Settings.AttackChainActionCancelFlag, 0, 255);
                ImAdd::CheckBox("Owner Action Only", &g_Settings.AttackChainOwnerActionOnly);
                ImAdd::CheckBox("Validate Next Reserved", &g_Settings.AttackChainValidateNextReservedAction);
            }

            SeparatorLabel("Attack speed");
            ImAdd::CheckBox("Set Animation Rate", &g_Settings.AttackChainUseAnimationRate);
            if (g_Settings.AttackChainUseAnimationRate)
            {
                ImAdd::SliderFloat("Animation Rate", &g_Settings.AttackChainAnimationRate, 0.1f, 30.0f, "%.2f");
                ImAdd::CheckBox("Nagara Slot", &g_Settings.AttackChainAnimationRateNagara);
            }

            ImGui::NextColumn();

            SeparatorLabel("Phase end");
            ImAdd::CheckBox("Set End Condition", &g_Settings.AttackChainUsePhaseEndCondition);
            if (g_Settings.AttackChainUsePhaseEndCondition)
            {
                ImAdd::CheckBox("Combo Command", &g_Settings.AttackChainComboCommand);
                ImAdd::CheckBox("Grabbed", &g_Settings.AttackChainGrabbed);
                ImAdd::SliderFloat("End Timer", &g_Settings.AttackChainEndTimer, 0.0f, 10.0f, "%.2f");
                ImAdd::CheckBox("Landing", &g_Settings.AttackChainLanding);
                ImAdd::CheckBox("End Anim", &g_Settings.AttackChainEndAnim);
                ImAdd::SliderInt("End Anim Slot", &g_Settings.AttackChainEndAnimSlot, 0, 6);
            }

            SeparatorLabel("Force actions");
            ImAdd::CheckBox("Finish Current Phase", &g_Settings.AttackChainFinishCurrentPhase);
            ImAdd::CheckBox("Terminate Attack Layer", &g_Settings.AttackChainTerminateAttackLayer);

            SeparatorLabel("Execution");
            ImAdd::CheckBox("Auto Apply", &g_Settings.EnableAttackChainAuto);
            if (g_Settings.EnableAttackChainAuto)
                ImAdd::SliderInt("Interval", &g_Settings.AttackChainIntervalMs, 16, 1000, "%d ms");

            if (FullWidthButton("APPLY ACTION CANCEL NOW"))
                QueueAttackChainRequest("Action Cancel");

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static void RenderClearInvinciblePage(float groupWidth)
    {
        RefreshClearInvincibleTargetNames(false);

        if (BeginRugirCard("character-clear-invincible-page", "CLEAR INVINCIBLE", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "character-clear-invincible-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            static const char* kTargetModes[] = {
                "Forward ESP",
                "All Enemies",
                "All Non-Local",
                "Selected"
            };
            static const char* kMethods[] = {
                "Clear All",
                "From Attack",
                "With Tag"
            };
            static const char* kAttackIds[] = {
                "NONE",
                "UNIQUE1",
                "UNIQUE2",
                "UNIQUE3",
                "MELEE",
                "FINISHER",
                "SPECIAL",
                "PICKUPITEM",
                "USEITEM"
            };

            g_Settings.ClearInvincibleTargetMode = std::clamp(g_Settings.ClearInvincibleTargetMode, 0, 3);
            g_Settings.ClearInvincibleMethod = std::clamp(g_Settings.ClearInvincibleMethod, 0, 2);
            g_Settings.ClearInvincibleAttackId = std::clamp(g_Settings.ClearInvincibleAttackId, 0, 8);
            g_Settings.ClearInvincibleIntervalMs = std::clamp(g_Settings.ClearInvincibleIntervalMs, 50, 2000);

            SeparatorLabel("Target");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("Target Mode", &g_Settings.ClearInvincibleTargetMode, kTargetModes, IM_ARRAYSIZE(kTargetModes));

            if (g_Settings.ClearInvincibleTargetMode == 3)
            {
                if (FullWidthButton("REFRESH TARGETS"))
                    RefreshClearInvincibleTargetNames(true);

                if (!g_ClearInvincibleTargetNames.empty())
                {
                    const int selectedIndex = std::clamp(
                        g_Settings.ClearInvincibleSelectedCharacterIndex,
                        0,
                        static_cast<int>(g_ClearInvincibleTargetNames.size()) - 1);
                    g_Settings.ClearInvincibleSelectedCharacterIndex = selectedIndex;

                    const char* preview = g_ClearInvincibleTargetNames[static_cast<size_t>(selectedIndex)].c_str();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::BeginCombo("Selected Target", preview, ImGuiComboFlags_HeightLarge))
                    {
                        for (int i = 0; i < static_cast<int>(g_ClearInvincibleTargetNames.size()); ++i)
                        {
                            const bool selected = (i == g_Settings.ClearInvincibleSelectedCharacterIndex);
                            if (ImGui::Selectable(g_ClearInvincibleTargetNames[static_cast<size_t>(i)].c_str(), selected))
                                g_Settings.ClearInvincibleSelectedCharacterIndex = i;

                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }

            SeparatorLabel("Method");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("Clear Method", &g_Settings.ClearInvincibleMethod, kMethods, IM_ARRAYSIZE(kMethods));
            ImAdd::CheckBox("Ignore Fixed", &g_Settings.ClearInvincibleIgnoreFixed);

            if (g_Settings.ClearInvincibleMethod == 1)
            {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Combo("Attack ID", &g_Settings.ClearInvincibleAttackId, kAttackIds, IM_ARRAYSIZE(kAttackIds));
            }
            else if (g_Settings.ClearInvincibleMethod == 2)
            {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputTextWithHint(
                    "Tag FName",
                    "Tag",
                    g_Settings.ClearInvincibleTagBuffer,
                    sizeof(g_Settings.ClearInvincibleTagBuffer));
            }

            ImGui::NextColumn();

            SeparatorLabel("Execution");
            ImAdd::CheckBox("Auto Clear", &g_Settings.EnableClearInvincibleAuto);
            if (g_Settings.EnableClearInvincibleAuto)
                ImAdd::SliderInt("Interval", &g_Settings.ClearInvincibleIntervalMs, 50, 2000, "%d ms");

            if (FullWidthButton("CLEAR INVINCIBLE NOW"))
                QueueClearInvincibleRequest("Clear Invincible");

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static void RenderPreviewCharacterPage(int subtab, float groupWidth)
    {
        static CharacterEditorState ownerState;

        if (subtab == 0)
        {
            float gridWidth = ImGui::GetContentRegionAvail().x * 0.66f;
            ImGui::BeginGroup();
            static bool ownerSwapEnabled = true;
            if (BeginRugirCard("owner-character-swap-page", "OWNER CHARACTER SWAP", ImVec2(gridWidth, 0.0f), &ownerSwapEnabled))
            {
                SeparatorLabel("Character icons");
                CharacterIconGridPanel("##owner-character-icons", ownerState.selectedVariationIndex, 0.0f, 42.0f);
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            RenderCharacterConfigCard(
                "owner-character-config-page",
                "CHARACTER CONFIGURATION",
                "APPLY CONFIGURATION",
                ownerState,
                [](int characterId, int variationId, int costumeCode)
                {
                    InGameHack_ApplyPlayerConfiguration(characterId, variationId, s_techAlpha, s_techBeta, s_techGamma, costumeCode, 0, 0);
                });
            return;
        }

        if (subtab == 1 && BeginRugirCard("character-tools-page", "TOOLS", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "character-tools-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            DrawPlayerNameInlineControls();

            SeparatorLabel("Revive");
            ImAdd::CheckBox("Recover Me (Self)", &g_Settings.EnableRecoveryMe);
            ImAdd::CheckBox("Recover Team", &g_Settings.EnableRecoveryTeam);
            ImAdd::CheckBox("Recover All ESP", &g_Settings.EnableRecoveryAllESP);

            float halfButtonWidth = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);
            if (ImAdd::Button("USER RESPAWN", ImVec2(halfButtonWidth, 0.0f)))
                InGameHack_TestUserRespawnAction();
            ImGui::SameLine();
            if (ImAdd::Button("RESPAWN CARD", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                InGameHack_TestUseRespawnCardSupply();

            SeparatorLabel("Plus Ultra");
            ImAdd::CheckBox("Keep Plus Ultra Ready", &g_HackSettings.EnableInfinitePlusUltra);
            ImAdd::CheckBox("Fast Plus Ultra Charge", &g_Settings.EnableFastPlusUltraCharge);
            if (FullWidthButton("GET PLUS ULTRA NOW"))
            {
                EnqueueGameThreadMenuTask([]() {
                    InGameHack_GivePlusUltra();
                }, "Give Plus Ultra");
            }

            SeparatorLabel("Movement");
            const bool wasNoCollisionEnabled = g_Settings.EnableNoCollision;
            ImAdd::CheckBox("No Collision", &g_Settings.EnableNoCollision);
            ImAdd::SliderFloat("No Collision Speed", &g_Settings.NoCollisionSpeed, 1.0f, 1000.0f, "%.0f");
            DrawHotkeyConfigButton("No Collision hold", g_Settings.NoCollisionHoldKey, 108);
            if (wasNoCollisionEnabled && !g_Settings.EnableNoCollision)
            {
                EnqueueGameThreadMenuTask([]() {
                    InGameHack_SetNoCollision(false);
                }, "Disable No Collision");
            }

            SeparatorLabel("Camera");
            ImAdd::CheckBox("Camera FOV", &g_Settings.EnableCameraFOV);
            ImAdd::SliderFloat("Camera FOV Value##CameraFOV", &g_Settings.CameraFOV, 30.0f, 170.0f, "%.0f");

            RefreshRecoveryTeamCache();

            const float rosterButtonWidth = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);
            if (ImAdd::Button("REFRESH ROSTER", ImVec2(rosterButtonWidth, 0.0f)))
            {
                RefreshRecoveryTeamCache(true);
                g_Settings.SelectedRecoveryTeamIndex = -1;
            }
            ImGui::SameLine();
            ImAdd::CheckBox("Recover Selected Team", &g_Settings.EnableRecoverySelectedTeam);

            if (g_CachedRecoveryTeamNames.empty())
            {
                ImGui::TextColored(g_Colors.warning, "No team members found.");
            }
            else
            {
                const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
                float rosterHeight = rowHeight * (float)g_CachedRecoveryTeamNames.size();
                const float maxRosterHeight = rowHeight * 3.5f;
                if (rosterHeight > maxRosterHeight)
                    rosterHeight = maxRosterHeight;

                ImGui::BeginChild("##recovery-team-list", ImVec2(0.0f, rosterHeight), false);
                for (int i = 0; i < (int)g_CachedRecoveryTeamNames.size(); i++)
                {
                    if (SelectableRow(g_CachedRecoveryTeamNames[i].c_str(), g_Settings.SelectedRecoveryTeamIndex == i))
                        g_Settings.SelectedRecoveryTeamIndex = i;
                }
                ImGui::EndChild();
            }

            ImGui::NextColumn();

            SeparatorLabel("Unique Skill");
            static float lastRefreshTime = 0.0f;
            if (ImGui::GetTime() - lastRefreshTime > 0.5f)
            {
                lastRefreshTime = (float)ImGui::GetTime();
                int level1 = InGameHack_GetSkillLevel(1);
                int level2 = InGameHack_GetSkillLevel(2);
                int level3 = InGameHack_GetSkillLevel(3);
                if (level1 > 0) g_HackSettings.SupplyUniqueSkill1Level = level1;
                if (level2 > 0) g_HackSettings.SupplyUniqueSkill2Level = level2;
                if (level3 > 0) g_HackSettings.SupplyUniqueSkill3Level = level3;
            }

            if (ImAdd::SliderInt("Unique Skill 1", &g_HackSettings.SupplyUniqueSkill1Level, 1, 9))
                InGameHack_SetSkillLevel(1, g_HackSettings.SupplyUniqueSkill1Level);
            if (ImAdd::SliderInt("Unique Skill 2", &g_HackSettings.SupplyUniqueSkill2Level, 1, 9))
                InGameHack_SetSkillLevel(2, g_HackSettings.SupplyUniqueSkill2Level);
            if (ImAdd::SliderInt("Unique Skill 3", &g_HackSettings.SupplyUniqueSkill3Level, 1, 9))
                InGameHack_SetSkillLevel(3, g_HackSettings.SupplyUniqueSkill3Level);
            if (FullWidthButton("TEST USE ITEM ACTION"))
                InGameHack_TestUseItemAction();
            if (ImAdd::SliderFloat("Reload Rate (Blue Flame)", &g_Settings.ReloadAdjustRate_WearBlueFlame, 1.0f, 50.0f, "%.2fx"))
                InGameHack_SetReloadAdjustRate_WearBlueFlame(g_Settings.ReloadAdjustRate_WearBlueFlame);
            if (ImAdd::SliderFloat("Damage Multiplier", &g_Settings.CvNoneDamageCurveValue, 1.0f, 300.0f, "%.2f"))
            {
                const float curveValue = g_Settings.CvNoneDamageCurveValue;
                EnqueueGameThreadMenuTask([curveValue]() {
                    InGameHack_SetCvNoneCurveValue(curveValue);
                }, "Damage Multiplier");
            }

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        if (subtab == 1)
        {
            EndRugirCard();
            return;
        }

        if (subtab == 2)
        {
            RenderCh202Unique3ParamsPage(groupWidth);
            return;
        }

        if (subtab == 3)
        {
            RenderAttackChainPage(groupWidth);
            return;
        }

        if (subtab == 4)
        {
            RenderClearInvinciblePage(groupWidth);
            return;
        }

        if (subtab == 5)
        {
            RenderDownPowerPage(groupWidth);
            return;
        }
    }

    static void RenderPreviewAimbotPage(int subtab, float groupWidth)
    {
        (void)subtab;
        if (BeginRugirCard("aimbot-page", "AIMBOT / BULLET TP", ImVec2(0.0f, 0.0f)))
        {
            ImGuiStyle& style = ImGui::GetStyle();
            const float columnWidth = (groupWidth > 0.0f)
                ? groupWidth
                : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
            ImGui::Columns(2, "aimbot-columns", false);
            ImGui::SetColumnWidth(0, columnWidth);

            RugirHeaderToggle("AIMBOT SETTINGS", &g_Settings.EnableAimbot);
            if (g_Settings.EnableAimbot)
            {
                ImAdd::CheckBox("Smooth Aiming", &g_Settings.AimbotSmoothing);
                if (g_Settings.AimbotSmoothing)
                    ImAdd::SliderFloat("Smooth Factor", &g_Settings.AimbotSmoothFactor, 0.01f, 10.0f, "%.2f");
                ImAdd::CheckBox("Ignore Downed Targets", &g_Settings.AimbotIgnoreDownedTargets);
                ImAdd::CheckBox("Draw Aim Line", &g_Settings.AimbotDrawLine);
                ImAdd::CheckBox("Draw Crosshair", &g_Settings.AimbotDrawCrosshair);
                ImAdd::CheckBox("Draw Aim FOV", &g_Settings.AimbotDrawFOV);
                if (g_Settings.AimbotDrawFOV)
                    ImAdd::SliderFloat("Aimbot FOV Radius##AimbotFOVRadius", &g_Settings.AimbotFOVRadius, 100.0f, 1000.0f, "%.0f");
                ImAdd::SliderFloat("Aimbot Max Distance##AimbotMaxDistance", &g_Settings.AimbotMaxDistance, 10.0f, 2000.0f, "%.0f m");
            }

            DrawHotkeyConfigButton("Aimbot hold", g_Settings.AimbotHoldKey, 100);

            ImGui::NextColumn();

            RugirHeaderToggle("BULLET TP", &g_Settings.EnableBulletTP);
            ImAdd::CheckBox("Alpha (Unique1)", &g_Settings.BulletTP_IncludeAlpha);
            ImAdd::CheckBox("Beta (Unique2)", &g_Settings.BulletTP_IncludeBeta);
            ImAdd::CheckBox("Gamma (Unique3)", &g_Settings.BulletTP_IncludeGamma);
            ImAdd::CheckBox("Special", &g_Settings.BulletTP_IncludeSpecial);
            ImAdd::CheckBox("Ignore Downed Targets", &g_Settings.BulletTPIgnoreDownedTargets);
            ImAdd::SliderFloat("Bullet TP FOV Radius##BulletTPFOVRadius", &g_Settings.BulletTP_FOVRadius, 100.0f, 1000.0f, "%.0f");
            ImAdd::SliderFloat("Max Distance", &g_Settings.BulletTP_MaxDistance, 1000.0f, 100000.0f, "%.0f");

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        EndRugirCard();
    }

    static void RenderPreviewCombatPage(int subtab, float groupWidth)
    {
        if (subtab == 0)
        {
            if (BeginRugirCard("combat-tools-page", "COMBAT", ImVec2(0.0f, 0.0f)))
            {
                ImGuiStyle& style = ImGui::GetStyle();
                const float columnWidth = (groupWidth > 0.0f)
                    ? groupWidth
                    : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
                ImGui::Columns(2, "combat-tools-columns", false);
                ImGui::SetColumnWidth(0, columnWidth);

                RugirHeaderToggle("INVINCIBILITY & RECOVERY", &g_HackSettings.EnableInvincible);
                DrawHotkeyConfigButton("Invincibility", g_Settings.SetInvincibleKey, 104);
                if (FullWidthButton("ACTIVATE INVINCIBILITY NOW"))
                    InGameHack_SetInvincible();

                SeparatorLabel("Projectile Debug");
                float halfButtonWidth = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);
                if (ImAdd::Button("DUMP REP", ImVec2(halfButtonWidth, 0.0f)))
                    InGameHack_DumpLastProjectileGenerateRep();
                ImGui::SameLine();
                if (ImAdd::Button("DUMP RUNTIME", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    InGameHack_DumpProjectileRuntimeDebug();

                if (FullWidthButton("PROJECTILE TEST"))
                    InGameHack_GenerateProjectileInFront();

                RugirHeaderToggle("PROJECTILE GENERATION", &g_Settings.EnableGenerateProjectile);
                DrawHotkeyConfigButton("Generate Projectile", g_Settings.GenerateProjectileKey, 107);
                if (FullWidthButton("GENERATE NOW"))
                    InGameHack_GenerateProjectileInFront();

                ImGui::NextColumn();

                SeparatorLabel("Ability Levels");
                ImAdd::SliderInt("Attack Level", &g_HackSettings.AbilityAttackLevel, 1, 100);
                if (FullWidthButton("ABILITY ATTACK"))
                {
                    const int level = g_HackSettings.AbilityAttackLevel;
                    EnqueueGameThreadMenuTask([level]() {
                        InGameHack_AbilityAttack(level);
                    }, "Ability Attack");
                }
                ImAdd::SliderInt("Durable Level", &g_HackSettings.AbilityDurableLevel, 1, 100);
                if (FullWidthButton("ABILITY DURABLE"))
                {
                    const int level = g_HackSettings.AbilityDurableLevel;
                    EnqueueGameThreadMenuTask([level]() {
                        InGameHack_AbilityDurable(level);
                    }, "Ability Durable");
                }
                ImAdd::SliderInt("Movespeed Level", &g_HackSettings.AbilityMovespeedLevel, 1, 100);
                if (FullWidthButton("ABILITY MOVESPEED"))
                {
                    const int level = g_HackSettings.AbilityMovespeedLevel;
                    EnqueueGameThreadMenuTask([level]() {
                        InGameHack_AbilityMovespeed(level);
                    }, "Ability Movespeed");
                }
                ImAdd::SliderInt("Heal Level", &g_HackSettings.AbilityHealLevel, 1, 100);
                if (FullWidthButton("ABILITY HEAL"))
                {
                    const int level = g_HackSettings.AbilityHealLevel;
                    EnqueueGameThreadMenuTask([level]() {
                        InGameHack_AbilityHeal(level);
                    }, "Ability Heal");
                }
                ImAdd::SliderInt("Technique Level", &g_HackSettings.AbilityTechniqueLevel, 1, 100);
                if (FullWidthButton("ABILITY TECHNIQUE"))
                {
                    const int level = g_HackSettings.AbilityTechniqueLevel;
                    EnqueueGameThreadMenuTask([level]() {
                        InGameHack_AbilityTechnique(level);
                    }, "Ability Technique");
                }

                ImGui::Columns(1);
                ImGui::PopStyleVar();
            }
            EndRugirCard();
            return;
        }

        if (subtab == 1)
        {
            g_Settings.EnableCH202InitTransLevel5 = false;
            g_Settings.EnableSupplyMaxStackTo100 = false;

            if (BeginRugirCard("character-conditions-page", "CHARACTER CONDITIONS", ImVec2(0.0f, 0.0f)))
            {
                ImAdd::CheckBox("Enable Rebuild Myself", &g_Settings.EnableRebuildMyself);
                if (FullWidthButton("OVERHAUL HEAL"))
                    InGameHack_RebuildMyself();
                DrawHotkeyConfigButton("Rebuild Myself", g_Settings.RebuildMyselfKey, 105);
                SeparatorLabel("Condition Editor");
                RenderCharacterConditionEditor();
            }
            EndRugirCard();
            return;
        }
    }

    static void RenderPreviewHacksPage(int subtab, float groupWidth)
    {
        if (subtab == 0)
        {
            if (BeginRugirCard("imitation-copy-skills-page", "IMITATION / COPY SKILLS", ImVec2(0.0f, 0.0f)))
            {
                ImGuiStyle& style = ImGui::GetStyle();
                const float columnWidth = (groupWidth > 0.0f)
                    ? groupWidth
                    : std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 6.0f));
                ImGui::Columns(2, "imitation-copy-skills-columns", false);
                ImGui::SetColumnWidth(0, columnWidth);

                RugirHeaderToggle("TRANSFORM TOGA", &g_Settings.EnableTransformIntoRandomESP);
                DrawHotkeyConfigButton("Transform", g_Settings.TransformIntoRandomESPKey, 102);
                ImGui::TextWrapped("Transform to a random ESP target.");

                RugirHeaderToggle("DUPLICATE IMITATION", &g_Settings.EnableDuplicateIntoImitationRandomESP);
                ImAdd::SliderFloat("Imitation Lifetime", &g_Settings.DuplicateImitationLifeTime, 5.0f, 120.0f, "%.0fs");
                ImAdd::SliderInt("Imitation Duplicate Count", &g_Settings.DuplicateIntoImitationCount, 1, 100);
                DrawHotkeyConfigButton("Imitation hotkey", g_Settings.DuplicateIntoImitationRandomESPKey, 103);

                ImGui::NextColumn();

                RugirHeaderToggle("COPY SKILLS", &g_Settings.EnableCopySkillsFromNearestEnemy);
                ImAdd::CheckBox("Set Copy Skill", &g_Settings.CopySkillsSetCopySkill);
                ImAdd::CheckBox("Use Owner Character Level", &g_Settings.CopySkillsUseOwnerCharacterLevel);
                DrawHotkeyConfigButton("Copy Skills", g_Settings.CopySkillsFromNearestEnemyKey, 106);

                ImGui::Columns(1);
                ImGui::PopStyleVar();
            }
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("teleport-kota-page", "TELEPORT TO KOTA", ImVec2(groupWidth, 0.0f), &g_Settings.EnableTeleportToKota))
        {
            ImAdd::CheckBox("Infinite Objects", &g_Settings.EnableInfiniteObjects);
            DrawHotkeyConfigButton("KOTA hotkey", g_Settings.TeleportToKotaKey, 101);
        }
        EndRugirCard();
        ImGui::EndGroup();

        ImGui::SameLine();

        if (BeginRugirCard("teleport-items-page", "TELEPORT ITEMS", ImVec2(0.0f, 0.0f), &g_Settings.EnableTeleportLevelUpCards))
        {
            if (FullWidthButton(">> TELEPORT ALL <<"))
                SDK_RunTeleportLevelUpCards();

            SeparatorLabel("Health / Guard");
            DrawActionButtonRow("HP S", SDK_TeleportItem_HPDrinkSmall, "HP L", SDK_TeleportItem_HPDrink, "Team HP", SDK_TeleportItem_HPDrinkMedium);
            DrawActionButtonRow("GP S", SDK_TeleportItem_GPDrinkSmall, "GP L", SDK_TeleportItem_GPDrink, "Team GP", SDK_TeleportItem_GPDrinkMedium);

            SeparatorLabel("Support / Bags");
            DrawActionButtonRow("Full Sup", SDK_TeleportItem_FullSupport, "Team Sup", SDK_TeleportItem_TeamSupport, "Bag S", SDK_TeleportItem_BagSmall);
            DrawActionButtonRow("Bag M", SDK_TeleportItem_BagMedium, "Bag L", SDK_TeleportItem_BagLarge, "Box", SDK_TeleportItem_Box);
            DrawActionButtonRow("Revive 1/3", SDK_TeleportItem_Revive1_3, "Revive Full", SDK_TeleportItem_ReviveFull);

            SeparatorLabel("Boxes / Cards");
            DrawActionButtonRow("Box S", SDK_TeleportItem_BoxSmall, "Box L", SDK_TeleportItem_BoxLarge, "Box Gold", SDK_TeleportItem_BoxGold);
            DrawActionButtonRow("LvlUp", SDK_TeleportItem_LevelUpCard, "Enhance", SDK_TeleportItem_TeamEnhancementKit);
            DrawActionButtonRow("Card A", SDK_TeleportItem_AbilitySupplyAlpha, "Card B", SDK_TeleportItem_AbilitySupplyBeta, "Card Y", SDK_TeleportItem_AbilitySupplyGamma);
        }
        EndRugirCard();

        // ===== CUSTOM DROP (drop arbitrary world item x quantity) =====
        static std::vector<std::string> s_dropCatalog;
        static int s_dropScanCount = -1;

        if (BeginRugirCard("custom-drop-page", "DROP ITEMS (CUSTOM)", ImVec2(0.0f, 0.0f), &g_Settings.EnableCustomDrop))
        {
            if (s_dropScanCount < 0)
            {
                s_dropScanCount = InGameHack_ScanWorldItemCatalog();
                InGameHack_GetWorldItemCatalogNames(s_dropCatalog);
                if (g_Settings.CustomDropSelectedIndex >= static_cast<int>(s_dropCatalog.size()))
                    g_Settings.CustomDropSelectedIndex = 0;
            }

            if (s_dropScanCount >= 0)
                ImGui::TextDisabled("Loaded %d known item(s)", s_dropScanCount);

            const bool hasSelection =
                g_Settings.CustomDropSelectedIndex >= 0 &&
                g_Settings.CustomDropSelectedIndex < static_cast<int>(s_dropCatalog.size());
            const char* preview = hasSelection
                ? s_dropCatalog[g_Settings.CustomDropSelectedIndex].c_str()
                : "(catalog unavailable)";

            if (ImGui::BeginCombo("Item", preview))
            {
                for (int i = 0; i < static_cast<int>(s_dropCatalog.size()); ++i)
                {
                    const bool selected = (i == g_Settings.CustomDropSelectedIndex);
                    if (ImGui::Selectable(s_dropCatalog[i].c_str(), selected))
                        g_Settings.CustomDropSelectedIndex = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SliderInt("Quantity Passes", &g_Settings.CustomDropQuantity, 1, 100);
            if (g_Settings.CustomDropSerialId < 1)
                g_Settings.CustomDropSerialId = 1;
            if (g_Settings.CustomDropSerialId > 512)
                g_Settings.CustomDropSerialId = 512;

            ImGui::SliderInt("Serial Count", &g_Settings.CustomDropSerialId, 1, 512);

            if (FullWidthButton(">> DROP <<"))
            {
                InGameHack_DropCatalogItem(
                    g_Settings.CustomDropSelectedIndex,
                    g_Settings.CustomDropQuantity,
                    g_Settings.CustomDropSerialId,
                    false);
            }

            DrawHotkeyConfigButton("Custom Drop", g_Settings.CustomDropKey, 109);
            ImGui::TextDisabled("Uses the selected runtime supplyId with the selected serial ID.");
        }
        EndRugirCard();
    }

    static std::vector<std::string>& CachedLobbyPlayers()
    {
        static std::vector<std::string> playerNames;
        static DWORD lastFetchTick = 0;
        DWORD now = GetTickCount();
        if (lastFetchTick == 0 || (now - lastFetchTick) > GAME_LIST_CACHE_INTERVAL_MS)
        {
            playerNames = InGameHack_GetAllPlayerNames();
            lastFetchTick = now;
        }
        return playerNames;
    }

    static void RenderLobbyTargetCard(int subtab, float width, int& selectedPlayer)
    {
        if (BeginRugirCard("lobby-target-card", subtab == 0 ? "APPLY TO TEAM" : subtab == 1 ? "APPLY TO ALL PLAYERS" : "APPLY TO SPECIFIC PLAYER", ImVec2(width, 0.0f)))
        {
            if (subtab == 0)
            {
                RefreshLobbyTeamsCache();

                if (FullWidthButton("Refresh Team List"))
                {
                    RefreshLobbyTeamsCache(true);
                    g_SelectedTeamId = -1;
                }

                SeparatorLabel("Available Teams");
                if (g_AvailableTeamIds.empty())
                    ImGui::TextColored(g_Colors.warning, "No teams found. Refresh team list.");

                for (unsigned char teamId : g_AvailableTeamIds)
                {
                    const TeamListEntry* cachedTeam = FindCachedLobbyTeam(teamId);
                    size_t playerCount = cachedTeam ? cachedTeam->playerNames.size() : 0;
                    char label[128];
                    if ((int)teamId == g_CachedLobbyMyTeamId)
                        snprintf(label, sizeof(label), "TEAM %d [MY TEAM] (%zu players)", (int)teamId, playerCount);
                    else
                        snprintf(label, sizeof(label), "TEAM %d (%zu players)", (int)teamId, playerCount);

                    if (SelectableRow(label, g_SelectedTeamId == (int)teamId))
                        g_SelectedTeamId = (int)teamId;
                }

                if (g_SelectedTeamId >= 0)
                {
                    SeparatorLabel("Selected team players");
                    const TeamListEntry* cachedTeam = FindCachedLobbyTeam((unsigned char)g_SelectedTeamId);
                    if (cachedTeam)
                    {
                        for (const auto& name : cachedTeam->playerNames)
                            ImGui::TextColored(g_Colors.textDisabled, "%s", name.c_str());
                    }
                }
            }
            else
            {
                auto& playerNames = CachedLobbyPlayers();
                if (FullWidthButton("Refresh Player List"))
                    playerNames = InGameHack_GetAllPlayerNames();

                SeparatorLabel(subtab == 1 ? "Affected Players" : "Player list");
                if (playerNames.empty())
                    ImGui::TextColored(g_Colors.warning, "No players found.");

                for (int i = 0; i < (int)playerNames.size(); i++)
                {
                    bool selected = subtab == 2 && selectedPlayer == i;
                    if (SelectableRow(playerNames[i].c_str(), selected))
                        selectedPlayer = i;
                }
            }
        }
        EndRugirCard();
    }

    static void RenderPreviewLobbyPage(int subtab, float groupWidth)
    {
        static CharacterEditorState teamState;
        static CharacterEditorState allState;
        static CharacterEditorState specificState;
        static int selectedPlayer = 0;

        if (subtab <= 2)
        {
            CharacterEditorState& state = subtab == 0 ? teamState : subtab == 1 ? allState : specificState;
            const char* actionLabel = subtab == 0 ? "APPLY TO TEAM" : subtab == 1 ? "APPLY TO ALL PLAYERS" : "APPLY TO SELECTED PLAYER";
            bool actionEnabled = subtab != 0 || g_SelectedTeamId >= 0;
            if (subtab == 2)
            {
                auto& players = CachedLobbyPlayers();
                actionEnabled = !players.empty() && selectedPlayer >= 0 && selectedPlayer < (int)players.size();
            }

            ImGui::BeginGroup();
            RenderLobbyTargetCard(subtab, groupWidth, selectedPlayer);
            ImGui::EndGroup();
            ImGui::SameLine();

            RenderCharacterConfigCard(
                actionLabel,
                "APPLY CONFIGURATION",
                actionLabel,
                state,
                [subtab](int characterId, int variationId, int costumeCode)
                {
                    if (subtab == 0)
                    {
                        if (g_SelectedTeamId >= 0)
                            InGameHack_ApplyToTeam((unsigned char)g_SelectedTeamId, characterId, variationId, s_techAlpha, s_techBeta, s_techGamma, costumeCode, 0);
                    }
                    else if (subtab == 1)
                    {
                        InGameHack_ApplyToAllControllers(characterId, variationId, s_techAlpha, s_techBeta, s_techGamma, costumeCode, 0);
                    }
                    else
                    {
                        SDK::EVariationCharacterId variationCharacterId = GetVariationCharacterId((SDK::ECharacterId)characterId, variationId);
                        InGameHack_ApplyToSpecificPlayer(selectedPlayer, variationCharacterId, s_techAlpha, s_techBeta, s_techGamma, costumeCode, 0);
                    }
                },
                actionEnabled,
                "Select a target first",
                true);
            return;
        }

        if (subtab == 3)
        {
            ImGui::BeginGroup();
            if (BeginRugirCard("lobby-change-team-page", "CHANGE MY TEAM", ImVec2(groupWidth, 0.0f)))
            {
                if (FullWidthButton("REFRESH TEAMS"))
                    RefreshLobbyTeamsCache(true);
                static int newTeamId = 0;
                ImAdd::SliderInt("New Team ID", &newTeamId, 0, 20);
                if (FullWidthButton("CHANGE MY TEAM"))
                    InGameHack_ChangeMyTeamTo((unsigned char)newTeamId);
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            if (BeginRugirCard("lobby-team-list-page", "TEAM LIST", ImVec2(0.0f, 0.0f)))
            {
                RefreshLobbyTeamsCache();

                for (const TeamListEntry& team : g_CachedLobbyTeams)
                {
                    char label[128];
                    snprintf(label, sizeof(label), "TEAM %d%s (%zu players)", (int)team.teamId, (int)team.teamId == g_CachedLobbyMyTeamId ? " [MY TEAM]" : "", team.playerNames.size());
                    if (SelectableRow(label, false))
                        InGameHack_ChangeMyTeamTo(team.teamId);
                }
            }
            EndRugirCard();
            return;
        }

        if (!g_LicenseSectionUnlocked)
        {
            RenderProtectedAccessCard(groupWidth);
            return;
        }

        if (BeginRugirCard("lobby-license-page", "SPECIAL LICENSE EXP", ImVec2(0.0f, 0.0f)))
        {
            ImAdd::SliderInt("Special License EXP", &g_HackSettings.BuyLicenseExpCount, 1, 300000, "%d");
            if (FullWidthButton("ADD SPECIAL LICENSE EXP (LOCAL)"))
                InGameHack_AddSpecialLicenseExpLocal(g_HackSettings.BuyLicenseExpCount);
            if (FullWidthButton("DUMP LICENSE GETTERS"))
                InGameHack_DumpSeasonLicenseGetters();

            SeparatorLabel("Rental Ticket Bypass");
            ImGuiHelper::ToggleSwitch("Bypass Rental Ticket Amount", &g_HackSettings.Hack_BypassRentalTickets);

            DrawLicenseBackendClaimTestControls();
            DrawSeasonRankRewardReplacementControls();
        }
        EndRugirCard();
    }

    static void DrawProfileManagementSettingsSection()
    {
        SeparatorLabel("Profile Manager");
        ImGui::InputTextWithHint("##ProfileNameInput", "Enter profile name...", g_ProfileNameBuffer, sizeof(g_ProfileNameBuffer));
        if (FullWidthButton("Reload Profiles"))
            g_ProfilesList = SettingsManager::GetProfilesList();

        if (ImAdd::Button("Save Configuration", ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f)))
        {
            std::string profileName(g_ProfileNameBuffer);
            if (!profileName.empty() && SettingsManager::SaveCurrentProfile(profileName))
            {
                g_CurrentProfileName = profileName;
                g_ProfilesList = SettingsManager::GetProfilesList();
            }
        }
        ImGui::SameLine();
        if (ImAdd::Button("Load Configuration", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            std::string profileName(g_ProfileNameBuffer);
            if (!profileName.empty() && SettingsManager::LoadCurrentProfile(profileName))
                g_CurrentProfileName = profileName;
        }

        SeparatorLabel("Available Profiles");
        if (g_ProfilesList.empty())
        {
            ImGui::TextColored(g_Colors.textDisabled, "No profiles found.");
        }
        else
        {
            static int selectedProfile = 0;
            std::vector<const char*> profileItems;
            profileItems.reserve(g_ProfilesList.size());

            for (int i = 0; i < (int)g_ProfilesList.size(); i++)
            {
                profileItems.push_back(g_ProfilesList[i].c_str());
                if (g_ProfilesList[i] == g_CurrentProfileName)
                    selectedProfile = i;
            }

            if (selectedProfile < 0 || selectedProfile >= (int)g_ProfilesList.size())
                selectedProfile = 0;

            const int previousProfile = selectedProfile;
            ImAdd::Combo("Profile", &selectedProfile, profileItems);
            if (selectedProfile != previousProfile)
            {
                const std::string& profile = g_ProfilesList[selectedProfile];
                if (SettingsManager::LoadCurrentProfile(profile))
                {
                    strcpy_s(g_ProfileNameBuffer, sizeof(g_ProfileNameBuffer), profile.c_str());
                    g_CurrentProfileName = profile;
                }
                else
                {
                    selectedProfile = previousProfile;
                }
            }
        }
    }

    static void DrawMenuVisualsSettingsSection()
    {
        SeparatorLabel("Visual");
        const bool wasEnabled = g_Settings.EnableMenuBackgroundVideo;
        ImAdd::CheckBox("Background Video", &g_Settings.EnableMenuBackgroundVideo);
        if (wasEnabled && !g_Settings.EnableMenuBackgroundVideo)
            ReleaseMenuBackgroundVideo();
    }

    static void DrawPlayerNameInlineControls()
    {
        SeparatorLabel("Player Name");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        const bool submitted = ImGui::InputTextWithHint(
            "##PlayerNameInput",
            "Enter new player name...",
            g_HackSettings.ChangePlayerNameBuffer,
            sizeof(g_HackSettings.ChangePlayerNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        if (submitted || FullWidthButton("UPDATE PLAYER NAME"))
        {
            if (!InGameHack_ChangePlayerName(g_HackSettings.ChangePlayerNameBuffer))
                Logger::LogError("[Menu] Update player name request failed");
        }
    }

    static void RenderPlayerNameSettingsCard(float width)
    {
        if (BeginRugirCard("player-name-settings-page", "PLAYER NAME", ImVec2(width, 0.0f)))
        {
            DrawPlayerNameInlineControls();
        }
        EndRugirCard();
    }

    static void RenderPlatformSettingsCard(float width)
    {
        if (BeginRugirCard("platform-settings-page", "PLATFORM", ImVec2(width, 0.0f)))
        {
            const char* platformItems[] = {
                "Invalid",
                "PlayStation",
                "Xbox",
                "Windows",
                "Switch",
                "None"
            };

            int selectedPlatform = g_HackSettings.BackendPlatformCode;
            if (selectedPlatform < 0 || selectedPlatform > 5)
                selectedPlatform = 3;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##BackendPlatformCode", &selectedPlatform, platformItems, IM_ARRAYSIZE(platformItems)))
                g_HackSettings.BackendPlatformCode = selectedPlatform;

            if (FullWidthButton("SET PLAYER PLATFORM"))
            {
                if (!InGameHack_SetBackendPlayerPlatform(g_HackSettings.BackendPlatformCode))
                    Logger::LogError("[Menu] Set player platform request failed");
            }

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            const bool submitted = ImGui::InputTextWithHint(
                "##FakePlatformInput",
                "Fake platform string...",
                g_HackSettings.FakePlatformBuffer,
                sizeof(g_HackSettings.FakePlatformBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue);

            if (submitted || FullWidthButton("FORCE FAKE PLATFORM"))
            {
                if (!InGameHack_ForceFakePlatform(g_HackSettings.FakePlatformBuffer))
                    Logger::LogError("[Menu] Force fake platform request failed");
            }
        }
        EndRugirCard();
    }

    static void RenderPreviewSettingsPage(float groupWidth)
    {
        if (BeginRugirCard("settings-page", "SETTINGS", ImVec2(groupWidth, 0.0f)))
        {
            DrawMenuVisualsSettingsSection();
            DrawInventoryDebugSettingsSection();
            DrawDllControlSettingsSection();
            DrawProfileManagementSettingsSection();
        }
        EndRugirCard();
    }

    // ============================================================================
    //  PREVIEW MENU
    // ============================================================================
    static void DrawFreeImGuiStyleMenu()
    {
        struct Subtab
        {
            const char* label;
            const char* icon;
        };

        struct Page
        {
            const char* label;
            const char* icon;
            const Subtab* subtabs;
            int subtabCount;
        };

        static int selectedSubtabs[7] = {};

        const Subtab espSubtabs[] =
        {
            { "ESP", "E" },
        };

        const Subtab characterSubtabs[] =
        {
            { "Swap", "S" },
            { "Tools", "T" },
            { "CH202 U3", "U" },
            { "Action Cancel", "A" },
            { "Invincible", "I" },
            { "DownPower", "D" },
        };

        const Subtab aimbotSubtabs[] =
        {
            { "Aimbot", "A" },
        };

        const Subtab combatSubtabs[] =
        {
            { "Combat", "C" },
            { "Conditions", "C" },
        };

        const Subtab hacksSubtabs[] =
        {
            { "Imitation/Copy", "I" },
            { "Kota/Items", "K" },
        };

        const Subtab lobbySubtabsLocked[] =
        {
            { "Apply Team", "T" },
            { "Apply All", "A" },
            { "Specific", "S" },
            { "Change Team", "C" },
            { "Access", "A" },
        };

        const Subtab lobbySubtabsUnlocked[] =
        {
            { "Apply Team", "T" },
            { "Apply All", "A" },
            { "Specific", "S" },
            { "Change Team", "C" },
            { "License EXP", "L" },
        };

        const Subtab* lobbySubtabs = g_LicenseSectionUnlocked ? lobbySubtabsUnlocked : lobbySubtabsLocked;
        const int lobbySubtabCount = g_LicenseSectionUnlocked ? IM_ARRAYSIZE(lobbySubtabsUnlocked) : IM_ARRAYSIZE(lobbySubtabsLocked);

        const Subtab settingsSubtabs[] =
        {
            { "Profiles", "P" },
        };

        const Page pages[] =
        {
            { "ESP", "E", espSubtabs, IM_ARRAYSIZE(espSubtabs) },
            { "Character", "C", characterSubtabs, IM_ARRAYSIZE(characterSubtabs) },
            { "Aimbot", "A", aimbotSubtabs, IM_ARRAYSIZE(aimbotSubtabs) },
            { "Combat", "X", combatSubtabs, IM_ARRAYSIZE(combatSubtabs) },
            { "Hacks", "H", hacksSubtabs, IM_ARRAYSIZE(hacksSubtabs) },
            { "Lobby", "L", lobbySubtabs, lobbySubtabCount },
            { "Settings", "S", settingsSubtabs, IM_ARRAYSIZE(settingsSubtabs) },
        };

        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();
        const int pageCount = IM_ARRAYSIZE(pages);
        if (g_SelectedTab < 0 || g_SelectedTab >= pageCount)
            g_SelectedTab = 0;

        const float sidebarWidth = 236.0f;
        const ImVec2 defaultSize(900.0f, 590.0f);
        ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        bool open = ImGui::Begin("RUGIR INTERNAL##free-imgui", &g_Visible,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (!open)
        {
            ImGui::End();
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 windowMax(p.x + size.x, p.y + size.y);
        ImVec2 sidebarMax(p.x + sidebarWidth, p.y + size.y);

        const bool videoDrawn = DrawMenuBackgroundVideo(drawList, p, windowMax, 12.0f);
        if (!videoDrawn)
            drawList->AddRectFilled(p, windowMax, ImGui::GetColorU32(ImVec4(7.0f / 255.0f, 7.0f / 255.0f, 10.0f / 255.0f, 0.56f)), 12.0f);
        drawList->AddRectFilled(p, windowMax, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.10f)), 12.0f);
        drawList->AddRectFilled(p, sidebarMax, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.38f)), 12.0f, ImDrawFlags_RoundCornersLeft);
        drawList->AddRectFilled(ImVec2(p.x + sidebarWidth, p.y), ImVec2(p.x + sidebarWidth + 1.0f, windowMax.y), ImGui::GetColorU32(ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.24f)));
        drawList->AddRect(p, windowMax, ImGui::GetColorU32(ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.20f)), 12.0f, 0, 1.0f);

        auto addText = [&](ImFont* font, float fontSize, const ImVec2& pos, ImU32 color, const char* text)
        {
            drawList->AddText(font ? font : ImGui::GetFont(), fontSize, pos, color, text);
        };

        auto calcTextSize = [](ImFont* font, float fontSize, const char* text) -> ImVec2
        {
            ImFont* resolvedFont = font ? font : ImGui::GetFont();
            return resolvedFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text ? text : "");
        };

        auto sidebarTab = [&](const char* label, const char* icon, bool selected, const ImVec2& tabSize) -> bool
        {
            ImGui::PushID(label);
            ImGui::InvisibleButton("##free-tab", tabSize);
            bool clicked = ImGui::IsItemClicked();
            bool hovered = ImGui::IsItemHovered();
            ImVec2 tabMin = ImGui::GetItemRectMin();
            ImVec2 tabMax = ImGui::GetItemRectMax();
            ImGui::PopID();

            ImU32 bg = ImGui::GetColorU32(selected ? ImVec4(34.0f / 255.0f, 31.0f / 255.0f, 48.0f / 255.0f, 0.62f)
                                                   : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (hovered && !selected)
                bg = ImGui::GetColorU32(ImVec4(33.0f / 255.0f, 32.0f / 255.0f, 43.0f / 255.0f, 0.46f));

            drawList->AddRectFilled(tabMin, tabMax, bg, 6.0f);
            if (selected)
            {
                drawList->AddRectFilled(ImVec2(tabMin.x, tabMin.y + 5.0f), ImVec2(tabMin.x + 3.0f, tabMax.y - 5.0f), ImGui::GetColorU32(g_Colors.accentColor), 2.0f);
                drawList->AddRect(tabMin, tabMax, ImGui::GetColorU32(ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.28f)), 6.0f);
            }

            ImU32 iconColor = ImGui::GetColorU32(selected ? g_Colors.accentColor : g_Colors.textDisabled);
            ImU32 labelColor = ImGui::GetColorU32(selected ? g_Colors.textPrimary : g_Colors.textSecondary);
            addText(g_FreeIconFont ? g_FreeIconFont : g_FreeFont, 23.0f, ImVec2(tabMin.x + 18.0f, tabMin.y + 9.0f), iconColor, icon);
            addText(g_FreeFontLarge, 17.0f, ImVec2(tabMin.x + 52.0f, tabMin.y + 11.0f), labelColor, label);
            return clicked;
        };

        auto subtabButton = [&](const char* label, bool selected, const ImVec2& buttonSize) -> bool
        {
            ImGui::PushID(label);
            ImGui::InvisibleButton("##free-subtab", buttonSize);
            bool clicked = ImGui::IsItemClicked();
            bool hovered = ImGui::IsItemHovered();
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImGui::PopID();

            ImVec4 bgColor = selected ? ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.82f) : ImVec4(24.0f / 255.0f, 25.0f / 255.0f, 34.0f / 255.0f, 0.52f);
            if (hovered && !selected)
                bgColor = ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 56.0f / 255.0f, 0.66f);

            drawList->AddRectFilled(buttonMin, buttonMax, ImGui::GetColorU32(bgColor), 4.0f);
            drawList->AddRect(buttonMin, buttonMax, ImGui::GetColorU32(ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, selected ? 0.38f : 0.16f)), 4.0f);

            ImVec2 textSize = calcTextSize(g_FreeFontSmall, 14.0f, label);
            ImVec2 textPos(buttonMin.x + (buttonSize.x - textSize.x) * 0.5f, buttonMin.y + (buttonSize.y - textSize.y) * 0.5f);
            addText(g_FreeFontSmall, 14.0f, textPos, ImGui::GetColorU32(selected ? g_Colors.textPrimary : g_Colors.textSecondary), label);
            return clicked;
        };

#if RUGIR_MENU_CHEAT_FACTORY
        addText(g_FreeFontBrand, 29.0f, ImVec2(p.x + 24.0f, p.y + 28.0f), ImGui::GetColorU32(g_Colors.accentColor), "CHEAT");
        addText(g_FreeFontBrand, 29.0f, ImVec2(p.x + 24.0f, p.y + 61.0f), ImGui::GetColorU32(g_Colors.textPrimary), "FACTORY");
#elif RUGIR_MENU_VALARIA
        addText(g_FreeFontBrand, 34.0f, ImVec2(p.x + 27.0f, p.y + 32.0f), ImGui::GetColorU32(g_Colors.accentColor), "VALA");
        addText(g_FreeFontBrand, 34.0f, ImVec2(p.x + 111.0f, p.y + 32.0f), ImGui::GetColorU32(g_Colors.textPrimary), "RIA");
#else
        addText(g_FreeFontBrand, 34.0f, ImVec2(p.x + 27.0f, p.y + 32.0f), ImGui::GetColorU32(g_Colors.accentColor), "RUGIR");
        addText(g_FreeFontBrand, 34.0f, ImVec2(p.x + 132.0f, p.y + 32.0f), ImGui::GetColorU32(g_Colors.textPrimary), "INT");
#endif

        ImGui::SetCursorScreenPos(ImVec2(p.x + size.x - 34.0f, p.y + 16.0f));
        if (ImAdd::ButtonXMark("free-close-button", ImVec2(18.0f, 18.0f)))
            g_Visible = false;

        // License gate: until the server authorizes this machine, the whole
        // menu is replaced by the activation card. No tabs, no features.
        if (!Auth::IsAuthorized())
        {
            const float cardWidth = size.x - sidebarWidth - 48.0f;
            ImGui::SetCursorScreenPos(ImVec2(p.x + sidebarWidth + 24.0f, p.y + 150.0f));
            ImGui::BeginGroup();
            ImGui::PushItemWidth(cardWidth > 200.0f ? cardWidth : 200.0f);
            RenderProtectedAccessCard(cardWidth > 200.0f ? cardWidth : 200.0f);
            ImGui::PopItemWidth();
            ImGui::EndGroup();
            ImGui::End();
            return;
        }

        // Once authorized, keep the lobby "Access" section open too.
        g_LicenseSectionUnlocked = true;

        ImGui::SetCursorScreenPos(ImVec2(p.x + 8.0f, p.y + 104.0f));
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f));
        for (int i = 0; i < pageCount; i++)
        {
            if (sidebarTab(pages[i].label, pages[i].icon, g_SelectedTab == i, ImVec2(220.0f, 42.0f)))
                g_SelectedTab = i;
        }
        ImGui::PopStyleVar();
        ImGui::EndGroup();

        const Page& page = pages[g_SelectedTab];
        if (selectedSubtabs[g_SelectedTab] < 0 || selectedSubtabs[g_SelectedTab] >= page.subtabCount)
            selectedSubtabs[g_SelectedTab] = 0;

        const float contentX = p.x + sidebarWidth + 14.0f;
        const float contentRight = p.x + size.x - 18.0f;
        const ImVec2 pageLabelSize = calcTextSize(g_FreeFontTitle, 23.0f, page.label);
        addText(g_FreeFontTitle, 23.0f, ImVec2(contentX, p.y + 18.0f), ImGui::GetColorU32(g_Colors.textPrimary), "[");
        addText(g_FreeFontTitle, 23.0f, ImVec2(contentX + 13.0f, p.y + 18.0f), ImGui::GetColorU32(g_Colors.accentColor), page.label);
        addText(g_FreeFontTitle, 23.0f, ImVec2(contentX + 18.0f + pageLabelSize.x, p.y + 18.0f), ImGui::GetColorU32(g_Colors.textPrimary), "]");

        char fpsText[32] = {};
        snprintf(fpsText, sizeof(fpsText), "FPS %.0f", ImGui::GetIO().Framerate);
        ImGui::PushFont(g_FreeFontSmall ? g_FreeFontSmall : g_FreeFont);
        const ImVec2 fpsTextSize = ImGui::CalcTextSize(fpsText);
        ImGui::PopFont();
        const ImVec2 fpsBoxSize(fpsTextSize.x + 18.0f, 24.0f);
        const ImVec2 fpsBoxMin((std::max)(contentX + 180.0f, p.x + size.x - 128.0f), p.y + 55.0f);
        const ImVec2 fpsBoxMax(fpsBoxMin.x + fpsBoxSize.x, fpsBoxMin.y + fpsBoxSize.y);
        drawList->AddRectFilled(fpsBoxMin, fpsBoxMax, ImGui::GetColorU32(ImVec4(15.0f / 255.0f, 17.0f / 255.0f, 21.0f / 255.0f, 0.58f)), 4.0f);
        drawList->AddRect(fpsBoxMin, fpsBoxMax, ImGui::GetColorU32(ImVec4(g_Colors.accentColor.x, g_Colors.accentColor.y, g_Colors.accentColor.z, 0.30f)), 4.0f);
        addText(g_FreeFontSmall, 14.0f, ImVec2(fpsBoxMin.x + 9.0f, fpsBoxMin.y + 4.0f), ImGui::GetColorU32(g_Colors.textSecondary), fpsText);

        float subtabY = p.y + 56.0f;
        ImGui::SetCursorScreenPos(ImVec2(contentX, subtabY));
        ImGui::BeginGroup();
        for (int i = 0; i < page.subtabCount; i++)
        {
            if (i > 0)
                ImGui::SameLine();

            float labelWidth = calcTextSize(g_FreeFontSmall, 14.0f, page.subtabs[i].label).x + 28.0f;
            labelWidth = (std::max)(84.0f, labelWidth);
            if (subtabButton(page.subtabs[i].label, selectedSubtabs[g_SelectedTab] == i, ImVec2(labelWidth, 28.0f)))
                selectedSubtabs[g_SelectedTab] = i;
        }
        ImGui::EndGroup();

        const float contentTop = p.y + 96.0f;
        const ImVec2 contentSize((std::max)(240.0f, contentRight - contentX), (std::max)(160.0f, p.y + size.y - contentTop - 18.0f));
        char pageId[32] = {};
        snprintf(pageId, sizeof(pageId), "free-page:%d:%d", g_SelectedTab, selectedSubtabs[g_SelectedTab]);

        ImGui::SetCursorScreenPos(ImVec2(contentX, contentTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 10.0f));
        bool childOpen = ImGui::BeginChild(pageId, contentSize, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        if (childOpen)
        {
            float groupWidth = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
            int subtab = selectedSubtabs[g_SelectedTab];
            switch (g_SelectedTab)
            {
            case 0: RenderPreviewEspPage(subtab, groupWidth); break;
            case 1: RenderPreviewCharacterPage(subtab, groupWidth); break;
            case 2: RenderPreviewAimbotPage(subtab, groupWidth); break;
            case 3: RenderPreviewCombatPage(subtab, groupWidth); break;
            case 4: RenderPreviewHacksPage(subtab, groupWidth); break;
            case 5: RenderPreviewLobbyPage(subtab, groupWidth); break;
            case 6: RenderPreviewSettingsPage(groupWidth); break;
            default: break;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        ImGui::End();
    }
    // ============================================================================
    // INITIALIZATION & SHUTDOWN
    // ============================================================================
    void SetModuleHandle(HMODULE module)
    {
        if (module)
            g_DllModule = module;
    }

    bool PreloadEmbeddedVideoResource(HMODULE module)
    {
        if (!module)
        {
            g_MenuVideoStatus = "module fail";
            return false;
        }

        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_MENU_BACKGROUND_VIDEO), RT_RCDATA);
        if (!resource)
        {
            g_MenuVideoStatus = "resource fail";
            return false;
        }

        const DWORD resourceSize = SizeofResource(module, resource);
        HGLOBAL loadedResource = LoadResource(module, resource);
        const void* resourceBytes = loadedResource ? LockResource(loadedResource) : nullptr;
        if (!resourceBytes || resourceSize == 0)
        {
            g_MenuVideoStatus = "resource empty";
            return false;
        }

        g_MenuVideoEmbeddedBytes.assign(
            static_cast<const unsigned char*>(resourceBytes),
            static_cast<const unsigned char*>(resourceBytes) + resourceSize);
        g_MenuVideoStatus = "resource cached";
        return !g_MenuVideoEmbeddedBytes.empty();
    }

    bool Initialize(IDXGISwapChain* pSwapChain, HWND hWnd)
    {
        if (g_Initialized)
            return true;

        if (!hWnd)
        {
return false;
        }

        if (!pSwapChain)
        {
return false;
        }

        g_GameWindow = hWnd;
        g_Visible = true;
        g_MenuHotkeyDown = false;

        // Create ImGui context
        g_Context = ImGui::CreateContext();
        if (!g_Context)
        {
return false;
        }

        ImGui::SetCurrentContext(g_Context);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disabled by default - only enable when configuring hotkey
        // NOTE: Cursor visibility is now controlled dynamically in Render() based on menu state
        io.IniFilename = nullptr;

        // Load fonts with high-quality rendering
        // Police embarqu�e dans le dossier du DLL pour fiabilit� maximale
        io.Fonts->Clear();

        ImFontConfig freeFontConfig;
        freeFontConfig.FontDataOwnedByAtlas = false;
        freeFontConfig.OversampleH = 3;
        freeFontConfig.OversampleV = 2;

        g_FreeFont = io.Fonts->AddFontFromMemoryTTF(
            Inter_SemmiBold,
            sizeof(Inter_SemmiBold),
            17.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());
        g_FreeFontLarge = io.Fonts->AddFontFromMemoryTTF(
            Inter_SemmiBold,
            sizeof(Inter_SemmiBold),
            18.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());
        g_FreeFontTitle = io.Fonts->AddFontFromMemoryTTF(
            Inter_SemmiBold,
            sizeof(Inter_SemmiBold),
            23.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());
        g_FreeFontSmall = io.Fonts->AddFontFromMemoryTTF(
            Inter_SemmiBold,
            sizeof(Inter_SemmiBold),
            15.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());
        g_FreeFontBrand = io.Fonts->AddFontFromMemoryTTF(
            Inter_Bold,
            sizeof(Inter_Bold),
            34.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());
        g_FreeIconFont = io.Fonts->AddFontFromMemoryTTF(
            Icon_Pack,
            sizeof(Icon_Pack),
            26.0f,
            &freeFontConfig,
            io.Fonts->GetGlyphRangesDefault());

        if (!g_FreeFont)
            g_FreeFont = io.Fonts->AddFontDefault();

        io.FontGlobalScale = 1.0f;
        
        // Load symbolfont (icon font from memory)
        ImFontConfig symbolFontConfig;
        symbolFontConfig.SizePixels = 16.0f;
        symbolFontConfig.OversampleH = 3;
        symbolFontConfig.OversampleV = 3;
        g_SymbolFont = io.Fonts->AddFontFromMemoryTTF(
            (void*)icon, sizeof(icon), 16.0f, &symbolFontConfig
        );
        
        // Fallback: Load Segoe UI font from memory
        if (!g_SymbolFont)
        {
            ImFontConfig segueFontConfig;
            segueFontConfig.SizePixels = 14.0f;
            segueFontConfig.OversampleH = 3;
            segueFontConfig.OversampleV = 3;
            g_SymbolFont = io.Fonts->AddFontFromMemoryTTF(
                (void*)seguoe, sizeof(seguoe), 14.0f, &segueFontConfig
            );
        }
        
        // Rebuild fonts after adding symbol font
        io.Fonts->Build();

        try
        {
            ImGui::StyleColorsDark();
            SetupFreeImGuiTheme();
        }
        catch (...)
        {
return false;
        }

        // Try to initialize ImGui backends
        int retries = 0;
        const int MAX_RETRIES = 5;

        while (retries < MAX_RETRIES)
        {
            try
            {
                if (!ImGui_ImplWin32_Init(hWnd))
                {
retries++;
                    continue;
                }

                ID3D11Device* pDevice = D3D11Hook::GetDevice();
                ID3D11DeviceContext* pContext = D3D11Hook::GetContext();

                if (!pDevice || !pContext)
                {
ImGui_ImplWin32_Shutdown();
                    retries++;
                    continue;
                }

                if (!ImGui_ImplDX11_Init(pDevice, pContext))
                {
ImGui_ImplWin32_Shutdown();
                    return false;
                }

                g_Device = pDevice;
                g_DeviceContext = pContext;

                // Initialize SVG loader for dynamic logo rendering
                SVGLoader::SVGTextureGenerator::Initialize(pDevice, pContext);

                break;  // Success
            }
            catch (...)
            {
return false;
            }
        }

        if (retries >= MAX_RETRIES)
        {
return false;
        }

        try
        {
            g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
            g_WndProcRestored = false;
        }
        catch (...)
        {
return false;
        }

        // Initialize profile system
        SettingsManager::EnsureProfilesDirectory();
        g_ProfilesList = SettingsManager::GetProfilesList();
        if (g_ProfilesList.empty())
        {
            // Create default profile with all settings disabled
            SettingsManager::CreateDefaultProfile();
            g_ProfilesList = SettingsManager::GetProfilesList();
        }

        // Load the last saved profile if available
        if (!g_ProfilesList.empty())
        {
            // Get the most recently modified profile (last one saved)
            std::string lastProfileName = g_ProfilesList.back();
            if (SettingsManager::LoadCurrentProfile(lastProfileName))
            {
                g_CurrentProfileName = lastProfileName;
                Logger::LogInfo("[Menu] Loaded last saved profile: " + lastProfileName);
            }
        }

        // preview layout has no visible master switch. Keep the legacy
        // runtime gate enabled so preview-visible feature toggles are effective.
        g_Settings.EnableGlobal = true;

        // Initialize character icon loader
        ID3D11Device* iconDevice = D3D11Hook::GetDevice();
        ID3D11DeviceContext* iconContext = D3D11Hook::GetContext();
        if (iconDevice && iconContext)
        {
            g_Device = iconDevice;
            g_DeviceContext = iconContext;
            IconLoader::CharacterIconCache::Initialize(iconDevice, iconContext);
            Logger::LogInfo("[Menu] Character icon loader initialized");
            
            // Load RUGIR logo
            LoadRugirLogo();
            LoadPlatformIcons();
            if (g_Settings.EnableMenuBackgroundVideo)
                InitializeMenuBackgroundVideo();
        }

        g_Initialized = true;
return true;
    }

    void Shutdown()
    {
        if (!g_Initialized)
            return;

        RestoreWindowProc();

        for (int i = 0; i < 200 && g_WndProcCallDepth.load(std::memory_order_acquire) > 0; ++i)
            Sleep(1);

        g_OriginalWndProc = nullptr;
        g_WndProcRestored = false;

        ReleaseRenderTargetView();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();

        // Shutdown loaders
        SVGLoader::SVGTextureGenerator::Shutdown();
        if (g_RugirLogo)
        {
            g_RugirLogo->Release();
            g_RugirLogo = nullptr;
        }
        ReleasePlatformIcons();
        ReleaseMenuBackgroundVideo();
        g_CharacterIconGridCache.clear();
        IconLoader::CharacterIconCache::Shutdown();
        g_Device = nullptr;
        g_DeviceContext = nullptr;

        if (g_Context)
        {
            ImGui::DestroyContext(g_Context);
            g_Context = nullptr;
        }

        g_GameWindow = nullptr;
        g_Initialized = false;
}

    void InvalidateRenderTarget()
    {
        ReleaseRenderTargetView();
    }

    void RestoreWindowProc()
    {
        if (g_GameWindow && g_OriginalWndProc && !g_WndProcRestored)
        {
            SetWindowLongPtr(g_GameWindow, GWLP_WNDPROC, (LONG_PTR)g_OriginalWndProc);
            g_WndProcRestored = true;
        }
    }

    bool HasActiveWindowProc()
    {
        return g_WndProcCallDepth.load(std::memory_order_acquire) > 0;
    }

    // ============================================================================
    // HOTKEY LISTENER UPDATE - Called every frame
    // ============================================================================
    static void UpdateHotkeyListener()
    {
        if (!g_ListeningForHotkey)
        {
            StopHotkeyListening();
            return;
        }

        if (!IsKnownHotkeyId(g_CurrentHotkeyValue))
        {
            StopHotkeyListening();
            return;
        }

        if ((GetTickCount64() - g_HotkeyListenStartTime) > HOTKEY_LISTEN_TIMEOUT_MS)
        {
            StopHotkeyListening();
            return;
        }

        // ESC cancels without assigning it as a usable hotkey.
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
        {
            StopHotkeyListening();
            return;
        }

        const int keyboardKey = GetHotkey(0);
        const int gamepadKey = GetHotkey(1);

        if (g_HotkeyWaitingForRelease)
        {
            if (keyboardKey == 0 && gamepadKey == 0)
                g_HotkeyWaitingForRelease = false;

            return;
        }

        const int pressedKey = keyboardKey != 0 ? keyboardKey : gamepadKey;
        const int inputType = keyboardKey != 0 ? 0 : 1;  // 0=keyboard, 1=gamepad

        if (pressedKey == 0)
            return;

        if ((inputType == 0 && !IsValidKeyboardHotkey(pressedKey)) ||
            (inputType == 1 && !IsValidGamepadHotkey(pressedKey)))
        {
            return;
        }

        // Determine which hotkey we're listening for (unified system: 100=Aimbot, 101=Teleport, 102=Transform, 103=DuplicateImitation)
        int hotkeyType = g_CurrentHotkeyValue;

        // Update the corresponding hotkey setting based on unified type
        if (hotkeyType == 100)  // Aimbot hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.AimbotHoldKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.AimbotHoldKey.Xbox = pressedKey;
                g_Settings.AimbotHoldKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 101)  // Teleport hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.TeleportToKotaKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.TeleportToKotaKey.Xbox = pressedKey;
                g_Settings.TeleportToKotaKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 102)  // Transform hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.TransformIntoRandomESPKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.TransformIntoRandomESPKey.Xbox = pressedKey;
                g_Settings.TransformIntoRandomESPKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 103)  // DuplicateImitation hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.DuplicateIntoImitationRandomESPKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.DuplicateIntoImitationRandomESPKey.Xbox = pressedKey;
                g_Settings.DuplicateIntoImitationRandomESPKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 104)  // SetInvincible hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.SetInvincibleKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.SetInvincibleKey.Xbox = pressedKey;
                g_Settings.SetInvincibleKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 105)  // RebuildMyself hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.RebuildMyselfKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.RebuildMyselfKey.Xbox = pressedKey;
                g_Settings.RebuildMyselfKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 106)  // CopySkills hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.CopySkillsFromNearestEnemyKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.CopySkillsFromNearestEnemyKey.Xbox = pressedKey;
                g_Settings.CopySkillsFromNearestEnemyKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 107)  // GenerateProjectile hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.GenerateProjectileKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.GenerateProjectileKey.Xbox = pressedKey;
                g_Settings.GenerateProjectileKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 108)  // No Collision hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.NoCollisionHoldKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.NoCollisionHoldKey.Xbox = pressedKey;
                g_Settings.NoCollisionHoldKey.PS4 = pressedKey;
            }
        }
        else if (hotkeyType == 109)  // Custom Drop hotkey (unified)
        {
            if (inputType == 0)  // Keyboard
                g_Settings.CustomDropKey.Keyboard = pressedKey;
            else  // Gamepad
            {
                g_Settings.CustomDropKey.Xbox = pressedKey;
                g_Settings.CustomDropKey.PS4 = pressedKey;
            }
        }

        StopHotkeyListening();
    }

    static bool HasPendingCharacterConditionAutoExecution()
    {
        return g_Settings.EnableGlobal &&
            (g_HackSettings.CharCondition_EnableDekuMode ||
             g_HackSettings.CharCondition_EnableUnbreakable ||
             g_HackSettings.CharCondition_EnableCompressionRegen ||
             g_HackSettings.CharCondition_EnableMirioMode ||
             g_HackSettings.CharCondition_EnableTokoyamiMode ||
             g_HackSettings.CharCondition_AutoExecute);
    }

    static bool HasSdkOverlayDraw()
    {
        if (!g_Settings.EnableGlobal)
            return false;

        return g_Settings.EnablePlayerESP ||
            g_Settings.ShowPlayerSkeleton ||
            g_Settings.AimbotDrawCrosshair ||
            (g_Settings.EnableAimbot && (g_Settings.AimbotDrawLine || g_Settings.AimbotDrawFOV)) ||
            g_Settings.EnableBulletTP;
    }

    static bool NeedsImGuiFrame()
    {
        return g_Visible ||
            g_PlayerNetworkTableVisible ||
            g_PlayerListHotkeyHintVisible ||
            HasSdkOverlayDraw();
    }

    // ============================================================================
    // MAIN RENDER FUNCTION - MHUR PATTERN
    // ============================================================================
    void Render(IDXGISwapChain* pSwapChain)
    {
        if (!g_Initialized || !pSwapChain)
            return;

        ImGui::SetCurrentContext(g_Context);

        // Update hotkey listener state every frame
        UpdateHotkeyListener();
        ProcessTimedLicenseClaimTest();

        if (HasPendingCharacterConditionAutoExecution())
        {
            static DWORD s_lastConditionAutoQueueTick = 0;
            const DWORD now = GetTickCount();
            if (now - s_lastConditionAutoQueueTick >= 250)
            {
                s_lastConditionAutoQueueTick = now;

                const bool enableDekuMode = g_HackSettings.CharCondition_EnableDekuMode;
                const bool enableUnbreakable = g_HackSettings.CharCondition_EnableUnbreakable;
                const bool enableCompressionRegen = g_HackSettings.CharCondition_EnableCompressionRegen;
                const bool enableMirioMode = g_HackSettings.CharCondition_EnableMirioMode;
                const bool enableTokoyamiMode = g_HackSettings.CharCondition_EnableTokoyamiMode;
                const bool enableGenericCondition = g_HackSettings.CharCondition_AutoExecute;
                const int conditionId = g_HackSettings.CharCondition_SelectedConditionId;
                const int applyMode = g_HackSettings.CharCondition_ApplyMode;
                const int level = g_HackSettings.CharCondition_Level;
                const float duration = g_HackSettings.CharCondition_Duration;
                const float value = g_HackSettings.CharCondition_Value;
                const float interval = g_HackSettings.CharCondition_Interval;
                const int subLevel = g_HackSettings.CharCondition_SubLevel;
                const bool timeOverwrite = g_HackSettings.CharCondition_TimeOverwrite;

                EnqueueGameThreadMenuTask([enableDekuMode, enableUnbreakable, enableCompressionRegen, enableMirioMode,
                                           enableTokoyamiMode, enableGenericCondition, conditionId, applyMode, level,
                                           duration, value, interval, subLevel, timeOverwrite]() {
                    InGameHack_ProcessCharacterConditionAutoExecution(
                        enableDekuMode,
                        enableUnbreakable,
                        enableCompressionRegen,
                        enableMirioMode,
                        enableTokoyamiMode,
                        enableGenericCondition,
                        conditionId,
                        applyMode,
                        level,
                        duration,
                        value,
                        interval,
                        subLevel,
                        timeOverwrite
                    );
                }, "Character Condition Auto Execute");
            }
        }

        // ⭐ RETRY MECHANISM: Try to initialize GameThreadHook every frame if not active yet
        if (!GameThreadHook::IsHookActive())
        {
            static int retryCounter = 0;
            retryCounter++;
            if (retryCounter % 30 == 0)  // Retry every ~30 frames (0.5s at 60 FPS)
            {
                if (GameThreadHook::Initialize())
                {
}
                // Else: GWorld not ready yet, retry next attempt
            }
        }

        UpdatePlayerListHotkeyHintState();

        const bool needsFrame = NeedsImGuiFrame();
        if (!needsFrame)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = false;
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
            return;
        }

        // Get device and context from D3D11Hook (via getter)
        ID3D11Device* device = D3D11Hook::GetDevice();
        ID3D11DeviceContext* context = D3D11Hook::GetContext();
        if (!device || !context)
            return;

        g_Device = device;
        g_DeviceContext = context;

        if (!EnsureRenderTargetView(pSwapChain, device))
            return;

        // ========================================================================
        // STEP 3: SET IMGUI CONTEXT & BEGIN NEW FRAME
        // ========================================================================
        // Set display size to game window dimensions
        ImGuiIO& io = ImGui::GetIO();
        RECT rect{};
        if (GetClientRect(g_GameWindow, &rect))
        {
            io.DisplaySize = ImVec2(
                static_cast<float>(rect.right - rect.left),
                static_cast<float>(rect.bottom - rect.top)
            );
        }
        
        // ===== CURSOR VISIBILITY CONTROL =====
        // Show mouse cursor when an ImGui window is visible.
        io.MouseDrawCursor = g_Visible || g_PlayerNetworkTableVisible;
        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_Settings.ShowServerStatusOverlay)
            DrawServerStatusOverlay();
        DrawPlayerListHotkeyHint();
        
        // Hotkey capture polls XInput directly. Keep ImGui gamepad navigation disabled so
        // the menu never steals controller input or changes nav state mid-frame.
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;

        // ========================================================================
        // STEP 3B: SDK RENDERING (on RenderThread with ImGui context)
        // ========================================================================
        if (Auth::IsAuthorized() && g_Settings.EnableGlobal)
        {
            if (g_Settings.EnablePlayerESP)
            {
                SDK_DrawAllRectangles();
                SDK_DrawAllTextLabels();
            }
            
            // Only draw skeletons if enabled
            if (g_Settings.ShowPlayerSkeleton)
            {
                SDK_DrawAllSkeletons();
            }
            
            if (g_Settings.EnableAimbot && g_Settings.AimbotDrawLine)
                SDK_DrawAimbotInfo();
            if (g_Settings.EnableAimbot && g_Settings.AimbotDrawFOV)
                SDK_DrawAimbotFOV();
            if (g_Settings.AimbotDrawCrosshair)
                SDK_DrawCrosshair();
            if (g_Settings.EnableBulletTP)
                SDK_DrawBulletRedirectionFOV();
        }

        // ========================================================================
        // STEP 4: DRAW MAIN MENU WINDOW (cheat-factory structure)
        // ========================================================================
        if (g_Visible)
        {
            DrawFreeImGuiStyleMenu();
        }
        DrawPlayerNetworkTableWindow();

        // ========================================================================
        // STEP 5: DRAW ESP BOXES (always drawn, menu or not)
        // ========================================================================
        // Rectangles are drawn via SDK_DrawAllRectangles() called after ImGui::NewFrame()

        // ========================================================================
        // STEP 6: BUILD IMGUI DRAW DATA
        // ========================================================================
        ImGui::Render();

        // ========================================================================
        // STEP 7: SAVE CURRENT RENDER TARGETS
        // ========================================================================
        ID3D11RenderTargetView* oldRTV = nullptr;
        ID3D11DepthStencilView* oldDSV = nullptr;
        context->OMGetRenderTargets(1, &oldRTV, &oldDSV);

        // ========================================================================
        // STEP 8: SET LOCAL RTV (BACKBUFFER) FOR IMGUI DRAWING
        // ========================================================================
        context->OMSetRenderTargets(1, &g_RenderTargetView, nullptr);

        // ========================================================================
        // STEP 9: RENDER IMGUI DRAW DATA TO GPU
        // ========================================================================
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // ========================================================================
        // STEP 10: RESTORE ORIGINAL RENDER TARGETS
        // ========================================================================
        context->OMSetRenderTargets(1, &oldRTV, oldDSV);

        // ========================================================================
        // STEP 11: CLEANUP - RELEASE COM OBJECTS
        // ========================================================================
        if (oldRTV)
            oldRTV->Release();
        if (oldDSV)
            oldDSV->Release();

    }

    bool IsInitialized() { return g_Initialized; }
    bool IsVisible() { return g_Visible; }
    void SetVisible(bool visible) { g_Visible = visible; }
}
