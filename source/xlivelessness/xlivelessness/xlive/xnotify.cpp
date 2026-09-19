#include <winsock2.h>
#include "xdefs.hpp"
#include "xnotify.hpp"
#include "../xlln/debug-log.hpp"
#include "xlive.hpp"
#include <map>
#include <set>
#include <vector>
#include <intrin.h>

typedef struct {
	HANDLE listener;
	uint32_t area;
} XNOTIFY_LISTENER_INFO;

CRITICAL_SECTION xlive_critsec_xnotify;
// Key: listener handle.
static std::map<HANDLE, XNOTIFY_LISTENER_INFO> xlive_notify_listeners;
// Key: single notification area. Value: listener handles that are in that area.
static std::map<uint32_t, std::vector<HANDLE>> xlive_notify_listener_areas;
// Key: notification_area. Key: notification_id. Value: notification_value.
static std::map<uint32_t, std::map<uint32_t, size_t>> xlive_notify_pending_notifications;
// Each LIVE subscriber needs its own accepted-invite delivery. Fable's other
// LIVE handlers poll unfiltered and ignore this ID; they must not consume the
// multiplayer handler's copy. Other notification behavior is unchanged here.
static std::map<HANDLE, uint32_t> xlive_notify_pending_invites;

static bool ConsumePendingInvite(HANDLE listener, uint32_t* id, void* value)
{
	auto invite = xlive_notify_pending_invites.find(listener);
	if (invite == xlive_notify_pending_invites.end()) return false;
	*id = XN_LIVE_INVITE_ACCEPTED;
	if (value) memcpy(value, &invite->second, sizeof(uint32_t));
	xlive_notify_pending_invites.erase(invite);
	return true;
}

bool xlive_notify_system_ui_open = false;

bool XLiveNotifyIsSystemUIClosed()
{
	EnterCriticalSection(&xlive_critsec_xnotify);
	const auto &pending = xlive_notify_pending_notifications[XNOTIFY_SYSTEM];
	const bool closed = !xlive_notify_system_ui_open && pending.find(XN_SYS_UI) == pending.end();
	LeaveCriticalSection(&xlive_critsec_xnotify);
	return closed;
}

static uint32_t GetNotificationArea(uint32_t notification_id)
{
	uint32_t notificationArea = 0;
	
	if (!notification_id) {
		return notificationArea;
	}
	
	switch (XNID_AREA(notification_id)) {
		case _XNAREA_SYSTEM:
			notificationArea = XNOTIFY_SYSTEM;
			break;
		case _XNAREA_LIVE:
			notificationArea = XNOTIFY_LIVE;
			break;
		case _XNAREA_FRIENDS:
			notificationArea = XNOTIFY_FRIENDS;
			break;
		case _XNAREA_CUSTOM:
			notificationArea = XNOTIFY_CUSTOM;
			break;
		case _XNAREA_XMP:
			notificationArea = XNOTIFY_XMP;
			break;
		case _XNAREA_MSGR:
			notificationArea = XNOTIFY_MSGR;
			break;
		case _XNAREA_PARTY:
			notificationArea = XNOTIFY_PARTY;
			break;
	}
	
	return notificationArea;
}

static bool XLiveNotifyRetainNotificationOnZeroListeners(uint32_t notification_id)
{
	// Only hold these specific notifications when there are no existing listeners for them.
	switch (notification_id) {
		case XN_LIVE_INVITE_ACCEPTED:
		case XN_SYS_UI:
		case XN_SYS_SIGNINCHANGED: {
			return true;
		}
	}
		
	return false;
}

static size_t XLiveNotifyValueSize(uint32_t notification_id)
{
	switch (notification_id) {
		case XN_SYS_SIGNINCHANGED:
		case XN_SYS_PROFILESETTINGCHANGED:
		case XN_LIVE_CONNECTIONCHANGED:
		case XN_LIVE_INVITE_ACCEPTED:
		case XN_FRIENDS_PRESENCE_CHANGED:
		case XN_FRIENDS_FRIEND_ADDED:
		case XN_FRIENDS_FRIEND_REMOVED:
		case XN_CUSTOM_ACTIONPRESSED:
		case XN_CUSTOM_GAMERCARD: {
			return sizeof(uint32_t);
		}
		case XN_SYS_UI:
		case XN_LIVE_LINK_STATE_CHANGED:
		case XN_LIVE_VOICECHAT_AWAY: {
			return sizeof(BOOL);
		}
		case XN_SYS_MUTELISTCHANGED:
		case XN_LIVE_CONTENT_INSTALLED:
		case XN_LIVE_PRESENCE_CHANGED: {
			return 0;
		}
		//case XN_LIVE_CONTENT_CHANGED: {
		//	return XLIVE_CONTENT_ID_SIZE;
		//}
	}
	
	__debugbreak();
	
	return 0;
}

