#include <winsock2.h>
#include "social.hpp"
#include <Windows.h>
#include <wincodec.h>
#include "./wnd-main.hpp"
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "./xlln-config.hpp"
#include "./wnd-achievements.hpp"
#include "./wnd-stats.hpp"
#include "./wnd-debug-log.hpp"
#include "./wnd-sockets.hpp"
#include "./wnd-connections.hpp"
#include "./wnd-message-box.hpp"
#include "./wnd-user-custom-list.hpp"
#include "./wnd-user-card.hpp"
#include "./wnd-image.hpp"
#include "./xlln-keep-alive.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/live-over-lan.hpp"
#include "../xlive/xnetqos.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xnotify.hpp"
#include "../xlive/xhv-engine.hpp"
#include <time.h>
#include <CommCtrl.h>
#include <dbt.h>

HWND xlln_window_hwnd = NULL;
static HMENU xlln_window_hMenu = NULL;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

static IWICFormatConverter* wic_converted_source_bitmap_xlln_logo = 0;

void XLLNWindowUpdateUserInputBoxes(uint32_t user_index)
{
	for (uint32_t iPlayer = 0; iPlayer < XLIVE_LOCAL_USER_COUNT; iPlayer++) {
		bool checked = (iPlayer == xlln_login_player);
		CheckMenuItem(xlln_window_hMenu, XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_P1 + iPlayer, checked ? MF_CHECKED : MF_UNCHECKED);
	}
	
	if (user_index != xlln_login_player) {
		return;
	}
	
	SetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_USERNAME, xlive_local_users[user_index].username);
	
	CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE, xlive_local_users[user_index].live_enabled ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE, xlive_local_users[user_index].online_enabled ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, xlive_local_users[user_index].auto_login ? BST_CHECKED : BST_UNCHECKED);
	
	bool loggedIn = xlive_local_users[user_index].signin_state != eXUserSigninState_NotSignedIn;
	ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGIN), loggedIn ? SW_HIDE : SW_SHOWNORMAL);
	ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGOUT), loggedIn ? SW_SHOWNORMAL : SW_HIDE);
	
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_MICROPHONE_DEVICE);
		LRESULT resultFind = SendMessageW(controlAudioDeviceComboBox, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)xlive_local_users[user_index].audio_input_device_name);
		if (resultFind == CB_ERR) {
			resultFind = 0;
		}
		SendMessageW(controlAudioDeviceComboBox, CB_SETCURSEL, resultFind, 0);
	}
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_SPEAKER_DEVICE);
		LRESULT resultFind = SendMessageW(controlAudioDeviceComboBox, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)xlive_local_users[user_index].audio_output_device_name);
		if (resultFind == CB_ERR) {
			resultFind = 0;
		}
		SendMessageW(controlAudioDeviceComboBox, CB_SETCURSEL, resultFind, 0);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_VOLUME);
		SendMessageW(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)xlive_local_users[user_index].audio_input_device_volume);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_THRESHOLD);
		SendMessageW(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)xlive_local_users[user_index].audio_input_device_threshold);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_SPEAKER_VOLUME);
		SendMessageW(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)xlive_local_users[user_index].audio_output_device_volume);
	}
	
	InvalidateRect(xlln_window_hwnd, NULL, FALSE);
}

static bool RefreshAvailableAudioDevices()
{
	if (WAVE_MAPPER != (uint32_t)-1) {
		__debugbreak();
	}
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_MICROPHONE_DEVICE);
		while (SendMessageA(controlAudioDeviceComboBox, CB_DELETESTRING, 0, 0) > 0);
		
		SendMessageA(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)("None"));
		SendMessageA(controlAudioDeviceComboBox, CB_SETCURSEL, 0, 0);
		
		WAVEINCAPSW waveInCaps;
		uint32_t deviceCount = waveInGetNumDevs();
		for (uint32_t iDevice = (uint32_t)-1; iDevice == (uint32_t)-1 || iDevice < deviceCount; iDevice++) {
			MMRESULT resultDeviceCaps = waveInGetDevCapsW(iDevice, &waveInCaps, sizeof(waveInCaps));
			if (resultDeviceCaps == MMSYSERR_NOERROR) {
				SendMessageW(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)(waveInCaps.szPname));
			}
		}
	}
	
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_SPEAKER_DEVICE);
		while (SendMessageA(controlAudioDeviceComboBox, CB_DELETESTRING, 0, 0) > 0);
		
		SendMessageA(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)("None"));
		SendMessageA(controlAudioDeviceComboBox, CB_SETCURSEL, 0, 0);
		
		WAVEOUTCAPSW waveOutCaps;
		uint32_t deviceCount = waveOutGetNumDevs();
		for (uint32_t iDevice = (uint32_t)-1; iDevice == (uint32_t)-1 || iDevice < deviceCount; iDevice++) {
			MMRESULT resultDeviceCaps = waveOutGetDevCapsW(iDevice, &waveOutCaps, sizeof(waveOutCaps));
			if (resultDeviceCaps == MMSYSERR_NOERROR) {
				SendMessageW(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)(waveOutCaps.szPname));
			}
		}
	}
	
	XLLNWindowUpdateUserInputBoxes(xlln_login_player);
	
	return true;
}

