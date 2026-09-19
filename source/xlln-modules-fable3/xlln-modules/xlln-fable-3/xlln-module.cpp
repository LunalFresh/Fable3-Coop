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

static bool xmod_log_funcs_patched = false;
static size_t xmod_file_offset_to_rva = 0;

// File offsets (in the on-disk image) of Fable's three internal debug-logger stubs.
// Retail compiled these out to `ret`; this module resurrects them by redirecting the
// CALL sites to LogFuncType130/131/132 so Fable's own logging appears in the XLLN log.
// The offsets differ per Title version; they are assigned from the version in FindLogCalls.
//   type 130 = enter logger  (level, fmt, ...)  e.g. "bAllow %i"
//   type 131 = exit  logger  (fmt, ...)         e.g. "void. No link and not allowing connections"
//   type 132 = general logger (level, fmt, ...) e.g. "Dropping connect request packet 0x%p."
static size_t xmod_log_func_type_130_offset = INVALID_OFFSET;
static std::vector<uint8_t*> xmod_log_func_type_130;

static size_t xmod_log_func_type_131_offset = INVALID_OFFSET;
static std::vector<uint8_t*> xmod_log_func_type_131;

static size_t xmod_log_func_type_132_offset = INVALID_OFFSET;
static std::vector<uint8_t*> xmod_log_func_type_132;

// Co-op enable patches (expressed as RVAs = ghidra VA - 0x400000; applied at module base).
// Only valid for TITLE_VERSION::tTITLE_PATCH_2 (SHA-256 aac98798...).
//  - Allow-connections: CNetEndpoint::DispatchRecvPacket @VA 0x0155581f is
//      `JNZ process` (75) taken only when the endpoint's allow-connections flag (+0x1b8&1)
//      is set. Rewriting it to `JMP process` (EB) makes the host always run the packet-driven
//      accept path (CNetEndpoint::AcceptConnectRequest) for a peer with no existing link.
static const size_t xmod_coop_allow_connections_rva = 0x0115581f;
static const uint8_t xmod_coop_allow_connections_orig = 0x75; // JNZ rel8
static const uint8_t xmod_coop_allow_connections_new = 0xEB; // JMP rel8
//  - NLivePresence gate: FUN_00691310 @VA 0x00691716 is `JZ skip` (74 10) after
//      `CMP byte[ESI+0xb2],0`; NOPing it forces NLivePresence to start (which calls
//      CLivePresence::RegisterLinkListener -> SetAllowConnections(TRUE)) regardless of the
//      +0xb2 game-state flag. Still guarded by the +0xec "already started" check. Optional /
//      more aggressive: enable only if allow-connections alone is insufficient.
static bool xmod_coop_force_nlivepresence = false;
static const size_t xmod_coop_nlivepresence_gate_rva = 0x00291716;
static const uint8_t xmod_coop_nlivepresence_gate_orig[2] = {0x74, 0x10}; // JZ rel8
static const uint8_t xmod_coop_nlivepresence_gate_new[2] = {0x90, 0x90}; // NOP NOP

static void __cdecl LogFuncType130(uint32_t log_level, const char* const message_format, ...)
{
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return;
	}
	
	size_t messageSizeLength = messageLength + 1;
	char* message = new char[messageSizeLength];
	messageLength = _vsnprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return;
	}
	
	uint32_t newLogLevel = XLLN_LOG_CONTEXT_TITLE | XLLN_LOG_LEVEL_INFO;
	
	if (
		message[0] != 0
		&& !(message[0] == ' ' && message[1] == 0)
		// Enter logger: drop all "void*" messages (hot-path "void. m_pCurrentRecvPkt/SendPkt"
		// spam that fires per recv/send poll). Decisions come through types 131/132.
		&& !(message[0] == 'v' && message[1] == 'o' && message[2] == 'i' && message[3] == 'd')
	) {
		XLLN_DEBUG_LOG(newLogLevel, "130 0x%x: %s", log_level, message);
	}
	
	delete[] message;
	message = 0;
}

static void __cdecl LogFuncType131(const char* const message_format, ...)
{
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return;
	}
	
	size_t messageSizeLength = messageLength + 1;
	char* message = new char[messageSizeLength];
	messageLength = _vsnprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return;
	}
	
	uint32_t newLogLevel = XLLN_LOG_CONTEXT_TITLE | XLLN_LOG_LEVEL_INFO;
	
	if (
		message[0] != 0
		&& !(message[0] == ' ' && message[1] == 0)
		// Exit logger: drop the exact "void" spam (fires per poll) but KEEP "void. <reason>"
		// exit messages such as "void. No link and not allowing connections".
		&& !(message[0] == 'v' && message[1] == 'o' && message[2] == 'i' && message[3] == 'd' && message[4] == 0)
	) {
		XLLN_DEBUG_LOG(newLogLevel, "131: %s", message);
	}
	
	delete[] message;
	message = 0;
}

