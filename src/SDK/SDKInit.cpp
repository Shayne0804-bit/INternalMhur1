#include "SDKInit.h"
#include "../Utils/Logger.h"
#include <sstream>
#include <fstream>
#include <Windows.h>

namespace SDKInit
{
	bool g_SDKInitialized = false;
	static std::vector<std::pair<std::string, UClass*>> g_AvailableClasses; // (ClassName, UClass pointer)

	bool Initialize()
	{
		if (g_SDKInitialized)
		{
			return true;
		}

		Logger::LogInfo("[SDK] Initializing SDK systems...");

		try
		{
			// Get image base - used by SDK for offset calculations
			uintptr_t ImageBase = InSDKUtils::GetImageBase();
			if (!ImageBase)
			{
				Logger::LogError("[SDK] Failed to get process image base!");
				return false;
			}
			
			std::stringstream ss;
			ss << std::hex << ImageBase;
			Logger::LogInfo("[SDK] Image base: 0x" + ss.str());

			// GObjects will be initialized by SDK as needed
			Logger::LogInfo("[SDK] SDK ready for use");

			// Enumerate available character classes for spawn selector
			Logger::LogInfo("[SDK] Enumerating available character classes...");
			EnumerateAvailableCharacterClasses();

			g_SDKInitialized = true;
			Logger::LogInfo("[SDK] SDK initialization complete!");
			return true;
		}
		catch (const std::exception& ex)
		{
			Logger::LogError(std::string("[SDK] Exception: ") + ex.what());
			return false;
		}
		catch (...)
		{
			Logger::LogError("[SDK] Unknown exception!");
			return false;
		}
	}

	bool IsInitialized()
	{
		return g_SDKInitialized;
	}

	void Shutdown()
	{
		// NO LOGGING during shutdown - Logger::LogInfo creates stringstream objects that crash
		g_SDKInitialized = false;
	}

	UWorld* GetGameWorld()
	{
		// Will be implemented when we can safely call UWorld functions
		return nullptr;
	}

	void LogExistingPawns()
	{
		try
		{
			std::ofstream log("c:\\temp\\pawns.log", std::ios::app);
			if (!log.is_open())
			{
				Logger::LogWarning("[SDK] Could not open pawns.log");
				return;
			}

			log << "\n========== [LogExistingPawns] Pawn enumeration ==========\n";

			int32_t PawnCount = 0;
			int32_t TotalObjects = 0;

			// Iterate all objects in GObjects
			for (int32_t i = 0; i < UObject::GObjects->Num(); ++i)
			{
				UObject* Object = UObject::GObjects->GetByIndex(i);
				if (!Object)
					continue;

				TotalObjects++;

			// Check if this object is a Pawn using SDK's HasTypeFlag (like Basic.cpp does)
			if (Object->HasTypeFlag(EClassCastFlags::Pawn))
			{
				PawnCount++;
				
				// Get class name from Class field
				std::string ClassName = "Unknown";
				if (Object->Class)
				{
					ClassName = Object->Class->GetName();
				}
				
				// Log pawn info: Name, Type, Address
				log << "[PAWN #" << PawnCount << "] ";
				log << "Name: " << Object->GetName() << " | ";
				log << "Type: " << ClassName << " | ";
				log << "Address: 0x" << std::hex << reinterpret_cast<uint64_t>(Object) << std::dec << "\n";
			}
			}

			log << "\n[SUMMARY] Total Objects: " << TotalObjects << " | Total Pawns: " << PawnCount << "\n";
			log << "========== [LogExistingPawns] Complete ==========\n\n";
			log.close();

			Logger::LogInfo("[SDK] LogExistingPawns executed - Found " + std::to_string(PawnCount) + " pawns");
		}
		catch (const std::exception& ex)
		{
			Logger::LogError(std::string("[SDK] Exception in LogExistingPawns: ") + ex.what());
		}
		catch (...)
		{
			Logger::LogError("[SDK] Unknown exception in LogExistingPawns");
		}
	}

