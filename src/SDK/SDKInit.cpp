#include "SDKInit.h"
#include "../Utils/Logger.h"
#include <sstream>
#include <Windows.h>

// Local implementation of GetImageBase since SDK version has linking issues
namespace SDK
{
	namespace InSDKUtils
	{
		uintptr_t GetImageBase()
		{
			return reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
		}
	}
}

namespace SDKInit
{
	bool g_SDKInitialized = false;

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

			// Initialize GObjects pointer to actual game memory
			uintptr_t GObjectsAddr = ImageBase + SDK::Offsets::GObjects;
			
			// Copy the TUObjectArrayWrapper from game memory into SDK
			SDK::UObject::GObjects = *(SDK::TUObjectArrayWrapper*)(GObjectsAddr);
			
			if (!SDK::UObject::GObjects)
			{
				Logger::LogWarning("[SDK] GObjects not accessible yet - might be null before game fully loads");
			}
			else
			{
				Logger::LogInfo("[SDK] GObjects array initialized successfully!");
				
				// Try to verify we can access objects
				if (SDK::UObject::GObjects.Num() > 0)
				{
					Logger::LogInfo("[SDK] GObjects has entries - SDK ready!");
				}
			}

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
		Logger::LogInfo("[SDK] Shutting down...");
		g_SDKInitialized = false;
	}

	UWorld* GetGameWorld()
	{
		// Will be implemented when we can safely call UWorld functions
		return nullptr;
	}
}
