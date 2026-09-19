#include <winsock2.h>
#include "xdefs.hpp"
#include "xnetqos.hpp"
#include "xsocket.hpp"
#include "xnet.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "live-over-lan.hpp"
#include <vector>
#include <thread>
#include <chrono>

struct QOS_TRANSIT_INFO {
	uint32_t probesSent = 0;
	std::chrono::steady_clock::time_point timeLastCommSent = std::chrono::high_resolution_clock::now();
};

struct QOS_PENDING_LOOKUP {
	uint32_t qosLookupId = 0;
	XNQOS* xnQos = 0;
	QOS_TRANSIT_INFO* qosTransitInfo = 0;
	WSAEVENT hEvent = INVALID_HANDLE_VALUE;
	std::chrono::steady_clock::time_point timeCreated = std::chrono::high_resolution_clock::now();
	uint32_t countProbes = 0;
	uint32_t maxBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	size_t countXn = 0;
	XNADDR* xnAddrs = 0;
	XNKID* xnKids = 0;
	XNKEY* xnKeys = 0;
	size_t countIn = 0;
	IN_ADDR* inAddrs = 0;
	uint32_t* inServiceIds = 0;
};

CRITICAL_SECTION xlive_critsec_qos_listeners;
// Key: sessionId / xnkid.
std::map<uint64_t, QOS_LISTENER_INFO*> xlive_qos_listeners;

CRITICAL_SECTION xlive_critsec_qos_lookups;
// Key: qosLookupId.
static std::map<uint32_t, QOS_PENDING_LOOKUP*> xlive_qos_lookups;

static HANDLE xlive_xnetqos_thread_event = INVALID_HANDLE_VALUE;
static bool xlive_xnetqos_thread_shutdown = false;
static std::thread xlive_xnetqos_thread;

