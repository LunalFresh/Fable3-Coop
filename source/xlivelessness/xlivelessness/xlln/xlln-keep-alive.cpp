#include <winsock2.h>
#include <Windows.h>
#include "./xlln-keep-alive.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xfuncs.hpp"
#include "./debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-network.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xnet.hpp"
#include "../resource.h"
#include <thread>

static HANDLE xlln_keep_alive_thread_event = INVALID_HANDLE_VALUE;
static bool xlln_keep_alive_thread_shutdown = false;
static std::thread xlln_keep_alive_thread;

static void ThreadKeepAlive()
{
	__time64_t nextHubIteration = 0;
	
	while (1) {
		TRACE_FX();
		
		__time64_t ltime;
		_time64(&ltime);//seconds since epoch.
		
		if (xlln_direct_ip_connect.timeoutAt && xlln_direct_ip_connect.timeoutAt < ltime) {
			XllnDirectIpConnectCancel();
			MessageBoxW(xlln_window_hwnd, L"Connection attempt timed out.", L"XLLN Direct IP Connect Error", MB_OK);
		}
		
		if (nextHubIteration < ltime) {
			// Check again in about 5 seconds.
			nextHubIteration = ltime + 5;
			
			{
				EnterCriticalSection(&xlln_critsec_network_broadcast_addresses);
				
				__time64_t timeToSendKeepAlive = ltime - 30;
				for (auto const &broadcastEntity : xlln_network_broadcast_addresses) {
					if (broadcastEntity.entityType != XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_HUB_SERVER && broadcastEntity.entityType != XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_UNKNOWN) {
						continue;
					}
					if (broadcastEntity.lastComm > timeToSendKeepAlive) {
						continue;
					}
					
					XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
					sendPacket->destinationAddress = broadcastEntity.sockaddr;
					{
						const size_t hubRequestPacketBufferSize = sizeof(XllnNetworkPacket::TYPE) + sizeof(XllnNetworkPacket::HUB_REQUEST_PACKET);
						uint8_t* hubRequestPacketBuffer = new uint8_t[hubRequestPacketBufferSize];
						hubRequestPacketBuffer[0] = XllnNetworkPacket::TYPE::XLLN_NPT_HUB_REQUEST;
						XllnNetworkPacket::HUB_REQUEST_PACKET* hubRequest = (XllnNetworkPacket::HUB_REQUEST_PACKET*)&hubRequestPacketBuffer[sizeof(XllnNetworkPacket::TYPE)];
						hubRequest->xllnVersion = (DLL_VERSION_MAJOR << 24) + (DLL_VERSION_MINOR << 16) + (DLL_VERSION_REVISION << 8) + DLL_VERSION_BUILD;
						hubRequest->titleId = xlive_title_id;
						hubRequest->titleVersion = xlive_title_version;
						hubRequest->instanceId = xlln_global_instance_id;
						
						sendPacket->data = hubRequestPacketBuffer;
						sendPacket->dataSize = hubRequestPacketBufferSize;
					}
					
					if (!SendPacketToRemoteInstance(sendPacket)) {
						delete sendPacket;
					}
					sendPacket = 0;
				}
				
				LeaveCriticalSection(&xlln_critsec_network_broadcast_addresses);
			}
		}
		
		XllnWndSocketsInvalidateSockets();
		
		DWORD resultWait = WaitForSingleObject(xlln_keep_alive_thread_event, 1000);
		if (resultWait != WAIT_OBJECT_0 && resultWait != STATUS_TIMEOUT) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s failed to wait on xlln_keep_alive_thread_event (0x%zx)."
				, __func__
				, xlln_keep_alive_thread_event
			);
			break;
		}
		
		if (xlln_keep_alive_thread_shutdown) {
			break;
		}
	}
}

void XllnThreadKeepAliveStart()
{
	TRACE_FX();
	
	XllnThreadKeepAliveStop();
	xlln_keep_alive_thread_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_keep_alive_thread_shutdown = false;
	xlln_keep_alive_thread = std::thread(ThreadKeepAlive);
}

void XllnThreadKeepAliveStop()
{
	TRACE_FX();
	
	if (xlln_keep_alive_thread_event != INVALID_HANDLE_VALUE) {
		xlln_keep_alive_thread_shutdown = true;
		SetEvent(xlln_keep_alive_thread_event);
		xlln_keep_alive_thread.join();
		CloseHandle(xlln_keep_alive_thread_event);
		xlln_keep_alive_thread_event = INVALID_HANDLE_VALUE;
	}
}

// Process-termination path: the OS already killed the thread, so detach it so
// the global's destructor cannot std::terminate() during CRT cleanup.
void XllnThreadKeepAliveAbandon()
{
	if (xlln_keep_alive_thread.joinable()) {
		xlln_keep_alive_thread.detach();
	}
}