	APawn* SpawnCharacterBattle(const FVector& SpawnLocation)
	{
		try
		{
			// Log immediately to see if function is called at all
			Logger::LogInfo("[SDK] SpawnCharacterBattle() called!");

			std::ofstream log("c:\\temp\\spawn.log", std::ios::app);
			if (!log.is_open())
			{
				Logger::LogError("[SDK] CRITICAL: Could not open c:\\temp\\spawn.log!");
				return nullptr;
			}

			log << "\n========== [SpawnCharacterBattle] START ==========\n";
			log << "[INFO] Function called at address 0x" << std::hex << reinterpret_cast<uint64_t>(&SpawnCharacterBattle) << std::dec << "\n";

			if (!g_SDKInitialized)
			{
				log << "[ERROR] SDK not initialized! g_SDKInitialized = " << (g_SDKInitialized ? "true" : "false") << "\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] SDK not initialized!");
				return nullptr;
			}

			log << "[INFO] SDK initialized, proceeding with spawn...\n";
			log.flush();

			// Get the world
			UWorld* World = UWorld::GetWorld();
			if (!World)
			{
				log << "[ERROR] UWorld::GetWorld() returned nullptr!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] Failed to get world!");
				return nullptr;
			}
			log << "[INFO] World retrieved successfully at 0x" << std::hex << reinterpret_cast<uint64_t>(World) << std::dec << "\n";
			log.flush();

