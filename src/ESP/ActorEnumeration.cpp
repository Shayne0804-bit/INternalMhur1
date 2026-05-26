/*
 * ============================================================================
 *  ACTOR ENUMERATION SYSTEM - ESP Module
 *  File: ActorEnumeration.cpp
 *  Description: Enumerate and cache actors from the game world with throttling
 *  Pattern: Double-buffering like zero1 (EntityList + EntityList2)
 * 
 *  SDK PROPERTIES ACCESSED (NO OFFSETS):
 *  ✓ _guardPoint from APlayerStateBattle - Get guard point current value
 *  ✓ _plusUltraPoint from APlayerStateBattle - Get plus ultra current value
 * 
 *  SDK FUNCTIONS IMPLEMENTED (NO OFFSETS):
 *  ✓ BP_GetTeamId() from ACharacterBattle - Get team ID
 *  ✓ BP_IsDying() from APlayerStateBattle - Check dying status
 *  ✓ BP_GetMainHealthMax() from APlayerStateBattle - Get max health
 *  ✓ BP_GetMainHealthRate() from APlayerStateBattle - Get health as 0.0-1.0 ratio
 *  ✓ GetPlayerNameIgnorePrioritySetting() from APlayerStateBattle - Get player name
 *  ✓ OnRep_GuardPoint() from APlayerStateBattle - Called when GuardPoint changes
 *  ✓ OnRep_HealthData() from APlayerStateBattle - Called when HealthData changes
 * 
 *  REMAINING (NO SDK ALTERNATIVE FOUND):
 *  ⚠️ MaxGuardPoint value (max barrier point)
 *  ⚠️ Platform enum (0x364 in PlayerState)
 *  ⚠️ Character ID / Costume Code (CharacterOutGame specific)
 * 
 *  OTHER SDK FUNCTIONS AVAILABLE (USED ELSEWHERE):
 *  - UWorld::GetWorld() - Get world instance
 *  - AActor::K2_GetActorLocation() - Position 3D
 *  - AActor::IsDefaultObject() - Check if default object
 *  - UClass::GetName() - Class name
 *  - UGameplayStatics::ProjectWorldToScreen() - W2S
 *  - USkeletalMeshComponent::GetNumBones() - Bone count
 *  - USkeletalMeshComponent::GetBoneName() - Bone name by index
 * ============================================================================
 */

#include <Windows.h>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

// Include UE4 SDK
#include "../../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/CoreUObject_classes.hpp"
#include "../../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"
#include "../../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Basic.hpp"

// Include Menu Settings
#include "../Menu/ImGuiMenu.h"

// Include Logger
#include "../Utils/Logger.h"

SDK_NAMESPACE_START

/* ============================================================================
   FILE LOGGING UTILITIES
   ============================================================================ */

static std::mutex g_LogFileMutex;
static const char* g_LogFilePath = "C:\\temp\\rugir_esp.log";

/**
 * @brief Write log message to file with timestamp
 * @param message Log message
 */
static void LogToFile(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_LogFileMutex);
    try
    {
        // Create C:\temp directory if it doesn't exist
        CreateDirectoryA("C:\\temp", nullptr);
        
        std::ofstream logFile(g_LogFilePath, std::ios::app);
        if (!logFile.is_open()) return;
        
        // Get current time
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        
        // Format timestamp: [2026-05-05 14:30:45]
        std::ostringstream timestamp;
        timestamp << std::put_time(&timeinfo, "[%Y-%m-%d %H:%M:%S]");
        
        logFile << timestamp.str() << " " << message << "\n";
        logFile.close();
    }
    catch (...)
    {
        // Silently fail - don't interrupt game
    }
}

/* ============================================================================
   ACTOR CACHE STRUCTURES & GLOBALS
   ============================================================================ */

/**
 * @struct CachedActorInfo
 * @brief Lightweight cached actor information (optimized like zero1's Tier 1/2)
 * 
 * Stores only essential data needed for ESP rendering to minimize memory footprint
 * and reduce cache misses during rendering.
 */
struct CachedActorInfo
{
    // Core identification
    AActor* ActorPtr;                  // Pointer to actual actor (for verification)
    std::string ClassName;             // Class name (e.g., "CharacterBattle", "NPCCitizen")
    FVector WorldPosition;             // Actor world position (cached for distance calc)
    
