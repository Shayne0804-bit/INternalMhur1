#define _CRT_SECURE_NO_WARNINGS

#include "HackThread.h"

#include "InGameModuleHacks.h"
#include "Character_Changer.h"
#include "../Menu/ImGuiMenu.h"
#include "../Auth/LicenseAuth.h"
#include <algorithm>
#include <chrono>
#include <Windows.h>
#include <Xinput.h>

namespace
{
    constexpr int XINPUT_LT_HOTKEY = 0x1F00;
    constexpr int XINPUT_RT_HOTKEY = 0x2F00;
    constexpr int XINPUT_LEFT_STICK_UP_HOTKEY = 0x3001;
    constexpr int XINPUT_LEFT_STICK_DOWN_HOTKEY = 0x3002;
    constexpr int XINPUT_LEFT_STICK_LEFT_HOTKEY = 0x3003;
    constexpr int XINPUT_LEFT_STICK_RIGHT_HOTKEY = 0x3004;
    constexpr int XINPUT_RIGHT_STICK_UP_HOTKEY = 0x4001;
    constexpr int XINPUT_RIGHT_STICK_DOWN_HOTKEY = 0x4002;
    constexpr int XINPUT_RIGHT_STICK_LEFT_HOTKEY = 0x4003;
    constexpr int XINPUT_RIGHT_STICK_RIGHT_HOTKEY = 0x4004;
    constexpr BYTE XINPUT_TRIGGER_THRESHOLD = 128;
    constexpr SHORT XINPUT_STICK_THRESHOLD = 20000;
    constexpr int RECOVERY_SCAN_INTERVAL_MS = 250;
    constexpr int BULLET_TP_SCAN_INTERVAL_MS = 50;

    struct GamepadSnapshot
    {
        XINPUT_STATE states[4]{};
        bool connected[4]{};
    };

