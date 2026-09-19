#pragma once
#include "../xlive/xdefs.hpp"
#include <stdint.h>

extern HWND xlln_hwnd_input_box;

uint32_t InitXllnWndInputBox();
uint32_t UninitXllnWndInputBox();

uint32_t XllnWndInputBoxOpen(const wchar_t* title, const wchar_t* description, const wchar_t* default_text, wchar_t* result_text, size_t result_text_length, XOVERLAPPED* xoverlapped);
