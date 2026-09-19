#pragma once
#include <stdint.h>

template <typename T>
inline void SafeRelease(T* &p)
{
	if (p) {
		p->Release();
		p = 0;
	}
}

HWND XllnWndImageCreate(
	HWND hwnd_parent
	, int pos_x
	, int pos_y
	, int width
	, int height
	, IWICFormatConverter* wic_converted_source_bitmap
);
