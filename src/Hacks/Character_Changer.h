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

    // Max Hit Count — overrides UANS_Attack._maxHitCount on all attack notifies so
    // a single attack window lands more hits. Independent from kill-all.
    void MaxHitCount_SetActive(bool active);
    bool MaxHitCount_IsActive();
    void MaxHitCount_SetValue(int value);
    int  MaxHitCount_GetValue();

    // Fills out[] with enemy ACharacterBattle* (as void*), skipping own team and
    // 'exceptPawn'. Used by the attack-hit re-broadcast hook. Returns count.
    int CollectEnemyBattlePawns(void** out, int maxCount, void* exceptPawn);

    // Fills out[] with MY team's ACharacterBattle* (as void*), INCLUDING self.
    // Used by Max Hit Count to target only our own side's active attack notifies.
    int CollectMyTeamBattlePawns(void** out, int maxCount);

    // ── State check ───────────────────────────────────────────────────────────
    bool IsAllPlayersActive();

    // ── Frame update ──────────────────────────────────────────────────────────
    void Tick();

} // namespace Cheats
