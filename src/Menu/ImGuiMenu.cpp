#include "ImGuiMenu.h"
#include "../Utils/Logger.h"
#include "../Hooks/D3D11Hook.h"
#include "../Hooks/GameThreadHook.h"
#include "../Hacks/InGameModuleHacks.h"
#include "../SDK/SDKInit.h"
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
        ImVec4 bgDarkest = ImVec4(0.04f, 0.04f, 0.05f, 1.0f);
        ImVec4 bgDark = ImVec4(0.075f, 0.075f, 0.085f, 1.0f);

        ImVec4 primaryBlue = ImVec4(0.0f, 0.65f, 1.0f, 1.0f);
        ImVec4 primaryBlueBright = ImVec4(0.2f, 0.8f, 1.0f, 1.0f);

        ImVec4 accentCyan = ImVec4(0.0f, 1.0f, 0.8f, 1.0f);
        ImVec4 accentMagenta = ImVec4(1.0f, 0.0f, 0.8f, 1.0f);
        ImVec4 accentYellow = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        ImVec4 accentOrange = ImVec4(1.0f, 0.5f, 0.2f, 1.0f);
        ImVec4 accentPurple = ImVec4(0.8f, 0.2f, 1.0f, 1.0f);
        ImVec4 accentGreen = ImVec4(0.2f, 1.0f, 0.5f, 1.0f);
        ImVec4 accentAqua = ImVec4(0.2f, 1.0f, 0.8f, 1.0f);

        ImVec4 textPrimary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 textSecondary = ImVec4(0.7f, 0.7f, 0.72f, 1.0f);
        ImVec4 textDisabled = ImVec4(0.5f, 0.5f, 0.52f, 1.0f);
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
    // STYLING & COLOR SETUP
    // ============================================================================
    static void SetupColorScheme()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4* colors = style->Colors;

        // Windows & Backgrounds
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.12f, 0.98f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.05f, 0.1f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);

        // Borders - Vibrant Cyan
        colors[ImGuiCol_Border] = ImVec4(0.0f, 0.65f, 1.0f, 0.5f);
        colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.65f, 1.0f, 0.2f);

        // Title Bar
        colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.1f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.4f, 0.6f, 0.8f);

        // Tab
        colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.3f, 0.5f, 0.5f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.0f, 0.5f, 0.8f, 0.8f);
        colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.65f, 1.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.0f, 0.2f, 0.4f, 0.4f);

        // Headers - Bright Cyan
        colors[ImGuiCol_Header] = ImVec4(0.0f, 0.6f, 0.9f, 0.3f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.7f, 1.0f, 0.5f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);

        // Buttons - Neon Style
        colors[ImGuiCol_Button] = ImVec4(0.0f, 0.4f, 0.7f, 0.6f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.6f, 1.0f, 0.8f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);

        // Sliders
        colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.8f, 1.0f, 0.8f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2f, 1.0f, 1.0f, 1.0f);

        // Checkmark - Vivid Cyan
        colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.8f, 1.0f);

        // FrameBg
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.1f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.0f, 0.3f, 0.5f, 0.6f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.0f, 0.5f, 0.8f, 0.8f);

        // Text
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 1.0f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.7f, 1.0f);

        // Scrollbar - Neon
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.0f, 0.6f, 0.9f, 0.8f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.2f, 1.0f, 1.0f, 1.0f);

        // Rounding & Spacing
        style->WindowRounding = 12.0f;
        style->FrameRounding = 6.0f;
        style->PopupRounding = 8.0f;
        style->TabRounding = 6.0f;
        style->GrabRounding = 4.0f;

        style->WindowPadding = ImVec2(8, 8);
        style->FramePadding = ImVec2(6, 4);
        style->ItemSpacing = ImVec2(8, 3.0f);
        style->ItemInnerSpacing = ImVec2(4, 2);
        style->WindowBorderSize = 1.5f;
        style->FrameBorderSize = 1.0f;
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
            ImGui::SliderFloat("Reload Rate (Blue Flame)##blueflame", &g_Settings.ReloadAdjustRate_WearBlueFlame, 0.1f, 5.0f, "%.2f");
            
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

    static void RenderTrainingCharacterMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("TRAINING MODE - PLAYER SETUP");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentYellow);
        ImGui::Text("Configure Player Character");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Character ID selector (0=UNDEF, 1=Ch000, 2=Ch001, ... 44=Ch999, MAX=45)
        ImGui::SliderInt("Player Character##training", &g_Settings.TrainingPlayerCharacter, 0, 44, "CharID%03d");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Skill levels
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
        ImGui::Text("Unique Skill Levels");
        ImGui::PopStyleColor();
        
        ImGui::SliderInt("Unique 1 Level##p1", &g_Settings.TrainingPlayerUnique1, 0, 10, "%d");
        ImGui::SliderInt("Unique 2 Level##p2", &g_Settings.TrainingPlayerUnique2, 0, 10, "%d");
        ImGui::SliderInt("Unique 3 Level##p3", &g_Settings.TrainingPlayerUnique3, 0, 10, "%d");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Skill variation code
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentPurple);
        ImGui::Text("Skill Configuration");
        ImGui::PopStyleColor();
        
        ImGui::SliderInt("Skill Variation Code##pskill", &g_Settings.TrainingPlayerSkillCode, 0, 5, "%d");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Costume settings
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
        ImGui::Text("Costume Settings");
        ImGui::PopStyleColor();
        
        ImGui::SliderInt("Costume Code##pcostume", &g_Settings.TrainingPlayerCostumeCode, 0, 100, "%d");
        ImGui::SliderInt("Costume Aura Type##paura", &g_Settings.TrainingPlayerCostumeAuraType, 0, 5, "%d");

        ImGui::Spacing();
        ImGui::Spacing();

        // Apply button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        if (ImGui::Button("Apply Player Configuration", ImVec2(-1, 0)))
        {
            InGameHack_ApplyPlayerConfiguration(
                g_Settings.TrainingPlayerCharacter,
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
        if (ImGui::Button("Load Current Config", ImVec2(-1, 0)))
        {
            Logger::LogInfo("[Menu] Attempting to load current training player config...");
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Confirm and start training match
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
        ImGui::Text("Current Configuration:");
        ImGui::PopStyleColor();
        
        ImGui::Text("Character ID: Ch%03d", g_Settings.TrainingPlayerCharacter);
        ImGui::Text("Unique 1: Level %d", g_Settings.TrainingPlayerUnique1);
        ImGui::Text("Unique 2: Level %d", g_Settings.TrainingPlayerUnique2);
        ImGui::Text("Unique 3: Level %d", g_Settings.TrainingPlayerUnique3);
        ImGui::Text("Skill Code: %d", g_Settings.TrainingPlayerSkillCode);
    }

    // Forward declaration
    static void RenderAbilityHackMenu();

    static void RenderCombatSupportMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentCyan);
        ImGui::Text("COMBAT SUPPORT");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentYellow);
        ImGui::Text("Invincibility (Hardcoded: 10s-120s, All Effects Enabled)");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Invincibility Hotkey button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
        {
            std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.SetInvincibleKey.Keyboard, 0) + 
                                      " | [X] " + GetKeyName(g_Settings.SetInvincibleKey.Xbox, 1) + 
                                      " | [PS] " + GetKeyName(g_Settings.SetInvincibleKey.PS4, 1);
            if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
            {
                g_ListeningForHotkey = true;
                g_HotkeyListenStartTime = GetTickCount();
                g_CurrentHotkeyValue = 104;  // SetInvincible hotkey (unified)
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
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

        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentGreen);
        ImGui::Text("Recovery Options");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Recover Team button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.6f, 1.0f));
        if (ImGui::Button("Recover Team Now", ImVec2(-1, 0)))
        {
            if (InGameHack_RecoverDyingTeam())
            {
                Logger::LogInfo("[COMBAT] Team recovery executed!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to recover team");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Recover All ESP button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.8f, 0.2f, 1.0f));
        if (ImGui::Button("Recover All ESP Targets", ImVec2(-1, 0)))
        {
            if (InGameHack_RecoverDyingAllESP())
            {
                Logger::LogInfo("[COMBAT] All ESP targets recovery executed!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to recover ESP targets");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentOrange);
        ImGui::Text("Character Conditions");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        
        // Rebuild Myself button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.8f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.8f, 1.0f));
        {
            std::string hotkeyLabel = "[KB] " + GetKeyName(g_Settings.RebuildMyselfKey.Keyboard, 0) + 
                                      " | [X] " + GetKeyName(g_Settings.RebuildMyselfKey.Xbox, 1) + 
                                      " | [PS] " + GetKeyName(g_Settings.RebuildMyselfKey.PS4, 1);
            if (ImGui::Button(hotkeyLabel.c_str(), ImVec2(-1, 0)))
            {
                g_ListeningForHotkey = true;
                g_HotkeyListenStartTime = GetTickCount();
                g_CurrentHotkeyValue = 105;  // RebuildMyself hotkey (unified)
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // CH202 Trans Mission button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        if (ImGui::Button("DEKU MODE", ImVec2(-1, 0)))
        {
            if (InGameHack_CH202TransMission())
            {
                Logger::LogInfo("[COMBAT] CH202 Trans Mission executed!");
            }
            else
            {
                Logger::LogError("[COMBAT] Failed to execute CH202 Trans Mission");
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Unbreakable button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.6f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.8f, 1.0f));
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.4f, 1.0f));
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

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        RenderAbilityHackMenu();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Character change for all players
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentMagenta);
        ImGui::Text("APPLY CHARACTER TO ALL PLAYERS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::Spacing();

        // Character ID selector
        ImGui::SliderInt("Character ID##char_id", &g_HackSettings.CharacterId, 1, 44);
        ImGui::Spacing();

        // Unique skills sliders
        ImGui::SliderInt("Unique 1 Level##char_unique1", &g_HackSettings.CharacterUnique1, 1, 100);
        ImGui::SliderInt("Unique 2 Level##char_unique2", &g_HackSettings.CharacterUnique2, 1, 100);
        ImGui::SliderInt("Unique 3 Level##char_unique3", &g_HackSettings.CharacterUnique3, 1, 100);
        ImGui::Spacing();

        // Variation and cosmetics
        ImGui::SliderInt("Skill Code##char_skill", &g_HackSettings.CharacterSkillCode, 0, 5);
        ImGui::SliderInt("Costume Code##char_costume", &g_HackSettings.CharacterCostumeCode, 0, 100);
        ImGui::SliderInt("Costume Aura Type##char_aura", &g_HackSettings.CharacterCostumeAuraType, 0, 5);
        ImGui::Spacing();

        // Apply button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.9f, 1.0f));
        if (ImGui::Button("APPLY CHARACTER TO ALL PLAYERS", ImVec2(-1, 0)))
        {
            if (InGameHack_ApplyToAllControllers(
                g_HackSettings.CharacterId,
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

    static void RenderAbilityHackMenu()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.accentMagenta);
        ImGui::Text("ABILITY HACKS");
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::Spacing();

        // Ability Attack
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::SliderInt("Attack Level##ability_attack", &g_HackSettings.AbilityAttackLevel, 1, 100);
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
        // STEP 4: DRAW MAIN MENU WINDOW (only if visible)
        // ========================================================================
        if (g_Visible)
        {
            ImGui::SetNextWindowSize(ImVec2(550, 650), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(100, 50), ImGuiCond_FirstUseEver);

            if (ImGui::Begin("RUGIR-INTERNAL Cheat##menu", &g_Visible, 
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, g_Colors.primaryBlue);
                ImGui::Text("RUGIR - Internal Cheat Menu");
                ImGui::PopStyleColor();
                ImGui::Separator();

                // Tab Bar
                if (ImGui::BeginTabBar("##MenuTabs", ImGuiTabBarFlags_Reorderable))
                {
                    if (ImGui::BeginTabItem("ESP"))
                    {
                        RenderESPMenu();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("AIMBOT"))
                    {
                        RenderAimbotMenu();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("CHARACTER"))
                    {
                        RenderTrainingCharacterMenu();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("COMBAT"))
                    {
                        RenderCombatSupportMenu();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("HACKS"))
                    {
                        RenderHacksMenu();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("SETTINGS"))
                    {
                        RenderSettingsMenu();
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::Separator();
                
                // Unload button with warning
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));  // Red
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));  // Lighter red
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));   // Darker red
                
                if (ImGui::Button("UNLOAD DLL (Complete Cleanup)", ImVec2(ImGui::GetContentRegionAvail().x, 30)))
                {
                    Logger::LogWarning("[Menu] UNLOAD DLL button pressed - complete cleanup starting...");
                    DLL_Unload();
                }
                
                ImGui::PopStyleColor(3);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠️  Removes ALL hooks and unloads DLL");
                
                ImGui::Separator();
                ImGui::TextColored(g_Colors.textSecondary, "Press INSERT to toggle menu");
                ImGui::TextColored(g_Colors.accentCyan, "FPS: %.1f", ImGui::GetIO().Framerate);

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
