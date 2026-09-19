#include <winsock2.h>
#include "xdefs.hpp"
#include "xfuncs.hpp"
#include "xsocket.hpp"
#include "xlive.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlln/xlln-keep-alive.hpp"
#include "xnet.hpp"
#include "xlocator.hpp"
#include "xnetqos.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include <map>

uint16_t xlive_port_online = 3074;
uint16_t xlive_port_system_link = 0;
bool xlive_netsocket_abort = false;
uint32_t xlive_network_rcvbuf = 0x00010000;
uint32_t xlive_network_sndbuf = 0x00010000;

CRITICAL_SECTION xlive_critsec_sockets;
// Key: title_socket_handle.
std::map<SOCKET, XLIVE_TITLE_SOCKET*> xlive_title_sockets;
// Key: <is a stream socket, portActual>.
std::map<TitleTransportPort, XLIVE_TITLE_SOCKET*> xlive_title_socket_port_actual_to_title_socket;
// Key: <tcpRemoteInstanceId, tcpRemoteTitlePort>.
std::map<InstanceIdTitlePort, XLIVE_TITLE_SOCKET*> xlive_title_socket_tcp_connected_sockets;

XLIVE_TITLE_SOCKET* XllnGetTitleSocketByTitlePort_(bool is_stream, uint16_t title_port_actual)
{
	XLIVE_TITLE_SOCKET* titleSocket = 0;
	
	auto itrTspaToTs = xlive_title_socket_port_actual_to_title_socket.find(std::make_pair(is_stream, title_port_actual));
	if (itrTspaToTs != xlive_title_socket_port_actual_to_title_socket.end()) {
		titleSocket = itrTspaToTs->second;
	}
	
	return titleSocket;
}

bool SocketImplicitBind_(XLIVE_TITLE_SOCKET* title_socket)
{
	if (!title_socket->hasBinded) {
		std::set<uint16_t> takenPorts;
		
		for (auto itrTitleSocket : xlive_title_sockets) {
			if (!itrTitleSocket.second->hasBinded) {
				continue;
			}
			
			takenPorts.insert(itrTitleSocket.second->portActual);
		}
		
		title_socket->portRequested = 0;
		{
			// generate an internally unused ephemeral port.
			const uint16_t base = 0xC000;
			const uint16_t range = 0xFFFF - base;
			do {
				title_socket->portActual = base + (rand() % range);
			} while (takenPorts.count(title_socket->portActual));
		}
		
		xlive_title_socket_port_actual_to_title_socket[std::make_pair(title_socket->protocol == IPPROTO_TCP, title_socket->portActual)] = title_socket;
		
		title_socket->hasBinded = true;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s title_socket_handle (0x%zx) implicitly binded to port (%hu)."
			, __func__
			, title_socket->handle
			, title_socket->portActual
		);
		
		XllnWndSocketsInvalidateSockets();
	}
	
	return true;
}

// #3
SOCKET WINAPI XSocketCreate(int32_t address_family, int32_t type, int32_t protocol)
{
	TRACE_FX();
	if (address_family != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s address_family (%d) must be AF_INET (%d)."
			, __func__
			, address_family
			, AF_INET
		);
		WSASetLastError(WSAEAFNOSUPPORT);
		return INVALID_SOCKET;
	}
	if (type == 0 && protocol == 0) {
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	}
	else if (type == SOCK_DGRAM && protocol == 0) {
		protocol = IPPROTO_UDP;
	}
	else if (type == SOCK_STREAM && protocol == 0) {
		protocol = IPPROTO_TCP;
	}
	bool isVdpProtocol = false;
	if (protocol == IPPROTO_VDP) {
		isVdpProtocol = true;
		// I think VDP (Voice + Data Protocol) is some custom protocol to combine XHV Engine data and regular UDP packets.
		protocol = IPPROTO_UDP;
	}
	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s type (%d) must be either SOCK_STREAM (%d) or SOCK_DGRAM (%d)."
			, __func__
			, type
			, SOCK_STREAM
			, SOCK_DGRAM
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return INVALID_SOCKET;
	}
	
	if (xlive_netsocket_abort) {
		WSASetLastError(WSAENETDOWN);
		return INVALID_SOCKET;
	}
	
	XLIVE_TITLE_SOCKET* titleSocket = new XLIVE_TITLE_SOCKET(protocol);
	SOCKET socketHandle = titleSocket->handle;
	titleSocket->type = type;
	titleSocket->protocol = protocol;
	titleSocket->isVdpProtocol = isVdpProtocol;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s title_socket_handle (0x%zx), address_family (%d), type (%d), protocol (%d).%s"
		, __func__
		, socketHandle
		, address_family
		, type
		, protocol
		, isVdpProtocol ? " Is a VDP Socket." : ""
	);
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		xlive_title_sockets[titleSocket->handle] = titleSocket;
		
		XllnWndSocketsInvalidateSockets();
		
		titleSocket = 0;
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return socketHandle;
}

