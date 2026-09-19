#include "../../../../dllmain.hpp"
#include "../../../xlln-module.hpp"
#include "adv-controls.hpp"
#include "../../custom-menu-functions.hpp"
#include "../../../language/custom-labels.hpp"
#include "../../../language/custom-language.hpp"
#include "../../../config/config.hpp"
#include "sensitivity-native-controller.hpp"
#include "sensitivity-native-mouse.hpp"
#include "sensitivity-raw-mouse.hpp"
#include "sensitivity-xy-axis-ratio-controller.hpp"
#include "sensitivity-xy-axis-ratio-mouse.hpp"
#include "sensitivity-vehicle-ratio-controller.hpp"
#include "sensitivity-vehicle-ratio-mouse.hpp"
#include "../../error/error.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));

	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_AdvControlsSettings);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_AdvControlsSettings, 0xFFFFFFF0, 0xFFFFFFF1);
}

static void loadLabelToggle(int lblIndex, int lblTogglePrefix, bool isEnabled) {
	combineCartographerLabels(CMLabelMenuId_AdvControlsSettings, lblTogglePrefix + (isEnabled ? 1 : 0), 0xFFFF0000 + lblIndex, lblIndex);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	switch (button_id) {
		case 0: {
			CustomMenuCall_EditSensitivityRawMouse();
			break;
		}
		case 1: {
			CustomMenuCall_EditSensitivityNativeMouse();
			break;
		}
		case 2: {
			CustomMenuCall_EditSensitivityXYAxisRatioMouse();
			break;
		}
		case 3: {
			CustomMenuCall_EditSensitivityVehicleRatioMouse();
			break;
		}
		case 4: {
			CustomMenuCall_EditSensitivityNativeController();
			break;
		}
		case 5: {
			CustomMenuCall_EditSensitivityXYAxisRatioController();
			break;
		}
		case 6: {
			CustomMenuCall_EditSensitivityVehicleRatioController();
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
	//loadLabelToggle(7, 0xFFFFFFF2, !H2Config_hide_first_person);
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 7, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_AdvControlsSettings()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_AdvControlsSettings()
{
	CallWidget(widget_call_head);
}