// #69
// Calling XNetUnregisterKey with the session Id / xnkid will release this QoS listener automatically.
// Therefore the session Id needs to be registered via XNetRegisterKey before use here.
int32_t WINAPI XNetQosListen(XNKID* xnkid, uint8_t* data, uint32_t data_size, uint32_t bits_per_second, uint32_t flags)
{
	TRACE_FX();
	if (!flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags is 0.", __func__);
		return WSAEINVAL;
	}
	if (flags & ~(XNET_QOS_LISTEN_ENABLE | XNET_QOS_LISTEN_DISABLE | XNET_QOS_LISTEN_SET_DATA | XNET_QOS_LISTEN_SET_BITSPERSEC | XNET_QOS_LISTEN_RELEASE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Unknown flags (0x%08x).", __func__, flags);
		return WSAEINVAL;
	}
	if ((flags & XNET_QOS_LISTEN_ENABLE) && (flags & XNET_QOS_LISTEN_DISABLE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid flags: cannot enable and disable.", __func__);
		return WSAEINVAL;
	}
	if ((flags & XNET_QOS_LISTEN_RELEASE) && (flags ^ XNET_QOS_LISTEN_RELEASE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid flags: cannot release and change properties.", __func__);
		return WSAEINVAL;
	}
	if ((flags & XNET_QOS_LISTEN_SET_DATA) && data_size > 0 && !data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data is NULL when told to set data of size (%u).", __func__, data_size);
		return WSAEFAULT;
	}
	if (!(flags & XNET_QOS_LISTEN_SET_DATA) && (data_size > 0 || data)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_size and/or data is set when XNET_QOS_LISTEN_SET_DATA flag not set.", __func__);
		return WSAEINVAL;
	}
	if (data_size > 0 && data_size > ((uint32_t)xlive_net_startup_params.cfgQosDataLimitDiv4) * 4) {
		XLLN_DEBUG_LOG(
			XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_size bigger than (cfgQosDataLimitDiv4 * 4) (0x%08x > 0x%08x)."
			, __func__
			, data_size
			, ((uint32_t)xlive_net_startup_params.cfgQosDataLimitDiv4) * 4
		);
		return WSAEMSGSIZE;
	}
	if (bits_per_second == 0) {
		bits_per_second = QOS_LISTEN_BITS_PER_SEC_DEFAULT;
	}
	else if (bits_per_second < 4000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s bits_per_second (%u) < 4000 and > 0. Corrected to 4000."
			, __func__
			, bits_per_second
		);
		bits_per_second = 4000;
	}
	else if (bits_per_second > 100000000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s bits_per_second (%u) > 100000000. Corrected to 100000000."
			, __func__
			, bits_per_second
		);
		bits_per_second = 100000000;
	}
	
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	QOS_LISTENER_INFO* qosListenerInfo = 0;
	
	if (flags & XNET_QOS_LISTEN_RELEASE) {
		if (xlive_qos_listeners.count(*(uint64_t*)xnkid)) {
			qosListenerInfo = xlive_qos_listeners[*(uint64_t*)xnkid];
			xlive_qos_listeners.erase(*(uint64_t*)xnkid);
			
			if (qosListenerInfo->pData) {
				delete[] qosListenerInfo->pData;
				qosListenerInfo->pData = 0;
			}
			delete qosListenerInfo;
			qosListenerInfo = 0;
		}
	}
	else {
		if (xlive_qos_listeners.count(*(uint64_t*)xnkid)) {
			qosListenerInfo = xlive_qos_listeners[*(uint64_t*)xnkid];
		}
		else {
			qosListenerInfo = new QOS_LISTENER_INFO;
			qosListenerInfo->sessionId = *(uint64_t*)xnkid;
			xlive_qos_listeners[*(uint64_t*)xnkid] = qosListenerInfo;
		}
		
		if (flags & XNET_QOS_LISTEN_SET_DATA) {
			qosListenerInfo->dataSize = data_size;
			if (qosListenerInfo->pData) {
				delete[] qosListenerInfo->pData;
				qosListenerInfo->pData = 0;
			}
			if (qosListenerInfo->dataSize) {
				qosListenerInfo->pData = new uint8_t[qosListenerInfo->dataSize];
				memcpy(qosListenerInfo->pData, data, qosListenerInfo->dataSize);
			}
		}
		
		if (flags & XNET_QOS_LISTEN_SET_BITSPERSEC) {
			qosListenerInfo->maxBitsPerSec = bits_per_second;
		}
		
		if (flags & XNET_QOS_LISTEN_ENABLE) {
			qosListenerInfo->active = true;
		}
		if (flags & XNET_QOS_LISTEN_DISABLE) {
			qosListenerInfo->active = false;
		}
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
	
	return S_OK;
}

// #70
uint32_t WINAPI XNetQosLookup(
	size_t lookup_instance_count
	, XNADDR* xnaddrs[]
	, XNKID* xnkids[]
	, XNKEY* xnkeys[]
	, size_t lookup_service_count
	, IN_ADDR in_addrs[]
	, uint32_t service_ids[]
	, uint32_t probe_count
	, uint32_t bits_per_second
	, uint32_t flags
	, WSAEVENT wsa_event
	, XNQOS** xnqos
)
{
	TRACE_FX();
	if (!xnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	*xnqos = 0;
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No flags (0x%08x) are officially supported.", __func__, flags);
		return WSAEINVAL;
	}
	if (probe_count > UINT16_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s probe_count (0x%08x) > UINT16_MAX.", __func__, probe_count);
		return WSAEINVAL;
	}
	if (!lookup_instance_count && !lookup_service_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s lookup_instance_count and lookup_service_count are 0.", __func__);
		return WSAEINVAL;
	}
	if (lookup_instance_count) {
		if (!xnaddrs) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnaddrs is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!xnkids) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnkids is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!xnkeys) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnkeys is NULL.", __func__);
			return WSAEFAULT;
		}
	}
	if (lookup_service_count) {
		if (!in_addrs) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s in_addrs is NULL.", __func__);
			return WSAEFAULT;
		}
		if (!service_ids) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s service_ids is NULL.", __func__);
			return WSAEFAULT;
		}
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME need to see how results are populated.", __func__);
	}
	if (bits_per_second == 0) {
		bits_per_second = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	}
	else if (bits_per_second < 4000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s bits_per_second (%u) < 4000 and > 0. Corrected to 4000."
			, __func__
			, bits_per_second
		);
		bits_per_second = 4000;
	}
	else if (bits_per_second > 100000000) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s bits_per_second (%u) > 100000000. Corrected to 100000000."
			, __func__
			, bits_per_second
		);
		bits_per_second = 100000000;
	}
	
	QOS_PENDING_LOOKUP* qosPendingLookup = new QOS_PENDING_LOOKUP;
	// A probe count of zero is the documented connectivity-only test, not a request for nothing: the
	// host still has to be probed once, or the lookup completes without contact and reports no replies.
	qosPendingLookup->countProbes = probe_count ? probe_count : 1;
	qosPendingLookup->maxBitsPerSec = bits_per_second;
	
	if (wsa_event != INVALID_HANDLE_VALUE && wsa_event) {
		qosPendingLookup->hEvent = wsa_event;
	}
	
	qosPendingLookup->countXn = lookup_instance_count;
	if (qosPendingLookup->countXn) {
		qosPendingLookup->xnAddrs = new XNADDR[qosPendingLookup->countXn];
		qosPendingLookup->xnKids = new XNKID[qosPendingLookup->countXn];
		qosPendingLookup->xnKeys = new XNKEY[qosPendingLookup->countXn];
		// These parameters are arrays of pointers, not contiguous arrays. Copying in bulk from the
		// first element reads past that element for every target after the first.
		for (size_t iXn = 0; iXn < qosPendingLookup->countXn; iXn++) {
			qosPendingLookup->xnAddrs[iXn] = *xnaddrs[iXn];
			qosPendingLookup->xnKids[iXn] = *xnkids[iXn];
			qosPendingLookup->xnKeys[iXn] = *xnkeys[iXn];
		}
	}
	
	qosPendingLookup->countIn = lookup_service_count;
	if (qosPendingLookup->countIn) {
		qosPendingLookup->inAddrs = new IN_ADDR[qosPendingLookup->countIn];
		memcpy(qosPendingLookup->inAddrs, in_addrs, qosPendingLookup->countIn * sizeof(*qosPendingLookup->inAddrs));
		qosPendingLookup->inServiceIds = new uint32_t[qosPendingLookup->countIn];
		memcpy(qosPendingLookup->inServiceIds, service_ids, qosPendingLookup->countIn * sizeof(*qosPendingLookup->inServiceIds));
	}
	
	size_t xnQoSSize = (size_t)&(*xnqos)->axnqosinfo - (size_t)*xnqos;
	xnQoSSize += qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	xnQoSSize += qosPendingLookup->countIn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	qosPendingLookup->xnQos = (XNQOS*)new uint8_t[xnQoSSize];
	memset(qosPendingLookup->xnQos, 0, xnQoSSize);
	
	qosPendingLookup->xnQos->cxnqos = qosPendingLookup->countXn + qosPendingLookup->countIn;
	qosPendingLookup->xnQos->cxnqosPending = qosPendingLookup->xnQos->cxnqos;
	
	qosPendingLookup->qosTransitInfo = new QOS_TRANSIT_INFO[qosPendingLookup->xnQos->cxnqosPending];
	
	*xnqos = qosPendingLookup->xnQos;
	
	{
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		
		// Get a unique ID.
		do {
			qosPendingLookup->qosLookupId = rand();
		}
		while (xlive_qos_lookups.count(qosPendingLookup->qosLookupId));
		
		xlive_qos_lookups[qosPendingLookup->qosLookupId] = qosPendingLookup;
		
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
	}
	
	return S_OK;
}

