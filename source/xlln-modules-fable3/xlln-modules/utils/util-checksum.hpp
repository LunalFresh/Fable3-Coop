#pragma once
#include <stdio.h>
#include <stdint.h>

bool GetFileSha256(const wchar_t* item_filepath, uint8_t** result_checksum);
char* Sha256ByteArrayToStr(const uint8_t* checksum);
char* GetPESha256FlagFix(const wchar_t* module_file_path);
char* GetPESha256FlagFix(HMODULE hModule);
char* GetSha256Sum(uint8_t* buffer, size_t buf_size);
uint32_t crc32buf(uint8_t* buffer, size_t buf_size);
bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD &rtncrc32);