static void XLiveNotifyAddEvent_(uint32_t notification_id, size_t notification_value)
{
	uint32_t notificationArea = GetNotificationArea(notification_id);
	
	if (!notificationArea) {
		return;
	}
	
	switch (notification_id) {
		case XN_SYS_UI: {
			xlive_notify_system_ui_open = (notification_value != 0);
			break;
		}
	}
	
	bool saveNotification = false;
	
	for (const HANDLE &xnotifyListener : xlive_notify_listener_areas[notificationArea]) {
		saveNotification = true;
		if (notification_id == XN_LIVE_INVITE_ACCEPTED)
			xlive_notify_pending_invites[xnotifyListener] = (uint32_t)notification_value;
		SetEvent(xnotifyListener);
	}
	if (notification_id == XN_LIVE_INVITE_ACCEPTED && saveNotification) return;
	
	if (!saveNotification) {
		saveNotification = XLiveNotifyRetainNotificationOnZeroListeners(notification_id);
	}
	
	if (saveNotification) {
		xlive_notify_pending_notifications[notificationArea][notification_id] = notification_value;
	}
}

void XLiveNotifyAddEvent(uint32_t notification_id, size_t notification_value)
{
	uint32_t notificationArea = GetNotificationArea(notification_id);
	
	if (!notificationArea) {
		return;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		XLiveNotifyAddEvent_(notification_id, notification_value);
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
}

static bool XLiveNotifyDeleteListener_(HANDLE notification_listener)
{
	if (!xlive_notify_listeners.count(notification_listener)) {
		return false;
	}
	auto invite = xlive_notify_pending_invites.find(notification_listener);
	if (invite != xlive_notify_pending_invites.end()) {
		// Retain an undelivered invite only when the last LIVE subscriber leaves.
		if (xlive_notify_listener_areas[XNOTIFY_LIVE].size() == 1)
			xlive_notify_pending_notifications[XNOTIFY_LIVE][XN_LIVE_INVITE_ACCEPTED] = invite->second;
		xlive_notify_pending_invites.erase(invite);
	}
	
	{
		uint32_t iArea = 1;
		while (1) {
			// Erase entries of this listener.
			for (auto itrNotificationListener = xlive_notify_listener_areas[iArea].begin(); itrNotificationListener != xlive_notify_listener_areas[iArea].end(); ) {
				if (*itrNotificationListener == notification_listener) {
					xlive_notify_listener_areas[iArea].erase(itrNotificationListener++);
					
					if (xlive_notify_listener_areas[iArea].size() == 0) {
						// Erase pending notifications.
						for (auto itrPendingNotification = xlive_notify_pending_notifications[iArea].begin(); itrPendingNotification != xlive_notify_pending_notifications[iArea].end(); ) {
							if (XLiveNotifyRetainNotificationOnZeroListeners(itrPendingNotification->first)) {
								++itrPendingNotification;
							}
							else {
								size_t notificationValueSize = XLiveNotifyValueSize(itrPendingNotification->first);
								if (notificationValueSize > sizeof(size_t)) {
									delete[] (void*)itrPendingNotification->second;
								}
								xlive_notify_pending_notifications[iArea].erase(itrPendingNotification++);
							}
						}
					}
					
					break;
				}
				else {
					++itrNotificationListener;
				}
			}
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
	}
	
	xlive_notify_listeners.erase(notification_listener);
	
	return true;
}

bool XLiveNotifyDeleteListener(HANDLE notification_listener)
{
	bool result = false;
	EnterCriticalSection(&xlive_critsec_xnotify);
	result = XLiveNotifyDeleteListener_(notification_listener);
	LeaveCriticalSection(&xlive_critsec_xnotify);
	return result;
}

// #651
BOOL WINAPI XNotifyGetNext(HANDLE notification_listener, uint32_t notification_id_filter, uint32_t* notification_id, void* notification_value)
{
	TRACE_FX();
	if (!notification_listener) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s notification_listener is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (notification_listener == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s notification_listener is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (!notification_id) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s notification_id is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	
	*notification_id = 0;
	
	uint32_t notificationArea = GetNotificationArea(notification_id_filter);
	
	if (notification_id_filter && !notificationArea) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s unknown notification_id_filter value (0x%08x)."
			, __func__
			, notification_id_filter
		);
		return FALSE;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		auto const &itrNotificationListener = xlive_notify_listeners.find(notification_listener);
		if (itrNotificationListener != xlive_notify_listeners.end()) {
			XNOTIFY_LISTENER_INFO &notificationListenerInfo = itrNotificationListener->second;
			if (notification_id_filter) {
				if (!(notificationListenerInfo.area & notificationArea)) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s notification_id_filter (0x%08x) is not part of notification_listener (0x%zx) subscribed notification area (0x%08x)."
						, __func__
						, notification_id_filter
						, notification_listener
						, notificationListenerInfo.area
					);
				}
				else if (notification_id_filter == XN_LIVE_INVITE_ACCEPTED) {
					ConsumePendingInvite(notification_listener, notification_id, notification_value);
				}
				else {
					auto &pendingNotificationArea = xlive_notify_pending_notifications[notificationArea];
					auto const &itrPendingNotification = pendingNotificationArea.find(notification_id_filter);
					if (itrPendingNotification != pendingNotificationArea.end()) {
						*notification_id = itrPendingNotification->first;
						size_t notificationValueSize = XLiveNotifyValueSize(*notification_id);
						if (notification_value && notificationValueSize) {
							if (notificationValueSize > sizeof(size_t)) {
								memcpy_s(notification_value, notificationValueSize, (void*)itrPendingNotification->second, notificationValueSize);
							}
							else {
								memcpy_s(notification_value, notificationValueSize, &itrPendingNotification->second, notificationValueSize);
							}
						}
						if (notificationValueSize > sizeof(size_t)) {
							delete[] (void*)itrPendingNotification->second;
						}
						pendingNotificationArea.erase(itrPendingNotification);
					}
				}
			}
			else {
				uint32_t iArea = 1;
				while (1) {
					if (notificationListenerInfo.area & iArea) {
						auto &pendingNotificationArea = xlive_notify_pending_notifications[iArea];
						auto const &itrPendingNotification = pendingNotificationArea.begin();
						if (iArea == XNOTIFY_LIVE &&
							(itrPendingNotification == pendingNotificationArea.end() || XN_LIVE_INVITE_ACCEPTED < itrPendingNotification->first) &&
							ConsumePendingInvite(notification_listener, notification_id, notification_value)) break;
						if (itrPendingNotification != pendingNotificationArea.end()) {
							*notification_id = itrPendingNotification->first;
							size_t notificationValueSize = XLiveNotifyValueSize(*notification_id);
							if (notification_value && notificationValueSize) {
								if (notificationValueSize > sizeof(size_t)) {
									memcpy_s(notification_value, notificationValueSize, (void*)itrPendingNotification->second, notificationValueSize);
								}
								else {
									memcpy_s(notification_value, notificationValueSize, &itrPendingNotification->second, notificationValueSize);
								}
							}
							if (notificationValueSize > sizeof(size_t)) {
								delete[] (void*)itrPendingNotification->second;
							}
							pendingNotificationArea.erase(itrPendingNotification);
							break;
						}
					}
					
					if (iArea == (1 << 31)) {
						break;
					}
					iArea <<= 1;
				}
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	if (*notification_id) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s notification_id (0x%08x) consumed by notification_listener (0x%zx), caller=%p."
			, __func__
			, *notification_id
			, notification_listener
			, _ReturnAddress()
		);
	}
	else {
		EnterCriticalSection(&xlive_critsec_xnotify);
		// An unrelated filtered poll must not clear an outstanding invite's wakeup.
		if (xlive_notify_pending_invites.count(notification_listener)) SetEvent(notification_listener);
		else ResetEvent(notification_listener);
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return *notification_id ? TRUE : FALSE;
}

// #652
void WINAPI XNotifyPositionUI(uint32_t position_flags)
{
	TRACE_FX();
	if (
		position_flags & (~XNOTIFYUI_POS_MASK)
		|| (position_flags & XNOTIFYUI_POS_TOPCENTER && position_flags & XNOTIFYUI_POS_BOTTOMCENTER)
		|| (position_flags & XNOTIFYUI_POS_CENTERLEFT && position_flags & XNOTIFYUI_POS_CENTERRIGHT)
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s Invalid position_flags (0x%08x). Check XNOTIFYUI_POS_* bits. Do not specify both TOP and BOTTOM or both LEFT and RIGHT."
			, __func__
			, position_flags
		);
		return;
	}
	
	// TODO Set overlay/popup preferred position whenever it actually gets implemented.
}

