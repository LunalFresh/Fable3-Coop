#include <winsock2.h>
#include "xdefs.hpp"
#include "xfuncs.hpp"
#include "xwsa.hpp"
#include "xnet.hpp"
#include "xsocket.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-network.hpp"
#include "../xlln/wnd-sockets.hpp"

static uint32_t xlive_wsa_initialised_count = 0;

// #1
int32_t WINAPI XWSAStartup(uint16_t version_requested, WSADATA* wsa_data)
{
	TRACE_FX();
	if (!wsa_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_data is NULL."
			, __func__
		);
		return WSAEFAULT;
	}
	
	wsa_data->iMaxSockets = 0;
	wsa_data->iMaxUdpDg = 0;
	wsa_data->lpVendorInfo = 0;
	strncpy_s(wsa_data->szDescription, "XLiveLessNess WinSock 2", WSADESCRIPTION_LEN);
	strncpy_s(wsa_data->szSystemStatus, "Running", WSASYS_STATUS_LEN);
	wsa_data->wHighVersion = version_requested;
	wsa_data->wVersion = version_requested;
	
	if (!xlive_net_initialized) {
		int32_t result = XNetStartup(0);
		if (result != ERROR_SUCCESS) {
			XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XNetStartup."
				, __func__
			);
			return result;
		}
	}
	
	xlive_wsa_initialised_count++;
	
	return ERROR_SUCCESS;
}

// #2
int32_t WINAPI XWSACleanup()
{
	TRACE_FX();
	if (xlive_wsa_initialised_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s WSANOTINITIALISED."
			, __func__
		);
		WSASetLastError(WSANOTINITIALISED);
		return SOCKET_ERROR;
	}
	xlive_wsa_initialised_count--;
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #16
BOOL WINAPI XWSAGetOverlappedResult(SOCKET title_socket_handle, WSAOVERLAPPED* wsa_overlapped, uint32_t* transferred_size, BOOL wait_for_completion, uint32_t* flags)
{
	TRACE_FX();
	if (!wsa_overlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_overlapped is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return FALSE;
	}
	if (!transferred_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) transferred_size is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return FALSE;
	}
	if (!flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) flags is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return FALSE;
	}
	
	*transferred_size = 0;
	*flags = 0;
	
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
			return FALSE;
		}
		
		XLIVE_TITLE_SOCKET* titleSocket = itrTitleSocket->second;

		size_t iPendingRecv = SIZE_MAX;
		for (size_t iPending = 0; iPending < titleSocket->recvPendingOverlapped.size(); iPending++) {
			if (titleSocket->recvPendingOverlapped[iPending].wsaOverlapped == wsa_overlapped) {
				iPendingRecv = iPending;
				break;
			}
		}

		if (iPendingRecv != SIZE_MAX) {
			// TRACE (not DEBUG): titles like Fable 3 poll this every frame (~175/s) on an
			// event-less overlapped socket, which floods the debug log. Raise to TRACE to see it.
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_TRACE
				, "%s title_socket_handle (0x%zx) RECV wsa_event (0x%zx)."
				, __func__
				, title_socket_handle
				, wsa_overlapped->hEvent
			);

			if (titleSocket->recvInCritSec) {

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) an existing Recv operation is already in progress."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAEINPROGRESS);
				return FALSE;
			}

			if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAESHUTDOWN);
				return FALSE;
			}

			while (wait_for_completion && !titleSocket->recvPacketQueue.size()) {

				EnterCriticalSection(&titleSocket->recvCritSec);
				titleSocket->recvInCritSec = true;
				LeaveCriticalSection(&xlive_critsec_sockets);

				DWORD resultWait = WaitForSingleObject(titleSocket->recvNotify, INFINITE);
				if (resultWait != WAIT_OBJECT_0) {
					__debugbreak();
				}

				if (titleSocket->recvWsaOverlappedCancelled) {

					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) has been cancelled."
						, __func__
						, title_socket_handle
						, wsa_overlapped
					);
					WSASetLastError(WSAECANCELLED);
					return FALSE;
				}

				if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {

					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAESHUTDOWN);
					return FALSE;
				}

				// Never wait on the global lock while still holding recvCritSec - closesocket
				// acquires them in the opposite order (global, then a recvCritSec barrier) and
				// the two threads would deadlock. If the global lock is contended, release
				// recvCritSec first and re-resolve the socket (it may be mid-teardown).
				if (TryEnterCriticalSection(&xlive_critsec_sockets)) {
					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);
				}
				else {
					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					EnterCriticalSection(&xlive_critsec_sockets);

					auto itrTitleSocketAgain = xlive_title_sockets.find(title_socket_handle);
					if (itrTitleSocketAgain == xlive_title_sockets.end()) {

						LeaveCriticalSection(&xlive_critsec_sockets);

						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s title_socket_handle (0x%zx) was closed while waiting."
							, __func__
							, title_socket_handle
						);
						WSASetLastError(WSAENOTSOCK);
						return FALSE;
					}
					titleSocket = itrTitleSocketAgain->second;
				}

				// The registration may have been consumed/cancelled while unlocked: re-resolve.
				iPendingRecv = SIZE_MAX;
				for (size_t iPending = 0; iPending < titleSocket->recvPendingOverlapped.size(); iPending++) {
					if (titleSocket->recvPendingOverlapped[iPending].wsaOverlapped == wsa_overlapped) {
						iPendingRecv = iPending;
						break;
					}
				}
				if (iPendingRecv == SIZE_MAX) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) is no longer pending."
						, __func__
						, title_socket_handle
						, wsa_overlapped
					);
					WSASetLastError(WSA_OPERATION_ABORTED);
					return FALSE;
				}
				// Loop: a spurious wake (or a racing consumer) leaves the queue empty; wait again.
			}

			if (!titleSocket->recvPacketQueue.size()) {

				LeaveCriticalSection(&xlive_critsec_sockets);

				WSASetLastError(WSA_IO_INCOMPLETE);
				return FALSE;
			}

			XTS_PENDING_OVERLAPPED_RECV pendingRecv = titleSocket->recvPendingOverlapped[iPendingRecv];
			titleSocket->recvPendingOverlapped.erase(titleSocket->recvPendingOverlapped.begin() + iPendingRecv);

			auto itrRecvPacket = titleSocket->recvPacketQueue.begin();
			XTS_RECV_PACKET* recvPacket = *itrRecvPacket;

			// Scatter the datagram across the registration's buffers.
			size_t dataWholeSize = recvPacket->dataFilledSize;
			size_t transferredTotal = 0;
			for (size_t iBuffer = 0; iBuffer < pendingRecv.buffers.size() && recvPacket->dataFilledSize; iBuffer++) {
				if (!pendingRecv.buffers[iBuffer].len) {
					continue;
				}
				size_t copySize = pendingRecv.buffers[iBuffer].len < recvPacket->dataFilledSize ? pendingRecv.buffers[iBuffer].len : recvPacket->dataFilledSize;
				memcpy(pendingRecv.buffers[iBuffer].buf, &recvPacket->data[recvPacket->dataConsumedSize], copySize);
				recvPacket->dataConsumedSize += copySize;
				recvPacket->dataFilledSize -= copySize;
				transferredTotal += copySize;
			}
			*transferred_size = transferredTotal;
			bool truncated = recvPacket->dataFilledSize > 0;

			if (pendingRecv.addressFrom) {
				sockaddr_in* sockAddrIpv4Xlive = ((sockaddr_in*)pendingRecv.addressFrom);
				sockAddrIpv4Xlive->sin_family = AF_INET;
				sockAddrIpv4Xlive->sin_addr.s_addr = htonl(recvPacket->remoteInstanceId);
				sockAddrIpv4Xlive->sin_port = htons(recvPacket->remoteInstanceTitlePort);
				*pendingRecv.addressFromSize = sizeof(sockaddr_in);
			}

			// A datagram socket must consume the whole message: a truncated remainder is
			// DISCARDED (and the call reports WSAEMSGSIZE). Only a stream socket keeps the
			// unconsumed remainder queued for the next recv.
			if (truncated && titleSocket->type != SOCK_DGRAM) {
				recvPacket = 0;
			}
			else {
				titleSocket->recvPacketQueue.erase(itrRecvPacket);
			}

			// Make the overlapped self-describing for later reaps/Internal-polling titles.
			wsa_overlapped->Internal = 0;
			wsa_overlapped->InternalHigh = transferredTotal;

			if (titleSocket->recvPacketQueue.size()) {
				// More datagrams queued: keep the socket handle signaled so a title
				// waiting on it (hEvent=NULL overlapped I/O) drains them, and wake the
				// next pending overlapped recv if there is one.
				SetEvent((HANDLE)titleSocket->handle);
				if (titleSocket->wsaEventSelectRead != WSA_INVALID_EVENT) {
					SetEvent(titleSocket->wsaEventSelectRead);
				}
				if (titleSocket->recvPendingOverlapped.size()) {
					WSAOVERLAPPED* headOverlapped = titleSocket->recvPendingOverlapped.front().wsaOverlapped;
					if (headOverlapped->hEvent && headOverlapped->hEvent != INVALID_HANDLE_VALUE) {
						WSASetEvent(headOverlapped->hEvent);
					}
				}
			}
			else {
				// Queue drained: the socket handle must go non-signaled so the next
				// wait on it blocks until fresh data arrives.
				ResetEvent((HANDLE)titleSocket->handle);
			}

			titleSocket = 0;
			LeaveCriticalSection(&xlive_critsec_sockets);

			if (recvPacket) {
				delete recvPacket;
				recvPacket = 0;

				if (truncated) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) data size (%zu) truncated to fit into Title buffer (%zu)."
						, __func__
						, title_socket_handle
						, dataWholeSize
						, transferredTotal
					);
					// BOOL contract: failure must be FALSE (0). SOCKET_ERROR (-1) reads as
					// success to any caller testing the BOOL.
					WSASetLastError(WSAEMSGSIZE);
					return FALSE;
				}
			}
		}
		else if (titleSocket->sendWsaOverlapped == wsa_overlapped) {
			// TRACE (not DEBUG): polled every frame on event-less overlapped sockets; see the RECV note.
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_TRACE
				, "%s title_socket_handle (0x%zx) SEND wsa_event (0x%zx)."
				, __func__
				, title_socket_handle
				, wsa_overlapped->hEvent
			);
			
			if (titleSocket->sendInCritSec) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) an existing Send operation is already in progress."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAEINPROGRESS);
				return FALSE;
			}
			
			if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) socket has been shutdown for sending."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAESHUTDOWN);
				return FALSE;
			}
			
			if (wait_for_completion && !titleSocket->sendCompleted) {
				
				EnterCriticalSection(&titleSocket->sendCritSec);
				titleSocket->sendInCritSec = true;
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				DWORD resultWait = WaitForSingleObject(titleSocket->sendNotify, INFINITE);
				if (resultWait != WAIT_OBJECT_0) {
					__debugbreak();
				}
				
				if (titleSocket->sendWsaOverlappedCancelled) {
					
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) has been cancelled."
						, __func__
						, title_socket_handle
						, wsa_overlapped
					);
					WSASetLastError(WSAECANCELLED);
					return FALSE;
				}
				
				if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND) {
					
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAESHUTDOWN);
					return FALSE;
				}
				
				// Never wait on the global lock while still holding sendCritSec - closesocket
				// acquires them in the opposite order (global, then a sendCritSec barrier) and
				// the two threads would deadlock. If the global lock is contended, release
				// sendCritSec first and re-resolve the socket (it may be mid-teardown).
				if (TryEnterCriticalSection(&xlive_critsec_sockets)) {
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
				}
				else {
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);

					EnterCriticalSection(&xlive_critsec_sockets);

					auto itrTitleSocketAgain = xlive_title_sockets.find(title_socket_handle);
					if (itrTitleSocketAgain == xlive_title_sockets.end()) {

						LeaveCriticalSection(&xlive_critsec_sockets);

						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s title_socket_handle (0x%zx) was closed while waiting."
							, __func__
							, title_socket_handle
						);
						WSASetLastError(WSAENOTSOCK);
						return FALSE;
					}
					titleSocket = itrTitleSocketAgain->second;
				}
			}

			if (!titleSocket->sendCompleted) {

				if (wait_for_completion) {
					__debugbreak();
				}

				LeaveCriticalSection(&xlive_critsec_sockets);

				WSASetLastError(WSA_IO_INCOMPLETE);
				return FALSE;
			}
			
			if (titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT) {
				SetEvent(titleSocket->wsaEventSelectWrite);
			}
			
			titleSocket->sendWsaOverlapped = 0;
			titleSocket->sendWsaOverlappedCancelled = false;
			
			*transferred_size = titleSocket->sendTransferredSize;
			bool sendTransferTruncated = titleSocket->sendTransferTruncated;
			
			titleSocket = 0;
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			if (sendTransferTruncated) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) data transfer truncated."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAEMSGSIZE);
				return FALSE;
			}
		}
		else {

			LeaveCriticalSection(&xlive_critsec_sockets);

			const DWORD kStatusPending = 0x00000103;
			const DWORD kStatusCancelled = 0xC0000120;
			if (wsa_overlapped->Internal != kStatusPending) {
				if (wsa_overlapped->Internal == 0) {
					*transferred_size = wsa_overlapped->InternalHigh;
					WSASetLastError(ERROR_SUCCESS);
					return TRUE;
				}
				if (wsa_overlapped->Internal == kStatusCancelled) {
					// The operation was cancelled (XWSACancelOverlappedIO) before completing.
					WSASetLastError(WSA_OPERATION_ABORTED);
					return FALSE;
				}
			}

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) does not exist for this socket."
				, __func__
				, title_socket_handle
				, wsa_overlapped
			);
			WSASetLastError(WSA_INVALID_PARAMETER);
			return FALSE;
		}
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return TRUE;
}

