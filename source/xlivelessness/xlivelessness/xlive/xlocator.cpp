#include <winsock2.h>
#include "xdefs.hpp"
#include "xfuncs.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "xnet.hpp"
#include "xlive.hpp"
#include <thread>
#include <vector>

static bool xlive_xlocator_initialized = false;

CRITICAL_SECTION xlive_critsec_xlocator_enumerators;
// Key: enumerator handle (id).
// Value: Vector of InstanceIds that have already been returned for that enumerator.
std::map<HANDLE, std::vector<uint32_t>> xlive_xlocator_enumerators;
LIVE_SESSION* xlive_xlocator_local_session = 0;

// #5230
HRESULT WINAPI XLocatorServerAdvertise(
	uint32_t user_index
	, uint32_t server_type
	, XNKID xnkid
	, XNKEY xnkey
	, uint32_t max_public_slots
	, uint32_t max_private_slots
	, uint32_t filled_public_slots
	, uint32_t filled_private_slots
	, size_t server_property_count
	, XUSER_PROPERTY* server_properties
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (server_type > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_type (0x%08x) is bigger than INT_MAX.", __func__, server_type);
		return E_INVALIDARG;
	}
	if (!XNetXnKidIsOnlinePeer(&xnkid)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !XNetXnKidIsOnlinePeer(&xnkid).", __func__);
		return E_INVALIDARG;
	}
	if (max_public_slots < filled_public_slots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s max_public_slots (%u) > filled_public_slots (%u).", __func__, max_public_slots, filled_public_slots);
		return E_INVALIDARG;
	}
	if (max_private_slots < filled_private_slots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s max_private_slots (%u) > filled_private_slots (%u).", __func__, max_private_slots, filled_private_slots);
		return E_INVALIDARG;
	}
	if (max_public_slots > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s max_public_slots (0x%08x) is bigger than INT_MAX.", __func__, max_public_slots);
		return E_INVALIDARG;
	}
	if (max_private_slots > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s max_private_slots (0x%08x) is bigger than INT_MAX.", __func__, max_private_slots);
		return E_INVALIDARG;
	}
	if (!server_property_count && server_properties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!server_property_count && server_properties).", __func__);
		return E_INVALIDARG;
	}
	if (server_property_count && !server_properties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (server_property_count && !server_properties).", __func__);
		return E_INVALIDARG;
	}
	if (server_property_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_property_count (0x%zx) is bigger than INT_MAX.", __func__, server_property_count);
		return E_INVALIDARG;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}

	LIVE_SESSION* liveSession = new LIVE_SESSION;
	liveSession->instanceId = xlln_global_instance_id;
	liveSession->xuid = xlive_local_users[user_index].xuid;
	liveSession->sessionType = XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR;
	liveSession->sessionFlags = server_type;
	liveSession->xnkid = xnkid;
	liveSession->xnkey = xnkey;
	liveSession->slotsPublicMaxCount = max_public_slots;
	liveSession->slotsPublicFilledCount = filled_public_slots;
	liveSession->slotsPrivateMaxCount = max_private_slots;
	liveSession->slotsPrivateFilledCount = filled_private_slots;
	liveSession->slotsPrivateFilledCount = filled_private_slots;
	
	std::map<uint32_t, XUSER_PROPERTY*> xuserProperties;
	
	for (size_t iProperty = 0; iProperty < server_property_count; iProperty++) {
		XUSER_PROPERTY &propertyOrig = server_properties[iProperty];
		
		if (xuserProperties.count(propertyOrig.dwPropertyId)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XUser Property 0x%08x exists more than once."
				, __func__
				, propertyOrig.dwPropertyId
			);
			continue;
		}
		
		XUSER_PROPERTY* propertyListCopy = new XUSER_PROPERTY;
		*propertyListCopy = propertyOrig;
		
		switch (propertyListCopy->value.type) {
			case XUSER_DATA_TYPE_BINARY: {
				if (propertyListCopy->value.binary.cbData == 0) {
					propertyListCopy->value.binary.pbData = 0;
					break;
				}
				propertyListCopy->value.binary.pbData = new uint8_t[propertyListCopy->value.binary.cbData];
				memcpy(propertyListCopy->value.binary.pbData, propertyOrig.value.binary.pbData, propertyListCopy->value.binary.cbData);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				if (propertyListCopy->value.string.cbData < sizeof(wchar_t)) {
					propertyListCopy->value.string.cbData = sizeof(wchar_t);
				}
				else if (propertyListCopy->value.string.cbData % 2) {
					propertyListCopy->value.string.cbData += 1;
				}
				propertyListCopy->value.string.pwszData = new wchar_t[propertyListCopy->value.string.cbData / 2];
				memcpy(propertyListCopy->value.string.pwszData, propertyOrig.value.string.pwszData, propertyListCopy->value.string.cbData);
				propertyListCopy->value.string.pwszData[(propertyListCopy->value.string.cbData / 2) - 1] = 0;
				break;
			}
		}
		
		xuserProperties[propertyListCopy->dwPropertyId] = propertyListCopy;
	}
	
	// Add username as server name if it does not already exist.
	if (!xuserProperties.count(XUSER_PROPERTY_SERVER_NAME)) {
		XUSER_PROPERTY* property = new XUSER_PROPERTY;
		property->dwPropertyId = XUSER_PROPERTY_SERVER_NAME;
		property->value.type = XUSER_DATA_TYPE_UNICODE;
		size_t usernameLenSize = strnlen(xlive_local_users[user_index].username, sizeof(xlive_local_users[user_index].username) - 1) + 1;
		property->value.string.cbData = (uint32_t)(usernameLenSize * sizeof(wchar_t));
		property->value.string.pwszData = new wchar_t[usernameLenSize];
		swprintf_s(property->value.string.pwszData, usernameLenSize, L"%hs", xlive_local_users[user_index].username);
		
		xuserProperties[XUSER_PROPERTY_SERVER_NAME] = property;
	}
	
	liveSession->propertiesCount = (uint32_t)xuserProperties.size();
	liveSession->pProperties = new XUSER_PROPERTY[liveSession->propertiesCount];
	{
		uint32_t iProperty = 0;
		for (auto const &entry : xuserProperties) {
			XUSER_PROPERTY* propertyListCopy = entry.second;
			XUSER_PROPERTY &propertyCopy = liveSession->pProperties[iProperty++];
			memcpy(&propertyCopy, propertyListCopy, sizeof(propertyCopy));
			delete propertyListCopy;
		}
	}
	xuserProperties.clear();
	
	EnterCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	if (xlive_xlocator_local_session) {
		LiveOverLanDestroyLiveSession(&xlive_xlocator_local_session);
	}
	xlive_xlocator_local_session = liveSession;
	LeaveCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5231
