#include <winsock2.h>
#include <Windows.h>
#include <wincodec.h>
#include "./wnd-image.hpp"
#include "../utils/utils.hpp"
#include "./debug-log.hpp"
#include <CommCtrl.h>
#include <d2d1.h>

typedef struct _XLLN_WND_IMAGE_INFO {
	IWICFormatConverter* wic_converted_source_bitmap = 0;
	ID2D1Factory* d2d_factory = 0;
	ID2D1HwndRenderTarget* d2d_render_target = 0;
	ID2D1Bitmap* d2d_bitmap = 0;
} XLLN_WND_IMAGE_INFO;

static LRESULT CALLBACK SubclassWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	// Default DPI that maps image resolution directly to screen resolution.
	static const float DEFAULT_DPI = 96.0f;
	
	XLLN_WND_IMAGE_INFO* wnd_image_info = (XLLN_WND_IMAGE_INFO*)dwRefData;
	
	switch (uMsg) {
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			if (hdc) {
				HRESULT hr = S_OK;
				
				if (wnd_image_info->wic_converted_source_bitmap) {
					// Create render target if not yet created.
					if (!wnd_image_info->d2d_render_target) {
						if (!wnd_image_info->d2d_factory) {
							hr = E_FAIL;
						}
						
						RECT rc;
						if (SUCCEEDED(hr)) {
							hr = (GetClientRect(hWnd, &rc) ? S_OK : E_FAIL);
						}
						
						if (SUCCEEDED(hr)) {
							D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties();
							
							// Set the DPI to be the default system DPI to allow direct mapping between image pixels and desktop pixels in different system DPI settings.
							renderTargetProperties.dpiX = DEFAULT_DPI;
							renderTargetProperties.dpiY = DEFAULT_DPI;
							
							// Create a D2D render target.
							D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
							
							D2D1_HWND_RENDER_TARGET_PROPERTIES renderTargetPropertiesHandle = D2D1::HwndRenderTargetProperties(hWnd, size);
							
							hr = wnd_image_info->d2d_factory->CreateHwndRenderTarget(renderTargetProperties, renderTargetPropertiesHandle, &wnd_image_info->d2d_render_target);
						}
					}
					
					if (SUCCEEDED(hr) && !(wnd_image_info->d2d_render_target->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED)) {
						wnd_image_info->d2d_render_target->BeginDraw();
						
						wnd_image_info->d2d_render_target->SetTransform(D2D1::Matrix3x2F::Identity());
						
						// Get the background color.
						HWND parentWnd = GetParent(hWnd);
						LRESULT resultBrush = SendMessage(parentWnd, WM_CTLCOLORSTATIC, (WPARAM)hdc, (LPARAM)hWnd);
						if (resultBrush) {
							DeleteObject((HBRUSH)resultBrush);
						}
						COLORREF backgroundColor = GetBkColor(hdc);
						// Clear the background.
						wnd_image_info->d2d_render_target->Clear(D2D1::ColorF(backgroundColor));
						
						// D2DBitmap may have been released due to device loss. 
						// If so, re-create it from the source bitmap.
						if (!wnd_image_info->d2d_bitmap) {
							wnd_image_info->d2d_render_target->CreateBitmapFromWicBitmap(wnd_image_info->wic_converted_source_bitmap, 0, &wnd_image_info->d2d_bitmap);
						}
						
						if (wnd_image_info->d2d_bitmap) {
							D2D1_SIZE_F rtSize = wnd_image_info->d2d_render_target->GetSize();
							// Create a rectangle same size of current window.
							D2D1_RECT_F rectangle = D2D1::RectF(0.0f, 0.0f, rtSize.width, rtSize.height);
							wnd_image_info->d2d_render_target->DrawBitmap(wnd_image_info->d2d_bitmap, rectangle);
						}
						
						hr = wnd_image_info->d2d_render_target->EndDraw();
						
						// In case of device loss, discard D2D render target and D2DBitmap.
						// They will be re-create in the next rendering pass
						if (hr == D2DERR_RECREATE_TARGET) {
							SafeRelease(wnd_image_info->d2d_bitmap);
							SafeRelease(wnd_image_info->d2d_render_target);
							// Force a re-render.
							hr = (InvalidateRect(hWnd, NULL, TRUE) ? S_OK : E_FAIL);
						}
					}
				}
				
				if (!SUCCEEDED(hr) || !wnd_image_info->wic_converted_source_bitmap) {
					RECT rc;
					hr = (GetClientRect(hWnd, &rc) ? S_OK : E_FAIL);
					HICON hIcon = LoadIconW(NULL, IDI_WARNING);
					DrawIconEx(hdc, 0, 0, hIcon, (rc.right - rc.left), (rc.bottom - rc.top), 0, 0, DI_NORMAL);
				}
				
				EndPaint(hWnd, &ps);
				
				return 0;
			}
			break;
		}
		case WM_NCDESTROY: {
			SafeRelease(wnd_image_info->d2d_bitmap);
			SafeRelease(wnd_image_info->d2d_render_target);
			SafeRelease(wnd_image_info->d2d_factory);
			
			delete wnd_image_info;
			wnd_image_info = 0;
			
			RemoveWindowSubclass(hWnd, SubclassWindowProc, uIdSubclass);
			break;
		}
	}
	
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HWND XllnWndImageCreate(
	HWND hwnd_parent
	, int pos_x
	, int pos_y
	, int width
	, int height
	, IWICFormatConverter* wic_converted_source_bitmap
)
{
	XLLN_WND_IMAGE_INFO* wndImageInfo = new XLLN_WND_IMAGE_INFO;
	wndImageInfo->wic_converted_source_bitmap = wic_converted_source_bitmap;
	
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &wndImageInfo->d2d_factory);
	if (!SUCCEEDED(hr)) {
		DWORD errorD2D1CreateFactory = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorD2D1CreateFactory, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s D2D1CreateFactory failed."
			, __func__
		);
	}
	
	bool resultSetWindowSubclass = false;
	HWND resultControl = CreateWindowExW(0, L"STATIC", 0, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, pos_x, pos_y, width, height, hwnd_parent, 0, 0, 0);
	if (!resultControl) {
		DWORD errorCreateWindowEx = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCreateWindowEx, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s CreateWindowExW failed."
			, __func__
		);
	}
	else {
		resultSetWindowSubclass = !!SetWindowSubclass(resultControl, &SubclassWindowProc, 0, (DWORD_PTR)wndImageInfo);
		if (!resultSetWindowSubclass) {
			DWORD errorSetWindowSubclass = GetLastError();
			XLLN_DEBUG_LOG_ECODE(errorSetWindowSubclass, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
				, "%s SetWindowSubclass failed."
				, __func__
			);
			
			SendMessage(resultControl, WM_CLOSE, (WPARAM)0, (LPARAM)0);
			
			resultControl = 0;
		}
	}
	
	if (!resultSetWindowSubclass) {
		SafeRelease(wndImageInfo->d2d_factory);
		
		delete wndImageInfo;
		wndImageInfo = 0;
	}
	
	return resultControl;
}
