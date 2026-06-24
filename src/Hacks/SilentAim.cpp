/*
    BulletTP / Silent Aim reconstruction file.

    This file is intentionally NOT added to RUGIR-INTERNAL.vcxproj.
    It is a readable reconstruction of the active BulletTP code path currently
    used by the project, with the SDK_ helper implementations it uses.

    Runtime path in the compiled project:
      HackThread.cpp -> InGameHack_RedirectBulletsToNearestEnemy()
      InGameModuleHacks.cpp -> SDK_GetBulletTPFOVTargetWithPosition()
      Basic.cpp -> cached FOV target selection

    It keeps only the active BulletTP redirection flow.
*/

#include "InGameModuleHacks.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CoreUObject_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_structs.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/InGameModule_classes.hpp"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/GameModule_structs.hpp"
#include "../Menu/ImGuiMenu.h"
#include "../Utils/SafeMemory.h"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace SDK;

namespace
{
    constexpr bool RUNTIME_FILE_DEBUG_LOGGING = false;
    constexpr ULONGLONG BATTLE_MODE_CACHE_MS = 250;

    static void WriteRuntimeDebugLog(const char* path, const std::string& message)
    {
        (void)path;
        (void)message;
    }

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
}

// ============================================================================
// SDK_ HELPERS USED BY BULLETTP TARGETING
// Reconstructed from CppSDK/SDK/Basic.cpp.
// ============================================================================

static inline bool SDK_IsReadableMemory(const void* ptr, size_t size = 1)
{
    if (!ptr)
        return false;

    uintptr_t current = reinterpret_cast<uintptr_t>(ptr);
    if (current < 0x10000)
        return false;

    size_t remaining = (size == 0) ? 1 : size;
    while (remaining > 0)
    {
        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &mbi, sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS))
            return false;

        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (regionEnd <= current)
            return false;

        const size_t available = static_cast<size_t>(regionEnd - current);
        const size_t step = (available < remaining) ? available : remaining;
        current += step;
        remaining -= step;
    }

    return true;
}

template <typename T>
static inline bool SDK_TryReadMemory(const void* ptr, T& outValue)
{
    if (!SDK_IsReadableMemory(ptr, sizeof(T)))
        return false;

    std::memcpy(&outValue, ptr, sizeof(T));
    return true;
}

static inline bool SDK_IsReadableActor(AActor* actor, size_t requiredBytes = sizeof(void*))
{
    return SDK_IsReadableMemory(actor, requiredBytes);
}

static inline float SDK_ClampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline bool SDK_IsZeroVector(const FVector& value)
{
    return value.X == 0.0f && value.Y == 0.0f && value.Z == 0.0f;
}

static inline bool SDK_IsFiniteVector(const FVector& value)
{
    return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
}

extern "C" APlayerController* SDK_GetPlayerController()
{
    UWorld* world = UWorld::GetWorld();
    if (world)
        return UGameplayStatics::GetPlayerController(world, 0);
    return nullptr;
}

extern "C" void SDK_GetViewportSize(int32* outX, int32* outY)
{
    APlayerController* pc = SDK_GetPlayerController();
    if (pc)
        pc->GetViewportSize(outX, outY);
}

static inline void SDK_GetOverlayViewportSize(float* outX, float* outY)
{
    float width = 0.0f;
    float height = 0.0f;

    if (ImGui::GetCurrentContext())
    {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        if (displaySize.x > 1.0f && displaySize.y > 1.0f)
        {
            width = displaySize.x;
            height = displaySize.y;
        }
    }

    if (width <= 1.0f || height <= 1.0f)
    {
        int32 viewportX = 0;
        int32 viewportY = 0;
        SDK_GetViewportSize(&viewportX, &viewportY);
        width = static_cast<float>(viewportX);
        height = static_cast<float>(viewportY);
    }

    if (outX) *outX = width;
    if (outY) *outY = height;
}

static inline float SDK_GetEffectiveFovRadius(float radius)
{
    return SDK_ClampFloat(radius, 1.0f, 10000.0f);
}

// ============================================================================
// TARGET CACHE SHAPES USED BY SDK_GetBulletTPFOVTargetWithPosition()
// Reconstructed from Basic.cpp. In the compiled project, g_ActorsForRendering is
// filled by SDK_UpdateActorCache() in Basic.cpp before BulletTP consumes it.
// ============================================================================