bool ShutdownTitleSocket_(XLIVE_TITLE_SOCKET* titleSocket, XTS_SHUTDOWN_STATE howTypeRequired)
{
	if (howTypeRequired & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {
		if (titleSocket->recvInCritSec) {
			SetEvent(titleSocket->recvNotify);
		}
		// Wait for any parked receiver to exit. Unconditional: the waiter clears its
		// recvInCritSec flag moments before releasing recvCritSec, so gating this barrier on
		// the flag leaves a window where the teardown proceeds (and frees the critical
		// section) while the waiter is still inside it.
		EnterCriticalSection(&titleSocket->recvCritSec);
		LeaveCriticalSection(&titleSocket->recvCritSec);
		// Release every pending overlapped recv; a subsequent reap reports WSAESHUTDOWN.
		for (XTS_PENDING_OVERLAPPED_RECV &pendingRecv : titleSocket->recvPendingOverlapped) {
			if (pendingRecv.wsaOverlapped->hEvent && pendingRecv.wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
				WSASetEvent(pendingRecv.wsaOverlapped->hEvent);
			}
		}
		titleSocket->recvWsaOverlappedCancelled = false;
		
		for (XTS_RECV_PACKET* recvPacket : titleSocket->recvPacketQueue) {
			delete recvPacket;
		}
		titleSocket->recvPacketQueue.clear();
		
		titleSocket->selectNotifyReadPending = true;
		SetEvent(titleSocket->selectNotifyRead);
		
		// An accepted socket inherits the listener's port without registering it, so a close that
		// erased the entry regardless would hand a listening port to a socket with no backlog.
		const TitleTransportPort titlePort(titleSocket->protocol == IPPROTO_TCP, titleSocket->portActual);
		const auto itrPortRegistered = xlive_title_socket_port_actual_to_title_socket.find(titlePort);
		if (itrPortRegistered != xlive_title_socket_port_actual_to_title_socket.end()
			&& itrPortRegistered->second == titleSocket) {
			xlive_title_socket_port_actual_to_title_socket.erase(itrPortRegistered);
			
			// For shared ports.
			for (auto itrTitleSocket : xlive_title_sockets) {
				if (!itrTitleSocket.second->hasBinded) {
					continue;
				}
				if ((itrTitleSocket.second->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV)) {
					continue;
				}
				if (itrTitleSocket.second->handle == titleSocket->handle) {
					continue;
				}
				if (titlePort == TitleTransportPort(itrTitleSocket.second->protocol == IPPROTO_TCP, itrTitleSocket.second->portActual)) {
					xlive_title_socket_port_actual_to_title_socket[titlePort] = itrTitleSocket.second;
					break;
				}
			}
		}
	}
	if (howTypeRequired & XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND) {
		
		{
			EnterCriticalSection(&xlln_critsec_network_send);
			
			for (auto itrSendPacket = xlln_network_send_queue.begin(); itrSendPacket != xlln_network_send_queue.end();) {
				if ((*itrSendPacket)->sourceTitleSocketHandle == titleSocket->handle) {
					delete (*itrSendPacket);
					itrSendPacket = xlln_network_send_queue.erase(itrSendPacket);
				}
				else {
					itrSendPacket++;
				}
			}
			
			LeaveCriticalSection(&xlln_critsec_network_send);
		}
		
		if (titleSocket->sendInCritSec) {
			SetEvent(titleSocket->sendNotify);
		}
		// Wait for any parked sender to exit (unconditional, same flag-clear window as the
		// receive barrier above).
		EnterCriticalSection(&titleSocket->sendCritSec);
		LeaveCriticalSection(&titleSocket->sendCritSec);
		if (titleSocket->sendWsaOverlapped && titleSocket->sendWsaOverlapped->hEvent && titleSocket->sendWsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
			WSASetEvent(titleSocket->sendWsaOverlapped->hEvent);
		}
		
		titleSocket->sendWsaOverlapped = 0;
		titleSocket->sendWsaOverlappedCancelled = false;
	}
	
	return true;
}

// #4
int32_t WINAPI XSocketClose(SOCKET title_socket_handle)
{
	TRACE_FX();
	
	const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
	const size_t packetSizeTypeTcpConnectResponse = sizeof(XllnNetworkPacket::TCP_CONNECT_RESPONSE);
	
	XLIVE_TITLE_SOCKET* titleSocket = 0;
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		{
			auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
			if (itrTitleSocket == xlive_title_sockets.end()) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) does not exist."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			titleSocket = itrTitleSocket->second;
		}
		
		titleSocket->hasShutdown = XTS_SHUTDOWN_STATE::XTS_SS_INACTIVE;
		
		ShutdownTitleSocket_(titleSocket, titleSocket->hasShutdown);
		
		if (titleSocket->tcpIsConnecting) {
			titleSocket->tcpIsConnecting = false;
			titleSocket->socketOptions[SO_ERROR] = WSAEINTR;
			
			SetEvent(titleSocket->tcpConnectNotify);
			titleSocket->selectNotifyExceptPending = true;
			SetEvent(titleSocket->selectNotifyExcept);
		}
		if (titleSocket->tcpIsAccepting) {
			titleSocket->tcpIsAccepting = false;
			
			SetEvent(titleSocket->tcpConnectNotify);
		}
		// Wait for any blocked connect()/accept() to exit. Unconditional: the waiter clears
		// tcpInCritSec moments before releasing tcpCritSec, so gating on the flag leaves a
		// window where teardown frees the critical section while the waiter is still inside it.
		EnterCriticalSection(&titleSocket->tcpCritSec);
		LeaveCriticalSection(&titleSocket->tcpCritSec);
		
		if (titleSocket->tcpIsConnected) {
			if (titleSocket->tcpSendQueue.size()) {
				titleSocket->tcpIsClosing = true;
			}
			XllnNetworkPacket::TCP_RESPONSE_CODE tcpResponseCode = titleSocket->tcpIsClosing ? XllnNetworkPacket::TCP_RESPONSE_CODE::XLLN_NP_TRC_NOT_ACCEPTING_DATA : XllnNetworkPacket::TCP_RESPONSE_CODE::XLLN_NP_TRC_REJECT;
			
			XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
			sendPacket->destinationInstanceId = titleSocket->tcpRemoteInstanceId;
			{
				const size_t packetSize = packetSizeType + packetSizeTypeTcpConnectResponse;
				
				uint8_t* packetBuffer = new uint8_t[packetSize];
				packetBuffer[0] = XllnNetworkPacket::TYPE::XLLN_NPT_TCP_CONNECT_RESPONSE;
				XllnNetworkPacket::TCP_CONNECT_RESPONSE &packetTcpConnectResponse = *(XllnNetworkPacket::TCP_CONNECT_RESPONSE*)&packetBuffer[packetSizeType];
				packetTcpConnectResponse.titlePacket.instanceId = xlln_global_instance_id;
				packetTcpConnectResponse.titlePacket.titlePortDestination = titleSocket->tcpRemoteTitlePort;
				packetTcpConnectResponse.titlePacket.titlePortSource = titleSocket->portActual;
				packetTcpConnectResponse.responseCode = tcpResponseCode;
				
				sendPacket->data = packetBuffer;
				sendPacket->dataSize = packetSize;
			}
			
			if (SendPacketToRemoteInstance(sendPacket)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s title_socket_handle (0x%zx) TCP sent %s %s to 0x%08x:%hu."
					, __func__
					, titleSocket->handle
					, XllnNetworkPacket::GetTcpResponseCodeName(tcpResponseCode)
					, XllnNetworkPacket::GetPacketTypeName(XllnNetworkPacket::TYPE::XLLN_NPT_TCP_CONNECT_RESPONSE)
					, titleSocket->tcpRemoteInstanceId
					, titleSocket->tcpRemoteTitlePort
				);
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) TCP failed to send %s %s to 0x%08x:%hu."
					, __func__
					, titleSocket->handle
					, XllnNetworkPacket::GetTcpResponseCodeName(tcpResponseCode)
					, XllnNetworkPacket::GetPacketTypeName(XllnNetworkPacket::TYPE::XLLN_NPT_TCP_CONNECT_RESPONSE)
					, titleSocket->tcpRemoteInstanceId
					, titleSocket->tcpRemoteTitlePort
				);
				
				delete sendPacket;
			}
			sendPacket = 0;
		}
		
		XllnWndSocketsInvalidateSockets();
		
		if (titleSocket->tcpIsClosing) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s title_socket_handle (0x%zx) is being closed with tcpSendQueue size (0x%zx)."
				, __func__
				, title_socket_handle
				, titleSocket->tcpSendQueue.size()
			);
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			WSASetLastError(ERROR_SUCCESS);
			return ERROR_SUCCESS;
		}
		
		if (titleSocket->wsaEventSelectClose != WSA_INVALID_EVENT) {
			SetEvent(titleSocket->wsaEventSelectClose);
		}
		
		xlive_title_sockets.erase(titleSocket->handle);
		
		for (auto itrTitleSocketConnected = xlive_title_socket_tcp_connected_sockets.begin(); itrTitleSocketConnected != xlive_title_socket_tcp_connected_sockets.end(); ) {
			if (itrTitleSocketConnected->second == titleSocket) {
				xlive_title_socket_tcp_connected_sockets.erase(itrTitleSocketConnected++);
			}
			else {
				itrTitleSocketConnected++;
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s title_socket_handle (0x%zx) has been closed."
		, __func__
		, title_socket_handle
	);
	
	delete titleSocket;
	titleSocket = 0;
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #5
int32_t WINAPI XSocketShutdown(SOCKET title_socket_handle, int32_t how)
{
	TRACE_FX();
	if (
		how != SD_BOTH
		&& how != SD_RECEIVE
		&& how != SD_SEND
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) invalid value for how (%d)."
			, __func__
			, title_socket_handle
			, how
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	
	XTS_SHUTDOWN_STATE howType = (how == SD_RECEIVE ? XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV : (how == SD_SEND ? XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND : XTS_SHUTDOWN_STATE::XTS_SS_INACTIVE));
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		if (titleSocket->protocol == IPPROTO_TCP && !titleSocket->tcpIsConnected) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) TCP was not connected."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTCONN);
			return SOCKET_ERROR;
		}
		
		XTS_SHUTDOWN_STATE howTypeRequired = (XTS_SHUTDOWN_STATE)((~titleSocket->hasShutdown) & howType);
		
		titleSocket->hasShutdown = (XTS_SHUTDOWN_STATE)(titleSocket->hasShutdown | howTypeRequired);
		
		ShutdownTitleSocket_(titleSocket, howTypeRequired);
		
		if (howTypeRequired != XTS_SHUTDOWN_STATE::XTS_SS_STILL_ACTIVE) {
			XllnWndSocketsInvalidateSockets();
		}
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "%s title_socket_handle (0x%zx) how (%d)."
		, __func__
		, title_socket_handle
		, how
	);
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #6
int32_t WINAPI XSocketIOCTLSocket(SOCKET title_socket_handle, int32_t command, unsigned long* command_value)
{
	TRACE_FX();
	if (!command_value) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s command_value is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		// FIXME implement the commands used.
		switch (command) {
			case FIONBIO:
			case FIONREAD: {
				auto itrIoctl = titleSocket->socketIoctls.find(command);
				if (itrIoctl == titleSocket->socketIoctls.end()) {
					__debugbreak();
				}
				switch (command) {
					case FIONBIO: {
						if (
							!*command_value
							&& (
								titleSocket->wsaEventSelectRead != WSA_INVALID_EVENT
								|| titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT
								|| titleSocket->wsaEventSelectOob != WSA_INVALID_EVENT
								|| titleSocket->wsaEventSelectAccept != WSA_INVALID_EVENT
								|| titleSocket->wsaEventSelectConnect != WSA_INVALID_EVENT
								|| titleSocket->wsaEventSelectClose != WSA_INVALID_EVENT
							)
						) {
							LeaveCriticalSection(&xlive_critsec_sockets);
							
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s on title_socket_handle (0x%zx) failed, cannot set a socket to BLOCK when there is an event registered with WSAEventSelect."
								, __func__
								, title_socket_handle
							);
							WSASetLastError(WSAEINVAL);
							return SOCKET_ERROR;
						}
						
						itrIoctl->second = *command_value;
						titleSocket->isBlocking = !*command_value;
						
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
							, "%s on title_socket_handle (0x%zx) FIONBIO set to %s."
							, __func__
							, title_socket_handle
							, (titleSocket->isBlocking ? "BLOCK" : "ASYNC")
						);
						
						XllnWndSocketsInvalidateSockets();
						
						break;
					}
					case FIONREAD: {
						// Compute from the live queue rather than the arrival-time cache: the cached
						// value is only recomputed when a packet ARRIVES, so it over-reports after a
						// recv consumes data. For a message-oriented socket FIONREAD is the size of
						// the FIRST queued message; for a stream it is the total buffered.
						unsigned long readableSize = 0;
						if (titleSocket->type == SOCK_DGRAM) {
							if (titleSocket->recvPacketQueue.size()) {
								readableSize = (*titleSocket->recvPacketQueue.begin())->dataFilledSize;
							}
						}
						else {
							for (XTS_RECV_PACKET* recvPacketNext : titleSocket->recvPacketQueue) {
								readableSize += recvPacketNext->dataFilledSize;
							}
						}
						itrIoctl->second = readableSize;
						*command_value = readableSize;
						break;
					}
					default: {
						__debugbreak();
						break;
					}
				}
				break;
			}
			default: {
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s on title_socket_handle (0x%zx) unknown command (0x%08x) value (0x%08x)."
					, __func__
					, title_socket_handle
					, command
					, *command_value
				);
				WSASetLastError(WSAEFAULT);
				return SOCKET_ERROR;
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #7
int32_t WINAPI XSocketSetSockOpt(SOCKET title_socket_handle, int32_t option_level, int32_t option_name, const char* option_value, int32_t option_size)
{
	TRACE_FX();
	if (!option_value) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s option_value is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		uint32_t optionValue = 0;
		memcpy(&optionValue, option_value, option_size > 4 ? 4 : option_size);
		
		bool unknown = true;
		
		switch (option_level) {
			case SOL_SOCKET: {
				switch (option_name) {
					case SO_BROADCAST: {
						unknown = false;
						titleSocket->udpIsBroadcastEnabled = optionValue > 0;
						break;
					}
					case SO_RCVBUF: {
						unknown = false;
						if (optionValue > xlive_network_rcvbuf) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
								, "%s on title_socket_handle (0x%zx) SO_RCVBUF set to (0x%08x) which is bigger than xlive_network_rcvbuf (0x%08x)."
								, __func__
								, title_socket_handle
								, optionValue
								, xlive_network_rcvbuf
							);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
								, "%s on title_socket_handle (0x%zx) SO_RCVBUF set (0x%08x)."
								, __func__
								, title_socket_handle
								, optionValue
							);
						}
						break;
					}
					case SO_SNDBUF: {
						unknown = false;
						if (optionValue > xlive_network_sndbuf && titleSocket->protocol == IPPROTO_UDP) {
							XllnNetworkSocketSetSendBufferSize(optionValue);
						}
						if (optionValue > xlive_network_sndbuf) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
								, "%s on title_socket_handle (0x%zx) SO_SNDBUF set to (0x%08x) which is bigger than xlive_network_sndbuf (0x%08x)."
								, __func__
								, title_socket_handle
								, optionValue
								, xlive_network_sndbuf
							);
						}
						else {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
								, "%s on title_socket_handle (0x%zx) SO_SNDBUF set (0x%08x)."
								, __func__
								, title_socket_handle
								, optionValue
							);
						}
						break;
					}
					case SO_REUSEADDR: {
						unknown = false;
						if (optionValue) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
								, "%s on title_socket_handle (0x%zx) set SO_REUSEADDR."
								, __func__
								, title_socket_handle
								, optionValue
							);
						}
						break;
					}
					case SO_EXCLUSIVEADDRUSE: {
						unknown = false;
						if (optionValue) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
								, "%s on title_socket_handle (0x%zx) set SO_EXCLUSIVEADDRUSE."
								, __func__
								, title_socket_handle
								, optionValue
							);
						}
						break;
					}
					// Standard options a title may set that need no special emulation: accept and
					// record them instead of failing the call (titles often treat a setsockopt
					// failure as fatal).
					case SO_KEEPALIVE:
					case SO_DONTLINGER:
					case SO_LINGER:
					case SO_DONTROUTE:
					case SO_OOBINLINE: {
						unknown = false;
						break;
					}
					case SO_RCVTIMEO: {
						unknown = false;
						// Winsock passes the timeout as a DWORD of milliseconds. Consumed by the
						// blocking recv wait; 0 means never time out.
						titleSocket->recvTimeoutMs = optionValue;
						break;
					}
					case SO_SNDTIMEO: {
						unknown = false;
						titleSocket->sendTimeoutMs = optionValue;
						break;
					}
				}
				break;
			}
			case IPPROTO_TCP: {
				switch (option_name) {
					// User-space TCP has no Nagle algorithm to toggle: accept and record.
					case TCP_NODELAY: {
						unknown = false;
						break;
					}
				}
				break;
			}
		}
		
		if (unknown) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s on title_socket_handle (0x%zx) unknown option_level (0x%08x) option_name (0x%08x) optionValue (0x%08x)."
				, __func__
				, title_socket_handle
				, option_level
				, option_name
				, optionValue
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		titleSocket->socketOptions[option_name] = optionValue;
		
		XllnWndSocketsInvalidateSockets();
		
		LeaveCriticalSection(&xlive_critsec_sockets);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s on title_socket_handle (0x%zx) set option_level (0x%08x) option_name (0x%08x) optionValue (0x%08x)."
			, __func__
			, title_socket_handle
			, option_level
			, option_name
			, optionValue
		);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #8
