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
#include <vector>
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
    constexpr uintptr_t NO_COLLISION_SPHERE_COMPONENT_OFFSET = 0x290;
    constexpr uintptr_t NO_COLLISION_TELEPORT_POSITION_OFFSET = 0x1D0;

    struct NoCollisionVector3
    {
        float X;
        float Y;
        float Z;
    };

    struct NoCollisionRotator3
    {
        float Pitch;
        float Yaw;
        float Roll;
    };

    static std::mutex g_InfiniteObjectsPatchMutex;
    static bool g_InfiniteObjectsPatchApplied = false;
    static bool g_InfiniteObjectsMismatchLogged = false;
    static bool g_NoCollisionInitialized = false;
    static SDK::FVector g_NoCollisionTargetPosition{};

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

static bool IsNoCollisionGameMode()
{
    try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!SafeMemory::IsReadable(world, sizeof(void*)))
            return false;

        SDK::AGameStateBase* baseGameState = nullptr;
        if (!SafeMemory::TryRead(&world->GameState, baseGameState))
            return false;

        SDK::AHerovsGameState* gameState = static_cast<SDK::AHerovsGameState*>(baseGameState);
        if (!SafeMemory::IsReadable(gameState, sizeof(void*)))
            return false;

        SDK::EGameModeType modeType{};
        if (!TryGetGameModeTypeSafe(gameState, modeType))
            return false;

        const int modeValue = static_cast<int>(modeType);
        return modeValue >= 1 && modeValue <= 9;
    }
    catch (...)
    {
        return false;
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
static inline bool IsLiveUObjectPointer(SDK::UObject* object);

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
    if (!IsLiveUObjectPointer(character))
        return nullptr;

    SDK::USkillManagementComponent* skillComponent = nullptr;
    if (!SafeReadMember(&character->_skillManagementComponent, skillComponent) || !IsLiveUObjectPointer(skillComponent))
        return nullptr;

    return skillComponent;
}

