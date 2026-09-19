#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_stats;
extern CRITICAL_SECTION xlln_critsec_stats;

uint32_t InitXllnWndStats();
uint32_t UninitXllnWndStats();

uint32_t XllnStatsShow(uint32_t user_index);
void XllnWndStatsInvalidateStats();
