#include "../../../dllmain.hpp"
#include "../../xlln-module.hpp"
#include "guide.hpp"
#include "../custom-menu-functions.hpp"
#include "../../language/custom-labels.hpp"
#include "../../language/custom-language.hpp"
#include "../../../utils/utils.hpp"
#include "../../config/config.hpp"
#include "../credits/credits.hpp"
#include "../advanced-settings/advanced-settings.hpp"
#include "../error/error.hpp"
#include "../accounts/Account-management.hpp"
#include "../accounts/Account-helper.hpp"
#include "../../xlive.hpp"
#include <Shellapi.h>

static void __fastcall c_list_widget_label_buttons(DWORD _ECX, DWORD _EDX, int a1, int a2)
{
	int(__thiscall *sub_211909)(int, int, int, int) = (int(__thiscall*)(int, int, int, int))(GetOffsetAddress(CLIENT_11122, 0x00211909));

	__int16 button_id = *(WORD*)(a1 + 112);
	int v3 = sub_211909(a1, 6, 0, 0);
	if (v3) {
		sub_21bf85_CMLTD(v3, button_id + 1, CMLabelMenuId_Guide);
	}
}

static int __fastcall widget_title_description(int a1, DWORD _EDX, char a2)
{
	return sub_2111ab_CMLTD(a1, a2, CMLabelMenuId_Guide, 0xFFFFFFF0, 0xFFFFFFF1);
}

static int __fastcall widget_button_handler(void *thisptr, DWORD _EDX, int a2, DWORD *a3)
{
	unsigned __int16 button_id = *a3 & 0xFFFF;
	
	switch (button_id) {
		case 2:
		case 0: {
			if (DetourXShowGuideUI) {
				DetourXShowGuideUI(0);
			}
			else {
				CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 0xA, 0xB);
			}
			break;
		}
		case 1: {
			CustomMenuCall_AdvSettings();
			break;
		}
		//case 2: {
		//	CustomMenuCall_AccountManagement(0);
		//	break;
		//}
		case 3: {
			ShellExecuteA(NULL, "open", "https://cartographer.glitchyscripts.com/", NULL, NULL, SW_SHOWDEFAULT);
			break;
		}
		case 4: {
			CustomMenuCall_Credits();
			break;
		}
		case 5: {
			//TODO CustomMenuCall_Update();
			CustomMenuCall_Error_Inner(CMLabelMenuId_Error, 0x8, 0x9);
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
	char* guide_desc_base = H2CustomLanguageGetLabel(CMLabelMenuId_Guide, 0xFFFFFFF2);
	DWORD guide_desc_buflen = strlen(guide_desc_base) + 50;
	char* guide_description = (char*)malloc(guide_desc_buflen);
	char hotkeyname[20];
	XINPUT_KEYSTROKE keystrokeGuide;
	XLiveGetGuideKey(&keystrokeGuide);
	GetVKeyCodeString(keystrokeGuide.VirtualKey, hotkeyname, 20);
	snprintf(guide_description, guide_desc_buflen, guide_desc_base, hotkeyname);
	add_cartographer_label(CMLabelMenuId_Guide, 0xFFFFFFF1, guide_description, true);
	free(guide_description);
	return CustomMenu_CallHead(a1, menu_vftable_1, menu_vftable_2, (DWORD)widget_button_handler, 5, 272);
}

static int(__cdecl *widget_function_ptr_helper())(int)
{
	return widget_call_head;
}

void CMSetupVFTables_Guide()
{
	CMSetupVFTables(&menu_vftable_1, &menu_vftable_2, (DWORD)c_list_widget_label_buttons, (DWORD)widget_title_description, (DWORD)widget_function_ptr_helper, (DWORD)widget_preselected_button, false, 0);
}

bool CustomMenuCall_Guide()
{
	if (menu_vftable_1 && menu_vftable_2) {
		if (!isAccountingActiveHandle()) {
			void* resultWidget = CallWidget(widget_call_head);
			return !!resultWidget;
		}
	}
	
	return false;
}
