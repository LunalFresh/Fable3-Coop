#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "adv-lobby.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../virtual-keyboard/virtual-keyboard.hpp"
#include "../../../config/config.hpp"
#include "../../error/error.hpp"
#include "../../../title-patches.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));

	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_AdvLobbySettings);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_AdvLobbySettings, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadLabelToggle(int lblIndex, int lblTogglePrefix, bool isEnabled) {
	combineCartographerLabels(CMLabelMenuId_AdvLobbySettings, lblTogglePrefix + (isEnabled ? 1 : 0), 0xFFFF0000 + lblIndex, lblIndex);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	switch (button_id) {
		case 0: {
			wchar_t* bufferLobbyName = (wchar_t*)H2CustomLanguageGetLabel(CMLabelMenuId_AdvLobbySettings, 0xFFFFF001);
			CustomMenuCall_VKeyboard_Inner(bufferLobbyName, 32, 0b11, CMLabelMenuId_AdvLobbySettings, 0xFFFFFF02, CMLabelMenuId_AdvLobbySettings, 0xFFFFFF03);
			break;
		}
		case 1: {
			loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_xDelay_enabled = !H2Config_xDelay_enabled));
			RefreshToggleXDelay();
			break;
		}
		case 2: {
			loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_vehicle_flip_eject_enabled = !H2Config_vehicle_flip_eject_enabled));
			RefreshToggleVehicleFlipEject();
			break;
		}
		case 3: {
			loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_kill_volumes_enabled = !H2Config_kill_volumes_enabled));
			RefreshToggleKillVolumes();
			break;
		}
		default: {
			CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 1, 2);
			break;
		}
	}
	
	return CM_PressButtonAnimation(thisptr);
}

static int __fastcall widget_preselected_button(DWORD a1, DWORD _EDX)
{
	return sub_20F790_CM(a1, 0);//selected button id
}

static DWORD* menu_vftable_1 = 0;
static DWORD* menu_vftable_2 = 0;

static int __cdecl widget_call_head(int a1)
{
	loadLabelToggle(2, 0xFFFFFFF2, !H2Config_xDelay_enabled);
	loadLabelToggle(3, 0xFFFFFFF2, !H2Config_vehicle_flip_eject_enabled);
	loadLabelToggle(4, 0xFFFFFFF2, !H2Config_kill_volumes_enabled);
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 4, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

static void* __fastcall widget_deconstructor(LPVOID lpMem, DWORD _EDX, char a2)
{
	wchar_t* bufferLobbyName = (wchar_t*)H2CustomLanguageGetLabel(CMLabelMenuId_AdvLobbySettings, 0xFFFFF001);
	if (wcslen(bufferLobbyName) == 0) {
		wchar_t* ClientName = (wchar_t*)GetOffsetAddress(CLIENT_11122, 0x0051a638);
		memset(bufferLobbyName, 0, 32 * sizeof(wchar_t));
		memcpy(bufferLobbyName, ClientName, 15 * sizeof(wchar_t));
	}
	//wchar_t* ServerName = (wchar_t*)GetOffsetAddress(CLIENT_11122, 0x0096da94);
	//memcpy(ServerName, bufferLobbyName, 32 * sizeof(wchar_t));
	//ServerName[31] = 0;
	wchar_t* ServerNameActive = (wchar_t*)GetOffsetAddress(CLIENT_11122, 0x0051a5b2);
	memcpy(ServerNameActive, bufferLobbyName, 32 * sizeof(wchar_t));
	ServerNameActive[31] = 0;
	
	return sub_248beb_deconstructor(lpMem, _EDX, a2);
}

void CMSetupVFTables_AdvLobbySettings()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, (DWORD)widget_deconstructor);
}

void CustomMenuCall_AdvLobbySettings()
{
	wchar_t* bufferLobbyName = (wchar_t*)H2CustomLanguageGetLabel(CMLabelMenuId_AdvLobbySettings, 0xFFFFF001);
	if (wcslen(bufferLobbyName) == 0) {
		wchar_t* ClientName = (wchar_t*)GetOffsetAddress(CLIENT_11122, 0x0051a638);
		memset(bufferLobbyName, 0, 32 * sizeof(wchar_t));
		memcpy(bufferLobbyName, ClientName, 15 * sizeof(wchar_t));
	}
	
	CallWidget(widget_call_head);
}
