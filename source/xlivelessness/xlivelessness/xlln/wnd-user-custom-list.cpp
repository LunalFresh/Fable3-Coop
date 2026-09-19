#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "./wnd-user-custom-list.hpp"
#include "./wnd-main.hpp"
#include "./wnd-user-card.hpp"
#include "./xlln.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xsession.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "./debug-log.hpp"
#include "../xlive/xnotify.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_user_custom_list = 0;
static HFONT xlln_user_custom_list_title_font = 0;
static HWND hwndListView = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

CRITICAL_SECTION xlln_critsec_user_custom_list;
static uint32_t xlln_user_custom_list_user_index = 0;
static XPLAYERLIST_RESULT* xlln_user_custom_list_user_result = 0;
static HANDLE xlln_user_custom_list_user_result_event = INVALID_HANDLE_VALUE;
static XOVERLAPPED* xlln_user_custom_list_user_result_overlapped = 0;

typedef struct _CUSTOM_LIST_USER_INFO {
	XUID xuid = 0;
	wchar_t* custom_text = 0;
	
	~_CUSTOM_LIST_USER_INFO() {
		if (custom_text) {
			delete[] custom_text;
			custom_text = 0;
		}
	}
} CUSTOM_LIST_USER_INFO;

static std::map<XUID, CUSTOM_LIST_USER_INFO*> xlln_user_custom_list_info;

//void XllnWndUserCustomListInvalidate()
//{
//	InvalidateRect(xlln_hwnd_user_custom_list, NULL, FALSE);
//}

