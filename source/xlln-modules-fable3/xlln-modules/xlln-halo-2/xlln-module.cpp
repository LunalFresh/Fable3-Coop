#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include <stdint.h>
#include "../utils/util-hook.hpp"
#include "../utils/util-checksum.hpp"

#pragma region GetOffsetAddress

#define TITLEMODULEANY 0xFFFFFFFF
#define INVALID_OFFSET 0xFFFFFFFF
#define CLIENT_11081 1
#define CLIENT_11091 2
#define CLIENT_11122 3
#define SERVER_11081 4
#define SERVER_11091 5
#define SERVER_11122 6
static uint32_t halo2_version = 0;

static uint32_t GetOffsetAddress(uint32_t version, uint32_t offset)
{
	if (offset == INVALID_OFFSET || (halo2_version != version && version != TITLEMODULEANY)) {
		return 0;
	}
	return (uint32_t)xlln_hmod_title + offset;
}
static uint32_t GetOffsetAddressC(uint32_t offsetC_11081, uint32_t offsetC_11091, uint32_t offsetC_11122)
{
	switch (halo2_version) {
	case CLIENT_11081:
		return offsetC_11081 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11081);
	case CLIENT_11091:
		return offsetC_11091 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11091);
	case CLIENT_11122:
		return offsetC_11122 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11122);
	}
	return 0;
}
static uint32_t GetOffsetAddressS(uint32_t offsetS_11081, uint32_t offsetS_11091, uint32_t offsetS_11122)
{
	switch (halo2_version) {
	case SERVER_11081:
		return offsetS_11081 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11081);
	case SERVER_11091:
		return offsetS_11091 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11091);
	case SERVER_11122:
		return offsetS_11122 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11122);
	}
	return 0;
}
static uint32_t GetOffsetAddress(uint32_t offsetC_11081, uint32_t offsetC_11091, uint32_t offsetC_11122, uint32_t offsetS_11081, uint32_t offsetS_11091, uint32_t offsetS_11122)
{
	switch (halo2_version) {
	case CLIENT_11081:
		return offsetC_11081 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11081);
	case CLIENT_11091:
		return offsetC_11091 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11091);
	case CLIENT_11122:
		return offsetC_11122 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetC_11122);
	case SERVER_11081:
		return offsetS_11081 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11081);
	case SERVER_11091:
		return offsetS_11091 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11091);
	case SERVER_11122:
		return offsetS_11122 == INVALID_OFFSET ? 0 : ((uint32_t)xlln_hmod_title + offsetS_11122);
	}
	return 0;
}

#pragma endregion

#pragma region Hooks

static DWORD Import_XLivePBufferSetByte = 0;
static tXLivePBufferSetByte DetourXLivePBufferSetByte = NULL;
static HRESULT WINAPI HookXLivePBufferSetByte(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t value)
{
	value = 0;
	HRESULT result = DetourXLivePBufferSetByte(protected_buffer, protected_buffer_offset, value);
	return result;
}

struct filo
{
	unsigned long		signature;
	unsigned short		flags;
	signed short		location;
	char				path[256];
	HANDLE				handle;
	HRESULT				api_result;
};

static void DuplicateDataBlob(DATA_BLOB *pDataIn, DATA_BLOB *pDataOut)
{
	pDataOut->cbData = pDataIn->cbData;
	pDataOut->pbData = static_cast<BYTE*>(LocalAlloc(LMEM_FIXED, pDataIn->cbData));
	CopyMemory(pDataOut->pbData, pDataIn->pbData, pDataIn->cbData);
}

// #1221 CryptProtectData
typedef BOOL(WINAPI *tCryptProtectData)(
	_In_ DATA_BLOB *pDataIn,
	_In_opt_ LPCWSTR szDataDescr,
	_In_opt_ DATA_BLOB *pOptionalEntropy,
	_Reserved_ PVOID pvReserved,
	_In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_ DWORD dwFlags,
	_Out_ DATA_BLOB *pDataOut
);
static DWORD Import_CryptProtectData = 0;
static tCryptProtectData DetourCryptProtectData = NULL;
static BOOL WINAPI HookCryptProtectData(
	_In_ DATA_BLOB *pDataIn,
	_In_opt_ LPCWSTR szDataDescr,
	_In_opt_ DATA_BLOB *pOptionalEntropy,
	_Reserved_ PVOID pvReserved,
	_In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_ DWORD dwFlags,
	_Out_ DATA_BLOB *pDataOut
)
{
	BOOL result = TRUE;
	
	DuplicateDataBlob(pDataIn, pDataOut);
	
	//result = DetourCryptProtectData(pDataIn, szDataDescr, pOptionalEntropy, pvReserved, pPromptStruct, dwFlags, pDataOut);
	
	return result;
}

