#pragma once

extern bool xlive_net_initialized;
extern XNetStartupParams xlive_net_startup_params;
extern CRITICAL_SECTION xlive_critsec_xnet_session_keys;

bool InitXNet();
bool UninitXNet();