// #17
int32_t WINAPI XWSACancelOverlappedIO(SOCKET title_socket_handle)
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
			titleSocket->sendWsaOverlappedCancelled = true;
			SetEvent(titleSocket->sendNotify);
			// Wait for it to exit.
			EnterCriticalSection(&titleSocket->sendCritSec);
			LeaveCriticalSection(&titleSocket->sendCritSec);
		}
		if (titleSocket->sendWsaOverlapped && titleSocket->sendWsaOverlapped->hEvent && titleSocket->sendWsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) SEND wsa_event (0x%zx)."
				, __func__
				, title_socket_handle
				, titleSocket->sendWsaOverlapped->hEvent
			);
			WSASetEvent(titleSocket->sendWsaOverlapped->hEvent);
		}
		titleSocket->sendWsaOverlapped = 0;
		titleSocket->sendWsaOverlappedCancelled = false;
		
		if (titleSocket->recvInCritSec) {
			titleSocket->recvWsaOverlappedCancelled = true;
			SetEvent(titleSocket->recvNotify);
			// Wait for it to exit.
			EnterCriticalSection(&titleSocket->recvCritSec);
			LeaveCriticalSection(&titleSocket->recvCritSec);
		}
		for (XTS_PENDING_OVERLAPPED_RECV &pendingRecv : titleSocket->recvPendingOverlapped) {
			// Mark the overlapped as aborted (STATUS_CANCELLED) so a later reap reports
			// WSA_OPERATION_ABORTED instead of finding an unrecognized overlapped.
			pendingRecv.wsaOverlapped->Internal = (ULONG_PTR)0xC0000120;
			pendingRecv.wsaOverlapped->InternalHigh = 0;
			if (pendingRecv.wsaOverlapped->hEvent && pendingRecv.wsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s title_socket_handle (0x%zx) RECV wsa_event (0x%zx)."
					, __func__
					, title_socket_handle
					, pendingRecv.wsaOverlapped->hEvent
				);
				WSASetEvent(pendingRecv.wsaOverlapped->hEvent);
			}
		}
		titleSocket->recvPendingOverlapped.clear();
		titleSocket->recvWsaOverlappedCancelled = false;
		
		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s title_socket_handle (0x%zx)."
		, __func__
		, title_socket_handle
	);
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #19
int32_t WINAPI XWSARecv(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* received_size
	, uint32_t* flags
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
)
{
	TRACE_FX();
	int32_t result = XWSARecvFrom(title_socket_handle, wsa_buffers, wsa_buffer_count, received_size, flags, 0, 0, wsa_overlapped, wsa_completion_routine);
	return result;
}