// #1251 CryptUnprotectData
typedef BOOL(WINAPI *tCryptUnprotectData)(
	_In_ DATA_BLOB *pDataIn,
	_Out_opt_ LPWSTR *ppszDataDescr,
	_In_opt_ DATA_BLOB *pOptionalEntropy,
	_Reserved_ PVOID pvReserved,
	_In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_ DWORD dwFlags,
	_Out_ DATA_BLOB *pDataOut
);
static DWORD Import_CryptUnprotectData = 0;
static tCryptUnprotectData DetourCryptUnprotectData = NULL;
static BOOL WINAPI HookCryptUnprotectData(
	_In_ DATA_BLOB *pDataIn,
	_Out_opt_ LPWSTR *ppszDataDescr,
	_In_opt_ DATA_BLOB *pOptionalEntropy,
	_Reserved_ PVOID pvReserved,
	_In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_ DWORD dwFlags,
	_Out_ DATA_BLOB *pDataOut
)
{
	BOOL result = DetourCryptUnprotectData(pDataIn, ppszDataDescr, pOptionalEntropy, pvReserved, pPromptStruct, dwFlags, pDataOut);
	if (!result) {
		// If decrypting the data fails just assume it is already unencrypted.
		DuplicateDataBlob(pDataIn, pDataOut);
		result = TRUE;
	}
	
	return result;
}