// #653
uint32_t WINAPI XNotifyDelayUI(uint32_t milliseconds)
{
	TRACE_FX();
	return ERROR_SUCCESS;
}

// #5270: Requires XNotifyGetNext to process the listener.
HANDLE WINAPI XNotifyCreateListener(uint64_t notification_area)
{
	TRACE_FX();
	if (HIDWORD(notification_area)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s HIDWORD(notification_area) value set (0x%08x)."
			, __func__
			, HIDWORD(notification_area)
		);
	}
	if ((uint32_t)notification_area & ~XNOTIFY_ALL) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
			, "%s unknown dwMsgFilter value (0x%08x) outside of XNOTIFY_ALL (0x%08x)."
			, __func__
			, (uint32_t)notification_area
			, XNOTIFY_ALL
		);
	}
	
	uint32_t unsanitisedAreas = (uint32_t)notification_area;
	if (!unsanitisedAreas) {
		unsanitisedAreas = XNOTIFY_ALL;
	}
	uint32_t sanitisedAreas = unsanitisedAreas & XNOTIFY_ALL;
	HANDLE xnotifyListener = CreateEvent(NULL, TRUE, FALSE, NULL);;
	
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		// A title may close its listener without XCloseHandle, so this value can be a recycled one
		// still mapped to the previous listener. Purge that before reusing the key.
		XLiveNotifyDeleteListener_(xnotifyListener);
		
		xlive_notify_listeners[xnotifyListener];
		xlive_notify_listeners[xnotifyListener].listener = xnotifyListener;
		xlive_notify_listeners[xnotifyListener].area = unsanitisedAreas;
		
		uint32_t iArea = 1;
		while (1) {
			if (iArea & sanitisedAreas) {
				xlive_notify_listener_areas[iArea].push_back(xnotifyListener);
				if (iArea == XNOTIFY_LIVE) {
					auto &retained = xlive_notify_pending_notifications[iArea];
					auto invite = retained.find(XN_LIVE_INVITE_ACCEPTED);
					if (invite != retained.end()) {
						xlive_notify_pending_invites[xnotifyListener] = (uint32_t)invite->second;
						retained.erase(invite);
					}
				}
				
				if (xlive_notify_pending_notifications[iArea].size() || xlive_notify_pending_invites.count(xnotifyListener)) {
					SetEvent(xnotifyListener);
				}
			}
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s hNotification (0x%08x) created, areas=0x%08x, caller=%p."
		, __func__
		, xnotifyListener
		, sanitisedAreas
		, _ReturnAddress()
	);
	
	return xnotifyListener;
}