static void __cdecl LogFuncType132(uint32_t log_level, const char* const message_format, ...)
{
	va_list message_arguments;
	va_start(message_arguments, message_format);
	int messageLength = _vscprintf_p(message_format, message_arguments);
	
	if (messageLength < 0) {
		va_end(message_arguments);
		__debugbreak();
		return;
	}
	
	size_t messageSizeLength = messageLength + 1;
	char* message = new char[messageSizeLength];
	messageLength = _vsnprintf_s(message, messageSizeLength, _TRUNCATE, message_format, message_arguments);
	va_end(message_arguments);
	
	if (messageLength < 0) {
		delete[] message;
		message = 0;
		__debugbreak();
		return;
	}
	
	uint32_t newLogLevel = XLLN_LOG_CONTEXT_TITLE | XLLN_LOG_LEVEL_INFO;
	
	if (
		message[0] != 0
	) {
		XLLN_DEBUG_LOG(newLogLevel, "132 0x%x: %s", log_level, message);
	}
	
	delete[] message;
	message = 0;
}

static bool FindLogCalls()
{
	IMAGE_DOS_HEADER* imageDosHeader = (IMAGE_DOS_HEADER*)xlln_hmod_title;
	
	if (imageDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[XLLN-Fable-3] %s unable to find the loaded image's DOS header signature."
			, __func__
		);
		
		return false;
	}
	
	IMAGE_NT_HEADERS* imageNtHeaders = (IMAGE_NT_HEADERS*)((size_t)xlln_hmod_title + imageDosHeader->e_lfanew);
	
	if (imageNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[XLLN-Fable-3] %s unable to find the loaded image's NT header signature."
			, __func__
		);
		return false;
	}
	
	uint16_t imageSectionHeaderTableSectionCount = imageNtHeaders->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER* imageSectionHeaderTable = (IMAGE_SECTION_HEADER*)((size_t)xlln_hmod_title + imageDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	
	IMAGE_SECTION_HEADER* imageSectionHeaderText = 0;
	
	for (uint16_t iSection = 0; iSection < imageSectionHeaderTableSectionCount; iSection++) {
		if (strncmp((char*)imageSectionHeaderTable[iSection].Name, ".text", IMAGE_SIZEOF_SHORT_NAME) == 0) {
			imageSectionHeaderText = &imageSectionHeaderTable[iSection];
		}
	}
	
	if (!imageSectionHeaderText) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[XLLN-Fable-3] %s unable to find '.text' section header in binary file."
			, __func__
		);
		return false;
	}
	
	xmod_file_offset_to_rva = imageSectionHeaderText->VirtualAddress - imageSectionHeaderText->PointerToRawData;

	// Per-version file offsets of the three logger stubs (type 130/131/132). Same three
	// functions across versions, only shifted (SHIPPED: 0x1153549/54a/51; PATCH_2: +0xFC).
	if (title_version == TITLE_VERSION::tTITLE_SHIPPED) {
		xmod_log_func_type_130_offset = 0x01153549;
		xmod_log_func_type_131_offset = 0x0115354a;
		xmod_log_func_type_132_offset = 0x01153551;
	}
	else if (title_version == TITLE_VERSION::tTITLE_PATCH_2) {
		xmod_log_func_type_130_offset = 0x01153645;
		xmod_log_func_type_131_offset = 0x01153646;
		xmod_log_func_type_132_offset = 0x0115364d;
	}

	uint8_t patternSearch[] = {0xe8};
	size_t patternSearchSize = sizeof(patternSearch);
	
	const uint8_t* patternMatch = (const uint8_t*)((size_t)xlln_hmod_title + imageSectionHeaderText->VirtualAddress);
	size_t remainingSize = (size_t)imageSectionHeaderText->SizeOfRawData;
	while (1) {
		patternMatch = FindBytes(patternMatch, remainingSize, (const uint8_t*)patternSearch, patternSearchSize, true);
		if (!patternMatch) {
			break;
		}
		remainingSize = (size_t)imageSectionHeaderText->SizeOfRawData - ((size_t)patternMatch - ((size_t)xlln_hmod_title + imageSectionHeaderText->VirtualAddress));
		
		if (remainingSize < 5) {
			break;
		}
		
		size_t patternCallAddress = ((size_t)(&patternMatch[5]) + *(int32_t*)(&patternMatch[1]));
		size_t patternCallFileOffset = patternCallAddress - ((size_t)xlln_hmod_title + xmod_file_offset_to_rva);
		// if/else (not switch) because the offsets are runtime version-dependent values.
		if (patternCallFileOffset == xmod_log_func_type_130_offset) {
			xmod_log_func_type_130.push_back((uint8_t*)patternMatch);
		}
		else if (patternCallFileOffset == xmod_log_func_type_131_offset) {
			xmod_log_func_type_131.push_back((uint8_t*)patternMatch);
		}
		else if (patternCallFileOffset == xmod_log_func_type_132_offset) {
			xmod_log_func_type_132.push_back((uint8_t*)patternMatch);
		}
		
		patternMatch += 1;
		remainingSize += 1;
	}
	
	return true;
}