int32_t WINAPI XSocketGetSockOpt(SOCKET title_socket_handle, int32_t option_level, int32_t option_name, char* option_value, int32_t* option_size)
{
	TRACE_FX();
	if (!option_value) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s option_value is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (!option_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s option_size is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	bool unknown = false;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

		// Standard readable options that are not stored in the option map.
		if (option_level == SOL_SOCKET && option_name == SO_TYPE) {
			uint32_t socketType = titleSocket->type;
			memcpy(option_value, &socketType, *option_size > 4 ? 4 : *option_size);
			*option_size = sizeof(uint32_t) < (uint32_t)*option_size ? sizeof(uint32_t) : *option_size;

			LeaveCriticalSection(&xlive_critsec_sockets);

			WSASetLastError(ERROR_SUCCESS);
			return ERROR_SUCCESS;
		}
		if (option_level == SOL_SOCKET && option_name == SO_ACCEPTCONN) {
			uint32_t isListening = titleSocket->tcpIsListening ? 1 : 0;
			memcpy(option_value, &isListening, *option_size > 4 ? 4 : *option_size);
			*option_size = sizeof(uint32_t) < (uint32_t)*option_size ? sizeof(uint32_t) : *option_size;

			LeaveCriticalSection(&xlive_critsec_sockets);

			WSASetLastError(ERROR_SUCCESS);
			return ERROR_SUCCESS;
		}

		auto itrOption = titleSocket->socketOptions.find(option_name);
		if (itrOption == titleSocket->socketOptions.end()) {
			int32_t protocol = titleSocket->protocol;

			LeaveCriticalSection(&xlive_critsec_sockets);

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s on title_socket_handle (0x%zx) unknown option_level (0x%08x) option_name (0x%08x) for socket protocol (%d)."
				, __func__
				, title_socket_handle
				, option_level
				, option_name
				, protocol
			);
			WSASetLastError(WSAENOPROTOOPT);
			return SOCKET_ERROR;
		}

		memcpy(option_value, &itrOption->second, *option_size > 4 ? 4 : *option_size);
		*option_size = sizeof(uint32_t) < (uint32_t)*option_size ? sizeof(uint32_t) : *option_size;

		// Reading SO_ERROR returns the pending error and resets it (per the getsockopt contract).
		if (option_level == SOL_SOCKET && option_name == SO_ERROR) {
			itrOption->second = ERROR_SUCCESS;
		}

		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #9
int32_t WINAPI XSocketGetSockName(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size)
{
	TRACE_FX();
	if (!sock_addr_xlive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (!sock_addr_xlive_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive_size is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (*sock_addr_xlive_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s *sock_addr_xlive_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, *sock_addr_xlive_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	
	uint16_t portActual = 0;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		if (!titleSocket->hasBinded) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) has not yet binded."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		portActual = titleSocket->portActual;
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s title_socket_handle (0x%zx) is binded to %hu."
		, __func__
		, title_socket_handle
		, portActual
	);
	
	sockaddr_in* sockAddrIpv4Xlive = ((sockaddr_in*)sock_addr_xlive);
	sockAddrIpv4Xlive->sin_family = AF_INET;
	sockAddrIpv4Xlive->sin_addr.s_addr = htonl(xlln_global_instance_id);
	sockAddrIpv4Xlive->sin_port = htons(portActual);
	*sock_addr_xlive_size = sizeof(sockaddr_in);
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #10
int32_t WINAPI XSocketGetPeerName(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size)
{
	TRACE_FX();
	if (!sock_addr_xlive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (!sock_addr_xlive_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive_size is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (*sock_addr_xlive_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s *sock_addr_xlive_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, *sock_addr_xlive_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	
	uint32_t tcpRemoteInstanceId = 0;
	uint16_t tcpRemoteTitlePort = 0;
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

		if (titleSocket->tcpIsConnected) {
			tcpRemoteInstanceId = titleSocket->tcpRemoteInstanceId;
			tcpRemoteTitlePort = titleSocket->tcpRemoteTitlePort;
		}
		else if (titleSocket->udpIsConnected) {
			tcpRemoteInstanceId = titleSocket->udpRemoteInstanceId;
			tcpRemoteTitlePort = titleSocket->udpRemoteTitlePort;
		}

		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	if (!tcpRemoteInstanceId) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) is not connected to a peer."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAENOTCONN);
		return SOCKET_ERROR;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s title_socket_handle (0x%zx) is connected to NetEntity 0x%08x:%hu."
		, __func__
		, title_socket_handle
		, tcpRemoteInstanceId
		, tcpRemoteTitlePort
	);
	
	sockaddr_in* sockAddrIpv4Xlive = ((sockaddr_in*)sock_addr_xlive);
	sockAddrIpv4Xlive->sin_family = AF_INET;
	sockAddrIpv4Xlive->sin_addr.s_addr = htonl(tcpRemoteInstanceId);
	sockAddrIpv4Xlive->sin_port = htons(tcpRemoteTitlePort);
	*sock_addr_xlive_size = sizeof(sockaddr_in);
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #11
SOCKET WINAPI XSocketBind(SOCKET title_socket_handle, const sockaddr* name, int32_t name_size)
{
	TRACE_FX();
	if (!name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s name is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (name->sa_family != AF_INET && name->sa_family != AF_INET6) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) bind on unsupported socket address family 0x%04hx."
			, __func__
			, title_socket_handle
			, name->sa_family
		);
		
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	if (name->sa_family == AF_INET && name_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s name_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, name_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	if (name->sa_family == AF_INET6 && name_size < sizeof(sockaddr_in6)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s name_size (0x%08x) < sizeof(sockaddr_in6) (0x%zx)."
			, __func__
			, name_size
			, (size_t)sizeof(sockaddr_in6)
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	
	uint16_t portRequested = GetSockAddrPort((const SOCKADDR_STORAGE*)name);
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		if (titleSocket->hasBinded) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) has already binded."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		std::set<uint16_t> takenPorts;
		
		// TCP and UDP have separate port spaces, so only same transport binds contend.
		const bool bindingStream = titleSocket->protocol == IPPROTO_TCP;
		
		for (auto itrTitleSocket : xlive_title_sockets) {
			if (!itrTitleSocket.second->hasBinded) {
				continue;
			}
			if (portRequested) {
				if (bindingStream != (itrTitleSocket.second->protocol == IPPROTO_TCP)) {
					continue;
				}
				// NOTE: SO_EXCLUSIVEADDRUSE acts as a share-ENABLER here, inverted versus real
				// Winsock (where it forbids sharing). Long-standing behaviour that bound titles
				// depend on; deliberately left as-is.
				if (
					portRequested == itrTitleSocket.second->portActual
					&& !(
						(titleSocket->socketOptions[SO_REUSEADDR] || titleSocket->socketOptions[SO_EXCLUSIVEADDRUSE])
						&& (itrTitleSocket.second->socketOptions[SO_REUSEADDR] || itrTitleSocket.second->socketOptions[SO_EXCLUSIVEADDRUSE])
					)
				) {
					SOCKET titleSocketExisting = itrTitleSocket.second->handle;
					
					LeaveCriticalSection(&xlive_critsec_sockets);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) failed to bind to port (%hu) because it was already taken by (0x%zx)."
						, __func__
						, title_socket_handle
						, portRequested
						, titleSocketExisting
					);
					WSASetLastError(WSAEADDRINUSE);
					return SOCKET_ERROR;
				}
			}
			else {
				takenPorts.insert(itrTitleSocket.second->portActual);
			}
		}
		
		titleSocket->portRequested = portRequested;
		titleSocket->portActual = portRequested;
		if (titleSocket->portActual == 0) {
			// generate an internally unused ephemeral port.
			const uint16_t base = 0xC000;
			const uint16_t range = 0xFFFF - base;
			do {
				titleSocket->portActual = base + (rand() % range);
			} while (takenPorts.count(titleSocket->portActual));
		}
		
		auto itrTspaToTs = xlive_title_socket_port_actual_to_title_socket.find(std::make_pair(bindingStream, titleSocket->portActual));
		if (itrTspaToTs != xlive_title_socket_port_actual_to_title_socket.end()) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO | XLLN_LOG_LEVEL_WARN
				, "%s title_socket_handle (0x%zx) binding to shared port (%hu) now overwriting title_socket_handle (0x%zx)."
				, __func__
				, title_socket_handle
				, titleSocket->portActual
				, itrTspaToTs->second->handle
			);
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s title_socket_handle (0x%zx) binded to port (%hu)."
				, __func__
				, title_socket_handle
				, titleSocket->portActual
			);
		}
		
		xlive_title_socket_port_actual_to_title_socket[std::make_pair(bindingStream, titleSocket->portActual)] = titleSocket;
		
		titleSocket->hasBinded = true;
		
		XllnWndSocketsInvalidateSockets();
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #12
int32_t WINAPI XSocketConnect(SOCKET title_socket_handle, const sockaddr* sock_addr_xlive, int32_t sock_addr_xlive_size)
{
	TRACE_FX();
	if (!sock_addr_xlive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (sock_addr_xlive_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, sock_addr_xlive_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	if (sock_addr_xlive->sa_family != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) bind on unsupported socket address family 0x%04hx."
			, __func__
			, title_socket_handle
			, sock_addr_xlive->sa_family
		);
		
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	
	const uint32_t ipv4NBO = ((sockaddr_in*)sock_addr_xlive)->sin_addr.s_addr;
	uint32_t instanceId = ntohl(ipv4NBO);
	const uint16_t portHBO = GetSockAddrPort((const SOCKADDR_STORAGE*)sock_addr_xlive);
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

		if (titleSocket->type == SOCK_DGRAM) {
			// Connected-UDP: connect() on a datagram socket sets (or, for INADDR_ANY,
			// dissolves) the default peer used by send()/WSASend() without an address.
			// There is no handshake; the call completes immediately.
			if (instanceId == INADDR_ANY) {
				titleSocket->udpIsConnected = false;
				titleSocket->udpRemoteInstanceId = 0;
				titleSocket->udpRemoteTitlePort = 0;
			}
			else {
				titleSocket->udpRemoteInstanceId = instanceId;
				titleSocket->udpRemoteTitlePort = portHBO;
				titleSocket->udpIsConnected = true;
				SocketImplicitBind_(titleSocket);
			}

			XllnWndSocketsInvalidateSockets();

			LeaveCriticalSection(&xlive_critsec_sockets);

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s title_socket_handle (0x%zx) connected datagram socket to NetEntity 0x%08x:%hu."
				, __func__
				, title_socket_handle
				, instanceId
				, portHBO
			);

			WSASetLastError(ERROR_SUCCESS);
			return ERROR_SUCCESS;
		}

		if (titleSocket->protocol != IPPROTO_TCP) {
			int32_t protocol = titleSocket->protocol;

			LeaveCriticalSection(&xlive_critsec_sockets);

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) protocol (%d) is not TCP (%d)."
				, __func__
				, title_socket_handle
				, protocol
				, IPPROTO_TCP
			);
			WSASetLastError(WSAEOPNOTSUPP);
			return SOCKET_ERROR;
		}

		if (titleSocket->tcpIsListening) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) is already listening for connections."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->tcpIsConnected) {
			uint32_t tcpRemoteInstanceId = titleSocket->tcpRemoteInstanceId;
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) is already connected to instance (0x%08x)."
				, __func__
				, title_socket_handle
				, tcpRemoteInstanceId
			);
			WSASetLastError(WSAEISCONN);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->tcpIsConnecting) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) is already connecting to a remote instance."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEALREADY);
			return SOCKET_ERROR;
		}
		
		if (instanceId == INADDR_BROADCAST) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) cannot connect to INADDR_BROADCAST."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEADDRNOTAVAIL);
			return SOCKET_ERROR;
		}
		
		if (instanceId == INADDR_ANY) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) cannot connect to INADDR_ANY."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEADDRNOTAVAIL);
			return SOCKET_ERROR;
		}
		
		if (instanceId == INADDR_LOOPBACK) {
			instanceId = xlln_global_instance_id;
		}
		
		titleSocket->tcpConnectionAttempts = 0;
		SetTimeUnset(titleSocket->tcpConnectionAttemptLast);
		SetTimeUnset(titleSocket->tcpLastResponse);
		titleSocket->tcpRemoteInstanceId = instanceId;
		titleSocket->tcpRemoteTitlePort = portHBO;
		titleSocket->tcpIsConnecting = true;
		SocketImplicitBind_(titleSocket);
		xlive_title_socket_tcp_connected_sockets[std::make_pair(titleSocket->tcpRemoteInstanceId, titleSocket->tcpRemoteTitlePort)] = titleSocket;
		SetEvent(xlln_tcp_sockets_update);
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s title_socket_handle (0x%zx) is connecting to instanceId (0x%08x) titlePort (%hu)."
			, __func__
			, titleSocket->handle
			, titleSocket->tcpRemoteInstanceId
			, titleSocket->tcpRemoteTitlePort
		);
		
		if (!titleSocket->isBlocking) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			WSASetLastError(WSAEWOULDBLOCK);
			return SOCKET_ERROR;
		}
		
		ResetEvent(titleSocket->tcpConnectNotify);
		
		EnterCriticalSection(&titleSocket->tcpCritSec);
		titleSocket->tcpInCritSec = true;
		LeaveCriticalSection(&xlive_critsec_sockets);
		
		DWORD resultWait = WaitForSingleObject(titleSocket->tcpConnectNotify, INFINITE);
		if (resultWait != WAIT_OBJECT_0) {
			__debugbreak();
		}
		uint32_t resultCode = titleSocket->socketOptions[SO_ERROR];
		
		titleSocket->tcpInCritSec = false;
		LeaveCriticalSection(&titleSocket->tcpCritSec);
		
		if (resultCode) {
			WSASetLastError(resultCode);
			return SOCKET_ERROR;
		}
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #13
int32_t WINAPI XSocketListen(SOCKET title_socket_handle, int32_t backlog)
{
	TRACE_FX();
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		if (titleSocket->protocol != IPPROTO_TCP) {
			int32_t protocol = titleSocket->protocol;
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) protocol (%d) is not TCP (%d)."
				, __func__
				, title_socket_handle
				, protocol
				, IPPROTO_TCP
			);
			WSASetLastError(WSAEOPNOTSUPP);
			return SOCKET_ERROR;
		}
		
		if (!titleSocket->hasBinded) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) has not yet binded."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->tcpIsConnected) {
			uint32_t tcpRemoteInstanceId = titleSocket->tcpRemoteInstanceId;
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) is already connected to instance (0x%08x)."
				, __func__
				, title_socket_handle
				, tcpRemoteInstanceId
			);
			WSASetLastError(WSAEISCONN);
			return SOCKET_ERROR;
		}
		
		titleSocket->tcpIsListening = true;
		titleSocket->tcpConnectionBacklogLimit = backlog;
		
		XllnWndSocketsInvalidateSockets();
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #14
SOCKET WINAPI XSocketAccept(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size)
{
	TRACE_FX();
	// Both address arguments are optional: accept(s, NULL, NULL) is the documented way to
	// accept without retrieving the peer address.
	if (!!sock_addr_xlive != !!sock_addr_xlive_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s sock_addr_xlive or sock_addr_xlive_size is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return INVALID_SOCKET;
	}
	if (sock_addr_xlive_size && *sock_addr_xlive_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s *sock_addr_xlive_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, *sock_addr_xlive_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEINVAL);
		return INVALID_SOCKET;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_sockets);
		
		auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
		if (itrTitleSocket == xlive_title_sockets.end()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) does not exist."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAENOTSOCK);
			return SOCKET_ERROR;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;
		
		if (titleSocket->protocol != IPPROTO_TCP) {
			int32_t protocol = titleSocket->protocol;
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) protocol (%d) is not TCP (%d)."
				, __func__
				, title_socket_handle
				, protocol
				, IPPROTO_TCP
			);
			WSASetLastError(WSAEOPNOTSUPP);
			return SOCKET_ERROR;
		}
		
		if (!titleSocket->tcpIsListening) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) is not listening yet."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINVAL);
			return SOCKET_ERROR;
		}
		
		__timeb64 timeCurrent;
		_ftime64_s(&timeCurrent);
		
		// Empty TCP backlog of old entries.
		for (auto itrPendingConnection = titleSocket->tcpConnectionBacklog.begin(); itrPendingConnection != titleSocket->tcpConnectionBacklog.end(); ) {
			if (TimeDiffMilliseconds(itrPendingConnection->second.lastResponse, timeCurrent) > (xlln_tcp_max_connection_attempts * xlln_tcp_packet_timeout)) {
				titleSocket->tcpConnectionBacklog.erase(itrPendingConnection++);
			}
			else {
				itrPendingConnection++;
			}
		}
		
		if (titleSocket->isBlocking) {
			if (!titleSocket->tcpConnectionBacklog.size()) {
				titleSocket->tcpIsAccepting = true;

				// Mark tcpInCritSec (NOT recvInCritSec): closesocket's teardown barrier
				// synchronizes on tcpCritSec via tcpInCritSec, so using the recv flag here
				// would let closesocket free the socket while this thread is still inside it.
				EnterCriticalSection(&titleSocket->tcpCritSec);
				titleSocket->tcpInCritSec = true;
				LeaveCriticalSection(&xlive_critsec_sockets);

				DWORD resultWait = WaitForSingleObject(titleSocket->tcpConnectNotify, INFINITE);
				if (resultWait != WAIT_OBJECT_0) {
					__debugbreak();
				}

				if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_INACTIVE) {

					titleSocket->tcpIsAccepting = false;
					titleSocket->tcpInCritSec = false;
					LeaveCriticalSection(&titleSocket->tcpCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) socket has been closed."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAEINTR);
					return INVALID_SOCKET;
				}

				// Never re-acquire the global lock while still holding tcpCritSec: closesocket
				// holds the global and blocks on the tcpCritSec teardown barrier, so taking
				// them in this order deadlocks against it. Release tcpCritSec first, then
				// re-resolve the socket (it may have been torn down while unlocked).
				titleSocket->tcpIsAccepting = false;
				titleSocket->tcpInCritSec = false;
				LeaveCriticalSection(&titleSocket->tcpCritSec);

				EnterCriticalSection(&xlive_critsec_sockets);

				auto itrTitleSocketAgain = xlive_title_sockets.find(title_socket_handle);
				if (itrTitleSocketAgain == xlive_title_sockets.end()) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) was closed while waiting."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAEINTR);
					return INVALID_SOCKET;
				}
				titleSocket = itrTitleSocketAgain->second;
			}
		}
		else {
			if (!titleSocket->tcpConnectionBacklog.size()) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				WSASetLastError(WSAEWOULDBLOCK);
				return SOCKET_ERROR;
			}
		}
		
		SOCKET resultSocket = SOCKET_ERROR;
		uint32_t tcpRemoteInstanceId = 0;
		uint16_t tcpRemoteTitlePort = 0;
		{
			auto itrTcpConnectionPending = titleSocket->tcpConnectionBacklog.begin();
			if (itrTcpConnectionPending == titleSocket->tcpConnectionBacklog.end()) {
				// Raced another accept for the only pending connection (or a spurious wake).
				bool isBlocking = titleSocket->isBlocking;

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) woke with an empty connection backlog."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(isBlocking ? WSAEINTR : WSAEWOULDBLOCK);
				return INVALID_SOCKET;
			}
			tcpRemoteInstanceId = itrTcpConnectionPending->second.remoteInstanceId;
			tcpRemoteTitlePort = itrTcpConnectionPending->second.remoteInstanceTitlePort;
			titleSocket->tcpConnectionBacklog.erase(itrTcpConnectionPending);
		}
		{
			XLIVE_TITLE_SOCKET* titleSocketConnection = new XLIVE_TITLE_SOCKET(titleSocket->protocol);
			titleSocketConnection->type = titleSocket->type;
			titleSocketConnection->protocol = titleSocket->protocol;
			titleSocketConnection->isVdpProtocol = titleSocket->isVdpProtocol;
			titleSocketConnection->portRequested = titleSocket->portRequested;
			titleSocketConnection->portActual = titleSocket->portActual;
			titleSocketConnection->hasBinded = titleSocket->hasBinded;
			titleSocketConnection->hasShutdown = titleSocket->hasShutdown;
			titleSocketConnection->isBlocking = titleSocket->isBlocking;
			titleSocketConnection->socketOptions = titleSocket->socketOptions;
			titleSocketConnection->socketIoctls = titleSocket->socketIoctls;
			
			titleSocketConnection->tcpIsConnected = true;
			titleSocketConnection->tcpRemoteInstanceId = tcpRemoteInstanceId;
			titleSocketConnection->tcpRemoteTitlePort = tcpRemoteTitlePort;
			
			resultSocket = titleSocketConnection->handle;
			xlive_title_sockets[titleSocketConnection->handle] = titleSocketConnection;
			xlive_title_socket_tcp_connected_sockets[std::make_pair(titleSocketConnection->tcpRemoteInstanceId, titleSocketConnection->tcpRemoteTitlePort)] = titleSocketConnection;
			SetEvent(xlln_tcp_sockets_update);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s title_socket_handle (0x%zx) connected socket has been created to instanceId (0x%08x) titlePort (%hu)."
				, __func__
				, titleSocketConnection->handle
				, titleSocketConnection->tcpRemoteInstanceId
				, titleSocketConnection->tcpRemoteTitlePort
			);
		}
		
		// accept() is FD_ACCEPT's re-enabling routine: Winsock re-records the event when the
		// backlog is still not empty, so a title that accepts one connection per wake keeps
		// draining instead of stalling until the peer's next CONNECT retransmit.
		if (titleSocket->tcpConnectionBacklog.size() && titleSocket->wsaEventSelectAccept != WSA_INVALID_EVENT) {
			SetEvent(titleSocket->wsaEventSelectAccept);
		}

		XllnWndSocketsInvalidateSockets();

		titleSocket = 0;

		LeaveCriticalSection(&xlive_critsec_sockets);

		if (sock_addr_xlive) {
			sockaddr_in* sockAddrIpv4Xlive = ((sockaddr_in*)sock_addr_xlive);
			sockAddrIpv4Xlive->sin_family = AF_INET;
			sockAddrIpv4Xlive->sin_addr.s_addr = htonl(tcpRemoteInstanceId);
			sockAddrIpv4Xlive->sin_port = htons(tcpRemoteTitlePort);
			*sock_addr_xlive_size = sizeof(sockaddr_in);
		}

		WSASetLastError(ERROR_SUCCESS);
		return resultSocket;
	}
}

