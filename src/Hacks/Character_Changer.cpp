// SDK MUST come first
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/BackendSubsystem_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "Character_Changer.h"
#include "Character_Data.h"
#include "../Utils/CostumeHelper.h"
#include "../Utils/SafeMemory.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>
#include <cstdint>
#include <Windows.h>

// Global variables from ImGuiMenu - used for character customization
extern int s_techAlpha;
extern int s_techBeta;
extern int s_techGamma;
extern int s_costumeCode;
extern int s_emoteSlot[8];
extern int s_lastInGameChar;

namespace Cheats
{
    // ── Internal state ────────────────────────────────────────────────────────
    static bool  s_AllPlayersActive = false;
    static bool  s_EnemiesOnlyActive = false;
    static bool  s_TeammatesOnlyActive = false;  // NEW: Team swap persistent mode
    static int   s_TargetId = 0;
    static int   s_TargetVariation = 0;
    static float s_RetryIntervalSec = 2.0f;
    static std::chrono::steady_clock::time_point s_LastApplyTime = {};

    // ── chNum lookup ──────────────────────────────────────────────────────────
    static int GetCharNum(int characterId)
    {
        for (int i = 0; i < CharacterCount; i++)
            if (Characters[i].id == characterId)
                return Characters[i].chNum;
        return characterId;
    }

    static const char* GetPlatformName(uint8_t platform)
    {
        switch (platform)
        {
        case 1: return "PlayStation";
        case 2: return "Xbox";
        case 3: return "Windows";
        case 4: return "Switch";
        case 5: return "None";
        default: return "Bot";
        }
    }

    // ── Map Character_Data.h id to SDK enum value ─────────────────────────────
    static SDK::ECharacterId GetSDKCharacterId(int characterId)
    {
        // Explicit mapping - each index is the Character_Data.h ID
        switch (characterId) {
        case 1: return (SDK::ECharacterId)2;      // Izuku
        case 2: return (SDK::ECharacterId)3;      // Bakugo
        case 3: return (SDK::ECharacterId)4;      // Uraraka
        case 4: return (SDK::ECharacterId)5;      // Todoroki
        case 5: return (SDK::ECharacterId)6;      // Tenya
        case 6: return (SDK::ECharacterId)7;      // Tsuyu
        case 7: return (SDK::ECharacterId)8;      // Denki
        case 8: return (SDK::ECharacterId)9;      // Kirishima
        case 10: return (SDK::ECharacterId)10;    // Momo
        case 11: return (SDK::ECharacterId)11;    // Tokoyami
        case 12: return (SDK::ECharacterId)12;    // All Might
        case 13: return (SDK::ECharacterId)13;    // Aizawa
        case 15: return (SDK::ECharacterId)14;    // Shigaraki
        case 16: return (SDK::ECharacterId)15;    // All For One
        case 17: return (SDK::ECharacterId)16;    // Dabi
        case 18: return (SDK::ECharacterId)17;    // Toga
        case 23: return (SDK::ECharacterId)18;    // Endeavor
        case 24: return (SDK::ECharacterId)19;    // Mirio
        case 25: return (SDK::ECharacterId)20;    // Nejire
        case 26: return (SDK::ECharacterId)21;    // Tamaki
        case 34: return (SDK::ECharacterId)22;    // Overhaul
        case 37: return (SDK::ECharacterId)23;    // Twice
        case 38: return (SDK::ECharacterId)24;    // Mr. Compress
        case 43: return (SDK::ECharacterId)25;    // Hawks
        case 46: return (SDK::ECharacterId)26;    // Itsuka Kendo
        case 100: return (SDK::ECharacterId)27;   // Mt. Lady
        case 101: return (SDK::ECharacterId)28;   // Cementoss
        case 102: return (SDK::ECharacterId)29;   // Ibara
        case 103: return (SDK::ECharacterId)30;   // Kurogiri
        case 104: return (SDK::ECharacterId)31;   // Neito Monoma
        case 105: return (SDK::ECharacterId)32;   // Hitoshi Shinso
        case 109: return (SDK::ECharacterId)33;   // Present Mic
        case 111: return (SDK::ECharacterId)34;   // Mirko
        case 114: return (SDK::ECharacterId)35;   // Star & Stripe
        case 115: return (SDK::ECharacterId)36;   // Lady Nagant
        case 200: return (SDK::ECharacterId)37;   // Armored All Might
        case 201: return (SDK::ECharacterId)38;   // Prime All For One
        case 202: return (SDK::ECharacterId)39;   // Deku Final
        case 502: return (SDK::ECharacterId)41;
        default: return (SDK::ECharacterId)characterId;
        }
    }

    static int GetCharacterIndexFromSDKId(int sdkCharacterId)
    {
        for (int i = 0; i < CharacterCount; i++)
        {
            if ((int)GetSDKCharacterId(Characters[i].id) == sdkCharacterId)
                return i;
        }
        return -1;
    }

    static int ReadPlayerPingMs(SDK::APlayerState* PS)
    {
        if (IsBadReadPtr(PS, sizeof(SDK::APlayerState))) return -1;
        return (int)PS->ping * 4;
    }

    static size_t CopyFStringToChar(const SDK::FString& str, char* out, size_t maxLen)
    {
        if (!out || maxLen == 0) return 0;
        out[0] = '\0';

        size_t i = 0;
        const int32_t strLen = str.Num();
        const wchar_t* data = str.CStr();

        if (strLen > 0 && strLen < 4096 && SafeMemory::IsReadable(data, sizeof(wchar_t) * static_cast<size_t>(strLen)))
        {
            while (i < maxLen - 1 && i < static_cast<size_t>(strLen))
            {
                wchar_t ch = data[i];
                if (ch == L'\0') break;
                out[i++] = (char)ch;
            }
        }

        out[i] = '\0';
        return i;
    }

