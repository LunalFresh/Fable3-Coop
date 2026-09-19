#include "../../../dllmain.hpp"
#include "../../xlln-module.hpp"
#include "coop-mode.hpp"
#include "../custom-menu-functions.hpp"
#include "../../language/custom-labels.hpp"
#include "../../language/custom-language.hpp"
#include "../../config/config.hpp"
#include "../../../utils/utils.hpp"
#include "../../title-patches.hpp"
#include "../../coop/coop.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));
	
	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_CoopMode);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_CoopMode, 0xFFFFFFF0, 0xFFFFFFF1);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	if (button_id >= 0 && button_id <= 2) {
		ToggleCoopModeFix((uint8_t)button_id);
	}
	
	return CM_CloseMenu(thisptr);
}

static int __fastcall widget_preselected_button(DWORD a1, DWORD _EDX)
{
	return sub_20F790_CM(a1, coop_mode_fix_setting_state);//selected button id
}

static DWORD* menu_vftable_1 = 0;
static DWORD* menu_vftable_2 = 0;

static int __cdecl widget_call_head(int a1)
{
	char* descriptionFormatter = H2CustomLanguageGetLabel(CMLabelMenuId_CoopMode, 0xFFFFFFF2);
	char* modeStatus = H2CustomLanguageGetLabel(
		CMLabelMenuId_CoopMode
		, coop_mode_fix_active ? 0xFFFFFFF3 : 0xFFFFFFF4
	);
	
	char *description = FormMallocString(descriptionFormatter, modeStatus);
	add_cartographer_label(CMLabelMenuId_CoopMode, 0xFFFFFFF1, description, true);
	free(description);
	
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 3, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_CoopMode()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_CoopMode()
{
	CallWidget(widget_call_head);
}
