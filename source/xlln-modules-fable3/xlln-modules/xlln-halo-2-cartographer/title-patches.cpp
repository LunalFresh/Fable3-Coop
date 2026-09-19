#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "title-patches.hpp"
#include "../utils/util-hook.hpp"
#include "xlive.hpp"
#include "../xlivelessness.hpp"
#include "menus/custom-menu.hpp"
#include "menus/advanced-settings/adv-lobby/adv-lobby.hpp"
#include "language/custom-language.hpp"
#include "config/config.hpp"
#include "language/custom-language.hpp"
#include "run-loop/run-loop.hpp"
#include "coop/coop.hpp"
#include "../utils/utils.hpp"
#define _USE_MATH_DEFINES
#include "math.h"
#include "title-modules/Tags/TagInterface.hpp"
#include <dinput.h>

float player_controls_sensitivity_vehicle_ratios_controller[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float player_controls_sensitivity_vehicle_ratios_mouse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

#pragma region Custom_Language_And_Font

typedef char*(__stdcall *tH2GetLabel)(int, int, int, int);
tH2GetLabel pH2GetLabel;
char* __stdcall H2GetLabel(int a1, int label_id, int a3, int a4)
{ //sub_3defd
	//int label_menu_id = *(int*)(*(int*)a1 + 8 * a3 + 4);
	int label_menu_id = a3;
	char* label = get_cartographer_label(label_menu_id, label_id, 0b10);
	if (label)
		return label;
	label = get_custom_label(current_language, label_menu_id, label_id, 0);
	if (label)
		return label;
	label = get_cartographer_label(label_menu_id, label_id, 0b01);
	if (!label && a1)
		label = pH2GetLabel(a1, label_id, a3, a4);
	//if (strcmp(label, "PROFILE NAME") == 0) {
	//	return label;//in order to breakpoint and get label_id's.
	//}
	if (!H2Config_custom_labels_capture_missing) {
		return label;
	}
	return get_custom_label(current_language, label_menu_id, label_id, label);
}

char* __cdecl cave_c00031b97(char* result, int buff_len)//Font Table Filename Override
{
	strcpy_s(result, buff_len, current_language->font_table_filename);
	return result;
}

char*(__cdecl* pfn_c00031b97)(int, int) = 0;
//__usercall - edi a1, stack a2
__declspec(naked) char* nak_c00031b97()
{
	__asm {
		push ebp
		push edi
		push esi
		push ecx
		push ebx

		mov eax, [esp + 18h]
		push eax//buff_len

		push eax
		call pfn_c00031b97
		add esp, 4h

		push eax//result
		call cave_c00031b97
		add esp, 8h

		pop ebx
		pop ecx
		pop esi
		pop edi
		pop ebp

		retn
	}
}

DWORD langAfterJmpAddr;
__declspec(naked) void getSystemLanguageMethodJmp() {
	CLSetGameLanguage();
	__asm {
		jmp langAfterJmpAddr
	}
}

#pragma endregion

#pragma region Skulls

static uint32_t GetSkullIndexOffset(int skullLabelIndex)
{
	// Skulls are ordered/indexed alphabetically.
	switch (skullLabelIndex) {
	case 0:
		return 0x4D832D; // Anger
	case 1:
		return 0x4D8322; // Assassins
	case 2:
		return 0x4D8328; // Black Eye
	case 3:
		return 0x4D8326; // Blind
	case 4:
		return 0x4D8329; // Catch
	case 5:
		return 0x4D8320; // Envy
	case 6:
		return 0x4D8324; // Famine
	case 7:
		return 0x4D8327; // Ghost
	case 8:
		return 0x4D8321; // Grunt Birthday Party
	case 9:
		return 0x4D832B; // Iron
	case 0xA:
		return 0x4D8325; // IWHBYD
	case 0xB:
		return 0x4D832C; // Mythic
	case 0xC:
		return 0x4D832A; // Sputnik
	case 0xD:
		return 0x4D8323; // Thunderstorm
	case 0xE:
		return 0x4D832E; // Whuppopotamus
	}
	return 0;
}

bool ToggleSkull(int skullLabelIndex)
{
	bool isSkullActive = false;

	if (skullLabelIndex >= 0 && skullLabelIndex < 15) {
		uint8_t &skull = *(uint8_t*)(GetOffsetAddress(CLIENT_11122, GetSkullIndexOffset(skullLabelIndex)));
		skull = !skull;
		isSkullActive = skull != 0;
	}

	return isSkullActive;
}

bool LoadSkullStatus(int skullLabelIndex)
{
	bool isSkullActive = false;

	if (skullLabelIndex >= 0 && skullLabelIndex < 15) {
		uint8_t &skull = *(uint8_t*)(GetOffsetAddress(CLIENT_11122, GetSkullIndexOffset(skullLabelIndex)));
		isSkullActive = skull != 0;
	}

	return isSkullActive;
}

typedef uint8_t(__cdecl *tHookIsSkullActive)(uint32_t);
tHookIsSkullActive pHookIsSkullActive;
static uint8_t __cdecl HookIsSkullActive(uint32_t skull_index)
{
	if (skull_index > 0xE) {
		return 0;
	}
	
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	
	if (gameEngine != 1 && !coop_mode_fix_active) {
		return 0;
	}
	
	uint8_t *skulls = (uint8_t*)GetOffsetAddress(CLIENT_11122, 0x004d8320);
	uint8_t result = skulls[skull_index];
	return result;
}

#pragma endregion

#pragma region Trigger_Volumes

static int32_t get_scenario_volume_count()
{
	uint32_t &scenarioGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00479e74);
	int32_t volume_count = *(int32_t*)(scenarioGlobals + 0x108);
	return volume_count;
}