static LRESULT CALLBACK DLLWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HDEVNOTIFY hDeviceNotify = 0;
	
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if (hdc) {
				SetTextColor(hdc, RGB(0, 0, 0));
				SetBkColor(hdc, 0x00C8C8C8);
				
				//HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
				//SelectObject(hdc, hBrush);
				//RECT bgRect;
				//GetClientRect(hwnd, &bgRect);
				//HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
				//FillRgn(hdc, bgRgn, hBrush);
				//DeleteObject(bgRgn);
				//DeleteObject(hBrush);
				
				{
					char* textLabel = FormMallocString("Player %u username:", xlln_login_player + 1);
					TextOutA(hdc, 160, 10, textLabel, (uint32_t)strlen(textLabel));
					free(textLabel);
				}
				{
					char textLabel[] = "Voice Microphone:";
					TextOutA(hdc, 160, 62, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Volume (0-1000%):";
					TextOutA(hdc, 165, 114, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Threshold (Low-Hi):";
					TextOutA(hdc, 165, 144, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Voice Speaker:";
					TextOutA(hdc, 160, 172, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Volume (0-100%):";
					TextOutA(hdc, 165, 224, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Direct IP Password:";
					TextOutA(hdc, 5, 308, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "Direct IP Host IP:Port:";
					TextOutA(hdc, 5, 334, textLabel, (uint32_t)strlen(textLabel));
				}
				{
					char textLabel[] = "XLLN-Hub / Direct address(es):";
					TextOutA(hdc, 5, 370, textLabel, (uint32_t)strlen(textLabel));
				}
				
				EndPaint(hwnd, &ps);
				
				return 0;
			}
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_RESTORE || wParam == SC_MINIMIZE) {
				XllnUpdateXliveSystemUIOpenState();
			}
			else if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::MAIN_HIDE);
				return 0;
			}
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_CREATE: {
            SetTimer(hwnd, 711, 1000, NULL);
            CreateWindowW(L"BUTTON", L"Friends / invites", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 155, 130, 25, hwnd, (HMENU)30711, xlln_hModule, NULL);
			
			XllnWndImageCreate(hwnd, 10, 10, 140, 140, wic_converted_source_bitmap_xlln_logo);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				160, 28, 300, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBX_USERNAME, xlln_hModule, NULL);
			
			CreateWindowA(WC_COMBOBOXA, 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL,
				160, 80, 300, 300, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_CMB_MICROPHONE_DEVICE, xlln_hModule, NULL);
			
			{
				HWND controlAudioDeviceTrackBar = CreateWindowA(TRACKBAR_CLASSA, 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					290, 110, 180, 30, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_VOLUME, xlln_hModule, NULL);
				
				SendMessage(controlAudioDeviceTrackBar, TBM_SETRANGE, (WPARAM)TRUE /* redraw flag */, (LPARAM) MAKELONG(0, XHV_AUDIO_SETTING_VOLUME_MAX_BOOST));
				SendMessage(controlAudioDeviceTrackBar, TBM_SETPAGESIZE, 0, (LPARAM)(XHV_AUDIO_SETTING_VOLUME_MAX/10));
				SendMessage(controlAudioDeviceTrackBar, TBM_SETTIC, (WPARAM)0, (LPARAM)XHV_AUDIO_SETTING_VOLUME_MAX);
			}
			
			{
				HWND controlAudioDeviceTrackBar = CreateWindowA(TRACKBAR_CLASSA, 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					290, 140, 180, 30, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_THRESHOLD, xlln_hModule, NULL);
				
				SendMessage(controlAudioDeviceTrackBar, TBM_SETRANGE, (WPARAM)TRUE /* redraw flag */, (LPARAM) MAKELONG(0, XHV_AUDIO_SETTING_VOLUME_MAX));
				SendMessage(controlAudioDeviceTrackBar, TBM_SETPAGESIZE, 0, (LPARAM)(XHV_AUDIO_SETTING_VOLUME_MAX/10));
				SendMessage(controlAudioDeviceTrackBar, TBM_SETTIC, (WPARAM)0, (LPARAM)XHV_AUDIO_SETTING_INPUT_THRESHOLD_DEFAULT);
			}
			
			CreateWindowA(WC_COMBOBOXA, 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL,
				160, 190, 300, 300, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_CMB_SPEAKER_DEVICE, xlln_hModule, NULL);
			
			{
				HWND controlAudioDeviceTrackBar = CreateWindowA(TRACKBAR_CLASSA, 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					290, 220, 180, 30, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBR_SPEAKER_VOLUME, xlln_hModule, NULL);
				
				SendMessage(controlAudioDeviceTrackBar, TBM_SETRANGE, (WPARAM)TRUE /* redraw flag */, (LPARAM) MAKELONG(0, XHV_AUDIO_SETTING_VOLUME_MAX));
				SendMessage(controlAudioDeviceTrackBar, TBM_SETPAGESIZE, 0, (LPARAM)(XHV_AUDIO_SETTING_VOLUME_MAX/10));
			}
			
			CreateWindowA(WC_BUTTONA, "Live Enabled", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 180, 130, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Signin 'Online'", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				15, 210, 130, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Auto Login", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 240, 130, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Login", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				280, 260, 180, 25, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_BTN_LOGIN, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Logout", WS_CHILD | WS_TABSTOP,
				280, 260, 180, 25, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_BTN_LOGOUT, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, xlln_direct_ip_connect_password, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				150, 306, 250, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, xlln_direct_ip_connect_ip_port, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				150, 332, 250, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_IP_PORT, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Connect", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				404, 331, 75, 25, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_BTN_DIRECT_IP_CONNECT, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, broadcastAddrInput, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				5, 390, 475, 22, hwnd, (HMENU)XLLNControlsMessageNumbers::MAIN_TBX_BROADCAST, xlln_hModule, NULL);
			
			{
				DEV_BROADCAST_DEVICEINTERFACE_W NotificationFilter;
				
				ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
				NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);
				NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
				
				hDeviceNotify = RegisterDeviceNotificationW(
					hwnd
					, &NotificationFilter
					, DEVICE_NOTIFY_WINDOW_HANDLE
				);
				
				if (!hDeviceNotify) {
					uint32_t errorRegisterDeviceNotificationW = GetLastError();
					XLLN_DEBUG_LOG_ECODE(errorRegisterDeviceNotificationW, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
						, "%s Failed to RegisterDeviceNotificationW(...)."
						, __func__
					);
				}
			}
			
			SetEvent(xlln_window_create_event);
			
			break;
		}
		case WM_CLOSE: {
			if (hDeviceNotify && !UnregisterDeviceNotification(hDeviceNotify)) {
				uint32_t errorUnregisterDeviceNotification = GetLastError();
				XLLN_DEBUG_LOG_ECODE(errorUnregisterDeviceNotification, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s Failed to UnregisterDeviceNotification(...)."
					, __func__
				);
			}
			DestroyWindow(hwnd);
			break;
		}
		case WM_APP + 711: { SocialOpenOnUiThread((uint32_t)wParam, lParam == 0); return 0; }
        case WM_TIMER: { if (wParam == 711) SocialTick(); return 0; }
        case WM_DESTROY: {
            KillTimer(hwnd, 711);
			PostQuitMessage(0);
			return 0;
		}
		case WM_DEVICECHANGE: {
			if (wParam == DBT_DEVNODES_CHANGED) {
				RefreshAvailableAudioDevices();
			}
			break;
		}
		case WM_HSCROLL: {
			if ((wParam & 0xFFFF) == TB_ENDTRACK) {
				{
					HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_VOLUME);
					if ((HWND)lParam == controlAudioDeviceTrackBar) {
						LRESULT resultItem = SendMessageA(controlAudioDeviceTrackBar, TBM_GETPOS, 0, 0);
						if (resultItem >= 0 && resultItem <= XHV_AUDIO_SETTING_VOLUME_MAX_BOOST) {
							xlive_local_users[xlln_login_player].audio_input_device_volume = (uint16_t)resultItem;
						}
						return 0;
					}
				}
				{
					HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_THRESHOLD);
					if ((HWND)lParam == controlAudioDeviceTrackBar) {
						LRESULT resultItem = SendMessageA(controlAudioDeviceTrackBar, TBM_GETPOS, 0, 0);
						if (resultItem >= 0 && resultItem <= XHV_AUDIO_SETTING_VOLUME_MAX) {
							xlive_local_users[xlln_login_player].audio_input_device_threshold = (uint8_t)resultItem;
						}
						return 0;
					}
				}
				{
					HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_SPEAKER_VOLUME);
					if ((HWND)lParam == controlAudioDeviceTrackBar) {
						LRESULT resultItem = SendMessageA(controlAudioDeviceTrackBar, TBM_GETPOS, 0, 0);
						if (resultItem >= 0 && resultItem <= XHV_AUDIO_SETTING_VOLUME_MAX) {
							xlive_local_users[xlln_login_player].audio_output_device_volume = (uint8_t)resultItem;
						}
						return 0;
					}
				}
			}
			break;
		}
		case WM_COMMAND: {
            if (LOWORD(wParam) == 30711) { SocialOpenOnUiThread(0); return 0; }
			bool notHandled = false;
			
			switch (wParam) {
				case XLLNControlsMessageNumbers::MAIN_MENU_EXIT: {
					// Kill any threads and kill the program.
					XllnThreadHotkeysStop();
					XllnThreadLiveOverLanStop();
					XllnThreadKeepAliveStop();
					XLiveThreadQosStop();
					exit(EXIT_SUCCESS);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_VIEW_USER_CARD: {
					XllnWndUserCardShow(true, xlln_login_player, 0);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_SAVE_CONFIG: {
					XllnConfig(true);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_ALWAYS_TOP: {
					BOOL checked = GetMenuState(xlln_window_hMenu, XLLNControlsMessageNumbers::MAIN_MENU_ALWAYS_TOP, 0) != MF_CHECKED;
					if (checked) {
						SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					}
					else {
						SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					}
					CheckMenuItem(xlln_window_hMenu, XLLNControlsMessageNumbers::MAIN_MENU_ALWAYS_TOP, checked ? MF_CHECKED : MF_UNCHECKED);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_ABOUT: {
					const wchar_t* mbButtons[] = { L"Close" };
					XllnWndMessageBoxOpen(
						L"About"
						, L"Created by Glitchy Scripts.\r\n\
https://xlln.glitchyscripts.com/\r\n\
Please read the accompanying README.md file for all third party code and library licenses.\r\n\
\r\n\
Executable Launch Parameters:\r\n\
-xlivefps=<uint> ? 0 to disable fps limiter (default 60).\r\n\
-xllndebug ? Sleep until debugger attach.\r\n\
-xllndebuglog ? Enable debug log.\r\n\
-xlivedebug ? Sleep XLiveInitialize until debugger attach.\r\n\
-xlivenetdisable ? Disable all network functionality.\r\n\
-xlln_local_instance_id=<uint> ? 0 (default) to automatically assign the Local Instance ID.\r\n\
-xlln_network_instance_port=<ushort> ? Instance's network port (DEFAULT 39000 + X).\r\n\
-xllnbroadcastaddr=<string> ? Set the broadcast address.\r\n\
-xlivenetworkadapter=<string> ? Set the desired network adapter (via {GUID} format).\r\n\
-xllnconfig=<string> ? Sets the location of the config file."
						, XMB_NOICON
						, mbButtons
						, sizeof(mbButtons)/sizeof(mbButtons[0])
						, 0
						, 0
						, 0
					);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_REFRESH: {
					INT errorNetworkAdapter = RefreshNetworkAdapters();
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED: {
					bool checked = GetMenuState(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED, 0) != MF_CHECKED;
					if (!checked) {
						break;
					}
					{
						EnterCriticalSection(&xlive_critsec_network_adapter);
						CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED, MF_CHECKED);
						const uint32_t numOfItemsBefore = XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST - XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_MENU_FIRST_ITEM;
						const uint32_t numOfNetAdapterMenuItems = GetMenuItemCount(hMenu_network_adapters) - numOfItemsBefore;
						for (uint32_t iItem = 0; iItem < numOfNetAdapterMenuItems; iItem++) {
							CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST + iItem, MF_UNCHECKED);
						}
						if (xlive_config_specific_network_adapter_name) {
							delete[] xlive_config_specific_network_adapter_name;
							xlive_config_specific_network_adapter_name = 0;
						}
						xlive_specific_network_adapter = 0;
						LeaveCriticalSection(&xlive_critsec_network_adapter);
					}
					XllnNetworkInterfaceChanged();
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_IGNORE_TITLE: {
					bool checked = GetMenuState(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_IGNORE_TITLE, 0) != MF_CHECKED;
					CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_IGNORE_TITLE, checked ? MF_CHECKED : MF_UNCHECKED);
					xlive_ignore_title_network_adapter = checked;
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_DEBUG_LOG: {
					XllnShowWindow(XllnShowType::DEBUG_LOG_SHOW);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_SOCKETS: {
					XllnShowWindow(XllnShowType::SOCKETS_SHOW);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_CONNECTIONS: {
					XllnShowWindow(XllnShowType::CONNECTIONS_SHOW);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_ACHIEVEMENTS: {
					if (XllnAchievementsShow(xlln_login_player) != ERROR_SUCCESS) {
						XllnShowWindow(XllnShowType::ACHIEVEMENTS_SHOW);
					}
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_STATS: {
					if (XllnStatsShow(xlln_login_player) != ERROR_SUCCESS) {
						XllnShowWindow(XllnShowType::STATS_SHOW);
					}
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE: {
					bool checked = IsDlgButtonChecked(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE) != BST_CHECKED;
					CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
					xlive_local_users[xlln_login_player].live_enabled = checked;
					if (!checked) {
						CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
						xlive_local_users[xlln_login_player].online_enabled = checked;
					}
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE: {
					bool checked = IsDlgButtonChecked(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE) != BST_CHECKED;
					CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
					xlive_local_users[xlln_login_player].online_enabled = checked;
					if (checked) {
						CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE, checked ? BST_CHECKED : BST_UNCHECKED);
						xlive_local_users[xlln_login_player].live_enabled = checked;
					}
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN: {
					bool checked = IsDlgButtonChecked(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN) != BST_CHECKED;
					CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, checked ? BST_CHECKED : BST_UNCHECKED);
					xlive_local_users[xlln_login_player].auto_login = checked;
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_BTN_LOGIN: {
					char jlbuffer[32];
					GetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_USERNAME, jlbuffer, 32);
					TrimRemoveConsecutiveSpaces(jlbuffer);
					jlbuffer[XUSER_NAME_SIZE - 1] = 0;
					SetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_USERNAME, jlbuffer);
					
					bool liveEnabled = IsDlgButtonChecked(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE) == BST_CHECKED;
					bool onlineEnabled = IsDlgButtonChecked(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE) == BST_CHECKED;
					bool autoLoginChecked = xlive_local_users[xlln_login_player].auto_login;
					if (jlbuffer[0] != 0) {
						memcpy(xlive_local_users[xlln_login_player].username, jlbuffer, sizeof(xlive_local_users[xlln_login_player].username));
					}
					uint32_t result_login = XLLNLogin(
						xlln_login_player
						, ((liveEnabled ? 0x01 : 0) + (onlineEnabled ? 0 : 0x0100))
						, 0
						, (jlbuffer[0] == 0 ? NULL : jlbuffer)
					);
					xlive_local_users[xlln_login_player].auto_login = autoLoginChecked;
					CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_BTN_LOGOUT: {
					uint32_t result_logout = XLLNLogout(xlln_login_player);
					break;
				}
				case XLLNControlsMessageNumbers::MAIN_BTN_DIRECT_IP_CONNECT: {
					char jlbuffer[500];
					GetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_IP_PORT, jlbuffer, 500);
					char* jlbuffer2 = CloneString(jlbuffer);
					
					char* current = jlbuffer;
					char* colon = strrchr(current, ':');
					if (!colon) {
						MessageBoxW(xlln_window_hwnd, L"No port was specified.", L"XLLN Direct IP Connect Error", MB_OK);
						delete[] jlbuffer2;
						break;
					}
					
					colon[0] = 0;
					
					if (current[0] == '[') {
						current = &current[1];
						if (colon[-1] != ']') {
							MessageBoxW(xlln_window_hwnd, L"An invalid IP was specified.", L"XLLN Direct IP Connect Error", MB_OK);
							delete[] jlbuffer2;
							break;
						}
						colon[-1] = 0;
					}
					
					uint16_t portHBO = 0;
					if (sscanf_s(&colon[1], "%hu", &portHBO) != 1 || !portHBO) {
						MessageBoxW(xlln_window_hwnd, L"An invalid port was specified.", L"XLLN Direct IP Connect Error", MB_OK);
						delete[] jlbuffer2;
						break;
					}
					
					addrinfo hints;
					memset(&hints, 0, sizeof(hints));
					
					hints.ai_family = PF_UNSPEC;
					hints.ai_socktype = SOCK_DGRAM;
					hints.ai_protocol = IPPROTO_UDP;
					
					in6_addr serveraddr;
					int rc = WS2_32_inet_pton(AF_INET, current, &serveraddr);
					if (rc == 1) {
						hints.ai_family = AF_INET;
						hints.ai_flags |= AI_NUMERICHOST;
					}
					else {
						rc = WS2_32_inet_pton(AF_INET6, current, &serveraddr);
						if (rc == 1) {
							hints.ai_family = AF_INET6;
							hints.ai_flags |= AI_NUMERICHOST;
						}
					}
					
					SOCKADDR_STORAGE sockaddr;
					memset(&sockaddr, 0, sizeof(sockaddr));
					{
						addrinfo* res;
						int error = getaddrinfo(current, NULL, &hints, &res);
						if (error) {
							MessageBoxW(xlln_window_hwnd, L"Unable to parse the IP address.", L"XLLN Direct IP Connect Error", MB_OK);
							delete[] jlbuffer2;
							break;
						}
						
						addrinfo* nextRes = res;
						while (nextRes) {
							if (nextRes->ai_family == AF_INET) {
								memcpy(&sockaddr, res->ai_addr, res->ai_addrlen);
								(*(sockaddr_in*)&sockaddr).sin_port = htons(portHBO);
								break;
							}
							else if (nextRes->ai_family == AF_INET6) {
								memcpy(&sockaddr, res->ai_addr, res->ai_addrlen);
								(*(sockaddr_in6*)&sockaddr).sin6_port = htons(portHBO);
								break;
							}
							else {
								nextRes = nextRes->ai_next;
							}
						}
						
						freeaddrinfo(res);
					}
					
					if (!sockaddr.ss_family) {
						MessageBoxW(xlln_window_hwnd, L"Unable to parse the IP address.", L"XLLN Direct IP Connect Error", MB_OK);
						delete[] jlbuffer2;
						break;
					}
					
					delete[] xlln_direct_ip_connect_ip_port;
					xlln_direct_ip_connect_ip_port = jlbuffer2;
					jlbuffer2 = 0;
					
					XllnDirectIpConnectTo(xlln_login_player, &sockaddr, xlln_direct_ip_connect_password);
					
					break;
				}
				case ((EN_CHANGE << 16) | XLLNControlsMessageNumbers::MAIN_TBX_BROADCAST): {
					if (xlln_window_hwnd) {
						char jlbuffer[500];
						GetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_BROADCAST, jlbuffer, 500);
						delete[] broadcastAddrInput;
						broadcastAddrInput = CloneString(jlbuffer);
						char* temp = CloneString(broadcastAddrInput);
						ParseBroadcastAddrInput(temp);
						delete[] temp;
					}
					break;
				}
				case ((EN_CHANGE << 16) | XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD): {
					if (xlln_window_hwnd) {
						char jlbuffer[500];
						GetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD, jlbuffer, 500);
						delete[] xlln_direct_ip_connect_password;
						xlln_direct_ip_connect_password = CloneString(jlbuffer);
					}
					break;
				}
				case ((CBN_SELCHANGE << 16) | XLLNControlsMessageNumbers::MAIN_CMB_MICROPHONE_DEVICE): {
					HWND controlAudioDeviceComboBox = (HWND)lParam;
					LRESULT resultItemIndex = SendMessageA(controlAudioDeviceComboBox, CB_GETCURSEL, 0, 0);
					if (resultItemIndex != CB_ERR) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "CMB changed to: %zu.", resultItemIndex);
						if (xlive_local_users[xlln_login_player].audio_input_device_name) {
							delete[] xlive_local_users[xlln_login_player].audio_input_device_name;
							xlive_local_users[xlln_login_player].audio_input_device_name = 0;
						}
						if (resultItemIndex > 0) {
							LRESULT resultItemLength = SendMessageA(controlAudioDeviceComboBox, CB_GETLBTEXTLEN, resultItemIndex, 0);
							if (resultItemLength > 0) {
								resultItemLength++;
								xlive_local_users[xlln_login_player].audio_input_device_name = new wchar_t[resultItemLength];
								LRESULT resultItemSuccess = SendMessageW(controlAudioDeviceComboBox, CB_GETLBTEXT, resultItemIndex, (LPARAM)xlive_local_users[xlln_login_player].audio_input_device_name);
								if (resultItemSuccess == 0 || resultItemSuccess == CB_ERR) {
									delete[] xlive_local_users[xlln_login_player].audio_input_device_name;
									xlive_local_users[xlln_login_player].audio_input_device_name = 0;
								}
							}
						}
						SetAudioDeviceInput(xlln_login_player, (uint32_t)resultItemIndex - 2);
					}
					break;
				}
				case ((CBN_SELCHANGE << 16) | XLLNControlsMessageNumbers::MAIN_CMB_SPEAKER_DEVICE): {
					HWND controlAudioDeviceComboBox = (HWND)lParam;
					LRESULT resultItemIndex = SendMessageA(controlAudioDeviceComboBox, CB_GETCURSEL, 0, 0);
					if (resultItemIndex != CB_ERR) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "CMB changed to: %zu.", resultItemIndex);
						if (xlive_local_users[xlln_login_player].audio_output_device_name) {
							delete[] xlive_local_users[xlln_login_player].audio_output_device_name;
							xlive_local_users[xlln_login_player].audio_output_device_name = 0;
						}
						if (resultItemIndex > 0) {
							LRESULT resultItemLength = SendMessageA(controlAudioDeviceComboBox, CB_GETLBTEXTLEN, resultItemIndex, 0);
							if (resultItemLength > 0) {
								resultItemLength++;
								xlive_local_users[xlln_login_player].audio_output_device_name = new wchar_t[resultItemLength];
								LRESULT resultItemSuccess = SendMessageW(controlAudioDeviceComboBox, CB_GETLBTEXT, resultItemIndex, (LPARAM)xlive_local_users[xlln_login_player].audio_output_device_name);
								if (resultItemSuccess == 0 || resultItemSuccess == CB_ERR) {
									delete[] xlive_local_users[xlln_login_player].audio_output_device_name;
									xlive_local_users[xlln_login_player].audio_output_device_name = 0;
								}
							}
						}
						SetAudioDeviceOutput(xlln_login_player, (uint32_t)resultItemIndex - 2);
					}
					break;
				}
				default: {
					
					if (wParam >= XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_P1 && wParam <= XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_PX_MAX) {
						xlln_login_player = (uint32_t)wParam - XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_P1;
						XLLNWindowUpdateUserInputBoxes(xlln_login_player);
						break;
					}
					
					if (wParam >= XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST && wParam <= XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_LAST) {
						int numOfNetAdapterMenuItems = GetMenuItemCount(hMenu_network_adapters);
						const uint32_t numOfItemsBefore = XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST - XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_MENU_FIRST_ITEM;
						if (numOfNetAdapterMenuItems > numOfItemsBefore && wParam >= XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST && wParam < XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST + (uint32_t)numOfNetAdapterMenuItems - numOfItemsBefore) {
							const uint32_t networkAdapterIndex = (uint32_t)wParam - XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST;
							{
								EnterCriticalSection(&xlive_critsec_network_adapter);
								if (networkAdapterIndex < xlive_eligible_network_adapters.size()) {
									xlive_specific_network_adapter = xlive_eligible_network_adapters[networkAdapterIndex];
									if (xlive_config_specific_network_adapter_name) {
										delete[] xlive_config_specific_network_adapter_name;
									}
									xlive_config_specific_network_adapter_name = CloneString(xlive_specific_network_adapter->name);
									CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED, MF_UNCHECKED);
									for (uint32_t iEA = 0; iEA < xlive_eligible_network_adapters.size(); iEA++) {
										ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface = xlive_eligible_network_adapters[iEA];
										CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST + iEA, eligibleNetworkInterface == xlive_specific_network_adapter ? MF_CHECKED : MF_UNCHECKED);
									}
								}
								LeaveCriticalSection(&xlive_critsec_network_adapter);
							}
							
							XllnNetworkInterfaceChanged();
							
							break;
						}
						
						notHandled = true;
						break;
					}
					
					notHandled = true;
					break;
				}
			}
			if (!notHandled) {
				return 0;
			}
			break;
		}
		default: {
			break;
		}
	}
	
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HMENU CreateDLLWindowMenu(HINSTANCE hModule)
{
	HMENU hMenu;
	hMenu = CreateMenu();
	HMENU hMenuPopup;
	if (hMenu == NULL) {
		return FALSE;
	}
	
	hMenuPopup = CreatePopupMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, L"File");
	AppendMenuW(hMenuPopup, MF_STRING, XLLNControlsMessageNumbers::MAIN_MENU_VIEW_USER_CARD, L"View User Card");
	AppendMenuW(hMenuPopup, MF_STRING, XLLNControlsMessageNumbers::MAIN_MENU_SAVE_CONFIG, L"Save Current Configuration");
	AppendMenuW(hMenuPopup, MF_STRING, XLLNControlsMessageNumbers::MAIN_MENU_EXIT, L"Exit");
	
	hMenuPopup = CreatePopupMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, L"Login");
	for (size_t iPlayer = 0; iPlayer < XLIVE_LOCAL_USER_COUNT; iPlayer++) {
		wchar_t* buttonText = FormMallocString(L"Login P%u", iPlayer + 1);
		AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_P1 + iPlayer, buttonText);
		free(buttonText);
		buttonText = 0;
	}
	
	hMenu_network_adapters = hMenuPopup = CreatePopupMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, L"Network Adapters");
	AppendMenuW(hMenu_network_adapters
		, (xlive_ignore_title_network_adapter ? MF_CHECKED : MF_UNCHECKED)
		, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_IGNORE_TITLE
		, L"Ignore Title Specified Adapter"
	);
	AppendMenuW(hMenu_network_adapters
		, xlive_config_specific_network_adapter_name ? MF_UNCHECKED : MF_CHECKED
		, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED
		, L"Use Every Network Adapter"
	);
	AppendMenuW(hMenu_network_adapters, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_REFRESH, L"Refresh");
	AppendMenuW(hMenu_network_adapters, MF_SEPARATOR, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_SEPARATOR, NULL);
	
	hMenuPopup = CreatePopupMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, L"Debug");
