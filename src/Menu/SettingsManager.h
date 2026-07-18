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
					std::string name = filename.substr(0, filename.length() - 5);
					// "Default" is the baseline for users without profiles —
					// never listed nor loadable from the profile list.
					if (_stricmp(name.c_str(), "Default") != 0)
						profiles.push_back(name);
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
			file << "    \"EnableMenuBackgroundVideo\": " << BoolToJson(menuSettings.EnableMenuBackgroundVideo) << ",\n";
			file << "    \"EnableStreamProofMenu\": " << BoolToJson(menuSettings.EnableStreamProofMenu) << ",\n";

			// ESP - Display
			file << "    \"EnablePlayerESP\": " << BoolToJson(menuSettings.EnablePlayerESP) << ",\n";
			file << "    \"Player_Box\": " << BoolToJson(menuSettings.Player_Box) << ",\n";
			file << "    \"Player_Box_Filled\": " << BoolToJson(menuSettings.Player_Box_Filled) << ",\n";
			file << "    \"Player_Box_Filled_Alpha\": " << FloatToJson(menuSettings.Player_Box_Filled_Alpha) << ",\n";
			file << "    \"PlayerColor_R\": " << FloatToJson(menuSettings.PlayerColor[0]) << ",\n";
			file << "    \"PlayerColor_G\": " << FloatToJson(menuSettings.PlayerColor[1]) << ",\n";
			file << "    \"PlayerColor_B\": " << FloatToJson(menuSettings.PlayerColor[2]) << ",\n";
			file << "    \"PlayerColor_A\": " << FloatToJson(menuSettings.PlayerColor[3]) << ",\n";
			file << "    \"TeamColor_R\": " << FloatToJson(menuSettings.TeamColor[0]) << ",\n";
			file << "    \"TeamColor_G\": " << FloatToJson(menuSettings.TeamColor[1]) << ",\n";
			file << "    \"TeamColor_B\": " << FloatToJson(menuSettings.TeamColor[2]) << ",\n";
			file << "    \"TeamColor_A\": " << FloatToJson(menuSettings.TeamColor[3]) << ",\n";
			file << "    \"Player_Health\": " << BoolToJson(menuSettings.Player_Health) << ",\n";
			file << "    \"Player_Distance\": " << BoolToJson(menuSettings.Player_Distance) << ",\n";
			file << "    \"Player_Name\": " << BoolToJson(menuSettings.Player_Name) << ",\n";
			file << "    \"ShowHP\": " << BoolToJson(menuSettings.ShowHP) << ",\n";
			file << "    \"ShowGP\": " << BoolToJson(menuSettings.ShowGP) << ",\n";
			file << "    \"ShowPU\": " << BoolToJson(menuSettings.ShowPU) << ",\n";
			file << "    \"ShowHPText\": " << BoolToJson(menuSettings.ShowHPText) << ",\n";
			file << "    \"ShowGPText\": " << BoolToJson(menuSettings.ShowGPText) << ",\n";
			file << "    \"ShowPlatform\": " << BoolToJson(menuSettings.ShowPlatform) << ",\n";
			file << "    \"ShowTeamId\": " << BoolToJson(menuSettings.ShowTeamId) << ",\n";
			file << "    \"ShowPlayerSkeleton\": " << BoolToJson(menuSettings.ShowPlayerSkeleton) << ",\n";
			file << "    \"ShowServerStatusOverlay\": " << BoolToJson(menuSettings.ShowServerStatusOverlay) << ",\n";

			// Aimbot
			file << "    \"EnableAimbot\": " << BoolToJson(menuSettings.EnableAimbot) << ",\n";
			file << "    \"AimbotSmoothing\": " << BoolToJson(menuSettings.AimbotSmoothing) << ",\n";
			file << "    \"AimbotSmoothFactor\": " << FloatToJson(menuSettings.AimbotSmoothFactor) << ",\n";
			file << "    \"AimbotDrawLine\": " << BoolToJson(menuSettings.AimbotDrawLine) << ",\n";
			file << "    \"AimbotDrawFOV\": " << BoolToJson(menuSettings.AimbotDrawFOV) << ",\n";
			file << "    \"AimbotDrawCrosshair\": " << BoolToJson(menuSettings.AimbotDrawCrosshair) << ",\n";
			file << "    \"AimbotFOVRadius\": " << FloatToJson(menuSettings.AimbotFOVRadius) << ",\n";
			file << "    \"AimbotMaxDistance\": " << FloatToJson(menuSettings.AimbotMaxDistance) << ",\n";
			file << "    \"AimbotRequireHold\": " << BoolToJson(menuSettings.AimbotRequireHold) << ",\n";
			file << "    \"AimbotIgnoreDownedTargets\": " << BoolToJson(menuSettings.AimbotIgnoreDownedTargets) << ",\n";

			// Aimbot Hotkeys
			file << "    \"AimbotHoldKey_Keyboard\": " << IntToJson(menuSettings.AimbotHoldKey.Keyboard) << ",\n";
			file << "    \"AimbotHoldKey_Xbox\": " << IntToJson(menuSettings.AimbotHoldKey.Xbox) << ",\n";

			// Teleport to Kota
			file << "    \"EnableTeleportToKota\": " << BoolToJson(menuSettings.EnableTeleportToKota) << ",\n";
			file << "    \"EnableInfiniteObjects\": " << BoolToJson(menuSettings.EnableInfiniteObjects) << ",\n";
			file << "    \"TeleportToKotaKey_Keyboard\": " << IntToJson(menuSettings.TeleportToKotaKey.Keyboard) << ",\n";
			file << "    \"TeleportToKotaKey_Xbox\": " << IntToJson(menuSettings.TeleportToKotaKey.Xbox) << ",\n";

			// Custom Drop
			file << "    \"EnableCustomDrop\": " << BoolToJson(menuSettings.EnableCustomDrop) << ",\n";
			file << "    \"CustomDropQuantity\": " << IntToJson(menuSettings.CustomDropQuantity) << ",\n";
			file << "    \"CustomDropSelectedIndex\": " << IntToJson(menuSettings.CustomDropSelectedIndex) << ",\n";
			file << "    \"CustomDropSerialId\": " << IntToJson(menuSettings.CustomDropSerialId) << ",\n";
			file << "    \"CustomDropKey_Keyboard\": " << IntToJson(menuSettings.CustomDropKey.Keyboard) << ",\n";
			file << "    \"CustomDropKey_Xbox\": " << IntToJson(menuSettings.CustomDropKey.Xbox) << ",\n";

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
			file << "    \"CopySkillsModeType\": " << menuSettings.CopySkillsModeType << ",\n";

			// Generate Projectile
			file << "    \"EnableGenerateProjectile\": " << BoolToJson(menuSettings.EnableGenerateProjectile) << ",\n";
			file << "    \"GenerateProjectileKey_Keyboard\": " << IntToJson(menuSettings.GenerateProjectileKey.Keyboard) << ",\n";
			file << "    \"GenerateProjectileKey_Xbox\": " << IntToJson(menuSettings.GenerateProjectileKey.Xbox) << ",\n";

			// Reload Adjust Rates
			file << "    \"ReloadAdjustRate\": " << FloatToJson(menuSettings.ReloadAdjustRate) << ",\n";
			file << "    \"ReloadAdjustRate_RollSlot\": " << FloatToJson(menuSettings.ReloadAdjustRate_RollSlot) << ",\n";
			file << "    \"ReloadAdjustRate_WearBlueFlame\": " << FloatToJson(menuSettings.ReloadAdjustRate_WearBlueFlame) << ",\n";
			file << "    \"CvNoneDamageCurveValue\": " << FloatToJson(menuSettings.CvNoneDamageCurveValue) << ",\n";

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
			file << "    \"BulletTPIgnoreDownedTargets\": " << BoolToJson(menuSettings.BulletTPIgnoreDownedTargets) << ",\n";
			file << "    \"EnableCameraFOV\": " << BoolToJson(menuSettings.EnableCameraFOV) << ",\n";
			file << "    \"CameraFOV\": " << FloatToJson(menuSettings.CameraFOV) << ",\n";

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
			file << "    \"EnableSupplyMaxStackTo100\": " << BoolToJson(menuSettings.EnableSupplyMaxStackTo100) << ",\n";
			file << "    \"EnableFastPlusUltraCharge\": " << BoolToJson(menuSettings.EnableFastPlusUltraCharge) << ",\n";
			file << "    \"EnableNoCollision\": " << BoolToJson(menuSettings.EnableNoCollision) << ",\n";
			file << "    \"NoCollisionSpeed\": " << FloatToJson(menuSettings.NoCollisionSpeed) << ",\n";
			file << "    \"NoCollisionHoldKey_Keyboard\": " << IntToJson(menuSettings.NoCollisionHoldKey.Keyboard) << ",\n";
			file << "    \"NoCollisionHoldKey_Xbox\": " << IntToJson(menuSettings.NoCollisionHoldKey.Xbox) << ",\n";
			file << "    \"EnableClearInvincibleAuto\": " << BoolToJson(menuSettings.EnableClearInvincibleAuto) << ",\n";
			file << "    \"ClearInvincibleTargetMode\": " << IntToJson(menuSettings.ClearInvincibleTargetMode) << ",\n";
			file << "    \"ClearInvincibleMethod\": " << IntToJson(menuSettings.ClearInvincibleMethod) << ",\n";
			file << "    \"ClearInvincibleIgnoreFixed\": " << BoolToJson(menuSettings.ClearInvincibleIgnoreFixed) << ",\n";
			file << "    \"ClearInvincibleAttackId\": " << IntToJson(menuSettings.ClearInvincibleAttackId) << ",\n";
			file << "    \"ClearInvincibleIntervalMs\": " << IntToJson(menuSettings.ClearInvincibleIntervalMs) << ",\n";
			file << "    \"ClearInvincibleSelectedCharacterIndex\": " << IntToJson(menuSettings.ClearInvincibleSelectedCharacterIndex) << ",\n";
			file << "    \"ClearInvincibleTagBuffer\": \"" << EscapeJsonString(menuSettings.ClearInvincibleTagBuffer) << "\",\n";
			file << "    \"EnableAttackChainAuto\": " << BoolToJson(menuSettings.EnableAttackChainAuto) << ",\n";
			file << "    \"AttackChainIntervalMs\": " << IntToJson(menuSettings.AttackChainIntervalMs) << ",\n";
			file << "    \"AttackChainUseChainComboFlag\": " << BoolToJson(menuSettings.AttackChainUseChainComboFlag) << ",\n";
			file << "    \"AttackChainComboFlagTime\": " << FloatToJson(menuSettings.AttackChainComboFlagTime) << ",\n";
			file << "    \"AttackChainEnableShiftAttackActions\": " << BoolToJson(menuSettings.AttackChainEnableShiftAttackActions) << ",\n";
			file << "    \"AttackChainClearShiftActionAttackFlags\": " << BoolToJson(menuSettings.AttackChainClearShiftActionAttackFlags) << ",\n";
			file << "    \"AttackChainUseAnimationRate\": " << BoolToJson(menuSettings.AttackChainUseAnimationRate) << ",\n";
			file << "    \"AttackChainAnimationRate\": " << FloatToJson(menuSettings.AttackChainAnimationRate) << ",\n";
			file << "    \"AttackChainAnimationRateNagara\": " << BoolToJson(menuSettings.AttackChainAnimationRateNagara) << ",\n";
			file << "    \"AttackChainUsePhaseEndCondition\": " << BoolToJson(menuSettings.AttackChainUsePhaseEndCondition) << ",\n";
			file << "    \"AttackChainComboCommand\": " << BoolToJson(menuSettings.AttackChainComboCommand) << ",\n";
			file << "    \"AttackChainGrabbed\": " << BoolToJson(menuSettings.AttackChainGrabbed) << ",\n";
			file << "    \"AttackChainEndTimer\": " << FloatToJson(menuSettings.AttackChainEndTimer) << ",\n";
			file << "    \"AttackChainLanding\": " << BoolToJson(menuSettings.AttackChainLanding) << ",\n";
			file << "    \"AttackChainEndAnim\": " << BoolToJson(menuSettings.AttackChainEndAnim) << ",\n";
			file << "    \"AttackChainEndAnimSlot\": " << IntToJson(menuSettings.AttackChainEndAnimSlot) << ",\n";
			file << "    \"AttackChainFinishCurrentPhase\": " << BoolToJson(menuSettings.AttackChainFinishCurrentPhase) << ",\n";
			file << "    \"AttackChainTerminateAttackLayer\": " << BoolToJson(menuSettings.AttackChainTerminateAttackLayer) << ",\n";
			file << "    \"AttackChainEnableAimingActionCancel\": " << BoolToJson(menuSettings.AttackChainEnableAimingActionCancel) << ",\n";
			file << "    \"AttackChainActionCancelFlag\": " << IntToJson(menuSettings.AttackChainActionCancelFlag) << ",\n";
			file << "    \"AttackChainOwnerActionOnly\": " << BoolToJson(menuSettings.AttackChainOwnerActionOnly) << ",\n";
			file << "    \"AttackChainValidateNextReservedAction\": " << BoolToJson(menuSettings.AttackChainValidateNextReservedAction) << ",\n";
			file << "    \"EnableDownPowerAuto\": " << BoolToJson(menuSettings.EnableDownPowerAuto) << ",\n";
			file << "    \"DownPowerIntervalMs\": " << IntToJson(menuSettings.DownPowerIntervalMs) << ",\n";
			file << "    \"DownPowerTargetMode\": " << IntToJson(menuSettings.DownPowerTargetMode) << ",\n";
			file << "    \"DownPowerSelectedCharacterIndex\": " << IntToJson(menuSettings.DownPowerSelectedCharacterIndex) << ",\n";
			file << "    \"DownPowerPatchDamageParams\": " << BoolToJson(menuSettings.DownPowerPatchDamageParams) << ",\n";
			file << "    \"DownPowerDamageType\": " << IntToJson(menuSettings.DownPowerDamageType) << ",\n";
			file << "    \"DownPowerIncludeNoActionDamage\": " << BoolToJson(menuSettings.DownPowerIncludeNoActionDamage) << ",\n";
			file << "    \"DownPowerOverrideRecoverSpan\": " << BoolToJson(menuSettings.DownPowerOverrideRecoverSpan) << ",\n";
			file << "    \"DownPowerRecoverSpan\": " << FloatToJson(menuSettings.DownPowerRecoverSpan) << ",\n";
			file << "    \"DownPowerApplyDurableRates\": " << BoolToJson(menuSettings.DownPowerApplyDurableRates) << ",\n";
			file << "    \"DownPowerDurableRate\": " << FloatToJson(menuSettings.DownPowerDurableRate) << ",\n";
			file << "    \"DownPowerDurableAttackActionRate\": " << FloatToJson(menuSettings.DownPowerDurableAttackActionRate) << ",\n";
			file << "    \"DownPowerDurableTakeDamageRate\": " << FloatToJson(menuSettings.DownPowerDurableTakeDamageRate) << ",\n";
			file << "    \"DownPowerDurableSpecialActionRate\": " << FloatToJson(menuSettings.DownPowerDurableSpecialActionRate) << ",\n";
			file << "    \"DownPowerDurableRollSlotRate\": " << FloatToJson(menuSettings.DownPowerDurableRollSlotRate) << ",\n";
			file << "    \"DownPowerDurableTakeDamageRollSlotRate\": " << FloatToJson(menuSettings.DownPowerDurableTakeDamageRollSlotRate) << ",\n";
			file << "    \"DownPowerApplyBreakDownSuperArmorRate\": " << BoolToJson(menuSettings.DownPowerApplyBreakDownSuperArmorRate) << ",\n";
			file << "    \"DownPowerBreakDownSuperArmorRate\": " << FloatToJson(menuSettings.DownPowerBreakDownSuperArmorRate) << ",\n";
			file << "    \"DownPowerDisableTargetSuperArmor\": " << BoolToJson(menuSettings.DownPowerDisableTargetSuperArmor) << ",\n";
			file << "    \"Ch202MeleeAChaseHeightOffset\": " << FloatToJson(menuSettings.Ch202MeleeAChaseHeightOffset) << ",\n";
			file << "    \"Ch202MeleeAChaseStartSpeedMax\": " << FloatToJson(menuSettings.Ch202MeleeAChaseStartSpeedMax) << ",\n";
			file << "    \"Ch202MeleeAChaseStartSpeedMin\": " << FloatToJson(menuSettings.Ch202MeleeAChaseStartSpeedMin) << ",\n";
			file << "    \"Ch202MeleeAChaseLastSpeed\": " << FloatToJson(menuSettings.Ch202MeleeAChaseLastSpeed) << ",\n";
			file << "    \"Ch202MeleeAChaseSpeedSpan\": " << FloatToJson(menuSettings.Ch202MeleeAChaseSpeedSpan) << ",\n";
			file << "    \"Ch202MeleeAChaseSpan\": " << FloatToJson(menuSettings.Ch202MeleeAChaseSpan) << ",\n";
			file << "    \"Ch202MeleeADistance\": " << FloatToJson(menuSettings.Ch202MeleeADistance) << ",\n";
			file << "    \"Ch202MeleeABreakTargetSpeed\": " << FloatToJson(menuSettings.Ch202MeleeABreakTargetSpeed) << ",\n";
			file << "    \"Ch202MeleeABreakTargetSpan\": " << FloatToJson(menuSettings.Ch202MeleeABreakTargetSpan) << ",\n";
			file << "    \"Ch202Unique1HoldTime\": " << FloatToJson(menuSettings.Ch202Unique1HoldTime) << ",\n";
			file << "    \"Ch202Unique2NeedFieldTime\": " << FloatToJson(menuSettings.Ch202Unique2NeedFieldTime) << ",\n";
			file << "    \"Ch202Unique2HoldTime\": " << FloatToJson(menuSettings.Ch202Unique2HoldTime) << ",\n";
			file << "    \"Ch202Unique2MoveTime\": " << FloatToJson(menuSettings.Ch202Unique2MoveTime) << ",\n";
			file << "    \"Ch202Unique2MoveSpeedMin\": " << FloatToJson(menuSettings.Ch202Unique2MoveSpeedMin) << ",\n";
			file << "    \"Ch202Unique2MoveSpeedMax\": " << FloatToJson(menuSettings.Ch202Unique2MoveSpeedMax) << ",\n";
			file << "    \"Ch202Unique2QuintupleRiseSlideSpeed\": " << FloatToJson(menuSettings.Ch202Unique2QuintupleRiseSlideSpeed) << ",\n";
			file << "    \"Ch202Unique2QuintupleRiseSlideSpan\": " << FloatToJson(menuSettings.Ch202Unique2QuintupleRiseSlideSpan) << ",\n";
			file << "    \"Ch202Unique2QuintupleFallSlideSpeed\": " << FloatToJson(menuSettings.Ch202Unique2QuintupleFallSlideSpeed) << ",\n";
			file << "    \"Ch202Unique2QuintupleFallSlideSpan\": " << FloatToJson(menuSettings.Ch202Unique2QuintupleFallSlideSpan) << ",\n";
			file << "    \"Ch202Unique2AfterLevel\": " << IntToJson(menuSettings.Ch202Unique2AfterLevel) << ",\n";
			file << "    \"Ch202Unique2QuintupleAllAmmoConsumed\": " << BoolToJson(menuSettings.Ch202Unique2QuintupleAllAmmoConsumed) << ",\n";
			file << "    \"Ch202U3InitTransMissionLevel\": " << IntToJson(menuSettings.Ch202U3InitTransMissionLevel) << ",\n";
			file << "    \"Ch202U3EndDistanceOfFollowing\": " << FloatToJson(menuSettings.Ch202U3EndDistanceOfFollowing) << ",\n";
			file << "    \"Ch202U3MoveSpeedStart\": " << FloatToJson(menuSettings.Ch202U3MoveSpeedStart) << ",\n";
			file << "    \"Ch202U3MoveSpeedEnd\": " << FloatToJson(menuSettings.Ch202U3MoveSpeedEnd) << ",\n";
			file << "    \"Ch202U3SpeedChangeTime\": " << FloatToJson(menuSettings.Ch202U3SpeedChangeTime) << ",\n";
			file << "    \"Ch202U3ExitSpeedStart\": " << FloatToJson(menuSettings.Ch202U3ExitSpeedStart) << ",\n";
			file << "    \"Ch202U3ExitSpeedEnd\": " << FloatToJson(menuSettings.Ch202U3ExitSpeedEnd) << ",\n";
			file << "    \"Ch202U3ExitChangeTime\": " << FloatToJson(menuSettings.Ch202U3ExitChangeTime) << ",\n";
			file << "    \"Ch202U3LimitCount\": " << IntToJson(menuSettings.Ch202U3LimitCount) << ",\n";
			file << "    \"Ch202U3MoveMagazinePercentMin\": " << FloatToJson(menuSettings.Ch202U3MoveMagazinePercentMin) << ",\n";
			file << "    \"Ch202U3MoveMagazinePercentMax\": " << FloatToJson(menuSettings.Ch202U3MoveMagazinePercentMax) << ",\n";
			file << "    \"Ch202U3PunchMagazinePercentMin\": " << FloatToJson(menuSettings.Ch202U3PunchMagazinePercentMin) << ",\n";
			file << "    \"Ch202U3PunchMagazinePercentMax\": " << FloatToJson(menuSettings.Ch202U3PunchMagazinePercentMax) << ",\n";
			file << "    \"Ch202U3RecoverMagazineBase\": " << IntToJson(menuSettings.Ch202U3RecoverMagazineBase) << ",\n";
			file << "    \"Ch202U3RecoverMagazineAdd\": " << IntToJson(menuSettings.Ch202U3RecoverMagazineAdd) << ",\n";
			file << "    \"Ch202U3DelayCancelTimer\": " << FloatToJson(menuSettings.Ch202U3DelayCancelTimer) << ",\n";
			file << "    \"Ch202U3LockOnSec\": " << FloatToJson(menuSettings.Ch202U3LockOnSec) << ",\n";
			file << "    \"Ch202U3LockOnMinSec\": " << FloatToJson(menuSettings.Ch202U3LockOnMinSec) << ",\n";
			file << "    \"Ch202U3LockOnRange\": " << FloatToJson(menuSettings.Ch202U3LockOnRange) << ",\n";
			file << "    \"Ch202U3LockOnAttackTargetCheckType\": " << IntToJson(menuSettings.Ch202U3LockOnAttackTargetCheckType) << ",\n";
			file << "    \"Ch202U3CurveHorizontalDistanceMin\": " << FloatToJson(menuSettings.Ch202U3CurveHorizontalDistanceMin) << ",\n";
			file << "    \"Ch202U3CurveHorizontalDistanceMax\": " << FloatToJson(menuSettings.Ch202U3CurveHorizontalDistanceMax) << ",\n";
			file << "    \"Ch202U3MiddleUpOffset\": " << FloatToJson(menuSettings.Ch202U3MiddleUpOffset) << ",\n";
			file << "    \"Ch202U3TargetOffset\": " << FloatToJson(menuSettings.Ch202U3TargetOffset) << ",\n";
			file << "    \"Ch202U3SplitDistance\": " << FloatToJson(menuSettings.Ch202U3SplitDistance) << ",\n";
			file << "    \"Ch202U3SplitLerpRate\": " << FloatToJson(menuSettings.Ch202U3SplitLerpRate) << ",\n";
			file << "    \"Ch202U3ControlPointsRate\": " << FloatToJson(menuSettings.Ch202U3ControlPointsRate) << ",\n";
			file << "    \"Ch202U3MinDetroitRange\": " << FloatToJson(menuSettings.Ch202U3MinDetroitRange) << ",\n";
			file << "    \"Ch202U3MinDetroitSpeed\": " << FloatToJson(menuSettings.Ch202U3MinDetroitSpeed) << ",\n";
			file << "    \"Ch202U3MaxDetroitRange\": " << FloatToJson(menuSettings.Ch202U3MaxDetroitRange) << ",\n";
			file << "    \"Ch202U3MaxDetroitSpeed\": " << FloatToJson(menuSettings.Ch202U3MaxDetroitSpeed) << ",\n";
			file << "    \"Ch202U3MiddleOffsetX\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetX) << ",\n";
			file << "    \"Ch202U3MiddleOffsetY\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetY) << ",\n";
			file << "    \"Ch202U3MiddleOffsetZ\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetZ) << ",\n";
			file << "    \"Ch202U3MiddleOffsetAerialX\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetAerialX) << ",\n";
			file << "    \"Ch202U3MiddleOffsetAerialY\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetAerialY) << ",\n";
			file << "    \"Ch202U3MiddleOffsetAerialZ\": " << FloatToJson(menuSettings.Ch202U3MiddleOffsetAerialZ) << ",\n";
			file << "    \"Ch202SpecialHoldTime\": " << FloatToJson(menuSettings.Ch202SpecialHoldTime) << ",\n";
			file << "    \"Ch202SpecialActivationTime\": " << FloatToJson(menuSettings.Ch202SpecialActivationTime) << ",\n";
			file << "    \"Ch202SpecialSmokeMagazineCost\": " << IntToJson(menuSettings.Ch202SpecialSmokeMagazineCost) << ",\n";
			file << "    \"Ch202SpecialLegCount\": " << IntToJson(menuSettings.Ch202SpecialLegCount) << ",\n";
			file << "    \"Ch202SpecialJumpVerticalSpan\": " << FloatToJson(menuSettings.Ch202SpecialJumpVerticalSpan) << ",\n";
			file << "    \"Ch202SpecialJumpVerticalHeight\": " << FloatToJson(menuSettings.Ch202SpecialJumpVerticalHeight) << ",\n";
			file << "    \"Ch202SpecialJumpForwardSpan\": " << FloatToJson(menuSettings.Ch202SpecialJumpForwardSpan) << ",\n";
			file << "    \"Ch202SpecialJumpForwardHeight\": " << FloatToJson(menuSettings.Ch202SpecialJumpForwardHeight) << ",\n";
			file << "    \"Ch202SpecialJumpForwardInitialSpeedH\": " << FloatToJson(menuSettings.Ch202SpecialJumpForwardInitialSpeedH) << ",\n";
			file << "    \"Ch202SpecialJumpForwardLastSpeedH\": " << FloatToJson(menuSettings.Ch202SpecialJumpForwardLastSpeedH) << ",\n";
			file << "    \"Ch202SpecialJumpForwardAccelSpanH\": " << FloatToJson(menuSettings.Ch202SpecialJumpForwardAccelSpanH) << ",\n";
			file << "    \"Ch202SpecialWallJumpSpan\": " << FloatToJson(menuSettings.Ch202SpecialWallJumpSpan) << ",\n";
			file << "    \"Ch202SpecialWallJumpHeight\": " << FloatToJson(menuSettings.Ch202SpecialWallJumpHeight) << ",\n";
			file << "    \"Ch202SpecialWallJumpInitialSpeed\": " << FloatToJson(menuSettings.Ch202SpecialWallJumpInitialSpeed) << ",\n";
			file << "    \"Ch202SpecialWallJumpLastSpeed\": " << FloatToJson(menuSettings.Ch202SpecialWallJumpLastSpeed) << ",\n";
			file << "    \"Ch202ConditionAnimationMultiplyRate\": " << FloatToJson(menuSettings.Ch202ConditionAnimationMultiplyRate) << ",\n";
			file << "    \"Ch202ConditionMoveMultiplyRate\": " << FloatToJson(menuSettings.Ch202ConditionMoveMultiplyRate) << ",\n";
			file << "    \"Ch202ConditionJumpAdjustMultiplyRate\": " << FloatToJson(menuSettings.Ch202ConditionJumpAdjustMultiplyRate) << ",\n";
			file << "    \"Ch025U2MoveSpeedXY\": " << FloatToJson(menuSettings.Ch025U2MoveSpeedXY) << ",\n";
			file << "    \"Ch025U2MoveSpeedZUp\": " << FloatToJson(menuSettings.Ch025U2MoveSpeedZUp) << ",\n";
			file << "    \"Ch025U2MoveSpeedZDown\": " << FloatToJson(menuSettings.Ch025U2MoveSpeedZDown) << ",\n";
			file << "    \"Ch025U2ReinforceMoveSpeedXY\": " << FloatToJson(menuSettings.Ch025U2ReinforceMoveSpeedXY) << ",\n";
			file << "    \"Ch025U2ReinforceMoveSpeedZUp\": " << FloatToJson(menuSettings.Ch025U2ReinforceMoveSpeedZUp) << ",\n";
			file << "    \"Ch025U2ReinforceMoveSpeedZDown\": " << FloatToJson(menuSettings.Ch025U2ReinforceMoveSpeedZDown) << ",\n";
			file << "    \"Ch025U2ActivityLimitTime\": " << FloatToJson(menuSettings.Ch025U2ActivityLimitTime) << ",\n";
			file << "    \"Ch025U2ActivityLowestTime\": " << FloatToJson(menuSettings.Ch025U2ActivityLowestTime) << ",\n";
			file << "    \"Ch025U2ShockWaveMagRateL1\": " << FloatToJson(menuSettings.Ch025U2ShockWaveMagRateL1) << ",\n";
			file << "    \"Ch025U2ShockWaveMagRateL2\": " << FloatToJson(menuSettings.Ch025U2ShockWaveMagRateL2) << ",\n";
			file << "    \"Ch025U2ShockWaveMagRateL3\": " << FloatToJson(menuSettings.Ch025U2ShockWaveMagRateL3) << ",\n";
			file << "    \"Ch025U2ActionMagRateL1\": " << FloatToJson(menuSettings.Ch025U2ActionMagRateL1) << ",\n";
			file << "    \"Ch025U2ActionMagRateL2\": " << FloatToJson(menuSettings.Ch025U2ActionMagRateL2) << ",\n";
			file << "    \"Ch025U2ActionMagRateL3\": " << FloatToJson(menuSettings.Ch025U2ActionMagRateL3) << ",\n";
			file << "    \"Ch025U3InitialVerticalSpeed\": " << FloatToJson(menuSettings.Ch025U3InitialVerticalSpeed) << ",\n";
			file << "    \"Ch025U3LastVerticalSpeed\": " << FloatToJson(menuSettings.Ch025U3LastVerticalSpeed) << ",\n";
			file << "    \"Ch025U3Span\": " << FloatToJson(menuSettings.Ch025U3Span) << ",\n";
			file << "    \"Ch025U3WaitTurnTime\": " << FloatToJson(menuSettings.Ch025U3WaitTurnTime) << ",\n";
			file << "    \"Ch025U3ApplyInertiaSpawn\": " << FloatToJson(menuSettings.Ch025U3ApplyInertiaSpawn) << ",\n";
			file << "    \"Ch025U3ApplyInertiaSpawnHRate\": " << FloatToJson(menuSettings.Ch025U3ApplyInertiaSpawnHRate) << ",\n";
			file << "    \"Ch025U3ApplyInertiaSpawnVRate\": " << FloatToJson(menuSettings.Ch025U3ApplyInertiaSpawnVRate) << ",\n";
			file << "    \"Ch025U3ConditionTimeL1\": " << FloatToJson(menuSettings.Ch025U3ConditionTimeL1) << ",\n";
			file << "    \"Ch025U3ConditionTimeL2\": " << FloatToJson(menuSettings.Ch025U3ConditionTimeL2) << ",\n";
			file << "    \"Ch025U3ConditionTimeL3\": " << FloatToJson(menuSettings.Ch025U3ConditionTimeL3) << ",\n";
			file << "    \"Ch025U3BarrierValueL1\": " << FloatToJson(menuSettings.Ch025U3BarrierValueL1) << ",\n";
			file << "    \"Ch025U3BarrierValueL2\": " << FloatToJson(menuSettings.Ch025U3BarrierValueL2) << ",\n";
			file << "    \"Ch025U3BarrierValueL3\": " << FloatToJson(menuSettings.Ch025U3BarrierValueL3) << ",\n";
			file << "    \"Ch025U3BarrierValueAllyL1\": " << FloatToJson(menuSettings.Ch025U3BarrierValueAllyL1) << ",\n";
			file << "    \"Ch025U3BarrierValueAllyL2\": " << FloatToJson(menuSettings.Ch025U3BarrierValueAllyL2) << ",\n";
			file << "    \"Ch025U3BarrierValueAllyL3\": " << FloatToJson(menuSettings.Ch025U3BarrierValueAllyL3) << ",\n";
			file << "    \"Ch025U3TurnAngleDeg\": " << FloatToJson(menuSettings.Ch025U3TurnAngleDeg) << ",\n";
			file << "    \"Ch025U3TurnTime\": " << FloatToJson(menuSettings.Ch025U3TurnTime) << ",\n";
			file << "    \"Ch025U3TurnBlendExp\": " << FloatToJson(menuSettings.Ch025U3TurnBlendExp) << ",\n";
			file << "    \"Ch025U3TurnSteps\": " << menuSettings.Ch025U3TurnSteps << ",\n";
			file << "    \"Ch025U3ManyAllyBarrierRate\": " << FloatToJson(menuSettings.Ch025U3ManyAllyBarrierRate) << ",\n";
			file << "    \"Ch025U3OwnerBarrierListValue\": " << FloatToJson(menuSettings.Ch025U3OwnerBarrierListValue) << ",\n";
			file << "    \"Ch025U3AllyBarrierListValue\": " << FloatToJson(menuSettings.Ch025U3AllyBarrierListValue) << ",\n";
			file << "    \"Ch025SpecialLowGravityTime\": " << FloatToJson(menuSettings.Ch025SpecialLowGravityTime) << ",\n";
			file << "    \"Ch025SpecialStartSpeed\": " << FloatToJson(menuSettings.Ch025SpecialStartSpeed) << ",\n";
			file << "    \"Ch025SpecialMiddleSpeed\": " << FloatToJson(menuSettings.Ch025SpecialMiddleSpeed) << ",\n";
			file << "    \"Ch025SpecialEndSpeed\": " << FloatToJson(menuSettings.Ch025SpecialEndSpeed) << ",\n";
			file << "    \"Ch025SpecialStartSpan\": " << FloatToJson(menuSettings.Ch025SpecialStartSpan) << ",\n";
			file << "    \"Ch025SpecialEndSpan\": " << FloatToJson(menuSettings.Ch025SpecialEndSpan) << ",\n";
			file << "    \"Ch025SpecialDashTime\": " << FloatToJson(menuSettings.Ch025SpecialDashTime) << ",\n";
			file << "    \"Ch025SpecialStartVerticalSpeed\": " << FloatToJson(menuSettings.Ch025SpecialStartVerticalSpeed) << ",\n";
			file << "    \"Ch025SpecialMiddleVerticalSpeed\": " << FloatToJson(menuSettings.Ch025SpecialMiddleVerticalSpeed) << ",\n";
			file << "    \"Ch025SpecialEndVerticalSpeed\": " << FloatToJson(menuSettings.Ch025SpecialEndVerticalSpeed) << ",\n";
			file << "    \"Ch025SpecialStartVerticalSpan\": " << FloatToJson(menuSettings.Ch025SpecialStartVerticalSpan) << ",\n";
			file << "    \"Ch025SpecialEndVerticalSpan\": " << FloatToJson(menuSettings.Ch025SpecialEndVerticalSpan) << ",\n";
			file << "    \"Ch025BarrierMaxTime\": " << FloatToJson(menuSettings.Ch025BarrierMaxTime) << ",\n";
			file << "    \"Ch025BarrierDurability\": " << FloatToJson(menuSettings.Ch025BarrierDurability) << ",\n";
			file << "    \"Ch025BarrierCopyRate\": " << FloatToJson(menuSettings.Ch025BarrierCopyRate) << ",\n";
			file << "    \"Ch025RecoveryHealthValue\": " << FloatToJson(menuSettings.Ch025RecoveryHealthValue) << "\n";

			file << "  },\n";

			// ========== HACK SETTINGS ==========
			file << "  \"HackSettings\": {\n";
			file << "    \"EnableInvincible\": " << BoolToJson(hackSettings.EnableInvincible) << ",\n";
			file << "    \"EnableInfiniteHealth\": " << BoolToJson(hackSettings.EnableInfiniteHealth) << ",\n";
			file << "    \"EnableInfiniteBarrier\": " << BoolToJson(hackSettings.EnableInfiniteBarrier) << ",\n";
			file << "    \"EnableInfinitePlusUltra\": " << BoolToJson(hackSettings.EnableInfinitePlusUltra) << ",\n";
			file << "    \"EnableFullBuff\": " << BoolToJson(hackSettings.EnableFullBuff) << ",\n";
			file << "    \"EnableHideKills\": " << BoolToJson(hackSettings.EnableHideKills) << ",\n";
			file << "    \"EnableInfiniteSkills\": " << BoolToJson(hackSettings.EnableInfiniteSkills) << ",\n";
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
			file << "    \"CharCondition_AutoExecute\": " << BoolToJson(hackSettings.CharCondition_AutoExecute) << ",\n";
			file << "    \"CharCondition_SelectedConditionId\": " << IntToJson(hackSettings.CharCondition_SelectedConditionId) << ",\n";
			file << "    \"CharCondition_ApplyMode\": " << IntToJson(hackSettings.CharCondition_ApplyMode) << ",\n";
			file << "    \"CharCondition_Level\": " << IntToJson(hackSettings.CharCondition_Level) << ",\n";
			file << "    \"CharCondition_Duration\": " << FloatToJson(hackSettings.CharCondition_Duration) << ",\n";
			file << "    \"CharCondition_Value\": " << FloatToJson(hackSettings.CharCondition_Value) << ",\n";
			file << "    \"CharCondition_Interval\": " << FloatToJson(hackSettings.CharCondition_Interval) << ",\n";
			file << "    \"CharCondition_SubLevel\": " << IntToJson(hackSettings.CharCondition_SubLevel) << ",\n";
			file << "    \"CharCondition_TimeOverwrite\": " << BoolToJson(hackSettings.CharCondition_TimeOverwrite) << ",\n";
			file << "    \"ChangePlayerNameBuffer\": \"" << EscapeJsonString(hackSettings.ChangePlayerNameBuffer) << "\",\n";
			file << "    \"BackendPlatformCode\": " << IntToJson(hackSettings.BackendPlatformCode) << ",\n";
			file << "    \"FakePlatformBuffer\": \"" << EscapeJsonString(hackSettings.FakePlatformBuffer) << "\",\n";
			file << "    \"BuyLicenseExpCount\": " << IntToJson(hackSettings.BuyLicenseExpCount) << ",\n";
			file << "    \"LicenseClaimSeasonCode\": " << IntToJson(hackSettings.LicenseClaimSeasonCode) << ",\n";
			file << "    \"LicenseClaimFreeRank\": " << IntToJson(hackSettings.LicenseClaimFreeRank) << ",\n";
			file << "    \"LicenseClaimPremiumRank\": " << IntToJson(hackSettings.LicenseClaimPremiumRank) << ",\n";
			file << "    \"LicenseClaimSpecialRank\": " << IntToJson(hackSettings.LicenseClaimSpecialRank) << ",\n";
			file << "    \"LicenseClaimRepeatCount\": " << IntToJson(hackSettings.LicenseClaimRepeatCount) << ",\n";
			file << "    \"LicenseClaimDelayMs\": " << IntToJson(hackSettings.LicenseClaimDelayMs) << "\n";
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
		emptyMenu.EnableMenuBackgroundVideo = true;
		emptyMenu.EnableStreamProofMenu = false;
		emptyMenu.EnablePlayerESP = false;
		emptyMenu.Player_Box = false;
		emptyMenu.Player_Health = false;
		emptyMenu.Player_Distance = false;
		emptyMenu.Player_Name = false;
		emptyMenu.ShowHP = false;
		emptyMenu.ShowGP = false;
		emptyMenu.ShowPU = false;
		emptyMenu.ShowHPText = false;
		emptyMenu.ShowGPText = false;
		emptyMenu.ShowPlatform = false;
		emptyMenu.ShowTeamId = false;
		emptyMenu.ShowPlayerSkeleton = false;
		emptyMenu.ShowServerStatusOverlay = false;

		emptyMenu.EnableAimbot = false;
		emptyMenu.AimbotSmoothing = false;
		emptyMenu.AimbotDrawLine = false;
		emptyMenu.AimbotDrawFOV = false;
		emptyMenu.AimbotDrawCrosshair = false;
		emptyMenu.AimbotRequireHold = false;
		emptyMenu.AimbotIgnoreDownedTargets = true;

		emptyMenu.EnableTeleportToKota = false;
		emptyMenu.EnableInfiniteObjects = false;
		emptyMenu.EnableTransformIntoRandomESP = false;
		emptyMenu.EnableDuplicateIntoImitationRandomESP = false;
		emptyMenu.EnableCopySkillsFromNearestEnemy = false;
		emptyMenu.CopySkillsSetCopySkill = false;
		emptyMenu.CopySkillsUseOwnerCharacterLevel = false;
		emptyMenu.CopySkillsModeType = 0;
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
		emptyMenu.BulletTPIgnoreDownedTargets = true;
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
		emptyMenu.AimbotMaxDistance = 0.0f;
		emptyMenu.DuplicateImitationLifeTime = 0.0f;
		emptyMenu.ReloadAdjustRate = 1.0f;  // Keep at 1.0f (normal speed) not 0
		emptyMenu.ReloadAdjustRate_RollSlot = 1.0f;  // Keep at 1.0f
		emptyMenu.ReloadAdjustRate_WearBlueFlame = 1.0f;  // Keep at 1.0f
		emptyMenu.CvNoneDamageCurveValue = 1.0f;
		emptyMenu.BulletTP_FOVRadius = 0.0f;
		emptyMenu.BulletTP_MaxDistance = 0.0f;
		emptyMenu.CameraFOV = 90.0f;
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
		emptyMenu.CustomDropKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.TransformIntoRandomESPKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.DuplicateIntoImitationRandomESPKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.CopySkillsFromNearestEnemyKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.GenerateProjectileKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.SetInvincibleKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.RebuildMyselfKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.NoCollisionHoldKey = ImGuiMenu::HotkeySet(0, 0);
		emptyMenu.EnableFastPlusUltraCharge = false;
		emptyMenu.EnableNoCollision = false;
		emptyMenu.EnableCameraFOV = false;
		emptyMenu.NoCollisionSpeed = 0.0f;
		emptyMenu.EnableClearInvincibleAuto = false;
		emptyMenu.ClearInvincibleTargetMode = 1;
		emptyMenu.ClearInvincibleMethod = 0;
		emptyMenu.ClearInvincibleIgnoreFixed = true;
		emptyMenu.ClearInvincibleAttackId = 4;
		emptyMenu.ClearInvincibleIntervalMs = 250;
		emptyMenu.ClearInvincibleSelectedCharacterIndex = -1;
		emptyMenu.ClearInvincibleTagBuffer[0] = '\0';
		emptyMenu.EnableAttackChainAuto = false;
		emptyMenu.AttackChainIntervalMs = 50;
		emptyMenu.AttackChainUseChainComboFlag = true;
		emptyMenu.AttackChainComboFlagTime = 0.25f;
		emptyMenu.AttackChainEnableShiftAttackActions = true;
		emptyMenu.AttackChainClearShiftActionAttackFlags = true;
		emptyMenu.AttackChainUseAnimationRate = true;
		emptyMenu.AttackChainAnimationRate = 2.0f;
		emptyMenu.AttackChainAnimationRateNagara = true;
		emptyMenu.AttackChainUsePhaseEndCondition = false;
		emptyMenu.AttackChainComboCommand = true;
		emptyMenu.AttackChainGrabbed = false;
		emptyMenu.AttackChainEndTimer = 0.0f;
		emptyMenu.AttackChainLanding = false;
		emptyMenu.AttackChainEndAnim = true;
		emptyMenu.AttackChainEndAnimSlot = 0;
		emptyMenu.AttackChainFinishCurrentPhase = false;
		emptyMenu.AttackChainTerminateAttackLayer = false;
		emptyMenu.AttackChainEnableAimingActionCancel = true;
		emptyMenu.AttackChainActionCancelFlag = 255;
		emptyMenu.AttackChainOwnerActionOnly = false;
		emptyMenu.AttackChainValidateNextReservedAction = true;
		emptyMenu.EnableDownPowerAuto = false;
		emptyMenu.DownPowerIntervalMs = 100;
		emptyMenu.DownPowerTargetMode = 1;
		emptyMenu.DownPowerSelectedCharacterIndex = -1;
		emptyMenu.DownPowerPatchDamageParams = true;
		emptyMenu.DownPowerDamageType = 16;
		emptyMenu.DownPowerIncludeNoActionDamage = true;
		emptyMenu.DownPowerOverrideRecoverSpan = false;
		emptyMenu.DownPowerRecoverSpan = 0.0f;
		emptyMenu.DownPowerApplyDurableRates = true;
		emptyMenu.DownPowerDurableRate = 1.0f;
		emptyMenu.DownPowerDurableAttackActionRate = 1.0f;
		emptyMenu.DownPowerDurableTakeDamageRate = 1.0f;
		emptyMenu.DownPowerDurableSpecialActionRate = 1.0f;
		emptyMenu.DownPowerDurableRollSlotRate = 1.0f;
		emptyMenu.DownPowerDurableTakeDamageRollSlotRate = 1.0f;
		emptyMenu.DownPowerApplyBreakDownSuperArmorRate = true;
		emptyMenu.DownPowerBreakDownSuperArmorRate = 1.0f;
		emptyMenu.DownPowerDisableTargetSuperArmor = true;
		emptyMenu.Ch202MeleeAChaseHeightOffset = 0.0f;
		emptyMenu.Ch202MeleeAChaseStartSpeedMax = 1.0f;
		emptyMenu.Ch202MeleeAChaseStartSpeedMin = 1.0f;
		emptyMenu.Ch202MeleeAChaseLastSpeed = 1.0f;
		emptyMenu.Ch202MeleeAChaseSpeedSpan = 1.0f;
		emptyMenu.Ch202MeleeAChaseSpan = 1.0f;
		emptyMenu.Ch202MeleeADistance = 1.0f;
		emptyMenu.Ch202MeleeABreakTargetSpeed = 1.0f;
		emptyMenu.Ch202MeleeABreakTargetSpan = 1.0f;
		emptyMenu.Ch202Unique1HoldTime = 1.0f;
		emptyMenu.Ch202Unique2NeedFieldTime = 1.0f;
		emptyMenu.Ch202Unique2HoldTime = 1.0f;
		emptyMenu.Ch202Unique2MoveTime = 1.0f;
		emptyMenu.Ch202Unique2MoveSpeedMin = 1.0f;
		emptyMenu.Ch202Unique2MoveSpeedMax = 1.0f;
		emptyMenu.Ch202Unique2QuintupleRiseSlideSpeed = 1.0f;
		emptyMenu.Ch202Unique2QuintupleRiseSlideSpan = 1.0f;
		emptyMenu.Ch202Unique2QuintupleFallSlideSpeed = 1.0f;
		emptyMenu.Ch202Unique2QuintupleFallSlideSpan = 1.0f;
		emptyMenu.Ch202Unique2AfterLevel = 1;
		emptyMenu.Ch202Unique2QuintupleAllAmmoConsumed = false;
		emptyMenu.Ch202U3InitTransMissionLevel = 1;
		emptyMenu.Ch202U3EndDistanceOfFollowing = 1.0f;
		emptyMenu.Ch202U3MoveSpeedStart = 1.0f;
		emptyMenu.Ch202U3MoveSpeedEnd = 1.0f;
		emptyMenu.Ch202U3SpeedChangeTime = 1.0f;
		emptyMenu.Ch202U3ExitSpeedStart = 1.0f;
		emptyMenu.Ch202U3ExitSpeedEnd = 1.0f;
		emptyMenu.Ch202U3ExitChangeTime = 1.0f;
		emptyMenu.Ch202U3LimitCount = 1;
		emptyMenu.Ch202U3MoveMagazinePercentMin = 1.0f;
		emptyMenu.Ch202U3MoveMagazinePercentMax = 1.0f;
		emptyMenu.Ch202U3PunchMagazinePercentMin = 1.0f;
		emptyMenu.Ch202U3PunchMagazinePercentMax = 1.0f;
		emptyMenu.Ch202U3RecoverMagazineBase = 1;
		emptyMenu.Ch202U3RecoverMagazineAdd = 1;
		emptyMenu.Ch202U3DelayCancelTimer = 1.0f;
		emptyMenu.Ch202U3LockOnSec = 1.0f;
		emptyMenu.Ch202U3LockOnMinSec = 0.1f;
		emptyMenu.Ch202U3LockOnRange = 1000.0f;
		emptyMenu.Ch202U3LockOnAttackTargetCheckType = 0;
		emptyMenu.Ch202U3CurveHorizontalDistanceMin = 1.0f;
		emptyMenu.Ch202U3CurveHorizontalDistanceMax = 1.0f;
		emptyMenu.Ch202U3MiddleUpOffset = 1.0f;
		emptyMenu.Ch202U3TargetOffset = 1.0f;
		emptyMenu.Ch202U3SplitDistance = 1.0f;
		emptyMenu.Ch202U3SplitLerpRate = 1.0f;
		emptyMenu.Ch202U3ControlPointsRate = 1.0f;
		emptyMenu.Ch202U3MinDetroitRange = 1.0f;
		emptyMenu.Ch202U3MinDetroitSpeed = 1.0f;
		emptyMenu.Ch202U3MaxDetroitRange = 1.0f;
		emptyMenu.Ch202U3MaxDetroitSpeed = 1.0f;
		emptyMenu.Ch202U3MiddleOffsetX = 0.0f;
		emptyMenu.Ch202U3MiddleOffsetY = 0.0f;
		emptyMenu.Ch202U3MiddleOffsetZ = 0.0f;
		emptyMenu.Ch202U3MiddleOffsetAerialX = 0.0f;
		emptyMenu.Ch202U3MiddleOffsetAerialY = 0.0f;
		emptyMenu.Ch202U3MiddleOffsetAerialZ = 0.0f;
		emptyMenu.Ch202SpecialHoldTime = 1.0f;
		emptyMenu.Ch202SpecialActivationTime = 1.0f;
		emptyMenu.Ch202SpecialSmokeMagazineCost = 1;
		emptyMenu.Ch202SpecialLegCount = 1;
		emptyMenu.Ch202SpecialJumpVerticalSpan = 1.0f;
		emptyMenu.Ch202SpecialJumpVerticalHeight = 1.0f;
		emptyMenu.Ch202SpecialJumpForwardSpan = 1.0f;
		emptyMenu.Ch202SpecialJumpForwardHeight = 1.0f;
		emptyMenu.Ch202SpecialJumpForwardInitialSpeedH = 1.0f;
		emptyMenu.Ch202SpecialJumpForwardLastSpeedH = 1.0f;
		emptyMenu.Ch202SpecialJumpForwardAccelSpanH = 1.0f;
		emptyMenu.Ch202SpecialWallJumpSpan = 1.0f;
		emptyMenu.Ch202SpecialWallJumpHeight = 1.0f;
		emptyMenu.Ch202SpecialWallJumpInitialSpeed = 1.0f;
		emptyMenu.Ch202SpecialWallJumpLastSpeed = 1.0f;
		emptyMenu.Ch202ConditionAnimationMultiplyRate = 1.0f;
		emptyMenu.Ch202ConditionMoveMultiplyRate = 1.0f;
		emptyMenu.Ch202ConditionJumpAdjustMultiplyRate = 1.0f;

		emptyMenu.Ch025U2MoveSpeedXY = 1.0f;
		emptyMenu.Ch025U2MoveSpeedZUp = 1.0f;
		emptyMenu.Ch025U2MoveSpeedZDown = 1.0f;
		emptyMenu.Ch025U2ReinforceMoveSpeedXY = 1.0f;
		emptyMenu.Ch025U2ReinforceMoveSpeedZUp = 1.0f;
		emptyMenu.Ch025U2ReinforceMoveSpeedZDown = 1.0f;
		emptyMenu.Ch025U2ActivityLimitTime = 1.0f;
		emptyMenu.Ch025U2ActivityLowestTime = 1.0f;
		emptyMenu.Ch025U2ShockWaveMagRateL1 = 1.0f;
		emptyMenu.Ch025U2ShockWaveMagRateL2 = 1.0f;
		emptyMenu.Ch025U2ShockWaveMagRateL3 = 1.0f;
		emptyMenu.Ch025U2ActionMagRateL1 = 1.0f;
		emptyMenu.Ch025U2ActionMagRateL2 = 1.0f;
		emptyMenu.Ch025U2ActionMagRateL3 = 1.0f;
		emptyMenu.Ch025U3InitialVerticalSpeed = 1.0f;
		emptyMenu.Ch025U3LastVerticalSpeed = 1.0f;
		emptyMenu.Ch025U3Span = 1.0f;
		emptyMenu.Ch025U3WaitTurnTime = 1.0f;
		emptyMenu.Ch025U3ApplyInertiaSpawn = 1.0f;
		emptyMenu.Ch025U3ApplyInertiaSpawnHRate = 1.0f;
		emptyMenu.Ch025U3ApplyInertiaSpawnVRate = 1.0f;
		emptyMenu.Ch025U3ConditionTimeL1 = 1.0f;
		emptyMenu.Ch025U3ConditionTimeL2 = 1.0f;
		emptyMenu.Ch025U3ConditionTimeL3 = 1.0f;
		emptyMenu.Ch025U3BarrierValueL1 = 1.0f;
		emptyMenu.Ch025U3BarrierValueL2 = 1.0f;
		emptyMenu.Ch025U3BarrierValueL3 = 1.0f;
		emptyMenu.Ch025U3BarrierValueAllyL1 = 1.0f;
		emptyMenu.Ch025U3BarrierValueAllyL2 = 1.0f;
		emptyMenu.Ch025U3BarrierValueAllyL3 = 1.0f;
		emptyMenu.Ch025U3TurnAngleDeg = 1.0f;
		emptyMenu.Ch025U3TurnTime = 1.0f;
		emptyMenu.Ch025U3TurnBlendExp = 1.0f;
		emptyMenu.Ch025U3TurnSteps = 1;
		emptyMenu.Ch025U3ManyAllyBarrierRate = 1.0f;
		emptyMenu.Ch025U3OwnerBarrierListValue = 1.0f;
		emptyMenu.Ch025U3AllyBarrierListValue = 1.0f;
		emptyMenu.Ch025SpecialLowGravityTime = 1.0f;
		emptyMenu.Ch025SpecialStartSpeed = 1.0f;
		emptyMenu.Ch025SpecialMiddleSpeed = 1.0f;
		emptyMenu.Ch025SpecialEndSpeed = 1.0f;
		emptyMenu.Ch025SpecialStartSpan = 1.0f;
		emptyMenu.Ch025SpecialEndSpan = 1.0f;
		emptyMenu.Ch025SpecialDashTime = 1.0f;
		emptyMenu.Ch025SpecialStartVerticalSpeed = 1.0f;
		emptyMenu.Ch025SpecialMiddleVerticalSpeed = 1.0f;
		emptyMenu.Ch025SpecialEndVerticalSpeed = 1.0f;
		emptyMenu.Ch025SpecialStartVerticalSpan = 1.0f;
		emptyMenu.Ch025SpecialEndVerticalSpan = 1.0f;
		emptyMenu.Ch025BarrierMaxTime = 1.0f;
		emptyMenu.Ch025BarrierDurability = 1.0f;
		emptyMenu.Ch025BarrierCopyRate = 1.0f;
		emptyMenu.Ch025RecoveryHealthValue = 1.0f;

		// Disable all HackSettings
		emptyHack.EnableInvincible = false;
		emptyHack.EnableInfiniteHealth = false;
		emptyHack.EnableInfiniteBarrier = false;
		emptyHack.EnableInfinitePlusUltra = false;
		emptyHack.EnableFullBuff = false;
		emptyHack.EnableHideKills = false;
		emptyHack.EnableInfiniteSkills = false;

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
		emptyHack.CharCondition_AutoExecute = false;
		emptyHack.CharCondition_SelectedConditionId = 35;
		emptyHack.CharCondition_ApplyMode = 0;
		emptyHack.CharCondition_Level = 0;
		emptyHack.CharCondition_Duration = 50.0f;
		emptyHack.CharCondition_Value = 0.0f;
		emptyHack.CharCondition_Interval = 0.0f;
		emptyHack.CharCondition_SubLevel = 0;
		emptyHack.CharCondition_TimeOverwrite = false;
		emptyHack.ChangePlayerNameBuffer[0] = '\0';
		emptyHack.BackendPlatformCode = 3;
		strcpy_s(emptyHack.FakePlatformBuffer, sizeof(emptyHack.FakePlatformBuffer), "Windows");
		emptyHack.BuyLicenseExpCount = 30000;
		emptyHack.LicenseClaimSeasonCode = 1;
		emptyHack.LicenseClaimFreeRank = 1;
		emptyHack.LicenseClaimPremiumRank = 0;
		emptyHack.LicenseClaimSpecialRank = 1;
		emptyHack.LicenseClaimRepeatCount = 1;
		emptyHack.LicenseClaimDelayMs = 1000;

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

			auto ExtractBoolDefault = [&](const std::string& key, bool defaultValue) -> bool
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return defaultValue;
				
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

			auto ExtractIntDefault = [&](const std::string& key, int defaultValue) -> int
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return defaultValue;
				
				pos += searchStr.length();
				std::string numStr;
				while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-'))
				{
					numStr += json[pos];
					pos++;
				}
				return numStr.empty() ? defaultValue : std::stoi(numStr);
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

			auto ExtractFloatDefault = [&](const std::string& key, float defaultValue) -> float
			{
				std::string searchStr = "\"" + key + "\": ";
				size_t pos = json.find(searchStr);
				if (pos == std::string::npos) return defaultValue;
				
				pos += searchStr.length();
				std::string numStr;
				while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-'))
				{
					numStr += json[pos];
					pos++;
				}
				return numStr.empty() ? defaultValue : std::stof(numStr);
			};

			// Load MenuSettings
			menuSettings.EnableGlobal = ExtractBool("EnableGlobal");
			menuSettings.EnableESP = ExtractBool("EnableESP");
			menuSettings.EnableMenuBackgroundVideo = ExtractBoolDefault("EnableMenuBackgroundVideo", true);
			menuSettings.EnableStreamProofMenu = ExtractBoolDefault("EnableStreamProofMenu", false);
			menuSettings.EnablePlayerESP = ExtractBool("EnablePlayerESP");
			menuSettings.Player_Box = ExtractBool("Player_Box");
			menuSettings.Player_Box_Filled = ExtractBoolDefault("Player_Box_Filled", false);
			menuSettings.Player_Box_Filled_Alpha = ExtractFloatDefault("Player_Box_Filled_Alpha", 10.0f);
			menuSettings.PlayerColor[0] = ExtractFloatDefault("PlayerColor_R", 1.0f);
			menuSettings.PlayerColor[1] = ExtractFloatDefault("PlayerColor_G", 0.0f);
			menuSettings.PlayerColor[2] = ExtractFloatDefault("PlayerColor_B", 0.0f);
			menuSettings.PlayerColor[3] = ExtractFloatDefault("PlayerColor_A", 1.0f);
			menuSettings.TeamColor[0] = ExtractFloatDefault("TeamColor_R", 0.0f);
			menuSettings.TeamColor[1] = ExtractFloatDefault("TeamColor_G", 0.588f);
			menuSettings.TeamColor[2] = ExtractFloatDefault("TeamColor_B", 1.0f);
			menuSettings.TeamColor[3] = ExtractFloatDefault("TeamColor_A", 1.0f);
			menuSettings.Player_Health = ExtractBool("Player_Health");
			menuSettings.Player_Distance = ExtractBool("Player_Distance");
			menuSettings.Player_Name = ExtractBool("Player_Name");
			menuSettings.ShowHP = ExtractBool("ShowHP");
			menuSettings.ShowGP = ExtractBool("ShowGP");
			menuSettings.ShowPU = ExtractBool("ShowPU");
			menuSettings.ShowHPText = ExtractBoolDefault("ShowHPText", false);
			menuSettings.ShowGPText = ExtractBoolDefault("ShowGPText", false);
			menuSettings.ShowPlatform = ExtractBool("ShowPlatform");
			menuSettings.ShowTeamId = ExtractBool("ShowTeamId");
			menuSettings.ShowPlayerSkeleton = ExtractBool("ShowPlayerSkeleton");
			menuSettings.ShowServerStatusOverlay = ExtractBoolDefault("ShowServerStatusOverlay", false);

			menuSettings.EnableAimbot = ExtractBool("EnableAimbot");
			menuSettings.AimbotSmoothing = ExtractBool("AimbotSmoothing");
			menuSettings.AimbotSmoothFactor = ExtractFloat("AimbotSmoothFactor");
			menuSettings.AimbotDrawLine = ExtractBool("AimbotDrawLine");
			menuSettings.AimbotDrawFOV = ExtractBool("AimbotDrawFOV");
			menuSettings.AimbotDrawCrosshair = ExtractBool("AimbotDrawCrosshair");
			menuSettings.AimbotFOVRadius = ExtractFloat("AimbotFOVRadius");
			menuSettings.AimbotMaxDistance = ExtractFloat("AimbotMaxDistance");
			menuSettings.AimbotRequireHold = ExtractBool("AimbotRequireHold");
			menuSettings.AimbotIgnoreDownedTargets = ExtractBoolDefault("AimbotIgnoreDownedTargets", true);

			menuSettings.AimbotHoldKey.Keyboard = ExtractInt("AimbotHoldKey_Keyboard");
			menuSettings.AimbotHoldKey.Xbox = ExtractInt("AimbotHoldKey_Xbox");
			menuSettings.AimbotHoldKey.PS4 = menuSettings.AimbotHoldKey.Xbox; // PS4 uses same value

			menuSettings.EnableTeleportToKota = ExtractBool("EnableTeleportToKota");
			menuSettings.EnableInfiniteObjects = ExtractBool("EnableInfiniteObjects");
			menuSettings.TeleportToKotaKey.Keyboard = ExtractInt("TeleportToKotaKey_Keyboard");
			menuSettings.TeleportToKotaKey.Xbox = ExtractInt("TeleportToKotaKey_Xbox");
			menuSettings.TeleportToKotaKey.PS4 = menuSettings.TeleportToKotaKey.Xbox;

			menuSettings.EnableCustomDrop = ExtractBool("EnableCustomDrop");
			menuSettings.CustomDropQuantity = ExtractIntDefault("CustomDropQuantity", 1);
			menuSettings.CustomDropSelectedIndex = ExtractInt("CustomDropSelectedIndex");
			menuSettings.CustomDropSerialId = ExtractIntDefault("CustomDropSerialId", 1);
			menuSettings.CustomDropKey.Keyboard = ExtractInt("CustomDropKey_Keyboard");
			menuSettings.CustomDropKey.Xbox = ExtractInt("CustomDropKey_Xbox");
			menuSettings.CustomDropKey.PS4 = menuSettings.CustomDropKey.Xbox;

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
			menuSettings.CopySkillsModeType = ExtractIntDefault("CopySkillsModeType", 0);

			menuSettings.EnableGenerateProjectile = ExtractBool("EnableGenerateProjectile");
			menuSettings.GenerateProjectileKey.Keyboard = ExtractInt("GenerateProjectileKey_Keyboard");
			menuSettings.GenerateProjectileKey.Xbox = ExtractInt("GenerateProjectileKey_Xbox");
			menuSettings.GenerateProjectileKey.PS4 = menuSettings.GenerateProjectileKey.Xbox;

			menuSettings.ReloadAdjustRate = ExtractFloat("ReloadAdjustRate");
			menuSettings.ReloadAdjustRate_RollSlot = ExtractFloat("ReloadAdjustRate_RollSlot");
			menuSettings.ReloadAdjustRate_WearBlueFlame = ExtractFloat("ReloadAdjustRate_WearBlueFlame");
			menuSettings.CvNoneDamageCurveValue = ExtractFloatDefault("CvNoneDamageCurveValue", 1.0f);

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
			menuSettings.BulletTPIgnoreDownedTargets = ExtractBoolDefault("BulletTPIgnoreDownedTargets", true);
			menuSettings.EnableCameraFOV = ExtractBoolDefault("EnableCameraFOV", false);
			menuSettings.CameraFOV = ExtractFloatDefault("CameraFOV", 90.0f);

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
			menuSettings.EnableFastPlusUltraCharge = ExtractBoolDefault("EnableFastPlusUltraCharge", false);
			menuSettings.EnableNoCollision = ExtractBoolDefault("EnableNoCollision", false);
			menuSettings.NoCollisionSpeed = ExtractFloatDefault("NoCollisionSpeed", 100.0f);
			menuSettings.NoCollisionHoldKey.Keyboard = ExtractIntDefault("NoCollisionHoldKey_Keyboard", 0x54);
			menuSettings.NoCollisionHoldKey.Xbox = ExtractIntDefault("NoCollisionHoldKey_Xbox", 0);
			menuSettings.NoCollisionHoldKey.PS4 = menuSettings.NoCollisionHoldKey.Xbox;
			menuSettings.EnableClearInvincibleAuto = ExtractBoolDefault("EnableClearInvincibleAuto", false);
			menuSettings.ClearInvincibleTargetMode = ExtractIntDefault("ClearInvincibleTargetMode", 1);
			menuSettings.ClearInvincibleMethod = ExtractIntDefault("ClearInvincibleMethod", 0);
			menuSettings.ClearInvincibleIgnoreFixed = ExtractBoolDefault("ClearInvincibleIgnoreFixed", true);
			menuSettings.ClearInvincibleAttackId = ExtractIntDefault("ClearInvincibleAttackId", 4);
			menuSettings.ClearInvincibleIntervalMs = ExtractIntDefault("ClearInvincibleIntervalMs", 250);
			menuSettings.ClearInvincibleSelectedCharacterIndex = ExtractIntDefault("ClearInvincibleSelectedCharacterIndex", -1);
			{
				std::string clearInvincibleTag = ExtractString("ClearInvincibleTagBuffer");
				strncpy_s(menuSettings.ClearInvincibleTagBuffer, sizeof(menuSettings.ClearInvincibleTagBuffer), clearInvincibleTag.c_str(), _TRUNCATE);
			}
			menuSettings.EnableAttackChainAuto = ExtractBoolDefault("EnableAttackChainAuto", false);
			menuSettings.AttackChainIntervalMs = ExtractIntDefault("AttackChainIntervalMs", 50);
			menuSettings.AttackChainUseChainComboFlag = ExtractBoolDefault("AttackChainUseChainComboFlag", true);
			menuSettings.AttackChainComboFlagTime = ExtractFloatDefault("AttackChainComboFlagTime", 0.25f);
			menuSettings.AttackChainEnableShiftAttackActions = ExtractBoolDefault("AttackChainEnableShiftAttackActions", true);
			menuSettings.AttackChainClearShiftActionAttackFlags = ExtractBoolDefault("AttackChainClearShiftActionAttackFlags", true);
			menuSettings.AttackChainUseAnimationRate = ExtractBoolDefault("AttackChainUseAnimationRate", true);
			menuSettings.AttackChainAnimationRate = ExtractFloatDefault("AttackChainAnimationRate", 2.0f);
			menuSettings.AttackChainAnimationRateNagara = ExtractBoolDefault("AttackChainAnimationRateNagara", true);
			menuSettings.AttackChainUsePhaseEndCondition = ExtractBoolDefault("AttackChainUsePhaseEndCondition", false);
			menuSettings.AttackChainComboCommand = ExtractBoolDefault("AttackChainComboCommand", true);
			menuSettings.AttackChainGrabbed = ExtractBoolDefault("AttackChainGrabbed", false);
			menuSettings.AttackChainEndTimer = ExtractFloatDefault("AttackChainEndTimer", 0.0f);
			menuSettings.AttackChainLanding = ExtractBoolDefault("AttackChainLanding", false);
			menuSettings.AttackChainEndAnim = ExtractBoolDefault("AttackChainEndAnim", true);
			menuSettings.AttackChainEndAnimSlot = ExtractIntDefault("AttackChainEndAnimSlot", 0);
			menuSettings.AttackChainFinishCurrentPhase = ExtractBoolDefault("AttackChainFinishCurrentPhase", false);
			menuSettings.AttackChainTerminateAttackLayer = ExtractBoolDefault("AttackChainTerminateAttackLayer", false);
			menuSettings.AttackChainEnableAimingActionCancel = ExtractBoolDefault("AttackChainEnableAimingActionCancel", true);
			menuSettings.AttackChainActionCancelFlag = ExtractIntDefault("AttackChainActionCancelFlag", 255);
			menuSettings.AttackChainOwnerActionOnly = ExtractBoolDefault("AttackChainOwnerActionOnly", false);
			menuSettings.AttackChainValidateNextReservedAction = ExtractBoolDefault("AttackChainValidateNextReservedAction", true);
			menuSettings.EnableDownPowerAuto = ExtractBoolDefault("EnableDownPowerAuto", false);
			menuSettings.DownPowerIntervalMs = ExtractIntDefault("DownPowerIntervalMs", 100);
			menuSettings.DownPowerTargetMode = ExtractIntDefault("DownPowerTargetMode", 1);
			menuSettings.DownPowerSelectedCharacterIndex = ExtractIntDefault("DownPowerSelectedCharacterIndex", -1);
			menuSettings.DownPowerPatchDamageParams = ExtractBoolDefault("DownPowerPatchDamageParams", true);
			menuSettings.DownPowerDamageType = ExtractIntDefault("DownPowerDamageType", 16);
			menuSettings.DownPowerIncludeNoActionDamage = ExtractBoolDefault("DownPowerIncludeNoActionDamage", true);
			menuSettings.DownPowerOverrideRecoverSpan = ExtractBoolDefault("DownPowerOverrideRecoverSpan", false);
			menuSettings.DownPowerRecoverSpan = ExtractFloatDefault("DownPowerRecoverSpan", 0.0f);
			menuSettings.DownPowerApplyDurableRates = ExtractBoolDefault("DownPowerApplyDurableRates", true);
			menuSettings.DownPowerDurableRate = ExtractFloatDefault("DownPowerDurableRate", 1.0f);
			menuSettings.DownPowerDurableAttackActionRate = ExtractFloatDefault("DownPowerDurableAttackActionRate", 1.0f);
			menuSettings.DownPowerDurableTakeDamageRate = ExtractFloatDefault("DownPowerDurableTakeDamageRate", 1.0f);
			menuSettings.DownPowerDurableSpecialActionRate = ExtractFloatDefault("DownPowerDurableSpecialActionRate", 1.0f);
			menuSettings.DownPowerDurableRollSlotRate = ExtractFloatDefault("DownPowerDurableRollSlotRate", 1.0f);
			menuSettings.DownPowerDurableTakeDamageRollSlotRate = ExtractFloatDefault("DownPowerDurableTakeDamageRollSlotRate", 1.0f);
			menuSettings.DownPowerApplyBreakDownSuperArmorRate = ExtractBoolDefault("DownPowerApplyBreakDownSuperArmorRate", true);
			menuSettings.DownPowerBreakDownSuperArmorRate = ExtractFloatDefault("DownPowerBreakDownSuperArmorRate", 1.0f);
			menuSettings.DownPowerDisableTargetSuperArmor = ExtractBoolDefault("DownPowerDisableTargetSuperArmor", true);
			menuSettings.Ch202MeleeAChaseHeightOffset = ExtractFloatDefault("Ch202MeleeAChaseHeightOffset", 0.0f);
			menuSettings.Ch202MeleeAChaseStartSpeedMax = ExtractFloatDefault("Ch202MeleeAChaseStartSpeedMax", 1.0f);
			menuSettings.Ch202MeleeAChaseStartSpeedMin = ExtractFloatDefault("Ch202MeleeAChaseStartSpeedMin", 1.0f);
			menuSettings.Ch202MeleeAChaseLastSpeed = ExtractFloatDefault("Ch202MeleeAChaseLastSpeed", 1.0f);
			menuSettings.Ch202MeleeAChaseSpeedSpan = ExtractFloatDefault("Ch202MeleeAChaseSpeedSpan", 1.0f);
			menuSettings.Ch202MeleeAChaseSpan = ExtractFloatDefault("Ch202MeleeAChaseSpan", 1.0f);
			menuSettings.Ch202MeleeADistance = ExtractFloatDefault("Ch202MeleeADistance", 1.0f);
			menuSettings.Ch202MeleeABreakTargetSpeed = ExtractFloatDefault("Ch202MeleeABreakTargetSpeed", 1.0f);
			menuSettings.Ch202MeleeABreakTargetSpan = ExtractFloatDefault("Ch202MeleeABreakTargetSpan", 1.0f);
			menuSettings.Ch202Unique1HoldTime = ExtractFloatDefault("Ch202Unique1HoldTime", 1.0f);
			menuSettings.Ch202Unique2NeedFieldTime = ExtractFloatDefault("Ch202Unique2NeedFieldTime", 1.0f);
			menuSettings.Ch202Unique2HoldTime = ExtractFloatDefault("Ch202Unique2HoldTime", 1.0f);
			menuSettings.Ch202Unique2MoveTime = ExtractFloatDefault("Ch202Unique2MoveTime", 1.0f);
			menuSettings.Ch202Unique2MoveSpeedMin = ExtractFloatDefault("Ch202Unique2MoveSpeedMin", 1.0f);
			menuSettings.Ch202Unique2MoveSpeedMax = ExtractFloatDefault("Ch202Unique2MoveSpeedMax", 1.0f);
			menuSettings.Ch202Unique2QuintupleRiseSlideSpeed = ExtractFloatDefault("Ch202Unique2QuintupleRiseSlideSpeed", 1.0f);
			menuSettings.Ch202Unique2QuintupleRiseSlideSpan = ExtractFloatDefault("Ch202Unique2QuintupleRiseSlideSpan", 1.0f);
			menuSettings.Ch202Unique2QuintupleFallSlideSpeed = ExtractFloatDefault("Ch202Unique2QuintupleFallSlideSpeed", 1.0f);
			menuSettings.Ch202Unique2QuintupleFallSlideSpan = ExtractFloatDefault("Ch202Unique2QuintupleFallSlideSpan", 1.0f);
			menuSettings.Ch202Unique2AfterLevel = ExtractIntDefault("Ch202Unique2AfterLevel", 1);
			menuSettings.Ch202Unique2QuintupleAllAmmoConsumed = ExtractBoolDefault("Ch202Unique2QuintupleAllAmmoConsumed", false);
			menuSettings.Ch202U3InitTransMissionLevel = ExtractIntDefault("Ch202U3InitTransMissionLevel", 1);
			menuSettings.Ch202U3EndDistanceOfFollowing = ExtractFloatDefault("Ch202U3EndDistanceOfFollowing", 1.0f);
			menuSettings.Ch202U3MoveSpeedStart = ExtractFloatDefault("Ch202U3MoveSpeedStart", 1.0f);
			menuSettings.Ch202U3MoveSpeedEnd = ExtractFloatDefault("Ch202U3MoveSpeedEnd", 1.0f);
			menuSettings.Ch202U3SpeedChangeTime = ExtractFloatDefault("Ch202U3SpeedChangeTime", 1.0f);
			menuSettings.Ch202U3ExitSpeedStart = ExtractFloatDefault("Ch202U3ExitSpeedStart", 1.0f);
			menuSettings.Ch202U3ExitSpeedEnd = ExtractFloatDefault("Ch202U3ExitSpeedEnd", 1.0f);
			menuSettings.Ch202U3ExitChangeTime = ExtractFloatDefault("Ch202U3ExitChangeTime", 1.0f);
			menuSettings.Ch202U3LimitCount = ExtractIntDefault("Ch202U3LimitCount", 1);
			menuSettings.Ch202U3MoveMagazinePercentMin = ExtractFloatDefault("Ch202U3MoveMagazinePercentMin", 1.0f);
			menuSettings.Ch202U3MoveMagazinePercentMax = ExtractFloatDefault("Ch202U3MoveMagazinePercentMax", 1.0f);
			menuSettings.Ch202U3PunchMagazinePercentMin = ExtractFloatDefault("Ch202U3PunchMagazinePercentMin", 1.0f);
			menuSettings.Ch202U3PunchMagazinePercentMax = ExtractFloatDefault("Ch202U3PunchMagazinePercentMax", 1.0f);
			menuSettings.Ch202U3RecoverMagazineBase = ExtractIntDefault("Ch202U3RecoverMagazineBase", 1);
			menuSettings.Ch202U3RecoverMagazineAdd = ExtractIntDefault("Ch202U3RecoverMagazineAdd", 1);
			menuSettings.Ch202U3DelayCancelTimer = ExtractFloatDefault("Ch202U3DelayCancelTimer", 1.0f);
			menuSettings.Ch202U3LockOnSec = ExtractFloatDefault("Ch202U3LockOnSec", 1.0f);
			menuSettings.Ch202U3LockOnMinSec = ExtractFloatDefault("Ch202U3LockOnMinSec", 0.1f);
			menuSettings.Ch202U3LockOnRange = ExtractFloatDefault("Ch202U3LockOnRange", 1000.0f);
			menuSettings.Ch202U3LockOnAttackTargetCheckType = ExtractIntDefault("Ch202U3LockOnAttackTargetCheckType", 0);
			menuSettings.Ch202U3CurveHorizontalDistanceMin = ExtractFloatDefault("Ch202U3CurveHorizontalDistanceMin", 1.0f);
			menuSettings.Ch202U3CurveHorizontalDistanceMax = ExtractFloatDefault("Ch202U3CurveHorizontalDistanceMax", 1.0f);
			menuSettings.Ch202U3MiddleUpOffset = ExtractFloatDefault("Ch202U3MiddleUpOffset", 1.0f);
			menuSettings.Ch202U3TargetOffset = ExtractFloatDefault("Ch202U3TargetOffset", 1.0f);
			menuSettings.Ch202U3SplitDistance = ExtractFloatDefault("Ch202U3SplitDistance", 1.0f);
			menuSettings.Ch202U3SplitLerpRate = ExtractFloatDefault("Ch202U3SplitLerpRate", 1.0f);
			menuSettings.Ch202U3ControlPointsRate = ExtractFloatDefault("Ch202U3ControlPointsRate", 1.0f);
			menuSettings.Ch202U3MinDetroitRange = ExtractFloatDefault("Ch202U3MinDetroitRange", 1.0f);
			menuSettings.Ch202U3MinDetroitSpeed = ExtractFloatDefault("Ch202U3MinDetroitSpeed", 1.0f);
			menuSettings.Ch202U3MaxDetroitRange = ExtractFloatDefault("Ch202U3MaxDetroitRange", 1.0f);
			menuSettings.Ch202U3MaxDetroitSpeed = ExtractFloatDefault("Ch202U3MaxDetroitSpeed", 1.0f);
			menuSettings.Ch202U3MiddleOffsetX = ExtractFloatDefault("Ch202U3MiddleOffsetX", 0.0f);
			menuSettings.Ch202U3MiddleOffsetY = ExtractFloatDefault("Ch202U3MiddleOffsetY", 0.0f);
			menuSettings.Ch202U3MiddleOffsetZ = ExtractFloatDefault("Ch202U3MiddleOffsetZ", 0.0f);
			menuSettings.Ch202U3MiddleOffsetAerialX = ExtractFloatDefault("Ch202U3MiddleOffsetAerialX", 0.0f);
			menuSettings.Ch202U3MiddleOffsetAerialY = ExtractFloatDefault("Ch202U3MiddleOffsetAerialY", 0.0f);
			menuSettings.Ch202U3MiddleOffsetAerialZ = ExtractFloatDefault("Ch202U3MiddleOffsetAerialZ", 0.0f);
			menuSettings.Ch202SpecialHoldTime = ExtractFloatDefault("Ch202SpecialHoldTime", 1.0f);
			menuSettings.Ch202SpecialActivationTime = ExtractFloatDefault("Ch202SpecialActivationTime", 1.0f);
			menuSettings.Ch202SpecialSmokeMagazineCost = ExtractIntDefault("Ch202SpecialSmokeMagazineCost", 1);
			menuSettings.Ch202SpecialLegCount = ExtractIntDefault("Ch202SpecialLegCount", 1);
			menuSettings.Ch202SpecialJumpVerticalSpan = ExtractFloatDefault("Ch202SpecialJumpVerticalSpan", 1.0f);
			menuSettings.Ch202SpecialJumpVerticalHeight = ExtractFloatDefault("Ch202SpecialJumpVerticalHeight", 1.0f);
			menuSettings.Ch202SpecialJumpForwardSpan = ExtractFloatDefault("Ch202SpecialJumpForwardSpan", 1.0f);
			menuSettings.Ch202SpecialJumpForwardHeight = ExtractFloatDefault("Ch202SpecialJumpForwardHeight", 1.0f);
			menuSettings.Ch202SpecialJumpForwardInitialSpeedH = ExtractFloatDefault("Ch202SpecialJumpForwardInitialSpeedH", 1.0f);
			menuSettings.Ch202SpecialJumpForwardLastSpeedH = ExtractFloatDefault("Ch202SpecialJumpForwardLastSpeedH", 1.0f);
			menuSettings.Ch202SpecialJumpForwardAccelSpanH = ExtractFloatDefault("Ch202SpecialJumpForwardAccelSpanH", 1.0f);
			menuSettings.Ch202SpecialWallJumpSpan = ExtractFloatDefault("Ch202SpecialWallJumpSpan", 1.0f);
			menuSettings.Ch202SpecialWallJumpHeight = ExtractFloatDefault("Ch202SpecialWallJumpHeight", 1.0f);
			menuSettings.Ch202SpecialWallJumpInitialSpeed = ExtractFloatDefault("Ch202SpecialWallJumpInitialSpeed", 1.0f);
			menuSettings.Ch202SpecialWallJumpLastSpeed = ExtractFloatDefault("Ch202SpecialWallJumpLastSpeed", 1.0f);
			menuSettings.Ch202ConditionAnimationMultiplyRate = ExtractFloatDefault("Ch202ConditionAnimationMultiplyRate", 1.0f);
			menuSettings.Ch202ConditionMoveMultiplyRate = ExtractFloatDefault("Ch202ConditionMoveMultiplyRate", 1.0f);
			menuSettings.Ch202ConditionJumpAdjustMultiplyRate = ExtractFloatDefault("Ch202ConditionJumpAdjustMultiplyRate", 1.0f);

			menuSettings.Ch025U2MoveSpeedXY = ExtractFloatDefault("Ch025U2MoveSpeedXY", 1.0f);
			menuSettings.Ch025U2MoveSpeedZUp = ExtractFloatDefault("Ch025U2MoveSpeedZUp", 1.0f);
			menuSettings.Ch025U2MoveSpeedZDown = ExtractFloatDefault("Ch025U2MoveSpeedZDown", 1.0f);
			menuSettings.Ch025U2ReinforceMoveSpeedXY = ExtractFloatDefault("Ch025U2ReinforceMoveSpeedXY", 1.0f);
			menuSettings.Ch025U2ReinforceMoveSpeedZUp = ExtractFloatDefault("Ch025U2ReinforceMoveSpeedZUp", 1.0f);
			menuSettings.Ch025U2ReinforceMoveSpeedZDown = ExtractFloatDefault("Ch025U2ReinforceMoveSpeedZDown", 1.0f);
			menuSettings.Ch025U2ActivityLimitTime = ExtractFloatDefault("Ch025U2ActivityLimitTime", 1.0f);
			menuSettings.Ch025U2ActivityLowestTime = ExtractFloatDefault("Ch025U2ActivityLowestTime", 1.0f);
			menuSettings.Ch025U2ShockWaveMagRateL1 = ExtractFloatDefault("Ch025U2ShockWaveMagRateL1", 1.0f);
			menuSettings.Ch025U2ShockWaveMagRateL2 = ExtractFloatDefault("Ch025U2ShockWaveMagRateL2", 1.0f);
			menuSettings.Ch025U2ShockWaveMagRateL3 = ExtractFloatDefault("Ch025U2ShockWaveMagRateL3", 1.0f);
			menuSettings.Ch025U2ActionMagRateL1 = ExtractFloatDefault("Ch025U2ActionMagRateL1", 1.0f);
			menuSettings.Ch025U2ActionMagRateL2 = ExtractFloatDefault("Ch025U2ActionMagRateL2", 1.0f);
			menuSettings.Ch025U2ActionMagRateL3 = ExtractFloatDefault("Ch025U2ActionMagRateL3", 1.0f);
			menuSettings.Ch025U3InitialVerticalSpeed = ExtractFloatDefault("Ch025U3InitialVerticalSpeed", 1.0f);
			menuSettings.Ch025U3LastVerticalSpeed = ExtractFloatDefault("Ch025U3LastVerticalSpeed", 1.0f);
			menuSettings.Ch025U3Span = ExtractFloatDefault("Ch025U3Span", 1.0f);
			menuSettings.Ch025U3WaitTurnTime = ExtractFloatDefault("Ch025U3WaitTurnTime", 1.0f);
			menuSettings.Ch025U3ApplyInertiaSpawn = ExtractFloatDefault("Ch025U3ApplyInertiaSpawn", 1.0f);
			menuSettings.Ch025U3ApplyInertiaSpawnHRate = ExtractFloatDefault("Ch025U3ApplyInertiaSpawnHRate", 1.0f);
			menuSettings.Ch025U3ApplyInertiaSpawnVRate = ExtractFloatDefault("Ch025U3ApplyInertiaSpawnVRate", 1.0f);
			menuSettings.Ch025U3ConditionTimeL1 = ExtractFloatDefault("Ch025U3ConditionTimeL1", 1.0f);
			menuSettings.Ch025U3ConditionTimeL2 = ExtractFloatDefault("Ch025U3ConditionTimeL2", 1.0f);
			menuSettings.Ch025U3ConditionTimeL3 = ExtractFloatDefault("Ch025U3ConditionTimeL3", 1.0f);
			menuSettings.Ch025U3BarrierValueL1 = ExtractFloatDefault("Ch025U3BarrierValueL1", 1.0f);
			menuSettings.Ch025U3BarrierValueL2 = ExtractFloatDefault("Ch025U3BarrierValueL2", 1.0f);
			menuSettings.Ch025U3BarrierValueL3 = ExtractFloatDefault("Ch025U3BarrierValueL3", 1.0f);
			menuSettings.Ch025U3BarrierValueAllyL1 = ExtractFloatDefault("Ch025U3BarrierValueAllyL1", 1.0f);
			menuSettings.Ch025U3BarrierValueAllyL2 = ExtractFloatDefault("Ch025U3BarrierValueAllyL2", 1.0f);
			menuSettings.Ch025U3BarrierValueAllyL3 = ExtractFloatDefault("Ch025U3BarrierValueAllyL3", 1.0f);
			menuSettings.Ch025U3TurnAngleDeg = ExtractFloatDefault("Ch025U3TurnAngleDeg", 1.0f);
			menuSettings.Ch025U3TurnTime = ExtractFloatDefault("Ch025U3TurnTime", 1.0f);
			menuSettings.Ch025U3TurnBlendExp = ExtractFloatDefault("Ch025U3TurnBlendExp", 1.0f);
			menuSettings.Ch025U3TurnSteps = ExtractIntDefault("Ch025U3TurnSteps", 1);
			menuSettings.Ch025U3ManyAllyBarrierRate = ExtractFloatDefault("Ch025U3ManyAllyBarrierRate", 1.0f);
			menuSettings.Ch025U3OwnerBarrierListValue = ExtractFloatDefault("Ch025U3OwnerBarrierListValue", 1.0f);
			menuSettings.Ch025U3AllyBarrierListValue = ExtractFloatDefault("Ch025U3AllyBarrierListValue", 1.0f);
			menuSettings.Ch025SpecialLowGravityTime = ExtractFloatDefault("Ch025SpecialLowGravityTime", 1.0f);
			menuSettings.Ch025SpecialStartSpeed = ExtractFloatDefault("Ch025SpecialStartSpeed", 1.0f);
			menuSettings.Ch025SpecialMiddleSpeed = ExtractFloatDefault("Ch025SpecialMiddleSpeed", 1.0f);
			menuSettings.Ch025SpecialEndSpeed = ExtractFloatDefault("Ch025SpecialEndSpeed", 1.0f);
			menuSettings.Ch025SpecialStartSpan = ExtractFloatDefault("Ch025SpecialStartSpan", 1.0f);
			menuSettings.Ch025SpecialEndSpan = ExtractFloatDefault("Ch025SpecialEndSpan", 1.0f);
			menuSettings.Ch025SpecialDashTime = ExtractFloatDefault("Ch025SpecialDashTime", 1.0f);
			menuSettings.Ch025SpecialStartVerticalSpeed = ExtractFloatDefault("Ch025SpecialStartVerticalSpeed", 1.0f);
			menuSettings.Ch025SpecialMiddleVerticalSpeed = ExtractFloatDefault("Ch025SpecialMiddleVerticalSpeed", 1.0f);
			menuSettings.Ch025SpecialEndVerticalSpeed = ExtractFloatDefault("Ch025SpecialEndVerticalSpeed", 1.0f);
			menuSettings.Ch025SpecialStartVerticalSpan = ExtractFloatDefault("Ch025SpecialStartVerticalSpan", 1.0f);
			menuSettings.Ch025SpecialEndVerticalSpan = ExtractFloatDefault("Ch025SpecialEndVerticalSpan", 1.0f);
			menuSettings.Ch025BarrierMaxTime = ExtractFloatDefault("Ch025BarrierMaxTime", 1.0f);
			menuSettings.Ch025BarrierDurability = ExtractFloatDefault("Ch025BarrierDurability", 1.0f);
			menuSettings.Ch025BarrierCopyRate = ExtractFloatDefault("Ch025BarrierCopyRate", 1.0f);
			menuSettings.Ch025RecoveryHealthValue = ExtractFloatDefault("Ch025RecoveryHealthValue", 1.0f);

			// Load HackSettings
			hackSettings.EnableInvincible = ExtractBool("EnableInvincible");
			hackSettings.EnableInfiniteHealth = ExtractBool("EnableInfiniteHealth");
			hackSettings.EnableInfiniteBarrier = ExtractBool("EnableInfiniteBarrier");
			hackSettings.EnableInfinitePlusUltra = ExtractBool("EnableInfinitePlusUltra");
			hackSettings.EnableFullBuff = ExtractBool("EnableFullBuff");
			hackSettings.EnableHideKills = ExtractBool("EnableHideKills");
			hackSettings.EnableInfiniteSkills = ExtractBool("EnableInfiniteSkills");
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
			hackSettings.CharCondition_AutoExecute = ExtractBool("CharCondition_AutoExecute");
			hackSettings.CharCondition_SelectedConditionId = ExtractIntDefault("CharCondition_SelectedConditionId", 35);
			hackSettings.CharCondition_ApplyMode = ExtractIntDefault("CharCondition_ApplyMode", 0);
			hackSettings.CharCondition_Level = ExtractIntDefault("CharCondition_Level", 0);
			hackSettings.CharCondition_Duration = ExtractFloatDefault("CharCondition_Duration", 50.0f);
			hackSettings.CharCondition_Value = ExtractFloatDefault("CharCondition_Value", 0.0f);
			hackSettings.CharCondition_Interval = ExtractFloatDefault("CharCondition_Interval", 0.0f);
			hackSettings.CharCondition_SubLevel = ExtractIntDefault("CharCondition_SubLevel", 0);
			hackSettings.CharCondition_TimeOverwrite = ExtractBool("CharCondition_TimeOverwrite");
			{
				std::string playerNameBuffer = ExtractString("ChangePlayerNameBuffer");
				strncpy_s(hackSettings.ChangePlayerNameBuffer, sizeof(hackSettings.ChangePlayerNameBuffer), playerNameBuffer.c_str(), _TRUNCATE);
			}
			hackSettings.BackendPlatformCode = ExtractIntDefault("BackendPlatformCode", 3);
			{
				std::string fakePlatformBuffer = ExtractString("FakePlatformBuffer");
				if (fakePlatformBuffer.empty())
					fakePlatformBuffer = "Windows";
				strncpy_s(hackSettings.FakePlatformBuffer, sizeof(hackSettings.FakePlatformBuffer), fakePlatformBuffer.c_str(), _TRUNCATE);
			}
			hackSettings.BuyLicenseExpCount = ExtractIntDefault("BuyLicenseExpCount", 30000);
			hackSettings.LicenseClaimSeasonCode = ExtractIntDefault("LicenseClaimSeasonCode", 1);
			hackSettings.LicenseClaimFreeRank = ExtractIntDefault("LicenseClaimFreeRank", 1);
			hackSettings.LicenseClaimPremiumRank = ExtractIntDefault("LicenseClaimPremiumRank", 0);
			hackSettings.LicenseClaimSpecialRank = ExtractIntDefault("LicenseClaimSpecialRank", 1);
			hackSettings.LicenseClaimRepeatCount = ExtractIntDefault("LicenseClaimRepeatCount", 1);
			hackSettings.LicenseClaimDelayMs = ExtractIntDefault("LicenseClaimDelayMs", 1000);

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