static char(*DetourWriteEncryptedData)(filo *file_ptr, DWORD nNumberOfBytesToWrite, LPVOID lpBuffer) = NULL;
static char hook_filo_write_encrypted_data(filo *file_ptr, DWORD nNumberOfBytesToWrite, LPVOID lpBuffer)
{
	DWORD file_size = GetFileSize(file_ptr->handle, NULL);
	
	// Clear the file since unencrypted data is shorter than encrypted data.
	if (file_size > nNumberOfBytesToWrite) {
		if (file_ptr->handle) {
			if (SetFilePointer(file_ptr->handle, 0, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
				SetEndOfFile(file_ptr->handle);
			}
		}
		else {
			SetLastError(ERROR_INVALID_HANDLE);
		}
	}
	char result = DetourWriteEncryptedData(file_ptr, nNumberOfBytesToWrite, lpBuffer);
	return result;
}

#pragma endregion

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;
	
	// Allow multiple instances of the game.
	if (patchAddress = GetOffsetAddressC(0x0039bce8, 0x0039bce8, 0x0039bcf0)) {
		// Patch to an empty string: "Global\\Halo2AlreadyRunning".
		WriteBytes(patchAddress, L"", 2);
	}
	
	// Allows on a remote desktop connection.
	if (patchAddress = GetOffsetAddressC(0x00039e7f, 0x00039ea2, 0x00039ea2)) {
		// MOV 32-bit value into EAX.
		WriteValue<BYTE>(patchAddress, 0xB8);
		WriteValue(patchAddress + 1, 0);
		// RETN
		WriteValue<BYTE>(patchAddress + 5, 0xC3);
	}
	
	// Disables the ESRB warning (only occurs for English language).
	if (patchAddress = GetOffsetAddressC(0x0040e030, 0x00411030, 0x00411030)) {
		// Disables the one if no intro video occurs, by changing a global variable value.
		WriteValue<BYTE>(patchAddress, 0);
	}
	if (patchAddress = GetOffsetAddressC(0x00039925, 0x00039948, 0x00039948)) {
		// Disables the one after the intro video, by removing 0x40 flag from 0x7C6 bitmask.
		WriteValue(patchAddress + 2, 0x7C6 & ~0x40);
	}
	
	// Set the LAN Server List Ping Frequency (milliseconds).
	if (patchAddress = GetOffsetAddressC(0x001e9898, 0x001e9a38, 0x001e9a88)) {
		// From 1500 to 3000 milliseconds.
		WriteValue(patchAddress + 1, 3000);
	}
	// Set the LAN Server List Delete Entry After (milliseconds).
	if (patchAddress = GetOffsetAddressC(0x001e9919, 0x001e9ab9, 0x001e9b09)) {
		// From 2000 to 9000 milliseconds.
		WriteValue(patchAddress + 1, 9000);
	}
	
	// Show game details menu in NETWORK serverlist too.
	if (patchAddress = GetOffsetAddressC(0x0021988d, 0x00219c1d, 0x00219d6d)) {
		// NOP a JZ.
		NopFill(patchAddress, 2);
	}
	
	// Disable part of custom map tag verification.
	if (
		(patchAddress = GetOffsetAddress(CLIENT_11081, 0x0004f8ca))
		|| (patchAddress = GetOffsetAddress(CLIENT_11091, 0x0004fa0a))
		|| (patchAddress = GetOffsetAddress(CLIENT_11122, 0x0004fa0a))
		|| (patchAddress = GetOffsetAddress(SERVER_11081, 0x0005696a))
		|| (patchAddress = GetOffsetAddress(SERVER_11091, 0x00056bba))
		|| (patchAddress = GetOffsetAddress(SERVER_11122, 0x00056c0a))
	) {
		NopFill(patchAddress, 6);
	}
	
	// Allow custom map downloading in network lobby.
	if (patchAddress = GetOffsetAddressC(0x0021561e, 0x0021594e, 0x00215a9e)) {
		// Write JMP instruction instead of JZ.
		WriteValue<BYTE>(patchAddress, 0xEB);
	}
	if (patchAddress = GetOffsetAddress(0x001e56db, 0x001e587b, 0x001e58cb, 0x001b6ffe, 0x001b726e, 0x001b72de)) {
		// NOP a JNZ.
		NopFill(patchAddress, 2);
	}
	
	// Allow downloading custom maps when game is in progress. Otherwise you will get match has already begun error.
	if (patchAddress = GetOffsetAddressC(0x00215649, 0x00215979, 0x00215ac9)) {
		// Write JMP instruction instead of JZ.
		WriteValue<BYTE>(patchAddress, 0xEB);
	}
	
	// Better windowed mode experience which allows a resizable and maximisable window.
	if (patchAddress = GetOffsetAddressC(0x0025d4bb, 0x0025e00b, 0x0025e15b)) {
		// MOV 32-bit value into EAX.
		WriteValue<BYTE>(patchAddress, 0xB8);
		WriteValue(patchAddress + 1, WS_DLGFRAME | WS_BORDER | WS_CAPTION | WS_TABSTOP | WS_GROUP | WS_SIZEBOX | WS_SYSMENU);
		NopFill(patchAddress + 5, 12);
	}
	
	// Disables profiles/game saves encryption.
	{
		// Hook imports.
		PE_HOOK_ARG pe_hack[1];
		DWORD ordinal_addrs[2];
		WORD ordinals[2] = { 0, 0 };
		const char* ordinalNames[2] = { "CryptProtectData", "CryptUnprotectData" };
		pe_hack->ordinals_len = 2;
		pe_hack->ordinals = ordinals;
		pe_hack->ordinal_names = ordinalNames;
		pe_hack->ordinal_addrs = ordinal_addrs;
		pe_hack->pe_name = "crypt32.dll";
		pe_hack->pe_err = ERROR_FUNCTION_FAILED;
		
		if (
			PEImportHack(xlln_hmod_title, pe_hack, 1) == ERROR_SUCCESS
			&& pe_hack->pe_err == ERROR_SUCCESS
			&& (patchAddress = GetOffsetAddress(0x0009af3f, 0x0009b08f, 0x0009b09f, 0x00085cb3, 0x00085f23, 0x00085f73))
		) {
			HookImport(&Import_CryptProtectData, &DetourCryptProtectData, HookCryptProtectData, ordinal_addrs[0]);
			HookImport(&Import_CryptUnprotectData, &DetourCryptUnprotectData, HookCryptUnprotectData, ordinal_addrs[1]);
			HookCall(patchAddress, (uint32_t)hook_filo_write_encrypted_data, (uint32_t*)&DetourWriteEncryptedData);
		}
	}
	
	// Easiest way to overcome the licensing/activated checks.
	// The other way is to deobfuscate and patch the various functions responsible in the checks which has not been done in full yet.
	{
		// Hook import.
		PE_HOOK_ARG pe_hack[1];
		DWORD ordinal_addrs[1];
		WORD ordinals[1] = { 5019 };
		const char* ordinalNames[1] = { "XLivePBufferSetByte" };
		pe_hack->ordinals_len = 1;
		pe_hack->ordinals = ordinals;
		pe_hack->ordinal_names = ordinalNames;
		pe_hack->ordinal_addrs = ordinal_addrs;
		pe_hack->pe_name = "xlive.dll";
		pe_hack->pe_err = ERROR_FUNCTION_FAILED;
		
		if (PEImportHack(xlln_hmod_title, pe_hack, 1) == ERROR_SUCCESS) {
			HookImport(&Import_XLivePBufferSetByte, &DetourXLivePBufferSetByte, HookXLivePBufferSetByte, ordinal_addrs[0]);
		}
	}
	
	return ERROR_SUCCESS;
}

