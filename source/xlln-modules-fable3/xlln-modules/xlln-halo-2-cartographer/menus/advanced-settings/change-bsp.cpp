#include "../../../dllmain.hpp"
#include "../../xlln-module.hpp"
#include "change-bsp.hpp"
#include "../custom-menu-functions.hpp"
#include "../../language/custom-labels.hpp"
#include "../../language/custom-language.hpp"
#include "../../config/config.hpp"
#include "../../../utils/utils.hpp"
#include "../../title-patches.hpp"

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));
	
	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_ChangeBsp);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_ChangeBsp, 0xFFFFFFF0, 0xFFFFFFF1);
}

int __cdecl widget_call_head(int a1);

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	if (button_id >= 0) {
		typedef void(__cdecl* tSetStructureBspByIndex)(int16_t bsp_index);
		tSetStructureBspByIndex SetStructureBspByIndex = (tSetStructureBspByIndex)GetOffsetAddress(CLIENT_11122, 0x00039563);
		SetStructureBspByIndex(button_id);
	}
	
	return (int)CallWidget(widget_call_head, false);
}

static int __fastcall widget_preselected_button(DWORD a1, DWORD _EDX)
{
	uint16_t &currentBspIndex = *(uint16_t*)GetOffsetAddress(CLIENT_11122, 0x004119a4);
	int16_t &switchingToBspIndex = *(int16_t*)GetOffsetAddress(CLIENT_11122, 0x00482258);
	uint16_t bspIndex = switchingToBspIndex != -1 ? switchingToBspIndex : currentBspIndex;
	
	return sub_20F790_CM(a1, bspIndex);//selected button id
}

static DWORD* menu_vftable_1 = 0;
static DWORD* menu_vftable_2 = 0;

static int __cdecl widget_call_head(int a1)
{
	uint32_t &scenarioGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00479e74);
	uint32_t &bspIndexCount = *(uint32_t*)(scenarioGlobals + 0x210);
	
	for (uint32_t i = 0; i < bspIndexCount; i++) {
		char* changeBspButtonLabelFormatter = H2CustomLanguageGetLabel(CMLabelMenuId_ChangeBsp, 0xFFFFFFF3);
		char *changeBspButtonLabel = FormMallocString(changeBspButtonLabelFormatter, i);
		add_cartographer_label(CMLabelMenuId_ChangeBsp, i + 1, changeBspButtonLabel, true);
		free(changeBspButtonLabel);
	}
	
	uint16_t &currentBspIndex = *(uint16_t*)GetOffsetAddress(CLIENT_11122, 0x004119a4);
	int16_t &switchingToBspIndex = *(int16_t*)GetOffsetAddress(CLIENT_11122, 0x00482258);
	uint16_t bspIndex = switchingToBspIndex != -1 ? switchingToBspIndex : currentBspIndex;
	
	char* changeBspDescriptionFormatter = H2CustomLanguageGetLabel(CMLabelMenuId_ChangeBsp, 0xFFFFFFF2);
	char *changeBspDescription = FormMallocString(changeBspDescriptionFormatter, bspIndex);
	add_cartographer_label(CMLabelMenuId_ChangeBsp, 0xFFFFFFF1, changeBspDescription, true);
	free(changeBspDescription);
	
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, bspIndexCount, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_ChangeBsp()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_ChangeBsp()
{
	CallWidget(widget_call_head);
}