// Readiness tests for XSocketSelect. select() readiness must be LEVEL-triggered: a socket
// stays readable while anything remains queued and writable while a send would not block,
// regardless of whether the notify flag edge has already been consumed by an earlier call.
// The caller must hold xlive_critsec_sockets.
static bool TitleSocketSelectReadable_(XLIVE_TITLE_SOCKET* titleSocket)
{
	if (titleSocket->selectNotifyReadPending) {
		return true;
	}
	if (titleSocket->recvPacketQueue.size()) {
		return true;
	}
	// A listening socket is readable when a connection is waiting to be accepted.
	if (titleSocket->tcpIsListening && titleSocket->tcpConnectionBacklog.size()) {
		return true;
	}
	// EOF and shutdown are PERSISTENT readiness: recv() would return immediately (0 for an
	// orderly close, WSAESHUTDOWN after shutdown), and real select keeps reporting a
	// closed/reset connection readable on every call, not just for one notify edge.
	if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {
		return true;
	}
	if (titleSocket->tcpIsConnected && titleSocket->tcpIsClosing) {
		return true;
	}
	return false;
}
static bool TitleSocketSelectWritable_(XLIVE_TITLE_SOCKET* titleSocket)
{
	if (titleSocket->selectNotifyWritePending) {
		return true;
	}
	// A datagram socket can always take another send unless a blocking send is in flight.
	if (titleSocket->type == SOCK_DGRAM && !titleSocket->sendInCritSec) {
		return true;
	}
	// A connected stream socket is writable unless a blocking send is in flight.
	if (titleSocket->protocol == IPPROTO_TCP && titleSocket->tcpIsConnected && !titleSocket->sendInCritSec) {
		return true;
	}
	return false;
}

