#pragma once

namespace Cheats
{
    // ── Player info struct ─────────────────────────────────────────────────────
    struct PlayerInfo
    {
        int index;
        char name[256];
        int currentCharacterId;
        int characterIndex;
        int teamId;
        int platform;
        int pingMs;
        char platformName[32];
    };

    // ── Collect all players in world ───────────────────────────────────────────
    int CollectPlayerList(PlayerInfo* outPlayers, int maxPlayers);
    int CollectLobbyPlayerList(PlayerInfo* outPlayers, int maxPlayers);
    int GetLocalPingMs();
    bool GetCurrentServerRegion(char* outRegion, int maxLen);

    // ── Self-only targeting ───────────────────────────────────────────────────
    void ChangeCharacter(int characterId, int variation);

    // ── Individual player targeting ────────────────────────────────────────────
    void ChangeCharacterIndividual(int playerIndex, int characterId, int variation);

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

    // ── State check ───────────────────────────────────────────────────────────
    bool IsAllPlayersActive();

    // ── Frame update ──────────────────────────────────────────────────────────
    void Tick();

} // namespace Cheats