static bool scenario_trigger_volume_will_kill(uint32_t volume_id)
{
	uint32_t &scenarioGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00479e74);
	uint32_t &var_00482290 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482290);
	uint32_t &var_004d2ab0 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004d2ab0);
	bool result = false;
	
	uint32_t v1;
	uint32_t v2;
	bool v3;
	uint32_t v4;
	
	if (volume_id != -1 ) {
		v1 = *(uint32_t*)(scenarioGlobals + 268);
		v2 = 0;
		if ( v1 != -1 ) {
			v2 = v1 + var_00482290;
		}
		v3 = *(uint16_t*)(v2 + 68 * volume_id + 64) == 0xFFFF;
		v4 = v2 + 68 * volume_id;
		if ( !v3 ) {
			uint32_t c1 = *(uint32_t*)(var_004d2ab0 + 4 * (*(uint16_t*)(v4 + 64) >> 5));
			uint32_t c2 = (1 << (*(uint16_t*)(v4 + 64) & 0x1F));
			result = !(c1 & c2);
		}
	}
	
	return result;
}

static std::vector<bool> trigger_volumes_kill_enabled;

typedef void(__cdecl *tKillVolumeEnable)(int32_t volume_id);
static tKillVolumeEnable pKillVolumeEnable;
static void __cdecl KillVolumeEnable(int32_t volume_id)
{
	if (H2Config_kill_volumes_enabled) {
		pKillVolumeEnable(volume_id);
	}
	
	if (volume_id < 0 || (uint32_t)volume_id >= trigger_volumes_kill_enabled.size()) {
		return;
	}
	
	trigger_volumes_kill_enabled[volume_id] = true;
}

typedef void(__cdecl *tKillVolumeDisable)(int32_t volume_id);
static tKillVolumeDisable pKillVolumeDisable;
static void __cdecl KillVolumeDisable(int32_t volume_id)
{
	pKillVolumeDisable(volume_id);
	
	if (volume_id < 0 || (uint32_t)volume_id >= trigger_volumes_kill_enabled.size()) {
		return;
	}
	
	trigger_volumes_kill_enabled[volume_id] = false;
}

static void RecordTriggerVolumeActiveKillState()
{
	// Only supporting this version at the moment.
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	int32_t triggerVolumeCount = get_scenario_volume_count();
	
	XLLN_DEBUG_LOG(
		XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Number of Trigger Volumes: %d."
		, __func__
		, triggerVolumeCount
	);
	
	trigger_volumes_kill_enabled.clear();
	
	for (int32_t iVolume = 0; iVolume < triggerVolumeCount; iVolume++) {
		bool willKill = scenario_trigger_volume_will_kill(iVolume);
		trigger_volumes_kill_enabled.push_back(willKill);
	}
}

void RefreshToggleKillVolumes()
{
	// Only supporting this version at the moment.
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	int32_t triggerVolumeCount = get_scenario_volume_count();
	
	if (H2Config_kill_volumes_enabled) {
		if (trigger_volumes_kill_enabled.size() == triggerVolumeCount) {
			for (int32_t iVolume = 0; iVolume < triggerVolumeCount; iVolume++) {
				if (trigger_volumes_kill_enabled[iVolume]) {
					pKillVolumeEnable(iVolume);
				}
				else {
					pKillVolumeDisable(iVolume);
				}
			}
		}
	}
	else {
		for (int32_t iVolume = 0; iVolume < triggerVolumeCount; iVolume++) {
			pKillVolumeDisable(iVolume);
		}
	}
}

#pragma endregion

static bool __cdecl HideIngameChat()
{
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	uint8_t &gameState = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00420fc4);

	if (H2Config_hide_ingame_chat) {
		return true;
	}
	else if (gameEngine != 3 && gameState == 3) {
		// Enable chat in engine mode and game state mp.
		return false;
	}
	else {
		// The original test: if is campaign.
		return gameEngine == 1;
	}
}

// Render first person model.
static uint8_t __cdecl HideFirstPerson(uint32_t a1)
{
	uint8_t result = H2Config_hide_first_person ? 1 : 0;
	//if (AdvLobbySettings_mp_blind & 0b10) {
	//	result = 1;
	//}
	return result;
}

// Render hud.
static uint8_t __cdecl HideHUD(uint32_t a1)
{
	uint32_t PlayerGlobalBase = (uint32_t)0x30004B60;
	bool& IsPlayerDead = (bool&)*(uint8_t*)(PlayerGlobalBase + 4);
	// Fixes game notification text from glitching out.
	if (IsPlayerDead) {
		return 0;
	}
	uint8_t result = H2Config_hide_HUD ? 1 : 0;
	//if (AdvLobbySettings_mp_blind & 0b01) {
	//	result = 1;
	//}
	return result;
}

static uint32_t __cdecl BansheeBombIsDisabled()
{
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	
	if (gameEngine == 1 || (coop_mode_fix_active && gameEngine != 3)) {
		return 0;
	}
	
	return 1;
}

static uint32_t __cdecl GrenadeChainReactionsAreDisabled()
{
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	
	if (gameEngine == 1 || (coop_mode_fix_active && gameEngine != 3)) {
		return 0;
	}
	
	return 1;
}

static bool __cdecl ExplosionPhysicsEnabledOnNonHost()
{
	return coop_mode_fix_active;
}

static __declspec(naked) void CodeCaveNakExplosionPhysicsEnabledOnNonHost()
{
	__asm {
		test ecx, 0x100
		jnz Exit
		
		test ecx, 0x12
		jz Exit
		
		push ecx
		call ExplosionPhysicsEnabledOnNonHost
		pop ecx
		test al, al
		
	Exit:
		retn
	}
}

