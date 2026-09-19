#include <winsock2.h>
#include "../xlln/social.hpp"
#include "./xdefs.hpp"
#include "./xshow.hpp"
#include "../xlln/debug-log.hpp"
#include "./xlive.hpp"
#include "./xrender.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-achievements.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/wnd-message-box.hpp"
#include "../xlln/wnd-input-box.hpp"
#include "../xlln/wnd-user-custom-list.hpp"
#include "../xlln/wnd-user-card.hpp"

// #5206
uint32_t WINAPI XShowMessagesUI(uint32_t user_index)
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
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5208
uint32_t WINAPI XShowGameInviteUI(uint32_t user_index, const XUID* recipient_xuids, size_t recipient_xuid_count, void* reserved)
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
	if (recipient_xuid_count && !recipient_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (recipient_xuid_count && !recipient_xuids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!recipient_xuid_count && recipient_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!recipient_xuid_count && recipient_xuids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (recipient_xuid_count > XMSG_MAX_RECIPIENTS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s recipient_xuid_count (%u) is greater than XMSG_MAX_RECIPIENTS (%u).", __func__, recipient_xuid_count, XMSG_MAX_RECIPIENTS);
		return ERROR_INVALID_PARAMETER;
	}
	if (reserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s reserved is not officially implemented.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (recipient_xuid_count) {
        uint32_t result = SocialInvite(user_index, recipient_xuid_count, recipient_xuids);
        if (result != ERROR_SUCCESS) return result;
    }
    SocialShow(user_index);
	
	return ERROR_SUCCESS;
}

// #5209
uint32_t WINAPI XShowMessageComposeUI(uint32_t user_index, const XUID* recipient_xuids, size_t recipient_xuid_count, const wchar_t* message_text)
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
	if (recipient_xuid_count && !recipient_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (recipient_xuid_count && !recipient_xuids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!recipient_xuid_count && recipient_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!recipient_xuid_count && recipient_xuids).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (recipient_xuid_count > XMSG_MAX_RECIPIENTS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s recipient_xuid_count (%u) is greater than XMSG_MAX_RECIPIENTS (%u).", __func__, recipient_xuid_count, XMSG_MAX_RECIPIENTS);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (message_text) {}
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5210
uint32_t WINAPI XShowFriendRequestUI(uint32_t user_index, XUID xuid_user)
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
	if (xlive_local_users[user_index].xuid == xuid_user) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_user (0x%016I64x) cannot be the local player.", __func__, xuid_user);
		return ERROR_INVALID_PARAMETER;
	}
	if (xuid_user && !IsLiveEnabledXUID(xuid_user)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_user (0x%016I64x) must be a LIVE enabled account.", __func__, xuid_user);
		return ERROR_INVALID_PARAMETER;
	}
	
	XllnWndUserCardShow(true, user_index, xuid_user);
	
	return ERROR_SUCCESS;
}

// #5212
uint32_t WINAPI XShowCustomPlayerListUI(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* title_text
	, const wchar_t* description_text
	, const uint8_t* image_data
	, size_t image_data_size
	, const XPLAYERLIST_USER* list_players
	, size_t list_player_count
	, const XPLAYERLIST_BUTTON* x_button_action
	, const XPLAYERLIST_BUTTON* y_button_action
	, XPLAYERLIST_RESULT* result_player_list_action
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	uint32_t result = XllnUserCustomListShow(
		user_index
		, flags
		, title_text
		, description_text
		, image_data
		, image_data_size
		, list_players
		, list_player_count
		, x_button_action
		, y_button_action
		, result_player_list_action
		, xoverlapped
	);
	return result;
}

// #5214
uint32_t WINAPI XShowPlayerReviewUI(uint32_t user_index, XUID xuid_feedback_target)
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
	if (!xuid_feedback_target) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid_feedback_target is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XllnWndUserCardShow(true, user_index, xuid_feedback_target);
	
	return ERROR_SUCCESS;
}

// #5215
uint32_t WINAPI XShowGuideUI(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5216
uint32_t WINAPI XShowKeyboardUI(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* default_text
	, const wchar_t* title_text
	, const wchar_t* description_text
	, wchar_t* result_text
	, size_t result_text_size
	, XOVERLAPPED* xoverlapped
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
	if (flags && (flags & ~(VKBD_LATIN_PASSWORD | VKBD_HIGHLIGHT_TEXT | VKBD_ENABLEIME | VKBD_MULTILINE))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Invalid flags (0x%08x). Must only be any of VKBD_DEFAULT | VKBD_LATIN_PASSWORD | VKBD_HIGHLIGHT_TEXT | VKBD_ENABLEIME | VKBD_MULTILINE.", __func__, flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_text) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_text is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_text_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_text_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	//TODO: Use flags.
	uint32_t result = XllnWndInputBoxOpen(title_text, description_text, default_text, result_text, result_text_size, xoverlapped);
	
	return result;
}

// #5218
uint32_t WINAPI XShowArcadeUI(uint32_t user_index)
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
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5250
uint32_t WINAPI XShowAchievementsUI(uint32_t user_index)
{
	TRACE_FX();
	uint32_t result = XllnAchievementsShow(user_index);
	return result;
}

// #5252
uint32_t WINAPI XShowGamerCardUI(uint32_t user_index, XUID xuid_player)
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
	
	XllnWndUserCardShow(true, user_index, xuid_player);
	
	return ERROR_SUCCESS;
}

// #5260
// pane_count - Number of users to sign in.
uint32_t WINAPI XShowSigninUI(uint32_t pane_count, uint32_t flags)
{
	TRACE_FX();
	if (pane_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pane_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (pane_count > XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s pane_count (%u) cannot sign in more than %u users.", __func__, pane_count, XLIVE_LOCAL_USER_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if ((flags & XSSUI_FLAGS_LOCALSIGNINONLY) && (flags & XSSUI_FLAGS_SHOWONLYONLINEENABLED)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags (0x%08x) cannot specify both XSSUI_FLAGS_LOCALSIGNINONLY and XSSUI_FLAGS_SHOWONLYONLINEENABLED.", __func__, flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (flags & (XSSUI_FLAGS_CONVERTOFFLINETOGUEST | (uint16_t)~(XSSUI_FLAGS_LOCALSIGNINONLY | XSSUI_FLAGS_SHOWONLYONLINEENABLED))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags (0x%08x) no support for XSSUI_FLAGS_CONVERTOFFLINETOGUEST or (uint16_t)~(XSSUI_FLAGS_LOCALSIGNINONLY | XSSUI_FLAGS_SHOWONLYONLINEENABLED).", __func__, flags);
		return ERROR_INVALID_PARAMETER;
	}
	
	XllnShowWindow(XllnShowType::MAIN_SHOW_LOGIN);
	
	return ERROR_SUCCESS;
	// no users signed in with multiplayer privilege. if XSSUI_FLAGS_SHOWONLYONLINEENABLED is flagged?
	return ERROR_FUNCTION_FAILED;
}

// #5266
uint32_t WINAPI XShowMessageBoxUI(
	uint32_t user_index
	, const wchar_t* title_text
	, const wchar_t* message_text
	, uint32_t button_count
	, const wchar_t** button_texts
	, uint32_t focused_button_index
	, uint32_t flags
	, MESSAGEBOX_RESULT* result_message_box_action
	, XOVERLAPPED* xoverlapped
)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (!result_message_box_action) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_message_box_action is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!title_text) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s title_text is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!message_text) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s message_text is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (flags & ~(XMB_ICON_MASK | XMB_MODE_MASK)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ~(XMB_ICON_MASK | XMB_MODE_MASK) (0x%08x) inverted mask includes bits in flags (0x%08x).", __func__, ~(XMB_ICON_MASK | XMB_MODE_MASK), flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (flags & (XMB_VERIFYPASSCODEMODE | XMB_PASSCODEMODE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags does not officially support XMB_VERIFYPASSCODEMODE or XMB_PASSCODEMODE.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (button_count <= 0 || button_count > XMB_MAXBUTTONS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s button_count (%u) must be between 1 and %u.", __func__, button_count, XMB_MAXBUTTONS);
		return ERROR_INVALID_PARAMETER;
	}
	if (focused_button_index > button_count - 1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (focused_button_index > button_count - 1) (%u > %u - 1).", __func__, focused_button_index, button_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (!button_texts) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s button_texts is NULL when button_count > 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	for (DWORD i = 0; i < button_count; i++) {
		if (!button_texts[i]) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s button_texts[%u] is NULL.", __func__, i);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	uint32_t result = XllnWndMessageBoxOpen(title_text, message_text, (flags & XMB_ICON_MASK), button_texts, button_count, focused_button_index, result_message_box_action, xoverlapped);
	
	return result;
}

// #5271
uint32_t WINAPI XShowPlayersUI(uint32_t user_index)
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
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5275
uint32_t WINAPI XShowFriendsUI(uint32_t user_index)
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
	
	SocialShow(user_index);
	
	return ERROR_SUCCESS;
}

