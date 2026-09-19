#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "sensitivity-raw-mouse.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../../config/config.hpp"
#include "../../../../utils/utils.hpp"
#include "../../../title-patches.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));
	
	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditSensitivityRawMouse);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditSensitivityRawMouse, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadValueLabel() {
	if (H2Config_sensitivity_raw_mouse != 0) {
		char* lblFpsLimitNum = H2CustomLanguageGetLabel(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFF0003);
		if (!lblFpsLimitNum) {
			return;
		}
		int buildLimitLabelLen = strlen(lblFpsLimitNum) + 20;
		char* buildLimitLabel = (char*)malloc(sizeof(char) * buildLimitLabelLen);
		snprintf(buildLimitLabel, buildLimitLabelLen, lblFpsLimitNum, H2Config_sensitivity_raw_mouse);
		add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 3, buildLimitLabel, true);
		free(buildLimitLabel);
	}
	else {
		char* lblFpsLimitDisabled = H2CustomLanguageGetLabel(CMLabelMenuId_EditSensitivityRawMouse, 0xFFFF0013);
		add_cartographer_label(CMLabelMenuId_EditSensitivityRawMouse, 3, lblFpsLimitDisabled, true);
	}
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	const uint32_t upper_limit = 10000;
	const uint32_t lower_limit = 1;
	const uint32_t major_inc = 50;
	const uint32_t minor_inc = 1;
	uint32_t& settingValue = *&H2Config_sensitivity_raw_mouse;
	if (button_id == 0) {
		if (settingValue <= upper_limit - major_inc) {
			settingValue += major_inc;
		}
		else {
			settingValue = upper_limit;
		}
	}
	else if (button_id == 1) {
		if (settingValue <= upper_limit - minor_inc) {
			settingValue += minor_inc;
		}
		else {
			settingValue = upper_limit;
		}
	}
	else if (button_id == 3) {
		if (settingValue > lower_limit + minor_inc) {
			settingValue -= minor_inc;
		}
		else {
			settingValue = lower_limit;
		}
	}
	else if (button_id == 4) {
		if (settingValue > lower_limit + major_inc) {
			settingValue -= major_inc;
		}
		else {
			settingValue = lower_limit;
		}
	}
	else if (button_id == 2) {
		if (settingValue == 0) {
			settingValue = 200;
		}
		else {
			settingValue = 0;
		}
	}
	loadValueLabel();
	
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
	loadValueLabel();
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 5, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_EditSensitivityRawMouse()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditSensitivityRawMouse()
{
	CallWidget(widget_call_head);
}
