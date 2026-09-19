#include <winsock2.h>
#include "xdefs.hpp"
#include "xsession.hpp"
#include "live-over-lan.hpp"
#include "../xlln/xlln-network.hpp"
#include "../xlln/debug-log.hpp"
#include "xlive.hpp"
#include "xnet.hpp"
#include "xuser.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-stats.hpp"
#include "../xlln/wnd-user-card.hpp"
#include <algorithm>

CRITICAL_SECTION xlive_critsec_xsession;
// Key: session handle (id).
// Value: the details of the session.
std::map<HANDLE, LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;

static bool xlive_xsession_initialised = false;

int32_t InitXSession()
{
	TRACE_FX();
	if (xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is already initialised.", __func__);
		return E_UNEXPECTED;
	}
	
	xlive_xsession_initialised = true;
	
	return S_OK;
}

int32_t UninitXSession()
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return E_UNEXPECTED;
	}
	
	xlive_xsession_initialised = false;
	
	return S_OK;
}

// #5300
uint32_t WINAPI XSessionCreate(uint32_t create_flags, uint32_t user_index, size_t max_public_slots, size_t max_private_slots, uint64_t* session_nonce, XSESSION_INFO* session_info, XOVERLAPPED* xoverlapped, HANDLE* session_handle)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!session_nonce) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_nonce is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!session_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	//FIXME unknown macro or their mistake?
	if (create_flags & ~(XSESSION_CREATE_USES_MASK | XSESSION_CREATE_MODIFIERS_MASK | 0x1000)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s FIXME.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((create_flags & XSESSION_CREATE_USES_MATCHMAKING) && !(create_flags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_USES_MATCHMAKING) && !(create_flags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((create_flags & XSESSION_CREATE_USES_ARBITRATION) && !(create_flags & XSESSION_CREATE_USES_STATS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_USES_ARBITRATION) && !(create_flags & XSESSION_CREATE_USES_STATS)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((create_flags & XSESSION_CREATE_USES_ARBITRATION) && !(create_flags & XSESSION_CREATE_USES_PEER_NETWORK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_USES_ARBITRATION) && !(create_flags & XSESSION_CREATE_USES_PEER_NETWORK)).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((create_flags & XSESSION_CREATE_HOST) && !(create_flags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_HOST) && !(create_flags & (XSESSION_CREATE_USES_PEER_NETWORK | XSESSION_CREATE_USES_STATS | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (create_flags & XSESSION_CREATE_MODIFIERS_MASK) {
		if (!(create_flags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_MODIFIERS_MASK) && !(create_flags & (XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_MATCHMAKING))).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (!(create_flags & XSESSION_CREATE_USES_PRESENCE) && (create_flags & XSESSION_CREATE_USES_MATCHMAKING) && (create_flags & XSESSION_CREATE_MODIFIERS_MASK) != (create_flags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_MODIFIERS_MASK) && !(create_flags & XSESSION_CREATE_USES_PRESENCE) && (create_flags & XSESSION_CREATE_USES_MATCHMAKING) && (create_flags & XSESSION_CREATE_MODIFIERS_MASK) != (create_flags & XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if ((create_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (create_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ((create_flags & XSESSION_CREATE_MODIFIERS_MASK) && (create_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (create_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)).", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	LIVE_SESSION_XSESSION* xsessionDetails = new LIVE_SESSION_XSESSION;
	xsessionDetails->liveSession = new LIVE_SESSION;
	xsessionDetails->liveSession->sessionType = XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION;
	
	if (create_flags & XSESSION_CREATE_HOST) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s User %u is hosting a session."
			, __func__
			, user_index
		);
		
		xsessionDetails->liveSession->instanceId = xlln_global_instance_id;
		xsessionDetails->liveSession->xuid = xlive_local_users[user_index].xuid;
		xsessionDetails->gameType = 0;
		xsessionDetails->gameMode = 0;
		xsessionDetails->qwNonce = xlive_local_users[user_index].xuid;

		// The address is derived from this machine's instanceId, but the session key must be UNIQUE
		// per session (a host can run concurrent sessions, e.g. a party and a game), so generate it
		// separately instead of deriving it from the instanceId too.
		XllnNetEntityGetXnaddrXnkidByInstanceId(xlln_global_instance_id, &xsessionDetails->xnAddr, 0);
		XllnGenerateSessionXnkid(&xsessionDetails->liveSession->xnkid);
		
		// TODO XNKEY
		memset(&xsessionDetails->liveSession->xnkey, 0xAA, sizeof(XNKEY));
		
		session_info->hostAddress = xsessionDetails->xnAddr;
		session_info->keyExchangeKey = xsessionDetails->liveSession->xnkey;
		session_info->sessionID = xsessionDetails->liveSession->xnkid;
		
		*session_nonce = xsessionDetails->qwNonce;
		
		{
			EnterCriticalSection(&xlive_critsec_xuser_context_properties);
			
			xsessionDetails->liveSession->contextsCount = (uint32_t)xlive_user_contexts[user_index].size();
			xsessionDetails->liveSession->pContexts = new XUSER_CONTEXT[xsessionDetails->liveSession->contextsCount];
			
			{
				uint32_t iContext = 0;
				for (auto const &entry : xlive_user_contexts[user_index]) {
					XUSER_CONTEXT &contextCopy = xsessionDetails->liveSession->pContexts[iContext++];
					contextCopy.dwContextId = entry.first;
					contextCopy.dwValue = entry.second;
					
					switch (contextCopy.dwContextId) {
						case X_CONTEXT_GAME_MODE: {
							xsessionDetails->gameMode = contextCopy.dwValue;
							break;
						}
						case X_CONTEXT_GAME_TYPE: {
							xsessionDetails->gameType = contextCopy.dwValue;
							break;
						}
					}
				}
			}
			
			const auto &userProperties = xlive_user_properties[user_index];
			
			xsessionDetails->liveSession->propertiesCount = (uint32_t)userProperties.size();
			xsessionDetails->liveSession->pProperties = new XUSER_PROPERTY[xsessionDetails->liveSession->propertiesCount];
			{
				uint32_t iProperty = 0;
				for (const auto &entry : userProperties) {
					XUSER_PROPERTY &propertyCopy = xsessionDetails->liveSession->pProperties[iProperty++];
					propertyCopy.dwPropertyId = entry.first;
					propertyCopy.value = entry.second;
					
					switch(propertyCopy.value.type) {
						case XUSER_DATA_TYPE_UNICODE: {
							propertyCopy.value.string.pwszData = new wchar_t[propertyCopy.value.string.cbData/sizeof(*XUSER_DATA::string.pwszData)];
							memcpy(propertyCopy.value.string.pwszData, entry.second.string.pwszData, propertyCopy.value.string.cbData);
							break;
						}
						case XUSER_DATA_TYPE_BINARY: {
							propertyCopy.value.binary.pbData = new uint8_t[propertyCopy.value.binary.cbData];
							memcpy(propertyCopy.value.binary.pbData, entry.second.binary.pbData, propertyCopy.value.binary.cbData);
							break;
						}
					}
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
		}
	}
	else {
		xsessionDetails->liveSession->instanceId = 0;
		xsessionDetails->liveSession->xuid = INVALID_XUID;
		
		xsessionDetails->xnAddr = session_info->hostAddress;
		xsessionDetails->liveSession->xnkey = session_info->keyExchangeKey;
		xsessionDetails->liveSession->xnkid = session_info->sessionID;
		
		xsessionDetails->qwNonce = *session_nonce;
	}
	
	memset(&xsessionDetails->xnkidArbitration, 0x8B, sizeof(XNKID));
	
	xsessionDetails->liveSession->sessionFlags = create_flags;
	
	xsessionDetails->liveSession->slotsPublicMaxCount = max_public_slots;
	xsessionDetails->liveSession->slotsPrivateMaxCount = max_private_slots;
	xsessionDetails->liveSession->slotsPublicFilledCount = 0;
	xsessionDetails->liveSession->slotsPrivateFilledCount = 0;
	
	*session_handle = CreateMutex(NULL, NULL, NULL);
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		xlive_xsession_local_sessions[*session_handle] = xsessionDetails;
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return ERROR_SUCCESS;
}

// #5317
uint32_t WINAPI XSessionWriteStats(HANDLE session_handle, XUID user_xuid, size_t session_view_count, const XSESSION_VIEW_PROPERTIES* session_views, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!session_view_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_view_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!session_views) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_views is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				const auto &itrSessionMember = xsessionDetails->sessionMembers.find(user_xuid);
				if (itrSessionMember == xsessionDetails->sessionMembers.end()) {
					result = XONLINE_E_SESSION_REGISTRATION_ERROR;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XSession (0x%zx) member XUID (0x%016I64x) not found."
						, __func__
						, session_handle
						, user_xuid
					);
				}
				else {
					result = ERROR_SUCCESS;

					// Validate every view first; storing as we go would leave a rejected call half applied.
					for (size_t iSessionView = 0; iSessionView < session_view_count; iSessionView++) {
						const XSESSION_VIEW_PROPERTIES &sessionView = session_views[iSessionView];
						if (sessionView.dwNumProperties && !sessionView.pProperties) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s view 0x%08x declares %u properties but supplies no array.", __func__, sessionView.dwViewId, sessionView.dwNumProperties);
							result = ERROR_INVALID_PARAMETER;
							break;
						}
					}

					for (size_t iSessionView = 0; result == ERROR_SUCCESS && iSessionView < session_view_count; iSessionView++) {
						const XSESSION_VIEW_PROPERTIES &sessionView = session_views[iSessionView];
						for (size_t iProperty = 0; iProperty < sessionView.dwNumProperties; iProperty++) {
							const XUSER_PROPERTY &sessionProperty = sessionView.pProperties[iProperty];
							// The key below keeps only the low id bits, so a system property would land on the cell of the
							// title column sharing them (X_PROPERTY_GAMERNAME and a title's own id 2 both key 2). Live fills
							// these from session state and no title reads one back as a stat, so drop them rather than collide.
							if (XISSYSTEMPROPERTY(sessionProperty.dwPropertyId)) {
								continue;
							}
							// Key each stat by the low bits of its property id (its column id). A title's read
							// request addresses the field by a different id, which XllnStatsResolveColumnId maps back.
							uint32_t columnId = sessionProperty.dwPropertyId & X_PROPERTY_ID_MASK;
							// Deep-copies so a UNICODE/BINARY value outlives the title's transient buffer.
							XllnStatsStoreValue(user_xuid, sessionView.dwViewId, columnId, sessionProperty.value);
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s stored view 0x%08x column %u (propId 0x%08x) type %hhu value 0x%016I64x.", __func__, sessionView.dwViewId, columnId, sessionProperty.dwPropertyId, sessionProperty.value.type, sessionProperty.value.i64Data);
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	// Persist outside the session lock. Only a local user signed in to LIVE has a profile directory to
	// write to; a remote session member's stats stay memory-only.
	if (result == ERROR_SUCCESS) {
		for (uint32_t iLocalUser = 0; iLocalUser < XLIVE_LOCAL_USER_COUNT; iLocalUser++) {
			if (xlive_local_users[iLocalUser].signin_state == eXUserSigninState_SignedInToLive && xlive_local_users[iLocalUser].xuid == user_xuid) {
				XllnStatsSaveUserToDisk(iLocalUser);
				XllnWndStatsInvalidateStats();
				break;
			}
		}
	}

	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5318
uint32_t WINAPI XSessionStart(HANDLE session_handle, uint32_t start_flags, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (start_flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s start_flags (0x%08x) is not 0.", __func__, start_flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				xsessionDetails->eState = XSESSION_STATE_INGAME;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// What is left of a slot allocation. The counts come from a remote host's advertisement, so a
// filled count larger than the maximum has to be tolerated rather than wrapped.
static size_t SlotsOpen(size_t slots_max, size_t slots_filled)
{
	return slots_filled < slots_max ? slots_max - slots_filled : 0;
}

// The value a session advertises for one context. A context the session never set reads as 0, as the
// service does.
static uint32_t LiveSessionContextValue(const LIVE_SESSION* live_session, uint32_t context_id)
{
	for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
		if (live_session->pContexts[iContext].dwContextId == context_id) {
			return live_session->pContexts[iContext].dwValue;
		}
	}
	return 0;
}

// Matches a session against every context a search asked for. Only for a title that declares no
// matchmaking query: equality on whatever was supplied is all that can be assumed without one.
static bool LiveSessionMatchesSearchContexts(
	const LIVE_SESSION* live_session
	, uint16_t search_context_count
	, const XUSER_CONTEXT* search_contexts
)
{
	for (uint16_t iSearchContext = 0; iSearchContext < search_context_count; iSearchContext++) {
		if (LiveSessionContextValue(live_session, search_contexts[iSearchContext].dwContextId) != search_contexts[iSearchContext].dwValue) {
			return false;
		}
	}

	return true;
}

// The value a session advertises for one attribute id, if it set one. Contexts and properties share
// the id space - the top nibble is the XUSER_DATA type and a context is type 0 - so the id alone says
// which of the two lists holds it.
static bool LiveSessionAttributeValue(const LIVE_SESSION* live_session, uint32_t attribute_id, XUSER_DATA* result_value)
{
	if (XPROPERTYTYPEFROMID(attribute_id) == XUSER_DATA_TYPE_CONTEXT) {
		for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
			if (live_session->pContexts[iContext].dwContextId == attribute_id) {
				memset(result_value, 0, sizeof(*result_value));
				result_value->type = XUSER_DATA_TYPE_CONTEXT;
				result_value->nData = (LONG)live_session->pContexts[iContext].dwValue;
				return true;
			}
		}
		return false;
	}
	for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
		if (live_session->pProperties[iProperty].dwPropertyId == attribute_id) {
			*result_value = live_session->pProperties[iProperty].value;
			return true;
		}
	}
	return false;
}

// The value a search supplies for one attribute id, from whichever of its two arrays that id belongs to.
static bool SearchAttributeValue(
	uint32_t attribute_id
	, uint16_t search_property_count
	, const XUSER_PROPERTY* search_properties
	, uint16_t search_context_count
	, const XUSER_CONTEXT* search_contexts
	, XUSER_DATA* result_value
)
{
	if (XPROPERTYTYPEFROMID(attribute_id) == XUSER_DATA_TYPE_CONTEXT) {
		for (uint16_t iContext = 0; iContext < search_context_count; iContext++) {
			if (search_contexts[iContext].dwContextId == attribute_id) {
				memset(result_value, 0, sizeof(*result_value));
				result_value->type = XUSER_DATA_TYPE_CONTEXT;
				result_value->nData = (LONG)search_contexts[iContext].dwValue;
				return true;
			}
		}
		return false;
	}
	for (uint16_t iProperty = 0; iProperty < search_property_count; iProperty++) {
		if (search_properties[iProperty].dwPropertyId == attribute_id) {
			*result_value = search_properties[iProperty].value;
			return true;
		}
	}
	return false;
}

// The numeric types share one scale, so a filter can order any two of them against each other. A
// string or a blob has no place on that scale and only supports equality.
static bool XllnXuserDataAsDouble(const XUSER_DATA &value, double* result_value)
{
	switch (value.type) {
		case XUSER_DATA_TYPE_CONTEXT: { *result_value = (double)(uint32_t)value.nData; return true; }
		case XUSER_DATA_TYPE_INT32: { *result_value = (double)value.nData; return true; }
		case XUSER_DATA_TYPE_INT64: { *result_value = (double)value.i64Data; return true; }
		case XUSER_DATA_TYPE_DOUBLE: { *result_value = value.dblData; return true; }
		case XUSER_DATA_TYPE_FLOAT: { *result_value = (double)value.fData; return true; }
		case XUSER_DATA_TYPE_DATETIME: {
			*result_value = (double)(((uint64_t)value.ftData.dwHighDateTime << 32) | value.ftData.dwLowDateTime);
			return true;
		}
	}
	return false;
}

// Applies one of a query's filters to one advertised session, resolving the right side from the
// search's own parameters, the XLAST's constants, or the value written into the query.
//
// Passes whenever there is nothing to compare - the search supplied no value for the parameter, the
// session never advertised the attribute, or the two sides have no order. A search supplies a value
// only for the rules it wants applied, so failing the rest would leave every query filtering on
// everything it declares.
static bool LiveSessionPassesQueryFilter(
	const LIVE_SESSION* live_session
	, const XLLN_XLAST_QUERY_FILTER &filter
	, uint16_t search_property_count
	, const XUSER_PROPERTY* search_properties
	, uint16_t search_context_count
	, const XUSER_CONTEXT* search_contexts
)
{
	XUSER_DATA sessionValue;
	if (!LiveSessionAttributeValue(live_session, filter.attributeId, &sessionValue)) {
		return true;
	}
	
	XUSER_DATA wantedValue;
	switch (filter.operandType) {
		case XLLN_XLAST_OPERAND_CONSTANT: {
			double constantValue = 0;
			if (!XllnXlastConstantValue(filter.operand, &constantValue)) {
				return true;
			}
			memset(&wantedValue, 0, sizeof(wantedValue));
			wantedValue.type = XUSER_DATA_TYPE_DOUBLE;
			wantedValue.dblData = constantValue;
			break;
		}
		case XLLN_XLAST_OPERAND_CONTEXT_VALUE: {
			memset(&wantedValue, 0, sizeof(wantedValue));
			wantedValue.type = XUSER_DATA_TYPE_CONTEXT;
			wantedValue.nData = (LONG)filter.operand;
			break;
		}
		default: {
			if (!SearchAttributeValue(filter.operand, search_property_count, search_properties, search_context_count, search_contexts, &wantedValue)) {
				return true;
			}
			break;
		}
	}
	
	double sessionNumber = 0;
	double wantedNumber = 0;
	if (!XllnXuserDataAsDouble(sessionValue, &sessionNumber) || !XllnXuserDataAsDouble(wantedValue, &wantedNumber)) {
		if (filter.comparison != XLLN_XLAST_COMPARISON_EQUAL && filter.comparison != XLLN_XLAST_COMPARISON_NOT_EQUAL) {
			return true;
		}
		if (sessionValue.type != wantedValue.type) {
			return true;
		}
		bool same;
		if (sessionValue.type == XUSER_DATA_TYPE_UNICODE) {
			same = sessionValue.string.cbData == wantedValue.string.cbData
				&& sessionValue.string.pwszData
				&& wantedValue.string.pwszData
				&& memcmp(sessionValue.string.pwszData, wantedValue.string.pwszData, sessionValue.string.cbData) == 0;
		}
		else if (sessionValue.type == XUSER_DATA_TYPE_BINARY) {
			same = sessionValue.binary.cbData == wantedValue.binary.cbData
				&& sessionValue.binary.pbData
				&& wantedValue.binary.pbData
				&& memcmp(sessionValue.binary.pbData, wantedValue.binary.pbData, sessionValue.binary.cbData) == 0;
		}
		else {
			return true;
		}
		return same == (filter.comparison == XLLN_XLAST_COMPARISON_EQUAL);
	}
	
	switch (filter.comparison) {
		case XLLN_XLAST_COMPARISON_NOT_EQUAL: { return sessionNumber != wantedNumber; }
		case XLLN_XLAST_COMPARISON_LESS: { return sessionNumber < wantedNumber; }
		case XLLN_XLAST_COMPARISON_LESS_EQUAL: { return sessionNumber <= wantedNumber; }
		case XLLN_XLAST_COMPARISON_GREATER: { return sessionNumber > wantedNumber; }
		case XLLN_XLAST_COMPARISON_GREATER_EQUAL: { return sessionNumber >= wantedNumber; }
	}
	return sessionNumber == wantedNumber;
}

// Decides whether one advertised session is a result. The query the procedure index selects owns
// this: its filters name which advertised attributes matter and how each is compared, and the
// operator is often not equality.
//
// Game type and game mode are matched whether or not the query lists them, as the service applies
// those two regardless. Without a query there is nothing declared to honour, so every supplied
// context is matched for equality instead.
static bool LiveSessionMatchesSearch(
	const LIVE_SESSION* live_session
	, const XLLN_XLAST_QUERY* query
	, uint16_t search_property_count
	, const XUSER_PROPERTY* search_properties
	, uint16_t search_context_count
	, const XUSER_CONTEXT* search_contexts
)
{
	if (!query) {
		return LiveSessionMatchesSearchContexts(live_session, search_context_count, search_contexts);
	}
	
	for (uint16_t iSearchContext = 0; iSearchContext < search_context_count; iSearchContext++) {
		const uint32_t contextId = search_contexts[iSearchContext].dwContextId;
		if (contextId != X_CONTEXT_GAME_TYPE && contextId != X_CONTEXT_GAME_MODE) {
			continue;
		}
		if (LiveSessionContextValue(live_session, contextId) != search_contexts[iSearchContext].dwValue) {
			return false;
		}
	}
	
	for (const XLLN_XLAST_QUERY_FILTER &filter : query->filters) {
		if (!LiveSessionPassesQueryFilter(live_session, filter, search_property_count, search_properties, search_context_count, search_contexts)) {
			return false;
		}
	}
	
	return true;
}

// The XLAST query a search's procedure index selects. Null when the title ships no usable SPA, or
// names a query it never declared.
static const XLLN_XLAST_QUERY* XSessionSearchQuery(uint32_t procedure_index)
{
	auto itrQuery = xlive_xlast_queries.find(procedure_index);
	return itrQuery == xlive_xlast_queries.end() ? 0 : &itrQuery->second;
}

// The attribute list a query declares it returns. Null when it declares none and there is no
// declaration to honour.
static const std::vector<uint32_t>* XSessionSearchQueryReturns(const XLLN_XLAST_QUERY* query)
{
	return query && !query->returns.empty() ? &query->returns : 0;
}

// The service supplies these rather than the host, and returns them on every result whatever the
// query declares.
static bool IsSearchAlwaysReturnedProperty(uint32_t property_id)
{
	return property_id == X_PROPERTY_GAMER_HOSTNAME;
}

// Resolve what a search returns for one session: the query's declared attributes, in the order it
// declares them, or everything the session advertises when the title declares no query. Both the
// sizing and the fill pass resolve through here so they cannot disagree about the buffer.
static void LiveSessionResolveSearchAttributes(
	const LIVE_SESSION* live_session
	, const std::vector<uint32_t>* query_returns
	, bool report_missing
	, std::vector<const XUSER_CONTEXT*> &result_contexts
	, std::vector<const XUSER_PROPERTY*> &result_properties
)
{
	result_contexts.clear();
	result_properties.clear();

	if (!query_returns) {
		for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
			result_contexts.push_back(&live_session->pContexts[iContext]);
		}
		for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
			result_properties.push_back(&live_session->pProperties[iProperty]);
		}
		return;
	}

	for (const uint32_t attributeId : *query_returns) {
		bool resolved = false;

		if (XPROPERTYTYPEFROMID(attributeId) == XUSER_DATA_TYPE_CONTEXT) {
			for (uint32_t iContext = 0; !resolved && iContext < live_session->contextsCount; iContext++) {
				if (live_session->pContexts[iContext].dwContextId == attributeId) {
					result_contexts.push_back(&live_session->pContexts[iContext]);
					resolved = true;
				}
			}
		}
		else {
			for (uint32_t iProperty = 0; !resolved && iProperty < live_session->propertiesCount; iProperty++) {
				if (live_session->pProperties[iProperty].dwPropertyId == attributeId) {
					result_properties.push_back(&live_session->pProperties[iProperty]);
					resolved = true;
				}
			}
		}

		// Omitted rather than defaulted: a fabricated value cannot be told from one the host set.
		if (!resolved && report_missing) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
				, "%s query declares attribute (0x%08x) that session (0x%08x) never set; omitted from the result."
				, __func__
				, attributeId
				, live_session->instanceId
			);
		}
	}

	for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
		const XUSER_PROPERTY* property = &live_session->pProperties[iProperty];
		if (!IsSearchAlwaysReturnedProperty(property->dwPropertyId)) {
			continue;
		}
		if (std::find(result_properties.begin(), result_properties.end(), property) != result_properties.end()) {
			continue;
		}
		result_properties.push_back(property);
	}

	// The counterpart to the omissions above: what the declaration excluded.
	if (report_missing) {
		for (uint32_t iContext = 0; iContext < live_session->contextsCount; iContext++) {
			const XUSER_CONTEXT* context = &live_session->pContexts[iContext];
			if (std::find(result_contexts.begin(), result_contexts.end(), context) == result_contexts.end()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s session (0x%08x) advertises context (0x%08x) value (0x%08x) that the query does not declare; dropped from the result."
					, __func__
					, live_session->instanceId
					, context->dwContextId
					, context->dwValue
				);
			}
		}
		for (uint32_t iProperty = 0; iProperty < live_session->propertiesCount; iProperty++) {
			const XUSER_PROPERTY* property = &live_session->pProperties[iProperty];
			if (std::find(result_properties.begin(), result_properties.end(), property) == result_properties.end()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s session (0x%08x) advertises property (0x%08x) that the query does not declare; dropped from the result."
					, __func__
					, live_session->instanceId
					, property->dwPropertyId
				);
			}
		}
	}
}

uint32_t XSessionSearchHelper(
	uint32_t procedure_index
	, uint32_t user_index
	, size_t result_count
	, uint32_t local_user_count
	, uint16_t search_property_count
	, uint16_t search_context_count
	, XUSER_PROPERTY* search_properties
	, XUSER_CONTEXT* search_contexts
	, XNKID* search_session_id
	, size_t* result_buffer_size
	, XSESSION_SEARCHRESULT_HEADER* result_search_sessions
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!result_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!local_user_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (search_property_count) {
		if (!search_properties) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s search_properties is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (search_property_count > 0x40) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (search_property_count > 0x40) (0x%hx > 0x40).", __func__, search_property_count);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (search_properties) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s search_properties is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (search_context_count) {
		if (!search_contexts) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s search_contexts is NULL.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else if (search_contexts) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s search_contexts is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (result_search_sessions && *result_buffer_size >= sizeof(XSESSION_SEARCHRESULT_HEADER)) {
		result_search_sessions->dwSearchResults = 0;
		result_search_sessions->pResults = 0;
	}
	
	{
		EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
		
		// Calculate the space required.
		const XLLN_XLAST_QUERY* query = XSessionSearchQuery(procedure_index);
		const std::vector<uint32_t>* queryReturns = XSessionSearchQueryReturns(query);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s procedure_index (%u) selects an XLAST query declaring (%zu) filters and (%zu) returns; result_count (%zu), search contexts (%hu), search properties (%hu), buffer (%s)."
			, __func__
			, procedure_index
			, query ? query->filters.size() : (size_t)0
			, queryReturns ? queryReturns->size() : (size_t)0
			, result_count
			, search_context_count
			, search_property_count
			, result_search_sessions ? "fill" : "size query"
		);
		std::vector<const XUSER_CONTEXT*> resultContexts;
		std::vector<const XUSER_PROPERTY*> resultProperties;
		size_t bufferSizeRequired = sizeof(XSESSION_SEARCHRESULT_HEADER);
		size_t searchResultCount = 0;
		for (auto const &session : liveoverlan_remote_sessions_xsession) {

			if (searchResultCount >= result_count) {
				break;
			}

			// Ensure this is an XSession item.
			if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION) {
				continue;
			}

			// Our own advertisement loops back to us; a title must not find its own lobby.
			if (session.second->liveSession->instanceId == xlln_global_instance_id) {
				continue;
			}

			// A party session - presence without matchmaking - is reachable by session id, never by search.
			if (!search_session_id
				&& (session.second->liveSession->sessionFlags & XSESSION_CREATE_USES_PRESENCE)
				&& !(session.second->liveSession->sessionFlags & XSESSION_CREATE_USES_MATCHMAKING)) {
				continue;
			}
			
			// Only a session that can seat the searching players is a result; private slots are reserved
			// for invited players, so an invite-only session has no public room.
			if (!search_session_id
				&& SlotsOpen(session.second->liveSession->slotsPublicMaxCount, session.second->liveSession->slotsPublicFilledCount) < local_user_count) {
				continue;
			}
			
			if (search_session_id && memcmp(&session.second->liveSession->xnkid, search_session_id, sizeof(XNKID)) != 0) {
				continue;
			}

			// A search by session id names one session and is how an invite or a gamercard join reaches
			// it; the query's filters describe a browse and must not narrow that.
			if (!search_session_id
				&& !LiveSessionMatchesSearch(session.second->liveSession, query, search_property_count, search_properties, search_context_count, search_contexts)) {
				continue;
			}

			LiveSessionResolveSearchAttributes(session.second->liveSession, queryReturns, false, resultContexts, resultProperties);

			searchResultCount++;
			bufferSizeRequired += sizeof(XSESSION_SEARCHRESULT);
			bufferSizeRequired += resultContexts.size() * sizeof(*session.second->liveSession->pContexts);
			for (const XUSER_PROPERTY* property : resultProperties) {
				bufferSizeRequired += sizeof(*session.second->liveSession->pProperties);
				switch (property->value.type) {
					case XUSER_DATA_TYPE_BINARY: {
						bufferSizeRequired += property->value.binary.cbData;
						break;
					}
					case XUSER_DATA_TYPE_UNICODE: {
						bufferSizeRequired += property->value.string.cbData;
						break;
					}
				}
			}
		}
		
		if (*result_buffer_size < bufferSizeRequired || !result_search_sessions) {
			*result_buffer_size = bufferSizeRequired;
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
			return ERROR_INSUFFICIENT_BUFFER;
		}
		
		if (searchResultCount > 0) {
			result_search_sessions->pResults = (XSESSION_SEARCHRESULT*)&((uint8_t*)result_search_sessions)[sizeof(XSESSION_SEARCHRESULT_HEADER)];
			uint8_t* searchResultsData = &((uint8_t*)result_search_sessions->pResults)[searchResultCount * sizeof(*result_search_sessions->pResults)];
			
			size_t iSearchResult = 0;
			for (auto const &session : liveoverlan_remote_sessions_xsession) {
				// The buffer holds exactly the number of entries the sizing loop above settled on.
				if (iSearchResult >= searchResultCount) {
					break;
				}
				
				// Ensure this is an XSession item.
				if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XSESSION) {
					continue;
				}

				if (session.second->liveSession->instanceId == xlln_global_instance_id) {
					continue;
				}

				// A party session - presence without matchmaking - is reachable by session id, never by search.
				if (!search_session_id
					&& (session.second->liveSession->sessionFlags & XSESSION_CREATE_USES_PRESENCE)
					&& !(session.second->liveSession->sessionFlags & XSESSION_CREATE_USES_MATCHMAKING)) {
					continue;
				}
				
				// Only a session that can seat the searching players is a result; private slots are reserved
				// for invited players, so an invite-only session has no public room.
				if (!search_session_id
					&& SlotsOpen(session.second->liveSession->slotsPublicMaxCount, session.second->liveSession->slotsPublicFilledCount) < local_user_count) {
					continue;
				}
				
				if (search_session_id && memcmp(&session.second->liveSession->xnkid, search_session_id, sizeof(XNKID)) != 0) {
					continue;
				}

				if (!search_session_id
					&& !LiveSessionMatchesSearch(session.second->liveSession, query, search_property_count, search_properties, search_context_count, search_contexts)) {
					continue;
				}

				{
					EnterCriticalSection(&xlive_critsec_remote_user);
					
					// Make an entry if one didn't already exist for this user.
					xlln_remote_user_usernames[session.second->liveSession->xuid];
					PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE, 0, 0);
					
					LeaveCriticalSection(&xlive_critsec_remote_user);
				}
				
				// Copy over all the memory into the buffer.
				XSESSION_SEARCHRESULT &searchResult = result_search_sessions->pResults[iSearchResult++];
				searchResult.info.sessionID = session.second->liveSession->xnkid;
				searchResult.info.hostAddress = session.second->xnAddr;
				searchResult.info.keyExchangeKey = session.second->liveSession->xnkey;
				searchResult.dwOpenPublicSlots = SlotsOpen(session.second->liveSession->slotsPublicMaxCount, session.second->liveSession->slotsPublicFilledCount);
				searchResult.dwOpenPrivateSlots = SlotsOpen(session.second->liveSession->slotsPrivateMaxCount, session.second->liveSession->slotsPrivateFilledCount);
				searchResult.dwFilledPublicSlots = session.second->liveSession->slotsPublicFilledCount;
				searchResult.dwFilledPrivateSlots = session.second->liveSession->slotsPrivateFilledCount;
				
				LiveSessionResolveSearchAttributes(session.second->liveSession, queryReturns, true, resultContexts, resultProperties);

				// The counts a session advertises against the counts it is served: the only signal that
				// the query dropped attributes the host actually set.
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
					, "%s session (0x%08x) advertises (%u) contexts and (%u) properties; returning (%zu) contexts and (%zu) properties."
					, __func__
					, session.second->liveSession->instanceId
					, session.second->liveSession->contextsCount
					, session.second->liveSession->propertiesCount
					, resultContexts.size()
					, resultProperties.size()
				);

				searchResult.cContexts = (DWORD)resultContexts.size();
				if (searchResult.cContexts == 0) {
					searchResult.pContexts = 0;
				}
				else {
					searchResult.pContexts = (XUSER_CONTEXT*)searchResultsData;
					searchResultsData += searchResult.cContexts * sizeof(*searchResult.pContexts);
					for (uint32_t iContext = 0; iContext < searchResult.cContexts; iContext++) {
						searchResult.pContexts[iContext] = *resultContexts[iContext];
					}
				}

				searchResult.cProperties = (DWORD)resultProperties.size();
				if (searchResult.cProperties == 0) {
					searchResult.pProperties = 0;
				}
				else {
					searchResult.pProperties = (XUSER_PROPERTY*)searchResultsData;
					searchResultsData += searchResult.cProperties * sizeof(*searchResult.pProperties);

					for (uint32_t iProperty = 0; iProperty < searchResult.cProperties; iProperty++) {
						const XUSER_PROPERTY &propertyOrig = *resultProperties[iProperty];
						XUSER_PROPERTY &propertyCopy = searchResult.pProperties[iProperty];
						propertyCopy = propertyOrig;
						switch (propertyCopy.value.type) {
							case XUSER_DATA_TYPE_BINARY: {
								propertyCopy.value.binary.pbData = searchResultsData;
								memcpy(propertyCopy.value.binary.pbData, propertyOrig.value.binary.pbData, propertyCopy.value.binary.cbData);
								searchResultsData += propertyCopy.value.binary.cbData;
								break;
							}
							case XUSER_DATA_TYPE_UNICODE: {
								propertyCopy.value.string.pwszData = (wchar_t*)searchResultsData;
								memcpy(propertyCopy.value.string.pwszData, propertyOrig.value.string.pwszData, propertyCopy.value.string.cbData);
								searchResultsData += propertyCopy.value.string.cbData;
								break;
							}
						}
					}
				}
			}
			
			if (searchResultsData != &((uint8_t*)result_search_sessions)[bufferSizeRequired]) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
					, "%s the end result of searchResultData (0x%zx) should not be different from result_search_sessions (0x%zx) plus bufferSizeRequired (0x%zx) giving 0x%zx."
					, __func__
					, searchResultsData
					, (uint8_t*)result_search_sessions
					, bufferSizeRequired
					, &((uint8_t*)result_search_sessions)[bufferSizeRequired]
				);
				__debugbreak();
				LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
				return ERROR_FATAL_APP_EXIT;
			}
			
			result_search_sessions->dwSearchResults = searchResultCount;
		}
		
		LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return ERROR_SUCCESS;
}

// #5319
uint32_t WINAPI XSessionSearchEx(
	uint32_t procedure_index
	, uint32_t user_index
	, size_t result_count
	, uint32_t local_user_count
	, uint16_t search_property_count
	, uint16_t search_context_count
	, XUSER_PROPERTY* search_properties
	, XUSER_CONTEXT* search_contexts
	, size_t* result_buffer_size
	, XSESSION_SEARCHRESULT_HEADER* result_search_sessions
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	
	uint32_t result = XSessionSearchHelper(
		procedure_index
		, user_index
		, result_count
		, local_user_count
		, search_property_count
		, search_context_count
		, search_properties
		, search_contexts
		, 0
		, result_buffer_size
		, result_search_sessions
		, xoverlapped
	);
	
	return result;
}

// #5320
uint32_t WINAPI XSessionSearchByID(XNKID session_id, uint32_t user_index, size_t* result_buffer_size, XSESSION_SEARCHRESULT_HEADER* result_search_sessions, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	
	uint32_t result = XSessionSearchHelper(
		(uint32_t)-1
		, user_index
		, (size_t)-1
		, 1
		, 0
		, 0
		, 0
		, 0
		, &session_id
		, result_buffer_size
		, result_search_sessions
		, xoverlapped
	);
	
	return result;
}

// #5321
uint32_t WINAPI XSessionSearch(
	uint32_t procedure_index
	, uint32_t user_index
	, size_t result_count
	, uint16_t search_property_count
	, uint16_t search_context_count
	, XUSER_PROPERTY* search_properties
	, XUSER_CONTEXT* search_contexts
	, size_t* result_buffer_size
	, XSESSION_SEARCHRESULT_HEADER* result_search_sessions
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	
	uint32_t result = XSessionSearchHelper(
		procedure_index
		, user_index
		, result_count
		, 1
		, search_property_count
		, search_context_count
		, search_properties
		, search_contexts
		, 0
		, result_buffer_size
		, result_search_sessions
		, xoverlapped
	);
	
	return result;
}

// #5322
uint32_t WINAPI XSessionModify(HANDLE session_handle, uint32_t modify_flags, size_t max_public_slots, size_t max_private_slots, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if ((modify_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED) && (modify_flags & XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s modify_flags cannot set XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED and XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				const uint32_t maskModifiers = XSESSION_CREATE_MODIFIERS_MASK - XSESSION_CREATE_SOCIAL_MATCHMAKING_ALLOWED; // not sure if social property is modifiable.
				xsessionDetails->liveSession->sessionFlags = (xsessionDetails->liveSession->sessionFlags & (~maskModifiers)) | (maskModifiers & modify_flags);
				
				if (max_public_slots < xsessionDetails->liveSession->slotsPublicFilledCount) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s XSession (0x%zx) max_public_slots (%zu) < slotsPublicFilledCount (%zu)."
						, __func__
						, session_handle
						, max_public_slots
						, xsessionDetails->liveSession->slotsPublicFilledCount
					);
				}
				if (max_private_slots < xsessionDetails->liveSession->slotsPrivateFilledCount) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
						, "%s XSession (0x%zx) max_private_slots (%zu) < slotsPrivateFilledCount (%zu)."
						, __func__
						, session_handle
						, max_private_slots
						, xsessionDetails->liveSession->slotsPrivateFilledCount
					);
				}
				
				xsessionDetails->liveSession->slotsPublicMaxCount = max_public_slots;
				xsessionDetails->liveSession->slotsPrivateMaxCount = max_private_slots;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5323
