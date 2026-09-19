#include <Winsock2.h>
#include "../../dllmain.hpp"
#include "config.hpp"
#include "../../utils/utils.hpp"
#include "../xlln-module-resource.h"
#include "../../xlivelessness.hpp"
#include "../title-patches.hpp"
#include <stdio.h>
#include <vector>

bool H2Portable = false;
int H2Config_language_code_main = -1;
int H2Config_language_code_variant = 0;
bool H2Config_custom_labels_capture_missing = false;
int H2Config_hotkeyIdGuide = VK_HOME;
bool H2Config_hide_ingame_chat = false;
bool H2Config_hide_first_person = false;
bool H2Config_hide_HUD = false;
int32_t H2Config_field_of_view = 70;
int32_t H2Config_field_of_view_vehicle = 70;
float H2Config_crosshair_offset = NAN;
float H2Config_sensitivity_native_controller = 3.0f;
float H2Config_sensitivity_native_mouse = 3.0f;
uint32_t H2Config_sensitivity_raw_mouse = 0;
int8_t H2Config_sensitivity_xy_axis_ratio_controller = 50;
int8_t H2Config_sensitivity_xy_axis_ratio_mouse = 50;
float& H2Config_sensitivity_vehicle_ratio_controller = player_controls_sensitivity_vehicle_ratios_controller[0];
float& H2Config_sensitivity_vehicle_ratio_mouse = player_controls_sensitivity_vehicle_ratios_mouse[0];
bool H2Config_xDelay_enabled = true;
bool H2Config_warp_fix_enabled = false;
bool H2Config_vehicle_flip_eject_enabled = true;
bool H2Config_kill_volumes_enabled = true;

const char XllnConfigHeader[] = "Halo-2-Project-Cartographer-Config-Version";
const char XllnConfigVersion[] = DLL_VERSION_STR;

typedef struct {
	bool keepInvalidLines = false;
	bool saveValuesRead = true;
	std::vector<char*> badConfigEntries;
	std::vector<char*> otherConfigHeaderEntries;
	std::vector<char*> readSettings;
} INTERPRET_CONFIG_CONTEXT;

