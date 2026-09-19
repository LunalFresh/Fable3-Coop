#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "hud-gui.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../../config/config.hpp"
#include "edit-fov.hpp"
#include "edit-fov-vehicle.hpp"
#include "edit-crosshair-offset.hpp"
#include "edit-crosshair-size.hpp"
#include "../../error/error.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));

	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditHudGui);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditHudGui, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadLabelToggle(int lblIndex, int lblTogglePrefix, bool isEnabled) {
	combineCartographerLabels(CMLabelMenuId_EditHudGui, lblTogglePrefix + (isEnabled ? 1 : 0), 0xFFFF0000 + lblIndex, lblIndex);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;

	if (button_id == 0) {
		CustomMenuCall_EditFOV();
	}
	else if (button_id == 1) {
		CustomMenuCall_EditFOVVehicle();
	}
	else if (button_id == 2) {
		CustomMenuCall_EditCrosshairOffset();
	}
	else if (button_id == 3) {
		//CustomMenuCall_EditCrosshairSize();
		CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 8, 9);
	}
	else if (button_id == 4) {
		loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_hide_ingame_chat = !H2Config_hide_ingame_chat));
	}
	else if (button_id == 5) {
		loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_hide_HUD = !H2Config_hide_HUD));
	}
	else if (button_id == 6) {
		loadLabelToggle(button_id + 1, 0xFFFFFFF2, !(H2Config_hide_first_person = !H2Config_hide_first_person));
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
	loadLabelToggle(5, 0xFFFFFFF2, !H2Config_hide_ingame_chat);
	loadLabelToggle(6, 0xFFFFFFF2, !H2Config_hide_HUD);
	loadLabelToggle(7, 0xFFFFFFF2, !H2Config_hide_first_person);
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 7, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_EditHudGui()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditHudGui()
{
	CallWidget(widget_call_head);
}
