#include <Winsock2.h>
#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "xlive.hpp"
#include "../xlivelessness.hpp"
#include "../utils/util-hook.hpp"
#include "title-patches.hpp"
#include "menus/custom-menu.hpp"
#include "menus/accounts/account-management.hpp"
#include <stdint.h>
#include <thread>
#include <condition_variable>
#include <atomic>

HWND H2hWnd = NULL;

static tXLiveInput DetourXLiveInput = 0;
static HRESULT WINAPI HookXLiveInput(XLIVE_INPUT_INFO *pPii);
static DWORD HookHelperXLiveInput = (DWORD)HookXLiveInput;
static HRESULT WINAPI HookXLiveInput(XLIVE_INPUT_INFO *pPii)
{
	// Undo hook.
	WritePointer(GetOffsetAddress(0x0000E892 + 2, 0x0000e892 + 2, 0x0000E892 + 2), &DetourXLiveInput);
	HookHelperXLiveInput = 0;
	
	H2hWnd = pPii->hWnd;
	
	HRESULT result = DetourXLiveInput(pPii);
	return result;
}

static tXShowSigninUI DetourXShowSigninUI = 0;
static DWORD WINAPI HookXShowSigninUI(DWORD cPanes, DWORD dwFlags)
{
	DWORD result;
	if (Title_Version == CLIENT_11122) {
		CustomMenuCall_AccountManagement(1);
		result = ERROR_SUCCESS;
	}
	else {
		//result = DetourXShowSigninUI(cPanes, dwFlags);
		result = XLLNLogin(0, TRUE, 0, 0);
	}
	return result;
}
static DWORD HookHelperXShowSigninUI = (DWORD)HookXShowSigninUI;

tXShowGuideUI DetourXShowGuideUI = 0;
static DWORD WINAPI HookXShowGuideUI(DWORD dwUserIndex)
{
	DWORD result;
	if (CMGuideMenu()) {
		result = ERROR_SUCCESS;
	}
	else {
		result = DetourXShowGuideUI(dwUserIndex);
		//DWORD result = XLLNLogout(0);
	}
	return result;
}
static DWORD HookHelperXShowGuideUI = (DWORD)HookXShowGuideUI;

/*// #20
typedef INT(WINAPI *tXSocketRecvFrom)(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
tXSocketRecvFrom XSocketRecvFrom = NULL;
static INT WINAPI XSocketRecvFromHook(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
	INT result = XSocketRecvFrom(s, buf, len, flags, from, fromlen);
	
	return result;
}
static DWORD XSocketRecvFromHookHelper = (DWORD)XSocketRecvFromHook;*/

static SOCKET pcarto_socket_udp_rand = INVALID_SOCKET;

static CRITICAL_SECTION pcarto_lock_customsocketlisten_thread;
static std::thread pcarto_thread_customsocketlisten;
static BOOL pcarto_customsocketlisten_running = FALSE;
static std::condition_variable pcarto_cond_customsocketlisten;
static std::atomic<bool> pcarto_customsocketlisten_exit = TRUE;
static std::atomic<bool> pcarto_customsocketlisten_break_sleep = FALSE;

static VOID CustomSocketListen()
{
	std::mutex mutexPause;
	char buff[1024];
	sockaddr SenderAddr;
	int SenderAddrSize = sizeof(SenderAddr);

	char packetHeadToClient[4] = { XLLN_CUSTOM_PACKET_SENTINEL, XLLNCustomPacketType::CUSTOM_OTHER, 'P', 'C' };

	while (1) {
		//EnterCriticalSection(&pcarto_lock_customsocketlisten_thread);

		int result = 0;
		do {
			result = recvfrom(pcarto_socket_udp_rand, buff, sizeof(buff), 0, &SenderAddr, &SenderAddrSize);
			if (result > 0) {
				if (result > sizeof(packetHeadToClient) && *(unsigned int*)buff == *(unsigned int*)&packetHeadToClient) {
					switch (buff[sizeof(packetHeadToClient)]) {
						case PCartoCustomPacketType::UNKNOWN: {
							XLLNDebugLog(0, "PCartoOnline: Received PCarto 00 response.");
							break;
						}
						default: {
							XLLNDebugLog(0, "PCartoOnline: Received UNKNOWN PCarto packet.");
							continue;
						}
					}
				}
				else {
					XLLNDebugLog(0, "PCartoOnline: Received UNKNOWN Custom UDP 0 packet.");
				}
			}
		} while (result > 0);
		
		//LeaveCriticalSection(&pcarto_lock_customsocketlisten_thread);

		std::unique_lock<std::mutex> lock(mutexPause);
		pcarto_cond_customsocketlisten.wait_for(lock, std::chrono::seconds(1), []() { return pcarto_customsocketlisten_exit == TRUE || pcarto_customsocketlisten_break_sleep == TRUE; });
		if (pcarto_customsocketlisten_exit) {
			break;
		}
		pcarto_customsocketlisten_break_sleep = FALSE;
	}

	shutdown(pcarto_socket_udp_rand, SD_SEND);

	unsigned int retryNumber = 0;
	const unsigned int MAX_RETRY_NUMBER = 2 * 1000 / 10; // 10ms sleep from below. Wait no longer than 2 seconds.

	while (true) {
		int result = recv(pcarto_socket_udp_rand, buff, sizeof(buff), 0);

		if (result == 0) {
			break;
			//return 0; // Client gracefully closed connection.
		}
		else if (result > 0) {
			break;
			//return -1; // Received unexpected data instead of socket closure
		}
		else
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				if (retryNumber++ == MAX_RETRY_NUMBER) {
					break;
					//return -1; // Client didn't close socket within specified time
				}

				Sleep(10); // wait 10ms
			}
			else {
				break;
				//return -1; // Unexpected error occured
			}
		}
	}

	closesocket(pcarto_socket_udp_rand);
	pcarto_socket_udp_rand = INVALID_SOCKET;
}

