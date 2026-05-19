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
	
	// Get module base directly
	uintptr_t ModuleBase = (uintptr_t)GetModuleHandle(nullptr);
	std::stringstream ss;
	
	uintptr_t GObjectsAddr = ModuleBase + Offsets::GObjects;
	ss.str("");
	
	// Try to access GObjects via SDK wrapper
	
	try {
		// This will auto-initialize if needed via operator->()
		int32 GObjectsCount = UObject::GObjects->Num();
		
		ss.str("");
		
		if (GObjectsCount > 0) {
			
		} else {
			
		}
		
	} catch (const std::exception& e) {
		ss.str("");
	} catch (...) {
	}
	
}