void SetFOV(int field_of_view_degrees)
{
	static float fov = (float)(70.0f * M_PI / 180.0f);
	static bool fov_redirected = false;
	if (field_of_view_degrees > 0 && field_of_view_degrees <= 110)
	{
		if (!fov_redirected)
		{
			// fld dword ptr[fov]
			uint8_t opcode[6] = { 0xD9, 0x05, 0x00, 0x00, 0x00, 0x00 };
			WritePointer((uint32_t)&opcode[2], &fov);
			WriteBytes(GetOffsetAddress(CLIENT_11122, 0x000907f3), opcode, sizeof(opcode));

			fov_redirected = true;
		}

		//const double default_radians_field_of_view = 70.0f * M_PI / 180.0f;
		fov = (float)((float)field_of_view_degrees * M_PI / 180.0f);
	}
}

void SetFOVVehicle(int field_of_view_degrees)
{
	if (field_of_view_degrees > 0 && field_of_view_degrees <= 110)
	{
		float calculated_radians_FOV = (float)((float)field_of_view_degrees * M_PI / 180.0f);
		WriteValue(GetOffsetAddress(CLIENT_11122, 0x00413780), calculated_radians_FOV);
	}
}

void SetCrosshairVerticalOffset(float crosshair_offset)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "Setting crosshair vertical position."
	);
	if (!FloatIsNaN(crosshair_offset)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "Setting crosshair vertical position to: %.6f."
			, crosshair_offset
		);
		tags::tag_data_block* player_controls_block = reinterpret_cast<tags::tag_data_block*>(tags::get_matg_globals_ptr() + 240);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "Player_Controls_Block - Offset: %08x, Count: %d."
			, player_controls_block->block_data_offset
			, player_controls_block->block_count
		);
		if (player_controls_block->block_count > 0)
		{
			for (int i = 0; i < player_controls_block->block_count; i++) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
					, "Setting Player Control Tag Block Index: %d."
					, player_controls_block->block_count
				);
				*(float*)(tags::get_tag_data() + player_controls_block->block_data_offset + 128 * i + 28) = crosshair_offset;
			}
		}
	}
}

typedef char(__cdecl *tLocalPlayerMouseInput)(uint32_t local_player_index, void *data, int a3, float *a4, float *a5, void *a6);
static tLocalPlayerMouseInput pLocalPlayerMouseInput;
static char __cdecl LocalPlayerMouseInput(uint32_t local_player_index, uint8_t *data, int a3, float *a4, float *a5, uint8_t *local_player_mouse_stuff)
{
	char result = pLocalPlayerMouseInput(local_player_index, data, a3, a4, a5, local_player_mouse_stuff);
	
	if (local_player_index == 0 && H2Config_sensitivity_raw_mouse > 0) {
		float& dx = *(float*)&local_player_mouse_stuff[0x98];
		float& dy = *(float*)&local_player_mouse_stuff[0x9c];
		
		bool& invertedYAxisMouse = *(bool*)&data[0x11];
		
		uint8_t *GlobalsTime = *(uint8_t**)GetOffsetAddress(CLIENT_11122, 0x004c06e4);
		float& secondsPerTick = *(float*)&GlobalsTime[0x4];
		
		// This part only found for player index 0.
		DIMOUSESTATE *ms = (DIMOUSESTATE*)GetOffsetAddress(CLIENT_11122, 0x0047a570);
		
		float rawDX = secondsPerTick * (float)ms->lX * -(H2Config_sensitivity_raw_mouse / 1000.0f);
		float rawDY = secondsPerTick * (float)ms->lY * -(H2Config_sensitivity_raw_mouse / 1000.0f);
		
		if (invertedYAxisMouse) {
			rawDY *= -1.0f;
		}
		
		if (H2Config_sensitivity_xy_axis_ratio_mouse > 0 && H2Config_sensitivity_xy_axis_ratio_mouse <= 100) {
			rawDY *= (100 - H2Config_sensitivity_xy_axis_ratio_mouse) / 100.0f;
		}
		else if (H2Config_sensitivity_xy_axis_ratio_mouse < 0 && H2Config_sensitivity_xy_axis_ratio_mouse >= -100) {
			rawDX *= (100 - -H2Config_sensitivity_xy_axis_ratio_mouse) / 100.0f;
		}
		
		dx = rawDX;
		dy = rawDY;
	}
	
	return result;
}

