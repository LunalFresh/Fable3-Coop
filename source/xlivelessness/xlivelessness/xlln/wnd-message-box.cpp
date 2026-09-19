#include <winsock2.h>
#include <Windows.h>
#include "./wnd-message-box.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xrender.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "./debug-log.hpp"
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_message_box = 0;
static uint32_t xlln_message_box_thread_id = 0;
static const wchar_t* xlln_message_box_icon = 0;
static MESSAGEBOX_RESULT* xlln_message_box_result = 0;
static XOVERLAPPED* xlln_message_box_xoverlapped = 0;
static const uint32_t xlln_message_box_button_height = 25;
static uint32_t xlln_message_box_button_count = 3;
static HFONT xlln_message_box_title_font = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

static void UpdateContentLayout(uint32_t width, uint32_t height)
{
	uint32_t buttonPosY = height - xlln_message_box_button_height - 10;
	uint32_t buttonWidth = (width - (10 * (1 + xlln_message_box_button_count))) / xlln_message_box_button_count;
	
	HWND button0 = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0);
	MoveWindow(button0, 10, buttonPosY, buttonWidth, xlln_message_box_button_height, TRUE);
	
	HWND button1 = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1);
	if (xlln_message_box_button_count >= 2) {
		MoveWindow(button1, 20 + buttonWidth, buttonPosY, buttonWidth, xlln_message_box_button_height, TRUE);
		ShowWindow(button1, SW_SHOWNORMAL);
	}
	else {
		ShowWindow(button1, SW_HIDE);
	}
	
	HWND button2 = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2);
	if (xlln_message_box_button_count >= 3) {
		MoveWindow(button2, 30 + (buttonWidth * 2), buttonPosY, buttonWidth, xlln_message_box_button_height, TRUE);
		ShowWindow(button2, SW_SHOWNORMAL);
	}
	else {
		ShowWindow(button2, SW_HIDE);
	}
	
	HWND textboxTitle = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_TBX_TITLE);
	if (xlln_message_box_icon) {
		uint32_t textboxTitleWidth = 10 + 64 + 10 + 10;
		if (textboxTitleWidth + 20 > width) {
			textboxTitleWidth = 20;
		}
		else {
			textboxTitleWidth = width - textboxTitleWidth;
		}
		MoveWindow(textboxTitle, 10 + 64 + 10, 10, textboxTitleWidth, 64, TRUE);
	}
	else {
		MoveWindow(textboxTitle, 10, 10, width - 20, 64, TRUE);
	}
	
	HWND textboxDescription = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_TBX_DESCRIPTION);
	uint32_t textboxDescriptionHeight = 90 + 10 + 10 + xlln_message_box_button_height;
	if (textboxDescriptionHeight + 20 > height) {
		textboxDescriptionHeight = 20;
	}
	else {
		textboxDescriptionHeight = height - textboxDescriptionHeight;
	}
	MoveWindow(textboxDescription, 10, 90, width - 20, textboxDescriptionHeight, TRUE);
}

