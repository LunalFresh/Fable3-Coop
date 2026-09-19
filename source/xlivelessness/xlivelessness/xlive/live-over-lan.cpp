#include <winsock2.h>
#include "xdefs.hpp"
#include "live-over-lan.hpp"
#include "xsocket.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-network.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include <thread>
#include <vector>

static HANDLE xlln_lol_thread_event = INVALID_HANDLE_VALUE;
static bool xlln_lol_thread_shutdown = false;
static std::thread xlln_lol_thread;

CRITICAL_SECTION xlln_critsec_liveoverlan_broadcast;

CRITICAL_SECTION xlln_critsec_liveoverlan_sessions;
// Key: InstanceId.
std::map<uint32_t, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xlocator;
// Key: InstanceId and session id.
std::map<LIVE_SESSION_REMOTE_KEY, LIVE_SESSION_REMOTE*> liveoverlan_remote_sessions_xsession;

void LiveOverLanBroadcastLocalSessionUnadvertise(const XUID xuid)
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (xlive_xlocator_local_session && xlive_xlocator_local_session->xuid == xuid) {
		LiveOverLanDestroyLiveSession(&xlive_xlocator_local_session);
	}
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
		, "%s broadcasting local session(s) unadvertise."
		, __func__
	);
	
	XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
	sendPacket->destinationInstanceId = INADDR_BROADCAST;
	{
		const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
		const size_t packetSizeTypeLiveOverLanUnadvertise = sizeof(XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE);
		const size_t packetSizeTotal = packetSizeType + packetSizeTypeLiveOverLanUnadvertise;
		
		uint8_t* sendData = new uint8_t[packetSizeTotal];
		{
			size_t iData = 0;
			
			XllnNetworkPacket::TYPE &packetType = *(XllnNetworkPacket::TYPE*)&sendData[iData];
			iData += packetSizeType;
			packetType = XllnNetworkPacket::TYPE::XLLN_NPT_LIVE_OVER_LAN_UNADVERTISE;
			
			XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE &packetLolUnadvertise = *(XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE*)&sendData[iData];
			iData += packetSizeTypeLiveOverLanUnadvertise;
			// Placement over a raw buffer skips the struct's member initialisers; set every field.
			packetLolUnadvertise.instanceId = xlln_global_instance_id;
			packetLolUnadvertise.sessionType = XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR;
			packetLolUnadvertise.xuid = xuid;
		}
		
		sendPacket->data = sendData;
		sendPacket->dataSize = packetSizeTotal;
	}
	
	if (!SendPacketToRemoteInstance(sendPacket)) {
		delete sendPacket;
	}
	sendPacket = 0;
}

void LiveOverLanBroadcastRemoteSessionUnadvertise(const uint32_t instance_id, const uint8_t session_type, const XUID xuid, const uint64_t session_id)
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xlocator.count(instance_id)) {
			LIVE_SESSION_REMOTE* liveSessionRemoteOld = liveoverlan_remote_sessions_xlocator[instance_id];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
			liveoverlan_remote_sessions_xlocator.erase(instance_id);
		}
	}
	else {
		// A session id of zero comes from an instance too old to name one, and means all of them.
		for (auto itrSession = liveoverlan_remote_sessions_xsession.begin(); itrSession != liveoverlan_remote_sessions_xsession.end();) {
			if (itrSession->first.first != instance_id || (session_id && itrSession->first.second != session_id)) {
				itrSession++;
				continue;
			}
			LiveOverLanDestroyLiveSession(&itrSession->second->liveSession);
			delete itrSession->second;
			itrSession = liveoverlan_remote_sessions_xsession.erase(itrSession);
		}
	}
	
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
}