// #15
int32_t WINAPI XSocketSelect(int32_t title_socket_count, fd_set* title_sockets_read, fd_set* title_sockets_write, fd_set* title_sockets_except, const timeval* select_timeout)
{
	TRACE_FX();
	// Note: the first parameter (nfds) is ignored by Winsock select and exists only for
	// Berkeley-sockets source compatibility, so any value a Title passes is legal here.
	if (!title_sockets_read && !title_sockets_write && !title_sockets_except) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_sockets_read, title_sockets_write and title_sockets_except are all NULL."
			, __func__
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	if (
		(!title_sockets_read || (title_sockets_read && !title_sockets_read->fd_count))
		&& (!title_sockets_write || (title_sockets_write && !title_sockets_write->fd_count))
		&& (!title_sockets_except || (title_sockets_except && !title_sockets_except->fd_count))
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_sockets_read, title_sockets_write and title_sockets_except all do not contain any sockets."
			, __func__
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	
	uint32_t timeout = 0;
	if (!select_timeout) {
		timeout = INFINITE;
	}
	else if (select_timeout->tv_sec < 0 || select_timeout->tv_usec < 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s select_timeout is negative."
			, __func__
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}
	else if (select_timeout->tv_sec != 0 || select_timeout->tv_usec != 0) {
		// timeval carries seconds + MICROseconds. Convert to milliseconds rounding up so a
		// sub-millisecond request still waits instead of degrading to a zero-timeout poll.
		uint64_t timeoutMs = ((uint64_t)select_timeout->tv_sec * 1000) + (((uint64_t)select_timeout->tv_usec + 999) / 1000);
		timeout = timeoutMs >= INFINITE ? (INFINITE - 1) : (uint32_t)timeoutMs;
	}

	uint64_t waitDeadline = 0;
	if (timeout != 0 && timeout != INFINITE) {
		waitDeadline = GetTickCount64() + timeout;
	}

	EnterCriticalSection(&xlive_critsec_sockets);

	// select() flow: poll first, and only when nothing is ready park on the per-socket notify
	// events until something might be, then poll again. The poll is the source of truth (the
	// events only wake the wait), so readiness stays level-triggered.
	while (true) {
		int32_t readyCount = 0;

		for (size_t iSet = 0; iSet < 3; iSet++) {
			fd_set* title_sockets = 0;
			switch (iSet) {
				case 0: {
					title_sockets = title_sockets_read;
					break;
				}
				case 1: {
					title_sockets = title_sockets_write;
					break;
				}
				case 2: {
					title_sockets = title_sockets_except;
					break;
				}
			}

			if (!title_sockets) {
				continue;
			}

			for (u_int iSocket = 0; iSocket < title_sockets->fd_count; iSocket++) {
				SOCKET &title_socket_handle = title_sockets->fd_array[iSocket];

				auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
				if (itrTitleSocket == xlive_title_sockets.end()) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) does not exist in %s at iSocket (0x%08x)."
						, __func__
						, title_socket_handle
						, (iSet == 0 ? "READ" : (iSet == 1 ? "WRITE" : "EXCEPT"))
						, iSocket
					);
					WSASetLastError(WSAENOTSOCK);
					return SOCKET_ERROR;
				}

				XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

				switch (iSet) {
					case 0: {
						if (TitleSocketSelectReadable_(titleSocket)) {
							readyCount++;
						}
						break;
					}
					case 1: {
						if (TitleSocketSelectWritable_(titleSocket)) {
							readyCount++;
						}
						break;
					}
					case 2: {
						if (titleSocket->selectNotifyExceptPending) {
							readyCount++;
						}
						break;
					}
				}
			}
		}

		if (readyCount || timeout == 0) {
			break;
		}

		// Nothing ready: arm a wait on every involved socket's notify events.
		std::set<HANDLE> waitForEvents;

		for (size_t iSet = 0; iSet < 3; iSet++) {
			fd_set* title_sockets = (iSet == 0 ? title_sockets_read : (iSet == 1 ? title_sockets_write : title_sockets_except));
			if (!title_sockets) {
				continue;
			}

			for (u_int iSocket = 0; iSocket < title_sockets->fd_count; iSocket++) {
				SOCKET &title_socket_handle = title_sockets->fd_array[iSocket];

				auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
				if (itrTitleSocket == xlive_title_sockets.end()) {
					continue;
				}

				XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

				switch (iSet) {
					case 0: {
						waitForEvents.insert(titleSocket->selectNotifyRead);
						break;
					}
					case 1: {
						waitForEvents.insert(titleSocket->selectNotifyWrite);
						break;
					}
					case 2: {
						waitForEvents.insert(titleSocket->selectNotifyExcept);
						break;
					}
				}
			}
		}

		LeaveCriticalSection(&xlive_critsec_sockets);

		uint32_t waitEventsCount = 0;
		HANDLE* waitEvents = new HANDLE[waitForEvents.size()];

		for (HANDLE event : waitForEvents) {
			waitEvents[waitEventsCount++] = event;
		}
		waitForEvents.clear();

		uint32_t timeoutRemaining = INFINITE;
		if (timeout != INFINITE) {
			uint64_t timeNow = GetTickCount64();
			timeoutRemaining = (timeNow >= waitDeadline) ? 0 : (uint32_t)(waitDeadline - timeNow);
		}

		uint32_t resultWait = WaitForMultipleObjects(waitEventsCount, waitEvents, FALSE, timeoutRemaining);

		delete[] waitEvents;
		waitEvents = 0;

		if (resultWait == WAIT_TIMEOUT) {
			// The timeout elapsing is a normal select() outcome: report that no sockets are
			// ready (empty sets, return 0) rather than an error.
			if (title_sockets_read) {
				title_sockets_read->fd_count = 0;
			}
			if (title_sockets_write) {
				title_sockets_write->fd_count = 0;
			}
			if (title_sockets_except) {
				title_sockets_except->fd_count = 0;
			}
			WSASetLastError(ERROR_SUCCESS);
			return 0;
		}
		if (resultWait < WAIT_OBJECT_0 || resultWait > WAIT_OBJECT_0 + waitEventsCount - 1) {
			uint32_t errorWait = GetLastError();
			XLLN_DEBUG_LOG_ECODE(errorWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s WaitForMultipleObjects failed with result 0x%08x."
				, __func__
				, resultWait
			);
			WSASetLastError(WSAEINTR);
			return SOCKET_ERROR;
		}

		EnterCriticalSection(&xlive_critsec_sockets);
		// Woken: loop back and poll again. If the wakeup raced another consumer the poll finds
		// nothing and we wait again for whatever remains of the timeout.
	}

	int32_t resultCount = 0;

	for (size_t iSet = 0; iSet < 3; iSet++) {
		fd_set* title_sockets = 0;
		switch (iSet) {
			case 0: {
				title_sockets = title_sockets_read;
				break;
			}
			case 1: {
				title_sockets = title_sockets_write;
				break;
			}
			case 2: {
				title_sockets = title_sockets_except;
				break;
			}
			default: {
				__debugbreak();
			}
		}
		
		if (!title_sockets) {
			continue;
		}
		
		u_int fdNewCount = 0;
		
		for (u_int iSocket = 0; iSocket < title_sockets->fd_count; iSocket++) {
			SOCKET &title_socket_handle = title_sockets->fd_array[iSocket];
			
			auto itrTitleSocket = xlive_title_sockets.find(title_socket_handle);
			XLIVE_TITLE_SOCKET* titleSocket = 0;
			if (itrTitleSocket != xlive_title_sockets.end()) {
				titleSocket = itrTitleSocket->second;
			}
			
			if (!titleSocket && timeout == 0) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) does not exist in %s at iSocket (0x%08x)."
					, __func__
					, title_socket_handle
					, (iSet == 0 ? "READ" : (iSet == 1 ? "WRITE" : "EXCEPT"))
					, iSocket
				);
				WSASetLastError(WSAENOTSOCK);
				return SOCKET_ERROR;
			}
			
			if (!titleSocket && iSet != 2) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) no longer exists in %s at iSocket (0x%08x)."
					, __func__
					, title_socket_handle
					, (iSet == 0 ? "READ" : (iSet == 1 ? "WRITE" : "EXCEPT"))
					, iSocket
				);
				continue;
			}
			
			if (titleSocket) {
				bool skip = true;
				switch (iSet) {
					case 0: {
						if (TitleSocketSelectReadable_(titleSocket)) {
							titleSocket->selectNotifyReadPending = false;
							ResetEvent(titleSocket->selectNotifyRead);
							skip = false;
						}
						break;
					}
					case 1: {
						if (TitleSocketSelectWritable_(titleSocket)) {
							titleSocket->selectNotifyWritePending = false;
							ResetEvent(titleSocket->selectNotifyWrite);
							skip = false;
						}
						break;
					}
					case 2: {
						if (titleSocket->selectNotifyExceptPending) {
							titleSocket->selectNotifyExceptPending = false;
							ResetEvent(titleSocket->selectNotifyExcept);
							skip = false;
						}
						break;
					}
				}
				if (skip) {
					continue;
				}
			}
			
			title_sockets->fd_array[fdNewCount++] = title_socket_handle;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s RESULT title_socket_handle (0x%zx) in %s at iSocket (0x%08x)."
				, __func__
				, title_socket_handle
				, (iSet == 0 ? "READ" : (iSet == 1 ? "WRITE" : "EXCEPT"))
				, iSocket
			);
		}
		
		resultCount += fdNewCount;
		title_sockets->fd_count = fdNewCount;
	}
	
	LeaveCriticalSection(&xlive_critsec_sockets);
	
	WSASetLastError(ERROR_SUCCESS);
	return resultCount;
}

