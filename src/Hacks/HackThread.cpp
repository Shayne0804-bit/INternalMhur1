#define _CRT_SECURE_NO_WARNINGS

#include "HackThread.h"

#include "InGameModuleHacks.h"
#include "../Menu/ImGuiMenu.h"
#include <algorithm>
#include <chrono>
#include <Windows.h>
#include <Xinput.h>

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

            bool shouldTransform = false;
            
            // Check keyboard hotkey
            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.TransformIntoRandomESPKey.Keyboard) & 0x8000) != 0)
            {
                shouldTransform = true;
            }
            
            // Check gamepad hotkeys
            if (!shouldTransform)
            {
                XINPUT_STATE xInputState = {};
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.TransformIntoRandomESPKey.Xbox) != 0)
                    {
                        shouldTransform = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.TransformIntoRandomESPKey.PS4) != 0)
                    {
                        shouldTransform = true;
                    }
                }
            }
            
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

    // ===== DUPLICATE INTO IMITATION RANDOM ESP TARGET =====
    if (ImGuiMenu::g_Settings.EnableDuplicateIntoImitationRandomESP)
    {
        try
        {
            static bool lastDuplicateImitationPressed = false;
            static auto lastDuplicateImitationTime = std::chrono::high_resolution_clock::now();
            const int DUPLICATE_IMITATION_COOLDOWN_MS = 100;

            bool shouldDuplicateImitation = false;
            
            // Check keyboard hotkey
            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.Keyboard) & 0x8000) != 0)
            {
                shouldDuplicateImitation = true;
            }
            
            // Check gamepad hotkeys
            if (!shouldDuplicateImitation)
            {
                XINPUT_STATE xInputState = {};
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.Xbox) != 0)
                    {
                        shouldDuplicateImitation = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.DuplicateIntoImitationRandomESPKey.PS4) != 0)
                    {
                        shouldDuplicateImitation = true;
                    }
                }
            }
            
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
    try
    {
        static auto lastSetInvincibleTime = std::chrono::high_resolution_clock::now();
        static bool lastSetInvinciblePressed = false;
        const int SET_INVINCIBLE_COOLDOWN_MS = 500;

        bool shouldSetInvincible = false;

        if ((GetAsyncKeyState(ImGuiMenu::g_Settings.SetInvincibleKey.Keyboard) & 0x8000) != 0)
        {
            shouldSetInvincible = true;
        }

        if (!shouldSetInvincible)
        {
            XINPUT_STATE xInputState = {};
            if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
            {
                if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.SetInvincibleKey.Xbox) != 0)
                {
                    shouldSetInvincible = true;
                }
                else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.SetInvincibleKey.PS4) != 0)
                {
                    shouldSetInvincible = true;
                }
            }
        }

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

    // ===== REBUILD MYSELF HOTKEY =====
    try
    {
        if (IsValidBattleMode())
        {
            static auto lastRebuildMyselfTime = std::chrono::high_resolution_clock::now();
            static bool lastRebuildMyselfPressed = false;
            const int REBUILD_MYSELF_COOLDOWN_MS = 200;

            bool shouldRebuildMyself = false;

            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.RebuildMyselfKey.Keyboard) & 0x8000) != 0)
            {
                shouldRebuildMyself = true;
            }

            if (!shouldRebuildMyself)
            {
                XINPUT_STATE xInputState = {};
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.RebuildMyselfKey.Xbox) != 0)
                    {
                        shouldRebuildMyself = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.RebuildMyselfKey.PS4) != 0)
                    {
                        shouldRebuildMyself = true;
                    }
                }
            }

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

    // ===== COPY SKILLS FROM NEAREST ENEMY HOTKEY =====
    if (ImGuiMenu::g_Settings.EnableCopySkillsFromNearestEnemy)
    {
        try
        {
            static auto lastCopySkillsTime = std::chrono::high_resolution_clock::now();
            static bool lastCopySkillsPressed = false;
            const int COPY_SKILLS_COOLDOWN_MS = 500;

            bool shouldCopySkills = false;

            if ((GetAsyncKeyState(ImGuiMenu::g_Settings.CopySkillsFromNearestEnemyKey.Keyboard) & 0x8000) != 0)
            {
                shouldCopySkills = true;
            }

            if (!shouldCopySkills)
            {
                XINPUT_STATE xInputState = {};
                if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
                {
                    if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.CopySkillsFromNearestEnemyKey.Xbox) != 0)
                    {
                        shouldCopySkills = true;
                    }
                    else if ((xInputState.Gamepad.wButtons & ImGuiMenu::g_Settings.CopySkillsFromNearestEnemyKey.PS4) != 0)
                    {
                        shouldCopySkills = true;
                    }
                }
            }

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

    // ===== RECOVERY ME (EVERY FRAME) =====
    if (ImGuiMenu::g_Settings.EnableRecoveryMe)
    {
        try
        {
            EnqueueHack([]() {
                InGameHack_RecoverMe();
            });
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
            EnqueueHack([]() {
                InGameHack_RecoverDyingTeam();
            });
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
            EnqueueHack([]() {
                InGameHack_RecoverDyingAllESP();
            });
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
            // Get the selected team member index
            int selectedIndex = ImGuiMenu::g_Settings.SelectedRecoveryTeamIndex;
            
            // Validate index
            if (selectedIndex >= 0 && selectedIndex < (int)ImGuiMenu::g_CurrentTeamCharacters.size())
            {
                EnqueueHack([selectedIndex]() {
                    InGameHack_RecoverDyingSpecificTeamMember(selectedIndex);
                });
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
            // Always redirect bullets every frame (no hotkey required)
            EnqueueHack([alpha = ImGuiMenu::g_Settings.BulletTP_IncludeAlpha,
                        beta = ImGuiMenu::g_Settings.BulletTP_IncludeBeta,
                        gamma = ImGuiMenu::g_Settings.BulletTP_IncludeGamma,
                        special = ImGuiMenu::g_Settings.BulletTP_IncludeSpecial]() {
                InGameHack_RedirectBulletsToNearestEnemy(alpha, beta, gamma, special);
            });
        }
        catch (...)
        {
}
    }

    // ===== CH101 ROLLSLOT LAUNCH (EVERY FRAME - TEST) =====
    try
    {
        EnqueueHack([]() {
            InGameHack_LaunchCh101RollSlotSkill();
        });
    }
    catch (...)
    {
}

}