// #21
int32_t WINAPI XWSARecvFrom(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* received_size
	, uint32_t* flags
	, sockaddr* address_from
	, int32_t* address_from_size
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
)
{
	TRACE_FX();
	if (!flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) flags is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (*flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) flags value (0x%08x) must be 0."
			, __func__
			, title_socket_handle
			, *flags
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (!wsa_buffers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_buffers is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (!wsa_buffer_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_buffer_count is 0."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	// A recv must SCATTER across the whole WSABUF array. Zero-length entries (buf may be
	// NULL) are legal unused slots; only a NULL buf that would actually be written (len > 0)
	// is a real WSAEFAULT.
	for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
		if (!wsa_buffers[iWsaBuffer].buf && wsa_buffers[iWsaBuffer].len) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) wsa_buffers[%u].buf is NULL with len (%u)."
				, __func__
				, title_socket_handle
				, iWsaBuffer
				, wsa_buffers[iWsaBuffer].len
			);
			WSASetLastError(WSAEFAULT);
			return SOCKET_ERROR;
		}
	}
	if (wsa_completion_routine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_completion_routine must be NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (address_from && !address_from_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) address_from_size is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (address_from && *address_from_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx): *address_from_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, title_socket_handle
			, *address_from_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (wsa_overlapped && wsa_overlapped->hEvent == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_overlapped->hEvent is INVALID."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(ERROR_INVALID_HANDLE);
		return SOCKET_ERROR;
	}
	
	if (address_from) {
		address_from->sa_family = AF_UNSPEC;
	}
	if (received_size) {
		*received_size = 0;
	}
	if (flags) {
		*flags = 0;
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
		
		if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAESHUTDOWN);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->protocol == IPPROTO_TCP) {
			if (!titleSocket->tcpIsConnected) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) TCP is not connected."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAENOTCONN);
				return SOCKET_ERROR;
			}
		}
		
		if (titleSocket->recvInCritSec) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) an existing Recv operation is in progress."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINPROGRESS);
			return SOCKET_ERROR;
		}
		if (wsa_overlapped) {
			// Multiple pending overlapped recvs are legal (they complete in arrival order).
			// Re-arming the SAME still-pending overlapped is idempotent: report still-pending
			// rather than WSAEINPROGRESS so the caller keeps waiting instead of treating it
			// as fatal and tearing down the receive.
			for (const XTS_PENDING_OVERLAPPED_RECV &pendingRecv : titleSocket->recvPendingOverlapped) {
				if (pendingRecv.wsaOverlapped == wsa_overlapped) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					WSASetLastError(WSA_IO_PENDING);
					return SOCKET_ERROR;
				}
			}
		}

		titleSocket->recvWsaOverlappedCancelled = false;
		ResetEvent(titleSocket->recvNotify);
		
		if (titleSocket->tcpIsConnected && titleSocket->tcpIsClosing && !titleSocket->recvPacketQueue.size()) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) connection is closing and there is no more data."
				, __func__
				, title_socket_handle
			);
			
			*received_size = 0;
			WSASetLastError(ERROR_SUCCESS);
			return ERROR_SUCCESS;
		}
		
		if (!wsa_overlapped) {

			while (!titleSocket->recvPacketQueue.size()) {

				// Peer performed an orderly close and everything has been read: EOF.
				if (titleSocket->tcpIsConnected && titleSocket->tcpIsClosing) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s title_socket_handle (0x%zx) connection is closing and there is no more data."
						, __func__
						, title_socket_handle
					);

					if (received_size) {
						*received_size = 0;
					}
					WSASetLastError(ERROR_SUCCESS);
					return ERROR_SUCCESS;
				}

				if (!titleSocket->isBlocking) {

					LeaveCriticalSection(&xlive_critsec_sockets);

					WSASetLastError(WSAEWOULDBLOCK);
					return SOCKET_ERROR;
				}

				EnterCriticalSection(&titleSocket->recvCritSec);
				titleSocket->recvInCritSec = true;
				LeaveCriticalSection(&xlive_critsec_sockets);

				// SO_RCVTIMEO: a configured receive timeout bounds the wait (0 = infinite).
				DWORD recvWaitTimeout = titleSocket->recvTimeoutMs ? titleSocket->recvTimeoutMs : INFINITE;
				DWORD resultWait = WaitForSingleObject(titleSocket->recvNotify, recvWaitTimeout);
				if (resultWait == WAIT_TIMEOUT) {

					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) blocking recv timed out (SO_RCVTIMEO %ums)."
						, __func__
						, title_socket_handle
						, recvWaitTimeout
					);
					WSASetLastError(WSAETIMEDOUT);
					return SOCKET_ERROR;
				}
				if (resultWait != WAIT_OBJECT_0) {
					__debugbreak();
				}

				if (titleSocket->recvWsaOverlappedCancelled) {

					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) has been cancelled."
						, __func__
						, title_socket_handle
						, wsa_overlapped
					);
					WSASetLastError(WSAECANCELLED);
					return SOCKET_ERROR;
				}

				if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV) {

					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAESHUTDOWN);
					return SOCKET_ERROR;
				}

				// Never wait on the global lock while still holding recvCritSec - closesocket
				// acquires them in the opposite order (global, then a recvCritSec barrier) and
				// the two threads would deadlock. If the global lock is contended, release
				// recvCritSec first and re-resolve the socket (it may be mid-teardown).
				if (TryEnterCriticalSection(&xlive_critsec_sockets)) {
					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);
				}
				else {
					titleSocket->recvInCritSec = false;
					LeaveCriticalSection(&titleSocket->recvCritSec);

					EnterCriticalSection(&xlive_critsec_sockets);

					auto itrTitleSocketAgain = xlive_title_sockets.find(title_socket_handle);
					if (itrTitleSocketAgain == xlive_title_sockets.end()) {

						LeaveCriticalSection(&xlive_critsec_sockets);

						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s title_socket_handle (0x%zx) was closed while waiting."
							, __func__
							, title_socket_handle
						);
						WSASetLastError(WSAENOTSOCK);
						return SOCKET_ERROR;
					}
					titleSocket = itrTitleSocketAgain->second;
				}
				// Loop: a spurious wake (or a racing consumer) leaves the queue empty; wait again.
			}

			auto itrRecvPacket = titleSocket->recvPacketQueue.begin();
			XTS_RECV_PACKET* recvPacket = *itrRecvPacket;

			// Scatter the message across the title's WSABUF array.
			size_t dataWholeSize = recvPacket->dataFilledSize;
			size_t transferredTotal = 0;
			for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count && recvPacket->dataFilledSize; iWsaBuffer++) {
				if (!wsa_buffers[iWsaBuffer].len) {
					continue;
				}
				size_t copySize = wsa_buffers[iWsaBuffer].len < recvPacket->dataFilledSize ? wsa_buffers[iWsaBuffer].len : recvPacket->dataFilledSize;
				memcpy(wsa_buffers[iWsaBuffer].buf, &recvPacket->data[recvPacket->dataConsumedSize], copySize);
				recvPacket->dataConsumedSize += copySize;
				recvPacket->dataFilledSize -= copySize;
				transferredTotal += copySize;
			}
			if (received_size) {
				*received_size = transferredTotal;
			}
			bool truncated = recvPacket->dataFilledSize > 0;

			if (address_from) {
				sockaddr_in* sockAddrIpv4Xlive = ((sockaddr_in*)address_from);
				sockAddrIpv4Xlive->sin_family = AF_INET;
				sockAddrIpv4Xlive->sin_addr.s_addr = htonl(recvPacket->remoteInstanceId);
				sockAddrIpv4Xlive->sin_port = htons(recvPacket->remoteInstanceTitlePort);
				*address_from_size = sizeof(sockaddr_in);
			}

			// A datagram socket must consume the whole message: a truncated remainder is
			// DISCARDED (and the call reports WSAEMSGSIZE) - splitting one datagram across
			// recv calls silently desyncs message-oriented titles. Only a stream socket
			// keeps the unconsumed remainder queued for the next recv.
			if (truncated && titleSocket->type != SOCK_DGRAM) {
				recvPacket = 0;
			}
			else {
				titleSocket->recvPacketQueue.erase(itrRecvPacket);
			}

			if (titleSocket->wsaEventSelectRead != WSA_INVALID_EVENT && titleSocket->recvPacketQueue.size()) {
				SetEvent(titleSocket->wsaEventSelectRead);
			}

			titleSocket = 0;
			LeaveCriticalSection(&xlive_critsec_sockets);

			if (recvPacket) {
				delete recvPacket;
				recvPacket = 0;

				if (truncated) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) data size (%zu) truncated to fit into Title buffer (%zu)."
						, __func__
						, title_socket_handle
						, dataWholeSize
						, transferredTotal
					);
					WSASetLastError(WSAEMSGSIZE);
					return SOCKET_ERROR;
				}
			}
		}
		else {
			XTS_PENDING_OVERLAPPED_RECV pendingRecv;
			pendingRecv.wsaOverlapped = wsa_overlapped;
			pendingRecv.buffers.assign(wsa_buffers, wsa_buffers + wsa_buffer_count);
			if (address_from) {
				pendingRecv.addressFrom = address_from;
				pendingRecv.addressFromSize = address_from_size;
			}
			titleSocket->recvPendingOverlapped.push_back(pendingRecv);

			// Deliberately do NOT stamp wsa_overlapped->Internal = STATUS_PENDING here. An
			// overlapped recv completes LAZILY - the data is only moved into the title's
			// buffer when it reaps via XWSAGetOverlappedResult - and nothing flips Internal
			// back when a datagram merely arrives, so a title driving its read loop off
			// HasOverlappedIoCompleted() (`Internal != STATUS_PENDING`) would poll "still
			// pending" forever and never reap. Instead RESET the overlapped to the state a
			// freshly memset one has: the reap still reports the true state
			// (WSA_IO_INCOMPLETE when no data is queued yet). The explicit clear also heals
			// a struct reused after XWSACancelOverlappedIO, whose STATUS_CANCELLED stamp
			// would otherwise go stale across the re-arm.
			wsa_overlapped->Internal = 0;
			wsa_overlapped->InternalHigh = 0;

			if (titleSocket->recvPacketQueue.size()) {
				// Data already queued: complete immediately. Signal the socket handle
				// for titles that wait on it (hEvent=NULL overlapped I/O) and the OLDEST
				// pending overlapped (completions happen in FIFO order).
				SetEvent((HANDLE)titleSocket->handle);
				WSAOVERLAPPED* headOverlapped = titleSocket->recvPendingOverlapped.front().wsaOverlapped;
				if (headOverlapped->hEvent && headOverlapped->hEvent != INVALID_HANDLE_VALUE) {
					WSASetEvent(headOverlapped->hEvent);
				}
			}
			else if (titleSocket->recvPendingOverlapped.size() == 1) {
				// First registration armed with nothing ready yet: the socket handle must
				// be non-signaled so a wait on it blocks until data arrives.
				ResetEvent((HANDLE)titleSocket->handle);
			}

			LeaveCriticalSection(&xlive_critsec_sockets);

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) wsa_overlapped->hEvent (0x%zx)."
				, __func__
				, title_socket_handle
				, wsa_overlapped->hEvent
			);

			WSASetLastError(WSA_IO_PENDING);
			return SOCKET_ERROR;
		}
	}

	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #23
