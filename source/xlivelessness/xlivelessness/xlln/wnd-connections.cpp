#include <winsock2.h>
#include <Windows.h>
#include "../xlive/xdefs.hpp"
#include "../xlive/xnet.hpp"
#include "./wnd-connections.hpp"
#include "./debug-log.hpp"
#include "./xlln.hpp"
#include "./xlln-network.hpp"
#include "../resource.h"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xrender.hpp"
#include <stdint.h>
#include <time.h>
#include <CommCtrl.h>

HWND xlln_hwnd_connections = 0;
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

void XllnWndConnectionsInvalidateConnections()
{
	InvalidateRect(xlln_hwnd_connections, NULL, FALSE);
}

static LRESULT CALLBACK DLLWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		case WM_SIZE: {
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			
			HWND controlSocketsList = GetDlgItem(xlln_hwnd_connections, XLLNControlsMessageNumbers::CONNS_LST_CONNECTIONS);
			MoveWindow(controlSocketsList, 10, 40, width - 20, height - 50, TRUE);
			
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(xlln_hwnd_connections, &ps);
			SetTextColor(hdc, RGB(0, 0, 0));
			SetBkColor(hdc, 0x00C8C8C8);
			
			HBRUSH hBrush = CreateSolidBrush(0x00C8C8C8);
			SelectObject(hdc, hBrush);
			RECT bgRect;
			GetClientRect(hWnd, &bgRect);
			HRGN bgRgn = CreateRectRgnIndirect(&bgRect);
			FillRgn(hdc, bgRgn, hBrush);
			DeleteObject(bgRgn);
			DeleteObject(hBrush);
			
			{
				char* textLabel = FormMallocString(
					"Global Instance Id: 0x%08x."
					, xlln_global_instance_id
				);
				TextOutA(hdc, 190, 15, textLabel, (uint32_t)strlen(textLabel));
				free(textLabel);
			}
			
			EndPaint(xlln_hwnd_connections, &ps);
			
			ListView_DeleteAllItems(hwndListView);
			{
				EnterCriticalSection(&xlln_critsec_network_net_entity);
				
				uint32_t i = -1;
				for (auto itrIiToRa : xlln_net_entity_instance_id_to_remote_address) {
					const uint32_t &instanceId = itrIiToRa.first;
					const SOCKADDR_STORAGE &instanceSockaddr = itrIiToRa.second;
					char* sockAddrInfo = GetSockAddrInfo(&instanceSockaddr);
					
					wchar_t* textItemLabel;
					uint32_t j = 0;
					CreateItem(hwndListView, ++i);
#define AddSocketItemColumn(format, ...) textItemLabel = FormMallocString(format, __VA_ARGS__); ListView_SetItemText(hwndListView, i, j++, textItemLabel); free(textItemLabel)
					
					AddSocketItemColumn(L"0x%08x", instanceId);
					AddSocketItemColumn(L"%hs", sockAddrInfo ? sockAddrInfo : "");
					
#undef AddSocketItemColumn
					if (sockAddrInfo) {
						free(sockAddrInfo);
						sockAddrInfo = 0;
					}
				}
				
				LeaveCriticalSection(&xlln_critsec_network_net_entity);
			}
			
			break;
		}
		case WM_SYSCOMMAND: {
			if (wParam == SC_CLOSE) {
				XllnShowWindow(XllnShowType::CONNECTIONS_HIDE);
				return 0;
			}
			break;
		}
		case WM_NOTIFY: {
			switch (LOWORD(wParam)) {
				case XLLNControlsMessageNumbers::CONNS_LST_CONNECTIONS: {
					
					NMHDR* nmhdr = (NMHDR*)lParam;
					//if (nmhdr->code == NM_DBLCLK) {}
					//else if (nmhdr->code == NM_RETURN) {}
					
					break;
				}
			}
			break;
		}
		case WM_COMMAND: {
			if (wParam == XLLNControlsMessageNumbers::CONNS_BTN_REFRESH_CONNS) {
				XllnWndConnectionsInvalidateConnections();
			}
			else if (wParam == XLLNControlsMessageNumbers::CONNS_BTN_DELETE_CONNS) {
				EnterCriticalSection(&xlln_critsec_network_net_entity);
				
				xlln_net_entity_instance_id_to_remote_address.clear();
				
				LeaveCriticalSection(&xlln_critsec_network_net_entity);
				
				XllnWndConnectionsInvalidateConnections();
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
			CreateWindowA(WC_BUTTONA, "Refresh", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				10, 10, 75, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::CONNS_BTN_REFRESH_CONNS, xlln_hModule, NULL);
			
			CreateWindowA(WC_BUTTONA, "Clear", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				100, 10, 75, 25, hWnd, (HMENU)XLLNControlsMessageNumbers::CONNS_BTN_DELETE_CONNS, xlln_hModule, NULL);
			
			hwndListView = CreateWindowA(WC_LISTVIEWA, "",
				WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | WS_TABSTOP,
				10, 40, 400, 310,
				hWnd, (HMENU)XLLNControlsMessageNumbers::CONNS_LST_CONNECTIONS, xlln_hModule, 0);
			
			uint32_t j = 0;
			CreateColumn(hwndListView, ++j, L"Instance ID", 100);
			CreateColumn(hwndListView, ++j, L"Address", 300);
			
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

static DWORD WINAPI XllnThreadWndConnections(void* lpParam)
{
	srand((unsigned int)time(NULL));
	
	xlln_window_create_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	const wchar_t* windowclassname = L"XLLNDLLWindowConnectionsClass";
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
		wchar_t* windowTitle = FormMallocString(L"XLLN Connections #%u v%d.%d.%d.%d", xlln_local_instance_id, DLL_VERSION);
		
		HWND hwdParent = NULL;
		xlln_hwnd_connections = CreateWindowExW(0, windowclassname, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 440, 400, hwdParent, 0, hModule, NULL);
		
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
	
	ShowWindow(xlln_hwnd_connections, SW_HIDE);
	
	SetEvent(xlln_window_initialised_event);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// Do message processing here.
		
		// Handle tab ordering.
		bool handleListViewReturnKey = (msg.hwnd == hwndListView && LOWORD(msg.message) == WM_KEYDOWN && msg.wParam == VK_RETURN);
		if (handleListViewReturnKey || !IsDialogMessage(xlln_hwnd_connections, &msg)) {
			// Translate virtual-key msg into character msg
			TranslateMessage(&msg);
			// Send msg to WindowProcedure(s)
			DispatchMessage(&msg);
		}
	}
	
	SetEvent(xlln_window_destroy_event);
	
	return ERROR_SUCCESS;
}

uint32_t InitXllnWndConnections()
{
	xlln_window_destroy_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_window_initialised_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	
	CreateThread(0, NULL, XllnThreadWndConnections, (void*)xlln_hModule, NULL, NULL);
	
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

uint32_t UninitXllnWndConnections()
{
	SendMessage(xlln_hwnd_connections, WM_CLOSE, (WPARAM)0, (LPARAM)0);
	
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
