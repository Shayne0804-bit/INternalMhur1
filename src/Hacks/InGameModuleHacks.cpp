#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include "InGameModuleHacks.h"
#include "Character_Changer.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/OutGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CommonModule_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/BackendSubsystem_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../Utils/Logger.h"
#include "../Utils/SafeMemory.h"
#include "../Menu/ImGuiMenu.h"
#include <set>
#include <map>
#include <cctype>
#include <string>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <Windows.h>

namespace
{
    constexpr bool RUNTIME_FILE_DEBUG_LOGGING = false;
    constexpr ULONGLONG BATTLE_MODE_CACHE_MS = 250;
    constexpr uintptr_t INFINITE_OBJECTS_PATCH_OFFSET = 0x3ED21F5;
    constexpr size_t INFINITE_OBJECTS_PATCH_SIZE = 3;
    constexpr BYTE INFINITE_OBJECTS_ORIGINAL_BYTES[INFINITE_OBJECTS_PATCH_SIZE] = { 0x89, 0x51, 0x08 };
    constexpr BYTE INFINITE_OBJECTS_PATCH_BYTES[INFINITE_OBJECTS_PATCH_SIZE] = { 0x90, 0x90, 0x90 };

    static std::mutex g_InfiniteObjectsPatchMutex;
    static bool g_InfiniteObjectsPatchApplied = false;
    static bool g_InfiniteObjectsMismatchLogged = false;

    static void WriteRuntimeDebugLog(const char* path, const std::string& message)
    {
        (void)path;
        (void)message;
    }

    static BYTE* GetInfiniteObjectsPatchAddress()
    {
        HMODULE module = GetModuleHandleW(L"MHUR.exe");
        if (!module)
            module = GetModuleHandleW(nullptr);

        if (!module)
            return nullptr;

        return reinterpret_cast<BYTE*>(reinterpret_cast<uintptr_t>(module) + INFINITE_OBJECTS_PATCH_OFFSET);
    }