static VOID CustomSocketListenStart()
{
	EnterCriticalSection(&pcarto_lock_customsocketlisten_thread);

	if (pcarto_customsocketlisten_running) {
		pcarto_customsocketlisten_break_sleep = TRUE;
		pcarto_cond_customsocketlisten.notify_all();
	}
	else {
		pcarto_customsocketlisten_running = TRUE;
		pcarto_customsocketlisten_exit = FALSE;
		pcarto_thread_customsocketlisten = std::thread(CustomSocketListen);
	}

	LeaveCriticalSection(&pcarto_lock_customsocketlisten_thread);
}
static VOID CustomSocketListenStop()
{
	EnterCriticalSection(&pcarto_lock_customsocketlisten_thread);

	if (pcarto_customsocketlisten_running) {
		pcarto_customsocketlisten_running = FALSE;
		LeaveCriticalSection(&pcarto_lock_customsocketlisten_thread);
		if (pcarto_customsocketlisten_exit == FALSE) {
			pcarto_customsocketlisten_exit = TRUE;
			pcarto_cond_customsocketlisten.notify_all();
			pcarto_thread_customsocketlisten.join();
		}
	}
	else {
		LeaveCriticalSection(&pcarto_lock_customsocketlisten_thread);
	}
}


static DWORD WINAPI ThreadProc(LPVOID lpParam)
{
	srand((unsigned int)time(NULL));

	CustomSocketListenStart();

	return TRUE;
}

static BOOL InitOther()
{
	InitializeCriticalSection(&pcarto_lock_customsocketlisten_thread);

	WSADATA wsaData;
	DWORD result = WSAStartup(2, &wsaData);
	//BOOL success = result == 0;

	if ((pcarto_socket_udp_rand = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		XLLNDebugLog(0, "PCartoOnline: ERROR: Could not create UDP 0 socket.");
	}

	char broadcast = '1';
	if (setsockopt(pcarto_socket_udp_rand, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		XLLNDebugLog(0, "PCartoOnline: ERROR: Unable to setsockopt SO_BROADCAST for UDP 0 socket.");
	}

	u_long iMode = 1;
	if (ioctlsocket(pcarto_socket_udp_rand, FIONBIO, &iMode) != NO_ERROR) {
		XLLNDebugLog(0, "PCartoOnline: ERROR: Unable to ioctlsocket FIONBIO 1 for UDP 0 socket.");
	}

	CreateThread(0, NULL, ThreadProc, (LPVOID)xlln_hmod_xlln_module, NULL, NULL);

	return TRUE;
}

static BOOL UninitOther()
{
	CustomSocketListenStop();

	WSACleanup();

	DeleteCriticalSection(&pcarto_lock_customsocketlisten_thread);
	return TRUE;
}

// #41101
uint32_t WINAPI XLLNModulePostInit()
{
	return ERROR_SUCCESS;
}

// #41102
uint32_t WINAPI XLLNModulePreUninit()
{
	return ERROR_SUCCESS;
}

bool InitXLive()
{
	
	if (Title_Version == CLIENT_11081 || Title_Version == CLIENT_11091 || Title_Version == CLIENT_11122) {
		// Hook imports.
		DetourXLiveInput = (tXLiveInput)**(DWORD**)(GetOffsetAddress(0x0000E892 + 2, 0x0000e892 + 2, 0x0000E892 + 2));
		WritePointer(GetOffsetAddress(0x0000E892 + 2, 0x0000e892 + 2, 0x0000E892 + 2), &HookHelperXLiveInput);
		
		//DetourXShowSigninUI = (tXShowSigninUI)**(DWORD**)(GetOffsetAddress(0x002a7acd + 2, 0x002a861d + 2, 0x002a8769 + 2));
		//WritePointer(GetOffsetAddress(0x002a7acd + 2, 0x002a861d + 2, 0x002a8769 + 2), &HookHelperXShowSigninUI);
		
		DetourXShowGuideUI = (tXShowGuideUI)**(DWORD**)(GetOffsetAddress(0x0000e8a4 + 2, 0x0000e8a4 + 2, 0x0000e8a4 + 2));
		WritePointer(GetOffsetAddress(0x0000e8a4 + 2, 0x0000e8a4 + 2, 0x0000e8a4 + 2), &HookHelperXShowGuideUI);
		
	}
	
	//InitOther();
	
	return true;
}

bool UninitXLive()
{
	//UninitOther();
	
	if (Title_Version == CLIENT_11081 || Title_Version == CLIENT_11091 || Title_Version == CLIENT_11122) {
		// Undo hooks.
		if (HookHelperXLiveInput && DetourXLiveInput) {
			WritePointer(GetOffsetAddress(0x0000E892 + 2, 0x0000e892 + 2, 0x0000E892 + 2), &DetourXLiveInput);
		}
		if (DetourXShowSigninUI) {
			WritePointer(GetOffsetAddress(0x002a7acd + 2, 0x002a861d + 2, 0x002a8769 + 2), &DetourXShowSigninUI);
		}
		if (DetourXShowGuideUI) {
			WritePointer(GetOffsetAddress(0x0000e8a4 + 2, 0x0000e8a4 + 2, 0x0000e8a4 + 2), &DetourXShowGuideUI);
		}
	}
	return true;
}
