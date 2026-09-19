#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_sockets;

uint32_t InitXllnWndSockets();
uint32_t UninitXllnWndSockets();

void XllnWndSocketsInvalidateSockets();