#ifdef XLLN_DEBUG
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_DEBUG_LOG, L"Debug Log");
#endif
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_SOCKETS, L"Sockets");
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_CONNECTIONS, L"Connections");
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_ACHIEVEMENTS, L"Achievements");
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_DEBUG_STATS, L"Stats");
	
	hMenuPopup = CreatePopupMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMenuPopup, L"Help");
	AppendMenuW(hMenuPopup, MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_ALWAYS_TOP, L"Always on top");
	AppendMenuW(hMenuPopup, MF_STRING, XLLNControlsMessageNumbers::MAIN_MENU_ABOUT, L"About");
	
	CheckMenuItem(hMenu, XLLNControlsMessageNumbers::MAIN_MENU_LOGIN_P1, MF_CHECKED);
	
	return hMenu;
}

static DWORD WINAPI XllnThreadWndMain(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	
	{
		IWICImagingFactory* wicFactory = 0;
		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
		}
		
		HRSRC imageResHandle = 0;
		if (SUCCEEDED(hr)) {
			imageResHandle = FindResourceW(xlln_hModule, XLLN_LOGO_COMPACT_SQUARE_192_DPI, RT_IMAGE);
			hr = (imageResHandle ? S_OK : E_FAIL);
		}
		
		HGLOBAL imageResDataHandle = 0;
		// Load the resource to the HGLOBAL.
		if (SUCCEEDED(hr)) {
			imageResDataHandle = LoadResource(xlln_hModule, imageResHandle);
			hr = (imageResDataHandle ? S_OK : E_FAIL);
		}
		
		void* pImageFile = 0;
		// Lock the resource to retrieve memory pointer. Does not need to be unlocked later.
		if (SUCCEEDED(hr)) {
			pImageFile = LockResource(imageResDataHandle);
			hr = (pImageFile ? S_OK : E_FAIL);
		}
		
		DWORD imageFileSize = 0;
		if (SUCCEEDED(hr)) {
			imageFileSize = SizeofResource(xlln_hModule, imageResHandle);
			hr = (imageFileSize ? S_OK : E_FAIL);
		}
		
		IWICStream* pIWICStream = 0;
		if (SUCCEEDED(hr)) {
			hr = wicFactory->CreateStream(&pIWICStream);
		}
		
		if (SUCCEEDED(hr)) {
			hr = pIWICStream->InitializeFromMemory(reinterpret_cast<uint8_t*>(pImageFile), imageFileSize);
		}
		
		IWICBitmapDecoder* pIDecoder = 0;
		
		if (SUCCEEDED(hr)) {
			hr = wicFactory->CreateDecoderFromStream(
				pIWICStream // The stream to use to create the decoder.
				, 0 // Do not prefer a particular vendor.
				, WICDecodeMetadataCacheOnLoad // Cache metadata when needed.
				,&pIDecoder // Pointer to the decoder.
			);
		}
		
		IWICBitmapFrameDecode* wicDecoderFrame = 0;
		
		// Retrieve the initial frame.
		if (SUCCEEDED(hr)) {
			hr = pIDecoder->GetFrame(0, &wicDecoderFrame);
		}
		
		SafeRelease(pIDecoder);
		SafeRelease(pIWICStream);
		
		if (SUCCEEDED(hr)) {
			hr = wicFactory->CreateFormatConverter(&wic_converted_source_bitmap_xlln_logo);
		}
		
		if (SUCCEEDED(hr) && wic_converted_source_bitmap_xlln_logo) {
			// Format convert the current progressive level frame to 32bppPBGRA 
			hr = wic_converted_source_bitmap_xlln_logo->Initialize(
				wicDecoderFrame // Source bitmap to convert.
				, GUID_WICPixelFormat32bppPBGRA // Destination pixel format.
				, WICBitmapDitherTypeNone // Specified dither pattern.
				, 0 // Specify a particular palette.
				, 0.0f // Alpha threshold.
				, WICBitmapPaletteTypeCustom // Palette translation type.
			);
		}
		
		SafeRelease(wicDecoderFrame);
		SafeRelease(wicFactory);
	}
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowClass";
	xlln_window_hMenu = CreateDLLWindowMenu(xlln_hModule);
	
	// Register the windows Class.
	WNDCLASSEXW wc;
	wc.hInstance = xlln_hModule;
	wc.lpszClassName = windowclassname;
	wc.lpfnWndProc = DLLWindowProc;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hIcon = LoadIconW(xlln_hModule, XLLN_LOGO_COMPACT_SQUARE_ICON);
	wc.hIconSm = LoadIconW(xlln_hModule, XLLN_LOGO_COMPACT_SQUARE_ICON);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	if (!RegisterClassExW(&wc)) {
		return FALSE;
	}
	
	{
		wchar_t* windowTitle = FormMallocString(L"XLLN #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_window_hwnd = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 492, 470, hwdParent, xlln_window_hMenu, xlln_hModule, NULL);
		
		free(windowTitle);
		windowTitle = 0;
	}
	
	DWORD resultWait = WaitForSingleObject(xlln_window_create_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to be created."
			, __func__
		);
	}
	CloseHandle(xlln_window_create_event);
	xlln_window_create_event = INVALID_HANDLE_VALUE;
	
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_MICROPHONE_DEVICE);
		SendMessageA(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)("None"));
		SendMessageA(controlAudioDeviceComboBox, CB_SETCURSEL, 0, 0);
	}
	{
		HWND controlAudioDeviceComboBox = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CMB_SPEAKER_DEVICE);
		SendMessageA(controlAudioDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)("None"));
		SendMessageA(controlAudioDeviceComboBox, CB_SETCURSEL, 0, 0);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_VOLUME);
		SendMessage(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)XHV_AUDIO_SETTING_VOLUME_MAX);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_THRESHOLD);
		SendMessage(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)XHV_AUDIO_SETTING_INPUT_THRESHOLD_DEFAULT);
	}
	{
		HWND controlAudioDeviceTrackBar = GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBR_SPEAKER_VOLUME);
		SendMessage(controlAudioDeviceTrackBar, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)XHV_AUDIO_SETTING_VOLUME_MAX);
	}
	
	RefreshAvailableAudioDevices();
	
	ShowWindow(xlln_window_hwnd, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::MAIN_TBX_USERNAME
		, XLLNControlsMessageNumbers::MAIN_TBX_BROADCAST
		, XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD
		, XLLNControlsMessageNumbers::MAIN_TBX_DIRECT_IP_CONNECT_IP_PORT
	};
	const uint32_t trackbars[] = {
		XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_VOLUME
		, XLLNControlsMessageNumbers::MAIN_TBR_MICROPHONE_THRESHOLD
		, XLLNControlsMessageNumbers::MAIN_TBR_SPEAKER_VOLUME
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_window_hwnd, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		
		if (WndTrackBarMessageHandling(&msg, xlln_window_hwnd, trackbars, sizeof(trackbars) / sizeof(trackbars[0]))) {
			continue;
		}
		
		if (!IsDialogMessage(xlln_window_hwnd, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	SafeRelease(wic_converted_source_bitmap_xlln_logo);
	
	CoUninitialize();
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

bool XllnWndMainUpdateNetworkAdapters()
{
	if (!hMenu_network_adapters) {
		return false;
	}
	
	EnterCriticalSection(&xlive_critsec_network_adapter);
	
	const uint32_t numOfItemsBefore = XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST - XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_MENU_FIRST_ITEM;
	const uint32_t numOfNetAdapterMenuItems = GetMenuItemCount(hMenu_network_adapters) - numOfItemsBefore;
	for (uint32_t iItem = 0; iItem < numOfNetAdapterMenuItems; iItem++) {
		RemoveMenu(hMenu_network_adapters, numOfItemsBefore, MF_BYPOSITION);
	}
	
	for (size_t iEA = 0; iEA < xlive_eligible_network_adapters.size(); iEA++) {
		ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface = xlive_eligible_network_adapters[iEA];
		AppendMenuW(hMenu_network_adapters, eligibleNetworkInterface == xlive_specific_network_adapter ? MF_CHECKED : MF_UNCHECKED, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTERS_FIRST + iEA, eligibleNetworkInterface->description ? eligibleNetworkInterface->description : L"NULL");
	}
	
	if (xlive_specific_network_adapter) {
		CheckMenuItem(hMenu_network_adapters, XLLNControlsMessageNumbers::MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED, MF_UNCHECKED);
	}
	
	LeaveCriticalSection(&xlive_critsec_network_adapter);
	
	return true;
}


uint32_t InitXllnWndMain()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(0, NULL, XllnThreadWndMain, (void*)NULL, NULL, NULL);
	
	DWORD resultWait = WaitForSingleObject(xlln_window_initialised_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to finish initialising."
			, __func__
		);
	}
	CloseHandle(xlln_window_initialised_event);
	xlln_window_initialised_event = INVALID_HANDLE_VALUE;
	
	XllnWndMainUpdateNetworkAdapters();
	
	return ERROR_SUCCESS;
}

uint32_t UninitXllnWndMain()
{
	SendMessage(xlln_window_hwnd, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
	DWORD resultWait = WaitForSingleObject(xlln_window_destroy_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to be destroyed."
			, __func__
		);
	}
	CloseHandle(xlln_window_destroy_event);
	xlln_window_destroy_event = INVALID_HANDLE_VALUE;
	
	return ERROR_SUCCESS;
}