int32_t WINAPI XWSASend(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* sent_size
	, uint32_t flags
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
)
{
	TRACE_FX();
	int32_t result = XWSASendTo(title_socket_handle, wsa_buffers, wsa_buffer_count, sent_size, flags, 0, 0, wsa_overlapped, wsa_completion_routine);
	return result;
}

// #25
int32_t WINAPI XWSASendTo(
	SOCKET title_socket_handle
	// each buffer is essentially its own message afaik. so wrap each one.
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* sent_size
	, uint32_t flags
	, const sockaddr* address_to
	, int32_t address_to_size
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) flags (0x%08x) must be 0."
			, __func__
			, flags
			, title_socket_handle
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (!wsa_buffers) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_buffers is NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (!wsa_buffer_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_buffer_count is 0."
			, __func__
			, title_socket_handle
			, wsa_buffer_count
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	// Zero-length WSABUF entries (buf may be NULL) are legal in real Winsock: AFD only
	// probes/locks buffers with len > 0, so a {NULL, 0} entry contributes nothing and is
	// accepted. Titles pass fixed-size WSABUF arrays with unused {NULL, 0} tail slots, and a
	// single zero-length buffer is the legitimate way to send a zero-byte datagram. Only a
	// NULL buf that would actually be read (len > 0) is a real WSAEFAULT.
	for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
		if (!wsa_buffers[iWsaBuffer].buf && wsa_buffers[iWsaBuffer].len) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) wsa_buffers[%u].buf is NULL with len (%u)."
				, __func__
				, title_socket_handle
				, iWsaBuffer
				, wsa_buffers[iWsaBuffer].len
			);
			WSASetLastError(WSAEFAULT);
			return SOCKET_ERROR;
		}
	}
	if (wsa_completion_routine) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_completion_routine must be NULL."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (address_to && address_to_size < sizeof(sockaddr_in)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx): address_to_size (0x%08x) < sizeof(sockaddr_in) (0x%zx)."
			, __func__
			, title_socket_handle
			, address_to_size
			, (size_t)sizeof(sockaddr_in)
		);
		WSASetLastError(WSAEFAULT);
		return SOCKET_ERROR;
	}
	if (address_to && address_to->sa_family != AF_INET) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) bind on unsupported socket address family 0x%04hx."
			, __func__
			, title_socket_handle
			, address_to->sa_family
		);
		WSASetLastError(WSAEAFNOSUPPORT);
		return SOCKET_ERROR;
	}
	if (wsa_overlapped && wsa_overlapped->hEvent == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_handle (0x%zx) wsa_overlapped->hEvent is INVALID."
			, __func__
			, title_socket_handle
		);
		WSASetLastError(ERROR_INVALID_HANDLE);
		return SOCKET_ERROR;
	}
	
	if (sent_size) {
		*sent_size = 0;
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
		
		if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "%s title_socket_handle (0x%zx) socket has been shutdown for sending."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAESHUTDOWN);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->sendInCritSec) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) an existing Send operation is in progress."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINPROGRESS);
			return SOCKET_ERROR;
		}
		
		if (titleSocket->sendWsaOverlapped) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) an existing Send overlapped operation is in progress."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEINPROGRESS);
			return SOCKET_ERROR;
		}
		
		uint32_t instanceId = 0;
		uint16_t instanceTitlePort = 0;
		
		if (titleSocket->protocol == IPPROTO_TCP) {
			if (address_to) {
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
					, "(RESEARCH) %s TCP title_socket_handle (0x%zx) address_to is set."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAEINVAL);
				return SOCKET_ERROR;
			}
			
			if (!titleSocket->tcpIsConnected) {
				
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) TCP is not connected."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAENOTCONN);
				return SOCKET_ERROR;
			}
			
			instanceId = titleSocket->tcpRemoteInstanceId;
			instanceTitlePort = titleSocket->tcpRemoteTitlePort;
		}
		else {
			if (!address_to) {
				// send()/WSASend() on a connected-UDP socket targets the default peer.
				if (titleSocket->udpIsConnected) {
					instanceId = titleSocket->udpRemoteInstanceId;
					instanceTitlePort = titleSocket->udpRemoteTitlePort;
				}
				else {

					LeaveCriticalSection(&xlive_critsec_sockets);

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) address_to is NULL."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAEDESTADDRREQ);
					return SOCKET_ERROR;
				}
			}
			else {
				const uint32_t ipv4NBO = ((sockaddr_in*)address_to)->sin_addr.s_addr;
				instanceId = ntohl(ipv4NBO);
				instanceTitlePort = GetSockAddrPort((const SOCKADDR_STORAGE*)address_to);
			}
		}
		
		if (instanceId == INADDR_ANY) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) instanceId is 0."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEADDRNOTAVAIL);
			return SOCKET_ERROR;
		}
		
		if (instanceId == INADDR_BROADCAST && !titleSocket->udpIsBroadcastEnabled) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) has not been enabled for broadcasting."
				, __func__
				, title_socket_handle
			);
			WSASetLastError(WSAEACCES);
			return SOCKET_ERROR;
		}
		
		SocketImplicitBind_(titleSocket);
		
		if (instanceTitlePort == 0) {
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s title_socket_handle (0x%zx) instanceId (0x%08x) instanceTitlePort is 0."
				, __func__
				, title_socket_handle
				, instanceId
			);
			WSASetLastError(WSAEADDRNOTAVAIL);
			return SOCKET_ERROR;
		}
		
		if (address_to) {
			const size_t packetSizeWrappingSpace = 0x200;
			for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
				if (wsa_buffers[iWsaBuffer].len > xlive_network_sndbuf - packetSizeWrappingSpace) {
					
					LeaveCriticalSection(&xlive_critsec_sockets);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) packet size is too big: wsa_buffers[%u].len (0x%08x) > xlive_network_sndbuf (0x%08x) - (packetSizeWrappingSpace) (0x%zx)."
						, __func__
						, title_socket_handle
						, iWsaBuffer
						, wsa_buffers[iWsaBuffer].len
						, xlive_network_sndbuf
						, packetSizeWrappingSpace
					);
					
					WSASetLastError(WSAEMSGSIZE);
					return SOCKET_ERROR;
				}
			}
		}
		
		titleSocket->sendWsaOverlapped = 0;
		titleSocket->sendWsaOverlappedCancelled = false;
		titleSocket->sendPacketCount = 0;
		titleSocket->sendTransferredSize = 0;
		titleSocket->sendTransferTruncated = false;
		titleSocket->sendCompleted = false;
		ResetEvent(titleSocket->sendNotify);
		
		if (instanceId == INADDR_LOOPBACK || instanceId == xlln_global_instance_id) {
			SOCKET titleSocketHandleDestination = INVALID_SOCKET;

			for (auto itrTitleSocketDestination : xlive_title_sockets) {
				XLIVE_TITLE_SOCKET* titleSocketDestination = itrTitleSocketDestination.second;
				if (titleSocket->protocol == IPPROTO_TCP && !address_to && !titleSocketDestination->tcpIsConnected) {
					continue;
				}
				// Keep scanning past a matching-port socket that is shut down for receiving:
				// with shared ports another local socket may still be able to take the data.
				if (titleSocketDestination->portActual == instanceTitlePort) {
					if (!(titleSocketDestination->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_RECV)) {
						titleSocketHandleDestination = titleSocketDestination->handle;
						break;
					}
				}
			}

			if (titleSocketHandleDestination == INVALID_SOCKET && titleSocket->type != SOCK_DGRAM) {

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s title_socket_handle (0x%zx) attempted send to unused local instanceTitlePort (%hu)."
					, __func__
					, title_socket_handle
					, instanceTitlePort
				);
				WSASetLastError(WSAEHOSTUNREACH);
				return SOCKET_ERROR;
			}

			size_t totalPayloadSize = 0;
			for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
				totalPayloadSize += wsa_buffers[iWsaBuffer].len;
			}

			if (titleSocketHandleDestination == INVALID_SOCKET) {
				// A datagram to a local port nobody is listening on: real UDP sendto still
				// SUCCEEDS (fire-and-forget) and the datagram is simply dropped.
				titleSocket->sendTransferredSize += totalPayloadSize;

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) dropping datagram to unused local instanceTitlePort (%hu)."
					, __func__
					, title_socket_handle
					, instanceTitlePort
				);
			}
			else if (titleSocket->type == SOCK_DGRAM) {
				// Loopback datagram: gather ALL the WSABUFs into ONE message, exactly like
				// the remote path. Splitting per-WSABUF (or at SO_SNDBUF) would turn one send
				// into several phantom datagrams for the receiver.
				XTS_RECV_PACKET* recvPacket = new XTS_RECV_PACKET(totalPayloadSize);
				{
					size_t iData = 0;
					for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
						if (!wsa_buffers[iWsaBuffer].len) {
							continue;
						}
						memcpy(&recvPacket->data[iData], wsa_buffers[iWsaBuffer].buf, wsa_buffers[iWsaBuffer].len);
						iData += wsa_buffers[iWsaBuffer].len;
					}
				}
				recvPacket->dataFilledSize = totalPayloadSize;
				recvPacket->remoteInstanceId = xlln_global_instance_id;
				recvPacket->remoteInstanceTitlePort = titleSocket->portActual;

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s title_socket_handle (0x%zx) submitting packet(s) (1) to local title_socket_handle (0x%zx)."
					, __func__
					, title_socket_handle
					, titleSocketHandleDestination
				);

				bool resultSubmit = SubmitDataToTitleSocketHandle_(titleSocketHandleDestination, recvPacket);
				if (!resultSubmit) {
					// The local receiver refused (e.g. shut down mid-call): like the wire
					// path, the datagram is dropped and the send still succeeds.
					delete recvPacket;
				}
				recvPacket = 0;

				titleSocket->sendTransferredSize += totalPayloadSize;
			}
			else {
				// Loopback stream: chunking at SO_SNDBUF is legal for a byte stream.
				for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
					if (!wsa_buffers[iWsaBuffer].len) {
						continue;
					}

					size_t packetCount = wsa_buffers[iWsaBuffer].len / titleSocket->socketOptions[SO_SNDBUF];
					if (wsa_buffers[iWsaBuffer].len > packetCount * titleSocket->socketOptions[SO_SNDBUF]) {
						packetCount++;
					}

					XTS_RECV_PACKET** recvPackets = new XTS_RECV_PACKET*[packetCount];
					size_t iData = 0;

					for (size_t iPacket = 0; iPacket < packetCount; iPacket++) {
						recvPackets[iPacket] = new XTS_RECV_PACKET(iPacket == packetCount - 1 ? wsa_buffers[iWsaBuffer].len - iData : titleSocket->socketOptions[SO_SNDBUF]);

						memcpy(recvPackets[iPacket]->data, &(wsa_buffers[iWsaBuffer].buf[iData]), recvPackets[iPacket]->dataSize);
						recvPackets[iPacket]->dataFilledSize = recvPackets[iPacket]->dataSize;

						recvPackets[iPacket]->remoteInstanceId = xlln_global_instance_id;
						recvPackets[iPacket]->remoteInstanceTitlePort = titleSocket->portActual;

						iData += recvPackets[iPacket]->dataSize;
					}

					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s title_socket_handle (0x%zx) submitting packet(s) (%zu) to local title_socket_handle (0x%zx)."
						, __func__
						, title_socket_handle
						, packetCount
						, titleSocketHandleDestination
					);

					for (size_t iPacket = 0; iPacket < packetCount; iPacket++) {

						// TODO If one of many fails then that violates the TCP protocol requirements, also might apply to iWsaBuffer too?
						bool resultSubmit = SubmitDataToTitleSocketHandle_(titleSocketHandleDestination, recvPackets[iPacket]);
						if (!resultSubmit) {
							delete recvPackets[iPacket];
						}
						recvPackets[iPacket] = 0;
					}

					delete[] recvPackets;
					recvPackets = 0;

					titleSocket->sendTransferredSize += iData;
				}
			}

			titleSocket->sendCompleted = true;

			if (sent_size) {
				*sent_size = titleSocket->sendTransferredSize;
			}

			if (!wsa_overlapped) {

				if (titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT) {
					SetEvent(titleSocket->wsaEventSelectWrite);
				}

				LeaveCriticalSection(&xlive_critsec_sockets);

				WSASetLastError(ERROR_SUCCESS);
				return ERROR_SUCCESS;
			}
			else {
				// The data was handed to the local receiver synchronously, so the overlapped
				// send completes inline exactly like the remote datagram path: it is NOT
				// parked in sendWsaOverlapped (that would wrongly refuse a second concurrent
				// overlapped send with WSAEINPROGRESS) and the overlapped is made
				// self-describing per the OVERLAPPED contract.
				wsa_overlapped->Internal = 0;
				wsa_overlapped->InternalHigh = titleSocket->sendTransferredSize;

				SetEvent(titleSocket->sendNotify);
				if (wsa_overlapped->hEvent && wsa_overlapped->hEvent != INVALID_HANDLE_VALUE) {
					WSASetEvent(wsa_overlapped->hEvent);
				}

				titleSocket->selectNotifyWritePending = true;
				SetEvent(titleSocket->selectNotifyWrite);

				if (titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT) {
					SetEvent(titleSocket->wsaEventSelectWrite);
				}

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s title_socket_handle (0x%zx) overlapped loopback send completed synchronously (wsa_overlapped->hEvent (0x%zx))."
					, __func__
					, title_socket_handle
					, wsa_overlapped->hEvent
				);

				WSASetLastError(ERROR_SUCCESS);
				return ERROR_SUCCESS;
			}
		}
		
		// Route by SOCKET TYPE, not by the presence of a destination address: a send() on a
		// connected datagram socket also arrives here with address_to NULL, and it must go
		// down the datagram path (its peer was already resolved above), not be wrapped into
		// the sequenced-stream protocol.
		if (!address_to && titleSocket->type != SOCK_DGRAM) {
			if (!wsa_overlapped && !titleSocket->isBlocking) {
				uint16_t oldestSequenceId = 0;
				if (
					titleSocket->tcpSendQueue.size() >= xlln_tcp_send_queue_max
					|| (
						titleSocket->tcpSendQueue.size()
						&& ((oldestSequenceId = (*titleSocket->tcpSendQueue.begin())->sequenceId), true)
						&& (
							((titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max) > titleSocket->tcpSendSequenceId)
							? (
								oldestSequenceId <= (titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max)
								&& oldestSequenceId >= titleSocket->tcpSendSequenceId
							)
							: (
								oldestSequenceId <= (titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max)
								|| oldestSequenceId >= titleSocket->tcpSendSequenceId
							)
						)
					)
				) {
					LeaveCriticalSection(&xlive_critsec_sockets);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s title_socket_handle (0x%zx) WSAEWOULDBLOCK."
						, __func__
						, title_socket_handle
					);
					
					WSASetLastError(WSAEWOULDBLOCK);
					return SOCKET_ERROR;
				}
			}
			
			for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
				
				// 576 (minimum maximum reassembly buffer size) minus maximum 60-byte IP header and 8-byte UDP header.
				const size_t packetSizeMax = 576 - 68 - sizeof(XllnNetworkPacket::TYPE) - sizeof(XllnNetworkPacket::TCP_SEQUENCED_DATA);
				
				size_t packetCount = wsa_buffers[iWsaBuffer].len / packetSizeMax;
				if (wsa_buffers[iWsaBuffer].len > packetCount * packetSizeMax) {
					packetCount++;
				}
				
				size_t iDataBuffer = 0;
				
				for (size_t iPacket = 0; iPacket < packetCount; iPacket++) {
					
					XTS_TCP_SEND_SEQUENCE_INFO* sendSequenceInfo = new XTS_TCP_SEND_SEQUENCE_INFO;
					sendSequenceInfo->sequenceId = titleSocket->tcpSendSequenceId++;
					{
						XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
						sendPacket->destinationInstanceId = instanceId;
						{
							const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
							const size_t packetSizeTypeTcpSequencedData = sizeof(XllnNetworkPacket::TCP_SEQUENCED_DATA);
							const size_t packetSizeTitlePayload = (iPacket == packetCount - 1 ? wsa_buffers[iWsaBuffer].len - iDataBuffer : packetSizeMax);
							const size_t packetSizeTotal = packetSizeType + packetSizeTypeTcpSequencedData + packetSizeTitlePayload;
							
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "%s title_socket_handle (0x%zx) packetSizeTitlePayload (0x%zx)."
								, __func__
								, title_socket_handle
								, packetSizeTitlePayload
							);
							
							uint8_t* sendData = new uint8_t[packetSizeTotal];
							{
								size_t iData = 0;
								
								XllnNetworkPacket::TYPE &packetType = *(XllnNetworkPacket::TYPE*)&sendData[iData];
								iData += packetSizeType;
								packetType = XllnNetworkPacket::TYPE::XLLN_NPT_TCP_SEQUENCED_DATA;
								
								XllnNetworkPacket::TCP_SEQUENCED_DATA &packetTcpSequencedData = *(XllnNetworkPacket::TCP_SEQUENCED_DATA*)&sendData[iData];
								iData += packetSizeTypeTcpSequencedData;
								packetTcpSequencedData.titlePacket.instanceId = xlln_global_instance_id;
								packetTcpSequencedData.titlePacket.titlePortSource = titleSocket->portActual;
								packetTcpSequencedData.titlePacket.titlePortDestination = instanceTitlePort;
								packetTcpSequencedData.dataSize = (uint32_t)packetSizeTitlePayload;
								packetTcpSequencedData.sequenceId = sendSequenceInfo->sequenceId;
								
								memcpy(&sendData[iData], &(wsa_buffers[iWsaBuffer].buf[iDataBuffer]), packetSizeTitlePayload);
								iDataBuffer += packetSizeTitlePayload;
							}
							
							sendPacket->data = sendData;
							sendPacket->dataSize = packetSizeTotal;
						}
						
						sendSequenceInfo->sendPacket = sendPacket;
					}
					titleSocket->tcpSendQueue.push_back(sendSequenceInfo);
				}
				
				titleSocket->sendTransferredSize += wsa_buffers[iWsaBuffer].len;
			}
			
			SetEvent(xlln_tcp_sockets_update);
			
			{
				uint16_t oldestSequenceId = 0;
				if (!(
					titleSocket->tcpSendQueue.size() >= xlln_tcp_send_queue_max
					|| (
						titleSocket->tcpSendQueue.size()
						&& ((oldestSequenceId = (*titleSocket->tcpSendQueue.begin())->sequenceId), true)
						&& (
							((titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max) > titleSocket->tcpSendSequenceId)
							? (
								oldestSequenceId <= (titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max)
								&& oldestSequenceId >= titleSocket->tcpSendSequenceId
							)
							: (
								oldestSequenceId <= (titleSocket->tcpSendSequenceId - xlln_tcp_send_queue_max)
								|| oldestSequenceId >= titleSocket->tcpSendSequenceId
							)
						)
					)
				)) {
					titleSocket->sendCompleted = true;
					
					SetEvent(titleSocket->sendNotify);
					if (titleSocket->sendWsaOverlapped && titleSocket->sendWsaOverlapped->hEvent && titleSocket->sendWsaOverlapped->hEvent != INVALID_HANDLE_VALUE) {
						WSASetEvent(titleSocket->sendWsaOverlapped->hEvent);
					}
					
					titleSocket->selectNotifyWritePending = true;
					SetEvent(titleSocket->selectNotifyWrite);
				}
			}
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) TCP queued packet for sending with sequenceId (%hu)."
				, __func__
				, title_socket_handle
				, titleSocket->tcpSendSequenceId - 1
			);
		}
		else {
			bool sendPacketQueued = false;
			size_t totalPayloadSize = 0;
			for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
				totalPayloadSize += wsa_buffers[iWsaBuffer].len;
			}
			{
				XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
				// A datagram overlapped send completes synchronously below, so it must not be registered
				// for the network thread's completion reap: that reap re-fires this socket's writable
				// notifications (sendNotify is auto-reset) after the send has already reported done. Only
				// blocking sends and overlapped stream sends are genuinely deferred and need reaping.
				if (titleSocket->isBlocking || (wsa_overlapped && titleSocket->type != SOCK_DGRAM)) {
					sendPacket->sourceTitleSocketHandle = title_socket_handle;
					titleSocket->sendPacketCount++;
				}
				sendPacket->destinationInstanceId = instanceId;
				{
					const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
					const size_t packetSizeTypeTitle = sizeof(XllnNetworkPacket::TITLE_PACKET);
					const size_t packetSizeTotal = packetSizeType + packetSizeTypeTitle + totalPayloadSize;
					
					uint8_t* sendData = new uint8_t[packetSizeTotal];
					{
						size_t iData = 0;
						
						XllnNetworkPacket::TYPE &packetType = *(XllnNetworkPacket::TYPE*)&sendData[iData];
						iData += packetSizeType;
						packetType = XllnNetworkPacket::TYPE::XLLN_NPT_TITLE_PACKET;
						
						XllnNetworkPacket::TITLE_PACKET &packetTitle = *(XllnNetworkPacket::TITLE_PACKET*)&sendData[iData];
						iData += packetSizeTypeTitle;
						packetTitle.instanceId = xlln_global_instance_id;
						packetTitle.titlePortSource = titleSocket->portActual;
						packetTitle.titlePortDestination = instanceTitlePort;
						
						for (uint32_t iWsaBuffer = 0; iWsaBuffer < wsa_buffer_count; iWsaBuffer++) {
							memcpy(&sendData[iData], wsa_buffers[iWsaBuffer].buf, wsa_buffers[iWsaBuffer].len);
							iData += wsa_buffers[iWsaBuffer].len;
						}
					}
					
					sendPacket->data = sendData;
					sendPacket->dataSize = packetSizeTotal;
				}
				
				if (SendPacketToRemoteInstance(sendPacket)) {
					sendPacketQueued = true;
					titleSocket->sendTransferredSize += totalPayloadSize;
				}
				else {
					if (sendPacket->sourceTitleSocketHandle != INVALID_SOCKET) {
						titleSocket->sendPacketCount--;
					}
					delete sendPacket;
					sendPacket = 0;
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s title_socket_handle (0x%zx) SendPacketToRemoteInstance failed."
						, __func__
						, title_socket_handle
					);
				}
				
				sendPacket = 0;
			}
			
			if (!sendPacketQueued) {
				LeaveCriticalSection(&xlive_critsec_sockets);

				WSASetLastError(WSAENETDOWN);
				return SOCKET_ERROR;
			}
		}
		
		if (!wsa_overlapped) {
			
			if (titleSocket->isBlocking) {
				EnterCriticalSection(&titleSocket->sendCritSec);
				titleSocket->sendInCritSec = true;
				LeaveCriticalSection(&xlive_critsec_sockets);
				
				DWORD resultWait = WaitForSingleObject(titleSocket->sendNotify, INFINITE);
				if (resultWait != WAIT_OBJECT_0) {
					__debugbreak();
				}
				
				if (titleSocket->sendWsaOverlappedCancelled) {
					
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) wsa_overlapped (0x%zx) has been cancelled."
						, __func__
						, title_socket_handle
						, wsa_overlapped
					);
					WSASetLastError(WSAECANCELLED);
					return SOCKET_ERROR;
				}
				
				if (titleSocket->hasShutdown & XTS_SHUTDOWN_STATE::XTS_SS_NO_SEND) {
					
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s title_socket_handle (0x%zx) socket has been shutdown for receiving."
						, __func__
						, title_socket_handle
					);
					WSASetLastError(WSAESHUTDOWN);
					return SOCKET_ERROR;
				}
				
				// Never wait on the global lock while still holding sendCritSec - closesocket
				// acquires them in the opposite order (global, then a sendCritSec barrier) and
				// the two threads would deadlock. If the global lock is contended, release
				// sendCritSec first and re-resolve the socket (it may be mid-teardown).
				if (TryEnterCriticalSection(&xlive_critsec_sockets)) {
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);
				}
				else {
					titleSocket->sendInCritSec = false;
					LeaveCriticalSection(&titleSocket->sendCritSec);

					EnterCriticalSection(&xlive_critsec_sockets);

					auto itrTitleSocketAgain = xlive_title_sockets.find(title_socket_handle);
					if (itrTitleSocketAgain == xlive_title_sockets.end()) {

						LeaveCriticalSection(&xlive_critsec_sockets);

						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
							, "%s title_socket_handle (0x%zx) was closed while waiting."
							, __func__
							, title_socket_handle
						);
						WSASetLastError(WSAENOTSOCK);
						return SOCKET_ERROR;
					}
					titleSocket = itrTitleSocketAgain->second;
				}
			}
			else {
				// FIXME in this scenario we might want to return WSAEWOULDBLOCK to prevent the Title from filling up our network faster than we can send.
			}
			
			if (sent_size) {
				*sent_size = titleSocket->sendTransferredSize;
			}
			
			if (titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT) {
				SetEvent(titleSocket->wsaEventSelectWrite);
			}
			
			bool sendTransferTruncated = titleSocket->sendTransferTruncated;
			
			LeaveCriticalSection(&xlive_critsec_sockets);
			
			if (sendTransferTruncated) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
					, "%s title_socket_handle (0x%zx) data transfer truncated."
					, __func__
					, title_socket_handle
				);
				WSASetLastError(WSAEMSGSIZE);
				return SOCKET_ERROR;
			}
		}
		else {
			if (titleSocket->type == SOCK_DGRAM) {
				if (sent_size) {
					*sent_size = titleSocket->sendTransferredSize;
				}

				// This overlapped completed inline, so it is NOT parked in sendWsaOverlapped and a
				// later XWSAGetOverlappedResult(wsa_overlapped) would not find it. Make the overlapped
				// self-describing per the Winsock OVERLAPPED contract
				wsa_overlapped->Internal = 0;
				wsa_overlapped->InternalHigh = titleSocket->sendTransferredSize;

				if (titleSocket->wsaEventSelectWrite != WSA_INVALID_EVENT) {
					SetEvent(titleSocket->wsaEventSelectWrite);
				}
				if (wsa_overlapped->hEvent && wsa_overlapped->hEvent != INVALID_HANDLE_VALUE) {
					WSASetEvent(wsa_overlapped->hEvent);
				}

				LeaveCriticalSection(&xlive_critsec_sockets);

				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s title_socket_handle (0x%zx) overlapped UDP send completed synchronously (wsa_overlapped->hEvent (0x%zx))."
					, __func__
					, title_socket_handle
					, wsa_overlapped->hEvent
				);

				WSASetLastError(ERROR_SUCCESS);
				return ERROR_SUCCESS;
			}

			titleSocket->sendWsaOverlapped = wsa_overlapped;

			LeaveCriticalSection(&xlive_critsec_sockets);

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) wsa_overlapped->hEvent (0x%zx)."
				, __func__
				, title_socket_handle
				, wsa_overlapped->hEvent
			);

			WSASetLastError(WSA_IO_PENDING);
			return SOCKET_ERROR;
		}
	}

	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #28
