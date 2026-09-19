#pragma once
#include <vector>

typedef bool (__cdecl* tGuideUiHandler)();
typedef struct _GUIDE_UI_HANDLER_INFO {
	size_t xllnModule = 0;
	tGuideUiHandler guideUiHandler;
} GUIDE_UI_HANDLER_INFO;

typedef enum : uint32_t {
	MAIN_SHOW,
	MAIN_SHOW_LOGIN,
	MAIN_HIDE,
	DEBUG_LOG_SHOW,
	DEBUG_LOG_HIDE,
	SOCKETS_SHOW,
	SOCKETS_HIDE,
	CONNECTIONS_SHOW,
	CONNECTIONS_HIDE,
	MESSAGE_BOX_SHOW,
	MESSAGE_BOX_HIDE,
	INPUT_BOX_SHOW,
	INPUT_BOX_HIDE,
	USER_CUSTOM_LIST_SHOW,
	USER_CUSTOM_LIST_HIDE,
	USER_CARD_SHOW,
	USER_CARD_HIDE,
	ACHIEVEMENTS_SHOW,
	ACHIEVEMENTS_HIDE,
	STATS_SHOW,
	STATS_HIDE,
} XllnShowType;

extern bool Initialised_XRender;
extern CRITICAL_SECTION xlive_critsec_fps_limit;
extern uint32_t xlive_fps_limit;
extern CRITICAL_SECTION xlln_critsec_guide_ui_handlers;
extern std::vector<GUIDE_UI_HANDLER_INFO> xlln_guide_ui_handlers;
extern CRITICAL_SECTION xlive_critsec_hotkeys;
extern uint16_t xlive_hotkey_id_guide;
bool XllnUpdateXliveSystemUIOpenState(bool is_system_ui_open);
bool XllnUpdateXliveSystemUIOpenState();
bool XllnShowWindow(XllnShowType show_type);
bool XllnIsXliveSystemUIOpen();
HRESULT D3DOnCreateDevice(IUnknown* d3d_device, void* d3d_presentation_parameters);
void XllnThreadHotkeysStop();
void XllnThreadHotkeysAbandon();
int32_t InitXRender();
int32_t UninitXRender();
uint32_t SetFPSLimit(uint32_t fps_limit);
