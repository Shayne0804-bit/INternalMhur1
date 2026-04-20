// Test logging SDK information
#include <Windows.h>
#include <sstream>
#include <iomanip>

// SDK headers
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"

// Logger
#include "Utils/Logger.h"

using namespace SDK;

void LogSDKInfo()
{
	Logger::LogInfo("[SDK Test] ========== SDK INITIALIZATION CHECK ==========");
	
	// Get module base directly
	uintptr_t ModuleBase = (uintptr_t)GetModuleHandle(nullptr);
	std::stringstream ss;
	
	// Log Module Base
	ss << "[SDK Test] Module Base: 0x" << std::hex << std::uppercase << ModuleBase;
	Logger::LogInfo(ss.str());
	
	// Log REAL address of GObjects
	Logger::LogInfo("[SDK Test] ========== GOBJECTS ADDRESS ==========");
	
	uintptr_t GObjectsAddr = ModuleBase + Offsets::GObjects;
	ss.str("");
	ss << "[SDK Test] GObjects Real Address: 0x" << std::hex << std::uppercase << GObjectsAddr;
	Logger::LogInfo(ss.str());
	
	// Try to access GObjects via SDK wrapper
	Logger::LogInfo("[SDK Test] ========== ACCESSING SDK GOBJECTS ==========");
	
	try {
		// This will auto-initialize if needed via operator->()
		int32 GObjectsCount = UObject::GObjects->Num();
		
		ss.str("");
		ss << "[SDK Test] GObjects->Num() = " << std::dec << GObjectsCount;
		Logger::LogInfo(ss.str());
		
		if (GObjectsCount > 0) {
			Logger::LogInfo("[SDK Test] ✅ GObjects successfully initialized and accessible!");
		} else {
			Logger::LogWarning("[SDK Test] ⚠️ GObjects is initialized but empty (game might not be fully loaded)");
		}
		
	} catch (const std::exception& e) {
		ss.str("");
		ss << "[SDK Test] ❌ Exception accessing GObjects: " << e.what();
		Logger::LogError(ss.str());
	} catch (...) {
		Logger::LogError("[SDK Test] ❌ Unknown exception accessing GObjects");
	}
	
	Logger::LogInfo("[SDK Test] ========== SDK INITIALIZATION COMPLETE ==========");
}