// #5298
uint32_t WINAPI XLiveGetGuideKey(XINPUT_KEYSTROKE* input_keystroke)
{
	TRACE_FX();
	if (!input_keystroke) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s input_keystroke is NULL.", __func__);
		return E_INVALIDARG;
	}
	input_keystroke->VirtualKey = xlive_hotkey_id_guide;
	input_keystroke->Unicode = 0;
	return S_OK;
}

// #5299
uint32_t WINAPI XShowGuideKeyRemapUI(uint32_t user_index)
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
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// #5365
uint32_t WINAPI XShowMarketplaceUI(uint32_t user_index, uint32_t entry_point_flag, uint64_t offer_id, uint32_t content_category_flags)
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
	if (entry_point_flag >= XSHOWMARKETPLACEUI_ENTRYPOINT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown entry_point_flag enum (0x%08x).", __func__, entry_point_flag);
		return ERROR_INVALID_PARAMETER;
	}
	if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%sdwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPLIST.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%sdwEntryPoint == XSHOWMARKETPLACEUI_ENTRYPOINT_MEMBERSHIPITEM.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (offer_id) {
		if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_id (0x%016I64x) should be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST.", __func__, offer_id);
			return ERROR_INVALID_PARAMETER;
		}
		if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_id (0x%016I64x) should be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND.", __func__, offer_id);
			return ERROR_INVALID_PARAMETER;
		}
	}
	else {
		if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_id must not be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
		if (entry_point_flag == XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_id must not be 0 when XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTITEM_BACKGROUND.", __func__);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	return ERROR_SUCCESS;
}

// Result errors = 0x8057F0XX
// #5366
uint32_t WINAPI XShowMarketplaceDownloadItemsUI(uint32_t user_index, uint32_t entry_point_flag, const uint64_t* offer_ids, size_t offer_id_count, HRESULT* result_status, XOVERLAPPED* xoverlapped)
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
	if (entry_point_flag >= XSHOWMARKETPLACEUI_ENTRYPOINT_MAX) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown entry_point_flag enum (0x%08x).", __func__, entry_point_flag);
		return ERROR_INVALID_PARAMETER;
	}
	if (entry_point_flag != XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS && entry_point_flag != XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s entry_point_flag enum (0x%08x) is not XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_FREEITEMS or XSHOWMARKETPLACEDOWNLOADITEMS_ENTRYPOINT_PAIDITEMS.", __func__, entry_point_flag);
		return ERROR_INVALID_PARAMETER;
	}
	if (!offer_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_ids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!offer_id_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s offer_id_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (offer_id_count > XMARKETPLACE_MAX_OFFERIDS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (offer_id_count > XMARKETPLACE_MAX_OFFERIDS) (%zu > %u).", __func__, offer_id_count, XMARKETPLACE_MAX_OFFERIDS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_status) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_status is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	*result_status = MPDI_E_INVALIDARG;
	*result_status = MPDI_E_OPERATION_FAILED;
	*result_status = S_OK;
	*result_status = MPDI_E_CANCELLED;
	
	XllnShowWindow(XllnShowType::MAIN_SHOW);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	return ERROR_SUCCESS;
}