// #71
int32_t WINAPI XNetQosServiceLookup(uint32_t flags, WSAEVENT wsa_event, XNQOS** xnqos)
{
	TRACE_FX();
	if (flags & ~(XNET_QOS_SERVICE_LOOKUP_RESERVED)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s No flags (0x%08x) are officially supported.", __func__, flags);
		return WSAEINVAL;
	}
	if (!xnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnqos is NULL.", __func__);
		return WSAEINVAL;
	}
	
	
	QOS_PENDING_LOOKUP* qosPendingLookup = new QOS_PENDING_LOOKUP;
	qosPendingLookup->countProbes = 0;
	qosPendingLookup->maxBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
	
	if (wsa_event != INVALID_HANDLE_VALUE && wsa_event) {
		qosPendingLookup->hEvent = wsa_event;
	}
	
	qosPendingLookup->countIn = 1;
	qosPendingLookup->inAddrs = new IN_ADDR[qosPendingLookup->countIn];
	qosPendingLookup->inAddrs[0].s_addr = 0;
	qosPendingLookup->inServiceIds = new uint32_t[qosPendingLookup->countIn];
	qosPendingLookup->inServiceIds[0] = 0;
	
	size_t xnQoSSize = (size_t)&(*xnqos)->axnqosinfo - (size_t)*xnqos;
	xnQoSSize += qosPendingLookup->countXn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	xnQoSSize += qosPendingLookup->countIn * sizeof(*qosPendingLookup->xnQos->axnqosinfo);
	qosPendingLookup->xnQos = (XNQOS*)new uint8_t[xnQoSSize];
	memset(qosPendingLookup->xnQos, 0, xnQoSSize);
	
	qosPendingLookup->xnQos->cxnqos = 0;
	qosPendingLookup->xnQos->cxnqosPending = qosPendingLookup->countXn + qosPendingLookup->countIn;
	
	qosPendingLookup->qosTransitInfo = new QOS_TRANSIT_INFO[qosPendingLookup->xnQos->cxnqosPending];
	
	*xnqos = qosPendingLookup->xnQos;
	
	// Remove this and actually QoS lookup on Hub server(s?).
	
	qosPendingLookup->xnQos->cxnqos = 1;
	qosPendingLookup->xnQos->cxnqosPending = 0;
	
	{
		size_t iXnQoS = 0;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags = XNET_XNQOSINFO_TARGET_CONTACTED | XNET_XNQOSINFO_COMPLETE;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = 1;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv = 1;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].wRttMinInMsecs = 50;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].wRttMedInMsecs = 60;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].dwUpBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].dwDnBitsPerSec = QOS_LOOKUP_BITS_PER_SEC_DEFAULT;
		// This probably does not need setting.
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cbData = 0;
		qosPendingLookup->xnQos->axnqosinfo[iXnQoS].pbData = 0;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		
		// Get a unique ID.
		do {
			qosPendingLookup->qosLookupId = rand();
		}
		while (xlive_qos_lookups.count(qosPendingLookup->qosLookupId));
		
		xlive_qos_lookups[qosPendingLookup->qosLookupId] = qosPendingLookup;
		
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
	}
	
	return S_OK;
}