void WINAPI XWSASetLastError(int32_t error_code)
{
	TRACE_FX();
	XLLN_DEBUG_LOG_ECODE(error_code, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s"
		, __func__
	);
	WSASetLastError(error_code);
}

// #29
WSAEVENT WINAPI XWSACreateEvent()
{
	TRACE_FX();
	WSAEVENT wsa_event = WSACreateEvent();
	// Essentially equivalent to:
	//WSAEVENT wsa_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!wsa_event) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s failed."
			, __func__
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s wsa_event (0x%zx)."
			, __func__
			, wsa_event
		);
	}
	return wsa_event;
}

// #30
BOOL WINAPI XWSACloseEvent(WSAEVENT wsa_event)
{
	TRACE_FX();
	if (!wsa_event || wsa_event == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_event is INVALID."
			, __func__
		);
		WSASetLastError(WSA_INVALID_HANDLE);
		return FALSE;
	}
	BOOL result = WSACloseEvent(wsa_event);
	if (!result) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s failed."
			, __func__
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s wsa_event (0x%zx)."
			, __func__
			, wsa_event
		);
	}
	return result;
}

// #31
BOOL WINAPI XWSASetEvent(WSAEVENT wsa_event)
{
	TRACE_FX();
	if (!wsa_event || wsa_event == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_event is INVALID."
			, __func__
		);
		WSASetLastError(WSA_INVALID_HANDLE);
		return FALSE;
	}
	BOOL result = WSASetEvent(wsa_event);
	if (!result) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s failed."
			, __func__
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s wsa_event (0x%zx)."
			, __func__
			, wsa_event
		);
	}
	return result;
}

