#pragma once

/**
 * @file SDKESPFunctions.h
 * @brief Public interface for ESP functions using Dumper-7 pattern
 * 
 * These functions follow the Dumper-7 recommended pattern:
 * - Use SDK function accessors (UWorld::GetWorld, UGameplayStatics, etc.)
 * - Lazy-load values directly (no global cache)
 * - GNames is fallback only (use FName::AppendString)
 * 
 * @note All functions are initialized in Basic.cpp via SDK_RunComprehensiveDiagnostics()
 */

#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"

// ========== ESP INITIALIZATION ==========

/**
 * @brief Initialize and test ESP functions
 * @note Called automatically in Main.cpp during DLL initialization
 * @return true if all ESP functions are working, false if game is not loaded
 */
extern "C" void SDK_RunComprehensiveDiagnostics();

/**
 * @brief Check if ESP system is fully initialized
 * @return true if GObjects, World, PlayerController are all accessible
 */
extern "C" bool SDK_IsESPInitialized();

// ========== ESP ACCESSORS (DUMPER-7 LAZY-LOAD PATTERN) ==========

/**
 * @brief Get the world instance (Dumper-7 recommended way)
 * @return UWorld* to the current game world, nullptr if game not loaded
 * @note Use this instead of GWorld offset from memory
 */
extern "C" SDK::UWorld* SDK_GetWorld();

/**
 * @brief Get the local player controller
 * @return APlayerController* for the local player, nullptr if not found
 * @note Lazy-loads via UGameplayStatics::GetPlayerController()
 */
extern "C" SDK::APlayerController* SDK_GetPlayerController();

/**
 * @brief Get the camera location (player eye position)
 * @return FVector with camera position in world space
 * @note Returns zero vector if camera not available
 */
extern "C" SDK::FVector SDK_GetCameraLocation();

/**
 * @brief Get the viewport size (screen resolution)
 * @param OutX [out] Screen width
 * @param OutY [out] Screen height
 */
extern "C" void SDK_GetViewportSize(SDK::int32* OutX, SDK::int32* OutY);

// ========== ESP PROJECTION FUNCTIONS (FOR DRAWING) ==========

/**
 * @brief Project a world position to screen coordinates (World-to-Screen)
 * 
 * Used for ESP drawing - converts 3D world position to 2D screen position
 * 
 * @param WorldPosition 3D position in world space
 * @param OutScreenPosition [out] 2D position on screen (0,0 = top-left)
 * @return true if projection successful, false if position is behind camera
 * 
 * Example:
 * @code
 * SDK::FVector ActorPos = { 100.0f, 200.0f, 300.0f };
 * SDK::FVector2D ScreenPos = { 0.0f, 0.0f };
 * if (SDK_ProjectWorldToScreen(ActorPos, &ScreenPos)) {
 *     DrawOnScreen(ScreenPos.X, ScreenPos.Y, "Enemy");
 * }
 * @endcode
 */
extern "C" bool SDK_ProjectWorldToScreen(
    const SDK::FVector& WorldPosition, 
    SDK::FVector2D* OutScreenPosition
);

/**
 * @brief Deproject a screen position to world coordinates (Screen-to-World)
 * 
 * Converts 2D screen position to world ray (useful for raycasting)
 * 
 * @param ScreenPosition 2D position on screen (0,0 = top-left)
 * @param OutWorldPosition [out] Starting position of the ray in world space
 * @param OutWorldDirection [out] Direction of the ray (normalized)
 * @return true if deproject successful, false if player controller not found
 * 
 * Example:
 * @code
 * SDK::FVector2D ScreenPos = { 960.0f, 540.0f };  // Center of screen
 * SDK::FVector WorldPos, WorldDir;
 * if (SDK_DeprojectScreenToWorld(ScreenPos, &WorldPos, &WorldDir)) {
 *     // Can now raycast from WorldPos in direction WorldDir
 * }
 * @endcode
 */
extern "C" bool SDK_DeprojectScreenToWorld(
    const SDK::FVector2D& ScreenPosition, 
    SDK::FVector* OutWorldPosition, 
    SDK::FVector* OutWorldDirection
);

// ========== ESP HELPER FUNCTIONS ==========

/**
 * @brief Check if an actor is visible from current camera
 * @param Actor Target actor to check
 * @return true if actor is on screen, false otherwise
 */
extern "C" bool SDK_IsActorVisible(SDK::AActor* Actor);

/**
 * @brief Get distance from camera to target position
 * @param TargetPos Position to measure distance to
 * @return Distance in Unreal units (cm)
 */
extern "C" float SDK_GetDistanceToPosition(const SDK::FVector& TargetPos);

/**
 * @brief Get distance from camera to actor
 * @param Actor Target actor
 * @return Distance in Unreal units (cm)
 */
extern "C" float SDK_GetDistanceToActor(SDK::AActor* Actor);

