#pragma once
#include <stdint.h>
#include "../xlivelessness.hpp"

#define XLLN_CUSTOM_PACKET_SENTINEL (BYTE)0x00
namespace XLLNCustomPacketType {
	enum Type : BYTE {
		UNKNOWN = 0x00,
		STOCK_PACKET,
		STOCK_PACKET_FORWARDED,
		CUSTOM_OTHER,
		UNKNOWN_USER_ASK,
		UNKNOWN_USER_REPLY,
		LIVE_OVER_LAN_ADVERTISE,
		LIVE_OVER_LAN_UNADVERTISE,
	};
}

namespace PCartoCustomPacketType {
	enum Type : BYTE {
		UNKNOWN = 0x00,
	};
}

extern HWND H2hWnd;

extern tXShowGuideUI DetourXShowGuideUI;

bool InitXLive();
bool UninitXLive();
