#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"
#include "../SDK/SDKESPFunctions.h"

namespace ActorEnumerator
{
	// Énumération des types d'acteurs
	enum class ActorType : uint8_t
	{
		UNKNOWN = 0,
		CHARACTER_BATTLE = 1,        // Ch001-Ch130, Ch500-Ch520, Ch200-Ch202 (héros combattants)
		CHARACTER_OUTGAME = 2,       // CharacterOutGame, BP_CharacterCustomize_C, PL_CharacterCustomize_C (lobby)
		FOGGRAB = 3,                 // BPB_FogGrab_L1/L2/L3_C (Kurogiri fog)
		ITEM = 4,                    // BP_Su001-Su021, NPCCitizen, etc. (items/supplies)
		OTHER = 5                    // Autres acteurs (lights, volumes, etc.)
	};

	// Structure pour stocker les infos d'un acteur
	struct ActorInfo
	{
		uint64_t Address = 0;
		std::string Name;
		std::string ClassName;
		std::string FullName;
		float Distance = 0.0f;
		bool IsVisible = false;
		ActorType Type = ActorType::UNKNOWN;
		
		// CharacterOutGame specific
		bool IsLobbyCharacter = false;
		bool IsMySelf = false;
		int CharacterID = -1;
		int CostumeCode = -1;
	};

	// Initialise l'énumérateur (vérification du SDK)
	bool Initialize();

	// Énumère tous les acteurs du niveau - appelé chaque frame
	std::vector<ActorInfo> EnumerateActors();

	// Log tous les acteurs découverts
	void LogAllActors(const std::vector<ActorInfo>& actors);

	// Obtient le nombre total d'acteurs
	int GetActorCount();

	// Enumère et log les acteurs (fonction complète)
	void ProcessAndLogActors();

	// Resetters pour nettoyer les caches si nécessaire
	void Reset();

	// ===== CHARACTEROUTGAME HELPERS =====

	/**
	 * @brief Check if an actor is an instance of ACharacterOutGame
	 * @param actor Actor pointer to check
	 * @return true if actor is CharacterOutGame or derived class
	 */
	bool IsCharacterOutGame(SDK::AActor* actor);

	/**
	 * @brief Get CharacterOutGame properties for ESP rendering
	 * @param character Pointer to ACharacterOutGame
	 * @return struct with character info (id, is_mine, costume_code)
	 */
	struct CharacterOutGameInfo
	{
		bool is_mine = false;
		int character_id = 0;
		int costume_code = -1;
	};

	CharacterOutGameInfo GetCharacterOutGameInfo(void* character);
}
