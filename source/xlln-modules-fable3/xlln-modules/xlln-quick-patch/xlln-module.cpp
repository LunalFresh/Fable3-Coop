#ifndef NOMINMAX
# define NOMINMAX
#endif
#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-checksum.hpp"
#include "../utils/util-hook.hpp"
#include "../third-party/rapidjson/document.h"
#include "../third-party/rapidjson/prettywriter.h"
#include <vector>

typedef struct _BINARY_PATCH_INFO {
	size_t patchOffset = 0;
	uint8_t* patchData = 0;
	size_t patchDataSize = 0;
	
	~_BINARY_PATCH_INFO()
	{
		if (this->patchData) {
			delete[] this->patchData;
			this->patchData = 0;
		}
	}
	
} BINARY_PATCH_INFO;

typedef struct _QUICK_PATCH_CONFIG {
	// [Beginning, End] offsets.
	std::vector<std::pair<size_t, size_t>> nopOffsets;
	std::vector<BINARY_PATCH_INFO*> binaryPatches;
	
	~_QUICK_PATCH_CONFIG()
	{
		for (auto &binaryPatch : this->binaryPatches) {
			delete binaryPatch;
		}
		this->binaryPatches.clear();
	}
	
} QUICK_PATCH_CONFIG;

static std::vector<QUICK_PATCH_CONFIG*> xmod_quick_patch_configs;

static rapidjson::Value* FindJsonMember(rapidjson::Document *document, const char *member_name, rapidjson::Type member_type)
{
	auto itrMember = document->FindMember(member_name);
	if (itrMember == document->MemberEnd()) {
		return 0;
	}
	
	auto &memberValue = (*itrMember).value;
	
	if (memberValue.GetType() != member_type) {
		return 0;
	}
	
	return &memberValue;
}

static rapidjson::Value* FindJsonMember(rapidjson::Value *document, const char *member_name, rapidjson::Type member_type)
{
	auto itrMember = document->FindMember(member_name);
	if (itrMember == document->MemberEnd()) {
		return 0;
	}
	
	auto &memberValue = (*itrMember).value;
	
	if (memberValue.GetType() != member_type) {
		return 0;
	}
	
	return &memberValue;
}

static rapidjson::Value* FindJsonMember(rapidjson::Value *document, const char *member_name)
{
	auto itrMember = document->FindMember(member_name);
	if (itrMember == document->MemberEnd()) {
		return 0;
	}
	
	auto &memberValue = (*itrMember).value;
	
	return &memberValue;
}