// sensitivity: Valid values are anything >0.0f. 1.0f to 10.0f correspond to the native game sensitivity discrete setting values.
// sensitivity_xy_axis_ratio:
//   A value from 1 to 100 is when the X axis is the base value and the Y axis is the product of the base and the inverse ratio value (100 - value).
//   A value from -1 to -100 is when the Y asis becomes the base value and the X is the product of that and the absolute value of the inverse ratio value (100 - abs(value)).
//   E.g. Ratio = 80, Base = 50 corresponds to X = 50, Y = 50 * (100 - 80)/100 = 10 (which is 20% of X, or 80% less than X) Ratio = -20, Base = 50 corresponds to X = 50 * (100 - -(-20))/100 = 40 (which is 80% of Y, or 20% less than Y), Y = 50.
//   A value of 0 is a 1:1 ratio where X and Y are both the Base sensitivity value.
//   The native default is 50 such that the Y axis is 50% less sensitive than the X.
void UpdateLocalPlayerControlsSensitivity(uint32_t local_player_index, float sensitivity_controller, int8_t sensitivity_xy_axis_ratio_controller, float sensitivity_mouse, int8_t sensitivity_xy_axis_ratio_mouse)
{
	bool mouse = false;
	do {
		float sensitivity;
		int8_t sensitivity_xy_axis_ratio;
		
		uint8_t* sensitivitySetting;
		float* sensitivityAxisX;
		float* sensitivityAxisY;
		
		if (mouse) {
			sensitivity = sensitivity_mouse;
			sensitivity_xy_axis_ratio = sensitivity_xy_axis_ratio_mouse;
			uint8_t& sensitivitySettingMouse = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0096cb5f + (0x127C * local_player_index));
			float& sensitivityAxisXMouse = *(float*)GetOffsetAddress(CLIENT_11122, 0x004a89b0 + (0x5a0 * local_player_index));
			float& sensitivityAxisYMouse = *(float*)GetOffsetAddress(CLIENT_11122, 0x004a89b4 + (0x5a0 * local_player_index));
			sensitivitySetting = &sensitivitySettingMouse;
			sensitivityAxisX = &sensitivityAxisXMouse;
			sensitivityAxisY = &sensitivityAxisYMouse;
		}
		else {
			sensitivity = sensitivity_controller;
			sensitivity_xy_axis_ratio = sensitivity_xy_axis_ratio_controller;
			uint8_t& sensitivitySettingController = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0096cb5e + (0x127C * local_player_index));
			float& sensitivityAxisXController = *(float*)GetOffsetAddress(CLIENT_11122, 0x004a89b8 + (0x5a0 * local_player_index));
			float& sensitivityAxisYController = *(float*)GetOffsetAddress(CLIENT_11122, 0x004a89bc + (0x5a0 * local_player_index));
			sensitivitySetting = &sensitivitySettingController;
			sensitivityAxisX = &sensitivityAxisXController;
			sensitivityAxisY = &sensitivityAxisYController;
			
		}
		
		uint32_t sensitivityConvertedNative = (uint32_t)(sensitivity + 0.5f);
		if (sensitivityConvertedNative < 1 || sensitivity <= 0.0f) {
			sensitivityConvertedNative = 1;
		}
		else if (sensitivityConvertedNative > 10 || sensitivity > 10.0f) {
			sensitivityConvertedNative = 10;
		}
		*sensitivitySetting = (uint8_t)sensitivityConvertedNative;
		
		// This formula originates from the discrete but linear series of 10 floats (10 because in the gui there are 10 sensitivity settings) found in the X axis sensitivity value list (the Y axis sensitivity list is comprised of values equal to half of what is found in the X list).
		// For controller the full value is used, for mouse 30.0f is subtracted from X and 15.0f (again, exactly half) for Y.
		// Halo 2 version 0x10000481 offset for X and Y axis 10 floats array: 0x003cb784, 0x003cb75c; offset for subtract 30.0f and 15.0f: 0x002081f9, 0x002081d1.
		// Formula is 1.0f to 10.0f matches the original sensitivity values while anything outside of that range is more or less.
		// Values <= 0.0f are invalid and treated as 1.0f.
		float sensitivityBaseValue = (20.0f * (sensitivity >= 1.0f ? sensitivity - 1.0f : 0.0f)) + 80.0f;
		if (mouse) {
			sensitivityBaseValue -= 30.0f;
		}
		sensitivityBaseValue *= ((sensitivity > 0.0f && sensitivity < 1.0f) ? sensitivity : 1.0f);
		
		*sensitivityAxisX = *sensitivityAxisY = sensitivityBaseValue;
		
		if (sensitivity_xy_axis_ratio > 0 && sensitivity_xy_axis_ratio <= 100) {
			*sensitivityAxisY *= (100 - sensitivity_xy_axis_ratio) / 100.0f;
		}
		else if (sensitivity_xy_axis_ratio < 0 && sensitivity_xy_axis_ratio >= -100) {
			*sensitivityAxisX *= (100 - -sensitivity_xy_axis_ratio) / 100.0f;
		}
		
	} while (mouse = !mouse);
}

typedef bool(__cdecl *tSetLocalPlayerSensitivity)(uint32_t local_player_index, uint8_t* a2, int a3);
static tSetLocalPlayerSensitivity pSetLocalPlayerSensitivity;
static bool __cdecl HookSetLocalPlayerSensitivity(uint32_t local_player_index, uint8_t* a2, int a3)
{
	uint8_t& sensitivitySettingController = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0096cb5e + (0x127C * local_player_index));
	uint8_t& sensitivitySettingMouse = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0096cb5f + (0x127C * local_player_index));
	
	bool initialising = (sensitivitySettingController == 0);
	
	bool result = pSetLocalPlayerSensitivity(local_player_index, a2, a3);
	
	if (local_player_index == 0) {
		// Update our setting if the user changed it from the native GUI.
		if (!initialising) {
			H2Config_sensitivity_native_controller = (float)sensitivitySettingController;
			H2Config_sensitivity_native_mouse = (float)sensitivitySettingMouse;
		}
		
		UpdateLocalPlayerControlsSensitivity(0, H2Config_sensitivity_native_controller, H2Config_sensitivity_xy_axis_ratio_controller, H2Config_sensitivity_native_mouse, H2Config_sensitivity_xy_axis_ratio_mouse);
	}
	else {
		UpdateLocalPlayerControlsSensitivity(local_player_index, (float)sensitivitySettingController, 50, (float)sensitivitySettingMouse, 50);
	}
	
	return result;
}