void LiveOverLanAddRemoteLiveSession(const uint8_t session_type, LIVE_SESSION* live_session)
{
	const LIVE_SESSION_REMOTE_KEY sessionKey(live_session->instanceId, *(uint64_t*)&live_session->xnkid);
	
	EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
	
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xlocator.count(live_session->instanceId)) {
			LIVE_SESSION_REMOTE* liveSessionRemoteOld = liveoverlan_remote_sessions_xlocator[live_session->instanceId];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
		}
	}
	else {
		// Delete the old entry if there already is one.
		if (liveoverlan_remote_sessions_xsession.count(sessionKey)) {
			LIVE_SESSION_REMOTE* liveSessionRemoteOld = liveoverlan_remote_sessions_xsession[sessionKey];
			LiveOverLanDestroyLiveSession(&liveSessionRemoteOld->liveSession);
			delete liveSessionRemoteOld;
		}
	}
	
	LIVE_SESSION_REMOTE* liveSessionRemote = new LIVE_SESSION_REMOTE;
	liveSessionRemote->liveSession = live_session;
	
	XllnNetEntityGetXnaddrXnkidByInstanceId(live_session->instanceId, &liveSessionRemote->xnAddr, 0);
	
	time_t ltime;
	time(&ltime);
	liveSessionRemote->timeOfLastContact = (unsigned long)ltime;
	if (session_type == XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
		liveoverlan_remote_sessions_xlocator[live_session->instanceId] = liveSessionRemote;
	}
	else {
		liveoverlan_remote_sessions_xsession[sessionKey] = liveSessionRemote;
	}
	
	LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
}

// A deleted session keeps its entry until the Title closes the handle, so the state has to be tested
// too or a dead session is broadcast for the life of the process.
static bool LiveOverLanIsLocalXSessionAdvertisable(const LIVE_SESSION_XSESSION* xsession_details)
{
	return xsession_details->liveSession->sessionType == XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION
		&& (xsession_details->liveSession->sessionFlags & XSESSION_CREATE_HOST)
		&& xsession_details->eState != XSESSION_STATE_DELETED;
}

static void LiveOverLanBroadcastLocalSession()
{
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	// Guards the local sessions read and serialised below; XCloseHandle frees them from the Title's thread.
	EnterCriticalSection(&xlive_critsec_xsession);
	
	bool anyXSessionLocalSessions = false;
	for (auto const &entry : xlive_xsession_local_sessions) {
		if (LiveOverLanIsLocalXSessionAdvertisable(entry.second)) {
			anyXSessionLocalSessions = true;
			break;
		}
	}
	
	if (!xlive_xlocator_local_session && !anyXSessionLocalSessions) {
		LeaveCriticalSection(&xlive_critsec_xsession);
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		return;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_DEBUG
		, "%s broadcast local session(s) advertise."
		, __func__
	);
	
	if (xlive_xlocator_local_session) {
		XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
		sendPacket->destinationInstanceId = INADDR_BROADCAST;
		if (LiveOverLanSerialiseLiveSessionIntoNetPacket(xlive_xlocator_local_session, &sendPacket->data, &sendPacket->dataSize)) {
			if (!SendPacketToRemoteInstance(sendPacket)) {
				delete sendPacket;
			}
		}
		else {
			delete sendPacket;
		}
		sendPacket = 0;
	}
	
	if (anyXSessionLocalSessions) {
		for (auto const &entry : xlive_xsession_local_sessions) {
			if (!LiveOverLanIsLocalXSessionAdvertisable(entry.second)) {
				continue;
			}
			
			XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
			sendPacket->destinationInstanceId = INADDR_BROADCAST;
			if (LiveOverLanSerialiseLiveSessionIntoNetPacket(entry.second->liveSession, &sendPacket->data, &sendPacket->dataSize)) {
				if (!SendPacketToRemoteInstance(sendPacket)) {
					delete sendPacket;
				}
			}
			else {
				delete sendPacket;
			}
			sendPacket = 0;
		}
	}
	
	LeaveCriticalSection(&xlive_critsec_xsession);
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
}

