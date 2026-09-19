#include <winsock2.h>
#include <Windows.h>
#include "./wnd-input-box.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xrender.hpp"
#include "./xlln.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "./debug-log.hpp"
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_input_box = 0;
static uint32_t xlln_input_box_thread_id = 0;
static const uint32_t xlln_input_box_button_height = 25;
static HFONT xlln_input_box_title_font = 0;
static XOVERLAPPED* xlln_input_box_xoverlapped = 0;
static wchar_t* xlln_input_box_result = 0;
static size_t xlln_input_box_result_length = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

static void UpdateContentLayout(uint32_t width, uint32_t height)
{
	uint32_t itemWidth = width - 10 - 10;
	uint32_t currentHeight = 0;
	
	HWND textboxTitle = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_TITLE);
	MoveWindow(textboxTitle, 10, (currentHeight += 10), itemWidth, 57, TRUE);
	
	HWND textboxDescription = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_DESCRIPTION);
	MoveWindow(textboxDescription, 10, (currentHeight += 57 + 4), itemWidth, 48, TRUE);
	
	HWND textboxResult = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT);
	MoveWindow(textboxResult, 10, (currentHeight += 48 + 20), itemWidth, 20, TRUE);
	
	HWND textboxSubmit = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_SUBMIT);
	MoveWindow(textboxSubmit, 10, (currentHeight += 20 + 10), 180, xlln_input_box_button_height, TRUE);
	
	HWND textboxCancel = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_CANCEL);
	MoveWindow(textboxCancel, width - 10 - 180, currentHeight, 180, xlln_input_box_button_height, TRUE);
}