struct Bones
{
    ImVec2 Head;
    ImVec2 Neck;
    ImVec2 Spine0;
    ImVec2 Spine1;
    ImVec2 Spine2;
    ImVec2 Spine3;
    ImVec2 LTight;
    ImVec2 LCalf;
    ImVec2 LFoot;
    ImVec2 LToe;
    ImVec2 RTight;
    ImVec2 RCalf;
    ImVec2 RFoot;
    ImVec2 RToe;
    ImVec2 LClavicle;
    ImVec2 LUpperArm;
    ImVec2 LForeArm;
    ImVec2 LHand;
    ImVec2 RClavicle;
    ImVec2 RUpperArm;
    ImVec2 RForeArm;
    ImVec2 RHand;
};

struct CachedActor
{
    AActor* ActorPtr = nullptr;
    std::string ClassName;
    FVector Position = FVector(0, 0, 0);
    bool IsMySelf = false;
    int CharacterID = 0;
    int CostumeCode = -1;
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    float GuardPoint = 0.0f;
    float MaxGuardPoint = 0.0f;
    int TeamId = 255;
    bool IsBot = false;
    bool IsAlly = false;
    uint8 Platform = 0;
    uint8 CitizenType = 0;
    Bones Skeleton{};
    bool HasValidSkeleton = false;
    FVector HeadWorldPosition = FVector(0, 0, 0);
    bool HasValidHeadWorldPosition = false;
};

struct BulletTPTarget
{
    AActor* actorPtr = nullptr;
    std::string className;
    FVector position = FVector(0, 0, 0);
    FVector aimWorldPosition = FVector(0, 0, 0);
    bool isAlly = false;
    ImVec2 headScreenPos = ImVec2(0, 0);
    float distanceToCenter = 0.0f;
    bool isValid = false;
};

static std::vector<CachedActor> g_ActorsForRendering;
static std::mutex g_ActorCacheMutex;
static AActor* g_LastBulletTPTargetPtr = nullptr;

static inline bool SDK_IsChxxxCharacterClassName(const std::string& className)
{
    size_t offset = 0;
    if (className.length() == 6 && className[0] == 'A')
        offset = 1;
    else if (className.length() != 5)
        return false;

    return className[offset] == 'C' &&
           className[offset + 1] == 'h' &&
           std::isdigit(static_cast<unsigned char>(className[offset + 2])) &&
           std::isdigit(static_cast<unsigned char>(className[offset + 3])) &&
           std::isdigit(static_cast<unsigned char>(className[offset + 4]));
}

static inline bool SDK_IsBattleCharacterClassName(const std::string& className)
{
    if (className == "CharacterBattle" || className == "ACharacterBattle")
        return true;

    return SDK_IsChxxxCharacterClassName(className);
}

