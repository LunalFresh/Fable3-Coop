#pragma once
#include <map>
#include <set>

extern CRITICAL_SECTION xlive_critsec_presence_enumerators;
extern std::map<HANDLE, std::set<XUID>> xlive_presence_enumerators;