static uint32_t RemovePatches()
{
	uint32_t patchAddress = 0;
	
	// Undo hooks.
	HookImport(&Import_CryptProtectData, &DetourCryptProtectData, HookCryptProtectData, 0);
	HookImport(&Import_CryptUnprotectData, &DetourCryptUnprotectData, HookCryptUnprotectData, 0);
	if (
		DetourWriteEncryptedData
		&& (patchAddress = GetOffsetAddress(0x0009af3f, 0x0009b08f, 0x0009b09f, 0x00085cb3, 0x00085f23, 0x00085f73))
	) {
		HookCall(patchAddress, (uint32_t)DetourWriteEncryptedData, 0);
	}
	HookImport(&Import_XLivePBufferSetByte, &DetourXLivePBufferSetByte, HookXLivePBufferSetByte, 0);
	
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
	char *checksumTitle = GetPESha256FlagFix(xlln_hmod_title);
	if (!checksumTitle) {
		return false;
	}
	
	if (_strcmpi(checksumTitle, "0bc0e90042fbdd6aa6848ecbac621f2617f1bd5ac49f9d4f4cb6d87d9fa70a2c") == 0) {
		halo2_version = CLIENT_11081;
	}
	else if (_strcmpi(checksumTitle, "8297d21585f46de5f88269c0a27c8bfedd13345fb0573720968277ca5019f24e") == 0) {
		halo2_version = CLIENT_11091;
	}
	else if (_strcmpi(checksumTitle, "33119398d3f68ad569c5010b652039105d3ce1b6512c418508e49ce69309f109") == 0) {
		halo2_version = CLIENT_11122;
	}
	else if (_strcmpi(checksumTitle, "e9388d479bceec1484def89acf664701d8b9404b79a31f4a3984b7ba79f0a07f") == 0) {
		halo2_version = SERVER_11081;
	}
	else if (_strcmpi(checksumTitle, "850d84c5eef21ffc1f31f3c890e0fe741bed55ed8b66557c4b8839b01404c599") == 0) {
		halo2_version = SERVER_11091;
	}
	else if (_strcmpi(checksumTitle, "422556f7a99ecde2e2cbb8fdda45e1c28b583d8f829d00ab24419213985e5a3e") == 0) {
		halo2_version = SERVER_11122;
	}
	// Glitchy Scripts' deobfuscated halo2.exe version 2.
	else if (_strcmpi(checksumTitle, "e2b8f259170182921a6922f8be3c4be3c9ae75b9f8530563b4c209e55d0c0aec") == 0) {
		halo2_version = CLIENT_11122;
	}
	// Glitchy Scripts' deobfuscated h2server.exe version 2.
	else if (_strcmpi(checksumTitle, "5775d9607132f38a4190be445ae287cbdd87c5ea617cac6a7d1c42294432e6cb") == 0) {
		halo2_version = SERVER_11122;
	}
	
	free(checksumTitle);
	
	if (!halo2_version) {
		return false;
	}
	
	return true;
}

bool UninitXllnModule()
{
	return true;
}