    static bool PatchBytesMatch(const BYTE* address, const BYTE* expectedBytes)
    {
        if (!SafeMemory::IsReadable(address, INFINITE_OBJECTS_PATCH_SIZE))
            return false;

        __try
        {
            return std::memcmp(address, expectedBytes, INFINITE_OBJECTS_PATCH_SIZE) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool WritePatchBytes(BYTE* address, const BYTE* bytes)
    {
        if (!address)
            return false;

        DWORD oldProtect = 0;
        if (!VirtualProtect(address, INFINITE_OBJECTS_PATCH_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        bool success = false;
        __try
        {
            std::memcpy(address, bytes, INFINITE_OBJECTS_PATCH_SIZE);
            success = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            success = false;
        }

        DWORD unusedProtect = 0;
        VirtualProtect(address, INFINITE_OBJECTS_PATCH_SIZE, oldProtect, &unusedProtect);

        if (success)
            FlushInstructionCache(GetCurrentProcess(), address, INFINITE_OBJECTS_PATCH_SIZE);

        return success;
    }
}

static inline BOOL SafeIsBadReadPtr(const void* ptr, UINT_PTR size)
{
    return !SafeMemory::IsReadable(ptr, static_cast<size_t>(size));
}

#define IsBadReadPtr SafeIsBadReadPtr

// ============================================
// EXTERNAL DECLARATIONS FROM BASIC.CPP
// ============================================

// Forward declarations from Basic.cpp
extern "C" SDK::APlayerController* SDK_GetPlayerController();
extern "C" SDK::AActor* SDK_GetRandomESPTarget();  // Returns random ACharacterBattle as AActor*
extern "C" SDK::AActor* SDK_GetForwardESPTarget();  // Returns closest ACharacterBattle in front as AActor*
extern "C" SDK::AActor* SDK_GetBulletTPFOVTarget();  // Returns closest valid BulletTP FOV target
extern "C" SDK::AActor* SDK_GetBulletTPFOVTargetWithPosition(SDK::FVector* OutTargetPosition);
extern "C" const char* SDK_GetPlayerName(void* PlayerState);  // Get player name from PlayerState

// UTILITY FUNCTIONS
// ============================================

static inline bool TryGetGameModeTypeSafe(SDK::AHerovsGameState* gameState, SDK::EGameModeType& modeType)
{
    if (!SafeMemory::IsReadable(gameState, sizeof(void*)))
        return false;

    __try
    {
        modeType = gameState->GetGameModeType();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static inline bool TryGetObjectArrayCountSafe(SDK::TUObjectArray* gObjects, int32_t& objectCount)
{
    if (!SafeMemory::IsReadable(gObjects, sizeof(SDK::TUObjectArray)))
        return false;

    __try
    {
        objectCount = gObjects->Num();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static inline SDK::UObject* GetObjectByIndexSafe(SDK::TUObjectArray* gObjects, int32_t index)
{
    if (!SafeMemory::IsReadable(gObjects, sizeof(SDK::TUObjectArray)))
        return nullptr;

    __try
    {
        return gObjects->GetByIndex(index);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// Offsets for PlayerState (from Basic.cpp)
#define OFFSET_PLAYERSTATE 0x6E0          // AActor::PlayerState
#define OFFSET_PLAYERSTATE_IFDYING 1416   // Dying flag at +1400

/**
 * Check if a character is dying using PlayerState offset
 * Same method as Basic.cpp to avoid crashes
 */
static inline bool IsCharacterDyingOffset(SDK::ACharacterBattle* Character)
{
    if (!SafeMemory::IsReadable(Character, sizeof(void*))) return false;
    try
    {
        // Read PlayerState pointer from Character
        void* PlayerState = nullptr;
        auto* playerStateAddress = reinterpret_cast<void**>((uintptr_t)Character + OFFSET_PLAYERSTATE);
        if (!SafeMemory::TryRead(playerStateAddress, PlayerState) ||
            !SafeMemory::IsReadable(PlayerState, OFFSET_PLAYERSTATE_IFDYING + sizeof(uint8_t)))
        {
            return false;
        }

        // Check dying flag at offset 1400 (bit 2)
        uint8_t dyingFlags = 0;
        if (!SafeMemory::TryRead(reinterpret_cast<uint8_t*>((uintptr_t)PlayerState + OFFSET_PLAYERSTATE_IFDYING), dyingFlags))
            return false;

        bool isDying = (dyingFlags & 2) != 0;
        return isDying;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Check if current game mode is a valid battle mode (2-9)
 * Valid modes: SOLO_BATTLE(2), DUO_BATTLE(3), SQUAD_BATTLE(4), LEADERS_BATTLE(5),
 *             DOMINATION_BATTLE(6), SOLOPICK_BATTLE(7), TUTORIAL(8), TRAINING(9)
 */
bool IsValidBattleMode()
{
    static ULONGLONG lastCheckTime = 0;
    static bool hasCachedResult = false;
    static bool cachedResult = false;

    const ULONGLONG now = GetTickCount64();
    if (hasCachedResult && (now - lastCheckTime) < BATTLE_MODE_CACHE_MS)
        return cachedResult;

    auto updateCache = [&](bool result) -> bool
    {
        cachedResult = result;
        lastCheckTime = now;
        hasCachedResult = true;
        return result;
    };

    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!SafeMemory::IsReadable(world, sizeof(void*)))
        {
            return updateCache(false);
        }

        // Get GameState
        SDK::AGameStateBase* baseGameState = nullptr;
        if (!SafeMemory::TryRead(&world->GameState, baseGameState))
        {
            return updateCache(false);
        }

        SDK::AHerovsGameState* gameState = static_cast<SDK::AHerovsGameState*>(baseGameState);
        if (!SafeMemory::IsReadable(gameState, sizeof(void*)))
        {
            return updateCache(false);
        }

        // Get GameModeType using public method (no offsets)
        SDK::EGameModeType modeType{};
        if (!TryGetGameModeTypeSafe(gameState, modeType))
        {
            return updateCache(false);
        }

        // Check if valid battle mode (2-9) - cast to int for comparison
        int modeValue = (int)modeType;
        bool isValid = (modeValue >= 2 && modeValue <= 9);

        return updateCache(isValid);
    }
    catch (const std::exception&)
    {
        return updateCache(false);
    }
    catch (...)
    {
        return updateCache(false);
    }
}

/**
 * Validate if a pointer is safe to access
 * Checks for NULL and obviously invalid addresses
 */
template<typename T>
static inline bool IsValidPointer(T* ptr)
{
    return SafeMemory::IsReadable(ptr, sizeof(void*));
}

template<typename T>
static inline bool SafeReadMember(const T* memberAddress, T& out)
{
    return SafeMemory::TryRead(memberAddress, out);
}

static inline bool IsObjectDefaultSafe(SDK::UObject* object);
static inline bool IsObjectAUnsafeGuarded(SDK::UObject* object, SDK::UClass* klass);

template<typename T>
static inline bool SafeArrayCount(const UC::TArray<T>& array, int32_t& count, int32_t maxReasonableCount = 200000)
{
    if (!SafeMemory::IsReadable(&array, sizeof(array)))
        return false;

    int32_t num = 0;
    int32_t max = 0;
    const T* data = nullptr;

    try
    {
        num = array.Num();
        max = array.Max();
        data = array.GetDataPtr();
    }
    catch (...)
    {
        return false;
    }

    if (num < 0 || max < num || num > maxReasonableCount)
        return false;

    if (num > 0 && !SafeMemory::IsReadable(data, sizeof(T) * static_cast<size_t>(num)))
        return false;

    count = num;
    return true;
}

template<typename T>
static inline bool SafeArrayGet(const UC::TArray<T>& array, int32_t index, T& out, int32_t maxReasonableCount = 200000)
{
    int32_t count = 0;
    if (!SafeArrayCount(array, count, maxReasonableCount) || index < 0 || index >= count)
        return false;

    const T* data = nullptr;
    try
    {
        data = array.GetDataPtr();
    }
    catch (...)
    {
        return false;
    }

    if (!SafeMemory::IsReadable(data + index, sizeof(T)))
        return false;

    return SafeMemory::TryRead(data + index, out);
}

static inline SDK::ULevel* GetPersistentLevelSafe(SDK::UWorld* world)
{
    if (!IsValidPointer(world))
        return nullptr;

    SDK::ULevel* level = nullptr;
    if (!SafeReadMember(&world->PersistentLevel, level) || !IsValidPointer(level))
        return nullptr;

    return level;
}

static inline bool GetWorldActorsCountSafe(SDK::UWorld* world, int32_t& count)
{
    SDK::ULevel* level = GetPersistentLevelSafe(world);
    return level && SafeArrayCount(level->Actors, count, 50000);
}

static inline SDK::AActor* GetWorldActorSafe(SDK::UWorld* world, int32_t index)
{
    SDK::ULevel* level = GetPersistentLevelSafe(world);
    if (!level)
        return nullptr;

    SDK::AActor* actor = nullptr;
    if (!SafeArrayGet(level->Actors, index, actor, 50000) || !IsValidPointer(actor))
        return nullptr;

    return actor;
}

static inline SDK::APawn* GetPawnSafe(SDK::APlayerController* playerController)
{
    if (!IsValidPointer(playerController))
        return nullptr;

    SDK::APawn* pawn = nullptr;
    if (!SafeReadMember(&playerController->Pawn, pawn) || !IsValidPointer(pawn))
        return nullptr;

    return pawn;
}

static inline SDK::APlayerState* GetControllerPlayerStateSafe(SDK::APlayerController* playerController)
{
    if (!IsValidPointer(playerController))
        return nullptr;

    SDK::APlayerState* playerState = nullptr;
    if (!SafeReadMember(&playerController->PlayerState, playerState) || !IsValidPointer(playerState))
        return nullptr;

    return playerState;
}

static inline SDK::APlayerState* GetCharacterPlayerStateSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    SDK::APlayerState* playerState = nullptr;
    if (!SafeReadMember(&character->PlayerState, playerState) || !IsValidPointer(playerState))
        return nullptr;

    return playerState;
}

static inline SDK::AGameStateBase* GetGameStateSafe(SDK::UWorld* world)
{
    if (!IsValidPointer(world))
        return nullptr;

    SDK::AGameStateBase* gameState = nullptr;
    if (!SafeReadMember(&world->GameState, gameState) || !IsValidPointer(gameState))
        return nullptr;

    return gameState;
}

static inline SDK::UBuffParam* GetBuffParamSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
        return nullptr;

    SDK::UBuffParam* buffParam = nullptr;
    if (!SafeReadMember(&playerState->_buffParam, buffParam) || !IsValidPointer(buffParam))
        return nullptr;

    return buffParam;
}

static inline SDK::USupplyHolderComponent* GetSupplyHolderComponentSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
        return nullptr;

    SDK::USupplyHolderComponent* supplyHolderComp = nullptr;
    if (!SafeReadMember(&playerState->_supplyHolderComponent, supplyHolderComp) || !IsValidPointer(supplyHolderComp))
        return nullptr;

    return supplyHolderComp;
}

static inline SDK::UCharacterActionControlComponent* GetActionControlComponentSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    SDK::UCharacterActionControlComponent* actionControlComp = nullptr;
    if (!SafeReadMember(&character->_actionControlComponent, actionControlComp) || !IsValidPointer(actionControlComp))
        return nullptr;

    return actionControlComp;
}

static inline bool IsNonNoneFName(const SDK::FName& name)
{
    return name.ComparisonIndex != 0;
}

static inline SDK::FName GetUseItemSupplyIdSafe(SDK::APlayerStateBattle* playerState)
{
    SDK::FName supplyId{};

    SDK::USupplyHolderComponent* supplyHolderComp = GetSupplyHolderComponentSafe(playerState);
    if (!IsValidPointer(supplyHolderComp))
        return supplyId;

    if (SafeReadMember(&supplyHolderComp->_shortCutSupplyId, supplyId) && IsNonNoneFName(supplyId))
        return supplyId;

    supplyId = SDK::FName();
    if (SafeReadMember(&supplyHolderComp->_lastUsedSupplyId, supplyId) && IsNonNoneFName(supplyId))
        return supplyId;

    return SDK::FName();
}

static inline bool TrySetUseItemSupplyIdSafe(SDK::UActionAttackUseItem* useItemAction, const SDK::FName& supplyId)
{
    if (!IsValidPointer(useItemAction) || !IsNonNoneFName(supplyId))
        return false;

    __try
    {
        useItemAction->BP_SetSupplyId(supplyId);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static inline SDK::FVector_NetQuantize100 GetActorLocationNetQuantize100Safe(SDK::ACharacterBattle* character)
{
    SDK::FVector_NetQuantize100 targetLocation{};

    if (!IsValidPointer(character))
        return targetLocation;

    __try
    {
        SDK::FVector playerLocation = character->K2_GetActorLocation();
        targetLocation.X = playerLocation.X;
        targetLocation.Y = playerLocation.Y;
        targetLocation.Z = playerLocation.Z;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        targetLocation = SDK::FVector_NetQuantize100();
    }

    return targetLocation;
}

static inline SDK::USkillManagementComponent* GetSkillManagementComponentSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    SDK::USkillManagementComponent* skillComponent = nullptr;
    if (!SafeReadMember(&character->_skillManagementComponent, skillComponent) || !IsValidPointer(skillComponent))
        return nullptr;

    return skillComponent;
}

static inline SDK::UProjectileReplicateBattleComponent* GetProjectileReplicatorSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    SDK::UProjectileReplicateBattleComponent* projectileComp = nullptr;
    if (!SafeReadMember(&character->_projectileReplicator, projectileComp) || !IsValidPointer(projectileComp))
        return nullptr;

    return projectileComp;
}

static inline SDK::UCharacterConditionControlComponent* GetConditionControlComponentSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    SDK::UCharacterConditionControlComponent* conditionComponent = nullptr;
    __try
    {
        conditionComponent = character->BP_GetConditionControlComponent();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }

    if (!IsValidPointer(conditionComponent))
        return nullptr;

    return conditionComponent;
}

static inline SDK::APlayerStateBattle* GetPlayerStateBattleFromCharacterSafe(SDK::ACharacterBattle* character)
{
    SDK::APlayerState* playerState = GetCharacterPlayerStateSafe(character);
    if (!playerState)
        return nullptr;

    SDK::APlayerStateBattle* battleState = static_cast<SDK::APlayerStateBattle*>(playerState);
    if (!IsValidPointer(battleState))
        return nullptr;

    return battleState;
}

static inline SDK::UCharacterRollSlotUniqueSkillControlComponent* GetRollSlotControlComponentSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
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

    if (!IsValidPointer(rollSlotCtrl))
        return nullptr;

    return rollSlotCtrl;
}

static void WriteTogaRollSlotLog(const std::string& message)
{
    (void)message;
}

static void WriteCurrentRoleSlotLog(const std::string& message)
{
    (void)message;
}

static const char* AbilityTypeName(SDK::EMdAbilityType value)
{
    switch (value)
    {
    case SDK::EMdAbilityType::Invalid: return "Invalid";
    case SDK::EMdAbilityType::Undef: return "Undef";
    case SDK::EMdAbilityType::Power: return "Power";
    case SDK::EMdAbilityType::Support: return "Support";
    case SDK::EMdAbilityType::Speed: return "Speed";
    case SDK::EMdAbilityType::Defense: return "Defense";
    case SDK::EMdAbilityType::Technique: return "Technique";
    case SDK::EMdAbilityType::Special: return "Special";
    case SDK::EMdAbilityType::Max: return "Max";
    default: return "Unknown";
    }
}

static const char* CharacterAssignName(SDK::EMdCharacterAssign value)
{
    switch (value)
    {
    case SDK::EMdCharacterAssign::Invalid: return "Invalid";
    case SDK::EMdCharacterAssign::Undef: return "Undef";
    case SDK::EMdCharacterAssign::HERO: return "HERO";
    case SDK::EMdCharacterAssign::VILLAIN: return "VILLAIN";
    case SDK::EMdCharacterAssign::Max: return "Max";
    default: return "Unknown";
    }
}

static void AppendRoleSlotEffectMasterSummary(std::stringstream& ss, const std::string& indent, const char* name, const SDK::FMasterDataRoleSlotEffect& effect)
{
    ss << "\n" << indent << name << ":"
       << "\n" << indent << "  code: " << effect.code
       << "\n" << indent << "  nameKey: " << effect.Name
       << "\n" << indent << "  descKey: " << effect.Description
       << "\n" << indent << "  levelUpDescKey: " << effect.LevelUpDescription
       << "\n" << indent << "  group: " << effect.GroupCode
       << "\n" << indent << "  levels: [" << effect.Level1 << ", " << effect.Level2 << ", " << effect.Level3
       << ", " << effect.Level4 << ", " << effect.Level5 << ", " << effect.Level6
       << ", " << effect.Level7 << ", " << effect.Level8 << ", " << effect.Level9
       << ", " << effect.Level10 << ", " << effect.Level11 << "]"
       << "\n" << indent << "  subEffects: [" << effect.SubEffect1 << ", " << effect.SubEffect2 << ", " << effect.SubEffect3 << "]";
}

static void AppendRoleSlotTypeSummary(std::stringstream& ss, const std::string& indent, const SDK::FDbsRoleSlotType& type)
{
    ss << "\n" << indent << "type:"
       << "\n" << indent << "  code: " << type.code
       << "\n" << indent << "  targetRole: " << static_cast<int>(type.TargetRole)
       << "\n" << indent << "  targetRoleName: " << AbilityTypeName(type.TargetRole)
       << "\n" << indent << "  masterCode: " << type.RoleSlotType.code
       << "\n" << indent << "  masterTargetRole: " << type.RoleSlotType.TargetRole
       << "\n" << indent << "  masterTargetAssign: " << static_cast<int>(type.RoleSlotType.TargetAssign)
       << "\n" << indent << "  masterTargetAssignName: " << CharacterAssignName(type.RoleSlotType.TargetAssign);
}

static void AppendRoleSlotEffectSummary(std::stringstream& ss, const std::string& indent, const SDK::FDbsRoleSlotEffect& effect)
{
    ss << "\n" << indent << "effect:"
       << "\n" << indent << "  code: " << effect.code;
    AppendRoleSlotEffectMasterSummary(ss, indent + "  ", "primary", effect.RoleSlotEffect);
    AppendRoleSlotEffectMasterSummary(ss, indent + "  ", "secondary", effect.RoleSlotEffect2);
}

static void AppendRoleSlotEntrySummary(std::stringstream& ss, const std::string& indent, const std::string& label, const SDK::FDbsRoleSlot& slot)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  index: " << slot.Index
       << "\n" << indent << "  level: " << slot.Level
       << "\n" << indent << "  unique: " << (slot.UniqueSlot ? 1 : 0)
       << "\n" << indent << "  initLevel: " << slot.InitLevel
       << "\n" << indent << "  initLevelCap: " << slot.InitLevelCap
       << "\n" << indent << "  maxLevel: " << slot.MaxLevel
       << "\n" << indent << "  levelUpItem: " << slot.LevelUpItem
       << "\n" << indent << "  levelUpItemNum: " << slot.LevelUpItemNum
       << "\n" << indent << "  unlockedItem: " << slot.UnlockedItem
       << "\n" << indent << "  unlockedItemNum: " << slot.UnlockedItemNum
       << "\n" << indent << "  unlockedLevelCapUp: " << (slot.UnlockedLevelCapUp ? 1 : 0)
       << "\n" << indent << "  tipCode: " << slot.TipCode;

    AppendRoleSlotTypeSummary(ss, indent + "  ", slot.Type);
    AppendRoleSlotEffectSummary(ss, indent + "  ", slot.Effect);
}

static void AppendCurrentRoleSlotArraySummary(std::stringstream& ss, const char* name, const SDK::TArray<SDK::FDbsRoleSlot>& array)
{
    int32_t count = 0;
    if (!SafeArrayCount(array, count, 64))
    {
        ss << "\n  " << name << ":"
           << "\n    count: -1";
        return;
    }

    ss << "\n  " << name << ":"
       << "\n    count: " << count;

    if (count <= 0)
        return;

    ss << "\n    entries:";

    for (int32_t i = 0; i < count; ++i)
    {
        SDK::FDbsRoleSlot slot{};
        if (!SafeArrayGet(array, i, slot, 64))
            break;

        std::stringstream label;
        label << name << "[" << i << "]";
        AppendRoleSlotEntrySummary(ss, "      ", label.str(), slot);
    }
}

static void AppendCurrentRoleSlotCostumeSummary(std::stringstream& ss, const SDK::FMasterDataCustomizeCostume& costume)
{
    ss << "\n  costume:"
       << "\n    code: " << costume.code
       << "\n    character: " << costume.Character
       << "\n    group: " << costume.GroupCode
       << "\n    rarity: " << static_cast<int>(costume.Rarity)
       << "\n    shopItemCode: " << costume.shopItemCode
       << "\n    descriptionKey: " << costume.Description
       << "\n    obtainFromKey: " << costume.ObtainFrom
       << "\n    baseNameKey: " << costume.BaseName
       << "\n    subNameKey: " << costume.SubName
       << "\n    seriesNormal: " << costume.SeriesNormal
       << "\n    cpuEquip: " << costume.CpuEquip
       << "\n    roleSlotPattern: " << costume.RoleSlotPattern
       << "\n    roleSlots: [" << costume.RoleSlot1 << ", " << costume.RoleSlot2 << ", " << costume.RoleSlot3
       << ", " << costume.RoleSlot4 << ", " << costume.RoleSlot5 << ", " << costume.RoleSlot6
       << ", " << costume.RoleSlot7 << ", " << costume.RoleSlot8 << ", " << costume.RoleSlot9
       << ", " << costume.RoleSlot10 << "]"
       << "\n    uniqueRoleSlots: [" << costume.RoleSlotUnique1 << ", " << costume.RoleSlotUnique2 << "]";
}

struct RoleSlotPatternSlotDebug
{
    int initLevel;
    int maxLevel;
    int levelUpItem;
    int levelUpItemNum;
    int unlockedItem;
    int unlockedItemNum;
    int unlockedLevelCapUp;
};

static void AppendPatternSlotSummary(std::stringstream& ss, const std::string& indent, int index, const RoleSlotPatternSlotDebug& slot)
{
    ss << "\n" << indent << "slot" << index << ":"
       << "\n" << indent << "  initLevel: " << slot.initLevel
       << "\n" << indent << "  maxLevel: " << slot.maxLevel
       << "\n" << indent << "  levelUpItem: " << slot.levelUpItem
       << "\n" << indent << "  levelUpItemNum: " << slot.levelUpItemNum
       << "\n" << indent << "  unlockedItem: " << slot.unlockedItem
       << "\n" << indent << "  unlockedItemNum: " << slot.unlockedItemNum
       << "\n" << indent << "  unlockedLevelCapUp: " << slot.unlockedLevelCapUp;
}

static void AppendCurrentRoleSlotPatternSummary(std::stringstream& ss, const SDK::FMasterDataRoleSlotPattern& pattern)
{
    const RoleSlotPatternSlotDebug slots[10] = {
        { pattern.Slot1InitLevel, pattern.Slot1MaxLevel, pattern.Slot1LevelupItem, pattern.Slot1LevelupItemNum, pattern.Slot1UnlockedItem, pattern.Slot1UnlockedItemNum, pattern.Slot1UnlockedLevelCapUp },
        { pattern.Slot2InitLevel, pattern.Slot2MaxLevel, pattern.Slot2LevelupItem, pattern.Slot2LevelupItemNum, pattern.Slot2UnlockedItem, pattern.Slot2UnlockedItemNum, pattern.Slot2UnlockedLevelCapUp },
        { pattern.Slot3InitLevel, pattern.Slot3MaxLevel, pattern.Slot3LevelupItem, pattern.Slot3LevelupItemNum, pattern.Slot3UnlockedItem, pattern.Slot3UnlockedItemNum, pattern.Slot3UnlockedLevelCapUp },
        { pattern.Slot4InitLevel, pattern.Slot4MaxLevel, pattern.Slot4LevelupItem, pattern.Slot4LevelupItemNum, pattern.Slot4UnlockedItem, pattern.Slot4UnlockedItemNum, pattern.Slot4UnlockedLevelCapUp },
        { pattern.Slot5InitLevel, pattern.Slot5MaxLevel, pattern.Slot5LevelupItem, pattern.Slot5LevelupItemNum, pattern.Slot5UnlockedItem, pattern.Slot5UnlockedItemNum, pattern.Slot5UnlockedLevelCapUp },
        { pattern.Slot6InitLevel, pattern.Slot6MaxLevel, pattern.Slot6LevelupItem, pattern.Slot6LevelupItemNum, pattern.Slot6UnlockedItem, pattern.Slot6UnlockedItemNum, pattern.Slot6UnlockedLevelCapUp },
        { pattern.Slot7InitLevel, pattern.Slot7MaxLevel, pattern.Slot7LevelupItem, pattern.Slot7LevelupItemNum, pattern.Slot7UnlockedItem, pattern.Slot7UnlockedItemNum, pattern.Slot7UnlockedLevelCapUp },
        { pattern.Slot8InitLevel, pattern.Slot8MaxLevel, pattern.Slot8LevelupItem, pattern.Slot8LevelupItemNum, pattern.Slot8UnlockedItem, pattern.Slot8UnlockedItemNum, pattern.Slot8UnlockedLevelCapUp },
        { pattern.Slot9InitLevel, pattern.Slot9MaxLevel, pattern.Slot9LevelupItem, pattern.Slot9LevelupItemNum, pattern.Slot9UnlockedItem, pattern.Slot9UnlockedItemNum, pattern.Slot9UnlockedLevelCapUp },
        { pattern.Slot10InitLevel, pattern.Slot10MaxLevel, pattern.Slot10LevelupItem, pattern.Slot10LevelupItemNum, pattern.Slot10UnlockedItem, pattern.Slot10UnlockedItemNum, pattern.Slot10UnlockedLevelCapUp },
    };

    ss << "\n  pattern:"
       << "\n    code: " << pattern.code
       << "\n    slotInitNum: " << pattern.SlotInitNum
       << "\n    slots:";

    for (int i = 0; i < 10; ++i)
    {
        AppendPatternSlotSummary(ss, "      ", i + 1, slots[i]);
    }

    ss << "\n    uniqueSlots:"
       << "\n      uniqueSlot1:"
       << "\n        initLevel: " << pattern.UniqueSlot1InitLevel
       << "\n        initLevelCap: " << pattern.UniqueSlot1InitLevelCap
       << "\n        levelUpItem: " << pattern.UniqueSlot1LevelupItem
       << "\n        levelUpItemNum: " << pattern.UniqueSlot1LevelupItemNum
       << "\n      uniqueSlot2:"
       << "\n        initLevel: " << pattern.UniqueSlot2InitLevel
       << "\n        initLevelCap: " << pattern.UniqueSlot2InitLevelCap
       << "\n        levelUpItem: " << pattern.UniqueSlot2LevelupItem
       << "\n        levelUpItemNum: " << pattern.UniqueSlot2LevelupItemNum;
}

static void AppendCurrentRoleSlotParamSummary(std::stringstream& ss, const char* label, const SDK::FDbsCostumeRoleSlotParam& param, int costumeCode, bool hasBoolResult, bool boolResult)
{
    ss << "\n\n=== " << label << " ===";

    if (hasBoolResult)
        ss << "\n  result: " << (boolResult ? 1 : 0);

    ss << "\n  inputCostume: " << costumeCode
       << "\n  unlockedRoleSlot: " << param.UnlockedRoleSlot;

    AppendCurrentRoleSlotCostumeSummary(ss, param.Costume);
    AppendCurrentRoleSlotPatternSummary(ss, param.RoleSlotPattern);

    AppendCurrentRoleSlotArraySummary(ss, "slotList", param.SlotList);
    AppendCurrentRoleSlotArraySummary(ss, "skillList", param.SkillList);
}

static SDK::UBackendSubsystem* GetBackendSubsystemSafe()
{
    __try
    {
        SDK::UBackendSubsystem* backendSubsystem = SDK::UBackendSubsystem::GetDefaultObj();
        return IsValidPointer(backendSubsystem) ? backendSubsystem : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static SDK::UDatabaseParams* GetDatabaseParamsSafe(SDK::UBackendSubsystem* backendSubsystem)
{
    if (!IsValidPointer(backendSubsystem))
        return nullptr;

    __try
    {
        SDK::UDatabaseParams* dbParams = backendSubsystem->GetDatabaseParams();
        return IsValidPointer(dbParams) ? dbParams : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static SDK::UDbpSeason* GetSeasonDataSafe(SDK::UDatabaseParams* dbParams)
{
    if (!IsValidPointer(dbParams))
        return nullptr;

    __try
    {
        SDK::UDbpSeason* seasonData = dbParams->GetSeasonData();
        return IsValidPointer(seasonData) ? seasonData : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static SDK::UDatabaseParams* GetDatabaseParamsMemberSafe(SDK::UBackendSubsystem* backendSubsystem)
{
    if (!IsValidPointer(backendSubsystem))
        return nullptr;

    SDK::UDatabaseParams* dbParams = nullptr;
    if (!SafeReadMember(&backendSubsystem->_params, dbParams))
        return nullptr;

    return IsValidPointer(dbParams) ? dbParams : nullptr;
}

static SDK::UDbpSeason* GetSeasonDataMemberSafe(SDK::UDatabaseParams* dbParams)
{
    if (!IsValidPointer(dbParams))
        return nullptr;

    SDK::UDbpSeason* seasonData = nullptr;
    if (!SafeReadMember(&dbParams->_dbpSeason, seasonData))
        return nullptr;

    return IsValidPointer(seasonData) ? seasonData : nullptr;
}

struct SeasonLicenseDataSource
{
    SDK::UBackendSubsystem* backendSubsystem = nullptr;
    SDK::UDatabaseParams* dbParams = nullptr;
    SDK::UDbpSeason* seasonData = nullptr;
    int32_t scannedObjects = 0;
    int32_t backendCandidates = 0;
    int32_t dbParamsCandidates = 0;
    int32_t seasonCandidates = 0;
    const char* dbParamsSource = "none";
    const char* seasonDataSource = "none";
};

static bool TryScanSeasonLicenseDataSource(SeasonLicenseDataSource& source)
{
    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    int32_t objectCount = 0;
    if (!IsValidPointer(gObjects) || !TryGetObjectArrayCountSafe(gObjects, objectCount))
        return false;

    if (objectCount <= 0 || objectCount > 2000000)
        return false;

    source.scannedObjects = objectCount;
    SDK::UClass* backendClass = SDK::UBackendSubsystem::StaticClass();
    SDK::UClass* dbParamsClass = SDK::UDatabaseParams::StaticClass();
    SDK::UClass* seasonClass = SDK::UDbpSeason::StaticClass();

    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
            continue;

        if (IsObjectAUnsafeGuarded(obj, backendClass))
        {
            ++source.backendCandidates;
            SDK::UBackendSubsystem* candidateBackend = static_cast<SDK::UBackendSubsystem*>(obj);
            if (!source.backendSubsystem)
                source.backendSubsystem = candidateBackend;

            if (!source.dbParams)
            {
                SDK::UDatabaseParams* memberParams = GetDatabaseParamsMemberSafe(candidateBackend);
                if (memberParams)
                {
                    source.dbParams = memberParams;
                    source.dbParamsSource = "gobjects_backend_member";
                }
            }
        }

        if (IsObjectAUnsafeGuarded(obj, dbParamsClass))
        {
            ++source.dbParamsCandidates;
            if (!source.dbParams)
            {
                source.dbParams = static_cast<SDK::UDatabaseParams*>(obj);
                source.dbParamsSource = "gobjects_databaseparams";
            }
        }

        if (IsObjectAUnsafeGuarded(obj, seasonClass))
        {
            ++source.seasonCandidates;
            if (!source.seasonData)
            {
                source.seasonData = static_cast<SDK::UDbpSeason*>(obj);
                source.seasonDataSource = "gobjects_dbpseason";
            }
        }

        if (source.dbParams && !source.seasonData)
        {
            SDK::UDbpSeason* memberSeason = GetSeasonDataMemberSafe(source.dbParams);
            if (memberSeason)
            {
                source.seasonData = memberSeason;
                source.seasonDataSource = "databaseparams_member";
            }
        }

        if (source.dbParams && source.seasonData && source.backendSubsystem)
            return true;
    }

    return source.dbParams || source.seasonData || source.backendSubsystem;
}

static SeasonLicenseDataSource ResolveSeasonLicenseDataSource()
{
    SeasonLicenseDataSource source{};
    source.backendSubsystem = GetBackendSubsystemSafe();
    source.dbParams = GetDatabaseParamsSafe(source.backendSubsystem);
    if (source.dbParams)
        source.dbParamsSource = "backend_getter";

    if (!source.dbParams)
    {
        source.dbParams = GetDatabaseParamsMemberSafe(source.backendSubsystem);
        if (source.dbParams)
            source.dbParamsSource = "backend_member";
    }

    source.seasonData = GetSeasonDataSafe(source.dbParams);
    if (source.seasonData)
        source.seasonDataSource = "databaseparams_getter";

    if (!source.seasonData)
    {
        source.seasonData = GetSeasonDataMemberSafe(source.dbParams);
        if (source.seasonData)
            source.seasonDataSource = "databaseparams_member";
    }

    if (!source.dbParams || !source.seasonData)
    {
        SeasonLicenseDataSource scanned{};
        if (TryScanSeasonLicenseDataSource(scanned))
        {
            source.scannedObjects = scanned.scannedObjects;
            source.backendCandidates = scanned.backendCandidates;
            source.dbParamsCandidates = scanned.dbParamsCandidates;
            source.seasonCandidates = scanned.seasonCandidates;

            if (!source.backendSubsystem && scanned.backendSubsystem)
                source.backendSubsystem = scanned.backendSubsystem;

            if (!source.dbParams && scanned.dbParams)
            {
                source.dbParams = scanned.dbParams;
                source.dbParamsSource = scanned.dbParamsSource;
            }

            if (!source.seasonData && scanned.seasonData)
            {
                source.seasonData = scanned.seasonData;
                source.seasonDataSource = scanned.seasonDataSource;
            }

            if (source.dbParams && !source.seasonData)
            {
                source.seasonData = GetSeasonDataSafe(source.dbParams);
                if (source.seasonData)
                    source.seasonDataSource = "scanned_databaseparams_getter";
            }
        }
    }

    return source;
}

static const char* SeasonPassRankName(SDK::ESeasonPassRank value)
{
    switch (value)
    {
    case SDK::ESeasonPassRank::Hero: return "Hero";
    case SDK::ESeasonPassRank::Pro: return "Pro";
    case SDK::ESeasonPassRank::Middle: return "Middle";
    case SDK::ESeasonPassRank::Max: return "Max";
    default: return "Unknown";
    }
}

static const char* ItemCategoryName(SDK::EItemCategory value)
{
    switch (value)
    {
    case SDK::EItemCategory::Invalid: return "Invalid";
    case SDK::EItemCategory::Character: return "Character";
    case SDK::EItemCategory::Currency: return "Currency";
    case SDK::EItemCategory::Emblem: return "Emblem";
    case SDK::EItemCategory::CustomizeCostume: return "CustomizeCostume";
    case SDK::EItemCategory::CustomizeAppeal: return "CustomizeAppeal";
    case SDK::EItemCategory::CustomizeVoice: return "CustomizeVoice";
    case SDK::EItemCategory::MyAdParts: return "MyAdParts";
    case SDK::EItemCategory::Pack: return "Pack";
    case SDK::EItemCategory::Variation: return "Variation";
    case SDK::EItemCategory::ExperiencePoint: return "ExperiencePoint";
    case SDK::EItemCategory::SeasonLicense: return "SeasonLicense";
    case SDK::EItemCategory::NameplateBg: return "NameplateBg";
    case SDK::EItemCategory::MyRoom: return "MyRoom";
    case SDK::EItemCategory::RandomPack: return "RandomPack";
    case SDK::EItemCategory::Max: return "Max";
    default: return "Unknown";
    }
}

static std::string SafeFStringToString(const SDK::FString& value)
{
    int32_t count = 0;
    if (!SafeArrayCount(value, count, 4096))
        return "";

    try
    {
        return value.ToString();
    }
    catch (...)
    {
        return "";
    }
}

static std::string SafeFTextToString(const SDK::FText& value)
{
    if (!SafeMemory::IsReadable(&value, sizeof(value)) ||
        !SafeMemory::IsReadable(value.TextData, sizeof(SDK::FTextImpl::FTextData)))
    {
        return "";
    }

    return SafeFStringToString(value.TextData->TextSource);
}

static void AppendStringField(std::stringstream& ss, const std::string& indent, const char* label, const std::string& value)
{
    if (!value.empty())
        ss << "\n" << indent << label << ": " << value;
}

static bool GetSeasonBoolGetterSafe(SDK::UDbpSeason* seasonData, int getterId, bool& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        switch (getterId)
        {
        case 0: outValue = seasonData->CanBuyProLicense(); return true;
        case 1: outValue = seasonData->CanBuyProLicenseWithExp(); return true;
        case 2: outValue = seasonData->HasMiddleLicense(); return true;
        case 3: outValue = seasonData->HasProLicense(); return true;
        default: return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonIntGetterSafe(SDK::UDbpSeason* seasonData, int getterId, SDK::int32& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        switch (getterId)
        {
        case 0: outValue = seasonData->GetAvailableSpecialLicenseExpCount(); return true;
        case 1: outValue = seasonData->GetDiscountProLicensePrice(); return true;
        case 2: outValue = seasonData->GetHeroCrystal(); return true;
        case 3: outValue = seasonData->GetLicensePrice(); return true;
        case 4: outValue = seasonData->GetNextRankExp(); return true;
        case 5: outValue = seasonData->GetProLicenseLightPrice(); return true;
        case 6: outValue = seasonData->GetProLicensePrice(); return true;
        case 7: outValue = seasonData->GetProLicensePriceWithExp(); return true;
        case 8: outValue = seasonData->GetSpecialLicenseExp(); return true;
        case 9: outValue = seasonData->GetSpecialLicenseExpPrice(); return true;
        case 10: outValue = seasonData->GetSpecialLicenseMaxExp(); return true;
        case 11: outValue = seasonData->GetSpecialLicenseRank(); return true;
        default: return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonInfoSafe(SDK::UDbpSeason* seasonData, SDK::FDbSeasonParam& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetSeasonInfo();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonRewardsSafe(SDK::UDbpSeason* seasonData, SDK::int32 rank, SDK::TArray<SDK::FDbSeasonPassParam>& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetRewards(rank);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonRewardRangeSafe(SDK::UDbpSeason* seasonData, SDK::int32 rankFrom, SDK::int32 rankTo, SDK::TArray<SDK::FDbSeasonPassParam>& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetRewardRange(rankFrom, rankTo);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSpecialLicenseLastRewardsSafe(SDK::UDbpSeason* seasonData, SDK::TArray<SDK::FDbSpecialLicenseReward>& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetSpecialLicenseLastRewards();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSpecialLicenseListSafe(SDK::UDbpSeason* seasonData, SDK::TMap<SDK::int32, SDK::FDbSpecialLicenseParam>& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetSpecialLicenseList();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonStockItemsSafe(SDK::UDbpSeason* seasonData, SDK::TMap<SDK::FDbItemCategoryParam, SDK::int32>& outValue)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        outValue = seasonData->GetStockItems();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetDirectDbSeasonParamSafe(SDK::UDatabaseParams* dbParams, SDK::FDbSeasonParam& outValue)
{
    if (!IsValidPointer(dbParams))
        return false;

    __try
    {
        outValue = dbParams->season;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetDirectDbSpecialLicenseSafe(SDK::UDatabaseParams* dbParams, SDK::FDbSpecialLicenseListParam& outValue)
{
    if (!IsValidPointer(dbParams))
        return false;

    __try
    {
        outValue = dbParams->SpecialLicense;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonMasterSafe(SDK::int32 code, SDK::FMasterDataSeason& outValue)
{
    __try
    {
        SDK::UMasterDataCache::GetSeason(code, &outValue);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSeasonLicenseMasterSafe(SDK::int32 code, SDK::FMasterDataSeasonLicense& outValue)
{
    __try
    {
        SDK::UMasterDataCache::GetSeasonLicense(code, &outValue);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetSpecialLicenseMasterSafe(SDK::int32 code, SDK::FMasterDataSpecialLicense& outValue)
{
    __try
    {
        SDK::UMasterDataCache::GetSpecialLicense(code, &outValue);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void AppendSeasonMasterSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FMasterDataSeason& season)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  code: " << season.code
       << "\n" << indent << "  seasonPassGroup: " << season.SeasonPassGroup;
    AppendStringField(ss, indent + "  ", "name", SafeFTextToString(season.Name));
}

static void AppendSeasonLicenseMasterSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FMasterDataSeasonLicense& license)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  code: " << license.code
       << "\n" << indent << "  season: " << license.season
       << "\n" << indent << "  type: " << license.Type;
    AppendStringField(ss, indent + "  ", "name", SafeFStringToString(license.Name));
    AppendStringField(ss, indent + "  ", "itemCategory", SafeFStringToString(license.itemCategory));
}

static void AppendSpecialLicenseMasterSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FMasterDataSpecialLicense& license)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  code: " << license.code
       << "\n" << indent << "  rank: " << license.Rank
       << "\n" << indent << "  exp: " << license.Exp
       << "\n" << indent << "  itemCode: " << license.ItemCode
       << "\n" << indent << "  quantity: " << license.Quantity;
    AppendStringField(ss, indent + "  ", "itemCategory", SafeFStringToString(license.itemCategory));
}

static void AppendMasterDataCacheProbeSummary(std::stringstream& ss, int32_t limit)
{
    ss << "\n\n=== MasterDataCache fallback scan ==="
       << "\nnotes: static master/config data only; player progress/received flags still require DbpSeason/DatabaseParams"
       << "\nseasonCodeRange: 1..100"
       << "\nseasonLicenseCodeRange: 1..1000"
       << "\nspecialLicenseCodeRange: 1..1000"
       << "\nloggedLimitPerType: " << limit;

    int32_t loggedSeasons = 0;
    ss << "\n\n--- MasterDataCache.GetSeason ---";
    for (SDK::int32 code = 1; code <= 100 && loggedSeasons < limit; ++code)
    {
        SDK::FMasterDataSeason season{};
        if (GetSeasonMasterSafe(code, season) && season.code > 0)
        {
            std::stringstream label;
            label << "season[" << loggedSeasons << "]";
            AppendSeasonMasterSummary(ss, "  ", label.str().c_str(), season);
            ++loggedSeasons;
        }
    }
    ss << "\n  logged: " << loggedSeasons;

    int32_t loggedSeasonLicenses = 0;
    ss << "\n\n--- MasterDataCache.GetSeasonLicense ---";
    for (SDK::int32 code = 1; code <= 1000 && loggedSeasonLicenses < limit; ++code)
    {
        SDK::FMasterDataSeasonLicense license{};
        if (GetSeasonLicenseMasterSafe(code, license) && license.code > 0)
        {
            std::stringstream label;
            label << "seasonLicense[" << loggedSeasonLicenses << "]";
            AppendSeasonLicenseMasterSummary(ss, "  ", label.str().c_str(), license);
            ++loggedSeasonLicenses;
        }
    }
    ss << "\n  logged: " << loggedSeasonLicenses;

    int32_t loggedSpecialLicenses = 0;
    ss << "\n\n--- MasterDataCache.GetSpecialLicense ---";
    for (SDK::int32 code = 1; code <= 1000 && loggedSpecialLicenses < limit; ++code)
    {
        SDK::FMasterDataSpecialLicense license{};
        if (GetSpecialLicenseMasterSafe(code, license) && license.code > 0)
        {
            std::stringstream label;
            label << "specialLicense[" << loggedSpecialLicenses << "]";
            AppendSpecialLicenseMasterSummary(ss, "  ", label.str().c_str(), license);
            ++loggedSpecialLicenses;
        }
    }
    ss << "\n  logged: " << loggedSpecialLicenses;
}

static void AppendItemCategoryParamSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FDbItemCategoryParam& item)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  category: " << ItemCategoryName(item.eCategory) << " (" << static_cast<int>(item.eCategory) << ")"
       << "\n" << indent << "  code: " << item.code;
    AppendStringField(ss, indent + "  ", "assetName", SafeFStringToString(item.AssetName));
    AppendStringField(ss, indent + "  ", "displayName", SafeFTextToString(item.DisplayName));

    if (item.eCategory == SDK::EItemCategory::SeasonLicense && item.code > 0)
    {
        SDK::FMasterDataSeasonLicense seasonLicense{};
        if (GetSeasonLicenseMasterSafe(item.code, seasonLicense))
            AppendSeasonLicenseMasterSummary(ss, indent + "  ", "seasonLicenseMaster", seasonLicense);
        else
            ss << "\n" << indent << "  seasonLicenseMaster: failed";
    }
}

static void AppendSeasonPassRewardSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FDbSeasonPassRewardParam& reward)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  received: " << (reward.bReceived ? 1 : 0)
       << "\n" << indent << "  quantity: " << reward.Quantity;
    AppendItemCategoryParamSummary(ss, indent + "  ", "item", reward);
}

static void AppendExchangeItemSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FDbExchangeItemCategoryParam& item)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  quantity: " << item.Quantity;
    AppendItemCategoryParamSummary(ss, indent + "  ", "item", item);
}

static void AppendSeasonPassParamSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FDbSeasonPassParam& reward)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  code: " << reward.code
       << "\n" << indent << "  seasonRank: " << reward.SeasonRank
       << "\n" << indent << "  exp: " << reward.Exp
       << "\n" << indent << "  freeItemCode: " << reward.FreeItemCode
       << "\n" << indent << "  freeQuantity: " << reward.FreeQuantity
       << "\n" << indent << "  premiumItemCode: " << reward.PremiumItemCode
       << "\n" << indent << "  premiumQuantity: " << reward.PremiumQuantity
       << "\n" << indent << "  groupCode: " << reward.GroupCode;
    AppendStringField(ss, indent + "  ", "freeItemCategory", SafeFStringToString(reward.FreeItemCategory));
    AppendStringField(ss, indent + "  ", "premiumItemCategory", SafeFStringToString(reward.PremiumItemCategory));
    AppendStringField(ss, indent + "  ", "note", SafeFStringToString(reward.Note));
    AppendSeasonPassRewardSummary(ss, indent + "  ", "freeItem", reward.FreeItem);
    AppendSeasonPassRewardSummary(ss, indent + "  ", "premiumItem", reward.PremiumItem);

    int32_t exItemCount = 0;
    if (SafeArrayCount(reward.ExItems, exItemCount, 512))
    {
        ss << "\n" << indent << "  exItemsCount: " << exItemCount;

        const SDK::FDbExchangeItemCategoryParam* data = reward.ExItems.GetDataPtr();
        const int32_t maxEntries = std::min<int32_t>(exItemCount, 10);
        for (int32_t i = 0; data && i < maxEntries; ++i)
        {
            if (!SafeMemory::IsReadable(data + i, sizeof(SDK::FDbExchangeItemCategoryParam)))
                break;

            std::stringstream itemLabel;
            itemLabel << "exItems[" << i << "]";
            AppendExchangeItemSummary(ss, indent + "  ", itemLabel.str().c_str(), data[i]);
        }
    }
    else
    {
        ss << "\n" << indent << "  exItemsCount: unreadable";
    }
}

static void AppendSpecialLicenseParamSummary(std::stringstream& ss, const std::string& indent, const char* label, SDK::int32 mapKey, const SDK::FDbSpecialLicenseParam& license)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  mapKey: " << mapKey
       << "\n" << indent << "  code: " << license.code
       << "\n" << indent << "  rank: " << license.Rank
       << "\n" << indent << "  exp: " << license.Exp
       << "\n" << indent << "  itemCode: " << license.ItemCode
       << "\n" << indent << "  quantity: " << license.Quantity
       << "\n" << indent << "  received: " << (license.bReceived ? 1 : 0);
    AppendStringField(ss, indent + "  ", "itemCategory", SafeFStringToString(license.itemCategory));
    AppendItemCategoryParamSummary(ss, indent + "  ", "reward", license.Reward);

    SDK::FMasterDataSpecialLicense master{};
    if (GetSpecialLicenseMasterSafe(license.code, master))
        AppendSpecialLicenseMasterSummary(ss, indent + "  ", "specialLicenseMaster", master);
    else
        ss << "\n" << indent << "  specialLicenseMaster: failed";
}

static void AppendSpecialLicenseRewardSummary(std::stringstream& ss, const std::string& indent, const char* label, const SDK::FDbSpecialLicenseReward& reward)
{
    ss << "\n" << indent << label << ":"
       << "\n" << indent << "  count: " << reward.count;
    AppendItemCategoryParamSummary(ss, indent + "  ", "item", reward);
}

static void AppendSeasonPassArraySummary(std::stringstream& ss, const char* label, const SDK::TArray<SDK::FDbSeasonPassParam>& rewards, int32_t limit)
{
    int32_t count = 0;
    if (!SafeArrayCount(rewards, count, 4096))
    {
        ss << "\n\n=== " << label << " ==="
           << "\n  count: unreadable";
        return;
    }

    ss << "\n\n=== " << label << " ==="
       << "\n  count: " << count
       << "\n  loggedLimit: " << limit;

    const SDK::FDbSeasonPassParam* data = rewards.GetDataPtr();
    const int32_t maxEntries = std::min<int32_t>(count, limit);
    for (int32_t i = 0; data && i < maxEntries; ++i)
    {
        if (!SafeMemory::IsReadable(data + i, sizeof(SDK::FDbSeasonPassParam)))
            break;

        std::stringstream entryLabel;
        entryLabel << label << "[" << i << "]";
        AppendSeasonPassParamSummary(ss, "  ", entryLabel.str().c_str(), data[i]);
    }
}

static void AppendSpecialLicenseRewardArraySummary(std::stringstream& ss, const char* label, const SDK::TArray<SDK::FDbSpecialLicenseReward>& rewards, int32_t limit)
{
    int32_t count = 0;
    if (!SafeArrayCount(rewards, count, 4096))
    {
        ss << "\n\n=== " << label << " ==="
           << "\n  count: unreadable";
        return;
    }

    ss << "\n\n=== " << label << " ==="
       << "\n  count: " << count
       << "\n  loggedLimit: " << limit;

    const SDK::FDbSpecialLicenseReward* data = rewards.GetDataPtr();
    const int32_t maxEntries = std::min<int32_t>(count, limit);
    for (int32_t i = 0; data && i < maxEntries; ++i)
    {
        if (!SafeMemory::IsReadable(data + i, sizeof(SDK::FDbSpecialLicenseReward)))
            break;

        std::stringstream entryLabel;
        entryLabel << label << "[" << i << "]";
        AppendSpecialLicenseRewardSummary(ss, "  ", entryLabel.str().c_str(), data[i]);
    }
}

static bool GetMapCountsForDump(const SDK::TMap<SDK::int32, SDK::FDbSpecialLicenseParam>& map, int32_t& count, int32_t& allocated)
{
    if (!SafeMemory::IsReadable(&map, sizeof(map)))
        return false;

    try
    {
        count = map.Num();
        allocated = map.NumAllocated();
        return count >= 0 && allocated >= count && allocated <= 4096;
    }
    catch (...)
    {
        return false;
    }
}

static bool GetMapCountsForDump(const SDK::TMap<SDK::FDbItemCategoryParam, SDK::int32>& map, int32_t& count, int32_t& allocated)
{
    if (!SafeMemory::IsReadable(&map, sizeof(map)))
        return false;

    try
    {
        count = map.Num();
        allocated = map.NumAllocated();
        return count >= 0 && allocated >= count && allocated <= 4096;
    }
    catch (...)
    {
        return false;
    }
}

struct SpecialLicenseProgressFields
{
    SDK::int32 currentRank = 0;
    SDK::int32 currentExp = 0;
    SDK::int32 totalExp = 0;
    SDK::int32 nextExp = 0;
    SDK::int32 maxExp = 0;
};

static bool CallAddSpecialLicenseExpSafe(SDK::UDbpSeason* seasonData, SDK::int32 exp)
{
    if (!IsValidPointer(seasonData))
        return false;

    __try
    {
        seasonData->AddSpecialLicenseExp(exp);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool DidSpecialLicenseProgressChange(
    const SDK::FDbSpecialLicenseListParam& before,
    const SDK::FDbSpecialLicenseListParam& after)
{
    return before.TotalExp != after.TotalExp ||
           before.CurrentExp != after.CurrentExp ||
           before.CurrentRank != after.CurrentRank ||
           before.NextExp != after.NextExp ||
           before.MaxExp != after.MaxExp;
}

static bool CalculateSpecialLicenseProgressFields(
    const SDK::FDbSpecialLicenseListParam& specialLicense,
    SDK::int32 requestedTotalExp,
    SpecialLicenseProgressFields& outFields)
{
    int32_t count = 0;
    int32_t allocated = 0;
    if (!GetMapCountsForDump(specialLicense.SpecialLicenseList, count, allocated) || count <= 0)
        return false;

    SDK::int32 totalExp = requestedTotalExp;
    if (totalExp < 0)
        totalExp = 0;

    bool foundAny = false;
    bool foundNext = false;
    SDK::int32 bestRank = 0;
    SDK::int32 bestThreshold = 0;
    SDK::int32 nextThreshold = 0;
    SDK::int32 maxThreshold = 0;
    SDK::int32 maxRank = 0;

    try
    {
        for (auto it = begin(specialLicense.SpecialLicenseList); it != end(specialLicense.SpecialLicenseList); ++it)
        {
            const SDK::FDbSpecialLicenseParam& param = it->Value();
            const SDK::int32 threshold = param.Exp;
            if (threshold <= 0)
                continue;

            SDK::int32 rank = param.Rank;
            if (rank <= 0)
                rank = it->Key();

            foundAny = true;

            if (threshold > maxThreshold)
            {
                maxThreshold = threshold;
                maxRank = rank;
            }

            if (threshold <= totalExp)
            {
                if (threshold > bestThreshold || (threshold == bestThreshold && rank > bestRank))
                {
                    bestThreshold = threshold;
                    bestRank = rank;
                }
            }
            else if (!foundNext || threshold < nextThreshold)
            {
                foundNext = true;
                nextThreshold = threshold;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    if (!foundAny)
        return false;

    if (maxThreshold > 0 && totalExp > maxThreshold)
    {
        totalExp = maxThreshold;
        bestThreshold = maxThreshold;
        bestRank = maxRank;
        foundNext = false;
        nextThreshold = 0;
    }

    if (!foundNext)
    {
        try
        {
            for (auto it = begin(specialLicense.SpecialLicenseList); it != end(specialLicense.SpecialLicenseList); ++it)
            {
                const SDK::FDbSpecialLicenseParam& param = it->Value();
                if (param.Exp > totalExp && (!foundNext || param.Exp < nextThreshold))
                {
                    foundNext = true;
                    nextThreshold = param.Exp;
                }
            }
        }
        catch (...)
        {
            return false;
        }
    }

    const SDK::int32 rankMaxExp = foundNext ? (nextThreshold - bestThreshold) : 0;
    SDK::int32 currentExp = totalExp - bestThreshold;
    if (currentExp < 0)
        currentExp = totalExp;

    outFields.currentRank = bestRank;
    outFields.currentExp = currentExp;
    outFields.totalExp = totalExp;
    outFields.nextExp = foundNext ? (nextThreshold - totalExp) : 0;
    outFields.maxExp = rankMaxExp > 0 ? rankMaxExp : 0;
    return true;
}

static bool ApplySpecialLicenseProgressFields(
    SDK::FDbSpecialLicenseListParam& specialLicense,
    const SpecialLicenseProgressFields& fields)
{
    if (!SafeMemory::IsWritable(&specialLicense.CurrentRank, sizeof(SDK::int32) * 5))
        return false;

    __try
    {
        specialLicense.CurrentRank = fields.currentRank;
        specialLicense.CurrentExp = fields.currentExp;
        specialLicense.TotalExp = fields.totalExp;
        specialLicense.NextExp = fields.nextExp;
        specialLicense.MaxExp = fields.maxExp;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void AppendSpecialLicenseMapSummary(std::stringstream& ss, const char* label, const SDK::TMap<SDK::int32, SDK::FDbSpecialLicenseParam>& map, int32_t limit)
{
    int32_t count = 0;
    int32_t allocated = 0;
    if (!GetMapCountsForDump(map, count, allocated))
    {
        ss << "\n\n=== " << label << " ==="
           << "\n  count: unreadable";
        return;
    }

    ss << "\n\n=== " << label << " ==="
       << "\n  count: " << count
       << "\n  allocated: " << allocated
       << "\n  loggedLimit: " << limit;

    int32_t logged = 0;
    try
    {
        for (auto it = begin(map); it != end(map) && logged < limit; ++it)
        {
            std::stringstream entryLabel;
            entryLabel << label << "[" << logged << "]";
            AppendSpecialLicenseParamSummary(ss, "  ", entryLabel.str().c_str(), it->Key(), it->Value());
            ++logged;
        }
    }
    catch (...)
    {
        ss << "\n  iteration: failed";
    }

    ss << "\n  logged: " << logged;
}

static void AppendDbSeasonParamSummary(std::stringstream& ss, const char* label, const SDK::FDbSeasonParam& seasonInfo, int32_t limit)
{
    ss << "\n\n=== " << label << " ==="
       << "\ncode: " << seasonInfo.code
       << "\nseasonPassGroup: " << seasonInfo.SeasonPassGroup
       << "\nseasonPassRank: " << SeasonPassRankName(seasonInfo.SeasonPassRank) << " (" << static_cast<int>(seasonInfo.SeasonPassRank) << ")"
       << "\nseasonRank: " << seasonInfo.SeasonRank
       << "\nseasonRankExp: " << seasonInfo.SeasonRankExp
       << "\nstockCount: " << seasonInfo.StockCount;
    AppendStringField(ss, "", "name", SafeFTextToString(seasonInfo.Name));

    int32_t rankListCount = 0;
    if (SafeArrayCount(seasonInfo.ranks, rankListCount, 4096))
        ss << "\nranksCount: " << rankListCount;
    else
        ss << "\nranksCount: unreadable";

    AppendSeasonPassArraySummary(ss, "DatabaseParams.season.ranks", seasonInfo.ranks, limit);
}

static void AppendDbSpecialLicenseListParamSummary(std::stringstream& ss, const char* label, const SDK::FDbSpecialLicenseListParam& specialLicense, int32_t limit)
{
    ss << "\n\n=== " << label << " ==="
       << "\ncurrentRank: " << specialLicense.CurrentRank
       << "\ncurrentExp: " << specialLicense.CurrentExp
       << "\ntotalExp: " << specialLicense.TotalExp
       << "\nnextExp: " << specialLicense.NextExp
       << "\nmaxExp: " << specialLicense.MaxExp
       << "\nbuyExpPrice: " << specialLicense.BuyExpPrice
       << "\nbuyExpCount: " << specialLicense.BuyExpCount
       << "\nviewResult: " << (specialLicense.bViewResult ? 1 : 0);

    AppendSpecialLicenseMapSummary(ss, "DatabaseParams.SpecialLicense.SpecialLicenseList", specialLicense.SpecialLicenseList, limit);
    AppendSpecialLicenseRewardArraySummary(ss, "DatabaseParams.SpecialLicense.LastRewardsList", specialLicense.LastRewardsList, limit);
}

static SDK::FDbSeasonPassRewardParam* GetSeasonRankRewardSlot(SDK::FDbSeasonPassParam& rank, int slot)
{
    if (slot == 0)
        return &rank.FreeItem;
    if (slot == 1)
        return &rank.PremiumItem;
    return nullptr;
}

static const SDK::FDbSeasonPassRewardParam* GetSeasonRankRewardSlot(const SDK::FDbSeasonPassParam& rank, int slot)
{
    if (slot == 0)
        return &rank.FreeItem;
    if (slot == 1)
        return &rank.PremiumItem;
    return nullptr;
}

static bool IsUsableSeasonRewardItem(const SDK::FDbSeasonPassRewardParam& reward)
{
    return reward.code > 0 && reward.eCategory != SDK::EItemCategory::Invalid;
}

static std::string BuildSeasonRewardOptionLabel(const SDK::FDbSeasonPassParam& rank, int slot, const SDK::FDbSeasonPassRewardParam& reward)
{
    std::stringstream label;
    label << "Rank " << rank.SeasonRank << " "
          << (slot == 0 ? "Free" : "Premium") << " - "
          << ItemCategoryName(reward.eCategory) << " #" << reward.code;

    const std::string displayName = SafeFTextToString(reward.DisplayName);
    if (!displayName.empty())
        label << " - " << displayName;

    if (reward.Quantity > 0)
        label << " x" << reward.Quantity;

    return label.str();
}

static bool GetWritableSeasonRanks(SDK::FDbSeasonPassParam*& outRanks, int32_t& outCount)
{
    outRanks = nullptr;
    outCount = 0;

    SeasonLicenseDataSource source = ResolveSeasonLicenseDataSource();
    if (!IsValidPointer(source.dbParams))
        return false;

    int32_t count = 0;
    if (!SafeArrayCount(source.dbParams->season.ranks, count, 4096))
        return false;

    if (count <= 0)
        return false;

    const SDK::FDbSeasonPassParam* constData = source.dbParams->season.ranks.GetDataPtr();
    if (!constData)
        return false;

    const size_t bytes = sizeof(SDK::FDbSeasonPassParam) * static_cast<size_t>(count);
    if (!SafeMemory::IsReadable(constData, bytes) || !SafeMemory::IsWritable(const_cast<SDK::FDbSeasonPassParam*>(constData), bytes))
        return false;

    outRanks = const_cast<SDK::FDbSeasonPassParam*>(constData);
    outCount = count;
    return true;
}

std::vector<SeasonRankRewardItemOption> InGameHack_GetSeasonRankRewardItemOptions()
{
    std::vector<SeasonRankRewardItemOption> options;

    SDK::FDbSeasonPassParam* ranks = nullptr;
    int32_t count = 0;
    if (!GetWritableSeasonRanks(ranks, count))
        return options;

    for (int32_t i = 0; i < count; ++i)
    {
        if (!SafeMemory::IsReadable(ranks + i, sizeof(SDK::FDbSeasonPassParam)))
            break;

        const SDK::FDbSeasonPassParam& rank = ranks[i];
        for (int slot = 0; slot < 2; ++slot)
        {
            const SDK::FDbSeasonPassRewardParam* reward = GetSeasonRankRewardSlot(rank, slot);
            if (!reward || !IsUsableSeasonRewardItem(*reward))
                continue;

            SeasonRankRewardItemOption option{};
            option.arrayIndex = static_cast<int>(i);
            option.rank = rank.SeasonRank;
            option.slot = slot;
            option.code = reward->code;
            option.category = static_cast<int>(reward->eCategory);
            option.quantity = reward->Quantity;
            option.label = BuildSeasonRewardOptionLabel(rank, slot, *reward);
            options.push_back(option);
        }
    }

    return options;
}

static void ApplySeasonRewardReplacement(
    SDK::FDbSeasonPassParam& targetRank,
    int targetSlot,
    const SDK::FDbSeasonPassRewardParam& sourceReward,
    const SDK::FString& sourceCategory,
    int quantity)
{
    SDK::FDbSeasonPassRewardParam* targetReward = GetSeasonRankRewardSlot(targetRank, targetSlot);
    if (!targetReward)
        return;

    const bool wasReceived = targetReward->bReceived;
    *targetReward = sourceReward;
    targetReward->bReceived = wasReceived;
    targetReward->Quantity = quantity;

    if (targetSlot == 0)
    {
        targetRank.FreeItemCategory = sourceCategory;
        targetRank.FreeItemCode = sourceReward.code;
        targetRank.FreeQuantity = quantity;
    }
    else
    {
        targetRank.PremiumItemCategory = sourceCategory;
        targetRank.PremiumItemCode = sourceReward.code;
        targetRank.PremiumQuantity = quantity;
    }
}

int InGameHack_ReplaceSeasonRankRewardsFromExistingReward(
    int sourceArrayIndex,
    int sourceSlot,
    int targetRank,
    int quantity,
    int targetSlotMask,
    bool applyAllRanks)
{
    if (sourceSlot < 0 || sourceSlot > 1 || targetSlotMask == 0)
        return -1;

    if (quantity < 1)
        quantity = 1;
    else if (quantity > 999999)
        quantity = 999999;

    SDK::FDbSeasonPassParam* ranks = nullptr;
    int32_t count = 0;
    if (!GetWritableSeasonRanks(ranks, count))
    {
        Logger::LogError("[SeasonRankRewardReplace] Could not resolve writable DatabaseParams.season.ranks");
        return -1;
    }

    if (sourceArrayIndex < 0 || sourceArrayIndex >= count)
    {
        Logger::LogError("[SeasonRankRewardReplace] Source reward index is out of range");
        return -1;
    }

    const SDK::FDbSeasonPassRewardParam* sourceReward = nullptr;
    const SDK::FString* sourceCategory = nullptr;
    if (SafeMemory::IsReadable(ranks + sourceArrayIndex, sizeof(SDK::FDbSeasonPassParam)))
    {
        sourceReward = GetSeasonRankRewardSlot(ranks[sourceArrayIndex], sourceSlot);
        if (sourceReward && IsUsableSeasonRewardItem(*sourceReward))
        {
            sourceCategory = sourceSlot == 0
                ? &ranks[sourceArrayIndex].FreeItemCategory
                : &ranks[sourceArrayIndex].PremiumItemCategory;
        }
    }

    if (!sourceReward || !sourceCategory)
    {
        Logger::LogError("[SeasonRankRewardReplace] Source reward was not found");
        return -1;
    }

    const SDK::FDbSeasonPassRewardParam sourceCopy = *sourceReward;
    const SDK::FString sourceCategoryCopy = *sourceCategory;
    int modified = 0;

    for (int32_t i = 0; i < count; ++i)
    {
        if (!SafeMemory::IsWritable(ranks + i, sizeof(SDK::FDbSeasonPassParam)))
            break;

        if (!applyAllRanks && ranks[i].SeasonRank != targetRank)
            continue;

        for (int slot = 0; slot < 2; ++slot)
        {
            if ((targetSlotMask & (1 << slot)) == 0)
                continue;

            ApplySeasonRewardReplacement(ranks[i], slot, sourceCopy, sourceCategoryCopy, quantity);
            ++modified;
        }
    }

    Logger::LogInfo("[SeasonRankRewardReplace] Modified reward slots: " + std::to_string(modified));
    return modified;
}

static void AppendStockItemsMapSummary(std::stringstream& ss, const char* label, const SDK::TMap<SDK::FDbItemCategoryParam, SDK::int32>& map, int32_t limit)
{
    int32_t count = 0;
    int32_t allocated = 0;
    if (!GetMapCountsForDump(map, count, allocated))
    {
        ss << "\n\n=== " << label << " ==="
           << "\n  count: unreadable";
        return;
    }

    ss << "\n\n=== " << label << " ==="
       << "\n  count: " << count
       << "\n  allocated: " << allocated
       << "\n  loggedLimit: " << limit;

    int32_t logged = 0;
    try
    {
        for (auto it = begin(map); it != end(map) && logged < limit; ++it)
        {
            std::stringstream entryLabel;
            entryLabel << label << "[" << logged << "]";
            ss << "\n  " << entryLabel.str() << ":"
               << "\n    stockCount: " << it->Value();
            AppendItemCategoryParamSummary(ss, "    ", "item", it->Key());
            ++logged;
        }
    }
    catch (...)
    {
        ss << "\n  iteration: failed";
    }

    ss << "\n  logged: " << logged;
}

static SDK::UDbpCharacterCustomize* GetCharacterCustomizeDataSafe(SDK::UDatabaseParams* dbParams, int characterCode)
{
    if (!IsValidPointer(dbParams))
        return nullptr;

    __try
    {
        SDK::UDbpCharacterCustomize* customize = dbParams->GetCharacterCustomizeData(characterCode);
        return IsValidPointer(customize) ? customize : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool GetEquippedCostumeRoleSlotSafe(SDK::UDbpCharacterCustomize* customize, SDK::FDbsCostumeRoleSlotParam& outParam)
{
    if (!IsValidPointer(customize))
        return false;

    __try
    {
        outParam = customize->GetEquippedCostumeRoleSlot();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetDbpCostumeRoleSlotSafe(SDK::UDbpCharacterCustomize* customize, int costumeCode, SDK::FDbsCostumeRoleSlotParam& outParam)
{
    if (!IsValidPointer(customize))
        return false;

    __try
    {
        return customize->GetRoleSlot(costumeCode, &outParam);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetStaticCostumeRoleSlotSafe(int costumeCode, SDK::FDbsCostumeRoleSlotParam& outParam)
{
    __try
    {
        return SDK::URoleSlotStatics::GetCostumeRoleSlotParam(costumeCode, &outParam);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string GetObjectNameSafe(SDK::UObject* object)
{
    if (!IsValidPointer(object))
        return "null";

    try
    {
        return object->GetName();
    }
    catch (...)
    {
        return "unreadable";
    }
}

static std::string DescribeRollSlotState(SDK::APlayerStateBattle* playerState)
{
    std::stringstream ss;

    SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl = GetRollSlotControlComponentSafe(playerState);
    ss << "rollSlotCtrl=0x" << std::hex << reinterpret_cast<uintptr_t>(rollSlotCtrl) << std::dec;
    if (!rollSlotCtrl)
        return ss.str();

    int registeredCount = 0;
    ss << " registered=[";

    for (SDK::EVariationCharacterId id : GetAllVariationCharacterIds())
    {
        SDK::UCharacterRollSlotUniqueSkillBase* rollSlotObject = nullptr;
        try
        {
            rollSlotObject = rollSlotCtrl->BP_GetObject(id);
        }
        catch (...)
        {
            rollSlotObject = nullptr;
        }

        if (!IsValidPointer(rollSlotObject))
            continue;

        if (registeredCount > 0)
            ss << ", ";

        ss << static_cast<int>(id)
           << ":0x" << std::hex << reinterpret_cast<uintptr_t>(rollSlotObject) << std::dec
           << ":" << GetObjectNameSafe(rollSlotObject);

        registeredCount++;
    }

    ss << "] count=" << registeredCount;
    return ss.str();
}

static void LogTogaTransformRollSlotState(const char* phase, SDK::APlayerController* playerController, SDK::ACharacterBattle* playerCharacter, const SDK::ACharacterBattle* targetCharacter)
{
    (void)phase;
    (void)playerController;
    (void)playerCharacter;
    (void)targetCharacter;
}

static inline bool IsObjectDefaultSafe(SDK::UObject* object)
{
    if (!IsValidPointer(object))
        return true;

    try
    {
        return object->IsDefaultObject();
    }
    catch (...)
    {
        return true;
    }
}

static inline bool IsObjectAUnsafeGuarded(SDK::UObject* object, SDK::UClass* klass)
{
    if (!IsValidPointer(object) || !IsValidPointer(klass))
        return false;

    try
    {
        return object->IsA(klass);
    }
    catch (...)
    {
        return false;
    }
}

static inline bool GetClassNameSafe(SDK::UObject* object, std::string& className)
{
    className = "Unknown";
    if (!IsValidPointer(object))
        return false;

    SDK::UClass* objectClass = nullptr;
    if (!SafeReadMember(&object->Class, objectClass) || !IsValidPointer(objectClass))
        return false;

    try
    {
        className = objectClass->GetName();
        return !className.empty();
    }
    catch (...)
    {
        className = "Unknown";
        return false;
    }
}

static inline bool IsCharacterBattleClassName(const std::string& className)
{
    if (className == "CharacterBattle" || className == "ACharacterBattle")
        return true;

    return className.length() >= 5 &&
           className[0] == 'C' &&
           className[1] == 'h' &&
           std::isdigit(static_cast<unsigned char>(className[2])) &&
           std::isdigit(static_cast<unsigned char>(className[3])) &&
           std::isdigit(static_cast<unsigned char>(className[4]));
}

static inline SDK::ACharacterBattle* ActorToCharacterBattleSafe(SDK::AActor* actor)
{
    if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor))
        return nullptr;

    if (!IsObjectAUnsafeGuarded(actor, SDK::ACharacterBattle::StaticClass()))
    {
        std::string className;
        if (!GetClassNameSafe(actor, className) || !IsCharacterBattleClassName(className))
            return nullptr;
    }

    SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(actor);
    if (!IsValidPointer(character) || !GetCharacterPlayerStateSafe(character))
        return nullptr;

    return character;
}

static inline std::string GetPlayerNameSafe(SDK::APlayerState* playerState)
{
    if (!IsValidPointer(playerState))
        return "Unknown";

    try
    {
        const char* namePtr = SDK_GetPlayerName(playerState);
        if (SafeMemory::IsReadable(namePtr, 1) && namePtr[0] != '\0')
            return namePtr;
    }
    catch (...)
    {
    }

    return "Unknown";
}

/**
 * Safe getter for player pawn with validation
 */
static inline SDK::ACharacterBattle* GetPlayerCharacterBattle(SDK::APlayerController* playerController)
{
    SDK::APawn* pawn = GetPawnSafe(playerController);
    if (!pawn) return nullptr;

    SDK::ACharacterBattle* character = static_cast<SDK::ACharacterBattle*>(pawn);
    if (!IsValidPointer(character)) return nullptr;
    
    return character;
}

/**
 * Safe getter for player state with validation
 */
static inline SDK::APlayerStateBattle* GetPlayerStateBattle(SDK::APlayerController* playerController)
{
    SDK::APlayerState* playerState = GetControllerPlayerStateSafe(playerController);
    if (!playerState) return nullptr;

    SDK::APlayerStateBattle* playerStateBattle = static_cast<SDK::APlayerStateBattle*>(playerState);
    if (!IsValidPointer(playerStateBattle)) return nullptr;
    
    return playerStateBattle;
}

static int GetDatabaseCharacterCodeFromSDKId(int sdkCharacterId)
{
    switch (sdkCharacterId)
    {
    case 2: return 1;      // Izuku
    case 3: return 2;      // Bakugo
    case 4: return 3;      // Uraraka
    case 5: return 4;      // Todoroki
    case 6: return 5;      // Tenya
    case 7: return 6;      // Tsuyu
    case 8: return 7;      // Denki
    case 9: return 8;      // Kirishima
    case 10: return 10;    // Momo
    case 11: return 11;    // Tokoyami
    case 12: return 12;    // All Might
    case 13: return 13;    // Aizawa
    case 14: return 15;    // Shigaraki
    case 15: return 16;    // All For One
    case 16: return 17;    // Dabi
    case 17: return 18;    // Toga
    case 18: return 23;    // Endeavor
    case 19: return 24;    // Mirio
    case 20: return 25;    // Nejire
    case 21: return 26;    // Tamaki
    case 22: return 34;    // Overhaul
    case 23: return 37;    // Twice
    case 24: return 38;    // Mr. Compress
    case 25: return 43;    // Hawks
    case 26: return 46;    // Itsuka Kendo
    case 27: return 100;   // Mt. Lady
    case 28: return 101;   // Cementoss
    case 29: return 102;   // Ibara
    case 30: return 103;   // Kurogiri
    case 31: return 104;   // Monoma
    case 32: return 105;   // Shinso
    case 33: return 109;   // Present Mic
    case 34: return 111;   // Mirko
    case 35: return 114;   // Star & Stripe
    case 36: return 115;   // Lady Nagant
    case 37: return 200;   // Armored All Might
    case 38: return 201;   // Prime All For One
    case 39: return 202;   // Deku Final
    case 41: return 502;   // Kota
    default: return sdkCharacterId;
    }
}

static bool GetCurrentCharacterCustomizeSnapshot(
    SDK::ACharacterBattle* character,
    int& sdkCharacterId,
    int& dbCharacterCode,
    int& variationNo,
    int& costumeCode)
{
    if (!IsValidPointer(character))
        return false;

    SDK::ACharacterGame* characterGame = static_cast<SDK::ACharacterGame*>(character);
    if (!IsValidPointer(characterGame))
        return false;

    __try
    {
        sdkCharacterId = static_cast<int>(characterGame->BP_GetCharacterId());
        dbCharacterCode = GetDatabaseCharacterCodeFromSDKId(sdkCharacterId);
        variationNo = characterGame->BP_GetVariationNo();
        costumeCode = characterGame->BP_GetCostumeCode();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void InGameHack_AutoLogCurrentRoleSlotState()
{
}

/**
 * Get list of all available variation character IDs as enums
 * Returns all EVariationCharacterId enums in order: Ch001_Variation0, Ch001_Variation1, Ch002_Variation0, etc.
 * This is the direct enum list - no conversion needed!
 */
std::vector<SDK::EVariationCharacterId> GetAllVariationCharacterIds()
{
    static std::vector<SDK::EVariationCharacterId> allIds = {
        // Ch001: 2 variations
        SDK::EVariationCharacterId::Ch001_Variation0,
        SDK::EVariationCharacterId::Ch001_Variation1,
        
        // Ch002: 3 variations
        SDK::EVariationCharacterId::Ch002_Variation0,
        SDK::EVariationCharacterId::Ch002_Variation1,
        SDK::EVariationCharacterId::Ch002_Variation2,
        
        // Ch003: 2 variations
        SDK::EVariationCharacterId::Ch003_Variation0,
        SDK::EVariationCharacterId::Ch003_Variation1,
        
        // Ch004: 2 variations
        SDK::EVariationCharacterId::Ch004_Variation0,
        SDK::EVariationCharacterId::Ch004_Variation1,
        
        // Ch005: 2 variations
        SDK::EVariationCharacterId::Ch005_Variation0,
        SDK::EVariationCharacterId::Ch005_Variation1,
        
        // Ch006: 2 variations
        SDK::EVariationCharacterId::Ch006_Variation0,
        SDK::EVariationCharacterId::Ch006_Variation1,
        
        // Ch007: 2 variations
        SDK::EVariationCharacterId::Ch007_Variation0,
        SDK::EVariationCharacterId::Ch007_Variation1,
        
        // Ch008: 2 variations
        SDK::EVariationCharacterId::Ch008_Variation0,
        SDK::EVariationCharacterId::Ch008_Variation1,
        
        // Ch010: 2 variations
        SDK::EVariationCharacterId::Ch010_Variation0,
        SDK::EVariationCharacterId::Ch010_Variation1,
        
        // Ch011: 2 variations
        SDK::EVariationCharacterId::Ch011_Variation0,
        SDK::EVariationCharacterId::Ch011_Variation1,
        
        // Ch012: 2 variations
        SDK::EVariationCharacterId::Ch012_Variation0,
        SDK::EVariationCharacterId::Ch012_Variation1,
        
        // Ch013: 2 variations
        SDK::EVariationCharacterId::Ch013_Variation0,
        SDK::EVariationCharacterId::Ch013_Variation1,
        
        // Ch015: 3 variations
        SDK::EVariationCharacterId::Ch015_Variation0,
        SDK::EVariationCharacterId::Ch015_Variation1,
        SDK::EVariationCharacterId::Ch015_Variation2,
        
        // Ch016: 2 variations
        SDK::EVariationCharacterId::Ch016_Variation0,
        SDK::EVariationCharacterId::Ch016_Variation1,
        
        // Ch017: 2 variations
        SDK::EVariationCharacterId::Ch017_Variation0,
        SDK::EVariationCharacterId::Ch017_Variation1,
        
        // Ch018: 2 variations
        SDK::EVariationCharacterId::Ch018_Variation0,
        SDK::EVariationCharacterId::Ch018_Variation1,
        
        // Ch023: 2 variations
        SDK::EVariationCharacterId::Ch023_Variation0,
        SDK::EVariationCharacterId::Ch023_Variation1,
        
        // Ch024: 2 variations
        SDK::EVariationCharacterId::Ch024_Variation0,
        SDK::EVariationCharacterId::Ch024_Variation1,
        
        // Ch025: 2 variations
        SDK::EVariationCharacterId::Ch025_Variation0,
        SDK::EVariationCharacterId::Ch025_Variation1,
        
        // Ch026: 2 variations
        SDK::EVariationCharacterId::Ch026_Variation0,
        SDK::EVariationCharacterId::Ch026_Variation1,
        
        // Ch034: 2 variations
        SDK::EVariationCharacterId::Ch034_Variation0,
        SDK::EVariationCharacterId::Ch034_Variation1,
        
        // Ch037: 2 variations
        SDK::EVariationCharacterId::Ch037_Variation0,
        SDK::EVariationCharacterId::Ch037_Variation1,
        
        // Ch038: 2 variations
        SDK::EVariationCharacterId::Ch038_Variation0,
        SDK::EVariationCharacterId::Ch038_Variation1,
        
        // Ch043: 2 variations
        SDK::EVariationCharacterId::Ch043_Variation0,
        SDK::EVariationCharacterId::Ch043_Variation1,
        
        // Ch046: 2 variations
        SDK::EVariationCharacterId::Ch046_Variation0,
        SDK::EVariationCharacterId::Ch046_Variation1,
        
        // Ch100: 2 variations
        SDK::EVariationCharacterId::Ch100_Variation0,
        SDK::EVariationCharacterId::Ch100_Variation1,
        
        // Ch101: 2 variations
        SDK::EVariationCharacterId::Ch101_Variation0,
        SDK::EVariationCharacterId::Ch101_Variation1,
        
        // Ch102: 2 variations
        SDK::EVariationCharacterId::Ch102_Variation0,
        SDK::EVariationCharacterId::Ch102_Variation1,
        
        // Ch103: 2 variations
        SDK::EVariationCharacterId::Ch103_Variation0,
        SDK::EVariationCharacterId::Ch103_Variation1,
        
        // Ch104: 2 variations
        SDK::EVariationCharacterId::Ch104_Variation0,
        SDK::EVariationCharacterId::Ch104_Variation1,
        
        // Ch105: 2 variations
        SDK::EVariationCharacterId::Ch105_Variation0,
        SDK::EVariationCharacterId::Ch105_Variation1,
        
        // Ch109: 2 variations
        SDK::EVariationCharacterId::Ch109_Variation0,
        SDK::EVariationCharacterId::Ch109_Variation1,
        
        // Ch111: 2 variations
        SDK::EVariationCharacterId::Ch111_Variation0,
        SDK::EVariationCharacterId::Ch111_Variation1,
        
        // Ch114: 2 variations
        SDK::EVariationCharacterId::Ch114_Variation0,
        SDK::EVariationCharacterId::Ch114_Variation1,
        
        // Ch115: 2 variations
        SDK::EVariationCharacterId::Ch115_Variation0,
        SDK::EVariationCharacterId::Ch115_Variation1,
        
        // Ch200: 2 variations
        SDK::EVariationCharacterId::Ch200_Variation0,
        SDK::EVariationCharacterId::Ch200_Variation1,
        
        // Ch201: 2 variations
        SDK::EVariationCharacterId::Ch201_Variation0,
        SDK::EVariationCharacterId::Ch201_Variation1,
        
        // Ch202: 2 variations
        SDK::EVariationCharacterId::Ch202_Variation0,
        SDK::EVariationCharacterId::Ch202_Variation1,
    };
    
    return allIds;
}

// ============================================
// CHARACTER VARIATION MAPPING
// ============================================

/**
 * Convert ECharacterId + variation index to EVariationCharacterId
 * @param characterId - The character ID
 * @param variationIndex - The variation index (0, 1, 2, etc.)
 * @return The corresponding EVariationCharacterId
 */
SDK::EVariationCharacterId GetVariationCharacterId(SDK::ECharacterId characterId, int32_t variationIndex)
{
    // Map (ECharacterId, variationIndex) to EVariationCharacterId
    static std::map<std::pair<SDK::ECharacterId, int32_t>, SDK::EVariationCharacterId> variationMap = {
        // Ch001: 2 variations
        { {SDK::ECharacterId::Ch001, 0}, SDK::EVariationCharacterId::Ch001_Variation0 },
        { {SDK::ECharacterId::Ch001, 1}, SDK::EVariationCharacterId::Ch001_Variation1 },
        
        // Ch002: 3 variations
        { {SDK::ECharacterId::Ch002, 0}, SDK::EVariationCharacterId::Ch002_Variation0 },
        { {SDK::ECharacterId::Ch002, 1}, SDK::EVariationCharacterId::Ch002_Variation1 },
        { {SDK::ECharacterId::Ch002, 2}, SDK::EVariationCharacterId::Ch002_Variation2 },
        
        // Ch003: 2 variations
        { {SDK::ECharacterId::Ch003, 0}, SDK::EVariationCharacterId::Ch003_Variation0 },
        { {SDK::ECharacterId::Ch003, 1}, SDK::EVariationCharacterId::Ch003_Variation1 },
        
        // Ch004: 2 variations
        { {SDK::ECharacterId::Ch004, 0}, SDK::EVariationCharacterId::Ch004_Variation0 },
        { {SDK::ECharacterId::Ch004, 1}, SDK::EVariationCharacterId::Ch004_Variation1 },
        
        // Ch005: 2 variations
        { {SDK::ECharacterId::Ch005, 0}, SDK::EVariationCharacterId::Ch005_Variation0 },
        { {SDK::ECharacterId::Ch005, 1}, SDK::EVariationCharacterId::Ch005_Variation1 },
        
        // Ch006: 2 variations
        { {SDK::ECharacterId::Ch006, 0}, SDK::EVariationCharacterId::Ch006_Variation0 },
        { {SDK::ECharacterId::Ch006, 1}, SDK::EVariationCharacterId::Ch006_Variation1 },
        
        // Ch007: 2 variations
        { {SDK::ECharacterId::Ch007, 0}, SDK::EVariationCharacterId::Ch007_Variation0 },
        { {SDK::ECharacterId::Ch007, 1}, SDK::EVariationCharacterId::Ch007_Variation1 },
        
        // Ch008: 2 variations
        { {SDK::ECharacterId::Ch008, 0}, SDK::EVariationCharacterId::Ch008_Variation0 },
        { {SDK::ECharacterId::Ch008, 1}, SDK::EVariationCharacterId::Ch008_Variation1 },
        
        // Ch010: 2 variations
        { {SDK::ECharacterId::Ch010, 0}, SDK::EVariationCharacterId::Ch010_Variation0 },
        { {SDK::ECharacterId::Ch010, 1}, SDK::EVariationCharacterId::Ch010_Variation1 },
        
        // Ch011: 2 variations
        { {SDK::ECharacterId::Ch011, 0}, SDK::EVariationCharacterId::Ch011_Variation0 },
        { {SDK::ECharacterId::Ch011, 1}, SDK::EVariationCharacterId::Ch011_Variation1 },
        
        // Ch012: 2 variations
        { {SDK::ECharacterId::Ch012, 0}, SDK::EVariationCharacterId::Ch012_Variation0 },
        { {SDK::ECharacterId::Ch012, 1}, SDK::EVariationCharacterId::Ch012_Variation1 },
        
        // Ch013: 2 variations
        { {SDK::ECharacterId::Ch013, 0}, SDK::EVariationCharacterId::Ch013_Variation0 },
        { {SDK::ECharacterId::Ch013, 1}, SDK::EVariationCharacterId::Ch013_Variation1 },
        
        // Ch015: 3 variations
        { {SDK::ECharacterId::Ch015, 0}, SDK::EVariationCharacterId::Ch015_Variation0 },
        { {SDK::ECharacterId::Ch015, 1}, SDK::EVariationCharacterId::Ch015_Variation1 },
        { {SDK::ECharacterId::Ch015, 2}, SDK::EVariationCharacterId::Ch015_Variation2 },
        
        // Ch016: 2 variations
        { {SDK::ECharacterId::Ch016, 0}, SDK::EVariationCharacterId::Ch016_Variation0 },
        { {SDK::ECharacterId::Ch016, 1}, SDK::EVariationCharacterId::Ch016_Variation1 },
        
        // Ch017: 2 variations
        { {SDK::ECharacterId::Ch017, 0}, SDK::EVariationCharacterId::Ch017_Variation0 },
        { {SDK::ECharacterId::Ch017, 1}, SDK::EVariationCharacterId::Ch017_Variation1 },
        
        // Ch018: 2 variations
        { {SDK::ECharacterId::Ch018, 0}, SDK::EVariationCharacterId::Ch018_Variation0 },
        { {SDK::ECharacterId::Ch018, 1}, SDK::EVariationCharacterId::Ch018_Variation1 },
        
        // Ch023: 2 variations
        { {SDK::ECharacterId::Ch023, 0}, SDK::EVariationCharacterId::Ch023_Variation0 },
        { {SDK::ECharacterId::Ch023, 1}, SDK::EVariationCharacterId::Ch023_Variation1 },
        
        // Ch024: 2 variations
        { {SDK::ECharacterId::Ch024, 0}, SDK::EVariationCharacterId::Ch024_Variation0 },
        { {SDK::ECharacterId::Ch024, 1}, SDK::EVariationCharacterId::Ch024_Variation1 },
        
        // Ch025: 2 variations
        { {SDK::ECharacterId::Ch025, 0}, SDK::EVariationCharacterId::Ch025_Variation0 },
        { {SDK::ECharacterId::Ch025, 1}, SDK::EVariationCharacterId::Ch025_Variation1 },
        
        // Ch026: 2 variations
        { {SDK::ECharacterId::Ch026, 0}, SDK::EVariationCharacterId::Ch026_Variation0 },
        { {SDK::ECharacterId::Ch026, 1}, SDK::EVariationCharacterId::Ch026_Variation1 },
        
        // Ch034: 2 variations
        { {SDK::ECharacterId::Ch034, 0}, SDK::EVariationCharacterId::Ch034_Variation0 },
        { {SDK::ECharacterId::Ch034, 1}, SDK::EVariationCharacterId::Ch034_Variation1 },
        
        // Ch037: 2 variations
        { {SDK::ECharacterId::Ch037, 0}, SDK::EVariationCharacterId::Ch037_Variation0 },
        { {SDK::ECharacterId::Ch037, 1}, SDK::EVariationCharacterId::Ch037_Variation1 },
        
        // Ch038: 2 variations
        { {SDK::ECharacterId::Ch038, 0}, SDK::EVariationCharacterId::Ch038_Variation0 },
        { {SDK::ECharacterId::Ch038, 1}, SDK::EVariationCharacterId::Ch038_Variation1 },
        
        // Ch043: 2 variations
        { {SDK::ECharacterId::Ch043, 0}, SDK::EVariationCharacterId::Ch043_Variation0 },
        { {SDK::ECharacterId::Ch043, 1}, SDK::EVariationCharacterId::Ch043_Variation1 },
        
        // Ch046: 2 variations
        { {SDK::ECharacterId::Ch046, 0}, SDK::EVariationCharacterId::Ch046_Variation0 },
        { {SDK::ECharacterId::Ch046, 1}, SDK::EVariationCharacterId::Ch046_Variation1 },
        
        // Ch100: 2 variations
        { {SDK::ECharacterId::Ch100, 0}, SDK::EVariationCharacterId::Ch100_Variation0 },
        { {SDK::ECharacterId::Ch100, 1}, SDK::EVariationCharacterId::Ch100_Variation1 },
        
        // Ch101: 2 variations
        { {SDK::ECharacterId::Ch101, 0}, SDK::EVariationCharacterId::Ch101_Variation0 },
        { {SDK::ECharacterId::Ch101, 1}, SDK::EVariationCharacterId::Ch101_Variation1 },
        
        // Ch102: 2 variations
        { {SDK::ECharacterId::Ch102, 0}, SDK::EVariationCharacterId::Ch102_Variation0 },
        { {SDK::ECharacterId::Ch102, 1}, SDK::EVariationCharacterId::Ch102_Variation1 },
        
        // Ch103: 2 variations
        { {SDK::ECharacterId::Ch103, 0}, SDK::EVariationCharacterId::Ch103_Variation0 },
        { {SDK::ECharacterId::Ch103, 1}, SDK::EVariationCharacterId::Ch103_Variation1 },
        
        // Ch104: 2 variations
        { {SDK::ECharacterId::Ch104, 0}, SDK::EVariationCharacterId::Ch104_Variation0 },
        { {SDK::ECharacterId::Ch104, 1}, SDK::EVariationCharacterId::Ch104_Variation1 },
        
        // Ch105: 2 variations
        { {SDK::ECharacterId::Ch105, 0}, SDK::EVariationCharacterId::Ch105_Variation0 },
        { {SDK::ECharacterId::Ch105, 1}, SDK::EVariationCharacterId::Ch105_Variation1 },
        
        // Ch109: 2 variations
        { {SDK::ECharacterId::Ch109, 0}, SDK::EVariationCharacterId::Ch109_Variation0 },
        { {SDK::ECharacterId::Ch109, 1}, SDK::EVariationCharacterId::Ch109_Variation1 },

        { {SDK::ECharacterId::Ch111, 0}, SDK::EVariationCharacterId::Ch109_Variation0 },
        { {SDK::ECharacterId::Ch111, 1}, SDK::EVariationCharacterId::Ch109_Variation1 },

        // Ch114: 2 variations
        { {SDK::ECharacterId::Ch114, 0}, SDK::EVariationCharacterId::Ch114_Variation0 },
        { {SDK::ECharacterId::Ch114, 1}, SDK::EVariationCharacterId::Ch114_Variation1 },
        
        // Ch115: 2 variations
        { {SDK::ECharacterId::Ch115, 0}, SDK::EVariationCharacterId::Ch115_Variation0 },
        { {SDK::ECharacterId::Ch115, 1}, SDK::EVariationCharacterId::Ch115_Variation1 },
        
        // Ch200: 2 variations
        { {SDK::ECharacterId::Ch200, 0}, SDK::EVariationCharacterId::Ch200_Variation0 },
        { {SDK::ECharacterId::Ch200, 1}, SDK::EVariationCharacterId::Ch200_Variation1 },
        
        // Ch201: 2 variations
        { {SDK::ECharacterId::Ch201, 0}, SDK::EVariationCharacterId::Ch201_Variation0 },
        { {SDK::ECharacterId::Ch201, 1}, SDK::EVariationCharacterId::Ch201_Variation1 },
        
        // Ch202: 2 variations
        { {SDK::ECharacterId::Ch202, 0}, SDK::EVariationCharacterId::Ch202_Variation0 },
        { {SDK::ECharacterId::Ch202, 1}, SDK::EVariationCharacterId::Ch202_Variation1 },
    };

    auto it = variationMap.find({characterId, variationIndex});
    if (it != variationMap.end())
    {
        return it->second;
    }
    
    // Default fallback
    return SDK::EVariationCharacterId::UNDEF;
}

/**
 * Get available variations for a character
 * @param characterId - The character to get variations for
 * @return Vector of variation indices (0, 1, 2, etc.)
 */
std::vector<int32_t> GetVariationsForCharacter(SDK::ECharacterId characterId)
{
    // Map each character to their available variations
    // Variation ID is 0 = Variation0, 1 = Variation1, 2 = Variation2, etc.
    static std::map<SDK::ECharacterId, std::vector<int32_t>> variationMap = {
        { SDK::ECharacterId::Ch001, {0, 1} },
        { SDK::ECharacterId::Ch002, {0, 1, 2} },
        { SDK::ECharacterId::Ch003, {0, 1} },
        { SDK::ECharacterId::Ch004, {0, 1} },
        { SDK::ECharacterId::Ch005, {0, 1} },
        { SDK::ECharacterId::Ch006, {0, 1} },
        { SDK::ECharacterId::Ch007, {0, 1} },
        { SDK::ECharacterId::Ch008, {0, 1} },
        { SDK::ECharacterId::Ch010, {0, 1} },
        { SDK::ECharacterId::Ch011, {0, 1} },
        { SDK::ECharacterId::Ch012, {0, 1} },
        { SDK::ECharacterId::Ch013, {0, 1} },
        { SDK::ECharacterId::Ch015, {0, 1, 2} },
        { SDK::ECharacterId::Ch016, {0, 1} },
        { SDK::ECharacterId::Ch017, {0, 1} },
        { SDK::ECharacterId::Ch018, {0, 1} },
        { SDK::ECharacterId::Ch023, {0, 1} },
        { SDK::ECharacterId::Ch024, {0, 1} },
        { SDK::ECharacterId::Ch025, {0, 1} },
        { SDK::ECharacterId::Ch026, {0, 1} },
        { SDK::ECharacterId::Ch034, {0, 1} },
        { SDK::ECharacterId::Ch037, {0, 1} },
        { SDK::ECharacterId::Ch038, {0, 1} },
        { SDK::ECharacterId::Ch043, {0, 1} },
        { SDK::ECharacterId::Ch046, {0, 1} },
        { SDK::ECharacterId::Ch100, {0, 1} },
        { SDK::ECharacterId::Ch101, {0, 1} },
        { SDK::ECharacterId::Ch102, {0, 1} },
        { SDK::ECharacterId::Ch103, {0, 1} },
        { SDK::ECharacterId::Ch104, {0, 1} },
        { SDK::ECharacterId::Ch105, {0, 1} },
        { SDK::ECharacterId::Ch109, {0, 1} },
        { SDK::ECharacterId::Ch111, {0, 1} },
        { SDK::ECharacterId::Ch114, {0, 1} },
        { SDK::ECharacterId::Ch115, {0, 1} },
        { SDK::ECharacterId::Ch200, {0, 1} },
        { SDK::ECharacterId::Ch201, {0, 1} },
        { SDK::ECharacterId::Ch202, {0, 1} },
    };

    auto it = variationMap.find(characterId);
    if (it != variationMap.end())
    {
        return it->second;
    }
    
    // Default: if not found, return just variation 0
    return {0};
}

/**
 * Get the name of a variation for display
 * @param variationId - The variation index (0, 1, 2, etc.)
 * @return String name like "Variation 0", "Variation 1", etc.
 */
std::string GetVariationName(int32_t variationId)
{
    return "Variation " + std::to_string(variationId);
}

/**
 * Convert combo index to actual variation ID
 * @param characterId - The character ID  
 * @param comboIndex - The index from the combo box (0, 1, 2, etc.)
 * @return The actual variation ID
 */
int32_t GetVariationIdFromComboIndex(SDK::ECharacterId characterId, int32_t comboIndex)
{
    auto variations = GetVariationsForCharacter(characterId);
    if (comboIndex < 0 || comboIndex >= static_cast<int32_t>(variations.size()))
    {
        return variations.empty() ? 0 : variations[0];
    }
    return variations[comboIndex];
}

/**
 * Get list of all available variation names for combo display
 * Generates names from enums: Ch001_Variation0, Ch001_Variation1, etc.
 */
std::vector<std::string> GetAllVariationNames()
{
    auto allIds = GetAllVariationCharacterIds();
    std::vector<std::string> names;
    
    // Character code to name mapping
    auto getCharacterName = [](const char* charCode) -> std::string {
        if (strcmp(charCode, "Ch001") == 0) return "Izuku Midoriya";
        if (strcmp(charCode, "Ch002") == 0) return "Katsuki Bakugo";
        if (strcmp(charCode, "Ch003") == 0) return "Ochaco Uraraka";
        if (strcmp(charCode, "Ch004") == 0) return "Shoto Todoroki";
        if (strcmp(charCode, "Ch005") == 0) return "Tenya Iida";
        if (strcmp(charCode, "Ch006") == 0) return "Tsuyu Asui";
        if (strcmp(charCode, "Ch007") == 0) return "Denki Kaminari";
        if (strcmp(charCode, "Ch008") == 0) return "Eijiro Kirishima";
        if (strcmp(charCode, "Ch010") == 0) return "Momo Yaoyorozu";
        if (strcmp(charCode, "Ch011") == 0) return "Fumikage Tokoyami";
        if (strcmp(charCode, "Ch012") == 0) return "All Might";
        if (strcmp(charCode, "Ch013") == 0) return "Shota Aizawa";
        if (strcmp(charCode, "Ch015") == 0) return "Tomura Shigaraki";
        if (strcmp(charCode, "Ch016") == 0) return "All For One";
        if (strcmp(charCode, "Ch017") == 0) return "Dabi";
        if (strcmp(charCode, "Ch018") == 0) return "Himiko Toga";
        if (strcmp(charCode, "Ch023") == 0) return "Endeavor";
        if (strcmp(charCode, "Ch024") == 0) return "Mirio Togata";
        if (strcmp(charCode, "Ch025") == 0) return "Nejire Hado";
        if (strcmp(charCode, "Ch026") == 0) return "Tamaki Amajiki";
        if (strcmp(charCode, "Ch034") == 0) return "Overhaul";
        if (strcmp(charCode, "Ch037") == 0) return "Twice";
        if (strcmp(charCode, "Ch038") == 0) return "Mr Compress";
        if (strcmp(charCode, "Ch043") == 0) return "Hawks";
        if (strcmp(charCode, "Ch046") == 0) return "Itsuka Kendo";
        if (strcmp(charCode, "Ch100") == 0) return "Mt. Lady";
        if (strcmp(charCode, "Ch101") == 0) return "Cementoss";
        if (strcmp(charCode, "Ch102") == 0) return "Ibara Shiozaki";
        if (strcmp(charCode, "Ch103") == 0) return "Kurogiri";
        if (strcmp(charCode, "Ch104") == 0) return "Neito Monoma";
        if (strcmp(charCode, "Ch105") == 0) return "Hitoshi Shinso";
        if (strcmp(charCode, "Ch109") == 0) return "Present Mic";
        if (strcmp(charCode, "Ch111") == 0) return "Mirko";
        if (strcmp(charCode, "Ch114") == 0) return "Star and Stripe";
        if (strcmp(charCode, "Ch115") == 0) return "Lady Nagant";
        if (strcmp(charCode, "Ch200") == 0) return "Armored All Might";
        if (strcmp(charCode, "Ch201") == 0) return "All For One (Young)";
        if (strcmp(charCode, "Ch202") == 0) return "Midoriya (OFA)";
        return charCode;  // Fallback to code
    };
    
    for (auto id : allIds)
    {
        // Get character name and variation from enum value
        const char* charCode = nullptr;
        int varIdx = 0;
        
        // Map enum to character code and variation index
        switch (id)
        {
            case SDK::EVariationCharacterId::Ch001_Variation0: charCode = "Ch001"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch001_Variation1: charCode = "Ch001"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation0: charCode = "Ch002"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch002_Variation1: charCode = "Ch002"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch002_Variation2: charCode = "Ch002"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch003_Variation0: charCode = "Ch003"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch003_Variation1: charCode = "Ch003"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch004_Variation0: charCode = "Ch004"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch004_Variation1: charCode = "Ch004"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch005_Variation0: charCode = "Ch005"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch005_Variation1: charCode = "Ch005"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch006_Variation0: charCode = "Ch006"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch006_Variation1: charCode = "Ch006"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch007_Variation0: charCode = "Ch007"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch007_Variation1: charCode = "Ch007"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch008_Variation0: charCode = "Ch008"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch008_Variation1: charCode = "Ch008"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch010_Variation0: charCode = "Ch010"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch010_Variation1: charCode = "Ch010"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch011_Variation0: charCode = "Ch011"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch011_Variation1: charCode = "Ch011"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch012_Variation0: charCode = "Ch012"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch012_Variation1: charCode = "Ch012"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch013_Variation0: charCode = "Ch013"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch013_Variation1: charCode = "Ch013"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation0: charCode = "Ch015"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch015_Variation1: charCode = "Ch015"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch015_Variation2: charCode = "Ch015"; varIdx = 2; break;
            case SDK::EVariationCharacterId::Ch016_Variation0: charCode = "Ch016"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch016_Variation1: charCode = "Ch016"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch017_Variation0: charCode = "Ch017"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch017_Variation1: charCode = "Ch017"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch018_Variation0: charCode = "Ch018"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch018_Variation1: charCode = "Ch018"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch023_Variation0: charCode = "Ch023"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch023_Variation1: charCode = "Ch023"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch024_Variation0: charCode = "Ch024"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch024_Variation1: charCode = "Ch024"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch025_Variation0: charCode = "Ch025"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch025_Variation1: charCode = "Ch025"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch026_Variation0: charCode = "Ch026"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch026_Variation1: charCode = "Ch026"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch034_Variation0: charCode = "Ch034"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch034_Variation1: charCode = "Ch034"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch037_Variation0: charCode = "Ch037"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch037_Variation1: charCode = "Ch037"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch038_Variation0: charCode = "Ch038"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch038_Variation1: charCode = "Ch038"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch043_Variation0: charCode = "Ch043"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch043_Variation1: charCode = "Ch043"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch046_Variation0: charCode = "Ch046"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch046_Variation1: charCode = "Ch046"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch100_Variation0: charCode = "Ch100"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch100_Variation1: charCode = "Ch100"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch101_Variation0: charCode = "Ch101"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch101_Variation1: charCode = "Ch101"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch102_Variation0: charCode = "Ch102"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch102_Variation1: charCode = "Ch102"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch103_Variation0: charCode = "Ch103"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch103_Variation1: charCode = "Ch103"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch104_Variation0: charCode = "Ch104"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch104_Variation1: charCode = "Ch104"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch105_Variation0: charCode = "Ch105"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch105_Variation1: charCode = "Ch105"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch109_Variation0: charCode = "Ch109"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch109_Variation1: charCode = "Ch109"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch111_Variation0: charCode = "Ch111"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch111_Variation1: charCode = "Ch111"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch114_Variation0: charCode = "Ch114"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch114_Variation1: charCode = "Ch114"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch115_Variation0: charCode = "Ch115"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch115_Variation1: charCode = "Ch115"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch200_Variation0: charCode = "Ch200"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch200_Variation1: charCode = "Ch200"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch201_Variation0: charCode = "Ch201"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch201_Variation1: charCode = "Ch201"; varIdx = 1; break;
            case SDK::EVariationCharacterId::Ch202_Variation0: charCode = "Ch202"; varIdx = 0; break;
            case SDK::EVariationCharacterId::Ch202_Variation1: charCode = "Ch202"; varIdx = 1; break;
            default: charCode = "Unknown"; break;
        }
        
        if (charCode)
        {
            std::string displayName = getCharacterName(charCode) + " (Variation " + std::to_string(varIdx) + ")";
            names.push_back(displayName);
        }
    }
    
    return names;
}

/**
 * Convert EVariationCharacterId back to (ECharacterId, variationId)
 * @param variationCharacterId - The combined variation character ID
 * @return Pair of (ECharacterId, variationId) or (UNDEF, -1) if invalid
 */
std::pair<SDK::ECharacterId, int32_t> GetCharacterAndVariationFromVariationCharacterId(SDK::EVariationCharacterId variationCharacterId)
{
    // Map EVariationCharacterId enum values back to (ECharacterId, variationId)
    // This is the inverse of GetVariationCharacterId()
    
    switch (variationCharacterId)
    {
        // Ch001: variations 0, 1
        case SDK::EVariationCharacterId::Ch001_Variation0: return {SDK::ECharacterId::Ch001, 0};
        case SDK::EVariationCharacterId::Ch001_Variation1: return {SDK::ECharacterId::Ch001, 1};
        
        // Ch002: variations 0, 1, 2
        case SDK::EVariationCharacterId::Ch002_Variation0: return {SDK::ECharacterId::Ch002, 0};
        case SDK::EVariationCharacterId::Ch002_Variation1: return {SDK::ECharacterId::Ch002, 1};
        case SDK::EVariationCharacterId::Ch002_Variation2: return {SDK::ECharacterId::Ch002, 2};
        
        // Ch003: variations 0, 1
        case SDK::EVariationCharacterId::Ch003_Variation0: return {SDK::ECharacterId::Ch003, 0};
        case SDK::EVariationCharacterId::Ch003_Variation1: return {SDK::ECharacterId::Ch003, 1};
        
        // Ch004: variations 0, 1
        case SDK::EVariationCharacterId::Ch004_Variation0: return {SDK::ECharacterId::Ch004, 0};
        case SDK::EVariationCharacterId::Ch004_Variation1: return {SDK::ECharacterId::Ch004, 1};
        
        // Ch005: variations 0, 1
        case SDK::EVariationCharacterId::Ch005_Variation0: return {SDK::ECharacterId::Ch005, 0};
        case SDK::EVariationCharacterId::Ch005_Variation1: return {SDK::ECharacterId::Ch005, 1};
        
        // Ch006: variations 0, 1
        case SDK::EVariationCharacterId::Ch006_Variation0: return {SDK::ECharacterId::Ch006, 0};
        case SDK::EVariationCharacterId::Ch006_Variation1: return {SDK::ECharacterId::Ch006, 1};
        
        // Ch007: variations 0, 1
        case SDK::EVariationCharacterId::Ch007_Variation0: return {SDK::ECharacterId::Ch007, 0};
        case SDK::EVariationCharacterId::Ch007_Variation1: return {SDK::ECharacterId::Ch007, 1};
        
        // Ch008: variations 0, 1
        case SDK::EVariationCharacterId::Ch008_Variation0: return {SDK::ECharacterId::Ch008, 0};
        case SDK::EVariationCharacterId::Ch008_Variation1: return {SDK::ECharacterId::Ch008, 1};
        
        // Ch010: variations 0, 1
        case SDK::EVariationCharacterId::Ch010_Variation0: return {SDK::ECharacterId::Ch010, 0};
        case SDK::EVariationCharacterId::Ch010_Variation1: return {SDK::ECharacterId::Ch010, 1};
        
        // Ch011: variations 0, 1
        case SDK::EVariationCharacterId::Ch011_Variation0: return {SDK::ECharacterId::Ch011, 0};
        case SDK::EVariationCharacterId::Ch011_Variation1: return {SDK::ECharacterId::Ch011, 1};
        
        // Ch012: variations 0, 1
        case SDK::EVariationCharacterId::Ch012_Variation0: return {SDK::ECharacterId::Ch012, 0};
        case SDK::EVariationCharacterId::Ch012_Variation1: return {SDK::ECharacterId::Ch012, 1};
        
        // Ch013: variations 0, 1
        case SDK::EVariationCharacterId::Ch013_Variation0: return {SDK::ECharacterId::Ch013, 0};
        case SDK::EVariationCharacterId::Ch013_Variation1: return {SDK::ECharacterId::Ch013, 1};
        
        // Ch015: variations 0, 1, 2
        case SDK::EVariationCharacterId::Ch015_Variation0: return {SDK::ECharacterId::Ch015, 0};
        case SDK::EVariationCharacterId::Ch015_Variation1: return {SDK::ECharacterId::Ch015, 1};
        case SDK::EVariationCharacterId::Ch015_Variation2: return {SDK::ECharacterId::Ch015, 2};
        
        // Ch016: variations 0, 1
        case SDK::EVariationCharacterId::Ch016_Variation0: return {SDK::ECharacterId::Ch016, 0};
        case SDK::EVariationCharacterId::Ch016_Variation1: return {SDK::ECharacterId::Ch016, 1};
        
        // Ch017: variations 0, 1
        case SDK::EVariationCharacterId::Ch017_Variation0: return {SDK::ECharacterId::Ch017, 0};
        case SDK::EVariationCharacterId::Ch017_Variation1: return {SDK::ECharacterId::Ch017, 1};
        
        // Ch018: variations 0, 1
        case SDK::EVariationCharacterId::Ch018_Variation0: return {SDK::ECharacterId::Ch018, 0};
        case SDK::EVariationCharacterId::Ch018_Variation1: return {SDK::ECharacterId::Ch018, 1};
        
        // Ch023: variations 0, 1
        case SDK::EVariationCharacterId::Ch023_Variation0: return {SDK::ECharacterId::Ch023, 0};
        case SDK::EVariationCharacterId::Ch023_Variation1: return {SDK::ECharacterId::Ch023, 1};
        
        // Ch024: variations 0, 1
        case SDK::EVariationCharacterId::Ch024_Variation0: return {SDK::ECharacterId::Ch024, 0};
        case SDK::EVariationCharacterId::Ch024_Variation1: return {SDK::ECharacterId::Ch024, 1};
        
        // Ch025: variations 0, 1
        case SDK::EVariationCharacterId::Ch025_Variation0: return {SDK::ECharacterId::Ch025, 0};
        case SDK::EVariationCharacterId::Ch025_Variation1: return {SDK::ECharacterId::Ch025, 1};
        
        // Ch026: variations 0, 1
        case SDK::EVariationCharacterId::Ch026_Variation0: return {SDK::ECharacterId::Ch026, 0};
        case SDK::EVariationCharacterId::Ch026_Variation1: return {SDK::ECharacterId::Ch026, 1};
        
        // Ch034: variations 0, 1
        case SDK::EVariationCharacterId::Ch034_Variation0: return {SDK::ECharacterId::Ch034, 0};
        case SDK::EVariationCharacterId::Ch034_Variation1: return {SDK::ECharacterId::Ch034, 1};
        
        // Ch037: variations 0, 1
        case SDK::EVariationCharacterId::Ch037_Variation0: return {SDK::ECharacterId::Ch037, 0};
        case SDK::EVariationCharacterId::Ch037_Variation1: return {SDK::ECharacterId::Ch037, 1};
        
        // Ch038: variations 0, 1
        case SDK::EVariationCharacterId::Ch038_Variation0: return {SDK::ECharacterId::Ch038, 0};
        case SDK::EVariationCharacterId::Ch038_Variation1: return {SDK::ECharacterId::Ch038, 1};
        
        // Ch043: variations 0, 1
        case SDK::EVariationCharacterId::Ch043_Variation0: return {SDK::ECharacterId::Ch043, 0};
        case SDK::EVariationCharacterId::Ch043_Variation1: return {SDK::ECharacterId::Ch043, 1};
        
        // Ch046: variations 0, 1
        case SDK::EVariationCharacterId::Ch046_Variation0: return {SDK::ECharacterId::Ch046, 0};
        case SDK::EVariationCharacterId::Ch046_Variation1: return {SDK::ECharacterId::Ch046, 1};
        
        // Ch100: variations 0, 1
        case SDK::EVariationCharacterId::Ch100_Variation0: return {SDK::ECharacterId::Ch100, 0};
        case SDK::EVariationCharacterId::Ch100_Variation1: return {SDK::ECharacterId::Ch100, 1};
        
        // Ch101: variations 0, 1
        case SDK::EVariationCharacterId::Ch101_Variation0: return {SDK::ECharacterId::Ch101, 0};
        case SDK::EVariationCharacterId::Ch101_Variation1: return {SDK::ECharacterId::Ch101, 1};
        
        // Ch102: variations 0, 1
        case SDK::EVariationCharacterId::Ch102_Variation0: return {SDK::ECharacterId::Ch102, 0};
        case SDK::EVariationCharacterId::Ch102_Variation1: return {SDK::ECharacterId::Ch102, 1};
        
        // Ch103: variations 0, 1
        case SDK::EVariationCharacterId::Ch103_Variation0: return {SDK::ECharacterId::Ch103, 0};
        case SDK::EVariationCharacterId::Ch103_Variation1: return {SDK::ECharacterId::Ch103, 1};
        
        // Ch104: variations 0, 1
        case SDK::EVariationCharacterId::Ch104_Variation0: return {SDK::ECharacterId::Ch104, 0};
        case SDK::EVariationCharacterId::Ch104_Variation1: return {SDK::ECharacterId::Ch104, 1};
        
        // Ch105: variations 0, 1
        case SDK::EVariationCharacterId::Ch105_Variation0: return {SDK::ECharacterId::Ch105, 0};
        case SDK::EVariationCharacterId::Ch105_Variation1: return {SDK::ECharacterId::Ch105, 1};
        
        // Ch109: variations 0, 1
        case SDK::EVariationCharacterId::Ch109_Variation0: return {SDK::ECharacterId::Ch109, 0};
        case SDK::EVariationCharacterId::Ch109_Variation1: return {SDK::ECharacterId::Ch109, 1};

        case SDK::EVariationCharacterId::Ch111_Variation0: return {SDK::ECharacterId::Ch111, 0};
        case SDK::EVariationCharacterId::Ch111_Variation1: return {SDK::ECharacterId::Ch111, 1};
        
        // Ch114: variations 0, 1
        case SDK::EVariationCharacterId::Ch114_Variation0: return {SDK::ECharacterId::Ch114, 0};
        case SDK::EVariationCharacterId::Ch114_Variation1: return {SDK::ECharacterId::Ch114, 1};
        
        // Ch115: variations 0, 1
        case SDK::EVariationCharacterId::Ch115_Variation0: return {SDK::ECharacterId::Ch115, 0};
        case SDK::EVariationCharacterId::Ch115_Variation1: return {SDK::ECharacterId::Ch115, 1};
        
        // Ch200: variations 0, 1
        case SDK::EVariationCharacterId::Ch200_Variation0: return {SDK::ECharacterId::Ch200, 0};
        case SDK::EVariationCharacterId::Ch200_Variation1: return {SDK::ECharacterId::Ch200, 1};
        
        // Ch201: variations 0, 1
        case SDK::EVariationCharacterId::Ch201_Variation0: return {SDK::ECharacterId::Ch201, 0};
        case SDK::EVariationCharacterId::Ch201_Variation1: return {SDK::ECharacterId::Ch201, 1};
        
        // Ch202: variations 0, 1
        case SDK::EVariationCharacterId::Ch202_Variation0: return {SDK::ECharacterId::Ch202, 0};
        case SDK::EVariationCharacterId::Ch202_Variation1: return {SDK::ECharacterId::Ch202, 1};
        
        default: return {SDK::ECharacterId::UNDEF, -1};
    }
}

/**
 * Get character ID and variation index from combo index
 * Maps combo selection index back to (ECharacterId, variationIndex)
 */
std::pair<SDK::ECharacterId, int32_t> GetCharacterAndVariationFromIndex(int32_t comboIndex)
{
    static SDK::ECharacterId characterList[] = {
        SDK::ECharacterId::Ch001, SDK::ECharacterId::Ch002, SDK::ECharacterId::Ch003, SDK::ECharacterId::Ch004,
        SDK::ECharacterId::Ch005, SDK::ECharacterId::Ch006, SDK::ECharacterId::Ch007, SDK::ECharacterId::Ch008,
        SDK::ECharacterId::Ch010, SDK::ECharacterId::Ch011, SDK::ECharacterId::Ch012, SDK::ECharacterId::Ch013,
        SDK::ECharacterId::Ch015, SDK::ECharacterId::Ch016, SDK::ECharacterId::Ch017, SDK::ECharacterId::Ch018,
        SDK::ECharacterId::Ch023, SDK::ECharacterId::Ch024, SDK::ECharacterId::Ch025, SDK::ECharacterId::Ch026,
        SDK::ECharacterId::Ch034, SDK::ECharacterId::Ch037, SDK::ECharacterId::Ch038, SDK::ECharacterId::Ch043,
        SDK::ECharacterId::Ch046, SDK::ECharacterId::Ch100, SDK::ECharacterId::Ch101, SDK::ECharacterId::Ch102,
        SDK::ECharacterId::Ch103, SDK::ECharacterId::Ch104, SDK::ECharacterId::Ch105, SDK::ECharacterId::Ch109, SDK::ECharacterId::Ch111,
        SDK::ECharacterId::Ch114, SDK::ECharacterId::Ch115, SDK::ECharacterId::Ch200, SDK::ECharacterId::Ch201,
        SDK::ECharacterId::Ch202
    };
    
    int currentIndex = 0;
    
    // Find the character and variation at the given combo index
    for (const auto& charId : characterList)
    {
        auto variations = GetVariationsForCharacter(charId);
        if (comboIndex < currentIndex + static_cast<int32_t>(variations.size()))
        {
            int varIdx = variations[comboIndex - currentIndex];
            return {charId, varIdx};
        }
        currentIndex += static_cast<int32_t>(variations.size());
    }
    
    // Invalid index
    return {SDK::ECharacterId::UNDEF, -1};
}

/**
 * Change character on server with logging
 * @param PlayerController - The APlayerControllerBattle instance
 * @param CurrentCharacter - Current character battle pawn
 * @param NewCharacterData - The new character data to apply
 */
bool TrainingHack_ChangeCharacterOnServer(
    SDK::APlayerControllerBattle* PlayerController,
    SDK::ACharacterBattle* CurrentCharacter,
    const SDK::FInGameBattleCharacterData& NewCharacterData)
{
    if (!PlayerController || !CurrentCharacter)
    {
        Logger::LogError("[CHARACTER] Invalid controller or character");
        return false;
    }

    try
    {
        PlayerController->ChangeCharacter_OnServer(CurrentCharacter, NewCharacterData);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CHARACTER] Exception: " + std::string(e.what()));
        return false;
    }
}

// ============================================
// TRANSFORM HACKS - Character Transformation
// ============================================

/**
 * Transform player into target character on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to transform into
 */
bool InGameHack_TransformInto(
    SDK::APlayerController* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter)
{

    // Validate inputs
    if (!IsValidPointer(playerController) || !IsValidPointer(playerCharacter) || !IsValidPointer(targetCharacter))
    {
        return false;
    }

    try
    {
        // Get the player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UTransformControlComponent from player state
        SDK::UTransformControlComponent* transformComponent = playerStateBattle->_transformControlComponent;
        if (!IsValidPointer(transformComponent))
        {
            return false;
        }

        // Call the RPC function on the component
        transformComponent->TransformInto_RPC_ToServer(targetCharacter);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Transform into random ESP target (hotkey wrapper)
 * Gets player and random target from ESP, then calls InGameHack_TransformInto
 */
bool InGameHack_TransformIntoRandomESP()
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }
        
        // Get possessed pawn (player character) with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            return false;
        }
        
        // Get random target from ESP using SDK function
        SDK::AActor* targetActorPtr = SDK_GetRandomESPTarget();
        if (!targetActorPtr)
        {
            return false;
        }
        
        // Cast to ACharacterBattle
        SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(targetActorPtr);
        if (!targetCharacter)
        {
            return false;
        }
        
        // Call the transform hack
        return InGameHack_TransformInto(playerController, playerCharacter, targetCharacter);
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// DUPLICATE HACKS - Character Duplication
// ============================================

/**
 * Duplicate player into target character on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to duplicate into
 * @param spawnLocation - Where to spawn the duplicate
 */
/**
 * Duplicate player into target character for imitation on server
 * @param playerController - The player controller
 * @param playerCharacter - The player's current character pawn
 * @param targetCharacter - The target character to duplicate into
 * @param spawnLocation - Where to spawn the duplicate
 * @param lifeTime - How long the imitation lasts
 */
bool InGameHack_DuplicateIntoImitation(
    SDK::APlayerController* playerController,
    SDK::ACharacterBattle* playerCharacter,
    const SDK::ACharacterBattle* targetCharacter,
    const SDK::FVector& spawnLocation,
    float lifeTime)
{
    // Validate inputs
    if (!IsValidPointer(playerController) || !IsValidPointer(playerCharacter) || !IsValidPointer(targetCharacter))
    {
        return false;
    }

    try
    {
        // Get the player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access the UDuplicateControlComponent from player state (offset 0x0C68)
        SDK::UDuplicateControlComponent* duplicateComponent = playerStateBattle->_duplicateControlComponent;
        if (!IsValidPointer(duplicateComponent))
        {
            return false;
        }
        
        // Call the RPC function on the component with attackId = 0 (default)
        // EAttackId is an enum, using 0 as default
        duplicateComponent->DuplicateIntoForImitation_RPC_ToServer(targetCharacter, spawnLocation, (SDK::EAttackId)1, lifeTime);
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Duplicate into random ESP target for imitation (hotkey wrapper)
 * Gets player and forward target from ESP, then calls InGameHack_DuplicateIntoImitation N times
 */
bool InGameHack_DuplicateIntoImitationRandomESP(float spawnOffsetZ, float lifeTime, int count)
{
    try
    {
        // Clamp count to valid range
        if (count < 1 || count > 100)
        {
            count = 1;
        }

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            return false;
        }
        
        // Get possessed pawn (player character)
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            return false;
        }
        
        // Get player position and forward vector
        SDK::FVector playerPos = static_cast<SDK::AActor*>(playerCharacter)->K2_GetActorLocation();
        SDK::FVector forwardVector = static_cast<SDK::AActor*>(playerCharacter)->GetActorForwardVector();
        
        // Spawn N imitation duplicates in front of player
        bool anySuccess = false;
        for (int i = 0; i < count; i++)
        {
            // Get forward target from ESP using SDK function
            SDK::AActor* targetActorPtr = SDK_GetForwardESPTarget();
            if (!targetActorPtr)
            {
                continue;  // Try next duplicate
            }
            
            // Cast to ACharacterBattle
            SDK::ACharacterBattle* targetCharacter = ActorToCharacterBattleSafe(targetActorPtr);
            if (!targetCharacter)
            {
                continue;  // Try next duplicate
            }
            
            // Calculate spawn location: player position + forward * 200 (no Z offset)
            SDK::FVector spawnLocation = playerPos + (forwardVector * 200.0f);
            
            // Call the duplicate imitation hack
            if (InGameHack_DuplicateIntoImitation(playerController, playerCharacter, targetCharacter, spawnLocation, lifeTime))
            {
                anySuccess = true;
            }
        }
        
        return anySuccess;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// BUFF/STAT ADJUSTMENT IMPLEMENTATIONS
// ============================================

/**
 * Set general reload adjust rate via BuffParam
 */
bool InGameHack_SetReloadAdjustRate(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = GetBuffParamSafe(playerStateBattle);
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate
        buffParam->BP_SetAttackAdjustRate_Unique1(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set reload adjust rate for roll slot
 */
bool InGameHack_SetReloadAdjustRate_RollSlot(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate_RollSlot] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = GetBuffParamSafe(playerStateBattle);
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate for roll slot
        buffParam->BP_SetReloadAdjustRate_RollSlot(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set reload adjust rate while wearing blue flame
 */
bool InGameHack_SetReloadAdjustRate_WearBlueFlame(float rate)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetReloadAdjustRate_WearBlueFlame] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        // Get player state with validation
        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerController);
        if (!playerStateBattle)
        {
            return false;
        }
        
        // Access BuffParam
        SDK::UBuffParam* buffParam = GetBuffParamSafe(playerStateBattle);
        if (!IsValidPointer(buffParam))
        {
            return false;
        }
        
        // Set the reload adjust rate for blue flame
        buffParam->BP_SetReloadAdjustRate_WearBlueFlame(rate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}


/**
 * Apply training player configuration (player only, no AI)
 */
bool InGameHack_ApplyPlayerConfiguration(int characterId, int variationId, int unique1, int unique2, int unique3, int skillCode, int costumeCode, int costumeAuraType)
{
    try
    {
        // Check if in valid battle mode first
        if (!IsValidBattleMode())
        {
            Logger::LogWarning("[ApplyPlayerConfiguration] Not in valid battle mode");
            return false;
        }

        // Use Character_Changer API for self change - it handles the SDK enum mapping correctly
        Cheats::ChangeCharacter(characterId, variationId);
        Logger::LogInfo("[ApplyPlayerConfiguration] Applied character change to self");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyPlayerConfiguration] Exception caught");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ApplyPlayerConfiguration] Unknown exception");
        return false;
    }
}

// ============================================
// APPLY TO ALL CONTROLLERS
// ============================================

/**
 * Apply player configuration to ALL enemy characters in the match
 * Same exploit method as Dll.cpp: local player controller calls ChangeCharacter_OnServer for each enemy
 * Uses SDK method call directly (not ProcessEvent)
 * Returns: number of characters changed (0 if failed or no characters found)
 */
int InGameHack_ApplyToAllControllers(int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToAll] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
        {
            Logger::LogError("[ApplyToAll] Could not get world");
            return 0;
        }

        // Get LOCAL player controller
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[ApplyToAll] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ApplyToAll] Invalid battle PC");
            return 0;
        }

        // Use Character_Changer to handle all players - it manages the SDK enum mapping correctly
        Cheats::ChangeCharacterAllPlayers(characterId, variationId);
        Logger::LogInfo("[ApplyToAll] Applied character change to all players");
        return 1;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToAll] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToAll] Unknown exception");
        return 0;
    }
}

// ============================================
// GET ALL PLAYER NAMES
// ============================================

/**
 * Get all player names on the map
 * Returns vector with format: "PlayerName (CharacterClass)"
 */
std::vector<std::string> InGameHack_GetAllPlayerNames()
{
    std::vector<std::string> playerNames;
    try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
            return playerNames;

        // Loop over all actors to find characters with PlayerState
        for (int i = 0; i < actorCount; i++)
        {
            SDK::ACharacterBattle* character = ActorToCharacterBattleSafe(GetWorldActorSafe(world, i));
            if (!character)
                continue;

            SDK::APlayerState* playerState = GetCharacterPlayerStateSafe(character);
            if (!playerState)
                continue;

            std::string playerName = GetPlayerNameSafe(playerState);
            std::string className = "Unknown";
            GetClassNameSafe(character, className);

            playerNames.push_back(playerName + " (" + className + ")");
        }
    }
    catch (...)
    {
    }

    return playerNames;
}

// ============================================
// APPLY TO SPECIFIC PLAYER
// ============================================

/**
 * Apply character change to a specific player by index
 * @param playerIndex - Index in the player list (from GetAllPlayerNames)
 * @param variationCharacterId - The variation character ID enum
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_ApplyToSpecificPlayer(int playerIndex, SDK::EVariationCharacterId variationCharacterId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToSpecificPlayer] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Decode the variation character ID
        auto [characterId, variationId] = GetCharacterAndVariationFromVariationCharacterId(variationCharacterId);

        // Use Character_Changer API which handles mapping correctly
        Cheats::ChangeCharacterIndividual(playerIndex, (int)characterId, variationId);
        Logger::LogInfo("[ApplyToSpecificPlayer] Applied character change to player at index " + std::to_string(playerIndex));
        return 1;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToSpecificPlayer] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToSpecificPlayer] Unknown exception");
        return 0;
    }
}

// ============================================
// APPLY TO TEAM
// ============================================

/**
 * Apply player configuration to ALL characters in a specific team
 * Uses same exploit as ApplyToAllControllers but filters by team ID
 */
bool InGameHack_ApplyToTeam(unsigned char teamId, int characterId, int variationId, int unique1, int unique2, int unique3, int costumeCode, int costumeAuraType)
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[ApplyToTeam] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get LOCAL player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!basePlayerController)
        {
            Logger::LogError("[ApplyToTeam] Could not get player controller");
            return false;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ApplyToTeam] Invalid battle PC");
            return false;
        }

        // Verify player pawn exists
        SDK::ACharacterBattle* playerPawn = GetPlayerCharacterBattle(battlePC);
        if (!playerPawn)
        {
            Logger::LogError("[ApplyToTeam] No valid player pawn");
            return false;
        }

        // Get player's team ID
        unsigned char myTeamId = playerPawn->BP_GetTeamId();
        
        // Use Character_Changer API which handles mapping correctly
        if (teamId == myTeamId)
        {
            // Apply to teammates
            Cheats::ChangeCharacterTeammatesOnly(characterId, variationId);
            Logger::LogInfo("[ApplyToTeam] Applied character change to team (teammates)");
        }
        else
        {
            // Apply to enemies
            Cheats::ChangeCharacterEnemiesOnly(characterId, variationId);
            Logger::LogInfo("[ApplyToTeam] Applied character change to team (enemies)");
        }
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ApplyToTeam] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ApplyToTeam] Unknown exception");
        return false;
    }
}

// ============================================
// COPY SKILLS FROM CHARACTER
// ============================================

/**
 * Copy skills from a RANDOM enemy character to the local player
 * Automatically finds a random enemy and copies their skills
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_CopySkillsFromNearestEnemy(bool bSetCopySkill, bool bUseOwnerCharacterLevel)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[CopySkillsFromNearestEnemy] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Get local player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get valid battle controller");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get player character");
            return 0;
        }

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Could not get world");
            return 0;
        }

        // Find ALL enemies
        std::vector<SDK::ACharacterBattle*> enemies;

        for (int i = 0; i < actorCount; i++)
        {
            SDK::ACharacterBattle* targetCharacter = ActorToCharacterBattleSafe(GetWorldActorSafe(world, i));
            if (!targetCharacter)
                continue;
            
            // Skip if it's the local player
            if (targetCharacter == localCharacter)
                continue;

            // Skip if character has no SkillManagementComponent
            if (!GetSkillManagementComponentSafe(targetCharacter))
                continue;

            enemies.push_back(targetCharacter);
        }

        if (enemies.empty())
        {
            Logger::LogWarning("[CopySkillsFromNearestEnemy] No valid enemies found to copy skills from");
            return 0;
        }

        // Pick a random enemy
        int randomIndex = rand() % enemies.size();
        SDK::ACharacterBattle* randomEnemy = enemies[randomIndex];

        // Final safety check
        if (!IsValidPointer(randomEnemy) || IsObjectDefaultSafe(randomEnemy) || !GetSkillManagementComponentSafe(randomEnemy))
        {
            Logger::LogError("[CopySkillsFromNearestEnemy] Selected random enemy became invalid");
            return 0;
        }

        // Now copy skills from the random enemy
        return InGameHack_CopySkillsFromCharacter(randomEnemy, bSetCopySkill, bUseOwnerCharacterLevel);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CopySkillsFromNearestEnemy] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[CopySkillsFromNearestEnemy] Unknown exception");
        return 0;
    }
}

/**
 * Copy skills from ONE specific character to the local player
 * The character to copy from must NOT be the local player
 * Returns: 1 if successful, 0 if failed
 */
int InGameHack_CopySkillsFromCharacter(SDK::ACharacterBattle* masterCharacter, bool bSetCopySkill, bool bUseOwnerCharacterLevel)
{
    // Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[CopySkillsFromCharacter] Not in valid battle mode");
        return 0;
    }

    try
    {
        // Validate master character early
        if (!IsValidPointer(masterCharacter) || IsObjectDefaultSafe(masterCharacter))
        {
            Logger::LogError("[CopySkillsFromCharacter] Invalid master character");
            return 0;
        }

        // Get local player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get valid battle controller");
            return 0;
        }

        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get player character");
            return 0;
        }

        // IMPORTANT: Do NOT copy from yourself!
        if (masterCharacter == localCharacter)
        {
            Logger::LogWarning("[CopySkillsFromCharacter] Cannot copy skills from yourself");
            return 0;
        }

        // Get the SkillManagementComponent from the LOCAL player
        SDK::USkillManagementComponent* skillMgmt = GetSkillManagementComponentSafe(localCharacter);
        
        if (!IsValidPointer(skillMgmt))
        {
            Logger::LogError("[CopySkillsFromCharacter] Could not get SkillManagementComponent from local player");
            return 0;
        }

        // Verify master character has a SkillManagementComponent before attempting to copy
        if (!GetSkillManagementComponentSafe(masterCharacter))
        {
            Logger::LogError("[CopySkillsFromCharacter] Master character has no SkillManagementComponent");
            return 0;
        }

        // Verify master character is still valid and not default object
        if (IsObjectDefaultSafe(masterCharacter))
        {
            Logger::LogError("[CopySkillsFromCharacter] Master character became invalid (default object)");
            return 0;
        }

        // Activate copy mode first, then set the character to copy from
        try
        {
            // Start copy mode with a 30 second duration (using None type)
            skillMgmt->BP_StartCopyMode(300.0f, (SDK::ECopyModeCharacterType)1);
            Logger::LogInfo("[CopySkillsFromCharacter] Started copy mode");

            // Verify master character is STILL valid after starting copy mode
            if (IsObjectDefaultSafe(masterCharacter))
            {
                Logger::LogError("[CopySkillsFromCharacter] Master character became invalid after starting copy mode");
                return 0;
            }

            // Now set the master character to copy from
            skillMgmt->BP_SetCopyCharacter(masterCharacter, bSetCopySkill, bUseOwnerCharacterLevel);
            Logger::LogInfo("[CopySkillsFromCharacter] Successfully copied skills from enemy character");
            return 1;
        }
        catch (const std::exception& ex)
        {
            Logger::LogError("[CopySkillsFromCharacter] Failed to execute copy sequence: " + std::string(ex.what()));
            return 0;
        }
        catch (...)
        {
            Logger::LogError("[CopySkillsFromCharacter] Failed to execute copy sequence (unknown error)");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CopySkillsFromCharacter] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[CopySkillsFromCharacter] Unknown exception");
        return 0;
    }
}

/**
 * Change my team ID to a random available team (excluding current team)
 * Gets all available teams, excludes current team, and switches to a random one
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeam()
{
    try
    {
        if (!IsValidBattleMode())
        {
            Logger::LogWarning("[ChangeMyTeam] Not in valid battle mode");
            return 0;
        }

        // Get player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[ChangeMyTeam] Could not get valid player controller");
            return 0;
        }

        // Cast to HerovsPlayerState to access team functions
        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(GetControllerPlayerStateSafe(basePlayerController));
        if (!IsValidPointer(playerState))
        {
            Logger::LogError("[ChangeMyTeam] Could not get HerovsPlayerState");
            return 0;
        }

        // Get my current team ID
        int myCurrentTeamId = -1;
        try
        {
            myCurrentTeamId = playerState->BP_GetTeamId();
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeam] Failed to get current team ID");
            return 0;
        }

        Logger::LogInfo("[ChangeMyTeam] Current team ID: " + std::to_string(myCurrentTeamId));

        // Get world to scan all characters and find available teams
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
        {
            Logger::LogError("[ChangeMyTeam] Could not get world");
            return 0;
        }

        // Collect all unique team IDs in the match
        std::vector<int> availableTeams;
        for (int i = 0; i < actorCount; i++)
        {
            SDK::ACharacterBattle* character = ActorToCharacterBattleSafe(GetWorldActorSafe(world, i));
            if (!character)
                continue;
            
            try
            {
                int teamId = character->BP_GetTeamId();
                
                // Check if this team ID is already in our list
                bool teamFound = false;
                for (int existingTeamId : availableTeams)
                {
                    if (existingTeamId == teamId)
                    {
                        teamFound = true;
                        break;
                    }
                }
                
                // Add new team ID if not already in list and valid
                if (!teamFound && teamId >= 0 && teamId != myCurrentTeamId)
                {
                    availableTeams.push_back(teamId);
                }
            }
            catch (...)
            {
                continue;
            }
        }

        // Check if we found any other teams
        if (availableTeams.empty())
        {
            Logger::LogWarning("[ChangeMyTeam] No other teams found to join");
            return 0;
        }

        // Select a random team from available teams
        int randomIndex = rand() % availableTeams.size();
        int newTeamId = availableTeams[randomIndex];

        Logger::LogInfo("[ChangeMyTeam] Switching to team ID: " + std::to_string(newTeamId));

        // Change team via BP_SetTeamId
        try
        {
            playerState->BP_SetTeamId(newTeamId);
            Logger::LogInfo("[ChangeMyTeam] Successfully changed team to: " + std::to_string(newTeamId));
            return 1;
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeam] Failed to call BP_SetTeamId");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeMyTeam] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ChangeMyTeam] Unknown exception");
        return 0;
    }
}

/**
 * Change my team ID to a specific team ID
 * @param targetTeamId - The team ID to switch to
 * @return 1 if successful, 0 if failed
 */
int InGameHack_ChangeMyTeamTo(unsigned char targetTeamId)
{
    try
    {
        if (!IsValidBattleMode())
            return 0;

        // Get player controller and character
        SDK::APlayerController* basePlayerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePlayerController))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get valid player controller");
            return 0;
        }

        SDK::APlayerControllerBattle* battlePC = static_cast<SDK::APlayerControllerBattle*>(basePlayerController);
        if (!IsValidPointer(battlePC))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get valid battle controller");
            return 0;
        }

        // Get player character with validation
        SDK::ACharacterBattle* localCharacter = GetPlayerCharacterBattle(battlePC);
        if (!localCharacter)
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get player character");
            return 0;
        }

        SDK::AHerovsPlayerState* playerState = static_cast<SDK::AHerovsPlayerState*>(GetCharacterPlayerStateSafe(localCharacter));
        if (!IsValidPointer(playerState))
        {
            Logger::LogError("[ChangeMyTeamTo] Could not get HerovsPlayerState");
            return 0;
        }

        // Get my current team ID
        int myCurrentTeamId = playerState->BP_GetTeamId();

        Logger::LogInfo("[ChangeMyTeamTo] Switching to team ID: " + std::to_string(targetTeamId));

        // Change team via BP_SetTeamId
        try
        {
            playerState->BP_SetTeamId(targetTeamId);
            Logger::LogInfo("[ChangeMyTeamTo] Successfully changed team to: " + std::to_string(targetTeamId));
            return 1;
        }
        catch (...)
        {
            Logger::LogError("[ChangeMyTeamTo] Failed to call BP_SetTeamId");
            return 0;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeMyTeamTo] Exception: " + std::string(e.what()));
        return 0;
    }
    catch (...)
    {
        Logger::LogError("[ChangeMyTeamTo] Unknown exception");
        return 0;
    }
}

/**
 * Recover self (player only) with full health restoration
 * Uses RecoverDying_ToClient on player state
 */
bool InGameHack_RecoverMe()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        // Get player state with validation
        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        if (!playerState)
            return false;

        // Check if player is dying FIRST
        if (!playerState->BP_IsDying())
            return false;  // Not dying, no need to recover

        // Get player character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
            return false;

        playerCharacter->RecoverDyingAlly_ToServer(playerCharacter, true);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Set player invincible with custom settings via SetInvincible_Server
 * @param fixTime - Fixed invincibility time
 * @param maxTime - Maximum invincibility time
 * @param enableEffect - Show visual effects
 * @param projectileThrough - Projectiles pass through
 * @param slipDamageThrough - Slip damage passes through
 * @param activedTransparent - Make player transparent
 */
bool InGameHack_SetInvincible()
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[SetInvincible] Not in valid battle mode");
        return false;
    }

    try
    {
        SDK::APlayerController* playerControllerPtr = SDK_GetPlayerController();
        if (!IsValidPointer(playerControllerPtr))
        {
            return false;
        }

        SDK::APlayerStateBattle* playerStateBattle = GetPlayerStateBattle(playerControllerPtr);
        if (!playerStateBattle)
        {
            return false;
        }

        // Hardcoded parameters - all features enabled
        float fixTime = 10.0f;          // Fixed duration
        float maxTime = 120.0f;         // Maximum duration
        bool enableEffect = true;       // Show effects
        // Create FInvincibleData structure
        SDK::FInvincibleData invincibleData;
        invincibleData._lifeTime = maxTime;
        invincibleData._fixedLifeTime = fixTime;
        invincibleData._bProjectileThrough = true;
        invincibleData._bSlipDamageThrough = true;
        invincibleData._bFromTransparent = true;
        invincibleData._bEnableEffect = enableEffect;
        invincibleData._bRep = true;
        // invincibleData._ignoreClearAction and _tags are already initialized to empty by constructor

        // Call SetInvincible_Server with FInvincibleData
        playerStateBattle->SetInvincible_Server(invincibleData);

        return true;
    }
    catch (const std::exception& ex)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// REBUILD MYSELF
// ============================================

/**
 * Rebuild/reset the player character state
 * Calls SetCharacterCondition with REBUILD_MYSELF (enum value 24)
 */
bool InGameHack_RebuildMyself()
{
    // Check if in valid battle mode first
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[RebuildMyself] Not in valid battle mode");
        return false;
    }

    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[RebuildMyself] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[RebuildMyself] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[RebuildMyself] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with REBUILD_MYSELF (enum value 24)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)23,  // REBUILD_MYSELF = 24
            0,                               // Level
            0.0f,                            // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        Logger::LogInfo("[COMBAT] RebuildMyself executed successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in RebuildMyself");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in RebuildMyself");
        return false;
    }
}

bool InGameHack_SetInfiniteObjectsPatch(bool enabled)
{
    std::lock_guard<std::mutex> lock(g_InfiniteObjectsPatchMutex);

    BYTE* patchAddress = GetInfiniteObjectsPatchAddress();
    if (!patchAddress)
        return false;

    if (!enabled)
    {
        if (!g_InfiniteObjectsPatchApplied)
            return true;

        if (!PatchBytesMatch(patchAddress, INFINITE_OBJECTS_PATCH_BYTES))
        {
            g_InfiniteObjectsPatchApplied = false;
            return true;
        }

        const bool restored = WritePatchBytes(patchAddress, INFINITE_OBJECTS_ORIGINAL_BYTES);
        if (restored)
            g_InfiniteObjectsPatchApplied = false;

        return restored;
    }

    if (PatchBytesMatch(patchAddress, INFINITE_OBJECTS_PATCH_BYTES))
    {
        g_InfiniteObjectsPatchApplied = true;
        return true;
    }

    if (!PatchBytesMatch(patchAddress, INFINITE_OBJECTS_ORIGINAL_BYTES))
    {
        if (!g_InfiniteObjectsMismatchLogged)
        {
            Logger::LogWarning("[InfiniteObjects] Byte assertion failed at MHUR.exe+3ED21F5");
            g_InfiniteObjectsMismatchLogged = true;
        }
        return false;
    }

    const bool patched = WritePatchBytes(patchAddress, INFINITE_OBJECTS_PATCH_BYTES);
    if (patched)
    {
        g_InfiniteObjectsPatchApplied = true;
        g_InfiniteObjectsMismatchLogged = false;
    }

    return patched;
}

void InGameHack_RestoreInfiniteObjectsPatch()
{
    (void)InGameHack_SetInfiniteObjectsPatch(false);
}

/**
 * Apply CH202_TRANS_MISSION condition to player character
 * Enables Ch202 transformation/mission state (ECharacterConditionId = 85)
 */
bool InGameHack_CH202TransMission()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[CH202TransMission] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[CH202TransMission] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[CH202TransMission] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with CH202_TRANS_MISSION (enum value 85)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)85,  // CH202_TRANS_MISSION = 85
            5,                               // Level
            0.0f,                            // span
            0.0f,                            // value
            0.0f,                            // interval
            5,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        Logger::LogInfo("[COMBAT] CH202TransMission executed successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH202TransMission");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH202TransMission");
        return false;
    }
}

