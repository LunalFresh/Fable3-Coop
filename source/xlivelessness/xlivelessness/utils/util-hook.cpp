#include <Windows.h>
#include "util-hook.hpp"


void WriteBytes(DWORD destAddress, LPVOID bytesToWrite, int numBytes)
{
	DWORD OldProtection;
	DWORD temp;

	VirtualProtect((LPVOID)destAddress, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
	memcpy((LPVOID)destAddress, bytesToWrite, numBytes);
	VirtualProtect((LPVOID)destAddress, numBytes, OldProtection, &temp); //quick fix for exception that happens here
}

void WritePointer(DWORD offset, void *ptr)
{
	BYTE* pbyte = (BYTE*)&ptr;
	BYTE assmNewFuncRel[0x4] = { pbyte[0], pbyte[1], pbyte[2], pbyte[3] };
	WriteBytes(offset, assmNewFuncRel, 0x4);
}

void WriteValue(DWORD offset, DWORD value)
{
	/*uint32_t byteSwap =
		((value >> 24) & 0xff)
		| ((value << 8) & 0xff0000)
		| ((value >> 8) & 0xff00)
		| ((value << 24) & 0xff000000);*/
	WriteBytes(offset, &value, 0x4);
}

void CodeCave(uint8_t *src_fn, void(*cave_fn)(), const int extra_nop_count)
{
	DWORD dwBack;
	uint8_t *codecave = (uint8_t*)malloc(1 + 1 + 5 + 1 + 1 + (5 + extra_nop_count) + 5);

	VirtualProtect(src_fn, 5 + extra_nop_count, PAGE_READWRITE, &dwBack);

	codecave[0] = 0x60; // PUSHAD
	codecave[1] = 0x9C; // PUSHFD
	codecave[1 + 1] = 0xE8; // CALL ...
	*(uint32_t*)&codecave[1 + 1 + 1] = (uint32_t)((uint8_t*)cave_fn - &codecave[1 + 1]) - 5;
	codecave[1 + 1 + 5] = 0x9D; // POPFD
	codecave[1 + 1 + 5 + 1] = 0x61; // POPAD
	memcpy(&codecave[1 + 1 + 5 + 1 + 1], src_fn, 5 + extra_nop_count);
	codecave[1 + 1 + 5 + 1 + 1 + (5 + extra_nop_count)] = 0xE9; // JMP ...
	*(uint32_t*)&codecave[1 + 1 + 5 + 1 + 1 + (5 + extra_nop_count) + 1] = (uint32_t)(&src_fn[5 + extra_nop_count] - &codecave[1 + 1 + 5 + 1 + 1 + (5 + extra_nop_count)]) - 5;

	src_fn[0] = 0xE9;
	*(uint32_t*)(&src_fn[1]) = (uint32_t)(codecave - src_fn) - 5;

	for (int i = 5; i < (5 + extra_nop_count); i++) {
		src_fn[i] = 0x90; // NOP
	}

	VirtualProtect(src_fn, 5 + extra_nop_count, dwBack, &dwBack);

	VirtualProtect(codecave, 1 + 1 + 5 + 1 + 1 + (5 + extra_nop_count) + 5, PAGE_EXECUTE_READWRITE, &dwBack);
}

void *DetourFunc(BYTE *src, const BYTE *dst, const int len)
{
	DWORD dwBack;
	
	BYTE *jmp = (BYTE*)malloc(len + 5);
	VirtualProtect(jmp, len + 5, PAGE_EXECUTE_READWRITE, &dwBack);
	
	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	
	memcpy(jmp, src, len);
	jmp += len;
	
	jmp[0] = 0xE9;
	*(DWORD*)(jmp + 1) = (DWORD)(src + len - jmp) - 5;
	
	src[0] = 0xE9;
	*(DWORD*)(src + 1) = (DWORD)(dst - src) - 5;
	
	for (int i = 5; i < len; i++) {
		src[i] = 0x90;
	}
	
	VirtualProtect(src, len, dwBack, &dwBack);
	
	return (jmp - len);
}

void RetourFunc(BYTE *src, BYTE *restore, const int len)
{
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(src, restore, len);

	restore[0] = 0xE9;
	*(DWORD*)(restore + 1) = (DWORD)(src - restore) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);
}