// Tells peers to drop one hosted session now rather than waiting out the fifteen second expiry.
void LiveOverLanBroadcastLocalXSessionUnadvertise(const XNKID xnkid)
{
	XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
	sendPacket->destinationInstanceId = INADDR_BROADCAST;
	{
		const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
		const size_t packetSizeTypeLiveOverLanUnadvertise = sizeof(XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE);
		const size_t packetSizeTotal = packetSizeType + packetSizeTypeLiveOverLanUnadvertise;
		
		uint8_t* sendData = new uint8_t[packetSizeTotal];
		{
			size_t iData = 0;
			
			XllnNetworkPacket::TYPE &packetType = *(XllnNetworkPacket::TYPE*)&sendData[iData];
			iData += packetSizeType;
			packetType = XllnNetworkPacket::TYPE::XLLN_NPT_LIVE_OVER_LAN_UNADVERTISE;
			
			XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE &packetLolUnadvertise = *(XllnNetworkPacket::LIVE_OVER_LAN_UNADVERTISE*)&sendData[iData];
			iData += packetSizeTypeLiveOverLanUnadvertise;
			// Placement over a raw buffer skips the struct's member initialisers; set every field.
			packetLolUnadvertise.instanceId = xlln_global_instance_id;
			packetLolUnadvertise.sessionType = XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION;
			packetLolUnadvertise.xuid = 0;
			packetLolUnadvertise.xnkid = xnkid;
		}
		
		sendPacket->data = sendData;
		sendPacket->dataSize = packetSizeTotal;
	}
	
	if (!SendPacketToRemoteInstance(sendPacket)) {
		delete sendPacket;
	}
	sendPacket = 0;
}

void LiveOverLanDestroyLiveSession(LIVE_SESSION** live_session)
{
	for (uint32_t iProperty = 0; iProperty < (*live_session)->propertiesCount; iProperty++) {
		XUSER_PROPERTY &property = (*live_session)->pProperties[iProperty];
		if (property.value.type == XUSER_DATA_TYPE_UNICODE) {
			delete[] property.value.string.pwszData;
		}
		else if (property.value.type == XUSER_DATA_TYPE_BINARY && property.value.binary.cbData) {
			delete[] property.value.binary.pbData;
		}
	}
	delete[] (*live_session)->pProperties;
	delete[] (*live_session)->pContexts;
	delete *live_session;
	*live_session = 0;
}