/**
 * Apply UNBREAKABLE condition to player character
 * Makes player unbreakable/invulnerable (ECharacterConditionId = 35)
 */
bool InGameHack_Unbreakable()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[Unbreakable] Could not get valid PlayerController");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[Unbreakable] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[Unbreakable] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with UNBREAKABLE (enum value 35)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)35,  // UNBREAKABLE = 35
            0,                               // Level
            50.0f,                           // span
            0.0f,                            // value
            0.0f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in Unbreakable");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in Unbreakable");
        return false;
    }
}

/**
 * Get the team ID of a character using SDK function
 */
static inline unsigned char GetCharacterTeamId(SDK::ACharacterBattle* Character)
{
    if (!IsValidPointer(Character)) return 255;  // Invalid
    try {
        // Use SDK function instead of offset reading
        return Character->BP_GetTeamId();
    }
    catch (...) {
        return 255;  // Return invalid if call fails
    }
}

/**
 * Recover player + all team members with full health restoration
 * Uses RecoverDyingAlly_ToServer on all team characters
 */
bool InGameHack_RecoverDyingTeam()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        // Get player character and state
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter || !GetCharacterPlayerStateSafe(playerCharacter))
            return false;

        // Get player's team ID
        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return false;  // Invalid team

        int recoveredCount = 0;
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount) || actorCount == 0)
            return false;

        // Recover all team characters on map
        for (int i = 0; i < actorCount; i++)
        {
            try
            {
                SDK::AActor* actor = GetWorldActorSafe(world, i);
                std::string className = "Unknown";
                
                // 1️⃣ Vérifie que l'acteur existe et n'est pas un default object
                if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor))
                    continue;

                // 2️⃣ Vérifie le classname (CharacterBattle ou Chxxx)
                if (!GetClassNameSafe(actor, className))
                    continue;

                bool isValidCharacter = (className == "CharacterBattle" || className == "ACharacterBattle");
                
                // Check for Chxxx pattern (Ch001-Ch999)
                bool isChxxx = false;
                if (className.length() == 5 && className[0] == 'C' && className[1] == 'h' &&
                    std::isdigit(className[2]) && std::isdigit(className[3]) && std::isdigit(className[4]))
                {
                    isChxxx = true;
                }

                if (!isValidCharacter && !isChxxx)
                    continue;

                // 3️⃣ Cast à ACharacterBattle
                SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
                
                // 4️⃣ Récupère et vérifie le PlayerState
                if (!IsValidPointer(targetCharacter) || !GetCharacterPlayerStateSafe(targetCharacter))
                    continue;

                // 5️⃣ Vérifie que le target est dans la même équipe
                unsigned char targetTeamId = GetCharacterTeamId(targetCharacter);
                if (targetTeamId != playerTeamId)
                    continue;  // Not in same team, skip

                // 6️⃣ Vérifie que le target est dying (using offset)
                if (!IsCharacterDyingOffset(targetCharacter))
                    continue;  // Not dying, skip

                // 7️⃣ Appelle la recovery
                playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
                recoveredCount++;
            }
            catch (...)
            {
                continue;
            }
        }

        return recoveredCount > 0;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Recover a specific team member by index
 * Uses RecoverDyingAlly_ToServer on selected team character
 */
