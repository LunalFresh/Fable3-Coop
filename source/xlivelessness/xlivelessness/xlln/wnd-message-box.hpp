#pragma once
#include "../xlive/xdefs.hpp"
#include <stdint.h>

extern HWND xlln_hwnd_message_box;

uint32_t InitXllnWndMessageBox();
uint32_t UninitXllnWndMessageBox();

uint32_t XllnWndMessageBoxOpen(const wchar_t* title, const wchar_t* description, uint32_t icon_type, const wchar_t** button_labels, uint32_t button_count, uint32_t focus_button, MESSAGEBOX_RESULT* messagebox_result, XOVERLAPPED* xoverlapped);