bool InitXNotify()
{
	// Initialise every possible existing area so there are no key issues later.
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		uint32_t iArea = 1;
		while (1) {
			xlive_notify_listener_areas[iArea];
			xlive_notify_pending_notifications[iArea];
			
			if (iArea == (1 << 31)) {
				break;
			}
			iArea <<= 1;
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return TRUE;
}

bool UninitXNotify()
{
	{
		EnterCriticalSection(&xlive_critsec_xnotify);
		
		for (auto itrNotificationListener = xlive_notify_listeners.begin(); itrNotificationListener != xlive_notify_listeners.end(); ) {
			HANDLE hNotificationListener = (itrNotificationListener++)->first;
			if (!hNotificationListener) {
				continue;
			}
			XLiveNotifyDeleteListener_(hNotificationListener);
		}
		xlive_notify_listeners.clear();
		xlive_notify_listener_areas.clear();
		xlive_notify_pending_invites.clear();
		xlive_notify_pending_notifications[XNOTIFY_LIVE].erase(XN_LIVE_INVITE_ACCEPTED);
		
		{
			uint32_t iArea = 1;
			while (1) {
				xlive_notify_listener_areas[iArea];
				xlive_notify_pending_notifications[iArea];
				
				if (iArea == (1 << 31)) {
					break;
				}
				iArea <<= 1;
			}
		}
		
		LeaveCriticalSection(&xlive_critsec_xnotify);
	}
	
	return TRUE;
}