void RefreshToggleWarpFix()
{
	// Only supporting this version at the moment.
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	uint32_t patchAddress;
	// Improves warping issues.
	if (patchAddress = GetOffsetAddress(CLIENT_11122, 0x004f958c)) {
		WriteValue<float>(patchAddress, H2Config_warp_fix_enabled ? 4.0f : 2.5f);
	}
	if (patchAddress = GetOffsetAddress(CLIENT_11122, 0x004f9594)) {
		WriteValue<float>(patchAddress, H2Config_warp_fix_enabled ? 10.0f : 7.5f);
	}
	
	XLLN_DEBUG_LOG(
		XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warp Fix %s."
		, __func__
		, H2Config_warp_fix_enabled ? "enabled" : "disabled"
	);
}

void RefreshToggleXDelay()
{
	uint32_t patchAddress;
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001c9d8e)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001a1316))) {
		WriteValue<uint8_t>(patchAddress, H2Config_xDelay_enabled ? (uint8_t)0x74 : (uint8_t)0xEB);
	}
}

void RefreshToggleVehicleFlipEject()
{
	uint32_t patchAddress;
	if (patchAddress = GetOffsetAddress(CLIENT_11122, 0x00159e5d)) {
		WriteValue<uint8_t>(patchAddress, H2Config_vehicle_flip_eject_enabled ? (uint8_t)0x7E : (uint8_t)0xEB);
	}
}

#pragma region Experimental

typedef int(__cdecl *tfn_0020B8C3)(DWORD*, DWORD*);
static tfn_0020B8C3 pfn_0020B8C3;
static int __cdecl fn_0020B8C3(DWORD *a1, DWORD *a2)
{
	char menu_details[150];
	snprintf(menu_details, 150, "MENU l0:%hX h0:%hX 1:%X 2:%X 3:%X 4:%X 5:%X 6:%X 7:%X", ((WORD*)a2)[0], ((WORD*)a2)[1], a2[1], a2[2], a2[3], a2[4], a2[5], a2[6], a2[7]);
	XLLNDebugLog(0, menu_details);
	int result = pfn_0020B8C3(a1, a2);
	return result;
}

#pragma endregion

#pragma region Lobby_Button_Hooks

typedef char(__stdcall *tfn_c0024eeef)(DWORD*, int, int);
tfn_c0024eeef pfn_c0024eeef;
char __stdcall fn_c0024eeef(DWORD* thisptr, int a2, int a3)//__thiscall
{
	//char result = pfn_c0024eeef(thisptr, a2, a3);
	//return result;

	char(__thiscall* fn_c002139f8)(DWORD*, int, int, int, int*, int) = (char(__thiscall*)(DWORD*, int, int, int, int*, int))(GetOffsetAddress(CLIENT_11122, 0x002139f8));

	int label_list[16];
	label_list[0] = 0;
	label_list[1] = 0xA0005D1;//"Change Map..."
	label_list[2] = 1;
	label_list[3] = 0xE0005D2;//"Change Rules..."
	label_list[4] = 2;
	label_list[5] = 0xD000428;//"Quick Options..."
	label_list[6] = 3;
	label_list[7] = 0x1000095D;//"Party Management..."
	label_list[8] = 4;
	label_list[9] = 0x0E0005D9;//"Switch To: Co-Op Game"
	label_list[10] = 5;
	label_list[11] = 0x150005D8;//"Switch To: Custom Game"
	label_list[12] = 6;
	label_list[13] = 0x130005DA;//"Switch To: Matchmaking"
	label_list[14] = 7;
	label_list[15] = 0xD0005D6;//"Change Match Settings..."

	//label_list[8] = ?;
	//label_list[9] = 0x120005D7;//"Switch To: Playlist"
	//label_list[10] = ?;
	//label_list[11] = 0x150005d8;//"Switch To: Custom Game"

	return fn_c002139f8(thisptr, a2, a3, 0, label_list, 8);
}

typedef int(__stdcall *tfn_c0024fa19)(DWORD*, int, int*);
tfn_c0024fa19 pfn_c0024fa19;
int __stdcall fn_c0024fa19(DWORD* thisptr, int a2, int* a3)//__thiscall
{
	//int result = pfn_c0024fa19(thisptr, a2, a3);
	//return result;

	int(__stdcall* fn_c0024f9a1)(int) = (int(__stdcall*)(int))(GetOffsetAddress(CLIENT_11122, 0x0024f9a1));
	int(__stdcall* fn_c0024f9dd)(int) = (int(__stdcall*)(int))(GetOffsetAddress(CLIENT_11122, 0x0024f9dd));
	int(__stdcall* fn_c0024ef79)(int) = (int(__stdcall*)(int))(GetOffsetAddress(CLIENT_11122, 0x0024ef79));
	int(__stdcall* fn_c0024f5fd)(int) = (int(__stdcall*)(int))(GetOffsetAddress(CLIENT_11122, 0x0024f5fd));
	int(__thiscall* fn_c0024f015)(DWORD* thisptr, int) = (int(__thiscall*)(DWORD*, int))(GetOffsetAddress(CLIENT_11122, 0x0024f015));
	int(__thiscall* fn_c0024f676)(DWORD* thisptr, int) = (int(__thiscall*)(DWORD*, int))(GetOffsetAddress(CLIENT_11122, 0x0024f676));
	int(__thiscall* fn_c0024f68a)(DWORD* thisptr, int) = (int(__thiscall*)(DWORD*, int))(GetOffsetAddress(CLIENT_11122, 0x0024f68a));

	int result = *a3;
	if (*a3 != -1) {
		result = *(signed __int16 *)(*(DWORD *)(thisptr[28] + 68) + 4 * (unsigned __int16)result + 2);
		switch (result) {
			case 0: {
				result = fn_c0024f9a1(a2);
				break;
			}
			case 1: {
				result = fn_c0024f9dd(a2);
				break;
			}
			case 2: {
				result = fn_c0024ef79(a2);
				break;
			}
			case 3: {
				result = fn_c0024f5fd(a2);
				break;
			}
			case 4: {
				CustomMenuCall_AdvLobbySettings();
				break;
			}
			case 5: {
				result = fn_c0024f015(thisptr, a2);
				break;
			}
			case 6: {
				result = fn_c0024f676(thisptr, a2);
				break;
			}
			case 7: {
				result = fn_c0024f68a(thisptr, a2);
				break;
			}
			default: {
				return result;
			}
		}
	}
	return result;
}

