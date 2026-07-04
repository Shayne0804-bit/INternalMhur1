#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>

// ============================================================================
// HACK THREAD MANAGER - Custom threading system for game hacks
// ============================================================================
// 
// Purpose: Execute hacks asynchronously in a dedicated worker thread to avoid
//          blocking the main menu/rendering thread
// 
// Usage:
//   HackThreadManager::GetInstance().EnqueueHack([]() {
//       InGameHack_RebuildMyself();
//   });
//
// ============================================================================

class HackThreadManager
{
public:
    // Singleton instance
    static HackThreadManager& GetInstance();

    // Delete copy constructor and assignment operator
    HackThreadManager(const HackThreadManager&) = delete;
    HackThreadManager& operator=(const HackThreadManager&) = delete;

    // Initialize the hack thread (call once at startup)
    void Initialize();

    // Shutdown the hack thread (call before exit)
    void Shutdown();

    // Enqueue a hack function to execute asynchronously
    // The function will be executed in the worker thread
    void EnqueueHack(std::function<void()> hackFunction);

    // Enqueue a hack with a delay (in milliseconds)
    void EnqueueHackWithDelay(std::function<void()> hackFunction, int delayMs);

    // Get the number of pending hack tasks
    size_t GetPendingTaskCount();

    // Check if thread is running
    bool IsRunning() const { return m_isRunning.load(); }

    // Wait for all pending hacks to complete (blocking)
    void WaitForCompletion();

    // Process frame hacks - called every frame from D3D11Hook
    // Contains all continuous hack logic (slow updates, hotkeys, recovery, etc.)
    void FrameUpdateHacks();

private:
    HackThreadManager();
    ~HackThreadManager();

    // Worker thread function
    void WorkerThreadProc();

    void FrameUpdateHacksImpl();

    // Hack task structure
    struct HackTask
    {
        std::function<void()> func;
        std::chrono::steady_clock::time_point executeTime;
        
        HackTask(std::function<void()> f)
            : func(f), executeTime(std::chrono::steady_clock::now())
        {
        }

        HackTask(std::function<void()> f, int delayMs)
            : func(f), executeTime(std::chrono::steady_clock::now() + 
                                   std::chrono::milliseconds(delayMs))
        {
        }
    };

    std::unique_ptr<std::thread> m_workerThread;
    std::queue<HackTask> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_taskAvailable;
    std::condition_variable m_tasksCompleted;
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_shutdown;
    std::atomic<size_t> m_pendingTasks;
};

// ============================================================================
// CONVENIENCE MACRO FOR QUICK HACK ENQUEUEING
// ============================================================================

#define ENQUEUE_HACK(func) HackThreadManager::GetInstance().EnqueueHack(func)
#define ENQUEUE_HACK_DELAYED(func, delayMs) HackThreadManager::GetInstance().EnqueueHackWithDelay(func, delayMs)
