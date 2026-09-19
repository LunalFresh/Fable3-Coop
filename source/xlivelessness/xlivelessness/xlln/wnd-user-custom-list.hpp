#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_user_custom_list;

extern CRITICAL_SECTION xlln_critsec_user_custom_list;

uint32_t InitXllnWndUserCustomList();
uint32_t UninitXllnWndUserCustomList();

uint32_t XllnUserCustomListShow(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* title_text
	, const wchar_t* description_text
	, const uint8_t* image_data
	, size_t image_data_size
	, const XPLAYERLIST_USER* list_players
	, size_t list_player_count
	, const XPLAYERLIST_BUTTON* x_button_action
	, const XPLAYERLIST_BUTTON* y_button_action
	, XPLAYERLIST_RESULT* result_player_list_action
	, XOVERLAPPED* xoverlapped
);
