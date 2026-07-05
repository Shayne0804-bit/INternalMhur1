#pragma once

#include <string>

// Client-side license / HWID authentication against the RUGIR auth server.
// The server binds the first HWID it sees for a key; a different machine using
// the same key is rejected (hwid_limit_reached). This module generates the
// machine HWID, talks to the server, and exposes the gate the rest of the
// cheat checks before doing anything.
namespace Auth
{
    enum class State
    {
        Idle,        // no key entered yet
        Checking,    // a verify request is in flight
        Authorized,  // server said ok, session valid
        Denied       // server rejected (invalid/expired/revoked/hwid), or network down with no valid session
    };

    // Stable machine fingerprint (SHA-256 hex, lowercase). Computed once, cached.
    const std::string& GetHwid();

    // First 8 hex chars of the HWID, for display / support.
    std::string GetHwidShort();

    // Start verification on a detached worker thread. Returns immediately.
    // On success the key is persisted (DPAPI) so it survives restarts.
    void ActivateAsync(const std::string& key);

    // The gate. True only while a valid, non-expired session is held.
    bool IsAuthorized();

    State GetState();

    // Human-readable status line (French) for the activation UI.
    std::string GetStatusText();

    // Detected license tier once authorized ("premium", ...), empty otherwise.
    std::string GetTier();

    // Human-readable expiration ("2026-08-03 23:07 UTC"), empty if none/lifetime.
    std::string GetExpiresAt();

    // Previously saved key (DPAPI), for pre-filling the input. Does NOT connect.
    std::string GetSavedKey();

    // Periodic re-validation. Safe to call every frame/tick; it self-throttles.
    void HeartbeatTick();

    // Forget the saved key and reset to Idle (logout).
    void Clear();
}
