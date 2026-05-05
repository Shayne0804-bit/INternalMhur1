/*
 * ============================================================================
 *  ACTOR ENUMERATION SYSTEM - Header
 *  File: ActorEnumeration.h
 *  Description: Public API for actor enumeration and caching
 * ============================================================================
 */

#pragma once

#include <vector>
#include <string>

// Forward declare structures
struct Bones;
class AActor;

SDK_NAMESPACE_START

/**
 * @struct CachedActorInfo
 * @brief Lightweight cached actor information for ESP rendering
 */
struct CachedActorInfo
{
    // Core identification
    AActor* ActorPtr;                  // Pointer to actual actor
    std::string ClassName;             // Class name (CharacterBattle, NPCCitizen, etc)
    struct FVector WorldPosition;      // Actor world position
    
    // Character-specific properties
    uint8 CharacterID;                 // Character ID (0-38)
    int32 CostumeCode;                 // Costume code (-1=unknown)
    
    // Combat statistics
    float Health;                      // Current HP
    float MaxHealth;                   // Max HP
    float GuardPoint;                  // Guard Point (shield)
    float MaxGuardPoint;               // Max GP
    float PlusUltra;                   // Plus Ultra value
    
    // Team & Status
    uint8 TeamId;                      // Team ID (0-7, 255=unknown)
    bool IsAlly;                       // Same team as local player?
    bool IsBot;                        // AI controlled?
    bool IsMySelf;                     // Is this our character?
    bool IsDying;                      // In dying state?
    uint8 Platform;                    // Platform enum (PS=1, Xbox=2, PC=3, Switch=4)
    uint8 CitizenType;                 // For NPCCitizen (4=Kota)
    
    // Skeleton data
    Bones SkeletonData;                // 22 bone 2D screen positions
    bool HasValidSkeleton;             // Skeleton is valid?
};

/* ============================================================================
   PUBLIC API DECLARATIONS
   ============================================================================ */

/**
 * @brief Enumerate actors from game world
 * 
 * Called from ESP render loop. Uses throttling (200ms) to avoid per-frame lag.
 * Fills cache with characters, NPCs, and items.
 * 
 * Thread-safe double-buffering: data for rendering is always available
 * in g_ActorsRendering while enumeration fills g_ActorsProcessing.
 */
extern "C" void ESP_EnumerateActorsFromWorld();

/**
 * @brief Get current cached actors for rendering
 * @return Vector of CachedActorInfo (safe copy)
 */
extern "C" std::vector<CachedActorInfo> ESP_GetCachedActors();

/**
 * @brief Get local player team ID
 * @return Team ID (0-7) or 255 if unknown
 */
extern "C" uint8 ESP_GetLocalPlayerTeamId();

/**
 * @brief Get local player character pointer
 * @return AActor* or nullptr
 */
extern "C" AActor* ESP_GetLocalPlayerCharacter();

/**
 * @brief Get number of cached actors
 * @return Actor count
 */
extern "C" size_t ESP_GetCachedActorCount();

SDK_NAMESPACE_END