bool InGameHack_RecoverDyingSpecificTeamMember(int teamMemberIndex)
{
    // 0️⃣ Check if in valid battle mode
    if (!IsValidBattleMode())
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Not in valid battle mode");
        return false;
    }

    // 1️⃣ Validate team member index
    if (teamMemberIndex < 0 || teamMemberIndex >= (int)ImGuiMenu::g_CurrentTeamCharacters.size())
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Invalid team member index: " + std::to_string(teamMemberIndex));
        return false;
    }

    // 2️⃣ Get player controller
    SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
    if (!IsValidPointer(playerController))
    {
        Logger::LogWarning("[RECOVER SPECIFIC] No player controller");
        return false;
    }

    // 3️⃣ Get player character
    SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
    if (!playerCharacter || !GetCharacterPlayerStateSafe(playerCharacter))
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Player character not found");
        return false;
    }

    // 4️⃣ Get target team member
    SDK::ACharacterBattle* targetCharacter = ImGuiMenu::g_CurrentTeamCharacters[teamMemberIndex];
    if (!IsValidPointer(targetCharacter) || IsObjectDefaultSafe(targetCharacter) || !GetCharacterPlayerStateSafe(targetCharacter))
    {
        Logger::LogWarning("[RECOVER SPECIFIC] Target character is invalid");
        return false;
    }

    try
    {
        // 5️⃣ Verify target is on same team
        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        unsigned char targetTeamId = GetCharacterTeamId(targetCharacter);
        
        if (playerTeamId == 255 || targetTeamId != playerTeamId)
        {
            Logger::LogWarning("[RECOVER SPECIFIC] Target is not on same team");
            return false;
        }

        // 6️⃣ Check if target is dying
        if (!IsCharacterDyingOffset(targetCharacter))
        {
            Logger::LogInfo("[RECOVER SPECIFIC] Target is not dying, skipping recovery");
            return false;
        }

        // 7️⃣ Call recovery on target
        Logger::LogInfo("[RECOVER SPECIFIC] Recovering specific team member at index " + std::to_string(teamMemberIndex));
        playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[RECOVER SPECIFIC] Exception: " + std::string(e.what()));
        return false;
    }
}

