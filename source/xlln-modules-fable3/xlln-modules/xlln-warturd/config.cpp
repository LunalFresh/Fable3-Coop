#ifndef NOMINMAX
# define NOMINMAX
#endif
#include "../dllmain.hpp"
#include "./config.hpp"
#include "./deobfuscator.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../third-party/rapidjson/document.h"
#include "../third-party/rapidjson/prettywriter.h"
#include <list>
#include <vector>
#include <map>
#include <set>

wchar_t *warturd_storage_filepath = 0;
static uint32_t xlln_local_instance_id = 0;

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

static bool ParseWarturdConfig(uint8_t *buffer, size_t buffer_size)
{
	rapidjson::Document document;
	if (document.Parse< rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag >((char*)buffer, buffer_size).HasParseError()) {
		rapidjson::ParseErrorCode errorCode = document.GetParseError();
		size_t errorInFileOffset = document.GetErrorOffset();
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Failed to parse Warturd JSON config file with error 0x%08x at offset %d."
			, __func__
			, errorCode
			, errorInFileOffset
		);
		return false;
	}
	
	//auto jsonEnableForInstances = document.FindMember("enableForInstances");
	//if (jsonEnableForInstances != document.MemberEnd() && (*jsonEnableForInstances).value.IsArray()) {
	//	auto itrEnableForInstance = (*jsonEnableForInstances).value.GetArray();
	rapidjson::Value* jsonEnableForInstances = FindJsonMember(&document, "enableForInstances", rapidjson::Type::kArrayType);
	if (jsonEnableForInstances) {
		auto itrEnableForInstance = jsonEnableForInstances->GetArray();
		bool enableForInstance = true;
		if (itrEnableForInstance.Size()) {
			enableForInstance = false;
			for (auto &jsonInstanceId : itrEnableForInstance) {
				if (jsonInstanceId.IsUint()) {
					uint32_t localInstanceId = jsonInstanceId.GetUint();
					if (xlln_local_instance_id == localInstanceId) {
						enableForInstance = true;
					}
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error 'enableForInstances' array contains a value that is not uint32_t."
						, __func__
					);
					return false;
				}
			}
		}
		
		if (!enableForInstance) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
				, "%s Warturd module is not configured to run for this local instance id of %d."
				, __func__
				, xlln_local_instance_id
			);
			return false;
		}
	}
	
	rapidjson::Value* jsonObfuscatedBinaries = FindJsonMember(&document, "obfuscatedBinaries", rapidjson::Type::kArrayType);
	if (!jsonObfuscatedBinaries) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd JSON config file error 'obfuscatedBinaries' does not exist."
			, __func__
		);
		return false;
	}
	
	for (auto &jsonObfuscatedBinary : jsonObfuscatedBinaries->GetArray()) {
		OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig = new OBFUSCATED_BINARY_CONFIG;
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "binaryName", rapidjson::Type::kStringType);
			if (!jsonMember) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'binaryName' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			size_t binaryNameSize = strlen(jsonMember->GetString()) + 1;
			obfuscatedBinaryConfig->binaryName = new wchar_t[binaryNameSize];
			swprintf_s(obfuscatedBinaryConfig->binaryName, binaryNameSize, L"%hs", jsonMember->GetString());
			obfuscatedBinaryConfig->binaryName[binaryNameSize - 1] = 0;
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "binarySha256", rapidjson::Type::kStringType);
			if (!jsonMember) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'binarySha256' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			size_t binarySha256Size = strlen(jsonMember->GetString()) + 1;
			obfuscatedBinaryConfig->binarySha256 = new char[binarySha256Size];
			memcpy(obfuscatedBinaryConfig->binarySha256, jsonMember->GetString(), binarySha256Size);
			obfuscatedBinaryConfig->binarySha256[binarySha256Size - 1] = 0;
			
			if (binarySha256Size / sizeof(char) != 64 + 1) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
					, "%s Warturd JSON config file warning 'binarySha256' is not 64 characters long."
					, __func__
				);
			}
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "binaryIsImported");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'binaryIsImported' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->binaryIsImported = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "binaryIsLoadedProgrammatically");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'binaryIsLoadedProgrammatically' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->binaryIsLoadedProgrammatically = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "resultDestinationPath", rapidjson::Type::kStringType);
			if (!jsonMember) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'resultDestinationPath' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			size_t resultDestinationPathSize = strlen(jsonMember->GetString()) + 1;
			obfuscatedBinaryConfig->resultDestinationPath = new wchar_t[resultDestinationPathSize];
			swprintf_s(obfuscatedBinaryConfig->resultDestinationPath, resultDestinationPathSize, L"%hs", jsonMember->GetString());
			obfuscatedBinaryConfig->resultDestinationPath[resultDestinationPathSize - 1] = 0;
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "offsetFuncDeobfuscateStackSetupType1", rapidjson::Type::kNumberType);
			if (jsonMember) {
				if (!jsonMember->IsUint()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error 'offsetFuncDeobfuscateStackSetupType1' is not a uint32_t."
						, __func__
					);
					delete obfuscatedBinaryConfig;
					continue;
				}
				
				obfuscatedBinaryConfig->manuallyLocatedDeobfuscateStackSetupFunctions = true;
				obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1 = jsonMember->GetUint();
			}
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "offsetFuncDeobfuscateStackSetupType2", rapidjson::Type::kNumberType);
			if (jsonMember) {
				if (!jsonMember->IsUint()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error 'offsetFuncDeobfuscateStackSetupType2' is not a uint32_t."
						, __func__
					);
					delete obfuscatedBinaryConfig;
					continue;
				}
				
				obfuscatedBinaryConfig->manuallyLocatedDeobfuscateStackSetupFunctions = true;
				obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2 = jsonMember->GetUint();
			}
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "offsetFuncDeobfuscateStackSetupType2b", rapidjson::Type::kNumberType);
			if (jsonMember) {
				if (!jsonMember->IsUint()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error 'offsetFuncDeobfuscateStackSetupType2b' is not a uint32_t."
						, __func__
					);
					delete obfuscatedBinaryConfig;
					continue;
				}
				
				obfuscatedBinaryConfig->manuallyLocatedDeobfuscateStackSetupFunctions = true;
				obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b = jsonMember->GetUint();
			}
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "scanForObfuscation");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'scanForObfuscation' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->scanForObfuscation = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "patchLimitedAccuracy");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'patchLimitedAccuracy' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->patchLimitedAccuracy = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "patchRuntime");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'patchRuntime' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->patchRuntime = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonMember = FindJsonMember(&jsonObfuscatedBinary, "patchStatically");
			if (!jsonMember || !jsonMember->IsBool()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'patchStatically' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			obfuscatedBinaryConfig->patchStatically = jsonMember->GetBool();
		}
		
		{
			rapidjson::Value* jsonObfuscatedFunctionsType1 = FindJsonMember(&jsonObfuscatedBinary, "obfuscatedFunctionsType1", rapidjson::Type::kArrayType);
			if (!jsonObfuscatedFunctionsType1) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'obfuscatedFunctionsType1' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			bool parseError = false;
			
			for (auto &jsonObfuscatedFunctionType1 : jsonObfuscatedFunctionsType1->GetArray()) {
				if (!jsonObfuscatedFunctionType1.IsArray()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error: an element within 'obfuscatedFunctionsType1' is not an array."
						, __func__
					);
					parseError = true;
					break;
				}
				
				auto itrJsonObfuscatedFunctionType1 = jsonObfuscatedFunctionType1.GetArray();
				if (itrJsonObfuscatedFunctionType1.Size() != 3 && itrJsonObfuscatedFunctionType1.Size() != 4) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error: an element within 'obfuscatedFunctionsType1' is not an array 3 or 4 items in size."
						, __func__
					);
					parseError = true;
					break;
				}
				
				OBFUSCATION_TYPE_1_FUNC_INFO *obfuscationType1FuncInfo = new OBFUSCATION_TYPE_1_FUNC_INFO;
				
				for (uint8_t iVal = 0; iVal < itrJsonObfuscatedFunctionType1.Size(); iVal++) {
					
					switch (iVal) {
						case 0:
						case 1:
						case 2: {
							if (!itrJsonObfuscatedFunctionType1[iVal].IsUint()) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
									, "%s Warturd JSON config file error: element %hhd within an 'obfuscatedFunctionsType1' item is not uint32_t."
									, __func__
									, iVal
								);
								parseError = true;
							}
							break;
						}
						case 3: {
							if (!itrJsonObfuscatedFunctionType1[iVal].IsArray()) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
									, "%s Warturd JSON config file error: element %hhd within an 'obfuscatedFunctionsType1' item is not an array."
									, __func__
									, iVal
								);
								parseError = true;
							}
							break;
						}
					}
					if (parseError) {
						break;
					}
					
					switch (iVal) {
						case 0: {
							uint32_t value = itrJsonObfuscatedFunctionType1[iVal].GetUint();
							obfuscationType1FuncInfo->offsetObfuscatedData = value;
							break;
						}
						case 1: {
							uint32_t value = itrJsonObfuscatedFunctionType1[iVal].GetUint();
							obfuscationType1FuncInfo->offsetBeginning = value;
							break;
						}
						case 2: {
							uint32_t value = itrJsonObfuscatedFunctionType1[iVal].GetUint();
							obfuscationType1FuncInfo->offsetEnd = value;
							break;
						}
						case 3: {
							for (auto &jsonBinaryNopRangePatch : itrJsonObfuscatedFunctionType1[iVal].GetArray()) {
								if (!jsonBinaryNopRangePatch.IsArray()) {
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
										, "%s Warturd JSON config file error: an element within 'obfuscatedFunctionsType1' 'nopRangePatches' is not an array."
										, __func__
									);
									parseError = true;
									break;
								}
								
								auto itrJsonBinaryNopRangePatch = jsonBinaryNopRangePatch.GetArray();
								if (itrJsonBinaryNopRangePatch.Size() != 2) {
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
										, "%s Warturd JSON config file error: an element within 'obfuscatedFunctionsType1' 'nopRangePatches' is not an array 2 items in size."
										, __func__
									);
									parseError = true;
									break;
								}
								
								BINARY_NOP_RANGE *binaryNopRange = new BINARY_NOP_RANGE;
								
								for (uint8_t iVal = 0; iVal < itrJsonBinaryNopRangePatch.Size(); iVal++) {
									if (!itrJsonBinaryNopRangePatch[iVal].IsUint()) {
										XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
											, "%s Warturd JSON config file error: an element within an 'obfuscatedFunctionsType1' 'nopRangePatches' item is not uint32_t."
											, __func__
										);
										parseError = true;
										break;
									}
									
									uint32_t value = itrJsonBinaryNopRangePatch[iVal].GetUint();
									switch (iVal) {
										case 0: {
											binaryNopRange->offsetBeginning = value;
											break;
										}
										case 1: {
											binaryNopRange->offsetEnd = value;
											break;
										}
										default: {
											__debugbreak();
											break;
										}
									}
								}
								
								if (parseError) {
									delete binaryNopRange;
									break;
								}
								
								obfuscationType1FuncInfo->nopRangePatches.push_back(binaryNopRange);
							}
							break;
						}
						default: {
							__debugbreak();
							break;
						}
					}
				}
				
				if (parseError) {
					delete obfuscationType1FuncInfo;
					break;
				}
				
				obfuscatedBinaryConfig->obfuscatedFunctionsType1.push_back(obfuscationType1FuncInfo);
			}
			
			if (parseError) {
				delete obfuscatedBinaryConfig;
				continue;
			}
		}
		
		{
			rapidjson::Value* jsonBinaryNopRangePatches = FindJsonMember(&jsonObfuscatedBinary, "binaryNopRangePatches", rapidjson::Type::kArrayType);
			if (!jsonBinaryNopRangePatches) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd JSON config file error 'binaryNopRangePatches' does not exist."
					, __func__
				);
				delete obfuscatedBinaryConfig;
				continue;
			}
			
			bool parseError = false;
			
			for (auto &jsonBinaryNopRangePatch : jsonBinaryNopRangePatches->GetArray()) {
				if (!jsonBinaryNopRangePatch.IsArray()) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error: an element within 'binaryNopRangePatches' is not an array."
						, __func__
					);
					parseError = true;
					break;
				}
				
				auto itrJsonBinaryNopRangePatch = jsonBinaryNopRangePatch.GetArray();
				if (itrJsonBinaryNopRangePatch.Size() != 2) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "%s Warturd JSON config file error: an element within 'binaryNopRangePatches' is not an array 2 items in size."
						, __func__
					);
					parseError = true;
					break;
				}
				
				BINARY_NOP_RANGE *binaryNopRange = new BINARY_NOP_RANGE;
				
				for (uint8_t iVal = 0; iVal < itrJsonBinaryNopRangePatch.Size(); iVal++) {
					if (!itrJsonBinaryNopRangePatch[iVal].IsUint()) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
							, "%s Warturd JSON config file error: an element within an 'binaryNopRangePatches' item is not uint32_t."
							, __func__
						);
						parseError = true;
						break;
					}
					
					uint32_t value = itrJsonBinaryNopRangePatch[iVal].GetUint();
					switch (iVal) {
						case 0: {
							binaryNopRange->offsetBeginning = value;
							break;
						}
						case 1: {
							binaryNopRange->offsetEnd = value;
							break;
						}
						default: {
							__debugbreak();
							break;
						}
					}
				}
				
				if (parseError) {
					delete binaryNopRange;
					break;
				}
				
				obfuscatedBinaryConfig->binaryNopRangePatches.push_back(binaryNopRange);
			}
			
			if (parseError) {
				delete obfuscatedBinaryConfig;
				continue;
			}
		}
		
		{
			rapidjson::Value* jsonBinaryPatches = FindJsonMember(&jsonObfuscatedBinary, "binaryPatches", rapidjson::Type::kArrayType);
			if (jsonBinaryPatches) {
				bool parseError = false;
				
				for (auto &jsonBinaryPatch : jsonBinaryPatches->GetArray()) {
					
					BINARY_PATCH_INFO *binaryPatch = new BINARY_PATCH_INFO;
					
					{
						rapidjson::Value* jsonMember = FindJsonMember(&jsonBinaryPatch, "patchOffset", rapidjson::Type::kNumberType);
						if (!jsonMember || !jsonMember->IsUint()) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
								, "%s Warturd JSON config file error 'patchOffset' does not exist in 'binaryPatches' array item."
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
								, "%s Warturd JSON config file error 'patchData' does not exist in 'binaryPatches' array item."
								, __func__
							);
							parseError = true;
							delete binaryPatch;
							break;
						}
						
						auto itrJsonBinaryPatchData = jsonMember->GetArray();
						if (itrJsonBinaryPatchData.Size() <= 0) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
								, "%s Warturd JSON config file error: 'patchData' is an empty array."
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
									, "%s Warturd JSON config file error: an element within an 'patchData' item is not type uint32_t."
									, __func__
								);
								parseError = true;
								break;
							}
							
							uint32_t value = itrJsonBinaryPatchData[iVal].GetUint();
							if (value > 0xFF) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
									, "%s Warturd JSON config file error: an element within an 'binaryPatches' item is larger than uint8_t."
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
					
					obfuscatedBinaryConfig->binaryPatches.push_back(binaryPatch);
				}
				
				if (parseError) {
					delete obfuscatedBinaryConfig;
					continue;
				}
			}
		}
		
		obfuscated_binary_configurations.push_back(obfuscatedBinaryConfig);
		
	}
	
	return true;
}