static inline bool TryIsPlusUltraActionActiveSafe(SDK::ACharacterBattle* character, bool& isActive)
{
    isActive = false;
    if (!IsLiveUObjectPointer(character))
        return false;

    SDK::UCharacterState* characterState = nullptr;
    __try
    {
        characterState = character->BP_GetState();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (!IsLiveUObjectPointer(characterState))
        return false;

    __try
    {
        isActive = characterState->BP_IsPlusUltraAction();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static inline bool TrySetPlusUltraStateSafe(SDK::APlayerStateBattle* playerState, bool enable)
{
    if (!IsLiveUObjectPointer(playerState))
        return false;

    __try
    {
        playerState->SetPlusUltraFastReload(enable);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static inline bool TryPrimePlusUltraPointSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsLiveUObjectPointer(playerState))
        return false;

    constexpr float ReadyPlusUltraPoint = 100.0f;
    constexpr float ReadyPlusUltraThreshold = 99.5f;

    float currentPoint = 0.0f;
    if (!SafeReadMember(&playerState->_plusUltraPoint, currentPoint) || !std::isfinite(currentPoint))
        return false;

    bool pointUpdated = false;
    if (currentPoint < ReadyPlusUltraThreshold)
    {
        if (!SafeMemory::TryWrite<float>(&playerState->_plusUltraPoint, ReadyPlusUltraPoint))
            return false;

        pointUpdated = true;
    }

    __try
    {
        if (pointUpdated)
            playerState->OnRep_PlusUltraPoint();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static inline bool TrySetPlusUltraSkillSafe(SDK::USkillManagementComponent* skillComponent)
{
    if (!IsLiveUObjectPointer(skillComponent))
        return false;

    __try
    {
        skillComponent->BP_SetPlusUltra();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static inline bool TrySetNoCollisionSafe(SDK::ACharacterBattle* character, bool enable)
{
    if (!IsLiveUObjectPointer(character))
        return false;

    __try
    {
        character->BP_SetEnableWallThrough(enable, enable);
        character->BP_SetEnableCharacterThrough(enable);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}

static inline bool TryGetNoCollisionPositionAddress(SDK::ACharacterBattle* character, SDK::FVector** outPosition)
{
    if (!outPosition)
        return false;

    *outPosition = nullptr;

    if (!IsValidPointer(character))
        return false;

    auto sphereComponentAddress = reinterpret_cast<SDK::UObject**>(
        reinterpret_cast<uintptr_t>(character) + NO_COLLISION_SPHERE_COMPONENT_OFFSET);

    SDK::UObject* sphereComponent = nullptr;
    if (!SafeReadMember(sphereComponentAddress, sphereComponent) || !IsLiveUObjectPointer(sphereComponent))
        return false;

    auto positionAddress = reinterpret_cast<SDK::FVector*>(
        reinterpret_cast<uintptr_t>(sphereComponent) + NO_COLLISION_TELEPORT_POSITION_OFFSET);

    if (!SafeMemory::IsReadable(positionAddress, sizeof(SDK::FVector)))
        return false;

    *outPosition = positionAddress;
    return true;
}

static inline bool TryReadNoCollisionPosition(SDK::ACharacterBattle* character, SDK::FVector& outPosition)
{
    SDK::FVector* positionAddress = nullptr;
    if (!TryGetNoCollisionPositionAddress(character, &positionAddress))
        return false;

    NoCollisionVector3 rawPosition{};
    if (!SafeMemory::TryRead(reinterpret_cast<const NoCollisionVector3*>(positionAddress), rawPosition) ||
        !std::isfinite(rawPosition.X) ||
        !std::isfinite(rawPosition.Y) ||
        !std::isfinite(rawPosition.Z))
    {
        return false;
    }

    outPosition = SDK::FVector(rawPosition.X, rawPosition.Y, rawPosition.Z);
    return true;
}

static inline bool TryWriteNoCollisionPosition(SDK::ACharacterBattle* character, const SDK::FVector& position)
{
    SDK::FVector* positionAddress = nullptr;
    if (!TryGetNoCollisionPositionAddress(character, &positionAddress))
        return false;

    const NoCollisionVector3 rawPosition{ position.X, position.Y, position.Z };
    return SafeMemory::TryWrite(reinterpret_cast<NoCollisionVector3*>(positionAddress), rawPosition);
}

static inline bool TryGetNoCollisionCameraRotation(SDK::APlayerController* playerController, SDK::FRotator& outRotation)
{
    if (!IsValidPointer(playerController))
        return false;

    SDK::APlayerCameraManager* cameraManager = nullptr;
    if (SafeReadMember(&playerController->PlayerCameraManager, cameraManager) && IsLiveUObjectPointer(cameraManager))
    {
        __try
        {
            outRotation = cameraManager->GetCameraRotation();
            if (std::isfinite(outRotation.Pitch) && std::isfinite(outRotation.Yaw) && std::isfinite(outRotation.Roll))
                return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    NoCollisionRotator3 rawRotation{};
    if (!SafeMemory::TryRead(reinterpret_cast<const NoCollisionRotator3*>(&playerController->ControlRotation), rawRotation))
        return false;

    if (!std::isfinite(rawRotation.Pitch) ||
        !std::isfinite(rawRotation.Yaw) ||
        !std::isfinite(rawRotation.Roll))
    {
        return false;
    }

    outRotation = SDK::FRotator(rawRotation.Pitch, rawRotation.Yaw, rawRotation.Roll);
    return true;
}

static inline bool TryGetActorLocationSafe(SDK::AActor* actor, SDK::FVector& outLocation)
{
    if (!IsLiveUObjectPointer(actor))
        return false;

    __try
    {
        outLocation = actor->K2_GetActorLocation();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return std::isfinite(outLocation.X) &&
        std::isfinite(outLocation.Y) &&
        std::isfinite(outLocation.Z);
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

    if (!IsValidPointer(conditionComponent) || !IsLiveUObjectPointer(conditionComponent))
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
    if (!IsLiveUObjectPointer(object))
        return "null";

    std::ostringstream oss;
    oss << "object@" << std::hex << reinterpret_cast<uintptr_t>(object);
    return oss.str();
}

static inline bool IsLiveUObjectPointer(SDK::UObject* object)
{
    if (!IsValidPointer(object))
        return false;

    SDK::UClass* objectClass = nullptr;
    if (!SafeReadMember(&object->Class, objectClass) || !IsValidPointer(objectClass))
        return false;

    return true;
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
    if (!IsLiveUObjectPointer(object))
        return true;

    __try
    {
        return object->IsDefaultObject();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
}

static inline bool IsObjectAUnsafeGuarded(SDK::UObject* object, SDK::UClass* klass)
{
    if (!IsLiveUObjectPointer(object) || !IsLiveUObjectPointer(klass))
        return false;

    __try
    {
        return object->IsA(klass);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
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

// ============================================
// INFINITE OBJECTS - SDK VERSION
// ============================================
// Pure-SDK replacement for the "mov [rcx+0x08], edx -> nop" byte patch.
// [rcx+0x08] is UC::TArray::NumElements; the patch froze it so consuming an
// item never decremented the holder array. Here we re-inflate NumElements of
// the local player's inventory holders each frame instead of patching code,
// so there is no hardcoded MHUR.exe+3ED21F5 offset to break on game updates.

namespace
{
    std::mutex g_InfiniteObjectsSdkMutex;
    SDK::USupplyHolderComponent* g_InfiniteObjectsLastComp = nullptr;
    std::map<SDK::USupplyHolder*, int32_t> g_InfiniteObjectsFrozen;  // holder -> highest legit count
}

// Force UC::TArray::NumElements (offset +0x08) without ever exceeding capacity.
template<typename T>
static inline bool SafeArrayForceCount(UC::TArray<T>& array, int32_t newCount)
{
    if (!SafeMemory::IsReadable(&array, sizeof(array)))
        return false;

    int32_t curMax = 0;
    try
    {
        curMax = array.Max();
    }
    catch (...)
    {
        return false;
    }

    // Never grow past MaxElements: that slot would point past allocated data.
    if (newCount < 0 || curMax < newCount)
        return false;

    int32_t* numAddr = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(&array) + 0x08);
    return SafeMemory::TryWrite(numAddr, newCount);
}

void InGameHack_ResetInfiniteObjectsSDK()
{
    std::lock_guard<std::mutex> lock(g_InfiniteObjectsSdkMutex);
    g_InfiniteObjectsFrozen.clear();
    g_InfiniteObjectsLastComp = nullptr;
}

// Call every frame while the feature is enabled. Keeps each enabled inventory
// holder's item count pinned to the highest value ever observed for it.
void InGameHack_TickInfiniteObjectsSDK()
{
    if (!IsValidBattleMode())
        return;

    SDK::APlayerController* playerController = SDK_GetPlayerController();
    SDK::ACharacterBattle* character = GetPlayerCharacterBattle(playerController);
    if (!IsValidPointer(character))
        return;

    SDK::APlayerStateBattle* playerState = GetPlayerStateBattleFromCharacterSafe(character);
    SDK::USupplyHolderComponent* comp = GetSupplyHolderComponentSafe(playerState);
    if (!IsValidPointer(comp))
        return;

    std::lock_guard<std::mutex> lock(g_InfiniteObjectsSdkMutex);

    // Drop tracking when the inventory component changes (new match / respawn)
    // so recycled holder pointers can't inherit a stale ceiling.
    if (comp != g_InfiniteObjectsLastComp)
    {
        g_InfiniteObjectsFrozen.clear();
        g_InfiniteObjectsLastComp = comp;
    }

    int32_t inventoryCount = 0;
    if (!SafeArrayCount(comp->_inventory, inventoryCount, 128))
        return;

    for (int32_t i = 0; i < inventoryCount; ++i)
    {
        SDK::USupplyHolder* holder = nullptr;
        if (!SafeArrayGet(comp->_inventory, i, holder, 128) || !IsValidPointer(holder))
            continue;

        bool enabled = false;
        if (!SafeReadMember(&holder->_bEnable, enabled) || !enabled)
            continue;

        int32_t supplyCount = 0;
        if (!SafeArrayCount(holder->_supplies, supplyCount, 64))
            continue;

        int32_t& frozen = g_InfiniteObjectsFrozen[holder];
        if (supplyCount > frozen)
            frozen = supplyCount;   // picked up more -> raise the ceiling
        if (frozen <= 0)
            continue;

        if (supplyCount < frozen)
        {
            // Re-inflate both parallel arrays together (1 supply <-> 1 serial)
            // so the holder stays internally consistent.
            SafeArrayForceCount(holder->_supplies, frozen);
            SafeArrayForceCount(holder->_serverSerialList, frozen);
        }
    }
}

// ============================================
// INVENTORY READER (read-only)
// ============================================
// Walks the local player's USupplyHolderComponent and reports which supplies
// each holder currently contains. Pure SDK reads, no writes — this is the
// foundation the InfiniteObjects logic builds on (know what we have before we
// decide what to keep). Everything is bounded + guarded like the rest of the
// file so a malformed holder can never fault the game thread.

static inline const char* SupplyHolderTypeName(SDK::ESupplyHolderType type)
{
    switch (type)
    {
    case SDK::ESupplyHolderType::INVENTORY:       return "INVENTORY";
    case SDK::ESupplyHolderType::ABILITYSLOT:     return "ABILITYSLOT";
    case SDK::ESupplyHolderType::SKILLSLOT:       return "SKILLSLOT";
    case SDK::ESupplyHolderType::ORBSLOT:         return "ORBSLOT";
    case SDK::ESupplyHolderType::SKILLCHANGESLOT: return "SKILLCHANGESLOT";
    case SDK::ESupplyHolderType::UNDEF:           return "UNDEF";
    default:                                      return "?";
    }
}

bool InGameHack_ReadInventory(InGameInventorySnapshot& out)
{
    out = InGameInventorySnapshot{};

    if (!IsValidBattleMode())
        return false;

    SDK::APlayerController* playerController = SDK_GetPlayerController();
    SDK::ACharacterBattle* character = GetPlayerCharacterBattle(playerController);
    if (!IsValidPointer(character))
        return false;

    SDK::APlayerStateBattle* playerState = GetPlayerStateBattleFromCharacterSafe(character);
    SDK::USupplyHolderComponent* comp = GetSupplyHolderComponentSafe(playerState);
    if (!IsValidPointer(comp))
        return false;

    int32_t inventoryCount = 0;
    if (!SafeArrayCount(comp->_inventory, inventoryCount, 128))
        return false;

    out.valid = true;

    for (int32_t i = 0; i < inventoryCount; ++i)
    {
        SDK::USupplyHolder* holder = nullptr;
        if (!SafeArrayGet(comp->_inventory, i, holder, 128) || !IsValidPointer(holder))
            continue;

        InGameInventoryHolder holderInfo{};
        holderInfo.inventoryIndex = i;

        SafeReadMember(&holder->_bEnable, holderInfo.enabled);
        SafeReadMember(&holder->_index, holderInfo.holderIndex);

        SDK::ESupplyHolderType holderType = SDK::ESupplyHolderType::UNDEF;
        if (SafeReadMember(&holder->_type, holderType))
            holderInfo.typeName = SupplyHolderTypeName(holderType);

        int32_t supplyCount = 0;
        if (SafeArrayCount(holder->_supplies, supplyCount, 64))
        {
            holderInfo.supplyCount = supplyCount;

            for (int32_t j = 0; j < supplyCount; ++j)
            {
                SDK::USupply* supply = nullptr;
                if (!SafeArrayGet(holder->_supplies, j, supply, 64) || !IsValidPointer(supply))
                {
                    holderInfo.supplyClassNames.push_back("<null>");
                    continue;
                }

                std::string supplyClass = "Unknown";
                GetClassNameSafe(supply, supplyClass);
                holderInfo.supplyClassNames.push_back(supplyClass);
            }
        }

        int32_t serialCount = 0;
        if (SafeArrayCount(holder->_serverSerialList, serialCount, 64))
            holderInfo.serialCount = serialCount;

        out.totalSupplies += holderInfo.supplyCount;
        out.holders.push_back(std::move(holderInfo));
    }

    return true;
}

// Dedicated inventory log file. The shared Logger is a no-op in release builds,
// so we write straight to disk here. Appends so successive dumps stack up.
static const char* const INVENTORY_LOG_PATH = "C:/temp/rugir_inventory.log";

static std::string CurrentTimestampString()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);

    struct tm timeinfo {};
    localtime_s(&timeinfo, &t);

    std::stringstream ss;
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void InGameHack_LogInventory()
{
    // Make sure C:\temp exists before opening the file.
    CreateDirectoryA("C:\\temp", nullptr);

    std::ofstream out(INVENTORY_LOG_PATH, std::ios::app);
    if (!out.is_open())
        return;

    out << "==== " << CurrentTimestampString() << " ====\n";

    InGameInventorySnapshot snapshot;
    if (!InGameHack_ReadInventory(snapshot) || !snapshot.valid)
    {
        out << "[Inventory] Could not read inventory (not in battle / no holder component)\n\n";
        return;
    }

    out << "[Inventory] holders=" << snapshot.holders.size()
        << " totalSupplies=" << snapshot.totalSupplies << "\n";

    for (const InGameInventoryHolder& holder : snapshot.holders)
    {
        out << "  holder #" << holder.inventoryIndex
            << " idx=" << holder.holderIndex
            << " type=" << holder.typeName
            << " enabled=" << (holder.enabled ? "1" : "0")
            << " supplies=" << holder.supplyCount
            << " serials=" << holder.serialCount;

        if (!holder.supplyClassNames.empty())
        {
            out << " [";
            for (size_t k = 0; k < holder.supplyClassNames.size(); ++k)
            {
                if (k != 0)
                    out << ", ";
                out << holder.supplyClassNames[k];
            }
            out << "]";
        }

        out << "\n";
    }

    out << "\n";
}

// ============================================
// DROP / DUPLICATE SUPPLIES
// ============================================
// USupplyHolderComponent::OnDrop_ToServer(serials, levelOverWrite, longDistance)
// asks the server to spawn the given supplies on the ground. Normally dropping
// also removes them from the inventory; with Infinite Objects active the count
// never decrements, so the dropped items are effectively duplicated.

static bool OnDropToServerRawSafe(
    SDK::USupplyHolderComponent* supplyHolderComp,
    const SDK::TArray<SDK::uint32>& serialID,
    SDK::uint8 levelOverWrite,
    bool longDropDistance)
{
    if (!IsValidPointer(supplyHolderComp))
        return false;

    __try
    {
        supplyHolderComp->OnDrop_ToServer(serialID, levelOverWrite, longDropDistance);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Drop (and, with Infinite Objects on, duplicate) every supply currently in the
// local player's inventory holders. Returns the number of serials sent.
int32_t InGameHack_DropInventorySupplies(bool longDropDistance)
{
    if (!IsValidBattleMode())
        return 0;

    SDK::APlayerController* playerController = SDK_GetPlayerController();
    SDK::ACharacterBattle* character = GetPlayerCharacterBattle(playerController);
    if (!IsValidPointer(character))
        return 0;

    SDK::APlayerStateBattle* playerState = GetPlayerStateBattleFromCharacterSafe(character);
    SDK::USupplyHolderComponent* comp = GetSupplyHolderComponentSafe(playerState);
    if (!IsValidPointer(comp))
        return 0;

    int32_t inventoryCount = 0;
    if (!SafeArrayCount(comp->_inventory, inventoryCount, 128))
        return 0;

    int32_t totalDropped = 0;

    for (int32_t i = 0; i < inventoryCount; ++i)
    {
        SDK::USupplyHolder* holder = nullptr;
        if (!SafeArrayGet(comp->_inventory, i, holder, 128) || !IsValidPointer(holder))
            continue;

        bool enabled = false;
        if (!SafeReadMember(&holder->_bEnable, enabled) || !enabled)
            continue;

        int32_t serialCount = 0;
        if (!SafeArrayCount(holder->_serverSerialList, serialCount, 64) || serialCount <= 0)
            continue;

        SDK::TAllocatedArray<SDK::uint32> serials(serialCount);
        for (int32_t j = 0; j < serialCount; ++j)
        {
            SDK::uint32 serial = 0;
            if (SafeArrayGet(holder->_serverSerialList, j, serial, 64))
                serials.Add(serial);
        }

        if (serials.Num() <= 0)
            continue;

        if (OnDropToServerRawSafe(comp, serials, 0, longDropDistance))
            totalDropped += serials.Num();
    }

    return totalDropped;
}

void InGameHack_LogDropInventorySupplies()
{
    CreateDirectoryA("C:\\temp", nullptr);

    std::ofstream out(INVENTORY_LOG_PATH, std::ios::app);
    if (out.is_open())
        out << "==== " << CurrentTimestampString() << " (DROP) ====\n";

    const int32_t dropped = InGameHack_DropInventorySupplies(false);

    if (out.is_open())
        out << "[Drop] serials dropped=" << dropped << "\n\n";
}

// ============================================
// CUSTOM DROP - WORLD ITEM CATALOG
// ============================================
// FName cannot be built from a string in this SDK, so to drop an item that is
// not in the inventory we must harvest a valid supplyId FName at runtime. We
// scan GObjects for AItemBase actors present in the match and read their
// ASupplyActorBase::_supplyId (+ _netSupplyId as a best-effort serial). The UI
// then lets the user pick one of these by name and drop N copies.

namespace
{
    struct WorldDropItem
    {
        std::string displayName;   // actor class name, e.g. "BP_Su011_C"
        SDK::FName  supplyId{};     // harvested ASupplyActorBase::_supplyId
        uint32_t    sampleSerial;  // ASupplyActorBase::_netSupplyId (best-effort)
    };

    std::mutex g_WorldItemCatalogMutex;
    std::vector<WorldDropItem> g_WorldItemCatalog;
}

static bool OnPickupToServerRawSafe(
    SDK::USupplyHolderComponent* supplyHolderComp,
    uint32_t serialID,
    const SDK::FName& supplyId)
{
    if (!IsValidPointer(supplyHolderComp))
        return false;

    __try
    {
        supplyHolderComp->OnPickup_ToServer(serialID, supplyId);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Scan the world for droppable item actors and rebuild the catalog.
// Returns the number of distinct items found.
int InGameHack_ScanWorldItemCatalog()
{
    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!SafeMemory::IsReadable(gObjects, sizeof(SDK::TUObjectArray)))
        return 0;

    int32_t objectCount = 0;
    if (!TryGetObjectArrayCountSafe(gObjects, objectCount))
        return 0;
    if (objectCount <= 0 || objectCount > 2000000)
        return 0;

    SDK::UClass* itemClass = SDK::AItemBase::StaticClass();

    std::vector<WorldDropItem> found;
    std::set<std::string> seenClasses;

    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
            continue;
        if (!IsObjectAUnsafeGuarded(obj, itemClass))
            continue;

        SDK::AItemBase* item = static_cast<SDK::AItemBase*>(obj);

        SDK::FName supplyId{};
        if (!SafeReadMember(&item->_supplyId, supplyId) || supplyId.IsNone())
            continue;

        std::string className = "Unknown";
        GetClassNameSafe(obj, className);
        if (seenClasses.count(className))
            continue;
        seenClasses.insert(className);

        uint16_t netSupplyId = 0;
        SafeReadMember(&item->_netSupplyId, netSupplyId);

        WorldDropItem entry;
        entry.displayName = className;
        entry.supplyId = supplyId;
        entry.sampleSerial = netSupplyId;
        found.push_back(std::move(entry));
    }

    std::lock_guard<std::mutex> lock(g_WorldItemCatalogMutex);
    g_WorldItemCatalog = std::move(found);
    return static_cast<int>(g_WorldItemCatalog.size());
}

// Copy the current catalog display names for the UI combo.
void InGameHack_GetWorldItemCatalogNames(std::vector<std::string>& out)
{
    out.clear();
    std::lock_guard<std::mutex> lock(g_WorldItemCatalogMutex);
    out.reserve(g_WorldItemCatalog.size());
    for (const WorldDropItem& e : g_WorldItemCatalog)
        out.push_back(e.displayName);
}

// Drop `quantity` copies of the catalog item at `catalogIndex`. With Infinite
// Objects active the inventory count is frozen, so the drops are duplicated.
// Logs results to C:\temp\rugir_inventory.log so the serial/pickup mechanism
// can be iterated on empirically. Returns the number of serials sent.
int InGameHack_DropCatalogItem(int catalogIndex, int quantity, bool longDrop)
{
    if (quantity < 1)   quantity = 1;
    if (quantity > 100) quantity = 100;

    CreateDirectoryA("C:\\temp", nullptr);
    std::ofstream out(INVENTORY_LOG_PATH, std::ios::app);
    if (out.is_open())
        out << "==== " << CurrentTimestampString() << " (CUSTOM DROP) ====\n";

    WorldDropItem item;
    {
        std::lock_guard<std::mutex> lock(g_WorldItemCatalogMutex);
        if (catalogIndex < 0 || catalogIndex >= static_cast<int>(g_WorldItemCatalog.size()))
        {
            if (out.is_open())
                out << "[CustomDrop] invalid catalog index=" << catalogIndex
                    << " size=" << g_WorldItemCatalog.size() << "\n\n";
            return 0;
        }
        item = g_WorldItemCatalog[catalogIndex];
    }

    if (out.is_open())
        out << "[CustomDrop] item=" << item.displayName
            << " serial=" << item.sampleSerial
            << " qty=" << quantity << "\n";

    if (!IsValidBattleMode())
    {
        if (out.is_open()) out << "[CustomDrop] not in battle mode\n\n";
        return 0;
    }

    SDK::APlayerController* playerController = SDK_GetPlayerController();
    SDK::ACharacterBattle* character = GetPlayerCharacterBattle(playerController);
    SDK::APlayerStateBattle* playerState = GetPlayerStateBattleFromCharacterSafe(character);
    SDK::USupplyHolderComponent* comp = GetSupplyHolderComponentSafe(playerState);
    if (!IsValidPointer(comp))
    {
        if (out.is_open()) out << "[CustomDrop] no SupplyHolderComponent\n\n";
        return 0;
    }

    // Build a serial array of `quantity` entries (same serial repeated). If the
    // server dedupes by serial we will vary these in a follow-up iteration.
    SDK::TAllocatedArray<SDK::uint32> serials(quantity);
    for (int i = 0; i < quantity; ++i)
        serials.Add(item.sampleSerial);

    // Best-effort: inject the item via pickup first (for items not owned), then
    // drop. Both are logged so we can see which step the server accepts.
    const bool pickup = OnPickupToServerRawSafe(comp, item.sampleSerial, item.supplyId);
    const bool dropped = OnDropToServerRawSafe(comp, serials, 0, longDrop);

    if (out.is_open())
        out << "[CustomDrop] pickup=" << (pickup ? "1" : "0")
            << " drop=" << (dropped ? "1" : "0")
            << " sent=" << serials.Num() << "\n\n";

    return dropped ? serials.Num() : 0;
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
static bool CallBPSetConditionSafe(
    SDK::UCharacterConditionControlComponent* conditionComponent,
    SDK::ECharacterConditionId id,
    int level,
    float span,
    float value,
    float interval,
    int subLevel,
    SDK::APlayerStateBattle* instigatedPlayer,
    int damageActionSerialNo);
static bool CallSetConditionToServerSafe(
    SDK::UCharacterConditionControlComponent* conditionComponent,
    SDK::ECharacterConditionId id,
    int level,
    float span,
    float value,
    float interval,
    int subLevel,
    SDK::APlayerStateBattle* instigatedPlayer,
    int damageActionSerialNo,
    bool timeOverwrite);

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

            if (!GetCharacterPlayerStateSafe(playerCharacter))
            {
                Logger::LogError("[CharacterCondition] Player character is not fully ready for ability condition");
                return false;
            }

            instigatedPlayer = nullptr;
            applyMode = 1;
            value = value > 0.0f ? value : 1000.0f;
            interval = interval > 0.0f ? interval : 0.1f;
            subLevel = subLevel > 0 ? subLevel : level;
        }

        switch (applyMode)
        {
        case 1:
            if (!CallBPSetConditionSafe(conditionComponent, id, level, duration, value, interval, subLevel, instigatedPlayer, 0))
            {
                Logger::LogError("[CharacterCondition] SEH exception applying BP condition ID " + std::to_string(conditionId));
                return false;
            }
            break;
        case 2:
            conditionComponent->BP_SetConditionLocal(id, level, duration, value, interval, subLevel, instigatedPlayer, 0);
            break;
        case 0:
        default:
            if (!CallSetConditionToServerSafe(conditionComponent, id, level, duration, value, interval, subLevel, instigatedPlayer, 0, timeOverwrite))
            {
                Logger::LogError("[CharacterCondition] SEH exception applying condition ID " + std::to_string(conditionId));
                return false;
            }
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
    if (!IsValidPointer(conditionComponent) || !IsLiveUObjectPointer(conditionComponent))
        return false;

    if (instigatedPlayer && !IsLiveUObjectPointer(instigatedPlayer))
        instigatedPlayer = nullptr;

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

static bool CallSetConditionToServerSafe(
    SDK::UCharacterConditionControlComponent* conditionComponent,
    SDK::ECharacterConditionId id,
    int level,
    float span,
    float value,
    float interval,
    int subLevel,
    SDK::APlayerStateBattle* instigatedPlayer,
    int damageActionSerialNo,
    bool timeOverwrite)
{
    if (!IsValidPointer(conditionComponent) || !IsLiveUObjectPointer(conditionComponent))
        return false;

    if (instigatedPlayer && !IsLiveUObjectPointer(instigatedPlayer))
        instigatedPlayer = nullptr;

    __try
    {
        conditionComponent->SetCondition_ToServer(
            id,
            level,
            span,
            value,
            interval,
            subLevel,
            instigatedPlayer,
            damageActionSerialNo,
            timeOverwrite,
            nullptr
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

    if (!GetCharacterPlayerStateSafe(playerCharacter))
    {
        Logger::LogError(std::string("[COMBAT] Player character is not fully ready for ") + logName);
        return false;
    }

    if (!CallBPSetConditionSafe(conditionComponent, id, level, 500.0f, 1000.0f, 0.1f, level, nullptr, 0))
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

static bool TextContainsCvNoneCurveName(const std::string& text)
{
    return text.find("CV_none") != std::string::npos ||
           text.find("CV_None") != std::string::npos;
}

static SDK::UCurveFloat* FindCvNoneCurveFloat()
{
    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!IsValidPointer(gObjects))
        return nullptr;

    int32_t objectCount = 0;
    if (!TryGetObjectArrayCountSafe(gObjects, objectCount) || objectCount <= 0 || objectCount > 2000000)
        return nullptr;

    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsLiveUObjectPointer(obj) || IsObjectDefaultSafe(obj))
            continue;

        std::string className;
        if (!GetClassNameSafe(obj, className) || className != "CurveFloat")
            continue;

        std::string objectName;
        std::string fullName;
        try
        {
            objectName = obj->GetName();
            fullName = obj->GetFullName();
        }
        catch (...)
        {
            objectName.clear();
            fullName.clear();
        }

        if (TextContainsCvNoneCurveName(objectName) || TextContainsCvNoneCurveName(fullName))
            return static_cast<SDK::UCurveFloat*>(obj);
    }

    return nullptr;
}

static bool GetCvNoneCurveKeyData(SDK::UCurveFloat* curve, SDK::FRichCurveKey*& keyData, int32_t& keyCount)
{
    keyData = nullptr;
    keyCount = 0;

    if (!IsLiveUObjectPointer(curve))
        return false;

    SDK::FRichCurve* richCurve = &curve->FloatCurve;
    if (!SafeMemory::IsReadable(richCurve, sizeof(SDK::FRichCurve)))
        return false;

    if (!SafeArrayCount(richCurve->Keys, keyCount, 4096) || keyCount <= 0)
        return false;

    const SDK::FRichCurveKey* constKeyData = nullptr;
    try
    {
        constKeyData = richCurve->Keys.GetDataPtr();
    }
    catch (...)
    {
        return false;
    }

    if (!SafeMemory::IsReadable(constKeyData, sizeof(SDK::FRichCurveKey) * static_cast<size_t>(keyCount)))
        return false;

    keyData = const_cast<SDK::FRichCurveKey*>(constKeyData);
    return true;
}

static bool WriteFloatValueWithProtection(float* target, float value)
{
    if (!target)
        return false;

    if (SafeMemory::TryWrite<float>(target, value))
        return true;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sizeof(float), PAGE_READWRITE, &oldProtect))
        return false;

    const bool success = SafeMemory::TryWrite<float>(target, value);

    DWORD unusedProtect = 0;
    VirtualProtect(target, sizeof(float), oldProtect, &unusedProtect);
    return success;
}

bool InGameHack_SetCvNoneCurveValue(float value)
{
    static std::mutex s_CvNoneCurveMutex;
    static SDK::UCurveFloat* s_CachedCurve = nullptr;
    static SDK::FRichCurveKey* s_CachedKeyData = nullptr;
    static int32_t s_CachedKeyCount = 0;
    static std::vector<float> s_OriginalValues;

    if (!std::isfinite(value))
        value = 1.0f;

    if (value < 1.0f)
        value = 1.0f;
    if (value > 300.0f)
        value = 300.0f;

    std::lock_guard<std::mutex> lock(s_CvNoneCurveMutex);

    SDK::UCurveFloat* curve = s_CachedCurve;
    SDK::FRichCurveKey* keyData = nullptr;
    int32_t keyCount = 0;

    if (!curve || !GetCvNoneCurveKeyData(curve, keyData, keyCount) ||
        keyData != s_CachedKeyData || keyCount != s_CachedKeyCount)
    {
        s_CachedCurve = nullptr;
        s_CachedKeyData = nullptr;
        s_CachedKeyCount = 0;
        s_OriginalValues.clear();

        curve = FindCvNoneCurveFloat();
        if (!curve || !GetCvNoneCurveKeyData(curve, keyData, keyCount))
            return false;

        s_CachedCurve = curve;
        s_CachedKeyData = keyData;
        s_CachedKeyCount = keyCount;

        s_OriginalValues.reserve(static_cast<size_t>(keyCount));
        for (int32_t i = 0; i < keyCount; ++i)
        {
            SDK::FRichCurveKey key{};
            if (!SafeMemory::TryRead(keyData + i, key))
            {
                s_CachedCurve = nullptr;
                s_CachedKeyData = nullptr;
                s_CachedKeyCount = 0;
                s_OriginalValues.clear();
                return false;
            }

            s_OriginalValues.push_back(key.value);
        }
    }

    const bool restoreOriginals = value <= 1.0001f;
    int32_t writtenCount = 0;

    for (int32_t i = 0; i < keyCount; ++i)
    {
        const float keyValue = (restoreOriginals && i < static_cast<int32_t>(s_OriginalValues.size()))
            ? s_OriginalValues[static_cast<size_t>(i)]
            : value;

        if (WriteFloatValueWithProtection(&keyData[i].value, keyValue))
            ++writtenCount;
    }

    return writtenCount == keyCount;
}

bool InGameHack_DumpCvNoneCurveScan()
{
    try
    {
        CreateDirectoryA("C:\\Temp", nullptr);

        std::ofstream logFile("C:\\Temp\\cv_none_curve_scan.log", std::ios::out | std::ios::trunc);
        if (!logFile.is_open())
            return false;

        logFile << "============ CV_none CurveFloat Scan ============\n";
        logFile << "Timestamp: " << std::time(nullptr) << "\n";
        logFile << "Target: CurveFloat objects where name/fullname contains CV_none or CV_None\n";
        logFile << "=================================================\n\n";

        SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
        if (!IsValidPointer(gObjects))
        {
            logFile << "ERROR: GObjects pointer is not readable\n";
            return false;
        }

        int32_t objectCount = 0;
        if (!TryGetObjectArrayCountSafe(gObjects, objectCount) || objectCount <= 0 || objectCount > 2000000)
        {
            logFile << "ERROR: invalid GObjects count: " << objectCount << "\n";
            return false;
        }

        int32_t curveFloatCount = 0;
        int32_t cvNoneCount = 0;
        int32_t totalKeys = 0;

        logFile << "GObjects count: " << objectCount << "\n\n";

        for (int32_t i = 0; i < objectCount; ++i)
        {
            SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
            if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
                continue;

            std::string className;
            if (!GetClassNameSafe(obj, className) || className != "CurveFloat")
                continue;

            ++curveFloatCount;

            std::string objectName;
            std::string fullName;
            try
            {
                objectName = obj->GetName();
                fullName = obj->GetFullName();
            }
            catch (...)
            {
                objectName.clear();
                fullName.clear();
            }

            const bool nameMatches =
                objectName.find("CV_none") != std::string::npos ||
                objectName.find("CV_None") != std::string::npos ||
                fullName.find("CV_none") != std::string::npos ||
                fullName.find("CV_None") != std::string::npos;

            if (!nameMatches)
                continue;

            ++cvNoneCount;
            auto* curve = static_cast<SDK::UCurveFloat*>(obj);

            logFile << "[MATCH #" << cvNoneCount << "] index=" << i
                    << " object=0x" << std::hex << reinterpret_cast<uintptr_t>(obj)
                    << " curve=0x" << reinterpret_cast<uintptr_t>(curve)
                    << std::dec << "\n";
            logFile << "  name=" << objectName << "\n";
            logFile << "  fullName=" << fullName << "\n";

            SDK::FRichCurve* richCurve = &curve->FloatCurve;

            if (!richCurve || !SafeMemory::IsReadable(richCurve, sizeof(SDK::FRichCurve)))
            {
                logFile << "  keys=unreadable\n\n";
                continue;
            }

            int32_t keyCount = 0;
            if (!SafeArrayCount(richCurve->Keys, keyCount, 4096))
            {
                logFile << "  keys=invalid-or-unreadable\n\n";
                continue;
            }

            totalKeys += keyCount;
            logFile << "  keys=" << keyCount << "\n";

            for (int32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
            {
                SDK::FRichCurveKey key{};
                if (!SafeArrayGet(richCurve->Keys, keyIndex, key, 4096))
                {
                    logFile << "    [" << keyIndex << "] unreadable\n";
                    continue;
                }

                logFile << "    [" << keyIndex << "] time=" << key.Time
                        << " value=" << key.value
                        << " interp=" << static_cast<int>(key.InterpMode)
                        << " tangent=" << static_cast<int>(key.TangentMode)
                        << "\n";
            }

            logFile << "\n";
        }

        logFile << "================ Summary ================\n";
        logFile << "CurveFloat objects scanned: " << curveFloatCount << "\n";
        logFile << "CV_none matches: " << cvNoneCount << "\n";
        logFile << "Total matched keys: " << totalKeys << "\n";
        logFile << "Output: C:\\Temp\\cv_none_curve_scan.log\n";
        return true;
    }
    catch (...)
    {
        return false;
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

bool InGameHack_GivePlusUltra()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsLiveUObjectPointer(playerChar))
            return false;

        bool plusUltraActive = false;
        if (TryIsPlusUltraActionActiveSafe(playerChar, plusUltraActive) && plusUltraActive)
            return false;

        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        const bool primedPlayerState = TryPrimePlusUltraPointSafe(playerState);

        SDK::USkillManagementComponent* skillComponent = GetSkillManagementComponentSafe(playerChar);
        if (!IsValidPointer(skillComponent))
            return primedPlayerState;

        return TrySetPlusUltraSkillSafe(skillComponent) || primedPlayerState;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[PlusUltra] Exception in GivePlusUltra: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[PlusUltra] Unknown exception in GivePlusUltra");
        return false;
    }
}

bool InGameHack_KeepPlusUltraReady()
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsLiveUObjectPointer(playerChar))
            return false;

        bool plusUltraActive = false;
        if (TryIsPlusUltraActionActiveSafe(playerChar, plusUltraActive) && plusUltraActive)
            return true;

        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        return TryPrimePlusUltraPointSafe(playerState);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[PlusUltra] Exception in KeepPlusUltraReady: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[PlusUltra] Unknown exception in KeepPlusUltraReady");
        return false;
    }
}

bool InGameHack_SetPlusUltraFastCharge(bool enable)
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        if (!IsValidPointer(playerState))
            return false;

        return TrySetPlusUltraStateSafe(playerState, enable);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[PlusUltra] Exception in SetPlusUltraFastCharge: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[PlusUltra] Unknown exception in SetPlusUltraFastCharge");
        return false;
    }
}

bool InGameHack_SetNoCollision(bool enable)
{
    try
    {
        if (!enable)
            g_NoCollisionInitialized = false;

        if (!IsNoCollisionGameMode())
            return false;

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsLiveUObjectPointer(playerChar))
            return false;

        return TrySetNoCollisionSafe(playerChar, enable);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[NoCollision] Exception in SetNoCollision: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[NoCollision] Unknown exception in SetNoCollision");
        return false;
    }
}

bool InGameHack_UpdateNoCollisionMovement(bool holdActive, float forwardAxis, float rightAxis, float speed)
{
    try
    {
        if (!IsNoCollisionGameMode())
        {
            g_NoCollisionInitialized = false;
            return false;
        }

        SDK::APlayerController* playerController = SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsLiveUObjectPointer(playerChar))
            return false;

        SDK::FVector currentPosition{};
        const bool hasCurrentPosition = TryReadNoCollisionPosition(playerChar, currentPosition);
        if (!g_NoCollisionInitialized)
        {
            if (hasCurrentPosition)
            {
                g_NoCollisionTargetPosition = currentPosition;
            }
            else
            {
                if (!TryGetActorLocationSafe(playerChar, g_NoCollisionTargetPosition))
                    return false;
            }

            g_NoCollisionInitialized = true;
        }

        if (!holdActive)
        {
            if (hasCurrentPosition)
                g_NoCollisionTargetPosition = currentPosition;
            return true;
        }

        constexpr float AxisDeadzone = 0.001f;
        if (!std::isfinite(forwardAxis))
            forwardAxis = 0.0f;
        if (!std::isfinite(rightAxis))
            rightAxis = 0.0f;

        forwardAxis = std::clamp(forwardAxis, -1.0f, 1.0f);
        rightAxis = std::clamp(rightAxis, -1.0f, 1.0f);

        if (std::abs(forwardAxis) < AxisDeadzone && std::abs(rightAxis) < AxisDeadzone)
        {
            if (hasCurrentPosition)
                g_NoCollisionTargetPosition = currentPosition;
            return true;
        }

        if (!std::isfinite(speed) || speed < 0.0f)
            speed = 0.0f;

        SDK::FRotator rotation{};
        if (!TryGetNoCollisionCameraRotation(playerController, rotation))
            return false;

        constexpr float DegToRad = 0.017453292519943295769f;
        constexpr float HalfPi = 1.570796326794896619f;
        const float yaw = rotation.Yaw * DegToRad;
        const float pitch = rotation.Pitch * DegToRad;
        const float cosPitch = std::cos(pitch);

        const SDK::FVector forward(
            cosPitch * std::cos(yaw),
            cosPitch * std::sin(yaw),
            std::sin(pitch));

        const SDK::FVector right(
            std::cos(yaw + HalfPi),
            std::sin(yaw + HalfPi),
            0.0f);

        g_NoCollisionTargetPosition.X += (forward.X * forwardAxis + right.X * rightAxis) * speed;
        g_NoCollisionTargetPosition.Y += (forward.Y * forwardAxis + right.Y * rightAxis) * speed;
        g_NoCollisionTargetPosition.Z += forward.Z * forwardAxis * speed;

        if (!std::isfinite(g_NoCollisionTargetPosition.X) ||
            !std::isfinite(g_NoCollisionTargetPosition.Y) ||
            !std::isfinite(g_NoCollisionTargetPosition.Z))
        {
            g_NoCollisionInitialized = false;
            return false;
        }

        return TryWriteNoCollisionPosition(playerChar, g_NoCollisionTargetPosition);
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[NoCollision] Exception in UpdateNoCollisionMovement: " + std::string(e.what()));
        g_NoCollisionInitialized = false;
        return false;
    }
    catch (...)
    {
        Logger::LogError("[NoCollision] Unknown exception in UpdateNoCollisionMovement");
        g_NoCollisionInitialized = false;
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

static bool GetUserRespawnCommandSatisfiedSafe(SDK::ACharacterBattle* playerChar)
{
    if (!IsValidPointer(playerChar))
        return false;

    __try
    {
        SDK::UCharacterCommandComponent* command = playerChar->BP_GetCommandComponent();
        return IsValidPointer(command) && command->BP_IsSatisfiedCommand(SDK::ECommandId::USERESPAWN);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static SDK::FVector_NetQuantize100 MakeNetQuantize100Safe(const SDK::FVector& location)
{
    SDK::FVector_NetQuantize100 targetLocation{};
    targetLocation.X = location.X;
    targetLocation.Y = location.Y;
    targetLocation.Z = location.Z;
    return targetLocation;
}

static SDK::UCharacterRespawnControlCompnent* GetRespawnControlComponentSafe(SDK::ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    __try
    {
        SDK::UCharacterRespawnControlCompnent* component = character->BP_GetRespawnControlComponent();
        return IsValidPointer(component) ? component : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static SDK::FVector_NetQuantize100 GetRespawnTargetLocationSafe(SDK::ACharacterBattle* targetCharacter, bool& usedRespawnComponent)
{
    usedRespawnComponent = false;

    SDK::FVector_NetQuantize100 fallback = GetActorLocationNetQuantize100Safe(targetCharacter);
    SDK::UCharacterRespawnControlCompnent* respawnComponent = GetRespawnControlComponentSafe(targetCharacter);
    if (!IsValidPointer(respawnComponent))
        return fallback;

    __try
    {
        SDK::FVector respawnLocation = respawnComponent->BP_GetRespawnLocation();
        usedRespawnComponent = true;
        return MakeNetQuantize100Safe(respawnLocation);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        usedRespawnComponent = false;
        return fallback;
    }
}

static bool SetCurrentActionButtonTargetSafe(SDK::UCharacterActionControlComponent* actionControl, SDK::AActor* targetActor)
{
    if (!IsValidPointer(actionControl) || !IsValidPointer(targetActor))
        return false;

    return SafeMemory::TryWrite(&actionControl->_currentActionButtonTarget, targetActor);
}

static void LogRespawnDebugFile(const std::string& message)
{
    CreateDirectoryA("C:\\Temp", nullptr);

    std::ofstream logFile("C:\\Temp\\respawn_debug.log", std::ios::out | std::ios::app);
    if (!logFile.is_open())
        return;

    logFile << "tick=" << GetTickCount64() << " " << message << "\n";
}

struct RespawnRosterCandidate
{
    SDK::APlayerStateBattle* playerState = nullptr;
    SDK::ACharacterBattle* character = nullptr;
    std::string playerName = "Unknown";
    int playerId = -1;
    int teamId = -1;
    bool dead = false;
    bool dying = false;
    bool spectator = false;
    bool onlySpectator = false;
    bool inactive = false;
    int respawnTagState = -1;
    int numberOfSpectators = 0;
    int score = 0;
};

static std::string PtrToHexString(const void* ptr)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr);
    return oss.str();
}

static const char* RespawnTagStateName(int state)
{
    switch (state)
    {
    case 0: return "ALIVE";
    case 1: return "SPAWNED";
    case 2: return "COLLECTED";
    case 3: return "RETIRE";
    default: return "UNKNOWN";
    }
}

static int GetHerovsPlayerTeamIdSafe(SDK::APlayerState* playerState)
{
    if (!IsValidPointer(playerState))
        return -1;

    __try
    {
        SDK::AHerovsPlayerState* herovsState = static_cast<SDK::AHerovsPlayerState*>(playerState);
        return herovsState->BP_GetTeamId();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    uint8_t teamId = 255;
    if (SafeMemory::TryRead(reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(playerState) + 0x32A), teamId))
        return teamId;

    return -1;
}

static int GetHerovsPlayerIdSafe(SDK::APlayerState* playerState)
{
    if (!IsValidPointer(playerState))
        return -1;

    __try
    {
        SDK::AHerovsPlayerState* herovsState = static_cast<SDK::AHerovsPlayerState*>(playerState);
        return herovsState->BP_GetPlayerId();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    int16_t herovsPlayerId = -1;
    if (SafeMemory::TryRead(reinterpret_cast<int16_t*>(reinterpret_cast<uintptr_t>(playerState) + 0x328), herovsPlayerId))
        return herovsPlayerId;

    int32_t enginePlayerId = -1;
    if (SafeReadMember(&playerState->playerId, enginePlayerId))
        return enginePlayerId;

    return -1;
}

static void GetPlayerStateSpectatorFlagsSafe(
    SDK::APlayerState* playerState,
    bool& spectator,
    bool& onlySpectator,
    bool& inactive)
{
    spectator = false;
    onlySpectator = false;
    inactive = false;

    if (!IsValidPointer(playerState))
        return;

    uint8_t flags = 0;
    if (!SafeMemory::TryRead(reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(playerState) + 0x22A), flags))
        return;

    spectator = (flags & 0x02) != 0;
    onlySpectator = (flags & 0x04) != 0;
    inactive = (flags & 0x20) != 0;
}

static bool GetPlayerStateDeadSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
        return false;

    __try
    {
        return playerState->BP_IsDead();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GetPlayerStateDyingSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
        return false;

    __try
    {
        return playerState->BP_IsDying();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static int GetRespawnTagStateValueSafe(SDK::APlayerStateBattle* playerState)
{
    if (!IsValidPointer(playerState))
        return -1;

    SDK::ERespawnTagState tagState = SDK::ERespawnTagState::ALIVE;
    if (!SafeReadMember(&playerState->_respawnTagState, tagState))
        return -1;

    return static_cast<int>(tagState);
}

static SDK::ACharacterBattle* PlayerIdsToCharacterBattleRawSafe(SDK::UWorld* world, int playerId)
{
    if (!IsValidPointer(world) || playerId < 0)
        return nullptr;

    __try
    {
        return SDK::UInGameStatics::PlayerIdsToCharacterBattle(world, playerId);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static SDK::ACharacterBattle* FindCharacterForPlayerStateSafe(
    SDK::UWorld* world,
    SDK::APlayerStateBattle* targetPlayerState,
    int playerId)
{
    if (!IsValidPointer(targetPlayerState))
        return nullptr;

    if (playerId >= 0 && IsValidPointer(world))
    {
        SDK::ACharacterBattle* byPlayerId = PlayerIdsToCharacterBattleRawSafe(world, playerId);
        if (IsValidPointer(byPlayerId) &&
            !IsObjectDefaultSafe(byPlayerId) &&
            GetCharacterPlayerStateSafe(byPlayerId) == targetPlayerState)
        {
            return byPlayerId;
        }
    }

    auto allCharacters = InGameHack_GetAllCharacterBattles();
    for (SDK::ACharacterBattle* character : allCharacters)
    {
        if (!IsValidPointer(character) || IsObjectDefaultSafe(character))
            continue;

        if (GetCharacterPlayerStateSafe(character) == targetPlayerState)
            return character;
    }

    return nullptr;
}

static bool IsRespawnCandidateState(const RespawnRosterCandidate& candidate)
{
    const bool tagNeedsRespawn =
        candidate.respawnTagState >= 0 &&
        candidate.respawnTagState != static_cast<int>(SDK::ERespawnTagState::ALIVE);

    return candidate.dead ||
           candidate.spectator ||
           candidate.onlySpectator ||
           tagNeedsRespawn;
}

static int ScoreRespawnCandidate(const RespawnRosterCandidate& candidate)
{
    if (!IsRespawnCandidateState(candidate))
        return 0;

    int score = 1;
    if (candidate.dead)
        score += 100;
    if (candidate.spectator || candidate.onlySpectator)
        score += 80;
    if (candidate.respawnTagState == static_cast<int>(SDK::ERespawnTagState::COLLECTED))
        score += 70;
    else if (candidate.respawnTagState > static_cast<int>(SDK::ERespawnTagState::ALIVE))
        score += 35;
    if (!candidate.character)
        score += 20;
    else
        score += 5;

    return score;
}

static void LogRespawnRosterCandidate(const RespawnRosterCandidate& candidate, int rosterIndex, bool sameTeam)
{
    std::string message =
        "[RespawnRoster] idx=" +
        std::to_string(rosterIndex) +
        " name=" +
        candidate.playerName +
        " playerId=" +
        std::to_string(candidate.playerId) +
        " team=" +
        std::to_string(candidate.teamId) +
        " sameTeam=" +
        (sameTeam ? "true" : "false") +
        " dead=" +
        (candidate.dead ? "true" : "false") +
        " dying=" +
        (candidate.dying ? "true" : "false") +
        " spectator=" +
        (candidate.spectator ? "true" : "false") +
        " onlySpectator=" +
        (candidate.onlySpectator ? "true" : "false") +
        " inactive=" +
        (candidate.inactive ? "true" : "false") +
        " tagState=" +
        RespawnTagStateName(candidate.respawnTagState) +
        "(" +
        std::to_string(candidate.respawnTagState) +
        ") hasCharacter=" +
        (IsValidPointer(candidate.character) ? "true" : "false") +
        " character=" +
        PtrToHexString(candidate.character) +
        " spectators=" +
        std::to_string(candidate.numberOfSpectators) +
        " score=" +
        std::to_string(candidate.score);

    LogRespawnDebugFile(message);
}

static bool FindAutoRespawnRosterCandidate(RespawnRosterCandidate& outCandidate)
{
    outCandidate = RespawnRosterCandidate{};

    SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
    if (!IsValidPointer(playerController))
    {
        LogRespawnDebugFile("[RespawnRoster] Could not get PlayerController");
        return false;
    }

    SDK::APlayerState* localBaseState = GetControllerPlayerStateSafe(playerController);
    if (!IsValidPointer(localBaseState))
    {
        LogRespawnDebugFile("[RespawnRoster] Could not get local PlayerState");
        return false;
    }

    const int localTeamId = GetHerovsPlayerTeamIdSafe(localBaseState);
    if (localTeamId < 0 || localTeamId == 255)
    {
        LogRespawnDebugFile("[RespawnRoster] Invalid local team id: " + std::to_string(localTeamId));
        return false;
    }

    SDK::UWorld* world = SDK::UWorld::GetWorld();
    SDK::AGameStateBase* gameState = GetGameStateSafe(world);
    if (!IsValidPointer(gameState))
    {
        LogRespawnDebugFile("[RespawnRoster] Could not get GameState");
        return false;
    }

    int32_t playerCount = 0;
    if (!SafeArrayCount(gameState->PlayerArray, playerCount, 128))
    {
        LogRespawnDebugFile("[RespawnRoster] Could not read GameState.PlayerArray");
        return false;
    }

    LogRespawnDebugFile(
        "[RespawnRoster] Scan start; players=" +
        std::to_string(playerCount) +
        " localTeam=" +
        std::to_string(localTeamId));

    bool found = false;
    int bestScore = 0;

    for (int32_t i = 0; i < playerCount; ++i)
    {
        SDK::APlayerState* baseState = nullptr;
        if (!SafeArrayGet(gameState->PlayerArray, i, baseState, 128) || !IsValidPointer(baseState))
            continue;

        if (baseState == localBaseState)
            continue;

        RespawnRosterCandidate candidate{};
        candidate.playerState = static_cast<SDK::APlayerStateBattle*>(baseState);
        candidate.playerName = GetPlayerNameSafe(baseState);
        candidate.playerId = GetHerovsPlayerIdSafe(baseState);
        candidate.teamId = GetHerovsPlayerTeamIdSafe(baseState);
        GetPlayerStateSpectatorFlagsSafe(
            baseState,
            candidate.spectator,
            candidate.onlySpectator,
            candidate.inactive);

        const bool sameTeam = candidate.teamId == localTeamId;
        if (sameTeam && IsValidPointer(candidate.playerState))
        {
            candidate.dead = GetPlayerStateDeadSafe(candidate.playerState);
            candidate.dying = GetPlayerStateDyingSafe(candidate.playerState);
            candidate.respawnTagState = GetRespawnTagStateValueSafe(candidate.playerState);
            SafeReadMember(&candidate.playerState->_numberOfSpectator, candidate.numberOfSpectators);
            candidate.character = FindCharacterForPlayerStateSafe(world, candidate.playerState, candidate.playerId);
            candidate.score = ScoreRespawnCandidate(candidate);
        }

        LogRespawnRosterCandidate(candidate, i, sameTeam);

        if (!sameTeam || candidate.score <= 0)
            continue;

        if (!found || candidate.score > bestScore)
        {
            outCandidate = candidate;
            bestScore = candidate.score;
            found = true;
        }
    }

    if (!found)
    {
        LogRespawnDebugFile("[RespawnRoster] No dead/spectator same-team candidate found");
        return false;
    }

    LogRespawnDebugFile(
        "[RespawnRoster] Selected target name=" +
        outCandidate.playerName +
        " playerId=" +
        std::to_string(outCandidate.playerId) +
        " team=" +
        std::to_string(outCandidate.teamId) +
        " tagState=" +
        RespawnTagStateName(outCandidate.respawnTagState) +
        " hasCharacter=" +
        (IsValidPointer(outCandidate.character) ? "true" : "false") +
        " score=" +
        std::to_string(outCandidate.score));

    return true;
}

static SDK::UDogTagManagerComponent* GetDogTagManagerSafe()
{
    SDK::UWorld* world = SDK::UWorld::GetWorld();
    SDK::AGameStateBase* baseGameState = GetGameStateSafe(world);
    if (!IsValidPointer(baseGameState))
        return nullptr;

    SDK::AGameStateBattle* gameStateBattle = static_cast<SDK::AGameStateBattle*>(baseGameState);
    if (!IsValidPointer(gameStateBattle))
        return nullptr;

    SDK::UDogTagManagerComponent* dogTagManager = nullptr;
    if (!SafeReadMember(&gameStateBattle->_dogTagManager, dogTagManager) || !IsValidPointer(dogTagManager))
        return nullptr;

    return dogTagManager;
}

static bool ConsumeDogTagToServerRawSafe(
    SDK::UDogTagManagerComponent* dogTagManager,
    SDK::APlayerStateBattle* targetPlayerState)
{
    if (!IsValidPointer(dogTagManager) || !IsValidPointer(targetPlayerState))
        return false;

    __try
    {
        dogTagManager->ConsumeDogTag_ToServer(targetPlayerState);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool SendRespawnDogTagConsumeSafe(SDK::APlayerStateBattle* targetPlayerState)
{
    if (!IsValidPointer(targetPlayerState))
    {
        LogRespawnDebugFile("[RespawnDogTag] No target PlayerStateBattle");
        return false;
    }

    SDK::UDogTagManagerComponent* dogTagManager = GetDogTagManagerSafe();
    if (!IsValidPointer(dogTagManager))
    {
        LogRespawnDebugFile("[RespawnDogTag] Could not get DogTagManagerComponent");
        return false;
    }

    if (!ConsumeDogTagToServerRawSafe(dogTagManager, targetPlayerState))
    {
        LogRespawnDebugFile("[RespawnDogTag] ConsumeDogTag_ToServer failed");
        return false;
    }

    LogRespawnDebugFile(
        "[RespawnDogTag] ConsumeDogTag_ToServer sent; target=" +
        GetPlayerNameSafe(targetPlayerState) +
        " playerId=" +
        std::to_string(GetHerovsPlayerIdSafe(targetPlayerState)));
    return true;
}

static bool InGameHack_SendUserRespawnAction(SDK::ACharacterBattle* targetCharacter)
{
    try
    {
        if (!IsValidBattleMode())
        {
            LogRespawnDebugFile("[UserRespawnAction] Not in valid battle mode");
            Logger::LogWarning("[UserRespawnAction] Not in valid battle mode");
            return false;
        }

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            LogRespawnDebugFile("[UserRespawnAction] Could not get PlayerController");
            Logger::LogError("[UserRespawnAction] Could not get PlayerController");
            return false;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsValidPointer(playerChar))
        {
            LogRespawnDebugFile("[UserRespawnAction] Could not get player character");
            Logger::LogError("[UserRespawnAction] Could not get player character");
            return false;
        }

        SDK::UCharacterActionControlComponent* actionControl = GetActionControlComponentSafe(playerChar);
        if (!IsValidPointer(actionControl))
        {
            LogRespawnDebugFile("[UserRespawnAction] Could not get CharacterActionControlComponent");
            Logger::LogError("[UserRespawnAction] Could not get CharacterActionControlComponent");
            return false;
        }

        if (!IsValidPointer(targetCharacter) || IsObjectDefaultSafe(targetCharacter))
        {
            LogRespawnDebugFile("[UserRespawnAction] No target CharacterBattle; auto target may be spectator-only");
            Logger::LogWarning("[UserRespawnAction] No target CharacterBattle");
            return false;
        }

        SDK::AActor* targetActor = static_cast<SDK::AActor*>(targetCharacter);

        unsigned char playerTeamId = GetCharacterTeamId(playerChar);
        unsigned char targetTeamId = GetCharacterTeamId(targetCharacter);
        if (playerTeamId == 255 || targetTeamId != playerTeamId)
        {
            LogRespawnDebugFile(
                "[UserRespawnAction] Selected target is not on same team; playerTeam=" +
                std::to_string(playerTeamId) +
                " targetTeam=" +
                std::to_string(targetTeamId));
            Logger::LogWarning("[UserRespawnAction] Selected target is not on same team");
            return false;
        }

        bool usedRespawnComponent = false;
        bool targetDying = IsCharacterDyingOffset(targetCharacter);
        bool commandBefore = GetUserRespawnCommandSatisfiedSafe(playerChar);
        bool actionTargetWritten = SetCurrentActionButtonTargetSafe(actionControl, targetActor);

        int32_t recoverAllyCount = 0;
        SafeArrayCount(actionControl->_characterListForRecoverAlly, recoverAllyCount, 64);

        SDK::FVector_NetQuantize100 targetLocation = GetRespawnTargetLocationSafe(targetCharacter, usedRespawnComponent);

        actionControl->SetAttackAction_ToServer(
            SDK::EAttackId::USEITEM,
            targetLocation,
            targetActor,
            0,
            SDK::ECommandId::USERESPAWN,
            true,
            0,
            SDK::EAttackBeginStateFlags::NONE);

        bool commandAfter = GetUserRespawnCommandSatisfiedSafe(playerChar);
        std::string message =
            std::string("[UserRespawnAction] SetAttackAction_ToServer(USERESPAWN) sent; target=CharacterBattle") +
            " commandBefore=" +
            (commandBefore ? "true" : "false") +
            " commandAfter=" +
            (commandAfter ? "true" : "false") +
            " targetDying=" +
            (targetDying ? "true" : "false") +
            " actionTargetWritten=" +
            (actionTargetWritten ? "true" : "false") +
            " recoverAllyCount=" +
            std::to_string(recoverAllyCount) +
            " usedRespawnComponent=" +
            (usedRespawnComponent ? "true" : "false");
        LogRespawnDebugFile(message);
        Logger::LogInfo(message);
        return true;
    }
    catch (const std::exception& e)
    {
        LogRespawnDebugFile("[UserRespawnAction] Exception: " + std::string(e.what()));
        Logger::LogError("[UserRespawnAction] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        LogRespawnDebugFile("[UserRespawnAction] Unknown exception");
        Logger::LogError("[UserRespawnAction] Unknown exception");
        return false;
    }
}

bool InGameHack_TestUserRespawnAction()
{
    RespawnRosterCandidate candidate;
    if (!FindAutoRespawnRosterCandidate(candidate))
        return false;

    if (!IsValidPointer(candidate.character))
    {
        LogRespawnDebugFile(
            "[UserRespawnAction] Auto target has no CharacterBattle; use respawn card path for PlayerState target=" +
            candidate.playerName +
            " playerId=" +
            std::to_string(candidate.playerId));
        Logger::LogWarning("[UserRespawnAction] Auto target has no CharacterBattle");
        return false;
    }

    return InGameHack_SendUserRespawnAction(candidate.character);
}

bool InGameHack_TestUserRespawnSelectedTeamMember(int teamMemberIndex);

bool InGameHack_TestUserRespawnSelectedTeamMember(int teamMemberIndex)
{
    if (teamMemberIndex < 0 || teamMemberIndex >= (int)ImGuiMenu::g_CurrentTeamCharacters.size())
    {
        LogRespawnDebugFile("[UserRespawnAction] Invalid selected team member index: " + std::to_string(teamMemberIndex));
        Logger::LogWarning("[UserRespawnAction] Invalid selected team member index: " + std::to_string(teamMemberIndex));
        return InGameHack_TestUserRespawnAction();
    }

    return InGameHack_SendUserRespawnAction(ImGuiMenu::g_CurrentTeamCharacters[teamMemberIndex]);
}

static bool IsRespawnSupplySafe(SDK::USupply* supply, std::string& supplyClassName)
{
    supplyClassName = "Unknown";
    if (!IsValidPointer(supply) || IsObjectDefaultSafe(supply))
        return false;

    GetClassNameSafe(supply, supplyClassName);
    if (IsObjectAUnsafeGuarded(supply, SDK::USupplyRespawn::StaticClass()) ||
        IsObjectAUnsafeGuarded(supply, SDK::USupplyRespawnFlagment::StaticClass()))
    {
        return true;
    }

    return supplyClassName.find("Respawn") != std::string::npos;
}

static SDK::USupplyHolder* FindRespawnSupplyHolderSafe(
    SDK::USupplyHolderComponent* supplyHolderComp,
    std::string& supplyClassName,
    int32_t& inventoryIndex,
    int32_t& supplyIndex,
    int32_t& scannedHolders,
    int32_t& scannedSupplies)
{
    supplyClassName = "Unknown";
    inventoryIndex = -1;
    supplyIndex = -1;
    scannedHolders = 0;
    scannedSupplies = 0;

    if (!IsValidPointer(supplyHolderComp))
        return nullptr;

    int32_t inventoryCount = 0;
    if (!SafeArrayCount(supplyHolderComp->_inventory, inventoryCount, 128))
        return nullptr;

    scannedHolders = inventoryCount;
    for (int32_t i = 0; i < inventoryCount; ++i)
    {
        SDK::USupplyHolder* holder = nullptr;
        if (!SafeArrayGet(supplyHolderComp->_inventory, i, holder, 128) || !IsValidPointer(holder))
            continue;

        bool holderEnabled = false;
        SafeReadMember(&holder->_bEnable, holderEnabled);
        if (!holderEnabled)
            continue;

        int32_t supplyCount = 0;
        if (!SafeArrayCount(holder->_supplies, supplyCount, 16))
            continue;

        scannedSupplies += supplyCount;
        for (int32_t j = 0; j < supplyCount; ++j)
        {
            SDK::USupply* supply = nullptr;
            if (!SafeArrayGet(holder->_supplies, j, supply, 16) || !IsValidPointer(supply))
                continue;

            std::string currentClass;
            if (!IsRespawnSupplySafe(supply, currentClass))
                continue;

            supplyClassName = currentClass;
            inventoryIndex = i;
            supplyIndex = j;
            return holder;
        }
    }

    return nullptr;
}

struct RespawnSupplyHolderSource
{
    SDK::USupplyHolderComponent* component = nullptr;
    SDK::USupplyHolder* holder = nullptr;
    std::string supplyClassName = "Unknown";
    int32_t inventoryIndex = -1;
    int32_t supplyIndex = -1;
    int32_t scannedHolders = 0;
    int32_t scannedSupplies = 0;
    int32_t scannedObjects = 0;
    int32_t scannedComponents = 0;
    bool ownerMatchesLocalPlayerState = false;
    const char* source = "none";
};

static SDK::AActor* GetComponentOwnerSafe(SDK::UActorComponent* component)
{
    if (!IsValidPointer(component))
        return nullptr;

    __try
    {
        SDK::AActor* owner = component->GetOwner();
        return IsValidPointer(owner) ? owner : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool TryResolveRespawnSupplyFromComponent(
    SDK::USupplyHolderComponent* supplyHolderComp,
    const char* source,
    SDK::APlayerState* localPlayerState,
    RespawnSupplyHolderSource& out)
{
    if (!IsValidPointer(supplyHolderComp))
        return false;

    std::string supplyClassName;
    int32_t inventoryIndex = -1;
    int32_t supplyIndex = -1;
    int32_t scannedHolders = 0;
    int32_t scannedSupplies = 0;
    SDK::USupplyHolder* holder = FindRespawnSupplyHolderSafe(
        supplyHolderComp,
        supplyClassName,
        inventoryIndex,
        supplyIndex,
        scannedHolders,
        scannedSupplies);
    if (!IsValidPointer(holder))
    {
        out.scannedHolders += scannedHolders;
        out.scannedSupplies += scannedSupplies;
        return false;
    }

    out.component = supplyHolderComp;
    out.holder = holder;
    out.supplyClassName = supplyClassName;
    out.inventoryIndex = inventoryIndex;
    out.supplyIndex = supplyIndex;
    out.scannedHolders += scannedHolders;
    out.scannedSupplies += scannedSupplies;
    out.ownerMatchesLocalPlayerState =
        IsValidPointer(localPlayerState) &&
        GetComponentOwnerSafe(supplyHolderComp) == static_cast<SDK::AActor*>(localPlayerState);
    out.source = source;
    return true;
}

static bool TryFindRespawnSupplyHolderFromGObjects(
    SDK::APlayerState* localPlayerState,
    RespawnSupplyHolderSource& out)
{
    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    int32_t objectCount = 0;
    if (!IsValidPointer(gObjects) || !TryGetObjectArrayCountSafe(gObjects, objectCount))
        return false;

    if (objectCount <= 0 || objectCount > 2000000)
        return false;

    SDK::UClass* supplyHolderClass = SDK::USupplyHolderComponent::StaticClass();
    if (!IsValidPointer(supplyHolderClass))
        return false;

    RespawnSupplyHolderSource firstAnyMatch{};
    bool foundAnyMatch = false;
    out.scannedObjects = objectCount;

    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
            continue;

        if (!IsObjectAUnsafeGuarded(obj, supplyHolderClass))
            continue;

        ++out.scannedComponents;
        SDK::USupplyHolderComponent* component = static_cast<SDK::USupplyHolderComponent*>(obj);

        RespawnSupplyHolderSource candidate{};
        candidate.scannedObjects = objectCount;
        candidate.scannedComponents = out.scannedComponents;
        if (!TryResolveRespawnSupplyFromComponent(component, "gobjects_forced_any_holder", localPlayerState, candidate))
        {
            out.scannedHolders += candidate.scannedHolders;
            out.scannedSupplies += candidate.scannedSupplies;
            continue;
        }

        out.scannedHolders += candidate.scannedHolders;
        out.scannedSupplies += candidate.scannedSupplies;

        if (candidate.ownerMatchesLocalPlayerState)
        {
            candidate.source = "gobjects_local_owner";
            candidate.scannedObjects = objectCount;
            candidate.scannedComponents = out.scannedComponents;
            out = candidate;
            return true;
        }

        if (!foundAnyMatch)
        {
            firstAnyMatch = candidate;
            foundAnyMatch = true;
        }
    }

    if (foundAnyMatch)
    {
        firstAnyMatch.scannedObjects = objectCount;
        firstAnyMatch.scannedComponents = out.scannedComponents;
        firstAnyMatch.scannedHolders = out.scannedHolders;
        firstAnyMatch.scannedSupplies = out.scannedSupplies;
        out = firstAnyMatch;
        return true;
    }

    return false;
}

static bool OnUseSupplyToServerRawSafe(
    SDK::USupplyHolderComponent* supplyHolderComp,
    const SDK::FNetSupplyHolderData& data)
{
    if (!IsValidPointer(supplyHolderComp))
        return false;

    __try
    {
        supplyHolderComp->OnUseSupply_ToServer(data);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool InGameHack_TestUseRespawnCardSupplyForCandidate(const RespawnRosterCandidate& candidate)
{
    try
    {
        if (!IsValidBattleMode())
        {
            LogRespawnDebugFile("[RespawnCardSupply] Not in valid battle mode");
            Logger::LogWarning("[RespawnCardSupply] Not in valid battle mode");
            return false;
        }

        SDK::APlayerController* playerController = (SDK::APlayerController*)SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            LogRespawnDebugFile("[RespawnCardSupply] Could not get PlayerController");
            Logger::LogError("[RespawnCardSupply] Could not get PlayerController");
            return false;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        const bool hasLocalCharacter = IsValidPointer(playerChar);
        if (!hasLocalCharacter)
            LogRespawnDebugFile("[RespawnCardSupply] Local CharacterBattle unavailable; forcing supply path without pawn");

        if (!IsValidPointer(candidate.playerState))
        {
            LogRespawnDebugFile("[RespawnCardSupply] Auto target has no PlayerStateBattle; supply will be sent without DogTag consume");
            Logger::LogWarning("[RespawnCardSupply] Auto target has no PlayerStateBattle");
        }

        SDK::APlayerState* localPlayerState = GetControllerPlayerStateSafe(playerController);
        const bool hasLocalPlayerState = IsValidPointer(localPlayerState);
        const int playerTeamId = GetHerovsPlayerTeamIdSafe(localPlayerState);
        if (hasLocalPlayerState && (playerTeamId < 0 || playerTeamId == 255 || candidate.teamId != playerTeamId))
        {
            LogRespawnDebugFile(
                "[RespawnCardSupply] Auto target is not on same team; playerTeam=" +
                std::to_string(playerTeamId) +
                " targetTeam=" +
                std::to_string(candidate.teamId));
            Logger::LogWarning("[RespawnCardSupply] Auto target is not on same team");
            return false;
        }
        if (!hasLocalPlayerState)
            LogRespawnDebugFile("[RespawnCardSupply] Local PlayerState unavailable; trusting auto roster target and forcing supply holder scan");

        SDK::ACharacterBattle* targetCharacter = candidate.character;

        SDK::APlayerStateBattle* playerState = GetPlayerStateBattle(playerController);
        RespawnSupplyHolderSource supplySource{};
        SDK::USupplyHolderComponent* playerStateSupplyHolderComp = GetSupplyHolderComponentSafe(playerState);
        if (TryResolveRespawnSupplyFromComponent(playerStateSupplyHolderComp, "playerstate", localPlayerState, supplySource))
        {
            supplySource.ownerMatchesLocalPlayerState = true;
        }
        else
        {
            RespawnSupplyHolderSource forcedSource{};
            if (TryFindRespawnSupplyHolderFromGObjects(localPlayerState, forcedSource))
            {
                supplySource = forcedSource;
                LogRespawnDebugFile(
                    "[RespawnCardSupply] Forced supply holder fallback selected; source=" +
                    std::string(supplySource.source) +
                    " scannedObjects=" +
                    std::to_string(supplySource.scannedObjects) +
                    " components=" +
                    std::to_string(supplySource.scannedComponents) +
                    " ownerMatchesLocalPlayerState=" +
                    (supplySource.ownerMatchesLocalPlayerState ? "true" : "false"));
            }
            else
            {
                LogRespawnDebugFile(
                    "[RespawnCardSupply] Could not find respawn SupplyHolderComponent; hasLocalPlayerState=" +
                    std::string(hasLocalPlayerState ? "true" : "false") +
                    " hasLocalCharacter=" +
                    (hasLocalCharacter ? "true" : "false") +
                    " gobjectsScanned=" +
                    std::to_string(forcedSource.scannedObjects) +
                    " components=" +
                    std::to_string(forcedSource.scannedComponents));
                Logger::LogError("[RespawnCardSupply] Could not find respawn SupplyHolderComponent");
                return false;
            }
        }

        SDK::USupplyHolderComponent* supplyHolderComp = supplySource.component;
        SDK::USupplyHolder* holder = supplySource.holder;

        int32_t serialCount = 0;
        if (!SafeArrayCount(holder->_serverSerialList, serialCount, 16) || serialCount <= 0)
        {
            LogRespawnDebugFile("[RespawnCardSupply] Respawn supply holder has no server serials");
            Logger::LogWarning("[RespawnCardSupply] Respawn supply holder has no server serials");
            return false;
        }

        SDK::TAllocatedArray<SDK::uint32> serverSerials(serialCount);
        for (int32_t i = 0; i < serialCount; ++i)
        {
            SDK::uint32 serial = 0;
            if (SafeArrayGet(holder->_serverSerialList, i, serial, 16))
                serverSerials.Add(serial);
        }

        if (serverSerials.Num() <= 0)
        {
            LogRespawnDebugFile("[RespawnCardSupply] Failed to copy server serials");
            Logger::LogWarning("[RespawnCardSupply] Failed to copy server serials");
            return false;
        }

        bool holderEnabled = true;
        int32_t holderIndex = supplySource.inventoryIndex;
        SDK::ESupplyHolderType holderType = SDK::ESupplyHolderType::INVENTORY;
        SafeReadMember(&holder->_bEnable, holderEnabled);
        SafeReadMember(&holder->_index, holderIndex);
        SafeReadMember(&holder->_type, holderType);

        SDK::TAllocatedArray<SDK::uint32> levels(0);
        SDK::FNetSupplyHolderData data{};
        data._serverSerialList = serverSerials;
        data._levelList = levels;
        data._bEnable = holderEnabled;
        data._index = holderIndex;
        data._type = holderType;
        data._manipulation = SDK::ESupplyManipulationType::USE;

        bool commandBefore = hasLocalCharacter ? GetUserRespawnCommandSatisfiedSafe(playerChar) : false;
        const bool supplySent = OnUseSupplyToServerRawSafe(supplyHolderComp, data);
        if (!supplySent)
        {
            LogRespawnDebugFile(
                "[RespawnCardSupply] OnUseSupply_ToServer failed; source=" +
                std::string(supplySource.source) +
                " holderIndex=" +
                std::to_string(holderIndex) +
                " serials=" +
                std::to_string(serverSerials.Num()));
            Logger::LogWarning("[RespawnCardSupply] OnUseSupply_ToServer failed");
        }

        const bool dogTagSent = IsValidPointer(candidate.playerState) ? SendRespawnDogTagConsumeSafe(candidate.playerState) : false;
        const bool hasCharacterTarget = IsValidPointer(targetCharacter) && !IsObjectDefaultSafe(targetCharacter);
        const bool actionSent = (hasLocalCharacter && hasCharacterTarget) ? InGameHack_SendUserRespawnAction(targetCharacter) : false;
        if (!hasCharacterTarget)
        {
            LogRespawnDebugFile(
                "[RespawnCardSupply] Auto target has no CharacterBattle; skipped USERESPAWN action and used PlayerState/DogTag path target=" +
                candidate.playerName +
                " playerId=" +
                std::to_string(candidate.playerId));
        }
        else if (!hasLocalCharacter)
        {
            LogRespawnDebugFile("[RespawnCardSupply] Skipped USERESPAWN action because local CharacterBattle is unavailable");
        }

        std::string message =
            "[RespawnCardSupply] Force use result; supply=" +
            supplySource.supplyClassName +
            " target=" +
            candidate.playerName +
            " playerId=" +
            std::to_string(candidate.playerId) +
            " tagState=" +
            RespawnTagStateName(candidate.respawnTagState) +
            " hasCharacter=" +
            (hasCharacterTarget ? "true" : "false") +
            " holderIndex=" +
            std::to_string(holderIndex) +
            " inventoryIndex=" +
            std::to_string(supplySource.inventoryIndex) +
            " supplyIndex=" +
            std::to_string(supplySource.supplyIndex) +
            " serials=" +
            std::to_string(serverSerials.Num()) +
            " commandBefore=" +
            (commandBefore ? "true" : "false") +
            " hasLocalCharacter=" +
            (hasLocalCharacter ? "true" : "false") +
            " hasLocalPlayerState=" +
            (hasLocalPlayerState ? "true" : "false") +
            " source=" +
            supplySource.source +
            " ownerMatchesLocalPlayerState=" +
            (supplySource.ownerMatchesLocalPlayerState ? "true" : "false") +
            " scannedObjects=" +
            std::to_string(supplySource.scannedObjects) +
            " holderComponents=" +
            std::to_string(supplySource.scannedComponents) +
            " scannedHolders=" +
            std::to_string(supplySource.scannedHolders) +
            " scannedSupplies=" +
            std::to_string(supplySource.scannedSupplies) +
            " supplySent=" +
            (supplySent ? "true" : "false") +
            " dogTagSent=" +
            (dogTagSent ? "true" : "false") +
            " actionSent=" +
            (actionSent ? "true" : "false");
        LogRespawnDebugFile(message);
        Logger::LogInfo(message);
        return supplySent || dogTagSent || actionSent;
    }
    catch (const std::exception& e)
    {
        LogRespawnDebugFile("[RespawnCardSupply] Exception: " + std::string(e.what()));
        Logger::LogError("[RespawnCardSupply] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        LogRespawnDebugFile("[RespawnCardSupply] Unknown exception");
        Logger::LogError("[RespawnCardSupply] Unknown exception");
        return false;
    }
}

bool InGameHack_TestUseRespawnCardSupply()
{
    RespawnRosterCandidate candidate;
    if (!FindAutoRespawnRosterCandidate(candidate))
        return false;

    return InGameHack_TestUseRespawnCardSupplyForCandidate(candidate);
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

struct RuntimeBackendSubsystemSource
{
    SDK::UBackendSubsystem* backendSubsystem = nullptr;
    int32_t scannedObjects = 0;
    int32_t backendCandidates = 0;
    const char* source = "none";
};

static RuntimeBackendSubsystemSource ResolveRuntimeBackendSubsystem()
{
    RuntimeBackendSubsystemSource result{};

    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    int32_t objectCount = 0;
    if (!IsValidPointer(gObjects) || !TryGetObjectArrayCountSafe(gObjects, objectCount))
        return result;

    if (objectCount <= 0 || objectCount > 2000000)
        return result;

    SDK::UClass* backendClass = SDK::UBackendSubsystem::StaticClass();
    if (!IsValidPointer(backendClass))
        return result;

    result.scannedObjects = objectCount;
    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
            continue;

        if (!IsObjectAUnsafeGuarded(obj, backendClass))
            continue;

        ++result.backendCandidates;
        result.backendSubsystem = static_cast<SDK::UBackendSubsystem*>(obj);
        result.source = "gobjects_runtime_backend";
        return result;
    }

    SDK::UBackendSubsystem* defaultBackend = SDK::UBackendSubsystem::GetDefaultObj();
    if (IsValidPointer(defaultBackend))
    {
        result.backendSubsystem = defaultBackend;
        result.source = "default_object_fallback";
    }

    return result;
}

static bool TryMakeWideStringFromUtf8(const char* text, std::wstring& outText)
{
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (requiredSize <= 0)
        return false;

    outText.assign(static_cast<size_t>(requiredSize), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, outText.data(), requiredSize) <= 0)
        return false;

    return true;
}

static bool TryUpdatePlayerName(SDK::UBackendSubsystem* backendSubsystem, const SDK::FString& playerName, SDK::int32& requestId)
{
    __try
    {
        requestId = backendSubsystem->UpdatePlayerName(playerName);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TrySetBackendPlayerPlatform(SDK::UBackendSubsystem* backendSubsystem, int platform)
{
    __try
    {
        backendSubsystem->SetPlayerPlatform(platform);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryForceBackendFakePlatform(SDK::UBackendSubsystem* backendSubsystem, const SDK::FString& platform)
{
    __try
    {
        backendSubsystem->ForceFakePlatform(platform);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static const char* BackendPlatformNameFromCode(int platform)
{
    switch (platform)
    {
    case 0: return "Invalid";
    case 1: return "PlayStation";
    case 2: return "Xbox";
    case 3: return "Windows";
    case 4: return "Switch";
    case 5: return "None";
    default: return "Unknown";
    }
}

/**
 * Change player name in the game
 * Calls BackendSubsystem::UpdatePlayerName so the request follows the profile backend path.
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

        RuntimeBackendSubsystemSource backendSource = ResolveRuntimeBackendSubsystem();
        SDK::UBackendSubsystem* backendSubsystem = backendSource.backendSubsystem;
        if (!IsValidPointer(backendSubsystem))
        {
            Logger::LogError("[ChangeName] Could not resolve runtime BackendSubsystem");
            return false;
        }

        std::wstring wideName;
        if (!TryMakeWideStringFromUtf8(newName, wideName))
        {
            Logger::LogError("[ChangeName] Failed to convert player name to FString");
            return false;
        }

        SDK::FString newNameFString(wideName.c_str());
        SDK::int32 requestId = -1;
        if (!TryUpdatePlayerName(backendSubsystem, newNameFString, requestId))
        {
            Logger::LogError("[ChangeName] UpdatePlayerName raised an SEH exception");
            return false;
        }

        if (requestId <= 0)
        {
            Logger::LogError("[ChangeName] UpdatePlayerName failed, requestId=" + std::to_string(requestId));
            return false;
        }

        Logger::LogInfo(
            "[ChangeName] UpdatePlayerName sent requestId=" + std::to_string(requestId) +
            " source=" + backendSource.source +
            " name=" + std::string(newName));
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

bool InGameHack_SetBackendPlayerPlatform(int platform)
{
    try
    {
        if (platform < 0 || platform > 5)
        {
            Logger::LogError("[Platform] Invalid platform code: " + std::to_string(platform));
            return false;
        }

        RuntimeBackendSubsystemSource backendSource = ResolveRuntimeBackendSubsystem();
        SDK::UBackendSubsystem* backendSubsystem = backendSource.backendSubsystem;
        if (!IsValidPointer(backendSubsystem))
        {
            Logger::LogError("[Platform] Could not resolve runtime BackendSubsystem");
            return false;
        }

        if (!TrySetBackendPlayerPlatform(backendSubsystem, platform))
        {
            Logger::LogError("[Platform] SetPlayerPlatform raised an SEH exception");
            return false;
        }

        Logger::LogInfo(
            "[Platform] SetPlayerPlatform sent platform=" + std::to_string(platform) +
            " name=" + BackendPlatformNameFromCode(platform) +
            " source=" + backendSource.source);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[Platform] SetPlayerPlatform exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[Platform] SetPlayerPlatform unknown exception");
        return false;
    }
}

bool InGameHack_ForceFakePlatform(const char* platformName)
{
    try
    {
        if (!platformName || platformName[0] == '\0' || strlen(platformName) > 63)
        {
            Logger::LogError("[Platform] Invalid fake platform name");
            return false;
        }

        RuntimeBackendSubsystemSource backendSource = ResolveRuntimeBackendSubsystem();
        SDK::UBackendSubsystem* backendSubsystem = backendSource.backendSubsystem;
        if (!IsValidPointer(backendSubsystem))
        {
            Logger::LogError("[Platform] Could not resolve runtime BackendSubsystem");
            return false;
        }

        std::wstring widePlatform;
        if (!TryMakeWideStringFromUtf8(platformName, widePlatform))
        {
            Logger::LogError("[Platform] Failed to convert fake platform to FString");
            return false;
        }

        SDK::FString platformFString(widePlatform.c_str());
        if (!TryForceBackendFakePlatform(backendSubsystem, platformFString))
        {
            Logger::LogError("[Platform] ForceFakePlatform raised an SEH exception");
            return false;
        }

        Logger::LogInfo(
            "[Platform] ForceFakePlatform sent platform=" + std::string(platformName) +
            " source=" + backendSource.source);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[Platform] ForceFakePlatform exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[Platform] ForceFakePlatform unknown exception");
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

static bool BuildSingleRankArray(SDK::TAllocatedArray<SDK::int32>& ranks, int32_t rank)
{
    if (rank <= 0)
        return true;

    return ranks.Add(static_cast<SDK::int32>(rank));
}

static void LogLicenseClaimTestFile(const std::string& message)
{
    CreateDirectoryA("C:\\Temp", nullptr);

    std::ofstream logFile("C:\\Temp\\license_claim_test.log", std::ios::out | std::ios::app);
    if (!logFile.is_open())
        return;

    logFile << "tick=" << GetTickCount64() << " " << message << "\n";
}

struct LicenseClaimBackendSource
{
    SDK::UBackendSubsystem* backendSubsystem = nullptr;
    int32_t scannedObjects = 0;
    int32_t backendCandidates = 0;
    const char* source = "none";
};

static LicenseClaimBackendSource ResolveRuntimeBackendSubsystemForClaimTest()
{
    LicenseClaimBackendSource result{};

    SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
    int32_t objectCount = 0;
    if (!IsValidPointer(gObjects) || !TryGetObjectArrayCountSafe(gObjects, objectCount))
        return result;

    if (objectCount <= 0 || objectCount > 2000000)
        return result;

    SDK::UClass* backendClass = SDK::UBackendSubsystem::StaticClass();
    if (!IsValidPointer(backendClass))
        return result;

    result.scannedObjects = objectCount;
    for (int32_t i = 0; i < objectCount; ++i)
    {
        SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
        if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
            continue;

        if (!IsObjectAUnsafeGuarded(obj, backendClass))
            continue;

        ++result.backendCandidates;
        result.backendSubsystem = static_cast<SDK::UBackendSubsystem*>(obj);
        result.source = "gobjects_runtime_backend";
        return result;
    }

    return result;
}

static bool CallReceiveLicenseSafe(
    SDK::UBackendSubsystem* backendSubsystem,
    int32_t seasonCode,
    SDK::TArray<SDK::int32> freeRanks,
    SDK::TArray<SDK::int32> premiumRanks,
    SDK::int32& requestId)
{
    if (!IsValidPointer(backendSubsystem))
        return false;

    __try
    {
        requestId = backendSubsystem->ReceiveLicense(
            static_cast<SDK::int32>(seasonCode),
            freeRanks,
            premiumRanks);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        requestId = -1;
        return false;
    }
}

static bool CallReceiveSpecialLicenseSafe(
    SDK::UBackendSubsystem* backendSubsystem,
    SDK::TArray<SDK::int32> ranks,
    SDK::int32& requestId)
{
    if (!IsValidPointer(backendSubsystem))
        return false;

    __try
    {
        requestId = backendSubsystem->ReceiveSpecialLicense(ranks);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        requestId = -1;
        return false;
    }
}

bool InGameHack_ReceiveLicenseClaimTest(int32_t seasonCode, int32_t freeRank, int32_t premiumRank, int32_t repeatCount)
{
    try
    {
        if (seasonCode <= 0)
        {
            Logger::LogError("[ReceiveLicenseTest] Invalid seasonCode: " + std::to_string(seasonCode));
            LogLicenseClaimTestFile("[ReceiveLicenseTest] invalid seasonCode=" + std::to_string(seasonCode));
            return false;
        }

        if (freeRank <= 0 && premiumRank <= 0)
        {
            Logger::LogError("[ReceiveLicenseTest] At least one rank must be positive");
            LogLicenseClaimTestFile("[ReceiveLicenseTest] no positive ranks");
            return false;
        }

        repeatCount = (std::max)(1, (std::min)(repeatCount, 20));

        LicenseClaimBackendSource backendSource = ResolveRuntimeBackendSubsystemForClaimTest();
        SDK::UBackendSubsystem* backendSubsystem = backendSource.backendSubsystem;
        LogLicenseClaimTestFile(
            "[ReceiveLicenseTest] start backend=0x" + std::to_string(reinterpret_cast<uintptr_t>(backendSubsystem)) +
            " source=" + backendSource.source +
            " scannedObjects=" + std::to_string(backendSource.scannedObjects) +
            " backendCandidates=" + std::to_string(backendSource.backendCandidates) +
            " seasonCode=" + std::to_string(seasonCode) +
            " freeRank=" + std::to_string(freeRank) +
            " premiumRank=" + std::to_string(premiumRank) +
            " repeatCount=" + std::to_string(repeatCount));

        if (!backendSubsystem)
        {
            Logger::LogError("[ReceiveLicenseTest] Could not get runtime BackendSubsystem");
            LogLicenseClaimTestFile("[ReceiveLicenseTest] abort no runtime BackendSubsystem");
            return false;
        }

        bool sentAny = false;
        for (int i = 0; i < repeatCount; ++i)
        {
            SDK::TAllocatedArray<SDK::int32> freeRanks(freeRank > 0 ? 1 : 0);
            SDK::TAllocatedArray<SDK::int32> premiumRanks(premiumRank > 0 ? 1 : 0);

            if (!BuildSingleRankArray(freeRanks, freeRank) || !BuildSingleRankArray(premiumRanks, premiumRank))
            {
                Logger::LogError("[ReceiveLicenseTest] Failed to build rank arrays");
                LogLicenseClaimTestFile("[ReceiveLicenseTest] failed building rank arrays repeat=" + std::to_string(i + 1));
                return sentAny;
            }

            SDK::int32 requestId = -1;
            const bool callOk = CallReceiveLicenseSafe(
                backendSubsystem,
                seasonCode,
                static_cast<SDK::TArray<SDK::int32>>(freeRanks),
                static_cast<SDK::TArray<SDK::int32>>(premiumRanks),
                requestId);

            std::string line =
                "[ReceiveLicenseTest] repeat=" + std::to_string(i + 1) + "/" + std::to_string(repeatCount) +
                ", seasonCode=" + std::to_string(seasonCode) +
                ", freeRank=" + std::to_string(freeRank) +
                ", premiumRank=" + std::to_string(premiumRank) +
                ", callOk=" + std::to_string(callOk ? 1 : 0) +
                ", requestId=" + std::to_string(requestId);
            Logger::LogInfo(line);
            LogLicenseClaimTestFile(line);

            if (requestId > 0)
                sentAny = true;

            if (!callOk)
                return sentAny;
        }

        return sentAny;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ReceiveLicenseTest] Exception: " + std::string(e.what()));
        LogLicenseClaimTestFile("[ReceiveLicenseTest] exception=" + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ReceiveLicenseTest] Unknown exception");
        LogLicenseClaimTestFile("[ReceiveLicenseTest] unknown exception");
        return false;
    }
}

bool InGameHack_ReceiveSpecialLicenseClaimTest(int32_t rank, int32_t repeatCount)
{
    try
    {
        if (rank <= 0)
        {
            Logger::LogError("[ReceiveSpecialLicenseTest] Invalid rank: " + std::to_string(rank));
            LogLicenseClaimTestFile("[ReceiveSpecialLicenseTest] invalid rank=" + std::to_string(rank));
            return false;
        }

        repeatCount = (std::max)(1, (std::min)(repeatCount, 20));

        LicenseClaimBackendSource backendSource = ResolveRuntimeBackendSubsystemForClaimTest();
        SDK::UBackendSubsystem* backendSubsystem = backendSource.backendSubsystem;
        LogLicenseClaimTestFile(
            "[ReceiveSpecialLicenseTest] start backend=0x" + std::to_string(reinterpret_cast<uintptr_t>(backendSubsystem)) +
            " source=" + backendSource.source +
            " scannedObjects=" + std::to_string(backendSource.scannedObjects) +
            " backendCandidates=" + std::to_string(backendSource.backendCandidates) +
            " rank=" + std::to_string(rank) +
            " repeatCount=" + std::to_string(repeatCount));

        if (!backendSubsystem)
        {
            Logger::LogError("[ReceiveSpecialLicenseTest] Could not get runtime BackendSubsystem");
            LogLicenseClaimTestFile("[ReceiveSpecialLicenseTest] abort no runtime BackendSubsystem");
            return false;
        }

        bool sentAny = false;
        for (int i = 0; i < repeatCount; ++i)
        {
            SDK::TAllocatedArray<SDK::int32> ranks(1);
            if (!BuildSingleRankArray(ranks, rank))
            {
                Logger::LogError("[ReceiveSpecialLicenseTest] Failed to build rank array");
                LogLicenseClaimTestFile("[ReceiveSpecialLicenseTest] failed building rank array repeat=" + std::to_string(i + 1));
                return sentAny;
            }

            SDK::int32 requestId = -1;
            const bool callOk = CallReceiveSpecialLicenseSafe(
                backendSubsystem,
                static_cast<SDK::TArray<SDK::int32>>(ranks),
                requestId);

            std::string line =
                "[ReceiveSpecialLicenseTest] repeat=" + std::to_string(i + 1) + "/" + std::to_string(repeatCount) +
                ", rank=" + std::to_string(rank) +
                ", callOk=" + std::to_string(callOk ? 1 : 0) +
                ", requestId=" + std::to_string(requestId);
            Logger::LogInfo(line);
            LogLicenseClaimTestFile(line);

            if (requestId > 0)
                sentAny = true;

            if (!callOk)
                return sentAny;
        }

        return sentAny;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ReceiveSpecialLicenseTest] Exception: " + std::string(e.what()));
        LogLicenseClaimTestFile("[ReceiveSpecialLicenseTest] exception=" + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ReceiveSpecialLicenseTest] Unknown exception");
        LogLicenseClaimTestFile("[ReceiveSpecialLicenseTest] unknown exception");
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

static void AppendProjectileGenerateRepDump(
    std::ostream& out,
    const SDK::FProjectileGenerateRep& rep,
    int slotIndex)
{
    out << "slot=" << slotIndex
        << " parent=0x" << std::hex << reinterpret_cast<uintptr_t>(rep.Parent)
        << " parentComponent=0x" << reinterpret_cast<uintptr_t>(rep.ParentComponent)
        << std::dec
        << " generatorDataID=" << rep.generatorDataID
        << " generatorDataIndexID=" << rep.generatorDataIndexID
        << " genID=" << rep.genID
        << " serialID=" << rep.serialID
        << " charaId=" << static_cast<int>(rep.charaId)
        << " AttachType=" << static_cast<int>(rep.AttachType)
        << " Level=" << static_cast<int>(rep.Level)
        << " variationNo=" << static_cast<int>(rep.variationNo)
        << " commandId=" << static_cast<int>(rep.commandId)
        << " attackId=" << static_cast<int>(rep.attackId)
        << " conditionID=" << static_cast<int>(rep.conditionID)
        << " attackSerial=" << static_cast<int>(rep.attackSerial)
        << " overrideDamageAdjustAdd=" << static_cast<int>(rep.overrideDamageAdjustAdd)
        << " overrideJsonIDX=" << static_cast<int>(rep.overrideJsonIDX)
        << " attachTime=" << rep.attachTime
        << " socketIdx=" << rep.socketIdx
        << "\n";

    out << "  targetLocate=("
        << rep.targetLocate.X << ", "
        << rep.targetLocate.Y << ", "
        << rep.targetLocate.Z << ")"
        << " initLocate=("
        << rep.initLocate.X << ", "
        << rep.initLocate.Y << ", "
        << rep.initLocate.Z << ")"
        << " initScale=("
        << rep.initScale.X << ", "
        << rep.initScale.Y << ", "
        << rep.initScale.Z << ")"
        << " initQuat=("
        << rep.initQuat.Pitch << ", "
        << rep.initQuat.Yaw << ", "
        << rep.initQuat.Roll << ")"
        << "\n";
}

static bool ReadProjectileGenerateRepRawSafe(
    const SDK::FProjectileGenerateRep* source,
    SDK::FProjectileGenerateRep& out)
{
    if (!SafeMemory::IsReadable(source, sizeof(SDK::FProjectileGenerateRep)))
        return false;

    __try
    {
        std::memcpy(&out, source, sizeof(SDK::FProjectileGenerateRep));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool InGameHack_DumpLastProjectileGenerateRep()
{
    try
    {
        SDK::APlayerController* playerController = SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[ProjectileRepDump] No player controller found");
            return false;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsValidPointer(playerChar))
        {
            Logger::LogError("[ProjectileRepDump] No player character found");
            return false;
        }

        SDK::UProjectileReplicateBattleComponent* projectileComp = GetProjectileReplicatorSafe(playerChar);
        if (!IsValidPointer(projectileComp))
        {
            Logger::LogError("[ProjectileRepDump] No projectile replicator component found");
            return false;
        }

        SDK::FProjectileGenerateRep reps[2]{};
        for (int i = 0; i < 2; ++i)
        {
            if (!ReadProjectileGenerateRepRawSafe(&projectileComp->_createGenerateRep[i], reps[i]))
            {
                Logger::LogError("[ProjectileRepDump] Failed to read _createGenerateRep slot " + std::to_string(i));
                return false;
            }
        }

        CreateDirectoryA("C:\\Temp", nullptr);
        std::ofstream logFile("C:\\Temp\\projectile_rep_debug.log", std::ios::out | std::ios::app);
        if (!logFile.is_open())
        {
            Logger::LogError("[ProjectileRepDump] Could not open C:\\Temp\\projectile_rep_debug.log");
            return false;
        }

        SDK::FVector_NetQuantize100 playerLocation = GetActorLocationNetQuantize100Safe(playerChar);

        logFile << "tick=" << GetTickCount64()
                << " playerChar=0x" << std::hex << reinterpret_cast<uintptr_t>(playerChar)
                << " projectileComp=0x" << reinterpret_cast<uintptr_t>(projectileComp)
                << std::dec
                << " playerLocation=("
                << playerLocation.X << ", "
                << playerLocation.Y << ", "
                << playerLocation.Z << ")"
                << "\n";

        AppendProjectileGenerateRepDump(logFile, reps[0], 0);
        AppendProjectileGenerateRepDump(logFile, reps[1], 1);
        logFile << "\n";

        Logger::LogInfo("[ProjectileRepDump] Wrote C:\\Temp\\projectile_rep_debug.log");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ProjectileRepDump] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ProjectileRepDump] Unknown exception");
        return false;
    }
}

static const char* ProjectileGeneratorIdName(SDK::EProjectileGeneratorID value)
{
    switch (value)
    {
    case SDK::EProjectileGeneratorID::__NONE__: return "__NONE__";
    case SDK::EProjectileGeneratorID::__COMMON__: return "__COMMON__";
    case SDK::EProjectileGeneratorID::AttackLow: return "AttackLow";
    case SDK::EProjectileGeneratorID::AttackHigh: return "AttackHigh";
    case SDK::EProjectileGeneratorID::Shot: return "Shot";
    case SDK::EProjectileGeneratorID::Test: return "Test";
    case SDK::EProjectileGeneratorID::__DEKU__: return "__DEKU__";
    default: return "Unknown";
    }
}

static bool GetActorLocationRawSafe(SDK::AActor* actor, SDK::FVector& out)
{
    if (!IsValidPointer(actor))
        return false;

    __try
    {
        out = actor->K2_GetActorLocation();
        return IsFiniteVector(out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out = SDK::FVector(0.0f, 0.0f, 0.0f);
        return false;
    }
}

static bool GetActorVelocityRawSafe(SDK::AActor* actor, SDK::FVector& out)
{
    if (!IsValidPointer(actor))
        return false;

    __try
    {
        out = actor->GetVelocity();
        return IsFiniteVector(out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out = SDK::FVector(0.0f, 0.0f, 0.0f);
        return false;
    }
}

static bool GetGeneratorSpawnBulletCountRawSafe(SDK::AProjectileGeneratorBattle* generator, int32_t& out)
{
    if (!IsValidPointer(generator))
        return false;

    __try
    {
        out = static_cast<int32_t>(generator->BP_GetSpawnBulletCount());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out = -1;
        return false;
    }
}

static bool GetBulletAttackIdsRawSafe(
    SDK::ABullet* bullet,
    SDK::ECharacterId& characterId,
    SDK::ECommandId& commandId,
    SDK::EAttackId& attackId)
{
    if (!IsValidPointer(bullet))
        return false;

    __try
    {
        characterId = bullet->GetCharacterID();
        commandId = bullet->GetCommandID();
        attackId = bullet->GetAttackId();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        characterId = SDK::ECharacterId::UNDEF;
        commandId = SDK::ECommandId::NONE;
        attackId = SDK::EAttackId::NONE;
        return false;
    }
}

static std::string SafeFNameDebugString(const SDK::FName& name)
{
    std::stringstream ss;
    ss << "idx=" << name.ComparisonIndex << ",num=" << name.Number;

    if (!name.IsNone())
    {
        try
        {
            const std::string resolvedName = name.ToString();
            if (!resolvedName.empty())
                ss << ",str=" << resolvedName;
        }
        catch (...)
        {
            ss << ",str=unreadable";
        }
    }

    return ss.str();
}

static void AppendVectorDump(std::ostream& out, const char* label, const SDK::FVector& value)
{
    out << " " << label << "=(" << value.X << ", " << value.Y << ", " << value.Z << ")";
}

static void AppendProjectileBulletRuntimeDump(
    std::ostream& out,
    SDK::ABullet* bullet,
    SDK::ACharacterBattle* playerChar,
    int index,
    const char* source)
{
    if (!IsValidPointer(bullet))
    {
        out << source << "[" << index << "] bullet=null\n";
        return;
    }

    std::string className;
    GetClassNameSafe(bullet, className);

    SDK::FVector location{};
    const bool hasLocation = GetActorLocationRawSafe(bullet, location);

    SDK::FVector velocity{};
    const bool hasVelocity = GetActorVelocityRawSafe(bullet, velocity);

    SDK::ACharacterBattle* ownerChr = nullptr;
    SDK::ACharacterBattle* baseOwnerChr = nullptr;
    SDK::AProjectileGeneratorBattle* ownerGen = nullptr;
    SafeReadMember(&bullet->_ownerChr, ownerChr);
    SafeReadMember(&bullet->_baseOwnerChr, baseOwnerChr);
    SafeReadMember(&bullet->_ownerGen, ownerGen);

    SDK::ECharacterId characterId{};
    SDK::ECommandId commandId{};
    SDK::EAttackId attackId{};
    const bool hasIds = GetBulletAttackIdsRawSafe(bullet, characterId, commandId, attackId);

    out << source << "[" << index << "]"
        << " bullet=0x" << std::hex << reinterpret_cast<uintptr_t>(bullet)
        << " class=" << className
        << " ownerChr=0x" << reinterpret_cast<uintptr_t>(ownerChr)
        << " baseOwnerChr=0x" << reinterpret_cast<uintptr_t>(baseOwnerChr)
        << " ownerGen=0x" << reinterpret_cast<uintptr_t>(ownerGen)
        << std::dec
        << " ownerIsLocal=" << (ownerChr == playerChar ? 1 : 0)
        << " baseOwnerIsLocal=" << (baseOwnerChr == playerChar ? 1 : 0);

    if (hasIds)
    {
        out << " charaId=" << static_cast<int>(characterId)
            << " commandId=" << static_cast<int>(commandId)
            << " attackId=" << static_cast<int>(attackId);
    }
    else
    {
        out << " ids=unreadable";
    }

    if (hasLocation)
        AppendVectorDump(out, "loc", location);
    else
        out << " loc=unreadable";

    if (hasVelocity)
        AppendVectorDump(out, "vel", velocity);
    else
        out << " vel=unreadable";

    out << "\n";
}

static void AppendProjectileGeneratorRuntimeDump(
    std::ostream& out,
    SDK::AProjectileGeneratorBattle* generator,
    SDK::ACharacterBattle* playerChar,
    int index,
    const char* source)
{
    if (!IsValidPointer(generator))
    {
        out << source << "[" << index << "] generator=null\n";
        return;
    }

    std::string className;
    GetClassNameSafe(generator, className);

    SDK::FVector location{};
    const bool hasLocation = GetActorLocationRawSafe(generator, location);

    SDK::ACharacterBattle* ownerBtl = nullptr;
    SafeReadMember(&generator->_ownerBtl, ownerBtl);

    int32_t bulletArrayCount = -1;
    const bool hasBulletArray = SafeArrayCount(generator->_bulletTbl, bulletArrayCount, 2048);

    int32_t spawnBulletCount = -1;
    const bool hasSpawnBulletCount = GetGeneratorSpawnBulletCountRawSafe(generator, spawnBulletCount);

    out << source << "[" << index << "]"
        << " generator=0x" << std::hex << reinterpret_cast<uintptr_t>(generator)
        << " class=" << className
        << " ownerBtl=0x" << reinterpret_cast<uintptr_t>(ownerBtl)
        << std::dec
        << " ownerIsLocal=" << (ownerBtl == playerChar ? 1 : 0)
        << " bulletArrayCount=" << (hasBulletArray ? bulletArrayCount : -1)
        << " spawnBulletCount=" << (hasSpawnBulletCount ? spawnBulletCount : -1);

    if (hasLocation)
        AppendVectorDump(out, "loc", location);
    else
        out << " loc=unreadable";

    out << "\n";

    if (!hasBulletArray || bulletArrayCount <= 0)
        return;

    constexpr int32_t kMaxBulletsPerGenerator = 12;
    const int32_t bulletDumpCount = bulletArrayCount < kMaxBulletsPerGenerator ? bulletArrayCount : kMaxBulletsPerGenerator;
    for (int32_t bulletIndex = 0; bulletIndex < bulletDumpCount; ++bulletIndex)
    {
        SDK::ABullet* bullet = nullptr;
        if (!SafeArrayGet(generator->_bulletTbl, bulletIndex, bullet, 2048))
        {
            out << "  " << source << "[" << index << "].bullet[" << bulletIndex << "]=unreadable\n";
            continue;
        }

        AppendProjectileBulletRuntimeDump(out, bullet, playerChar, bulletIndex, "  generatorBullet");
    }

    if (bulletArrayCount > bulletDumpCount)
        out << "  generatorBullet_truncated remaining=" << (bulletArrayCount - bulletDumpCount) << "\n";
}

static void AppendProjectileDataAssetRuntimeDump(
    std::ostream& out,
    SDK::UProjectileGeneratorDataAsset* asset,
    int objectIndex,
    int dumpIndex)
{
    if (!IsValidPointer(asset))
        return;

    SDK::ECharacterId characterId{};
    SDK::FName socketName{};
    SDK::EAttachType attachType{};
    float attachTime = 0.0f;
    bool disableNotify = false;

    SafeReadMember(&asset->characterId, characterId);
    SafeReadMember(&asset->_socketName, socketName);
    SafeReadMember(&asset->_attachType, attachType);
    SafeReadMember(&asset->_attachTime, attachTime);
    SafeReadMember(&asset->_disableNotify, disableNotify);

    std::string className;
    GetClassNameSafe(asset, className);

    out << "dataAsset[" << dumpIndex << "]"
        << " objectIndex=" << objectIndex
        << " ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(asset)
        << std::dec
        << " objectName=" << GetObjectNameSafe(asset)
        << " class=" << className
        << " characterId=" << static_cast<int>(characterId)
        << " generatorDataName=" << SafeFStringToString(asset->_generatorDataName)
        << " socketName{" << SafeFNameDebugString(socketName) << "}"
        << " attachType=" << static_cast<int>(attachType)
        << " attachTime=" << attachTime
        << " disableNotify=" << (disableNotify ? 1 : 0)
        << "\n";
}

static void AppendProjectileNotifyRuntimeDump(
    std::ostream& out,
    SDK::UProjectileNotify* notify,
    int objectIndex,
    int dumpIndex)
{
    if (!IsValidPointer(notify))
        return;

    SDK::EProjectileGeneratorID generatorId{};
    SDK::FName socketName{};
    SDK::EAttachType attachType{};
    float attachTime = 0.0f;
    SDK::ECharacterId editCharaId{};
    bool disableNotify = false;

    SafeReadMember(&notify->_generatorID, generatorId);
    SafeReadMember(&notify->_socketName, socketName);
    SafeReadMember(&notify->_attachType, attachType);
    SafeReadMember(&notify->_attachTime, attachTime);
    SafeReadMember(&notify->_editCharaId, editCharaId);
    SafeReadMember(&notify->_disableNotify, disableNotify);

    std::string className;
    GetClassNameSafe(notify, className);

    out << "notify[" << dumpIndex << "]"
        << " objectIndex=" << objectIndex
        << " ptr=0x" << std::hex << reinterpret_cast<uintptr_t>(notify)
        << std::dec
        << " objectName=" << GetObjectNameSafe(notify)
        << " class=" << className
        << " generatorID=" << static_cast<int>(generatorId)
        << "(" << ProjectileGeneratorIdName(generatorId) << ")"
        << " generatorDataName=" << SafeFStringToString(notify->_generatorDataName)
        << " chargeGeneratorDataName=" << SafeFStringToString(notify->_chargeGeneratorDataName)
        << " lockonGeneratorDataName=" << SafeFStringToString(notify->_lockonGeneratorDataName)
        << " socketName{" << SafeFNameDebugString(socketName) << "}"
        << " attachType=" << static_cast<int>(attachType)
        << " attachTime=" << attachTime
        << " editCharaId=" << static_cast<int>(editCharaId)
        << " disableNotify=" << (disableNotify ? 1 : 0)
        << "\n";
}

bool InGameHack_DumpProjectileRuntimeDebug()
{
    try
    {
        SDK::APlayerController* playerController = SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
        {
            Logger::LogError("[ProjectileRuntimeDump] No player controller found");
            return false;
        }

        SDK::ACharacterBattle* playerChar = GetPlayerCharacterBattle(playerController);
        if (!IsValidPointer(playerChar))
        {
            Logger::LogError("[ProjectileRuntimeDump] No player character found");
            return false;
        }

        SDK::UProjectileReplicateBattleComponent* projectileComp = GetProjectileReplicatorSafe(playerChar);

        CreateDirectoryA("C:\\Temp", nullptr);
        std::ofstream logFile("C:\\Temp\\projectile_runtime_debug.log", std::ios::out | std::ios::app);
        if (!logFile.is_open())
        {
            Logger::LogError("[ProjectileRuntimeDump] Could not open C:\\Temp\\projectile_runtime_debug.log");
            return false;
        }

        SDK::FVector_NetQuantize100 playerLocation = GetActorLocationNetQuantize100Safe(playerChar);

        logFile << "\n=== PROJECTILE RUNTIME DUMP ===\n"
                << "tick=" << GetTickCount64()
                << " playerChar=0x" << std::hex << reinterpret_cast<uintptr_t>(playerChar)
                << " projectileComp=0x" << reinterpret_cast<uintptr_t>(projectileComp)
                << std::dec
                << " playerLocation=("
                << playerLocation.X << ", "
                << playerLocation.Y << ", "
                << playerLocation.Z << ")"
                << "\n";

        if (IsValidPointer(projectileComp))
        {
            int32_t genListCount = -1;
            if (SafeArrayCount(projectileComp->_genList, genListCount, 2048))
            {
                logFile << "\n-- local projectileComp._genList count=" << genListCount << " --\n";
                const int32_t dumpCount = genListCount < 48 ? genListCount : 48;
                for (int32_t i = 0; i < dumpCount; ++i)
                {
                    SDK::AProjectileGeneratorBattle* generator = nullptr;
                    if (!SafeArrayGet(projectileComp->_genList, i, generator, 2048))
                    {
                        logFile << "localGenList[" << i << "]=unreadable\n";
                        continue;
                    }
                    AppendProjectileGeneratorRuntimeDump(logFile, generator, playerChar, i, "localGenList");
                }
                if (genListCount > dumpCount)
                    logFile << "localGenList_truncated remaining=" << (genListCount - dumpCount) << "\n";
            }
            else
            {
                logFile << "\n-- local projectileComp._genList unreadable --\n";
            }
        }
        else
        {
            logFile << "\n-- local projectileComp missing --\n";
        }

        int32_t owningGenCount = -1;
        if (SafeArrayCount(playerChar->_owningProjectileGenerators, owningGenCount, 2048))
        {
            logFile << "\n-- player._owningProjectileGenerators count=" << owningGenCount << " --\n";
            const int32_t dumpCount = owningGenCount < 48 ? owningGenCount : 48;
            for (int32_t i = 0; i < dumpCount; ++i)
            {
                SDK::AProjectileGeneratorBattle* generator = nullptr;
                if (!SafeArrayGet(playerChar->_owningProjectileGenerators, i, generator, 2048))
                {
                    logFile << "owningGen[" << i << "]=unreadable\n";
                    continue;
                }
                AppendProjectileGeneratorRuntimeDump(logFile, generator, playerChar, i, "owningGen");
            }
            if (owningGenCount > dumpCount)
                logFile << "owningGen_truncated remaining=" << (owningGenCount - dumpCount) << "\n";
        }
        else
        {
            logFile << "\n-- player._owningProjectileGenerators unreadable --\n";
        }

        int32_t owningBulletCount = -1;
        if (SafeArrayCount(playerChar->_owningBullets, owningBulletCount, 4096))
        {
            logFile << "\n-- player._owningBullets count=" << owningBulletCount << " --\n";
            const int32_t dumpCount = owningBulletCount < 64 ? owningBulletCount : 64;
            for (int32_t i = 0; i < dumpCount; ++i)
            {
                SDK::ABullet* bullet = nullptr;
                if (!SafeArrayGet(playerChar->_owningBullets, i, bullet, 4096))
                {
                    logFile << "owningBullet[" << i << "]=unreadable\n";
                    continue;
                }
                AppendProjectileBulletRuntimeDump(logFile, bullet, playerChar, i, "owningBullet");
            }
            if (owningBulletCount > dumpCount)
                logFile << "owningBullet_truncated remaining=" << (owningBulletCount - dumpCount) << "\n";
        }
        else
        {
            logFile << "\n-- player._owningBullets unreadable --\n";
        }

        SDK::UWorld* world = SDK::UWorld::GetWorld();
        int32_t actorCount = 0;
        int32_t worldGenTotal = 0;
        int32_t worldBulletTotal = 0;
        int32_t worldGenLogged = 0;
        int32_t worldBulletLogged = 0;
        if (GetWorldActorsCountSafe(world, actorCount))
        {
            logFile << "\n-- world actors scan actorCount=" << actorCount << " --\n";
            for (int32_t i = 0; i < actorCount; ++i)
            {
                SDK::AActor* actor = GetWorldActorSafe(world, i);
                if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor))
                    continue;

                if (IsObjectAUnsafeGuarded(actor, SDK::AProjectileGeneratorBattle::StaticClass()))
                {
                    ++worldGenTotal;
                    if (worldGenLogged < 48)
                    {
                        AppendProjectileGeneratorRuntimeDump(logFile, static_cast<SDK::AProjectileGeneratorBattle*>(actor), playerChar, i, "worldGen");
                        ++worldGenLogged;
                    }
                    continue;
                }

                if (IsObjectAUnsafeGuarded(actor, SDK::ABullet::StaticClass()))
                {
                    ++worldBulletTotal;
                    if (worldBulletLogged < 64)
                    {
                        AppendProjectileBulletRuntimeDump(logFile, static_cast<SDK::ABullet*>(actor), playerChar, i, "worldBullet");
                        ++worldBulletLogged;
                    }
                }
            }
            logFile << "worldGenTotal=" << worldGenTotal
                    << " worldGenLogged=" << worldGenLogged
                    << " worldBulletTotal=" << worldBulletTotal
                    << " worldBulletLogged=" << worldBulletLogged
                    << "\n";
        }
        else
        {
            logFile << "\n-- world actors scan unavailable --\n";
        }

        SDK::TUObjectArray* gObjects = SDK::UObject::GObjects.GetTypedPtr();
        int32_t objectCount = 0;
        int32_t dataAssetTotal = 0;
        int32_t dataAssetLogged = 0;
        int32_t notifyTotal = 0;
        int32_t notifyLogged = 0;
        int32_t objectGeneratorTotal = 0;
        int32_t objectGeneratorLogged = 0;
        if (IsValidPointer(gObjects) && TryGetObjectArrayCountSafe(gObjects, objectCount) && objectCount > 0 && objectCount < 2000000)
        {
            logFile << "\n-- GObjects projectile scan objectCount=" << objectCount << " --\n";
            for (int32_t i = 0; i < objectCount; ++i)
            {
                SDK::UObject* obj = GetObjectByIndexSafe(gObjects, i);
                if (!IsValidPointer(obj) || IsObjectDefaultSafe(obj))
                    continue;

                if (IsObjectAUnsafeGuarded(obj, SDK::UProjectileGeneratorDataAsset::StaticClass()))
                {
                    ++dataAssetTotal;
                    if (dataAssetLogged < 160)
                    {
                        AppendProjectileDataAssetRuntimeDump(logFile, static_cast<SDK::UProjectileGeneratorDataAsset*>(obj), i, dataAssetLogged);
                        ++dataAssetLogged;
                    }
                    continue;
                }

                if (IsObjectAUnsafeGuarded(obj, SDK::UProjectileNotify::StaticClass()))
                {
                    ++notifyTotal;
                    if (notifyLogged < 220)
                    {
                        AppendProjectileNotifyRuntimeDump(logFile, static_cast<SDK::UProjectileNotify*>(obj), i, notifyLogged);
                        ++notifyLogged;
                    }
                    continue;
                }

                if (IsObjectAUnsafeGuarded(obj, SDK::AProjectileGeneratorBattle::StaticClass()))
                {
                    ++objectGeneratorTotal;
                    if (objectGeneratorLogged < 80)
                    {
                        AppendProjectileGeneratorRuntimeDump(logFile, static_cast<SDK::AProjectileGeneratorBattle*>(obj), playerChar, i, "gobjectGen");
                        ++objectGeneratorLogged;
                    }
                }
            }

            logFile << "dataAssetTotal=" << dataAssetTotal
                    << " dataAssetLogged=" << dataAssetLogged
                    << " notifyTotal=" << notifyTotal
                    << " notifyLogged=" << notifyLogged
                    << " objectGeneratorTotal=" << objectGeneratorTotal
                    << " objectGeneratorLogged=" << objectGeneratorLogged
                    << "\n";
        }
        else
        {
            logFile << "\n-- GObjects projectile scan unavailable objectCount=" << objectCount << " --\n";
        }

        Logger::LogInfo("[ProjectileRuntimeDump] Wrote C:\\Temp\\projectile_runtime_debug.log");
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::LogError("[ProjectileRuntimeDump] Exception: " + std::string(e.what()));
        return false;
    }
    catch (...)
    {
        Logger::LogError("[ProjectileRuntimeDump] Unknown exception");
        return false;
    }
}

// ============================================================================
// RENTAL TICKET BYPASS - Internal version of zero1 external bypass
//
// Chain: UWorld::GetWorld() -> OwningGameInstance(+0x180) -> +0xF0 -> +0x8 -> +0x6B0
// At final address + 0x1038, write 23 (CharacterRentalPointMAX from MasterDataModule)
//
// NOTE: Offsets 0xF0, 0x8, 0x6B0 are in unresolved padding areas of the SDK dump.
// They navigate through UGameInstance::SubsystemCollection (internal UE4 struct).
// If the rental bypass stops working after a game update, these 3 offsets need
// re-verification via runtime pointer scan.
//
// Per-frame write — no persistence. Must be called every frame while enabled.
// ============================================================================
bool InGameHack_BypassRentalTickets()
{
    // CharacterRentalPointMAX from SDK: MasterDataModule_structs.hpp
    constexpr BYTE RENTAL_POINT_MAX = 23;
    constexpr uintptr_t OFFSET_GAMEINSTANCE_INTERNAL = 0xF0;
    constexpr uintptr_t OFFSET_SUBSYSTEM_ARRAY      = 0x8;
    constexpr uintptr_t OFFSET_RENTAL_DATA          = 0x6B0;
    constexpr uintptr_t OFFSET_TICKET_COUNT         = 0x28 + 0x1010; // = 0x1038

    try
    {
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (!world)
            return false;

        SDK::UHerovsGameInstance* gameInstance = static_cast<SDK::UHerovsGameInstance*>(world->OwningGameInstance);
        if (!gameInstance)
            return false;

        uintptr_t addr = reinterpret_cast<uintptr_t>(gameInstance);

        uintptr_t step1 = *reinterpret_cast<uintptr_t*>(addr + OFFSET_GAMEINSTANCE_INTERNAL);
        if (!step1 || step1 < 0x10000)
            return false;

        uintptr_t step2 = *reinterpret_cast<uintptr_t*>(step1 + OFFSET_SUBSYSTEM_ARRAY);
        if (!step2 || step2 < 0x10000)
            return false;

        uintptr_t step3 = *reinterpret_cast<uintptr_t*>(step2 + OFFSET_RENTAL_DATA);
        if (!step3 || step3 < 0x10000)
            return false;

        *reinterpret_cast<BYTE*>(step3 + OFFSET_TICKET_COUNT) = RENTAL_POINT_MAX;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
