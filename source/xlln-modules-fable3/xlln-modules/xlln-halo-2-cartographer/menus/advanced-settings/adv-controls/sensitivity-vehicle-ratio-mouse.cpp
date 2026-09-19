#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "sensitivity-native-mouse.hpp"
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
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditSensitivityVehicleRatioMouse);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditSensitivityVehicleRatioMouse, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadValueLabel() {
	char* lblFpsLimitNum = H2CustomLanguageGetLabel(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 0xFFFF0003);
	if (!lblFpsLimitNum) {
		return;
	}
	int buildLimitLabelLen = strlen(lblFpsLimitNum) + 20;
	char* buildLimitLabel = (char*)malloc(sizeof(char) * buildLimitLabelLen);
	snprintf(buildLimitLabel, buildLimitLabelLen, lblFpsLimitNum, H2Config_sensitivity_vehicle_ratio_mouse);
	add_cartographer_label(CMLabelMenuId_EditSensitivityVehicleRatioMouse, 3, buildLimitLabel, true);
	free(buildLimitLabel);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	const float upper_limit = 1000.0f;
	const float lower_limit = 0.000001f;
	const float major_inc = 1.0f;
	const float minor_inc = 0.01f;
	float& settingValue = *&H2Config_sensitivity_vehicle_ratio_mouse;
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
		settingValue = 1.0f;
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

void CMSetupVFTables_EditSensitivityVehicleRatioMouse()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditSensitivityVehicleRatioMouse()
{
	CallWidget(widget_call_head);
}