/**
 * Recover ALL characters on map with full health restoration
 * Uses RecoverDyingAlly_ToServer on all characters regardless of team
 */
bool InGameHack_RecoverDyingAllESP()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        // Get player controller and character
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
            return false;

        int recoveredCount = 0;
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount) || actorCount == 0)
            return false;

        // Recover ALL characters on the map
        for (int i = 0; i < actorCount; i++)
        {
            try
            {
                SDK::AActor* actor = GetWorldActorSafe(world, i);
                std::string className = "Unknown";
                
                // 1️⃣ Vérifie que l'acteur existe et n'est pas un default object
                if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor))
                    continue;

                // 2️⃣ Vérifie le classname (CharacterBattle ou Chxxx)
                if (!GetClassNameSafe(actor, className))
                    continue;

                bool isValidCharacter = (className == "CharacterBattle" || className == "ACharacterBattle");
                
                // Check for Chxxx pattern (Ch001-Ch999)
                bool isChxxx = false;
                if (className.length() == 5 && className[0] == 'C' && className[1] == 'h' &&
                    std::isdigit(className[2]) && std::isdigit(className[3]) && std::isdigit(className[4]))
                {
                    isChxxx = true;
                }

                if (!isValidCharacter && !isChxxx)
                    continue;

                // 3️⃣ Cast à ACharacterBattle
                SDK::ACharacterBattle* targetCharacter = static_cast<SDK::ACharacterBattle*>(actor);
                
                // 4️⃣ Récupère et vérifie le PlayerState
                if (!IsValidPointer(targetCharacter) || !GetCharacterPlayerStateSafe(targetCharacter))
                    continue;

                // 5️⃣ Vérifie que le target est dying (using offset)
                if (!IsCharacterDyingOffset(targetCharacter))
                    continue;  // Not dying, skip

                // 6️⃣ Appelle la recovery
                playerCharacter->RecoverDyingAlly_ToServer(targetCharacter, false);
                recoveredCount++;
            }
            catch (...)
            {
                continue;
            }
        }

        return recoveredCount > 0;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================