typedef DWORD*(__stdcall *tfn_c0024fabc)(DWORD*, int);
tfn_c0024fabc pfn_c0024fabc;
DWORD* __stdcall fn_c0024fabc(DWORD* thisptr, int a2)//__thiscall
{
	//DWORD* result = pfn_c0024fabc(thisptr, a2);
	//return result;

	DWORD* var_c003d9254 = (DWORD*)(GetOffsetAddress(CLIENT_11122, 0x003d9254));
	DWORD* var_c003d9188 = (DWORD*)(GetOffsetAddress(CLIENT_11122, 0x003d9188));

	DWORD*(__thiscall* fn_c00213b1c)(DWORD* thisptr, int) = (DWORD*(__thiscall*)(DWORD*, int))(GetOffsetAddress(CLIENT_11122, 0x00213b1c));
	int(__thiscall* fn_c0000a551)(DWORD* thisptr) = (int(__thiscall*)(DWORD*))(GetOffsetAddress(CLIENT_11122, 0x0000a551));
	DWORD*(__thiscall* fn_c0021ffc9)(DWORD* thisptr) = (DWORD*(__thiscall*)(DWORD*))(GetOffsetAddress(CLIENT_11122, 0x0021ffc9));
	void(__stdcall* fn_c0028870b)(int, int, int, DWORD*(__thiscall*)(DWORD*), int(__thiscall*)(DWORD*)) = (void(__stdcall*)(int, int, int, DWORD*(__thiscall*)(DWORD*), int(__thiscall*)(DWORD*)))(GetOffsetAddress(CLIENT_11122, 0x0028870b));
	DWORD*(__thiscall* fn_c002113c6)(DWORD* thisptr) = (DWORD*(__thiscall*)(DWORD*))(GetOffsetAddress(CLIENT_11122, 0x002113c6));
	int(__thiscall* fn_c0024fa19)(DWORD* thisptr, int, int*) = (int(__thiscall*)(DWORD*, int, int*))(GetOffsetAddress(CLIENT_11122, 0x0024fa19));
	int(*fn_c00215ea9)() = (int(*)())(GetOffsetAddress(CLIENT_11122, 0x00215ea9));
	int(__cdecl* fn_c0020d1fd)(char*, int numberOfButtons, int) = (int(__cdecl*)(char*, int, int))(GetOffsetAddress(CLIENT_11122, 0x0020d1fd));
	int(__cdecl* fn_c00066b33)(int) = (int(__cdecl*)(int))(GetOffsetAddress(CLIENT_11122, 0x00066b33));
	int(__cdecl* fn_c000667a0)(int) = (int(__cdecl*)(int))(GetOffsetAddress(CLIENT_11122, 0x000667a0));
	int(*fn_c002152b0)() = (int(*)())(GetOffsetAddress(CLIENT_11122, 0x002152b0));
	int(*fn_c0021525a)() = (int(*)())(GetOffsetAddress(CLIENT_11122, 0x0021525a));
	int(__thiscall* fn_c002113d3)(DWORD* thisptr, DWORD*) = (int(__thiscall*)(DWORD*, DWORD*))(GetOffsetAddress(CLIENT_11122, 0x002113d3));

	DWORD* v2 = thisptr;
	fn_c00213b1c(thisptr, a2);
	//*v2 = &c_squad_settings_list::`vftable';
	v2[0] = (DWORD)var_c003d9254;
	//*v2 = (DWORD)((BYTE*)H2BaseAddr + 0x003d9254);
	fn_c0028870b((int)(v2 + 44), 132, 7, fn_c0021ffc9, fn_c0000a551);
	fn_c002113c6(v2 + 276);
	//v2[275] = &c_slot2<c_squad_settings_list, s_event_record *, long>::`vftable';
	v2[275] = (DWORD)var_c003d9188;
	//v2[275] = (DWORD)((BYTE*)H2BaseAddr + 0x003d9188);
	v2[279] = (DWORD)v2;
	v2[280] = (DWORD)fn_c0024fa19;
	int v3 = fn_c00215ea9();
	int v4 = fn_c0020d1fd("squad setting list", 8, 4);
	v2[28] = v4;
	fn_c00066b33(v4);
	*((BYTE *)v2 + 1124) = 1;
	switch (v3) {
		case 1:
		case 3: {
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 0;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 1;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 2;
			//*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 3;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 4;
			/*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 5;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 6;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 7;*/
			break;
		}
		case 5: {
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 0;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 1;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 2;
			if ((unsigned __int8)fn_c002152b0() && fn_c0021525a() > 1)
			{
				*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 3;
				*((BYTE *)v2 + 1124) = 0;
			}
			else
			{
				*((BYTE *)v2 + 1124) = 1;
			}
			break;
		}
		case 6: {
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 7;
			*(WORD *)(*(DWORD *)(v2[28] + 68) + 4 * (unsigned __int16)fn_c000667a0(v2[28]) + 2) = 5;
			break;
		}
		default: {
			break;
		}
	}
	DWORD* v6;
	if (v2 == (DWORD *)-1100) {
		v6 = 0;
	}
	else {
		v6 = v2 + 276;
	}
	fn_c002113d3(v2 + 43, v6);
	return v2;
}