void *DetourClassFunc(BYTE *src, const BYTE *dst, const int len)
{
	DWORD dwBack;
	
	BYTE *jmp = (BYTE*)malloc(len + 8);
	VirtualProtect(jmp, len + 8, PAGE_EXECUTE_READWRITE, &dwBack);
	
	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(jmp + 3, src, len);
	
	// calculate callback function call
	jmp[0] = 0x58;							// pop eax
	jmp[1] = 0x59;							// pop ecx
	jmp[2] = 0x50;							// push eax
	jmp[len + 3] = 0xE9;						// jmp
	*(DWORD*)(jmp + len + 4) = (DWORD)((src + len) - (jmp + len + 3)) - 5;
	
	// detour source function call
	src[0] = 0x58;							// pop eax;
	src[1] = 0x51;							// push ecx
	src[2] = 0x50;							// push eax
	src[3] = 0xE9;							// jmp
	*(DWORD*)(src + 4) = (DWORD)(dst - (src + 3)) - 5;
	
	for (int i = 8; i < len; i++) {
		src[i] = 0x90;
	}
	
	VirtualProtect(src, len, dwBack, &dwBack);
	
	return jmp;
}

void RetourClassFunc(BYTE *src, BYTE *restore, const int len)
{
	DWORD dwBack;

	VirtualProtect(src, len, PAGE_READWRITE, &dwBack);
	memcpy(src, restore + 3, len);

	restore[3] = 0xE9;
	*(DWORD*)(restore + 4) = (DWORD)(src - (restore + 3)) - 5;

	VirtualProtect(src, len, dwBack, &dwBack);
}

void PatchCall(DWORD call_addr, DWORD new_function_ptr)
{
	DWORD callRelative = new_function_ptr - (call_addr + 5);
	WritePointer(call_addr + 1, reinterpret_cast<void*>(callRelative));
}

void HookCall(uint32_t call_addr, uint32_t new_function_ptr, uint32_t *original_func_ptr)
{
	if (original_func_ptr) {
		uint32_t instrAddr = *(uint32_t*)(call_addr + 1);
		*original_func_ptr = instrAddr + (call_addr + 5);
	}

	uint32_t callRelative = new_function_ptr - (call_addr + 5);
	WritePointer(call_addr + 1, reinterpret_cast<void*>(callRelative));
}

void PatchWinAPICall(DWORD call_addr, DWORD new_function_ptr)
{
	BYTE call = 0xE8;
	WriteValue(call_addr, call);

	PatchCall(call_addr, new_function_ptr);

	// pad the extra unused byte
	BYTE padding = 0x90;
	WriteValue(call_addr + 5, padding);
}

void PatchWithJump(DWORD instruction_addr, DWORD new_function_ptr)
{
	WriteValue<uint8_t>(instruction_addr, 0xE9);
	PatchCall(instruction_addr, new_function_ptr);
}

void PatchWithCall(DWORD instruction_addr, DWORD new_function_ptr)
{
	WriteValue<uint8_t>(instruction_addr, 0xE8);
	PatchCall(instruction_addr, new_function_ptr);
}

VOID CodeCaveJumpTo(DWORD destAddress, VOID(*func)(VOID), BYTE nopCount)
{
	DWORD offset = (PtrToUlong(func) - destAddress) - 5;
	
	BYTE nopPatch[0xFF] = { 0 };
	
	BYTE patch[5] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
	memcpy(patch + 1, &offset, sizeof(DWORD));
	WriteBytes(destAddress, patch, 5);
	
	if (nopCount == 0)
		return;
	
	memset(nopPatch, 0x90, nopCount);
	
	WriteBytes(destAddress + 5, nopPatch, nopCount);
}

