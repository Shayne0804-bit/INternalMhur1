#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <Windows.h>
#include "../Utils/Logger.h"
#include "ImGuiMenu.h"

// The globals are defined in ImGuiMenu.cpp within the ImGuiMenu namespace
// We'll access them through the full qualified name in SettingsManager

namespace SettingsManager
{
	// ============================================================================
	// PROFILE MANAGEMENT
	// ============================================================================

	static std::string GetProfilesPath()
	{
		char buffer[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, buffer);
		std::string path = std::string(buffer) + "\\Profiles\\";
		return path;
	}

	static void EnsureProfilesDirectory()
	{
		std::string profilesPath = GetProfilesPath();
		CreateDirectoryA(profilesPath.c_str(), nullptr);
	}

	static std::vector<std::string> GetProfilesList()
	{
		std::vector<std::string> profiles;
		EnsureProfilesDirectory();

		WIN32_FIND_DATAA findFileData;
		HANDLE hFind = FindFirstFileA((GetProfilesPath() + "*.json").c_str(), &findFileData);

		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				std::string filename = findFileData.cFileName;
				if (filename.length() > 5) // Remove ".json"
				{
					profiles.push_back(filename.substr(0, filename.length() - 5));
				}
			} while (FindNextFileA(hFind, &findFileData));