bool LiveOverLanDeserialiseLiveSession(
	const uint8_t* live_session_buffer
	, const size_t live_session_buffer_size
	, LIVE_SESSION** result_live_session
)
{
	if (!result_live_session) {
		return false;
	}
	*result_live_session = 0;
	if (!live_session_buffer) {
		return false;
	}

	LIVE_SESSION &liveSessionSerialised = *(LIVE_SESSION*)live_session_buffer;
	const size_t liveSessionStaticDataSize = (size_t)&liveSessionSerialised.pContexts - (size_t)&liveSessionSerialised;

	// --- Verify the buffer and length to ensure the object passed in is valid and not corrupt or truncated. ---
	{
		size_t bufferSizeCheck = 0;

		bufferSizeCheck += liveSessionStaticDataSize;

		if (bufferSizeCheck > live_session_buffer_size) {
			return false;
		}

		bufferSizeCheck += (liveSessionSerialised.contextsCount * sizeof(*liveSessionSerialised.pContexts));

		if (bufferSizeCheck > live_session_buffer_size) {
			return false;
		}

		for (uint32_t iProperty = 0; iProperty < liveSessionSerialised.propertiesCount; iProperty++) {
			XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)&live_session_buffer[bufferSizeCheck];

			bufferSizeCheck += sizeof(propertySerialised.propertyId);
			bufferSizeCheck += sizeof(propertySerialised.type);

			if (bufferSizeCheck > live_session_buffer_size) {
				return false;
			}

			switch (propertySerialised.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					bufferSizeCheck += sizeof(propertySerialised.nData);
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					bufferSizeCheck += sizeof(propertySerialised.i64Data);
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					bufferSizeCheck += sizeof(propertySerialised.dblData);
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					bufferSizeCheck += sizeof(propertySerialised.fData);
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					bufferSizeCheck += sizeof(propertySerialised.ftData);
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					bufferSizeCheck += sizeof(propertySerialised.binary.cbData);
					if (bufferSizeCheck > live_session_buffer_size) {
						return false;
					}
					bufferSizeCheck += propertySerialised.binary.cbData;
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					bufferSizeCheck += sizeof(propertySerialised.string.cbData);
					if (bufferSizeCheck > live_session_buffer_size) {
						return false;
					}
					size_t dataSize = propertySerialised.string.cbData;
					if (dataSize < sizeof(wchar_t)) {
						return false;
					}
					else if (dataSize % 2) {
						return false;
					}
					bufferSizeCheck += dataSize;
					break;
				}
				case XUSER_DATA_TYPE_NULL: {
					break;
				}
				default: {
					return false;
				}
			}

			if (bufferSizeCheck > live_session_buffer_size) {
				return false;
			}
		}

		if (bufferSizeCheck != live_session_buffer_size) {
			return false;
		}
	}

	// --- Parse out all the contents into a new Live Session struct. ---

	LIVE_SESSION* liveSession = new LIVE_SESSION;

	memcpy(liveSession, live_session_buffer, liveSessionStaticDataSize);

	if (!liveSession->contextsCount) {
		liveSession->pContexts = 0;
	}
	else {
		liveSession->pContexts = new XUSER_CONTEXT[liveSession->contextsCount];
		XUSER_CONTEXT* pXUserContextsSerialised = (XUSER_CONTEXT*)&(live_session_buffer[liveSessionStaticDataSize]);
		for (uint32_t iContext = 0; iContext < liveSession->contextsCount; iContext++) {
			XUSER_CONTEXT &context = liveSession->pContexts[iContext];
			XUSER_CONTEXT &contextSerialised = pXUserContextsSerialised[iContext];
			context = contextSerialised;
		}
	}

	uint8_t* pXUserPropertiesBuffer = (uint8_t*)&(live_session_buffer[liveSessionStaticDataSize + (liveSession->contextsCount * sizeof(*liveSession->pContexts))]);

	if (!liveSession->propertiesCount) {
		liveSession->pProperties = 0;
	}
	else {
		liveSession->pProperties = new XUSER_PROPERTY[liveSession->propertiesCount];
		XUSER_CONTEXT* pXUserContextsSerialised = (XUSER_CONTEXT*)&(live_session_buffer[liveSessionStaticDataSize]);
		for (uint32_t iProperty = 0; iProperty < liveSession->propertiesCount; iProperty++) {
			XUSER_PROPERTY &property = liveSession->pProperties[iProperty];
			XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)pXUserPropertiesBuffer;

			size_t xuserPropertySize = 0;

			xuserPropertySize += sizeof(propertySerialised.propertyId);
			property.dwPropertyId = propertySerialised.propertyId;

			xuserPropertySize += sizeof(propertySerialised.type);
			property.value.type = propertySerialised.type;

			switch (propertySerialised.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					xuserPropertySize += sizeof(propertySerialised.nData);
					property.value.nData = propertySerialised.nData;
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					xuserPropertySize += sizeof(propertySerialised.i64Data);
					property.value.i64Data = propertySerialised.i64Data;
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					xuserPropertySize += sizeof(propertySerialised.dblData);
					property.value.dblData = propertySerialised.dblData;
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					xuserPropertySize += sizeof(propertySerialised.fData);
					property.value.fData = propertySerialised.fData;
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					xuserPropertySize += sizeof(propertySerialised.ftData);
					property.value.ftData = propertySerialised.ftData;
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					xuserPropertySize += sizeof(propertySerialised.binary.cbData);
					property.value.binary.cbData = propertySerialised.binary.cbData;
					if (property.value.binary.cbData == 0) {
						property.value.binary.pbData = 0;
						break;
					}
					xuserPropertySize += property.value.binary.cbData;
					property.value.binary.pbData = new uint8_t[property.value.binary.cbData];
					memcpy(property.value.binary.pbData, &propertySerialised.binary.pbData, property.value.binary.cbData);
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					xuserPropertySize += sizeof(propertySerialised.string.cbData);
					property.value.string.cbData = propertySerialised.string.cbData;
					xuserPropertySize += property.value.string.cbData;
					property.value.string.pwszData = new wchar_t[property.value.string.cbData / 2];
					memcpy(property.value.string.pwszData, &propertySerialised.string.pwszData, property.value.string.cbData);
					property.value.string.pwszData[(property.value.string.cbData / 2) - 1] = 0;
					break;
				}
			}

			pXUserPropertiesBuffer += xuserPropertySize;
		}
	}

	if (pXUserPropertiesBuffer != live_session_buffer + live_session_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s the end result of the pXUserPropertiesBuffer (0x%08x) should not be different from (0x%08x) the result buffer (0x%08x) + buffer size (0x%08x)."
			, __func__
			, pXUserPropertiesBuffer
			, live_session_buffer + live_session_buffer_size
			, live_session_buffer
			, live_session_buffer_size
		);
		__debugbreak();
		return false;
	}

	*result_live_session = liveSession;

	return true;
}

