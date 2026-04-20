#include "ActorEnumerator.h"
#include "../Utils/Logger.h"
#include "../../4.27.2-0+++UE4+Release-4.27-HerovsGame/CppSDK/SDK/Engine_classes.hpp"
#include <sstream>
#include <algorithm>
#include <map>
#include <fstream>
#include <Windows.h>

namespace ActorEnumerator
{
	static bool g_initialized = false;
	static int g_previous_actor_count = 0;
	static int g_frame_count = 0;
	static bool g_log_file_initialized = false;

	// Throttling: Enumerate actors only every N frames (adjust for performance)
	static const int g_enumeration_interval = 30;  // Enum every 30 frames (~0.5 sec at 60fps)
	static int g_frames_since_last_enumeration = 0;

	// ===== ACTOR CLASSIFICATION LISTS (from zero1 ActorHelper.cs) =====
	
	// Characters: Ch001-Ch130, Ch500-Ch520, Ch200-Ch202+ SubCharacter (140+ heroes)
	static const std::vector<std::string> g_character_prefixes = {
		"Ch001", "Ch002", "Ch003", "Ch004", "Ch005", "Ch006", "Ch007", "Ch008", "Ch009", "Ch010",
		"Ch011", "Ch012", "Ch013", "Ch014", "Ch015", "Ch016", "Ch017", "Ch018", "Ch019", "Ch020",
		"Ch021", "Ch022", "Ch023", "Ch024", "Ch025", "Ch026", "Ch027", "Ch028", "Ch029", "Ch030",
		"Ch031", "Ch032", "Ch033", "Ch034", "Ch035", "Ch036", "Ch037", "Ch038", "Ch039", "Ch040",
		"Ch041", "Ch042", "Ch043", "Ch044", "Ch045", "Ch046", "Ch047", "Ch048", "Ch049", "Ch050",
		"Ch051", "Ch052", "Ch053", "Ch054", "Ch055", "Ch056", "Ch057", "Ch058", "Ch059", "Ch060",
		"Ch061", "Ch062", "Ch063", "Ch064", "Ch065", "Ch066", "Ch067", "Ch068", "Ch069", "Ch070",
		"Ch071", "Ch072", "Ch073", "Ch074", "Ch075", "Ch076", "Ch077", "Ch078", "Ch079", "Ch080",
		"Ch081", "Ch082", "Ch083", "Ch084", "Ch085", "Ch086", "Ch087", "Ch088", "Ch089", "Ch090",
		"Ch091", "Ch092", "Ch093", "Ch094", "Ch095", "Ch096", "Ch097", "Ch098", "Ch099", "Ch100",
		"Ch101", "Ch102", "Ch103", "Ch104", "Ch105", "Ch106", "Ch107", "Ch108", "Ch109", "Ch110",
		"Ch111", "Ch112", "Ch113", "Ch114", "Ch115", "Ch116", "Ch117", "Ch118", "Ch119", "Ch120",
		"Ch121", "Ch122", "Ch123", "Ch124", "Ch125", "Ch126", "Ch127", "Ch128", "Ch129", "Ch130",
		"Ch500", "Ch501", "Ch502", "Ch503", "Ch504", "Ch505", "Ch506", "Ch507", "Ch508", "Ch509", "Ch510",
		"Ch511", "Ch512", "Ch513", "Ch514", "Ch515", "Ch516", "Ch517", "Ch518", "Ch519", "Ch520",
		"Ch200", "Ch201", "Ch202",
		"CharacterBattle", "SubCharacter"
	};

	// Items: supplies, NPCs, ability cards (exact names from zero1)
	static const std::vector<std::string> g_item_prefixes = {
		"BP_Su001_C", "BP_Su002_C", "BP_Su003_C", "BP_Su004_C", "BP_Su005_C", "BP_Su006_C", "BP_Su007_C", "BP_Su008_C", "BP_Su009_C", "BP_Su010_C",
		"BP_Su011_C", "BP_Su012_C", "BP_Su015_C", "BP_Su016_C", "BP_Su017_C", "BP_Su018_C", "BP_Su020_C", "BP_Su021_C", "BP_Su026_C",
		"NPCCitizen", "BP_St02_NPCBox_GEN_VARIABLE_BP_St02_NPCBox_C_CAT",
		"BP_AbilitySupplyAlpha_C", "BP_AbilitySupplyBeta_C", "BP_AbilitySupplyGamma_C"
	};

