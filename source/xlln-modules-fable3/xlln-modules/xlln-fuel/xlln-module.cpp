#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "../utils/util-checksum.hpp"
#include <tchar.h>
#include <vector>

TITLE_VERSION::TYPE title_version = TITLE_VERSION::tUNKNOWN;

static uint32_t GetOffsetAddress(TITLE_VERSION::TYPE version, uint32_t offset)
{
	if (offset == INVALID_OFFSET || (title_version != version && version != TITLEMODULEANY)) {
		return 0;
	}
	return (uint32_t)xlln_hmod_title + offset;
}

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;
	
	// Change the function that checks if the launcher is running and has verified to always return true.
	if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x004443c0)) {
		//xor eax, eax
		//mov al, 1
		//retn
		uint8_t assemblyInstructions[] = { 0x33, 0xC0, 0xB0, 0x01, 0xC3 };
		WriteBytes(patchAddress, assemblyInstructions, sizeof(assemblyInstructions));
	}
	
	// Allow multiple instances of the game.
	if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x00443DF0)) {
		//xor eax, eax
		//ret
		//nop
		//nop
		uint8_t assemblyInstructions[] = { 0x33, 0xC0, 0xC3, 0x90, 0x90 };
		WriteBytes(patchAddress, assemblyInstructions, sizeof(assemblyInstructions));
	}
	
	bool xliveXhvEngineEnabled = false;
	if (XLLNModifyProperty && XLLNModifyProperty(XLLNModifyPropertyTypes::tXHV_ENGINE_ENABLED, 0, (void**)&xliveXhvEngineEnabled) == ERROR_SUCCESS && !xliveXhvEngineEnabled) {
		// Change the function that assumes there will be voice to always skip voice initialization.
		if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x00475040)) {
			//ret
			uint8_t assemblyInstructions[] = { 0xC3 };
			WriteBytes(patchAddress, assemblyInstructions, sizeof(assemblyInstructions));
		}
	}
	
	// Skip the auto-save warning screen
	if (patchAddress = GetOffsetAddress(TITLE_VERSION::tTITLE_SHIPPED, 0x00194A17)) {
		NopFill(patchAddress, 11);
	}
	
	return ERROR_SUCCESS;
}

static uint32_t RemovePatches()
{
	uint32_t patchAddress = 0;
	
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
		
		if (_strcmpi(checksumTitle, "ac1b2077137b7c6299c344111857b032635fe3d4794bc5135dad7c35feeda856") == 0) {
			title_version = TITLE_VERSION::tTITLE_SHIPPED;
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
