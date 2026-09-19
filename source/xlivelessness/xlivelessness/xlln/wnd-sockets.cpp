#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "./wnd-sockets.hpp"
#include "./xlln.hpp"
#include "./xlln-network.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xrender.hpp"
#include "./debug-log.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_sockets = 0;
static HWND hwndListView = 0;
static HANDLE xlln_window_create_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_initialised_event = INVALID_HANDLE_VALUE;
static HANDLE xlln_window_destroy_event = INVALID_HANDLE_VALUE;

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
	/*{
		LV_ITEM item;
		item.mask = LVIF_TEXT;
		item.iItem = 0;
		item.iIndent = 0;
		item.iSubItem = 1;
		item.state = 0;
		item.cColumns = 0;
		item.pszText = (wchar_t*)L"Toothbrush";
		ListView_SetItem(hwndList, &item);
	}*/
}

void XllnWndSocketsInvalidateSockets()
{
	InvalidateRect(xlln_hwnd_sockets, NULL, FALSE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			HWND controlSocketsList = GetDlgItem(xlln_hwnd_sockets, XLLNControlsMessageNumbers::SOCKETS_LST_SOCKETS);
			MoveWindow(controlSocketsList, 10, 30, width - 20, height - 40, TRUE);
			
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_sockets, &ps);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			
			LOGFONT logfont;
			memset(&logfont, 0, sizeof(logfont));
			logfont.lfCharSet = DEFAULT_CHARSET;
			logfont.lfWeight = 400;
			logfont.lfHeight = -12;
			HFONT hNewFont = CreateFontIndirect(&logfont);
			HFONT hOldFont = (HFONT)SelectObject(hdc, hNewFont);
			
			HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
			SelectObject(hdc, hBrush);
			RECT bgRect;
			GetClientRect(hWnd, &bgRect);
			HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
			FillRgn(hdc, bgRgn, hBrush);
			DeleteObject(bgRgn);
			DeleteObject(hBrush);
			
			if (xlive_netsocket_abort) {
				SetTextColor(hdc, RGB(0xFF, 0, 0));
				const char* labelToUse = "Title Sockets DISABLED.";
				TextOutA(hdc, 10, 10, labelToUse, (uint32_t)strlen(labelToUse));
			}
			else {
				SetTextColor(hdc, RGB(0, 0xB0, 0));
				const char* labelToUse = "Title Sockets Enabled.";
				TextOutA(hdc, 10, 10, labelToUse, (uint32_t)strlen(labelToUse));
			}
			
			if (xlln_network_instance_port) {
				SetTextColor(hdc, RGB(0, 0xB0, 0));
				char* textLabel = FormMallocString(
					"XLLN Shared Port: %hu, Instance Port: %hu."
					, xlln_network_instance_base_port
					, xlln_network_instance_port
				);
				TextOutA(hdc, 150, 10, textLabel, (uint32_t)strlen(textLabel));
				free(textLabel);
				textLabel = 0;
			}
			else {
				SetTextColor(hdc, RGB(0xFF, 0, 0));
				const char* textLabel = "Real Networking DISABLED.";
				TextOutA(hdc, 150, 10, textLabel, (uint32_t)strlen(textLabel));
			}
			
			if (xlln_network_instance_port) {
				EnterCriticalSection(&xlln_critsec_network_send);
				
				size_t sendQueue = xlln_network_send_queue.size();
				
				LeaveCriticalSection(&xlln_critsec_network_send);
				
				if (sendQueue > 10) {
					SetTextColor(hdc, RGB(0xFF, 0, 0));
				}
				else {
					SetTextColor(hdc, RGB(0, 0xB0, 0));
				}
				
				char* textLabel = FormMallocString(
					"XLLN Queued Send: %zu."
					, sendQueue
				);
				TextOutA(hdc, 430, 10, textLabel, (uint32_t)strlen(textLabel));
				free(textLabel);
				textLabel = 0;
			}
			
			SetTextColor(hdc, RGB(0, 0, 0));
			
			
			SelectObject(hdc, hOldFont);
			DeleteObject(hNewFont);
			
			EndPaint(xlln_hwnd_sockets, &ps);
			
			ListView_DeleteAllItems(hwndListView);
			{
				EnterCriticalSection(&xlive_critsec_sockets);
				
				std::vector<XLIVE_TITLE_SOCKET*> socketsOrderedByPort;
				for (auto const &itrTitleSocket : xlive_title_sockets) {
					XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket.second;
					size_t j = 0;
					for (; j < socketsOrderedByPort.size(); j++) {
						XLIVE_TITLE_SOCKET* nextSocketInfo = socketsOrderedByPort.at(j);
						if (nextSocketInfo->portRequested > titleSocket->portRequested) {
							socketsOrderedByPort.insert(socketsOrderedByPort.begin() + j, titleSocket);
							break;
						}
						else if (nextSocketInfo->portRequested == 0 && titleSocket->portRequested == 0 && nextSocketInfo->portActual > titleSocket->portActual) {
							socketsOrderedByPort.insert(socketsOrderedByPort.begin() + j, titleSocket);
							break;
						}
					}
					if (j == socketsOrderedByPort.size()) {
						socketsOrderedByPort.push_back(titleSocket);
					}
				}
				
				for (uint32_t i = 0; i < socketsOrderedByPort.size(); i++) {
					XLIVE_TITLE_SOCKET* titleSocket = socketsOrderedByPort[i];
					
					wchar_t* textItemLabel;
					uint32_t j = 0;
					CreateItem(hwndListView, i);
#define AddSocketItemColumn(format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); ListView_SetItemText(hwndListView, i, j++, textItemLabel); free(textItemLabel)
					
					AddSocketItemColumn(L"0x%zx", titleSocket->handle);
					AddSocketItemColumn(L"%d", titleSocket->type);
					AddSocketItemColumn(L"%hs(%d)"
						, titleSocket->protocol == IPPROTO_UDP ? "UDP" : (titleSocket->protocol == IPPROTO_TCP ? "TCP" : "")
						, titleSocket->protocol
					);
					AddSocketItemColumn(L"%hu", titleSocket->portRequested);
					AddSocketItemColumn(L"%hu", titleSocket->portActual);
					textItemLabel = 0;
					if (titleSocket->protocol == IPPROTO_UDP) {
						if (titleSocket->hasBinded && titleSocket->hasShutdown == XTS_SHUTDOWN_STATE::XTS_SS_STILL_ACTIVE) {
							textItemLabel = (wchar_t*)L"Binded";
						}
					}
					else {
						if (titleSocket->tcpIsListening) {
							textItemLabel = (wchar_t*)L"Listening";
						}
						else if (titleSocket->tcpIsConnected) {
							textItemLabel = (wchar_t*)L"Connected";
						}
						else if (titleSocket->tcpIsConnecting) {
							textItemLabel = (wchar_t*)L"Connecting";
						}
						else if (titleSocket->tcpIsAccepting) {
							textItemLabel = (wchar_t*)L"Accepting";
						}
					}
					if (!textItemLabel) {
						textItemLabel = (wchar_t*)L"Inactive";
					}
					ListView_SetItemText(hwndListView, i, j++, textItemLabel);
					AddSocketItemColumn(L"%hs", titleSocket->isVdpProtocol ? "Y" : "");
					AddSocketItemColumn(L"%hs", titleSocket->udpIsBroadcastEnabled ? "Y" : "");
					AddSocketItemColumn(L"%zu", titleSocket->recvPacketQueue.size());
					
#undef AddSocketItemColumn
				}
				
				LeaveCriticalSection(&xlive_critsec_sockets);
			}
			
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::SOCKETS_HIDE);
				return 0;
			}
			break;
		}
		case WM_NOTIFY: {
			switch (LOWORD(wParam)) {
				case XLLNControlsMessageNumbers::SOCKETS_LST_SOCKETS: {
					
					NMHDR* nmhdr = (NMHDR*)lParam;
					//if (nmhdr->code == NM_DBLCLK) {}
					//else if (nmhdr->code == NM_RETURN) {}
					
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
			hwndListView = CreateWindowA(WC_LISTVIEWA, "",
				WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP,
				10, 30, 640, 110,
				hWnd, (HMENU)XLLNControlsMessageNumbers::SOCKETS_LST_SOCKETS, xlln_hModule, 0);
			
			uint32_t j = 0;
			CreateColumn(hwndListView, ++j, L"Title Socket", 90);
			CreateColumn(hwndListView, ++j, L"Type", 40);
			CreateColumn(hwndListView, ++j, L"Protocol", 60);
			CreateColumn(hwndListView, ++j, L"Port", 50);
			CreateColumn(hwndListView, ++j, L"Bind Port", 65);
			CreateColumn(hwndListView, ++j, L"Status", 90);
			CreateColumn(hwndListView, ++j, L"was VDP", 60);
			CreateColumn(hwndListView, ++j, L"Broadcast", 70);
			CreateColumn(hwndListView, ++j, L"Recv Queue", 80);
			
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

static DWORD WINAPI XllnThreadWndSockets(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowSocketsClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Sockets #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_sockets = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 650, 200, hwdParent, 0, hModule, NULL);
		
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
	
	ShowWindow(xlln_hwnd_sockets, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// Do message processing here.
		
		// Handle tab ordering.
		bool handleListViewReturnKey = (msg.hwnd == hwndListView && LOWORD(msg.message) == WM_KEYDOWN && msg.wParam == VK_RETURN);
		if (handleListViewReturnKey || !IsDialogMessage(xlln_hwnd_sockets, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndSockets()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(0, NULL, XllnThreadWndSockets, (void*)xlln_hModule, NULL, NULL);
	
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

uint32_t UninitXllnWndSockets()
{
	SendMessage(xlln_hwnd_sockets, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
