#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "fps-limit.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../../../utils/utils.hpp"
#include "../../../../xlivelessness.hpp"
#include "../../error/error.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));
	
	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditFpsLimit);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditFpsLimit, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadValueLabel(uint32_t setting_value) {
	if (setting_value != 0) {
		char* lblFpsLimitNum = H2CustomLanguageGetLabel(CMLabelMenuId_EditFpsLimit, 0xFFFF0003);
		if (!lblFpsLimitNum) {
			return;
		}
		int buildLimitLabelLen = strlen(lblFpsLimitNum) + 20;
		char* buildLimitLabel = (char*)malloc(sizeof(char) * buildLimitLabelLen);
		snprintf(buildLimitLabel, buildLimitLabelLen, lblFpsLimitNum, setting_value);
		add_cartographer_label(CMLabelMenuId_EditFpsLimit, 3, buildLimitLabel, true);
		free(buildLimitLabel);
	}
	else {
		char* lblFpsLimitDisabled = H2CustomLanguageGetLabel(CMLabelMenuId_EditFpsLimit, 0xFFFF0013);
		add_cartographer_label(CMLabelMenuId_EditFpsLimit, 3, lblFpsLimitDisabled, true);
	}
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	const uint32_t upper_limit = 10000;
	const uint32_t lower_limit = 1;
	const uint32_t major_inc = 10;
	const uint32_t minor_inc = 1;
	uint32_t settingValue = 0;
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tFPS_LIMIT, 0, (void**)&settingValue);
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
			settingValue = 60;
		}
		else {
			settingValue = 0;
		}
	}
	
	resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tFPS_LIMIT, (void**)&settingValue, 0);
	if (resultXllnModifyProperty != ERROR_SUCCESS) {
		CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 0xC, 0xD);
	}
	
	loadValueLabel(settingValue);
	
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
	uint32_t settingValue = 0;
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tFPS_LIMIT, 0, (void**)&settingValue);
	loadValueLabel(settingValue);
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 5, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_EditFpsLimit()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditFpsLimit()
{
	uint32_t settingValue = 0;
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::tFPS_LIMIT, 0, (void**)&settingValue);
	if (resultXllnModifyProperty != ERROR_SUCCESS) {
		CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 0xC, 0xD);
		return;
	}
	CallWidget(widget_call_head);
}
