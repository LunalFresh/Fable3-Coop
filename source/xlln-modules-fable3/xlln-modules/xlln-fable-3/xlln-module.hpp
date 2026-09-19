#pragma once
#include <stdint.h>

namespace TITLE_VERSION {
	const char* const TYPE_NAMES[] {
		"UNKNOWN",
		"TITLE_SHIPPED",
		"TITLE_PATCH_2",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tTITLE_SHIPPED,
		tTITLE_PATCH_2,
	} TYPE;
}

extern TITLE_VERSION::TYPE title_version;
#define TITLEMODULEANY 0xFFFFFFFF
#define INVALID_OFFSET 0xFFFFFFFF