uint32_t XllnUserCustomListShow(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* title_text
	, const wchar_t* description_text
	, const uint8_t* image_data
	, size_t image_data_size
	, const XPLAYERLIST_USER* list_players
	, size_t list_player_count
	, const XPLAYERLIST_BUTTON* x_button_action
	, const XPLAYERLIST_BUTTON* y_button_action
	, XPLAYERLIST_RESULT* result_player_list_action
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (flags && flags != XPLAYERLIST_FLAGS::XPLAYERLIST_FLAG_CUSTOMTEXT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags (0x%08x) invalid, XPLAYERLIST_FLAG_CUSTOMTEXT (%u) is the only flag supported.", __func__, flags, XPLAYERLIST_FLAGS::XPLAYERLIST_FLAG_CUSTOMTEXT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!title_text) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s title_text is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!description_text) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s description_text is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (image_data_size && !image_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (image_data_size && !image_data).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!image_data_size && image_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!image_data_size && image_data).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (image_data_size > XPLAYERLIST_IMAGE_MAX_SIZE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s image_data_size (%u) is greater than XPLAYERLIST_IMAGE_MAX_SIZE (%u).", __func__, image_data_size, XPLAYERLIST_IMAGE_MAX_SIZE);
		return ERROR_INVALID_PARAMETER;
	}
	if (!list_player_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s list_player_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (list_player_count > XPLAYERLIST_MAX_PLAYERS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s list_player_count (%u) is greater than XPLAYERLIST_MAX_PLAYERS (%u).", __func__, list_player_count, XPLAYERLIST_MAX_PLAYERS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!list_players) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s list_players is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (x_button_action) {
		if (x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_PLAYERREVIEW
			&& x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_GAMEINVITE
			&& x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_MESSAGE
			&& x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_FRIENDREQUEST
			&& x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL
			&& x_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL
		) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s x_button_action->dwType (%u) is invalid.", __func__, x_button_action->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!x_button_action->wszCustomText && (x_button_action->dwType == XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL || x_button_action->dwType == XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s x_button_action->wszCustomText cannot be NULL when x_button_action->dwType (%u) is XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL or XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL.", __func__, x_button_action->dwType);
			return ERROR_INVALID_PARAMETER;
		}
	}
	if (y_button_action) {
		if (y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_PLAYERREVIEW
			&& y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_GAMEINVITE
			&& y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_MESSAGE
			&& y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_FRIENDREQUEST
			&& y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL
			&& y_button_action->dwType != XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL
		) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s y_button_action->dwType (%u) is invalid.", __func__, y_button_action->dwType);
			return ERROR_INVALID_PARAMETER;
		}
		if (!y_button_action->wszCustomText && (y_button_action->dwType == XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL || y_button_action->dwType == XPLAYERLIST_BUTTON_TYPE::XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s y_button_action->wszCustomText cannot be NULL when y_button_action->dwType (%u) is XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMGLOBAL or XPLAYERLIST_BUTTON_TYPE_TITLECUSTOMINDIVIDUAL.", __func__, y_button_action->dwType);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	xlln_user_custom_list_user_index = user_index;
	
	if (title_text) {
		SetDlgItemTextW(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE, title_text);
	}
	else {
		SetDlgItemTextW(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE, L"Custom User List");
	}
	
	if (description_text) {
		SetDlgItemTextW(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_DESCRIPTION, description_text);
	}
	else {
		SetDlgItemTextW(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_DESCRIPTION, L"Select a user to open their User Card for more specific information and settings.");
	}
	
	EnterCriticalSection(&xlln_critsec_user_custom_list);
	
	for (const auto &itrCustomListUserInfo : xlln_user_custom_list_info) {
		if (itrCustomListUserInfo.second) {
			delete itrCustomListUserInfo.second;
		}
	}
	xlln_user_custom_list_info.clear();
	
	for (uint32_t iUser = 0; iUser < list_player_count; iUser++) {
		const XPLAYERLIST_USER* playerListUser = &list_players[iUser];
		if (playerListUser->xuid != xlive_local_users[user_index].xuid && !xlln_user_custom_list_info.count(playerListUser->xuid)) {
			CUSTOM_LIST_USER_INFO* customListUserInfo = xlln_user_custom_list_info[playerListUser->xuid] = new CUSTOM_LIST_USER_INFO;
			customListUserInfo->xuid = playerListUser->xuid;
			if (playerListUser->wszCustomText && flags == XPLAYERLIST_FLAGS::XPLAYERLIST_FLAG_CUSTOMTEXT) {
				customListUserInfo->custom_text = CloneString(playerListUser->wszCustomText);
			}
		}
	}
	
	xlln_user_custom_list_user_result = result_player_list_action;
	if (xoverlapped) {
		xlln_user_custom_list_user_result_overlapped = xoverlapped;
	}
	else {
		xlln_user_custom_list_user_result_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	}
	
	LeaveCriticalSection(&xlln_critsec_user_custom_list);
	
	XllnShowWindow(XllnShowType::USER_CUSTOM_LIST_SHOW);
	
	if (xoverlapped) {
		//asynchronous
		return ERROR_IO_PENDING;
	}
	
	DWORD resultWait = WaitForSingleObject(xlln_user_custom_list_user_result_event, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s failed to wait for the window."
			, __func__
		);
	}
	CloseHandle(xlln_user_custom_list_user_result_event);
	xlln_user_custom_list_user_result_event = INVALID_HANDLE_VALUE;
	
	return ERROR_SUCCESS;
}

static int CreateColumn(HWND hwndLV, int iCol, const wchar_t* text, int iWidth)
{
	LVCOLUMN lvc;
	
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = iWidth;
	lvc.pszText = (wchar_t*)text;
	lvc.iSubItem = iCol;
	return ListView_InsertColumn(hwndLV, iCol, &lvc);
}