bool ReadWarturdConfig()
{
	wchar_t *warturd_config_filepath = 0;
	
	{
		size_t storagePathBufSize = 0;
		uint32_t errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, 0, 0, &storagePathBufSize);
		if (errorGetStoragePath == ERROR_INSUFFICIENT_BUFFER) {
			warturd_storage_filepath = new wchar_t[storagePathBufSize / sizeof(wchar_t)];
			errorGetStoragePath = XLLNGetXLLNStoragePath((uint32_t)xlln_hmod_xlln_module, &xlln_local_instance_id, warturd_storage_filepath, &storagePathBufSize);
			if (errorGetStoragePath == ERROR_SUCCESS) {
				warturd_config_filepath = FormMallocString(L"%wsxlln-warturd.json", warturd_storage_filepath);
			}
			else {
				delete[] warturd_storage_filepath;
				warturd_storage_filepath = 0;
			}
		}
	}
	
	if (!warturd_config_filepath) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Unable to determine Warturd config path."
			, __func__
		);
		return false;
	}
	
	bool result = true;
	
	FILE* fileHandle = 0;
	errno_t errorFopen = _wfopen_s(&fileHandle, warturd_config_filepath, L"rb");
	if (!fileHandle) {
		if (errorFopen == ENOENT) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warturd config file not found: \"%ls\"."
				, warturd_config_filepath
			);
		}
		else {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd config file read error: %d, \"%ls\"."
				, __func__
				, errorFopen
				, warturd_config_filepath
			);
		}
		result = false;
	}
	
	free(warturd_config_filepath);
	warturd_config_filepath = 0;
	
	if (!result) {
		return false;
	}
	
	fseek(fileHandle, (long)0, SEEK_END);
	size_t fileSize = ftell(fileHandle);
	fseek(fileHandle, (long)0, SEEK_SET);
	fileSize -= ftell(fileHandle);
	uint8_t *buffer = new uint8_t[fileSize];
	size_t readC = fread(buffer, sizeof(uint8_t), fileSize / sizeof(uint8_t), fileHandle);
	
	fclose(fileHandle);
	fileHandle = 0;
	
	result = ParseWarturdConfig(buffer, readC);
	delete[] buffer;
	buffer = 0;
	
	return result;
}