    static SDK::UWorld* GetWorldSafe()
    {
        __try
        {
            SDK::UWorld* world = SDK::UWorld::GetWorld();
            return SafeMemory::IsReadable(world, sizeof(void*)) ? world : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    static SDK::UClass* GetDbpSettingClassSafe()
    {
        __try
        {
            SDK::UClass* settingClass = SDK::UDbpSetting::StaticClass();
            return SafeMemory::IsReadable(settingClass, sizeof(void*)) ? settingClass : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    static SDK::UDbpSetting* GetDbpSettingDefaultSafe()
    {
        __try
        {
            SDK::UDbpSetting* defaultObj = SDK::UDbpSetting::GetDefaultObj();
            return SafeMemory::IsReadable(defaultObj, sizeof(void*)) ? defaultObj : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    static int32_t GetObjectArrayCountSafe(SDK::TUObjectArray* objArray)
    {
        if (!SafeMemory::IsReadable(objArray, sizeof(SDK::TUObjectArray)))
            return 0;

        __try
        {
            const int32_t count = objArray->Num();
            return (count > 0 && count < 2000000) ? count : 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    static SDK::UObject* GetObjectByIndexSafe(SDK::TUObjectArray* objArray, int32_t index)
    {
        if (!SafeMemory::IsReadable(objArray, sizeof(SDK::TUObjectArray)))
            return nullptr;

        __try
        {
            SDK::UObject* obj = objArray->GetByIndex(index);
            return SafeMemory::IsReadable(obj, sizeof(SDK::UObject)) ? obj : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    static bool IsObjectAClassSafe(SDK::UObject* obj, SDK::UClass* targetClass)
    {
        if (!SafeMemory::IsReadable(obj, sizeof(SDK::UObject)) ||
            !SafeMemory::IsReadable(targetClass, sizeof(void*)))
        {
            return false;
        }

        __try
        {
            return obj->IsA(targetClass);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static SDK::TUObjectArray* GetGObjectsSafe()
    {
        __try
        {
            uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            SDK::TUObjectArray* objArray = reinterpret_cast<SDK::TUObjectArray*>(base + SDK::Offsets::GObjects);
            return SafeMemory::IsReadable(objArray, sizeof(SDK::TUObjectArray)) ? objArray : nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    static bool GetCurrentRegionSafe(SDK::UDbpSetting* setting, SDK::FDbMatchingRegionSettingParam& outRegion)
    {
        if (!SafeMemory::IsReadable(setting, sizeof(SDK::UDbpSetting)))
            return false;

        __try
        {
            outRegion = setting->GetCurrentRegion();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static SDK::ACharacterBattle* AsCharacterBattle(SDK::AActor* Actor)
    {
        if (IsBadReadPtr(Actor, sizeof(SDK::AActor))) return nullptr;
        if (!Actor->IsA(SDK::ACharacterBattle::StaticClass())) return nullptr;

        auto* Battle = static_cast<SDK::ACharacterBattle*>(Actor);
        if (IsBadReadPtr(Battle, sizeof(SDK::ACharacterBattle))) return nullptr;
        return Battle;
    }

    static SDK::AHerovsPlayerState* AsHerovsPlayerState(SDK::APlayerState* PlayerState)
    {
        if (IsBadReadPtr(PlayerState, sizeof(SDK::APlayerState))) return nullptr;
        if (!PlayerState->IsA(SDK::AHerovsPlayerState::StaticClass())) return nullptr;

        auto* HerovsState = static_cast<SDK::AHerovsPlayerState*>(PlayerState);
        if (IsBadReadPtr(HerovsState, sizeof(SDK::AHerovsPlayerState))) return nullptr;
        return HerovsState;
    }

    static int GetCharacterIdFromSDK(SDK::ACharacterGame* Character)
    {
        if (IsBadReadPtr(Character, sizeof(SDK::ACharacterGame))) return -1;
        return (int)Character->BP_GetCharacterId();
    }

    static int GetTeamIdFromSDK(SDK::ACharacterGame* Character)
    {
        if (auto* Battle = AsCharacterBattle(Character))
            return (int)Battle->BP_GetTeamId();

        SDK::APlayerState* PlayerState = nullptr;
        if (!IsBadReadPtr(Character, sizeof(SDK::ACharacterGame)))
            PlayerState = Character->PlayerState;

        if (auto* HerovsState = AsHerovsPlayerState(PlayerState))
            return HerovsState->BP_GetTeamId();

        return -1;
    }

    static int GetTeamIdFromSDK(SDK::APlayerState* PlayerState)
    {
        if (auto* HerovsState = AsHerovsPlayerState(PlayerState))
            return HerovsState->BP_GetTeamId();

        return -1;
    }

    static int GetSpawnCharacterIdFromSDK(SDK::APlayerState* PlayerState)
    {
        if (auto* HerovsState = AsHerovsPlayerState(PlayerState))
            return (int)HerovsState->BP_GetSpawnCharacterId();

        return -1;
    }

    static int GetPlatformFromSDK(SDK::APlayerState* PlayerState)
    {
        if (auto* HerovsState = AsHerovsPlayerState(PlayerState))
            return (int)HerovsState->GetPlatform();

        return 0;
    }

    static int GetGamePlayerIdFromSDK(SDK::APlayerState* PlayerState)
    {
        if (auto* HerovsState = AsHerovsPlayerState(PlayerState))
        {
            __try
            {
                return HerovsState->BP_GetPlayerId();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        return -1;
    }

    static size_t CopyPlayerNameFromSDK(SDK::APlayerState* PlayerState, char* out, size_t maxLen)
    {
        if (!out || maxLen == 0) return 0;
        out[0] = '\0';

        if (!IsBadReadPtr(PlayerState, sizeof(SDK::APlayerState)) &&
            PlayerState->IsA(SDK::APlayerStateBattle::StaticClass()))
        {
            auto* BattleState = static_cast<SDK::APlayerStateBattle*>(PlayerState);
            if (!IsBadReadPtr(BattleState, sizeof(SDK::APlayerStateBattle)))
            {
                size_t copied = CopyFStringToChar(BattleState->GetPlayerNameIgnorePrioritySetting(), out, maxLen);
                if (copied > 0) return copied;
            }
        }

        if (!IsBadReadPtr(PlayerState, sizeof(SDK::APlayerState)))
            return CopyFStringToChar(PlayerState->GetPlayerName(), out, maxLen);

        return 0;
    }

    // ── Skill variation code calculator ───────────────────────────────────────
    static int GetSkillVariationCode(int characterId, int variation)
    {
        int chNum = GetCharNum(characterId);
        return (chNum * 100) + variation;
    }

    // ── Build FInGameBattleCharacterData ──────────────────────────────────────
    static bool TryGetCostumeRoleSlotParam(int costumeCode, SDK::FDbsCostumeRoleSlotParam* outParam)
    {
        if (!outParam)
            return false;

        __try
        {
            return SDK::URoleSlotStatics::GetCostumeRoleSlotParam(costumeCode, outParam);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool FillRoleSlotWithGetterForChangeCharacter(SDK::FInGameBattleCharacterData& data)
    {
        if (data._costumeCode <= 0)
            return false;

        SDK::FDbsCostumeRoleSlotParam roleSlot{};
        if (!TryGetCostumeRoleSlotParam(data._costumeCode, &roleSlot))
            return false;

        data._roleSlot = roleSlot;
        return true;
    }

    static SDK::EVariationCharacterId GetRollSlotVariationIdForCharacter(int characterId, int variation)
    {
        int base = 0;
        int maxVariation = 0;

        switch (characterId)
        {
        case 1: base = 1; maxVariation = 1; break;
        case 2: base = 3; maxVariation = 2; break;
        case 3: base = 6; maxVariation = 1; break;
        case 4: base = 8; maxVariation = 1; break;
        case 5: base = 10; maxVariation = 1; break;
        case 6: base = 12; maxVariation = 1; break;
        case 7: base = 14; maxVariation = 1; break;
        case 8: base = 16; maxVariation = 1; break;
        case 10: base = 18; maxVariation = 1; break;
        case 11: base = 20; maxVariation = 1; break;
        case 12: base = 22; maxVariation = 1; break;
        case 13: base = 24; maxVariation = 1; break;
        case 15: base = 26; maxVariation = 2; break;
        case 16: base = 29; maxVariation = 1; break;
        case 17: base = 31; maxVariation = 1; break;
        case 18: base = 33; maxVariation = 1; break;
        case 23: base = 35; maxVariation = 1; break;
        case 24: base = 37; maxVariation = 1; break;
        case 25: base = 39; maxVariation = 1; break;
        case 26: base = 41; maxVariation = 1; break;
        case 34: base = 43; maxVariation = 1; break;
        case 37: base = 45; maxVariation = 1; break;
        case 38: base = 47; maxVariation = 1; break;
        case 43: base = 49; maxVariation = 1; break;
        case 46: base = 51; maxVariation = 1; break;
        case 100: base = 53; maxVariation = 1; break;
        case 101: base = 55; maxVariation = 1; break;
        case 102: base = 57; maxVariation = 1; break;
        case 103: base = 59; maxVariation = 1; break;
        case 104: base = 61; maxVariation = 1; break;
        case 105: base = 63; maxVariation = 1; break;
        case 109: base = 65; maxVariation = 1; break;
        case 111: base = 67; maxVariation = 1; break;
        case 114: base = 69; maxVariation = 1; break;
        case 115: base = 71; maxVariation = 1; break;
        case 200: base = 73; maxVariation = 1; break;
        case 201: base = 75; maxVariation = 1; break;
        case 202: base = 77; maxVariation = 1; break;
        default: return SDK::EVariationCharacterId::UNDEF;
        }

        if (variation < 0 || variation > maxVariation)
            return SDK::EVariationCharacterId::UNDEF;

        return static_cast<SDK::EVariationCharacterId>(base + variation);
    }

    static SDK::APlayerStateBattle* GetPlayerStateBattleFromTargetSafe(SDK::ACharacterBattle* targetCharacter)
    {
        if (IsBadReadPtr(targetCharacter, sizeof(SDK::ACharacterBattle)))
            return nullptr;

        SDK::APlayerStateBattle* battleState = nullptr;
        __try
        {
            battleState = targetCharacter->BP_GetPlayerStateBattle();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            battleState = nullptr;
        }

        if (!IsBadReadPtr(battleState, sizeof(SDK::APlayerStateBattle)))
            return battleState;

        SDK::ACharacterGame* characterGame = reinterpret_cast<SDK::ACharacterGame*>(targetCharacter);
        if (IsBadReadPtr(characterGame, sizeof(SDK::ACharacterGame)))
            return nullptr;

        SDK::APlayerState* playerState = characterGame->PlayerState;
        if (IsBadReadPtr(playerState, sizeof(SDK::APlayerState)))
            return nullptr;

        if (!playerState->IsA(SDK::APlayerStateBattle::StaticClass()))
            return nullptr;

        battleState = static_cast<SDK::APlayerStateBattle*>(playerState);
        return IsBadReadPtr(battleState, sizeof(SDK::APlayerStateBattle)) ? nullptr : battleState;
    }

    static SDK::UCharacterRollSlotUniqueSkillControlComponent* GetRollSlotControlComponentSafe(SDK::APlayerStateBattle* playerState)
    {
        if (IsBadReadPtr(playerState, sizeof(SDK::APlayerStateBattle)))
            return nullptr;

        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl = nullptr;
        __try
        {
            rollSlotCtrl = playerState->BP_GetCharacterRollSlotUniqueSkillControlComponent();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }

        return IsBadReadPtr(rollSlotCtrl, sizeof(SDK::UCharacterRollSlotUniqueSkillControlComponent)) ? nullptr : rollSlotCtrl;
    }

    static SDK::UCharacterRollSlotUniqueSkillBase* GetRegisteredRollSlotObjectSafe(
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl,
        SDK::EVariationCharacterId id)
    {
        if (IsBadReadPtr(rollSlotCtrl, sizeof(SDK::UCharacterRollSlotUniqueSkillControlComponent)))
            return nullptr;

        SDK::UCharacterRollSlotUniqueSkillBase* rollSlotObject = nullptr;
        __try
        {
            rollSlotObject = rollSlotCtrl->BP_GetObject(id);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }

        return IsBadReadPtr(rollSlotObject, sizeof(SDK::UCharacterRollSlotUniqueSkillBase)) ? nullptr : rollSlotObject;
    }

    static int CountRegisteredRollSlots(
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl,
        SDK::EVariationCharacterId desiredId,
        SDK::UCharacterRollSlotUniqueSkillBase** desiredObject)
    {
        if (desiredObject)
            *desiredObject = nullptr;

        if (IsBadReadPtr(rollSlotCtrl, sizeof(SDK::UCharacterRollSlotUniqueSkillControlComponent)))
            return 0;

        int count = 0;
        const int maxVariationId = static_cast<int>(SDK::EVariationCharacterId::MAX);
        for (int rawId = 1; rawId < maxVariationId; ++rawId)
        {
            SDK::EVariationCharacterId id = static_cast<SDK::EVariationCharacterId>(rawId);
            SDK::UCharacterRollSlotUniqueSkillBase* object = GetRegisteredRollSlotObjectSafe(rollSlotCtrl, id);
            if (!object)
                continue;

            ++count;
            if (desiredObject && id == desiredId)
                *desiredObject = object;
        }

        return count;
    }

    static bool SetRollSlotParamByCharacterIdSafe(
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl,
        SDK::EVariationCharacterId id,
        const SDK::FRollSlotUniqueSkillArgumentParam& param)
    {
        if (IsBadReadPtr(rollSlotCtrl, sizeof(SDK::UCharacterRollSlotUniqueSkillControlComponent)))
            return false;

        __try
        {
            rollSlotCtrl->BP_SetParamByCharacterId(id, param);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool BroadcastRollSlotByCharacterIdSafe(
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl,
        SDK::EVariationCharacterId id)
    {
        if (IsBadReadPtr(rollSlotCtrl, sizeof(SDK::UCharacterRollSlotUniqueSkillControlComponent)))
            return false;

        __try
        {
            float refParam = 1.0f;
            rollSlotCtrl->BP_BroadcastByCharacterId(id, refParam, 0);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool ApplyTargetRegisteredRollSlot(SDK::ACharacterBattle* targetCharacter, int characterId, int variation)
    {
        SDK::EVariationCharacterId desiredId = GetRollSlotVariationIdForCharacter(characterId, variation);
        if (desiredId == SDK::EVariationCharacterId::UNDEF)
            return false;

        SDK::APlayerStateBattle* targetPlayerState = GetPlayerStateBattleFromTargetSafe(targetCharacter);
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl = GetRollSlotControlComponentSafe(targetPlayerState);

        SDK::UCharacterRollSlotUniqueSkillBase* desiredObject = GetRegisteredRollSlotObjectSafe(rollSlotCtrl, desiredId);
        if (!desiredObject)
            return false;

        SDK::FRollSlotUniqueSkillArgumentParam param{};
        param._broadcastIndex = 0;
        param._targetCharacter = targetCharacter;

        const bool setParam = SetRollSlotParamByCharacterIdSafe(rollSlotCtrl, desiredId, param);
        const bool broadcast = setParam && BroadcastRollSlotByCharacterIdSafe(rollSlotCtrl, desiredId);

        return setParam && broadcast;
    }

    template<typename T>
    static int GetSafeArrayCount(const SDK::TArray<T>& array, int maxReasonableCount)
    {
        int count = 0;
        const T* data = nullptr;

        __try
        {
            count = array.Num();
            data = array.GetDataPtr();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -1;
        }

        if (count < 0 || count > maxReasonableCount)
            return -1;

        if (count > 0 && !SafeMemory::IsReadable(data, sizeof(T) * static_cast<size_t>(count)))
            return -1;

        return count;
    }

    static SDK::FInGameBattleCharacterData BuildData(
        int                  characterId,
        int                  variation,
        SDK::ACharacterGame* srcPawn)
    {
        SDK::FInGameBattleCharacterData data;
        memset(&data, 0, sizeof(data));

        int chNum = GetCharNum(characterId);
        int baseEmote = chNum * 10000;

        data._characterId = GetSDKCharacterId(characterId);
        data._variationId = variation;
        data._skillVariationCode = GetSkillVariationCode(characterId, variation);

        data._technique1Level = s_techAlpha;
        data._technique2Level = s_techBeta;
        data._technique3Level = s_techGamma;
        
        // Use CostumeHelper to get default costume if not explicitly set
        if (s_costumeCode != 0)
            data._costumeCode = s_costumeCode;
        else
            data._costumeCode = CostumeHelper::GetDefaultCostumeForCharacter(chNum);
        
        data._costumeAuraType = 5;

        // Emotes: copy from pawn if available, otherwise generate defaults
        if (!IsBadReadPtr(srcPawn, sizeof(SDK::ACharacterGame)) &&
            srcPawn->_emoteCodes.Num() > 0)
        {
            data._emoteCodes = srcPawn->_emoteCodes;
        }
        else
        {
            SDK::TArray<int> emotes;
            for (int s = 0; s < 8; s++)
                emotes.Add(baseEmote + s);
            data._emoteCodes = emotes;
        }

        // Override any slots the user explicitly set
        for (int s = 0; s < 8 && s < data._emoteCodes.Num(); s++)
            if (s_emoteSlot[s] != 0)
                data._emoteCodes[s] = s_emoteSlot[s];

        // Voice codes — copy from pawn if valid
        if (!IsBadReadPtr(srcPawn, sizeof(SDK::ACharacterGame)))
            data._voiceCodes = srcPawn->_voiceCodes;

        return data;
    }

    // ── Local player controller ───────────────────────────────────────────────
    static SDK::APlayerController* GetLocalPC()
    {
        __try
        {
            SDK::UWorld* World = GetWorldSafe();
            if (!World) return nullptr;
            if (IsBadReadPtr(World->OwningGameInstance, sizeof(SDK::UGameInstance))) return nullptr;
            if (World->OwningGameInstance->LocalPlayers.Num() == 0) return nullptr;
            SDK::ULocalPlayer* LP = World->OwningGameInstance->LocalPlayers[0];
            if (IsBadReadPtr(LP, sizeof(SDK::ULocalPlayer))) return nullptr;
            SDK::APlayerController* PC = LP->PlayerController;
            if (IsBadReadPtr(PC, sizeof(SDK::APlayerController))) return nullptr;
            return PC;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    // ── Get local player's team ID ────────────────────────────────────────────
    static uint8_t GetLocalPlayerTeam()
    {
        SDK::APlayerController* PC = GetLocalPC();
        if (!PC) return 0xFF;
        if (IsBadReadPtr(PC, sizeof(SDK::APlayerController))) return 0xFF;

        SDK::APawn* Pawn = PC->AcknowledgedPawn;
        if (IsBadReadPtr(Pawn, sizeof(SDK::APawn))) return 0xFF;

        auto* CharGame = reinterpret_cast<SDK::ACharacterGame*>(Pawn);
        int teamId = GetTeamIdFromSDK(CharGame);
        return teamId >= 0 ? (uint8_t)teamId : 0xFF;
    }

    // ── Check if actor is on same team ────────────────────────────────────────
    static bool IsSameTeam(SDK::AActor* Actor, uint8_t localTeam)
    {
        if (IsBadReadPtr(Actor, sizeof(SDK::AActor))) return false;
        if (localTeam == 0xFF) return false;

        auto* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
        int teamId = GetTeamIdFromSDK(CG);
        return teamId >= 0 && (uint8_t)teamId == localTeam;
    }

    // ── Fire RPC on one actor ─────────────────────────────────────────────────
    static void ApplyToActor(
        SDK::AActor* Actor,
        SDK::APlayerController* myPC,
        int                     characterId,
        int                     variation)
    {
        if (IsBadReadPtr(Actor, sizeof(SDK::AActor))) return;
        if (IsBadReadPtr(myPC, sizeof(SDK::APlayerController))) return;

        SDK::ACharacterGame* CharGame = reinterpret_cast<SDK::ACharacterGame*>(Actor);
        SDK::ACharacterBattle* CharPawn = static_cast<SDK::ACharacterBattle*>(Actor);
        if (IsBadReadPtr(CharGame, sizeof(SDK::ACharacterGame))) return;

        SDK::FInGameBattleCharacterData data = BuildData(characterId, variation, CharGame);

        SDK::AController* RawPC = CharGame->Controller;
        SDK::APlayerController* targetPC =
            (!IsBadReadPtr(RawPC, sizeof(SDK::AController)))
            ? reinterpret_cast<SDK::APlayerController*>(RawPC)
            : nullptr;

        SDK::APlayerControllerBattle* pcb = targetPC
            ? static_cast<SDK::APlayerControllerBattle*>(targetPC)
            : static_cast<SDK::APlayerControllerBattle*>(myPC);

        if (!IsBadReadPtr(pcb, sizeof(SDK::APlayerControllerBattle)))
        {
            FillRoleSlotWithGetterForChangeCharacter(data);
            ApplyTargetRegisteredRollSlot(CharPawn, characterId, variation);
            pcb->ChangeCharacter_OnServer(CharPawn, data);
        }
    }

    // ── Collect all other player pawns ────────────────────────────────────────
    static void CollectPlayerActors(
        SDK::UWorld* World,
        SDK::APawn* LocalPawn,
        std::vector<SDK::AActor*>& OutActors)
    {
        OutActors.clear();

        auto ProcessLevel = [&](SDK::ULevel* Level)
            {
                if (IsBadReadPtr(Level, sizeof(SDK::ULevel))) return;
                for (int i = 0; i < Level->Actors.Num(); i++)
                {
                    SDK::AActor* Actor = Level->Actors[i];
                    if (IsBadReadPtr(Actor, sizeof(SDK::AActor))) continue;
                    if (Actor == LocalPawn) continue;
                    if (!Actor->IsA(SDK::ACharacterGame::StaticClass())) continue;

                    SDK::ACharacterGame* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
                    if (IsBadReadPtr(CG, sizeof(SDK::ACharacterGame))) continue;

                    SDK::APlayerState* PS = CG->PlayerState;
                    if (IsBadReadPtr(PS, sizeof(SDK::APlayerState))) continue;

                    OutActors.push_back(Actor);
                }
            };

        ProcessLevel(World->PersistentLevel);
        for (int i = 0; i < World->StreamingLevels.Num(); i++)
        {
            SDK::ULevelStreaming* SL = World->StreamingLevels[i];
            if (!IsBadReadPtr(SL, sizeof(SDK::ULevelStreaming)))
                ProcessLevel(SL->GetLoadedLevel());
        }
    }

    // ── Apply to all other players ────────────────────────────────────────────
    static int ApplyToAllOtherPlayers(int characterId, int variation)
    {
        SDK::APlayerController* myPC = GetLocalPC();
        if (!myPC) return 0;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return 0;

        std::vector<SDK::AActor*> players;
        CollectPlayerActors(World, myPC->AcknowledgedPawn, players);

        int changed = 0;
        for (SDK::AActor* Actor : players)
        {
            ApplyToActor(Actor, myPC, characterId, variation);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ++changed;
        }
        return changed;
    }

    // ── Apply to enemies only ─────────────────────────────────────────────────
    static int ApplyToEnemiesOnly(int characterId, int variation)
    {
        SDK::APlayerController* myPC = GetLocalPC();
        if (!myPC) return 0;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return 0;

        uint8_t localTeam = GetLocalPlayerTeam();
        if (localTeam == 0xFF) return 0;  // Could not determine team

        std::vector<SDK::AActor*> players;
        CollectPlayerActors(World, myPC->AcknowledgedPawn, players);

        int changed = 0;
        for (SDK::AActor* Actor : players)
        {
            // Only change enemies, not teammates
            if (!IsSameTeam(Actor, localTeam))
            {
                ApplyToActor(Actor, myPC, characterId, variation);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ++changed;
            }
        }
        return changed;
    }

    // ── Apply to teammates only (NEW) ────────────────────────────────────────
    static int ApplyToTeammatesOnly(int characterId, int variation)
    {
        SDK::APlayerController* myPC = GetLocalPC();
        if (!myPC) return 0;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return 0;

        uint8_t localTeam = GetLocalPlayerTeam();
        if (localTeam == 0xFF) return 0;  // Could not determine team

        std::vector<SDK::AActor*> players;
        CollectPlayerActors(World, myPC->AcknowledgedPawn, players);

        int changed = 0;
        for (SDK::AActor* Actor : players)
        {
            // Only change teammates (same team, excluding self)
            if (IsSameTeam(Actor, localTeam))
            {
                ApplyToActor(Actor, myPC, characterId, variation);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ++changed;
            }
        }
        return changed;
    }

    // ── Public API ────────────────────────────────────────────────────────────

    int CollectPlayerList(PlayerInfo* outPlayers, int maxPlayers)
    {
        if (!outPlayers || maxPlayers <= 0) return 0;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return 0;

        // FIX: Get local pawn to exclude it, making the list consistent with ChangeCharacterIndividual
        SDK::APlayerController* myPC = GetLocalPC();
        SDK::APawn* localPawn = myPC ? myPC->AcknowledgedPawn : nullptr;

        std::vector<SDK::AActor*> actors;
        CollectPlayerActors(World, localPawn, actors);  // ← Now excludes self

        int count = 0;
        for (SDK::AActor* Actor : actors)
        {
            if (count >= maxPlayers) break;

            SDK::ACharacterGame* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
            if (IsBadReadPtr(CG, sizeof(SDK::ACharacterGame))) continue;

            SDK::APlayerState* PS = CG->PlayerState;
            if (IsBadReadPtr(PS, sizeof(SDK::APlayerState))) continue;

            // Player name comes from generated SDK getters instead of raw PlayerState fields.
            size_t i = CopyPlayerNameFromSDK(PS, outPlayers[count].name, sizeof(outPlayers[count].name));

            if (i == 0)
                snprintf(outPlayers[count].name, sizeof(outPlayers[count].name), "Player_%d", count);

            outPlayers[count].currentCharacterId = GetCharacterIdFromSDK(CG);
            outPlayers[count].characterIndex = GetCharacterIndexFromSDKId(outPlayers[count].currentCharacterId);
            outPlayers[count].teamId = GetTeamIdFromSDK(CG);
            outPlayers[count].gamePlayerId = GetGamePlayerIdFromSDK(PS);
            outPlayers[count].platform = GetPlatformFromSDK(PS);
            outPlayers[count].pingMs = ReadPlayerPingMs(PS);
            snprintf(outPlayers[count].platformName, sizeof(outPlayers[count].platformName), "%s", GetPlatformName((uint8_t)outPlayers[count].platform));
            if (outPlayers[count].platform == 0)
                snprintf(outPlayers[count].name, sizeof(outPlayers[count].name), "Bot_Player");
            outPlayers[count].index = count;

            count++;
        }

        return count;
    }

    int CollectLobbyPlayerList(PlayerInfo* outPlayers, int maxPlayers)
    {
        if (!outPlayers || maxPlayers <= 0) return 0;
        memset(outPlayers, 0, sizeof(PlayerInfo) * maxPlayers);

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;

        auto fillFromState = [&](SDK::APlayerState* PS, SDK::ACharacterGame* CG, int count) -> bool
            {
                if (IsBadReadPtr(PS, sizeof(SDK::APlayerState))) return false;

                PlayerInfo& info = outPlayers[count];
                info.index = count;
                info.currentCharacterId = -1;
                info.characterIndex = -1;
                info.teamId = -1;
                info.gamePlayerId = GetGamePlayerIdFromSDK(PS);
                info.platform = GetPlatformFromSDK(PS);
                info.pingMs = ReadPlayerPingMs(PS);
                snprintf(info.platformName, sizeof(info.platformName), "%s", GetPlatformName((uint8_t)info.platform));

                size_t len = CopyPlayerNameFromSDK(PS, info.name, sizeof(info.name));
                if (len == 0)
                    snprintf(info.name, sizeof(info.name), "Player_%d", count);
                if (info.platform == 0)
                    snprintf(info.name, sizeof(info.name), "Bot_Player");

                if (!IsBadReadPtr(CG, sizeof(SDK::ACharacterGame)))
                {
                    info.currentCharacterId = GetCharacterIdFromSDK(CG);
                    info.characterIndex = GetCharacterIndexFromSDKId(info.currentCharacterId);
                    info.teamId = GetTeamIdFromSDK(CG);
                }
                else
                {
                    info.teamId = GetTeamIdFromSDK(PS);
                    info.currentCharacterId = GetSpawnCharacterIdFromSDK(PS);
                    if (info.currentCharacterId >= 0)
                    {
                        info.characterIndex = GetCharacterIndexFromSDKId(info.currentCharacterId);
                    }
                }

                return true;
            };

        std::vector<SDK::APlayerState*> seen;
        int count = 0;

        auto addPlayer = [&](SDK::APlayerState* PS, SDK::ACharacterGame* CG)
            {
                if (count >= maxPlayers || IsBadReadPtr(PS, sizeof(SDK::APlayerState))) return;
                for (SDK::APlayerState* seenPS : seen)
                    if (seenPS == PS) return;

                if (fillFromState(PS, CG, count))
                {
                    seen.push_back(PS);
                    count++;
                }
            };

        if (!IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel)))
        {
            std::vector<SDK::AActor*> actors;
            CollectPlayerActors(World, nullptr, actors);
            for (SDK::AActor* Actor : actors)
            {
                if (count >= maxPlayers) break;
                SDK::ACharacterGame* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
                if (IsBadReadPtr(CG, sizeof(SDK::ACharacterGame))) continue;
                addPlayer(CG->PlayerState, CG);
            }
        }

        if (!IsBadReadPtr(World->GameState, sizeof(SDK::AGameStateBase)))
        {
            SDK::AGameStateBase* GS = World->GameState;
            for (int i = 0; i < GS->PlayerArray.Num() && count < maxPlayers; i++)
            {
                addPlayer(GS->PlayerArray[i], nullptr);
            }
        }

        return count;
    }

    int CollectPlayerNetworkInfo(PlayerNetworkInfo* outPlayers, int maxPlayers)
    {
        if (!outPlayers || maxPlayers <= 0) return 0;
        memset(outPlayers, 0, sizeof(PlayerNetworkInfo) * maxPlayers);

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return 0;

        SDK::APlayerController* localPC = GetLocalPC();
        SDK::APlayerState* localPS = (!IsBadReadPtr(localPC, sizeof(SDK::APlayerController)))
            ? localPC->PlayerState
            : nullptr;
        SDK::APawn* localPawn = (!IsBadReadPtr(localPC, sizeof(SDK::APlayerController)))
            ? localPC->AcknowledgedPawn
            : nullptr;

        std::vector<SDK::APlayerState*> seen;
        int count = 0;

        auto addPlayer = [&](SDK::APlayerState* PS, SDK::ACharacterGame* CG, bool hasActor)
            {
                if (count >= maxPlayers || IsBadReadPtr(PS, sizeof(SDK::APlayerState))) return;

                for (SDK::APlayerState* seenPS : seen)
                    if (seenPS == PS) return;

                PlayerNetworkInfo& info = outPlayers[count];
                info.index = count;
                info.teamId = -1;
                info.playerState = PS;
                info.actor = hasActor ? CG : nullptr;
                info.hasActor = hasActor;
                info.isLocal = (PS == localPS);
                info.pingMs = ReadPlayerPingMs(PS);
                info.platform = GetPlatformFromSDK(PS);
                info.gamePlayerId = GetGamePlayerIdFromSDK(PS);
                snprintf(info.platformName, sizeof(info.platformName), "%s", GetPlatformName((uint8_t)info.platform));

                if (CopyPlayerNameFromSDK(PS, info.displayName, sizeof(info.displayName)) == 0)
                    snprintf(info.displayName, sizeof(info.displayName), "N/A");
                snprintf(info.publicName, sizeof(info.publicName), "%s", info.displayName);
                if (CopyFStringToChar(PS->GetPlayerName(), info.privateName, sizeof(info.privateName)) == 0)
                    snprintf(info.privateName, sizeof(info.privateName), "N/A");

                if (!IsBadReadPtr(CG, sizeof(SDK::ACharacterGame)))
                {
                    info.teamId = GetTeamIdFromSDK(CG);
                }
                else
                {
                    info.teamId = GetTeamIdFromSDK(PS);
                }

                if (info.platform == 0 && info.displayName[0] == 'N' && info.displayName[1] == '/')
                    snprintf(info.displayName, sizeof(info.displayName), "Bot_Player");

                seen.push_back(PS);
                count++;
            };

        if (!IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel)))
        {
            std::vector<SDK::AActor*> actors;
            CollectPlayerActors(World, localPawn, actors);
            for (SDK::AActor* Actor : actors)
            {
                if (count >= maxPlayers) break;

                SDK::ACharacterGame* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
                if (IsBadReadPtr(CG, sizeof(SDK::ACharacterGame))) continue;
                addPlayer(CG->PlayerState, CG, true);
            }
        }

        if (!IsBadReadPtr(World->GameState, sizeof(SDK::AGameStateBase)))
        {
            SDK::AGameStateBase* GS = World->GameState;
            for (int i = 0; i < GS->PlayerArray.Num() && count < maxPlayers; i++)
            {
                SDK::APlayerState* PS = GS->PlayerArray[i];
                if (PS == localPS) continue;
                addPlayer(PS, nullptr, false);
            }
        }

        return count;
    }

    int GetLocalPingMs()
    {
        SDK::APlayerController* PC = GetLocalPC();
        if (IsBadReadPtr(PC, sizeof(SDK::APlayerController))) return -1;
        return ReadPlayerPingMs(PC->PlayerState);
    }

    bool CanReadServerConnectionInfo()
    {
        return GetWorldSafe() != nullptr;
    }

    bool GetCurrentServerRegion(char* outRegion, int maxLen)
    {
        if (!outRegion || maxLen <= 0) return false;
        outRegion[0] = '\0';

        if (!CanReadServerConnectionInfo())
            return false;

        SDK::UClass* settingClass = GetDbpSettingClassSafe();
        if (!settingClass) return false;

        SDK::TUObjectArray* objArray = GetGObjectsSafe();
        if (!objArray) return false;

        SDK::UDbpSetting* defaultSetting = GetDbpSettingDefaultSafe();
        SDK::UDbpSetting* setting = nullptr;
        const int32_t objectCount = GetObjectArrayCountSafe(objArray);
        for (int32_t i = 0; i < objectCount; i++)
        {
            SDK::UObject* obj = GetObjectByIndexSafe(objArray, i);
            if (!obj) continue;
            if (!IsObjectAClassSafe(obj, settingClass) || obj == defaultSetting) continue;

            setting = reinterpret_cast<SDK::UDbpSetting*>(obj);
            break;
        }

        if (!setting || !SafeMemory::IsReadable(setting, sizeof(SDK::UDbpSetting))) return false;

        SDK::FDbMatchingRegionSettingParam region{};
        if (!GetCurrentRegionSafe(setting, region))
            return false;

        if (CopyFStringToChar(region.Name, outRegion, (size_t)maxLen) == 0)
            snprintf(outRegion, (size_t)maxLen, "Region %d", region.code);

        return outRegion[0] != '\0';
    }

    bool GetCurrentServerConnectionInfo(ServerConnectionInfo* outInfo)
    {
        if (!outInfo) return false;
        memset(outInfo, 0, sizeof(ServerConnectionInfo));
        outInfo->pingMs = -1;

        if (!GetWorldSafe())
            return false;

        __try
        {
            outInfo->pingMs = GetLocalPingMs();
            outInfo->hasPing = outInfo->pingMs >= 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            outInfo->pingMs = -1;
            outInfo->hasPing = false;
        }

        outInfo->hasRegion = GetCurrentServerRegion(outInfo->region, sizeof(outInfo->region));

        return outInfo->hasRegion || outInfo->hasPing;
    }

    void ChangeCharacterIndividual(int playerIndex, int characterId, int variation)
    {
        SDK::APlayerController* myPC = GetLocalPC();
        if (!myPC) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return;

        std::vector<SDK::AActor*> players;
        CollectPlayerActors(World, myPC->AcknowledgedPawn, players);

        if (playerIndex < 0 || playerIndex >= (int)players.size()) return;

        ApplyToActor(players[playerIndex], myPC, characterId, variation);
    }

    bool ChangePlayerNetworkTargetToCh001(void* playerState)
    {
        SDK::APlayerState* targetPS = reinterpret_cast<SDK::APlayerState*>(playerState);
        if (IsBadReadPtr(targetPS, sizeof(SDK::APlayerState))) return false;

        SDK::APlayerController* myPC = GetLocalPC();
        if (!myPC) return false;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (IsBadReadPtr(World, sizeof(SDK::UWorld))) return false;
        if (IsBadReadPtr(World->PersistentLevel, sizeof(SDK::ULevel))) return false;

        std::vector<SDK::AActor*> players;
        CollectPlayerActors(World, myPC->AcknowledgedPawn, players);

        for (SDK::AActor* Actor : players)
        {
            SDK::ACharacterGame* CG = reinterpret_cast<SDK::ACharacterGame*>(Actor);
            if (IsBadReadPtr(CG, sizeof(SDK::ACharacterGame))) continue;
            if (CG->PlayerState != targetPS) continue;

            ApplyToActor(Actor, myPC, 502, 0);
            return true;
        }

        return false;
    }

    void ChangeCharacter(int characterId, int variation)
    {
        int chNum = GetCharNum(characterId);
        s_lastInGameChar = chNum;

        SDK::APlayerController* PC = GetLocalPC();
        if (!PC) return;

        SDK::APawn* Pawn = PC->AcknowledgedPawn;
        if (IsBadReadPtr(Pawn, sizeof(SDK::APawn))) return;

        SDK::ACharacterBattle* CB = static_cast<SDK::ACharacterBattle*>(Pawn);
        SDK::APlayerControllerBattle* PCB = static_cast<SDK::APlayerControllerBattle*>(PC);
        if (IsBadReadPtr(CB, sizeof(SDK::ACharacterBattle))) return;
        if (IsBadReadPtr(PCB, sizeof(SDK::APlayerControllerBattle))) return;

        SDK::FInGameBattleCharacterData data = BuildData(
            characterId, variation,
            static_cast<SDK::ACharacterGame*>(Pawn));

        FillRoleSlotWithGetterForChangeCharacter(data);
        PCB->ChangeCharacter_OnServer(CB, data);
    }

    void ChangeCharacterAllPlayers(int characterId, int variation)
    {
        ApplyToAllOtherPlayers(characterId, variation);
    }

    void ChangeCharacterAllPlayers_Start(int characterId, int variation)
    {
        s_TargetId = characterId;
        s_TargetVariation = variation;
        s_AllPlayersActive = true;
        s_EnemiesOnlyActive = false;  // Disable enemies-only mode
        s_TeammatesOnlyActive = false;  // Disable teammates-only mode
        s_LastApplyTime = {};
    }

    void ChangeCharacterAllPlayers_Stop()
    {
        s_AllPlayersActive = false;
    }

    void ChangeCharacterEnemiesOnly(int characterId, int variation)
    {
        ApplyToEnemiesOnly(characterId, variation);
    }

    void ChangeCharacterEnemiesOnly_Start(int characterId, int variation)
    {
        s_TargetId = characterId;
        s_TargetVariation = variation;
        s_EnemiesOnlyActive = true;
        s_AllPlayersActive = false;  // Disable all-players mode
        s_TeammatesOnlyActive = false;  // Disable teammates-only mode
        s_LastApplyTime = {};
    }

    void ChangeCharacterEnemiesOnly_Stop()
    {
        s_EnemiesOnlyActive = false;
    }

    // ── Team swap functions (NEW) ────────────────────────────────────────────
    void ChangeCharacterTeammatesOnly(int characterId, int variation)
    {
        ApplyToTeammatesOnly(characterId, variation);
    }

    void ChangeCharacterTeammatesOnly_Start(int characterId, int variation)
    {
        s_TargetId = characterId;
        s_TargetVariation = variation;
        s_TeammatesOnlyActive = true;
        s_AllPlayersActive = false;  // Disable all-players mode
        s_EnemiesOnlyActive = false;  // Disable enemies-only mode
        s_LastApplyTime = {};
    }

    void ChangeCharacterTeammatesOnly_Stop()
    {
        s_TeammatesOnlyActive = false;
    }

    bool IsTeammatesOnlyActive() { return s_TeammatesOnlyActive; }

    bool IsAllPlayersActive() { return s_AllPlayersActive; }
    bool IsEnemiesOnlyActive() { return s_EnemiesOnlyActive; }

    void Tick()
    {
        // Handle all-players persistent mode
        if (s_AllPlayersActive)
        {
            auto  now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - s_LastApplyTime).count();
            if (elapsed >= s_RetryIntervalSec)
            {
                s_LastApplyTime = now;
                ApplyToAllOtherPlayers(s_TargetId, s_TargetVariation);
            }
        }

        // Handle enemies-only persistent mode
        if (s_EnemiesOnlyActive)
        {
            auto  now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - s_LastApplyTime).count();
            if (elapsed >= s_RetryIntervalSec)
            {
                s_LastApplyTime = now;
                ApplyToEnemiesOnly(s_TargetId, s_TargetVariation);
            }
        }

        // Handle teammates-only persistent mode (NEW)
        if (s_TeammatesOnlyActive)
        {
            auto  now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - s_LastApplyTime).count();
            if (elapsed >= s_RetryIntervalSec)
            {
                s_LastApplyTime = now;
                ApplyToTeammatesOnly(s_TargetId, s_TargetVariation);
            }
        }
    }

    void SpawnMudClones(int numClones)
    {
        SDK::APlayerController* PC = GetLocalPC();
        if (!PC) return;

        SDK::APawn* Pawn = PC->AcknowledgedPawn;
        if (!Pawn) return;

        auto* Battle = AsCharacterBattle(static_cast<SDK::AActor*>(Pawn));
        if (!Battle) return;

        SDK::APlayerStateBattle* PlayerStateBattle = Battle->BP_GetPlayerStateBattle();
        if (!PlayerStateBattle || IsBadReadPtr(PlayerStateBattle, sizeof(SDK::APlayerStateBattle))) return;

        SDK::UDuplicateControlComponent* DuplicateComp = PlayerStateBattle->BP_GetDuplicateController();
        if (!DuplicateComp || IsBadReadPtr(DuplicateComp, sizeof(SDK::UDuplicateControlComponent))) return;

        // Get player location
        SDK::FVector PlayerLocation = Battle->K2_GetActorLocation();

        for (int i = 0; i < numClones; ++i)
        {
            // Offset clones around the player
            SDK::FVector SpawnLocation = PlayerLocation;
            SpawnLocation.X += (i - numClones / 2) * 100.0f;
            SpawnLocation.Y += 50.0f;

            // Call RPC to create clone
            DuplicateComp->DuplicateInto_RPC_ToServer(
                Battle,
                SpawnLocation
            );

        }
    }

} // namespace Cheats
