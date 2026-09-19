#include <winsock2.h>
#include "xdefs.hpp"
#include "./xcustom.hpp"
#include "./xlive.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-user-custom-list.hpp"
#include "../xlln/wnd-user-card.hpp"
#include "../utils/utils.hpp"

CRITICAL_SECTION xlive_critsec_custom_actions;
static bool xlive_custom_dynamic_actions_registered = false;
static uint16_t xlive_custom_dynamic_actions_count = 0;
std::map<uint32_t, CUSTOM_ACTION_INFO*> xlln_custom_actions;

// #472
void WINAPI XCustomSetAction(uint32_t action_index, const wchar_t* action_text, uint32_t flags)
{
	TRACE_FX();
	if (action_index > 3) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (action_index > 3) (%u).", __func__, action_index);
		return;
	}
	if (flags && flags != XCUSTOMACTION_FLAGS::XCUSTOMACTION_FLAG_CLOSES_GUIDE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (flags && flags != XCUSTOMACTION_FLAGS::XCUSTOMACTION_FLAG_CLOSES_GUIDE) (%u).", __func__, flags);
		return;
	}
	{
		EnterCriticalSection(&xlive_critsec_custom_actions);
		if (xlive_custom_dynamic_actions_registered) {
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlive_custom_dynamic_actions_registered.", __func__);
			return;
		}
		
		if (!action_text || action_text[0] == 0) {
			auto itrCustomActionInfo = xlln_custom_actions.find(action_index);
			if (itrCustomActionInfo != xlln_custom_actions.end()) {
				delete itrCustomActionInfo->second;
				xlln_custom_actions.erase(itrCustomActionInfo);
			}
		}
		else {
			CUSTOM_ACTION_INFO* customActionInfo = 0;
			
			auto itrCustomActionInfo = xlln_custom_actions.find(action_index);
			if (itrCustomActionInfo == xlln_custom_actions.end()) {
				customActionInfo = new CUSTOM_ACTION_INFO;
				customActionInfo->action_index = action_index;
				xlln_custom_actions[customActionInfo->action_index] = customActionInfo;
			}
			else {
				customActionInfo = itrCustomActionInfo->second;
			}
			
			if (customActionInfo->action_label) {
				delete[] customActionInfo->action_label;
			}
			customActionInfo->action_label = CloneString(action_text);
			
			customActionInfo->action_closes_menu = (flags == XCUSTOMACTION_FLAGS::XCUSTOMACTION_FLAG_CLOSES_GUIDE);
		}
		
		LeaveCriticalSection(&xlive_critsec_custom_actions);
		
		PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_ACTIONS_UPDATE, 0, 0);
	}
}

// #473
BOOL WINAPI XCustomGetLastActionPress(uint32_t* user_index, uint32_t* action_index, XUID* xuid)
{
	TRACE_FX();
	
	if (user_index) {
		*user_index = 0;
	}
	if (action_index) {
		*action_index = 0;
	}
	if (xuid) {
		*xuid = 0;
	}
	
	EnterCriticalSection(&xlln_critsec_user_card);
	if (xlln_user_card_custom_action_xuid) {
		if (xuid) {
			*xuid = xlln_user_card_custom_action_xuid;
		}
		if (action_index) {
			*action_index = xlln_user_card_custom_action_index;
		}
		if (user_index) {
			*user_index = xlln_user_card_custom_user_index;
		}
		xlln_user_card_custom_action_xuid = 0;
		xlln_user_card_custom_action_index = 0;
		xlln_user_card_custom_user_index = 0;
	}
	LeaveCriticalSection(&xlln_critsec_user_card);
	
	BOOL result = ((xuid && *xuid) ? TRUE : FALSE);
	return result;
}

// #476
uint32_t WINAPI XCustomGetLastActionPressEx(uint32_t* user_index, uint32_t* action_id, XUID* xuid, uint8_t* payload, uint16_t* payload_size)
{
	TRACE_FX();
	if (payload_size) {
		// check that the buffer is big enough (and internally it should not be bigger than 1kb aka 1000).
		if (*payload_size < 0) {
			*payload_size = 0;
			return ERROR_INSUFFICIENT_BUFFER;
		}
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	if (user_index) {
		*user_index = 0;
	}
	if (action_id) {
		*action_id = 0;
	}
	// if XUID on the last action (not parameter) is empty then return no data. otherwise return succeess and also clear that action xuid.
	if (xuid) {
		*xuid = 0;
	}
	if (payload_size) {
		*payload_size = 0;
	}
	result = ERROR_NO_DATA;
	
	return result;
}

// #474
uint32_t WINAPI XCustomSetDynamicActions(uint32_t user_index, XUID xuid, const XCUSTOMACTION* custom_actions, uint16_t custom_action_count)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to LIVE.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (custom_action_count > 3) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (custom_action_count > 3) (%hu).", __func__, custom_action_count);
		return ERROR_INVALID_PARAMETER;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_custom_actions);
		if (!xlive_custom_dynamic_actions_registered) {
			LeaveCriticalSection(&xlive_critsec_custom_actions);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s !xlive_custom_dynamic_actions_registered.", __func__);
			return ERROR_ACCESS_DENIED;
		}
		
		xlive_custom_dynamic_actions_count = custom_action_count;
		
		LeaveCriticalSection(&xlive_critsec_custom_actions);
	}
	
	return ERROR_SUCCESS;
}

// #477
void WINAPI XCustomRegisterDynamicActions()
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_custom_actions);
	if (!xlive_custom_dynamic_actions_registered) {
		xlive_custom_dynamic_actions_registered = true;
		
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s already xlive_custom_dynamic_actions_registered.", __func__);
	}
	LeaveCriticalSection(&xlive_critsec_custom_actions);
}

// #478
void WINAPI XCustomUnregisterDynamicActions()
{
	TRACE_FX();
	EnterCriticalSection(&xlive_critsec_custom_actions);
	if (xlive_custom_dynamic_actions_registered) {
		xlive_custom_dynamic_actions_registered = false;
		xlive_custom_dynamic_actions_count = 0;
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s already !xlive_custom_dynamic_actions_registered.", __func__);
	}
	LeaveCriticalSection(&xlive_critsec_custom_actions);
}

// #479
BOOL WINAPI XCustomGetCurrentGamercard(uint32_t* user_index, XUID* xuid)
{
	TRACE_FX();
	if (!user_index || !xuid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!user_index || !xuid).", __func__);
		return FALSE;
	}
	
	EnterCriticalSection(&xlln_critsec_user_card);
	if (IsWindowVisible(xlln_hwnd_user_card)) {
		*user_index = xlln_user_card_current_user_index;
		*xuid = xlln_user_card_current_xuid;
	}
	LeaveCriticalSection(&xlln_critsec_user_card);
	
	BOOL result = ((xuid && *xuid) ? TRUE : FALSE);
	return result;
}
