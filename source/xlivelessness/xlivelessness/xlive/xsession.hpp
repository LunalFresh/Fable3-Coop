#pragma once
#include "live-over-lan.hpp"
#include <map>

extern CRITICAL_SECTION xlive_critsec_xsession;
extern std::map<HANDLE, LIVE_SESSION_XSESSION*> xlive_xsession_local_sessions;

int32_t InitXSession();
int32_t UninitXSession();
