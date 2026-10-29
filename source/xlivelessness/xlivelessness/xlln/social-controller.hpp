#pragma once
#include <Windows.h>
// These are XInput's button bits. Keep its SDK types private to the input
// adapter: the legacy LIVE headers already declare XINPUT_KEYSTROKE.
const WORD SocialPadUp=0x0001, SocialPadDown=0x0002, SocialPadLeft=0x0004,
    SocialPadRight=0x0008, SocialPadA=0x1000, SocialPadB=0x2000;

struct SocialControllerFrame {
    bool connected = false;
    WORD pressed = 0; // Button edges; directional holds repeat after a delay.
};

bool SocialControllerAttach();
void SocialControllerDetach();
void SocialControllerCapture(bool capture);
void SocialControllerSuspend();
SocialControllerFrame SocialControllerPoll(bool foreground, ULONGLONG now);
