#include "GameThreadHook.h"

#include "../Core/UnloadManager.h"
#include "../Hacks/Character_Changer.h"
#include "../Hacks/HackThread.h"
#include "../Hacks/InGameModuleHacks.h"
#include "../Menu/ImGuiMenu.h"
#include "../SDK/SDKInit.h"
#include "../Utils/Logger.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

extern "C" SDK::APlayerController* SDK_GetPlayerController();
extern "C" void SDK_RunTeleportToKota();

namespace GameThreadHook
{
    using ProcessEventFn = void(*)(const SDK::UObject*, SDK::UFunction*, void*);

    struct HookRecord
    {
        SDK::UObject* object = nullptr;
        uintptr_t* originalVTable = nullptr;
        uintptr_t* shadowVTable = nullptr;
        int vtableSize = 0;
        ProcessEventFn originalProcessEvent = nullptr;
        bool isAttackComponent = false;
    };

    static std::mutex g_HookMutex;
    static std::vector<HookRecord> g_Hooks;
    static std::atomic<bool> g_Initialized(false);
    static std::atomic<int> g_ProcessEventCallDepth(0);
    static std::atomic<DWORD> g_GameThreadId(0); // captured from first attack-component PE (always game thread)
    static DWORD g_LastRefreshTick = 0;
    static constexpr DWORD REFRESH_INTERVAL_MS = 1000;
    static constexpr int PROCESS_EVENT_INDEX = SDK::Offsets::ProcessEventIdx;
    static constexpr int MAX_VTABLE_SCAN = 512;
    static constexpr int MIN_REASONABLE_VTABLE_SIZE = PROCESS_EVENT_INDEX + 1;
    static constexpr size_t MAX_HOOKED_OBJECTS = 24;

    static std::mutex g_TaskMutex;
    static std::queue<std::function<void()>> g_GameThreadTasks;
    static constexpr size_t MAX_GAME_THREAD_TASKS = 64;
    static constexpr int MAX_TASKS_PER_EVENT = 4;
    static thread_local bool g_IsPumpingTasks = false;
    static std::atomic<uint64_t> g_ProcessEventHits(0);
    static std::atomic<uint64_t> g_TasksExecuted(0);
    static std::atomic<uint64_t> g_TasksFailed(0);