HRESULT WINAPI XLocatorServerUnAdvertise(uint32_t user_index, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}
	
	LiveOverLanBroadcastLocalSessionUnadvertise(xlive_local_users[user_index].xuid);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	
	return S_OK;
}

// #5233
HRESULT WINAPI XLocatorGetServiceProperty(uint32_t user_index, size_t server_property_count, XUSER_PROPERTY* server_properties, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return E_INVALIDARG;
	}
	if (!server_property_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_property_count is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (server_property_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_property_count (0x%zx) is bigger than INT_MAX.", __func__, server_property_count);
		return E_INVALIDARG;
	}
	if (!server_properties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_properties is NULL.", __func__);
		return E_POINTER;
	}
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		return E_FAIL;
	}
	
	uint32_t countTotal = 0;
	uint32_t countPublic = 0;
	uint32_t countGold = 0;
	uint32_t countPeer = 0;
	
	{
		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
		
		for (auto const &session : liveoverlan_remote_sessions_xlocator) {
			// Ensure this is an XLocator item.
			if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
				continue;
			}
			countTotal++;
			switch (session.second->liveSession->sessionFlags) {
				case XLOCATOR_SERVERTYPE_PUBLIC: {
					countPublic++;
					break;
				}
				case XLOCATOR_SERVERTYPE_GOLD_ONLY: {
					countGold++;
					break;
				}
				case XLOCATOR_SERVERTYPE_PEER_HOSTED: {
					countPeer++;
					break;
				}
				case XLOCATOR_SERVERTYPE_PEER_HOSTED_GOLD_ONLY: {
					countGold++;
					countPeer++;
					break;
				}
			}
		}
		
		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	}
	
	for (size_t iProperty = 0; iProperty < server_property_count; iProperty++) {
		XUSER_PROPERTY &property = server_properties[iProperty];
		switch (property.dwPropertyId) {
			case XLOCATOR_PROPERTY_LIVE_COUNT_TOTAL: {
				if (property.value.type == XUSER_DATA_TYPE_INT32) {
					property.value.nData = countTotal;
				}
				break;
			}
			case XLOCATOR_PROPERTY_LIVE_COUNT_PUBLIC: {
				if (property.value.type == XUSER_DATA_TYPE_INT32) {
					property.value.nData = countPublic;
				}
				break;
			}
			case XLOCATOR_PROPERTY_LIVE_COUNT_GOLD: {
				if (property.value.type == XUSER_DATA_TYPE_INT32) {
					property.value.nData = countGold;
				}
				break;
			}
			case XLOCATOR_PROPERTY_LIVE_COUNT_PEER: {
				if (property.value.type == XUSER_DATA_TYPE_INT32) {
					property.value.nData = countPeer;
				}
				break;
			}
			default: {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s Unknown XUser Property 0x%08x of type 0x%02hhx."
					, __func__
					, property.dwPropertyId
					, property.value.type
				);
				break;
			}
		}
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
	}
	
	return S_OK;
}