// ========== CHARACTEROUTGAME FUNCTIONS (OUTGAME ESP) ==========

/**
 * @brief Check if a CharacterOutGame is the local player (Dumper-7 accessor method)
 * @param Character ACharacterOutGame* to check
 * @return true if this is the player's character, false otherwise
 * @note Uses bMySelf member variable
 * 
 * Example:
 * @code
 * ACharacterOutGame* OutGameChar = (ACharacterOutGame*)Actor;
 * if (SDK_GetCharacterOutGameIsMySelf(OutGameChar)) {
 *     // Skip rendering - don't show own character on ESP
 * }
 * @endcode
 */
extern "C" bool SDK_GetCharacterOutGameIsMySelf(void* Character);

/**
 * @brief Get the character ID of a CharacterOutGame (Dumper-7 accessor method)
 * @param Character ACharacterOutGame* to read from
 * @return ECharacterId enum value (character type)
 * @note Reads _outGameCharacterId member variable
 * 
 * Example:
 * @code
 * ECharacterId CharId = SDK_GetCharacterOutGameId(OutGameChar);
 * // Use CharId to lookup character name and display on ESP
 * @endcode
 */
extern "C" int SDK_GetCharacterOutGameId(void* Character);

/**
 * @brief Get the costume code of a CharacterOutGame (Dumper-7 accessor method)
 * @param Character ACharacterOutGame* to read from
 * @return Costume code (int32)
 * @note Reads _outGameCharacterCostumeCode member variable
 * @note Useful for additional character identification
 * 
 * Example:
 * @code
 * int32 CostumeCode = SDK_GetCharacterOutGameCostumeCode(OutGameChar);
 * if (CostumeCode == SKIN_LEGENDARY) {
 *     HighlightCharacter(); // Mark special skins
 * }
 * @endcode
 */
extern "C" int SDK_GetCharacterOutGameCostumeCode(void* Character);

// ========== DEPROJECT TESTING ==========

/**
 * @brief Test DeprojectScreenToWorld conversion with logging
 * 
 * Tests 5 different screen positions (center, corners, sides) and logs:
 * - Camera location
 * - Screen position → World position + direction
 * - Point 1000u along the ray
 * - Round-trip projection (world back to screen)
 * 
 * Output file: C:\temp\RUGIR_DeprojectTest.log
 * 
 * Call this once to verify screen-to-world conversion is working
 */
extern "C" void SDK_TestDeprojectScreenToWorld();

// ========== ESP DRAWING FUNCTIONS ==========

/**
 * @brief Draw a box outline on screen
 * 
 * Uses AHUD::DrawLine to render a rectangular outline
 * 
 * @param ScreenX Screen X coordinate (top-left)
 * @param ScreenY Screen Y coordinate (top-left)
 * @param ScreenW Box width in screen pixels
 * @param ScreenH Box height in screen pixels
 * @param BoxColor RGBA color (0.0-1.0 range)
 * @param Thickness Line thickness in pixels
 * 
 * Example:
 * @code
 * FLinearColor RedBox(1.0f, 0.0f, 0.0f, 1.0f); // Red
 * SDK_DrawESPBox(100.0f, 200.0f, 150.0f, 200.0f, RedBox, 2.0f);
 * @endcode
 */
extern "C" void SDK_DrawESPBox(float ScreenX, float ScreenY, float ScreenW, float ScreenH, 
                               const SDK::FLinearColor& BoxColor, float Thickness);

// Global drawing queue system
/**
 * @brief Clears all queued boxes waiting to be drawn
 */
extern "C" void SDK_ClearBoxesToDraw();

/**
 * @brief Adds a box to the drawing queue (will be drawn when SDK_DrawAllBoxes is called)
 * @param ScreenX Screen X coordinate (top-left corner)
 * @param ScreenY Screen Y coordinate (top-left corner)
 * @param ScreenW Box width in pixels
 * @param ScreenH Box height in pixels
 * @param Color Box outline color (FLinearColor with RGBA)
 * @param Thickness Line thickness in pixels
 */
extern "C" void SDK_AddBoxToDraw(float ScreenX, float ScreenY, float ScreenW, float ScreenH, 
                                 const SDK::FLinearColor& Color, float Thickness);

/**
 * @brief Draws all boxes in the queue using the HUD. Call this when HUD is available (e.g., in HUD::Draw hook).
 */
extern "C" void SDK_DrawAllBoxes();

/**
 * @brief Debug version of SDK_DrawAllBoxes that logs HUD availability
 */
extern "C" void SDK_DrawAllBoxesDebug();

/**
 * @brief Install PostRender hook via VTable hijacking
 * @note Hooks UGameViewportClient::PostRender to receive UCanvas parameter for drawing
 * @return true if hook installed successfully, false otherwise
 */
extern "C" bool SDK_HookPostRender();
