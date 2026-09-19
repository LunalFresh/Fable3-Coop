#pragma once
#include <stdint.h>
#include <map>
#include <vector>
#include <sys/timeb.h>

bool InitXSocket();
bool UninitXSocket();

extern uint16_t xlive_port_online;
extern uint16_t xlive_port_system_link;
extern bool xlive_netsocket_abort;
extern uint32_t xlive_network_rcvbuf;
extern uint32_t xlive_network_sndbuf;

typedef enum : uint8_t {
	XTS_SS_STILL_ACTIVE = 0,
	XTS_SS_NO_SEND = 0b01,
	XTS_SS_NO_RECV = 0b10,
	XTS_SS_INACTIVE = 0b11,
	XTS_SS_MASK = XTS_SS_INACTIVE,
} XTS_SHUTDOWN_STATE;

typedef struct _XTS_RECV_PACKET {
	uint8_t* data = 0;
	size_t dataConsumedSize = 0;
	size_t dataSize = 0;
	size_t dataFilledSize = 0;
	
	uint32_t remoteInstanceId = 0;
	uint16_t remoteInstanceTitlePort = 0;
	
	_XTS_RECV_PACKET(uint32_t data_buffer_size)
	{
		this->dataSize = data_buffer_size;
		this->data = new uint8_t[data_buffer_size];
	}
	
	~_XTS_RECV_PACKET()
	{
		if (this->data) {
			delete[] this->data;
			this->data = 0;
		}
	}
} XTS_RECV_PACKET;

// One outstanding overlapped recv registration (WSARecv/WSARecvFrom with an OVERLAPPED).
// The WSABUF array is copied at call time (the title may free its array immediately);
// the buffers the entries point at must stay valid until completion, per Winsock.
typedef struct _XTS_PENDING_OVERLAPPED_RECV {
	WSAOVERLAPPED* wsaOverlapped = 0;
	std::vector<WSABUF> buffers;
	sockaddr* addressFrom = 0;
	int32_t* addressFromSize = 0;
} XTS_PENDING_OVERLAPPED_RECV;

// <instanceId, titlePort>.
typedef std::pair<uint32_t, uint16_t> InstanceIdTitlePort;
// Identifies a listening Title socket. TCP and UDP have separate port spaces, so a Title may hold
// a stream and a datagram socket on one port number.
typedef std::pair<bool, uint16_t> TitleTransportPort;

typedef struct _XTS_TCP_CONNECTION {
	uint32_t remoteInstanceId = 0;
	uint16_t remoteInstanceTitlePort = 0;
	__timeb64 lastResponse = {0, 0, 0, 0};
} XTS_TCP_CONNECTION;

typedef struct _XLLN_NET_SEND_PACKET_INFO {
	// The data to send. The data must be wrapped in XllnNetworkPacket::TYPE.
	uint8_t* data = 0;
	size_t dataSize = 0;
	// If set, when data is sent this is used to notifiy the Title.
	SOCKET sourceTitleSocketHandle = INVALID_SOCKET;
	// If this is 0 then use destinationAddress instead.
	uint32_t destinationInstanceId = 0;
	SOCKADDR_STORAGE destinationAddress = {AF_UNSPEC};
	
	~_XLLN_NET_SEND_PACKET_INFO()
	{
		if (data) {
			delete[] data;
			data = 0;
		}
	}
	
	_XLLN_NET_SEND_PACKET_INFO* Clone()
	{
		_XLLN_NET_SEND_PACKET_INFO* sendPacket = new _XLLN_NET_SEND_PACKET_INFO;
		*sendPacket = *this;
		sendPacket->data = new uint8_t[sendPacket->dataSize];
		memcpy(sendPacket->data, this->data, sendPacket->dataSize);
		return sendPacket;
	}
} XLLN_NET_SEND_PACKET_INFO;

typedef struct _XTS_TCP_SEND_SEQUENCE_INFO {
	uint16_t sequenceId = 0;
	XLLN_NET_SEND_PACKET_INFO* sendPacket = 0;
	__timeb64 lastSent = {0, 0, 0, 0};
	
	~_XTS_TCP_SEND_SEQUENCE_INFO()
	{
		if (sendPacket) {
			delete sendPacket;
			sendPacket = 0;
		}
	}
} XTS_TCP_SEND_SEQUENCE_INFO;