static bool xmod_coop_patched = false;

// Patch-2 CNetOldSystem::Update callers, checked against the original CALL bytes.
// Run the optional friends-panel bridge on the game's update thread after its tick.
typedef void (WINAPI* NativeKickCallback)(void*);
typedef uint32_t (WINAPI* SocialKickUpdate)(uint32_t, BOOL, void*, uintptr_t, NativeKickCallback);
static SocialKickUpdate social_kick_update = nullptr;
static bool social_kick_patched = false;
static const uint32_t social_kick_calls[] = {0x002a67a2,0x00291e68};
static const uint8_t social_kick_original[][5] = {
    {0xe8,0x69,0xab,0xfe,0xff},{0xe8,0xa3,0xf4,0xff,0xff}
};
static void WINAPI NativeKickGuest(void* object)
{
    // FUN_0068a720: ServerKickedPacket -> flush sends -> disconnect state 5.
    ((void (__thiscall*)(void*))GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2,0x0028a720))(object);
}
static void __fastcall SocialGameUpdate(uint8_t* object,void*)
{
    ((void (__thiscall*)(void*))GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2,0x00291310))(object);
    // Same host predicate as FUN_006835d0, plus a remote player/live link and no
    // disconnect or native kick confirmation already in progress.
    BOOL hostReady=object && object[0x3c] && object[0x3d] && !object[0x3e] &&
        *(uint32_t*)(object+0x94)!=0 && !object[0x9c] && !object[0x119] &&
        *(uint32_t*)(object+0x110)!=5;
    social_kick_update(1,hostReady,object,object?*(uintptr_t*)(object+0xf0):0,NativeKickGuest);
}
static void AddSocialKickPatch()
{
    if(title_version!=TITLE_VERSION::tTITLE_PATCH_2)return;
    social_kick_update=(SocialKickUpdate)GetProcAddress(xlln_hmod_xlivelessness,"XLLNSocialKickUpdate");
    if(!social_kick_update)return; // Older shims keep the proven co-op behavior.
    for(size_t i=0;i<2;++i) {
        auto address=GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2,social_kick_calls[i]);
        if(memcmp((void*)address,social_kick_original[i],5)) {
            XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE|XLLN_LOG_LEVEL_ERROR,
                "[XLLN-Fable-3] Kick bridge skipped: unexpected update call bytes.");
            return;
        }
    }
    for(auto rva:social_kick_calls)PatchCall(GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2,rva),(void*)SocialGameUpdate);
    social_kick_patched=true;
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE|XLLN_LOG_LEVEL_INFO,
        "[XLLN-Fable-3] Native host kick bridge installed (game update, Patch 2).");
}
static void RemoveSocialKickPatch()
{
    if(!social_kick_patched)return;
    for(size_t i=0;i<2;++i)WriteBytes(GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2,social_kick_calls[i]),(void*)social_kick_original[i],5);
    social_kick_update(1,FALSE,nullptr,0,nullptr);
    social_kick_patched=false;
}