// #32
BOOL WINAPI XWSAResetEvent(WSAEVENT wsa_event)
{
	TRACE_FX();
	if (!wsa_event || wsa_event == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_event is INVALID."
			, __func__
		);
		WSASetLastError(WSA_INVALID_HANDLE);
		return FALSE;
	}
	BOOL result = WSAResetEvent(wsa_event);
	if (!result) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s failed."
			, __func__
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s wsa_event (0x%zx)."
			, __func__
			, wsa_event
		);
	}
	return result;
}

// #33
uint32_t WINAPI XWSAWaitForMultipleEvents(uint32_t event_count, const WSAEVENT* wsa_events, BOOL wait_on_all, uint32_t timeout, BOOL alertable)
{
	TRACE_FX();
	if (!event_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s event_count is 0."
			, __func__
		);
		WSASetLastError(WSA_INVALID_PARAMETER);
		return WSA_WAIT_FAILED;
	}
	if (!wsa_events) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_events is NULL."
			, __func__
		);
		WSASetLastError(WSA_INVALID_PARAMETER);
		return WSA_WAIT_FAILED;
	}
	#ifdef XLLN_DEBUG
	{
		char* printTextEvents = FormMallocString("0x%zx", wsa_events[0]);
		for (uint32_t iEvent = 1; iEvent < event_count; iEvent++) {
			char* printTextOld = printTextEvents;
			printTextEvents = FormMallocString("%s, 0x%zx", printTextOld, wsa_events[iEvent]);
			free(printTextOld);
			printTextOld = 0;
		}
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s wsa_events (%s), wait_on_all (%s), timeout (0x%08x), alertable (%s)."
			, __func__
			, printTextEvents
			, wait_on_all ? "TRUE" : "FALSE"
			, timeout
			, alertable ? "TRUE" : "FALSE"
		);
		free(printTextEvents);
		printTextEvents = 0;
	}
	#endif
	for (uint32_t iEvent = 0; iEvent < event_count; iEvent++) {
		if (!wsa_events[iEvent] || wsa_events[iEvent] == INVALID_HANDLE_VALUE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s wsa_events[iEvent (%u)] is INVALID."
				, __func__
				, iEvent
			);
			WSASetLastError(WSA_INVALID_HANDLE);
			return WSA_WAIT_FAILED;
		}
	}
	
	uint32_t result = WSAWaitForMultipleEvents(event_count, wsa_events, wait_on_all, timeout, alertable);
	if (result == WSA_WAIT_FAILED) {
		int32_t errorCode = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s failed."
			, __func__
		);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s result (0x%x)."
			, __func__
			, result
		);
	}
	return result;
}

