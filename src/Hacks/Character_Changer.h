#pragma once

namespace Cheats
{
    struct ServerConnectionInfo
    {
        char region[128];
        int pingMs;
        bool hasRegion;
        bool hasPing;
    };

    // ── Player info struct ─────────────────────────────────────────────────────
    struct PlayerInfo
    {
        int index;
        char name[256];
        int currentCharacterId;
        int characterIndex;
        int teamId;
        int gamePlayerId;
        int platform;
        int pingMs;
        char platformName[32];
    };

    struct PlayerNetworkInfo
    {
        int index;
        char displayName[256];
        char publicName[128];
        char privateName[128];
        int platform;
        char platformName[32];
        int pingMs;
        int teamId;
        int gamePlayerId;
        void* playerState;
        void* actor;
        bool hasActor;
        bool isLocal;
    };

    // ── Player snapshot (alive-state capture for dead-pawn swap) ─────────────
    struct PlayerSnapshot {
        bool valid       = false;
        int  characterId = 0;
        int  variationId = 0;
        int  costumeCode = 0;
        int  emoteCodes[8] = {};
        int  emoteCount  = 0;
        int  voiceCodes[8] = {};
        int  voiceCount  = 0;
    };

    // Scan every non-dead player in world and record their loadout.
    // Returns number of players snapshotted. Call once per match start or on demand.
    int  SnapshotAlivePlayers();

    // Look up snapshot by APlayerState* (stable key across pawn death).
    // Returns false if no snapshot exists for that pointer.
    bool GetPlayerSnapshot(void* playerStatePtr, PlayerSnapshot* outSnap);

    // Swap every currently-dead player using their stored snapshot loadout.
    // Returns number of swaps attempted.
    int  SwapDeadPlayersFromSnapshot();

    // Game-thread tick for the persistent respawn toggles. Auto-refreshes alive
    // snapshots, then swaps dead players matching each active mode. MUST run on
    // the game thread (ProcessEvent frame update), not the polling thread.
    void DeadSwap_Tick(bool self, bool team, bool everyone);

    // ── Collect all players in world ───────────────────────────────────────────
    int CollectPlayerList(PlayerInfo* outPlayers, int maxPlayers);
    int CollectLobbyPlayerList(PlayerInfo* outPlayers, int maxPlayers);
    int CollectPlayerNetworkInfo(PlayerNetworkInfo* outPlayers, int maxPlayers);
    int GetLocalPingMs();
    bool CanReadServerConnectionInfo();
    bool GetCurrentServerRegion(char* outRegion, int maxLen);
    bool GetCurrentServerConnectionInfo(ServerConnectionInfo* outInfo);

    // ── Self-only targeting ───────────────────────────────────────────────────
    void ChangeCharacter(int characterId, int variation);

    // ── Individual player targeting ────────────────────────────────────────────
    void ChangeCharacterIndividual(int playerIndex, int characterId, int variation);
    bool ChangePlayerNetworkTargetToCh001(void* playerState);

    // ── All other players ─────────────────────────────────────────────────────
    void ChangeCharacterAllPlayers(int characterId, int variation);
    void ChangeCharacterAllPlayers_Start(int characterId, int variation);
    void ChangeCharacterAllPlayers_Stop();

    // ── Enemy team only (NEW) ─────────────────────────────────────────────────
    void ChangeCharacterEnemiesOnly(int characterId, int variation);
    void ChangeCharacterEnemiesOnly_Start(int characterId, int variation);
    void ChangeCharacterEnemiesOnly_Stop();
    bool IsEnemiesOnlyActive();

    // ── Team mates only (NEW) ─────────────────────────────────────────────────
    void ChangeCharacterTeammatesOnly(int characterId, int variation);
    void ChangeCharacterTeammatesOnly_Start(int characterId, int variation);
    void ChangeCharacterTeammatesOnly_Stop();
    bool IsTeammatesOnlyActive();

    // ── Kill all enemies (NEW) ────────────────────────────────────────────────
    // Toggle: when active, the ProcessEvent hook re-broadcasts our real outgoing
    // melee-hit RPC to every enemy pawn (skips own team). Driven by a real attack.
    void KillAllEnemies_Start();
    void KillAllEnemies_Stop();
    bool IsKillAllEnemiesActive();

    // Fills out[] with enemy ACharacterBattle* (as void*), skipping own team and
    // 'exceptPawn'. Used by the attack-hit re-broadcast hook. Returns count.
    int CollectEnemyBattlePawns(void** out, int maxCount, void* exceptPawn);

    // ── State check ───────────────────────────────────────────────────────────
    bool IsAllPlayersActive();

    // ── Frame update ──────────────────────────────────────────────────────────
    void Tick();

} // namespace Cheats
