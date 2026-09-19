#include "../../../dllmain.hpp"
#include "../../xlln-module.hpp"
#include "change-difficulty.hpp"
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
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_ChangeDifficulty);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF0, 0xFFFFFFF1);
}

int __cdecl widget_call_head(int a1);

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	if (button_id >= 0 && button_id <= 3) {
		uint16_t &currentDifficulty = *(uint16_t*)0x300019F2;
		currentDifficulty = button_id;
	}
	
	return (int)CallWidget(widget_call_head, false);
}

static int __fastcall widget_preselected_button(DWORD a1, DWORD _EDX)
{
	uint16_t &currentDifficulty = *(uint16_t*)0x300019F2;
	
	return sub_20F790_CM(a1, currentDifficulty);//selected button id
}

static DWORD* menu_vftable_1 = 0;
static DWORD* menu_vftable_2 = 0;

static int __cdecl widget_call_head(int a1)
{
	//uint16_t &currentDifficulty = *(uint16_t*)GetOffsetAddress(CLIENT_11122, 0x004119a4);
	uint16_t &currentDifficulty = *(uint16_t*)0x300019F2;
	
	char* changeDifficultyDescriptionFormatter = H2CustomLanguageGetLabel(CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF2);
	char* difficultyDescription = H2CustomLanguageGetLabel(
		CMLabelMenuId_ChangeDifficulty
		, currentDifficulty >= 0 && currentDifficulty <= 3 ? currentDifficulty + 1 : 0xFFFFFFF3
	);
	
	char *changeDifficultyDescription = FormMallocString(changeDifficultyDescriptionFormatter, difficultyDescription);
	add_cartographer_label(CMLabelMenuId_ChangeDifficulty, 0xFFFFFFF1, changeDifficultyDescription, true);
	free(changeDifficultyDescription);
	
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 4, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_ChangeDifficulty()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, true, 0);
}

void CustomMenuCall_ChangeDifficulty()
{
	CallWidget(widget_call_head);
}