static BulletTPTarget SelectBulletTPTargetByFOV(
    float fovRadius,
    AActor*& stickyTargetPtr)
{
    BulletTPTarget bestTarget;

    float viewportX = 0.0f;
    float viewportY = 0.0f;
    SDK_GetOverlayViewportSize(&viewportX, &viewportY);
    if (viewportX <= 1.0f || viewportY <= 1.0f)
        return bestTarget;

    const float screenCenterX = viewportX / 2.0f;
    const float screenCenterY = viewportY / 2.0f;
    const float maxDistance = SDK_GetEffectiveFovRadius(fovRadius);
    float closestPriorityDistance = FLT_MAX;

    std::vector<CachedActor> actorsForTargeting;
    {
        std::lock_guard<std::mutex> lock(g_ActorCacheMutex);
        actorsForTargeting = g_ActorsForRendering;
    }

    for (auto& cachedActor : actorsForTargeting)
    {
        if (!SDK_IsReadableActor(cachedActor.ActorPtr))
            continue;
        if (cachedActor.IsMySelf)
            continue;
        if (!SDK_IsBattleCharacterClassName(cachedActor.ClassName))
            continue;
        if (cachedActor.TeamId == 255)
            continue;
        if (cachedActor.IsAlly)
            continue;

        ImVec2 headScreenPos = cachedActor.Skeleton.Head;
        if (!cachedActor.HasValidSkeleton ||
            !cachedActor.HasValidHeadWorldPosition ||
            headScreenPos.x <= 0.0f ||
            headScreenPos.y <= 0.0f)
            continue;

        const FVector framePosition = cachedActor.Position;
        const FVector aimWorldPosition = cachedActor.HeadWorldPosition;

        if (headScreenPos.x < 0.0f || headScreenPos.y < 0.0f ||
            headScreenPos.x > viewportX || headScreenPos.y > viewportY)
            continue;

        const float dx = headScreenPos.x - screenCenterX;
        const float dy = headScreenPos.y - screenCenterY;
        const float distToCenter = std::sqrt((dx * dx) + (dy * dy));
        if (distToCenter > maxDistance)
            continue;

        if (stickyTargetPtr != nullptr &&
            stickyTargetPtr == cachedActor.ActorPtr)
        {
            bestTarget.actorPtr = cachedActor.ActorPtr;
            bestTarget.className = cachedActor.ClassName;
            bestTarget.position = framePosition;
            bestTarget.aimWorldPosition = aimWorldPosition;
            bestTarget.isAlly = cachedActor.IsAlly;
            bestTarget.headScreenPos = headScreenPos;
            bestTarget.distanceToCenter = distToCenter;
            bestTarget.isValid = true;
            break;
        }

        if (distToCenter < closestPriorityDistance)
        {
            closestPriorityDistance = distToCenter;
            bestTarget.actorPtr = cachedActor.ActorPtr;
            bestTarget.className = cachedActor.ClassName;
            bestTarget.position = framePosition;
            bestTarget.aimWorldPosition = aimWorldPosition;
            bestTarget.isAlly = cachedActor.IsAlly;
            bestTarget.headScreenPos = headScreenPos;
            bestTarget.distanceToCenter = distToCenter;
            bestTarget.isValid = true;
        }
    }

    stickyTargetPtr = bestTarget.isValid ? bestTarget.actorPtr : nullptr;
    return bestTarget;
}

static BulletTPTarget SelectBulletTPTarget()
{
    return SelectBulletTPTargetByFOV(
        ImGuiMenu::g_Settings.BulletTP_FOVRadius,
        g_LastBulletTPTargetPtr);
}

extern "C" AActor* SDK_GetBulletTPFOVTargetWithPosition(FVector* outTargetPosition)
{
    if (outTargetPosition)
        *outTargetPosition = FVector(0, 0, 0);

    BulletTPTarget target = SelectBulletTPTarget();
    if (!target.isValid ||
        SDK_IsZeroVector(target.aimWorldPosition) ||
        !SDK_IsFiniteVector(target.aimWorldPosition))
        return nullptr;

    if (outTargetPosition)
        *outTargetPosition = target.aimWorldPosition;

    return target.actorPtr;
}

extern "C" AActor* SDK_GetBulletTPFOVTarget()
{
    return SDK_GetBulletTPFOVTargetWithPosition(nullptr);
}

// ============================================================================
// ACTIVE BULLETTP PATH
// Reconstructed from InGameModuleHacks.cpp.
// ============================================================================

static inline bool TryGetGameModeTypeSafe(AHerovsGameState* gameState, EGameModeType& modeType)
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
        UWorld* world = UWorld::GetWorld();
        if (!SafeMemory::IsReadable(world, sizeof(void*)))
            return updateCache(false);

        AGameStateBase* baseGameState = nullptr;
        if (!SafeMemory::TryRead(&world->GameState, baseGameState))
            return updateCache(false);

        AHerovsGameState* gameState = static_cast<AHerovsGameState*>(baseGameState);
        if (!SafeMemory::IsReadable(gameState, sizeof(void*)))
            return updateCache(false);

        EGameModeType modeType{};
        if (!TryGetGameModeTypeSafe(gameState, modeType))
            return updateCache(false);

        const int modeValue = static_cast<int>(modeType);
        return updateCache(modeValue >= 2 && modeValue <= 9);
    }
    catch (...)
    {
        return updateCache(false);
    }
}

static inline ULevel* GetPersistentLevelSafe(UWorld* world)
{
    if (!IsValidPointer(world))
        return nullptr;

    ULevel* level = nullptr;
    if (!SafeReadMember(&world->PersistentLevel, level) || !IsValidPointer(level))
        return nullptr;

    return level;
}

