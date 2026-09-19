#include <winsock2.h>
#include <Windows.h>
#include "./wnd-debug-log.hpp"
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "../xlive/xrender.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_debug_log = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;
static bool pause_visible_updates = false;
static size_t last_line_count = 0;

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			{
				HWND controlDebugLog = GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_LOG);
				MoveWindow(controlDebugLog, 5, 76, width - 10, height - 81, TRUE);
				SendMessageW(controlDebugLog, EM_LINESCROLL, (WPARAM)0, (LPARAM)last_line_count);
			}
			{
				HWND controlDebugLog = GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_BLACKLIST);
				MoveWindow(controlDebugLog, 165, 50, width - 170, 22, TRUE);
			}
			
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_DEBUG_TBX_LOG_UPDATE: {
			if (!pause_visible_updates) {
				EnterCriticalSection(&xlln_critsec_debug_log);
				wchar_t* messages = XllnGetDebugLogCacheMessages(&last_line_count);
				if (messages) {
					SetDlgItemTextW(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_LOG, messages);
					free(messages);
					messages = 0;
					HWND controlDebugLog = GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_LOG);
					SendMessageW(controlDebugLog, EM_LINESCROLL, (WPARAM)0, (LPARAM)last_line_count+1);
					LRESULT resultGetFirstVisibleLine = SendMessageW(controlDebugLog, EM_GETFIRSTVISIBLELINE, (WPARAM)0, (LPARAM)0);
					if (resultGetFirstVisibleLine >= 0) {
						size_t firstVisibleLine = resultGetFirstVisibleLine;
						if (firstVisibleLine > 1) {
							debug_log_cache_max = last_line_count - (firstVisibleLine - 1);
						}
						else if (firstVisibleLine == 0) {
							debug_log_cache_max = last_line_count + 10;
						}
					}
				}
				LeaveCriticalSection(&xlln_critsec_debug_log);
			}
			break;
		}
		case XLLNControlsMessageNumbers::EVENT_DEBUG_TBX_BLACKLIST_REFRESH: {
			char* blacklist = XllnGetDebugLogTraceBlacklist();
			if (blacklist) {
				SetDlgItemTextA(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_BLACKLIST, blacklist);
				free(blacklist);
				blacklist = 0;
			}
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::DEBUG_LOG_HIDE);
				return 0;
			}
			break;
		}
		case WM_COMMAND: {
			
			switch (wParam) {
				case XLLNControlsMessageNumbers::DEBUG_BTN_BLACKLIST_UPDATE: {
					HWND controlDebugLogBlacklist = GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_TBX_BLACKLIST);
					LRESULT resultGetTextLength = SendMessageW(controlDebugLogBlacklist, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);
					if (resultGetTextLength >= 0) {
						char* blacklistText = new char[resultGetTextLength + 1];
						LRESULT resultGetText = SendMessageA(controlDebugLogBlacklist, WM_GETTEXT, (WPARAM)(resultGetTextLength + 1), (LPARAM)blacklistText);
						if (resultGetText == resultGetTextLength) {
							XllnParseDebugLogTraceBlacklist(blacklistText);
						}
						delete[] blacklistText;
						blacklistText = 0;
					}
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_BTN_PAUSE:
				case XLLNControlsMessageNumbers::DEBUG_BTN_UNPAUSE: {
					pause_visible_updates = !pause_visible_updates;
					ShowWindow(GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_BTN_PAUSE), pause_visible_updates ? SW_HIDE : SW_SHOWNORMAL);
					ShowWindow(GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_BTN_UNPAUSE), pause_visible_updates ? SW_SHOWNORMAL : SW_HIDE);
					break;
				}
#define XLLN_CHK_DBG_TOGGLE(checkbox_menu_btn_id, log_level_flag) {\
bool checked = IsDlgButtonChecked(xlln_hwnd_debug_log, checkbox_menu_btn_id) != BST_CHECKED;\
CheckDlgButton(xlln_hwnd_debug_log, checkbox_menu_btn_id, checked ? BST_CHECKED : BST_UNCHECKED);\
xlln_debug_log_level = checked ? (xlln_debug_log_level | log_level_flag) : (xlln_debug_log_level & ~(log_level_flag));\
}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLIVE: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLIVE, XLLN_LOG_CONTEXT_XLIVE);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN, XLLN_LOG_CONTEXT_XLIVELESSNESS);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN_MOD: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN_MOD, XLLN_LOG_CONTEXT_XLLN_MODULE);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_OTHER: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_OTHER, XLLN_LOG_CONTEXT_OTHER);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_TITLE: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_TITLE, XLLN_LOG_CONTEXT_TITLE);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_TRACE: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_TRACE, XLLN_LOG_LEVEL_TRACE);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_DEBUG: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_DEBUG, XLLN_LOG_LEVEL_DEBUG);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_INFO: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_INFO, XLLN_LOG_LEVEL_INFO);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_WARN: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_WARN, XLLN_LOG_LEVEL_WARN);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_ERROR: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_ERROR, XLLN_LOG_LEVEL_ERROR);
					break;
				}
				case XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_FATAL: {
					XLLN_CHK_DBG_TOGGLE(XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_FATAL, XLLN_LOG_LEVEL_FATAL);
					break;
				}