static int interpretConfigSetting(const char *fileLine, const char *version, size_t lineNumber, void *interpretationContext)
{
	if (!version) {
		return 0;
	}
	INTERPRET_CONFIG_CONTEXT &configContext = *(INTERPRET_CONFIG_CONTEXT*)interpretationContext;

	bool unrecognised = false;
	bool duplicated = false;
	bool incorrect = false;
	size_t fileLineLen = strlen(fileLine);

	if (fileLine[0] == '#' || fileLine[0] == ';' || fileLineLen <= 2) {
		unrecognised = true;
	}
	// When the version of the header has changed.
	else if (version[-1] != 0) {
		unrecognised = true;
	}
	else {
		const char *equals = strchr(fileLine, '=');

		if (!equals || equals == fileLine) {
			unrecognised = true;
		}
		else {
			size_t iValueSearch = 0;
			const char *value = equals + 1;
			while (*value == '\t' || *value == ' ') {
				value++;
			}

			const char *whitespace = equals;
			while (&whitespace[-1] != fileLine && (whitespace[-1] == '\t' || whitespace[-1] == ' ')) {
				whitespace--;
			}
			size_t settingStrLen = whitespace - fileLine;

			char *settingName = new char[settingStrLen + 1];
			memcpy(settingName, fileLine, settingStrLen);
			settingName[settingStrLen] = 0;

			for (char *&readSettingName : configContext.readSettings) {
				if (_stricmp(readSettingName, settingName) == 0) {
					duplicated = true;
					break;
				}
			}

#define SettingNameMatches(x) _stricmp(x, settingName) == 0

			if (!unrecognised && !duplicated && !incorrect) {

				if (SettingNameMatches("hide_ingame_chat")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_hide_ingame_chat = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("hide_first_person")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_hide_first_person = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("hide_HUD")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_hide_HUD = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("field_of_view")) {
					int32_t tempint32;
					if (sscanf_s(value, "%d", &tempint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_field_of_view = tempint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("field_of_view_vehicle")) {
					int32_t tempint32;
					if (sscanf_s(value, "%d", &tempint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_field_of_view_vehicle = tempint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("crosshair_offset")) {
					float tempfloat;
					if (sscanf_s(value, "%f", &tempfloat) == 1 && (FloatIsNaN(tempfloat) || (tempfloat >= 0.0f && tempfloat <= 0.53f))) {
						if (configContext.saveValuesRead) {
							H2Config_crosshair_offset = tempfloat;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_native_controller")) {
					float tempfloat;
					if (sscanf_s(value, "%f", &tempfloat) == 1 && tempfloat > 0.0f && tempfloat <= 1000.0f) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_native_controller = tempfloat;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_native_mouse")) {
					float tempfloat;
					if (sscanf_s(value, "%f", &tempfloat) == 1 && tempfloat > 0.0f && tempfloat <= 1000.0f) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_native_mouse = tempfloat;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_raw_mouse")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1 && tempuint32 >= 0 && tempuint32 <= 10000) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_raw_mouse = tempuint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_xy_axis_ratio_controller")) {
					int8_t tempint8;
					if (sscanf_s(value, "%hhd", &tempint8) == 1 && tempint8 >= -100 && tempint8 <= 100) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_xy_axis_ratio_controller = tempint8;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_xy_axis_ratio_mouse")) {
					int8_t tempint8;
					if (sscanf_s(value, "%hhd", &tempint8) == 1 && tempint8 >= -100 && tempint8 <= 100) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_xy_axis_ratio_mouse = tempint8;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_vehicle_ratio_controller")) {
					float tempfloat;
					if (sscanf_s(value, "%f", &tempfloat) == 1 && tempfloat > 0.0f && tempfloat <= 1000.0f) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_vehicle_ratio_controller = tempfloat;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("sensitivity_vehicle_ratio_mouse")) {
					float tempfloat;
					if (sscanf_s(value, "%f", &tempfloat) == 1 && tempfloat > 0.0f && tempfloat <= 1000.0f) {
						if (configContext.saveValuesRead) {
							H2Config_sensitivity_vehicle_ratio_mouse = tempfloat;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xDelay_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_xDelay_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("warp_fix_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_warp_fix_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("vehicle_flip_eject_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_vehicle_flip_eject_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("kill_volumes_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							H2Config_kill_volumes_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else {
					unrecognised = true;
				}
				
				if (!unrecognised && !duplicated && !incorrect) {
					configContext.readSettings.push_back(CloneString(settingName));
				}
			}

#undef SettingNameMatches
			delete[] settingName;
		}
	}

	if (unrecognised || duplicated || incorrect) {

		if (configContext.keepInvalidLines) {
			// When the version of the header has changed.
			if (version[-1] != 0) {
				configContext.otherConfigHeaderEntries.push_back(CloneString(fileLine));
			}
			else {
				configContext.badConfigEntries.push_back(CloneString(fileLine));
			}
		}
	}
	return 0;
}

HRESULT SaveXllnConfig(const wchar_t *file_config_path, INTERPRET_CONFIG_CONTEXT *configContext)
{
	FILE* fileConfig = 0;
	
	errno_t err = _wfopen_s(&fileConfig, file_config_path, L"wb");
	if (err) {
		return E_HANDLE;
	}
	
#define WriteText(text) fputs(text, fileConfig)
#define WriteTextF(format, ...) fprintf_s(fileConfig, format, __VA_ARGS__)
	
	WriteText("#--- Halo 2 Project Cartographer Configuration File ---");
	WriteText("\n");
	
	WriteText("\n# hide_ingame_chat:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Allows the ingame chat to be visible.");
	WriteText("\n#   1 - Never show the ingame chat.");
	WriteText("\n# Allows permanently hiding the ingame chat in games.");
	WriteText("\n");
	
	WriteText("\n# hide_first_person:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Shows the first person model.");
	WriteText("\n#   1 - Hides the first person model.");
	WriteText("\n# Adds the ability to hide and show the first person model (arms and gun) of the player. This is like the Blind skull but it does not affect the HUD.");
	WriteText("\n");
	
	WriteText("\n# hide_HUD:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Shows the HUD.");
	WriteText("\n#   1 - Hides the HUD.");
	WriteText("\n# Adds the ability to hide the Heads Up Display (HUD). This is like the Blind skull but it does not affect the first person model (arms and gun).");
	WriteText("\n");
	
	WriteText("\n# field_of_view:");
	WriteText("\n# Valid values: 1 to 110.");
	WriteText("\n#   70 - (DEFAULT)");
	WriteText("\n# Adds the ability to change the Field of View.");
	WriteText("\n");
	
	WriteText("\n# field_of_view_vehicle:");
	WriteText("\n# Valid values: 1 to 110.");
	WriteText("\n#   70 - (DEFAULT)");
	WriteText("\n# Adds the ability to change the Field of View when the player is in a vehicle. Note this setting does not affect all third person view modes.");
	WriteText("\n");
	
	WriteText("\n# crosshair_offset:");
	WriteText("\n# Valid values: NaN, 0 to 0.53.");
	WriteText("\n#   0.165 - (DEFAULT)");
	WriteText("\n#   NaN   - Disables the setting.");
	WriteText("\n# Adds the ability to adjust the height on screen of the crosshair / gun reticles where 0 is the center of the screen and increasing in value is lower on-screen.");
	WriteText("\n");
	
	WriteText("\n# sensitivity_native_controller:");
	WriteText("\n# sensitivity_native_mouse:");
	WriteText("\n# Valid values: >0.0 to 1000.0.");
	WriteText("\n#   3.0 - (DEFAULT)");
	WriteText("\n# Allows finer control over the native in-game sensitivity. Whole values from 1.0 to 10.0 match the original sensitivity values as seen in the game's settings menu while anything outside of that range is further more or less.");
	WriteText("\n");
	
	WriteText("\n# sensitivity_raw_mouse:");
	WriteText("\n# Valid values: 0 to 10000.");
	WriteText("\n#   0   - (DEFAULT) Raw mouse disabled.");
	WriteText("\n#   200 - Suggested value.");
	WriteText("\n# This setting overrides the native mouse sensitivity (sensitivity_native_mouse) setting. The difference between this raw feature and the native sensitivity is that with raw mouse there is no acceleration performed on your aim. In other words, with raw mouse there is no difference in how far your aim moves if you physically move your mouse very quickly or very slowly.");
	WriteText("\n");
	
	WriteText("\n# nsensitivity_xy_axis_ratio_controller:");
	WriteText("\n# nsensitivity_xy_axis_ratio_mouse:");
	WriteText("\n# Valid values: -100 to 100.");
	WriteText("\n#   -100 - Changing aim in the X-axis is not possible.");
	WriteText("\n#   -20  - X-axis is 20% less than the Y-axis.");
	WriteText("\n#   0    - X and Y axis' are equal.");
	WriteText("\n#   50   - (DEFAULT) Y-axis is 50% less than the X-axis.");
	WriteText("\n#   100  - Changing aim in the Y-axis is not possible.");
	WriteText("\n# This setting changes the aiming sensitivity for particular axis'. By default the game's sensitivity in the Y-axis is 50% less than the X preventing you from looking up and down as quickly as side to side.");
	WriteText("\n");
	
	WriteText("\n# sensitivity_vehicle_ratio_controller:");
	WriteText("\n# sensitivity_vehicle_ratio_mouse:");
	WriteText("\n# Valid values: >0.0 to 1000.0.");
	WriteText("\n#   0.25 - A fourth as sensitive as when not in a vehicle.");
	WriteText("\n#   0.5  - Half as sensitive as when not in a vehicle.");
	WriteText("\n#   1.0  - (DEFAULT) Equal to sensitivity when not in a vehicle.");
	WriteText("\n#   2.0  - Twice as sensitive as when not in a vehicle.");
	WriteText("\n#   4.0  - Four times as sensitive as when not in a vehicle.");
	WriteText("\n# Allows changing the aiming sensitivity when using vehicles (relative to the normal aiming sensitivity). There was no native game setting for vehicle sensitivity and instead it was locked at a particular value.");
	WriteText("\n");
	
	WriteText("\n# xDelay_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Prevents any player other than the lobby host from cancelling the start game countdown.");
	WriteText("\n#   1 - (DEFAULT) Allows other players to cancel the start game countdown.");
	WriteText("\n");
	
	WriteText("\n# warp_fix_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) Normal value.");
	WriteText("\n#   1 - Helps against warping in laggy games.");
	WriteText("\n# This setting only affects you and not other players in your game.");
	WriteText("\n");
	
	WriteText("\n# vehicle_flip_eject_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Stops players from exiting vehicles automatically when it flips upside down.");
	WriteText("\n#   1 - (DEFAULT) Normal value.");
	WriteText("\n# This setting only applies if session host.");
	WriteText("\n");
	
	WriteText("\n# kill_volumes_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Stops all scenario trigger volumes from killing players.");
	WriteText("\n#   1 - (DEFAULT) Normal value (resets trigger volume kill state if setting off previously).");
	WriteText("\n# This setting only applies if session host.");
	WriteText("\n");
	
	WriteText("\n");
	WriteTextF("\n[%s:%s]", XllnConfigHeader, XllnConfigVersion);
	WriteTextF("\nhide_ingame_chat = %u", H2Config_hide_ingame_chat ? 1 : 0);
	WriteTextF("\nhide_first_person = %u", H2Config_hide_first_person ? 1 : 0);
	WriteTextF("\nhide_HUD = %u", H2Config_hide_HUD ? 1 : 0);
	WriteTextF("\nfield_of_view = %d", H2Config_field_of_view);
	WriteTextF("\nfield_of_view_vehicle = %d", H2Config_field_of_view_vehicle);
	if (FloatIsNaN(H2Config_crosshair_offset)) {
		WriteText("\ncrosshair_offset = NaN");
	}
	else {
		WriteTextF("\ncrosshair_offset = %f", H2Config_crosshair_offset);
	}
	WriteTextF("\nsensitivity_native_controller = %f", H2Config_sensitivity_native_controller);
	WriteTextF("\nsensitivity_native_mouse = %f", H2Config_sensitivity_native_mouse);
	WriteTextF("\nsensitivity_raw_mouse = %u", H2Config_sensitivity_raw_mouse);
	WriteTextF("\nsensitivity_xy_axis_ratio_controller = %hhd", H2Config_sensitivity_xy_axis_ratio_controller);
	WriteTextF("\nsensitivity_xy_axis_ratio_mouse = %hhd", H2Config_sensitivity_xy_axis_ratio_mouse);
	WriteTextF("\nsensitivity_vehicle_ratio_controller = %f", H2Config_sensitivity_vehicle_ratio_controller);
	WriteTextF("\nsensitivity_vehicle_ratio_mouse = %f", H2Config_sensitivity_vehicle_ratio_mouse);
	WriteTextF("\nxDelay_enabled = %u", H2Config_xDelay_enabled ? 1 : 0);
	WriteTextF("\nwarp_fix_enabled = %u", H2Config_warp_fix_enabled ? 1 : 0);
	WriteTextF("\nvehicle_flip_eject_enabled = %u", H2Config_vehicle_flip_eject_enabled ? 1 : 0);
	WriteTextF("\nkill_volumes_enabled = %u", H2Config_kill_volumes_enabled ? 1 : 0);
	WriteText("\n\n");
	
	if (configContext) {
		for (char *&badEntry : configContext->badConfigEntries) {
			WriteText(badEntry);
			WriteText("\n");
		}
		if (configContext->badConfigEntries.size()) {
			WriteText("\n\n");
		}
		for (char *&otherEntry : configContext->otherConfigHeaderEntries) {
			WriteText(otherEntry);
			WriteText("\n");
		}
		if (configContext->otherConfigHeaderEntries.size()) {
			WriteText("\n");
		}
	}
	
#undef WriteText
#undef WriteTextF
	
	fclose(fileConfig);
	
	return S_OK;
}

static uint32_t XllnConfig(const wchar_t *config_filepath, bool save_config)
{
	// Always read the/a config file before saving.
	uint32_t result = ERROR_FUNCTION_FAILED;
	FILE* fileConfig = 0;
	errno_t errorFopen = _wfopen_s(&fileConfig, config_filepath, L"rb");
	if (!fileConfig) {
		if (errorFopen == ENOENT) {
			result = ERROR_FILE_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file not found: \"%ls\".", config_filepath);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file read error: %d, \"%ls\".", errorFopen, config_filepath);
			return ERROR_FUNCTION_FAILED;
		}
	}

	INTERPRET_CONFIG_CONTEXT *interpretationContext = 0;
	if (fileConfig) {
		interpretationContext = new INTERPRET_CONFIG_CONTEXT;
		interpretationContext->keepInvalidLines = true;
		interpretationContext->saveValuesRead = !save_config;

		ReadIniFile(fileConfig, true, XllnConfigHeader, XllnConfigVersion, interpretConfigSetting, (void*)interpretationContext);

		for (char *&readSettingName : interpretationContext->readSettings) {
			delete[] readSettingName;
		}
		interpretationContext->readSettings.clear();

		fclose(fileConfig);
		fileConfig = 0;
	}

	wchar_t *saveToConfigPath = PathFromFilename(config_filepath);
	uint32_t errorMkdir = EnsureDirectoryExists(saveToConfigPath);
	delete[] saveToConfigPath;
	if (errorMkdir) {
		result = ERROR_DIRECTORY;
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "EnsureDirectoryExists(...) error: %u, \"%ls\".", errorMkdir, config_filepath);
	}
	else {
		uint32_t errorSaveConfig = SaveXllnConfig(config_filepath, interpretationContext);
		if (errorSaveConfig) {
			result = errorSaveConfig;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "SaveXllnConfig(...) error: %u, \"%ls\".", errorSaveConfig, config_filepath);
		}
		else {
			result = ERROR_SUCCESS;
		}
	}

	if (interpretationContext) {
		for (char *&badEntry : interpretationContext->badConfigEntries) {
			delete[] badEntry;
		}
		interpretationContext->badConfigEntries.clear();
		for (char *&otherEntry : interpretationContext->otherConfigHeaderEntries) {
			delete[] otherEntry;
		}
		interpretationContext->otherConfigHeaderEntries.clear();
	}

	return result;
}

static wchar_t *pcarto_config = 0;

HRESULT InitConfig()
{
	size_t storagePathBufSize = 0;
	uint32_t errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, 0, 0, &storagePathBufSize);
	if (errorGetStoragePath == ERROR_INSUFFICIENT_BUFFER) {
		uint32_t xllnLocalInstanceId = 0;
		wchar_t *storagePath = new wchar_t[storagePathBufSize / sizeof(wchar_t)];
		errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, &xllnLocalInstanceId, storagePath, &storagePathBufSize);
		if (errorGetStoragePath == ERROR_SUCCESS) {
			pcarto_config = FormMallocString(L"%sxlln-halo-2-cartographer-%u.ini", storagePath, xllnLocalInstanceId);
			XllnConfig(pcarto_config, false);
		}
		delete[] storagePath;
	}
	return ERROR_SUCCESS;
}

HRESULT UninitConfig()
{
	if (pcarto_config) {
		XllnConfig(pcarto_config, true);
		free(pcarto_config);
		pcarto_config = 0;
	}
	return ERROR_SUCCESS;
}