// #72
int32_t WINAPI XNetQosRelease(XNQOS* xnqos)
{
	TRACE_FX();
	if (!xnqos) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnqos is NULL.", __func__);
		return WSAEFAULT;
	}
	
	{
		QOS_PENDING_LOOKUP* qosPendingLookup = 0;
		
		EnterCriticalSection(&xlive_critsec_qos_lookups);
		for (auto itrQosPendingLookup = xlive_qos_lookups.begin(); itrQosPendingLookup != xlive_qos_lookups.end(); itrQosPendingLookup++) {
			if (itrQosPendingLookup->second->xnQos == xnqos) {
				qosPendingLookup = itrQosPendingLookup->second;
				break;
			}
		}
		if (qosPendingLookup) {
			xlive_qos_lookups.erase(qosPendingLookup->qosLookupId);
		}
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		
		if (qosPendingLookup) {
			if (qosPendingLookup->qosTransitInfo) {
				delete[] qosPendingLookup->qosTransitInfo;
			}
			if (qosPendingLookup->xnAddrs) {
				delete[] qosPendingLookup->xnAddrs;
			}
			if (qosPendingLookup->xnKids) {
				delete[] qosPendingLookup->xnKids;
			}
			if (qosPendingLookup->xnKeys) {
				delete[] qosPendingLookup->xnKeys;
			}
			if (qosPendingLookup->inAddrs) {
				delete[] qosPendingLookup->inAddrs;
			}
			if (qosPendingLookup->inServiceIds) {
				delete[] qosPendingLookup->inServiceIds;
			}
			// qosPendingLookup->xnQos does not need deleting as it is deleted below.
			delete qosPendingLookup;
		}
	}
	
	for (size_t iXnQoS = 0; iXnQoS < xnqos->cxnqos; iXnQoS++) {
		if (xnqos->axnqosinfo[iXnQoS].cbData && !xnqos->axnqosinfo[iXnQoS].pbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s xnqos (0x%08x) xnqos->axnqosinfo[%u].cbData is not 0 while pbData is NULL."
				, __func__
				, xnqos
				, iXnQoS
			);
		}
		if (xnqos->axnqosinfo[iXnQoS].pbData && !xnqos->axnqosinfo[iXnQoS].cbData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s xnqos (0x%08x) xnqos->axnqosinfo[%u].pbData is not null while cbData is 0."
				, __func__
				, xnqos
				, iXnQoS
			);
		}
		if (xnqos->axnqosinfo[iXnQoS].pbData) {
			delete[] xnqos->axnqosinfo[iXnQoS].pbData;
		}
	}
	
	delete[] xnqos;
	
	return S_OK;
}