    // Character-specific properties
    uint8 CharacterID;                 // Character ID for lookup table (0-38)
    int32 CostumeCode;                 // Costume code (-1 = unknown)
    
    // Combat statistics (cached, not real-time read)
    float Health;                      // Current HP
    float MaxHealth;                   // Max HP
    float GuardPoint;                  // Current GP (shield)
    float MaxGuardPoint;               // Max GP
    float PlusUltra;                   // Plus Ultra bar value
    
    // Team & Status
    uint8 TeamId;                      // Team ID (0-7, 255=unknown)
    bool IsAlly;                       // Is same team as local player?
    bool IsBot;                        // Is controlled by AI?
    bool IsMySelf;                     // Is this our character?
    bool IsDying;                      // Is in dying state?
    uint8 Platform;                    // Platform enum (PS=1, Xbox=2, PC=3, Switch=4)
    uint8 CitizenType;                 // For NPCCitizen only (4=Kota)
    
    // Skeleton data
    Bones SkeletonData;                // 22 bone 2D screen positions
    bool HasValidSkeleton;             // Is skeleton extraction valid?
};

// Double-buffered actor cache (like zero1's pattern)
static std::vector<CachedActorInfo> g_ActorsProcessing;  // Being filled by enumeration thread
static std::vector<CachedActorInfo> g_ActorsRendering;   // Being read by render thread
static std::mutex g_ActorCacheMutex;                     // Synchronization lock

// Cached local player info
static uint8 g_LocalPlayerTeamId = 255;        // Our team ID (255=unknown)
static AActor* g_LocalPlayerCharacter = nullptr; // Pointer to ourselves

// Enumeration throttling (200ms like zero1 to reduce lag)
static uint64_t g_LastActorEnumerationTime = 0;
static const uint64_t ACTOR_ENUMERATION_THROTTLE_MS = 200;

/* ============================================================================
   TIMING UTILITIES
   ============================================================================ */

/**
 * @brief Get current time in milliseconds (performance counter based)
 * @return Current time in milliseconds
 */
static inline uint64_t GetCurrentTimeMs()
{
    static LARGE_INTEGER frequency = {};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (currentTime.QuadPart * 1000) / frequency.QuadPart;
}

/* ============================================================================
   CHARACTER DATA EXTRACTION HELPERS
   
   PRIORITÉ: Utiliser SDK functions quand possible, offsets en dernier recours
   ============================================================================ */

/**
 * @brief Get actor class name SAFELY via SDK (no offsets)
 * @param Character Actor pointer
 * @return Class name string (safe)
 */
static inline std::string GetActorClassName_Safe(AActor* Character)
{
    if (!Character) return "Unknown";
    try
    {
        // SDK FUNCTION: K2_GetActorLocation() works, Class property works
        if (Character->Class)
            return std::string(TCHAR_TO_UTF8(*Character->Class->GetName()));
    }
    catch (...) {}
    return "Unknown";
}

/**
 * @brief Get actor position SAFELY via SDK (no offsets)
 * @param Character Actor pointer
 * @return World position
 */
static inline FVector GetActorPosition_Safe(AActor* Character)
{
    if (!Character) return FVector{0, 0, 0};
    try
    {
        // SDK FUNCTION: K2_GetActorLocation()
        return Character->K2_GetActorLocation();
    }
    catch (...)
    {
        return FVector{0, 0, 0};
    }
}

/**
 * @brief Get team ID from a character
 * @param Character Actor pointer to read from
 * @return Team ID (0-7) or 255 if unknown
 * 
 * SDK FUNCTION: Uses BP_GetTeamId() from ACharacterBattle/APlayerStateBattle
 * ✓ SAFE: No offsets required
 */
static inline uint8 ExtractCharacterTeamId(AActor* Character)
{
    if (!Character) return 255;
    try
    {
        // Try to cast to ACharacterBattle and call BP_GetTeamId()
        ACharacterBattle* CharBattle = (ACharacterBattle*)Character;
        if (!CharBattle) return 255;
        return CharBattle->BP_GetTeamId();
    }
    catch (...)
    {
        return 255;
    }
}

