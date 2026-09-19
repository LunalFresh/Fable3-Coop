#include <Windows.h>
#include "util-checksum.hpp"
#include "utils.hpp"
#include "sha256.hpp"

// new'ly[] allocates *result_checksum if it is not already allocated.
bool GetFileSha256(const wchar_t* item_filepath, uint8_t** result_checksum)
{
	FILE* fileHandle = 0;
	errno_t errOpen = _wfopen_s(&fileHandle, item_filepath, L"rb");
	if (!fileHandle || errOpen) {
		wprintf(L"Error (%d) failed to open file for reading: \"%ws\".\n", errOpen, item_filepath);
		return false;
	}
	
	int resultMbedtls = 0;
	mbedtls_sha256_context context;
	mbedtls_sha256_init(&context);
	
	if ((resultMbedtls = mbedtls_sha256_starts_ret(&context, 0))) {
		wprintf(L"%hs Error (%d) on call to mbedtls_sha256_starts_ret(...).\n", __func__, resultMbedtls);
	}
	else {
		uint8_t readBuffer[0x8000];
		size_t readCount = 0;
		do {
			readCount = fread(readBuffer, sizeof(uint8_t), sizeof(readBuffer), fileHandle);
			if (readCount) {
				resultMbedtls = mbedtls_sha256_update_ret(&context, readBuffer, readCount);
			}
		} while (readCount == sizeof(readBuffer) && resultMbedtls == 0);
		
		int errorRead = ferror(fileHandle);
		if (errorRead) {
			resultMbedtls = (int)-1;
			wprintf(L"%hs Error (%d) on fread(...) while reading: \"%ws\".\n", __func__, errorRead, item_filepath);
		}
		else {
			if (resultMbedtls) {
				wprintf(L"%hs Error (%d) on call to mbedtls_sha256_update_ret(...).\n", __func__, resultMbedtls);
			}
			else {
				uint8_t* checksum = 0;
				if (*result_checksum == 0) {
					checksum = new uint8_t[32];
				}
				
				if ((resultMbedtls = mbedtls_sha256_finish_ret(&context, checksum ? checksum : *result_checksum))) {
					wprintf(L"%hs Error (%d) on call to mbedtls_sha256_finish_ret(...).\n", __func__, resultMbedtls);
				}
				else {
					if (*result_checksum == 0) {
						*result_checksum = checksum;
						checksum = 0;
					}
				}
				if (checksum) {
					delete[] checksum;
					checksum = 0;
				}
			}
		}
	}
	
	mbedtls_sha256_free(&context);
	
	fclose(fileHandle);
	fileHandle = 0;
	
	return resultMbedtls == 0;
}

// new'ly[] allocates a 65 char buffer to store the given byte format.
char* Sha256ByteArrayToStr(const uint8_t* checksum)
{
	if (!checksum) {
		return 0;
	}
	
	char* checksumStr = new char[65];
	for (uint8_t i = 0; i < 32; i++) {
		sprintf_s(&checksumStr[i*2], sizeof(char) * 3, "%02hhx", checksum[i]);
	}
	checksumStr[64] = 0;
	
	return checksumStr;
}