	// FogGrab
	static const std::vector<std::string> g_foggrab_types = {
		"BPB_FogGrab_L1", "BPB_FogGrab_L2", "BPB_FogGrab_L3"
	};

	// Lobby/OutGame Characters
	static const std::vector<std::string> g_outgame_types = {
		"CharacterOutGame", "BP_CharacterCustomize", "PL_CharacterCustomize"
	};
	static const std::vector<std::string> g_excluded_actor_types = {
		// 🏮 LIGHTING (excluded 183 actors)
		"PointLight",                    // 132 - Street lights + UI lights
		"SpotLight",                     // 25  - Targeted lighting
		"SkyLight",
		"DirectionalLight",
		"RectLight",                     // 1   - Rectangle lights
		"LightmassImportanceVolume",

		// 🎨 LOBBY DÉCORATION (excluded 114 actors)
		"BP_St_Lobby",                   // Murs, sols, gondolas, moniteurs, lumières mobiles
		"BP_St_Season",                  // Ballons, drapeaux, tapis, couronnes, cartes
		"BP_st01_00",                    // Sky, zone extérieure
		"BP_CommandWheelPresenter",
		"BP_GameStateLobby",
		"BP_PlayerControllerVisualLobby",
		"BP_StagePostProcessManager",
		"BP_SystemMenuPresenterLobby",
		"BP_st_Lobby",
		"BP_st_outgam",
		"BP_st_outgame",
		"BP_st_season",
		"SL_PlayerStatusInfo",
		"SL_St_VisualLobby",
		"CameraActor",

		// 🖼️ HUD/INTERFACE (excluded 15 actors)
		"HUD",
		"HUDBasePresenter",
		"HUDCrossHairsPresenter",
		"HUDDialoguePresenter",
		"HUDGameButtonGuidePresenter",
		"HUDMagazinePresenter",
		"HUDPlayerTagPresenter",
		"HUDStatusPresenter",
		"InGameWidgetCreator",
		"WidgetCreator",
		"Presenter",                     // Generic presenter filter
		"WidgetItemWheel",
		"WidgetMarkerWheel",

		// 💡 ENVIRONNEMENT/RENDU (excluded 35 actors)
		"HISMContainer",                 // 14 - Instanced static mesh containers
		"ExponentialHeightFog",
		"Emitter",
		"ParticleEventManager",
		"DefaultPhysicsVolume",
		"TriggerVolume",
		"VolumetricLightmapDensityVolume",
		"InstancedFoliageActor",
		"PostProcessVolume",             // 15  - Effects zones
		"NavMeshBoundsVolume",
		"SphereReflectionCapture",
		"AtmosphericFog",
		"PoisonMist",                    // 1   - Zone hazard
		"SpawnPoint",                    // 2   - Spawn locations
		"BuoyancyManager",
		"WorldSettings",

		// ⚙️ SYSTÈMES/MANAGERS (non-pertinent pour ESP)
		"AbstractNavData",               // Navigation mesh system
		"GameNetworkManager",            // Network manager
		"GameStateMainMenu",             // Main menu state
		"GameStateLobby",                // Lobby state
		"HerovsGameSession",             // Game session
		"HerovsPlayerState",             // Player state system
		"PL_MainMenu",                   // Level blueprint (main menu)

		// 🎯 GAMEPLAY DÉCORATION/OBSTACLES (à exclure, sauf Items/NPCs)
		// NOTE: BP_Su001-Su018, Su020-Su021, Su026 + NPCCitizen + BP_St02_NPCBox + BP_AbilitySupply* GARDÉS
		"BPB_Unique",                    // Boss/unique objects
		"BPG_CommonShot",                // Common shots
		"BPG_Unique",                    // Unique shots
		"BP_Gi001",                      // Gibs/debris
		"BP_st01_Ver2_OakTree",          // Arbres/décoration
		"CullDistanceVolume",            // LOD culling zones
		"AtomAreaSoundVolume",           // Audio zones
		"BP_St01_RVT_Volume",            // Runtime virtual texture volumes
		"StaticMeshActor",               // Generic static meshes
		"PL_St01",                       // Level blueprint (stage)
		"Actor",                         // Generic actor base class
	};

	// ===== ACTOR CLASSIFICATION FUNCTION =====
	
