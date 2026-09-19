#include <winsock2.h>
#include <Windows.h>
#include "xdefs.hpp"
#include "xrender.hpp"
#include "./xlive.hpp"
#include "./xnotify.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/wnd-achievements.hpp"
#include "../xlln/wnd-stats.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlln/wnd-debug-log.hpp"
#include "../xlln/wnd-connections.hpp"
#include "../xlln/wnd-message-box.hpp"
#include "../xlln/wnd-input-box.hpp"
#include "../xlln/wnd-user-custom-list.hpp"
#include "../xlln/wnd-user-card.hpp"
#include "../xlln/social-overlay.hpp"
#include <chrono>
#include <thread>
#include <d3d11.h>
#include <d3d10.h>
#include <d3d9.h>

bool Initialised_XRender = false;

CRITICAL_SECTION xlive_critsec_fps_limit;
uint32_t xlive_fps_limit = 60;

static std::chrono::system_clock::time_point nextFrame;
static std::chrono::system_clock::duration desiredRenderTime;

static IDirect3DDevice9* device_d3d9 = 0;
static IDirect3DDevice9Ex* device_d3d9ex = 0;
static ID3D10Device* device_d3d10 = 0;
static ID3D11Device* device_d3d11 = 0;

static D3DPRESENT_PARAMETERS* presentation_parameters_d3d9 = 0;
static DXGI_SWAP_CHAIN_DESC* presentation_parameters_d3d10_d3d11 = 0;

CRITICAL_SECTION xlive_critsec_hotkeys;
static HANDLE xlive_xrender_thread_hotkeys_event = INVALID_HANDLE_VALUE;
static bool xlive_xrender_thread_hotkeys_shutdown = false;
static std::thread xlive_xrender_thread_hotkeys;
static uint32_t thread_id_gui = 0;

CRITICAL_SECTION xlln_critsec_guide_ui_handlers;
std::vector<GUIDE_UI_HANDLER_INFO> xlln_guide_ui_handlers;
uint16_t xlive_hotkey_id_guide = VK_HOME;

static bool xlln_xlive_system_ui_open = false;

static DWORD WINAPI ThreadCloseXliveSystemUI(void* lpParam)
{
	// Wait a moment to block any additional window messages before handing back control to the Title.
	// This is because the Title might have issues consuming input correctly and may for example repeat the action of opening the Xlive Guide over and over.
	Sleep(100);
	
	if (!xlln_xlive_system_ui_open && xlive_notify_system_ui_open) {
		XLiveNotifyAddEvent(XN_SYS_UI, 0);
	}
	
	return ERROR_SUCCESS;
}

bool XllnUpdateXliveSystemUIOpenState(bool is_system_ui_open)
{
	if (xlln_xlive_system_ui_open != is_system_ui_open) {
		xlln_xlive_system_ui_open = is_system_ui_open;
		if (xlln_xlive_system_ui_open) {
			XLiveNotifyAddEvent(XN_SYS_UI, 1);
		}
		else {
			CreateThread(0, 0, ThreadCloseXliveSystemUI, (void*)0, 0, 0);
		}
	}
	
	return true;
}

bool XllnUpdateXliveSystemUIOpenState()
{
	bool isSystemUIOpen = XllnIsXliveSystemUIOpen();
	return XllnUpdateXliveSystemUIOpenState(isSystemUIOpen);
}