// #18
int32_t WINAPI XSocketRecv(SOCKET title_socket_handle, char* data_buffer, int32_t data_buffer_size, int32_t flags)
{
	TRACE_FX();
	int32_t result = XSocketRecvFrom(title_socket_handle, data_buffer, data_buffer_size, flags, 0, 0);
	return result;
}

// #20
int32_t WINAPI XSocketRecvFrom(SOCKET title_socket_handle, char* data_buffer, int32_t data_buffer_size, int32_t flags, sockaddr* address_from, int32_t* address_from_size)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) flags value (0x%08x) must be 0."
			, __func__
			, title_socket_handle
			, flags
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (!data_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) data_buffer is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	WSABUF wsabuf;
	wsabuf.buf = data_buffer;
	wsabuf.len = data_buffer_size;
	uint32_t wsaFlags = 0;
	uint32_t received_size = 0;
	int32_t resultXWsaRecvFrom = XWSARecvFrom(title_socket_handle, &wsabuf, 1, &received_size, &wsaFlags, address_from, address_from_size, 0, 0);
	if (resultXWsaRecvFrom == ERROR_SUCCESS) {
		return received_size;
	}
	return SOCKET_ERROR;
}

// #22
int32_t WINAPI XSocketSend(SOCKET title_socket_handle, const char* data_buffer, int32_t data_buffer_size, int32_t flags)
{
	TRACE_FX();
	int32_t result = XSocketSendTo(title_socket_handle, data_buffer, data_buffer_size, flags, 0, 0);
	return result;
}