/**
 * @brief Get health information from character
 * @param Character Actor pointer
 * @return Health value (0.0 if failed)
 * 
 * SDK FUNCTION: Uses BP_GetMainHealthRate() from APlayerStateBattle
 * ✓ SAFE: No offsets required - returns current/max health ratio
 */
static inline float ExtractCharacterHealth(AActor* Character)
{
    if (!Character) return 0.0f;
    try
    {
        // Try to cast to APlayerStateBattle and call BP_GetMainHealthRate()
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return 0.0f;
        
        // Get max health first
        float maxHealth = PlayerState->BP_GetMainHealthMax();
        if (maxHealth <= 0.0f) return 0.0f;
        
        // Get health rate (0.0 to 1.0)
        float healthRate = PlayerState->BP_GetMainHealthRate();
        return healthRate * maxHealth;  // Convert rate to absolute HP
    }
    catch (...)
    {
        return 0.0f;
    }
}

/**
 * @brief Get max health from character
 * @param Character Actor pointer
 * @return Max health value (100.0f default)
 * 
 * SDK FUNCTION: Uses BP_GetMainHealthMax() from APlayerStateBattle
 * ✓ SAFE: No offsets required
 */
static inline float ExtractCharacterMaxHealth(AActor* Character)
{
    if (!Character) return 100.0f;
    try
    {
        // Try to cast to APlayerStateBattle and call BP_GetMainHealthMax()
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return 100.0f;
        
        float maxHealth = PlayerState->BP_GetMainHealthMax();
        return (maxHealth > 0.0f) ? maxHealth : 100.0f;
    }
    catch (...)
    {
        return 100.0f;
    }
}

/**
 * @brief Get guard point (shield) from character
 * @param Character Actor pointer
 * @return Guard point value (0.0 if failed)
 * 
 * SDK PROPERTY: Direct access to _guardPoint member property from APlayerStateBattle
 * ✓ SAFE: No offsets required - direct member access via SDK
 */
static inline float ExtractCharacterGuardPoint(AActor* Character)
{
    if (!Character) return 0.0f;
    try
    {
        // Direct member access via SDK - no offset calculation needed
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return 0.0f;
        
        return PlayerState->_guardPoint;
    }
    catch (...)
    {
        return 0.0f;
    }
}

/**
 * @brief Get max guard point from character
 * @param Character Actor pointer
 * @return Max guard point (50.0f default)
 * 
 * ⚠️ OFFSET-BASED: No SDK getter found for max barrier point
 */
static inline float ExtractCharacterMaxGuardPoint(AActor* Character)
{
    if (!Character) return 50.0f;
    try
    {
        // TODO: Find SDK function for max barrier point
        // For now return constant (typically 50)
        return 50.0f;
    }
    catch (...)
    {
        return 50.0f;
    }
}

/**
 * @brief Get Plus Ultra current value from character
 * @param Character Actor pointer
 * @return Plus Ultra current value (0.0 if none)
 * 
 * SDK PROPERTY: Direct access to _plusUltraPoint member property from APlayerStateBattle
 * ✓ SAFE: No offsets required - direct member access via SDK
 */
static inline float ExtractCharacterPlusUltra(AActor* Character)
{
    if (!Character) return 0.0f;
    try
    {
        // Direct member access via SDK - no offset calculation needed
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return 0.0f;
        
        return PlayerState->_plusUltraPoint;
    }
    catch (...)
    {
        return 0.0f;
    }
}

/**
 * @brief Get dying status from character
 * @param Character Actor pointer
 * @return True if character is downed/dying
 * 
 * SDK FUNCTION: Uses BP_IsDying() from APlayerStateBattle
 * ✓ SAFE: No offsets required
 */
static inline bool ExtractCharacterIsDying(AActor* Character)
{
    if (!Character) return false;
    try
    {
        // Try to cast to APlayerStateBattle and call BP_IsDying()
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return false;
        
        return PlayerState->BP_IsDying();
    }
    catch (...)
    {
        return false;
    }
}