    bool IsKeyboardHotkeyPressed(int key)
    {
        if (key <= 0)
            return false;

        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    GamepadSnapshot PollGamepads()
    {
        GamepadSnapshot snapshot{};

        for (DWORD i = 0; i < 4; i++)
        {
            snapshot.connected[i] = (XInputGetState(i, &snapshot.states[i]) == ERROR_SUCCESS);
        }

        return snapshot;
    }

    bool IsGamepadHotkeyPressed(const GamepadSnapshot& snapshot, int key)
    {
        if (key <= 0)
            return false;

        for (DWORD i = 0; i < 4; i++)
        {
            if (!snapshot.connected[i])
                continue;

            const XINPUT_GAMEPAD& gamepad = snapshot.states[i].Gamepad;

            if (key == XINPUT_LT_HOTKEY)
            {
                if (gamepad.bLeftTrigger > XINPUT_TRIGGER_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_RT_HOTKEY)
            {
                if (gamepad.bRightTrigger > XINPUT_TRIGGER_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_LEFT_STICK_UP_HOTKEY)
            {
                if (gamepad.sThumbLY > XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_LEFT_STICK_DOWN_HOTKEY)
            {
                if (gamepad.sThumbLY < -XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_LEFT_STICK_LEFT_HOTKEY)
            {
                if (gamepad.sThumbLX < -XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_LEFT_STICK_RIGHT_HOTKEY)
            {
                if (gamepad.sThumbLX > XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_RIGHT_STICK_UP_HOTKEY)
            {
                if (gamepad.sThumbRY > XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_RIGHT_STICK_DOWN_HOTKEY)
            {
                if (gamepad.sThumbRY < -XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_RIGHT_STICK_LEFT_HOTKEY)
            {
                if (gamepad.sThumbRX < -XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if (key == XINPUT_RIGHT_STICK_RIGHT_HOTKEY)
            {
                if (gamepad.sThumbRX > XINPUT_STICK_THRESHOLD)
                    return true;
                continue;
            }

            if ((gamepad.wButtons & key) != 0)
                return true;
        }

        return false;
    }

    bool IsHotkeyPressed(const ImGuiMenu::HotkeySet& hotkey, const GamepadSnapshot& snapshot)
    {
        return IsKeyboardHotkeyPressed(hotkey.Keyboard) ||
            IsGamepadHotkeyPressed(snapshot, hotkey.Xbox) ||
            IsGamepadHotkeyPressed(snapshot, hotkey.PS4);
    }

    float NormalizeNoCollisionStickAxis(SHORT value)
    {
        constexpr float MaxPositive = 32767.0f;
        constexpr float MaxNegative = 32768.0f;

        if (value > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
            return static_cast<float>(value) / MaxPositive;

        if (value < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
            return static_cast<float>(value) / MaxNegative;

        return 0.0f;
    }

    void AddNoCollisionGamepadMovement(const GamepadSnapshot& snapshot, float& forwardAxis, float& rightAxis)
    {
        for (DWORD i = 0; i < 4; i++)
        {
            if (!snapshot.connected[i])
                continue;

            const XINPUT_GAMEPAD& gamepad = snapshot.states[i].Gamepad;
            forwardAxis += NormalizeNoCollisionStickAxis(gamepad.sThumbLY);
            rightAxis += NormalizeNoCollisionStickAxis(gamepad.sThumbLX);
        }
    }

    bool IsIntervalDue(std::chrono::steady_clock::time_point& lastRun, int intervalMs)
    {
        const auto now = std::chrono::steady_clock::now();
        if (lastRun.time_since_epoch().count() != 0)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRun).count();
            if (elapsed < intervalMs)
                return false;
        }

        lastRun = now;
        return true;
    }
}

// ============================================================================
// HACKTHREADMANAGER IMPLEMENTATION
// ============================================================================

HackThreadManager& HackThreadManager::GetInstance()
{
    static HackThreadManager instance;
    return instance;
}

HackThreadManager::HackThreadManager()
    : m_isRunning(false), m_shutdown(false), m_pendingTasks(0)
{
}

HackThreadManager::~HackThreadManager()
{
    Shutdown();
}

void HackThreadManager::Initialize()
{
    if (m_isRunning.load())
    {
return;
    }

    m_isRunning.store(true);
    m_shutdown.store(false);
    m_pendingTasks.store(0);

    // No auto-connect: the user must confirm activation in the menu each launch.

    try
    {
        m_workerThread = std::make_unique<std::thread>(&HackThreadManager::WorkerThreadProc, this);
}
    catch (const std::exception& e)
    {
m_isRunning.store(false);
    }
}

void HackThreadManager::Shutdown()
{
    if (!m_isRunning.load())
    {
        return;
    }
// Signal shutdown
    m_shutdown.store(true);
    m_taskAvailable.notify_one();

    // Wait for worker thread to finish
    if (m_workerThread && m_workerThread->joinable())
    {
        m_workerThread->join();
    }

    m_isRunning.store(false);
}

void HackThreadManager::EnqueueHack(std::function<void()> hackFunction)
{
    if (!m_isRunning.load())
    {
hackFunction();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.emplace(hackFunction);
        m_pendingTasks.fetch_add(1);
    }

    m_taskAvailable.notify_one();
}

void HackThreadManager::EnqueueHackWithDelay(std::function<void()> hackFunction, int delayMs)
{
    if (!m_isRunning.load())
    {
hackFunction();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.emplace(hackFunction, delayMs);
        m_pendingTasks.fetch_add(1);
    }

    m_taskAvailable.notify_one();
}

size_t HackThreadManager::GetPendingTaskCount()
{
    return m_pendingTasks.load();
}

void HackThreadManager::WaitForCompletion()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    
    // Wait until all tasks are processed
    m_tasksCompleted.wait(lock, [this]() {
        return m_taskQueue.empty() && m_pendingTasks.load() == 0;
    });
}

void HackThreadManager::WorkerThreadProc()
{
while (!m_shutdown.load())
    {
        HackTask task(nullptr);
        bool hasTask = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            // Wait for a task to be available or shutdown signal
            m_taskAvailable.wait(lock, [this]() {
                return !m_taskQueue.empty() || m_shutdown.load();
            });

            if (m_shutdown.load() && m_taskQueue.empty())
            {
                break;
            }

            if (!m_taskQueue.empty())
            {
                task = m_taskQueue.front();
                m_taskQueue.pop();
                hasTask = true;
            }
        }

        if (hasTask && task.func)
        {
            // Check if we need to delay execution
            auto now = std::chrono::steady_clock::now();
            if (task.executeTime > now)
            {
                auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(task.executeTime - now);
                std::this_thread::sleep_for(delay);
            }

            try
            {
                // Execute the hack function
                task.func();
            }
            catch (const std::exception& e)
            {
}
            catch (...)
            {
}

            // Decrement pending tasks and notify waiters
            m_pendingTasks.fetch_sub(1);
            m_tasksCompleted.notify_all();
        }
    }
}

// ============================================================================
// FRAME UPDATE HACKS - Called every frame from D3D11Hook
// ============================================================================
// All continuous hack logic is executed here

void HackThreadManager::FrameUpdateHacks()
{
    if (!m_isRunning.load())
        return;

    // License re-validation (self-throttled) + global gate: no valid key = no hacks.
    Auth::HeartbeatTick();
    if (!Auth::IsAuthorized())
        return;

    if (!ImGuiMenu::g_Settings.EnableGlobal)
        return;

    const bool needsHotkeyPolling =
        ImGuiMenu::g_Settings.EnableTransformIntoRandomESP ||
        ImGuiMenu::g_Settings.EnableDuplicateIntoImitationRandomESP ||
        ImGuiMenu::g_HackSettings.EnableInvincible ||
        ImGuiMenu::g_Settings.EnableRebuildMyself ||
        ImGuiMenu::g_Settings.EnableCopySkillsFromNearestEnemy ||
        ImGuiMenu::g_Settings.EnableGenerateProjectile ||
        ImGuiMenu::g_Settings.EnableNoCollision ||
        ImGuiMenu::g_Settings.EnableCustomDrop;
    const GamepadSnapshot gamepadSnapshot = needsHotkeyPolling ? PollGamepads() : GamepadSnapshot{};

    auto enqueueContinuousHack = [this](std::function<void()> hackFunction) -> bool
    {
        if (m_pendingTasks.load(std::memory_order_relaxed) != 0)
            return false;

        EnqueueHack(hackFunction);
        return true;
    };

    // ===== AUTO CLEAR CONDITIONS ON GAME MODE CHANGE =====
    // DISABLED: Function not yet implemented
    // try
    // {
    //     EnqueueHack([]() {
    //         InGameHack_AutoClearConditionOnModeChange();
    //     });
    // }
    // catch (...)
    // {
    //     Logger::LogWarning("[FrameUpdateHacks] Exception during auto clear conditions");
    // }

    // ===== TRANSFORM INTO RANDOM ESP TARGET =====
    if (ImGuiMenu::g_Settings.EnableTransformIntoRandomESP)
    {
        try
        {
            static bool lastTransformPressed = false;
            static auto lastTransformTime = std::chrono::high_resolution_clock::now();
            const int TRANSFORM_COOLDOWN_MS = 100;

            bool shouldTransform = IsHotkeyPressed(ImGuiMenu::g_Settings.TransformIntoRandomESPKey, gamepadSnapshot);
            
            if (shouldTransform && !lastTransformPressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTransformTime).count();
                
                if (elapsed >= TRANSFORM_COOLDOWN_MS)
                {
                    EnqueueHack([]() {
                        InGameHack_TransformIntoRandomESP();
                    });
                    lastTransformTime = now;
                }
            }
            
            lastTransformPressed = shouldTransform;
        }
        catch (...)
        {
}
    }

    // ===== CUSTOM DROP (catalog item + quantity) =====
    if (ImGuiMenu::g_Settings.EnableCustomDrop)
    {
        try
        {
            static bool lastCustomDropPressed = false;
            static auto lastCustomDropTime = std::chrono::high_resolution_clock::now();
            const int CUSTOM_DROP_COOLDOWN_MS = 100;

            bool shouldCustomDrop = IsHotkeyPressed(ImGuiMenu::g_Settings.CustomDropKey, gamepadSnapshot);

            if (shouldCustomDrop && !lastCustomDropPressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCustomDropTime).count();

                if (elapsed >= CUSTOM_DROP_COOLDOWN_MS)
                {
                    EnqueueHack([index = ImGuiMenu::g_Settings.CustomDropSelectedIndex, qty = ImGuiMenu::g_Settings.CustomDropQuantity]() {
                        InGameHack_DropCatalogItem(index, qty, false);
                    });
                    lastCustomDropTime = now;
                }
            }

            lastCustomDropPressed = shouldCustomDrop;
        }
        catch (...)
        {
}
    }

    // ===== DUPLICATE INTO IMITATION RANDOM ESP TARGET =====
    if (ImGuiMenu::g_Settings.EnableDuplicateIntoImitationRandomESP)
    {
        try
        {
            static bool lastDuplicateImitationPressed = false;
            static auto lastDuplicateImitationTime = std::chrono::high_resolution_clock::now();
            const int DUPLICATE_IMITATION_COOLDOWN_MS = 100;

            bool shouldDuplicateImitation = IsHotkeyPressed(ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey, gamepadSnapshot);
            
            if (shouldDuplicateImitation && !lastDuplicateImitationPressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDuplicateImitationTime).count();
                
                if (elapsed >= DUPLICATE_IMITATION_COOLDOWN_MS)
                {
                    EnqueueHack([lifeTime = ImGuiMenu::g_Settings.DuplicateImitationLifeTime, count = ImGuiMenu::g_Settings.DuplicateIntoImitationCount]() {
                        InGameHack_DuplicateIntoImitationRandomESP(0.0f, lifeTime, count);
                    });
                    lastDuplicateImitationTime = now;
                }
            }
            
            lastDuplicateImitationPressed = shouldDuplicateImitation;
        }
        catch (...)
        {
}
    }

    // ===== SET INVINCIBLE HOTKEY =====
    if (ImGuiMenu::g_HackSettings.EnableInvincible)
    {
        try
        {
            static auto lastSetInvincibleTime = std::chrono::high_resolution_clock::now();
            static bool lastSetInvinciblePressed = false;
            const int SET_INVINCIBLE_COOLDOWN_MS = 500;

            bool shouldSetInvincible = IsHotkeyPressed(ImGuiMenu::g_Settings.SetInvincibleKey, gamepadSnapshot);

            if (shouldSetInvincible && !lastSetInvinciblePressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSetInvincibleTime).count();

                if (elapsed >= SET_INVINCIBLE_COOLDOWN_MS)
                {
                    EnqueueHack([]() {
                        InGameHack_SetInvincible();
                    });
                    lastSetInvincibleTime = now;
                }
            }

            lastSetInvinciblePressed = shouldSetInvincible;
        }
        catch (...)
        {
        }
}

    // ===== REBUILD MYSELF HOTKEY =====
    if (ImGuiMenu::g_Settings.EnableRebuildMyself)
    {
        try
        {
            if (IsValidBattleMode())
            {
                static auto lastRebuildMyselfTime = std::chrono::high_resolution_clock::now();
                static bool lastRebuildMyselfPressed = false;
                const int REBUILD_MYSELF_COOLDOWN_MS = 200;

                bool shouldRebuildMyself = IsHotkeyPressed(ImGuiMenu::g_Settings.RebuildMyselfKey, gamepadSnapshot);

                if (shouldRebuildMyself && !lastRebuildMyselfPressed)
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRebuildMyselfTime).count();

                    if (elapsed >= REBUILD_MYSELF_COOLDOWN_MS)
                    {
                        EnqueueHack([]() {
                            InGameHack_RebuildMyself();
                        });
                        lastRebuildMyselfTime = now;
                    }
                }

                lastRebuildMyselfPressed = shouldRebuildMyself;
            }
        }
        catch (...)
        {
        }
    }

    // ===== COPY SKILLS FROM NEAREST ENEMY HOTKEY =====
    if (ImGuiMenu::g_Settings.EnableCopySkillsFromNearestEnemy)
    {
        try
        {
            static auto lastCopySkillsTime = std::chrono::high_resolution_clock::now();
            static bool lastCopySkillsPressed = false;
            const int COPY_SKILLS_COOLDOWN_MS = 500;

            bool shouldCopySkills = IsHotkeyPressed(ImGuiMenu::g_Settings.CopySkillsFromNearestEnemyKey, gamepadSnapshot);

            if (shouldCopySkills && !lastCopySkillsPressed)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCopySkillsTime).count();

                if (elapsed >= COPY_SKILLS_COOLDOWN_MS)
                {
                    EnqueueHack([bSetCopySkill = ImGuiMenu::g_Settings.CopySkillsSetCopySkill, 
                                bUseOwnerCharacterLevel = ImGuiMenu::g_Settings.CopySkillsUseOwnerCharacterLevel]() {
                        InGameHack_CopySkillsFromNearestEnemy(bSetCopySkill, bUseOwnerCharacterLevel);
                    });
                    lastCopySkillsTime = now;
                }
            }

            lastCopySkillsPressed = shouldCopySkills;
        }
        catch (...)
        {
}
    }

    // ===== GENERATE PROJECTILE HOTKEY =====
    if (ImGuiMenu::g_Settings.EnableGenerateProjectile)
    {
        try
        {
            static bool lastGenerateProjectilePressed = false;
            bool shouldGenerateProjectile = IsHotkeyPressed(ImGuiMenu::g_Settings.GenerateProjectileKey, gamepadSnapshot);

            // Edge-triggered: Only execute on press (not held)
            if (shouldGenerateProjectile && !lastGenerateProjectilePressed)
            {
                EnqueueHack([]() {
                    InGameHack_GenerateProjectileInFront();
                });
            }

            lastGenerateProjectilePressed = shouldGenerateProjectile;
        }
        catch (...)
        {
}
    }

    // ===== RECOVERY ME (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableRecoveryMe)
    {
        try
        {
            static auto lastRecoveryMeTime = std::chrono::steady_clock::time_point{};
            if (IsIntervalDue(lastRecoveryMeTime, RECOVERY_SCAN_INTERVAL_MS))
                InGameHack_RecoverMe();
        }
        catch (...)
        {
}
    }

    // ===== RECOVERY TEAM (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableRecoveryTeam)
    {
        try
        {
            static auto lastRecoveryTeamTime = std::chrono::steady_clock::time_point{};
            if (IsIntervalDue(lastRecoveryTeamTime, RECOVERY_SCAN_INTERVAL_MS))
                InGameHack_RecoverDyingTeam();
        }
        catch (...)
        {
}
    }

    // ===== RECOVERY ALL ESP (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableRecoveryAllESP)
    {
        try
        {
            static auto lastRecoveryAllEspTime = std::chrono::steady_clock::time_point{};
            if (IsIntervalDue(lastRecoveryAllEspTime, RECOVERY_SCAN_INTERVAL_MS))
                InGameHack_RecoverDyingAllESP();
        }
        catch (...)
        {
}
    }

    // ===== RECOVERY SPECIFIC TEAM MEMBER (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableRecoverySelectedTeam)
    {
        try
        {
            static auto lastRecoverySelectedTeamTime = std::chrono::steady_clock::time_point{};
            if (IsIntervalDue(lastRecoverySelectedTeamTime, RECOVERY_SCAN_INTERVAL_MS))
            {
                // Get the selected team member index
                int selectedIndex = ImGuiMenu::g_Settings.SelectedRecoveryTeamIndex;
                
                // Validate index
                if (selectedIndex >= 0 && selectedIndex < (int)ImGuiMenu::g_CurrentTeamCharacters.size())
                {
                    InGameHack_RecoverDyingSpecificTeamMember(selectedIndex);
                }
            }
        }
        catch (...)
        {
}
    }

    // ===== PLUS ULTRA (CONTINUOUS) =====
    {
        static bool s_plusUltraFastChargeWasEnabled = false;
        static auto lastPlusUltraTime = std::chrono::steady_clock::time_point{};
        try
        {
            const bool keepReady = ImGuiMenu::g_HackSettings.EnableInfinitePlusUltra;
            const bool fastCharge = ImGuiMenu::g_Settings.EnableFastPlusUltraCharge;

            if (!keepReady && !fastCharge)
            {
                if (s_plusUltraFastChargeWasEnabled)
                {
                    s_plusUltraFastChargeWasEnabled = false;
                    InGameHack_SetPlusUltraFastCharge(false);
                }
            }
            else if (IsIntervalDue(lastPlusUltraTime, 250))
            {
                s_plusUltraFastChargeWasEnabled = fastCharge || keepReady;
                if (keepReady)
                    InGameHack_KeepPlusUltraReady();
                if (fastCharge)
                    InGameHack_SetPlusUltraFastCharge(true);
            }
        }
        catch (...)
        {
        }
    }

    // ===== NO COLLISION MOVEMENT (EVERY FRAME) =====
    {
        static bool s_noCollisionWasEnabled = false;
        try
        {
            const bool noCollision = ImGuiMenu::g_Settings.EnableNoCollision;
            if (noCollision)
            {
                s_noCollisionWasEnabled = true;
                const bool holdActive = IsHotkeyPressed(ImGuiMenu::g_Settings.NoCollisionHoldKey, gamepadSnapshot);
                float forwardAxis = 0.0f;
                float rightAxis = 0.0f;
                if (IsKeyboardHotkeyPressed('W'))
                    forwardAxis += 1.0f;
                if (IsKeyboardHotkeyPressed('S'))
                    forwardAxis -= 1.0f;
                if (IsKeyboardHotkeyPressed('D'))
                    rightAxis += 1.0f;
                if (IsKeyboardHotkeyPressed('A'))
                    rightAxis -= 1.0f;
                AddNoCollisionGamepadMovement(gamepadSnapshot, forwardAxis, rightAxis);
                forwardAxis = std::clamp(forwardAxis, -1.0f, 1.0f);
                rightAxis = std::clamp(rightAxis, -1.0f, 1.0f);
                InGameHack_UpdateNoCollisionMovement(
                    holdActive,
                    forwardAxis,
                    rightAxis,
                    ImGuiMenu::g_Settings.NoCollisionSpeed);
            }
            else if (s_noCollisionWasEnabled)
            {
                s_noCollisionWasEnabled = false;
                InGameHack_SetNoCollision(false);
            }
        }
        catch (...)
        {
        }
    }

    // ===== BULLET REDIRECTION (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableBulletTP)
    {
        try
        {
            static auto lastBulletTpTime = std::chrono::steady_clock::time_point{};
            if (!ImGuiMenu::IsVisible() && IsIntervalDue(lastBulletTpTime, BULLET_TP_SCAN_INTERVAL_MS))
            {
                enqueueContinuousHack([alpha = ImGuiMenu::g_Settings.BulletTP_IncludeAlpha,
                                      beta = ImGuiMenu::g_Settings.BulletTP_IncludeBeta,
                                      gamma = ImGuiMenu::g_Settings.BulletTP_IncludeGamma,
                                      special = ImGuiMenu::g_Settings.BulletTP_IncludeSpecial]() {
                    InGameHack_RedirectBulletsToNearestEnemy(alpha, beta, gamma, special);
                });
            }
        }
        catch (...)
        {
}
    }

    // ===== BYPASS RENTAL TICKETS (EVERY FRAME) =====
    if (ImGuiMenu::g_HackSettings.Hack_BypassRentalTickets)
    {
        try
        {
            InGameHack_BypassRentalTickets();
        }
        catch (...)
        {
        }
    }

    // ===== CHARACTER CHANGER PERSISTENT MODE UPDATE =====
    try
    {
        // Handle persistent character change modes (teammates/enemies/all)
        Cheats::Tick();
    }
    catch (...)
    {
        // Silent fail - character change tick error
    }

}
