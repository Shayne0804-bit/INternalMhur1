#include "ImGuiMenu.h"
#include "../Utils/Logger.h"
#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Hacks/InGameModuleHacks.h"
#include "../SDK/SDKInit.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <windowsx.h>
#include <Xinput.h>
#include <string>
#include <fstream>
#pragma comment(lib, "xinput.lib")

// Forward declare ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare SDK canvas resolution function
extern "C" void SDK_SetImGuiCanvasMaxResolution();

// Forward declare unload function
extern "C" __declspec(dllimport) void DLL_Unload();

// Forward declare SDK drawing functions
extern "C" void SDK_DrawAllRectangles();
extern "C" void SDK_DrawAllTextLabels();
extern "C" void SDK_DrawAllSkeletons();
extern "C" void SDK_DrawAimbotInfo();
extern "C" void SDK_DrawAimbotFOV();

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

    // ============================================================================
    // GLOBAL MENU STATE
    // ============================================================================
    static int g_SelectedTab = 0;  // 0=ESP, 1=AIMBOT, 2=COMBAT, 3=HACKS, 4=VISUAL, 5=SETTINGS
    
    static bool g_Initialized = false;
    static bool g_Visible = true;
    static ImGuiContext* g_Context = nullptr;
    static HWND g_GameWindow = nullptr;
    static WNDPROC g_OriginalWndProc = nullptr;
    static ID3D11Device* g_Device = nullptr;
    static ID3D11DeviceContext* g_DeviceContext = nullptr;

    // Section open/close states
    static bool g_ESP_DisplayOpen = true;
    static bool g_ESP_FiltersOpen = true;
    
    // Hotkey listening state (Zero1 exact)
    static bool g_ListeningForHotkey = false;
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
        ImVec4 bgDarkest = ImVec4(0.04f, 0.03f, 0.06f, 1.0f);
        ImVec4 bgDark = ImVec4(0.08f, 0.06f, 0.12f, 0.97f);
        ImVec4 bgMedium = ImVec4(0.12f, 0.10f, 0.18f, 1.0f);
        ImVec4 bgLight = ImVec4(0.2f, 0.16f, 0.26f, 1.0f);

        // Accent colors - Mauve/Magenta
        ImVec4 accentColor = ImVec4(0.65f, 0.35f, 0.95f, 1.0f);
        ImVec4 accentColorHover = ImVec4(0.75f, 0.45f, 1.0f, 1.0f);
        ImVec4 accentColorActive = ImVec4(0.55f, 0.25f, 0.85f, 1.0f);

        // Status colors
        ImVec4 success = ImVec4(0.45f, 0.95f, 0.55f, 1.0f);
        ImVec4 warning = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
        ImVec4 danger = ImVec4(0.95f, 0.35f, 0.45f, 1.0f);

        // Text colors
        ImVec4 textPrimary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 textSecondary = ImVec4(0.75f, 0.70f, 0.80f, 1.0f);
        ImVec4 textDisabled = ImVec4(0.5f, 0.5f, 0.52f, 1.0f);

        // Legacy accents (for compatibility)
        ImVec4 primaryBlue = accentColor;
        ImVec4 accentCyan = accentColor;
        ImVec4 accentMagenta = accentColor;
        ImVec4 accentYellow = warning;
        ImVec4 accentOrange = ImVec4(1.0f, 0.5f, 0.2f, 1.0f);
        ImVec4 accentPurple = accentColor;
        ImVec4 accentGreen = success;
        ImVec4 accentAqua = accentColorHover;
    };
    static ColorPalette g_Colors;

    // ============================================================================
    // WINDOW PROCEDURE - INPUT HANDLING
    // ============================================================================
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Toggle menu with INSERT key
        if (msg == WM_KEYDOWN && wParam == VK_INSERT)
        {
            g_Visible = !g_Visible;
            return true;
        }

        // Handle ImGui input
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGuiIO& io = ImGui::GetIO();

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
                if (g_Visible && io.WantCaptureKeyboard)
                {
                    return true;
                }
                break;
            }

            if (g_Visible && (io.WantCaptureMouse || io.WantCaptureKeyboard))
            {
                return true;
            }
        }

        return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);
    }

    // ============================================================================
    // STYLING & COLOR SETUP - MODERNE MAUVE THEME
    // ============================================================================
    static void SetupColorScheme()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4* colors = style->Colors;

        // Windows & Backgrounds
        colors[ImGuiCol_WindowBg] = g_Colors.bgDark;
        colors[ImGuiCol_ChildBg] = g_Colors.bgDarkest;
        colors[ImGuiCol_PopupBg] = g_Colors.bgDark;

        // Borders & Separators - Mauve Accent
        colors[ImGuiCol_Border] = g_Colors.accentColor;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_Separator] = g_Colors.accentColor;

        // Title Bar
        colors[ImGuiCol_TitleBg] = g_Colors.bgMedium;
        colors[ImGuiCol_TitleBgActive] = g_Colors.accentColor;
        colors[ImGuiCol_TitleBgCollapsed] = g_Colors.bgLight;

        // Menus
        colors[ImGuiCol_MenuBarBg] = g_Colors.bgMedium;
        colors[ImGuiCol_ScrollbarBg] = g_Colors.bgDarkest;
        colors[ImGuiCol_ScrollbarGrab] = g_Colors.accentColor;
        colors[ImGuiCol_ScrollbarGrabHovered] = g_Colors.accentColorHover;
        colors[ImGuiCol_ScrollbarGrabActive] = g_Colors.accentColorActive;

        // Tabs
        colors[ImGuiCol_Tab] = g_Colors.bgLight;
        colors[ImGuiCol_TabHovered] = g_Colors.accentColorHover;
        colors[ImGuiCol_TabActive] = g_Colors.accentColor;
        colors[ImGuiCol_TabUnfocused] = g_Colors.bgMedium;
        colors[ImGuiCol_TabUnfocusedActive] = g_Colors.accentColor;

        // Headers
        colors[ImGuiCol_Header] = g_Colors.bgLight;
        colors[ImGuiCol_HeaderHovered] = g_Colors.accentColorHover;
        colors[ImGuiCol_HeaderActive] = g_Colors.accentColor;

        // Buttons
        colors[ImGuiCol_Button] = g_Colors.bgLight;
        colors[ImGuiCol_ButtonHovered] = g_Colors.accentColorHover;
        colors[ImGuiCol_ButtonActive] = g_Colors.accentColorActive;

        // Frames
        colors[ImGuiCol_FrameBg] = g_Colors.bgMedium;
        colors[ImGuiCol_FrameBgHovered] = g_Colors.bgLight;
        colors[ImGuiCol_FrameBgActive] = g_Colors.accentColor;

        // Checkmark
        colors[ImGuiCol_CheckMark] = g_Colors.accentColor;

        // Sliders
        colors[ImGuiCol_SliderGrab] = g_Colors.accentColor;
        colors[ImGuiCol_SliderGrabActive] = g_Colors.accentColorActive;

        // Text
        colors[ImGuiCol_Text] = g_Colors.textPrimary;
        colors[ImGuiCol_TextDisabled] = g_Colors.textDisabled;
        colors[ImGuiCol_TextSelectedBg] = g_Colors.accentColorActive;

        // Plot
        colors[ImGuiCol_PlotLines] = g_Colors.accentColor;
        colors[ImGuiCol_PlotLinesHovered] = g_Colors.accentColorHover;
        colors[ImGuiCol_PlotHistogram] = g_Colors.accentColor;
        colors[ImGuiCol_PlotHistogramHovered] = g_Colors.accentColorHover;

        // Rounding & Spacing - Modern Style
        style->WindowRounding = 10.0f;
        style->FrameRounding = 6.0f;
        style->PopupRounding = 6.0f;
        style->ScrollbarRounding = 6.0f;
        style->GrabRounding = 4.0f;
        style->TabRounding = 6.0f;
        style->ChildRounding = 6.0f;

        style->WindowPadding = ImVec2(15.0f, 15.0f);
        style->FramePadding = ImVec2(10.0f, 6.0f);
        style->ItemSpacing = ImVec2(10.0f, 8.0f);
        style->ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style->IndentSpacing = 20.0f;
        style->ScrollbarSize = 12.0f;
        style->GrabMinSize = 8.0f;
        style->WindowBorderSize = 1.0f;
        style->FrameBorderSize = 0.0f;
        style->PopupBorderSize = 1.0f;
    }

    // ============================================================================
    // HELPER FUNCTIONS - MODERN UI COMPONENTS
    // ============================================================================
    static bool ModernButton(const char* label, ImVec2 size, bool selected = false)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? g_Colors.accentColor : g_Colors.bgLight);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? g_Colors.accentColorHover : g_Colors.accentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Colors.accentColorActive);
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.textPrimary);
        bool result = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return result;
    }

    static void SectionHeader(const char* title)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentColor);
        ImGui::Text(title);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
    }

    static void StatusIndicator(bool enabled)
    {
        ImVec4 color = (enabled ? g_Colors.success : g_Colors.danger);
        const char* status = (enabled ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("[%s]", status);
        ImGui::PopStyleColor();
    }

    // ============================================================================
    // RENDER FUNCTIONS - TAB CONTENT
    // ============================================================================
    static void RenderESPMenu()
    {
        // Global ESP Control Section
        if (ImGui::CollapsingHeader("ESP-CONTROL", &g_ESP_DisplayOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
            ImGui::Text("ESP CONTROL");
            ImGui::PopStyleColor();
            ImGui::Separator();

            ImGui::Checkbox("Global ESP Enable", &g_Settings.EnablePlayerESP);
            ImGui::Checkbox("Show Enemy", &g_Settings.ShowEnemies);
            ImGui::Checkbox("Show Team", &g_Settings.ShowTeam);
            ImGui::Checkbox("LobbyCharacter", &g_Settings.ShowLobbyCharacters);
            ImGui::Checkbox("KOTA", &g_Settings.ShowKota);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
            ImGui::Text("DISPLAY INFO");
            ImGui::PopStyleColor();
            
            ImGui::Checkbox("HP", &g_Settings.ShowHP);
            ImGui::SameLine();
            ImGui::Checkbox("GP", &g_Settings.ShowGP);
            ImGui::SameLine();
            ImGui::Checkbox("PU", &g_Settings.ShowPU);
            ImGui::SameLine();
            ImGui::Checkbox("Platform", &g_Settings.ShowPlatform);
            ImGui::SameLine();
            ImGui::Checkbox("Team ID", &g_Settings.ShowTeamId);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Checkbox("Show Skeleton", &g_Settings.ShowPlayerSkeleton);
            ImGui::SliderFloat("Max Draw Distance", &g_Settings.Player_DrawDistance, 100.0f, 5000.0f, "%.0f m");

            ImGui::Spacing();
        }
    }

    static void RenderAimbotMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("AIMBOT SETTINGS");
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Checkbox("Enable Aimbot", &g_Settings.EnableAimbot);
        ImGui::Checkbox("Smooth Aiming", &g_Settings.AimbotSmoothing);
        
        if (g_Settings.AimbotSmoothing)
        {
            ImGui::SliderFloat("Smooth Factor", &g_Settings.AimbotSmoothFactor, 0.01f, 1.0f, "%.2f");
        }
        
        ImGui::Checkbox("Draw Aim Line", &g_Settings.AimbotDrawLine);
        ImGui::Checkbox("Draw Aim FOV", &g_Settings.AimbotDrawFOV);
        
        if (g_Settings.AimbotDrawFOV)
        {
            ImGui::SliderFloat("FOV Radius (px)", &g_Settings.AimbotFOVRadius, 100.0f, 1000.0f, "%.0f");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
        ImGui::Text("HOTKEY CONFIGURATION");
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
        {
            std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.AimbotHoldKey.Keyboard, 0) + 
                                      " | [X] " + GetKeyName(g_Settings.AimbotHoldKey.Xbox, 1) + 
                                      " | [PS] " + GetKeyName(g_Settings.AimbotHoldKey.PS4, 1);
            if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
            {
                g_ListeningForHotkey = true;
                g_HotkeyListenStartTime = GetTickCount();
                g_CurrentHotkeyValue = 100;  // Aimbot hotkey (unified)
            }
        }
        ImGui::PopStyleColor();
        
        // ====================================================================
        // BULLET TP SECTION - ALPHA SKILLS ONLY
        // ====================================================================
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
        ImGui::Text("BULLET TP (ALPHA SKILLS ONLY)");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Checkbox("Enable BulletTP", &g_Settings.EnableBulletTP);
        ImGui::TextWrapped("Note: Uses same settings as Aimbot (FOV, Hotkeys)");
    }

    static void RenderHacksMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("HACKS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Transform Into Random ESP Target Section
        if (ImGui::CollapsingHeader("TRANSFORM TOGA", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Enable/Disable toggle
            ImGui::Checkbox("Enable Transform", &g_Settings.EnableTransformIntoRandomESP);
            
            if (g_Settings.EnableTransformIntoRandomESP)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
                ImGui::Text("Status: ENABLED");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
                ImGui::Text("Status: DISABLED");
                ImGui::PopStyleColor();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
            ImGui::Text("HOTKEY CONFIGURATION");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
            {
                std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.TransformIntoRandomESPKey.Keyboard, 0) + 
                                          " | [X] " + GetKeyName(g_Settings.TransformIntoRandomESPKey.Xbox, 1) + 
                                          " | [PS] " + GetKeyName(g_Settings.TransformIntoRandomESPKey.PS4, 1);
                if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
                {
                    g_ListeningForHotkey = true;
                    g_HotkeyListenStartTime = GetTickCount();
                    g_CurrentHotkeyValue = 102;  // Transform hotkey (unified)
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextWrapped("Info: Press hotkey in-game to transform to a random ESP target (works with menu open)");
        }

        // Duplicate Into Imitation Random ESP Target Section
        if (ImGui::CollapsingHeader("DUPLICATE IMITATION", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentYellow);
            ImGui::Text("DUPLICATE IMITATION VARIANT");
            ImGui::PopStyleColor();
            
            ImGui::Checkbox("Enable DuplicateIntoImitation", &g_Settings.EnableDuplicateIntoImitationRandomESP);
            ImGui::SliderFloat("Imitation Lifetime##DupIm", &g_Settings.DuplicateImitationLifeTime, 5.0f, 120.0f, "%.1f");
            ImGui::SliderInt("Imitation Duplicate Count##DupIm", &g_Settings.DuplicateIntoImitationCount, 1, 100, "%d");
            
            if (g_Settings.EnableDuplicateIntoImitationRandomESP)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
                ImGui::Text("DuplicateIntoImitation: ENABLED");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
                ImGui::Text("DuplicateIntoImitation: DISABLED");
                ImGui::PopStyleColor();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
            ImGui::Text("RELOAD ADJUST RATES");
            ImGui::PopStyleColor();
            
            ImGui::SliderFloat("General Reload Rate##reload", &g_Settings.ReloadAdjustRate, 0.1f, 5.0f, "%.2f");
            ImGui::SliderFloat("Reload Rate (Roll Slot)##rollslot", &g_Settings.ReloadAdjustRate_RollSlot, 0.1f, 5.0f, "%.2f");
            ImGui::SliderFloat("Reload Rate (Blue Flame)##blueflame", &g_Settings.ReloadAdjustRate_WearBlueFlame, 0.1f, 50.0f, "%.2f");
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
            ImGui::Text("IMITATION HOTKEYS");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
            {
                std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.DuplicateIntoImitationRandomESPKey.Keyboard, 0) + 
                                          " | [X] " + GetKeyName(g_Settings.DuplicateIntoImitationRandomESPKey.Xbox, 1) + 
                                          " | [PS] " + GetKeyName(g_Settings.DuplicateIntoImitationRandomESPKey.PS4, 1);
                if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
                {
                    g_ListeningForHotkey = true;
                    g_HotkeyListenStartTime = GetTickCount();
                    g_CurrentHotkeyValue = 103;  // DuplicateImitation hotkey (unified)
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::TextWrapped("Info: Press hotkey to spawn temporary duplicate near target.");
        }

        // Teleport to KOTA Section
        if (ImGui::CollapsingHeader("TELEPORT TO KOTA", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Enable/Disable toggle
            ImGui::Checkbox("Enable Teleport to KOTA", &g_Settings.EnableTeleportToKota);
            
            if (g_Settings.EnableTeleportToKota)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
                ImGui::Text("Status: ENABLED");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
                ImGui::Text("Status: DISABLED");
                ImGui::PopStyleColor();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
            ImGui::Text("HOTKEY CONFIGURATION");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
            {
                std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.TeleportToKotaKey.Keyboard, 0) + 
                                          " | [X] " + GetKeyName(g_Settings.TeleportToKotaKey.Xbox, 1) + 
                                          " | [PS] " + GetKeyName(g_Settings.TeleportToKotaKey.PS4, 1);
                if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
                {
                    g_ListeningForHotkey = true;
                    g_HotkeyListenStartTime = GetTickCount();
                    g_CurrentHotkeyValue = 101;  // Teleport hotkey (unified)
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextWrapped("Info: Press hotkey in-game to teleport to KOTA (works with menu open)");
        }

        // Teleport Items Section
        if (ImGui::CollapsingHeader("TELEPORT ITEMS", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Enable/Disable toggle
            ImGui::Checkbox("Enable Teleport Items", &g_Settings.EnableTeleportLevelUpCards);
            
            if (g_Settings.EnableTeleportLevelUpCards)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
                ImGui::Text("Status: ENABLED");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
                ImGui::Text("Status: DISABLED");
                ImGui::PopStyleColor();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // Global Teleport All Button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(">> TELEPORT ALL <<", ImVec2(-1, 0)))
            {
                SDK_RunTeleportLevelUpCards();
            }
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
            ImGui::Text("ITEMS");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            // Item buttons - compact style
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            
            if (ImGui::Button("HP S", ImVec2(50, 0))) { SDK_TeleportItem_HPDrinkSmall(); }
            ImGui::SameLine();
            if (ImGui::Button("HP L", ImVec2(50, 0))) { SDK_TeleportItem_HPDrink(); }
            ImGui::SameLine();
            if (ImGui::Button("Team HP", ImVec2(60, 0))) { SDK_TeleportItem_HPDrinkMedium(); }
            ImGui::SameLine();
            if (ImGui::Button("GP S", ImVec2(50, 0))) { SDK_TeleportItem_GPDrinkSmall(); }
            ImGui::SameLine();
            if (ImGui::Button("GP L", ImVec2(50, 0))) { SDK_TeleportItem_GPDrink(); }
            ImGui::SameLine();
            if (ImGui::Button("Team GP", ImVec2(60, 0))) { SDK_TeleportItem_GPDrinkMedium(); }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Full Sup", ImVec2(60, 0))) { SDK_TeleportItem_FullSupport(); }
            ImGui::SameLine();
            if (ImGui::Button("Team Sup", ImVec2(70, 0))) { SDK_TeleportItem_TeamSupport(); }
            ImGui::SameLine();
            if (ImGui::Button("Bag S", ImVec2(50, 0))) { SDK_TeleportItem_BagSmall(); }
            ImGui::SameLine();
            if (ImGui::Button("Bag M", ImVec2(50, 0))) { SDK_TeleportItem_BagMedium(); }
            ImGui::SameLine();
            if (ImGui::Button("Bag L", ImVec2(50, 0))) { SDK_TeleportItem_BagLarge(); }
            ImGui::SameLine();
            if (ImGui::Button("Box", ImVec2(40, 0))) { SDK_TeleportItem_Box(); }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Box S", ImVec2(50, 0))) { SDK_TeleportItem_BoxSmall(); }
            ImGui::SameLine();
            if (ImGui::Button("Box L", ImVec2(50, 0))) { SDK_TeleportItem_BoxLarge(); }
            ImGui::SameLine();
            if (ImGui::Button("Box Gold", ImVec2(70, 0))) { SDK_TeleportItem_BoxGold(); }
            ImGui::SameLine();
            if (ImGui::Button("LvlUp", ImVec2(50, 0))) { SDK_TeleportItem_LevelUpCard(); }
            ImGui::SameLine();
            if (ImGui::Button("Enhance", ImVec2(60, 0))) { SDK_TeleportItem_TeamEnhancementKit(); }
            ImGui::SameLine();
            if (ImGui::Button("Rev 1/3", ImVec2(60, 0))) { SDK_TeleportItem_Revive1_3(); }
            
            ImGui::Spacing();
            
            if (ImGui::Button("Rev Full", ImVec2(70, 0))) { SDK_TeleportItem_ReviveFull(); }
            ImGui::SameLine();
            if (ImGui::Button("Card A", ImVec2(50, 0))) { SDK_TeleportItem_AbilitySupplyAlpha(); }
            ImGui::SameLine();
            if (ImGui::Button("Card B", ImVec2(50, 0))) { SDK_TeleportItem_AbilitySupplyBeta(); }
            ImGui::SameLine();
            if (ImGui::Button("Card Y", ImVec2(50, 0))) { SDK_TeleportItem_AbilitySupplyGamma(); }
            
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
        }
    }

    // ============================================================================
    // COMBAT MENU - Fused CHARACTER + COMBAT SUPPORT with collapsible sections
    // ============================================================================
    static bool g_CombatTrainingOpen = true;
    static bool g_InvincibilityRecoveryOpen = true;
    static bool g_CharacterConditionsOpen = true;
    static bool g_CharacterControlOpen = true;
    static bool g_SupplyManagementOpen = true;
    static bool g_AbilityHacksOpen = true;
    static bool g_ApplyToAllPlayersOpen = true;

    // Helper function to create combo lists for ranges
    static void GenerateComboItems(std::vector<std::string>& items, int min_val, int max_val, const char* format = "%d")
    {
        items.clear();
        for (int i = min_val; i <= max_val; i++)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), format, i);
            items.push_back(buf);
        }
    }

    // Helper function to safely access combo items
    static std::vector<const char*> GetComboItemPtrs(const std::vector<std::string>& items)
    {
        std::vector<const char*> ptrs;
        for (const auto& item : items) ptrs.push_back(item.c_str());
        return ptrs;
    }
    
    // Forward declaration for RenderAbilityHackMenu
    static void RenderAbilityHackMenu();

    static void RenderCombatMenu()
    {
        // ===== TRAINING CHARACTER SECTION =====
        if (ImGui::CollapsingHeader("TRAINING CHARACTER", &g_CombatTrainingOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            SectionHeader("Player Setup");
            
            // Character ID selector (0=UNDEF, 1=Ch000, 2=Ch001, ... 44=Ch999, MAX=45)
            {
                static std::vector<std::string> char_items;
                if (char_items.empty()) GenerateComboItems(char_items, 0, 44, "Ch%03d");
                auto ptrs = GetComboItemPtrs(char_items);
                ImGui::Combo("Player Character##training", &g_Settings.TrainingPlayerCharacter, ptrs.data(), (int)ptrs.size());
            }
            
            ImGui::Spacing();

            // Variation selector (dynamic based on selected character)
            {
                // Get available variations for selected character
                // g_Settings.TrainingPlayerCharacter is the index (0-44), convert to ECharacterId (0-44)
                SDK::ECharacterId currentChar = static_cast<SDK::ECharacterId>(g_Settings.TrainingPlayerCharacter);
                auto available_variations = GetVariationsForCharacter(currentChar);
                
                // Build variation combo items
                static std::vector<std::string> variation_items;
                static int last_variation_count = -1;
                if (last_variation_count != static_cast<int>(available_variations.size()))
                {
                    variation_items.clear();
                    for (int var_id : available_variations)
                    {
                        variation_items.push_back(GetVariationName(var_id));
                    }
                    last_variation_count = static_cast<int>(available_variations.size());
                    
                    // Reset variation ID if it's out of range
                    if (g_Settings.TrainingPlayerVariationId >= static_cast<int>(available_variations.size()))
                    {
                        g_Settings.TrainingPlayerVariationId = 0;
                    }
                }
                
                auto variation_ptrs = GetComboItemPtrs(variation_items);
                ImGui::Combo("Player Variation##training_var", &g_Settings.TrainingPlayerVariationId, variation_ptrs.data(), (int)variation_ptrs.size());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Skill levels
            SectionHeader("Unique Skill Levels");
            {
                static std::vector<std::string> lvl_items;
                if (lvl_items.empty()) GenerateComboItems(lvl_items, 0, 10);
                auto ptrs = GetComboItemPtrs(lvl_items);
                ImGui::Combo("Unique 1 Level##p1", &g_Settings.TrainingPlayerUnique1, ptrs.data(), (int)ptrs.size());
                ImGui::Combo("Unique 2 Level##p2", &g_Settings.TrainingPlayerUnique2, ptrs.data(), (int)ptrs.size());
                ImGui::Combo("Unique 3 Level##p3", &g_Settings.TrainingPlayerUnique3, ptrs.data(), (int)ptrs.size());
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Skill variation code
            SectionHeader("Skill Configuration");
            {
                static std::vector<std::string> skill_items;
                if (skill_items.empty()) GenerateComboItems(skill_items, 0, 5);
                auto ptrs = GetComboItemPtrs(skill_items);
                ImGui::Combo("Skill Variation Code##pskill", &g_Settings.TrainingPlayerSkillCode, ptrs.data(), (int)ptrs.size());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Costume settings
            SectionHeader("Costume Settings");
            {
                static std::vector<std::string> costume_items;
                static std::vector<std::string> aura_items;
                if (costume_items.empty()) GenerateComboItems(costume_items, 0, 100);
                if (aura_items.empty()) GenerateComboItems(aura_items, 0, 5);
                auto costume_ptrs = GetComboItemPtrs(costume_items);
                auto aura_ptrs = GetComboItemPtrs(aura_items);
                ImGui::Combo("Costume Code##pcostume", &g_Settings.TrainingPlayerCostumeCode, costume_ptrs.data(), (int)costume_ptrs.size());
                ImGui::Combo("Costume Aura Type##paura", &g_Settings.TrainingPlayerCostumeAuraType, aura_ptrs.data(), (int)aura_ptrs.size());
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Apply button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            if (ImGui::Button("Apply Player Configuration", ImVec2(-1, 0)))
            {
                InGameHack_ApplyPlayerConfiguration(
                    g_Settings.TrainingPlayerCharacter,
                    g_Settings.TrainingPlayerVariationId,
                    g_Settings.TrainingPlayerUnique1,
                    g_Settings.TrainingPlayerUnique2,
                    g_Settings.TrainingPlayerUnique3,
                    g_Settings.TrainingPlayerSkillCode,
                    g_Settings.TrainingPlayerCostumeCode,
                    g_Settings.TrainingPlayerCostumeAuraType
                );
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Load current config button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            if (ImGui::Button("Load Current Config", ImVec2(-1, 0)))
            {
                Logger::LogInfo("[Menu] Attempting to load current training player config...");
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Confirm and start training match
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
            if (ImGui::Button("Confirm - Start Training", ImVec2(-1, 0)))
            {
                Logger::LogInfo("[Menu] Starting training match with current configuration...");
                InGameHack_DecideTraining();
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Info display
            SectionHeader("Current Configuration");
            ImGui::Text("Character ID: Ch%03d", g_Settings.TrainingPlayerCharacter);
            ImGui::Text("Unique 1: Level %d", g_Settings.TrainingPlayerUnique1);
            ImGui::Text("Unique 2: Level %d", g_Settings.TrainingPlayerUnique2);
            ImGui::Text("Unique 3: Level %d", g_Settings.TrainingPlayerUnique3);
            ImGui::Text("Skill Code: %d", g_Settings.TrainingPlayerSkillCode);
            ImGui::Spacing();
        }

        // ===== INVINCIBILITY & RECOVERY SECTION =====
        if (ImGui::CollapsingHeader("INVINCIBILITY & RECOVERY", &g_InvincibilityRecoveryOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            SectionHeader("Invincibility (10s-120s, All Effects Enabled)");

            // Invincibility Hotkey button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
            {
                std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.SetInvincibleKey.Keyboard, 0) + 
                                          " | [X] " + GetKeyName(g_Settings.SetInvincibleKey.Xbox, 1) + 
                                          " | [PS] " + GetKeyName(g_Settings.SetInvincibleKey.PS4, 1);
                if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
                {
                    g_ListeningForHotkey = true;
                    g_HotkeyListenStartTime = GetTickCount();
                    g_CurrentHotkeyValue = 104;
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
            if (ImGui::Button("ACTIVATE INVINCIBILITY NOW", ImVec2(-1, 0)))
            {
                if (InGameHack_SetInvincible())
                {
                    Logger::LogInfo("[COMBAT] Invincibility activated!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to activate invincibility");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            SectionHeader("Recovery Options");

            ImGui::Checkbox("Recover Me (Self)##RecoverMe", &g_Settings.EnableRecoveryMe);
            ImGui::SameLine();
            ImGui::TextColored(g_Colors.warning, "(Self only)");

            ImGui::Checkbox("Recover Team##RecoverTeam", &g_Settings.EnableRecoveryTeam);
            ImGui::SameLine();
            ImGui::TextColored(g_Colors.success, "(Team members)");

            ImGui::Checkbox("Recover All ESP##RecoverAllESP", &g_Settings.EnableRecoveryAllESP);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.2f, 1.0f), "(Whole map)");
        }

        // ===== CHARACTER CONDITIONS SECTION =====
        if (ImGui::CollapsingHeader("CHARACTER CONDITIONS", &g_CharacterConditionsOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            SectionHeader("Rebuild & Transformations");
            
            // Rebuild Myself button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            if (ImGui::Button("OVERHAUL HEAL", ImVec2(-1, 0)))
            {
                if (InGameHack_RebuildMyself())
                {
                    Logger::LogInfo("[COMBAT] Rebuild myself executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute rebuild myself");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Rebuild Myself Hotkey button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            {
                std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.RebuildMyselfKey.Keyboard, 0) + 
                                          " | [X] " + GetKeyName(g_Settings.RebuildMyselfKey.Xbox, 1) + 
                                          " | [PS] " + GetKeyName(g_Settings.RebuildMyselfKey.PS4, 1);
                if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
                {
                    g_ListeningForHotkey = true;
                    g_HotkeyListenStartTime = GetTickCount();
                    g_CurrentHotkeyValue = 105;
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            SectionHeader("Transformations");

            // Condition mode toggle
            // CH202 Trans Mission button (85)
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
            if (ImGui::Button("DEKU MODE", ImVec2(-1, 0)))
            {
                if (InGameHack_CH202TransMission())
                {
                    Logger::LogInfo("[COMBAT] CH202 Trans Mission (85) executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute CH202 Full Mode");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Unbreakable button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
            if (ImGui::Button("UNBREAKABLE", ImVec2(-1, 0)))
            {
                if (InGameHack_Unbreakable())
                {
                    Logger::LogInfo("[COMBAT] Unbreakable executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute Unbreakable");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Compression Regeneration button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            if (ImGui::Button("MR COMPRESSE MODE", ImVec2(-1, 0)))
            {
                if (InGameHack_CompressionRegeneration())
                {
                    Logger::LogInfo("[COMBAT] Compression Regeneration executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute Compression Regeneration");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // CH024 Transparent button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            if (ImGui::Button("MIRIO MODE", ImVec2(-1, 0)))
            {
                if (InGameHack_CH024Transparent())
                {
                    Logger::LogInfo("[COMBAT] CH024 Transparent executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute CH024 Transparent");
                }
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // CH011 Abyss Dark Body button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColorActive);
            if (ImGui::Button("TOKOYAMI DARK MODE", ImVec2(-1, 0)))
            {
                if (InGameHack_CH011AbyssDarkBody())
                {
                    Logger::LogInfo("[COMBAT] CH011 Abyss Dark Body executed!");
                }
                else
                {
                    Logger::LogError("[COMBAT] Failed to execute CH011 Abyss Dark Body");
                }
            }
            ImGui::PopStyleColor();
        }

        // ===== ABILITY HACKS SECTION =====
        if (ImGui::CollapsingHeader("ABILITY HACKS", &g_AbilityHacksOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderAbilityHackMenu();
        }

        // ===== CHARACTER CONTROL SECTION =====
        if (ImGui::CollapsingHeader("CHARACTER CONTROL", &g_CharacterControlOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Refresh character list button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            if (ImGui::Button("REFRESH CHARACTER LIST##refresh", ImVec2(-1, 0)))
            {
                g_AllCharactersList = InGameHack_GetAllCharacterBattles();
                Logger::LogInfo("[CONTROL] Character list refreshed: " + std::to_string(g_AllCharactersList.size()) + " characters found");
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Auto-refresh character list if empty
            if (g_AllCharactersList.size() == 0)
            {
                g_AllCharactersList = InGameHack_GetAllCharacterBattles();
            }

            // Character dropdown with names
            if (g_AllCharactersList.size() > 0)
            {
                auto char_names_str = InGameHack_GetCharacterNames();
                
                if (char_names_str.size() > 0 && char_names_str.size() == g_AllCharactersList.size())
                {
                    static std::vector<std::string> name_storage;
                    static std::vector<const char*> name_ptrs;
                    
                    name_storage = char_names_str;
                    name_ptrs.clear();
                    for (auto& name : name_storage)
                    {
                        name_ptrs.push_back(name.c_str());
                    }
                    
                    if (!name_ptrs.empty())
                    {
                        ImGui::Text("Select Target Character:");
                        ImGui::ListBox("##character_list", &g_Settings.SelectedCharacterIndex, name_ptrs.data(), (int)name_ptrs.size(), 5);
                        
                        ImGui::Spacing();

                        if (g_Settings.SelectedCharacterIndex >= 0 && g_Settings.SelectedCharacterIndex < (int)g_AllCharactersList.size())
                        {
                            // Set to Dying button
                            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.warning);
                            if (ImGui::Button("SET TARGET TO DYING##set_dying", ImVec2(-1, 0)))
                            {
                                if (InGameHack_SetCharacterDying(g_AllCharactersList[g_Settings.SelectedCharacterIndex]))
                                {
                                    Logger::LogInfo("[CONTROL] Target set to dying state");
                                }
                                else
                                {
                                    Logger::LogError("[CONTROL] Failed to set target to dying");
                                }
                            }
                            ImGui::PopStyleColor();

                            ImGui::Spacing();

                            // Kill Target button
                            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.danger);
                            if (ImGui::Button("KILL TARGET##kill_target", ImVec2(-1, 0)))
                            {
                                if (InGameHack_KillCharacter(g_AllCharactersList[g_Settings.SelectedCharacterIndex], nullptr))
                                {
                                    Logger::LogInfo("[CONTROL] Target eliminated");
                                }
                                else
                                {
                                    Logger::LogError("[CONTROL] Failed to kill target");
                                }
                            }
                            ImGui::PopStyleColor();
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Failed to get character names");
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No names available");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No characters found. Click 'Refresh Character List'");
            }
        }

        // ===== SUPPLY MANAGEMENT SECTION =====
        if (ImGui::CollapsingHeader("SUPPLY MANAGEMENT", &g_SupplyManagementOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            SectionHeader("Supply & Skill Management");

            // Prevent Drop on Death
            static bool bPreventDrop = false;
            ImGui::Checkbox("Prevent Drop on Death##prevent_drop", &bPreventDrop);
            
            // Call every frame while enabled
            if (bPreventDrop)
            {
                InGameHack_PreventDropOnDeath(true);
            }
            ImGui::Spacing();

            // Skill Level Selector (only available skills)
            {
                static const int SKILL_VALUES[] = { 1, 2, 3, 6, 7 };
                static const char* SKILL_NAMES[] = { "UNIQUE1", "UNIQUE2", "UNIQUE3", "ABILITY", "SHOULDER" };
                static int skillComboIndex = 0;
                static int skillLevel = 1;
                
                ImGui::Combo("Skill##skill_combo", &skillComboIndex, SKILL_NAMES, IM_ARRAYSIZE(SKILL_NAMES));
                ImGui::SliderInt("Skill Level##skill_lvl", &skillLevel, 1, 9);
                
                if (ImGui::Button("Set Skill Level##set_skill_btn"))
                {
                    InGameHack_SetSkillLevel(SKILL_VALUES[skillComboIndex], skillLevel);
                }
            }
            ImGui::Spacing();

            // Supply Upgrade (ABILITY & SHOULDER only)
            {
                static const int SUPPLY_VALUES[] = { 6, 7 };
                static const char* SUPPLY_NAMES[] = { "ABILITY", "SHOULDER" };
                static int supplyComboIndex = 0;
                static int supplyLevel = 1;
                
                ImGui::Combo("Supply Type##supply_combo", &supplyComboIndex, SUPPLY_NAMES, IM_ARRAYSIZE(SUPPLY_NAMES));
                ImGui::SliderInt("Supply Level##supply_lvl", &supplyLevel, 1, 99);
                
                if (ImGui::Button("Upgrade Supply##upgrade_supply_btn"))
                {
                    InGameHack_UpgradeSupply(SUPPLY_VALUES[supplyComboIndex], supplyLevel);
                }
            }
            ImGui::Spacing();

            // Stop Using Supply
            static bool bEnableStopSupply = false;
            if (ImGui::Checkbox("Stop Using Supply##stop_supply", &bEnableStopSupply))
            {
                if (bEnableStopSupply)
                {
                    InGameHack_StopUsingSupply();
                    bEnableStopSupply = false;
                }
            }
        }

        // ===== APPLY TO ALL PLAYERS SECTION =====
        if (ImGui::CollapsingHeader("APPLY TO ALL PLAYERS", &g_ApplyToAllPlayersOpen, ImGuiTreeNodeFlags_DefaultOpen))
        {
            SectionHeader("Apply Character Configuration");

            // Character ID selector
            {
                static std::vector<std::string> char_id_items;
                if (char_id_items.empty()) GenerateComboItems(char_id_items, 1, 44, "Ch%03d");
                auto ptrs = GetComboItemPtrs(char_id_items);
                ImGui::Combo("Character ID##char_id", &g_HackSettings.CharacterId, ptrs.data(), (int)ptrs.size());
            }
            ImGui::Spacing();

            // Unique skills
            {
                static std::vector<std::string> skill_lvl_items;
                if (skill_lvl_items.empty()) GenerateComboItems(skill_lvl_items, 1, 100);
                auto ptrs = GetComboItemPtrs(skill_lvl_items);
                ImGui::Combo("Unique 1 Level##char_unique1", &g_HackSettings.CharacterUnique1, ptrs.data(), (int)ptrs.size());
                ImGui::Combo("Unique 2 Level##char_unique2", &g_HackSettings.CharacterUnique2, ptrs.data(), (int)ptrs.size());
                ImGui::Combo("Unique 3 Level##char_unique3", &g_HackSettings.CharacterUnique3, ptrs.data(), (int)ptrs.size());
            }
            ImGui::Spacing();

            // Variation and cosmetics
            {
                // Get available variations for selected character
                // g_HackSettings.CharacterId is the index (0-43), convert to ECharacterId (1-44)
                SDK::ECharacterId currentChar = static_cast<SDK::ECharacterId>(g_HackSettings.CharacterId + 1);
                auto available_variations = GetVariationsForCharacter(currentChar);
                
                // Build variation combo items
                static std::vector<std::string> variation_items;
                static int last_variation_count = -1;
                if (last_variation_count != static_cast<int>(available_variations.size()))
                {
                    variation_items.clear();
                    for (int var_id : available_variations)
                    {
                        variation_items.push_back(GetVariationName(var_id));
                    }
                    last_variation_count = static_cast<int>(available_variations.size());
                    
                    // Reset variation ID if it's out of range
                    if (g_HackSettings.CharacterVariationId >= static_cast<int>(available_variations.size()))
                    {
                        g_HackSettings.CharacterVariationId = 0;
                    }
                }
                
                static std::vector<std::string> skill_code_items;
                static std::vector<std::string> costume_code_items;
                static std::vector<std::string> aura_type_items;
                if (skill_code_items.empty()) GenerateComboItems(skill_code_items, 0, 5);
                if (costume_code_items.empty()) GenerateComboItems(costume_code_items, 0, 100);
                if (aura_type_items.empty()) GenerateComboItems(aura_type_items, 0, 5);
                auto variation_ptrs = GetComboItemPtrs(variation_items);
                auto skill_ptrs = GetComboItemPtrs(skill_code_items);
                auto costume_ptrs = GetComboItemPtrs(costume_code_items);
                auto aura_ptrs = GetComboItemPtrs(aura_type_items);
                ImGui::Combo("Variation ID##char_variation", &g_HackSettings.CharacterVariationId, variation_ptrs.data(), (int)variation_ptrs.size());
                ImGui::Combo("Skill Code##char_skill", &g_HackSettings.CharacterSkillCode, skill_ptrs.data(), (int)skill_ptrs.size());
                ImGui::Combo("Costume Code##char_costume", &g_HackSettings.CharacterCostumeCode, costume_ptrs.data(), (int)costume_ptrs.size());
                ImGui::Combo("Costume Aura Type##char_aura", &g_HackSettings.CharacterCostumeAuraType, aura_ptrs.data(), (int)aura_ptrs.size());
            }
            ImGui::Spacing();

            // Apply button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            if (ImGui::Button("APPLY CHARACTER TO ALL PLAYERS", ImVec2(-1, 0)))
            {
                if (InGameHack_ApplyToAllControllers(
                    g_HackSettings.CharacterId,
                    g_HackSettings.CharacterVariationId,
                    g_HackSettings.CharacterUnique1,
                    g_HackSettings.CharacterUnique2,
                    g_HackSettings.CharacterUnique3,
                    g_HackSettings.CharacterSkillCode,
                    g_HackSettings.CharacterCostumeCode,
                    g_HackSettings.CharacterCostumeAuraType))
                {
                    Logger::LogInfo("[CHARACTER] Applied to all controllers!");
                }
                else
                {
                    Logger::LogError("[CHARACTER] Failed to apply to all controllers");
                }
            }
            ImGui::PopStyleColor();
        }
    }

    static void RenderAbilityHackMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentMagenta);
        ImGui::Text("ABILITY HACKS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::Spacing();

        // Ability Attack
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::SliderInt("Attack Level##ability_attack", &g_HackSettings.AbilityAttackLevel, 1, 10000);
        if (ImGui::Button("ABILITY ATTACK", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityAttack(g_HackSettings.AbilityAttackLevel))
            {
                Logger::LogInfo("[COMBAT] Ability Attack Level " + std::to_string(g_HackSettings.AbilityAttackLevel) + " applied!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to apply Ability Attack");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Durable
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::SliderInt("Durable Level##ability_durable", &g_HackSettings.AbilityDurableLevel, 1, 100);
        if (ImGui::Button("ABILITY DURABLE", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityDurable(g_HackSettings.AbilityDurableLevel))
            {
                Logger::LogInfo("[COMBAT] Ability Durable Level " + std::to_string(g_HackSettings.AbilityDurableLevel) + " applied!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to apply Ability Durable");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Movespeed
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
        ImGui::SliderInt("Movespeed Level##ability_movespeed", &g_HackSettings.AbilityMovespeedLevel, 1, 100);
        if (ImGui::Button("ABILITY MOVESPEED", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityMovespeed(g_HackSettings.AbilityMovespeedLevel))
            {
                Logger::LogInfo("[COMBAT] Ability Movespeed Level " + std::to_string(g_HackSettings.AbilityMovespeedLevel) + " applied!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to apply Ability Movespeed");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Heal
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 1.0f, 1.0f));
        ImGui::SliderInt("Heal Level##ability_heal", &g_HackSettings.AbilityHealLevel, 1, 100);
        if (ImGui::Button("ABILITY HEAL", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityHeal(g_HackSettings.AbilityHealLevel))
            {
                Logger::LogInfo("[COMBAT] Ability Heal Level " + std::to_string(g_HackSettings.AbilityHealLevel) + " applied!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to apply Ability Heal");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Technique
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::SliderInt("Technique Level##ability_technique", &g_HackSettings.AbilityTechniqueLevel, 1, 100);
        if (ImGui::Button("ABILITY TECHNIQUE", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityTechnique(g_HackSettings.AbilityTechniqueLevel))
            {
                Logger::LogInfo("[COMBAT] Ability Technique Level " + std::to_string(g_HackSettings.AbilityTechniqueLevel) + " applied!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to apply Ability Technique");
            }
        }
        ImGui::PopStyleColor();
    }

    static void RenderSettingsMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("SETTINGS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Canvas Quality Settings
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentYellow);
        ImGui::Text("Settings");
        ImGui::PopStyleColor();



        ImGui::Spacing();
        ImGui::Separator();

        // Debug section
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
        ImGui::Text("DEBUG");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();

        // Save/Load
        if (ImGui::Button("Save Configuration", ImVec2(-1, 0)))
        {
            Logger::LogInfo("Configuration saved");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Configuration", ImVec2(-1, 0)))
        {
            Logger::LogInfo("Configuration loaded");
        }
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
            Logger::LogError("ImGuiMenu: Game window is null");
            return false;
        }

        if (!pSwapChain)
        {
            Logger::LogError("ImGuiMenu: SwapChain is null");
            return false;
        }

        g_GameWindow = hWnd;

        // Create ImGui context
        g_Context = ImGui::CreateContext();
        if (!g_Context)
        {
            Logger::LogError("ImGuiMenu: Failed to create ImGui context");
            return false;
        }

        ImGui::SetCurrentContext(g_Context);
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disabled by default - only enable when configuring hotkey
        io.MouseDrawCursor = true;
        io.IniFilename = nullptr;

        // Load fonts with high-quality rendering
        // Police embarqu�e dans le dossier du DLL pour fiabilit� maximale
        io.Fonts->Clear();
        ImFontConfig fontConfig;
        fontConfig.SizePixels = 18.0f;
        fontConfig.PixelSnapH = false;
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 3;
        fontConfig.RasterizerMultiply = 1.5f;
        
        ImFont* font = nullptr;
        
        // Get current module path to load font from same directory as DLL
        char modulePath[MAX_PATH];
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        std::string dllDir = modulePath;
        size_t lastSlash = dllDir.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            dllDir = dllDir.substr(0, lastSlash + 1);
        }
        
        // Try to load embedded font from DLL directory
        std::string fontPath = dllDir + "segoeui.ttf";
        font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &fontConfig);
        
        // Fallback 1: Try Windows system font
        if (!font)
        {
            font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &fontConfig);
        }
        
        // Fallback 2: Use default ImGui font
        if (!font)
        {
            font = io.Fonts->AddFontDefault(&fontConfig);
        }
        
        io.Fonts->Build();
        io.FontGlobalScale = 1.0f;

        try
        {
            ImGui::StyleColorsDark();
            SetupColorScheme();
        }
        catch (...)
        {
            Logger::LogError("ImGuiMenu: Exception during style setup");
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
                    Logger::LogWarning("ImGuiMenu: Failed to init ImGui Win32 backend - retrying");
                    retries++;
                    continue;
                }

                ID3D11Device* pDevice = D3D11Hook::GetDevice();
                ID3D11DeviceContext* pContext = D3D11Hook::GetContext();

                if (!pDevice || !pContext)
                {
                    Logger::LogWarning("ImGuiMenu: D3D11 device not ready yet - retrying");
                    ImGui_ImplWin32_Shutdown();
                    retries++;
                    continue;
                }

                if (!ImGui_ImplDX11_Init(pDevice, pContext))
                {
                    Logger::LogError("ImGuiMenu: Failed to init ImGui DX11");
                    ImGui_ImplWin32_Shutdown();
                    return false;
                }

                break;  // Success
            }
            catch (...)
            {
                Logger::LogError("ImGuiMenu: Exception during ImGui backend init");
                return false;
            }
        }

        if (retries >= MAX_RETRIES)
        {
            Logger::LogError("ImGuiMenu: Failed to initialize backends after retries");
            return false;
        }

        // Set ImGui canvas to 4K resolution for sharp rendering
        SDK_SetImGuiCanvasMaxResolution();

        try
        {
            g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
        }
        catch (...)
        {
            Logger::LogError("ImGuiMenu: Exception during WndProc hook");
            return false;
        }

        g_Initialized = true;
        Logger::LogInfo("ImGuiMenu: Initialized successfully (Press INSERT to toggle)");
        return true;
    }

    void Shutdown()
    {
        if (!g_Initialized)
            return;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();

        if (g_Context)
        {
            ImGui::DestroyContext(g_Context);
            g_Context = nullptr;
        }

        if (g_GameWindow && g_OriginalWndProc)
        {
            SetWindowLongPtr(g_GameWindow, GWLP_WNDPROC, (LONG_PTR)g_OriginalWndProc);
        }

        g_Initialized = false;
        Logger::LogInfo("ImGuiMenu: Shutdown complete");
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
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable after timeout
                return;  // Timeout - cancel listening
            }
            
            // Check for ESC key (27) - cancel
            if ((GetAsyncKeyState(0x1B) & 0x8000) != 0)
            {
                g_ListeningForHotkey = false;
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable on cancel
                return;
            }
            
            // Determine which hotkey we're listening for (unified system: 100=Aimbot, 101=Teleport, 102=Transform, 103=DuplicateImitation)
            int hotkeyType = g_CurrentHotkeyValue;
            
            // Try both keyboard and gamepad inputs
            int keyboardKey = GetHotkey(0, 0);  // Check keyboard
            int gamepadKey = GetHotkey(0, 1);   // Check gamepad
            
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
                
                g_ListeningForHotkey = false;  // Stop listening
                io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;  // Disable after hotkey set
            }
        }
        else
        {
            // Not listening - make sure gamepad is disabled so game gets inputs
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }
    }

    // ============================================================================
    // MAIN RENDER FUNCTION - MHUR PATTERN
    // ============================================================================
    void Render(IDXGISwapChain* pSwapChain)
    {
        if (!g_Initialized || !pSwapChain)
            return;

        // Update hotkey listener state every frame (Zero1 exact)
        UpdateHotkeyListener();

        // ⭐ RETRY MECHANISM: Try to initialize GameThreadHook every frame if not active yet
        if (!GameThreadHook::IsHookActive())
        {
            static int retryCounter = 0;
            retryCounter++;
            if (retryCounter % 30 == 0)  // Retry every ~30 frames (0.5s at 60 FPS)
            {
                if (GameThreadHook::Initialize())
                {
                    Logger::LogInfo("[ImGuiMenu] ✅ GameThreadHook initialized successfully!");
                }
                // Else: GWorld not ready yet, retry next attempt
            }
        }

        // Get device and context from D3D11Hook (via getter)
        ID3D11Device* device = D3D11Hook::GetDevice();
        ID3D11DeviceContext* context = D3D11Hook::GetContext();
        if (!device || !context)
            return;

        // ========================================================================
        // STEP 1: GET BACKBUFFER FROM SWAPCHAIN
        // ========================================================================
        ID3D11Texture2D* backbuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))))
        {
            Logger::LogWarning("ImGuiMenu: Failed to get backbuffer");
            return;
        }

        // ========================================================================
        // STEP 2: CREATE NEW RENDER TARGET VIEW
        // ========================================================================
        ID3D11RenderTargetView* localRTV = nullptr;
        if (FAILED(device->CreateRenderTargetView(backbuffer, nullptr, &localRTV)))
        {
            Logger::LogError("ImGuiMenu: Failed to create RenderTargetView");
            backbuffer->Release();
            return;
        }

        if (!localRTV)
        {
            backbuffer->Release();
            return;
        }

        // ========================================================================
        // STEP 3: SET IMGUI CONTEXT & BEGIN NEW FRAME
        // ========================================================================
        ImGui::SetCurrentContext(g_Context);
        
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
        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
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
        SDK_DrawAllRectangles();
        SDK_DrawAllTextLabels();
        
        // Only draw skeletons if enabled
        if (g_Settings.ShowPlayerSkeleton)
        {
            SDK_DrawAllSkeletons();
        }
        
        SDK_DrawAimbotInfo();
        SDK_DrawAimbotFOV();

        // ========================================================================
        // STEP 4: DRAW MAIN MENU WINDOW (Sidebar + Content Layout)
        // ========================================================================
        if (g_Visible)
        {
            ImGui::SetNextWindowSize(ImVec2(850, 600), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(100, 50), ImGuiCond_FirstUseEver);

            if (ImGui::Begin("##MainWindow", &g_Visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse))
            {
                // ========== HEADER ==========
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentColor);
                ImGui::SetWindowFontScale(1.5f);
                ImGui::Text("RUGIR-INTERNAL");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();

                ImGui::SameLine(ImGui::GetWindowWidth() - 320.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.textSecondary);
                ImGui::Text("Global:");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                
                if (ImGui::Checkbox("##GlobalToggle", &g_Settings.EnableGlobal))
                {
                    // Global enable toggle
                }
                StatusIndicator(g_Settings.EnableGlobal);

                ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.textSecondary);
                ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ========== SIDEBAR + CONTENT LAYOUT ==========
                float sidebarWidth = 140.0f;
                ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0.0f), true);
                
                // Sidebar buttons
                ImGui::Spacing();
                if (ModernButton("ESP", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 0))
                    g_SelectedTab = 0;
                ImGui::Spacing();
                
                if (ModernButton("AIMBOT", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 1))
                    g_SelectedTab = 1;
                ImGui::Spacing();
                
                if (ModernButton("COMBAT", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 2))
                    g_SelectedTab = 2;
                ImGui::Spacing();
                
                if (ModernButton("HACKS", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 3))
                    g_SelectedTab = 3;
                ImGui::Spacing();
                
                if (ModernButton("VISUAL", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 4))
                    g_SelectedTab = 4;
                ImGui::Spacing();
                
                if (ModernButton("SETTINGS", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 5))
                    g_SelectedTab = 5;
                ImGui::Spacing();

                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 80.0f);
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.textSecondary);
                ImGui::Text("v1.0.0");
                ImGui::PopStyleColor();

                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("##Content", ImVec2(0.0f, 0.0f), true);

                // ========== CONTENT AREA ==========
                switch (g_SelectedTab)
                {
                case 0:  // ESP
                    RenderESPMenu();
                    break;
                case 1:  // AIMBOT
                    RenderAimbotMenu();
                    break;
                case 2:  // COMBAT
                    RenderCombatMenu();
                    break;
                case 3:  // HACKS
                    RenderHacksMenu();
                    break;
                case 4:  // VISUAL
                    RenderHacksMenu();  // Render hacks for visual section
                    break;
                case 5:  // SETTINGS
                    RenderSettingsMenu();
                    break;
                }

                ImGui::EndChild();

                ImGui::Separator();
                ImGui::Spacing();

                // Unload button with warning
                ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.danger);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                
                if (ImGui::Button("UNLOAD DLL (Complete Cleanup)", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
                {
                    Logger::LogWarning("[Menu] UNLOAD DLL button pressed - complete cleanup starting...");
                    DLL_Unload();
                }
                
                ImGui::PopStyleColor(3);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠️  Removes ALL hooks and unloads DLL");
                ImGui::TextColored(g_Colors.textSecondary, "Press INSERT to toggle menu");

                ImGui::End();
            }
        }

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
        context->OMSetRenderTargets(1, &localRTV, nullptr);

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

        if (localRTV)
        {
            localRTV->Release();
            localRTV = nullptr;
        }

        backbuffer->Release();
    }

    bool IsInitialized() { return g_Initialized; }
    bool IsVisible() { return g_Visible; }
    void SetVisible(bool visible) { g_Visible = visible; }
}