void NopFill(uint32_t address, int length)
{
	uint8_t* byteArray = new uint8_t[length];
	memset(byteArray, 0x90, length);
	WriteBytes(address, byteArray, length);
	delete[] byteArray;
}

static DWORD RVAToFileMap(LPVOID pMapping, DWORD ddva)
{
	return (DWORD)pMapping + ddva;
}

DWORD PEResolveImports(HMODULE hModule)
{
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hModule;

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		//XLLNDebugLog(0, "ERROR: Not DOS - This file is not a DOS application.\n");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((DWORD)hModule + dos_header->e_lfanew);

	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		//XLLNDebugLog(0, "ERROR: Not Valid PE - This file is not a valid NT Portable Executable.\n");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_DATA_DIRECTORY IDE_Import = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (IDE_Import.Size <= 0) {
		//XLLNDebugLog(0, "WARNING: No Import Table - No import information in this PE.\n");
		return ERROR_TAG_NOT_PRESENT;
	}

	if (IDE_Import.Size % sizeof(IMAGE_IMPORT_DESCRIPTOR) != 0) {
		//XLLNDebugLog(0, "WARNING: Import Table not expected size.\n");
		return ERROR_INVALID_DLL;
	}

	WORD import_section_id = 0;
	IMAGE_SECTION_HEADER* section_headers = (IMAGE_SECTION_HEADER*)((DWORD)&nt_headers->OptionalHeader + nt_headers->FileHeader.SizeOfOptionalHeader);

	{
		DWORD i;
		for (i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
			if (IDE_Import.VirtualAddress >= section_headers[i].VirtualAddress && IDE_Import.VirtualAddress < section_headers[i].VirtualAddress + section_headers[i].Misc.VirtualSize) {
				import_section_id = i & 0xFF;
				break;
			}
		}
		if (i >= nt_headers->FileHeader.NumberOfSections) {
			//XLLNDebugLog(0, "WARNING: Import Table section not found.\n");
			return ERROR_TAG_NOT_PRESENT;
		}
	}

	IMAGE_IMPORT_DESCRIPTOR* fm_import_dir = (IMAGE_IMPORT_DESCRIPTOR*)RVAToFileMap(hModule, IDE_Import.VirtualAddress);
	DWORD maxi = IDE_Import.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
	for (DWORD i = 0; i < maxi; i++) {
		if (fm_import_dir[i].Name == NULL) {
			break;
		}
		char *im_name = (char*)RVAToFileMap(hModule, fm_import_dir[i].Name);
		HMODULE hDllImport = GetModuleHandleA(im_name);
		if (!hDllImport) {
			//continue;
			hDllImport = LoadLibraryA(im_name);
			if (!hDllImport) {
				__debugbreak();
			}
		}
		DWORD pthunk_addr = fm_import_dir[i].FirstThunk;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
		DWORD pthunk_ordinal = fm_import_dir[i].OriginalFirstThunk;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
		IMAGE_THUNK_DATA* thunk_addrs = (IMAGE_THUNK_DATA*)((DWORD)hModule + pthunk_addr);
		IMAGE_THUNK_DATA* thunk_hints = (IMAGE_THUNK_DATA*)((DWORD)hModule + pthunk_ordinal);
		for (DWORD j = 0; thunk_hints[j].u1.AddressOfData != 0; j++) {
			WORD ordinal = thunk_hints[j].u1.Ordinal & 0xFFFF;
			char *ordinal_name = 0;
			DWORD ordinal_addr = (DWORD)&thunk_addrs[j].u1.AddressOfData;
			if (!(thunk_hints[j].u1.AddressOfData & 0x80000000)) {
				DWORD thunk_hints_i = thunk_hints[j].u1.AddressOfData;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
				ordinal = *(WORD*)((DWORD)hModule + (thunk_hints_i));
				ordinal_name = (char*)((DWORD)hModule + (thunk_hints_i + sizeof(WORD)));
			}
			DWORD ordinal_func = NULL;
			if (ordinal_name) {
				ordinal_func = (DWORD)GetProcAddress(hDllImport, ordinal_name);
			}
			if (!ordinal_func) {
				ordinal_func = (DWORD)GetProcAddress(hDllImport, (PCSTR)ordinal);
			}
			if (!ordinal_func) {
				__debugbreak();
			}
			WritePointer((DWORD)ordinal_addr, (VOID*)ordinal_func);
		}
	}

	return ERROR_SUCCESS;
	return ERROR_FUNCTION_FAILED;
}