bool XllnShowWindow(XllnShowType show_type)
{
	if (show_type == XllnShowType::MAIN_SHOW_LOGIN) {
		bool anyGotAutoLogged = false;
		
		// Check if there are any accounts already logged in.
		for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
			if (xlive_local_users[iUser].signin_state != eXUserSigninState_NotSignedIn) {
				anyGotAutoLogged = true;
				break;
			}
		}
		
		// If no accounts are logged in then auto login the ones that have the flag.
		if (!anyGotAutoLogged) {
			for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
				if (xlive_local_users[iUser].auto_login) {
					anyGotAutoLogged = true;
					bool autoLoginChecked = xlive_local_users[iUser].auto_login;
					XLLNLogin(
						iUser
						, ((xlive_local_users[iUser].live_enabled ? 0x01 : 0) + (xlive_local_users[iUser].online_enabled ? 0 : 0x0100))
						, 0
						, (xlive_local_users[iUser].username[0] == 0 ? 0 : xlive_local_users[iUser].username)
					);
					xlive_local_users[iUser].auto_login = autoLoginChecked;
					if (iUser == xlln_login_player) {
						CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, autoLoginChecked ? BST_CHECKED : BST_UNCHECKED);
					}
				}
			}
			
			// If there were accounts that were auto logged in then do not pop the XLLN window.
			if (anyGotAutoLogged) {
				bool resultCode = XllnUpdateXliveSystemUIOpenState();
				return resultCode;
			}
		}
	}
	
	switch (show_type) {
		case XllnShowType::MAIN_SHOW:
		case XllnShowType::MAIN_SHOW_LOGIN: {
			ShowWindow(xlln_window_hwnd, SW_SHOWNORMAL);
			SetWindowPos(xlln_window_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			if (show_type == XllnShowType::MAIN_SHOW_LOGIN) {
				SendMessageW(xlln_window_hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_USERNAME), TRUE);
			}
			break;
		}
		case XllnShowType::MAIN_HIDE: {
			ShowWindow(xlln_window_hwnd, SW_HIDE);
			break;
		}
		case XllnShowType::DEBUG_LOG_SHOW: {
			ShowWindow(xlln_hwnd_debug_log, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_debug_log, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::DEBUG_LOG_HIDE: {
			ShowWindow(xlln_hwnd_debug_log, SW_HIDE);
			break;
		}
		case XllnShowType::SOCKETS_SHOW: {
			ShowWindow(xlln_hwnd_sockets, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_sockets, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::SOCKETS_HIDE: {
			ShowWindow(xlln_hwnd_sockets, SW_HIDE);
			break;
		}
		case XllnShowType::CONNECTIONS_SHOW: {
			ShowWindow(xlln_hwnd_connections, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_connections, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::CONNECTIONS_HIDE: {
			ShowWindow(xlln_hwnd_connections, SW_HIDE);
			break;
		}
		case XllnShowType::MESSAGE_BOX_SHOW: {
			ShowWindow(xlln_hwnd_message_box, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_message_box, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::MESSAGE_BOX_HIDE: {
			ShowWindow(xlln_hwnd_message_box, SW_HIDE);
			break;
		}
		case XllnShowType::INPUT_BOX_SHOW: {
			ShowWindow(xlln_hwnd_input_box, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_input_box, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::INPUT_BOX_HIDE: {
			ShowWindow(xlln_hwnd_input_box, SW_HIDE);
			break;
		}
		case XllnShowType::USER_CUSTOM_LIST_SHOW: {
			ShowWindow(xlln_hwnd_user_custom_list, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_user_custom_list, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::USER_CUSTOM_LIST_HIDE: {
			ShowWindow(xlln_hwnd_user_custom_list, SW_HIDE);
			break;
		}
		case XllnShowType::USER_CARD_SHOW: {
			ShowWindow(xlln_hwnd_user_card, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_user_card, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::USER_CARD_HIDE: {
			ShowWindow(xlln_hwnd_user_card, SW_HIDE);
			break;
		}
		case XllnShowType::ACHIEVEMENTS_SHOW: {
			ShowWindow(xlln_hwnd_achievements, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_achievements, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::ACHIEVEMENTS_HIDE: {
			ShowWindow(xlln_hwnd_achievements, SW_HIDE);
			break;
		}
		case XllnShowType::STATS_SHOW: {
			ShowWindow(xlln_hwnd_stats, SW_SHOWNORMAL);
			SetWindowPos(xlln_hwnd_stats, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			break;
		}
		case XllnShowType::STATS_HIDE: {
			ShowWindow(xlln_hwnd_stats, SW_HIDE);
			break;
		}
	}
	
	bool resultCode = XllnUpdateXliveSystemUIOpenState();
	return resultCode;
}

bool XllnIsXliveSystemUIOpen()
{
    if (SocialOverlayIsOpen()) return true;
	HWND hWndFocus = GetFocus();
	HWND hWndForeground = GetForegroundWindow();
	bool titleWindowFocused = false;
	
	EnterCriticalSection(&xlive_critsec_hotkeys);
	if (presentation_parameters_d3d9
		&& presentation_parameters_d3d9->hDeviceWindow
		&& (
			hWndFocus == presentation_parameters_d3d9->hDeviceWindow
			|| hWndForeground == presentation_parameters_d3d9->hDeviceWindow
		)
	) {
		titleWindowFocused = true;
	}
	else if (presentation_parameters_d3d10_d3d11
		&& presentation_parameters_d3d10_d3d11->OutputWindow
		&& (
			hWndFocus == presentation_parameters_d3d10_d3d11->OutputWindow
			|| hWndForeground == presentation_parameters_d3d10_d3d11->OutputWindow
		)
	) {
		titleWindowFocused = true;
	}
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	
	if (titleWindowFocused) {
		return false;
	}
	
	bool xllnWindowOpen = 
		IsWindowVisible(xlln_window_hwnd)
		|| IsWindowVisible(xlln_hwnd_debug_log)
		|| IsWindowVisible(xlln_hwnd_sockets)
		|| IsWindowVisible(xlln_hwnd_connections)
		|| IsWindowVisible(xlln_hwnd_message_box)
		|| IsWindowVisible(xlln_hwnd_input_box)
		|| IsWindowVisible(xlln_hwnd_user_custom_list)
		|| IsWindowVisible(xlln_hwnd_user_card);
	
	return xllnWindowOpen;
}

static void hotkeyFuncGuide()
{
    if (SocialOverlayClose()) return;
	EnterCriticalSection(&xlln_critsec_guide_ui_handlers);
	bool consumed = false;
	for (auto itrGuideUiHandlerInfo = xlln_guide_ui_handlers.begin(); itrGuideUiHandlerInfo != xlln_guide_ui_handlers.end(); itrGuideUiHandlerInfo++) {
		consumed = itrGuideUiHandlerInfo->guideUiHandler();
		if (consumed) {
			break;
		}
	}
	LeaveCriticalSection(&xlln_critsec_guide_ui_handlers);
	if (!consumed) {
		XllnShowWindow(XllnShowType::MAIN_SHOW);
	}
}

static const int hotkeyLen = 1;
static int hotkeyListenLen = 1;
static uint16_t* hotkeyId[hotkeyLen] = { &xlive_hotkey_id_guide };
static bool hotkeyPressed[hotkeyLen] = { false };
static void(*hotkeyFunc[hotkeyLen])(void) = { hotkeyFuncGuide };

static void ThreadHotkeys()
{
	while (1) {
		TRACE_FX();
		
		HWND hWndFocus = GetFocus();
		HWND hWndForeground = GetForegroundWindow();
		
		EnterCriticalSection(&xlive_critsec_hotkeys);
		
		bool isTarget = false;
		
		if (presentation_parameters_d3d9
			&& presentation_parameters_d3d9->hDeviceWindow
			&& (
				hWndFocus == presentation_parameters_d3d9->hDeviceWindow
				|| hWndForeground == presentation_parameters_d3d9->hDeviceWindow
			)
		) {
			isTarget = true;
		}
		else if (presentation_parameters_d3d10_d3d11
			&& presentation_parameters_d3d10_d3d11->OutputWindow
			&& (
				hWndFocus == presentation_parameters_d3d10_d3d11->OutputWindow
				|| hWndForeground == presentation_parameters_d3d10_d3d11->OutputWindow
			)
		) {
			isTarget = true;
		}
		else if (xlln_window_hwnd
			&& (
				hWndFocus == xlln_window_hwnd
				|| hWndForeground == xlln_window_hwnd
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_debug_log
			&& (
				hWndFocus == xlln_hwnd_debug_log
				|| hWndForeground == xlln_hwnd_debug_log
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_sockets
			&& (
				hWndFocus == xlln_hwnd_sockets
				|| hWndForeground == xlln_hwnd_sockets
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_connections
			&& (
				hWndFocus == xlln_hwnd_connections
				|| hWndForeground == xlln_hwnd_connections
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_message_box
			&& (
				hWndFocus == xlln_hwnd_message_box
				|| hWndForeground == xlln_hwnd_message_box
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_input_box
			&& (
				hWndFocus == xlln_hwnd_input_box
				|| hWndForeground == xlln_hwnd_input_box
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_user_custom_list
			&& (
				hWndFocus == xlln_hwnd_user_custom_list
				|| hWndForeground == xlln_hwnd_user_custom_list
			)
		) {
			isTarget = true;
		}
		else if (xlln_hwnd_user_card
			&& (
				hWndFocus == xlln_hwnd_user_card
				|| hWndForeground == xlln_hwnd_user_card
			)
		) {
			isTarget = true;
		}
		
		if (isTarget) {
			for (int i = 0; i < hotkeyListenLen; i++) {
				if (!*hotkeyId[i]) {
					continue;
				}
				//& 0x8000 is pressed
				//& 0x1 Key just transitioned from released to pressed.
				if (GetAsyncKeyState(*hotkeyId[i]) & 0x8000) {
					hotkeyPressed[i] = true;
				}
				else if (!(GetAsyncKeyState(*hotkeyId[i]) & 0x8000) && hotkeyPressed[i]) {
					hotkeyPressed[i] = false;
					
					bool isPressingCtrl = (GetKeyState(VK_CONTROL) < 0);
					bool isPressingShift = (GetKeyState(VK_SHIFT) < 0);
					bool isPressingAlt = (GetKeyState(VK_MENU) < 0);
					bool isPressingLWin = (GetKeyState(VK_LWIN) < 0);
					bool isPressingRWin = (GetKeyState(VK_RWIN) < 0);
					// Do not acknowledge if modifiers are being used.
					if (!isPressingCtrl && !isPressingShift && !isPressingAlt && !isPressingLWin && !isPressingRWin) {
						hotkeyFunc[i]();
					}
				}
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_hotkeys);
		
		DWORD resultWait = WaitForSingleObject(xlive_xrender_thread_hotkeys_event, isTarget ? 10 : 1000);
		if (resultWait != WAIT_OBJECT_0 && resultWait != STATUS_TIMEOUT) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s failed to wait on xlive_xrender_thread_hotkeys_event (0x%zx)."
				, __func__
				, xlive_xrender_thread_hotkeys_event
			);
			break;
		}
		
		if (xlive_xrender_thread_hotkeys_shutdown) {
			break;
		}
	}
}

static void XllnThreadHotkeysStart()
{
	TRACE_FX();
	
	XllnThreadHotkeysStop();
	xlive_xrender_thread_hotkeys_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlive_xrender_thread_hotkeys_shutdown = false;
	xlive_xrender_thread_hotkeys = std::thread(ThreadHotkeys);
}

void XllnThreadHotkeysStop()
{
	TRACE_FX();
	
	if (xlive_xrender_thread_hotkeys_event != INVALID_HANDLE_VALUE) {
		xlive_xrender_thread_hotkeys_shutdown = true;
		SetEvent(xlive_xrender_thread_hotkeys_event);
		xlive_xrender_thread_hotkeys.join();
		CloseHandle(xlive_xrender_thread_hotkeys_event);
		xlive_xrender_thread_hotkeys_event = INVALID_HANDLE_VALUE;
	}
}

// Process-termination path: the OS already killed the thread, so detach it so
// the global's destructor cannot std::terminate() during CRT cleanup.
void XllnThreadHotkeysAbandon()
{
	if (xlive_xrender_thread_hotkeys.joinable()) {
		xlive_xrender_thread_hotkeys.detach();
	}
}

static void ReleaseD3DDevice()
{
    SocialOverlayDetach();
	if (device_d3d9) {
		device_d3d9->Release();
		device_d3d9 = 0;
	}
	if (device_d3d9ex) {
		device_d3d9ex->Release();
		device_d3d9ex = 0;
	}
	if (device_d3d10) {
		device_d3d10->Release();
		device_d3d10 = 0;
	}
	if (device_d3d11) {
		device_d3d11->Release();
		device_d3d11 = 0;
	}
}

static void DeleteD3DPresentationParameters()
{
	if (presentation_parameters_d3d9) {
		delete presentation_parameters_d3d9;
		presentation_parameters_d3d9 = 0;
	}
	if (presentation_parameters_d3d10_d3d11) {
		delete presentation_parameters_d3d10_d3d11;
		presentation_parameters_d3d10_d3d11 = 0;
	}
}

static HRESULT UpdatePresentationParameters(void* d3d_presentation_parameters)
{
	if (!d3d_presentation_parameters) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s d3d_presentation_parameters is NULL.", __func__);
		return E_POINTER;
	}
	if (!device_d3d9 && !device_d3d9ex && !device_d3d10 && !device_d3d11) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s D3D Device not initialised.", __func__);
		return E_INVALIDARG;
	}
	if (((device_d3d9 || device_d3d9ex) && !((D3DPRESENT_PARAMETERS*)d3d_presentation_parameters)->hDeviceWindow)
		|| ((device_d3d10 || device_d3d11) && !((DXGI_SWAP_CHAIN_DESC*)d3d_presentation_parameters)->OutputWindow)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s D3D Device Window is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	DeleteD3DPresentationParameters();
	
	if (device_d3d9 || device_d3d9ex) {
		presentation_parameters_d3d9 = new D3DPRESENT_PARAMETERS;
		memcpy(presentation_parameters_d3d9, d3d_presentation_parameters, sizeof(D3DPRESENT_PARAMETERS));
	}
	else {
		presentation_parameters_d3d10_d3d11 = new DXGI_SWAP_CHAIN_DESC;
		memcpy(presentation_parameters_d3d10_d3d11, d3d_presentation_parameters, sizeof(DXGI_SWAP_CHAIN_DESC));
	}
	
	return S_OK;
}

HRESULT D3DOnCreateDevice(IUnknown* d3d_device, void* d3d_presentation_parameters)
{
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	if (!d3d_device) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s d3d_device is NULL.", __func__);
		return E_POINTER;
	}
	if (!d3d_presentation_parameters) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s d3d_presentation_parameters is NULL.", __func__);
		return E_POINTER;
	}
	
	EnterCriticalSection(&xlive_critsec_hotkeys);
	
	thread_id_gui = GetCurrentThreadId();
	
	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();
	
	HRESULT resultQueryInterface = d3d_device->QueryInterface(IID_IDirect3DDevice9, (void**)&device_d3d9);
	if (FAILED(resultQueryInterface)) {
		resultQueryInterface = d3d_device->QueryInterface(IID_IDirect3DDevice9Ex, (void**)&device_d3d9ex);
		if (FAILED(resultQueryInterface)) {
			resultQueryInterface = d3d_device->QueryInterface(IID_ID3D10Device, (void**)&device_d3d10);
			if (FAILED(resultQueryInterface)) {
				resultQueryInterface = d3d_device->QueryInterface(IID_ID3D11Device, (void**)&device_d3d11);
				if (FAILED(resultQueryInterface)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pPii->d3d_device is not a valid D3D 9, 9Ex, 10 or 11 interface.", __func__);
					return E_INVALIDARG;
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 11 Device Initialised.");
				}
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 10 Device Initialised.");
			}
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 9 Ex Device Initialised.");
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Direct3D 9 Device Initialised.");
	}
	
	HRESULT errorPP = UpdatePresentationParameters(d3d_presentation_parameters);
	if (FAILED(errorPP)) {
		ReleaseD3DDevice();
		LeaveCriticalSection(&xlive_critsec_hotkeys);
		return errorPP;
	}

    if (device_d3d9 && presentation_parameters_d3d9)
        SocialOverlayAttach(device_d3d9, presentation_parameters_d3d9->hDeviceWindow);
	
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	
	return S_OK;
}

int32_t InitXRender()
{
	TRACE_FX();
	
	XllnThreadHotkeysStart();
	
	SetFPSLimit(xlive_fps_limit);
	
	Initialised_XRender = true;
	return S_OK;
}

int32_t UninitXRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	
	XllnThreadHotkeysStop();
	
	EnterCriticalSection(&xlive_critsec_hotkeys);
	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	
	Initialised_XRender = false;
	
	return S_OK;
}

uint32_t SetFPSLimit(uint32_t fps_limit)
{
	uint32_t old_limit;
	EnterCriticalSection(&xlive_critsec_fps_limit);
	old_limit = xlive_fps_limit;
	xlive_fps_limit = fps_limit;
	nextFrame = std::chrono::system_clock::now();
	desiredRenderTime = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(1.0 / (double)fps_limit));
	LeaveCriticalSection(&xlive_critsec_fps_limit);
	return old_limit;
}

static void frameTimeManagement()
{
	std::this_thread::sleep_until(nextFrame);
	do {
		nextFrame += desiredRenderTime;
	} while (std::chrono::system_clock::now() > nextFrame);
}

// #5002
int32_t WINAPI XLiveRender()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	
    SocialOverlayRender();
	if (xlive_fps_limit > 0) {
		frameTimeManagement();
	}
	return S_OK;
}

// #5005
HRESULT WINAPI XLiveOnCreateDevice(IUnknown* d3d_device, void* d3d_presentation_parameters)
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	
	HRESULT result = D3DOnCreateDevice(d3d_device, d3d_presentation_parameters);
	
	return result;
}

// #5006
HRESULT WINAPI XLiveOnDestroyDevice()
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	
	EnterCriticalSection(&xlive_critsec_hotkeys);
	DeleteD3DPresentationParameters();
	ReleaseD3DDevice();
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	
	return S_OK;
}

// #5007
HRESULT WINAPI XLiveOnResetDevice(void* d3d_presentation_parameters)
{
	TRACE_FX();
	if (!Initialised_XRender) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XRender is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	EnterCriticalSection(&xlive_critsec_hotkeys);
	HRESULT errorPP = UpdatePresentationParameters(d3d_presentation_parameters);
	LeaveCriticalSection(&xlive_critsec_hotkeys);
	if (FAILED(errorPP)) {
		return errorPP;
	}

    if (presentation_parameters_d3d9)
        SocialOverlayReset(presentation_parameters_d3d9->hDeviceWindow);
	
	return S_OK;
}