#pragma endregion

typedef bool(__cdecl *tOnMapLoad)(uint32_t*);
static tOnMapLoad pOnMapLoad;
static bool __cdecl OnMapLoad(uint32_t *a1)
{
	bool resultLoadSuccess = pOnMapLoad(a1);
	if (resultLoadSuccess) {
		SetCrosshairVerticalOffset(H2Config_crosshair_offset);
		// FIXME
		extern void get_object_table_memory();
		get_object_table_memory();
	}
	return resultLoadSuccess;
}

typedef int(__cdecl *tfn_001aa955)();
static tfn_001aa955 pfn_001aa955;
static int __cdecl fn_001aa955()
{
	int result = pfn_001aa955();
	
	RefreshToggleWarpFix();
	
	return result;
}

// XXX Hook Disabled.
// Hook for the function that executes Haloscript scripts. Returns -1 when a script ends (was its final run).
typedef uint8_t(__cdecl *tfn_00096a71)(uint32_t);
static tfn_00096a71 pfn_00096a71;
static __declspec(naked) uint8_t __cdecl pfn_nak_00096a71(uint32_t a1)
{
	__asm {
		push edi
		
		mov edi, [esp + 0x8]
		call pfn_00096a71
		
		pop edi
		retn
	}
}
static uint8_t __cdecl fn_00096a71(uint32_t a1)
{
	uint8_t result = pfn_nak_00096a71(a1);
	
	if (result == (uint8_t)-1) {
		
	}
	
	return result;
}
static __declspec(naked) void fn_nak_00096a71()
{
	__asm {
		push edi
		
		push edi
		call fn_00096a71
		add esp, 0x4
		
		pop edi
		retn
	}
}

typedef int32_t(__cdecl *tInitializeGameEngineSystems)(uint8_t*);
static tInitializeGameEngineSystems pInitializeGameEngineSystems;
static int32_t __cdecl InitializeGameEngineSystems(uint8_t *a1)
{
	CoopInitializeGameEngineSystems(a1);
	
	int32_t result = pInitializeGameEngineSystems(a1);
	
	RecordTriggerVolumeActiveKillState();
	RefreshToggleKillVolumes();
	
	return result;
}

typedef void(__cdecl *tSpawnPlayer)(int32_t);
static tSpawnPlayer pSpawnPlayer;
static void __cdecl SpawnPlayer(int32_t playerIndex)
{
	CoopSpawnPlayer(playerIndex);
	
	pSpawnPlayer(playerIndex);
}

typedef int32_t(__cdecl *tSetPlayerUnitDatumAndModel)(int32_t, int32_t, int32_t);
static tSetPlayerUnitDatumAndModel pSetPlayerUnitDatumAndModel;
static int32_t __cdecl SetPlayerUnitDatumAndModel(int32_t playerIndex, int32_t unitDatum, int32_t playerModel)
{
	CoopSetPlayerUnitDatumAndModel(playerIndex, &unitDatum, &playerModel);
	
	int32_t result = pSetPlayerUnitDatumAndModel(playerIndex, unitDatum, playerModel);
	return result;
}

static bool __cdecl XllnGuideUiHandler()
{
	if (Title_Version == CLIENT_11081 || Title_Version == CLIENT_11091 || Title_Version == CLIENT_11122) {
		tXShowGuideUI AnyXShowGuideUI = (tXShowGuideUI)**(DWORD**)(GetOffsetAddress(0x0000e8a4 + 2, 0x0000e8a4 + 2, 0x0000e8a4 + 2));
		AnyXShowGuideUI(0);
		
		return true;
	}
	return false;
}