DWORD PEImportHack(HMODULE hModule, PE_HOOK_ARG *pe_hooks, DWORD pe_hooks_len)
{
	if (pe_hooks_len == 0xFFFFFFFF) {
		return ERROR_INVALID_PARAMETER;
	}
	if (pe_hooks_len && pe_hooks == NULL) {
		return ERROR_INVALID_PARAMETER;
	}
	for (DWORD i = 0; i < pe_hooks_len; i++) {
		pe_hooks[i].pe_err = ERROR_FUNCTION_FAILED;
	}
	if (hModule == NULL || hModule == INVALID_HANDLE_VALUE) {
		return ERROR_INVALID_HANDLE;
	}

	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hModule;

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		//XLLNDebugLog(0, "ERROR: Not DOS - This file is not a DOS application.");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((DWORD)hModule + dos_header->e_lfanew);

	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		//XLLNDebugLog(0, "ERROR: Not Valid PE - This file is not a valid NT Portable Executable.");
		return ERROR_BAD_EXE_FORMAT;
	}

	IMAGE_DATA_DIRECTORY IDE_Import = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (IDE_Import.Size <= 0) {
		//XLLNDebugLog(0, "WARNING: No Import Table - No import information in this PE.");
		return ERROR_TAG_NOT_PRESENT;
	}

	if (IDE_Import.Size % sizeof(IMAGE_IMPORT_DESCRIPTOR) != 0) {
		//XLLNDebugLog(0, "WARNING: Import Table not expected size.");
		return ERROR_INVALID_DLL;
	}

	WORD import_section_id = 0;
	IMAGE_SECTION_HEADER* section_headers = (IMAGE_SECTION_HEADER*)((DWORD)&nt_headers->OptionalHeader + nt_headers->FileHeader.SizeOfOptionalHeader);

	{
		DWORD i;
		for (i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
			if (IDE_Import.VirtualAddress >= section_headers[i].VirtualAddress && IDE_Import.VirtualAddress < section_headers[i].VirtualAddress + section_headers[i].Misc.VirtualSize) {
				import_section_id = i & 0xFF;
				break;
			}
		}
		if (i >= nt_headers->FileHeader.NumberOfSections) {
			//XLLNDebugLog(0, "WARNING: Import Table section not found.");
			return ERROR_TAG_NOT_PRESENT;
		}
	}

	IMAGE_IMPORT_DESCRIPTOR* fm_import_dir = (IMAGE_IMPORT_DESCRIPTOR*)RVAToFileMap(hModule, IDE_Import.VirtualAddress);
	DWORD maxi = IDE_Import.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
	for (DWORD i = 0; i < pe_hooks_len; i++) {
		// using as idx
		pe_hooks[i].pe_err = 0xFFFFFFFF;
	}
	for (DWORD i = 0; i < maxi; i++) {
		if (fm_import_dir[i].Name == NULL) {
			break;
		}
		char *im_name = (char*)RVAToFileMap(hModule, fm_import_dir[i].Name);
		for (DWORD j = 0; j < pe_hooks_len; j++) {
			// check dll match.
			if (_strnicmp(im_name, pe_hooks[j].pe_name, MAX_PATH) == 0) {
				pe_hooks[j].pe_err = i;
				break;
			}
		}
	}

	DWORD result = ERROR_SUCCESS;

	for (DWORD i = 0; i < pe_hooks_len; i++) {
		for (DWORD j = 0; j < pe_hooks[i].ordinals_len; j++) {
			pe_hooks[i].ordinal_addrs[j] = NULL;
		}
		DWORD dlli = pe_hooks[i].pe_err;
		if (dlli == 0xFFFFFFFF) {
			//XLLNDebugLog(0, "WARNING: DLL not found.");
			pe_hooks[i].pe_err = ERROR_NOT_FOUND;
			result = ERROR_NOT_FOUND;
			continue;
		}
		pe_hooks[i].pe_err = ERROR_SUCCESS;
		DWORD pthunk_addr = fm_import_dir[dlli].FirstThunk;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
		DWORD pthunk_ordinal = fm_import_dir[dlli].OriginalFirstThunk;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
		IMAGE_THUNK_DATA* thunk_addrs = (IMAGE_THUNK_DATA*)((DWORD)hModule + pthunk_addr);
		IMAGE_THUNK_DATA* thunk_hints = (IMAGE_THUNK_DATA*)((DWORD)hModule + pthunk_ordinal);
		for (DWORD j = 0; thunk_hints[j].u1.AddressOfData != 0; j++) {
			WORD ordinal = thunk_hints[j].u1.Ordinal & 0xFFFF;
			char *ordinal_name = 0;
			DWORD ordinal_addr = (DWORD)&thunk_addrs[j].u1.AddressOfData;
			if (!(thunk_hints[j].u1.AddressOfData & 0x80000000)) {
				DWORD thunk_hints_i = thunk_hints[j].u1.AddressOfData;// + section_headers[import_section_id].PointerToRawData - section_headers[import_section_id].VirtualAddress;
				ordinal = *(WORD*)((DWORD)hModule + (thunk_hints_i));
				ordinal_name = (char*)((DWORD)hModule + (thunk_hints_i + sizeof(WORD)));
			}
			for (DWORD k = 0; k < pe_hooks[i].ordinals_len; k++) {
				// check ordinal match.
				if (ordinal_name == NULL || pe_hooks[i].ordinal_names == NULL || pe_hooks[i].ordinal_names[k] == NULL) {
					if (ordinal != pe_hooks[i].ordinals[k]) {
						continue;
					}
				}
				else if (_strnicmp(ordinal_name, pe_hooks[i].ordinal_names[k], MAX_PATH) != 0) {
					continue;
				}
				pe_hooks[i].ordinal_addrs[k] = ordinal_addr;
				break;
			}
		}
		for (DWORD j = 0; j < pe_hooks[i].ordinals_len; j++) {
			if (pe_hooks[i].ordinal_addrs[j] == NULL) {
				//XLLNDebugLog(0, "WARNING: DLL ordinal(s) not found.");
				pe_hooks[i].pe_err = ERROR_NO_MATCH;
				result = ERROR_NOT_FOUND;
				break;
			}
		}
	}

	return result;
}