			// Get the player controller (player index 0)
			APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
			if (!PlayerController)
			{
				log << "[ERROR] GetPlayerController returned nullptr!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] Failed to get player controller!");
				return nullptr;
			}
			log << "[INFO] PlayerController retrieved at 0x" << std::hex << reinterpret_cast<uint64_t>(PlayerController) << std::dec << "\n";
			log.flush();

			// Get the pawn currently possessed by the player (Pawn member at offset 0x0250)
			APawn* PlayerPawn = PlayerController->Pawn;
			if (!PlayerPawn)
			{
				log << "[ERROR] PlayerController->GetPawn() returned nullptr!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] PlayerController has no possessed pawn!");
				return nullptr;
			}
			log << "[INFO] Player pawn retrieved at 0x" << std::hex << reinterpret_cast<uint64_t>(PlayerPawn) << std::dec << "\n";
			log << "[INFO] Player pawn name: " << PlayerPawn->GetName() << "\n";
			log.flush();

			// Get the class of the player's pawn
			UClass* PlayerClass = PlayerPawn->Class;
			if (!PlayerClass)
			{
				log << "[ERROR] PlayerPawn->Class is nullptr!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] Player pawn has no class!");
				return nullptr;
			}
			log << "[INFO] Player pawn class: " << PlayerClass->GetName() << "\n";
			log << "[INFO] Using this class to spawn new pawn at 0x" << std::hex << reinterpret_cast<uint64_t>(PlayerClass) << std::dec << "\n";
			log.flush();

			// Create spawn transform with specified location
			FTransform SpawnTransform;
			SpawnTransform.Translation = SpawnLocation;
			SpawnTransform.Rotation = FQuat(0, 0, 0, 1); // Identity rotation
			SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);
			log << "[INFO] Spawn transform created at location (" << SpawnLocation.X << ", " << SpawnLocation.Y << ", " << SpawnLocation.Z << ")\n";
			log.flush();

			// Spawn actor using deferred spawn
			log << "[INFO] Calling BeginDeferredActorSpawnFromClass with class: " << PlayerClass->GetName() << "\n";
			log.flush();
			
			AActor* SpawnedActor = UGameplayStatics::BeginDeferredActorSpawnFromClass(
				World,
				TSubclassOf<AActor>(PlayerClass),
				SpawnTransform,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
				nullptr  // No owner
			);

			if (!SpawnedActor)
			{
				log << "[ERROR] BeginDeferredActorSpawnFromClass returned nullptr!\n";
				log << "[ERROR] This usually means the class cannot be instantiated (abstract, etc.)\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] BeginDeferredActorSpawnFromClass failed!");
				return nullptr;
			}
			log << "[INFO] Actor spawned (deferred) at 0x" << std::hex << reinterpret_cast<uint64_t>(SpawnedActor) << std::dec << "\n";
			log.flush();

			// Finish the spawn
			log << "[INFO] Calling FinishSpawningActor...\n";
			log.flush();
			
			AActor* FinalActor = UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);
			if (!FinalActor)
			{
				log << "[ERROR] FinishSpawningActor returned nullptr!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] FinishSpawningActor failed!");
				return nullptr;
			}
			log << "[INFO] Actor spawn finished at 0x" << std::hex << reinterpret_cast<uint64_t>(FinalActor) << std::dec << "\n";
			log.flush();

			// Cast to APawn
			APawn* SpawnedPawn = static_cast<APawn*>(FinalActor);
			if (!SpawnedPawn)
			{
				log << "[ERROR] Spawned actor is not a pawn!\n";
				log << "========== [SpawnCharacterBattle] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] Spawned actor is not a pawn!");
				return nullptr;
			}
			log << "[INFO] Successfully cast to APawn: " << FinalActor->GetName() << "\n";
			log.flush();

			// Possess the pawn with the player controller
			if (PlayerController)
			{
				log << "[INFO] Calling PlayerController->Possess()...\n";
				log.flush();
				
				PlayerController->Possess(SpawnedPawn);
				log << "[INFO] Possess() called successfully!\n";
				log.flush();
			}

			log << "[SUCCESS] Character spawn and possession completed!\n";
			log << "[SUCCESS] New pawn: " << SpawnedPawn->GetName() << " | Class: " << PlayerClass->GetName() << "\n";
			log << "========== [SpawnCharacterBattle] SUCCESS ==========\n\n";
			log.close();

			Logger::LogInfo("[SDK] Character spawned successfully - check c:/temp/spawn.log");
			return SpawnedPawn;
		}
		catch (const std::exception& ex)
		{
			Logger::LogError(std::string("[SDK] Exception in SpawnCharacterBattle: ") + ex.what());
			return nullptr;
		}
		catch (...)
		{
			Logger::LogError("[SDK] Unknown exception in SpawnCharacterBattle");
			return nullptr;
		}
	}

	void EnumerateAvailableCharacterClasses()
	{
		Logger::LogInfo("[SDK] Enumerating available character classes...");
		g_AvailableClasses.clear();

		std::ofstream log("c:\\temp\\class_enumeration.log", std::ios::app);
		if (!log.is_open())
		{
			Logger::LogWarning("[SDK] Could not open class_enumeration.log");
			return;
		}

		log << "\n========== [EnumerateAvailableCharacterClasses] START ==========\n";

		try
		{
			int classesFound = 0;
			int classesChecked = 0;

			log << "[DEBUG] Starting enumeration of GObjects\n";
			log << "[DEBUG] GObjects->Num() = " << UObject::GObjects->Num() << "\n";
			log.flush();

			for (int32_t i = 0; i < UObject::GObjects->Num(); ++i)
			{
				UObject* Object = UObject::GObjects->GetByIndex(i);
				if (!Object)
					continue;

				// Check if this is a UClass
				if (Object->Class && Object->Class->GetName() == std::string("Class"))
				{
					classesChecked++;
					std::string ClassName = Object->GetName();

					// Look for character classes (Ch001, Ch008, Ch046, etc)
					// Must NOT be Default__ (class definition template)
					if (ClassName.length() >= 5 && 
						ClassName.substr(0, 2) == "Ch" && 
						ClassName.find("Default__") == std::string::npos &&
						ClassName.find("_C") == std::string::npos)  // Skip Blueprint classes
					{
						// Try to parse class number to validate
						try
						{
							int classNum = std::stoi(ClassName.substr(2));
							if (classNum > 0 && classNum < 10000)  // Reasonable range
							{
								UClass* CharClass = static_cast<UClass*>(Object);
								g_AvailableClasses.push_back({ClassName, CharClass});
								classesFound++;

								log << "[FOUND] Class #" << classesFound << ": " << ClassName << " at 0x" << std::hex << reinterpret_cast<uint64_t>(CharClass) << std::dec << "\n";
								log.flush();

								Logger::LogInfo("[SDK] Found character class: " + ClassName);
							}
						}
						catch (...)
						{
							// Skip invalid class numbers
						}
					}
				}
			}

			log << "[SUMMARY] Classes checked: " << classesChecked << " | Classes found: " << classesFound << "\n";
			log << "========== [EnumerateAvailableCharacterClasses] COMPLETE ==========\n\n";
			log.close();

			Logger::LogInfo("[SDK] Character class enumeration complete - Found " + std::to_string(classesFound) + " classes");
		}
		catch (const std::exception& ex)
		{
			log << "[ERROR] Exception: " << ex.what() << "\n";
			log.close();
			Logger::LogError(std::string("[SDK] Exception in EnumerateAvailableCharacterClasses: ") + ex.what());
		}
		catch (...)
		{
			log << "[ERROR] Unknown exception\n";
			log.close();
			Logger::LogError("[SDK] Unknown exception in EnumerateAvailableCharacterClasses");
		}
	}

	const std::vector<std::string>& GetAvailableCharacterClasses()
	{
		static std::vector<std::string> classNames;
		
		if (classNames.empty() && !g_AvailableClasses.empty())
		{
			for (const auto& pair : g_AvailableClasses)
			{
				classNames.push_back(pair.first);
			}
		}

		return classNames;
	}

	APlayerController* CreateNewPlayerController(UWorld* World)
	{
		std::ofstream log("c:\\temp\\spawn_by_class.log", std::ios::app);
		log << "[DEBUG] Attempting to create new PlayerController...\n";
		log.flush();

		try
		{
			// Get the current PlayerController to know its ID
			APlayerController* CurrentPlayerController = UGameplayStatics::GetPlayerController(World, 0);
			if (CurrentPlayerController)
			{
				log << "[DEBUG] Current PlayerController found at 0x" << std::hex << reinterpret_cast<uint64_t>(CurrentPlayerController) << std::dec << "\n";
				// We can't directly get the ControllerId from the SDK, so we'll use a safe ID like 3
			}
			else
			{
				log << "[DEBUG] No current PlayerController found (using ID 3 as default)\n";
			}

			// Use ControllerId = 3 to avoid conflicts with existing controllers
			int32_t NewControllerId = 3;
			log << "[DEBUG] Creating new PlayerController with ControllerId: " << NewControllerId << "\n";
			log.flush();

			APlayerController* NewPlayerController = UGameplayStatics::CreatePlayer(
				World,
				NewControllerId,
				true  // bSpawnPlayerController = true
			);

			if (!NewPlayerController)
			{
				log << "[ERROR] CreatePlayer() returned nullptr with ControllerId " << NewControllerId << "\n";
				log << "[DEBUG] Trying with ControllerId 2...\n";
				log.flush();

				// Try with ID 2 as fallback
				NewPlayerController = UGameplayStatics::CreatePlayer(
					World,
					2,
					true
				);

				if (!NewPlayerController)
				{
					log << "[ERROR] CreatePlayer() also failed with ControllerId 2\n";
					log.flush();
					return nullptr;
				}
			}

			log << "[SUCCESS] Created new PlayerController at 0x" << std::hex << reinterpret_cast<uint64_t>(NewPlayerController) << std::dec << "\n";
			log.flush();

			return NewPlayerController;
		}
		catch (const std::exception& ex)
		{
			log << "[ERROR] Exception in CreateNewPlayerController: " << ex.what() << "\n";
			log.flush();
			return nullptr;
		}
		catch (...)
		{
			log << "[ERROR] Unknown exception in CreateNewPlayerController\n";
			log.flush();
			return nullptr;
		}
	}

	APawn* FindExistingCharacterBattle()
	{
		std::ofstream log("c:\\temp\\spawn_by_class.log", std::ios::app);
		log << "[DEBUG] Searching for existing CharacterBattle pawn...\n";
		
		try
		{
			for (int32_t i = 0; i < UObject::GObjects->Num(); ++i)
			{
				UObject* Object = UObject::GObjects->GetByIndex(i);
				if (!Object)
					continue;

				std::string ObjectName = Object->GetName();
				
				// Look for CharacterBattle pawns (check name contains "CharacterBattle")
				if (ObjectName.find("CharacterBattle") != std::string::npos && 
					ObjectName.find("Default__") == std::string::npos)
				{
					// Skip UClass objects (the class definitions themselves)
					if (Object->Class && Object->Class->GetName() == std::string("Class"))
						continue;

					// Check if the class name contains CharacterBattle too (type checking)
					if (Object->Class && Object->Class->GetName().find("CharacterBattle") != std::string::npos)
					{
						// This looks like a CharacterBattle pawn instance - cast to APawn
						APawn* Pawn = static_cast<APawn*>(Object);
						if (Pawn)
						{
							log << "[FOUND] CharacterBattle pawn: " << ObjectName << " at 0x" << std::hex << reinterpret_cast<uint64_t>(Pawn) << std::dec << "\n";
							log.close();
							return Pawn;
						}
					}
				}
			}
		}
		catch (...)
		{
			log << "[ERROR] Exception while searching for CharacterBattle\n";
		}

		log << "[WARNING] No existing CharacterBattle pawn found\n";
		log.close();
		return nullptr;
	}

	APawn* SpawnCharacterByClass(const std::string& ClassName, const FVector& SpawnLocation)
	{
		try
		{
			Logger::LogInfo("[SDK] SpawnCharacterByClass() called with class: " + ClassName);

			std::ofstream log("c:\\temp\\spawn_by_class.log", std::ios::app);
			if (!log.is_open())
			{
				Logger::LogError("[SDK] Could not open spawn_by_class.log");
				return nullptr;
			}

			log << "\n========== [SpawnCharacterByClass] START ==========\n";
			log << "[INFO] Requested class: " << ClassName << "\n";
			log.flush();

			if (!g_SDKInitialized)
			{
				log << "[ERROR] SDK not initialized!\n";
				log.close();
				return nullptr;
			}

			// IMPORTANT: First, try to find and possess an existing CharacterBattle pawn
			log << "[INFO] Searching for existing CharacterBattle pawn...\n";
			log.flush();
			
			APawn* ExistingCharacterBattle = FindExistingCharacterBattle();
			if (ExistingCharacterBattle)
			{
				log << "[SUCCESS] Found existing CharacterBattle pawn!\n";
				log << "[SUCCESS] Attempting to possess existing pawn with new PlayerController...\n";
				log.flush();

				// Get world
				UWorld* World = UWorld::GetWorld();
				if (!World)
				{
					log << "[ERROR] Failed to get world!\n";
					log.close();
					return nullptr;
				}

				// Create a NEW PlayerController to possess the existing CharacterBattle
				log << "[INFO] Creating new PlayerController to possess existing CharacterBattle...\n";
				log.flush();

				APlayerController* NewPlayerController = CreateNewPlayerController(World);
				if (!NewPlayerController)
				{
					log << "[ERROR] Failed to create new PlayerController!\n";
					log.close();
					return nullptr;
				}

				log << "[INFO] New PlayerController created at 0x" << std::hex << reinterpret_cast<uint64_t>(NewPlayerController) << std::dec << "\n";
				log << "[DEBUG]   CharacterBattle = 0x" << std::hex << reinterpret_cast<uint64_t>(ExistingCharacterBattle) << std::dec << "\n";
				log.flush();
				
				// Possess the existing CharacterBattle pawn with the new PlayerController
				log << "[INFO] Calling NewPlayerController->Possess() for CharacterBattle...\n";
				log.flush();
				
				NewPlayerController->Possess(ExistingCharacterBattle);
				
				log << "[INFO] Possess() call completed\n";
				log << "[DEBUG] After Possess():\n";
				log << "[DEBUG]   NewPlayerController->Pawn = 0x" << std::hex << reinterpret_cast<uint64_t>(NewPlayerController->Pawn) << std::dec << "\n";
				log.flush();

				if (NewPlayerController->Pawn == ExistingCharacterBattle)
				{
					log << "[SUCCESS] ✅ Successfully possessed existing CharacterBattle!\n";
					log << "[SUCCESS] Pawn: " << ExistingCharacterBattle->GetName() << "\n";
					log << "========== [SpawnCharacterByClass] SUCCESS ==========\n\n";
					log.close();
					return ExistingCharacterBattle;
				}
				else
				{
					log << "[WARNING] ⚠️ Possession of existing pawn may have failed\n";
					log << "[DEBUG] NewPlayerController->Pawn = 0x" << std::hex << reinterpret_cast<uint64_t>(NewPlayerController->Pawn) << std::dec << "\n";
					log << "========== [SpawnCharacterByClass] PARTIAL SUCCESS ==========\n\n";
					log.close();
					return ExistingCharacterBattle;
				}
			}
			
			log << "[INFO] No existing CharacterBattle found, proceeding with spawn...\n";
			log.flush();

			// Find the class in our enumerated list
			UClass* TargetClass = nullptr;
			for (const auto& pair : g_AvailableClasses)
			{
				if (pair.first == ClassName)
				{
					TargetClass = pair.second;
					log << "[DEBUG] Found class in enumerated list at 0x" << std::hex << reinterpret_cast<uint64_t>(TargetClass) << std::dec << "\n";
					break;
				}
			}

			if (!TargetClass)
			{
				log << "[ERROR] Class '" << ClassName << "' not found in enumerated list!\n";
				log << "[DEBUG] Available classes: " << g_AvailableClasses.size() << "\n";
				for (const auto& pair : g_AvailableClasses)
				{
					log << "  - " << pair.first << "\n";
				}
				log << "========== [SpawnCharacterByClass] FAILED ==========\n\n";
				log.close();
				Logger::LogError("[SDK] Class not found: " + ClassName);
				return nullptr;
			}

			// Get world and player controller
			UWorld* World = UWorld::GetWorld();
			if (!World)
			{
				log << "[ERROR] Failed to get world!\n";
				log.close();
				return nullptr;
			}
			log << "[INFO] World retrieved at 0x" << std::hex << reinterpret_cast<uint64_t>(World) << std::dec << "\n";
			log.flush();

			APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
			if (!PlayerController)
			{
				log << "[ERROR] Failed to get player controller!\n";
				log.close();
				return nullptr;
			}
			log << "[INFO] PlayerController retrieved at 0x" << std::hex << reinterpret_cast<uint64_t>(PlayerController) << std::dec << "\n";
			log.flush();

			// Get current player position - use GetViewTarget if no pawn is possessed
			FVector ActualSpawnLocation = SpawnLocation;
			APawn* CurrentPawn = PlayerController->Pawn;
			
			if (CurrentPawn)
			{
				ActualSpawnLocation = CurrentPawn->K2_GetActorLocation();
				log << "[INFO] Current pawn: " << CurrentPawn->GetName() << "\n";
				log << "[INFO] Using current pawn position for spawn: (" << ActualSpawnLocation.X << ", " << ActualSpawnLocation.Y << ", " << ActualSpawnLocation.Z << ")\n";
				log.flush();
			}
			else
			{
				// Try to get view target if no pawn is possessed
				AActor* ViewTarget = PlayerController->GetViewTarget();
				if (ViewTarget)
				{
					ActualSpawnLocation = ViewTarget->K2_GetActorLocation();
					log << "[INFO] No current pawn, using ViewTarget: " << ViewTarget->GetName() << "\n";
					log << "[INFO] Using ViewTarget position for spawn: (" << ActualSpawnLocation.X << ", " << ActualSpawnLocation.Y << ", " << ActualSpawnLocation.Z << ")\n";
					log.flush();
				}
				else
				{
					log << "[WARNING] No current pawn and no ViewTarget, using provided spawn location (0,0,0)\n";
					log.flush();
				}
			}

			// Create spawn transform
			FTransform SpawnTransform;
			SpawnTransform.Translation = ActualSpawnLocation;
			SpawnTransform.Rotation = FQuat(0, 0, 0, 1);
			SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);
			log << "[INFO] Spawn transform created at (" << ActualSpawnLocation.X << ", " << ActualSpawnLocation.Y << ", " << ActualSpawnLocation.Z << ")\n";
			log.flush();

			// IMPORTANT: UnPossess the current pawn first
			if (CurrentPawn && PlayerController)
			{
				log << "[INFO] UnPossessing current pawn...\n";
				log.flush();
				PlayerController->UnPossess();
				log << "[INFO] UnPossess() called successfully!\n";
				log.flush();
			}

			// Spawn actor
			log << "[INFO] Calling BeginDeferredActorSpawnFromClass with class: " << ClassName << "\n";
			log.flush();

			AActor* SpawnedActor = UGameplayStatics::BeginDeferredActorSpawnFromClass(
				World,
				TSubclassOf<AActor>(TargetClass),
				SpawnTransform,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
				nullptr
			);

			if (!SpawnedActor)
			{
				log << "[ERROR] BeginDeferredActorSpawnFromClass returned nullptr!\n";
				log << "========== [SpawnCharacterByClass] FAILED ==========\n\n";
				log.close();
				return nullptr;
			}
			log << "[INFO] Actor spawned (deferred) at 0x" << std::hex << reinterpret_cast<uint64_t>(SpawnedActor) << std::dec << "\n";
			log.flush();

			// Finish spawn
			AActor* FinalActor = UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);
			if (!FinalActor)
			{
				log << "[ERROR] FinishSpawningActor returned nullptr!\n";
				log << "========== [SpawnCharacterByClass] FAILED ==========\n\n";
				log.close();
				return nullptr;
			}
			log << "[INFO] Actor spawn finished at 0x" << std::hex << reinterpret_cast<uint64_t>(FinalActor) << std::dec << "\n";
			log.flush();

			// Cast to APawn
			APawn* SpawnedPawn = static_cast<APawn*>(FinalActor);
			if (!SpawnedPawn)
			{
				log << "[ERROR] Spawned actor is not a pawn!\n";
				log << "========== [SpawnCharacterByClass] FAILED ==========\n\n";
				log.close();
				return nullptr;
			}
			log << "[INFO] Successfully cast to APawn: " << FinalActor->GetName() << "\n";
			log.flush();

			// Possess the new pawn
			if (PlayerController)
			{
				log << "[INFO] Calling PlayerController->Possess() for new pawn...\n";
				log.flush();
				PlayerController->Possess(SpawnedPawn);
				
				// Verify possession
				log << "[INFO] Possess() called successfully!\n";
				log << "[INFO] PlayerController->Pawn is now: 0x" << std::hex << reinterpret_cast<uint64_t>(PlayerController->Pawn) << std::dec << "\n";
				if (PlayerController->Pawn == SpawnedPawn)
				{
					log << "[SUCCESS] ✅ Possession verified - controller possesses new pawn!\n";
				}
				else
				{
					log << "[WARNING] ⚠️ Possession may have failed - Pawn mismatch!\n";
				}
				log.flush();
			}

			log << "[SUCCESS] Character '" << ClassName << "' spawned and possessed!\n";
			log << "[SUCCESS] New pawn: " << SpawnedPawn->GetName() << "\n";
			log << "========== [SpawnCharacterByClass] SUCCESS ==========\n\n";
			log.close();

			Logger::LogInfo("[SDK] Character spawned successfully - check c:/temp/spawn_by_class.log");
			return SpawnedPawn;
		}
		catch (const std::exception& ex)
		{
			Logger::LogError(std::string("[SDK] Exception in SpawnCharacterByClass: ") + ex.what());
			return nullptr;
		}
		catch (...)
		{
			Logger::LogError("[SDK] Unknown exception in SpawnCharacterByClass");
			return nullptr;
		}
	}
}
