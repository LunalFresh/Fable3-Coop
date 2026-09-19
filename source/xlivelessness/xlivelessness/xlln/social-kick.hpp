#pragma once
#include "../xlive/xdefs.hpp"

// A token identifies the current guest connection, not just a saved friend.
uint64_t SocialKickToken(XUID guest);
uint32_t SocialKickRequest(XUID guest, uint64_t token);
void SocialKickCancel();
bool SocialKickTakeResult(uint32_t& result);

// Version 1 bridge called only by the version-checked Fable module's game update.
typedef void (WINAPI* SocialNativeKick)(void* context);
uint32_t WINAPI XLLNSocialKickUpdate(uint32_t version, BOOL hostReady,
    void* context, uintptr_t link, SocialNativeKick kick);
