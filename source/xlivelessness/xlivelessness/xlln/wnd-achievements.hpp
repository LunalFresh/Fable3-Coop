#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_achievements;
extern CRITICAL_SECTION xlln_critsec_achievements;

uint32_t InitXllnWndAchievements();
uint32_t UninitXllnWndAchievements();

uint32_t XllnAchievementsShow(uint32_t user_index);
void XllnWndAchievementsInvalidateAchievements();