static int CreateItem(HWND hwndListView, int iItem)
{
	LV_ITEM item;
	item.mask = LVIF_TEXT;
	item.iItem = iItem;
	item.iIndent = 0;
	item.iSubItem = 0;
	item.state = 0;
	item.cColumns = 0;
	item.pszText = (wchar_t*)L"";
	return ListView_InsertItem(hwndListView, &item);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			{
				HWND controlUserList = GetDlgItem(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_LST_USERS);
				MoveWindow(controlUserList, 10, 75, width - 20, height - 85, TRUE);
			}
			{
				HWND controlTextbox = GetDlgItem(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE);
				MoveWindow(controlTextbox, 10, 10, width - 20, 20, TRUE);
			}
			{
				HWND controlTextbox = GetDlgItem(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_DESCRIPTION);
				MoveWindow(controlTextbox, 10, 35, width - 20, 35, TRUE);
			}
			
			break;
		}
		case WM_PAINT: {
			
			ListView_DeleteAllItems(hwndListView);
			{
				EnterCriticalSection(&xlln_critsec_user_custom_list);
				EnterCriticalSection(&xlive_critsec_remote_user);
				
				uint32_t iUser = 0;
				for (const auto &itrCustomListUserInfo : xlln_user_custom_list_info) {
					CUSTOM_LIST_USER_INFO* customListUserInfo = itrCustomListUserInfo.second;
					CreateItem(hwndListView, iUser);
					if (customListUserInfo) {
						wchar_t* textItemLabel;
						uint32_t iCol = 0;
#define AddSocketItemColumn(format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); ListView_SetItemText(hwndListView, iUser, iCol++, textItemLabel); free(textItemLabel)
						
						AddSocketItemColumn(L"0x%016I64x", customListUserInfo->xuid);
						{
							char* username = 0;
							auto itrRemoteUserUsername = xlln_remote_user_usernames.find(customListUserInfo->xuid);
							if (itrRemoteUserUsername != xlln_remote_user_usernames.end()) {
								username = itrRemoteUserUsername->second;
							}
							LVITEMA listViewItem;
							listViewItem.iSubItem = iCol++;
							listViewItem.pszText = (char*)(username ? username : "");
							listViewItem.cchTextMax = (uint32_t)(strlen(listViewItem.pszText) + 1);
							SendMessageA(hwndListView, LVM_SETITEMTEXTA, (WPARAM)iUser, (LPARAM)&listViewItem);
						}
						{
							bool userInSession = false;
							
							EnterCriticalSection(&xlive_critsec_xsession);
							for (const auto &itrXSessionDetails : xlive_xsession_local_sessions) {
								LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails.second;
								
								if (xsessionDetails->eState != XSESSION_STATE_LOBBY
									&& xsessionDetails->eState != XSESSION_STATE_REGISTRATION
									&& xsessionDetails->eState != XSESSION_STATE_INGAME
									&& xsessionDetails->eState != XSESSION_STATE_REPORTING
								) {
									continue;
								}
								
								const auto& itrSessionMember = xsessionDetails->sessionMembers.find(customListUserInfo->xuid);
								if (itrSessionMember != xsessionDetails->sessionMembers.end()) {
									userInSession = true;
									break;
								}
							}
							LeaveCriticalSection(&xlive_critsec_xsession);
							
							AddSocketItemColumn((userInSession ? L"Yes" : L"No"));
						}
						AddSocketItemColumn((customListUserInfo->custom_text ? customListUserInfo->custom_text : L""));
						
#undef AddSocketItemColumn
					}
					iUser++;
				}
				
				LeaveCriticalSection(&xlive_critsec_remote_user);
				LeaveCriticalSection(&xlln_critsec_user_custom_list);
			}
			
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				EnterCriticalSection(&xlln_critsec_user_custom_list);
				
				if (xlln_user_custom_list_user_result) {
					// TODO custom user list X and Y actions.
					xlln_user_custom_list_user_result->xuidSelected;
					xlln_user_custom_list_user_result->dwKeyCode;
					xlln_user_custom_list_user_result = 0;
				}
				if (xlln_user_custom_list_user_result_event) {
					SetEvent(xlln_user_custom_list_user_result_event);
				}
				if (xlln_user_custom_list_user_result_overlapped) {
					xlln_user_custom_list_user_result_overlapped->InternalLow = ERROR_SUCCESS;
					xlln_user_custom_list_user_result_overlapped->InternalHigh = ERROR_SUCCESS;
					xlln_user_custom_list_user_result_overlapped->dwExtendedError = ERROR_SUCCESS;
					Check_Overlapped(xlln_user_custom_list_user_result_overlapped);
					
					xlln_user_custom_list_user_result_overlapped = 0;
				}
				
				LeaveCriticalSection(&xlln_critsec_user_custom_list);
				
				XllnShowWindow(XllnShowType::USER_CUSTOM_LIST_HIDE);
				
				return 0;
			}
			break;
		}
		case WM_NOTIFY: {
			switch (LOWORD(wParam)) {
				case XLLNControlsMessageNumbers::USER_CUSTOM_LIST_LST_USERS: {
					
					NMHDR* nmhdr = (NMHDR*)lParam;
					switch (nmhdr->code) {
						case NM_DBLCLK:
						case NM_RETURN: {
							LRESULT resultSelectedIndex = SendMessageA(nmhdr->hwndFrom, LVM_GETNEXTITEM, (WPARAM)-1, (LPARAM)(LVNI_FOCUSED | LVNI_SELECTED));
							if (resultSelectedIndex >= 0) {
								char userXuid[16+2+1] = "";
								{
									LVITEMA listViewItem;
									listViewItem.iSubItem = 0;
									listViewItem.pszText = userXuid;
									listViewItem.cchTextMax = sizeof(userXuid);
									SendMessageA(nmhdr->hwndFrom, LVM_GETITEMTEXTA, (WPARAM)resultSelectedIndex, (LPARAM)&listViewItem);
								}
								XUID xuidSelected = 0;
								if (sscanf_s(userXuid, "0x%I64x", &xuidSelected) == 1) {
									XllnWndUserCardShow(true , xlln_user_custom_list_user_index, xuidSelected);
								}
							}
							break;
						}
					}
					
					break;
				}
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
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				10, 10, 615, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE, xlln_hModule, 0);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				10, 35, 615, 35, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_DESCRIPTION, xlln_hModule, 0);
				
			hwndListView = CreateWindowA(WC_LISTVIEWA, ""
				, WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP
				, 10, 75, 615, 275, hWnd, (HMENU)XLLNControlsMessageNumbers::USER_CUSTOM_LIST_LST_USERS, xlln_hModule, 0
			);
			
			uint32_t j = 0;
			CreateColumn(hwndListView, ++j, L"User XUID", 120);
			CreateColumn(hwndListView, ++j, L"Username", 180);
			CreateColumn(hwndListView, ++j, L"XSession Peer", 90);
			CreateColumn(hwndListView, ++j, L"Info", 220);
			
			SetEvent(xlln_window_create_event);
			
			break;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
	}
	
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