#undef XLLN_CHK_DBG_TOGGLE
			}
			return 0;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_CREATE: {
			
			CreateWindowA(WC_BUTTONA, "Pause", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 12, 80, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_BTN_PAUSE, xlln_hModule, NULL);
			CreateWindowA(WC_BUTTONA, "Unpause", WS_CHILD | WS_TABSTOP,
				10, 12, 80, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_BTN_UNPAUSE, xlln_hModule, NULL);
			
			const int checkboxOffsetY = 5;
			const int checkboxOffsetX = 105;
			
			CreateWindowA(WC_BUTTONA, "XLIVE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX, checkboxOffsetY, 65, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLIVE, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "XLIVELESSNESS", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 65, checkboxOffsetY, 140, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "XLLN MODULE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 205, checkboxOffsetY, 130, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN_MOD, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "TITLE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 335, checkboxOffsetY, 65, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_TITLE, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "OTHER", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 400, checkboxOffsetY, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_OTHER, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "TRACE", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_TRACE, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "DEBUG", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 80, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_DEBUG, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "INFO", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 165, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_INFO, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "WARN", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 235, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_WARN, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "ERROR", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 315, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_ERROR, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "FATAL", BS_CHECKBOX | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				checkboxOffsetX + 400, checkboxOffsetY + 20, 70, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_FATAL, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Update Blacklist", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 48, 150, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_BTN_BLACKLIST_UPDATE, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				165, 50, 615, 22, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_TBX_BLACKLIST, xlln_hModule, NULL);
			
			{
				HWND controlDebugLog = CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_TABSTOP | WS_HSCROLL,
					5, 76, 775, 471, hWnd, (HMENU)XLLNControlsMessageNumbers::DEBUG_TBX_LOG, xlln_hModule, NULL);
				uint32_t tabWidth = 4*4;
				SendMessageW(controlDebugLog, EM_SETTABSTOPS, (WPARAM)1, (LPARAM)&tabWidth);
			}
			
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

static DWORD WINAPI XllnThreadWndDebugLog(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowDebugLogClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Debug Log #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_debug_log = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, hwdParent, 0, hModule, NULL);
		
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
	
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLIVE, (xlln_debug_log_level & XLLN_LOG_CONTEXT_XLIVE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN, (xlln_debug_log_level & XLLN_LOG_CONTEXT_XLIVELESSNESS) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_XLLN_MOD, (xlln_debug_log_level & XLLN_LOG_CONTEXT_XLLN_MODULE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_OTHER, (xlln_debug_log_level & XLLN_LOG_CONTEXT_OTHER) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_CTX_TITLE, (xlln_debug_log_level & XLLN_LOG_CONTEXT_TITLE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_TRACE, (xlln_debug_log_level & XLLN_LOG_LEVEL_TRACE) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_DEBUG, (xlln_debug_log_level & XLLN_LOG_LEVEL_DEBUG) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_INFO, (xlln_debug_log_level & XLLN_LOG_LEVEL_INFO) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_WARN, (xlln_debug_log_level & XLLN_LOG_LEVEL_WARN) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_ERROR, (xlln_debug_log_level & XLLN_LOG_LEVEL_ERROR) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_CHK_DBG_LVL_FATAL, (xlln_debug_log_level & XLLN_LOG_LEVEL_FATAL) ? BST_CHECKED : BST_UNCHECKED);
	
	PostMessageW(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::EVENT_DEBUG_TBX_BLACKLIST_REFRESH, 0, 0);
	
	if (xlln_debug) {
		XllnShowWindow(XllnShowType::DEBUG_LOG_SHOW);
	}
	else {
		pause_visible_updates = true;
		ShowWindow(GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_BTN_PAUSE), pause_visible_updates ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::DEBUG_BTN_UNPAUSE), pause_visible_updates ? SW_SHOWNORMAL : SW_HIDE);
		
		ShowWindow(xlln_hwnd_debug_log, SW_HIDE);
	}
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::DEBUG_TBX_LOG
		, XLLNControlsMessageNumbers::DEBUG_TBX_BLACKLIST
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_hwnd_debug_log, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		
		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_debug_log, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndDebugLog()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(0, NULL, XllnThreadWndDebugLog, (void*)xlln_hModule, NULL, NULL);
	
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

uint32_t UninitXllnWndDebugLog()
{
	SendMessage(xlln_hwnd_debug_log, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
