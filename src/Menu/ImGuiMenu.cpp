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
#include "../Core/UnloadManager.h"
#include <algorithm>
#include <climits>
#include <unordered_map>
#include "../SDK/SDKInit.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include <cmath>
#include <cstring>
#include <atomic>

// ============================================================================
// CHARACTER CHANGER GLOBAL STATE (Used by Character_Changer.cpp)
// ============================================================================
int s_techAlpha = 1;           // Alpha skill level (0-10)
int s_techBeta = 1;            // Beta skill level (0-10)
int s_techGamma = 1;           // Gamma skill level (0-10)
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

#include "../../cheat factory/projects/shared/Resources/textures/menu/image_running.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_code.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_eye.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_misc.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_palette.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_settings.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_target.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_click.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_clock.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_crime.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_cursor.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_evil.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_globe.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_knife.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_location.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_objects.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_pulse.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_verified.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_wrench.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_clear.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_open.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_save.h"
#include "../../cheat factory/projects/shared/Resources/textures/menu/image_play.h"
#include "../../cheat factory/projects/shared/Resources/fonts/font_inter_semibold.h"
#include "../../cheat factory/projects/shared/Resources/fonts/font_cascadia_mono_pl_regular.h"

// Forward declare ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare SDK canvas resolution function

// Forward declare unload function
extern "C" __declspec(dllimport) void DLL_Unload();