	static ActorType ClassifyActorType(const std::string& className)
	{
		// Combat Pion Characters
		for (const auto& prefix : g_character_prefixes)
		{
			if (className == prefix || className.find(prefix) != std::string::npos)
			{
				return ActorType::CHARACTER_BATTLE;
			}
		}

		// OutGame/Lobby Characters
		for (const auto& prefix : g_outgame_types)
		{
			if (className == prefix || className.find(prefix) != std::string::npos)
			{
				return ActorType::CHARACTER_OUTGAME;
			}
		}

		// FogGrab (Kurogiri)
		for (const auto& prefix : g_foggrab_types)
		{
			if (className == prefix || className.find(prefix) != std::string::npos)
			{
				return ActorType::FOGGRAB;
			}
		}

		// Items/Supplies
		for (const auto& prefix : g_item_prefixes)
		{
			if (className == prefix || className.find(prefix) != std::string::npos)
			{
				return ActorType::ITEM;
			}
		}

		return ActorType::UNKNOWN;
	}
	static bool IsActorTypeExcluded(const std::string& className)
	{
		for (const auto& excluded : g_excluded_actor_types)
		{
			if (className == excluded || className.find(excluded) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	// File logging for actor enumeration (separate from console)
	static void WriteToActorLog(const std::string& message)
	{
		try
		{
			// Create logs directory if it doesn't exist
			CreateDirectoryA("C:\\temp", nullptr);

			std::ofstream logFile("C:\\temp\\RUGIR_ActorEnumeration.log", std::ios::app);
			if (logFile.is_open())
			{
				logFile << message << "\n";
				logFile.close();
			}
		}
		catch (...)
		{
			// Silently fail if can't write to file
		}
	}

	// Initialize actor enumeration log file
	static void InitializeActorLogFile()
	{
		if (g_log_file_initialized) return;

		try
		{
			CreateDirectoryA("C:\\temp", nullptr);

			// Clear or create the log file
			std::ofstream logFile("C:\\temp\\RUGIR_ActorEnumeration.log", std::ios::trunc);
			if (logFile.is_open())
			{
				logFile << "=== Actor Enumeration Log ===\n";
				logFile << "Timestamp: " << __DATE__ << " " << __TIME__ << "\n";
				logFile << "============================================================\n\n";
				logFile.close();
				g_log_file_initialized = true;
			}
		}
		catch (...)
		{
			// Silently fail
		}
	}

	bool Initialize()
	{
		try
		{
			// Initialize actor log file
			InitializeActorLogFile();

			// Vérifier que le SDK est initialized
			if (!SDK::UObject::GObjects)
			{
				Logger::LogWarning("[ActorEnumerator] GObjects not initialized");
				return false;
			}

			SDK::UWorld* World = SDK::UWorld::GetWorld();
			if (!World || !World->PersistentLevel)
			{
				Logger::LogWarning("[ActorEnumerator] World or PersistentLevel not accessible");
				return false;
			}

			g_initialized = true;
			Logger::LogInfo("[ActorEnumerator] ✅ Successfully initialized");
			
			std::stringstream init_msg;
			init_msg << "[INIT] Actor enumeration started at frame 0";
			WriteToActorLog(init_msg.str());

			return true;
		}
		catch (const std::exception& e)
		{
			Logger::LogError(std::string("[ActorEnumerator] Initialization failed: ") + e.what());
			return false;
		}
		catch (...)
		{
			Logger::LogError("[ActorEnumerator] Unknown error during initialization");
			return false;
		}
	}

	std::vector<ActorInfo> EnumerateActors()
	{
		std::vector<ActorInfo> actors;

		try
		{
			if (!g_initialized)
				return actors;

			SDK::UWorld* World = SDK::UWorld::GetWorld();
			if (!World)
				return actors;

			// Direct access to PersistentLevel actors (faster than GetAllActorsOfClass)
			SDK::ULevel* PersistentLevel = World->PersistentLevel;
			if (!PersistentLevel || PersistentLevel->Actors.Num() <= 0)
				return actors;

			const SDK::TArray<SDK::AActor*>& AllActors = PersistentLevel->Actors;

			// Camera position pour calculer la distance
			SDK::FVector CameraLocation = { 0.0f, 0.0f, 0.0f };
			try
			{
				SDK::APlayerController* PC = nullptr;
				if (World->OwningGameInstance && World->OwningGameInstance->LocalPlayers.Num() > 0)
				{
					SDK::ULocalPlayer* LocalPlayer = World->OwningGameInstance->LocalPlayers[0];
					if (LocalPlayer)
						PC = LocalPlayer->PlayerController;
				}

				if (PC && PC->Pawn)
				{
					// Utiliser la position du Pawn comme caméra approximative
					CameraLocation = PC->Pawn->K2_GetActorLocation();
				}
			}
			catch (...) {}

			// Énumération de tous les acteurs via GetAllActorsOfClass
			for (int i = 0; i < AllActors.Num(); i++)
			{
				SDK::AActor* Actor = AllActors[i];
				if (!Actor)
					continue;

				try
				{
					ActorInfo info;
					info.Address = reinterpret_cast<uint64_t>(Actor);

					// Nom
					info.Name = Actor->GetName();

					// Classe
					if (Actor->Class)
					{
						info.ClassName = Actor->Class->GetName();
					}
					else
					{
						info.ClassName = "Unknown";
					}

					// ===== FILTERING: SKIP EXCLUDED ACTOR TYPES =====
					if (IsActorTypeExcluded(info.ClassName))
					{
						continue;
					}

					// Nom complet
					try
					{
						info.FullName = Actor->GetFullName();
					}
					catch (...)
					{
						info.FullName = info.ClassName + " " + info.Name;
					}

					// Visibilité
					try
					{
						if (Actor->Outer) 
						{
							info.IsVisible = true;
						}
					}
					catch (...)
					{
						info.IsVisible = false;
					}

					// Distance
					try
					{
						SDK::FVector ActorLocation = Actor->K2_GetActorLocation();
						float dx = ActorLocation.X - CameraLocation.X;
						float dy = ActorLocation.Y - CameraLocation.Y;
						float dz = ActorLocation.Z - CameraLocation.Z;
						info.Distance = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.01f; // Convert to meters
					}
					catch (...)
					{
						info.Distance = 0.0f;
					}

					// ===== CLASSIFY ACTOR TYPE =====
					info.Type = ClassifyActorType(info.ClassName);

					// ===== EXTRACT CHARACTEROUTGAME SPECIFIC INFO =====
					if (info.Type == ActorType::CHARACTER_OUTGAME)
					{
						info.IsLobbyCharacter = true;
						
						// Extract CharacterOutGame properties using SDK functions
						CharacterOutGameInfo cog_info = GetCharacterOutGameInfo(Actor);
						info.IsMySelf = cog_info.is_mine;
						info.CharacterID = cog_info.character_id;
						info.CostumeCode = cog_info.costume_code;
					}

					actors.push_back(info);
				}
				catch (...)
				{
					// Skip actors that cause errors
					continue;
				}
			}

			return actors;
		}
		catch (const std::exception& e)
		{
			Logger::LogError(std::string("[ActorEnumerator] Enumeration error: ") + e.what());
			return actors;
		}
		catch (...)
		{
			Logger::LogError("[ActorEnumerator] Unknown error during enumeration");
			return actors;
		}
	}

	void LogAllActors(const std::vector<ActorInfo>& actors)
	{
		try
		{
			if (actors.empty())
			{
				std::stringstream ss;
				ss << "[Frame " << g_frame_count << "] No actors to log";
				WriteToActorLog(ss.str());
				return;
			}

			// === CONSOLE OUTPUT (Minimal) ===
			std::stringstream console_msg;
			console_msg << "[ActorEnumerator] Frame " << g_frame_count << ": Found " << actors.size() << " actors";
			Logger::LogInfo(console_msg.str());

			// === FILE OUTPUT (Detailed by ActorType classification) ===
			std::stringstream file_content;

			// Header
			file_content << "\n";
			file_content << "╔═══════════════════════════════════════════════════════════════╗\n";
			file_content << "║  FRAME " << g_frame_count << " - ACTOR ENUMERATION - Total: " << actors.size() << " actors\n";
			file_content << "╠═══════════════════════════════════════════════════════════════╣\n";

			// Group actors by ActorType
			std::map<ActorType, std::vector<ActorInfo>> ActorsByRealType;
			for (const auto& actor : actors)
			{
				ActorsByRealType[actor.Type].push_back(actor);
			}

			// Type names for display
			auto GetActorTypeName = [](ActorType type) -> std::string
			{
				switch (type)
				{
					case ActorType::CHARACTER_BATTLE:   return "CHARACTER_BATTLE";
					case ActorType::CHARACTER_OUTGAME:  return "CHARACTER_OUTGAME";
					case ActorType::FOGGRAB:            return "FOGGRAB";
					case ActorType::ITEM:               return "ITEM";
					case ActorType::UNKNOWN:            return "UNKNOWN";
					case ActorType::OTHER:              return "OTHER";
					default:                            return "???";
				}
			};

			// Log each group by type
			for (const auto& [type, actorsOfType] : ActorsByRealType)
			{
				if (actorsOfType.empty()) continue;

				file_content << "\n[" << GetActorTypeName(type) << "] Count: " << actorsOfType.size() << "\n";

				int SubIndex = 0;
				for (const auto& actor : actorsOfType)
				{
					if (SubIndex >= 5)  // Show first 5 of each type
					{
						if (SubIndex == 5)
						{
							file_content << "   └─ ... and " << (actorsOfType.size() - 5) << " more\n";
						}
						SubIndex++;
						continue;
					}

					file_content << "   ├─ [" << (++SubIndex) << "] " << actor.ClassName << " | " << actor.Name;
					
					// Special info for CharacterOutGame
					if (actor.Type == ActorType::CHARACTER_OUTGAME)
					{
						file_content << " | ID=" << actor.CharacterID << " | Costume=" << actor.CostumeCode
							<< " | IsMine=" << (actor.IsMySelf ? "YES" : "NO");
					}
					
					file_content << " | 0x" << std::hex << actor.Address << std::dec
						<< " | Distance=" << actor.Distance << "m\n";
				}
			}

			// Summary footer
			file_content << "╠═══════════════════════════════════════════════════════════════╣\n";
			file_content << "║  ✅ Logged " << actors.size() << " actors (" << ActorsByRealType.size() << " types)\n";
			file_content << "╚═══════════════════════════════════════════════════════════════╝\n";

			// Write to file
			WriteToActorLog(file_content.str());

			g_previous_actor_count = static_cast<int>(actors.size());
		}
		catch (const std::exception& e)
		{
			Logger::LogError(std::string("[ActorEnumerator] Logging error: ") + e.what());
		}
		catch (...)
		{
			Logger::LogError("[ActorEnumerator] Unknown error during logging");
		}
	}

	int GetActorCount()
	{
		try
		{
			SDK::UWorld* World = SDK::UWorld::GetWorld();
			if (World && World->PersistentLevel)
			{
				return World->PersistentLevel->Actors.Num();
			}
			return 0;
		}
		catch (...)
		{
			return 0;
		}
	}

	void ProcessAndLogActors()
	{
		try
		{
			if (!g_initialized && !Initialize())
			{
				Logger::LogWarning("[ActorEnumerator] Not initialized, skipping enumeration");
				return;
			}

			g_frame_count++;
			g_frames_since_last_enumeration++;

			// THROTTLE: Only enumerate every N frames
			if (g_frames_since_last_enumeration < g_enumeration_interval)
			{
				return;  // Skip enumeration this frame
			}

			// Reset counter
			g_frames_since_last_enumeration = 0;

			// Enumerate (only called every 30 frames now)
			auto actors = EnumerateActors();

			// Log the enumeration
			LogAllActors(actors);
		}
		catch (const std::exception& e)
		{
			Logger::LogError(std::string("[ActorEnumerator] ProcessAndLogActors error: ") + e.what());
		}
		catch (...)
		{
			Logger::LogError("[ActorEnumerator] Unknown error in ProcessAndLogActors");
		}
	}

	void Reset()
	{
		g_initialized = false;
		g_previous_actor_count = 0;
		g_frame_count = 0;
		Logger::LogInfo("[ActorEnumerator] Reset complete");
	}

	// ===== CHARACTEROUTGAME HELPERS =====

	bool IsCharacterOutGame(SDK::AActor* actor)
	{
		try
		{
			if (!actor) return false;

			// Check class name
			if (!actor->Class) return false;

			std::string class_name = actor->Class->GetName();
			return (class_name == "CharacterOutGame" || 
					class_name == "ACharacterOutGame" ||
					class_name == "CharacterAvatarCreate" ||
					class_name == "CharacterCustomize");
		}
		catch (...)
		{
			return false;
		}
	}

	CharacterOutGameInfo GetCharacterOutGameInfo(void* character)
	{
		CharacterOutGameInfo info;

		try
		{
			if (!character) return info;

			// Use the SDK accessor functions from Basic.cpp
			info.is_mine = SDK_GetCharacterOutGameIsMySelf(character);
			info.character_id = SDK_GetCharacterOutGameId(character);
			info.costume_code = SDK_GetCharacterOutGameCostumeCode(character);
		}
		catch (...)
		{
			// Return default info on error
		}

		return info;
	}
}