static bool ParseConfigTitleEntry(rapidjson::Value* title_config_entry, char* loaded_title_sha256)
{
	if (title_config_entry->GetType() != rapidjson::Type::kObjectType) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s config file Title entry is not an object type."
			, __func__
		);
		return false;
	}
	
	{
		rapidjson::Value* jsonMember = FindJsonMember(title_config_entry, "titleSha256", rapidjson::Type::kStringType);
		if (!jsonMember) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "[Quick-Patch] %s config file 'titleSha256' does not exist."
				, __func__
			);
		}
		
		if (_strcmpi(loaded_title_sha256, jsonMember->GetString()) != 0) {
			return true;
		}
	}
	
	rapidjson::Value* jsonBinaryNopRangePatches = FindJsonMember(title_config_entry, "binaryNopRangePatches", rapidjson::Type::kArrayType);
	rapidjson::Value* jsonBinaryPatches = FindJsonMember(title_config_entry, "binaryPatches", rapidjson::Type::kArrayType);
	if (!jsonBinaryNopRangePatches && !jsonBinaryPatches) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s config file both 'binaryNopRangePatches' and 'binaryPatches' do not exist."
			, __func__
		);
		return false;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "[Quick-Patch] %s config file 'titleSha256' match."
		, __func__
	);
	
	QUICK_PATCH_CONFIG* qpConfig = new QUICK_PATCH_CONFIG;
	
	bool parseError = false;
	
	if (jsonBinaryNopRangePatches) {
		for (auto &jsonBinaryNopRangePatch : jsonBinaryNopRangePatches->GetArray()) {
			if (!jsonBinaryNopRangePatch.IsArray()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[Quick-Patch] %s JSON config file error: an element within 'binaryNopRangePatches' is not an array."
					, __func__
				);
				parseError = true;
				break;
			}
			
			auto itrJsonBinaryNopRangePatch = jsonBinaryNopRangePatch.GetArray();
			if (itrJsonBinaryNopRangePatch.Size() != 2) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[Quick-Patch] %s JSON config file error: an element within 'binaryNopRangePatches' is not an array 2 items in size."
					, __func__
				);
				parseError = true;
				break;
			}
			
			size_t offsetBeginning = 0;
			size_t offsetEnd = 0;
			
			for (uint8_t iVal = 0; iVal < itrJsonBinaryNopRangePatch.Size(); iVal++) {
				if (!itrJsonBinaryNopRangePatch[iVal].IsUint()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] %s JSON config file error: an element within an 'binaryNopRangePatches' item is not uint32_t."
						, __func__
					);
					parseError = true;
					break;
				}
				
				uint32_t value = itrJsonBinaryNopRangePatch[iVal].GetUint();
				switch (iVal) {
					case 0: {
						offsetBeginning = value;
						break;
					}
					case 1: {
						offsetEnd = value;
						break;
					}
					default: {
						__debugbreak();
						break;
					}
				}
			}
			
			if (parseError) {
				break;
			}
			
			qpConfig->nopOffsets.push_back(std::make_pair(offsetBeginning, offsetEnd));
		}
	}
	
	if (jsonBinaryPatches) {
		for (auto &jsonBinaryPatch : jsonBinaryPatches->GetArray()) {
			
			BINARY_PATCH_INFO* binaryPatch = new BINARY_PATCH_INFO;
			
			{
				rapidjson::Value* jsonMember = FindJsonMember(&jsonBinaryPatch, "patchOffset", rapidjson::Type::kNumberType);
				if (!jsonMember || !jsonMember->IsUint()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] %s JSON config file error 'patchOffset' does not exist in 'binaryPatches' array item."
						, __func__
					);
					parseError = true;
					delete binaryPatch;
					break;
				}
				
				binaryPatch->patchOffset = jsonMember->GetUint();
			}
			
			{
				rapidjson::Value* jsonMember = FindJsonMember(&jsonBinaryPatch, "patchData", rapidjson::Type::kArrayType);
				if (!jsonMember) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] %s JSON config file error 'patchData' does not exist in 'binaryPatches' array item."
						, __func__
					);
					parseError = true;
					delete binaryPatch;
					break;
				}
				
				auto itrJsonBinaryPatchData = jsonMember->GetArray();
				if (itrJsonBinaryPatchData.Size() <= 0) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] %s JSON config file error: 'patchData' is an empty array."
						, __func__
					);
					parseError = true;
					delete binaryPatch;
					break;
				}
				
				binaryPatch->patchDataSize = itrJsonBinaryPatchData.Size();
				binaryPatch->patchData = new uint8_t[binaryPatch->patchDataSize];
				
				for (uint8_t iVal = 0; iVal < binaryPatch->patchDataSize; iVal++) {
					if (!itrJsonBinaryPatchData[iVal].IsUint()) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
							, "[Quick-Patch] %s JSON config file error: an element within an 'patchData' item is not type uint32_t."
							, __func__
						);
						parseError = true;
						break;
					}
					
					uint32_t value = itrJsonBinaryPatchData[iVal].GetUint();
					if (value > 0xFF) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
							, "[Quick-Patch] %s JSON config file error: an element within an 'binaryPatches' item is larger than uint8_t."
							, __func__
						);
						parseError = true;
						break;
					}
					
					binaryPatch->patchData[iVal] = (value & 0xFF);
				}
				
				if (parseError) {
					delete binaryPatch;
					break;
				}
			}
			
			qpConfig->binaryPatches.push_back(binaryPatch);
		}
	}
	
	if (parseError) {
		delete qpConfig;
		qpConfig = 0;
	}
	else {
		xmod_quick_patch_configs.push_back(qpConfig);
	}
	
	return !parseError;
}