// #34
int32_t WINAPI XWSAFDIsSet(SOCKET title_socket_handle, fd_set* title_socket_set)
{
	TRACE_FX();
	if (!title_socket_set) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s title_socket_set is NULL."
			, __func__
		);
		WSASetLastError(WSAEFAULT);
		return 0;
	}
	
	bool containsSocket = false;
	
	for (u_int iSocket = 0; iSocket < title_socket_set->fd_count; iSocket++) {
		if (title_socket_handle == title_socket_set->fd_array[iSocket]) {
			containsSocket = true;
			break;
		}
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return containsSocket ? 1 : 0;
}

// #35
int32_t WINAPI XWSAEventSelect(SOCKET title_socket_handle, WSAEVENT wsa_event, uint32_t network_event_flags)
{
	TRACE_FX();
	// WSAEventSelect(s, NULL, 0) is the documented way to CANCEL the association, so a null
	// event is only invalid when registering for events.
	if (wsa_event == INVALID_HANDLE_VALUE || (!wsa_event && network_event_flags)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s wsa_event is null/INVALID_HANDLE_VALUE."
			, __func__
		);
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
	}

	// network_event_flags is a BITMASK - titles register OR-combined interests
	// (FD_READ | FD_CLOSE is the canonical registration), all on the one event.
	const uint32_t supportedEventFlags = FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE;
	const uint32_t unsupportedEventFlags = FD_QOS | FD_GROUP_QOS | FD_ROUTING_INTERFACE_CHANGE | FD_ADDRESS_LIST_CHANGE;
	if (network_event_flags & unsupportedEventFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s network_event_flags (0x%x) contains unsupported flags."
			, __func__
			, network_event_flags
		);
		WSASetLastError(WSAEOPNOTSUPP);
		return SOCKET_ERROR;
	}
	if (network_event_flags & ~supportedEventFlags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s network_event_flags (0x%x) contains unknown flags."
			, __func__
			, network_event_flags
		);
		WSASetLastError(WSAEINVAL);
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

		// Registering for events implicitly makes the socket non-blocking (per WSAEventSelect
		// docs); a plain cancel does not change the blocking mode.
		if (network_event_flags && titleSocket->isBlocking) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
				, "%s title_socket_handle (0x%zx) socket/FIONBIO set to ASYNC implicitly."
				, __func__
				, title_socket_handle
			);

			titleSocket->isBlocking = false;
			titleSocket->socketIoctls[FIONBIO] = 1;
		}
		
		if (titleSocket->wsaEventSelectRead == wsa_event) {
			titleSocket->wsaEventSelectRead = WSA_INVALID_EVENT;
			if (network_event_flags != FD_READ) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_READ."
					, __func__
					, wsa_event
				);
			}
		}
		if (titleSocket->wsaEventSelectWrite == wsa_event) {
			titleSocket->wsaEventSelectWrite = WSA_INVALID_EVENT;
			if (network_event_flags != FD_WRITE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_WRITE."
					, __func__
					, wsa_event
				);
			}
		}
		if (titleSocket->wsaEventSelectOob == wsa_event) {
			titleSocket->wsaEventSelectOob = WSA_INVALID_EVENT;
			if (network_event_flags != FD_OOB) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_OOB."
					, __func__
					, wsa_event
				);
			}
		}
		if (titleSocket->wsaEventSelectAccept == wsa_event) {
			titleSocket->wsaEventSelectAccept = WSA_INVALID_EVENT;
			if (network_event_flags != FD_ACCEPT) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_ACCEPT."
					, __func__
					, wsa_event
				);
			}
		}
		if (titleSocket->wsaEventSelectConnect == wsa_event) {
			titleSocket->wsaEventSelectConnect = WSA_INVALID_EVENT;
			if (network_event_flags != FD_CONNECT) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_CONNECT."
					, __func__
					, wsa_event
				);
			}
		}
		if (titleSocket->wsaEventSelectClose == wsa_event) {
			titleSocket->wsaEventSelectClose = WSA_INVALID_EVENT;
			if (network_event_flags != FD_CLOSE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s wsa_event (0x%zx) removed from FD_CLOSE."
					, __func__
					, wsa_event
				);
			}
		}
		
		if (!network_event_flags) {
			// Cancel: clear the whole event association for this socket.
			titleSocket->wsaEventSelectRead = WSA_INVALID_EVENT;
			titleSocket->wsaEventSelectWrite = WSA_INVALID_EVENT;
			titleSocket->wsaEventSelectOob = WSA_INVALID_EVENT;
			titleSocket->wsaEventSelectAccept = WSA_INVALID_EVENT;
			titleSocket->wsaEventSelectConnect = WSA_INVALID_EVENT;
			titleSocket->wsaEventSelectClose = WSA_INVALID_EVENT;

			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s title_socket_handle (0x%zx) event association cancelled."
				, __func__
				, title_socket_handle
			);
		}

		if (network_event_flags & FD_READ) {
			titleSocket->wsaEventSelectRead = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_READ."
				, __func__
				, titleSocket->wsaEventSelectRead
			);
		}
		if (network_event_flags & FD_WRITE) {
			titleSocket->wsaEventSelectWrite = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_WRITE."
				, __func__
				, titleSocket->wsaEventSelectWrite
			);
		}
		if (network_event_flags & FD_OOB) {
			titleSocket->wsaEventSelectOob = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_OOB."
				, __func__
				, titleSocket->wsaEventSelectOob
			);
		}
		if (network_event_flags & FD_ACCEPT) {
			titleSocket->wsaEventSelectAccept = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_ACCEPT."
				, __func__
				, titleSocket->wsaEventSelectAccept
			);
		}
		if (network_event_flags & FD_CONNECT) {
			titleSocket->wsaEventSelectConnect = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_CONNECT."
				, __func__
				, titleSocket->wsaEventSelectConnect
			);
		}
		if (network_event_flags & FD_CLOSE) {
			titleSocket->wsaEventSelectClose = wsa_event;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s wsa_event (0x%zx) added to FD_CLOSE."
				, __func__
				, titleSocket->wsaEventSelectClose
			);
		}

		LeaveCriticalSection(&xlive_critsec_sockets);
	}
	
	WSASetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}
