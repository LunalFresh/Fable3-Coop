#include <winsock2.h>
#include "xdefs.hpp"
#include "xpresence.hpp"
#include "xlive.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"

// idk real macro name.
#define XPRESENCE_MAX_SUBSCRIPTIONS 400

CRITICAL_SECTION xlive_critsec_presence_enumerators;
static size_t xlive_max_peer_subscriptions = 0;
// Key: enumerator handle (id).
// Value: Set of XUIDs that need to be returned in the enumeration.
std::map<HANDLE, std::set<XUID>> xlive_presence_enumerators;
// FIXME the enumeration does not check if the XUIDs exist in this subscription list (if they are not already friends).
std::set<XUID> xlive_presence_subscriptions;

// #5313
uint32_t WINAPI XPresenceInitialize(size_t peer_subscriptions_max_count)
{
	TRACE_FX();
	if (!peer_subscriptions_max_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_subscriptions_max_count is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (peer_subscriptions_max_count > XPRESENCE_MAX_SUBSCRIPTIONS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (peer_subscriptions_max_count > XPRESENCE_MAX_SUBSCRIPTIONS) (%zu > %u).", __func__, peer_subscriptions_max_count, XPRESENCE_MAX_SUBSCRIPTIONS);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	
	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (xlive_max_peer_subscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s already initialised.", __func__);
		SetLastError(E_ABORT);
		return ERROR_FUNCTION_FAILED;
	}
	xlive_max_peer_subscriptions = peer_subscriptions_max_count;
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	
	return ERROR_SUCCESS;
}

// #5338
uint32_t WINAPI XPresenceSubscribe(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuid_count is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuids is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	
	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (!xlive_max_peer_subscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XOnline not initialised.", __func__);
		SetLastError(XONLINE_E_NOT_INITIALIZED);
		return ERROR_FUNCTION_FAILED;
	}
	for (size_t iXuid = 0; iXuid < peer_xuid_count; iXuid++) {
		xlive_presence_subscriptions.insert(peer_xuids[iXuid]);
		if (xlive_presence_subscriptions.size() > xlive_max_peer_subscriptions) {
			xlive_presence_subscriptions.erase(peer_xuids[iXuid]);
			LeaveCriticalSection(&xlive_critsec_presence_enumerators);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s too many XUIDs were registered.", __func__);
			SetLastError(XONLINE_E_MATCH_TOO_MANY_USERS);
			return ERROR_FUNCTION_FAILED;
		}
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	
	return ERROR_SUCCESS;
}

// #5340
uint32_t WINAPI XPresenceCreateEnumerator(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids, size_t starting_index, size_t result_peer_count, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuid_count is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuids is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	if (!result_peer_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_peer_count is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (result_peer_count > peer_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (result_peer_count > peer_xuid_count) (%zu > %zu).", __func__, result_peer_count, peer_xuid_count);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (starting_index >= peer_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index >= peer_xuid_count) (%zu >= %zu).", __func__, starting_index, peer_xuid_count);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	
	*result_buffer_size = sizeof(XONLINE_PRESENCE) * result_peer_count;
	*enumerator_handle = CreateMutex(NULL, NULL, NULL);
	
	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	auto &xuidSet = xlive_presence_enumerators[*enumerator_handle];
	for (size_t iXuid = starting_index; iXuid < peer_xuid_count; iXuid++) {
		xuidSet.insert(peer_xuids[iXuid]);
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	
	return ERROR_SUCCESS;
}

// #5341
uint32_t WINAPI XPresenceUnsubscribe(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, user_index);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuid_count is 0.", __func__);
		SetLastError(E_INVALIDARG);
		return ERROR_FUNCTION_FAILED;
	}
	if (!peer_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s peer_xuids is NULL.", __func__);
		SetLastError(E_POINTER);
		return ERROR_FUNCTION_FAILED;
	}
	
	EnterCriticalSection(&xlive_critsec_presence_enumerators);
	if (!xlive_max_peer_subscriptions) {
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XOnline not initialised.", __func__);
		SetLastError(XONLINE_E_NOT_INITIALIZED);
		return ERROR_FUNCTION_FAILED;
	}
	for (size_t iXuid = 0; iXuid < peer_xuid_count; iXuid++) {
		xlive_presence_subscriptions.erase(peer_xuids[iXuid]);
	}
	LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	
	return ERROR_SUCCESS;
}
