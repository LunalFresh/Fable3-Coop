#pragma once
#include "live-over-lan.hpp"
#include <map>
#include <vector>

#define XLOCATOR_PROPERTY_LIVE_COUNT_TOTAL 0x10008101
#define XLOCATOR_PROPERTY_LIVE_COUNT_PUBLIC 0x10008102
#define XLOCATOR_PROPERTY_LIVE_COUNT_GOLD 0x10008103
#define XLOCATOR_PROPERTY_LIVE_COUNT_PEER 0x10008104

extern CRITICAL_SECTION xlive_critsec_xlocator_enumerators;
extern std::map<HANDLE, std::vector<uint32_t>> xlive_xlocator_enumerators;
extern LIVE_SESSION* xlive_xlocator_local_session;
