#pragma once
#include <stdint.h>

extern HWND xlln_window_hwnd;

void XLLNWindowUpdateUserInputBoxes(uint32_t user_index);
bool XllnWndMainUpdateNetworkAdapters();
uint32_t InitXllnWndMain();
uint32_t UninitXllnWndMain();