static inline bool GetWorldActorsCountSafe(UWorld* world, int32_t& count)
{
    ULevel* level = GetPersistentLevelSafe(world);
    return level && SafeArrayCount(level->Actors, count, 50000);
}

static inline AActor* GetWorldActorSafe(UWorld* world, int32_t index)
{
    ULevel* level = GetPersistentLevelSafe(world);
    if (!level)
        return nullptr;

    AActor* actor = nullptr;
    if (!SafeArrayGet(level->Actors, index, actor, 50000) || !IsValidPointer(actor))
        return nullptr;

    return actor;
}

static inline APawn* GetPawnSafe(APlayerController* playerController)
{
    if (!IsValidPointer(playerController))
        return nullptr;

    APawn* pawn = nullptr;
    if (!SafeReadMember(&playerController->Pawn, pawn) || !IsValidPointer(pawn))
        return nullptr;

    return pawn;
}

static inline APlayerState* GetCharacterPlayerStateSafe(ACharacterBattle* character)
{
    if (!IsValidPointer(character))
        return nullptr;

    APlayerState* playerState = nullptr;
    if (!SafeReadMember(&character->PlayerState, playerState) || !IsValidPointer(playerState))
        return nullptr;

    return playerState;
}

static inline bool IsObjectDefaultSafe(UObject* object)
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

static inline bool IsObjectAUnsafeGuarded(UObject* object, UClass* klass)
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

static inline bool GetClassNameSafe(UObject* object, std::string& className)
{
    className = "Unknown";
    if (!IsValidPointer(object))
        return false;

    UClass* objectClass = nullptr;
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

static inline ACharacterBattle* ActorToCharacterBattleSafe(AActor* actor)
{
    if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor))
        return nullptr;

    if (!IsObjectAUnsafeGuarded(actor, ACharacterBattle::StaticClass()))
    {
        std::string className;
        if (!GetClassNameSafe(actor, className) || !IsCharacterBattleClassName(className))
            return nullptr;
    }

    ACharacterBattle* character = static_cast<ACharacterBattle*>(actor);
    if (!IsValidPointer(character) || !GetCharacterPlayerStateSafe(character))
        return nullptr;

    return character;
}

static inline ACharacterBattle* GetPlayerCharacterBattle(APlayerController* playerController)
{
    APawn* pawn = GetPawnSafe(playerController);
    if (!pawn)
        return nullptr;

    ACharacterBattle* character = static_cast<ACharacterBattle*>(pawn);
    if (!IsValidPointer(character))
        return nullptr;

    return character;
}

static inline bool IsBulletMeleeAttack(const std::string& bulletName)
{
    if (bulletName.find("Melee") != std::string::npos) return true;
    if (bulletName.find("Punch") != std::string::npos) return true;
    if (bulletName.find("Slash") != std::string::npos) return true;
    if (bulletName.find("Strike") != std::string::npos) return true;
    if (bulletName.find("Attack") != std::string::npos && bulletName.find("Skill") == std::string::npos) return true;
    return false;
}

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

static inline bool IsBulletGammaSkill(const std::string& bulletName)
{
    if (bulletName.find("Unique3") != std::string::npos) return true;
    if (bulletName.find("Cement3Shot") != std::string::npos) return true;
    if (bulletName.find("BPB202_U3_") != std::string::npos) return true;
    return false;
}

static inline bool IsBulletSpecialSkill(const std::string& bulletName)
{
    if (bulletName.find("BPB_Special_C") != std::string::npos) return true;
    if (bulletName.find("BPB_UniqueSpecial_") != std::string::npos) return true;
    return false;
}

static inline bool IsBulletNonTeleportable(const std::string& bulletName)
{
    if (bulletName.find("Rebuild") != std::string::npos) return true;
    if (bulletName.find("BPB_Rebuild") != std::string::npos) return true;
    return false;
}

static inline bool IsFiniteVector(const FVector& value)
{
    return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
}

static inline float VectorLengthSquared(const FVector& value)
{
    return (value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z);
}

static inline FVector NormalizeVectorSafe(const FVector& value)
{
    const float lengthSquared = VectorLengthSquared(value);
    if (lengthSquared <= 0.0001f || !std::isfinite(lengthSquared))
        return FVector(0.0f, 0.0f, 0.0f);

    const float length = std::sqrt(lengthSquared);
    return FVector(value.X / length, value.Y / length, value.Z / length);
}

