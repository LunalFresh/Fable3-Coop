#pragma once
#include "../xlive/xsocket.hpp"
#include <vector>

bool InitXllnNetwork();
bool UninitXllnNetwork();

namespace XllnNetworkPacket {
	const char* const TYPE_NAMES[] {
		"UNKNOWN"
		, "TITLE_PACKET"
		, "TITLE_BROADCAST_PACKET"
		, "PACKET_FORWARDED"
		, "UNKNOWN_USER_ASK"
		, "UNKNOWN_USER_REPLY"
		, "CUSTOM_OTHER"
		, "LIVE_OVER_LAN_ADVERTISE"
		, "LIVE_OVER_LAN_UNADVERTISE"
		, "HUB_REQUEST"
		, "HUB_REPLY"
		, "QOS_REQUEST"
		, "QOS_RESPONSE"
		, "HUB_OUT_OF_BAND"
		, "HUB_RELAY"
		, "DIRECT_IP_REQUEST"
		, "DIRECT_IP_RESPONSE"
		, "TCP_CONNECT_REQUEST"
		, "TCP_CONNECT_RESPONSE"
		, "TCP_SEQUENCED_DATA"
		, "TCP_SEQUENCED_DATA_ACK"
	};
	typedef enum : uint8_t {
		XLLN_NPT_UNKNOWN = 0
		, XLLN_NPT_FIRST
		
		, XLLN_NPT_TITLE_PACKET = XLLN_NPT_FIRST
		, XLLN_NPT_TITLE_BROADCAST_PACKET // Unused from XLLN v1.6.
		, XLLN_NPT_PACKET_FORWARDED
		, XLLN_NPT_UNKNOWN_USER_ASK
		, XLLN_NPT_UNKNOWN_USER_REPLY
		, XLLN_NPT_CUSTOM_OTHER // Unused.
		, XLLN_NPT_LIVE_OVER_LAN_ADVERTISE
		, XLLN_NPT_LIVE_OVER_LAN_UNADVERTISE
		, XLLN_NPT_HUB_REQUEST
		, XLLN_NPT_HUB_REPLY
		, XLLN_NPT_QOS_REQUEST
		, XLLN_NPT_QOS_RESPONSE
		, XLLN_NPT_HUB_OUT_OF_BAND // Unused from XLLN v1.6.
		, XLLN_NPT_HUB_RELAY // Unused from XLLN v1.6.
		, XLLN_NPT_DIRECT_IP_REQUEST
		, XLLN_NPT_DIRECT_IP_RESPONSE
		, XLLN_NPT_TCP_CONNECT_REQUEST
		, XLLN_NPT_TCP_CONNECT_RESPONSE
		, XLLN_NPT_TCP_SEQUENCED_DATA
		, XLLN_NPT_TCP_SEQUENCED_DATA_ACK
		
		, XLLN_NPT_LAST = XLLN_NPT_TCP_SEQUENCED_DATA_ACK
	} TYPE;
	
	const char* GetPacketTypeName(XllnNetworkPacket::TYPE type);
	
#pragma pack(push, 1) // Save then set byte alignment setting.
	
	typedef struct _TITLE_PACKET {
		uint32_t instanceId = 0;
		uint16_t titlePortSource = 0; // Host Byte Order.
		uint16_t titlePortDestination = 0; // Host Byte Order.
	} TITLE_PACKET;
	
	typedef struct _PACKET_FORWARDED {
		SOCKADDR_STORAGE originSockAddr;
		// The data following this is the forwarded packet data.
	} PACKET_FORWARDED;
	
	typedef struct _UNKNOWN_USER_ASK {
		uint32_t instanceId = 0; // Instance ID of the asker.
		uint32_t instanceIdUnknown = 0; // Instance ID the asker has no network address for.
	} UNKNOWN_USER_ASK;
	
	typedef struct _UNKNOWN_USER_REPLY {
		uint32_t instanceId = 0; // Instance ID of the replier.
	} UNKNOWN_USER_REPLY;
	
	typedef struct _HUB_REQUEST_PACKET {
		uint32_t xllnVersion = 0; // version of the requester.
		uint32_t instanceId = 0; // Instance ID of the requester.
		uint32_t titleId = 0;
		uint32_t titleVersion = 0;
		uint32_t portBaseHBO = 0;
	} HUB_REQUEST_PACKET;
	
	typedef struct _HUB_REPLY_PACKET {
		uint8_t isHubServer = 0; // boolean.
		uint32_t xllnVersion = 0; // version of the replier.
		uint32_t recommendedInstanceId = 0; // the Instance ID that should be used instead (in case of collisions).
	} HUB_REPLY_PACKET;
	
	typedef struct _LIVE_OVER_LAN_UNADVERTISE {
		uint32_t instanceId = 0;
		uint8_t sessionType = 0;
		XUID xuid = 0;
		// Appended so the fields above keep their offsets and an older instance still reads the packet.
		// Zero means every session the instance hosts, which is what an older instance always sends.
		XNKID xnkid = {};
	} LIVE_OVER_LAN_UNADVERTISE;
	
	typedef struct _QOS_REQUEST {
		uint32_t qosLookupId = 0;
		uint64_t sessionId = 0; // XNKID
		uint32_t probeId = 0;
		uint32_t instanceId = 0; // Instance ID of the requester.
	} QOS_REQUEST;
	