static bool ValidateXLocatorServerEnumeratorPropertyIds(size_t required_property_id_count, const uint32_t* required_property_ids)
{
	for (size_t iProperty = 0; iProperty < required_property_id_count; iProperty++) {
		const uint32_t &propertyId = required_property_ids[iProperty];
		if (
			propertyId < 0x10008205
			|| (
				propertyId > 0x1000821A
				&& propertyId <= 0x10008230
			)
			|| (
				//propertyId > 0x2000821B + 9
				propertyId > XUSER_PROPERTY_UNKNOWN_INT64 + 9
				&& propertyId < 0x2000822F
			)
			|| (
				propertyId > 0x2000822F
				//&& propertyId < 0x40008225
				&& propertyId < XUSER_PROPERTY_SERVER_DESC
			)
			|| (
				propertyId > 0x4000822E
				//&& propertyId != 0x40008230
				&& propertyId != XUSER_PROPERTY_SERVER_NAME
			)
		) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s required_property_ids[%zu] (0x%08x) is invalid.", __func__, iProperty, propertyId);
			return false;
		}
	}
	return true;
}

// #5234
uint32_t WINAPI XLocatorCreateServerEnumerator(
	uint32_t user_index
	, size_t enumeration_item_count
	, size_t required_property_id_count
	, const uint32_t* required_property_ids
	, size_t filter_group_count
	, const XLOCATOR_FILTER_GROUP* filter_groups
	, size_t sorter_count
	, const XLOCATOR_SORTER* sorters
	, size_t* result_buffer_size
	, HANDLE* enumerator_handle
)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!enumeration_item_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_item_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (enumeration_item_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_item_count (0x%zx) is bigger than INT_MAX.", __func__, enumeration_item_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (required_property_id_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s required_property_id_count (0x%zx) is bigger than INT_MAX.", __func__, required_property_id_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (required_property_id_count && !required_property_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (required_property_id_count && !required_property_ids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!ValidateXLocatorServerEnumeratorPropertyIds(required_property_id_count, required_property_ids)) {
		return ERROR_INVALID_PARAMETER;
	}
	if (filter_group_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s filter_group_count (0x%zx) is bigger than INT_MAX.", __func__, filter_group_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (filter_group_count && !filter_groups) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (filter_group_count && !filter_groups).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (size_t i = 0; i < filter_group_count; i++) {
		if (filter_groups[i].unk1 && !filter_groups[i].unk2) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s (filter_groups[i].unk1 && !filter_groups[i].unk2) i (%u), unk1 (0x%08x), unk2 (0x%08x)."
				, __func__
				, i
				, filter_groups[i].unk1
				, filter_groups[i].unk2
			);
			return ERROR_INVALID_PARAMETER;
		}
	}
	if (sorter_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s sorter_count (0x%zx) is bigger than INT_MAX.", __func__, sorter_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (sorter_count && !sorters) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (sorter_count && !sorters).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (size_t i = 0; i < sorter_count; i++) {
		for (size_t j = i+1; j < sorter_count; j++) {
			if (sorters[i].unknownFlag == sorters[j].unknownFlag) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s duplicate sorters (0x%08x) in sorters.", __func__, sorters[i].unknownFlag);
				return ERROR_INVALID_PARAMETER;
			}
		}
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	
	if (enumeration_item_count > 200) {
		enumeration_item_count = 200;
	}
	*result_buffer_size = (enumeration_item_count) * sizeof(XLOCATOR_SEARCHRESULT);
	*enumerator_handle = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
	xlive_xlocator_enumerators[*enumerator_handle];
	LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	
	return S_OK;
}