// #77
int32_t WINAPI XNetQosGetListenStats(XNKID* xnkid, XNQOSLISTENSTATS* qos_listen_stats)
{
	TRACE_FX();
	if (!xnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnkid is NULL.", __func__);
		return WSAEINVAL;
	}
	if (!qos_listen_stats) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s qos_listen_stats is NULL.", __func__);
		return WSAEINVAL;
	}
	if (qos_listen_stats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (qos_listen_stats->dwSizeOfStruct != sizeof(XNQOSLISTENSTATS)) (0x%08x != 0x%08x)."
			, __func__
			, qos_listen_stats->dwSizeOfStruct
			, sizeof(XNQOSLISTENSTATS)
		);
		return WSAEINVAL;
	}
	
	qos_listen_stats->dwNumDataRequestsReceived = 0;
	qos_listen_stats->dwNumProbesReceived = 0;
	qos_listen_stats->dwNumSlotsFullDiscards = 0;
	qos_listen_stats->dwNumDataRepliesSent = 0;
	qos_listen_stats->dwNumDataReplyBytesSent = 0;
	qos_listen_stats->dwNumProbeRepliesSent = 0;
	
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	
	if (!xlive_qos_listeners.count(*(uint64_t*)xnkid)) {
		LeaveCriticalSection(&xlive_critsec_qos_listeners);
		return WSAEINVAL;
	}
	
	QOS_LISTENER_INFO* qosListenerInfo = xlive_qos_listeners[*(uint64_t*)xnkid];
	// TODO
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
	
	return S_OK;
}

static void ThreadXLiveQoS()
{
	const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
	const size_t packetSizeTypeQosRequest = sizeof(XllnNetworkPacket::QOS_REQUEST);
	const size_t packetSize = packetSizeType + packetSizeTypeQosRequest;
	
	while (1) {
		TRACE_FX();
		
		{
			EnterCriticalSection(&xlive_critsec_qos_lookups);
			for (auto itrQosPendingLookup = xlive_qos_lookups.begin(); itrQosPendingLookup != xlive_qos_lookups.end(); itrQosPendingLookup++) {
				QOS_PENDING_LOOKUP* qosPendingLookup = itrQosPendingLookup->second;
				if (qosPendingLookup->xnQos->cxnqosPending == 0) {
					continue;
				}
				
				for (size_t iXnQoS = 0; iXnQoS < qosPendingLookup->xnQos->cxnqos; iXnQoS++) {
					if (qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags & (XNET_XNQOSINFO_TARGET_DISABLED | XNET_XNQOSINFO_COMPLETE)) {
						continue;
					}
					
					// 7 seconds before probe timeout.
					if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - qosPendingLookup->qosTransitInfo[iXnQoS].timeLastCommSent).count() > 7000) {
						continue;
					}
					
					// TODO currently finishing on the first response.
					// if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent >= qosPendingLookup->countProbes) {
					if (qosPendingLookup->qosTransitInfo[iXnQoS].probesSent >= qosPendingLookup->countProbes || qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv) {
						// remove all these hardcoded completion values.
						qosPendingLookup->qosTransitInfo[iXnQoS].probesSent = qosPendingLookup->countProbes;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = qosPendingLookup->countProbes;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesRecv = qosPendingLookup->countProbes;
						
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags |= XNET_XNQOSINFO_COMPLETE;
						qosPendingLookup->xnQos->axnqosinfo[iXnQoS].bFlags &= ~(uint8_t)(XNET_XNQOSINFO_PARTIAL_COMPLETE);
						
						qosPendingLookup->xnQos->cxnqosPending--;
						
						if (qosPendingLookup->hEvent) {
							SetEvent(qosPendingLookup->hEvent);
						}
						
						continue;
					}
					
					qosPendingLookup->qosTransitInfo[iXnQoS].probesSent++;
					qosPendingLookup->xnQos->axnqosinfo[iXnQoS].cProbesXmit = qosPendingLookup->qosTransitInfo[iXnQoS].probesSent;
					
					if (iXnQoS >= qosPendingLookup->countXn) {
						qosPendingLookup->inAddrs[iXnQoS - qosPendingLookup->countXn];
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s QoS unhandled in_addr."
							, __func__
						);
						continue;
					}
					
					// TODO use ina instead?
					const uint32_t ipv4NBO = qosPendingLookup->xnAddrs[iXnQoS].inaOnline.s_addr;
					const uint16_t portNBO = qosPendingLookup->xnAddrs[iXnQoS].wPortOnline;
					// This address may (hopefully) be an instanceId.
					const uint32_t ipv4HBO = ntohl(ipv4NBO);
					uint16_t portHBO = ntohs(portNBO);
					
					XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
					sendPacket->destinationInstanceId = ipv4HBO;
					{
						sendPacket->dataSize = packetSize;
						sendPacket->data = new uint8_t[sendPacket->dataSize];
						sendPacket->data[0] = XllnNetworkPacket::TYPE::XLLN_NPT_QOS_REQUEST;
						XllnNetworkPacket::QOS_REQUEST &packetQosRequest = *(XllnNetworkPacket::QOS_REQUEST*)&sendPacket->data[packetSizeType];
						
						packetQosRequest.instanceId = xlln_global_instance_id;
						packetQosRequest.qosLookupId = qosPendingLookup->qosLookupId;
						packetQosRequest.sessionId = *(uint64_t*)(&qosPendingLookup->xnKids[iXnQoS]);
						packetQosRequest.probeId = qosPendingLookup->qosTransitInfo[iXnQoS].probesSent;
					}
					
					qosPendingLookup->qosTransitInfo[iXnQoS].timeLastCommSent = std::chrono::high_resolution_clock::now();
					
					if (!SendPacketToRemoteInstance(sendPacket)) {
						delete sendPacket;
					}
					sendPacket = 0;
				}
				
			}
			LeaveCriticalSection(&xlive_critsec_qos_lookups);
		}
		
		DWORD resultWait = WaitForSingleObject(xlive_xnetqos_thread_event, 1000);
		if (resultWait != WAIT_OBJECT_0 && resultWait != STATUS_TIMEOUT) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s failed to wait on xlive_xnetqos_thread_event (0x%zx)."
				, __func__
				, xlive_xnetqos_thread_event
			);
			break;
		}
		
		if (xlive_xnetqos_thread_shutdown) {
			break;
		}
	}
}

