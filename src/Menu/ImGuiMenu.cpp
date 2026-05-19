#include "ImGuiMenu.h"

#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Hacks/InGameModuleHacks.h"
#include <algorithm>
#include "../SDK/SDKInit.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include <imgui.h>
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
#pragma comment(lib, "xinput.lib")

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
    static int g_SelectedTab = 0;  // 0=ESP, 1=AIMBOT, 2=COMBAT, 3=HACKS, 4=LOBBY EXPLOIT, 5=SETTINGS
    
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
        style->ChildRounding = 5.0f;

        style->WindowPadding = ImVec2(15.0f, 15.0f);
        style->FramePadding = ImVec2(10.0f, 6.0f);
        style->ItemSpacing = ImVec2(10.0f, 8.0f);
        style->ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style->IndentSpacing = 20.0f;
        style->ScrollbarSize = 12.0f;
        style->GrabMinSize = 8.0f;
        style->WindowBorderSize = 1.50f;
        style->FrameBorderSize = 0.0f;
        style->PopupBorderSize = 1.0f;
    }

    // ============================================================================
    // HELPER FUNCTIONS - MODERN UI COMPONENTS
    // ============================================================================
    static bool ModernButton(const char* label, ImVec2 size, bool selected = false)
    {
        // Use Dummy for positioning without any default rendering
        ImGui::PushID(label);
        ImGui::Dummy(size);
        ImVec2 buttonPos = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();
        bool isHovered = ImGui::IsItemHovered();
        bool isClicked = isHovered && ImGui::IsMouseClicked(0);
        ImGui::PopID();
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw ONLY ONE background rect
        ImU32 bgColor;
        if (isHovered)
        {
            bgColor = ImGui::GetColorU32(selected ? g_Colors.accentColorHover : g_Colors.accentColor);
        }
        else
        {
            bgColor = ImGui::GetColorU32(selected ? g_Colors.accentColor : g_Colors.bgLight);
        }
        drawList->AddRectFilled(buttonPos, buttonMax, bgColor, 4.0f);
        
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
        ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        
        if (!iconChar.empty() && !labelText.empty() && g_SymbolFont)
        {
            // Draw icon with symbol font
            ImGui::PushFont(g_SymbolFont);
            ImVec2 iconSize = ImGui::CalcTextSize(iconChar.c_str());
            ImVec2 iconPos = ImVec2(
                buttonPos.x + 8.0f,
                buttonPos.y + (buttonSize.y - iconSize.y) / 2.0f
            );
            drawList->AddText(iconPos, textColor, iconChar.c_str());
            ImGui::PopFont();
            
            // Draw label text with default font
            ImVec2 textSize = ImGui::CalcTextSize(labelText.c_str());
            ImVec2 textPos = ImVec2(
                iconPos.x + iconSize.x + 8.0f,
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

            ImGuiHelper::ToggleSwitchLarge("Global ESP Enable", &g_Settings.EnablePlayerESP);
            ImGuiHelper::ToggleSwitch("Show Enemy", &g_Settings.ShowEnemies);
            ImGuiHelper::ToggleSwitch("Show Team", &g_Settings.ShowTeam);
            ImGuiHelper::ToggleSwitch("LobbyCharacter", &g_Settings.ShowLobbyCharacters);
            ImGuiHelper::ToggleSwitch("KOTA", &g_Settings.ShowKota);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
            ImGui::Text("DISPLAY INFO");
            ImGui::PopStyleColor();
            
            ImGuiHelper::ToggleSwitch("HP", &g_Settings.ShowHP);
            ImGui::SameLine();
            ImGuiHelper::ToggleSwitch("GP", &g_Settings.ShowGP);
            ImGui::SameLine();
            ImGuiHelper::ToggleSwitch("PU", &g_Settings.ShowPU);
            ImGui::SameLine();
            ImGuiHelper::ToggleSwitch("Platform", &g_Settings.ShowPlatform);
            ImGui::SameLine();
            ImGuiHelper::ToggleSwitch("Team ID", &g_Settings.ShowTeamId);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGuiHelper::ToggleSwitch("Show Skeleton", &g_Settings.ShowPlayerSkeleton);
            ImGuiSliderHelper::SliderFloat("Max Draw Distance", &g_Settings.Player_DrawDistance, 150.0f, 100.0f, 5000.0f);

            ImGui::Spacing();
        }
    }

    static void RenderAimbotMenu()
    {
        // ====================================================================
        // AIMBOT SECTION - Collapsible, closed by default
        // ====================================================================
        if (ImGui::CollapsingHeader("AIMBOT SETTINGS", ImGuiTreeNodeFlags_None))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
            ImGui::Spacing();
            ImGui::PopStyleColor();
            
            ImGuiHelper::ToggleSwitchLarge("Enable Aimbot", &g_Settings.EnableAimbot);
            ImGuiHelper::ToggleSwitchLarge("Smooth Aiming", &g_Settings.AimbotSmoothing);
            
            if (g_Settings.AimbotSmoothing)
            {
                ImGuiSliderHelper::SliderFloat("Smooth Factor", &g_Settings.AimbotSmoothFactor, 150.0f, 0.01f, 1.0f);
            }
            
            ImGuiHelper::ToggleSwitch("Draw Aim Line", &g_Settings.AimbotDrawLine);
            ImGuiHelper::ToggleSwitch("Draw Aim FOV", &g_Settings.AimbotDrawFOV);
            
            if (g_Settings.AimbotDrawFOV)
            {
                ImGuiSliderHelper::SliderFloat("FOV Radius", &g_Settings.AimbotFOVRadius, 150.0f, 100.0f, 1000.0f);
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
        }
        
        // ====================================================================
        // BULLET REDIRECTION SECTION
        // ====================================================================
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        
        if (ImGui::CollapsingHeader("BULLET TP (SILENT AIM)", ImGuiTreeNodeFlags_None))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
            ImGui::Spacing();
            ImGuiHelper::ToggleSwitchLarge("Enable Bullet Tp", &g_Settings.EnableBulletTP);
            ImGui::PopStyleColor();
            
            if (g_Settings.EnableBulletTP)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
                ImGui::Text("SKILL TYPE FILTERS");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                
                ImGuiHelper::ToggleSwitch("Alpha (Unique1)", &g_Settings.BulletTP_IncludeAlpha);
                ImGuiHelper::ToggleSwitch("Beta (Unique2)", &g_Settings.BulletTP_IncludeBeta);
                ImGuiHelper::ToggleSwitch("Gamma (Unique3)", &g_Settings.BulletTP_IncludeGamma);
                ImGuiHelper::ToggleSwitch("Special", &g_Settings.BulletTP_IncludeSpecial);
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
                ImGui::Text("BULLET TP");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                
                ImGuiSliderHelper::SliderFloat("FOV Radius ", &g_Settings.BulletTP_FOVRadius, 150.0f, 20.0f, 100.0f);
                ImGuiSliderHelper::SliderFloat("Max Distance ", &g_Settings.BulletTP_MaxDistance, 150.0f, 1000.0f, 100000.0f);
                
                ImGui::Spacing();
                ImGui::TextWrapped("Note: Uses same hotkey as Aimbot. Redirects filtered bullets to nearest forward target within FOV and distance constraints.");
            }
        }
    }

    static void RenderHacksMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("HACKS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Transform Into Random ESP Target Section
        if (ImGui::CollapsingHeader("TRANSFORM TOGA", ImGuiTreeNodeFlags_None))
        {
            // Enable/Disable toggle
            ImGuiHelper::ToggleSwitchLarge("Enable Transform", &g_Settings.EnableTransformIntoRandomESP);
            
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
        if (ImGui::CollapsingHeader("DUPLICATE IMITATION", ImGuiTreeNodeFlags_None))
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentYellow);
            ImGui::Text("DUPLICATE IMITATION VARIANT");
            ImGui::PopStyleColor();
            
            ImGuiHelper::ToggleSwitchLarge("Enable DuplicateIntoImitation", &g_Settings.EnableDuplicateIntoImitationRandomESP);
            ImGuiSliderHelper::SliderFloat("Imitation Lifetime", &g_Settings.DuplicateImitationLifeTime, 150.0f, 5.0f, 120.0f);
            ImGuiSliderHelper::SliderInt("Imitation Duplicate Count", &g_Settings.DuplicateIntoImitationCount, 150.0f, 1, 100);
            
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
            
            ImGuiSliderHelper::SliderFloat("General Reload Rate", &g_Settings.ReloadAdjustRate, 150.0f, 1.0f, 5.0f);
            ImGuiSliderHelper::SliderFloat("Reload Rate (Roll Slot)", &g_Settings.ReloadAdjustRate_RollSlot, 150.0f, 1.0f, 5.0f);
            ImGuiSliderHelper::SliderFloat("Reload Rate (Blue Flame)", &g_Settings.ReloadAdjustRate_WearBlueFlame, 150.0f, 1.0f, 50.0f);
            
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
        if (ImGui::CollapsingHeader("TELEPORT TO KOTA", ImGuiTreeNodeFlags_None))
        {
            // Enable/Disable toggle
            ImGuiHelper::ToggleSwitchLarge("Enable Teleport to KOTA", &g_Settings.EnableTeleportToKota);
            
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
        if (ImGui::CollapsingHeader("TELEPORT ITEMS", ImGuiTreeNodeFlags_None))
        {
            // Enable/Disable toggle
            ImGuiHelper::ToggleSwitchLarge("Enable Teleport Items", &g_Settings.EnableTeleportLevelUpCards);
            
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
    static bool g_TrainingIterationOpen = true;
    static bool g_AbilityHacksOpen = true;
    static bool g_ApplyToAllPlayersOpen = true;
    
    // Lobby Exploit Menu state
    static int g_SelectedTeamId = -1;  // -1 = no selection, or actual team ID
    static std::vector<unsigned char> g_AvailableTeamIds;  // List of teams in current match
    static bool g_LobbyExploitOpen = true;
    static bool g_ApplyToTeamOpen = true;

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
        if (ImGui::CollapsingHeader("OWNER CHARACTER SWAP", &g_CombatTrainingOpen, ImGuiTreeNodeFlags_None))
        {
            // Character + Variation selector
            static std::vector<SDK::EVariationCharacterId> all_variation_ids;
            static std::vector<std::string> all_variation_names;
            static std::vector<const char*> variation_ptrs;
            
            if (all_variation_ids.empty())
            {
                all_variation_ids = GetAllVariationCharacterIds();
                all_variation_names = GetAllVariationNames();
                for (const auto& name : all_variation_names)
                {
                    variation_ptrs.push_back(name.c_str());
                }
            }
            
            static int selected_variation_index_training = 0;
            ImGui::Combo("Character", &selected_variation_index_training, variation_ptrs.data(), (int)variation_ptrs.size());

            // Single unified slider for technique levels
            static int uniqueLevelTraining = 9;
            ImGuiSliderHelper::SliderInt("Technique Level", &uniqueLevelTraining, 150.0f, 1, 9);

            ImGui::Spacing();

            // Apply button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 1.0f, 0.65f, 1.0f));
            
            if (ImGui::Button("APPLY CONFIGURATION", ImVec2(ImGui::GetContentRegionAvail().x, 35)))
            {
                if (selected_variation_index_training >= 0 && selected_variation_index_training < (int)all_variation_ids.size())
                {
                    SDK::EVariationCharacterId variationCharId = all_variation_ids[selected_variation_index_training];
                    auto [characterId, variationId] = GetCharacterAndVariationFromVariationCharacterId(variationCharId);
                    
                    InGameHack_ApplyPlayerConfiguration(
                        (int)characterId,
                        variationId,
                        uniqueLevelTraining,
                        uniqueLevelTraining,
                        uniqueLevelTraining,
                        0,
                        0,
                        0
                    );
                }
            }
            
            ImGui::PopStyleColor(2);
        }

        // ===== INVINCIBILITY & RECOVERY SECTION =====
        if (ImGui::CollapsingHeader("INVINCIBILITY & RECOVERY", &g_InvincibilityRecoveryOpen, ImGuiTreeNodeFlags_None))
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
            static bool g_EnableInvincibility = false;
            if (ImGuiHelper::ToggleSwitch("ACTIVATE INVINCIBILITY NOW", &g_EnableInvincibility))
            {
                if (g_EnableInvincibility)
                {
                    if (InGameHack_SetInvincible())
                    {
}
                }
                else
                {
}
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            SectionHeader("Recovery Options");

            ImGuiHelper::ToggleSwitch("Recover Me (Self)", &g_Settings.EnableRecoveryMe);
            ImGui::SameLine();
            ImGui::TextColored(g_Colors.warning, "(Self only)");

            ImGuiHelper::ToggleSwitch("Recover Team", &g_Settings.EnableRecoveryTeam);
            ImGui::SameLine();
            ImGui::TextColored(g_Colors.success, "(Team members)");

            ImGuiHelper::ToggleSwitch("Recover All ESP", &g_Settings.EnableRecoveryAllESP);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.2f, 1.0f), "(Whole map)");

            ImGui::Spacing();
            SectionHeader("Recovery Team Roster");

            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            if (ImGui::Button("REFRESH TEAM ROSTER", ImVec2(-1, 0)))
            {
                g_CurrentTeamCharacters = InGameHack_GetTeamCharacterBattles();
                g_Settings.SelectedRecoveryTeamIndex = -1;
}
            ImGui::PopStyleColor();

            if (g_CurrentTeamCharacters.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No team members found. Refresh or join a battle.");
            }
            else
            {
                auto teamNames = InGameHack_GetTeamCharacterNames();
                static std::vector<std::string> teamNameStorage;
                static std::vector<const char*> teamNamePtrs;
                teamNameStorage = teamNames;
                teamNamePtrs.clear();
                for (auto& name : teamNameStorage)
                {
                    teamNamePtrs.push_back(name.c_str());
                }

                if (!teamNamePtrs.empty())
                {
                    ImGui::Text("Select team member to recover:");
                    ImGui::ListBox("##team_member_list", &g_Settings.SelectedRecoveryTeamIndex, teamNamePtrs.data(), (int)teamNamePtrs.size(), 5);

                    ImGui::Spacing();
                    if (g_Settings.SelectedRecoveryTeamIndex >= 0 && g_Settings.SelectedRecoveryTeamIndex < (int)g_CurrentTeamCharacters.size())
                    {
                        ImGuiHelper::ToggleSwitchLarge("Recover Selected Team Member", &g_Settings.EnableRecoverySelectedTeam);
                        ImGui::SameLine();
                        ImGui::TextColored(g_Colors.accentColor, "(Selected member only)");
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Select a team member from the list above.");
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No team names could be generated.");
                }
            }
        }

        // ===== CHARACTER CONDITIONS SECTION =====
        if (ImGui::CollapsingHeader("CHARACTER CONDITIONS", &g_CharacterConditionsOpen, ImGuiTreeNodeFlags_None))
        {
            SectionHeader("Rebuild & Transformations");
            
            // Rebuild Myself button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
            if (ImGui::Button("OVERHAUL HEAL", ImVec2(-1, 0)))
            {
                if (InGameHack_RebuildMyself())
                {
}
                else
                {
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
            // CH202 Trans Mission toggle (85)
            static bool g_EnableDekuMode = false;
            if (ImGuiHelper::ToggleSwitch("DEKU MODE", &g_EnableDekuMode))
            {
                if (g_EnableDekuMode)
                {
                    if (InGameHack_CH202TransMission())
                    {
}
                }
                else
                {
}
            }

            ImGui::Spacing();
            
            // CH202 Init Transmission Mission Level enable/disable toggle
            ImGuiHelper::ToggleSwitch("Enable CH202 Trans Level 5", &g_Settings.EnableCH202InitTransLevel5);
            
            ImGui::Spacing();
            
            // Supply Max Stack 100 enable/disable toggle
            ImGuiHelper::ToggleSwitch("Supply Max Stack 100", &g_Settings.EnableSupplyMaxStackTo100);
            
            ImGui::Spacing();

            // Unbreakable toggle
            static bool g_EnableUnbreakable = false;
            if (ImGuiHelper::ToggleSwitch("UNBREAKABLE", &g_EnableUnbreakable))
            {
                if (g_EnableUnbreakable)
                {
                    if (InGameHack_Unbreakable())
                    {
}
                }
                else
                {
}
            }

            ImGui::Spacing();

            // Compression Regeneration toggle
            static bool g_EnableCompressionRegen = false;
            if (ImGuiHelper::ToggleSwitch("MR COMPRESSE MODE", &g_EnableCompressionRegen))
            {
                if (g_EnableCompressionRegen)
                {
                    if (InGameHack_CompressionRegeneration())
                    {
}
                }
                else
                {
}
            }

            ImGui::Spacing();

            // CH024 Transparent toggle
            static bool g_EnableMirioMode = false;
            if (ImGuiHelper::ToggleSwitch("MIRIO MODE", &g_EnableMirioMode))
            {
                if (g_EnableMirioMode)
                {
                    if (InGameHack_CH024Transparent())
                    {
}
                }
                else
                {
}
            }

            ImGui::Spacing();

            // CH011 Abyss Dark Body toggle
            static bool g_EnableTokoyamiMode = false;
            if (ImGuiHelper::ToggleSwitch("TOKOYAMI DARK MODE", &g_EnableTokoyamiMode))
            {
                if (g_EnableTokoyamiMode)
                {
                    if (InGameHack_CH011AbyssDarkBody())
                    {
}
                }
                else
                {
}
            }
        }

        // ===== ABILITY HACKS SECTION =====
        if (ImGui::CollapsingHeader("ABILITY HACKS", &g_AbilityHacksOpen, ImGuiTreeNodeFlags_None))
        {
            RenderAbilityHackMenu();
        }

        // ===== SUPPLY MANAGEMENT SECTION =====
        if (ImGui::CollapsingHeader("SUPPLY MANAGEMENT", &g_SupplyManagementOpen, ImGuiTreeNodeFlags_None))
        {
            SectionHeader("Supply & Skill Management");

            // Prevent Drop on Death
            static bool bPreventDrop = false;
            ImGuiHelper::ToggleSwitch("Prevent Drop on Death", &bPreventDrop);
            
            // Call every frame while enabled
            if (bPreventDrop)
            {
                InGameHack_PreventDropOnDeath(true);
            }
            ImGui::Spacing();

            // Unique Skills Sliders (Unique 1, 2, 3)
            {
                static int unique1Level = 1;
                static int unique2Level = 1;
                static int unique3Level = 1;
                
                ImGuiSliderHelper::SliderInt("Unique Skill 1", &unique1Level, 150.0f, 1, 9);
                ImGuiSliderHelper::SliderInt("Unique Skill 2", &unique2Level, 150.0f, 1, 9);
                ImGuiSliderHelper::SliderInt("Unique Skill 3", &unique3Level, 150.0f, 1, 9);
                
                if (ImGui::Button("Set Unique Skills", ImVec2(-1, 0)))
                {
                    InGameHack_SetSkillLevel(1, unique1Level);  // UNIQUE1
                    InGameHack_SetSkillLevel(2, unique2Level);  // UNIQUE2
                    InGameHack_SetSkillLevel(3, unique3Level);  // UNIQUE3
                }
            }
            ImGui::Spacing();

            // Supply Upgrade (ABILITY & SHOULDER only)
            {
                static const int SUPPLY_VALUES[] = { 6, 7 };
                static const char* SUPPLY_NAMES[] = { "ABILITY", "SHOULDER" };
                static int supplyComboIndex = 0;
                static int supplyLevel = 1;
                
                ImGui::Combo("Supply Type", &supplyComboIndex, SUPPLY_NAMES, IM_ARRAYSIZE(SUPPLY_NAMES));
                ImGuiSliderHelper::SliderInt("Supply Level", &supplyLevel, 150.0f, 1, 99);
                
                if (ImGui::Button("Upgrade Supply"))
                {
                    InGameHack_UpgradeSupply(SUPPLY_VALUES[supplyComboIndex], supplyLevel);
                }
            }
            ImGui::Spacing();

            // Stop Using Supply
            static bool bEnableStopSupply = false;
            if (ImGuiHelper::ToggleSwitch("Stop Using Supply", &bEnableStopSupply))
            {
                if (bEnableStopSupply)
                {
                    InGameHack_StopUsingSupply();
                    bEnableStopSupply = false;
                }
            }
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
        ImGuiSliderHelper::SliderInt("Attack Level", &g_HackSettings.AbilityAttackLevel, 150.0f, 1, 10000);
        if (ImGui::Button("ABILITY ATTACK", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityAttack(g_HackSettings.AbilityAttackLevel))
            {
}
            else
            {
}
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Durable
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGuiSliderHelper::SliderInt("Durable Level", &g_HackSettings.AbilityDurableLevel, 150.0f, 1, 100);
        if (ImGui::Button("ABILITY DURABLE", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityDurable(g_HackSettings.AbilityDurableLevel))
            {
}
            else
            {
}
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Movespeed
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
        ImGuiSliderHelper::SliderInt("Movespeed Level", &g_HackSettings.AbilityMovespeedLevel, 150.0f, 1, 100);
        if (ImGui::Button("ABILITY MOVESPEED", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityMovespeed(g_HackSettings.AbilityMovespeedLevel))
            {
}
            else
            {
}
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Heal
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 1.0f, 1.0f));
        ImGuiSliderHelper::SliderInt("Heal Level", &g_HackSettings.AbilityHealLevel, 150.0f, 1, 100);
        if (ImGui::Button("ABILITY HEAL", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityHeal(g_HackSettings.AbilityHealLevel))
            {
}
            else
            {
}
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Ability Technique
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGuiSliderHelper::SliderInt("Technique Level", &g_HackSettings.AbilityTechniqueLevel, 150.0f, 1, 100);
        if (ImGui::Button("ABILITY TECHNIQUE", ImVec2(-1, 0)))
        {
            if (InGameHack_AbilityTechnique(g_HackSettings.AbilityTechniqueLevel))
            {
}
            else
            {
}
        }
        ImGui::PopStyleColor();
    }

    /**
     * Render Lobby Exploit Menu - Team-based character modification
     * Allows applying character changes to specific teams
     */
    static void RenderLobbyExploitMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text("LOBBY EXPLOIT");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ===== APPLY TO TEAM =====
        if (ImGui::CollapsingHeader("APPLY TO TEAM", &g_ApplyToTeamOpen))
        {
            ImGui::Spacing();

            // Refresh Team List button
            if (ImGui::Button("Refresh Team List", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
            {
                g_AvailableTeamIds = InGameHack_GetAllTeamIds();
                g_SelectedTeamId = -1;  // Reset selection
            }
            ImGui::Spacing();

            // Display available teams
            ImGui::Text("Available Teams: %zu", g_AvailableTeamIds.size());
            ImGui::Spacing();

            if (g_AvailableTeamIds.empty())
            {
                ImGui::TextColored(g_Colors.warning, "No teams found. Click 'Refresh Team List'.");
            }
            else
            {
                // Display team selection buttons
                ImGui::Text("Select Team:");
                for (unsigned char teamId : g_AvailableTeamIds)
                {
                    char label[64];
                    snprintf(label, sizeof(label), "Team %d", (int)teamId);
                    
                    bool isSelected = (g_SelectedTeamId == (int)teamId);
                    if (isSelected)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.accentColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Colors.accentColorHover);
                    }
                    
                    if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 25)))
                    {
                        g_SelectedTeamId = (int)teamId;
                    }
                    
                    if (isSelected)
                    {
                        ImGui::PopStyleColor(2);
                    }
                }
            }

            ImGui::Spacing();

            // Character + Variation selector
            static std::vector<SDK::EVariationCharacterId> all_variation_ids;
            static std::vector<std::string> all_variation_names;
            static std::vector<const char*> variation_ptrs;
            
            if (all_variation_ids.empty())
            {
                all_variation_ids = GetAllVariationCharacterIds();
                all_variation_names = GetAllVariationNames();
                for (const auto& name : all_variation_names)
                {
                    variation_ptrs.push_back(name.c_str());
                }
            }
            
            static int selected_variation_index = 0;
            ImGui::Combo("Character", &selected_variation_index, variation_ptrs.data(), (int)variation_ptrs.size());

            // Single unified slider for technique levels
            static int uniqueLevelTeam = 9;
            ImGuiSliderHelper::SliderInt("Technique Level", &uniqueLevelTeam, 150.0f, 1, 9);

            ImGui::Spacing();

            // Apply to Team button
            if (g_SelectedTeamId == -1)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.textDisabled);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Colors.textDisabled);
                ImGui::Button("Select a Team First", ImVec2(ImGui::GetContentRegionAvail().x, 35));
                ImGui::PopStyleColor(2);
            }
            else
            {
                char applyLabel[128];
                snprintf(applyLabel, sizeof(applyLabel), "APPLY TO TEAM %d", g_SelectedTeamId);
                
                ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 1.0f, 0.65f, 1.0f));
                
                if (ImGui::Button(applyLabel, ImVec2(ImGui::GetContentRegionAvail().x, 35)))
                {
                    if (selected_variation_index >= 0 && selected_variation_index < (int)all_variation_ids.size())
                    {
                        SDK::EVariationCharacterId variationCharId = all_variation_ids[selected_variation_index];
                        
                        if (InGameHack_ApplyToTeam(
                            (unsigned char)g_SelectedTeamId,
                            variationCharId,
                            uniqueLevelTeam,
                            uniqueLevelTeam,
                            uniqueLevelTeam,
                            0,
                            0))
                        {
                            ImGui::OpenPopup("##ApplyTeamSuccess");
                        }
                        else
                        {
                            ImGui::OpenPopup("##ApplyTeamFailed");
                        }
                    }
                }
                
                ImGui::PopStyleColor(2);
            }
        }

        // ===== APPLY TO ALL PLAYERS =====
        if (ImGui::CollapsingHeader("APPLY TO ALL PLAYERS", &g_ApplyToAllPlayersOpen, ImGuiTreeNodeFlags_None))
        {
            // Character + Variation selector
            static std::vector<SDK::EVariationCharacterId> all_players_variation_ids;
            static std::vector<std::string> all_players_variation_names;
            static std::vector<const char*> all_players_variation_ptrs;
            
            if (all_players_variation_ids.empty())
            {
                all_players_variation_ids = GetAllVariationCharacterIds();
                all_players_variation_names = GetAllVariationNames();
                for (const auto& name : all_players_variation_names)
                {
                    all_players_variation_ptrs.push_back(name.c_str());
                }
            }
            
            static int selected_all_players_index = 0;
            ImGui::Combo("Character", &selected_all_players_index, all_players_variation_ptrs.data(), (int)all_players_variation_ptrs.size());

            // Single unified slider for technique levels
            static int uniqueLevelAll = 9;
            ImGuiSliderHelper::SliderInt("Technique Level", &uniqueLevelAll, 150.0f, 1, 9);

            ImGui::Spacing();

            // Apply button
            ImGui::PushStyleColor(ImGuiCol_Button, g_Colors.success);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 1.0f, 0.65f, 1.0f));
            
            if (ImGui::Button("APPLY TO ALL PLAYERS", ImVec2(ImGui::GetContentRegionAvail().x, 35)))
            {
                if (selected_all_players_index >= 0 && selected_all_players_index < (int)all_players_variation_ids.size())
                {
                    SDK::EVariationCharacterId variationCharId = all_players_variation_ids[selected_all_players_index];
                    
                    if (InGameHack_ApplyToAllControllers(
                        variationCharId,
                        uniqueLevelAll,
                        uniqueLevelAll,
                        uniqueLevelAll,
                        0,
                        0))
                    {
                        ImGui::OpenPopup("ApplyAllSuccess");
                    }
                    else
                    {
                        ImGui::OpenPopup("ApplyAllFailed");
                    }
                }
            }
            
            ImGui::PopStyleColor(2);
        }

        // Success popup - Apply All
        if (ImGui::BeginPopupModal("ApplyAllSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(g_Colors.success, "Successfully applied changes to all players!");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }

        // Failed popup - Apply All
        if (ImGui::BeginPopupModal("ApplyAllFailed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(g_Colors.danger, "Failed to apply changes to all players.");
            ImGui::TextColored(g_Colors.textSecondary, "Ensure you are in a valid battle mode.");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("ApplyTeamSuccess", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(g_Colors.success, "Successfully applied changes to Team %d!", g_SelectedTeamId);
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }

        // Failed popup - Apply Team
        if (ImGui::BeginPopupModal("ApplyTeamFailed", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(g_Colors.danger, "Failed to apply changes to Team %d.", g_SelectedTeamId);
            ImGui::TextColored(g_Colors.textSecondary, "Ensure you are in a valid battle mode.");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
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
}
        ImGui::SameLine();
        if (ImGui::Button("Load Configuration", ImVec2(-1, 0)))
        {
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
            SetupColorScheme();
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
        }
        catch (...)
        {
return false;
        }

        g_Initialized = true;
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
return;
        }

        // ========================================================================
        // STEP 2: CREATE NEW RENDER TARGET VIEW
        // ========================================================================
        ID3D11RenderTargetView* localRTV = nullptr;
        if (FAILED(device->CreateRenderTargetView(backbuffer, nullptr, &localRTV)))
        {
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
        
        // ===== CURSOR VISIBILITY CONTROL =====
        // Only show mouse cursor when menu is visible
        io.MouseDrawCursor = g_Visible;
        
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
        SDK_DrawBulletRedirectionFOV();

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
                
                if (ImGuiHelper::ToggleSwitchLarge("Global", &g_Settings.EnableGlobal))
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
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("##Sidebar", ImVec2(sidebarWidth, 0.0f), false);
                ImGui::PopStyleColor();
                
                // Sidebar buttons
                ImGui::Spacing();
                if (ModernButton("P   ESP", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 0))
                    g_SelectedTab = 0;
                ImGui::Spacing();
                
                if (ModernButton("N   AIMBOT", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 1))
                    g_SelectedTab = 1;
                ImGui::Spacing();
                
                if (ModernButton("Q   COMBAT", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 2))
                    g_SelectedTab = 2;
                ImGui::Spacing();
                
                if (ModernButton("I   HACKS", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 3))
                    g_SelectedTab = 3;
                ImGui::Spacing();
                
                if (ModernButton("O   LOBBY EXPLOIT", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 4))
                    g_SelectedTab = 4;
                ImGui::Spacing();
                
                if (ModernButton("R   SETTINGS", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f), g_SelectedTab == 5))
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
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("##Content", ImVec2(0.0f, 0.0f), false);
                ImGui::PopStyleColor();

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
                case 4:  // LOBBY EXPLOIT
                    RenderLobbyExploitMenu();
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
