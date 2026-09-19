#include "../../dllmain.hpp"
#include "../xlln-module.hpp"
#include "custom-menu.hpp"
#include "custom-menu-functions.hpp"
#include "../language/custom-labels.hpp"
#include "../xlive.hpp"
#include "error/error.hpp"
#include "virtual-keyboard/virtual-keyboard.hpp"
#include "guide/guide.hpp"
#include "credits/credits.hpp"
#include "advanced-settings/advanced-settings.hpp"
#include "advanced-settings/adv-other/adv-other.hpp"
#include "advanced-settings/adv-other/fps-limit.hpp"
#include "advanced-settings/adv-controls/adv-controls.hpp"
#include "advanced-settings/adv-lobby/adv-lobby.hpp"
#include "advanced-settings/language/language-main.hpp"
#include "advanced-settings/language/language-sub.hpp"
#include "accounts/account-management.hpp"
#include "accounts/account-list.hpp"
#include "accounts/account-create.hpp"
#include "accounts/account-add.hpp"
#include "advanced-settings/hud-gui/hud-gui.hpp"
#include "advanced-settings/hud-gui/edit-fov.hpp"
#include "advanced-settings/hud-gui/edit-fov-vehicle.hpp"
#include "advanced-settings/hud-gui/edit-crosshair-offset.hpp"
#include "advanced-settings/hud-gui/edit-crosshair-size.hpp"
#include "advanced-settings/toggle-skulls.hpp"
#include "advanced-settings/adv-controls/sensitivity-native-controller.hpp"
#include "advanced-settings/adv-controls/sensitivity-native-mouse.hpp"
#include "advanced-settings/adv-controls/sensitivity-raw-mouse.hpp"
#include "advanced-settings/adv-controls/sensitivity-xy-axis-ratio-controller.hpp"
#include "advanced-settings/adv-controls/sensitivity-xy-axis-ratio-mouse.hpp"
#include "advanced-settings/adv-controls/sensitivity-vehicle-ratio-controller.hpp"
#include "advanced-settings/adv-controls/sensitivity-vehicle-ratio-mouse.hpp"
#include "advanced-settings/change-bsp.hpp"
#include "advanced-settings/change-difficulty.hpp"
#include "advanced-settings/coop-mode.hpp"

bool CMGuideMenu()
{
	if (Title_Version == CLIENT_11122) {
		HWND hWndFocus = GetFocus();
		HWND hWndForeground = GetForegroundWindow();
		
		if (H2hWnd && (H2hWnd == hWndFocus || H2hWnd == hWndForeground)) {
			//Override XLLN GUIDE Menu if true.
			return CustomMenuCall_Guide();
		}
	}
	return false;
}

bool InitCustomMenu()
{
	if (Title_Version == CLIENT_11122) {
		InitCMFunctions();
		CMSetupVFTables_Error_Inner();
		CMSetupVFTables_Error_Outer();
		CMSetupVFTablesVKb_VKeyTest();
		CMSetupVFTables_Guide();
		CMSetupVFTables_Credits();
		CMSetupVFTables_AdvSettings();
		CMSetupVFTables_AdvOtherSettings();
		CMSetupVFTables_EditFpsLimit();
		CMSetupVFTables_AdvControlsSettings();
		CMSetupVFTables_LanguageMain();
		CMSetupVFTables_LanguageSub();
		CMSetupVFTables_AccountManagement();
		CMSetupVFTables_AccountList();
		CMSetupVFTables_AccountCreate();
		CMSetupVFTables_AccountAdd();
		CMSetupVFTables_EditHudGui();
		CMSetupVFTables_EditFOV();
		CMSetupVFTables_EditFOVVehicle();
		CMSetupVFTables_EditCrosshairOffset();
		CMSetupVFTables_EditCrosshairSize();
		CMSetupVFTables_ToggleSkulls();
		CMSetupVFTables_EditSensitivityNativeController();
		CMSetupVFTables_EditSensitivityNativeMouse();
		CMSetupVFTables_EditSensitivityRawMouse();
		CMSetupVFTables_EditSensitivityXYAxisRatioController();
		CMSetupVFTables_EditSensitivityXYAxisRatioMouse();
		CMSetupVFTables_EditSensitivityVehicleRatioController();
		CMSetupVFTables_EditSensitivityVehicleRatioMouse();
		CMSetupVFTables_ChangeBsp();
		CMSetupVFTables_ChangeDifficulty();
		CMSetupVFTables_CoopMode();
		CMSetupVFTables_AdvLobbySettings();
	}

	return true;
}

bool UninitCustomMenu()
{
	return true;
}