			FindClose(hFind);
		}

		return profiles;
	}

	// ============================================================================
	// JSON SERIALIZATION HELPERS
	// ============================================================================

	static std::string EscapeJsonString(const std::string& str)
	{
		std::string result;
		for (char c : str)
		{
			switch (c)
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if ('\x00' <= c && c <= '\x1f')
					result += "\\u" + std::to_string(static_cast<int>(c));
				else
					result += c;
			}
		}
		return result;
	}

	static std::string BoolToJson(bool value)
	{
		return value ? "true" : "false";
	}

	static bool JsonToBool(const std::string& str)
	{
		return str == "true";
	}

	static std::string FloatToJson(float value)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%.6f", value);
		std::string result = buffer;
		// Remove trailing zeros
		while (result.back() == '0' && result.find('.') != std::string::npos)
			result.pop_back();
		if (result.back() == '.')
			result.pop_back();
		return result;
	}

	static float JsonToFloat(const std::string& str)
	{
		return std::stof(str);
	}

	static std::string IntToJson(int value)
	{
		return std::to_string(value);
	}

	static int JsonToInt(const std::string& str)
	{
		return std::stoi(str);
	}

	// ============================================================================
	// SAVE/LOAD FUNCTIONS
	// ============================================================================

	static bool SaveProfile(const ImGuiMenu::MenuSettings& menuSettings, 
							const ImGuiMenu::HackSettings& hackSettings,
							const std::string& profileName)
	{
		EnsureProfilesDirectory();

		std::string filename = GetProfilesPath() + profileName + ".json";
		std::ofstream file(filename);

		if (!file.is_open())
		{
			Logger::Log(Logger::LogLevel::Error, "[SettingsManager] Failed to create profile: " + filename);
			return false;
		}

		try
		{
			file << "{\n";
			file << "  \"ProfileName\": \"" << EscapeJsonString(profileName) << "\",\n";

			// ========== MENU SETTINGS ==========
			file << "  \"MenuSettings\": {\n";

			// Global
			file << "    \"EnableGlobal\": " << BoolToJson(menuSettings.EnableGlobal) << ",\n";
			file << "    \"EnableESP\": " << BoolToJson(menuSettings.EnableESP) << ",\n";

			// ESP - Display
			file << "    \"EnablePlayerESP\": " << BoolToJson(menuSettings.EnablePlayerESP) << ",\n";
			file << "    \"Player_Box\": " << BoolToJson(menuSettings.Player_Box) << ",\n";
			file << "    \"Player_Health\": " << BoolToJson(menuSettings.Player_Health) << ",\n";
			file << "    \"Player_Distance\": " << BoolToJson(menuSettings.Player_Distance) << ",\n";
			file << "    \"Player_Name\": " << BoolToJson(menuSettings.Player_Name) << ",\n";
			file << "    \"ShowHP\": " << BoolToJson(menuSettings.ShowHP) << ",\n";
			file << "    \"ShowGP\": " << BoolToJson(menuSettings.ShowGP) << ",\n";
			file << "    \"ShowPU\": " << BoolToJson(menuSettings.ShowPU) << ",\n";
			file << "    \"ShowPlatform\": " << BoolToJson(menuSettings.ShowPlatform) << ",\n";
			file << "    \"ShowTeamId\": " << BoolToJson(menuSettings.ShowTeamId) << ",\n";
			file << "    \"ShowPlayerSkeleton\": " << BoolToJson(menuSettings.ShowPlayerSkeleton) << ",\n";

			// Aimbot
			file << "    \"EnableAimbot\": " << BoolToJson(menuSettings.EnableAimbot) << ",\n";
			file << "    \"AimbotSmoothing\": " << BoolToJson(menuSettings.AimbotSmoothing) << ",\n";
			file << "    \"AimbotSmoothFactor\": " << FloatToJson(menuSettings.AimbotSmoothFactor) << ",\n";
			file << "    \"AimbotDrawLine\": " << BoolToJson(menuSettings.AimbotDrawLine) << ",\n";
			file << "    \"AimbotDrawFOV\": " << BoolToJson(menuSettings.AimbotDrawFOV) << ",\n";
			file << "    \"AimbotFOVRadius\": " << FloatToJson(menuSettings.AimbotFOVRadius) << ",\n";
			file << "    \"AimbotRequireHold\": " << BoolToJson(menuSettings.AimbotRequireHold) << ",\n";

			// Aimbot Hotkeys
			file << "    \"AimbotHoldKey_Keyboard\": " << IntToJson(menuSettings.AimbotHoldKey.Keyboard) << ",\n";
			file << "    \"AimbotHoldKey_Xbox\": " << IntToJson(menuSettings.AimbotHoldKey.Xbox) << ",\n";

			// Teleport to Kota
			file << "    \"EnableTeleportToKota\": " << BoolToJson(menuSettings.EnableTeleportToKota) << ",\n";
			file << "    \"TeleportToKotaKey_Keyboard\": " << IntToJson(menuSettings.TeleportToKotaKey.Keyboard) << ",\n";
			file << "    \"TeleportToKotaKey_Xbox\": " << IntToJson(menuSettings.TeleportToKotaKey.Xbox) << ",\n";

			// Transform Into Random ESP Target
			file << "    \"EnableTransformIntoRandomESP\": " << BoolToJson(menuSettings.EnableTransformIntoRandomESP) << ",\n";
			file << "    \"TransformIntoRandomESPKey_Keyboard\": " << IntToJson(menuSettings.TransformIntoRandomESPKey.Keyboard) << ",\n";
			file << "    \"TransformIntoRandomESPKey_Xbox\": " << IntToJson(menuSettings.TransformIntoRandomESPKey.Xbox) << ",\n";

			// Duplicate Into Imitation Random ESP Target
			file << "    \"EnableDuplicateIntoImitationRandomESP\": " << BoolToJson(menuSettings.EnableDuplicateIntoImitationRandomESP) << ",\n";
			file << "    \"DuplicateIntoImitationRandomESPKey_Keyboard\": " << IntToJson(menuSettings.DuplicateIntoImitationRandomESPKey.Keyboard) << ",\n";
			file << "    \"DuplicateIntoImitationRandomESPKey_Xbox\": " << IntToJson(menuSettings.DuplicateIntoImitationRandomESPKey.Xbox) << ",\n";
			file << "    \"DuplicateImitationLifeTime\": " << FloatToJson(menuSettings.DuplicateImitationLifeTime) << ",\n";
			file << "    \"DuplicateIntoImitationCount\": " << IntToJson(menuSettings.DuplicateIntoImitationCount) << ",\n";

			// Copy Skills From Nearest Enemy
			file << "    \"EnableCopySkillsFromNearestEnemy\": " << BoolToJson(menuSettings.EnableCopySkillsFromNearestEnemy) << ",\n";
			file << "    \"CopySkillsFromNearestEnemyKey_Keyboard\": " << IntToJson(menuSettings.CopySkillsFromNearestEnemyKey.Keyboard) << ",\n";
			file << "    \"CopySkillsFromNearestEnemyKey_Xbox\": " << IntToJson(menuSettings.CopySkillsFromNearestEnemyKey.Xbox) << ",\n";
			file << "    \"CopySkillsSetCopySkill\": " << BoolToJson(menuSettings.CopySkillsSetCopySkill) << ",\n";
			file << "    \"CopySkillsUseOwnerCharacterLevel\": " << BoolToJson(menuSettings.CopySkillsUseOwnerCharacterLevel) << ",\n";

			// Generate Projectile
			file << "    \"EnableGenerateProjectile\": " << BoolToJson(menuSettings.EnableGenerateProjectile) << ",\n";
			file << "    \"GenerateProjectileKey_Keyboard\": " << IntToJson(menuSettings.GenerateProjectileKey.Keyboard) << ",\n";
			file << "    \"GenerateProjectileKey_Xbox\": " << IntToJson(menuSettings.GenerateProjectileKey.Xbox) << ",\n";

			// Reload Adjust Rates
			file << "    \"ReloadAdjustRate\": " << FloatToJson(menuSettings.ReloadAdjustRate) << ",\n";
			file << "    \"ReloadAdjustRate_RollSlot\": " << FloatToJson(menuSettings.ReloadAdjustRate_RollSlot) << ",\n";
			file << "    \"ReloadAdjustRate_WearBlueFlame\": " << FloatToJson(menuSettings.ReloadAdjustRate_WearBlueFlame) << ",\n";

			// Training Mode
			file << "    \"TrainingPlayerCharacter\": " << IntToJson(menuSettings.TrainingPlayerCharacter) << ",\n";
			file << "    \"TrainingPlayerVariationId\": " << IntToJson(menuSettings.TrainingPlayerVariationId) << ",\n";
			file << "    \"TrainingPlayerUnique1\": " << IntToJson(menuSettings.TrainingPlayerUnique1) << ",\n";
			file << "    \"TrainingPlayerUnique2\": " << IntToJson(menuSettings.TrainingPlayerUnique2) << ",\n";
			file << "    \"TrainingPlayerUnique3\": " << IntToJson(menuSettings.TrainingPlayerUnique3) << ",\n";
			file << "    \"TrainingPlayerSkillCode\": " << IntToJson(menuSettings.TrainingPlayerSkillCode) << ",\n";
			file << "    \"TrainingPlayerCostumeCode\": " << IntToJson(menuSettings.TrainingPlayerCostumeCode) << ",\n";
			file << "    \"TrainingPlayerCostumeAuraType\": " << IntToJson(menuSettings.TrainingPlayerCostumeAuraType) << ",\n";

			// Invincibility Hotkey
			file << "    \"SetInvincibleKey_Keyboard\": " << IntToJson(menuSettings.SetInvincibleKey.Keyboard) << ",\n";
			file << "    \"SetInvincibleKey_Xbox\": " << IntToJson(menuSettings.SetInvincibleKey.Xbox) << ",\n";

			// Recovery Settings
			file << "    \"EnableRecoveryMe\": " << BoolToJson(menuSettings.EnableRecoveryMe) << ",\n";
			file << "    \"EnableRecoveryTeam\": " << BoolToJson(menuSettings.EnableRecoveryTeam) << ",\n";
			file << "    \"EnableRecoverySelectedTeam\": " << BoolToJson(menuSettings.EnableRecoverySelectedTeam) << ",\n";
			file << "    \"EnableRecoveryAllESP\": " << BoolToJson(menuSettings.EnableRecoveryAllESP) << ",\n";

			// Teleport Items
			file << "    \"EnableTeleportLevelUpCards\": " << BoolToJson(menuSettings.EnableTeleportLevelUpCards) << ",\n";

			// BulletTP
			file << "    \"EnableBulletTP\": " << BoolToJson(menuSettings.EnableBulletTP) << ",\n";
			file << "    \"BulletTP_IncludeAlpha\": " << BoolToJson(menuSettings.BulletTP_IncludeAlpha) << ",\n";
			file << "    \"BulletTP_IncludeBeta\": " << BoolToJson(menuSettings.BulletTP_IncludeBeta) << ",\n";
			file << "    \"BulletTP_IncludeGamma\": " << BoolToJson(menuSettings.BulletTP_IncludeGamma) << ",\n";
			file << "    \"BulletTP_IncludeSpecial\": " << BoolToJson(menuSettings.BulletTP_IncludeSpecial) << ",\n";
			file << "    \"BulletTP_FOVRadius\": " << FloatToJson(menuSettings.BulletTP_FOVRadius) << ",\n";
			file << "    \"BulletTP_MaxDistance\": " << FloatToJson(menuSettings.BulletTP_MaxDistance) << ",\n";

			// Rebuild Myself
			file << "    \"EnableRebuildMyself\": " << BoolToJson(menuSettings.EnableRebuildMyself) << ",\n";
			file << "    \"RebuildMyselfKey_Keyboard\": " << IntToJson(menuSettings.RebuildMyselfKey.Keyboard) << ",\n";
			file << "    \"RebuildMyselfKey_Xbox\": " << IntToJson(menuSettings.RebuildMyselfKey.Xbox) << ",\n";

			// ESP - Filters
			file << "    \"ShowEnemies\": " << BoolToJson(menuSettings.ShowEnemies) << ",\n";
			file << "    \"ShowTeam\": " << BoolToJson(menuSettings.ShowTeam) << ",\n";
			file << "    \"ShowLobbyCharacters\": " << BoolToJson(menuSettings.ShowLobbyCharacters) << ",\n";
			file << "    \"ShowKota\": " << BoolToJson(menuSettings.ShowKota) << ",\n";
			file << "    \"Player_DrawDistance\": " << FloatToJson(menuSettings.Player_DrawDistance) << ",\n";

			// Character Control
			file << "    \"SelectedCharacterIndex\": " << IntToJson(menuSettings.SelectedCharacterIndex) << ",\n";
			file << "    \"SelectedRecoveryTeamIndex\": " << IntToJson(menuSettings.SelectedRecoveryTeamIndex) << ",\n";
			file << "    \"EnableCH202InitTransLevel5\": " << BoolToJson(menuSettings.EnableCH202InitTransLevel5) << ",\n";
			file << "    \"EnableSupplyMaxStackTo100\": " << BoolToJson(menuSettings.EnableSupplyMaxStackTo100) << "\n";

			file << "  },\n";

			// ========== HACK SETTINGS ==========
			file << "  \"HackSettings\": {\n";
			file << "    \"EnableInvincible\": " << BoolToJson(hackSettings.EnableInvincible) << ",\n";
			file << "    \"EnableInfiniteHealth\": " << BoolToJson(hackSettings.EnableInfiniteHealth) << ",\n";
			file << "    \"EnableInfiniteBarrier\": " << BoolToJson(hackSettings.EnableInfiniteBarrier) << ",\n";
			file << "    \"EnableInfinitePlusUltra\": " << BoolToJson(hackSettings.EnableInfinitePlusUltra) << ",\n";
			file << "    \"EnableFullBuff\": " << BoolToJson(hackSettings.EnableFullBuff) << ",\n";
			file << "    \"AbilityAttackLevel\": " << IntToJson(hackSettings.AbilityAttackLevel) << ",\n";
			file << "    \"AbilityDurableLevel\": " << IntToJson(hackSettings.AbilityDurableLevel) << ",\n";
			file << "    \"AbilityMovespeedLevel\": " << IntToJson(hackSettings.AbilityMovespeedLevel) << ",\n";
			file << "    \"AbilityHealLevel\": " << IntToJson(hackSettings.AbilityHealLevel) << ",\n";
			file << "    \"AbilityTechniqueLevel\": " << IntToJson(hackSettings.AbilityTechniqueLevel) << ",\n";
			file << "    \"CharacterId\": " << IntToJson(hackSettings.CharacterId) << ",\n";
			file << "    \"CharacterVariationId\": " << IntToJson(hackSettings.CharacterVariationId) << ",\n";
			file << "    \"CharacterUnique1\": " << IntToJson(hackSettings.CharacterUnique1) << ",\n";
			file << "    \"CharacterUnique2\": " << IntToJson(hackSettings.CharacterUnique2) << ",\n";
			file << "    \"CharacterUnique3\": " << IntToJson(hackSettings.CharacterUnique3) << ",\n";
			file << "    \"CharacterSkillCode\": " << IntToJson(hackSettings.CharacterSkillCode) << ",\n";
			file << "    \"CharacterCostumeCode\": " << IntToJson(hackSettings.CharacterCostumeCode) << ",\n";
			file << "    \"CharacterCostumeAuraType\": " << IntToJson(hackSettings.CharacterCostumeAuraType) << ",\n";
			file << "    \"SupplyUniqueSkill1Level\": " << IntToJson(hackSettings.SupplyUniqueSkill1Level) << ",\n";
			file << "    \"SupplyUniqueSkill2Level\": " << IntToJson(hackSettings.SupplyUniqueSkill2Level) << ",\n";
			file << "    \"SupplyUniqueSkill3Level\": " << IntToJson(hackSettings.SupplyUniqueSkill3Level) << ",\n";
			file << "    \"CharCondition_EnableDekuMode\": " << BoolToJson(hackSettings.CharCondition_EnableDekuMode) << ",\n";
			file << "    \"CharCondition_EnableUnbreakable\": " << BoolToJson(hackSettings.CharCondition_EnableUnbreakable) << ",\n";
			file << "    \"CharCondition_EnableCompressionRegen\": " << BoolToJson(hackSettings.CharCondition_EnableCompressionRegen) << ",\n";
			file << "    \"CharCondition_EnableMirioMode\": " << BoolToJson(hackSettings.CharCondition_EnableMirioMode) << ",\n";
			file << "    \"CharCondition_EnableTokoyamiMode\": " << BoolToJson(hackSettings.CharCondition_EnableTokoyamiMode) << ",\n";
			file << "    \"BuyLicenseExpCount\": " << IntToJson(hackSettings.BuyLicenseExpCount) << "\n";
			file << "  }\n";

			file << "}\n";
			file.close();

			Logger::Log(Logger::LogLevel::Info, "[SettingsManager] Profile saved: " + profileName);
			return true;
		}
		catch (const std::exception& e)
		{
			Logger::Log(Logger::LogLevel::Error, "[SettingsManager] Exception while saving: " + std::string(e.what()));
			return false;
		}
	}

	static bool CreateDefaultProfile()
	{
		EnsureProfilesDirectory();

		// Create a completely empty/disabled profile
		ImGuiMenu::MenuSettings emptyMenu;
		ImGuiMenu::HackSettings emptyHack;

		// Disable all MenuSettings bools
		emptyMenu.EnableGlobal = false;
		emptyMenu.EnableESP = false;
		emptyMenu.EnablePlayerESP = false;
		emptyMenu.Player_Box = false;
		emptyMenu.Player_Health = false;
		emptyMenu.Player_Distance = false;
		emptyMenu.Player_Name = false;
		emptyMenu.ShowHP = false;
		emptyMenu.ShowGP = false;
		emptyMenu.ShowPU = false;
		emptyMenu.ShowPlatform = false;
		emptyMenu.ShowTeamId = false;
		emptyMenu.ShowPlayerSkeleton = false;

		emptyMenu.EnableAimbot = false;
		emptyMenu.AimbotSmoothing = false;
		emptyMenu.AimbotDrawLine = false;
		emptyMenu.AimbotDrawFOV = false;
		emptyMenu.AimbotRequireHold = false;

		emptyMenu.EnableTeleportToKota = false;
		emptyMenu.EnableTransformIntoRandomESP = false;
		emptyMenu.EnableDuplicateIntoImitationRandomESP = false;
		emptyMenu.EnableCopySkillsFromNearestEnemy = false;
		emptyMenu.CopySkillsSetCopySkill = false;
		emptyMenu.CopySkillsUseOwnerCharacterLevel = false;
		emptyMenu.EnableGenerateProjectile = false;
		emptyMenu.EnableRecoveryMe = false;
		emptyMenu.EnableRecoveryTeam = false;
		emptyMenu.EnableRecoverySelectedTeam = false;
		emptyMenu.EnableRecoveryAllESP = false;
		emptyMenu.EnableTeleportLevelUpCards = false;
		emptyMenu.EnableBulletTP = false;
		emptyMenu.BulletTP_IncludeAlpha = false;
		emptyMenu.BulletTP_IncludeBeta = false;
		emptyMenu.BulletTP_IncludeGamma = false;
		emptyMenu.BulletTP_IncludeSpecial = false;
		emptyMenu.EnableRebuildMyself = false;
		emptyMenu.ShowEnemies = false;
		emptyMenu.ShowTeam = false;
		emptyMenu.ShowLobbyCharacters = false;
		emptyMenu.ShowKota = false;
		emptyMenu.EnableCH202InitTransLevel5 = false;
		emptyMenu.EnableSupplyMaxStackTo100 = false;

		// Initialize all float sliders to 0
		emptyMenu.AimbotSmoothFactor = 0.0f;
		emptyMenu.AimbotFOVRadius = 0.0f;
		emptyMenu.DuplicateImitationLifeTime = 0.0f;
		emptyMenu.ReloadAdjustRate = 1.0f;  // Keep at 1.0f (normal speed) not 0
		emptyMenu.ReloadAdjustRate_RollSlot = 1.0f;  // Keep at 1.0f
		emptyMenu.ReloadAdjustRate_WearBlueFlame = 1.0f;  // Keep at 1.0f
		emptyMenu.BulletTP_FOVRadius = 0.0f;
		emptyMenu.BulletTP_MaxDistance = 0.0f;
		emptyMenu.Player_DrawDistance = 0.0f;

		// Initialize all int values to 0 or default
		emptyMenu.TrainingPlayerCharacter = 0;
		emptyMenu.TrainingPlayerVariationId = 0;
		emptyMenu.TrainingPlayerUnique1 = 1;  // Level - keep at 1
		emptyMenu.TrainingPlayerUnique2 = 1;  // Level - keep at 1
		emptyMenu.TrainingPlayerUnique3 = 1;  // Level - keep at 1
		emptyMenu.TrainingPlayerSkillCode = 0;
		emptyMenu.TrainingPlayerCostumeCode = 0;
		emptyMenu.TrainingPlayerCostumeAuraType = 0;
		emptyMenu.DuplicateIntoImitationCount = 1;  // At least 1
		emptyMenu.SelectedCharacterIndex = -1;
		emptyMenu.SelectedRecoveryTeamIndex = -1;

		// Initialize all hotkeys to 0
		emptyMenu.AimbotHoldKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.TeleportToKotaKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.TransformIntoRandomESPKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.DuplicateIntoImitationRandomESPKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.CopySkillsFromNearestEnemyKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.GenerateProjectileKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.SetInvincibleKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.RebuildMyselfKey = ImGuiMenu::HotkeySet(0, 0);

		// Disable all HackSettings
		emptyHack.EnableInvincible = false;
		emptyHack.EnableInfiniteHealth = false;
		emptyHack.EnableInfiniteBarrier = false;
		emptyHack.EnableInfinitePlusUltra = false;
		emptyHack.EnableFullBuff = false;

		// Set all ability levels to 1 (not 0)
		emptyHack.AbilityAttackLevel = 1;
		emptyHack.AbilityDurableLevel = 1;
		emptyHack.AbilityMovespeedLevel = 1;
		emptyHack.AbilityHealLevel = 1;
		emptyHack.AbilityTechniqueLevel = 1;

		// Set all character settings to 0 or default
		emptyHack.CharacterId = 0;
		emptyHack.CharacterVariationId = 0;
		emptyHack.CharacterUnique1 = 1;  // Level - keep at 1
		emptyHack.CharacterUnique2 = 1;  // Level - keep at 1
		emptyHack.CharacterUnique3 = 1;  // Level - keep at 1
		emptyHack.CharacterSkillCode = 0;
		emptyHack.CharacterCostumeCode = 0;
		emptyHack.CharacterCostumeAuraType = 0;

		// Supply Management - Unique Skill Levels
		emptyHack.SupplyUniqueSkill1Level = 1;  // Level - keep at 1
		emptyHack.SupplyUniqueSkill2Level = 1;  // Level - keep at 1
		emptyHack.SupplyUniqueSkill3Level = 1;  // Level - keep at 1
		emptyHack.CharCondition_EnableDekuMode = false;
		emptyHack.CharCondition_EnableUnbreakable = false;
		emptyHack.CharCondition_EnableCompressionRegen = false;
		emptyHack.CharCondition_EnableMirioMode = false;
		emptyHack.CharCondition_EnableTokoyamiMode = false;
		emptyHack.BuyLicenseExpCount = 100;

		return SaveProfile(emptyMenu, emptyHack, "Default");
	}

	static bool LoadProfile(ImGuiMenu::MenuSettings& menuSettings,
							ImGuiMenu::HackSettings& hackSettings,
							const std::string& profileName)
	{
		std::string filename = GetProfilesPath() + profileName + ".json";
		std::ifstream file(filename);

		if (!file.is_open())
		{
			Logger::Log(Logger::LogLevel::Error, "[SettingsManager] Failed to open profile: " + filename);
			return false;
		}

		try
		{
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string json = buffer.str();
			file.close();

			// Simple JSON parser - extract key-value pairs
			auto ExtractString = [&](const std::string& key) -> std::string
			{
				std::string searchStr = "\"" + key + "\": \"";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return "";
				
				pos += searchStr.length();
				size_t endPos = json.find("\"", pos);
				return json.substr(pos, endPos - pos);
			};

			auto ExtractBool = [&](const std::string& key) -> bool
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return false;
				
				pos += searchStr.length();
				return json.substr(pos, 4) == "true";
			};

			auto ExtractInt = [&](const std::string& key) -> int
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return 0;
				
				pos += searchStr.length();
				std::string numStr;
				while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-'))
				{
					numStr += json[pos];
					pos++;
				}
				return numStr.empty() ? 0 : std::stoi(numStr);
			};

			auto ExtractFloat = [&](const std::string& key) -> float
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return 0.0f;
				
				pos += searchStr.length();
				std::string numStr;
				while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-'))
				{
					numStr += json[pos];
					pos++;
				}
				return numStr.empty() ? 0.0f : std::stof(numStr);
			};

			// Load MenuSettings
			menuSettings.EnableGlobal = ExtractBool("EnableGlobal");
			menuSettings.EnableESP = ExtractBool("EnableESP");
			menuSettings.EnablePlayerESP = ExtractBool("EnablePlayerESP");
			menuSettings.Player_Box = ExtractBool("Player_Box");
			menuSettings.Player_Health = ExtractBool("Player_Health");
			menuSettings.Player_Distance = ExtractBool("Player_Distance");
			menuSettings.Player_Name = ExtractBool("Player_Name");
			menuSettings.ShowHP = ExtractBool("ShowHP");
			menuSettings.ShowGP = ExtractBool("ShowGP");
			menuSettings.ShowPU = ExtractBool("ShowPU");
			menuSettings.ShowPlatform = ExtractBool("ShowPlatform");
			menuSettings.ShowTeamId = ExtractBool("ShowTeamId");
			menuSettings.ShowPlayerSkeleton = ExtractBool("ShowPlayerSkeleton");

			menuSettings.EnableAimbot = ExtractBool("EnableAimbot");
			menuSettings.AimbotSmoothing = ExtractBool("AimbotSmoothing");
			menuSettings.AimbotSmoothFactor = ExtractFloat("AimbotSmoothFactor");
			menuSettings.AimbotDrawLine = ExtractBool("AimbotDrawLine");
			menuSettings.AimbotDrawFOV = ExtractBool("AimbotDrawFOV");
			menuSettings.AimbotFOVRadius = ExtractFloat("AimbotFOVRadius");
			menuSettings.AimbotRequireHold = ExtractBool("AimbotRequireHold");

			menuSettings.AimbotHoldKey.Keyboard = ExtractInt("AimbotHoldKey_Keyboard");
			menuSettings.AimbotHoldKey.Xbox = ExtractInt("AimbotHoldKey_Xbox");
			menuSettings.AimbotHoldKey.PS4 = menuSettings.AimbotHoldKey.Xbox; // PS4 uses same value

			menuSettings.EnableTeleportToKota = ExtractBool("EnableTeleportToKota");
			menuSettings.TeleportToKotaKey.Keyboard = ExtractInt("TeleportToKotaKey_Keyboard");
			menuSettings.TeleportToKotaKey.Xbox = ExtractInt("TeleportToKotaKey_Xbox");
			menuSettings.TeleportToKotaKey.PS4 = menuSettings.TeleportToKotaKey.Xbox;

			menuSettings.EnableTransformIntoRandomESP = ExtractBool("EnableTransformIntoRandomESP");
			menuSettings.TransformIntoRandomESPKey.Keyboard = ExtractInt("TransformIntoRandomESPKey_Keyboard");
			menuSettings.TransformIntoRandomESPKey.Xbox = ExtractInt("TransformIntoRandomESPKey_Xbox");
			menuSettings.TransformIntoRandomESPKey.PS4 = menuSettings.TransformIntoRandomESPKey.Xbox;

			menuSettings.EnableDuplicateIntoImitationRandomESP = ExtractBool("EnableDuplicateIntoImitationRandomESP");
			menuSettings.DuplicateIntoImitationRandomESPKey.Keyboard = ExtractInt("DuplicateIntoImitationRandomESPKey_Keyboard");
			menuSettings.DuplicateIntoImitationRandomESPKey.Xbox = ExtractInt("DuplicateIntoImitationRandomESPKey_Xbox");
			menuSettings.DuplicateIntoImitationRandomESPKey.PS4 = menuSettings.DuplicateIntoImitationRandomESPKey.Xbox;
			menuSettings.DuplicateImitationLifeTime = ExtractFloat("DuplicateImitationLifeTime");
			menuSettings.DuplicateIntoImitationCount = ExtractInt("DuplicateIntoImitationCount");

			menuSettings.EnableCopySkillsFromNearestEnemy = ExtractBool("EnableCopySkillsFromNearestEnemy");
			menuSettings.CopySkillsFromNearestEnemyKey.Keyboard = ExtractInt("CopySkillsFromNearestEnemyKey_Keyboard");
			menuSettings.CopySkillsFromNearestEnemyKey.Xbox = ExtractInt("CopySkillsFromNearestEnemyKey_Xbox");
			menuSettings.CopySkillsFromNearestEnemyKey.PS4 = menuSettings.CopySkillsFromNearestEnemyKey.Xbox;
			menuSettings.CopySkillsSetCopySkill = ExtractBool("CopySkillsSetCopySkill");
			menuSettings.CopySkillsUseOwnerCharacterLevel = ExtractBool("CopySkillsUseOwnerCharacterLevel");

			menuSettings.EnableGenerateProjectile = ExtractBool("EnableGenerateProjectile");
			menuSettings.GenerateProjectileKey.Keyboard = ExtractInt("GenerateProjectileKey_Keyboard");
			menuSettings.GenerateProjectileKey.Xbox = ExtractInt("GenerateProjectileKey_Xbox");
			menuSettings.GenerateProjectileKey.PS4 = menuSettings.GenerateProjectileKey.Xbox;

			menuSettings.ReloadAdjustRate = ExtractFloat("ReloadAdjustRate");
			menuSettings.ReloadAdjustRate_RollSlot = ExtractFloat("ReloadAdjustRate_RollSlot");
			menuSettings.ReloadAdjustRate_WearBlueFlame = ExtractFloat("ReloadAdjustRate_WearBlueFlame");

			menuSettings.TrainingPlayerCharacter = ExtractInt("TrainingPlayerCharacter");
			menuSettings.TrainingPlayerVariationId = ExtractInt("TrainingPlayerVariationId");
			menuSettings.TrainingPlayerUnique1 = ExtractInt("TrainingPlayerUnique1");
			menuSettings.TrainingPlayerUnique2 = ExtractInt("TrainingPlayerUnique2");
			menuSettings.TrainingPlayerUnique3 = ExtractInt("TrainingPlayerUnique3");
			menuSettings.TrainingPlayerSkillCode = ExtractInt("TrainingPlayerSkillCode");
			menuSettings.TrainingPlayerCostumeCode = ExtractInt("TrainingPlayerCostumeCode");
			menuSettings.TrainingPlayerCostumeAuraType = ExtractInt("TrainingPlayerCostumeAuraType");

			menuSettings.SetInvincibleKey.Keyboard = ExtractInt("SetInvincibleKey_Keyboard");
			menuSettings.SetInvincibleKey.Xbox = ExtractInt("SetInvincibleKey_Xbox");
			menuSettings.SetInvincibleKey.PS4 = menuSettings.SetInvincibleKey.Xbox;

			menuSettings.EnableRecoveryMe = ExtractBool("EnableRecoveryMe");
			menuSettings.EnableRecoveryTeam = ExtractBool("EnableRecoveryTeam");
			menuSettings.EnableRecoverySelectedTeam = ExtractBool("EnableRecoverySelectedTeam");
			menuSettings.EnableRecoveryAllESP = ExtractBool("EnableRecoveryAllESP");

			menuSettings.EnableTeleportLevelUpCards = ExtractBool("EnableTeleportLevelUpCards");

			menuSettings.EnableBulletTP = ExtractBool("EnableBulletTP");
			menuSettings.BulletTP_IncludeAlpha = ExtractBool("BulletTP_IncludeAlpha");
			menuSettings.BulletTP_IncludeBeta = ExtractBool("BulletTP_IncludeBeta");
			menuSettings.BulletTP_IncludeGamma = ExtractBool("BulletTP_IncludeGamma");
			menuSettings.BulletTP_IncludeSpecial = ExtractBool("BulletTP_IncludeSpecial");
			menuSettings.BulletTP_FOVRadius = ExtractFloat("BulletTP_FOVRadius");
			menuSettings.BulletTP_MaxDistance = ExtractFloat("BulletTP_MaxDistance");

			menuSettings.EnableRebuildMyself = ExtractBool("EnableRebuildMyself");
			menuSettings.RebuildMyselfKey.Keyboard = ExtractInt("RebuildMyselfKey_Keyboard");
			menuSettings.RebuildMyselfKey.Xbox = ExtractInt("RebuildMyselfKey_Xbox");
			menuSettings.RebuildMyselfKey.PS4 = menuSettings.RebuildMyselfKey.Xbox;

			menuSettings.ShowEnemies = ExtractBool("ShowEnemies");
			menuSettings.ShowTeam = ExtractBool("ShowTeam");
			menuSettings.ShowLobbyCharacters = ExtractBool("ShowLobbyCharacters");
			menuSettings.ShowKota = ExtractBool("ShowKota");
			menuSettings.Player_DrawDistance = ExtractFloat("Player_DrawDistance");

			menuSettings.SelectedCharacterIndex = ExtractInt("SelectedCharacterIndex");
			menuSettings.SelectedRecoveryTeamIndex = ExtractInt("SelectedRecoveryTeamIndex");
			menuSettings.EnableCH202InitTransLevel5 = ExtractBool("EnableCH202InitTransLevel5");
			menuSettings.EnableSupplyMaxStackTo100 = ExtractBool("EnableSupplyMaxStackTo100");

			// Load HackSettings
			hackSettings.EnableInvincible = ExtractBool("EnableInvincible");
			hackSettings.EnableInfiniteHealth = ExtractBool("EnableInfiniteHealth");
			hackSettings.EnableInfiniteBarrier = ExtractBool("EnableInfiniteBarrier");
			hackSettings.EnableInfinitePlusUltra = ExtractBool("EnableInfinitePlusUltra");
			hackSettings.EnableFullBuff = ExtractBool("EnableFullBuff");
			hackSettings.AbilityAttackLevel = ExtractInt("AbilityAttackLevel");
			hackSettings.AbilityDurableLevel = ExtractInt("AbilityDurableLevel");
			hackSettings.AbilityMovespeedLevel = ExtractInt("AbilityMovespeedLevel");
			hackSettings.AbilityHealLevel = ExtractInt("AbilityHealLevel");
			hackSettings.AbilityTechniqueLevel = ExtractInt("AbilityTechniqueLevel");
			hackSettings.CharacterId = ExtractInt("CharacterId");
			hackSettings.CharacterVariationId = ExtractInt("CharacterVariationId");
			hackSettings.CharacterUnique1 = ExtractInt("CharacterUnique1");
			hackSettings.CharacterUnique2 = ExtractInt("CharacterUnique2");
			hackSettings.CharacterUnique3 = ExtractInt("CharacterUnique3");
			hackSettings.CharacterSkillCode = ExtractInt("CharacterSkillCode");
			hackSettings.CharacterCostumeCode = ExtractInt("CharacterCostumeCode");
			hackSettings.CharacterCostumeAuraType = ExtractInt("CharacterCostumeAuraType");
			hackSettings.SupplyUniqueSkill1Level = ExtractInt("SupplyUniqueSkill1Level");
			hackSettings.SupplyUniqueSkill2Level = ExtractInt("SupplyUniqueSkill2Level");
			hackSettings.SupplyUniqueSkill3Level = ExtractInt("SupplyUniqueSkill3Level");
			hackSettings.CharCondition_EnableDekuMode = ExtractBool("CharCondition_EnableDekuMode");
			hackSettings.CharCondition_EnableUnbreakable = ExtractBool("CharCondition_EnableUnbreakable");
			hackSettings.CharCondition_EnableCompressionRegen = ExtractBool("CharCondition_EnableCompressionRegen");
			hackSettings.CharCondition_EnableMirioMode = ExtractBool("CharCondition_EnableMirioMode");
			hackSettings.CharCondition_EnableTokoyamiMode = ExtractBool("CharCondition_EnableTokoyamiMode");
			hackSettings.BuyLicenseExpCount = ExtractInt("BuyLicenseExpCount");

			Logger::Log(Logger::LogLevel::Info, "[SettingsManager] Profile loaded: " + profileName);
			return true;
		}
		catch (const std::exception& e)
		{
			Logger::Log(Logger::LogLevel::Error, "[SettingsManager] Exception while loading: " + std::string(e.what()));
			return false;
		}
	}

	static bool SaveCurrentProfile(const std::string& profileName)
	{
		return SaveProfile(ImGuiMenu::g_Settings, ImGuiMenu::g_HackSettings, profileName);
	}

	static bool LoadCurrentProfile(const std::string& profileName)
	{
		return LoadProfile(ImGuiMenu::g_Settings, ImGuiMenu::g_HackSettings, profileName);
	}
}