// #24
int32_t WINAPI XSocketSendTo(SOCKET title_socket_handle, const char* data_buffer, int32_t data_buffer_size, int32_t flags, sockaddr* address_to, int32_t address_to_size)
{
	TRACE_FX();
	if (!data_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) data_buffer is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	
	WSABUF wsabuf;
	wsabuf.buf = (char*)data_buffer;
	wsabuf.len = data_buffer_size;
	uint32_t sent_size = 0;
	int32_t resultXWsaRecvFrom = XWSASendTo(title_socket_handle, &wsabuf, 1, &sent_size, flags, address_to, address_to_size, 0, 0);
	if (resultXWsaRecvFrom == ERROR_SUCCESS) {
		return sent_size;
	}
	return SOCKET_ERROR;
}

// #26
uint32_t WINAPI XSocketInet_Addr(const char* ipv4_address)
{
	TRACE_FX();
	
	in_addr resultAddress;
	if (!WS2_32_inet_pton(AF_INET, ipv4_address, &resultAddress)) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s inet_pton(\"%s\")."
			, __func__
			, ipv4_address
		);
		WSASetLastError(errorCode);
		return htonl(INADDR_NONE);
	}
	return resultAddress.S_un.S_addr;
}

// #27
int32_t WINAPI XSocketWSAGetLastError()
{
	TRACE_FX();
	int32_t result = WSAGetLastError();
	return result;
}

// #37
unsigned long WINAPI XSocketHTONL(unsigned long host_order)
{
	TRACE_FX();
	unsigned long networkOrder = htonl(host_order);
	return networkOrder;
}

// #38
unsigned short WINAPI XSocketNTOHS(unsigned short network_order)
{
	TRACE_FX();
	unsigned short hostOrder = ntohs(network_order);
	return hostOrder;
}

// #39
unsigned long WINAPI XSocketNTOHL(unsigned long network_order)
{
	TRACE_FX();
	unsigned long hostOrder = ntohl(network_order);
	return hostOrder;
}

// #40
unsigned short WINAPI XSocketHTONS(unsigned short host_order)
{
	TRACE_FX();
	unsigned short networkOrder = htons(host_order);
	return networkOrder;
}

bool InitXSocket()
{
	TRACE_FX();
	
	XllnThreadKeepAliveStart();
	XLiveThreadQosStart();
	
	return true;
}

bool UninitXSocket()
{
	TRACE_FX();
	
	XLiveThreadQosStop();
	XllnThreadKeepAliveStop();
	
	return true;
}