uint32_t XllnWndMessageBoxOpen(const wchar_t* title, const wchar_t* description, uint32_t icon_type, const wchar_t** button_labels, uint32_t button_count, uint32_t focus_button, MESSAGEBOX_RESULT* messagebox_result, XOVERLAPPED* xoverlapped)
{
	if (!xlln_hwnd_message_box) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Has not been initialised yet."
			, __func__
		);
		return ERROR_NOT_READY;
	}
	
	if (xlln_message_box_xoverlapped) {
		xlln_message_box_xoverlapped->InternalLow = ERROR_CANCELLED;
		xlln_message_box_xoverlapped->InternalHigh = ERROR_CANCELLED;
		xlln_message_box_xoverlapped->dwExtendedError = ERROR_CANCELLED;
		
		Check_Overlapped(xlln_message_box_xoverlapped);
	}
	
	if (button_count > XMB_MAXBUTTONS) {
		button_count = XMB_MAXBUTTONS;
	}
	
	switch(icon_type) {
		case XMB_ERRORICON: {
			xlln_message_box_icon = IDI_ERROR;
			break;
		}
		case XMB_WARNINGICON: {
			xlln_message_box_icon = IDI_WARNING;
			break;
		}
		case XMB_ALERTICON: {
			xlln_message_box_icon = IDI_INFORMATION;
			break;
		}
		default: {
			xlln_message_box_icon = 0;
			break;
		}
	}
	
	xlln_message_box_result = messagebox_result;
	xlln_message_box_xoverlapped = xoverlapped;
	xlln_message_box_button_count = button_count;
	
	// IDK WHY I have to copy the string for this and the others below to work. I've tried SendMessageW even! It just randomly fails with no error in GetLastError. It could be some OS problem with accessing the string from another thread's stack space?
	wchar_t* clonedTitle = CloneString(title);
	SetDlgItemTextW(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_TBX_TITLE, clonedTitle);
	delete[] clonedTitle;
	clonedTitle = 0;
	
	wchar_t* clonedDescription = CloneString(description);
	SetDlgItemTextW(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_TBX_DESCRIPTION, clonedDescription);
	delete[] clonedDescription;
	clonedDescription = 0;
	
	HWND buttonFocus = 0;
	
	for (uint32_t iButton = 0; iButton < button_count; iButton++) {
		uint32_t windowButtonId = 0;
		switch (iButton) {
			case 0: {
				windowButtonId = XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0;
				break;
			}
			case 1: {
				windowButtonId = XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1;
				break;
			}
			case 2: {
				windowButtonId = XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2;
				break;
			}
		}
		if (windowButtonId) {
			wchar_t* clonedLabel = CloneString(button_labels[iButton]);
			SetDlgItemTextW(xlln_hwnd_message_box, windowButtonId, clonedLabel);
			delete[] clonedLabel;
			clonedLabel = 0;
			
			if (iButton == focus_button) {
				buttonFocus = GetDlgItem(xlln_hwnd_message_box, windowButtonId);
			}
		}
	}
	
	RECT rect;
	GetClientRect(xlln_hwnd_message_box, &rect);
	UpdateContentLayout(rect.right - rect.left, rect.bottom - rect.top);
	
	XllnShowWindow(XllnShowType::MESSAGE_BOX_SHOW);
	
	if (buttonFocus) {
		uint32_t currentThreadId = GetCurrentThreadId();
		// Attach to the main thread as secondary threads cannot use GUI functions.
		AttachThreadInput(currentThreadId, xlln_message_box_thread_id, TRUE);
		
		SetFocus(buttonFocus);
		
		AttachThreadInput(currentThreadId, xlln_message_box_thread_id, FALSE);
	}
	
	return ERROR_IO_PENDING;
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			UpdateContentLayout(width, height);
			
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_message_box, &ps);
			
			if (xlln_message_box_icon) {
				HICON hIcon = LoadIconW(NULL, xlln_message_box_icon);
				DrawIconEx(hdc, 10, 10, hIcon, 64, 64, 0, 0, DI_NORMAL);
			}
			
			EndPaint(xlln_hwnd_message_box, &ps);
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				if (xlln_message_box_xoverlapped) {
					xlln_message_box_result = 0;
					
					XOVERLAPPED* xoverlapped = xlln_message_box_xoverlapped;
					xlln_message_box_xoverlapped = 0;
					
					xoverlapped->InternalLow = ERROR_CANCELLED;
					xoverlapped->InternalHigh = ERROR_CANCELLED;
					xoverlapped->dwExtendedError = ERROR_CANCELLED;
					
					Check_Overlapped(xoverlapped);
				}
				
				XllnShowWindow(XllnShowType::MESSAGE_BOX_HIDE);
				
				SendMessage(GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
				SendMessage(GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
				SendMessage(GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
				
				return 0;
			}
			break;
		}
		case WM_COMMAND: {
			switch (wParam) {
				case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0:
				case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1:
				case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2: {
					if (xlln_message_box_result) {
						switch (wParam) {
							case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0: {
								xlln_message_box_result->dwButtonPressed = 0;
								break;
							}
							case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1: {
								xlln_message_box_result->dwButtonPressed = 1;
								break;
							}
							case XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2: {
								xlln_message_box_result->dwButtonPressed = 2;
								break;
							}
						}
						
						xlln_message_box_result = 0;
					}
					
					HWND buttonPressed = (HWND)lParam;
					SendMessage(buttonPressed, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
					
					if (xlln_message_box_xoverlapped) {
						XOVERLAPPED* xoverlapped = xlln_message_box_xoverlapped;
						xlln_message_box_xoverlapped = 0;
						
						xoverlapped->InternalLow = ERROR_SUCCESS;
						xoverlapped->InternalHigh = ERROR_SUCCESS;
						xoverlapped->dwExtendedError = ERROR_SUCCESS;
						
						Check_Overlapped(xoverlapped);
					}
					
					XllnShowWindow(XllnShowType::MESSAGE_BOX_HIDE);
					
					break;
				}
			}
			
			return 0;
		}
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_CREATE: {
			CreateWindowA(WC_EDITA, "Title", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				90, 10, 480, 64, hWnd, (HMENU)XLLNControlsMessageNumbers::MSG_BOX_TBX_TITLE, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, "Description", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				10, 90, 560, 100, hWnd, (HMENU)XLLNControlsMessageNumbers::MSG_BOX_TBX_DESCRIPTION, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Button 0", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_NOTIFY,
				10, 200, 180, xlln_message_box_button_height, hWnd, (HMENU)XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_0, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Button 1", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_NOTIFY,
				200, 200, 180, xlln_message_box_button_height, hWnd, (HMENU)XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_1, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Button 2", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_NOTIFY,
				390, 200, 180, xlln_message_box_button_height, hWnd, (HMENU)XLLNControlsMessageNumbers::MSG_BOX_BTN_BTN_2, xlln_hModule, NULL);
			
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

static DWORD WINAPI XllnThreadWndMessageBox(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_message_box_thread_id = GetCurrentThreadId();
	
	const wchar_t* windowclassname = L"XLLNDLLWindowMessageBoxClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Message Box #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_message_box = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 596, 270, hwdParent, 0, hModule, NULL);
		
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
		xlln_message_box_title_font = CreateFontIndirect(&logfont);
	}
	
	HWND textboxTitle = GetDlgItem(xlln_hwnd_message_box, XLLNControlsMessageNumbers::MSG_BOX_TBX_TITLE);
	SendMessage(textboxTitle, WM_SETFONT, (WPARAM)xlln_message_box_title_font, TRUE);
	
	ShowWindow(xlln_hwnd_message_box, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::MSG_BOX_TBX_TITLE
		, XLLNControlsMessageNumbers::MSG_BOX_TBX_DESCRIPTION
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_hwnd_message_box, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_message_box, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	DeleteObject(xlln_message_box_title_font);
	xlln_message_box_title_font = 0;
	
	xlln_hwnd_message_box = 0;
	xlln_message_box_thread_id = 0;
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndMessageBox()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(NULL, 0, XllnThreadWndMessageBox, (void*)xlln_hModule, 0, NULL);
	
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

uint32_t UninitXllnWndMessageBox()
{
	SendMessage(xlln_hwnd_message_box, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
