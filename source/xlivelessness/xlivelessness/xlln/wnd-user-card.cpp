#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "../xlive/xfuncs.hpp"
#include "./wnd-user-card.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xcustom.hpp"
#include "../xlive/xhv-engine.hpp"
#include "../xlive/xnotify.hpp"
#include "../xlive/xrender.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "./debug-log.hpp"
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_user_card = 0;
static HFONT xlln_user_card_font_title = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

CRITICAL_SECTION xlln_critsec_user_card;
uint32_t xlln_user_card_current_user_index = 0;
XUID xlln_user_card_current_xuid = 0;

XUID xlln_user_card_custom_action_xuid = 0;
uint32_t xlln_user_card_custom_action_index = 0;
uint32_t xlln_user_card_custom_user_index = 0;

void XllnWndUserCardShow(bool show_window, uint32_t user_index, XUID view_xuid)
{
	EnterCriticalSection(&xlln_critsec_user_card);
	
	bool update = (xlln_user_card_current_xuid != view_xuid || xlln_user_card_current_user_index != user_index);
	xlln_user_card_current_xuid = view_xuid;
	xlln_user_card_current_user_index = user_index;
	
	LeaveCriticalSection(&xlln_critsec_user_card);
	
	if (update) {
		PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USER_UPDATE, 0, 0);
		PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
		
		HWND controlComboBoxUsers = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_CMB_USERS);
		
		char* labelText = FormMallocString("0x%016I64x", view_xuid);
		LRESULT resultFind = SendMessageA(controlComboBoxUsers, CB_FINDSTRING, (WPARAM)-1, (LPARAM)labelText);
		free(labelText);
		labelText = 0;
		
		if (resultFind == CB_ERR) {
			SendMessageA(controlComboBoxUsers, CB_SETCURSEL, (WPARAM)0, 0);
		}
		else {
			SendMessageA(controlComboBoxUsers, CB_SETCURSEL, (WPARAM)resultFind, 0);
		}
	}
	
	XllnShowWindow(show_window ? XllnShowType::USER_CARD_SHOW : XllnShowType::USER_CARD_HIDE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_user_card, &ps);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			
			{
				char textLabel[] = "Voice Volume (0-1000%):";
				TextOutA(hdc, 15, 117, textLabel, (uint32_t)strlen(textLabel));
			}
			
			EndPaint(xlln_hwnd_user_card, &ps);
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::USER_CARD_HIDE);
				
				return 0;
			}
			break;
		}
		//case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_CREATE: {
			
			CreateWindowA(WC_COMBOBOXA, 0, WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_TABSTOP,
				10, 10, 330, 300, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_CMB_USERS, xlln_hModule, 0);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | ES_READONLY | WS_TABSTOP,
				10, 50, 330, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_TBX_XUID, xlln_hModule, 0);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | ES_READONLY | WS_TABSTOP,
				10, 80, 330, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_TBX_USERNAME, xlln_hModule, 0);
			
			{
				HWND controlTrackBarVolume = CreateWindowA(TRACKBAR_CLASSA, 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					175, 110, 170, 30, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_TBR_VOLUME, xlln_hModule, 0);
				
				SendMessage(controlTrackBarVolume, TBM_SETRANGE, (WPARAM)TRUE /* redraw flag */, (LPARAM) MAKELONG(0, XHV_AUDIO_SETTING_VOLUME_MAX_BOOST));
				SendMessage(controlTrackBarVolume, TBM_SETPAGESIZE, 0, (LPARAM)(XHV_AUDIO_SETTING_VOLUME_MAX/10));
				SendMessage(controlTrackBarVolume, TBM_SETTIC, (WPARAM)0, (LPARAM)XHV_AUDIO_SETTING_VOLUME_MAX);
			}
			
			CreateWindowA(WC_BUTTONA, "Is Currently Talking:", BS_CHECKBOX | BS_LEFTTEXT | BS_FLAT | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				14, 150, 181, 22, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_CHK_IS_TALKING, xlln_hModule, 0);
			
			CreateWindowA(WC_BUTTONA, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 185, 330, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0, xlln_hModule, 0);
			
			CreateWindowA(WC_BUTTONA, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 220, 330, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_1, xlln_hModule, 0);
			
			CreateWindowA(WC_BUTTONA, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 255, 330, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_2, xlln_hModule, 0);
			
			SetEvent(xlln_window_create_event);
			
			break;
		}
		case WM_HSCROLL: {
			if ((wParam & 0xFFFF) == TB_ENDTRACK) {
				{
					HWND controlTrackBarVolume = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBR_VOLUME);
					if ((HWND)lParam == controlTrackBarVolume) {
						LRESULT resultItem = SendMessageA(controlTrackBarVolume, TBM_GETPOS, 0, 0);
						if (resultItem >= 0 && resultItem <= XHV_AUDIO_SETTING_VOLUME_MAX_BOOST) {
							uint16_t remoteVoiceVolume = (uint16_t)resultItem;
							
							EnterCriticalSection(&xlln_critsec_user_card);
							
							if (xlln_user_card_current_xuid) {
								EnterCriticalSection(&xlive_critsec_local_user);
								auto &voiceRemoteVolumes = xlive_local_users[xlln_user_card_current_user_index].xhv_voice_remote_volume;
								if (remoteVoiceVolume == XHV_AUDIO_SETTING_VOLUME_MAX) {
									auto itrVoiceRemoteVolume = voiceRemoteVolumes.find(xlln_user_card_current_xuid);
									if (itrVoiceRemoteVolume != voiceRemoteVolumes.end()) {
										voiceRemoteVolumes.erase(itrVoiceRemoteVolume);
									}
								}
								else {
									voiceRemoteVolumes[xlln_user_card_current_xuid] = remoteVoiceVolume;
								}
								LeaveCriticalSection(&xlive_critsec_local_user);
							}
							
							LeaveCriticalSection(&xlln_critsec_user_card);
						}
						return 0;
					}
				}
			}
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_USER_CARD_ACTIONS_UPDATE: {
			
			EnterCriticalSection(&xlive_critsec_custom_actions);
			
			for (uint32_t iAction = 0; iAction <= XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_MAX - XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0; iAction++) {
				auto itrCustomActionInfo = xlln_custom_actions.find(iAction);
				CUSTOM_ACTION_INFO* customActionInfo = ((itrCustomActionInfo != xlln_custom_actions.end() && itrCustomActionInfo->second) ? itrCustomActionInfo->second : 0);
				
				HWND controlAction = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0 + iAction);
				if (customActionInfo && customActionInfo->action_label) {
					SetDlgItemTextW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0 + iAction, customActionInfo->action_label);
					ShowWindow(controlAction, SW_SHOWNORMAL);
				}
				else {
					SetDlgItemTextW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0 + iAction, L"");
					ShowWindow(controlAction, SW_HIDE);
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE: {
			HWND controlComboBoxUsers = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_CMB_USERS);
			
			EnterCriticalSection(&xlive_critsec_remote_user);
			
			while (SendMessageA(controlComboBoxUsers, CB_DELETESTRING, 0, 0) > 0);
			
			SendMessageA(controlComboBoxUsers, CB_ADDSTRING, 0, (LPARAM)(""));
			SendMessageA(controlComboBoxUsers, CB_SETCURSEL, 0, 0);
			
			size_t iItem = 1;
			for (auto &itrRemoteUser : xlln_remote_user_usernames) {
				char* itemLabel = FormMallocString("0x%016I64x %s", itrRemoteUser.first, (itrRemoteUser.second ? itrRemoteUser.second : ""));
				SendMessageA(controlComboBoxUsers, CB_ADDSTRING, 0, (LPARAM)itemLabel);
				free(itemLabel);
				itemLabel = 0;
				if (itrRemoteUser.first == xlln_user_card_current_xuid) {
					SendMessageA(controlComboBoxUsers, CB_SETCURSEL, (WPARAM)iItem, 0);
				}
				iItem++;
			}
			
			LeaveCriticalSection(&xlive_critsec_remote_user);
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_USER_CARD_USER_UPDATE: {
			
			EnterCriticalSection(&xlln_critsec_user_card);
			
			{
				wchar_t* textboxText = FormMallocString(L"0x%016I64x", xlln_user_card_current_xuid);
				SetDlgItemTextW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBX_XUID, textboxText);
				free(textboxText);
				textboxText = 0;
			}
			
			{
				EnterCriticalSection(&xlive_critsec_remote_user);
				
				char* username = 0;
				if (xlln_user_card_current_xuid) {
					auto itrRemoteUserUsername = xlln_remote_user_usernames.find(xlln_user_card_current_xuid);
					if (itrRemoteUserUsername != xlln_remote_user_usernames.end()) {
						username = itrRemoteUserUsername->second;
					}
				}
				
				SetDlgItemTextA(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBX_USERNAME, (username ? username : ""));
				
				LeaveCriticalSection(&xlive_critsec_remote_user);
			}
			
			{
				EnterCriticalSection(&xlive_critsec_local_user);
				
				auto &voiceRemoteVolumes = xlive_local_users[xlln_user_card_current_user_index].xhv_voice_remote_volume;
				auto itrVoiceRemoteVolume = voiceRemoteVolumes.find(xlln_user_card_current_xuid);
				uint16_t remoteVoiceVolume = XHV_AUDIO_SETTING_VOLUME_MAX;
				if (itrVoiceRemoteVolume != voiceRemoteVolumes.end()) {
					remoteVoiceVolume = itrVoiceRemoteVolume->second;
				}
				
				LeaveCriticalSection(&xlive_critsec_local_user);
				
				HWND controlTrackBarVolume = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBR_VOLUME);
				SendMessageW(controlTrackBarVolume, TBM_SETPOS, (WPARAM)TRUE /* redraw flag */, (LPARAM)remoteVoiceVolume);
			}
			
			LeaveCriticalSection(&xlln_critsec_user_card);
			
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE: {
			bool isTalking = false;
			
			EnterCriticalSection(&xlln_critsec_user_card);
			EnterCriticalSection(&xlive_critsec_xhv_engines);
			
			if (xlln_user_card_current_xuid) {
				BOOL isMuted = FALSE;
				XUserMuteListQuery(xlln_user_card_current_user_index, xlln_user_card_current_xuid, &isMuted);
				if (!isMuted) {
					for (const auto &xhvEngine : xlive_xhv_engines) {
						isTalking |= !!xhvEngine->isRemoteTalking(xlln_user_card_current_xuid);
					}
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_xhv_engines);
			LeaveCriticalSection(&xlln_critsec_user_card);
			
			CheckDlgButton(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_CHK_IS_TALKING, (isTalking ? BST_CHECKED : BST_UNCHECKED));
			
			break;
		}
		case WM_COMMAND: {
			switch (wParam) {
				case ((CBN_SELCHANGE << 16) | XLLNControlsMessageNumbers::USER_CARD_CMB_USERS): {
					HWND controlComboBoxUsers = (HWND)lParam;
					LRESULT resultItemIndex = SendMessageA(controlComboBoxUsers, CB_GETCURSEL, 0, 0);
					if (resultItemIndex != CB_ERR) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG, "CMB changed to: %u.", resultItemIndex);
						
						EnterCriticalSection(&xlln_critsec_user_card);
						
						xlln_user_card_current_xuid = 0;
						
						if (resultItemIndex > 0) {
							LRESULT resultItemLength = SendMessageA(controlComboBoxUsers, CB_GETLBTEXTLEN, resultItemIndex, 0);
							if (resultItemLength > 0) {
								resultItemLength++;
								char* labelText = new char[resultItemLength];
								labelText[0] = 0;
								LRESULT resultItemSuccess = SendMessageA(controlComboBoxUsers, CB_GETLBTEXT, resultItemIndex, (LPARAM)labelText);
								if (resultItemSuccess != 0 && resultItemSuccess != CB_ERR) {
									if (sscanf_s(labelText, "0x%016I64x", &xlln_user_card_current_xuid) == 1) {
										// success.
									}
								}
								delete[] labelText;
								labelText = 0;
							}
						}
						
						PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USER_UPDATE, 0, 0);
						PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_TALKING_UPDATE, 0, 0);
						
						LeaveCriticalSection(&xlln_critsec_user_card);
					}
					
					break;
				}
				case XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0:
				case XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_1:
				case XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_2: {
					
					EnterCriticalSection(&xlln_critsec_user_card);
					
					xlln_user_card_custom_action_xuid = xlln_user_card_current_xuid;
					xlln_user_card_custom_user_index = xlln_user_card_current_user_index;
					xlln_user_card_custom_action_index = (uint32_t)wParam - XLLNControlsMessageNumbers::USER_CARD_BTN_ACTION_0;
					
					LeaveCriticalSection(&xlln_critsec_user_card);
					
					XLiveNotifyAddEvent(XN_CUSTOM_ACTIONPRESSED, 0);
					
					break;
				}
			}
			
			return 0;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
	}
	
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

