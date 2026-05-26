#pragma once

namespace UnloadManager
{
    extern volatile bool g_UnloadRequested;

    inline bool IsUnloadRequested()
    {
        return g_UnloadRequested;
    }

    inline void RequestUnload()
    {
        g_UnloadRequested = true;
    }
}