bool InitTitlePatches()
{
	uint32_t patchAddress = 0;
	
	if (Title_Version == CLIENT_11122) {
		// Useful for finding/logging different kinds of menus that get rendered.
		//pfn_0020B8C3 = (tfn_0020B8C3)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0020B8C3), (uint8_t*)fn_0020B8C3, 7);
		
		// Required for Custom Menus and Custom Language.
		pH2GetLabel = (tH2GetLabel)DetourClassFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0003defd), (uint8_t*)H2GetLabel, 8);
		
		// Hook the function that sets the font table filename.
		pfn_c00031b97 = (char*(__cdecl*)(int, int))(GetOffsetAddress(CLIENT_11122, 0x00031b97));
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00031e89), nak_c00031b97);
		
		// Hook the part where it sets the global language id.
		langAfterJmpAddr = (DWORD)(GetOffsetAddress(CLIENT_11122, 0x0003828c));
		DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0003820d), (uint8_t*)getSystemLanguageMethodJmp, 6);
		
		// Hooks for advanced lobby options.
		pfn_c0024eeef = (tfn_c0024eeef)DetourClassFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0024eeef), (uint8_t*)fn_c0024eeef, 9);
		pfn_c0024fa19 = (tfn_c0024fa19)DetourClassFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0024fa19), (uint8_t*)fn_c0024fa19, 9);
		pfn_c0024fabc = (tfn_c0024fabc)DetourClassFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0024fabc), (uint8_t*)fn_c0024fabc, 13);
		
		pHookIsSkullActive = (tHookIsSkullActive)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x000bd114), (uint8_t*)HookIsSkullActive, 5);
		
		pKillVolumeEnable = (tKillVolumeEnable)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x000b3a64), (uint8_t*)KillVolumeEnable, 7);
		pKillVolumeDisable = (tKillVolumeDisable)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x000b3ab8), (uint8_t*)KillVolumeDisable, 7);
		
		// Redirects the is_campaign call that the in-game chat renderer makes so we can show/hide it as we like.
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x0022667b), HideIngameChat);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00226628), HideIngameChat);
		
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00228579), HideFirstPerson);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00223955), HideHUD);
		
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00092c05), BansheeBombIsDisabled);
		
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00182d6d), GrenadeChainReactionsAreDisabled);
		
		if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x0017a449))) {
			PatchWithCall(patchAddress, CodeCaveNakExplosionPhysicsEnabledOnNonHost);
			WriteValue<uint8_t>(patchAddress + 5, 0x90);
		}
		
		// SERVER_11122 0x00001f35c
		pOnMapLoad = (tOnMapLoad)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00008f62), (uint8_t*)OnMapLoad, 11);
		
		pfn_001aa955 = (tfn_001aa955)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x001aa955), (uint8_t*)fn_001aa955, 8);
		
		//pfn_00096a71 = (tfn_00096a71)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00096a71), (uint8_t*)fn_nak_00096a71, 7);
		
		pLocalPlayerMouseInput = (tLocalPlayerMouseInput)GetOffsetAddress(CLIENT_11122, 0x00061ea2);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00062f65), LocalPlayerMouseInput);
		
		pSetLocalPlayerSensitivity = (tSetLocalPlayerSensitivity)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x002087bf), (uint8_t*)HookSetLocalPlayerSensitivity, 8);
		
		// Get rid of vehicle controls sensitivity code and replace it with something better.
		{
			patchAddress = GetOffsetAddress(CLIENT_11122, 0x00092089);
			NopFill(patchAddress, 0x00092209 - 0x00092089);
			// MOV EAX, [local_player_index]
			{
				uint8_t opcode[] = { 0x8B, 0x84, 0x24, 0x44, 0x1C, 0x00, 0x00 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			
			// MOVSS xmm0, (controller X axis sensitivity)
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x6C };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MULSS xmm0, array_address[EAX*4]
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x59, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00 };
				*(uint32_t*)&opcode[5] = (uint32_t)&player_controls_sensitivity_vehicle_ratios_controller;
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MOVSS (controller X axis sensitivity), xmm0
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x6C };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			
			// MOVSS xmm0, (controller Y axis sensitivity)
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x70 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MULSS xmm0, array_address[EAX*4]
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x59, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00 };
				*(uint32_t*)&opcode[5] = (uint32_t)&player_controls_sensitivity_vehicle_ratios_controller;
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MOVSS (controller Y axis sensitivity), xmm0
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x70 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			
			// MOVSS xmm0, (mouse X axis sensitivity)
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x64 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MULSS xmm0, array_address[EAX*4]
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x59, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00 };
				*(uint32_t*)&opcode[5] = (uint32_t)&player_controls_sensitivity_vehicle_ratios_mouse;
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MOVSS (mouse X axis sensitivity), xmm0
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x64 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			
			// MOVSS xmm0, (mouse Y axis sensitivity)
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x10, 0x44, 0x24, 0x68 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MULSS xmm0, array_address[EAX*4]
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x59, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00 };
				*(uint32_t*)&opcode[5] = (uint32_t)&player_controls_sensitivity_vehicle_ratios_mouse;
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			// MOVSS (mouse Y axis sensitivity), xmm0
			{
				uint8_t opcode[] = { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x68 };
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
			
			// JMP to end of NOPs.
			{
				uint8_t opcode[] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
				*(uint32_t*)&opcode[1] = (uint32_t)(GetOffsetAddress(CLIENT_11122, 0x00092209) - patchAddress - sizeof(opcode));
				WriteBytes(patchAddress, opcode, sizeof(opcode));
				patchAddress += sizeof(opcode);
			}
		}
		
		//pInitializeGameEngineSystems = (tInitializeGameEngineSystems)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x0004a46e), (uint8_t*)InitializeGameEngineSystems, 6);
		// Hooking 0x0004a46e has problem of call instruction in first few bytes.
		pInitializeGameEngineSystems = (tInitializeGameEngineSystems)GetOffsetAddress(CLIENT_11122, 0x0004a46e);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x000086be), InitializeGameEngineSystems);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00009802), InitializeGameEngineSystems);
		
		pSpawnPlayer = (tSpawnPlayer)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00055952), (uint8_t*)SpawnPlayer, 6);
		
		pSetPlayerUnitDatumAndModel = (tSetPlayerUnitDatumAndModel)DetourFunc((uint8_t*)GetOffsetAddress(CLIENT_11122, 0x002295be), (uint8_t*)SetPlayerUnitDatumAndModel, 6);
	}
	
	InitCustomLanguage();
	InitCustomMenu();
	InitRunLoop();
	InitCoop();
	
	void* setHandler = (void*)XllnGuideUiHandler;
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tGUIDE_UI_HANDLER, &setHandler, 0);
	
	// Initialise any set values.
	if (Title_Version == CLIENT_11122) {
		SetFOV(H2Config_field_of_view);
		SetFOVVehicle(H2Config_field_of_view_vehicle);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "Patching Motion Sensor fix."
		);
		WriteValue<uint8_t>(GetOffsetAddress(CLIENT_11122, 0x002849c4), (uint8_t)4);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "Patching Sun Flare fix."
		);
		// rasterizer_near_clip_distance <real>
		// Changed from game default of 0.06 to 0.0601.
		WriteValue<float>(GetOffsetAddress(CLIENT_11122, 0x00468150), 0.0601f);
	}
	RefreshToggleXDelay();
	RefreshToggleVehicleFlipEject();
	
	return true;
}

bool UninitTitlePatches()
{
	void* setHandler = 0;
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tGUIDE_UI_HANDLER, &setHandler, 0);
	
	UninitCoop();
	UninitRunLoop();
	UninitCustomMenu();
	UninitCustomLanguage();
	return true;
}
