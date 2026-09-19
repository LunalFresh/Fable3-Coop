#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "../utils/util-checksum.hpp"
#include <stdint.h>

TITLE_VERSION::TYPE title_version = TITLE_VERSION::tUNKNOWN;

static uint32_t GetOffsetAddress(TITLE_VERSION::TYPE version, uint32_t offset)
{
	if (offset == INVALID_OFFSET || (title_version != version && version != TITLEMODULEANY)) {
		return 0;
	}
	return (uint32_t)xlln_hmod_title + offset;
}
static uint32_t GetOffsetAddress(uint32_t offset_shipped, uint32_t offset_patch_1, uint32_t offset_patch_2, uint32_t offset_patch_3)
{
	switch (title_version) {
		case TITLE_VERSION::tTITLE_SHIPPED:
			return offset_shipped == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offset_shipped);
		case TITLE_VERSION::tTITLE_PATCH_1:
			return offset_patch_1 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offset_patch_1);
		case TITLE_VERSION::tTITLE_PATCH_2:
			return offset_patch_2 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offset_patch_2);
		case TITLE_VERSION::tTITLE_PATCH_3:
			return offset_patch_3 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offset_patch_3);
	}
	return 0;
}

typedef void(__stdcall *tLoggingHook)(uint32_t, char*, char*);
tLoggingHook pLoggingHook = 0;
static void __stdcall LoggingHook(uint32_t log_level, char* log_title, char* log_msg)
{
	uint32_t logLevel = XLLN_LOG_CONTEXT_TITLE;
	
	switch (log_level) {
		case 0: {
			logLevel |= XLLN_LOG_LEVEL_TRACE;
			break;
		}
		case 1: {
			logLevel |= XLLN_LOG_LEVEL_DEBUG;
			break;
		}
		case 2: {
			logLevel |= XLLN_LOG_LEVEL_INFO;
			break;
		}
		case 3: {
			logLevel |= XLLN_LOG_LEVEL_WARN;
			break;
		}
		case 4: {
			logLevel |= XLLN_LOG_LEVEL_ERROR;
			break;
		}
		default: {
			logLevel |= XLLN_LOG_LEVEL_FATAL;
			break;
		}
	}
	
	XLLN_DEBUG_LOG(logLevel, "0x%x %s %s", log_level, log_title, log_msg);
	
	pLoggingHook(log_level, log_title, log_msg);
}

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;
	DWORD dwBack;
	
	// Improve existing windowed mode.
	if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x0000603f + 7)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x00005f7b + 7))) {
		WriteValue<uint32_t>(patchAddress, WS_DLGFRAME | WS_BORDER | WS_CAPTION | WS_TABSTOP | WS_GROUP | WS_SIZEBOX | WS_SYSMENU);
	}
	//if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x002ff01a + 1)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0030d00a + 1))) {
	//	WriteValue<uint32_t>(patchAddress, WS_DLGFRAME | WS_BORDER | WS_CAPTION | WS_TABSTOP | WS_GROUP | WS_SIZEBOX | WS_SYSMENU);
	//}
	if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x002ff048 + 1)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0030d038 + 1))) {
		WriteValue<uint32_t>(patchAddress, WS_DLGFRAME | WS_BORDER | WS_CAPTION | WS_TABSTOP | WS_GROUP | WS_SIZEBOX | WS_SYSMENU);
	}
	
	// Change the function that verifies the given data with the cryptographic signature to always return true.
	if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x00087730)) {
		//xor eax, eax
		//mov al, 1
		//retn
		uint8_t assemblyInstructions[] = { 0x33, 0xC0, 0xB0, 0x01, 0xC3 };
		WriteBytes(patchAddress, assemblyInstructions, sizeof(assemblyInstructions));
		NopFill(patchAddress + sizeof(assemblyInstructions), 2);
	}
	
	// NOP out conditional jump that would signify data does not match cryptographic signature.
	//if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0008778b)) {
	//	NopFill(patchAddress, 2);
	//}
	
	// Patch out checksum verification.
	//if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x00098544)) {
	//	WriteValue<uint8_t>(patchAddress, 0xEB);
	//}
	
	// If debug mode is active then enable this logger.
	uint32_t xllnDebugLogLevel = 0;
	if (XLLNGetDebugLogLevel(&xllnDebugLogLevel) == ERROR_SUCCESS) {
		// Hook native logging function.
		if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x0026e840)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0027b590))) {
			pLoggingHook = (tLoggingHook)DetourFunc((uint8_t*)patchAddress, (uint8_t*)LoggingHook, 7);
			VirtualProtect(pLoggingHook, 4, PAGE_EXECUTE_READWRITE, &dwBack);
		}
		// Enable all levels of logging.
		if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x0022689a + 6)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0023340a + 6))) {
			WriteValue<uint32_t>(patchAddress, 0);
		}
	}
	
	return ERROR_SUCCESS;
}

static uint32_t RemovePatches()
{
	uint32_t patchAddress = 0;
	
	if (pLoggingHook) {
		if ((patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x0026e840)) || (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_3, 0x0027b590))) {
			RetourFunc((uint8_t*)patchAddress, (uint8_t*)pLoggingHook, 7);
			free(pLoggingHook);
			pLoggingHook = 0;
		}
	}
	
	return ERROR_SUCCESS;
}

// #41101
uint32_t WINAPI XLLNModulePostInit()
{
	uint32_t result = ERROR_SUCCESS;
	
	AddPatches();
	
	return result;
}

// #41102
uint32_t WINAPI XLLNModulePreUninit()
{
	uint32_t result = ERROR_SUCCESS;
	
	RemovePatches();
	
	return result;
}

bool InitXllnModule()
{
	{
		char *checksumTitle = GetPESha256FlagFix(xlln_hmod_title);
		if (!checksumTitle) {
			return false;
		}
		
		if (_strcmpi(checksumTitle, "360a7a55194b52343113dd9a42a89d16ec82c61e0ceb390f569f0cde090e5c52") == 0) {
			title_version = TITLE_VERSION::tTITLE_SHIPPED;
		}
		else if (_strcmpi(checksumTitle, "00000000") == 0) {
			title_version = TITLE_VERSION::tTITLE_PATCH_1;
		}
		else if (_strcmpi(checksumTitle, "00000000") == 0) {
			title_version = TITLE_VERSION::tTITLE_PATCH_2;
		}
		else if (_strcmpi(checksumTitle, "c03c4e4e5b04ed972b013b5a124c8e37b339b5c58a1ae82ff1f2c0df95e6d567") == 0) {
			title_version = TITLE_VERSION::tTITLE_PATCH_3;
		}
		
		free(checksumTitle);
		
		if (title_version == TITLE_VERSION::tUNKNOWN) {
			return false;
		}
	}
	
	return true;
}

bool UninitXllnModule()
{
	return true;
}
