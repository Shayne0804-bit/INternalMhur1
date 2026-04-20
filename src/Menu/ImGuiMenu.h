#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <Windows.h>
#include <cstdint>

namespace ImGuiMenu
{
    // ============================================================================
    // SETTINGS STRUCTURE
    // ============================================================================
    struct MenuSettings
    {
        // Global
        bool EnableGlobal = true;
        bool EnableESP = true;

        // ESP - Display
        bool EnablePlayerESP = true;
        bool Player_Box = true;
        bool Player_Health = true;
        bool Player_Distance = true;
        bool Player_Name = true;
        bool ShowHP = true;         // Display health points
        bool ShowGP = true;         // Display guard points
        bool ShowPU = false;        // Display Plus Ultra (when available)
        bool ShowPlatform = true;   // Display player platform (PS/Xbox/PC)
        bool ShowTeamId = true;     // Display team ID in ESP text
        bool ShowPlayerSkeleton = false;  // Display skeleton bones (like zero1)

        // Aimbot
        bool EnableAimbot = false;
        bool AimbotSmoothing = true;
        float AimbotSmoothFactor = 0.3f;  // Default: 30% (faster targeting)
        bool AimbotDrawLine = true;  // Draw line from center to target
        bool AimbotDrawFOV = true;   // Draw FOV circle on screen
        float AimbotFOVRadius = 500.0f;  // FOV radius in pixels (adjustable)
        bool AimbotRequireHold = true;  // Require holding key (Zero1 exact - reduces detection)
        
        // Hotkeys (Keyboard/Mouse + Gamepad)
        int AimbotHoldKey = 0x02;  // VK_RBUTTON (right mouse button) - hold to activate
        int AimbotHoldKey_Xbox = 0x0100;  // XINPUT_GAMEPAD_LB (Left Shoulder) - Zero1 exact
        int AimbotHoldKey_PS4 = 0x0100;   // XINPUT_GAMEPAD_LB (L1) - Zero1 exact
        
        // Teleport to Kota
        bool EnableTeleportToKota = false;
        int TeleportToKotaKey = 0x50;  // VK_P (P key)
        int TeleportToKotaKey_Xbox = 0x0080;  // XINPUT_GAMEPAD_RB (Right Shoulder)
        int TeleportToKotaKey_PS4 = 0x0080;   // XINPUT_GAMEPAD_RB (R1)
        
        // Teleport Items (Level Up Cards)
        bool EnableTeleportLevelUpCards = false;
        
        // Duplicate Player
        bool EnableDuplicate = false;
        int DuplicateKey = 0x4F;  // VK_O (O key)
        int DuplicateKey_Xbox = 0x0040;  // XINPUT_GAMEPAD_Y
        int DuplicateKey_PS4 = 0x0040;   // XINPUT_GAMEPAD_Y (Triangle)
        
        // BulletTP (Silent Aim) - Alpha Skills Only - Uses same settings as Aimbot
        bool EnableBulletTP = false;

        // ESP - Filters
        bool ShowEnemies = true;
        bool ShowTeam = false;
        bool ShowLobbyCharacters = true;
        bool ShowKota = true;          // Display KOTA (special NPC)
        float Player_DrawDistance = 5000.0f;
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
}
