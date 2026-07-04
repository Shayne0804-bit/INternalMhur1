#define _CRT_SECURE_NO_WARNINGS

#include "HackThread.h"

#include "InGameModuleHacks.h"
#include "Character_Changer.h"
#include "../Menu/ImGuiMenu.h"
#include "../Utils/Logger.h"
#include "../Auth/LicenseAuth.h"
#include <algorithm>
#include <chrono>
#include <string>
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

    int HandleAccessViolation(_EXCEPTION_POINTERS* exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
        return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_CONTINUE_SEARCH;
    }

    bool IsValidBattleModeNoThrow()
    {
        __try
        {
            return IsValidBattleMode();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    void ExecuteHackTaskNoThrow(const std::function<void()>& task)
    {
        __try
        {
            if (task)
                task();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
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

    // Re-activate from a previously saved (DPAPI) key so the user does not have
    // to re-enter it every launch. No-op if no key stored or it is rejected.
    Auth::LoadSavedKeyAndActivate();

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
        ExecuteHackTaskNoThrow(hackFunction);
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
        ExecuteHackTaskNoThrow(hackFunction);
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

            ExecuteHackTaskNoThrow(task.func);

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
    __try
    {
        FrameUpdateHacksImpl();
    }
    __except (HandleAccessViolation(GetExceptionInformation()))
    {
    }
}

void HackThreadManager::FrameUpdateHacksImpl()
{
    if (!m_isRunning.load())
        return;

    // License re-validation (self-throttled). Runs every frame but only fires a
    // request every few minutes; catches server-side revocation/expiry.
    Auth::HeartbeatTick();

    // Global license gate: no valid key = no hacks, no hotkeys. Everything below
    // this point (including the hotkey polling) is bound to an authorized session.
    if (!Auth::IsAuthorized())
        return;

    if (!ImGuiMenu::g_Settings.EnableGlobal)
        return;

    const bool isBattleMode = IsValidBattleModeNoThrow();

    const bool needsHotkeyPolling =
        ImGuiMenu::g_Settings.EnableTransformIntoRandomESP ||
        ImGuiMenu::g_Settings.EnableDuplicateIntoImitationRandomESP ||
        ImGuiMenu::g_HackSettings.EnableInvincible ||
        ImGuiMenu::g_Settings.EnableRebuildMyself ||
        ImGuiMenu::g_Settings.EnableCopySkillsFromNearestEnemy ||
        ImGuiMenu::g_Settings.EnableGenerateProjectile ||
        ImGuiMenu::g_Settings.EnableNoCollision ||
        ImGuiMenu::g_Settings.EnableCustomDrop ||
        ImGuiMenu::g_Settings.CustomDropKey.Xbox > 0 ||
        ImGuiMenu::g_Settings.CustomDropKey.PS4 > 0;
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
                    InGameHack_DropCatalogItem(
                        ImGuiMenu::g_Settings.CustomDropSelectedIndex,
                        ImGuiMenu::g_Settings.CustomDropQuantity,
                        ImGuiMenu::g_Settings.CustomDropSerialId,
                        false);
                    lastCustomDropTime = now;
                }
            }

            lastCustomDropPressed = shouldCustomDrop;
        }
        catch (...)
        {
}
    }

    try
    {
        static auto lastCustomDropPlacementTime = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCustomDropPlacementTime).count();
        if (elapsed >= 16 && InGameHack_HasPendingCustomDropPlacement())
        {
            if (enqueueContinuousHack([]() {
                InGameHack_UpdateCustomDropPlacement();
            }))
            {
                lastCustomDropPlacementTime = now;
            }
        }
    }
    catch (...)
    {
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
            if (isBattleMode)
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

    // ===== CLEAR INVINCIBLE (AUTO) =====
    if (ImGuiMenu::g_Settings.EnableClearInvincibleAuto)
    {
        try
        {
            static auto lastClearInvincibleTime = std::chrono::steady_clock::time_point{};
            const int intervalMs = std::clamp(ImGuiMenu::g_Settings.ClearInvincibleIntervalMs, 50, 2000);
            if (isBattleMode && IsIntervalDue(lastClearInvincibleTime, intervalMs))
            {
                const int targetMode = std::clamp(ImGuiMenu::g_Settings.ClearInvincibleTargetMode, 0, 3);
                const int method = std::clamp(ImGuiMenu::g_Settings.ClearInvincibleMethod, 0, 2);
                const bool ignoreFixed = ImGuiMenu::g_Settings.ClearInvincibleIgnoreFixed;
                const int attackId = std::clamp(ImGuiMenu::g_Settings.ClearInvincibleAttackId, 0, 8);
                const int selectedIndex = ImGuiMenu::g_Settings.ClearInvincibleSelectedCharacterIndex;
                const std::string tag = ImGuiMenu::g_Settings.ClearInvincibleTagBuffer;

                enqueueContinuousHack([targetMode, method, ignoreFixed, attackId, selectedIndex, tag]() {
                    InGameHack_ClearInvincibleTargets(
                        targetMode,
                        method,
                        ignoreFixed,
                        attackId,
                        tag.c_str(),
                        selectedIndex);
                });
            }
        }
        catch (...)
        {
        }
    }

    // ===== ACTION CANCEL / ATTACK CHAIN (AUTO) =====
    if (ImGuiMenu::g_Settings.EnableAttackChainAuto)
    {
        try
        {
            static auto lastAttackChainTime = std::chrono::steady_clock::time_point{};
            const int intervalMs = std::clamp(ImGuiMenu::g_Settings.AttackChainIntervalMs, 16, 1000);
            if (isBattleMode && IsIntervalDue(lastAttackChainTime, intervalMs))
            {
                AttackChainConfig config{};
                config.useChainComboFlag = ImGuiMenu::g_Settings.AttackChainUseChainComboFlag;
                config.chainComboTime = std::clamp(ImGuiMenu::g_Settings.AttackChainComboFlagTime, 0.0f, 5.0f);
                config.enableShiftAttackActions = ImGuiMenu::g_Settings.AttackChainEnableShiftAttackActions;
                config.clearShiftActionAttackFlags = ImGuiMenu::g_Settings.AttackChainClearShiftActionAttackFlags;
                config.useAnimationRate = ImGuiMenu::g_Settings.AttackChainUseAnimationRate;
                config.animationRate = std::clamp(ImGuiMenu::g_Settings.AttackChainAnimationRate, 0.1f, 30.0f);
                config.animationRateNagara = ImGuiMenu::g_Settings.AttackChainAnimationRateNagara;
                config.usePhaseEndCondition = ImGuiMenu::g_Settings.AttackChainUsePhaseEndCondition;
                config.comboCommand = ImGuiMenu::g_Settings.AttackChainComboCommand;
                config.grabbed = ImGuiMenu::g_Settings.AttackChainGrabbed;
                config.endTimer = std::clamp(ImGuiMenu::g_Settings.AttackChainEndTimer, 0.0f, 10.0f);
                config.landing = ImGuiMenu::g_Settings.AttackChainLanding;
                config.endAnim = ImGuiMenu::g_Settings.AttackChainEndAnim;
                config.endAnimSlot = std::clamp(ImGuiMenu::g_Settings.AttackChainEndAnimSlot, 0, 6);
                config.finishCurrentPhase = ImGuiMenu::g_Settings.AttackChainFinishCurrentPhase;
                config.terminateAttackLayer = ImGuiMenu::g_Settings.AttackChainTerminateAttackLayer;
                config.enableAimingActionCancel = ImGuiMenu::g_Settings.AttackChainEnableAimingActionCancel;
                config.actionCancelFlag = std::clamp(ImGuiMenu::g_Settings.AttackChainActionCancelFlag, 0, 255);
                config.ownerActionOnly = ImGuiMenu::g_Settings.AttackChainOwnerActionOnly;
                config.validateNextReservedAction = ImGuiMenu::g_Settings.AttackChainValidateNextReservedAction;

                enqueueContinuousHack([config]() {
                    InGameHack_ApplyAttackChainControl(config);
                });
            }
        }
        catch (...)
        {
        }
    }

    // ===== DOWNPOWER (AUTO) =====
    if (ImGuiMenu::g_Settings.EnableDownPowerAuto)
    {
        try
        {
            static auto lastDownPowerTime = std::chrono::steady_clock::time_point{};
            const int intervalMs = std::clamp(ImGuiMenu::g_Settings.DownPowerIntervalMs, 50, 2000);
            if (isBattleMode && IsIntervalDue(lastDownPowerTime, intervalMs))
            {
                DownPowerConfig config{};
                config.patchDamageParams = ImGuiMenu::g_Settings.DownPowerPatchDamageParams;
                config.damageType = ImGuiMenu::g_Settings.DownPowerDamageType;
                config.includeNoActionDamage = ImGuiMenu::g_Settings.DownPowerIncludeNoActionDamage;
                config.overrideRecoverSpan = ImGuiMenu::g_Settings.DownPowerOverrideRecoverSpan;
                config.recoverSpan = std::clamp(ImGuiMenu::g_Settings.DownPowerRecoverSpan, 0.0f, 30.0f);
                config.applyDurableRates = ImGuiMenu::g_Settings.DownPowerApplyDurableRates;
                config.durableRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableRate, 0.0f, 100.0f);
                config.durableAttackActionRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableAttackActionRate, 0.0f, 100.0f);
                config.durableTakeDamageRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableTakeDamageRate, 0.0f, 100.0f);
                config.durableSpecialActionRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableSpecialActionRate, 0.0f, 100.0f);
                config.durableRollSlotRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableRollSlotRate, 0.0f, 100.0f);
                config.durableTakeDamageRollSlotRate = std::clamp(ImGuiMenu::g_Settings.DownPowerDurableTakeDamageRollSlotRate, 0.0f, 100.0f);
                config.applyBreakDownSuperArmorRate = ImGuiMenu::g_Settings.DownPowerApplyBreakDownSuperArmorRate;
                config.breakDownSuperArmorRate = std::clamp(ImGuiMenu::g_Settings.DownPowerBreakDownSuperArmorRate, 0.0f, 100.0f);
                config.disableTargetSuperArmor = ImGuiMenu::g_Settings.DownPowerDisableTargetSuperArmor;
                config.targetMode = std::clamp(ImGuiMenu::g_Settings.DownPowerTargetMode, 0, 3);
                config.selectedCharacterIndex = ImGuiMenu::g_Settings.DownPowerSelectedCharacterIndex;

                enqueueContinuousHack([config]() {
                    InGameHack_ApplyDownPowerConfig(config);
                });
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

    // ===== CAMERA FOV (SDK) =====
    if (ImGuiMenu::g_Settings.EnableCameraFOV)
    {
        try
        {
            static auto lastCameraFovTime = std::chrono::steady_clock::time_point{};
            if (IsIntervalDue(lastCameraFovTime, 50))
                InGameHack_SetCameraFOV(ImGuiMenu::g_Settings.CameraFOV);
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