typedef struct _XLIVE_TITLE_SOCKET {
	// Fake handle: a real, waitable kernel object whose signaled state means
	// "an overlapped recv on this socket has completed / data is ready to read".
	SOCKET handle = INVALID_SOCKET;
	int32_t type = 0;
	int32_t protocol = 0;
	bool isVdpProtocol = false;
	uint16_t portRequested = 0;
	// Simulated actual/binded port.
	uint16_t portActual = 0;
	bool hasBinded = false;
	XTS_SHUTDOWN_STATE hasShutdown = XTS_SHUTDOWN_STATE::XTS_SS_STILL_ACTIVE;
	bool isBlocking = true;
	// Key: optname. Value: optval.
	// Note: optname keys will either be SO or TCP options depending on if this is a UDP or TCP socket.
	std::map<int32_t, uint32_t> socketOptions;
	// Key: command. Value: cmd value.
	std::map<int32_t, unsigned long> socketIoctls;
	
	//readfds:
	//	If listen has been called and a connection is pending, accept will succeed.
	//	Data is available for reading
	//	Connection has been closed/reset/terminated.
	HANDLE selectNotifyRead = INVALID_HANDLE_VALUE;
	bool selectNotifyReadPending = false;
	//writefds:
	//	If processing a connect call (nonblocking), connection has succeeded.
	//	Data can be sent.
	HANDLE selectNotifyWrite = INVALID_HANDLE_VALUE;
	bool selectNotifyWritePending = false;
	//exceptfds:
	//	If processing a connect call (nonblocking), connection attempt failed.
	HANDLE selectNotifyExcept = INVALID_HANDLE_VALUE;
	bool selectNotifyExceptPending = false;
	
	WSAEVENT wsaEventSelectRead = WSA_INVALID_EVENT;
	WSAEVENT wsaEventSelectWrite = WSA_INVALID_EVENT;
	WSAEVENT wsaEventSelectOob = WSA_INVALID_EVENT;
	WSAEVENT wsaEventSelectAccept = WSA_INVALID_EVENT;
	WSAEVENT wsaEventSelectConnect = WSA_INVALID_EVENT;
	WSAEVENT wsaEventSelectClose = WSA_INVALID_EVENT;
	
	CRITICAL_SECTION recvCritSec;
	std::vector<XTS_RECV_PACKET*> recvPacketQueue;
	bool recvInCritSec = false;
	HANDLE recvNotify = INVALID_HANDLE_VALUE;
	// Outstanding overlapped recv registrations, oldest first. Winsock allows any number of
	// pending overlapped recvs on a socket; they are completed (lazily, at reap time) in the
	// order the datagrams arrive.
	std::vector<XTS_PENDING_OVERLAPPED_RECV> recvPendingOverlapped;
	bool recvWsaOverlappedCancelled = false;
	// SO_RCVTIMEO/SO_SNDTIMEO in milliseconds; 0 means block indefinitely.
	uint32_t recvTimeoutMs = 0;
	uint32_t sendTimeoutMs = 0;
	
	CRITICAL_SECTION sendCritSec;
	bool sendInCritSec = false;
	HANDLE sendNotify = INVALID_HANDLE_VALUE;
	WSAOVERLAPPED* sendWsaOverlapped = 0;
	bool sendWsaOverlappedCancelled = false;
	bool sendTransferTruncated = false;
	uint32_t sendPacketCount = 0;
	uint32_t sendTransferredSize = 0;
	bool sendCompleted = false;
	
	bool udpIsBroadcastEnabled = false;
	// Connected-UDP (a default peer set via connect() on a SOCK_DGRAM socket).
	bool udpIsConnected = false;
	uint32_t udpRemoteInstanceId = 0;
	uint16_t udpRemoteTitlePort = 0;

	bool tcpIsListening = false;
	uint32_t tcpConnectionBacklogLimit = 0;
	std::map<InstanceIdTitlePort, XTS_TCP_CONNECTION> tcpConnectionBacklog;
	bool tcpIsConnected = false;
	CRITICAL_SECTION tcpCritSec;
	bool tcpInCritSec = false;
	bool tcpIsConnecting = false;
	bool tcpIsAccepting = false;
	bool tcpIsClosing = false;
	__timeb64 tcpCloseAfter = {0, 0, 0, 0};
	uint8_t tcpConnectionAttempts = 0;
	__timeb64 tcpConnectionAttemptLast = {0, 0, 0, 0};
	__timeb64 tcpLastResponse = {0, 0, 0, 0};
	uint32_t tcpRemoteInstanceId = 0;
	uint16_t tcpRemoteTitlePort = 0;
	// For blocking calls to XSocketConnect().
	HANDLE tcpConnectNotify = INVALID_HANDLE_VALUE;
	// The next available sequence ID.
	uint16_t tcpSendSequenceId = 0;
	// Packets that have not yet been confirmed as being received.
	std::vector<XTS_TCP_SEND_SEQUENCE_INFO*> tcpSendQueue;
	// The current sequence number we are waiting on.
	uint16_t tcpRecvSequenceId = 0;
	// Store out-of-order packets/sequences until previous arrives then submit to `recvPacketQueue`. Null entries are missing sequences.
	std::vector<XTS_RECV_PACKET*> tcpRecvQueue;
	// If receiving empty with sequence and it matches our recv sequence then unset this.
	__timeb64 tcpRecvLastAck = {0, 0, 0, 0};
	
	_XLIVE_TITLE_SOCKET(int32_t protocol)
	{
		this->handle = (SOCKET)CreateEventA(NULL, TRUE, FALSE, NULL);
		
		this->selectNotifyRead = CreateEventA(NULL, FALSE, FALSE, NULL);
		this->selectNotifyWrite = CreateEventA(NULL, FALSE, FALSE, NULL);
		this->selectNotifyExcept = CreateEventA(NULL, FALSE, FALSE, NULL);
		
		InitializeCriticalSection(&this->recvCritSec);
		this->recvNotify = CreateEventA(NULL, FALSE, FALSE, NULL);
		
		InitializeCriticalSection(&this->sendCritSec);
		this->sendNotify = CreateEventA(NULL, FALSE, FALSE, NULL);
		
		InitializeCriticalSection(&this->tcpCritSec);
		this->tcpConnectNotify = CreateEventA(NULL, FALSE, FALSE, NULL);
		
		// If iMode = 0, blocking is enabled; 
		// If iMode != 0, non-blocking mode is enabled.
		this->socketIoctls[FIONBIO] = (this->isBlocking ? 0 : 1);
		// Data waiting to be read.
		this->socketIoctls[FIONREAD] = 0;
		
		this->socketOptions[SO_RCVBUF] = xlive_network_rcvbuf;
		this->socketOptions[SO_SNDBUF] = xlive_network_sndbuf;
		this->socketOptions[SO_REUSEADDR] = 0;
		this->socketOptions[SO_EXCLUSIVEADDRUSE] = 0;
		this->socketOptions[SO_ERROR] = ERROR_SUCCESS;
		if (protocol == IPPROTO_UDP) {
			this->socketOptions[SO_BROADCAST] = (this->udpIsBroadcastEnabled ? 1 : 0);
		}
	}
	
	~_XLIVE_TITLE_SOCKET()
	{
		this->socketOptions.clear();
		this->socketIoctls.clear();
		
		for (XTS_RECV_PACKET* recvPacket : this->recvPacketQueue) {
			delete recvPacket;
		}
		this->recvPacketQueue.clear();
		
		this->tcpConnectionBacklog.clear();
		
		for (XTS_TCP_SEND_SEQUENCE_INFO* sendSequenceInfo : this->tcpSendQueue) {
			delete sendSequenceInfo;
		}
		this->tcpSendQueue.clear();
		
		for (XTS_RECV_PACKET* recvPacket : this->tcpRecvQueue) {
			if (recvPacket) {
				delete recvPacket;
			}
		}
		this->tcpRecvQueue.clear();
		
		DeleteCriticalSection(&this->recvCritSec);
		CloseHandle(this->recvNotify);
		this->recvNotify = INVALID_HANDLE_VALUE;
		
		DeleteCriticalSection(&this->sendCritSec);
		CloseHandle(this->sendNotify);
		this->sendNotify = INVALID_HANDLE_VALUE;
		
		DeleteCriticalSection(&this->tcpCritSec);
		CloseHandle(this->tcpConnectNotify);
		this->tcpConnectNotify = INVALID_HANDLE_VALUE;
		
		CloseHandle(this->selectNotifyRead);
		this->selectNotifyRead = INVALID_HANDLE_VALUE;
		CloseHandle(this->selectNotifyWrite);
		this->selectNotifyWrite = INVALID_HANDLE_VALUE;
		CloseHandle(this->selectNotifyExcept);
		this->selectNotifyExcept = INVALID_HANDLE_VALUE;
		
		CloseHandle((HANDLE)this->handle);
		this->handle = INVALID_SOCKET;
	}
} XLIVE_TITLE_SOCKET;

extern CRITICAL_SECTION xlive_critsec_sockets;
extern std::map<SOCKET, XLIVE_TITLE_SOCKET*> xlive_title_sockets;
extern std::map<TitleTransportPort, XLIVE_TITLE_SOCKET*> xlive_title_socket_port_actual_to_title_socket;
extern std::map<InstanceIdTitlePort, XLIVE_TITLE_SOCKET*> xlive_title_socket_tcp_connected_sockets;

XLIVE_TITLE_SOCKET* XllnGetTitleSocketByTitlePort_(bool is_stream, uint16_t title_port_actual);
bool SocketImplicitBind_(XLIVE_TITLE_SOCKET* title_socket);
bool ShutdownTitleSocket_(XLIVE_TITLE_SOCKET* titleSocket, XTS_SHUTDOWN_STATE howTypeRequired);