// CHARACTER CONDITION HACKS (COMPRESSION_REGENERATION, CH024_TRANSPARENT, CH011_ABYSS_DARK_BODY)
// ============================================

/**
 * Apply COMPRESSION_REGENERATION condition to player character
 * Regenerates and applies compression state (ECharacterConditionId = 62)
 */
bool InGameHack_CompressionRegeneration()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CompressionRegeneration");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with COMPRESSION_REGENERATION (enum value 62)
        conditionComponent->BP_SetCondition(
            (SDK::ECharacterConditionId)75,  // COMPRESSION_REGENERATION = 62
            0,                               // Level
            500.0f,                           // span
            1.0f,                            // value
            0.1f,                          // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0                            // bTimeOverwrite
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CompressionRegeneration");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CompressionRegeneration");
        return false;
    }
}

/**
 * Apply CH024_TRANSPARENT condition to player character
 * Enables CH024 transparency effect (ECharacterConditionId = 65)
 */
bool InGameHack_CH024Transparent()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CH024Transparent");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with CH024_TRANSPARENT (enum value 65)
        conditionComponent->BP_SetCondition(
            (SDK::ECharacterConditionId)76,  // CH024_TRANSPARENT = 65
            0,                               // Level
            500.0f,                           // span
            5.0f,                            // value
            0.01f,                            // interval
            0,                               // subLevel
            nullptr,                         // instigatedPlayer
            0                           // bTimeOverwrite
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH024Transparent");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH024Transparent");
        return false;
    }
}

/**
 * Apply CH011_ABYSS_DARK_BODY condition to player character
 * Applies abyss dark body state (ECharacterConditionId = 95)
 */
bool InGameHack_CH011AbyssDarkBody()
{
    try
    {
        // Get player controller
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[COMBAT] Could not get PlayerController for CH011AbyssDarkBody");
            return false;
        }

        // Cast to APlayerControllerBattle
        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[COMBAT] Could not cast to APlayerControllerBattle");
            return false;
        }

        // Get the player's current character with validation
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[COMBAT] Could not get player character");
            return false;
        }

        // Get the condition control component
        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
        {
            return false;
        }

        // Call SetCondition_ToServer with CH011_ABYSS_DARK_BODY (enum value 95)
        conditionComponent->SetCondition_ToServer(
            (SDK::ECharacterConditionId)94,  // CH011_ABYSS_DARK_BODY = 95
            5,                               // Level
            50.0f,                           // span
            50,                              // value
            0.1f,                            // interval
            5,                               // subLevel
            nullptr,                         // instigatedPlayer
            0,                               // damageActionSerialNo
            false,                           // bTimeOverwrite
            nullptr                          // rpcInstigator
        );

        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[COMBAT] Exception in CH011AbyssDarkBody");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[COMBAT] Unknown exception in CH011AbyssDarkBody");
        return false;
    }
}

static bool IsAbilityConditionId(SDK::ECharacterConditionId id);
static SDK::APlayerStateBattle* GetLocalInstigatedPlayerState(
    SDK::APlayerControllerBattle* playerController,
    SDK::ACharacterBattle* playerCharacter);

bool InGameHack_ApplyCustomCharacterCondition(int conditionId, int applyMode, int level, float duration, float value, float interval, int subLevel, bool timeOverwrite)
{
    if (conditionId <= 0 || conditionId >= static_cast<int>(SDK::ECharacterConditionId::MAX))
    {
        Logger::LogError("[CharacterCondition] Invalid condition ID: " + std::to_string(conditionId));
        return false;
    }

    try
    {
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[CharacterCondition] Could not get valid PlayerController");
            return false;
        }

        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[CharacterCondition] Could not cast to APlayerControllerBattle");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[CharacterCondition] Could not get player character");
            return false;
        }

        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
            return false;

        SDK::ECharacterConditionId id = static_cast<SDK::ECharacterConditionId>(conditionId);
        SDK::APlayerStateBattle* instigatedPlayer = nullptr;
        if (IsAbilityConditionId(id))
        {
            if (!IsValidBattleMode())
            {
                Logger::LogError("[CharacterCondition] Ability condition blocked outside valid battle mode");
                return false;
            }

            instigatedPlayer = GetLocalInstigatedPlayerState(playerController, playerCharacter);
            if (!instigatedPlayer)
            {
                Logger::LogError("[CharacterCondition] Could not get local PlayerState for ability condition");
                return false;
            }

            applyMode = 1;
            value = value > 0.0f ? value : 1000.0f;
            interval = interval > 0.0f ? interval : 0.1f;
            subLevel = subLevel > 0 ? subLevel : level;
        }

        switch (applyMode)
        {
        case 1:
            conditionComponent->BP_SetCondition(id, level, duration, value, interval, subLevel, instigatedPlayer, 0);
            break;
        case 2:
            conditionComponent->BP_SetConditionLocal(id, level, duration, value, interval, subLevel, instigatedPlayer, 0);
            break;
        case 0:
        default:
            conditionComponent->SetCondition_ToServer(id, level, duration, value, interval, subLevel, instigatedPlayer, 0, timeOverwrite, nullptr);
            break;
        }

        Logger::LogInfo("[CharacterCondition] Applied condition ID " + std::to_string(conditionId));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CharacterCondition] Exception applying condition: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[CharacterCondition] Unknown exception applying condition");
        return false;
    }
}

bool InGameHack_ClearCustomCharacterCondition(int conditionId)
{
    if (conditionId <= 0 || conditionId >= static_cast<int>(SDK::ECharacterConditionId::MAX))
    {
        Logger::LogError("[CharacterCondition] Invalid clear condition ID: " + std::to_string(conditionId));
        return false;
    }

    try
    {
        SDK::APlayerController* baseController = SDK_GetPlayerController();
        if (!IsValidPointer(baseController))
        {
            Logger::LogError("[CharacterCondition] Could not get valid PlayerController for clear");
            return false;
        }

        SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[CharacterCondition] Could not cast to APlayerControllerBattle for clear");
            return false;
        }

        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            Logger::LogError("[CharacterCondition] Could not get player character for clear");
            return false;
        }

        auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
        if (!conditionComponent)
            return false;

        conditionComponent->ClearCondition_ToServer(
            static_cast<SDK::ECharacterConditionId>(conditionId),
            SDK::ECharacterConditionEndType::TIME_UP
        );

        Logger::LogInfo("[CharacterCondition] Cleared condition ID " + std::to_string(conditionId) + " with TIME_UP");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CharacterCondition] Exception clearing condition: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[CharacterCondition] Unknown exception clearing condition");
        return false;
    }
}

// ============================================
// ABILITY HACKS
// ============================================

static bool IsAbilityConditionId(SDK::ECharacterConditionId id)
{
    return id >= SDK::ECharacterConditionId::ABILITY_ATTACK &&
        id <= SDK::ECharacterConditionId::ABILITY_TECHNIQUE;
}

static SDK::APlayerStateBattle* GetLocalInstigatedPlayerState(
    SDK::APlayerControllerBattle* playerController,
    SDK::ACharacterBattle* playerCharacter)
{
    SDK::APlayerStateBattle* instigatedPlayer = GetPlayerStateBattle(playerController);
    if (!IsValidPointer(instigatedPlayer))
        instigatedPlayer = GetPlayerStateBattleFromCharacterSafe(playerCharacter);

    return IsValidPointer(instigatedPlayer) ? instigatedPlayer : nullptr;
}

static bool CallBPSetConditionSafe(
    SDK::UCharacterConditionControlComponent* conditionComponent,
    SDK::ECharacterConditionId id,
    int level,
    float span,
    float value,
    float interval,
    int subLevel,
    SDK::APlayerStateBattle* instigatedPlayer,
    int damageActionSerialNo)
{
    __try
    {
        conditionComponent->BP_SetCondition(
            id,
            level,
            span,
            value,
            interval,
            subLevel,
            instigatedPlayer,
            damageActionSerialNo
        );
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool ApplyAbilityCondition(SDK::ECharacterConditionId id, int level, const char* logName)
{
    if (!IsAbilityConditionId(id))
    {
        Logger::LogError(std::string("[COMBAT] Invalid ability condition for ") + logName);
        return false;
    }

    if (!IsValidBattleMode())
    {
        Logger::LogError(std::string("[COMBAT] Not in valid battle mode for ") + logName);
        return false;
    }

    level = std::clamp(level, 1, 100);

    SDK::APlayerController* baseController = SDK_GetPlayerController();
    if (!IsValidPointer(baseController))
    {
        Logger::LogError(std::string("[COMBAT] Could not get valid PlayerController for ") + logName);
        return false;
    }

    SDK::APlayerControllerBattle* playerController = static_cast<SDK::APlayerControllerBattle*>(baseController);
    if (!IsValidPointer(playerController))
    {
        Logger::LogError(std::string("[COMBAT] Could not cast PlayerController for ") + logName);
        return false;
    }

    SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
    if (!IsValidPointer(playerCharacter) || IsObjectDefaultSafe(playerCharacter))
    {
        Logger::LogError(std::string("[COMBAT] Could not get valid player character for ") + logName);
        return false;
    }

    auto* conditionComponent = GetConditionControlComponentSafe(playerCharacter);
    if (!conditionComponent)
    {
        Logger::LogError(std::string("[COMBAT] Could not get condition component for ") + logName);
        return false;
    }

    SDK::APlayerStateBattle* instigatedPlayer = GetLocalInstigatedPlayerState(playerController, playerCharacter);
    if (!instigatedPlayer)
    {
        Logger::LogError(std::string("[COMBAT] Could not get local PlayerState for ") + logName);
        return false;
    }

    if (!CallBPSetConditionSafe(conditionComponent, id, level, 500.0f, 1000.0f, 0.1f, level, instigatedPlayer, 0))
    {
        Logger::LogError(std::string("[COMBAT] SEH exception calling BP_SetCondition for ") + logName);
        return false;
    }

    Logger::LogInfo(std::string("[COMBAT] ") + logName + " applied with level " + std::to_string(level));
    return true;
}

bool InGameHack_AbilityAttack(int level)
{
    return ApplyAbilityCondition(SDK::ECharacterConditionId::ABILITY_ATTACK, level, "AbilityAttack");
}

bool InGameHack_AbilityDurable(int level)
{
    return ApplyAbilityCondition(SDK::ECharacterConditionId::ABILITY_DURABLE, level, "AbilityDurable");
}

bool InGameHack_AbilityMovespeed(int level)
{
    return ApplyAbilityCondition(SDK::ECharacterConditionId::ABILITY_MOVESPEED, level, "AbilityMovespeed");
}

bool InGameHack_AbilityHeal(int level)
{
    return ApplyAbilityCondition(SDK::ECharacterConditionId::ABILITY_HEAL, level, "AbilityHeal");
}

bool InGameHack_AbilityTechnique(int level)
{
    return ApplyAbilityCondition(SDK::ECharacterConditionId::ABILITY_TECHNIQUE, level, "AbilityTechnique");
}

// ============================================
// CHARACTER CONTROL FUNCTIONS
// ============================================

std::vector<SDK::ACharacterBattle*> InGameHack_GetAllCharacterBattles()
{
    std::vector<SDK::ACharacterBattle*> characters;
    std::set<SDK::ACharacterBattle*> uniqueCharacters;
    
    try
    {
        // Try world iteration first
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (GetWorldActorsCountSafe(world, actorCount))
        {
            for (int i = 0; i < actorCount; i++)
            {
                SDK::ACharacterBattle* character = ActorToCharacterBattleSafe(GetWorldActorSafe(world, i));
                if (!character)
                    continue;
                
                // Add if not already in set
                if (uniqueCharacters.find(character) == uniqueCharacters.end())
                {
                    uniqueCharacters.insert(character);
                    characters.push_back(character);
                }
            }
        }
        
        // Always use ESP fallback to ensure we get targets
        
        
        for (int attempt = 0; attempt < 100; attempt++)
        {
            SDK::AActor* targetActorPtr = SDK_GetRandomESPTarget();
            if (!targetActorPtr)
            {
                continue;
            }
            
            SDK::ACharacterBattle* targetCharacter = ActorToCharacterBattleSafe(targetActorPtr);
            if (!targetCharacter)
            {
                continue;
            }
            
            // Add if not already in set
            if (uniqueCharacters.find(targetCharacter) == uniqueCharacters.end())
            {
                uniqueCharacters.insert(targetCharacter);
                characters.push_back(targetCharacter);
            }
        }
        
    }
    catch (const std::exception& e)
    {
    }
    catch (...)
    {
    }
    
    return characters;
}

std::vector<std::string> InGameHack_GetCharacterNames()
{
    std::vector<std::string> names;
    
    try
    {
        auto characters = InGameHack_GetAllCharacterBattles();
        
        for (size_t idx = 0; idx < characters.size(); idx++)
        {
            auto* character = characters[idx];
            if (!IsValidPointer(character))
            {
                names.push_back("Unknown");
                continue;
            }
            
            // Get character class name (ChXXX pattern)
            std::string charClassName = "Unknown";
            GetClassNameSafe(character, charClassName);
            
            // Get player name from PlayerState
            std::string playerName = GetPlayerNameSafe(GetCharacterPlayerStateSafe(character));
            
            // Format: "PlayerName (ChXXX)"
            std::string displayName = playerName + " (" + charClassName + ")";
            names.push_back(displayName);
        }
    }
    catch (const std::exception& e)
    {
    }
    catch (...)
    {
    }
    
    return names;
}

std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharacterBattles()
{
    std::vector<SDK::ACharacterBattle*> teamCharacters;
    try
    {
        if (!IsValidBattleMode())
            return teamCharacters;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return teamCharacters;

        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter || !GetCharacterPlayerStateSafe(playerCharacter))
            return teamCharacters;

        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return teamCharacters;

        auto characters = InGameHack_GetAllCharacterBattles();
        for (auto* character : characters)
        {
            if (!IsValidPointer(character) || character == playerCharacter || IsObjectDefaultSafe(character))
                continue;

            if (!GetCharacterPlayerStateSafe(character))
                continue;

            unsigned char targetTeamId = GetCharacterTeamId(character);
            if (targetTeamId != playerTeamId)
                continue;

            teamCharacters.push_back(character);
        }
    }
    catch (...)
    {
        // ignore
    }

    return teamCharacters;
}

std::vector<std::string> InGameHack_GetTeamCharacterNames()
{
    std::vector<std::string> names;
    try
    {
        auto teamCharacters = InGameHack_GetTeamCharacterBattles();
        for (auto* character : teamCharacters)
        {
            if (!IsValidPointer(character))
            {
                names.push_back("Unknown");
                continue;
            }

            std::string playerName = GetPlayerNameSafe(GetCharacterPlayerStateSafe(character));

            std::string className = "Unknown";
            GetClassNameSafe(character, className);

            names.push_back(playerName + " (" + className + ")");
        }
    }
    catch (...)
    {
        // ignore
    }
    return names;
}

bool InGameHack_RecoverDyingTeamMember(SDK::ACharacterBattle* target)
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        if (!IsValidPointer(target) || IsObjectDefaultSafe(target) || !GetCharacterPlayerStateSafe(target))
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter || !GetCharacterPlayerStateSafe(playerCharacter))
            return false;

        unsigned char playerTeamId = GetCharacterTeamId(playerCharacter);
        if (playerTeamId == 255)
            return false;

        unsigned char targetTeamId = GetCharacterTeamId(target);
        if (targetTeamId != playerTeamId)
            return false;

        if (!IsCharacterDyingOffset(target))
            return false;

        playerCharacter->RecoverDyingAlly_ToServer(target, false);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void InGameHack_LogAllDamageAttenuationCurves()
{
    return;

    try
    {
        // Créer le dossier c:\temp s'il n'existe pas
        std::system("mkdir c:\\temp 2>nul");
        
        // Ouvrir le fichier log
        std::ofstream logFile("c:\\temp\\damage_attenuation_curves.log", std::ios::out | std::ios::trunc);
        if (!logFile.is_open())
        {
            Logger::LogError("[CURVE] Failed to open log file: c:\\temp\\damage_attenuation_curves.log");
            return;
        }

        Logger::LogInfo("[CURVE] ============ Logging All DamageAttenuation Curves ============");
        logFile << "============ DamageAttenuation Curves Log ============\n";
        logFile << "Timestamp: " << std::time(nullptr) << "\n";
        logFile << "================================================\n\n";
        
        SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
        if (!SafeMemory::IsReadable(gObjects, sizeof(SDK::TUObjectArray)))
        {
            Logger::LogWarning("[CURVE] GObjects is null, cannot scan");
            logFile << "ERROR: GObjects is null, cannot scan\n";
            logFile.close();
            return;
        }

        int32_t foundCount = 0;
        int32_t totalKeys = 0;
        int32_t objectCount = 0;
        if (!TryGetObjectArrayCountSafe(gObjects, objectCount))
        {
            Logger::LogWarning("[CURVE] GObjects count is unreadable, cannot scan");
            logFile << "ERROR: GObjects count is unreadable, cannot scan\n";
            logFile.close();
            return;
        }

        if (objectCount <= 0 || objectCount > 2000000)
        {
            Logger::LogWarning("[CURVE] GObjects count is invalid: " + std::to_string(objectCount));
            logFile << "ERROR: GObjects count is invalid: " << objectCount << "\n";
            logFile.close();
            return;
        }

        // Scan all objects for UANS_Attack instances
        for (int32_t i = 0; i < objectCount; ++i)
        {
            SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);

            if (!IsValidPointer(obj))
                continue;

            // Check class name to filter UANS_Attack objects
            std::string className = "Unknown";
            if (!GetClassNameSafe(obj, className))
                continue;
            if (className != "ANS_Attack")
                continue;

            // Cast to UANS_Attack
            auto* ansAttack = static_cast<SDK::UANS_Attack*>(obj);
            if (!IsValidPointer(ansAttack))
                continue;

            SDK::UCurveFloat* damageAttenuation = nullptr;
            if (!SafeReadMember(&ansAttack->_damageAttenuation, damageAttenuation) ||
                !IsValidPointer(damageAttenuation))
                continue;

            foundCount++;

            // Get object name
            std::string objName = "Unknown";
            try
            {
                objName = ansAttack->GetName();
            }
            catch (...)
            {
                continue;
            }
            
            // Log the curve pointer and class
            std::string logMsg = "[CURVE] #" + std::to_string(foundCount) + " - Name: " + objName +
                          " | Curve Ptr: 0x" + std::to_string((uint64_t)damageAttenuation);
            Logger::LogInfo(logMsg);
            logFile << logMsg << "\n";

            // Log curve details
            if (damageAttenuation)
            {
                SDK::FRichCurve* curvePtr = &(damageAttenuation->FloatCurve);
                if (!SafeMemory::IsReadable(curvePtr, sizeof(SDK::FRichCurve)))
                    continue;

                int32_t numKeys = 0;
                if (!SafeArrayCount(curvePtr->Keys, numKeys, 4096))
                    continue;
                
                std::string keysMsg = "[CURVE]   -> Keys: " + std::to_string(numKeys);
                Logger::LogInfo(keysMsg);
                logFile << keysMsg << "\n";
                totalKeys += numKeys;

                // Log each key
                if (numKeys > 0)
                {
                    for (int32_t keyIdx = 0; keyIdx < numKeys; ++keyIdx)
                    {
                        SDK::FRichCurveKey key{};
                        if (!SafeArrayGet(curvePtr->Keys, keyIdx, key, 4096))
                            continue;
                        
                        std::string keyMsg = "[CURVE]      Key[" + std::to_string(keyIdx) + "]: Time=" + 
                                      std::to_string(key.Time) + ", Value=" + std::to_string(key.value);
                        Logger::LogInfo(keyMsg);
                        logFile << keyMsg << "\n";
                    }
                }
            }
        }

        std::string summary = "[CURVE] ============ Summary ============\n" +
                             std::string("[CURVE] Total curves found: ") + std::to_string(foundCount) + "\n" +
                             std::string("[CURVE] Total keys logged: ") + std::to_string(totalKeys) + "\n" +
                             std::string("[CURVE] ====================================");
        Logger::LogInfo(summary);
        logFile << "\n================================================\n";
        logFile << "Total curves found: " << foundCount << "\n";
        logFile << "Total keys logged: " << totalKeys << "\n";
        logFile << "================================================\n";
        
        logFile.close();
        Logger::LogInfo("[CURVE] Log file saved to c:\\temp\\damage_attenuation_curves.log");
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CURVE] Exception in LogAllDamageAttenuationCurves: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[CURVE] Unknown exception in LogAllDamageAttenuationCurves");
    }
}

