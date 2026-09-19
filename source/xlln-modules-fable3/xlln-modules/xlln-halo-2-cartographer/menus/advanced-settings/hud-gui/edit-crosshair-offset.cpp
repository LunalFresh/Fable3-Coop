#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "edit-crosshair-offset.hpp"
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
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_EditCrosshairOffset);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_EditCrosshairOffset, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadValueLabel() {
	if (!FloatIsNaN(H2Config_crosshair_offset)) {
		char* lblFpsLimitNum = H2CustomLanguageGetLabel(CMLabelMenuId_EditCrosshairOffset, 0xFFFF0003);
		if (!lblFpsLimitNum) {
			return;
		}
		int buildLimitLabelLen = strlen(lblFpsLimitNum) + 20;
		char* buildLimitLabel = (char*)malloc(sizeof(char) * buildLimitLabelLen);
		snprintf(buildLimitLabel, buildLimitLabelLen, lblFpsLimitNum, H2Config_crosshair_offset);
		add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 3, buildLimitLabel, true);
		free(buildLimitLabel);
	}
	else {
		char* lblFpsLimitDisabled = H2CustomLanguageGetLabel(CMLabelMenuId_EditCrosshairOffset, 0xFFFF0013);
		add_cartographer_label(CMLabelMenuId_EditCrosshairOffset, 3, lblFpsLimitDisabled, true);
	}
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;

	const float upper_limit = 0.53f;
	const float major_inc = 0.02f;
	const float minor_inc = 0.001f;
	if (button_id == 0) {
		if (H2Config_crosshair_offset <= upper_limit - major_inc) {
			H2Config_crosshair_offset += major_inc;
		}
		else {
			H2Config_crosshair_offset = upper_limit;
		}
	}
	else if (button_id == 1) {
		if (H2Config_crosshair_offset <= upper_limit - minor_inc) {
			H2Config_crosshair_offset += minor_inc;
		}
		else {
			H2Config_crosshair_offset = upper_limit;
		}
	}
	else if (button_id == 3) {
		if (H2Config_crosshair_offset > 0.0f + minor_inc) {
			H2Config_crosshair_offset -= minor_inc;
		}
		else {
			H2Config_crosshair_offset = 0.0f;
		}
	}
	else if (button_id == 4) {
		if (H2Config_crosshair_offset > 0.0f + major_inc) {
			H2Config_crosshair_offset -= major_inc;
		}
		else {
			H2Config_crosshair_offset = 0.0f;
		}
	}
	else if (button_id == 2) {
		if (FloatIsNaN(H2Config_crosshair_offset)) {
			H2Config_crosshair_offset = 0.165f;
		}
		else {
			H2Config_crosshair_offset = NAN;
		}
	}
	loadValueLabel();

	SetCrosshairVerticalOffset(H2Config_crosshair_offset);

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

void CMSetupVFTables_EditCrosshairOffset()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_EditCrosshairOffset()
{
	CallWidget(widget_call_head);
}