static DWORD WINAPI XllnThreadWndUserCustomList(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowUserCustomListClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Custom User List #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_user_custom_list = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 650, 400, hwdParent, 0, hModule, NULL);
		
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
		xlln_user_custom_list_title_font = CreateFontIndirect(&logfont);
	}
	
	HWND textboxTitle = GetDlgItem(xlln_hwnd_user_custom_list, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE);
	SendMessage(textboxTitle, WM_SETFONT, (WPARAM)xlln_user_custom_list_title_font, TRUE);
	
	ShowWindow(xlln_hwnd_user_custom_list, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_TITLE
		, XLLNControlsMessageNumbers::USER_CUSTOM_LIST_TBX_DESCRIPTION
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_hwnd_user_custom_list, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		// Handle tab ordering.
		bool handleListViewReturnKey = (msg.hwnd == hwndListView && LOWORD(msg.message) == WM_KEYDOWN && msg.wParam == VK_RETURN);
		if (handleListViewReturnKey || !IsDialogMessage(xlln_hwnd_user_custom_list, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndUserCustomList()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(0, NULL, XllnThreadWndUserCustomList, (void*)xlln_hModule, NULL, NULL);
	
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

uint32_t UninitXllnWndUserCustomList()
{
	SendMessage(xlln_hwnd_user_custom_list, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