static bool ParseConfigFile(uint8_t* config_buffer, size_t config_buffer_size, char* loaded_title_sha256)
{
	rapidjson::Document document;
	if (document.Parse< rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag >((char*)config_buffer, config_buffer_size).HasParseError()) {
		rapidjson::ParseErrorCode errorCode = document.GetParseError();
		size_t errorInFileOffset = document.GetErrorOffset();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s Failed to parse JSON config file with error 0x%08x at offset %d."
			, __func__
			, errorCode
			, errorInFileOffset
		);
		return false;
	}
	
	if (document.GetType() != rapidjson::Type::kArrayType) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s config file is not an array of Title entries."
			, __func__
		);
		return false;
	}
	
	bool resultParseSuccess = true;
	
	for (auto &titleConfigEntry : document.GetArray()) {
		bool resultTitleEntry = ParseConfigTitleEntry(&titleConfigEntry, loaded_title_sha256);
		resultParseSuccess = resultParseSuccess && resultTitleEntry;
	}
	
	return resultParseSuccess;
}

static bool ReadConfigs()
{
	wchar_t* xmod_config_path = 0;
	uint32_t xlln_local_instance_id = 0;
	
	{
		size_t storagePathBufSize = 0;
		uint32_t errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, 0, 0, &storagePathBufSize);
		if (errorGetStoragePath == ERROR_INSUFFICIENT_BUFFER) {
			wchar_t* storagePath = new wchar_t[storagePathBufSize / sizeof(wchar_t)];
			errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, &xlln_local_instance_id, storagePath, &storagePathBufSize);
			if (errorGetStoragePath == ERROR_SUCCESS) {
				xmod_config_path = FormMallocString(L"%wsxlln-quick-patch/", storagePath);
			}
			delete[] storagePath;
			storagePath = 0;
		}
	}
	
	if (!xmod_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s Unable to determine config path."
			, __func__
		);
		return false;
	}
	
	char* checksumTitle = GetPESha256FlagFix(xlln_hmod_title);
	if (!checksumTitle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s Unable to calculate Title SHA-256."
			, __func__
		);
		return false;
	}
	
	// Load all additional modules.
	WIN32_FIND_DATAW data;
	wchar_t* modulesSearch = FormMallocString(L"%ws*.json", xmod_config_path);
	HANDLE hFind = FindFirstFileW(modulesSearch, &data);
	free(modulesSearch);
	modulesSearch = 0;
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!EndsWithCaseInsensitive(data.cFileName, L".json")) {
				continue;
			}
			wchar_t* xllnModuleFilePath = FormMallocString(L"%ws%ws", xmod_config_path, data.cFileName);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
				, "[Quick-Patch] Loading: \"%ls\"."
				, xllnModuleFilePath
			);
			
			FILE* fileHandle = 0;
			errno_t errorFopen = _wfopen_s(&fileHandle, xllnModuleFilePath, L"rb");
			if (!fileHandle) {
				if (errorFopen == ENOENT) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] config file not found: \"%ls\"."
						, xllnModuleFilePath
					);
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[Quick-Patch] %s config file read error: %d, \"%ls\"."
						, __func__
						, errorFopen
						, xllnModuleFilePath
					);
				}
			}
			else {
				fseek(fileHandle, (long)0, SEEK_END);
				size_t fileSize = ftell(fileHandle);
				fseek(fileHandle, (long)0, SEEK_SET);
				fileSize -= ftell(fileHandle);
				uint8_t *buffer = new uint8_t[fileSize];
				size_t readC = fread(buffer, sizeof(uint8_t), fileSize / sizeof(uint8_t), fileHandle);
				
				fclose(fileHandle);
				fileHandle = 0;
				
				ParseConfigFile(buffer, readC, checksumTitle);
				
				delete[] buffer;
				buffer = 0;
			}
			
			free(xllnModuleFilePath);
			xllnModuleFilePath = 0;
		} while (FindNextFileW(hFind, &data));
		FindClose(hFind);
	}
	
	free(checksumTitle);
	checksumTitle = 0;
	
	free(xmod_config_path);
	xmod_config_path = 0;
	
	return true;
}