static DWORD WINAPI XllnThreadWndUserCard(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowUserCardClass";
	HINSTANCE hModule = reinterpret_cast<HINSTANCE>(lpParam);
	
	WNDCLASSEXW wc;
	wc.hInstance = hModule;
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
		wchar_t* windowTitle = FormMallocString(L"XLLN User Card #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_user_card = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 360, 325, hwdParent, 0, hModule, NULL);
		
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
		LOGFONT logfont;
		memset(&logfont, 0, sizeof(logfont));
		logfont.lfCharSet = DEFAULT_CHARSET;
		logfont.lfWeight = 700;
		logfont.lfHeight = -16;
		xlln_user_card_font_title = CreateFontIndirect(&logfont);
	}
	
	{
		HWND controlTextbox = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBX_XUID);
		SendMessage(controlTextbox, WM_SETFONT, (WPARAM)xlln_user_card_font_title, TRUE);
	}
	{
		HWND controlTextbox = GetDlgItem(xlln_hwnd_user_card, XLLNControlsMessageNumbers::USER_CARD_TBX_USERNAME);
		SendMessage(controlTextbox, WM_SETFONT, (WPARAM)xlln_user_card_font_title, TRUE);
	}
	
	PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_ACTIONS_UPDATE, 0, 0);
	PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USER_UPDATE, 0, 0);
	PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE, 0, 0);
	
	ShowWindow(xlln_hwnd_user_card, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::USER_CARD_TBX_XUID
		, XLLNControlsMessageNumbers::USER_CARD_TBX_USERNAME
	};
	const uint32_t trackbars[] = {
		XLLNControlsMessageNumbers::USER_CARD_TBR_VOLUME
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_hwnd_user_card, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		
		if (WndTrackBarMessageHandling(&msg, xlln_hwnd_user_card, trackbars, sizeof(trackbars) / sizeof(trackbars[0]))) {
			continue;
		}
		
		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_user_card, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	DeleteObject(xlln_user_card_font_title);
	xlln_user_card_font_title = 0;
	
	xlln_hwnd_user_card = 0;
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndUserCard()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(NULL, 0, XllnThreadWndUserCard, (void*)xlln_hModule, 0, NULL);
	
	DWORD resultWait = WaitForSingleObject(xlln_window_initialised_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s failed to wait for the window to finish initialising."
			, __func__
		);
	}
	CloseHandle(xlln_window_initialised_event);
	xlln_window_initialised_event = INVALID_HANDLE_VALUE;
	
	return ERROR_SUCCESS;
}

uint32_t UninitXllnWndUserCard()
{
	SendMessage(xlln_hwnd_user_card, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