void XLiveQosReceiveRequest(XllnNetworkPacket::QOS_REQUEST* packetQosRequest, const SOCKADDR_STORAGE* sockaddr_external)
{
	EnterCriticalSection(&xlive_critsec_qos_listeners);
	
	if (!xlive_qos_listeners.count(packetQosRequest->sessionId)) {
		LeaveCriticalSection(&xlive_critsec_qos_listeners);
		return;
	}
	
	{
		char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s QOS_REQUEST from %s sessionId 0x%016I64x."
			, __func__
			, sockAddrInfo ? sockAddrInfo : "?"
			, packetQosRequest->sessionId
		);
		if (sockAddrInfo) {
			free(sockAddrInfo);
		}
	}
	
	QOS_LISTENER_INFO* qosListenerInfo = xlive_qos_listeners[packetQosRequest->sessionId];
	
	{
		XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
		sendPacket->destinationAddress = *sockaddr_external;
		{
			const int packetSizeType = sizeof(XllnNetworkPacket::TYPE);
			const int packetSizeTypeQosResponse = sizeof(XllnNetworkPacket::QOS_RESPONSE);
			const int packetSize = packetSizeType + packetSizeTypeQosResponse + qosListenerInfo->dataSize;
			
			sendPacket->dataSize = packetSize;
			sendPacket->data = new uint8_t[sendPacket->dataSize];
			sendPacket->data[0] = XllnNetworkPacket::TYPE::XLLN_NPT_QOS_RESPONSE;
			XllnNetworkPacket::QOS_RESPONSE &packetQosResponse = *(XllnNetworkPacket::QOS_RESPONSE*)&sendPacket->data[packetSizeType];
			
			packetQosResponse.qosLookupId = packetQosRequest->qosLookupId;
			packetQosResponse.probeId = packetQosRequest->probeId;
			packetQosResponse.instanceId = xlln_global_instance_id;
			packetQosResponse.sessionId = packetQosRequest->sessionId;
			packetQosResponse.enabled = qosListenerInfo->active ? 1 : 0;
			packetQosResponse.sizeData = qosListenerInfo->dataSize;
			
			if (qosListenerInfo->dataSize) {
				memcpy(&sendPacket->data[packetSizeType + packetSizeTypeQosResponse], qosListenerInfo->pData, qosListenerInfo->dataSize);
			}
		}
		if (!SendPacketToRemoteInstance(sendPacket)) {
			delete sendPacket;
		}
		sendPacket = 0;
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_listeners);
}

