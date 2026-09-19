#pragma once

extern CRITICAL_SECTION xlive_critsec_xnotify;
extern bool xlive_notify_system_ui_open;

// True only after the title has consumed the queued system-UI close event.
bool XLiveNotifyIsSystemUIClosed();

void XLiveNotifyAddEvent(uint32_t notification_id, size_t notification_value);
bool XLiveNotifyDeleteListener(HANDLE notification_listener);
bool InitXNotify();
bool UninitXNotify();