// Allow statically setting/patching the IMAGE_FILE_LARGE_ADDRESS_AWARE flag on a PE while still generating the same checksum as the original unmodified PE.
char* GetPESha256FlagFix(const wchar_t* module_file_path)
{
	FILE* fileHandle = 0;
	errno_t errOpen = _wfopen_s(&fileHandle, module_file_path, L"rb");
	if (!fileHandle || errOpen) {
		wprintf(L"Error (%d) failed to open file for reading: \"%ws\".\n", errOpen, module_file_path);
		return 0;
	}
	fseek(fileHandle, (long)0, SEEK_END);
	size_t binaryBufferSize = ftell(fileHandle);
	fseek(fileHandle, (long)0, SEEK_SET);
	binaryBufferSize -= ftell(fileHandle);
	uint8_t* binaryBuffer = (uint8_t*)malloc(sizeof(uint8_t) * binaryBufferSize);
	size_t readC = fread(binaryBuffer, sizeof(uint8_t), binaryBufferSize / sizeof(uint8_t), fileHandle);
	fclose(fileHandle);
	
	{
		IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)(binaryBuffer);
		IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((uint8_t*)(binaryBuffer) + dosHeader->e_lfanew);
		
		uint32_t generatedPEChecksum = 0;
		uint8_t iMatch = 0;
		
		uint16_t originalCharacteristics = ntHeaders->FileHeader.Characteristics;
		
		while (iMatch <= 1) {
			if (iMatch == 1) {
				if (!(ntHeaders->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)) {
					// if LAA is unset then skip this attempt.
					iMatch++;
					continue;
				}
				// Unset only this flag.
				ntHeaders->FileHeader.Characteristics &= ~IMAGE_FILE_LARGE_ADDRESS_AWARE;
			}
			
			generatedPEChecksum = GetPEChecksum(binaryBuffer, binaryBufferSize, (uint8_t*)&(ntHeaders->OptionalHeader.CheckSum) - (uint8_t*)(binaryBuffer));
			
			if (generatedPEChecksum == ntHeaders->OptionalHeader.CheckSum) {
				break;
			}
			
			iMatch++;
		}
		
		switch (iMatch) {
			case 0: {
				// No changes were made.
				break;
			}
			case 1: {
				wprintf(L"%hs INFO PE header LAA characteristic has been set for \"%ws\".\n", __func__, module_file_path);
				// Do not revert the change to the binaryBuffer so the original checksum will be generated.
				break;
			}
			default: {
				wprintf(L"%hs WARNING PE header checksum is incorrect for \"%ws\".\n", __func__, module_file_path);
				// Reset the original value so the correct checksum for the modified PE is generated.
				ntHeaders->FileHeader.Characteristics = originalCharacteristics;
				break;
			}
		}
	}
	
	uint8_t* checksum = (uint8_t*)malloc(sizeof(uint8_t) * 32);
	mbedtls_sha256(binaryBuffer, binaryBufferSize, checksum, 0);
	free(binaryBuffer);
	binaryBuffer = 0;
	
	char* checksumStr = (char*)malloc(sizeof(char) * 65);
	for (uint8_t i = 0; i < 32; i++) {
		sprintf_s(&checksumStr[i*2], sizeof(char) * 3, "%02hhx", checksum[i]);
	}
	checksumStr[64] = 0;
	
	free(checksum);
	
	return checksumStr;
}

char* GetPESha256FlagFix(HMODULE hModule)
{
	wchar_t* moduleFileName = GetModuleFilePathW(hModule);
	if (!moduleFileName) {
		return 0;
	}
	char* checksumStr = GetPESha256FlagFix(moduleFileName);
	free(moduleFileName);
	
	return checksumStr;
}

// new'ly[] allocate checksum string.
char* GetSha256Sum(uint8_t* buffer, size_t buf_size)
{
	uint8_t* checksum = new uint8_t[32];
	mbedtls_sha256(buffer, buf_size, checksum, 0);

	char* checksumStr = new char[65];
	for (uint8_t i = 0; i < 32; i++) {
		sprintf_s(&checksumStr[i*2], sizeof(char) * 3, "%02hhx", checksum[i]);
	}
	checksumStr[64] = 0;

	delete[] checksum;

	return checksumStr;
}

//Copyright (C) 1986 Gary S. Brown.  You may use this program, or
// code or tables extracted from it, as desired without restriction.
typedef DWORD UNS_32_BITS;
static UNS_32_BITS crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

#define UPDC32(octet, crc) (crc_32_tab[((crc) ^ (octet)) & 0xff] ^ ((crc) >> 8))

uint32_t crc32buf(uint8_t* buffer, size_t buf_size)
{
	register uint32_t oldcrc32;

	oldcrc32 = 0xFFFFFFFF;

	for (; buf_size; --buf_size, ++buffer)
	{
		oldcrc32 = UPDC32(*buffer, oldcrc32);
	}

	return ~oldcrc32;
}

bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD &rtncrc32)
{
	register DWORD oldcrc32;

	oldcrc32 = 0xFFFFFFFF;

	const int BUFSIZE = 1024;
	BOOL bResult = FALSE;
	HANDLE hFile = NULL;
	uint8_t rgbFile[BUFSIZE];
	DWORD cbRead = 0;

	hFile = CreateFile(filepath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		return false;
	}

	while (bResult = ReadFile(hFile, rgbFile, BUFSIZE, &cbRead, NULL))
	{
		if (cbRead == 0) {
			break;
		}

		for (unsigned int i = 0; i < cbRead; i++) {
			oldcrc32 = UPDC32(rgbFile[i], oldcrc32);
		}
	}

	if (!bResult) {
		return false;
	}

	CloseHandle(hFile);

	rtncrc32 = ~oldcrc32;

	return true;
}