bool HookImport(DWORD *Import_Addr_Location, VOID *Detour_Func, VOID *Hook_Func, DWORD Ordinal_Addr)
{
	// Undo hook.
	if (*Import_Addr_Location) {
		WriteValue((DWORD)*Import_Addr_Location, *(DWORD*)Detour_Func);
		//*(VOID**)Detour_Func = 0;
		*Import_Addr_Location = 0;
	}

	// Inject hook.
	if (Ordinal_Addr) {
		*Import_Addr_Location = (DWORD)Ordinal_Addr;
		*(VOID**)Detour_Func = *(VOID**)(Ordinal_Addr);
		WritePointer(Ordinal_Addr, Hook_Func);
	}

	return true;
}

size_t FileOffsetToRva(size_t file_offset, IMAGE_SECTION_HEADER* image_section_header, uint16_t section_count)
{
	// Find section holding the Offset.
	for (uint16_t iSection = 0; iSection < section_count; iSection++) {
		if ((file_offset >= image_section_header[iSection].PointerToRawData) && (file_offset <= image_section_header[iSection].PointerToRawData + image_section_header[iSection].SizeOfRawData)) {
			// Convert Offset to RVA.
			return file_offset + image_section_header[iSection].VirtualAddress - image_section_header[iSection].PointerToRawData;
		}
	}
	return 0;
}