static bool PatchTitle()
{
	IMAGE_DOS_HEADER* imageDosHeader = (IMAGE_DOS_HEADER*)xlln_hmod_title;
	
	if (imageDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s unable to find the loaded image's DOS header signature."
			, __func__
		);
		
		return false;
	}
	
	IMAGE_NT_HEADERS* imageNtHeaders = (IMAGE_NT_HEADERS*)((size_t)xlln_hmod_title + imageDosHeader->e_lfanew);
	
	if (imageNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[Quick-Patch] %s unable to find the loaded image's NT header signature."
			, __func__
		);
		return false;
	}
	
	uint16_t imageSectionHeaderTableSectionCount = imageNtHeaders->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER *imageSectionHeaderTable = (IMAGE_SECTION_HEADER*)((size_t)xlln_hmod_title + imageDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[Quick-Patch] Patching Title."
	);
	
	for (auto &patchConfig : xmod_quick_patch_configs) {
		for (auto &binaryPatch : patchConfig->nopOffsets) {
			uint8_t* patchAddressRuntime = (uint8_t*)((size_t)xlln_hmod_title + FileOffsetToRva(binaryPatch.first, imageSectionHeaderTable, imageSectionHeaderTableSectionCount));
			if (!patchAddressRuntime) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[Quick-Patch] %s unable to calculate the RVA offset from the file offset (0x%zx)."
					, __func__
					, binaryPatch.first
				);
			}
			uint32_t patchSize = binaryPatch.second - binaryPatch.first;
			
			DWORD dwBack;
			VirtualProtect(patchAddressRuntime, patchSize, PAGE_READWRITE, &dwBack);
			
			// NOP instruction.
			memset(patchAddressRuntime, 0x90, patchSize);
			
			VirtualProtect(patchAddressRuntime, patchSize, dwBack, &dwBack);
		}
		
		for (auto &binaryPatch : patchConfig->binaryPatches) {
			uint8_t* patchAddressRuntime = (uint8_t*)((size_t)xlln_hmod_title + FileOffsetToRva(binaryPatch->patchOffset, imageSectionHeaderTable, imageSectionHeaderTableSectionCount));
			if (!patchAddressRuntime) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[Quick-Patch] %s unable to calculate the RVA offset from the file offset (0x%zx)."
					, __func__
					, binaryPatch->patchOffset
				);
			}
			
			DWORD dwBack;
			VirtualProtect(patchAddressRuntime, binaryPatch->patchDataSize, PAGE_READWRITE, &dwBack);
			
			memcpy(patchAddressRuntime, binaryPatch->patchData, binaryPatch->patchDataSize);
			
			VirtualProtect(patchAddressRuntime, binaryPatch->patchDataSize, dwBack, &dwBack);
		}
	}
	
	return true;
}

// #41101
uint32_t WINAPI XLLNModulePostInit()
{
	if (!ReadConfigs()) {
		return ERROR_FUNCTION_FAILED;
	}
	
	if (!xmod_quick_patch_configs.size()) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "[Quick-Patch] No configs loaded."
		);
		return ERROR_NO_MORE_FILES;
	}
	
	PatchTitle();
	
	return ERROR_SUCCESS;
}

// #41102
uint32_t WINAPI XLLNModulePreUninit()
{
	uint32_t result = ERROR_SUCCESS;
	
	for (auto &patchConfig : xmod_quick_patch_configs) {
		delete patchConfig;
	}
	xmod_quick_patch_configs.clear();
	
	return result;
}

bool InitXllnModule()
{
	return true;
}

bool UninitXllnModule()
{
	return true;
}