// Apply the memory patches that let a Fable 3 host accept a joining co-op peer.
// These live here (in-memory, at the real module base) rather than as on-disk file-offset
// patches so they are ASLR-safe and version-gated. See the rva/opcode constants above.
static void AddCoopPatches()
{
	uint32_t allowConnAddr = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2, xmod_coop_allow_connections_rva);
	if (!allowConnAddr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
			, "[XLLN-Fable-3] %s co-op patches not applied (unsupported Title version)."
			, __func__
		);
		return;
	}

	// Verify the original opcode before writing so we never corrupt an unexpected build.
	if (*(uint8_t*)allowConnAddr != xmod_coop_allow_connections_orig) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[XLLN-Fable-3] %s allow-connections site 0x%08x byte 0x%02x != expected 0x%02x; skipping co-op patches."
			, __func__
			, allowConnAddr
			, *(uint8_t*)allowConnAddr
			, xmod_coop_allow_connections_orig
		);
		return;
	}

	WriteBytes(allowConnAddr, (LPVOID)&xmod_coop_allow_connections_new, 1);
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[XLLN-Fable-3] Applied allow-connections patch @0x%08x (JNZ->JMP): host now always runs the accept path for a joining peer."
		, allowConnAddr
	);
	xmod_coop_patched = true;

	// Optional / more aggressive: force NLivePresence to start (which enables incoming links
	// via CLivePresence::RegisterLinkListener -> SetAllowConnections(TRUE)) regardless of the
	// +0xb2 game-state flag. Off by default; the allow-connections patch above already bypasses
	// the gate at the transport layer. Enable only if that proves insufficient.
	if (xmod_coop_force_nlivepresence) {
		uint32_t gateAddr = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2, xmod_coop_nlivepresence_gate_rva);
		if (gateAddr
			&& ((uint8_t*)gateAddr)[0] == xmod_coop_nlivepresence_gate_orig[0]
			&& ((uint8_t*)gateAddr)[1] == xmod_coop_nlivepresence_gate_orig[1]
		) {
			WriteBytes(gateAddr, (LPVOID)&xmod_coop_nlivepresence_gate_new, 2);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "[XLLN-Fable-3] Applied NLivePresence-gate patch @0x%08x (JZ->NOP)."
				, gateAddr
			);
		}
	}
}

static void RemoveCoopPatches()
{
	if (!xmod_coop_patched) {
		return;
	}
	uint32_t allowConnAddr = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2, xmod_coop_allow_connections_rva);
	if (allowConnAddr) {
		WriteBytes(allowConnAddr, (LPVOID)&xmod_coop_allow_connections_orig, 1);
	}
	if (xmod_coop_force_nlivepresence) {
		uint32_t gateAddr = GetOffsetAddress(TITLE_VERSION::tTITLE_PATCH_2, xmod_coop_nlivepresence_gate_rva);
		if (gateAddr) {
			WriteBytes(gateAddr, (LPVOID)&xmod_coop_nlivepresence_gate_orig, 2);
		}
	}
	xmod_coop_patched = false;
}

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;

	AddCoopPatches();
	AddSocialKickPatch();

	xmod_log_funcs_patched = FindLogCalls();
	if (xmod_log_funcs_patched) {
		for (uint8_t* callInstruction : xmod_log_func_type_130) {
			PatchCall(callInstruction, LogFuncType130);
		}
		for (uint8_t* callInstruction : xmod_log_func_type_131) {
			PatchCall(callInstruction, LogFuncType131);
		}
		for (uint8_t* callInstruction : xmod_log_func_type_132) {
			PatchCall(callInstruction, LogFuncType132);
		}
	}
	
	return ERROR_SUCCESS;
}

static uint32_t RemovePatches()
{
	uint32_t patchAddress = 0;

	RemoveCoopPatches();
	RemoveSocialKickPatch();

	if (xmod_log_funcs_patched) {
		for (uint8_t* callInstruction : xmod_log_func_type_130) {
			PatchCall(callInstruction, (uint8_t*)((size_t)xlln_hmod_title + xmod_file_offset_to_rva + xmod_log_func_type_130_offset));
		}
		for (uint8_t* callInstruction : xmod_log_func_type_131) {
			PatchCall(callInstruction, (uint8_t*)((size_t)xlln_hmod_title + xmod_file_offset_to_rva + xmod_log_func_type_131_offset));
		}
		for (uint8_t* callInstruction : xmod_log_func_type_132) {
			PatchCall(callInstruction, (uint8_t*)((size_t)xlln_hmod_title + xmod_file_offset_to_rva + xmod_log_func_type_132_offset));
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
		
		if (_strcmpi(checksumTitle, "5a6dc0f02aae4fb2fd7b67662f3c9a65a6f1c446efb13d661f01730e0e01c914") == 0) {
			title_version = TITLE_VERSION::tTITLE_SHIPPED;
		}
		else if (_strcmpi(checksumTitle, "aac98798e77ca32282d663af54fc185c3b5256b5317ed8568d7f12a0ef42da42") == 0) {
			// `Fable3.exe` version 10000283 Patch 2 (the build analysed for the co-op work).
			title_version = TITLE_VERSION::tTITLE_PATCH_2;
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
