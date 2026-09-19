#include <Winsock2.h>
#include "dllmain.hpp"
#include "xlivelessness.hpp"

// #41140
tXLLNLogin XLLNLogin;
static uint32_t WINAPI iXLLNLogin(uint32_t user_index, BOOL lsb_8_live_enabled_8_online_disabled_msb_16_reserved, uint32_t user_id, const char* user_username)
{
	return ERROR_INVALID_FUNCTION;
}

// #41141
tXLLNLogout XLLNLogout;
static uint32_t WINAPI iXLLNLogout(uint32_t user_index)
{
	return ERROR_INVALID_FUNCTION;
}

// #41142
tXLLNModifyProperty XLLNModifyProperty;
static uint32_t WINAPI iXLLNModifyProperty(XLLNModifyPropertyTypes::TYPE property_id, void** new_value, void** old_value)
{
	return ERROR_INVALID_FUNCTION;
}

// #41143
tXLLNDebugLog XLLNDebugLog;
static uint32_t WINAPI iXLLNDebugLog(uint32_t log_level, const char* message)
{
	return ERROR_INVALID_FUNCTION;
}

// #41144
tXLLNDebugLogF XLLNDebugLogF;
static uint32_t WINAPI iXLLNDebugLogF(uint32_t log_level, const char* const message_format, ...)
{
	return ERROR_INVALID_FUNCTION;
}

// #41145
tXLLNGetXLLNStoragePath XLLNGetXLLNStoragePath;
static uint32_t WINAPI iXLLNGetXLLNStoragePath(uint32_t module_handle, uint32_t* result_local_instance_index, wchar_t* result_storage_path_buffer, size_t* result_storage_path_buffer_size)
{
	return ERROR_INVALID_FUNCTION;
}

// #41146
tXLLNSetBasePortOffsetMapping XLLNSetBasePortOffsetMapping;
static uint32_t WINAPI iXLLNSetBasePortOffsetMapping(uint8_t* port_offsets, uint16_t* port_originals, uint8_t port_mappings_size)
{
	return ERROR_INVALID_FUNCTION;
}

// #41147
tXLLNGetDebugLogLevel XLLNGetDebugLogLevel;
static uint32_t WINAPI iXLLNGetDebugLogLevel(uint32_t* log_level)
{
	return ERROR_INVALID_FUNCTION;
}

// #41148
tXllnDebugLogECodeF XllnDebugLogECodeF;
static uint32_t WINAPI iXllnDebugLogECodeF(uint32_t log_level, uint32_t error_code, const char* const message_format, ...)
{
	return ERROR_INVALID_FUNCTION;
}

bool InitXLiveLessNess()
{
	// Get XLLN exports.
	
	XLLNLogin = (tXLLNLogin)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41140);
	if (!XLLNLogin) {
		XLLNLogin = iXLLNLogin;
	}
	XLLNLogout = (tXLLNLogout)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41141);
	if (!XLLNLogout) {
		XLLNLogout = iXLLNLogout;
	}
	XLLNModifyProperty = (tXLLNModifyProperty)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41142);
	if (!XLLNModifyProperty) {
		XLLNModifyProperty = iXLLNModifyProperty;
	}
	XLLNDebugLog = (tXLLNDebugLog)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41143);
	if (!XLLNDebugLog) {
		XLLNDebugLog = iXLLNDebugLog;
	}
	XLLNDebugLogF = (tXLLNDebugLogF)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41144);
	if (!XLLNDebugLogF) {
		XLLNDebugLogF = iXLLNDebugLogF;
	}
	XLLNGetXLLNStoragePath = (tXLLNGetXLLNStoragePath)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41145);
	if (!XLLNGetXLLNStoragePath) {
		XLLNGetXLLNStoragePath = iXLLNGetXLLNStoragePath;
	}
	XLLNSetBasePortOffsetMapping = (tXLLNSetBasePortOffsetMapping)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41146);
	if (!XLLNSetBasePortOffsetMapping) {
		XLLNSetBasePortOffsetMapping = iXLLNSetBasePortOffsetMapping;
	}
	XLLNGetDebugLogLevel = (tXLLNGetDebugLogLevel)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41147);
	if (!XLLNGetDebugLogLevel) {
		XLLNGetDebugLogLevel = iXLLNGetDebugLogLevel;
	}
	XllnDebugLogECodeF = (tXllnDebugLogECodeF)GetProcAddress(xlln_hmod_xlivelessness, (PCSTR)41148);
	if (!XllnDebugLogECodeF) {
		XllnDebugLogECodeF = iXllnDebugLogECodeF;
	}
	
	return true;
}

bool UninitXLiveLessNess()
{
	
	return true;
}
