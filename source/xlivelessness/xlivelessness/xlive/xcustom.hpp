#pragma once
#include <stdint.h>
#include <map>

typedef struct _CUSTOM_ACTION_INFO {
	uint32_t action_index = 0;
	wchar_t* action_label = 0;
	bool action_closes_menu = false;
	
	~_CUSTOM_ACTION_INFO() {
		if (action_label) {
			delete[] action_label;
			action_label = 0;
		}
	}
} CUSTOM_ACTION_INFO;

extern CRITICAL_SECTION xlive_critsec_custom_actions;
extern std::map<uint32_t, CUSTOM_ACTION_INFO*> xlln_custom_actions;