uint32_t XllnWndInputBoxOpen(const wchar_t* title, const wchar_t* description, const wchar_t* default_text, wchar_t* result_text, size_t result_text_length, XOVERLAPPED* xoverlapped)
{
	if (!xlln_hwnd_input_box) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s Has not been initialised yet."
			, __func__
		);
		return ERROR_NOT_READY;
	}
	if (result_text_length < 2) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s result_text_length (%u) is too small. Must be at least 2 in length."
			, __func__
			, result_text_length
		);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	
	if (xlln_input_box_xoverlapped) {
		xlln_input_box_xoverlapped->InternalLow = ERROR_CANCELLED;
		xlln_input_box_xoverlapped->InternalHigh = ERROR_CANCELLED;
		xlln_input_box_xoverlapped->dwExtendedError = ERROR_CANCELLED;
		
		Check_Overlapped(xlln_input_box_xoverlapped);
	}
	
	xlln_input_box_xoverlapped = xoverlapped;
	xlln_input_box_result = result_text;
	xlln_input_box_result_length = result_text_length;
	
	memset(xlln_input_box_result, 0, sizeof(wchar_t) * result_text_length);
	
	// Set input textbox character limit.
	SendDlgItemMessage(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT, EM_LIMITTEXT, result_text_length - 1, 0);
	
	// IDK WHY I have to copy the string for this and the others below to work. I've tried SendMessageW even! It just randomly fails with no error in GetLastError. It could be some OS problem with accessing the string from another thread's stack space?
	wchar_t* clonedTitle = CloneString(title ? title : L"Input Text");
	SetDlgItemTextW(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_TITLE, clonedTitle);
	delete[] clonedTitle;
	clonedTitle = 0;
	
	wchar_t* clonedDescription = CloneString(description ? description : L"Please enter the desired text below:");
	SetDlgItemTextW(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_DESCRIPTION, clonedDescription);
	delete[] clonedDescription;
	clonedDescription = 0;
	
	wchar_t* clonedDefaultText = CloneString(default_text ? default_text : L"");
	SetDlgItemTextW(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT, clonedDefaultText);
	delete[] clonedDefaultText;
	clonedDefaultText = 0;
	
	HWND buttonFocus = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT);
	
	RECT rect;
	GetClientRect(xlln_hwnd_input_box, &rect);
	UpdateContentLayout(rect.right - rect.left, rect.bottom - rect.top);
	
	XllnShowWindow(XllnShowType::INPUT_BOX_SHOW);
	
	if (buttonFocus) {
		uint32_t currentThreadId = GetCurrentThreadId();
		// Attach to the main thread as secondary threads cannot use GUI functions.
		AttachThreadInput(currentThreadId, xlln_input_box_thread_id, TRUE);
		
		SetFocus(buttonFocus);
		
		AttachThreadInput(currentThreadId, xlln_input_box_thread_id, FALSE);
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
			HDC hdc = BeginPaint(xlln_hwnd_input_box, &ps);
			
			EndPaint(xlln_hwnd_input_box, &ps);
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				if (xlln_input_box_xoverlapped) {
					XOVERLAPPED* xoverlapped = xlln_input_box_xoverlapped;
					xlln_input_box_xoverlapped = 0;
					
					xoverlapped->InternalLow = ERROR_CANCELLED;
					xoverlapped->InternalHigh = ERROR_CANCELLED;
					xoverlapped->dwExtendedError = ERROR_CANCELLED;
					
					Check_Overlapped(xoverlapped);
				}
				
				XllnShowWindow(XllnShowType::INPUT_BOX_HIDE);
				
				SendMessage(GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_SUBMIT), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
				SendMessage(GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_CANCEL), BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
				
				return 0;
			}
			break;
		}
		case WM_COMMAND: {
			switch (wParam) {
				case XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_SUBMIT:
				case XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_CANCEL: {
					HWND buttonPressed = (HWND)lParam;
					SendMessage(buttonPressed, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
					
					if (xlln_input_box_xoverlapped) {
						XOVERLAPPED* xoverlapped = xlln_input_box_xoverlapped;
						wchar_t* result_text = xlln_input_box_result;
						size_t result_text_length = xlln_input_box_result_length;
						xlln_input_box_xoverlapped = 0;
						xlln_input_box_result = 0;
						xlln_input_box_result_length = 0;
						
						if (wParam == XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_SUBMIT && result_text) {
							if (result_text_length > 1) {
								HWND controlResult = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT);
								size_t resultLength = GetWindowTextLength(controlResult) + 1;
								if (result_text_length < resultLength) {
									resultLength = result_text_length;
								}
								
								GetDlgItemTextW(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT, result_text, (int32_t)resultLength);
								// If string is truncated, there is a Win32 GetDlgItemTextW bug where (unlike the GetDlgItemTextA variant) it does not add null termination.
								result_text[resultLength - 1] = 0;
							}
							
							xoverlapped->InternalLow = ERROR_SUCCESS;
							xoverlapped->InternalHigh = ERROR_SUCCESS;
							xoverlapped->dwExtendedError = ERROR_SUCCESS;
						}
						else {
							xoverlapped->InternalLow = ERROR_CANCELLED;
							xoverlapped->InternalHigh = ERROR_CANCELLED;
							xoverlapped->dwExtendedError = ERROR_CANCELLED;
						}
						
						Check_Overlapped(xoverlapped);
					}
					
					XllnShowWindow(XllnShowType::INPUT_BOX_HIDE);
					
					break;
				}
			}
			
			return 0;
		}
		//case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC: {
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			return (INT_PTR)CreateSolidBrush(0x00C8C8C8);
		}
		case WM_CREATE: {
			CreateWindowA(WC_EDITA, "Title", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				10, 10, 464, 57, hWnd, (HMENU)XLLNControlsMessageNumbers::INPUT_BOX_TBX_TITLE, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, "Description", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
				10, 71, 464, 48, hWnd, (HMENU)XLLNControlsMessageNumbers::INPUT_BOX_TBX_DESCRIPTION, xlln_hModule, NULL);
			
			CreateWindowA(WC_EDITA, "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
				10, 139, 464, 20, hWnd, (HMENU)XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Submit", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_NOTIFY,
				10, 169, 180, xlln_input_box_button_height, hWnd, (HMENU)XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_SUBMIT, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_NOTIFY,
				294, 169, 180, xlln_input_box_button_height, hWnd, (HMENU)XLLNControlsMessageNumbers::INPUT_BOX_BTN_BTN_CANCEL, xlln_hModule, NULL);
			
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

static DWORD WINAPI XllnThreadWndInputBox(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_input_box_thread_id = GetCurrentThreadId();
	
	const wchar_t* windowclassname = L"XLLNDLLWindowInputBoxClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Input Box #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_input_box = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 240, hwdParent, 0, hModule, NULL);
		
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
		xlln_input_box_title_font = CreateFontIndirect(&logfont);
	}
	
	HWND textboxTitle = GetDlgItem(xlln_hwnd_input_box, XLLNControlsMessageNumbers::INPUT_BOX_TBX_TITLE);
	SendMessage(textboxTitle, WM_SETFONT, (WPARAM)xlln_input_box_title_font, TRUE);
	
	ShowWindow(xlln_hwnd_input_box, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	const uint32_t textBoxes[] = {
		XLLNControlsMessageNumbers::INPUT_BOX_TBX_TITLE
		, XLLNControlsMessageNumbers::INPUT_BOX_TBX_DESCRIPTION
		, XLLNControlsMessageNumbers::INPUT_BOX_TBX_RESULT
	};
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (WndTextboxMessageHandling(&msg, xlln_hwnd_input_box, textBoxes, sizeof(textBoxes) / sizeof(textBoxes[0]))) {
			continue;
		}
		// Handle tab ordering
		if (!IsDialogMessage(xlln_hwnd_input_box, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	DeleteObject(xlln_input_box_title_font);
	xlln_input_box_title_font = 0;
	
	xlln_hwnd_input_box = 0;
	xlln_input_box_thread_id = 0;
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndInputBox()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(NULL, 0, XllnThreadWndInputBox, (void*)xlln_hModule, 0, NULL);
	
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

uint32_t UninitXllnWndInputBox()
{
	SendMessage(xlln_hwnd_input_box, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