void XLiveQosReceiveResponse(XllnNetworkPacket::QOS_RESPONSE* packetQosResponse)
{
	const int packetSizeTypeQosResponse = sizeof(XllnNetworkPacket::QOS_RESPONSE);
	
	std::chrono::steady_clock::time_point responseTime = std::chrono::high_resolution_clock::now();
	
	EnterCriticalSection(&xlive_critsec_qos_lookups);
	
	if (!xlive_qos_lookups.count(packetQosResponse->qosLookupId)) {
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	QOS_PENDING_LOOKUP* qosPendingLookup = xlive_qos_lookups[packetQosResponse->qosLookupId];
	
	// The session id is echoed back from the request, and every member of a session shares that
	// session's XNKID, so it identifies the session rather than which peer answered.
	uint32_t iSessionId;
	for (iSessionId = 0; iSessionId < qosPendingLookup->countXn; iSessionId++) {
		if (*(uint64_t*)(&qosPendingLookup->xnKids[iSessionId]) != packetQosResponse->sessionId) {
			continue;
		}
		if (ntohl(qosPendingLookup->xnAddrs[iSessionId].inaOnline.s_addr) == packetQosResponse->instanceId) {
			break;
		}
	}
	if (iSessionId >= qosPendingLookup->countXn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s QoS unhandled in_addr."
			, __func__
		);
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	if (packetQosResponse->probeId != qosPendingLookup->qosTransitInfo[iSessionId].probesSent) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s QoS old probe."
			, __func__
		);
		LeaveCriticalSection(&xlive_critsec_qos_lookups);
		return;
	}
	
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].cProbesRecv++;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_TARGET_CONTACTED;
	
	// TODO make a list of result pings to properly calculate these ping values. bandwidth idk.
	uint64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(responseTime - qosPendingLookup->qosTransitInfo[iSessionId].timeLastCommSent).count();
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMinInMsecs = milliseconds > 0xFFFFFFFF ? 0xFFFFFFFF : (uint32_t)milliseconds;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMedInMsecs = qosPendingLookup->xnQos->axnqosinfo[iSessionId].wRttMinInMsecs;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].dwUpBitsPerSec = QOS_LISTEN_BITS_PER_SEC_DEFAULT;
	qosPendingLookup->xnQos->axnqosinfo[iSessionId].dwDnBitsPerSec = qosPendingLookup->maxBitsPerSec;
	
	if (packetQosResponse->enabled) {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags &= ~(uint8_t)(XNET_XNQOSINFO_TARGET_DISABLED);
	}
	else {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_TARGET_DISABLED;
	}
	
	if (packetQosResponse->sizeData) {
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].bFlags |= XNET_XNQOSINFO_DATA_RECEIVED;
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData = packetQosResponse->sizeData;
		qosPendingLookup->xnQos->axnqosinfo[iSessionId].pbData = new uint8_t[qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData];
		uint8_t* titleData = (uint8_t*)&(((uint8_t*)packetQosResponse)[packetSizeTypeQosResponse]);
		memcpy(qosPendingLookup->xnQos->axnqosinfo[iSessionId].pbData, titleData, qosPendingLookup->xnQos->axnqosinfo[iSessionId].cbData);
	}
	
	LeaveCriticalSection(&xlive_critsec_qos_lookups);
}

void XLiveThreadQosStart()
{
	TRACE_FX();
	
	XLiveThreadQosStop();
	xlive_xnetqos_thread_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlive_xnetqos_thread_shutdown = false;
	xlive_xnetqos_thread = std::thread(ThreadXLiveQoS);
}

void XLiveThreadQosStop()
{
	TRACE_FX();
	
	if (xlive_xnetqos_thread_event != INVALID_HANDLE_VALUE) {
		xlive_xnetqos_thread_shutdown = true;
		SetEvent(xlive_xnetqos_thread_event);
		xlive_xnetqos_thread.join();
		CloseHandle(xlive_xnetqos_thread_event);
		xlive_xnetqos_thread_event = INVALID_HANDLE_VALUE;
	}
}

// Process-termination path: the OS already killed the thread, so detach it so
// the global's destructor cannot std::terminate() during CRT cleanup.
void XLiveThreadQosAbandon()
{
	if (xlive_xnetqos_thread.joinable()) {
		xlive_xnetqos_thread.detach();
	}
}
