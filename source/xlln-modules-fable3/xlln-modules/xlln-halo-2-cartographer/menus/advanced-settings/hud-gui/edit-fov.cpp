#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "edit-fov.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../../config/config.hpp"
#include "../../../title-patches.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));

	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditFOV);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditFOV, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadValueLabel() {
	if (H2Config_field_of_view != 70) {
		char* lblFpsLimitNum = H2CustomLanguageGetLabel(CMLabelMenuId_EditFOV, 0xFFFF0003);
		if (!lblFpsLimitNum) {
			return;
		}
		int buildLimitLabelLen = strlen(lblFpsLimitNum) + 20;
		char* buildLimitLabel = (char*)malloc(sizeof(char) * buildLimitLabelLen);
		snprintf(buildLimitLabel, buildLimitLabelLen, lblFpsLimitNum, H2Config_field_of_view);
		add_cartographer_label(CMLabelMenuId_EditFOV, 3, buildLimitLabel, true);
		free(buildLimitLabel);
	}
	else {
		char* lblFpsLimitDisabled = H2CustomLanguageGetLabel(CMLabelMenuId_EditFOV, 0xFFFF0013);
		add_cartographer_label(CMLabelMenuId_EditFOV, 3, lblFpsLimitDisabled, true);
	}
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;

	const int upper_limit = 110;
	if (button_id == 0) {
		if (H2Config_field_of_view <= upper_limit - 10) {
			H2Config_field_of_view += 10;
		}
		else {
			H2Config_field_of_view = upper_limit;
		}
	}
	else if (button_id == 1) {
		if (H2Config_field_of_view < upper_limit) {
			H2Config_field_of_view += 1;
		}
	}
	else if (button_id == 3) {
		if (H2Config_field_of_view > 1) {
			H2Config_field_of_view -= 1;
		}
	}
	else if (button_id == 4) {
		if (H2Config_field_of_view > 11) {
			H2Config_field_of_view -= 10;
		}
		else {
			H2Config_field_of_view = 1;
		}
	}
	else if (button_id == 2) {
		H2Config_field_of_view = 70;
	}
	loadValueLabel();

	SetFOV(H2Config_field_of_view);

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

void CMSetupVFTables_EditFOV()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditFOV()
{
	CallWidget(widget_call_head);
}