/**
 * @brief Get player name from character
 * @param Character Actor pointer (should be APlayerStateBattle)
 * @return Player name string or empty if failed
 * 
 * SDK FUNCTION: Uses GetPlayerNameIgnorePrioritySetting() from APlayerStateBattle
        // Direct member access via SDK
        APlayerStateBattle* PlayerState = (APlayerStateBattle*)Character;
        if (!PlayerState) return 0;
        
        return (uint8)PlayerState->_platform;
    catch (...)
    {
        return 0;
    }
}

/**
 * @brief Get costume code for CharacterOutGame
 * @param Character Actor pointer
 * @return Costume code (-1 if unknown)
 * 
 * SDK PROPERTY: Direct access to _outGameCharacterCostumeCode from ACharacterOutGame
 * ✓ SAFE: No offsets required - direct member access via SDK
 */
static inline int32 ExtractCharacterOutGameCostumeCode(AActor* Character)
{
    if (!Character) return -1;
    try
    {
        // Direct member access via SDK
        ACharacterOutGame* CharOutGame = (ACharacterOutGame*)Character;
        if (!CharOutGame) return -1;
        
        return CharOutGame->_outGameCharacterCostumeCode;
    }
    catch (...)
    {
        return -1;
    }
}

/**
 * @brief Get citizen type for NPCCitizen
 * @param Character Actor pointer
 * @return Citizen type enum (4=Kota special)
 * 
 * SDK PROPERTY: Direct access to _type member property from ANPCCitizen
 * ✓ SAFE: No offsets required - direct member access via SDK
 */
static inline uint8 ExtractNPCCitizenType(AActor* Character)
{
    if (!Character) return 0;
    try
    {
        // Direct member access via SDK
        ANPCCitizen* Citizen = (ANPCCitizen*)Character;
        if (!Citizen) return 0;
        
        return (uint8)Citizen->_type;
    }
    catch (...)
    {
        return 0;
    }
}

/* ============================================================================
   CLASS NAME PATTERN MATCHING
   ============================================================================ */

/**
 * @brief Check if class name matches "ChXXX" pattern (Ch001-Ch999)
 * @param ClassName Class name to check
 * @return True if matches Ch### pattern
 */
static inline bool IsChxxxPattern(const std::string& ClassName)
{
    if (ClassName.length() < 5) return false;
    
    return (ClassName[0] == 'C' && 
            ClassName[1] == 'h' &&
            std::isdigit(ClassName[2]) &&
            std::isdigit(ClassName[3]) &&
            std::isdigit(ClassName[4]));
}

/**
 * @brief Check if actor is a playable character type
 * @param ClassName Class name to check
 * @return True if character type actor
 */
static inline bool IsCharacterType(const std::string& ClassName)
{
    return (ClassName == "CharacterOutGame" || 
            ClassName == "ACharacterOutGame" ||
            ClassName == "CharacterBattle" || 
            ClassName == "ACharacterBattle" ||
            ClassName == "SubCharacter" ||
            IsChxxxPattern(ClassName));
}

/**
 * @brief Check if actor is an NPC citizen
 * @param ClassName Class name to check
 * @return True if NPC citizen type
 */
static inline bool IsNPCCitizenType(const std::string& ClassName)
{
    return (ClassName == "NPCCitizen");
}

/**
 * @brief Check if actor is a collectible item
 * @param ClassName Class name to check
 * @return True if item type actor (23 different items)
 */
static inline bool IsItemType(const std::string& ClassName)
{
    // 23 item class names from MHUR
    static const std::vector<std::string> ItemClasses = {
        "BP_Su001_C",                  // HP Recovery Drink (Small)
        "BP_Su002_C",                  // HP Recovery Drink (Large)
        "BP_Su003_C",                  // Team HP Recovery Kit
        "BP_Su004_C",                  // GP Recovery Drink (Small)
        "BP_Su005_C",                  // GP Recovery Drink (Large)
        "BP_Su006_C",                  // Team GP Recovery Kit
        "BP_Su007_C",                  // Full Support Drink
        "BP_Su008_C",                  // Item Bag (Small)
        "BP_Su009_C",                  // Item Bag (Medium)
        "BP_Su010_C",                  // Item Bag (Large)
        "BP_Su011_C",                  // Level Up Card
        "BP_Su012_C",                  // Item Box
        "BP_Su015_C",                  // Team Enhancement Kit
        "BP_Su016_C",                  // Item Box (Small)
        "BP_Su017_C",                  // Item Box (Large)
        "BP_Su018_C",                  // Item Box (Gold)
        "BP_Su020_C",                  // Revive Card (1/3)
        "BP_Su021_C",                  // Revive Card (Full)
        "BP_Su026_C",                  // Team Support Drink
        "BP_AbilitySupplyAlpha_C",     // Ability Card (A)
        "BP_AbilitySupplyBeta_C",      // Ability Card (B)
        "BP_AbilitySupplyGamma_C"      // Ability Card (Y)
    };
    
    return std::find(ItemClasses.begin(), ItemClasses.end(), ClassName) != ItemClasses.end();
}

