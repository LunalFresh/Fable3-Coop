#pragma once
#include <stdint.h>

namespace TITLE_VERSION {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"TITLE_SHIPPED",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tTITLE_SHIPPED,
	} TYPE;
}

extern TITLE_VERSION::TYPE title_version;
#define TITLEMODULEANY 0xFFFFFFFF
#define INVALID_OFFSET 0xFFFFFFFF
