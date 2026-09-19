#pragma once
#include <stdint.h>

void WriteBytes(DWORD destAddress, LPVOID bytesToWrite, int numBytes);
template <typename value_type>
inline void WriteValue(DWORD offset, value_type data)
{
	WriteBytes(offset, &data, sizeof(data));
}
void WritePointer(DWORD offset, void *ptr);
void WriteValue(DWORD offset, DWORD value);
void CodeCave(uint8_t *src_fn, void(*cave_fn)(), const int extra_instructions);
void *DetourFunc(BYTE *src, const BYTE *dst, const int len);
void RetourFunc(BYTE *src, BYTE *restore, const int len);
void *DetourClassFunc(BYTE *src, const BYTE *dst, const int len);
void RetourClassFunc(BYTE *src, BYTE *restore, const int len);
void PatchCall(DWORD call_addr, DWORD new_function_ptr);
inline void PatchCall(DWORD call_addr, void *new_function_ptr)
{
	PatchCall(call_addr, reinterpret_cast<DWORD>(new_function_ptr));
}
inline void PatchCall(void *call_addr, void *new_function_ptr)
{
	PatchCall(reinterpret_cast<DWORD>(call_addr), reinterpret_cast<DWORD>(new_function_ptr));
}
void HookCall(uint32_t call_addr, uint32_t new_function_ptr, uint32_t *original_func_ptr);
void PatchWinAPICall(DWORD call_addr, DWORD new_function_ptr);
inline void PatchWinAPICall(DWORD call_addr, void *new_function_ptr)
{
	PatchWinAPICall(call_addr, reinterpret_cast<DWORD>(new_function_ptr));
}
inline void PatchWinAPICall(void *call_addr, void *new_function_ptr)
{
	PatchWinAPICall(reinterpret_cast<DWORD>(call_addr), reinterpret_cast<DWORD>(new_function_ptr));
}
void PatchWithJump(DWORD instruction_addr, DWORD new_function_ptr);
inline void PatchWithJump(DWORD instruction_addr, void *new_function_ptr)
{
	PatchWithJump(instruction_addr, reinterpret_cast<DWORD>(new_function_ptr));
}
inline void PatchWithJump(void *instruction_addr, void *new_function_ptr)
{
	PatchWithJump(reinterpret_cast<DWORD>(instruction_addr), reinterpret_cast<DWORD>(new_function_ptr));
}
void PatchWithCall(DWORD instruction_addr, DWORD new_function_ptr);
inline void PatchWithCall(DWORD instruction_addr, void *new_function_ptr)
{
	PatchWithCall(instruction_addr, reinterpret_cast<DWORD>(new_function_ptr));
}
inline void PatchWithCall(void *instruction_addr, void *new_function_ptr)
{
	PatchWithCall(reinterpret_cast<DWORD>(instruction_addr), reinterpret_cast<DWORD>(new_function_ptr));
}
VOID CodeCaveJumpTo(DWORD destAddress, VOID(*func)(VOID), BYTE nopCount);
void NopFill(uint32_t address, int length);

DWORD PEResolveImports(HMODULE hModule);
typedef struct {
	const char *pe_name;
	DWORD pe_err;
	WORD *ordinals;
	const char **ordinal_names;
	DWORD *ordinal_addrs;
	DWORD ordinals_len;
} PE_HOOK_ARG;
DWORD PEImportHack(HMODULE hModule, PE_HOOK_ARG *pe_hooks, DWORD pe_hooks_len);
bool HookImport(DWORD *Import_Addr_Location, VOID *Detour_Func, VOID *Hook_Func, DWORD Ordinal_Addr);
size_t FileOffsetToRva(size_t file_offset, IMAGE_SECTION_HEADER* image_section_header, uint16_t section_count);