/* ============================================================================
   ACTOR ENUMERATION LOGIC
   ============================================================================ */

/**
 * @brief Enumerate all relevant actors from the game world
 * 
 * Called once every 200ms to avoid per-frame lag. Enumerates:
 * - CharacterOutGame / CharacterBattle / ChXXX (playable characters)
 * - NPCCitizen (special NPCs like Kota)
 * - Items (collectibles)
 * 
 * Uses double-buffering pattern like zero1 for thread-safe rendering
 * without holding locks during render time.
 */
extern "C" void ESP_EnumerateActorsFromWorld()
{
    // Throttle: Only enumerate if 200ms have passed (like zero1)
    uint64_t currentTime = GetCurrentTimeMs();
    if ((currentTime - g_LastActorEnumerationTime) < ACTOR_ENUMERATION_THROTTLE_MS)
    {
        return;  // Reuse cache from last enumeration
    }
    
    g_LastActorEnumerationTime = currentTime;
    
    LogToFile("[ESP_EnumerateActorsFromWorld] Starting actor enumeration");
    
    try
    {
        // STEP 1: Get world
        UWorld* World = UWorld::GetWorld();
        if (!World || !World->PersistentLevel)
        {
            Logger::LogWarning("[ESP_EnumerateActorsFromWorld] World or PersistentLevel is NULL");
            return;
        }
        
        // STEP 2: Prepare temporary lists
        std::vector<CachedActorInfo> newCharacterList;
        newCharacterList.reserve(50);  // Typical: 20-30 characters
        
        // STEP 3: Enumerate all actors in level
        const TArray<AActor*>& AllActors = World->PersistentLevel->Actors;
        int totalActorsChecked = 0;
        int charactersFound = 0;
        int npcsfound = 0;
        int itemsFound = 0;
        
        for (int i = 0; i < AllActors.Num(); i++)
        {
            AActor* Actor = AllActors[i];
            
            // Safety checks
            if (!Actor) continue;
            if (Actor->IsDefaultObject()) continue;
            
            totalActorsChecked++;
            
            // Get class safely
            SDK::UClass* ActorClass = nullptr;
            try
            {
                ActorClass = Actor->Class;
                if (!ActorClass) continue;
            }
            catch (...)
            {
                continue;
            }
            
            // Get class name safely
            std::string ClassName = "";
            try
            {
                ClassName = ActorClass->GetName();
                if (ClassName.empty()) continue;
            }
            catch (...)
            {
                continue;
            }
            
            // STEP 4: Filter by actor type
            bool isCharacter = IsCharacterType(ClassName);
            bool isNPC = IsNPCCitizenType(ClassName);
            bool isItem = IsItemType(ClassName);
            
            if (!isCharacter && !isNPC && !isItem)
                continue;
            
            // STEP 5: Create cached actor entry
            CachedActorInfo cachedActor = {};
            cachedActor.ActorPtr = Actor;
            cachedActor.ClassName = ClassName;
            cachedActor.WorldPosition = Actor->K2_GetActorLocation();
            cachedActor.HasValidSkeleton = false;
            
            // STEP 6: Extract character-specific data
            if (isCharacter)
            {
                charactersFound++;
                
                // Determine if it's our local player
                cachedActor.IsMySelf = false;  // Default false
                
                // For CharacterOutGame (lobby)
                if (ClassName == "CharacterOutGame" || ClassName == "ACharacterOutGame")
                {
                    cachedActor.CharacterID = ExtractCharacterOutGameId(Actor);
                    cachedActor.CostumeCode = ExtractCharacterOutGameCostumeCode(Actor);
                    cachedActor.Health = 0.0f;
                    cachedActor.MaxHealth = 0.0f;
                    cachedActor.GuardPoint = 0.0f;
                    cachedActor.MaxGuardPoint = 0.0f;
                    cachedActor.PlusUltra = 0.0f;
                    cachedActor.TeamId = 255;
                    cachedActor.IsAlly = false;
                    cachedActor.IsBot = false;
                    cachedActor.IsDying = false;
                    cachedActor.Platform = 0;
                }
                // For CharacterBattle (in-game) and ChXXX
                else if (ClassName == "CharacterBattle" || ClassName == "ACharacterBattle" || IsChxxxPattern(ClassName))
                {
                    cachedActor.CharacterID = 0;
                    cachedActor.CostumeCode = -1;
                    cachedActor.Health = ExtractCharacterHealth(Actor);
                    cachedActor.MaxHealth = ExtractCharacterMaxHealth(Actor);
                    cachedActor.GuardPoint = ExtractCharacterGuardPoint(Actor);
                    cachedActor.MaxGuardPoint = ExtractCharacterMaxGuardPoint(Actor);
                    cachedActor.PlusUltra = ExtractCharacterPlusUltra(Actor);
                    cachedActor.TeamId = ExtractCharacterTeamId(Actor);
                    cachedActor.IsDying = ExtractCharacterIsDying(Actor);
                    cachedActor.Platform = ExtractCharacterPlatform(Actor);
                    cachedActor.IsBot = false;  // TODO: Detect if AI controlled
                    
                    // Detect local player (update cached team ID when found)
                    if (cachedActor.TeamId != 255)
                    {
                        g_LocalPlayerTeamId = cachedActor.TeamId;
                        g_LocalPlayerCharacter = Actor;
                    }
                    
                    // Determine if ally (same team)
                    if (g_LocalPlayerTeamId != 255)
                    {
                        cachedActor.IsAlly = (cachedActor.TeamId == g_LocalPlayerTeamId);
                    }
                    else
                    {
                        cachedActor.IsAlly = false;
                    }
                }
                else  // SubCharacter or other character type
                {
                    cachedActor.CharacterID = 0;
                    cachedActor.CostumeCode = -1;
                    cachedActor.Health = ExtractCharacterHealth(Actor);
                    cachedActor.MaxHealth = ExtractCharacterMaxHealth(Actor);
                    cachedActor.GuardPoint = ExtractCharacterGuardPoint(Actor);
                    cachedActor.MaxGuardPoint = ExtractCharacterMaxGuardPoint(Actor);
                    cachedActor.PlusUltra = ExtractCharacterPlusUltra(Actor);
                    cachedActor.TeamId = ExtractCharacterTeamId(Actor);
                    cachedActor.IsDying = ExtractCharacterIsDying(Actor);
                    cachedActor.Platform = ExtractCharacterPlatform(Actor);
                    cachedActor.IsBot = false;
                    cachedActor.IsAlly = (cachedActor.TeamId == g_LocalPlayerTeamId);
                }
            }
            else if (isNPC)
            {
                npcsfound++;
                cachedActor.CharacterID = 0;
                cachedActor.CostumeCode = -1;
                cachedActor.Health = 0.0f;
                cachedActor.MaxHealth = 0.0f;
                cachedActor.GuardPoint = 0.0f;
                cachedActor.MaxGuardPoint = 0.0f;
                cachedActor.PlusUltra = 0.0f;
                cachedActor.TeamId = 255;
                cachedActor.IsAlly = false;
                cachedActor.IsBot = false;
                cachedActor.IsMySelf = false;
                cachedActor.IsDying = false;
                cachedActor.Platform = 0;
                cachedActor.CitizenType = ExtractNPCCitizenType(Actor);
            }
            else if (isItem)
            {
                itemsFound++;
                cachedActor.CharacterID = 0;
                cachedActor.CostumeCode = -1;
                cachedActor.Health = 0.0f;
                cachedActor.MaxHealth = 0.0f;
                cachedActor.GuardPoint = 0.0f;
                cachedActor.MaxGuardPoint = 0.0f;
                cachedActor.PlusUltra = 0.0f;
                cachedActor.TeamId = 255;
                cachedActor.IsAlly = false;
                cachedActor.IsBot = false;
                cachedActor.IsMySelf = false;
                cachedActor.IsDying = false;
                cachedActor.Platform = 0;
                cachedActor.CitizenType = 0;
            }
            
            newCharacterList.push_back(cachedActor);
        }
        
        // STEP 7: Double-buffer swap (thread-safe, minimal lock time)
        {
            std::lock_guard<std::mutex> lock(g_ActorCacheMutex);
            g_ActorsProcessing.swap(newCharacterList);
            g_ActorsRendering.swap(g_ActorsProcessing);
        }
        
        // STEP 8: Log summary to file and console
        {
            char logMsg[512];
            snprintf(logMsg, sizeof(logMsg),
                "[ESP_Enumeration] Checked %d actors | Characters: %d | NPCs: %d | Items: %d | Cache: %zu",
                totalActorsChecked, charactersFound, npcsfound, itemsFound, g_ActorsRendering.size());
            Logger::LogInfo(logMsg);
            LogToFile(logMsg);
            
            // Log detailed character information
            if (charactersFound > 0)
            {
                LogToFile("=== CHARACTER DETAILS ===");
                for (const auto& actor : g_ActorsRendering)
                {
                    if (actor.ClassName.find("Character") != std::string::npos ||
                        actor.ClassName.find("Ch") == 0)
                    {
                        char detailMsg[512];
                        snprintf(detailMsg, sizeof(detailMsg),
                            "Class: %s | Pos: (%.1f, %.1f, %.1f) | HP: %.1f/%.1f | GP: %.1f | PlusUltra: %.1f | Team: %d | Dying: %s | IsAlly: %s",
                            actor.ClassName.c_str(),
                            actor.WorldPosition.X, actor.WorldPosition.Y, actor.WorldPosition.Z,
                            actor.Health, actor.MaxHealth,
                            actor.GuardPoint,
                            actor.PlusUltra,
                            actor.TeamId,
                            actor.IsDying ? "YES" : "NO",
                            actor.IsAlly ? "YES" : "NO");
                        LogToFile(detailMsg);
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::string errorMsg = std::string("[ESP_EnumerateActorsFromWorld] Exception: ") + e.what();
        Logger::LogError(errorMsg);
        LogToFile(errorMsg);
    }
    catch (...)
    {
        LogToFile("[ESP_EnumerateActorsFromWorld] Unknown exception");
        Logger::LogError("[ESP_EnumerateActorsFromWorld] Unknown exception");
    }
}

/* ============================================================================
   PUBLIC API: ACTOR CACHE ACCESS
   ============================================================================ */

/**
 * @brief Get copy of current actor cache for rendering
 * @return Vector of cached actors (safe to iterate)
 */
extern "C" std::vector<CachedActorInfo> ESP_GetCachedActors()
{
    std::lock_guard<std::mutex> lock(g_ActorCacheMutex);
    
    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "[ESP_GetCachedActors] Returning %zu cached actors", g_ActorsRendering.size());
    LogToFile(logMsg);
    
    return g_ActorsRendering;  // Return copy (lock is released immediately after)
}

/**
 * @brief Get local player team ID
 * @return Team ID (0-7) or 255 if unknown
 */
extern "C" uint8 ESP_GetLocalPlayerTeamId()
{
    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "[ESP_GetLocalPlayerTeamId] LocalPlayer TeamId: %d", g_LocalPlayerTeamId);
    LogToFile(logMsg);
    
    return g_LocalPlayerTeamId;
}

/**
 * @brief Get local player character pointer
 * @return Pointer to local player actor or nullptr
 */
extern "C" AActor* ESP_GetLocalPlayerCharacter()
{
    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "[ESP_GetLocalPlayerCharacter] LocalPlayer Actor: %p", g_LocalPlayerCharacter);
    LogToFile(logMsg);
    
    return g_LocalPlayerCharacter;
}

/**
 * @brief Get actor cache size (for debugging)
 * @return Number of cached actors
 */
extern "C" size_t ESP_GetCachedActorCount()
{
    std::lock_guard<std::mutex> lock(g_ActorCacheMutex);
    
    char logMsg[256];
    snprintf(logMsg, sizeof(logMsg), "[ESP_GetCachedActorCount] Cache size: %zu", g_ActorsRendering.size());
    LogToFile(logMsg);
    
    return g_ActorsRendering.size();
}

SDK_NAMESPACE_END