static inline FRotator RotatorFromDirection(const FVector& direction)
{
    constexpr float RadToDeg = 57.29577951308232f;
    const float xyDistance = std::sqrt((direction.X * direction.X) + (direction.Y * direction.Y));
    const float pitch = std::atan2(direction.Z, xyDistance) * RadToDeg;
    const float yaw = std::atan2(direction.Y, direction.X) * RadToDeg;

    return FRotator(pitch, yaw, 0.0f);
}

static inline float GetUsefulBulletSpeed(ABullet* bullet)
{
    constexpr float MinBulletTPSpeed = 30000.0f;
    constexpr float MaxBulletTPSpeed = 120000.0f;

    float speed = 0.0f;
    try
    {
        const FVector currentVelocity = bullet->GetVelocity();
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

static inline bool RedirectBulletTowardTarget(ABullet* bullet, const FVector& targetPosition)
{
    if (!IsValidPointer(bullet) || !IsFiniteVector(targetPosition))
        return false;

    FVector bulletPosition = FVector(0.0f, 0.0f, 0.0f);
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

    FVector direction = NormalizeVectorSafe(targetPosition - bulletPosition);
    if (direction.IsZero())
        return false;

    const FRotator targetRotation = RotatorFromDirection(direction);
    const float speed = GetUsefulBulletSpeed(bullet);
    const FVector redirectedVelocity = direction * speed;

    const FVector impactStart = targetPosition - (direction * 120.0f);
    const FVector impactEnd = targetPosition + (direction * 90.0f);

    bool movedNearTarget = false;
    bool sweptThroughTarget = false;

    try
    {
        if (IsObjectAUnsafeGuarded(bullet, ACustomBullet::StaticClass()))
        {
            ACustomBullet* customBullet = static_cast<ACustomBullet*>(bullet);
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

bool InGameHack_RedirectBulletsToNearestEnemy(bool bIncludeAlpha, bool bIncludeBeta, bool bIncludeGamma, bool bIncludeSpecial)
{
    try
    {
        if (!IsValidBattleMode())
            return false;

        UWorld* world = UWorld::GetWorld();
        int32_t actorCount = 0;
        if (!GetWorldActorsCountSafe(world, actorCount))
            return false;

        APlayerController* playerController = SDK_GetPlayerController();
        if (!IsValidPointer(playerController))
            return false;

        ACharacterBattle* playerCharacter = GetPlayerCharacterBattle(playerController);
        if (!playerCharacter)
            return false;

        FVector targetPosition = FVector(0, 0, 0);
        AActor* nearestEnemyPtr = SDK_GetBulletTPFOVTargetWithPosition(&targetPosition);
        if (!nearestEnemyPtr)
            return false;

        ACharacterBattle* nearestEnemy = ActorToCharacterBattleSafe(nearestEnemyPtr);
        if (!nearestEnemy)
            return false;
        (void)nearestEnemy;

        if (targetPosition.X == 0.0f && targetPosition.Y == 0.0f && targetPosition.Z == 0.0f)
            return false;

        int redirectedCount = 0;
        int totalBulletsFound = 0;
        int bulletsFiltered = 0;

        for (int i = 0; i < actorCount; i++)
        {
            AActor* actor = GetWorldActorSafe(world, i);
            if (!IsValidPointer(actor) || IsObjectDefaultSafe(actor) || !IsObjectAUnsafeGuarded(actor, ABullet::StaticClass()))
                continue;

            ABullet* bullet = static_cast<ABullet*>(actor);
            if (!IsValidPointer(bullet))
                continue;

            totalBulletsFound++;

            if (bullet->_ownerChr != playerCharacter)
                continue;

            std::string bulletClassName;
            if (!GetClassNameSafe(bullet, bulletClassName))
                continue;

            if (IsBulletMeleeAttack(bulletClassName))
                continue;

            if (IsBulletNonTeleportable(bulletClassName))
                continue;

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
                continue;

            bulletsFiltered++;

            try
            {
                if (RedirectBulletTowardTarget(bullet, targetPosition))
                    redirectedCount++;
            }
            catch (...)
            {
                continue;
            }
        }

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