    static int HandleAccessViolation(_EXCEPTION_POINTERS* exceptionInfo)
    {
        if (!exceptionInfo || !exceptionInfo->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
        return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_CONTINUE_SEARCH;
    }

    static bool IsValidBattleModeNoThrow(const char* context)
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

    static void TickInfiniteObjectsNoThrow()
    {
        __try
        {
            InGameHack_TickInfiniteObjectsSDK();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void ResetInfiniteObjectsNoThrow()
    {
        __try
        {
            InGameHack_ResetInfiniteObjectsSDK();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void FrameUpdateHacksNoThrow()
    {
        __try
        {
            HackThreadManager::GetInstance().FrameUpdateHacks();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void SetInitTransMissionLevelNoThrow(int levelIndex, int value)
    {
        __try
        {
            InGameHack_SetInitTransMissionLevel(levelIndex, value);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void SetReloadAdjustRateNoThrow(float value)
    {
        __try
        {
            InGameHack_SetReloadAdjustRate(value);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void SetReloadAdjustRateRollSlotNoThrow(float value)
    {
        __try
        {
            InGameHack_SetReloadAdjustRate_RollSlot(value);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void SetReloadAdjustRateWearBlueFlameNoThrow(float value)
    {
        __try
        {
            InGameHack_SetReloadAdjustRate_WearBlueFlame(value);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void RunTeleportToKotaNoThrow()
    {
        __try
        {
            SDK_RunTeleportToKota();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static std::atomic<uint64_t> g_FrameUpdates(0);
    static DWORD g_LastProcessEventLogTick = 0;
    static DWORD g_LastFrameUpdateTick = 0;
    static DWORD g_LastFrameUpdateLogTick = 0;
    static DWORD g_LastInfiniteObjectsPatchSyncTick = 0;
    static DWORD g_LastReloadAdjustSyncTick = 0;
    static bool g_LastInfiniteObjectsPatchTarget = false;
    static bool g_HasInfiniteObjectsPatchTarget = false;
    static thread_local bool g_IsRunningFrameUpdate = false;
    static constexpr DWORD GAME_FRAME_UPDATE_INTERVAL_MS = 16;
    static constexpr DWORD INFINITE_OBJECTS_RESYNC_MS = 1000;
    static constexpr DWORD RELOAD_ADJUST_SYNC_MS = 500;

    static void SyncCH202InitTransNoThrow()
    {
        __try
        {
            if (ImGuiMenu::g_Settings.EnableCH202InitTransLevel5 &&
                IsValidBattleModeNoThrow("IsValidBattleMode:CH202InitTrans"))
            {
                SetInitTransMissionLevelNoThrow(0, 5);
                ImGuiMenu::g_Settings.EnableCH202InitTransLevel5 = false;
            }
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void SyncReloadAdjustNoThrow(DWORD now)
    {
        __try
        {
            const bool needsReloadAdjust =
                ImGuiMenu::g_Settings.ReloadAdjustRate != 1.0f ||
                ImGuiMenu::g_Settings.ReloadAdjustRate_RollSlot != 1.0f ||
                ImGuiMenu::g_Settings.ReloadAdjustRate_WearBlueFlame != 1.0f;

            const bool reloadAdjustDue =
                g_LastReloadAdjustSyncTick == 0 ||
                now - g_LastReloadAdjustSyncTick >= RELOAD_ADJUST_SYNC_MS;

            if (needsReloadAdjust &&
                reloadAdjustDue &&
                IsValidBattleModeNoThrow("IsValidBattleMode:ReloadAdjust"))
            {
                g_LastReloadAdjustSyncTick = now;

                if (ImGuiMenu::g_Settings.ReloadAdjustRate != 1.0f)
                    SetReloadAdjustRateNoThrow(ImGuiMenu::g_Settings.ReloadAdjustRate);

                if (ImGuiMenu::g_Settings.ReloadAdjustRate_RollSlot != 1.0f)
                    SetReloadAdjustRateRollSlotNoThrow(ImGuiMenu::g_Settings.ReloadAdjustRate_RollSlot);

                if (ImGuiMenu::g_Settings.ReloadAdjustRate_WearBlueFlame != 1.0f)
                    SetReloadAdjustRateWearBlueFlameNoThrow(ImGuiMenu::g_Settings.ReloadAdjustRate_WearBlueFlame);
            }
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    void Log(const char* format, ...)
    {
        char message[1024] = {};

        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        Logger::LogRaw(message);

        if (!GetConsoleWindow())
            return;

        const DWORD now = GetTickCount();
        const DWORD threadId = GetCurrentThreadId();
        std::printf("[%08lu][T:%lu] %s\n", now, threadId, message);
        std::fflush(stdout);
    }

    static bool IsReadable(const void* ptr, size_t size)
    {
        if (!ptr || reinterpret_cast<uintptr_t>(ptr) < 0x10000)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
            return false;

        const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t end = start + size;
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return end >= start && end <= regionEnd;
    }

    static bool IsWritableObjectPointer(SDK::UObject* object)
    {
        if (!IsReadable(object, sizeof(void*) * 3))
            return false;

        SDK::UClass* klass = nullptr;
        __try
        {
            klass = object->Class;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }

        return IsReadable(klass, sizeof(void*));
    }

    static int GetVTableSize(uintptr_t* vtable)
    {
        if (!IsReadable(vtable, sizeof(uintptr_t) * MIN_REASONABLE_VTABLE_SIZE))
            return -1;

        int count = 0;
        for (int i = 0; i < MAX_VTABLE_SCAN; ++i)
        {
            uintptr_t entry = 0;
            __try
            {
                entry = vtable[i];
            }
            __except (HandleAccessViolation(GetExceptionInformation()))
            {
                break;
            }

            if (!IsReadable(reinterpret_cast<void*>(entry), 1))
                break;

            ++count;
        }

        return count >= MIN_REASONABLE_VTABLE_SIZE ? count : -1;
    }

    static bool IsAlreadyHookedLocked(SDK::UObject* object)
    {
        return std::any_of(g_Hooks.begin(), g_Hooks.end(), [object](const HookRecord& hook) {
            return hook.object == object;
        });
    }

    static bool IsHookRecordActiveLocked(const HookRecord& hook)
    {
        if (!hook.object || !hook.originalVTable || !hook.shadowVTable)
            return false;

        __try
        {
            uintptr_t** objectVTable = reinterpret_cast<uintptr_t**>(hook.object);
            return IsReadable(objectVTable, sizeof(uintptr_t*)) && *objectVTable == hook.shadowVTable;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    static bool RestoreHookLocked(const HookRecord& hook)
    {
        if (!hook.object || !hook.originalVTable || !hook.shadowVTable)
            return false;

        __try
        {
            uintptr_t** objectVTable = reinterpret_cast<uintptr_t**>(hook.object);
            if (IsReadable(objectVTable, sizeof(uintptr_t*)) && *objectVTable == hook.shadowVTable)
                *objectVTable = hook.originalVTable;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }

        return true;
    }

    static void NeutralizeHookLocked(const HookRecord& hook)
    {
        if (hook.shadowVTable && hook.originalProcessEvent)
            hook.shadowVTable[PROCESS_EVENT_INDEX] = reinterpret_cast<uintptr_t>(hook.originalProcessEvent);

        RestoreHookLocked(hook);
    }

    static void ClearTasks()
    {
        std::lock_guard<std::mutex> lock(g_TaskMutex);
        const size_t oldSize = g_GameThreadTasks.size();
        std::queue<std::function<void()>> empty;
        g_GameThreadTasks.swap(empty);
        if (oldSize > 0)
            Log("[GameThreadHook] cleared %zu queued task(s)", oldSize);
    }

    static size_t GetQueuedTaskCount()
    {
        std::lock_guard<std::mutex> lock(g_TaskMutex);
        return g_GameThreadTasks.size();
    }

    static bool ExecuteTaskNoThrow(std::function<void()>* task)
    {
        __try
        {
            if (task && *task)
                (*task)();
            return true;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    static bool InitializeSdkNoThrow()
    {
        __try
        {
            return SDKInit::Initialize();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }
    }

    static void ShutdownSdkNoThrow()
    {
        __try
        {
            SDKInit::Shutdown();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void CallOriginalProcessEventNoThrow(ProcessEventFn original, const SDK::UObject* object, SDK::UFunction* function, void* params)
    {
        if (!original)
            return;

        __try
        {
            original(object, function, params);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static void PumpTasks()
    {
        if (g_IsPumpingTasks || !g_Initialized.load(std::memory_order_acquire) || UnloadManager::IsUnloadRequested())
            return;

        struct PumpScope
        {
            PumpScope() { g_IsPumpingTasks = true; }
            ~PumpScope() { g_IsPumpingTasks = false; }
        } pumpScope;

        for (int i = 0; i < MAX_TASKS_PER_EVENT; ++i)
        {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lock(g_TaskMutex);
                if (g_GameThreadTasks.empty())
                    break;

                task = std::move(g_GameThreadTasks.front());
                g_GameThreadTasks.pop();
            }

            if (task && ExecuteTaskNoThrow(&task))
            {
                const uint64_t executed = g_TasksExecuted.fetch_add(1, std::memory_order_relaxed) + 1;
                Log("[GameThreadHook] executed queued task #%llu", static_cast<unsigned long long>(executed));
            }
            else if (task)
            {
                const uint64_t failed = g_TasksFailed.fetch_add(1, std::memory_order_relaxed) + 1;
                Log("[GameThreadHook] queued task crashed, failed=%llu", static_cast<unsigned long long>(failed));
                Log("[GameThreadHook] queued task crashed");
            }
        }
    }

    static void DeadSwapTickNoThrow()
    {
        __try
        {
            if (IsValidBattleModeNoThrow("IsValidBattleMode:DeadSwap"))
                Cheats::DeadSwap_Tick(
                    ImGuiMenu::g_HackSettings.DeadSwapSelf,
                    ImGuiMenu::g_HackSettings.DeadSwapTeam,
                    ImGuiMenu::g_HackSettings.DeadSwapEveryone);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
        }
    }

    static bool NeedsFrameHackUpdate()
    {
        const auto& settings = ImGuiMenu::g_Settings;
        const auto& hackSettings = ImGuiMenu::g_HackSettings;
        return settings.EnableTransformIntoRandomESP ||
            settings.EnableDuplicateIntoImitationRandomESP ||
            settings.EnableCustomDrop ||
            settings.EnableRebuildMyself ||
            settings.EnableCopySkillsFromNearestEnemy ||
            hackSettings.EnableInvincible ||
            hackSettings.Hack_BypassRentalTickets ||
            hackSettings.EnableInfinitePlusUltra ||
            hackSettings.EnableInfiniteSkills ||
            settings.EnableFastPlusUltraCharge ||
            settings.EnableNoCollision ||
            settings.EnableCameraFOV ||
            settings.EnableRecoveryMe ||
            settings.EnableRecoveryTeam ||
            settings.EnableRecoverySelectedTeam ||
            settings.EnableRecoveryAllESP ||
            settings.EnableBulletTP ||
            settings.EnableAimSearch ||
            settings.EnableAttackChainAuto ||
            settings.EnableDownPowerAuto ||
            hackSettings.AbilityAttackActive ||
            hackSettings.AbilityDurableActive ||
            hackSettings.AbilityMovespeedActive ||
            hackSettings.AbilityHealActive ||
            hackSettings.AbilityTechniqueActive ||
            Cheats::IsAllPlayersActive() ||
            Cheats::IsEnemiesOnlyActive() ||
            Cheats::IsTeammatesOnlyActive();
    }

    static void RunGameFrameUpdate()
    {
        if (g_IsRunningFrameUpdate || !g_Initialized.load(std::memory_order_acquire) || UnloadManager::IsUnloadRequested())
            return;

        const DWORD now = GetTickCount();
        if (now - g_LastFrameUpdateTick < GAME_FRAME_UPDATE_INTERVAL_MS)
            return;

        g_LastFrameUpdateTick = now;

        struct FrameUpdateScope
        {
            FrameUpdateScope() { g_IsRunningFrameUpdate = true; }
            ~FrameUpdateScope() { g_IsRunningFrameUpdate = false; }
        } frameUpdateScope;

        const uint64_t updates = g_FrameUpdates.fetch_add(1, std::memory_order_relaxed) + 1;
        if (updates == 1 || now - g_LastFrameUpdateLogTick >= 5000)
        {
            g_LastFrameUpdateLogTick = now;
            Log("[GameThreadHook] gameplay frame update heartbeat updates=%llu",
                static_cast<unsigned long long>(updates));
        }

        const bool infiniteObjectsTarget =
            ImGuiMenu::g_Settings.EnableGlobal &&
            ImGuiMenu::g_Settings.EnableInfiniteObjects &&
            IsValidBattleModeNoThrow("IsValidBattleMode:InfiniteObjects");

        if (infiniteObjectsTarget)
            TickInfiniteObjectsNoThrow();
        else if (g_HasInfiniteObjectsPatchTarget && g_LastInfiniteObjectsPatchTarget)
            ResetInfiniteObjectsNoThrow();

        g_LastInfiniteObjectsPatchTarget = infiniteObjectsTarget;
        g_HasInfiniteObjectsPatchTarget = true;
        g_LastInfiniteObjectsPatchSyncTick = now;

        if (!ImGuiMenu::g_Settings.EnableGlobal)
            return;

        if (NeedsFrameHackUpdate())
        {
            FrameUpdateHacksNoThrow();
        }

        if (ImGuiMenu::g_Settings.EnableTeleportToKota)
        {
            RunTeleportToKotaNoThrow();
        }

        SyncCH202InitTransNoThrow();
        SyncReloadAdjustNoThrow(now);

        // Persistent respawn / dead-swap toggles — auto-snapshots alive players
        // and swaps the dead ones back in. Runs on the game thread by design.
        DeadSwapTickNoThrow();
    }

    static void HookedProcessEvent(const SDK::UObject* object, SDK::UFunction* function, void* params)
    {
        struct Scope
        {
            Scope() { g_ProcessEventCallDepth.fetch_add(1, std::memory_order_acq_rel); }
            ~Scope() { g_ProcessEventCallDepth.fetch_sub(1, std::memory_order_acq_rel); }
        } scope;

        ProcessEventFn original = nullptr;
        bool isAttackComponent = false;
        {
            std::lock_guard<std::mutex> lock(g_HookMutex);
            for (const HookRecord& hook : g_Hooks)
            {
                if (hook.object == object)
                {
                    original = hook.originalProcessEvent;
                    isAttackComponent = hook.isAttackComponent;
                    break;
                }
            }
        }

        const uint64_t hits = g_ProcessEventHits.fetch_add(1, std::memory_order_relaxed) + 1;
        const DWORD now = GetTickCount();
        if (hits == 1 || now - g_LastProcessEventLogTick >= 5000)
        {
            g_LastProcessEventLogTick = now;
            Log("[GameThreadHook] ProcessEvent heartbeat hits=%llu hookedObject=0x%p original=0x%p queued=%zu",
                static_cast<unsigned long long>(hits),
                object,
                reinterpret_cast<void*>(original),
                GetQueuedTaskCount());
        }

        // All hooked objects (GVC + attack components) dispatch PE from the game thread.
        // Capture once on first fire so GlobalHookedProcessEvent can filter other threads.
        {
            DWORD expected = 0;
            g_GameThreadId.compare_exchange_strong(expected, GetCurrentThreadId(),
                std::memory_order_relaxed, std::memory_order_relaxed);
        }

        if (original)
        {
            if (isAttackComponent && ImGuiMenu::g_Settings.EnableGlobal)
            {
                InGameHack_TryApplySuicideRPC(object, function, params);
                InGameHack_TryBroadcastAttackHit(object, function, params);
            }
            CallOriginalProcessEventNoThrow(original, object, function, params);
        }

        // Only pump/tick from the game thread.
        if (g_GameThreadId.load(std::memory_order_relaxed) == GetCurrentThreadId())
        {
            PumpTasks();
            RunGameFrameUpdate();
        }
    }

    static bool HookObjectLocked(SDK::UObject* object, bool isAttackComponent = false)
    {
        if (!IsWritableObjectPointer(object) || IsAlreadyHookedLocked(object))
            return false;

        if (g_Hooks.size() >= MAX_HOOKED_OBJECTS)
            return false;

        uintptr_t* originalVTable = nullptr;
        __try
        {
            originalVTable = *reinterpret_cast<uintptr_t**>(object);
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return false;
        }

        const int vtableSize = GetVTableSize(originalVTable);
        if (vtableSize < MIN_REASONABLE_VTABLE_SIZE)
            return false;

        auto* shadowVTable = reinterpret_cast<uintptr_t*>(malloc(sizeof(uintptr_t) * vtableSize));
        if (!shadowVTable)
            return false;

        std::memcpy(shadowVTable, originalVTable, sizeof(uintptr_t) * vtableSize);

        HookRecord record{};
        record.object = object;
        record.originalVTable = originalVTable;
        record.shadowVTable = shadowVTable;
        record.vtableSize = vtableSize;
        record.originalProcessEvent = reinterpret_cast<ProcessEventFn>(shadowVTable[PROCESS_EVENT_INDEX]);
        record.isAttackComponent = isAttackComponent;

        if (!record.originalProcessEvent)
        {
            free(shadowVTable);
            return false;
        }

        shadowVTable[PROCESS_EVENT_INDEX] = reinterpret_cast<uintptr_t>(&HookedProcessEvent);

        __try
        {
            *reinterpret_cast<uintptr_t**>(object) = shadowVTable;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            free(shadowVTable);
            return false;
        }

        g_Hooks.push_back(record);
        Log("[GameThreadHook] hooked object=0x%p vtableSize=%d originalPE=0x%p hooks=%zu",
            object,
            vtableSize,
            reinterpret_cast<void*>(record.originalProcessEvent),
            g_Hooks.size());
        return true;
    }

    static void AddCandidate(std::vector<std::pair<SDK::UObject*, bool>>& candidates, SDK::UObject* object, bool isAttackComponent)
    {
        if (!object)
            return;

        for (const auto& c : candidates)
            if (c.first == object) return;

        candidates.push_back({object, isAttackComponent});
    }

    static SDK::UEngine* GetEngineSafe()
    {
        __try
        {
            return SDK::UEngine::GetEngine();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return nullptr;
        }
    }

    static SDK::UWorld* GetWorldSafe()
    {
        __try
        {
            return SDK::UWorld::GetWorld();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return nullptr;
        }
    }

    static SDK::APlayerController* GetPlayerControllerSafe()
    {
        __try
        {
            return SDK_GetPlayerController();
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return nullptr;
        }
    }

    static SDK::UGameViewportClient* ReadGameViewportSafe(SDK::UEngine* engine)
    {
        if (!engine)
            return nullptr;

        __try
        {
            return engine->GameViewport;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return nullptr;
        }
    }

    static SDK::UGameInstance* ReadGameInstanceSafe(SDK::UGameViewportClient* viewport)
    {
        if (!viewport)
            return nullptr;

        __try
        {
            return viewport->GameInstance;
        }
        __except (HandleAccessViolation(GetExceptionInformation()))
        {
            return nullptr;
        }
    }

    static std::vector<std::pair<SDK::UObject*, bool>> CollectHookCandidates()
    {
        std::vector<std::pair<SDK::UObject*, bool>> candidates;
        candidates.reserve(MAX_HOOKED_OBJECTS);

        // Attack components — need isAttackComponent=true so HookedProcessEvent
        // runs the attack-rewrite logic (TryApplySuicideRPC, TryBroadcastAttackHit).
        SDK::UObject* attackReplicator = nullptr;
        SDK::UObject* attackCollision = nullptr;
        InGameHack_CollectSuicideHookObjects(&attackReplicator, &attackCollision);
        AddCandidate(candidates, attackReplicator, true);
        AddCandidate(candidates, attackCollision, true);

        // PlayerController — dispatches blueprint/input/RPC events constantly from
        // the game thread.  isAttackComponent=false: no attack-rewrite logic.
        // The game thread ID check in HookedProcessEvent filters out net-driver
        // thread calls, so no unsafe work runs off-thread.
        SDK::UObject* pc = reinterpret_cast<SDK::UObject*>(GetPlayerControllerSafe());
        AddCandidate(candidates, pc, false);

        return candidates;
    }

    static void RefreshHooks(bool force)
    {
        const DWORD now = GetTickCount();
        if (!force && now - g_LastRefreshTick < REFRESH_INTERVAL_MS)
            return;

        g_LastRefreshTick = now;
        Log("[GameThreadHook] refresh hooks force=%d", force ? 1 : 0);

        std::vector<std::pair<SDK::UObject*, bool>> candidates = CollectHookCandidates();
        std::lock_guard<std::mutex> lock(g_HookMutex);
        const bool canEraseHooks = g_ProcessEventCallDepth.load(std::memory_order_acquire) == 0;

        Log("[GameThreadHook] candidates=%zu currentHooks=%zu canErase=%d",
            candidates.size(),
            g_Hooks.size(),
            canEraseHooks ? 1 : 0);

        for (auto it = g_Hooks.begin(); it != g_Hooks.end();)
        {
            bool found = false;
            for (const auto& c : candidates)
                if (c.first == it->object) { found = true; break; }

            if (!IsWritableObjectPointer(it->object) || !found || !IsHookRecordActiveLocked(*it))
            {
                Log("[GameThreadHook] neutralize stale hook object=0x%p", it->object);
                NeutralizeHookLocked(*it);
                if (canEraseHooks)
                {
                    it = g_Hooks.erase(it);
                    continue;
                }

                ++it;
                continue;
            }

            ++it;
        }

        for (const auto& candidate : candidates)
        {
            if (g_Hooks.size() >= MAX_HOOKED_OBJECTS)
                break;

            HookObjectLocked(candidate.first, candidate.second);
        }
    }

    // =========================================================================
    // Global ProcessEvent inline hook — game-thread tick
    // =========================================================================
    // RVA from Dumpspace/OffsetsInfo.json: OFFSET_PROCESSEVENT = 27448624
    // Patch: 12-byte absolute JMP (MOV RAX, addr + JMP RAX) at function entry.
    // Trampoline: copied 12 prologue bytes + 12-byte JMP back to original+12.
    // All UObjects whose vtable[68] points to this function are intercepted,
    // which is the vast majority — including PlayerController, giving us a
    // stable high-frequency game-thread tick.
    // =========================================================================
    static constexpr uintptr_t GLOBAL_PE_RVA = 27448624; // 0x1A2D9F0
    static uint8_t* g_GlobalPETarget  = nullptr;
    static uint8_t* g_GlobalPETrampoline = nullptr;
    using GlobalPEFn = void(*)(SDK::UObject*, SDK::UFunction*, void*);
    static GlobalPEFn g_GlobalPEOriginal = nullptr;

    static void GlobalHookedProcessEvent(SDK::UObject* object, SDK::UFunction* function, void* params)
    {
        // Only run game logic on the game thread (ID captured from attack-component PE).
        // Animation/render/physics threads: just forward, no hacks.
        const DWORD knownGameThread = g_GameThreadId.load(std::memory_order_relaxed);
        if (knownGameThread != 0 && GetCurrentThreadId() == knownGameThread)
        {
            PumpTasks();
            RunGameFrameUpdate();
        }

        // Forward to original via trampoline.
        if (g_GlobalPEOriginal)
            g_GlobalPEOriginal(object, function, params);
    }

    static bool InstallGlobalPEHook()
    {
        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("HerovsGame-Win64-Shipping.exe"));
        if (!base)
        {
            Log("[GameThreadHook] GlobalPEHook: module not found");
            return false;
        }

        uint8_t* target = reinterpret_cast<uint8_t*>(base + GLOBAL_PE_RVA);

        // Allocate executable trampoline (24 bytes: 12 copied + 12 JMP back)
        uint8_t* tramp = reinterpret_cast<uint8_t*>(
            VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!tramp)
        {
            Log("[GameThreadHook] GlobalPEHook: trampoline alloc failed");
            return false;
        }

        // Copy first 12 bytes of original function to trampoline
        memcpy(tramp, target, 12);

        // Append: MOV RAX, (target+12); JMP RAX — jumps back past our patch
        const uintptr_t retAddr = reinterpret_cast<uintptr_t>(target + 12);
        tramp[12] = 0x48; tramp[13] = 0xB8;
        memcpy(tramp + 14, &retAddr, 8);
        tramp[22] = 0xFF; tramp[23] = 0xE0;

        g_GlobalPETrampoline = tramp;
        g_GlobalPEOriginal   = reinterpret_cast<GlobalPEFn>(tramp);

        // Patch target: MOV RAX, &GlobalHookedProcessEvent; JMP RAX
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, 12, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            VirtualFree(tramp, 0, MEM_RELEASE);
            Log("[GameThreadHook] GlobalPEHook: VirtualProtect failed");
            return false;
        }

        const uintptr_t hookAddr = reinterpret_cast<uintptr_t>(&GlobalHookedProcessEvent);
        target[0] = 0x48; target[1] = 0xB8;
        memcpy(target + 2, &hookAddr, 8);
        target[10] = 0xFF; target[11] = 0xE0;

        VirtualProtect(target, 12, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), target, 12);

        g_GlobalPETarget = target;
        Log("[GameThreadHook] GlobalPEHook installed target=0x%p tramp=0x%p", target, tramp);
        return true;
    }

    static void UninstallGlobalPEHook()
    {
        if (!g_GlobalPETarget || !g_GlobalPETrampoline)
            return;

        DWORD old = 0;
        if (VirtualProtect(g_GlobalPETarget, 12, PAGE_EXECUTE_READWRITE, &old))
        {
            memcpy(g_GlobalPETarget, g_GlobalPETrampoline, 12); // restore original bytes
            VirtualProtect(g_GlobalPETarget, 12, old, &old);
            FlushInstructionCache(GetCurrentProcess(), g_GlobalPETarget, 12);
        }

        VirtualFree(g_GlobalPETrampoline, 0, MEM_RELEASE);
        g_GlobalPETarget     = nullptr;
        g_GlobalPETrampoline = nullptr;
        g_GlobalPEOriginal   = nullptr;
        Log("[GameThreadHook] GlobalPEHook uninstalled");
    }

    bool Initialize()
    {
        if (UnloadManager::IsUnloadRequested())
            return false;

        Log("[GameThreadHook] Initialize begin initialized=%d", g_Initialized.load(std::memory_order_acquire) ? 1 : 0);
        const bool sdkReady = InitializeSdkNoThrow();
        Log("[GameThreadHook] SDKInit::Initialize returned %d", sdkReady ? 1 : 0);

        InstallGlobalPEHook();

        g_Initialized.store(true, std::memory_order_release);
        RefreshHooks(true);
        const bool active = IsHookActive();
        Log("[GameThreadHook] Initialize end active=%d", active ? 1 : 0);
        return active;
    }

    void Shutdown()
    {
        Log("[GameThreadHook] Shutdown begin hooks=%zu peHits=%llu tasksOk=%llu tasksFailed=%llu",
            g_Hooks.size(),
            static_cast<unsigned long long>(g_ProcessEventHits.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_TasksExecuted.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_TasksFailed.load(std::memory_order_relaxed)));
        g_Initialized.store(false, std::memory_order_release);
        UninstallGlobalPEHook();
        ClearTasks();

        {
            std::lock_guard<std::mutex> lock(g_HookMutex);
            for (HookRecord& hook : g_Hooks)
                NeutralizeHookLocked(hook);
        }

        for (int i = 0; i < 200 && g_ProcessEventCallDepth.load(std::memory_order_acquire) > 0; ++i)
            Sleep(1);

        {
            std::lock_guard<std::mutex> lock(g_HookMutex);
            g_Hooks.clear();
        }

        ShutdownSdkNoThrow();
        Log("[GameThreadHook] Shutdown end");
    }

    bool IsHookActive()
    {
        if (!g_Initialized.load(std::memory_order_acquire))
            return false;

        RefreshHooks(false);

        std::lock_guard<std::mutex> lock(g_HookMutex);
        return !g_Hooks.empty();
    }

    void TickFrameHacks()
    {
        PumpTasks();
        RunGameFrameUpdate();
    }

    bool EnqueueTask(std::function<void()> task)
    {
        if (!task || UnloadManager::IsUnloadRequested())
            return false;

        if (!g_Initialized.load(std::memory_order_acquire))
            Initialize();
        else
            RefreshHooks(false);

        std::lock_guard<std::mutex> taskLock(g_TaskMutex);
        if (g_GameThreadTasks.size() >= MAX_GAME_THREAD_TASKS)
        {
            Log("[GameThreadHook] enqueue rejected queue full size=%zu", g_GameThreadTasks.size());
            return false;
        }

        g_GameThreadTasks.push(std::move(task));
        Log("[GameThreadHook] task queued size=%zu", g_GameThreadTasks.size());
        return true;
    }

    void HookProcessEvent(uintptr_t objectAddress)
    {
        if (!objectAddress || UnloadManager::IsUnloadRequested())
            return;

        Log("[GameThreadHook] manual HookProcessEvent object=0x%p", reinterpret_cast<void*>(objectAddress));
        g_Initialized.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(g_HookMutex);
        HookObjectLocked(reinterpret_cast<SDK::UObject*>(objectAddress));
    }
}
