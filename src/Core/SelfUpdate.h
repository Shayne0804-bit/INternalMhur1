#pragma once

#include <Windows.h>
#include <string>

// In-DLL self-update. No loader involvement: the running module downloads the
// next build from the auth server, verifies its SHA-256 against the manifest,
// swaps it onto its own path on disk, unloads itself cleanly, and a tiny RWX
// trampoline (living outside the module) reloads the fresh DLL once the old one
// is fully unmapped. The new DLL's DllMain re-hooks everything from scratch and
// cleans up the leftover backup, closing the loop.
//
// The flow is a user-driven state machine so the menu can:
//   1. check the server in the background (Idle -> Checking -> Available)
//   2. prompt the user ("new update, download? yes/no")
//   3. on yes, download with live speed + progress (Downloading -> Ready)
//   4. on OK, apply (swap + unload + reload trampoline)
namespace SelfUpdate
{
    using CleanupFn = void(*)();

    enum class State
    {
        Idle,          // nothing happening / up to date
        Checking,      // manifest fetch in flight
        Available,     // a newer version exists; waiting for user yes/no
        Downloading,   // pulling the new dll, progress fields live
        Ready,         // download+hash ok, staged on disk; waiting for user OK
        Applying,      // swap + unload + reload in progress (module dying)
        Error          // something failed; errorText set, back to Idle-able
    };

    // Live progress snapshot, safe to read every frame from the render thread.
    struct Progress
    {
        State       state = State::Idle;
        std::string latestVersion;   // server version when Available/onward
        std::string errorText;       // populated on State::Error

        unsigned long long bytesReceived = 0;
        unsigned long long bytesTotal    = 0;    // 0 if server sent no length
        double             speedBytesPerSec = 0; // smoothed
        double             fraction = 0.0;       // 0..1, or 0 if total unknown
    };

    // Wire the module handle + cleanup routine. Call once from MainThread.
    // Sweeps any *.old / *.new leftovers from a previous update.
    void Configure(HMODULE self, CleanupFn cleanup);

    // Kick a background manifest check. Non-blocking. Moves Idle -> Checking and
    // then to Available (update found) or Idle (up to date) / Error.
    void CheckAsync();

    // User pressed "Yes, download". Valid only in Available. Starts the download
    // on a worker thread; progress fields update live. -> Downloading -> Ready.
    void AcceptDownload();

    // User pressed "No". Available -> Idle. Won't re-prompt until next CheckAsync.
    void DeclineDownload();

    // User pressed "OK" on the finished download. Valid only in Ready. Performs
    // the swap+unload+reload. The module starts dying right after. -> Applying.
    void ApplyUpdate();

    // Dismiss an error back to Idle.
    void DismissError();

    // Thread-safe snapshot for the UI.
    Progress GetProgress();

    // Convenience for the menu: is there anything to draw right now?
    bool HasPendingUI();

    // Delete sibling "*.old" / "*.new" backups next to the module.
    void CleanupLeftovers();
}
