#pragma once
#include <atomic>

namespace UnloadManager
{
    extern std::atomic_bool g_UnloadRequested;

    inline bool IsUnloadRequested()
    {
        return g_UnloadRequested.load(std::memory_order_acquire);
    }

    inline bool RequestUnload()
    {
        bool expected = false;
        return g_UnloadRequested.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }
}
