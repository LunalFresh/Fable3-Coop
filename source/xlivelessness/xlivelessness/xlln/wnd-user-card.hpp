#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_user_card;
extern CRITICAL_SECTION xlln_critsec_user_card;

extern uint32_t xlln_user_card_current_user_index;
extern XUID xlln_user_card_current_xuid;

extern XUID xlln_user_card_custom_action_xuid;
extern uint32_t xlln_user_card_custom_action_index;
extern uint32_t xlln_user_card_custom_user_index;

uint32_t InitXllnWndUserCard();
uint32_t UninitXllnWndUserCard();

void XllnWndUserCardShow(bool show_window, uint32_t user_index, XUID view_xuid);