	typedef struct _QOS_RESPONSE {
		uint32_t qosLookupId = 0;
		uint64_t sessionId = 0; // XNKID
		uint32_t probeId = 0;
		uint32_t instanceId = 0; // Instance ID of the responder.
		uint8_t enabled = 0;
		uint16_t sizeData = 0; // the amount of data appended to the end of this packet type.
	} QOS_RESPONSE;
	
	typedef struct _DIRECT_IP_REQUEST {
		uint32_t joinRequestSignature = 0;
		uint8_t passwordSha256[32];
	} DIRECT_IP_REQUEST;
	
	typedef struct _DIRECT_IP_RESPONSE {
		uint32_t joinRequestSignature = 0;
		uint32_t instanceId = 0;
		uint32_t titleId = 0;
	} DIRECT_IP_RESPONSE;
	
	typedef TITLE_PACKET TCP_CONNECT_REQUEST;
	
	const char* const TCP_RESPONSE_CODE_NAMES[] {
		"UNKNOWN"
		, "SUCCESS"
		, "REJECT"
		, "NOT_ACCEPTING_DATA"
		, "KEEP_ALIVE"
	};
	
	typedef enum : uint8_t {
		XLLN_NP_TRC_UNKNOWN = 0
		, XLLN_NP_TRC_FIRST
		
		, XLLN_NP_TRC_SUCCESS = XLLN_NP_TRC_FIRST
		, XLLN_NP_TRC_REJECT
		, XLLN_NP_TRC_NOT_ACCEPTING_DATA
		, XLLN_NP_TRC_KEEP_ALIVE
		
		, XLLN_NP_TRC_LAST = XLLN_NP_TRC_KEEP_ALIVE
	} TCP_RESPONSE_CODE;
	
	const char* GetTcpResponseCodeName(XllnNetworkPacket::TCP_RESPONSE_CODE response_code);
	
	typedef struct _TCP_CONNECT_RESPONSE {
		TITLE_PACKET titlePacket;
		TCP_RESPONSE_CODE responseCode = TCP_RESPONSE_CODE::XLLN_NP_TRC_REJECT;
	} TCP_CONNECT_RESPONSE;
	
	typedef struct _TCP_SEQUENCED_DATA {
		TITLE_PACKET titlePacket;
		uint16_t sequenceId = 0;
		uint32_t dataSize = 0;
	} TCP_SEQUENCED_DATA;
	
	typedef struct _TCP_SEQUENCED_DATA_ACK {
		TITLE_PACKET titlePacket;
		uint16_t recvSequenceAt = 0;
		// Count of pending sequenceIds waiting on previous sequenceId(s).
		uint16_t recvSequenceIdsCount = 0;
	} TCP_SEQUENCED_DATA_ACK;
	
#pragma pack(pop) // Return to original alignment setting.
}

namespace XllnNetworkBroadcastEntity {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"BROADCAST_ADDR",
		"HUB_SERVER",
		"OTHER_CLIENT",
	};
	typedef enum : uint8_t {
		XLLN_NBE_UNKNOWN = 0,
		XLLN_NBE_BROADCAST_ADDR,
		XLLN_NBE_HUB_SERVER,
		XLLN_NBE_OTHER_CLIENT,
	} TYPE;
	
	typedef struct _BROADCAST_ENTITY {
		SOCKADDR_STORAGE sockaddr;
		__time64_t lastComm = 0;
		TYPE entityType = TYPE::XLLN_NBE_UNKNOWN;
		uint32_t xllnVersion = 0;
	} BROADCAST_ENTITY;
}

extern CRITICAL_SECTION xlln_critsec_network_send;
extern std::vector<XLLN_NET_SEND_PACKET_INFO*> xlln_network_send_queue;

extern uint16_t xlln_network_instance_base_port;
extern uint16_t xlln_network_instance_port;
extern HANDLE xlln_network_instance_port_mutex;

extern CRITICAL_SECTION xlln_critsec_network_broadcast_addresses;
extern std::vector<XllnNetworkBroadcastEntity::BROADCAST_ENTITY> xlln_network_broadcast_addresses;

extern CRITICAL_SECTION xlln_critsec_network_net_entity;
extern std::map<uint32_t, SOCKADDR_STORAGE> xlln_net_entity_instance_id_to_remote_address;

extern HANDLE xlln_tcp_sockets_update;
const uint32_t xlln_tcp_packet_timeout = 500;
const uint32_t xlln_tcp_ack_frequency = 40;
const size_t xlln_tcp_send_queue_max = 100;
const uint32_t xlln_tcp_socket_close_timeout = 10*1000;
const uint8_t xlln_tcp_max_connection_attempts = 10;

bool XllnNetEntityGetXnaddrXnkidByInstanceId(const uint32_t instanceId, XNADDR* xnaddr, XNKID* xnkid);
void XllnGenerateSessionXnkid(XNKID* xnkid);
bool SendPacketToRemoteInstance(XLLN_NET_SEND_PACKET_INFO* send_packet);
bool SubmitDataToTitleSocket_(XLIVE_TITLE_SOCKET* title_socket, XTS_RECV_PACKET* recv_packet);
bool SubmitDataToTitleSocketHandle(SOCKET title_socket_handle, XTS_RECV_PACKET* recv_packet);
bool SubmitDataToTitleSocketHandle_(SOCKET title_socket_handle, XTS_RECV_PACKET* recv_packet);
bool XllnNetworkSocketSetSendBufferSize(uint32_t optionValue);
