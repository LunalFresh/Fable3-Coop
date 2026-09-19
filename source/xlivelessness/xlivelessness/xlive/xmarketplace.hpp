#pragma once
#include <map>

extern CRITICAL_SECTION xlive_critsec_xmarketplace;
extern std::map<HANDLE, void*> xlive_xmarketplace_enumerators;