// #5235
uint32_t WINAPI XLocatorCreateServerEnumeratorByIDs(
	uint32_t user_index
	, size_t enumeration_item_count
	, size_t required_property_id_count
	, const uint32_t* required_property_ids
	, size_t server_id_count
	, const uint64_t* server_ids
	, size_t* result_buffer_size
	, HANDLE* enumerator_handle
)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!enumeration_item_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_item_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (enumeration_item_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_item_count (0x%zx) is bigger than INT_MAX.", __func__, enumeration_item_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (required_property_id_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s required_property_id_count (0x%zx) is bigger than INT_MAX.", __func__, required_property_id_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (required_property_id_count && !required_property_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (required_property_id_count && !required_property_ids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!ValidateXLocatorServerEnumeratorPropertyIds(required_property_id_count, required_property_ids)) {
		return ERROR_INVALID_PARAMETER;
	}
	if (!server_id_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_id_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (server_id_count > INT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_id_count (0x%zx) is bigger than INT_MAX.", __func__, server_id_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (!server_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_ids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xlive_net_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	
	if (enumeration_item_count > 200) {
		enumeration_item_count = 200;
	}
	*result_buffer_size = (enumeration_item_count) * sizeof(XLOCATOR_SEARCHRESULT);
	*enumerator_handle = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
	xlive_xlocator_enumerators[*enumerator_handle];
	LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	
	return S_OK;
}

// #5236
HRESULT WINAPI XLocatorServiceInitialize(XLOCATOR_INIT_INFO* init_info, HANDLE* xlocator_service_handle)
{
	TRACE_FX();
	if (!init_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s init_info is NULL.", __func__);
		return E_POINTER;
	}
	if (xlive_netsocket_abort) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive NetSocket is disabled.", __func__);
		return E_FAIL;
	}
	
	if (xlocator_service_handle) {
		*xlocator_service_handle = CreateMutex(NULL, NULL, NULL);
	}
	
	xlive_xlocator_initialized = true;
	
	return S_OK;
}

// #5237
HRESULT WINAPI XLocatorServiceUnInitialize(HANDLE xlocator_service_handle)
{
	TRACE_FX();
	if (!xlive_xlocator_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XLocator is not initialised.", __func__);
		SetLastError(ERROR_FUNCTION_FAILED);
		return E_FAIL;
	}
	
	xlive_xlocator_initialized = false;
	
	if (!xlocator_service_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlocator_service_handle is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return S_FALSE;
	}
	if (xlocator_service_handle == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlocator_service_handle is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return S_FALSE;
	}
	if (!CloseHandle(xlocator_service_handle)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to close xlocator_service_handle handle 0x%zx.", __func__, xlocator_service_handle);
		SetLastError(ERROR_INVALID_HANDLE);
		return S_FALSE;
	}
	return S_OK;
}

// #5238
HRESULT WINAPI XLocatorCreateKey(XNKID* xnkid, XNKEY* xnkey)
{
	TRACE_FX();
	if (!xnkid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnkid is NULL.", __func__);
		return E_POINTER;
	}
	if (!xnkey) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xnkey is NULL.", __func__);
		return E_POINTER;
	}
	int32_t result = XNetCreateKey(xnkid, xnkey);
	if (result == S_OK && xnkid) {
		xnkid->ab[0] &= ~XNET_XNKID_MASK;
		xnkid->ab[0] |= XNET_XNKID_ONLINE_PEER;
	}
	return result;
}