bool InGameHack_SetCharacterDying(SDK::ACharacterBattle* character)
{
    try
    {
        if (!IsValidPointer(character) || !GetCharacterPlayerStateSafe(character))
        {
            return false;
        }
        
        SDK::APlayerStateBattle* victimState = static_cast<SDK::APlayerStateBattle*>(GetCharacterPlayerStateSafe(character));
        if (!victimState)
        {
            return false;
        }
        
        // Get player character as aggriever
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            return false;
        }
        
        // Get game state
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        SDK::AGameStateBattle* gameState = static_cast<SDK::AGameStateBattle*>(GetGameStateSafe(world));
        if (!gameState)
        {
            return false;
        }
        
        // Call OnCharacterDying_NetMulti on game state with victim and aggressor
        gameState->OnCharacterDying_NetMulti(victimState, playerCharacter);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_KillCharacter(SDK::ACharacterBattle* victim, SDK::ACharacterBattle* killer)
{
    try
    {
        if (!IsValidPointer(victim) || !GetCharacterPlayerStateSafe(victim))
        {
            return false;
        }
        
        SDK::APlayerStateBattle* victimState = static_cast<SDK::APlayerStateBattle*>(GetCharacterPlayerStateSafe(victim));
        if (!victimState)
        {
            return false;
        }
        
        // If killer not specified, use player character
        SDK::ACharacterBattle* killerCharacter = killer;
        if (!IsValidPointer(killerCharacter))
        {
            SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
            if (!IsValidPointer(playerController))
            {
                return false;
            }
            killerCharacter = GetPlayerCharacterBattle(playerController);
        }
        if (!IsValidPointer(killerCharacter))
        {
            return false;
        }
        
        // Get game state
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        SDK::AGameStateBattle* gameState = static_cast<SDK::AGameStateBattle*>(GetGameStateSafe(world));
        if (!gameState)
        {
            return false;
        }
        
        // Call OnCharacterDead_NetMulti on game state with victim state and killer character
        gameState->OnCharacterDead_NetMulti(victimState, killerCharacter);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_ValidateTransMissionLevel(int level)
{
    try
    {
        if (level < 0 || level > 9)
        {
            return false;
        }

        SDK::int32 validatedLevel = SDK::UCh202ActionAttackBase::ValidateTransMissionLevel(level);
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool InGameHack_SetInitTransMissionLevel(int levelIndex, int32_t newValue)
{
    try
    {
        if (levelIndex < 0)
        {
            Logger::LogError("[CH202] Invalid init trans mission index: " + std::to_string(levelIndex));
            return false;
        }

        SDK::UCh202Params* ch202Params = SDK::UCh202Params::GetDefaultObj();
        if (!IsValidPointer(ch202Params))
        {
            Logger::LogError("[CH202] Could not get UCh202Params default object");
            return false;
        }

        int32_t arraySize = 0;
        if (!SafeArrayCount(ch202Params->unique3_initLevel, arraySize, 64) ||
            levelIndex >= arraySize)
        {
            Logger::LogError("[CH202] Init trans mission index out of range: " + std::to_string(levelIndex) + ", size=" + std::to_string(arraySize));
            return false;
        }

        UC::int32 safeNewValue = static_cast<UC::int32>(newValue);
        auto* levelData = const_cast<UC::int32*>(ch202Params->unique3_initLevel.GetDataPtr());
        if (!SafeMemory::TryWrite<UC::int32>(levelData + levelIndex, safeNewValue))
        {
            Logger::LogError("[CH202] Failed to write unique3_initLevel at index " + std::to_string(levelIndex));
            return false;
        }

        Logger::LogInfo("[CH202] Set unique3_initLevel[" + std::to_string(levelIndex) + "] = " + std::to_string(newValue));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[CH202] Exception in SetInitTransMissionLevel");
        return false;
    }
    catch (...)
    {
        Logger::LogError("[CH202] Unknown exception in SetInitTransMissionLevel");
        return false;
    }
}

// Get current max stack count from selected supply widget
// Cached max stack value updated each frame
static int32_t g_CachedMaxStackValue = 100;

int32_t InGameHack_GetSupplyMaxStackCount()
{
    try
    {
        // Return the cached value that was updated from the widget
        // This value is updated in ModifySupplyMaxStack() each frame
        return g_CachedMaxStackValue;
    }
    catch (...)
    {
        return 0;
    }
}

// Helper function to search for UItemWidget and read maxStack value
int32_t GetMaxStackFromWidget()
{
    try
    {
        // UItemWidget has cached maxStack at offset 0x08A0
        // However, accessing UI widgets from gameplay layer is complex
        // as widgets exist in viewport/UI hierarchy
        
        // For now, use the actual game default value
        // When enabled in menu, the slider will override this with user value
        return 100;  // Default maxStack value in RUGIR
    }
    catch (...)
    {
        return 100;
    }
}

bool InGameHack_ModifySupplyMaxStack()
{
    try
    {
        // This function is called each frame to update supply max stack
        // Update the cached value from the actual widget
        g_CachedMaxStackValue = GetMaxStackFromWidget();
        
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[SUPPLY] Exception in ModifySupplyMaxStack: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[SUPPLY] Unknown exception in ModifySupplyMaxStack");
        return false;
    }
}

bool InGameHack_SetSkillLevel(int skillIndex, int level)
{
    try
    {
        if (skillIndex < 0 || skillIndex > 8 || level < 1 || level > 9)
        {
            return false;
        }
        
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return false;
        }
        
        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!playerChar)
        {
            return false;
        }
        
        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        if (!playerState)
        {
            return false;
        }
        
        auto supplyHolderComp = GetSupplyHolderComponentSafe(playerState);
        if (!IsValidPointer(supplyHolderComp))
        {
            return false;
        }
        
        // Call SetSkillLevel_ToServer with attack ID and level
        // EAttackId enum - skill index 0-8 maps to attack level
        supplyHolderComp->SetSkillLevel_ToServer((SDK::EAttackId)skillIndex, level);
        
        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool InGameHack_TestUseItemAction(int uniqueLevel)
{
    try
    {
        if (!IsValidBattleMode())
        {
            Logger::LogWarning("[UseItemAction] Not in valid battle mode");
            return false;
        }

        if (uniqueLevel < 0)
            uniqueLevel = 0;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[UseItemAction] Could not get PlayerController");
            return false;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsValidPointer(playerChar))
        {
            Logger::LogError("[UseItemAction] Could not get player character");
            return false;
        }

        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        const SDK::FName supplyId = GetUseItemSupplyIdSafe(playerState);

        SDK::UCharacterActionControlComponent* actionControl = GetActionControlComponentSafe(playerChar);
        if (!IsValidPointer(actionControl))
        {
            Logger::LogError("[UseItemAction] Could not get CharacterActionControlComponent");
            return false;
        }

        SDK::UActionAttackBase* aimingAction = nullptr;
        if (SafeReadMember(&actionControl->_aimingAttackActionFunc, aimingAction) && IsValidPointer(aimingAction) &&
            IsObjectAUnsafeGuarded(aimingAction, SDK::UActionAttackUseItem::StaticClass()))
        {
            auto* useItemAction = static_cast<SDK::UActionAttackUseItem*>(aimingAction);
            if (TrySetUseItemSupplyIdSafe(useItemAction, supplyId))
            {
                Logger::LogInfo("[UseItemAction] BP_SetSupplyId called on current UseItem action instance");
            }
            else if (IsNonNoneFName(supplyId))
            {
                Logger::LogWarning("[UseItemAction] BP_SetSupplyId failed on current UseItem action instance");
            }
            else
            {
                Logger::LogInfo("[UseItemAction] Current UseItem action instance found, but no shortcut/last supply id is available");
            }
        }
        else
        {
            Logger::LogInfo("[UseItemAction] No current UActionAttackUseItem instance found before RPC; sending server action only");
        }

        SDK::FVector_NetQuantize100 targetLocation = GetActorLocationNetQuantize100Safe(playerChar);

        actionControl->SetAttackAction_ToServer(
            SDK::EAttackId::USEITEM,
            targetLocation,
            nullptr,
            uniqueLevel,
            SDK::ECommandId::USEITEM,
            true,
            0,
            SDK::EAttackBeginStateFlags::NONE);

        Logger::LogInfo("[UseItemAction] SetAttackAction_ToServer(USEITEM) sent");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[UseItemAction] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[UseItemAction] Unknown exception");
        return false;
    }
}

int InGameHack_GetSkillLevel(int skillIndex)
{
    try
    {
        if (!IsValidBattleMode())
        {
            return -1;
        }

        if (skillIndex < 0 || skillIndex > 8)
        {
            return -1;
        }

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            return -1;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!playerChar)
        {
            return -1;
        }

        // Call BP_GetUniqueLevel to get current skill level
        return playerChar->BP_GetUniqueLevel((SDK::EAttackId)skillIndex);
    }
    catch (const std::exception& e)
    {
        return -1;
    }
}

void InGameHack_ValidateTransMissionLevel()
{
    try
    {
        SDK::int32 validatedLevel = SDK::UCh202ActionAttackBase::ValidateTransMissionLevel(5);
      
    }
    catch (const std::exception& e)
    {
        // Error occurred
    }
}

// ============================================
// GET ALL TEAM IDS
// ============================================

std::vector<unsigned char> InGameHack_GetAllTeamIds()
{
    std::vector<unsigned char> teamIds;
    
    try
    {
        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
        {
            return teamIds;  // Empty
        }

        // Iterate through all actors
        for (int i = 0; i < actorCount; i++)
        {
            SDK::ACharacterBattle* character = ActorToCharacterBattleSafe(GetWorldActorSafe(world, i));
            if (!character)
                continue;
            
            // Get team ID
            try
            {
                unsigned char teamId = character->BP_GetTeamId();
                
                // Add to vector if not already present
                bool found = false;
                for (unsigned char id : teamIds)
                {
                    if (id == teamId)
                    {
                        found = true;
                        break;
                    }
                }
                
                if (!found && teamId != 255)  // 255 = invalid
                {
                    teamIds.push_back(teamId);
                }
            }
            catch (...)
            {
                continue;
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[GetAllTeamIds] Exception: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[GetAllTeamIds] Unknown exception");
    }

    return teamIds;
}

/**
 * Get all characters in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of ACharacterBattle pointers for that team
 */
std::vector<SDK::ACharacterBattle*> InGameHack_GetTeamCharactersByTeamId(unsigned char teamId)
{
    std::vector<SDK::ACharacterBattle*> teamCharacters;
    try
    {
        if (!IsValidBattleMode())
            return teamCharacters;

        auto allCharacters = InGameHack_GetAllCharacterBattles();
        for (auto* character : allCharacters)
        {
            if (!IsValidPointer(character) || IsObjectDefaultSafe(character))
                continue;

            if (!GetCharacterPlayerStateSafe(character))
                continue;

            unsigned char characterTeamId = GetCharacterTeamId(character);
            if (characterTeamId == teamId)
            {
                teamCharacters.push_back(character);
            }
        }
    }
    catch (...)
    {
        Logger::LogError("[GetTeamCharactersByTeamId] Exception occurred");
    }

    return teamCharacters;
}

/**
 * Get all player names in a specific team by team ID
 * @param teamId - The team ID to search for
 * @return Vector of player names for that team
 */
std::vector<std::string> InGameHack_GetTeamNamesByTeamId(unsigned char teamId)
{
    std::vector<std::string> names;
    try
    {
        auto teamCharacters = InGameHack_GetTeamCharactersByTeamId(teamId);
        for (auto* character : teamCharacters)
        {
            if (!IsValidPointer(character))
            {
                names.push_back("Unknown");
                continue;
            }

            std::string playerName = GetPlayerNameSafe(GetCharacterPlayerStateSafe(character));

            names.push_back(playerName);
        }
    }
    catch (...)
    {
        Logger::LogError("[GetTeamNamesByTeamId] Exception occurred");
    }

    return names;
}

/**
 * Get my current team ID
 * @return Team ID of the local player, or -1 if not found
 */
int InGameHack_GetMyTeamId()
{
    try
    {
        SDK::APlayerController* basePC = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(basePC))
            return -1;

        SDK::AHerovsPlayerState* myPlayerState = static_cast<SDK::AHerovsPlayerState*>(GetControllerPlayerStateSafe(basePC));
        if (!myPlayerState)
            return -1;

        return myPlayerState->BP_GetTeamId();
    }
    catch (...)
    {
        Logger::LogError("[GetMyTeamId] Exception occurred");
        return -1;
    }
}

// ============================================
// BULLET REDIRECTION FUNCTIONS
// ============================================

/**
 * Helper: Check if bullet is a melee attack (should NOT be redirected)
 */
static inline bool IsBulletMeleeAttack(const std::string& bulletName)
{
    // Check for melee/punch/slash patterns
    if (bulletName.find("Melee") != std::string::npos) return true;
    if (bulletName.find("Punch") != std::string::npos) return true;
    if (bulletName.find("Slash") != std::string::npos) return true;
    if (bulletName.find("Strike") != std::string::npos) return true;
    if (bulletName.find("Attack") != std::string::npos && bulletName.find("Skill") == std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Alpha (Unique1) skill
 */
static inline bool IsBulletAlphaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique1") != std::string::npos) return true;
    if (bulletName.find("BPB_CementShot") != std::string::npos) return true;
    if (bulletName.find("BPB_Cement_L") != std::string::npos) return true;
    if (bulletName.find("BPB_Cracks_L") != std::string::npos) return true;
    if (bulletName.find("BPB_FogShot_") != std::string::npos) return true;
    if (bulletName.find("BPB202_U1_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Beta (Unique2) skill
 */
static inline bool IsBulletBetaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique2") != std::string::npos) return true;
    if (bulletName.find("BPB_ThunderBackGroundHit_C") != std::string::npos) return true;
    if (bulletName.find("BPB_CracksRange_L") != std::string::npos) return true;
    if (bulletName.find("BPB_RiseCement_L") != std::string::npos) return true;
    if (bulletName.find("BPB_PortalA_") != std::string::npos) return true;
    if (bulletName.find("BPB_PortalShotA_") != std::string::npos) return true;
    if (bulletName.find("BPB202_U2_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Gamma (Unique3) skill
 */
static inline bool IsBulletGammaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique3") != std::string::npos) return true;
    if (bulletName.find("Cement3Shot") != std::string::npos) return true;
    if (bulletName.find("BPB202_U3_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet name contains Special skill
 */
static inline bool IsBulletSpecialSkill(const std::string& bulletName)
{
    if (bulletName.find("BPB_Special_C") != std::string::npos) return true;
    if (bulletName.find("BPB_UniqueSpecial_") != std::string::npos) return true;
    return false;
}

/**
 * Helper: Check if bullet cannot be teleported (causes crash)
 * Some special balls like rebuild balls crash when teleported
 */
static inline bool IsBulletNonTeleportable(const std::string& bulletName)
{
    // Rebuild skill special ball - DO NOT teleport
    if (bulletName.find("Rebuild") != std::string::npos) return true;
    if (bulletName.find("BPB_Rebuild") != std::string::npos) return true;
    // Add more non-teleportable ball patterns here as needed
    return false;
}

static inline bool IsFiniteVector(const SDK::FVector& value)
{
    return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
}

static inline float VectorLengthSquared(const SDK::FVector& value)
{
    return (value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z);
}

static inline SDK::FVector NormalizeVectorSafe(const SDK::FVector& value)
{
    const float lengthSquared = VectorLengthSquared(value);
    if (lengthSquared <= 0.0001f || !std::isfinite(lengthSquared))
        return SDK::FVector(0.0f, 0.0f, 0.0f);

    const float length = std::sqrt(lengthSquared);
    return SDK::FVector(value.X / length, value.Y / length, value.Z / length);
}

static inline SDK::FRotator RotatorFromDirection(const SDK::FVector& direction)
{
    constexpr float RadToDeg = 57.29577951308232f;
    const float xyDistance = std::sqrt((direction.X * direction.X) + (direction.Y * direction.Y));
    const float pitch = std::atan2(direction.Z, xyDistance) * RadToDeg;
    const float yaw = std::atan2(direction.Y, direction.X) * RadToDeg;

    return SDK::FRotator(pitch, yaw, 0.0f);
}

static inline float GetUsefulBulletSpeed(SDK::ABullet* bullet)
{
    constexpr float MinBulletTPSpeed = 30000.0f;
    constexpr float MaxBulletTPSpeed = 120000.0f;

    float speed = 0.0f;
    try
    {
        const SDK::FVector currentVelocity = bullet->GetVelocity();
        if (IsFiniteVector(currentVelocity))
            speed = std::sqrt(VectorLengthSquared(currentVelocity));
    }
    catch (...)
    {
        speed = 0.0f;
    }

    if (speed < MinBulletTPSpeed)
        speed = MinBulletTPSpeed;
    if (speed > MaxBulletTPSpeed)
        speed = MaxBulletTPSpeed;
    return speed;
}

static inline bool RedirectBulletTowardTarget(SDK::ABullet* bullet, const SDK::FVector& targetPosition)
{
    if (!IsValidPointer(bullet) || !IsFiniteVector(targetPosition))
        return false;

    SDK::FVector bulletPosition = SDK::FVector(0.0f, 0.0f, 0.0f);
    try
    {
        bulletPosition = bullet->K2_GetActorLocation();
    }
    catch (...)
    {
        return false;
    }

    if (!IsFiniteVector(bulletPosition))
        return false;

    SDK::FVector direction = NormalizeVectorSafe(targetPosition - bulletPosition);
    if (direction.IsZero())
        return false;

    const SDK::FRotator targetRotation = RotatorFromDirection(direction);
    const float speed = GetUsefulBulletSpeed(bullet);
    const SDK::FVector redirectedVelocity = direction * speed;

    // Start just before the target, then sweep through it so the projectile collision can fire.
    const SDK::FVector impactStart = targetPosition - (direction * 120.0f);
    const SDK::FVector impactEnd = targetPosition + (direction * 90.0f);

    bool movedNearTarget = false;
    bool sweptThroughTarget = false;

    try
    {
        if (IsObjectAUnsafeGuarded(bullet, SDK::ACustomBullet::StaticClass()))
        {
            SDK::ACustomBullet* customBullet = static_cast<SDK::ACustomBullet*>(bullet);
            customBullet->SetRotationFollowsVelocity(true);
        }
    }
    catch (...)
    {
    }

    try
    {
        bullet->BP_SetVelocity(redirectedVelocity);
    }
    catch (...)
    {
    }

    try
    {
        bullet->K2_SetActorRotation(targetRotation, true);
    }
    catch (...)
    {
    }

    try
    {
        movedNearTarget = bullet->K2_SetActorLocationAndRotation(impactStart, targetRotation, false, nullptr, true);
        bullet->BP_SetVelocity(redirectedVelocity);
        sweptThroughTarget = bullet->K2_SetActorLocationAndRotation(impactEnd, targetRotation, true, nullptr, false);
        bullet->BP_SetVelocity(redirectedVelocity);
    }
    catch (...)
    {
        try
        {
            bullet->K2_TeleportTo(targetPosition, targetRotation);
            bullet->BP_SetVelocity(redirectedVelocity);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    return movedNearTarget || sweptThroughTarget;
}

/**
 * Redirect bullets to nearest enemy in front
 * Filters bullets by skill type (Alpha/Beta/Gamma/Special)
 * Repositions bullets onto a short swept path through the target, then keeps
 * their velocity aligned so hit validation is not tied to the original crosshair.
 * 
 * @param bIncludeAlpha - Include Alpha (Unique1) skills
 * @param bIncludeBeta - Include Beta (Unique2) skills
 * @param bIncludeGamma - Include Gamma (Unique3) skills
 * @param bIncludeSpecial - Include Special skills
 * @return true if bullets were redirected
 */
bool InGameHack_RedirectBulletsToNearestEnemy(bool bIncludeAlpha, bool bIncludeBeta, bool bIncludeGamma, bool bIncludeSpecial)
{
    try
    {
        // Validate we're in battle
        if (!IsValidBattleMode())
        {
            return false;
        }

        // Get world
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
            return false;

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        // Get player character
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
            return false;

        // Get target strictly from the BulletTP FOV circle and use the same
        // cached 3D aim point as the aimbot, not the actor root location.
        SDK::FVector targetPosition = SDK::FVector(0, 0, 0);
        SDK::AActor* nearestEnemyPtr = SDK_GetBulletTPFOVTargetWithPosition(&targetPosition);
        if (!nearestEnemyPtr)
            return false;

        SDK::ACharacterBattle* nearestEnemy = ActorToCharacterBattleSafe(nearestEnemyPtr);
        if (!nearestEnemy)
            return false;
        (void)nearestEnemy;

        if (targetPosition.X == 0.0f && targetPosition.Y == 0.0f && targetPosition.Z == 0.0f)
            return false;

        int redirectedCount = 0;
        int totalBulletsFound = 0;
        int bulletsFiltered = 0;

        // Iterate through all actors to find and redirect bullets
        for (int i = 0; i < actorCount; i++)
        {
            SDK::AActor* actor = GetWorldActorSafe(world, i);
            
            // Check if actor is a bullet
            if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor) || !IsObjectAUnsafeGuarded(actor, SDK::ABullet::StaticClass()))
                continue;

            SDK::ABullet* bullet = static_cast<SDK::ABullet*>(actor);
            if (!IsValidPointer(bullet))
                continue;

            totalBulletsFound++;

            // Check if bullet belongs to player character
            if (bullet->_ownerChr != playerCharacter)
                continue;

            // Get bullet class name
            std::string bulletClassName = "";
            if (!GetClassNameSafe(bullet, bulletClassName))
                continue;

            // CRITICAL: Skip melee attacks to prevent crash
            if (IsBulletMeleeAttack(bulletClassName))
                continue;

            // CRITICAL: Skip non-teleportable bullets (rebuild, etc) to prevent crash
            if (IsBulletNonTeleportable(bulletClassName))
                continue;

            // Check if bullet matches skill filter
            bool matchesFilter = false;
            if (bIncludeAlpha && IsBulletAlphaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeBeta && IsBulletBetaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeGamma && IsBulletGammaSkill(bulletClassName))
                matchesFilter = true;
            else if (bIncludeSpecial && IsBulletSpecialSkill(bulletClassName))
                matchesFilter = true;

            if (!matchesFilter)
                continue;  // Skip this bullet if it doesn't match filter

            bulletsFiltered++;

            // Redirect bullet trajectory to the cached aim point. A raw teleport
            // only works reliably when the original crosshair path already hits.
            try
            {
                if (RedirectBulletTowardTarget(bullet, targetPosition))
                    redirectedCount++;
            }
            catch (...)
            {
                continue;  // Skip if redirect fails
            }
        }

        // Log results for debugging
        if constexpr (RUNTIME_FILE_DEBUG_LOGGING)
        {
            if (totalBulletsFound > 0 || redirectedCount > 0)
            {
                std::stringstream log;
                log << "[BulletRedirect] Total Bullets: " << totalBulletsFound
                    << " | Filtered: " << bulletsFiltered
                    << " | Redirected: " << redirectedCount
                    << " | Filters(A:" << bIncludeAlpha
                    << " B:" << bIncludeBeta
                    << " G:" << bIncludeGamma
                    << " S:" << bIncludeSpecial << ")\n";
                WriteRuntimeDebugLog("C:\\temp\\bullet_redirect_debug.log", log.str());
            }
        }

        return redirectedCount > 0;
    }
    catch (const std::exception& e)
    {
        if constexpr (RUNTIME_FILE_DEBUG_LOGGING)
        {
            WriteRuntimeDebugLog("C:\\temp\\bullet_redirect_debug.log",
                std::string("[BulletRedirect] Exception: ") + e.what() + "\n");
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}

/**
 * Test: Launch Ch101 RollSlot Unique Skill
 * Uses verified public API: BP_GetObject() + OnBroadcastEvent()
 * @return true if skill was launched successfully
 */
bool InGameHack_LaunchCh101RollSlotSkill()
{
    std::stringstream logBuffer;
    auto flushRollSlotLog = [&]()
    {
        if constexpr (RUNTIME_FILE_DEBUG_LOGGING)
            WriteRuntimeDebugLog("C:\\temp\\rollslot_launch.log", logBuffer.str());
    };
    
    try
    {
        // Log start
        logBuffer << "[CH101_ROLLSLOT] ===== LAUNCH ATTEMPT START =====\n";
        
        // Step 1: Validate battle mode
        logBuffer << "[CH101_ROLLSLOT] Step 1: Validating battle mode...\n";
        if (!IsValidBattleMode())
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Not in valid battle mode\n";
            
            flushRollSlotLog();
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Valid battle mode\n";

        // Step 2: Get player controller
        logBuffer << "[CH101_ROLLSLOT] Step 2: Getting player controller...\n";
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get player controller\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            flushRollSlotLog();
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Player controller obtained\n";

        // Step 3: Get player character
        logBuffer << "[CH101_ROLLSLOT] Step 3: Getting player character...\n";
        SDK::ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get player character from Pawn\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            flushRollSlotLog();
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: Player character obtained at 0x" 
                 << std::hex << (uintptr_t)playerCharacter << std::dec << "\n";

        // Step 4: Get PlayerStateBattle
        logBuffer << "[CH101_ROLLSLOT] Step 4: Getting PlayerStateBattle...\n";
        SDK::APlayerStateBattle* playerState = GetPlayerStateBattleFromCharacterSafe(playerCharacter);
        if (!playerState)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get PlayerStateBattle\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            flushRollSlotLog();
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: PlayerStateBattle obtained at 0x" 
                 << std::hex << (uintptr_t)playerState << std::dec << "\n";

        // Step 5: Get RollSlot Control Component
        logBuffer << "[CH101_ROLLSLOT] Step 5: Getting RollSlot Control Component...\n";
        SDK::UCharacterRollSlotUniqueSkillControlComponent* rollSlotCtrl = GetRollSlotControlComponentSafe(playerState);
        if (!rollSlotCtrl)
        {
            logBuffer << "[CH101_ROLLSLOT] ERROR: Could not get RollSlot Control Component\n";
            logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
            
            flushRollSlotLog();
            return false;
        }
        logBuffer << "[CH101_ROLLSLOT] OK: RollSlot Control Component obtained at 0x" 
                 << std::hex << (uintptr_t)rollSlotCtrl << std::dec << "\n";

        // Step 6: Try different approaches to activate RollSlot
        logBuffer << "[CH101_ROLLSLOT] Step 6: Attempting multiple activation methods...\n";
        
        // Offset of _registerRollSlotUniqueSkillObjectList is 0x00D0
        SDK::uint8* rollSlotCtrlPtr = (SDK::uint8*)rollSlotCtrl;
        SDK::uint8* tMapPtr = rollSlotCtrlPtr + 0x00D0;
        
        logBuffer << "[CH101_ROLLSLOT] TMap address: 0x" << std::hex << (uintptr_t)tMapPtr << std::dec << "\n";
        
        // Method 1: Try BP_BroadcastByCharacterId with SetParam first
        logBuffer << "[CH101_ROLLSLOT] Step 7a: Method 1 - SetParam + Broadcast\n";
        SDK::EVariationCharacterId ch101Var0 = SDK::EVariationCharacterId::Ch101_Variation0;
        float refParam = 1.0f;  // Try with 1.0 instead of 0
        SDK::int32 broadcastIndex = 0;
        
        // Set parameters first
        SDK::FRollSlotUniqueSkillArgumentParam skillParam;
        memset(&skillParam, 0, sizeof(skillParam));
        skillParam._broadcastIndex = 0;
        rollSlotCtrl->BP_SetParamByCharacterId(ch101Var0, skillParam);
        
        // Then broadcast
        rollSlotCtrl->BP_BroadcastByCharacterId(ch101Var0, refParam, broadcastIndex);
        logBuffer << "[CH101_ROLLSLOT] BP_BroadcastByCharacterId completed\n";
        
        // Method 2: Try BP_BroadcastByTriggerType with StartBattleWithCooldown
        logBuffer << "[CH101_ROLLSLOT] Step 7b: Method 2 - BroadcastByTriggerType\n";
        SDK::ERollSlotTriggerType triggerType = SDK::ERollSlotTriggerType::StartBattleWithCooldown;
        rollSlotCtrl->BP_BroadcastByTriggerType(triggerType, refParam, 0);
        logBuffer << "[CH101_ROLLSLOT] BP_BroadcastByTriggerType completed\n";
        
        // Method 3: Try with DealDamage trigger
        logBuffer << "[CH101_ROLLSLOT] Step 7c: Method 3 - DealDamage trigger\n";
        rollSlotCtrl->BP_BroadcastByTriggerType(SDK::ERollSlotTriggerType::DealDamage, refParam, 0);
        logBuffer << "[CH101_ROLLSLOT] DealDamage broadcast completed\n";
        logBuffer << "[CH101_ROLLSLOT] ===== SUCCESS =====\n";

        flushRollSlotLog();
        return true;
    }
    catch (const std::exception& e)
    {
        logBuffer << "[CH101_ROLLSLOT] ERROR: Exception caught: " << e.what() << "\n";
        logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
        
        flushRollSlotLog();
        return false;
    }
    catch (...)
    {
        logBuffer << "[CH101_ROLLSLOT] ERROR: Unknown exception\n";
        logBuffer << "[CH101_ROLLSLOT] ===== FAILED =====\n";
        
        flushRollSlotLog();
        return false;
    }
}

// ============================================
// GET CURRENT SEASON PASS RANK
// ============================================

/**
 * Helper: Write timestamped log to rank file
 */
static void LogToSeasonPassRankFile(const std::string& message)
{
    if constexpr (!RUNTIME_FILE_DEBUG_LOGGING)
    {
        (void)message;
        return;
    }

    WriteRuntimeDebugLog("C:/Temp/SeasonPassRank.log", message + "\n");
}

/**
 * Get the current season pass rank
 * All logs written to C:/Temp/SeasonPassRank.log
 * @return Current season pass rank (int32), or -1 if error
 */
int InGameHack_GetCurrentSeasonPassRank()
{
    try
    {
        // Direct access to BackendSubsystem singleton
        SDK::UBackendSubsystem* backendSubsystem = SDK::UBackendSubsystem::GetDefaultObj();
        
        // Try to get the rank directly
        if (backendSubsystem)
        {
            SDK::UDatabaseParams* dbParams = backendSubsystem->GetDatabaseParams();
            if (dbParams)
            {
                SDK::UDbpResult* dbpResult = dbParams->GetResultData();
                if (dbpResult)
                {
                    int32_t currentRank = dbpResult->GetCurrentSeasonPassRank();
                    
                    // Log the result to file
                    LogToSeasonPassRankFile("[SUCCESS] Current Season Pass Rank: " + std::to_string(currentRank));
                    return currentRank;
                }
            }
        }

        // If we couldn't get rank through normal methods, return -1
        LogToSeasonPassRankFile("[ERROR] Could not retrieve rank from BackendSubsystem");
        return -1;
    }
    catch (const std::exception& e)
    {
        LogToSeasonPassRankFile("[ERROR] C++ Exception: " + std::string(e.what()));
        return -1;
    }
    catch (...)
    {
        LogToSeasonPassRankFile("[ERROR] Unknown exception occurred");
        return -1;
    }
}

// ============================================
// TEST SEASON PASS EXP INCREASE
// ============================================

/**
 * Helper: Write timestamped log to file
 */
static void LogToSeasonPassFile(const std::string& message)
{
    if constexpr (!RUNTIME_FILE_DEBUG_LOGGING)
    {
        (void)message;
        return;
    }

    WriteRuntimeDebugLog("C:/Temp/SeasonPassExp.log", message + "\n");
}

/**
 * Add real EXP to Season Pass (modify memory directly)
 * Modifies _prevSeasonExp and triggers rewards unlocking
 * Does NOT use BackendSubsystem functions
 * All logs go to C:/Temp/SeasonPassExp.log
 * SAFE: No function calls that could crash
 */
bool InGameHack_IncreaseSeasonPassExp(SDK::ESeasonType type, int32_t count)
{
    try
    {
        // Get widget instance
        SDK::USeasonPassLicensePurchaseWindow* purchaseWindow = nullptr;
        
        try
        {
            purchaseWindow = SDK::USeasonPassLicensePurchaseWindow::GetDefaultObj();
        }
        catch (...)
        {
            LogToSeasonPassFile("[ERROR] Exception getting widget instance");
            return false;
        }
        
        // Validate pointer is not null
        if (!purchaseWindow)
        {
            LogToSeasonPassFile("[ERROR] Widget GetDefaultObj() returned nullptr");
            return false;
        }

        // Additional validation: check pointer range (basic sanity check)
        uintptr_t addr = reinterpret_cast<uintptr_t>(purchaseWindow);
        if (addr < 0x10000 || addr > 0x7FFFFFFF0000)
        {
            LogToSeasonPassFile("[ERROR] Widget pointer invalid: 0x" + 
                              std::to_string(addr));
            return false;
        }

        // Access widget members safely - direct memory modification only
        try
        {
            // Read current EXP value (offset 0x03AC)
            int32_t currentExp = purchaseWindow->_prevSeasonExp;
            int32_t addedExp = count * 100;
            int32_t newExp = currentExp + addedExp;

            // Write new EXP value directly (SAFE: no function calls)
            purchaseWindow->_prevSeasonExp = newExp;
            
            // Update display value
            purchaseWindow->_purchaseExp = addedExp;

            // Log modification to file
            LogToSeasonPassFile("[SUCCESS] EXP modified: " + std::to_string(currentExp) + 
                              " -> " + std::to_string(newExp) + " (+= " + std::to_string(addedExp) + ")");
            
            LogToSeasonPassFile("[INFO] Season Type: " + std::to_string(static_cast<int32_t>(type)));
            
            return true;
        }
        catch (const std::exception& e)
        {
            LogToSeasonPassFile("[ERROR] Exception accessing widget members: " + 
                              std::string(e.what()));
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LogToSeasonPassFile("[ERROR] C++ Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        LogToSeasonPassFile("[ERROR] Unknown exception occurred");
        return false;
    }
}

// ============================================================================
// CHARACTER CONDITION AUTO-EXECUTION
// ============================================================================

/**
 * Process character condition auto-execution on battle mode entry
 * Detects transitions from invalid to valid battle mode and executes active toggles once
 */
void InGameHack_ProcessCharacterConditionAutoExecution(
    bool enableDekuMode,
    bool enableUnbreakable,
    bool enableCompressionRegen,
    bool enableMirioMode,
    bool enableTokoyamiMode,
    bool enableGenericCondition,
    int conditionId,
    int applyMode,
    int level,
    float duration,
    float value,
    float interval,
    int subLevel,
    bool timeOverwrite)
{
    // Static tracking of battle session state
    static bool wasInValidBattle = false;
    static bool hasExecutedInCurrentSession = false;  // Track if we already executed in this battle session
    
    try
    {
        // Check if currently in valid battle mode
        bool isCurrentlyInValidBattle = IsValidBattleMode();
        
        // Detect transition: from invalid to valid battle mode
        bool enteredValidBattle = (!wasInValidBattle && isCurrentlyInValidBattle);
        
        // Detect exit from battle: from valid to invalid
        bool exitedBattle = (wasInValidBattle && !isCurrentlyInValidBattle);
        
        // Update state for next frame
        wasInValidBattle = isCurrentlyInValidBattle;
        
        // If we exited battle, reset the execution flag for the next battle session
        if (exitedBattle)
        {
            hasExecutedInCurrentSession = false;
            Logger::LogInfo("[AUTO-EXECUTE] Exited battle mode - reset execution flag");
            return;
        }
        
        // If not in valid battle mode, don't do anything
        if (!isCurrentlyInValidBattle)
        {
            return;
        }
        
        // If we already executed in this session, don't execute again
        if (hasExecutedInCurrentSession)
        {
            return;
        }
        
        // If we just entered valid battle mode, execute active toggles ONCE
        if (enteredValidBattle)
        {
            Logger::LogInfo("[AUTO-EXECUTE] Entered valid battle mode - executing active conditions ONCE");
            
            // Mark that we've executed so we won't do it again this session
            hasExecutedInCurrentSession = true;
            
            // Execute DEKU MODE (CH202 Trans Mission - enum 85)
            if (enableDekuMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing DEKU MODE");
                InGameHack_CH202TransMission();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableDekuMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] DEKU MODE disabled after execution");
            }
            
            // Execute UNBREAKABLE
            if (enableUnbreakable)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing UNBREAKABLE");
                InGameHack_Unbreakable();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableUnbreakable = false;
                Logger::LogInfo("[AUTO-EXECUTE] UNBREAKABLE disabled after execution");
            }
            
            // Execute MR COMPRESSE MODE (Compression Regeneration)
            if (enableCompressionRegen)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing MR COMPRESSE MODE");
                InGameHack_CompressionRegeneration();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableCompressionRegen = false;
                Logger::LogInfo("[AUTO-EXECUTE] MR COMPRESSE MODE disabled after execution");
            }
            
            // Execute MIRIO MODE (CH024 Transparent)
            if (enableMirioMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing MIRIO MODE");
                InGameHack_CH024Transparent();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableMirioMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] MIRIO MODE disabled after execution");
            }
            
            // Execute TOKOYAMI DARK MODE (CH011 Abyss Dark Body)
            if (enableTokoyamiMode)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing TOKOYAMI DARK MODE");
                InGameHack_CH011AbyssDarkBody();
                // Auto-disable after execution
                ImGuiMenu::g_HackSettings.CharCondition_EnableTokoyamiMode = false;
                Logger::LogInfo("[AUTO-EXECUTE] TOKOYAMI DARK MODE disabled after execution");
            }

            if (enableGenericCondition)
            {
                Logger::LogInfo("[AUTO-EXECUTE] Executing generic character condition");
                InGameHack_ApplyCustomCharacterCondition(conditionId, applyMode, level, duration, value, interval, subLevel, timeOverwrite);
                ImGuiMenu::g_HackSettings.CharCondition_AutoExecute = false;
                Logger::LogInfo("[AUTO-EXECUTE] Generic character condition disabled after execution");
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[AUTO-EXECUTE] Exception: " + std::string(e.what()));
    }
    catch (...)
    {
        Logger::LogError("[AUTO-EXECUTE] Unknown exception");
    }
}

// ============================================================================
// PLAYER NAME CHANGE
// ============================================================================

/**
 * Change player name in the game
 * Calls AGameModeBase::ChangeName on server
 * @param newName - The new player name (max 255 characters)
 * @return true if successful, false otherwise
 */
bool InGameHack_ChangePlayerName(const char* newName)
{
    try
    {
        // Validate input
        if (!newName || newName[0] == '\0' || strlen(newName) > 255)
        {
            Logger::LogError("[ChangeName] Invalid player name");
            return false;
        }

        // Get player controller
        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!playerController)
        {
            Logger::LogError("[ChangeName] Could not get player controller");
            return false;
        }

        // Validate controller pointer
        if (IsBadReadPtr(playerController, sizeof(void*)))
        {
            Logger::LogError("[ChangeName] PlayerController pointer is invalid");
            return false;
        }

        // Convert C string (const char*) to wide string (wchar_t*)
        int requiredSize = MultiByteToWideChar(CP_UTF8, 0, newName, -1, NULL, 0);
        if (requiredSize <= 0)
        {
            Logger::LogError("[ChangeName] Failed to convert player name to wide string");
            return false;
        }

        wchar_t* wideNameBuffer = new wchar_t[requiredSize];
        MultiByteToWideChar(CP_UTF8, 0, newName, -1, wideNameBuffer, requiredSize);

        // Create FString from wide string
        SDK::FString newNameFString(wideNameBuffer);

        // Call ServerChangeName RPC on player controller
        playerController->ServerChangeName(newNameFString);

        // Clean up
        delete[] wideNameBuffer;

        Logger::LogInfo("[ChangeName] Successfully changed player name to: " + std::string(newName));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ChangeName] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ChangeName] Unknown exception");
        return false;
    }
}

bool InGameHack_BuyLicenseExp(int32_t count)
{
    try
    {
        // Validate input
        if (count <= 0 || count > 10000)
        {
            Logger::LogError("[BuyLicenseExp] Invalid count: " + std::to_string(count) + " (must be 1-10000)");
            return false;
        }

        // Get BackendSubsystem singleton via direct pointer access
        SDK::UBackendSubsystem* backendSubsystem = SDK::UBackendSubsystem::GetDefaultObj();
        if (!backendSubsystem)
        {
            Logger::LogError("[BuyLicenseExp] Could not get BackendSubsystem");
            return false;
        }

        if (IsBadReadPtr(backendSubsystem, sizeof(void*)))
        {
            Logger::LogError("[BuyLicenseExp] BackendSubsystem pointer is invalid");
            return false;
        }

        try
        {
            // Call BuyLicenseExp with conversion to SDK int32 type
            SDK::int32 requestId = backendSubsystem->BuyLicenseExp((SDK::int32)count);

            Logger::LogInfo("[BuyLicenseExp] Purchase request sent: count=" + std::to_string(count) + ", requestId=" + std::to_string(requestId));
            return true;
        }
        catch (...)
        {
            Logger::LogError("[BuyLicenseExp] Exception calling BuyLicenseExp method");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[BuyLicenseExp] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[BuyLicenseExp] Unknown exception");
        return false;
    }
}

bool InGameHack_AddSpecialLicenseExpLocal(int32_t exp)
{
    try
    {
        if (exp <= 0 || exp > 10000000)
        {
            Logger::LogError("[SpecialLicenseExpLocal] Invalid exp: " + std::to_string(exp) + " (must be 1-10000000)");
            return false;
        }

        SeasonLicenseDataSource source = ResolveSeasonLicenseDataSource();
        if (!IsValidPointer(source.seasonData) && !IsValidPointer(source.dbParams))
        {
            Logger::LogError("[SpecialLicenseExpLocal] Could not resolve UDbpSeason or UDatabaseParams");
            return false;
        }

        SDK::FDbSpecialLicenseListParam beforeDirect{};
        const bool hasBeforeDirect = GetDirectDbSpecialLicenseSafe(source.dbParams, beforeDirect);

        SDK::int32 beforeGetterExp = 0;
        SDK::int32 beforeGetterRank = 0;
        const bool hasBeforeGetterExp = GetSeasonIntGetterSafe(source.seasonData, 8, beforeGetterExp);
        const bool hasBeforeGetterRank = GetSeasonIntGetterSafe(source.seasonData, 11, beforeGetterRank);

        const bool nativeCalled = CallAddSpecialLicenseExpSafe(source.seasonData, static_cast<SDK::int32>(exp));

        SDK::FDbSpecialLicenseListParam afterNativeDirect{};
        const bool hasAfterNativeDirect = GetDirectDbSpecialLicenseSafe(source.dbParams, afterNativeDirect);

        SDK::int32 afterNativeGetterExp = 0;
        SDK::int32 afterNativeGetterRank = 0;
        const bool hasAfterNativeGetterExp = GetSeasonIntGetterSafe(source.seasonData, 8, afterNativeGetterExp);
        const bool hasAfterNativeGetterRank = GetSeasonIntGetterSafe(source.seasonData, 11, afterNativeGetterRank);

        bool nativeChanged = false;
        if (hasBeforeDirect && hasAfterNativeDirect && DidSpecialLicenseProgressChange(beforeDirect, afterNativeDirect))
            nativeChanged = true;
        if (hasBeforeGetterExp && hasAfterNativeGetterExp && beforeGetterExp != afterNativeGetterExp)
            nativeChanged = true;
        if (hasBeforeGetterRank && hasAfterNativeGetterRank && beforeGetterRank != afterNativeGetterRank)
            nativeChanged = true;

        bool directUpdated = false;
        if (!nativeChanged && IsValidPointer(source.dbParams) && hasAfterNativeDirect)
        {
            SDK::FDbSpecialLicenseListParam& specialLicense = source.dbParams->SpecialLicense;

            SDK::int64 targetTotalExp = static_cast<SDK::int64>(afterNativeDirect.TotalExp) + static_cast<SDK::int64>(exp);
            if (targetTotalExp < 0)
                targetTotalExp = 0;
            if (targetTotalExp > 2147483647LL)
                targetTotalExp = 2147483647LL;

            SpecialLicenseProgressFields fields{};
            if (CalculateSpecialLicenseProgressFields(afterNativeDirect, static_cast<SDK::int32>(targetTotalExp), fields))
            {
                directUpdated = ApplySpecialLicenseProgressFields(specialLicense, fields);
                if (!directUpdated)
                    Logger::LogError("[SpecialLicenseExpLocal] DatabaseParams.SpecialLicense fields are not writable");
            }
            else
            {
                Logger::LogError("[SpecialLicenseExpLocal] Could not calculate progress from SpecialLicenseList");
            }
        }

        SDK::FDbSpecialLicenseListParam finalDirect{};
        const bool hasFinalDirect = GetDirectDbSpecialLicenseSafe(source.dbParams, finalDirect);

        SDK::int32 finalGetterExp = 0;
        SDK::int32 finalGetterRank = 0;
        const bool hasFinalGetterExp = GetSeasonIntGetterSafe(source.seasonData, 8, finalGetterExp);
        const bool hasFinalGetterRank = GetSeasonIntGetterSafe(source.seasonData, 11, finalGetterRank);

        std::stringstream log;
        log << "[SpecialLicenseExpLocal] exp=" << exp
            << ", nativeCalled=" << (nativeCalled ? 1 : 0)
            << ", nativeChanged=" << (nativeChanged ? 1 : 0)
            << ", directUpdated=" << (directUpdated ? 1 : 0)
            << ", dbParamsSource=" << source.dbParamsSource
            << ", seasonDataSource=" << source.seasonDataSource;

        if (hasBeforeDirect)
        {
            log << ", beforeDirect(rank=" << beforeDirect.CurrentRank
                << ", currentExp=" << beforeDirect.CurrentExp
                << ", totalExp=" << beforeDirect.TotalExp
                << ", nextExp=" << beforeDirect.NextExp
                << ", maxExp=" << beforeDirect.MaxExp << ")";
        }

        if (hasFinalDirect)
        {
            log << ", finalDirect(rank=" << finalDirect.CurrentRank
                << ", currentExp=" << finalDirect.CurrentExp
                << ", totalExp=" << finalDirect.TotalExp
                << ", nextExp=" << finalDirect.NextExp
                << ", maxExp=" << finalDirect.MaxExp << ")";
        }

        if (hasBeforeGetterExp || hasBeforeGetterRank || hasFinalGetterExp || hasFinalGetterRank)
        {
            log << ", gettersBefore(rank=" << (hasBeforeGetterRank ? beforeGetterRank : -1)
                << ", exp=" << (hasBeforeGetterExp ? beforeGetterExp : -1)
                << "), gettersAfter(rank=" << (hasFinalGetterRank ? finalGetterRank : -1)
                << ", exp=" << (hasFinalGetterExp ? finalGetterExp : -1) << ")";
        }

        Logger::LogInfo(log.str());
        return nativeChanged || directUpdated || nativeCalled;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[SpecialLicenseExpLocal] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[SpecialLicenseExpLocal] Unknown exception");
        return false;
    }
}

bool InGameHack_DumpSeasonLicenseGetters()
{
    static constexpr const char* kLogPath = "C:\\Temp\\season_license_getters.log";
    static constexpr int32_t kLogLimit = 25;

    CreateDirectoryA("C:\\Temp", nullptr);

    std::ofstream logFile(kLogPath, std::ios::out | std::ios::trunc);
    if (!logFile.is_open())
    {
        Logger::LogError("[SeasonLicenseDump] Could not open C:\\Temp\\season_license_getters.log");
        return false;
    }

    std::stringstream ss;
    ss << "=== SEASON LICENSE GETTERS DUMP ==="
       << "\ndumpVersion: season-license-v2-gobjects-masterdata"
       << "\ncreatedTick: " << GetTickCount64()
       << "\nlogLimit: " << kLogLimit;

    SeasonLicenseDataSource source = ResolveSeasonLicenseDataSource();
    SDK::UBackendSubsystem* backendSubsystem = source.backendSubsystem;
    SDK::UDatabaseParams* dbParams = source.dbParams;
    SDK::UDbpSeason* seasonData = source.seasonData;

    ss << "\n\n=== POINTERS ==="
       << "\nbackendSubsystem: 0x" << std::hex << reinterpret_cast<uintptr_t>(backendSubsystem)
       << "\ndatabaseParams: 0x" << reinterpret_cast<uintptr_t>(dbParams)
       << "\nseasonData: 0x" << reinterpret_cast<uintptr_t>(seasonData) << std::dec
       << "\ndatabaseParamsSource: " << source.dbParamsSource
       << "\nseasonDataSource: " << source.seasonDataSource
       << "\nscannedObjects: " << source.scannedObjects
       << "\nbackendCandidates: " << source.backendCandidates
       << "\ndatabaseParamsCandidates: " << source.dbParamsCandidates
       << "\ndbpSeasonCandidates: " << source.seasonCandidates;

    if (IsValidPointer(dbParams))
    {
        SDK::FDbSeasonParam directSeasonInfo{};
        if (GetDirectDbSeasonParamSafe(dbParams, directSeasonInfo))
            AppendDbSeasonParamSummary(ss, "DatabaseParams.season direct field", directSeasonInfo, kLogLimit);
        else
            ss << "\n\n=== DatabaseParams.season direct field ==="
               << "\nstatus: failed";

        SDK::FDbSpecialLicenseListParam directSpecialLicense{};
        if (GetDirectDbSpecialLicenseSafe(dbParams, directSpecialLicense))
            AppendDbSpecialLicenseListParamSummary(ss, "DatabaseParams.SpecialLicense direct field", directSpecialLicense, kLogLimit);
        else
            ss << "\n\n=== DatabaseParams.SpecialLicense direct field ==="
               << "\nstatus: failed";
    }
    else
    {
        ss << "\n\n=== DatabaseParams direct fields ==="
           << "\nstatus: unavailable";
    }

    AppendMasterDataCacheProbeSummary(ss, kLogLimit);

    if (!IsValidPointer(seasonData))
    {
        ss << "\n\nERROR: UDbpSeason is not available. Direct DatabaseParams and MasterDataCache sections above are the fallback result.";
        logFile << ss.str() << "\n";
        Logger::LogError("[SeasonLicenseDump] UDbpSeason unavailable");
        return false;
    }

    ss << "\n\n=== BOOLEAN GETTERS ===";
    const char* boolGetterNames[] = {
        "CanBuyProLicense",
        "CanBuyProLicenseWithExp",
        "HasMiddleLicense",
        "HasProLicense",
    };

    for (int i = 0; i < static_cast<int>(sizeof(boolGetterNames) / sizeof(boolGetterNames[0])); ++i)
    {
        bool value = false;
        ss << "\n" << boolGetterNames[i] << ": ";
        if (GetSeasonBoolGetterSafe(seasonData, i, value))
            ss << (value ? 1 : 0);
        else
            ss << "failed";
    }

    ss << "\n\n=== INTEGER GETTERS ===";
    const char* intGetterNames[] = {
        "GetAvailableSpecialLicenseExpCount",
        "GetDiscountProLicensePrice",
        "GetHeroCrystal",
        "GetLicensePrice",
        "GetNextRankExp",
        "GetProLicenseLightPrice",
        "GetProLicensePrice",
        "GetProLicensePriceWithExp",
        "GetSpecialLicenseExp",
        "GetSpecialLicenseExpPrice",
        "GetSpecialLicenseMaxExp",
        "GetSpecialLicenseRank",
    };

    for (int i = 0; i < static_cast<int>(sizeof(intGetterNames) / sizeof(intGetterNames[0])); ++i)
    {
        SDK::int32 value = 0;
        ss << "\n" << intGetterNames[i] << ": ";
        if (GetSeasonIntGetterSafe(seasonData, i, value))
            ss << value;
        else
            ss << "failed";
    }

    SDK::FDbSeasonParam seasonInfo{};
    SDK::int32 currentSeasonRank = 1;
    if (GetSeasonInfoSafe(seasonData, seasonInfo))
    {
        currentSeasonRank = seasonInfo.SeasonRank > 0 ? seasonInfo.SeasonRank : 1;

        ss << "\n\n=== GetSeasonInfo ==="
           << "\ncode: " << seasonInfo.code
           << "\nseasonPassGroup: " << seasonInfo.SeasonPassGroup
           << "\nseasonPassRank: " << SeasonPassRankName(seasonInfo.SeasonPassRank) << " (" << static_cast<int>(seasonInfo.SeasonPassRank) << ")"
           << "\nseasonRank: " << seasonInfo.SeasonRank
           << "\nseasonRankExp: " << seasonInfo.SeasonRankExp
           << "\nstockCount: " << seasonInfo.StockCount;
        AppendStringField(ss, "", "name", SafeFTextToString(seasonInfo.Name));

        int32_t rankListCount = 0;
        if (SafeArrayCount(seasonInfo.ranks, rankListCount, 4096))
            ss << "\nranksCount: " << rankListCount;
        else
            ss << "\nranksCount: unreadable";
    }
    else
    {
        ss << "\n\n=== GetSeasonInfo ==="
           << "\nstatus: failed";
    }

    SDK::TArray<SDK::FDbSeasonPassParam> currentRankRewards;
    if (GetSeasonRewardsSafe(seasonData, currentSeasonRank, currentRankRewards))
    {
        std::stringstream label;
        label << "GetRewards(currentSeasonRank=" << currentSeasonRank << ")";
        AppendSeasonPassArraySummary(ss, label.str().c_str(), currentRankRewards, kLogLimit);
    }
    else
    {
        ss << "\n\n=== GetRewards(currentSeasonRank=" << currentSeasonRank << ") ==="
           << "\nstatus: failed";
    }

    SDK::TArray<SDK::FDbSeasonPassParam> firstRankRewards;
    if (currentSeasonRank != 1 && GetSeasonRewardsSafe(seasonData, 1, firstRankRewards))
        AppendSeasonPassArraySummary(ss, "GetRewards(rank=1)", firstRankRewards, kLogLimit);

    SDK::TArray<SDK::FDbSeasonPassParam> rewardRange;
    if (GetSeasonRewardRangeSafe(seasonData, 1, 10, rewardRange))
        AppendSeasonPassArraySummary(ss, "GetRewardRange(1,10)", rewardRange, kLogLimit);
    else
        ss << "\n\n=== GetRewardRange(1,10) ==="
           << "\nstatus: failed";

    SDK::TArray<SDK::FDbSpecialLicenseReward> lastRewards;
    if (GetSpecialLicenseLastRewardsSafe(seasonData, lastRewards))
        AppendSpecialLicenseRewardArraySummary(ss, "GetSpecialLicenseLastRewards", lastRewards, kLogLimit);
    else
        ss << "\n\n=== GetSpecialLicenseLastRewards ==="
           << "\nstatus: failed";

    SDK::TMap<SDK::int32, SDK::FDbSpecialLicenseParam> specialLicenseList;
    if (GetSpecialLicenseListSafe(seasonData, specialLicenseList))
        AppendSpecialLicenseMapSummary(ss, "GetSpecialLicenseList", specialLicenseList, kLogLimit);
    else
        ss << "\n\n=== GetSpecialLicenseList ==="
           << "\nstatus: failed";

    SDK::TMap<SDK::FDbItemCategoryParam, SDK::int32> stockItems;
    if (GetSeasonStockItemsSafe(seasonData, stockItems))
        AppendStockItemsMapSummary(ss, "GetStockItems", stockItems, kLogLimit);
    else
        ss << "\n\n=== GetStockItems ==="
           << "\nstatus: failed";

    logFile << ss.str() << "\n";
    logFile.close();

    Logger::LogInfo("[SeasonLicenseDump] Wrote C:\\Temp\\season_license_getters.log");
    return true;
}

bool InGameHack_GenerateProjectileInFront()
{
    try
    {
        // Get player controller
        SDK::APlayerController* playerController = SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[GenerateProjectile] No player controller found");
            return false;
        }

        // Get player character
        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!playerChar)
        {
            Logger::LogError("[GenerateProjectile] No player character found");
            return false;
        }

        // Get the projectile replicator component from the character
        SDK::UProjectileReplicateBattleComponent* projectileComp = GetProjectileReplicatorSafe(playerChar);
        if (!IsValidPointer(projectileComp))
        {
            Logger::LogError("[GenerateProjectile] No projectile replicator component found on player character");
            return false;
        }

        // Get player location
        SDK::FVector playerLocation = playerChar->K2_GetActorLocation();

        // Build FProjectileGenerateRep structure with hardcoded Ch010 PUSH_SPECIAL parameters
        SDK::FProjectileGenerateRep rep;
        rep.Parent = playerChar;
        rep.ParentComponent = nullptr;
        rep.generatorDataID = 1;        // Try ID 1 instead of 0
        rep.generatorDataIndexID = 0;
        rep.targetLocate.X = playerLocation.X;
        rep.targetLocate.Y = playerLocation.Y;
        rep.targetLocate.Z = playerLocation.Z;
        rep.initLocate.X = playerLocation.X;
        rep.initLocate.Y = playerLocation.Y;
        rep.initLocate.Z = playerLocation.Z;
        rep.initScale.X = 100;
        rep.initScale.Y = 100;
        rep.initScale.Z = 100;
        rep.initQuat.Pitch = 0;
        rep.initQuat.Yaw = 0;
        rep.initQuat.Roll = 0;
        rep.genID = 1;                  // Try genID 1 instead of 0
        rep.attachTime = 0.0f;
        rep.socketIdx = -1;             // -1 for no socket
        rep.serialID = 0;
        rep.charaId = 10;               // Ch010
        rep.AttachType = 0;
        rep.Level = 0;                  // Reset to 0
        rep.variationNo = 0;            // Default variation
        rep.commandId = 19;             // PUSH_SPECIAL
        rep.attackId = 6;               // SPECIAL attack type
        rep.conditionID = 0;            // Normal condition
        rep.attackSerial = 0;
        rep.overrideDamageAdjustAdd = 0;
        rep.overrideJsonIDX = -1;       // -1 for default JSON

        // Call the RPC to send projectile generation to server
        projectileComp->CreateGenerate_RPC(rep);
        
        Logger::LogInfo("[GenerateProjectile] Projectile generated successfully at Ch010 PUSH_SPECIAL");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[GenerateProjectile] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[GenerateProjectile] Unknown exception");
        return false;
    }
}