uint32_t WINAPI XSessionMigrateHost(HANDLE session_handle, uint32_t user_index, XSESSION_INFO* session_info, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_index != XUSER_INDEX_NONE && user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (user_index != XUSER_INDEX_NONE && xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!session_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				if (user_index != XUSER_INDEX_NONE) {
					// New host of session.
					
					xsessionDetails->liveSession->xuid = xlive_local_users[user_index].xuid;
					//xsessionDetails->qwNonce = xlive_local_users[user_index].xuid;
					
					// LiveOverLan only advertises sessions carrying the host flag, and identifies them by instanceId.
					xsessionDetails->liveSession->instanceId = xlln_global_instance_id;
					xsessionDetails->liveSession->sessionFlags |= XSESSION_CREATE_HOST;

					// Fresh unique session key for the migrated host; the address still derives from the instanceId.
					XllnNetEntityGetXnaddrXnkidByInstanceId(xlln_global_instance_id, &xsessionDetails->xnAddr, 0);
					XllnGenerateSessionXnkid(&xsessionDetails->liveSession->xnkid);
					
					// TODO XNKEY
					memset(&xsessionDetails->liveSession->xnkey, 0xAA, sizeof(XNKEY));
					
					session_info->hostAddress = xsessionDetails->xnAddr;
					session_info->keyExchangeKey = xsessionDetails->liveSession->xnkey;
					session_info->sessionID = xsessionDetails->liveSession->xnkid;
				}
				else {
					// Non-host of session.
					// Conversely a host that hands the session on has to stop advertising it.
					xsessionDetails->liveSession->sessionFlags &= ~XSESSION_CREATE_HOST;
					xsessionDetails->xnAddr = session_info->hostAddress;
					xsessionDetails->liveSession->xnkey = session_info->keyExchangeKey;
					xsessionDetails->liveSession->xnkid = session_info->sessionID;
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5325
uint32_t WINAPI XSessionLeaveLocal(HANDLE session_handle, uint32_t local_user_index_count, const uint32_t* local_user_indexes, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (local_user_index_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_index_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (local_user_index_count > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_index_count (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, local_user_index_count, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!local_user_indexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_indexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				for (uint32_t iUser = 0; iUser < local_user_index_count; iUser++) {
					const uint32_t &localUserIndex = local_user_indexes[iUser];
					const XUID &localUserXuid = xlive_local_users[localUserIndex].xuid;
					
					const auto &itrSessionMember = xsessionDetails->sessionMembers.find(localUserXuid);
					if (itrSessionMember == xsessionDetails->sessionMembers.end()) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s XSession (0x%zx) member index (%u) XUID (0x%016I64x) not found and could not be removed."
							, __func__
							, session_handle
							, localUserIndex
							, localUserXuid
						);
						// not found. should error?
					}
					else {
						XSESSION_MEMBER* sessionMember = itrSessionMember->second;
						if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->liveSession->slotsPrivateFilledCount--;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount--;
						}
						
						if (xsessionDetails->liveSession->sessionFlags & XSESSION_CREATE_USES_ARBITRATION) {
							sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_ZOMBIE;
						}
						else {
							xsessionDetails->sessionMembers.erase(itrSessionMember);
							delete sessionMember;
							sessionMember = 0;
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5326
uint32_t WINAPI XSessionJoinRemote(HANDLE session_handle, size_t user_count, const XUID* user_xuids, const BOOL* user_private_slots, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_private_slots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_private_slots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				uint32_t requestedSlotCountPublic = 0;
				uint32_t requestedSlotCountPrivate = 0;
				for (uint32_t iUser = 0; iUser < user_count; iUser++) {
					if (user_private_slots[iUser]) {
						requestedSlotCountPrivate++;
					}
					else {
						requestedSlotCountPublic++;
					}
				}
				
				if (requestedSlotCountPrivate > xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount) {
					result = XONLINE_E_SESSION_FULL;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XSession (0x%zx) Not enough private slots are available. (need > available) (%u > %u - %u)"
						, __func__
						, session_handle
						, requestedSlotCountPrivate
						, xsessionDetails->liveSession->slotsPrivateMaxCount
						, xsessionDetails->liveSession->slotsPrivateFilledCount
					);
				}
				else if (requestedSlotCountPublic > xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount) {
					result = XONLINE_E_SESSION_FULL;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XSession (0x%zx) Not enough public slots are available. (need > available) (%u > %u - %u)"
						, __func__
						, session_handle
						, requestedSlotCountPublic
						, xsessionDetails->liveSession->slotsPublicMaxCount
						, xsessionDetails->liveSession->slotsPublicFilledCount
					);
				}
				else {
					result = ERROR_SUCCESS;
					
					{
						EnterCriticalSection(&xlive_critsec_remote_user);
						
						for (size_t iXuid = 0; iXuid < user_count; iXuid++) {
							const XUID &remoteXuid = user_xuids[iXuid];
							
							// Make an entry if one didn't already exist for this user.
							xlln_remote_user_usernames[remoteXuid];
							PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE, 0, 0);
						}
						
						LeaveCriticalSection(&xlive_critsec_remote_user);
					}
					
					for (uint32_t iUser = 0; iUser < user_count; iUser++) {
						const XUID &userXuid = user_xuids[iUser];
						const bool takePrivateSlot = !!user_private_slots[iUser];
						
						const auto &itrSessionMember = xsessionDetails->sessionMembers.find(userXuid);
						if (itrSessionMember != xsessionDetails->sessionMembers.end()) {
							XSESSION_MEMBER* sessionMember = itrSessionMember->second;
							if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
									, "%s XSession (0x%zx) member XUID (0x%016I64x) was previously in this session."
									, __func__
									, session_handle
									, userXuid
								);
							}
							else {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s XSession (0x%zx) member XUID (0x%016I64x) is already in this session."
									, __func__
									, session_handle
									, userXuid
								);
								if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
									xsessionDetails->liveSession->slotsPrivateFilledCount--;
								}
								else {
									xsessionDetails->liveSession->slotsPublicFilledCount--;
								}
							}
							
							sessionMember->dwUserIndex = XLIVE_LOCAL_USER_INVALID;
							sessionMember->dwFlags = (sessionMember->dwFlags & ~((uint32_t)(XSESSION_MEMBER_FLAGS_ZOMBIE | XSESSION_MEMBER_FLAGS_PRIVATE_SLOT)));
							if (takePrivateSlot) {
								sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
								xsessionDetails->liveSession->slotsPrivateFilledCount++;
							}
							else {
								xsessionDetails->liveSession->slotsPublicFilledCount++;
							}
						}
						else {
							XSESSION_MEMBER* sessionMember = new XSESSION_MEMBER;
							sessionMember->xuidOnline = userXuid;
							sessionMember->dwUserIndex = XLIVE_LOCAL_USER_INVALID;
							sessionMember->dwFlags = 0;
							if (takePrivateSlot) {
								sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
								xsessionDetails->liveSession->slotsPrivateFilledCount++;
							}
							else {
								xsessionDetails->liveSession->slotsPublicFilledCount++;
							}
							
							xsessionDetails->sessionMembers[userXuid] = sessionMember;
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5327
uint32_t WINAPI XSessionJoinLocal(HANDLE session_handle, uint32_t local_user_count, const uint32_t* local_user_indexes, const BOOL* user_private_slots, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (local_user_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (local_user_count > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_count (%u) is greater than XLIVE_LOCAL_USER_COUNT (%u).", __func__, local_user_count, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!local_user_indexes) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s local_user_indexes is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_private_slots) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_private_slots is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				uint32_t requestedSlotCountPublic = 0;
				uint32_t requestedSlotCountPrivate = 0;
				for (uint32_t iUser = 0; iUser < local_user_count; iUser++) {
					if (user_private_slots[iUser]) {
						requestedSlotCountPrivate++;
					}
					else {
						requestedSlotCountPublic++;
					}
				}
				
				if (requestedSlotCountPrivate > xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount) {
					result = XONLINE_E_SESSION_FULL;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XSession (0x%zx) Not enough private slots are available. (need > available) (%u > %u - %u)"
						, __func__
						, session_handle
						, requestedSlotCountPrivate
						, xsessionDetails->liveSession->slotsPrivateMaxCount
						, xsessionDetails->liveSession->slotsPrivateFilledCount
					);
				}
				else if (requestedSlotCountPublic > xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount) {
					result = XONLINE_E_SESSION_FULL;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XSession (0x%zx) Not enough public slots are available. (need > available) (%u > %u - %u)"
						, __func__
						, session_handle
						, requestedSlotCountPublic
						, xsessionDetails->liveSession->slotsPublicMaxCount
						, xsessionDetails->liveSession->slotsPublicFilledCount
					);
				}
				else {
					result = ERROR_SUCCESS;
					
					for (uint32_t iUser = 0; iUser < local_user_count; iUser++) {
						const uint32_t &localUserIndex = local_user_indexes[iUser];
						const bool takePrivateSlot = !!user_private_slots[iUser];
						const XUID &localUserXuid = xlive_local_users[localUserIndex].xuid;
						
						const auto &itrSessionMember = xsessionDetails->sessionMembers.find(localUserXuid);
						if (itrSessionMember != xsessionDetails->sessionMembers.end()) {
							XSESSION_MEMBER* sessionMember = itrSessionMember->second;
							if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
									, "%s XSession (0x%zx) member index (%u) XUID (0x%016I64x) was previously in this session."
									, __func__
									, session_handle
									, localUserIndex
									, localUserXuid
								);
							}
							else {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
									, "%s XSession (0x%zx) member index (%u) XUID (0x%016I64x) is already in this session."
									, __func__
									, session_handle
									, localUserIndex
									, localUserXuid
								);
								if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
									xsessionDetails->liveSession->slotsPrivateFilledCount--;
								}
								else {
									xsessionDetails->liveSession->slotsPublicFilledCount--;
								}
							}
							
							sessionMember->dwUserIndex = localUserIndex;
							sessionMember->dwFlags = (sessionMember->dwFlags & ~((uint32_t)(XSESSION_MEMBER_FLAGS_ZOMBIE | XSESSION_MEMBER_FLAGS_PRIVATE_SLOT)));
							if (takePrivateSlot) {
								sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
								xsessionDetails->liveSession->slotsPrivateFilledCount++;
							}
							else {
								xsessionDetails->liveSession->slotsPublicFilledCount++;
							}
						}
						else {
							XSESSION_MEMBER* sessionMember = new XSESSION_MEMBER;
							sessionMember->xuidOnline = localUserXuid;
							sessionMember->dwUserIndex = localUserIndex;
							sessionMember->dwFlags = 0;
							if (takePrivateSlot) {
								sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_PRIVATE_SLOT;
								xsessionDetails->liveSession->slotsPrivateFilledCount++;
							}
							else {
								xsessionDetails->liveSession->slotsPublicFilledCount++;
							}
							
							xsessionDetails->sessionMembers[localUserXuid] = sessionMember;
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5328
uint32_t WINAPI XSessionGetDetails(HANDLE session_handle, size_t* result_buffer_size, XSESSION_LOCAL_DETAILS* result_session_details, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*result_buffer_size < sizeof(XSESSION_LOCAL_DETAILS)) {
		*result_buffer_size = sizeof(XSESSION_LOCAL_DETAILS);
		{
			EnterCriticalSection(&xlive_critsec_xsession);
			const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
			if (itrXSessionDetails != xlive_xsession_local_sessions.end()) {
				LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
				
				*result_buffer_size += (xsessionDetails->sessionMembers.size() * sizeof(XSESSION_MEMBER));
			}
			LeaveCriticalSection(&xlive_critsec_xsession);
		}
		return ERROR_INSUFFICIENT_BUFFER;
	}
	if (!result_session_details) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_session_details is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			result = ERROR_SUCCESS;
			
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			result_session_details->dwUserIndexHost = XLIVE_LOCAL_USER_INVALID;
			for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
				if (xlive_local_users[iUser].xuid == xsessionDetails->liveSession->xuid) {
					result_session_details->dwUserIndexHost = iUser;
					break;
				}
			}
			result_session_details->dwGameType = xsessionDetails->gameType;
			result_session_details->dwGameMode = xsessionDetails->gameMode;
			result_session_details->dwFlags = xsessionDetails->liveSession->sessionFlags;
			result_session_details->dwMaxPublicSlots = xsessionDetails->liveSession->slotsPublicMaxCount;
			result_session_details->dwMaxPrivateSlots = xsessionDetails->liveSession->slotsPrivateMaxCount;
			result_session_details->dwAvailablePublicSlots = xsessionDetails->liveSession->slotsPublicMaxCount - xsessionDetails->liveSession->slotsPublicFilledCount;
			result_session_details->dwAvailablePrivateSlots = xsessionDetails->liveSession->slotsPrivateMaxCount - xsessionDetails->liveSession->slotsPrivateFilledCount;
			result_session_details->eState = xsessionDetails->eState;
			result_session_details->qwNonce = xsessionDetails->qwNonce;
			result_session_details->sessionInfo.sessionID = xsessionDetails->liveSession->xnkid;
			result_session_details->sessionInfo.hostAddress = xsessionDetails->xnAddr;
			result_session_details->sessionInfo.keyExchangeKey = xsessionDetails->liveSession->xnkey;
			result_session_details->xnkidArbitration = xsessionDetails->xnkidArbitration;
			result_session_details->dwActualMemberCount = xsessionDetails->sessionMembers.size();
			
			result_session_details->dwReturnedMemberCount = 0;
			result_session_details->pSessionMembers = 0;
			
			size_t returnMemberCountMax = (*result_buffer_size - sizeof(XSESSION_LOCAL_DETAILS)) / sizeof(XSESSION_MEMBER);
			if (result_session_details->dwActualMemberCount > 0 && returnMemberCountMax > 0) {
				result_session_details->pSessionMembers = (XSESSION_MEMBER*)&((uint8_t*)result_session_details)[sizeof(XSESSION_LOCAL_DETAILS)];
				auto itrSessionMember = xsessionDetails->sessionMembers.begin();
				bool processZombies = false;
				while (result_session_details->dwReturnedMemberCount < returnMemberCountMax) {
					if (itrSessionMember == xsessionDetails->sessionMembers.end()) {
						if (!processZombies) {
							processZombies = true;
							itrSessionMember = xsessionDetails->sessionMembers.begin();
							continue;
						}
						break;
					}
					XSESSION_MEMBER* sessionMember = itrSessionMember->second;
					if (!!(sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_ZOMBIE) == processZombies) {
						XSESSION_MEMBER &returnSessionMember = result_session_details->pSessionMembers[result_session_details->dwReturnedMemberCount++];
						returnSessionMember = *sessionMember;
					}
					
					itrSessionMember++;
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5329
uint32_t WINAPI XSessionFlushStats(HANDLE session_handle, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5330
uint32_t WINAPI XSessionDelete(HANDLE session_handle, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	XNKID xnkidDeleted = {};
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			result = ERROR_SUCCESS;
			
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			xsessionDetails->eState = XSESSION_STATE_DELETED;
			xnkidDeleted = xsessionDetails->liveSession->xnkid;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (result == ERROR_SUCCESS) {
		// Called outside the section above: the broadcast takes the LiveOverLan lock before this one.
		LiveOverLanBroadcastLocalXSessionUnadvertise(xnkidDeleted);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5332
uint32_t WINAPI XSessionEnd(HANDLE session_handle, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				xsessionDetails->eState = XSESSION_STATE_REPORTING;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5333
uint32_t WINAPI XSessionArbitrationRegister(HANDLE session_handle, uint32_t arbitration_flags, uint64_t session_nonce, size_t* result_buffer_size, XSESSION_REGISTRATION_RESULTS* result_registration, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (arbitration_flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s arbitration_flags (0x%08x) is not 0.", __func__, arbitration_flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_registration && *result_buffer_size >= sizeof(XSESSION_REGISTRATION_RESULTS)) {
		result_registration->wNumRegistrants = 0;
		result_registration->rgRegistrants = 0;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				size_t requiredBufferSize = sizeof(XSESSION_REGISTRATION_RESULTS) + (xsessionDetails->sessionMembers.size() * (sizeof(XSESSION_REGISTRANT) + sizeof(XUID)));
				
				// a NULL result_registration buffer alongside a valid size pointer is a valid call: 
				// a Title passes no buffer to learn the required size, allocates it, then calls again to receive the registrant list.
				if (!result_registration || *result_buffer_size < requiredBufferSize) {
					*result_buffer_size = requiredBufferSize;
					result = ERROR_INSUFFICIENT_BUFFER;
				}
				else {
					result = ERROR_SUCCESS;
					
					if (xsessionDetails->sessionMembers.size()) {
						result_registration->rgRegistrants = ((XSESSION_REGISTRANT*)&((uint8_t*)result_registration)[sizeof(XSESSION_REGISTRATION_RESULTS)]);
						XUID* xuidArray = ((XUID*)&((uint8_t*)result_registration->rgRegistrants)[xsessionDetails->sessionMembers.size() * sizeof(XSESSION_REGISTRANT)]);
						
						for (const auto &itrSessionMember : xsessionDetails->sessionMembers) {
							XSESSION_MEMBER* sessionMember = itrSessionMember.second;
							XSESSION_REGISTRANT &registrant = result_registration->rgRegistrants[result_registration->wNumRegistrants++];
							// TODO qwMachineID.
							registrant.qwMachineID = (uint64_t)0;
							registrant.bTrustworthiness = TRUE;
							// TODO bNumUsers assuming every user is on a separate machine.
							registrant.bNumUsers = 1;
							registrant.rgUsers = xuidArray;
							registrant.rgUsers[0] = sessionMember->xuidOnline;
							xuidArray = &xuidArray[1];
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5336
uint32_t WINAPI XSessionLeaveRemote(HANDLE session_handle, size_t user_xuid_count, const XUID* user_xuids, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_xuid_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				for (uint32_t iUser = 0; iUser < user_xuid_count; iUser++) {
					const XUID &userXuid = user_xuids[iUser];
					
					const auto &itrSessionMember = xsessionDetails->sessionMembers.find(userXuid);
					if (itrSessionMember == xsessionDetails->sessionMembers.end()) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
							, "%s XSession (0x%zx) member XUID (0x%016I64x) not found and could not be removed."
							, __func__
							, session_handle
							, userXuid
						);
						// not found. should error?
					}
					else {
						XSESSION_MEMBER* sessionMember = itrSessionMember->second;
						if (sessionMember->dwFlags & XSESSION_MEMBER_FLAGS_PRIVATE_SLOT) {
							xsessionDetails->liveSession->slotsPrivateFilledCount--;
						}
						else {
							xsessionDetails->liveSession->slotsPublicFilledCount--;
						}
						
						if (xsessionDetails->liveSession->sessionFlags & XSESSION_CREATE_USES_ARBITRATION) {
							sessionMember->dwFlags |= XSESSION_MEMBER_FLAGS_ZOMBIE;
						}
						else {
							xsessionDetails->sessionMembers.erase(itrSessionMember);
							delete sessionMember;
							sessionMember = 0;
						}
					}
				}
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5342
uint32_t WINAPI XSessionModifySkill(HANDLE session_handle, size_t user_xuid_count, const XUID* user_xuids, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xlive_xsession_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive XSession is not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	if (!session_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s session_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_xuid_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	{
		EnterCriticalSection(&xlive_critsec_xsession);
		const auto &itrXSessionDetails = xlive_xsession_local_sessions.find(session_handle);
		if (itrXSessionDetails == xlive_xsession_local_sessions.end()) {
			result = XONLINE_E_SESSION_NOT_FOUND;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s XSession (0x%zx) does not exist."
				, __func__
				, session_handle
			);
		}
		else {
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessionDetails->second;
			
			if (xsessionDetails->eState == XSESSION_STATE_DELETED) {
				result = XONLINE_E_SESSION_WRONG_STATE;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
					, "%s XSession (0x%zx) wrong state (%u)."
					, __func__
					, session_handle
					, xsessionDetails->eState
				);
			}
			else {
				result = ERROR_SUCCESS;
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

// #5343
uint32_t WINAPI XSessionCalculateSkill(size_t skill_count, double* skill_mus, double* skill_sigmas, double* skill_aggregate_mus, double* skill_aggregate_sigmas)
{
	TRACE_FX();
	if (!skill_mus) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s skill_mus is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!skill_sigmas) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s skill_sigmas is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!skill_aggregate_mus) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s skill_aggregate_mus is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!skill_aggregate_sigmas) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s skill_aggregate_sigmas is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	*skill_aggregate_mus = 0.0;
	*skill_aggregate_sigmas = 0.0;
	if (skill_count) {
		for (size_t i = skill_count; i < skill_count; i++) {
			*skill_aggregate_mus += skill_mus[i];
			*skill_aggregate_sigmas += skill_sigmas[i] * skill_sigmas[i];
		}
		*skill_aggregate_mus /= skill_count;
		*skill_aggregate_sigmas = sqrt(*skill_aggregate_sigmas / skill_count);
	}
	return ERROR_SUCCESS;
}
