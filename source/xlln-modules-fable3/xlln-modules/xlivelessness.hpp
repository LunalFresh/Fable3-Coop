#pragma once
#include <stdint.h>
#include "./xlive/xdefs.hpp"

#define XLLN_LOG_LEVEL_MASK		0b00111111
// Function call tracing.
#define XLLN_LOG_LEVEL_TRACE	0b00000001
// Function, variable and operation logging.
#define XLLN_LOG_LEVEL_DEBUG	0b00000010
// Generally useful information to log (service start/stop, configuration assumptions, etc).
#define XLLN_LOG_LEVEL_INFO		0b00000100
// Anything that can potentially cause application oddities, but is being handled adequately.
#define XLLN_LOG_LEVEL_WARN		0b00001000
// Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).
#define XLLN_LOG_LEVEL_ERROR	0b00010000
// Errors that will terminate the application.
#define XLLN_LOG_LEVEL_FATAL	0b00100000

#define XLLN_LOG_CONTEXT_MASK			(0b11000111 << 8)
// Logs related to Xlive(GFWL) functionality.
#define XLLN_LOG_CONTEXT_XLIVE			(0b00000001 << 8)
// Logs related to XLiveLessNess functionality.
#define XLLN_LOG_CONTEXT_XLIVELESSNESS	(0b00000010 << 8)
// Logs related to XLLN-Module functionality.
#define XLLN_LOG_CONTEXT_XLLN_MODULE	(0b00000100 << 8)
// Logs related to functionality from other areas of the application.
#define XLLN_LOG_CONTEXT_OTHER			(0b10000000 << 8)
// Logs related to the Title functionality.
#define XLLN_LOG_CONTEXT_TITLE			(0b01000000 << 8)

#define IsUsingBasePort(base_port) (base_port != 0 && base_port != 0xFFFF)

#define CHECK_FUNC_PTR_TYPE(type, ptr) ((void*)(1 ? ptr : (type)0))

namespace XLLNModifyPropertyTypes {
	const char* const TypeNames[] {
		"UNKNOWN",
		"FPS_LIMIT",
		"LiveOverLan_BROADCAST_HANDLER",
		"RECVFROM_CUSTOM_HANDLER_REGISTER",
		"RECVFROM_CUSTOM_HANDLER_UNREGISTER",
		"BASE_PORT",
		"GUIDE_UI_HANDLER",
		"XHV_ENGINE_ENABLED",
		"CALLBACK_NETWORK_INTERFACE_CHANGED",
		"CALLBACK_NETWORK_SOCKET_BIND",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tFPS_LIMIT,
		tLiveOverLan_BROADCAST_HANDLER,
		tRECVFROM_CUSTOM_HANDLER_REGISTER,
		tRECVFROM_CUSTOM_HANDLER_UNREGISTER,
		tBASE_PORT,
		tGUIDE_UI_HANDLER,
		tXHV_ENGINE_ENABLED,
		tCALLBACK_NETWORK_INTERFACE_CHANGED,
		tCALLBACK_NETWORK_SOCKET_BIND,
	} TYPE;

#pragma pack(push, 1) // Save then set byte alignment setting.

	typedef struct {
		char* Identifier;
		uint32_t* FuncPtr;
	} RECVFROM_CUSTOM_HANDLER_REGISTER;

#pragma pack(pop) // Return to original alignment setting.
	
	typedef void (WINAPI* XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED)(const char* interface_name, const SOCKADDR_STORAGE* interface_address_gateway, const SOCKADDR_STORAGE* interface_address_unicast, const SOCKADDR_STORAGE* interface_address_broadcast);
	typedef void (WINAPI* XLLN_CALLBACK_NETWORK_SOCKET_BIND)(bool has_binded, int32_t port_protocol, uint16_t port_binded, uint16_t port_requested, int16_t port_base_offset);
	
}