// Forward declare SDK drawing functions
extern "C" void SDK_DrawAllRectangles();
extern "C" void SDK_DrawAllTextLabels();
extern "C" void SDK_DrawAllSkeletons();
extern "C" void SDK_DrawAimbotInfo();
extern "C" void SDK_DrawAimbotFOV();
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
    ImFont* g_SymbolFont = nullptr;  // Font for Zero1-style icons

    // ============================================================================
    // GLOBAL MENU STATE
    // ============================================================================
    static int g_SelectedTab = 0;  // 0=ESP, 1=CHARACTER, 2=AIMBOT, 3=COMBAT, 4=HACKS, 5=LOBBY EXPLOIT, 6=SETTINGS
    
    static bool g_Initialized = false;
    static bool g_Visible = true;
    static bool g_MenuHotkeyDown = false;
    static bool g_PlayerNetworkTableVisible = false;
    static bool g_PlayerNetworkTableHotkeyDown = false;
    static constexpr int PLAYER_NETWORK_TABLE_HOTKEY = VK_F8;
    static constexpr bool SHOW_SERVER_STATUS_OVERLAY = true;
    static bool g_PlayerListHotkeyHintVisible = false;
    static bool g_WasInValidBattleModeForHint = false;
    static DWORD g_LastBattleModeHintCheckTick = 0;
    static DWORD g_PlayerListHotkeyHintStartTick = 0;
    static constexpr DWORD PLAYER_LIST_HINT_CHECK_INTERVAL_MS = 1000;
    static constexpr DWORD PLAYER_LIST_HINT_DURATION_MS = 7000;
    static constexpr int PLATFORM_ICON_COUNT = 6;
    static ID3D11ShaderResourceView* g_PlatformIcons[PLATFORM_ICON_COUNT]{};
    static ImGuiContext* g_Context = nullptr;
    static HWND g_GameWindow = nullptr;
    static WNDPROC g_OriginalWndProc = nullptr;
    static bool g_WndProcRestored = false;
    static std::atomic<int> g_WndProcCallDepth(0);
    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_DeviceContext = nullptr;
    static ID3D11RenderTargetView* g_RenderTargetView = nullptr;
    static ID3D11ShaderResourceView* g_IconRunning = nullptr;
    static ID3D11ShaderResourceView* g_IconCode = nullptr;
    static ID3D11ShaderResourceView* g_IconEye = nullptr;
    static ID3D11ShaderResourceView* g_IconMisc = nullptr;
    static ID3D11ShaderResourceView* g_IconPalette = nullptr;
    static ID3D11ShaderResourceView* g_IconSettings = nullptr;
    static ID3D11ShaderResourceView* g_IconTarget = nullptr;
    static ID3D11ShaderResourceView* g_IconClick = nullptr;
    static ID3D11ShaderResourceView* g_IconClock = nullptr;
    static ID3D11ShaderResourceView* g_IconCrime = nullptr;
    static ID3D11ShaderResourceView* g_IconCursor = nullptr;
    static ID3D11ShaderResourceView* g_IconEvil = nullptr;
    static ID3D11ShaderResourceView* g_IconGlobe = nullptr;
    static ID3D11ShaderResourceView* g_IconKnife = nullptr;
    static ID3D11ShaderResourceView* g_IconLocation = nullptr;
    static ID3D11ShaderResourceView* g_IconObjects = nullptr;
    static ID3D11ShaderResourceView* g_IconPulse = nullptr;
    static ID3D11ShaderResourceView* g_IconVerified = nullptr;
    static ID3D11ShaderResourceView* g_IconWrench = nullptr;
    static ID3D11ShaderResourceView* g_IconClear = nullptr;
    static ID3D11ShaderResourceView* g_IconSave = nullptr;
    static ID3D11ShaderResourceView* g_IconOpen = nullptr;
    static ID3D11ShaderResourceView* g_IconRun = nullptr;

    // Profile Management
    static std::string g_CurrentProfileName = "Default";
    static char g_ProfileNameBuffer[256] = "Default";
    static std::vector<std::string> g_ProfilesList;

    // Section open/close states
    static bool g_ESP_DisplayOpen = true;
    static bool g_ESP_FiltersOpen = true;
    
    // Hotkey listening state (Zero1 exact)
    static bool g_ListeningForHotkey = false;
    static bool g_HotkeyWaitingForRelease = false;
    static int g_HotkeyListenStartTime = 0;
    static int g_CurrentHotkeyValue = 0;  // Temporary value being set

    // ============================================================================
    // HOTKEY LISTENER FUNCTION (Zero1 exact)
    // ============================================================================
    /**
     * @brief Listen for hotkey input (keyboard or gamepad) for 5 seconds
     * Returns 0 if timeout or ESC pressed, otherwise returns the key code
     */
    static int GetHotkey(int currentKey, int inputType)
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
                    // Check all button masks (XInput constants)
                    int buttonMasks[] = {
                        0x1000,  // A
                        0x2000,  // B
                        0x4000,  // X
                        0x8000,  // Y
                        0x0100,  // LB (Left Shoulder)
                        0x0200,  // RB (Right Shoulder)
                        0x0020,  // Back
                        0x0010   // Start
                    };
                    
                    for (int mask : buttonMasks)
                    {
                        if ((xInputState.Gamepad.wButtons & mask) != 0)
                        {
                            return mask;  // Return the button mask
                        }
                    }
                    
                    // Check analog triggers (L2/LT and R2/RT)
                    // Use special codes: 0x1F00 for LT, 0x2F00 for RT
                    const int TRIGGER_THRESHOLD = 128;
                    
                    if (xInputState.Gamepad.bLeftTrigger > TRIGGER_THRESHOLD)
                    {
                        return 0x1F00;  // Special code for LT/L2
                    }
                    
                    if (xInputState.Gamepad.bRightTrigger > TRIGGER_THRESHOLD)
                    {
                        return 0x2F00;  // Special code for RT/R2
                    }
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
        // Keyboard/Mouse names
        if (inputType == 0)  // HotKeyType::KeyboardMouse
        {
            switch (keyCode)
            {
                // Mouse buttons
                case 0x01: return "Left Mouse";
                case 0x02: return "Right Mouse";
                case 0x04: return "Middle Mouse";
                
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
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "Key 0x%02X", keyCode);
                    return buffer;
                }
            }
        }
        // Gamepad names (Xbox/PS4)
        else if (inputType == 1)  // HotKeyType::Gamepad
        {
            switch (keyCode)
            {
                case 0x1000: return "A";
                case 0x2000: return "B";
                case 0x4000: return "X";
                case 0x8000: return "Y";
                case 0x0100: return "LB";
                case 0x0200: return "RB";
                case 0x0020: return "Back";
                case 0x0010: return "Start";
                case 0x1F00: return "LT";
                case 0x2F00: return "RT";
                default:
                {
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "Button 0x%X", keyCode);
                    return buffer;
                }
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

    static ID3D11ShaderResourceView* LoadAssetTexture(const char* relativePath)
    {
        if (!relativePath || !relativePath[0])
            return nullptr;

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

    static ID3D11ShaderResourceView* CreatePreviewTextureFromBytes(ID3D11Device* device, const unsigned char* bytes, size_t byteCount)
    {
        if (!device || !bytes || byteCount == 0)
            return nullptr;

        HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool shouldUninitializeCom = SUCCEEDED(comHr);

        IWICImagingFactory* factory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        IWICStream* stream = nullptr;
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), (LPVOID*)&factory);
        if (SUCCEEDED(hr) && factory)
            hr = factory->CreateStream(&stream);
        if (SUCCEEDED(hr) && stream)
            hr = stream->InitializeFromMemory((BYTE*)bytes, (DWORD)byteCount);
        if (SUCCEEDED(hr) && factory && stream)
            hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (SUCCEEDED(hr) && decoder)
            hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr) && factory)
            hr = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr) && converter && frame)
        {
            hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        }

        UINT width = 0;
        UINT height = 0;
        if (SUCCEEDED(hr) && converter)
            hr = converter->GetSize(&width, &height);

        if (SUCCEEDED(hr) && width > 0 && height > 0)
        {
            UINT stride = width * 4;
            UINT dataSize = stride * height;
            std::vector<BYTE> pixels(dataSize);
            hr = converter->CopyPixels(nullptr, stride, dataSize, pixels.data());
            if (SUCCEEDED(hr))
            {
                D3D11_TEXTURE2D_DESC texDesc = {};
                texDesc.Width = width;
                texDesc.Height = height;
                texDesc.MipLevels = 1;
                texDesc.ArraySize = 1;
                texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                texDesc.SampleDesc.Count = 1;
                texDesc.Usage = D3D11_USAGE_IMMUTABLE;
                texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                D3D11_SUBRESOURCE_DATA initData = {};
                initData.pSysMem = pixels.data();
                initData.SysMemPitch = stride;

                hr = device->CreateTexture2D(&texDesc, &initData, &texture);
                if (SUCCEEDED(hr) && texture)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = texDesc.Format;
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
                }
            }
        }

        if (texture) texture->Release();
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        if (shouldUninitializeCom)
            CoUninitialize();

        return SUCCEEDED(hr) ? srv : nullptr;
    }

    static void ReleasePreviewTexture(ID3D11ShaderResourceView*& texture)
    {
        if (texture)
        {
            texture->Release();
            texture = nullptr;
        }
    }

    static void LoadPreviewMenuIcons(ID3D11Device* device)
    {
        if (!device || g_IconEye)
            return;

        g_IconRunning = CreatePreviewTextureFromBytes(device, running_bytes, sizeof(running_bytes));
        g_IconCode = CreatePreviewTextureFromBytes(device, code_bytes, sizeof(code_bytes));
        g_IconEye = CreatePreviewTextureFromBytes(device, eye_bytes, sizeof(eye_bytes));
        g_IconMisc = CreatePreviewTextureFromBytes(device, misc_bytes, sizeof(misc_bytes));
        g_IconPalette = CreatePreviewTextureFromBytes(device, palette_bytes, sizeof(palette_bytes));
        g_IconSettings = CreatePreviewTextureFromBytes(device, settings_bytes, sizeof(settings_bytes));
        g_IconTarget = CreatePreviewTextureFromBytes(device, target_bytes, sizeof(target_bytes));
        g_IconClick = CreatePreviewTextureFromBytes(device, click_bytes, sizeof(click_bytes));
        g_IconClock = CreatePreviewTextureFromBytes(device, clock_bytes, sizeof(clock_bytes));
        g_IconCrime = CreatePreviewTextureFromBytes(device, crime_bytes, sizeof(crime_bytes));
        g_IconCursor = CreatePreviewTextureFromBytes(device, cursor_bytes, sizeof(cursor_bytes));
        g_IconEvil = CreatePreviewTextureFromBytes(device, evil_bytes, sizeof(evil_bytes));
        g_IconGlobe = CreatePreviewTextureFromBytes(device, globe_bytes, sizeof(globe_bytes));
        g_IconKnife = CreatePreviewTextureFromBytes(device, knife_bytes, sizeof(knife_bytes));
        g_IconLocation = CreatePreviewTextureFromBytes(device, location_bytes, sizeof(location_bytes));
        g_IconObjects = CreatePreviewTextureFromBytes(device, objects_bytes, sizeof(objects_bytes));
        g_IconPulse = CreatePreviewTextureFromBytes(device, pulse_bytes, sizeof(pulse_bytes));
        g_IconVerified = CreatePreviewTextureFromBytes(device, verified_bytes, sizeof(verified_bytes));
        g_IconWrench = CreatePreviewTextureFromBytes(device, wrench_bytes, sizeof(wrench_bytes));
        g_IconClear = CreatePreviewTextureFromBytes(device, clear_bytes, sizeof(clear_bytes));
        g_IconSave = CreatePreviewTextureFromBytes(device, save_bytes, sizeof(save_bytes));
        g_IconOpen = CreatePreviewTextureFromBytes(device, open_bytes, sizeof(open_bytes));
        g_IconRun = CreatePreviewTextureFromBytes(device, play_bytes, sizeof(play_bytes));
    }

    static void ReleasePreviewMenuIcons()
    {
        ReleasePreviewTexture(g_IconRunning);
        ReleasePreviewTexture(g_IconCode);
        ReleasePreviewTexture(g_IconEye);
        ReleasePreviewTexture(g_IconMisc);
        ReleasePreviewTexture(g_IconPalette);
        ReleasePreviewTexture(g_IconSettings);
        ReleasePreviewTexture(g_IconTarget);
        ReleasePreviewTexture(g_IconClick);
        ReleasePreviewTexture(g_IconClock);
        ReleasePreviewTexture(g_IconCrime);
        ReleasePreviewTexture(g_IconCursor);
        ReleasePreviewTexture(g_IconEvil);
        ReleasePreviewTexture(g_IconGlobe);
        ReleasePreviewTexture(g_IconKnife);
        ReleasePreviewTexture(g_IconLocation);
        ReleasePreviewTexture(g_IconObjects);
        ReleasePreviewTexture(g_IconPulse);
        ReleasePreviewTexture(g_IconVerified);
        ReleasePreviewTexture(g_IconWrench);
        ReleasePreviewTexture(g_IconClear);
        ReleasePreviewTexture(g_IconSave);
        ReleasePreviewTexture(g_IconOpen);
        ReleasePreviewTexture(g_IconRun);
    }

    static void LoadPlatformIcons()
    {
        struct PlatformIconAsset
        {
            int platform;
            const char* path;
        };

        const PlatformIconAsset assets[] = {
            { 1, "src\\Assets\\Psn-Logo-PNG-Photos.png" },
            { 2, "src\\Assets\\xbox.png" },
            { 3, "src\\Assets\\Steam_icon_logo.svg.png" },
            { 4, "src\\Assets\\Nintendo-switch-icon.png" }
        };

        for (const PlatformIconAsset& asset : assets)
        {
            if (asset.platform <= 0 || asset.platform >= PLATFORM_ICON_COUNT || g_PlatformIcons[asset.platform])
                continue;

            g_PlatformIcons[asset.platform] = LoadAssetTexture(asset.path);
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

    static void SetupCheatFactoryPreviewTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const PreviewMenuTheme& theme = GetActivePreviewTheme();
        ApplyPreviewThemeToMenuColors(theme);

        style.WindowRounding    = 7.0f;
        style.ChildRounding     = 5.0f;
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 3.0f;

        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 1.0f;
        style.FrameBorderSize   = 0.0f;
        style.PopupBorderSize   = 0.0f;

        style.WindowPadding     = ImVec2(16.0f, 16.0f);
        style.ItemSpacing       = style.WindowPadding;
        style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
        style.FramePadding      = ImVec2(12.0f, 12.0f);
        style.CellPadding       = ImVec2(4.0f, 2.0f);

        style.ScrollbarSize     = 6.0f;
        style.GrabMinSize       = 14.0f;
        style.GrabRounding      = style.GrabMinSize / 2.0f;
        style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]             = theme.windowBg;
        colors[ImGuiCol_ChildBg]              = AlphaColor(theme.cardBg, 0.6f);
        colors[ImGuiCol_PopupBg]              = theme.cardBg;
        colors[ImGuiCol_TitleBg]              = theme.cardBg;
        colors[ImGuiCol_TitleBgActive]        = colors[ImGuiCol_TitleBg];
        colors[ImGuiCol_TitleBgCollapsed]     = colors[ImGuiCol_TitleBg];

        colors[ImGuiCol_Text]                 = theme.text;
        colors[ImGuiCol_TextDisabled]         = AlphaColor(theme.textDisabled, 0.7f);
        colors[ImGuiCol_CheckMark]            = theme.text;

        colors[ImGuiCol_Border]               = AlphaColor(theme.border, 0.75f);
        colors[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_Separator]            = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered]     = theme.accentHover;
        colors[ImGuiCol_SeparatorActive]      = theme.accentActive;

        colors[ImGuiCol_Header]               = theme.accent;
        colors[ImGuiCol_HeaderHovered]        = theme.accentHover;
        colors[ImGuiCol_HeaderActive]         = theme.accentActive;

        colors[ImGuiCol_SliderGrab]           = colors[ImGuiCol_Header];
        colors[ImGuiCol_SliderGrabActive]     = colors[ImGuiCol_HeaderActive];
        colors[ImGuiCol_TextSelectedBg]       = colors[ImGuiCol_Header];
        colors[ImGuiCol_TextSelectedBg].w     = 0.4f;

        colors[ImGuiCol_FrameBg]              = AlphaColor(theme.surfaceBg, 0.9f);
        colors[ImGuiCol_FrameBgHovered]       = AlphaColor(theme.surfaceHover, 0.9f);
        colors[ImGuiCol_FrameBgActive]        = AlphaColor(theme.surfaceActive, 0.9f);

        colors[ImGuiCol_Button]               = colors[ImGuiCol_FrameBg];
        colors[ImGuiCol_ButtonHovered]        = colors[ImGuiCol_FrameBgHovered];
        colors[ImGuiCol_ButtonActive]         = colors[ImGuiCol_FrameBgActive];

        colors[ImGuiCol_Tab]                  = LerpColor(theme.windowBg, theme.surfaceBg, 0.35f, 1.0f);
        colors[ImGuiCol_TabHovered]           = theme.surfaceHover;
        colors[ImGuiCol_TabActive]            = theme.surfaceActive;
        colors[ImGuiCol_TabUnfocused]         = colors[ImGuiCol_Tab];
        colors[ImGuiCol_TabUnfocusedActive]   = colors[ImGuiCol_TabActive];

        colors[ImGuiCol_ScrollbarBg]          = AlphaColor(theme.windowBg, 0.35f);
        colors[ImGuiCol_ScrollbarGrab]        = AlphaColor(theme.accent, 0.45f);
        colors[ImGuiCol_ScrollbarGrabHovered] = theme.accentHover;
        colors[ImGuiCol_ScrollbarGrabActive]  = theme.accentActive;

        colors[ImGuiCol_PlotLines]            = theme.accent;
        colors[ImGuiCol_PlotLinesHovered]     = theme.accentSecondary;
        colors[ImGuiCol_PlotHistogram]        = theme.accent;
        colors[ImGuiCol_PlotHistogramHovered] = theme.accentSecondary;
        colors[ImGuiCol_DragDropTarget]       = AlphaColor(theme.accent, 0.9f);
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
        SetupCheatFactoryPreviewTheme();
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

    // Button with icon from symbolfont (Zero1 style)
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
        if (hasCachedInfo && cachedInfo.hasEndpoint)
            snprintf(serverText, sizeof(serverText), "Server: %s (%s:%d)", region, cachedInfo.host, cachedInfo.port);
        else if (hasCachedInfo)
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

    static std::string GetCharacterVariationDisplayName(int index)
    {
        static auto options = GetCharacterVariationOptionsFromCharacterData();
        if (index >= 0 && index < (int)options.size()) {
            std::string name = options[index].displayName;
            if (options[index].variationId > 0) {
                name += " (V" + std::to_string(options[index].variationId) + ")";
            }
            return name;
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
                // Fallback: display text button if icon not available
                std::string fallback = "CH" + std::to_string(option.characterId);
                if (ImAdd::Button(fallback.c_str(), ImVec2(iconSize + 4.0f, iconSize + 4.0f)))
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
    static constexpr DWORD GAME_LIST_CACHE_INTERVAL_MS = 5000;

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

        g_AvailableTeamIds = InGameHack_GetAllTeamIds();
        g_CachedLobbyMyTeamId = InGameHack_GetMyTeamId();
        g_CachedLobbyTeams.clear();
        g_CachedLobbyTeams.reserve(g_AvailableTeamIds.size());

        for (unsigned char teamId : g_AvailableTeamIds)
        {
            TeamListEntry entry;
            entry.teamId = teamId;
            entry.playerNames = InGameHack_GetTeamNamesByTeamId(teamId);
            g_CachedLobbyTeams.push_back(entry);
        }

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
        int techniqueLevel = 1;
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
    }

    static void SeparatorLabel(const char* label)
    {
        ImAdd::SeparatorText(label);
    }

    static bool FullWidthButton(const char* label, float height = 0.0f)
    {
        return ImAdd::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, height));
    }

    static void RenderDllControlCard(float width)
    {
        if (BeginRugirCard("dll-control-page", "DLL CONTROL", ImVec2(width, 0.0f)))
        {
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
        }
        EndRugirCard();
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
                                  " | [PS] " + GetKeyName(hotkey.PS4, 1);
        if (ImAdd::Button(hotkeyLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            g_ListeningForHotkey = true;
            g_HotkeyWaitingForRelease = true;
            g_HotkeyListenStartTime = GetTickCount();
            g_CurrentHotkeyValue = hotkeyId;
        }
    }

    static void DrawCompactButtonRow(const char* first, const char* second = nullptr, const char* third = nullptr)
    {
        const char* labels[3] = { first, second, third };
        int count = 1;
        if (second) count++;
        if (third) count++;

        ImGuiStyle& style = ImGui::GetStyle();
        float width = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * (float)(count - 1)) / (float)count);

        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                ImGui::SameLine();
            ImAdd::Button(labels[i], ImVec2(width, 0.0f));
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

        ImGuiStyle& style = ImGui::GetStyle();
        float width = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * (float)(count - 1)) / (float)count);

        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                ImGui::SameLine();
            if (ImAdd::Button(labels[i], ImVec2(width, 0.0f)) && actions[i])
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

    static void SelectedCharacterSummary(int selectedVariationIndex)
    {
        auto [characterId, variationId] = GetCharacterAndVariationFromIndex(selectedVariationIndex);
        SeparatorLabel("Selected Character");
        ImGui::TextWrapped("%s", GetCharacterVariationDisplayName(selectedVariationIndex).c_str());
        ImGui::TextColored(g_Colors.textDisabled, "Character ID: %d | Variation: %d", characterId, variationId);
    }

    template<typename ApplyFn>
    static void RenderCharacterConfigCard(const char* id, const char* title, const char* actionLabel, CharacterEditorState& state, ApplyFn applyFn, bool actionEnabled = true, const char* disabledLabel = "Select a target first", bool showIconGrid = false)
    {
        if (BeginRugirCard(id, title, ImVec2(0.0f, 0.0f)))
        {
            ImGui::PushID(id);
            SelectedCharacterSummary(state.selectedVariationIndex);

            if (ImAdd::SliderInt("Technique Level", &state.techniqueLevel, 0, 10))
            {
                s_techAlpha = state.techniqueLevel;
                s_techBeta = state.techniqueLevel;
                s_techGamma = state.techniqueLevel;
            }

            RefreshCharacterEditorCostumes(state);
            if (!state.costumeNames.empty())
            {
                std::vector<const char*> costumePtrs;
                costumePtrs.reserve(state.costumeNames.size());
                for (const auto& name : state.costumeNames)
                    costumePtrs.push_back(name.c_str());
                ImAdd::Combo("Costume", &state.selectedCostumeIndex, costumePtrs);
            }
            else
            {
                ImGui::TextColored(g_Colors.textDisabled, "No costume list available");
            }

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
        if (subtab == 0)
        {
            if (BeginRugirCard("esp-control-page", "ESP CONTROL", ImVec2(groupWidth, 0.0f), &g_Settings.EnablePlayerESP))
            {
                if (g_Settings.EnablePlayerESP)
                {
                    ImAdd::CheckBox("Show Enemy", &g_Settings.ShowEnemies);
                    ImAdd::CheckBox("Show Team", &g_Settings.ShowTeam);
                    ImAdd::CheckBox("LobbyCharacter", &g_Settings.ShowLobbyCharacters);
                    ImAdd::CheckBox("KOTA", &g_Settings.ShowKota);
                }
            }
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("display-info-page", "DISPLAY INFO", ImVec2(groupWidth, 0.0f)))
        {
            ImAdd::CheckBox("HP", &g_Settings.ShowHP);
            ImAdd::CheckBox("GP", &g_Settings.ShowGP);
            ImAdd::CheckBox("PU", &g_Settings.ShowPU);
            ImAdd::CheckBox("Platform", &g_Settings.ShowPlatform);
            ImAdd::CheckBox("Team ID", &g_Settings.ShowTeamId);
        }
        EndRugirCard();
        ImGui::EndGroup();

        ImGui::SameLine();

        if (BeginRugirCard("display-render-page", "RENDER", ImVec2(0.0f, 0.0f)))
        {
            ImAdd::CheckBox("Show Skeleton", &g_Settings.ShowPlayerSkeleton);
            ImAdd::SliderFloat("Max Draw Distance", &g_Settings.Player_DrawDistance, 100.0f, 5000.0f, "%.0f");
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

        if (subtab == 1)
        {
            ImGui::BeginGroup();
            if (BeginRugirCard("reload-adjust-rates-page", "RELOAD ADJUST RATES", ImVec2(groupWidth, 0.0f)))
            {
                if (ImAdd::SliderFloat("General Reload Rate", &g_Settings.ReloadAdjustRate, 1.0f, 5.0f, "%.2fx"))
                    InGameHack_SetReloadAdjustRate(g_Settings.ReloadAdjustRate);
                if (ImAdd::SliderFloat("Reload Rate (Roll Slot)", &g_Settings.ReloadAdjustRate_RollSlot, 1.0f, 5.0f, "%.2fx"))
                    InGameHack_SetReloadAdjustRate_RollSlot(g_Settings.ReloadAdjustRate_RollSlot);
                if (ImAdd::SliderFloat("Reload Rate (Blue Flame)", &g_Settings.ReloadAdjustRate_WearBlueFlame, 1.0f, 50.0f, "%.2fx"))
                    InGameHack_SetReloadAdjustRate_WearBlueFlame(g_Settings.ReloadAdjustRate_WearBlueFlame);
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            if (BeginRugirCard("unique-skills-page", "UNIQUE SKILLS", ImVec2(0.0f, 0.0f)))
            {
                static float lastRefreshTime = 0.0f;
                if (ImGui::GetTime() - lastRefreshTime > 0.1f)
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
            }
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("revive-options", "REVIVE OPTIONS", ImVec2(groupWidth, 0.0f)))
        {
            ImAdd::CheckBox("Recover Me (Self)", &g_Settings.EnableRecoveryMe);
            ImAdd::CheckBox("Recover Team", &g_Settings.EnableRecoveryTeam);
            ImAdd::CheckBox("Recover All ESP", &g_Settings.EnableRecoveryAllESP);
            SeparatorLabel("Actions");
            if (FullWidthButton("RECOVER ME NOW"))
                InGameHack_RecoverMe();
            if (FullWidthButton("RECOVER TEAM NOW"))
                InGameHack_RecoverDyingTeam();
        }
        EndRugirCard();
        ImGui::EndGroup();

        ImGui::SameLine();

        if (BeginRugirCard("recovery-team-roster", "RECOVERY TEAM ROSTER", ImVec2(0.0f, 0.0f)))
        {
            RefreshRecoveryTeamCache();

            if (FullWidthButton("REFRESH TEAM ROSTER"))
            {
                RefreshRecoveryTeamCache(true);
                g_Settings.SelectedRecoveryTeamIndex = -1;
            }

            SeparatorLabel("Team members");
            if (g_CachedRecoveryTeamNames.empty())
            {
                ImGui::TextColored(g_Colors.warning, "No team members found.");
            }
            else
            {
                for (int i = 0; i < (int)g_CachedRecoveryTeamNames.size(); i++)
                {
                    if (SelectableRow(g_CachedRecoveryTeamNames[i].c_str(), g_Settings.SelectedRecoveryTeamIndex == i))
                        g_Settings.SelectedRecoveryTeamIndex = i;
                }
                if (FullWidthButton("RECOVER SELECTED MEMBER"))
                    InGameHack_RecoverDyingSpecificTeamMember(g_Settings.SelectedRecoveryTeamIndex);
            }
        }
        EndRugirCard();
    }

    static void RenderPreviewAimbotPage(int subtab, float groupWidth)
    {
        if (subtab == 0)
        {
            ImGui::BeginGroup();
            if (BeginRugirCard("aimbot-settings", "AIMBOT SETTINGS", ImVec2(groupWidth, 0.0f), &g_Settings.EnableAimbot))
            {
                ImAdd::CheckBox("Smooth Aiming", &g_Settings.AimbotSmoothing);
                if (g_Settings.AimbotSmoothing)
                    ImAdd::SliderFloat("Smooth Factor", &g_Settings.AimbotSmoothFactor, 0.01f, 1.0f, "%.2f");
                ImAdd::CheckBox("Draw Aim Line", &g_Settings.AimbotDrawLine);
                ImAdd::CheckBox("Draw Aim FOV", &g_Settings.AimbotDrawFOV);
                if (g_Settings.AimbotDrawFOV)
                    ImAdd::SliderFloat("FOV Radius", &g_Settings.AimbotFOVRadius, 100.0f, 1000.0f, "%.0f");
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            if (BeginRugirCard("aimbot-hotkey", "HOTKEY CONFIGURATION", ImVec2(0.0f, 0.0f)))
                DrawHotkeyConfigButton("Aimbot hold", g_Settings.AimbotHoldKey, 100);
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("bullet-tp-filters", "BULLET TP (SILENT AIM)", ImVec2(groupWidth, 0.0f), &g_Settings.EnableBulletTP))
        {
            SeparatorLabel("SKILL TYPE FILTERS");
            ImAdd::CheckBox("Alpha (Unique1)", &g_Settings.BulletTP_IncludeAlpha);
            ImAdd::CheckBox("Beta (Unique2)", &g_Settings.BulletTP_IncludeBeta);
            ImAdd::CheckBox("Gamma (Unique3)", &g_Settings.BulletTP_IncludeGamma);
            ImAdd::CheckBox("Special", &g_Settings.BulletTP_IncludeSpecial);
        }
        EndRugirCard();
        ImGui::EndGroup();

        ImGui::SameLine();

        if (BeginRugirCard("bullet-tp-config", "BULLET TP", ImVec2(0.0f, 0.0f)))
        {
            ImAdd::SliderFloat("FOV Radius", &g_Settings.BulletTP_FOVRadius, 20.0f, 100.0f, "%.0f");
            ImAdd::SliderFloat("Max Distance", &g_Settings.BulletTP_MaxDistance, 1000.0f, 100000.0f, "%.0f");
            DrawHotkeyConfigButton("Shared hotkey", g_Settings.AimbotHoldKey, 100);
        }
        EndRugirCard();
    }

    static void RenderPreviewCombatPage(int subtab, float groupWidth)
    {
        if (subtab == 0)
        {
            if (BeginRugirCard("invincibility-recovery-page", "INVINCIBILITY & RECOVERY", ImVec2(groupWidth, 0.0f), &g_HackSettings.EnableInvincible))
            {
                DrawHotkeyConfigButton("Invincibility", g_Settings.SetInvincibleKey, 104);
                if (FullWidthButton("ACTIVATE INVINCIBILITY NOW"))
                    InGameHack_SetInvincible();
            }
            EndRugirCard();
            return;
        }

        if (subtab == 1)
        {
            if (BeginRugirCard("character-conditions-page", "CHARACTER CONDITIONS", ImVec2(groupWidth, 0.0f)))
            {
                if (FullWidthButton("OVERHAUL HEAL"))
                    InGameHack_RebuildMyself();
                DrawHotkeyConfigButton("Rebuild Myself", g_Settings.RebuildMyselfKey, 105);
                SeparatorLabel("Transformations");
                ImAdd::CheckBox("DEKU MODE", &g_HackSettings.CharCondition_EnableDekuMode);
                ImAdd::CheckBox("Enable CH202 Trans Level 5", &g_Settings.EnableCH202InitTransLevel5);
                ImAdd::CheckBox("Supply Max Stack 100", &g_Settings.EnableSupplyMaxStackTo100);
                ImAdd::CheckBox("UNBREAKABLE", &g_HackSettings.CharCondition_EnableUnbreakable);
                ImAdd::CheckBox("MR COMPRESSE MODE", &g_HackSettings.CharCondition_EnableCompressionRegen);
                ImAdd::CheckBox("MIRIO MODE", &g_HackSettings.CharCondition_EnableMirioMode);
                ImAdd::CheckBox("TOKOYAMI DARK MODE", &g_HackSettings.CharCondition_EnableTokoyamiMode);
            }
            EndRugirCard();
            return;
        }

        if (subtab == 2)
        {
            ImGui::BeginGroup();
            if (BeginRugirCard("ability-levels", "ABILITY HACKS", ImVec2(groupWidth, 0.0f)))
            {
                ImAdd::SliderInt("Attack Level", &g_HackSettings.AbilityAttackLevel, 1, 10000);
                if (FullWidthButton("ABILITY ATTACK"))
                    InGameHack_AbilityAttack(g_HackSettings.AbilityAttackLevel);
                ImAdd::SliderInt("Durable Level", &g_HackSettings.AbilityDurableLevel, 1, 100);
                if (FullWidthButton("ABILITY DURABLE"))
                    InGameHack_AbilityDurable(g_HackSettings.AbilityDurableLevel);
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            if (BeginRugirCard("ability-extra", "ABILITY EXTRA", ImVec2(0.0f, 0.0f)))
            {
                ImAdd::SliderInt("Movespeed Level", &g_HackSettings.AbilityMovespeedLevel, 1, 100);
                if (FullWidthButton("ABILITY MOVESPEED"))
                    InGameHack_AbilityMovespeed(g_HackSettings.AbilityMovespeedLevel);
                ImAdd::SliderInt("Heal Level", &g_HackSettings.AbilityHealLevel, 1, 100);
                if (FullWidthButton("ABILITY HEAL"))
                    InGameHack_AbilityHeal(g_HackSettings.AbilityHealLevel);
                ImAdd::SliderInt("Technique Level", &g_HackSettings.AbilityTechniqueLevel, 1, 100);
                if (FullWidthButton("ABILITY TECHNIQUE"))
                    InGameHack_AbilityTechnique(g_HackSettings.AbilityTechniqueLevel);
            }
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("projectile-generation", "PROJECTILE GENERATION", ImVec2(groupWidth, 0.0f), &g_Settings.EnableGenerateProjectile))
        {
            DrawHotkeyConfigButton("Generate Projectile", g_Settings.GenerateProjectileKey, 107);
            if (FullWidthButton("GENERATE NOW"))
                InGameHack_GenerateProjectileInFront();
        }
        EndRugirCard();
        ImGui::EndGroup();

        ImGui::SameLine();

        if (BeginRugirCard("supply-management", "SUPPLY MANAGEMENT", ImVec2(0.0f, 0.0f)))
        {
            static bool preventDrop = false;
            static bool stopSupply = false;
            ImAdd::CheckBox("Prevent Drop on Death", &preventDrop);
            if (preventDrop)
                InGameHack_PreventDropOnDeath(true);

            static const int supplyValues[] = { 6, 7 };
            static const char* supplyNames[] = { "ABILITY", "SHOULDER" };
            static int supplyComboIndex = 0;
            static int supplyLevel = 1;
            ImAdd::Combo("Supply Type", &supplyComboIndex, std::vector<const char*>(supplyNames, supplyNames + IM_ARRAYSIZE(supplyNames)));
            ImAdd::SliderInt("Supply Level", &supplyLevel, 1, 99);
            if (FullWidthButton("Upgrade Supply"))
                InGameHack_UpgradeSupply(supplyValues[supplyComboIndex], supplyLevel);
            if (ImAdd::CheckBox("Stop Using Supply", &stopSupply) && stopSupply)
                InGameHack_StopUsingSupply();
        }
        EndRugirCard();
    }

    static void RenderPreviewHacksPage(int subtab, float groupWidth)
    {
        if (subtab == 0)
        {
            ImGui::BeginGroup();
            if (BeginRugirCard("transform-toga-page", "TRANSFORM TOGA", ImVec2(groupWidth, 0.0f), &g_Settings.EnableTransformIntoRandomESP))
            {
                DrawHotkeyConfigButton("Transform", g_Settings.TransformIntoRandomESPKey, 102);
                ImGui::TextWrapped("Transform to a random ESP target.");
            }
            EndRugirCard();
            ImGui::EndGroup();

            ImGui::SameLine();

            if (BeginRugirCard("duplicate-imitation-page", "DUPLICATE IMITATION", ImVec2(0.0f, 0.0f), &g_Settings.EnableDuplicateIntoImitationRandomESP))
            {
                ImAdd::SliderFloat("Imitation Lifetime", &g_Settings.DuplicateImitationLifeTime, 5.0f, 120.0f, "%.0fs");
                ImAdd::SliderInt("Imitation Duplicate Count", &g_Settings.DuplicateIntoImitationCount, 1, 100);
                DrawHotkeyConfigButton("Imitation hotkey", g_Settings.DuplicateIntoImitationRandomESPKey, 103);
            }
            EndRugirCard();
            return;
        }

        if (subtab == 1)
        {
            if (BeginRugirCard("copy-skills-page", "COPY SKILLS", ImVec2(groupWidth, 0.0f), &g_Settings.EnableCopySkillsFromNearestEnemy))
            {
                ImAdd::CheckBox("Set Copy Skill", &g_Settings.CopySkillsSetCopySkill);
                ImAdd::CheckBox("Use Owner Character Level", &g_Settings.CopySkillsUseOwnerCharacterLevel);
                DrawHotkeyConfigButton("Copy Skills", g_Settings.CopySkillsFromNearestEnemyKey, 106);
            }
            EndRugirCard();
            return;
        }

        ImGui::BeginGroup();
        if (BeginRugirCard("teleport-kota-page", "TELEPORT TO KOTA", ImVec2(groupWidth, 0.0f), &g_Settings.EnableTeleportToKota))
        {
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

            SeparatorLabel("Boxes / Cards");
            DrawActionButtonRow("Box S", SDK_TeleportItem_BoxSmall, "Box L", SDK_TeleportItem_BoxLarge, "Box Gold", SDK_TeleportItem_BoxGold);
            DrawActionButtonRow("LvlUp", SDK_TeleportItem_LevelUpCard, "Enhance", SDK_TeleportItem_TeamEnhancementKit);
            DrawActionButtonRow("Card A", SDK_TeleportItem_AbilitySupplyAlpha, "Card B", SDK_TeleportItem_AbilitySupplyBeta, "Card Y", SDK_TeleportItem_AbilitySupplyGamma);
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
            static float attack = 1.0f;
            static float durable = 1.0f;
            static float speed = 1.0f;
            static float healing = 1.0f;
            static float reload = 1.0f;
            if (BeginRugirCard("lobby-team-buffs-page", "APPLY TEAM BUFFS", ImVec2(groupWidth, 0.0f)))
            {
                ImAdd::SliderFloat("Attack Adjust", &attack, 0.5f, 3.0f, "%.2fx");
                ImAdd::SliderFloat("Durability Adjust", &durable, 0.5f, 3.0f, "%.2fx");
                ImAdd::SliderFloat("Speed Adjust", &speed, 0.5f, 3.0f, "%.2fx");
                ImAdd::SliderFloat("Healing Adjust", &healing, 0.5f, 3.0f, "%.2fx");
                ImAdd::SliderFloat("Reload Adjust", &reload, 0.5f, 3.0f, "%.2fx");
                if (FullWidthButton("APPLY TEAM BUFFS"))
                    InGameHack_ApplyTeamBuffs(attack, durable, speed, healing, reload);
            }
            EndRugirCard();
            return;
        }

        if (subtab == 4)
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

        if (BeginRugirCard("lobby-license-page", "BUY LICENSE EXP", ImVec2(groupWidth, 0.0f)))
        {
            ImAdd::SliderInt("License Exp Amount", &g_HackSettings.BuyLicenseExpCount, 1, 10000, "%d");
            if (FullWidthButton("BUY LICENSE EXP"))
                InGameHack_BuyLicenseExp(g_HackSettings.BuyLicenseExpCount);
        }
        EndRugirCard();
    }

    static void RenderPreviewSettingsPage(float groupWidth)
    {
        RenderDllControlCard(0.0f);

        if (BeginRugirCard("profiles-page", "PROFILE MANAGEMENT", ImVec2(0.0f, 0.0f)))
        {
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

                if (ImAdd::Combo("Profile", &selectedProfile, profileItems))
                {
                    const std::string& profile = g_ProfilesList[selectedProfile];
                    strcpy_s(g_ProfileNameBuffer, sizeof(g_ProfileNameBuffer), profile.c_str());
                    g_CurrentProfileName = profile;
                }
            }
        }
        EndRugirCard();
    }

    // ============================================================================
    // CHEAT FACTORY PREVIEW MENU
    // ============================================================================
    static void DrawCheatFactoryStyleMenu()
    {
        struct Subtab
        {
            const char* label;
            ID3D11ShaderResourceView* icon;
        };

        struct Page
        {
            const char* label;
            ID3D11ShaderResourceView* icon;
            const Subtab* subtabs;
            int subtabCount;
        };

        static int selectedSubtabs[7] = {};

        const Subtab espSubtabs[] =
        {
            { "Control", g_IconVerified },
            { "Display", g_IconObjects },
        };

        const Subtab characterSubtabs[] =
        {
            { "Owner Swap", g_IconCursor },
            { "Reload/Skills", g_IconClock },
            { "Revive", g_IconPulse },
        };

        const Subtab aimbotSubtabs[] =
        {
            { "Aimbot", g_IconTarget },
            { "Bullet TP", g_IconClick },
        };

        const Subtab combatSubtabs[] =
        {
            { "Recovery", g_IconPulse },
            { "Conditions", g_IconEvil },
            { "Abilities", g_IconVerified },
            { "Supply", g_IconObjects },
        };

        const Subtab hacksSubtabs[] =
        {
            { "Toga/Imitation", g_IconCrime },
            { "Copy Skills", g_IconWrench },
            { "Kota/Items", g_IconLocation },
        };

        const Subtab lobbySubtabs[] =
        {
            { "Apply Team", g_IconGlobe },
            { "Apply All", g_IconObjects },
            { "Specific", g_IconCursor },
            { "Team Buffs", g_IconPulse },
            { "Change Team", g_IconLocation },
            { "License EXP", g_IconVerified },
        };

        const Subtab settingsSubtabs[] =
        {
            { "Profiles", g_IconSave },
        };

        const Page pages[] =
        {
            { "ESP", g_IconEye, espSubtabs, IM_ARRAYSIZE(espSubtabs) },
            { "Character", g_IconRunning, characterSubtabs, IM_ARRAYSIZE(characterSubtabs) },
            { "Aimbot", g_IconTarget, aimbotSubtabs, IM_ARRAYSIZE(aimbotSubtabs) },
            { "Combat", g_IconKnife, combatSubtabs, IM_ARRAYSIZE(combatSubtabs) },
            { "Hacks", g_IconMisc, hacksSubtabs, IM_ARRAYSIZE(hacksSubtabs) },
            { "Lobby", g_IconGlobe, lobbySubtabs, IM_ARRAYSIZE(lobbySubtabs) },
            { "Settings", g_IconSettings, settingsSubtabs, IM_ARRAYSIZE(settingsSubtabs) },
        };

        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();
        const int pageCount = IM_ARRAYSIZE(pages);
        if (g_SelectedTab < 0 || g_SelectedTab >= pageCount)
            g_SelectedTab = 0;

        ImGui::SetNextWindowSize(ImVec2(760, 554), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        bool open = ImGui::Begin("CHEAT FACTORY", &g_Visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(2);

        if (!open)
        {
            ImGui::End();
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowMin = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowMax(windowMin.x + windowSize.x, windowMin.y + windowSize.y);
        float headerPadding = style.WindowPadding.y;
        ImFont* headerFont = (io.Fonts->Fonts.Size > 1) ? io.Fonts->Fonts[1] : ImGui::GetFont();
        float headerFontSize = headerFont ? headerFont->LegacySize : ImGui::GetFontSize();
        float headerHeight = headerFontSize + headerPadding * 2.0f;
        static float sidebarWidth = 180.0f;

        ImVec2 headerMin = windowMin;
        ImVec2 headerMax(windowMax.x, windowMin.y + headerHeight);
        ImVec2 headerSize(headerMax.x - headerMin.x, headerMax.y - headerMin.y);
        ImVec2 contentMin(windowMin.x, headerMax.y);
        ImVec2 contentMax = windowMax;
        ImVec2 contentSize(contentMax.x - contentMin.x, contentMax.y - contentMin.y);

        drawList->AddRectFilled(headerMin, headerMax, ImGui::GetColorU32(ImGuiCol_TitleBg), style.WindowRounding, ImDrawFlags_RoundCornersTop);
        drawList->AddRectFilled(contentMin, contentMax, ImGui::GetColorU32(ImGuiCol_WindowBg), style.WindowRounding, ImDrawFlags_RoundCornersBottom);
        if (style.WindowBorderSize > 0.0f)
            drawList->AddLine(ImVec2(headerMin.x, headerMax.y - style.WindowBorderSize), ImVec2(headerMax.x, headerMax.y - style.WindowBorderSize), ImGui::GetColorU32(ImGuiCol_Border), style.WindowBorderSize);

        ImGui::SetCursorScreenPos(headerMin);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(headerPadding, headerPadding));
        if (ImGui::BeginChild("header", headerSize, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding))
        {
            if (headerFont)
                ImGui::PushFont(headerFont);
            ImGui::TextUnformatted("CHEAT FACTORY");
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFontSize() - style.WindowPadding.x);
            if (ImAdd::ButtonXMark("close-button", ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize())))
                g_Visible = false;
            if (headerFont)
                ImGui::PopFont();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SetCursorScreenPos(contentMin);
        if (ImGui::BeginChild("content", contentSize, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar))
        {
            ImVec2 contentWindowPos = ImGui::GetWindowPos();
            ImGui::SetCursorScreenPos(ImVec2(contentWindowPos.x + style.WindowPadding.x, contentWindowPos.y + style.WindowPadding.y));
            if (ImGui::BeginChild("menubar", ImVec2(ImGui::GetWindowWidth() - style.WindowPadding.x * 2.0f, ImGui::GetFrameHeight()), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, style.ChildBorderSize);
                for (int i = 0; i < pageCount - 1; i++)
                {
                    ImAdd::TabIcon(pages[i].icon, pages[i].label, &g_SelectedTab, i, true);
                    if (i < pageCount - 2)
                        ImGui::SameLine();
                }
                ImGui::PopStyleVar();

                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFrameHeight());
                ImAdd::TabIcon(g_IconSettings, "##Settings", &g_SelectedTab, pageCount - 1);
            }
            ImGui::EndChild();

            const Page& page = pages[g_SelectedTab];
            if (selectedSubtabs[g_SelectedTab] >= page.subtabCount)
                selectedSubtabs[g_SelectedTab] = 0;

            const bool hasSubtabs = page.subtabCount > 0;
            if (hasSubtabs)
            {
                ImVec2 sidebarPos = ImGui::GetWindowPos();
                ImGui::SetCursorScreenPos(ImVec2(sidebarPos.x, sidebarPos.y + ImGui::GetFrameHeight() + style.WindowPadding.y));
                if (ImAdd::BeginChild("sidebar", ImVec2(sidebarWidth, ImGui::GetWindowHeight() - ImGui::GetFrameHeight() - style.WindowPadding.y), false))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.FrameRounding));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, style.ChildBorderSize);

                    for (int i = 0; i < page.subtabCount; i++)
                    {
                        const Subtab& subtab = page.subtabs[i];
                        ImAdd::TabIcon(subtab.icon, subtab.label, &selectedSubtabs[g_SelectedTab], i, false, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
                    }

                    ImGui::PopStyleVar(2);
                }
                ImAdd::EndChild();
            }

            ImVec2 pagePos = ImGui::GetWindowPos();
            ImGui::SetCursorScreenPos(ImVec2(pagePos.x + (hasSubtabs ? (sidebarWidth - style.WindowPadding.x) : 0.0f), pagePos.y + ImGui::GetFrameHeight() + style.WindowPadding.y));
            std::string pageId = "page:" + std::to_string(g_SelectedTab) + ":" + std::to_string(selectedSubtabs[g_SelectedTab]);
            if (ImAdd::BeginChild(pageId.c_str(), ImVec2(hasSubtabs ? (ImGui::GetWindowWidth() - sidebarWidth + style.WindowPadding.x) : ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - ImGui::GetFrameHeight() - style.WindowPadding.y), false))
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
            ImAdd::EndChild();
        }
        ImGui::EndChild();

        ImGui::End();
    }
    // ============================================================================
    // INITIALIZATION & SHUTDOWN
    // ============================================================================
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

        ImFontConfig fontConfig;
        fontConfig.SizePixels = 14.0f;
        fontConfig.GlyphOffset = ImVec2(0.0f, -1.0f);
        io.Fonts->AddFontFromMemoryCompressedTTF(
            font_inter_semibold_compressed_data,
            font_inter_semibold_compressed_size,
            fontConfig.SizePixels,
            &fontConfig,
            io.Fonts->GetGlyphRangesDefault());

        ImFontConfig headerFontConfig;
        headerFontConfig.SizePixels = 16.0f;
        headerFontConfig.GlyphOffset = ImVec2(0.0f, -1.0f);
        io.Fonts->AddFontFromMemoryCompressedTTF(
            font_inter_semibold_compressed_data,
            font_inter_semibold_compressed_size,
            headerFontConfig.SizePixels,
            &headerFontConfig,
            io.Fonts->GetGlyphRangesDefault());

        ImFontConfig codeFontConfig;
        codeFontConfig.SizePixels = 14.0f;
        io.Fonts->AddFontFromMemoryCompressedTTF(
            font_cascadia_mono_pl_regular_compressed_data,
            font_cascadia_mono_pl_regular_compressed_size,
            codeFontConfig.SizePixels,
            &codeFontConfig,
            io.Fonts->GetGlyphRangesDefault());

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
            SetupCheatFactoryPreviewTheme();
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

        // Cheat Factory preview layout has no visible master switch. Keep the legacy
        // runtime gate enabled so preview-visible feature toggles are effective.
        g_Settings.EnableGlobal = true;

        // Initialize character icon loader
        ID3D11Device* iconDevice = D3D11Hook::GetDevice();
        ID3D11DeviceContext* iconContext = D3D11Hook::GetContext();
        if (iconDevice && iconContext)
        {
            IconLoader::CharacterIconCache::Initialize(iconDevice, iconContext);
            Logger::LogInfo("[Menu] Character icon loader initialized");
            
            // Load RUGIR logo
            LoadRugirLogo();
            LoadPlatformIcons();
            LoadPreviewMenuIcons(iconDevice);
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
        ReleasePreviewMenuIcons();
        g_CharacterIconGridCache.clear();
        IconLoader::CharacterIconCache::Shutdown();

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
    // HOTKEY LISTENER UPDATE - Called every frame (Zero1 exact)
    // ============================================================================
    static void UpdateHotkeyListener()
    {
        ImGuiIO& io = ImGui::GetIO();
        
        // CRITICAL: Only enable gamepad navigation when actively listening for hotkey
        // Otherwise, game needs to receive gamepad inputs
        if (g_ListeningForHotkey)
        {
            // Enable gamepad so we can detect hotkey input
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
            
            // Check for 5-second timeout (5000 ms)
            int elapsedTime = GetTickCount() - g_HotkeyListenStartTime;
            if (elapsedTime > 5000)
            {
                g_ListeningForHotkey = false;
                g_HotkeyWaitingForRelease = false;
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable after timeout
                return;  // Timeout - cancel listening
            }
            
            // Check for ESC key (27) - cancel
            if ((GetAsyncKeyState(0x1B) & 0x8000) != 0)
            {
                g_ListeningForHotkey = false;
                g_HotkeyWaitingForRelease = false;
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable on cancel
                return;
            }
            
            // Determine which hotkey we're listening for (unified system: 100=Aimbot, 101=Teleport, 102=Transform, 103=DuplicateImitation)
            int hotkeyType = g_CurrentHotkeyValue;
            
            // Try both keyboard and gamepad inputs
            int keyboardKey = GetHotkey(0, 0);  // Check keyboard
            int gamepadKey = GetHotkey(0, 1);   // Check gamepad

            if (g_HotkeyWaitingForRelease)
            {
                if (keyboardKey == 0 && gamepadKey == 0)
                    g_HotkeyWaitingForRelease = false;

                return;
            }
            
            int pressedKey = keyboardKey != 0 ? keyboardKey : gamepadKey;
            int inputType = keyboardKey != 0 ? 0 : 1;  // 0=keyboard, 1=gamepad
            
            if (pressedKey != 0)
            {
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
                
                g_ListeningForHotkey = false;  // Stop listening
                g_HotkeyWaitingForRelease = false;
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable after hotkey set
            }
        }
        else
        {
            // Not listening - make sure gamepad is disabled so game gets inputs
            g_HotkeyWaitingForRelease = false;
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }
    }

    static bool HasPendingCharacterConditionAutoExecution()
    {
        return g_Settings.EnableGlobal &&
            (g_HackSettings.CharCondition_EnableDekuMode ||
             g_HackSettings.CharCondition_EnableUnbreakable ||
             g_HackSettings.CharCondition_EnableCompressionRegen ||
             g_HackSettings.CharCondition_EnableMirioMode ||
             g_HackSettings.CharCondition_EnableTokoyamiMode);
    }

    static bool HasSdkOverlayDraw()
    {
        if (!g_Settings.EnableGlobal)
            return false;

        return g_Settings.EnablePlayerESP ||
            g_Settings.ShowPlayerSkeleton ||
            (g_Settings.EnableAimbot && (g_Settings.AimbotDrawLine || g_Settings.AimbotDrawFOV)) ||
            g_Settings.EnableBulletTP;
    }

    static bool NeedsImGuiFrame()
    {
        return SHOW_SERVER_STATUS_OVERLAY ||
            g_Visible ||
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

        // Update hotkey listener state every frame (Zero1 exact)
        UpdateHotkeyListener();

        if (HasPendingCharacterConditionAutoExecution())
        {
            // Process character condition auto-execution on battle mode entry
            InGameHack_ProcessCharacterConditionAutoExecution(
                g_HackSettings.CharCondition_EnableDekuMode,
                g_HackSettings.CharCondition_EnableUnbreakable,
                g_HackSettings.CharCondition_EnableCompressionRegen,
                g_HackSettings.CharCondition_EnableMirioMode,
                g_HackSettings.CharCondition_EnableTokoyamiMode
            );
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

        ReleaseRenderTargetView();
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

        if (SHOW_SERVER_STATUS_OVERLAY)
            DrawServerStatusOverlay();
        DrawPlayerListHotkeyHint();
        
        // CRITICAL: Don't let ImGui capture gamepad inputs once hotkey is defined
        // Only process gamepad when actively listening for hotkey definition
        if (!g_ListeningForHotkey)
        {
            // Disable ImGui's gamepad navigation so game receives all gamepad inputs
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }
        else
        {
            // Enable gamepad navigation only while defining hotkey
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        // ========================================================================
        // STEP 3B: SDK RENDERING (on RenderThread with ImGui context)
        // ========================================================================
        if (g_Settings.EnableGlobal)
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
            if (g_Settings.EnableBulletTP)
                SDK_DrawBulletRedirectionFOV();
        }

        // ========================================================================
        // STEP 4: DRAW MAIN MENU WINDOW (cheat-factory structure)
        // ========================================================================
        if (g_Visible)
        {
            DrawCheatFactoryStyleMenu();
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

        ReleaseRenderTargetView();
    }

    bool IsInitialized() { return g_Initialized; }
    bool IsVisible() { return g_Visible; }
    void SetVisible(bool visible) { g_Visible = visible; }
}