bool LiveOverLanSerialiseLiveSessionIntoNetPacket(
	LIVE_SESSION* live_session
	, uint8_t** result_buffer
	, size_t* result_buffer_size
)
{
	if (result_buffer_size) {
		*result_buffer_size = 0;
	}
	if (result_buffer) {
		*result_buffer = 0;
	}
	if (!live_session || !result_buffer || !result_buffer_size) {
		return false;
	}

	*result_buffer_size += sizeof(XllnNetworkPacket::TYPE);
	const size_t liveSessionStaticDataSize = (size_t)&live_session->pContexts - (size_t)live_session;
	*result_buffer_size += liveSessionStaticDataSize;
	*result_buffer_size += live_session->contextsCount * sizeof(*live_session->pContexts);
	{
		for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
			XUSER_PROPERTY &property = live_session->pProperties[iProperty];

			size_t xuserPropertySize = 0;
			xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::propertyId);
			xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::type);

			switch (property.value.type) {
				case XUSER_DATA_TYPE_CONTEXT:
				case XUSER_DATA_TYPE_INT32: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::nData);
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::i64Data);
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::dblData);
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::fData);
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::ftData);
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					xuserPropertySize += sizeof(XUSER_PROPERTY_SERIALISED::binary.cbData);
					xuserPropertySize += property.value.binary.cbData;
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					size_t dataSize = property.value.string.cbData;
					if (dataSize < sizeof(wchar_t)) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s Property 0x%08x XUSER_DATA_TYPE_UNICODE (dataSize < sizeof(wchar_t)) (%u < %u)."
							, __func__
							, property.dwPropertyId
							, dataSize
							, sizeof(wchar_t)
						);
						dataSize = sizeof(wchar_t);
					}
					else if (dataSize % 2) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
							, "%s Property 0x%08x XUSER_DATA_TYPE_UNICODE dataSize (%u) is odd."
							, __func__
							, property.dwPropertyId
							, dataSize
						);
						dataSize += 1;
					}
					xuserPropertySize += sizeof(dataSize);
					xuserPropertySize += dataSize;
					break;
				}
			}

			*result_buffer_size += xuserPropertySize;
		}
	}

	*result_buffer = new uint8_t[*result_buffer_size];
	((uint8_t*)*result_buffer)[0] = XllnNetworkPacket::TYPE::XLLN_NPT_LIVE_OVER_LAN_ADVERTISE;
	memcpy(&(((uint8_t*)*result_buffer)[sizeof(XllnNetworkPacket::TYPE)]), live_session, liveSessionStaticDataSize);
	XUSER_CONTEXT* pXUserContexts = (XUSER_CONTEXT*)&(((uint8_t*)*result_buffer)[sizeof(XllnNetworkPacket::TYPE) + liveSessionStaticDataSize]);
	for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
		XUSER_CONTEXT &context = live_session->pContexts[iContext];
		XUSER_CONTEXT &contextSerialised = pXUserContexts[iContext];
		contextSerialised = context;
	}

	uint8_t* pXUserPropertiesBuffer = (uint8_t*)&(((uint8_t*)*result_buffer)[sizeof(XllnNetworkPacket::TYPE) + liveSessionStaticDataSize + (live_session->contextsCount * sizeof(*live_session->pContexts))]);
	for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
		XUSER_PROPERTY &property = live_session->pProperties[iProperty];
		XUSER_PROPERTY_SERIALISED &propertySerialised = *(XUSER_PROPERTY_SERIALISED*)pXUserPropertiesBuffer;
		
		size_t xuserPropertySize = 0;

		xuserPropertySize += sizeof(propertySerialised.propertyId);
		propertySerialised.propertyId = property.dwPropertyId;

		xuserPropertySize += sizeof(propertySerialised.type);
		propertySerialised.type = property.value.type;

		switch (propertySerialised.type) {
			case XUSER_DATA_TYPE_CONTEXT:
			case XUSER_DATA_TYPE_INT32: {
				xuserPropertySize += sizeof(propertySerialised.nData);
				propertySerialised.nData = property.value.nData;
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				xuserPropertySize += sizeof(propertySerialised.i64Data);
				propertySerialised.i64Data = property.value.i64Data;
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				xuserPropertySize += sizeof(propertySerialised.dblData);
				propertySerialised.dblData = property.value.dblData;
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				xuserPropertySize += sizeof(propertySerialised.fData);
				propertySerialised.fData = property.value.fData;
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				xuserPropertySize += sizeof(propertySerialised.ftData);
				propertySerialised.ftData = property.value.ftData;
				break;
			}
			case XUSER_DATA_TYPE_BINARY: {
				xuserPropertySize += sizeof(propertySerialised.binary.cbData);
				propertySerialised.binary.cbData = property.value.binary.cbData;
				if (propertySerialised.binary.cbData == 0) {
					break;
				}
				xuserPropertySize += property.value.binary.cbData;
				memcpy(&propertySerialised.binary.pbData, property.value.binary.pbData, propertySerialised.binary.cbData);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				size_t dataSize = property.value.string.cbData;
				if (dataSize < sizeof(wchar_t)) {
					dataSize = sizeof(wchar_t);
				}
				else if (dataSize % 2) {
					dataSize += 1;
				}
				xuserPropertySize += sizeof(dataSize);
				propertySerialised.string.cbData = dataSize;
				xuserPropertySize += dataSize;
				memcpy(&propertySerialised.string.pwszData, property.value.string.pwszData, dataSize);
				((wchar_t*)&propertySerialised.string.pwszData)[(dataSize / 2) - 1] = 0;
				break;
			}
			case XUSER_DATA_TYPE_NULL: {
				break;
			}
			default: {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
					, "%s Property 0x%08x has unknown XUSER_DATA_TYPE (0x%02hhx). Changing to NULL."
					, __func__
					, propertySerialised.propertyId
					, propertySerialised.type
				);
				propertySerialised.type = XUSER_DATA_TYPE_NULL;
				break;
			}
		}

		pXUserPropertiesBuffer += xuserPropertySize;
	}

	if (pXUserPropertiesBuffer != *result_buffer + *result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
			, "%s the end result of the pXUserPropertiesBuffer (0x%08x) should not be different from (0x%08x) the result buffer (0x%08x) + buffer size (0x%08x)."
			, __func__
			, pXUserPropertiesBuffer
			, *result_buffer + *result_buffer_size
			, *result_buffer
			, *result_buffer_size
		);
		__debugbreak();
		return false;
	}
	
	return true;
}