// #5000
typedef HRESULT (WINAPI* tXLiveInitialize)(XLIVE_INITIALIZE_INFO* xlive_initialise_info);
// #5001
typedef HRESULT (WINAPI* tXLiveInput)(XLIVE_INPUT_INFO* xlive_input_info);
// #5019
typedef HRESULT (WINAPI* tXLivePBufferSetByte)(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t value);
// #5028
typedef uint32_t (WINAPI* tXLiveLoadLibraryEx)(const wchar_t* module_pathname, HINSTANCE* module_handle, uint32_t flags);
// #5215
typedef uint32_t (WINAPI* tXShowGuideUI)(uint32_t user_index);
// #5260
typedef uint32_t (WINAPI* tXShowSigninUI)(uint32_t pane_count, uint32_t flags);
// #5297
typedef HRESULT (WINAPI* tXLiveInitializeEx)(XLIVE_INITIALIZE_INFO* xlive_initialise_info, uint32_t title_xlive_version);

// #41140
typedef uint32_t (WINAPI* tXLLNLogin)(uint32_t user_index, BOOL lsb_8_live_enabled_8_online_disabled_msb_16_reserved, uint32_t user_id, const char* user_username);
// #41141
typedef uint32_t (WINAPI* tXLLNLogout)(uint32_t user_index);
// #41142
typedef uint32_t (WINAPI* tXLLNModifyProperty)(XLLNModifyPropertyTypes::TYPE property_id, void** new_value, void** old_value);
// #41143
typedef uint32_t (WINAPI* tXLLNDebugLog)(uint32_t log_level, const char* message);
// #41144
typedef uint32_t (WINAPI* tXLLNDebugLogF)(uint32_t log_level, const char* const message_format, ...);
// #41145
typedef uint32_t (WINAPI* tXLLNGetXLLNStoragePath)(uint32_t module_handle, uint32_t* result_local_instance_index, wchar_t* result_storage_path_buffer, size_t* result_storage_path_buffer_size);
// #41146
typedef uint32_t (WINAPI* tXLLNSetBasePortOffsetMapping)(uint8_t* port_offsets, uint16_t* port_originals, uint8_t port_mappings_size);
// #41147
typedef uint32_t (WINAPI* tXLLNGetDebugLogLevel)(uint32_t* log_level);
// #41148
typedef uint32_t (WINAPI* tXllnDebugLogECodeF)(uint32_t log_level, uint32_t error_code, const char* const message_format, ...);

extern tXLLNLogin XLLNLogin;
extern tXLLNLogout XLLNLogout;
extern tXLLNModifyProperty XLLNModifyProperty;
extern tXLLNDebugLog XLLNDebugLog;
extern tXLLNDebugLogF XLLNDebugLogF;
extern tXLLNGetXLLNStoragePath XLLNGetXLLNStoragePath;
extern tXLLNSetBasePortOffsetMapping XLLNSetBasePortOffsetMapping;
extern tXLLNGetDebugLogLevel XLLNGetDebugLogLevel;
extern tXllnDebugLogECodeF XllnDebugLogECodeF;

bool InitXLiveLessNess();
bool UninitXLiveLessNess();

#ifdef _DEBUG
// Move this out to enable building with debug logs in release mode.
#define XLLN_DEBUG
#endif

#ifdef XLLN_DEBUG
#define XLLN_DEBUG_LOG(logLevel, format, ...) if (XLLNDebugLogF) { XLLNDebugLogF(logLevel, format, __VA_ARGS__); }
#define XLLN_DEBUG_LOG_ECODE(error_code, log_level, message_format, ...) XllnDebugLogECodeF(log_level, error_code, message_format, __VA_ARGS__)
#define GET_SOCKADDR_INFO(sockAddrStorage) GetSockAddrInfo(sockAddrStorage)
#else
#define XLLN_DEBUG_LOG(logLevel, format, ...)
#define XLLN_DEBUG_LOG_ECODE(error_code, log_level, message_format, ...)
#define GET_SOCKADDR_INFO(sockAddrStorage) (0)
#endif
