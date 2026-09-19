#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "../utils/util-checksum.hpp"
#include <stdint.h>

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;
	
	// Allow the game to run without startup.exe.
	// Change to JMP, disregard the function's result.
	WriteValue<BYTE>((DWORD)xlln_hmod_title + 0x00d23581, 0xEB);
	
	// Check DLL integrity function.
	// Replace the XOR and following JMP with a jump to higher up in the function where it returns 1 (success condition).
	PatchWithJump((DWORD)xlln_hmod_title + 0x00b78825, (DWORD)xlln_hmod_title + 0x00b787a4);
	
	// Allow modifying config files by bypassing the SHA Verification function check result to always return success.
	// Set EAX to 0: XOR EAX, EAX.
	WriteValue<uint16_t>((uint32_t)xlln_hmod_title + 0x00bf82fe + 0, 0xC033);
	// Set EDI to 0: XOR EDI, EDI.
	WriteValue<uint16_t>((uint32_t)xlln_hmod_title + 0x00bf82fe + 2, 0xFF33);
	// NOP the remaining instructions.
	WriteValue<uint32_t>((uint32_t)xlln_hmod_title + 0x00bf82fe + 4, 0x90909090);
	
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
	
	if (XLLNSetBasePortOffsetMapping) {
		uint8_t portOffsets[]{ 1,2,3,4,5,6 };
		uint16_t portOriginals[]{ 7777,8777,9777,13000,14000,14001 };
		XLLNSetBasePortOffsetMapping(portOffsets, portOriginals, 6);
	}
	
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
		
		bool titleImageSha256Matches = _strcmpi(checksumTitle, "968b7fa90f8579fed97d81d9dac7073fbe1cfabe519fb2af1f77fc499c01279e") == 0;
		
		free(checksumTitle);
		
		if (!titleImageSha256Matches) {
			return false;
		}
	}
	
	return true;
}

bool UninitXllnModule()
{
	return true;
}