static void ThreadLiveOverLan()
{
	while (1) {
		TRACE_FX();
		
		{
			EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
			
			__time64_t ltime;
			_time64(&ltime);//seconds since epoch.
			uint64_t timetoremove = ((uint64_t)ltime) - 15;
			std::vector<uint32_t> removesessions;
			
			for (auto const &session : liveoverlan_remote_sessions_xlocator) {
				if (session.second->timeOfLastContact > timetoremove) {
					continue;
				}
				LiveOverLanDestroyLiveSession(&session.second->liveSession);
				delete session.second;
				removesessions.push_back(session.first);
			}
			for (auto const &session : removesessions) {
				liveoverlan_remote_sessions_xlocator.erase(session);
			}
			removesessions.clear();
			
			std::vector<LIVE_SESSION_REMOTE_KEY> removesessionsxsession;
			for (auto const &session : liveoverlan_remote_sessions_xsession) {
				if (session.second->timeOfLastContact > timetoremove) {
					continue;
				}
				LiveOverLanDestroyLiveSession(&session.second->liveSession);
				delete session.second;
				removesessionsxsession.push_back(session.first);
			}
			for (auto const &session : removesessionsxsession) {
				liveoverlan_remote_sessions_xsession.erase(session);
			}
			removesessionsxsession.clear();
			
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
		}
		
		LiveOverLanBroadcastLocalSession();
		
		DWORD resultWait = WaitForSingleObject(xlln_lol_thread_event, 10*1000);
		if (resultWait != WAIT_OBJECT_0 && resultWait != STATUS_TIMEOUT) {
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s failed to wait on xlln_lol_thread_event (0x%zx)."
				, __func__
				, xlln_lol_thread_event
			);
			break;
		}
		
		if (xlln_lol_thread_shutdown) {
			break;
		}
	}
	
	{
		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
		
		for (auto const &session : liveoverlan_remote_sessions_xlocator) {
			LiveOverLanDestroyLiveSession(&session.second->liveSession);
			delete session.second;
		}
		liveoverlan_remote_sessions_xlocator.clear();
		
		for (auto const &session : liveoverlan_remote_sessions_xsession) {
			LiveOverLanDestroyLiveSession(&session.second->liveSession);
			delete session.second;
		}
		liveoverlan_remote_sessions_xsession.clear();
		
		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	}
	
	{
		EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
		if (xlive_xlocator_local_session) {
			LiveOverLanDestroyLiveSession(&xlive_xlocator_local_session);
		}
		xlive_xlocator_local_session = 0;
		
		LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	}
}

void XllnThreadLiveOverLanStart()
{
	TRACE_FX();
	
	XllnThreadLiveOverLanStop();
	xlln_lol_thread_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	xlln_lol_thread_shutdown = false;
	xlln_lol_thread = std::thread(ThreadLiveOverLan);
}

void XllnThreadLiveOverLanStop()
{
	TRACE_FX();

	if (xlln_lol_thread_event != INVALID_HANDLE_VALUE) {
		xlln_lol_thread_shutdown = true;
		SetEvent(xlln_lol_thread_event);
		xlln_lol_thread.join();
		CloseHandle(xlln_lol_thread_event);
		xlln_lol_thread_event = INVALID_HANDLE_VALUE;
	}
}

// Process-termination path: the OS already killed the thread, so detach it so
// the global's destructor cannot std::terminate() during CRT cleanup.
void XllnThreadLiveOverLanAbandon()
{
	if (xlln_lol_thread.joinable()) {
		xlln_lol_thread.detach();
	}
}
