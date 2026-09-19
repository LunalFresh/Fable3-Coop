#include "../dllmain.hpp"
#include "./scanner.hpp"
#include "./config.hpp"
#include "./deobfuscator.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include <list>
#include <vector>
#include <map>
#include <set>

bool ScanForObfuscationInBinary(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	if (!obfuscatedBinaryConfig->scanForObfuscation) {
		return true;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd is now scanning for obfuscation in binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)obfuscatedBinaryConfig->binaryBaseAddress;
	
	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd binary file is not a DOS application. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		
		return false;
	}
	
	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + dos_header->e_lfanew);
	
	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd binary file is not a valid NT Portable Executable. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		return false;
	}
	
	uint16_t imageSectionHeaderTableSectionCount = nt_headers->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER *imageSectionHeaderTable = (IMAGE_SECTION_HEADER*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	
	std::vector<IMAGE_SECTION_HEADER*> imageSectionHeadersForScanning;
	
	for (uint16_t iSection = 0; iSection < imageSectionHeaderTableSectionCount; iSection++) {
		if (imageSectionHeaderTable[iSection].Characteristics & IMAGE_SCN_CNT_CODE) {
			imageSectionHeadersForScanning.push_back(&imageSectionHeaderTable[iSection]);
		}
		// TODO allow the config to specify.
		//if (strncmp((char*)imageSectionHeaderTable[iSection].Name, ".text", 8) == 0) {
		//	imageSectionHeadersForScanning.push_back(&imageSectionHeaderTable[iSection]);
		//}
	}
	
	std::list<OBFUSCATION_SCAN_RESULT*> obfuscationScanResults;
	
	for (auto &imageSectionHeader : imageSectionHeadersForScanning) {
		const uint8_t *assemblySearch = (const uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + imageSectionHeader->PointerToRawData);
		DWORD dwBack;
		VirtualProtect((void*)assemblySearch, imageSectionHeader->SizeOfRawData, PAGE_EXECUTE_READWRITE, &dwBack);
		ScanForObfuscation(obfuscatedBinaryConfig, (const uint8_t*)assemblySearch, imageSectionHeader->SizeOfRawData, &obfuscationScanResults);
		VirtualProtect((void*)assemblySearch, imageSectionHeader->SizeOfRawData, dwBack, &dwBack);
	}
	
	if (obfuscationScanResults.size() == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
			, "%s Warturd scanned the binary file and found no obfuscation. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		
		return true;
	}
	
	wchar_t *scanFilepath = FormMallocString(
		L"%ws%wsObfuscation-Scan-%hs-%ws.txt"
		, warturd_storage_filepath
		, obfuscatedBinaryConfig->resultDestinationPath
		, obfuscatedBinaryConfig->binarySha256
		, obfuscatedBinaryConfig->binaryName
	);
	
	FILE* fileHandle = 0;
	errno_t errorFopen = _wfopen_s(&fileHandle, scanFilepath, L"wb");
	if (!fileHandle) {
		if (errorFopen == EACCES || errorFopen == EIO || errorFopen == EPERM) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warturd failed to open file for writing due to insufficient permissions: \"%ls\"."
				, scanFilepath
			);
		}
		else if (errorFopen == ESRCH) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warturd failed to open file for writing due to path not existing: \"%ls\"."
				, scanFilepath
			);
		}
		else {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd failed to open file for writing with error: %d, \"%ls\"."
				, __func__
				, errorFopen
				, scanFilepath
			);
		}
		
		free(scanFilepath);
		scanFilepath = 0;
		
		return false;
	}
	
	for (uint8_t iType = 1; iType <= 3; iType++) {
		uint32_t offsetObfuscationFunction = 0;
		switch (iType) {
			case 1: {
				fputs("// Obfuscation Type 1:\n", fileHandle);
				fputs("// [obfuscatedData, beginning, end],\n", fileHandle);
				offsetObfuscationFunction = obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1;
				break;
			}
			case 2: {
				fputs("// Obfuscation Type 2:\n", fileHandle);
				fputs("//  obfuscatedData is nulled out if consecutive Type 2 entries have been combined.\n", fileHandle);
				fputs("// [obfuscatedData, beginning, end],\n", fileHandle);
				offsetObfuscationFunction = obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2;
				break;
			}
			case 3: {
				fputs("// Obfuscation Unknown Type:\n", fileHandle);
				break;
			}
			default: {
				__debugbreak();
				break;
			}
		}
		
		for (auto &obfuscationScanResult : obfuscationScanResults) {
			if (
				obfuscationScanResult->offsetObfuscationFunction == offsetObfuscationFunction
				|| (
					iType == 3
					&& obfuscationScanResult->offsetObfuscationFunction != obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1
					&& obfuscationScanResult->offsetObfuscationFunction != obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2
				)
			) {
				if (obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1 && obfuscationScanResult->nopRangePatches.size()) {
					char *obfuscationEntry = FormMallocString(
						"\n[0x%08x, 0x%08x, 0x%08x, ["
						, obfuscationScanResult->addressPostRetn ? (uint32_t)obfuscationScanResult->addressPostRetn - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
						, obfuscationScanResult->addressBeginning ? (uint32_t)obfuscationScanResult->addressBeginning - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
						, obfuscationScanResult->addressEnd ? (uint32_t)obfuscationScanResult->addressEnd - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
					);
					fputs(obfuscationEntry, fileHandle);
					for (auto &nopRangePatch : obfuscationScanResult->nopRangePatches) {
						char *obfuscationEntry2 = FormMallocString(
							"[0x%08x, 0x%08x], "
							, nopRangePatch->offsetBeginning
							, nopRangePatch->offsetEnd
						);
						fputs(obfuscationEntry2, fileHandle);
						free(obfuscationEntry2);
					}
					fputs("]],", fileHandle);
					free(obfuscationEntry);
				}
				else {
					char *obfuscationEntry = FormMallocString(
						"\n[0x%08x, 0x%08x, 0x%08x],"
						, obfuscationScanResult->addressPostRetn ? (uint32_t)obfuscationScanResult->addressPostRetn - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
						, obfuscationScanResult->addressBeginning ? (uint32_t)obfuscationScanResult->addressBeginning - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
						, obfuscationScanResult->addressEnd ? (uint32_t)obfuscationScanResult->addressEnd - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress : 0
					);
					fputs(obfuscationEntry, fileHandle);
					free(obfuscationEntry);
				}
			}
		}
		fputs("\n\n\n", fileHandle);
	}
	
	fclose(fileHandle);
	fileHandle = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd exported the scan results to file: '%ls' '%ls' '%s'."
		, __func__
		, scanFilepath
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	for (auto &obfuscationScanResult : obfuscationScanResults) {
		delete obfuscationScanResult;
	}
	obfuscationScanResults.clear();
	
	free(scanFilepath);
	scanFilepath = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd has finished scanning the binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	return true;
}

void ScanForObfuscation(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, const uint8_t *assembly_search_start, const uint32_t assembly_search_size, std::list<OBFUSCATION_SCAN_RESULT*> *obfuscation_scan_results)
{
	std::vector<uint64_t> obfuscationRetnMatches;
	{
		const uint8_t *assemblySearch = assembly_search_start;
		uint32_t assemblySearchSize = assembly_search_size;
		const uint8_t signatureOpCodesRetn[] = { 0xC2, 0x04, 0x00 };
		while (1) {
			const uint8_t *assemblySearchResult = FindBytes(assemblySearch, assemblySearchSize, signatureOpCodesRetn, sizeof(signatureOpCodesRetn), true);
			if (!assemblySearchResult) {
				break;
			}
			
			uint64_t combined = 0;
			((uint32_t*)(&combined))[0] = (uint32_t)assemblySearchResult;
			((uint32_t*)(&combined))[1] = (uint32_t)assemblySearchResult + 3;
			obfuscationRetnMatches.push_back(combined);
			
			assemblySearchSize = assembly_search_size - ((uint32_t)assemblySearchResult - (uint32_t)assembly_search_start) - sizeof(signatureOpCodesRetn);
			assemblySearch = (uint8_t*)((uint32_t)assemblySearchResult + sizeof(signatureOpCodesRetn));
		}
	}
	{
		const uint8_t *assemblySearch = assembly_search_start;
		uint32_t assemblySearchSize = assembly_search_size;
		const uint8_t signatureOpCodesRetn[] = { 0xC3 };
		while (1) {
			const uint8_t *assemblySearchResult = FindBytes(assemblySearch, assemblySearchSize, signatureOpCodesRetn, sizeof(signatureOpCodesRetn), true);
			if (!assemblySearchResult) {
				break;
			}
			
			uint64_t combined = 0;
			((uint32_t*)(&combined))[0] = (uint32_t)assemblySearchResult;
			((uint32_t*)(&combined))[1] = (uint32_t)assemblySearchResult + 1;
			obfuscationRetnMatches.push_back(combined);
			
			assemblySearchSize = assembly_search_size - ((uint32_t)assemblySearchResult - (uint32_t)assembly_search_start) - sizeof(signatureOpCodesRetn);
			assemblySearch = (uint8_t*)((uint32_t)assemblySearchResult + sizeof(signatureOpCodesRetn));
		}
	}
	
	std::map<const uint8_t *, OBFUSCATION_SCAN_RESULT*> orderedObfuscationScanResults;
	
	for (auto &addressFound : obfuscationRetnMatches) {
		std::list<OBFUSCATION_BLOCK_SCAN*> assemblyPossibilities;
		{
			OBFUSCATION_BLOCK_SCAN *assemblyPossibility = new OBFUSCATION_BLOCK_SCAN;
			assemblyPossibility->assemblyInstructions.push_back((const uint8_t *)((uint32_t*)(&addressFound))[0]);
			assemblyPossibility->addressPostRetn = (const uint8_t *)((uint32_t*)(&addressFound))[1];
			assemblyPossibilities.push_back(assemblyPossibility);
		}
		
		std::vector<OBFUSCATION_BLOCK_SCAN*> obfuscationBlockScanResultOptions;
		while (assemblyPossibilities.size() > 0) {
			OBFUSCATION_BLOCK_SCAN *assemblyPossibility = *assemblyPossibilities.begin();
			assemblyPossibilities.pop_front();
			
			bool foundAnother = false;
			const uint8_t *previousInstruction = *--assemblyPossibility->assemblyInstructions.end();
			
			// 7 is the max length an instruction we are looking for is going to be.
			for (uint8_t iPos = 1; iPos <= 7; iPos++) {
				const uint8_t *assemblySearch = (const uint8_t*)((uint32_t)previousInstruction - iPos);
				if (assemblySearch < assembly_search_start) {
					// Cannot search past the beginning.
					break;
				}
				
				ZydisDecoder decoder;
				ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
				
				ZydisDecodedInstruction instruction;
				ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
				ZyanStatus zStat = ZydisDecoderDecodeFull(&decoder, assemblySearch, -1, &instruction, operands);
				if (!ZYAN_SUCCESS(zStat)) {
					continue;
				}
				
				if ((uint32_t)assemblySearch + instruction.length != (uint32_t)previousInstruction) {
					// Instruction is too long and overlaps the previous one or is not long enough to fill the space until the next instruction.
					continue;
				}
				
				bool isValid = false;
				uint32_t offsetObfuscationFunction = assemblyPossibility->offsetObfuscationFunction;
				const uint8_t *addressObfuscatedData = assemblyPossibility->addressObfuscatedData;
				ZydisRegister secondRegister = assemblyPossibility->secondRegister;
				
				switch (instruction.mnemonic) {
					case ZYDIS_MNEMONIC_PUSH:
					case ZYDIS_MNEMONIC_XCHG:
					case ZYDIS_MNEMONIC_LEA:
					case ZYDIS_MNEMONIC_MOV: {
						if (instruction.raw.prefix_count > 0) {
							break;
						}
						
						isValid = true;
						for (uint8_t iOperand = 0; iOperand < instruction.operand_count; iOperand++) {
							ZydisRegister instructionOperandRegister = ZYDIS_REGISTER_NONE;
							
							if (operands[iOperand].type == ZYDIS_OPERAND_TYPE_REGISTER) {
								instructionOperandRegister = operands[iOperand].reg.value;
							}
							else if (operands[iOperand].type == ZYDIS_OPERAND_TYPE_MEMORY) {
								instructionOperandRegister = operands[iOperand].mem.base;
							}
							
							if (instructionOperandRegister != ZYDIS_REGISTER_NONE) {
								if (secondRegister == ZYDIS_REGISTER_NONE) {
									if (instructionOperandRegister != ZYDIS_REGISTER_ESP) {
										secondRegister = instructionOperandRegister;
									}
								}
								else if (instructionOperandRegister != secondRegister && instructionOperandRegister != ZYDIS_REGISTER_ESP) {
									isValid = false;
									break;
								}
							}
						}
						break;
					}
				}
				
				if (!isValid) {
					continue;
				}
				
				if (
					instruction.mnemonic == ZYDIS_MNEMONIC_LEA
					&& instruction.operand_count == 2
					&& operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY
					&& operands[1].mem.base == ZYDIS_REGISTER_NONE
					&& operands[1].mem.disp.has_displacement
				) {
					ZydisDecodedInstruction instructionPrev;
					ZydisDecodedOperand operandsPrev[ZYDIS_MAX_OPERAND_COUNT];
					zStat = ZydisDecoderDecodeFull(&decoder, previousInstruction, -1, &instructionPrev, operandsPrev);
					if (!ZYAN_SUCCESS(zStat)) {
						continue;
					}
					
					if (
						instructionPrev.mnemonic == ZYDIS_MNEMONIC_LEA
						&& instructionPrev.operand_count == 2
						&& operandsPrev[1].type == ZYDIS_OPERAND_TYPE_MEMORY
						&& operandsPrev[1].mem.base == assemblyPossibility->secondRegister
						&& operandsPrev[1].mem.disp.has_displacement
					) {
						if (!offsetObfuscationFunction && !addressObfuscatedData) {
							offsetObfuscationFunction = (uint32_t)(operands[1].mem.disp.value + operandsPrev[1].mem.disp.value);
							if (offsetObfuscationFunction != obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b) {
								offsetObfuscationFunction -= (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
							}
						}
						else if (offsetObfuscationFunction && !addressObfuscatedData) {
							addressObfuscatedData = (const uint8_t*)(operands[1].mem.disp.value + operandsPrev[1].mem.disp.value);
							if (assemblyPossibility->addressPostRetn != addressObfuscatedData) {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
									, "%s Warturd found an obfuscated function (Type 0x%08x) but the location of the obfuscated data (0x%08x) is not the address directly after the return instruction (0x%08x). '%ls' '%s'."
									, __func__
									, assemblyPossibility->offsetObfuscationFunction
									, addressObfuscatedData
									, assemblyPossibility->addressPostRetn
									, obfuscatedBinaryConfig->binaryName
									, obfuscatedBinaryConfig->binarySha256
								);
								
								addressObfuscatedData = assemblyPossibility->addressPostRetn;
							}
						}
						else {
							// Already found the obfuscation setup values yet there is another. Something is amiss.
							continue;
						}
					}
				}
				
				foundAnother = true;
				{
					OBFUSCATION_BLOCK_SCAN *assemblyPossibilityInner = new OBFUSCATION_BLOCK_SCAN;
					assemblyPossibilityInner->offsetObfuscationFunction = offsetObfuscationFunction;
					assemblyPossibilityInner->addressObfuscatedData = addressObfuscatedData;
					assemblyPossibilityInner->addressPostRetn = assemblyPossibility->addressPostRetn;
					assemblyPossibilityInner->secondRegister = secondRegister;
					for (auto &assemblyInstruction : assemblyPossibility->assemblyInstructions) {
						assemblyPossibilityInner->assemblyInstructions.push_back(assemblyInstruction);
					}
					assemblyPossibilityInner->assemblyInstructions.push_back(assemblySearch);
					
					assemblyPossibilities.push_back(assemblyPossibilityInner);
				}
			}
			
			if (!foundAnother && assemblyPossibility->offsetObfuscationFunction && assemblyPossibility->addressObfuscatedData) {
				obfuscationBlockScanResultOptions.push_back(assemblyPossibility);
			}
			else {
				delete assemblyPossibility;
			}
		}
		
		if (obfuscationBlockScanResultOptions.size() == 0) {
			continue;
		}
		
		OBFUSCATION_BLOCK_SCAN *obfuscationBlockScanResult = 0;
		for (auto &obfuscationBlockScanResultOption : obfuscationBlockScanResultOptions) {
			
			// Remove instructions found at the beginning if they do not refer to a register.
			auto itrInstructions = --obfuscationBlockScanResultOption->assemblyInstructions.end();
			do {
				const uint8_t *assemblySearch = *itrInstructions;
				
				ZydisDecoder decoder;
				ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
				
				ZydisDecodedInstruction instruction;
				ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
				ZyanStatus zStat = ZydisDecoderDecodeFull(&decoder, assemblySearch, -1, &instruction, operands);
				if (!ZYAN_SUCCESS(zStat)) {
					break;
				}
				
				bool removeInstruction = true;
				
				switch (instruction.mnemonic) {
					case ZYDIS_MNEMONIC_PUSH:
					case ZYDIS_MNEMONIC_XCHG:
					case ZYDIS_MNEMONIC_LEA:
					case ZYDIS_MNEMONIC_MOV: {
						ZydisRegister instructionOperandRegister = ZYDIS_REGISTER_NONE;
						for (uint8_t iOperand = 0; iOperand < instruction.operand_count; iOperand++) {
							if (operands[iOperand].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT) {
								continue;
							}
							if (operands[iOperand].type == ZYDIS_OPERAND_TYPE_REGISTER) {
								if (operands[iOperand].reg.value != ZYDIS_REGISTER_ESP) {
									instructionOperandRegister = operands[iOperand].reg.value;
								}
							}
							else if (operands[iOperand].type == ZYDIS_OPERAND_TYPE_MEMORY) {
								instructionOperandRegister = operands[iOperand].mem.base;
							}
							
							if (instructionOperandRegister != ZYDIS_REGISTER_NONE) {
								break;
							}
						}
						if (instructionOperandRegister == ZYDIS_REGISTER_NONE) {
							removeInstruction = true;
						}
						else {
							removeInstruction = false;
						}
						break;
					}
				}
				
				if (removeInstruction) {
					obfuscationBlockScanResultOption->assemblyInstructions.erase(itrInstructions--);
				}
				else {
					itrInstructions--;
				}
				
			} while (itrInstructions != obfuscationBlockScanResultOption->assemblyInstructions.begin());
			
			if (!obfuscationBlockScanResult) {
				obfuscationBlockScanResult = obfuscationBlockScanResultOption;
				continue;
			}
			
			if (
				// Pick the result with the most instructions found.
				obfuscationBlockScanResultOption->assemblyInstructions.size() > obfuscationBlockScanResult->assemblyInstructions.size()
				// Or the result with the longest block.
				|| (
					obfuscationBlockScanResultOption->assemblyInstructions.size() == obfuscationBlockScanResult->assemblyInstructions.size()
					&& *(--obfuscationBlockScanResultOption->assemblyInstructions.end()) < *(--obfuscationBlockScanResult->assemblyInstructions.end())
				)
			) {
				delete obfuscationBlockScanResult;
				obfuscationBlockScanResult = obfuscationBlockScanResultOption;
				continue;
			}
			
			delete obfuscationBlockScanResultOption;
		}
		obfuscationBlockScanResultOptions.clear();
		
		OBFUSCATION_SCAN_RESULT *obfuscationScanResult = new OBFUSCATION_SCAN_RESULT;
		obfuscationScanResult->offsetObfuscationFunction = obfuscationBlockScanResult->offsetObfuscationFunction;
		obfuscationScanResult->addressPostRetn = obfuscationBlockScanResult->addressPostRetn;
		obfuscationScanResult->addressObfuscatedData = obfuscationBlockScanResult->addressObfuscatedData;
		obfuscationScanResult->addressBeginning = (const uint8_t*)*(--obfuscationBlockScanResult->assemblyInstructions.end());
		obfuscationScanResult->addressEnd = 0;
		
		delete obfuscationBlockScanResult;
		obfuscationBlockScanResult = 0;
		
		orderedObfuscationScanResults[obfuscationScanResult->addressPostRetn] = obfuscationScanResult;
	}
	
	// Add the ordered items to the result list.
	for (auto &obfuscationScanResult : orderedObfuscationScanResults) {
		(*obfuscation_scan_results).push_back(obfuscationScanResult.second);
	}
	
	// Find the addressEnd.
	
	uint32_t dataDeobfuscateType2[] = { 0, 0, 0, 0 };
	uint32_t(__fastcall* FuncDeobfuscateType2)(uint32_t*) = (uint32_t(__fastcall*)(uint32_t*))((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType2);
	uint32_t(__fastcall* FuncDeobfuscateType2b)(uint32_t*) = (uint32_t(__fastcall*)(uint32_t*))((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType2b);
	
	for (auto &obfuscationScanResult : *obfuscation_scan_results) {
		if (obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2) {
			dataDeobfuscateType2[1] = (uint32_t)obfuscationScanResult->addressObfuscatedData;
			uint8_t *addressNextBlock = (uint8_t*)FuncDeobfuscateType2(dataDeobfuscateType2);
			if (addressNextBlock) {
				obfuscationScanResult->addressEnd = (uint8_t*)dataDeobfuscateType2[1];
			}
			else {
				__debugbreak();
			}
		}
		else if (obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b) {
			dataDeobfuscateType2[1] = (uint32_t)obfuscationScanResult->addressObfuscatedData;
			uint8_t *addressNextBlock = (uint8_t*)FuncDeobfuscateType2b(dataDeobfuscateType2);
			if (addressNextBlock) {
				obfuscationScanResult->addressEnd = (uint8_t*)dataDeobfuscateType2[1];
			}
			else {
				__debugbreak();
			}
		}
		else if (obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1) {
			uint8_t *deobfuscatedFuncBuffer = 0;
			uint32_t deobfuscatedFuncBufferSize = 0;
			std::map<uint32_t, uint8_t> relocatableAddresses;
			
			// Because of unplanned recursion on ScanForObfuscation(...) Pre and Post need to be inside the loop unfortunately.
			ProcessObfuscationType1Pre(obfuscatedBinaryConfig);
			
			ProcessObfuscationType1Entry(
				obfuscatedBinaryConfig
				// Add 3 since the obfuscated data chunk starts immediately after the return instruction which is 3 bytes long.
				, (uint32_t)obfuscationScanResult->addressObfuscatedData - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress
				, &deobfuscatedFuncBuffer
				, &deobfuscatedFuncBufferSize
				, &relocatableAddresses
				, (uint32_t*)&obfuscationScanResult->addressEnd
			);
			
			ProcessObfuscationType1Post(obfuscatedBinaryConfig);
			
			if (obfuscationScanResult->addressEnd) {
				obfuscationScanResult->addressEnd += (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
				if (obfuscationScanResult->addressEnd < obfuscationScanResult->addressObfuscatedData) {
					obfuscationScanResult->addressEnd = obfuscationScanResult->addressObfuscatedData;
				}
			}
			
			std::list<OBFUSCATION_SCAN_RESULT*> obfuscationScanInnerResults;
			ScanForObfuscation(obfuscatedBinaryConfig, deobfuscatedFuncBuffer, deobfuscatedFuncBufferSize, &obfuscationScanInnerResults);
			
			for (auto &obfuscationScanInnerResult : obfuscationScanInnerResults) {
				if (obfuscationScanInnerResult->offsetObfuscationFunction != obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b) {
					__debugbreak();
				}
				
				BINARY_NOP_RANGE *binaryNopRange = new BINARY_NOP_RANGE;
				binaryNopRange->offsetBeginning = (uint32_t)obfuscationScanInnerResult->addressBeginning - (uint32_t)deobfuscatedFuncBuffer;
				binaryNopRange->offsetEnd = (uint32_t)obfuscationScanInnerResult->addressEnd - (uint32_t)deobfuscatedFuncBuffer;
				
				obfuscationScanResult->nopRangePatches.push_back(binaryNopRange);
			}
			
			for (auto &obfuscationScanResult : obfuscationScanInnerResults) {
				delete obfuscationScanResult;
			}
			obfuscationScanInnerResults.clear();
			
			relocatableAddresses.clear();
			if (deobfuscatedFuncBuffer) {
				delete[] deobfuscatedFuncBuffer;
				deobfuscatedFuncBuffer = 0;
			}
		}
	}
	
	// Combine adjacent Type 2's and fix Beginning offsets.
	for (auto itrObfuscationScanResult = obfuscation_scan_results->begin(); itrObfuscationScanResult != obfuscation_scan_results->end();) {
		OBFUSCATION_SCAN_RESULT *obfuscationScanResult = *itrObfuscationScanResult;
		itrObfuscationScanResult++;
		while (itrObfuscationScanResult != obfuscation_scan_results->end()) {
			OBFUSCATION_SCAN_RESULT *obfuscationScanResultNext = *itrObfuscationScanResult;
			
			// Fix the beginning of the next entry if the previous end goes past it.
			if (
				obfuscationScanResult->addressEnd
				&& obfuscationScanResultNext->addressBeginning
				&& obfuscationScanResultNext->addressObfuscatedData
				&& obfuscationScanResult->addressEnd > obfuscationScanResultNext->addressBeginning
				&& obfuscationScanResult->addressEnd < obfuscationScanResultNext->addressObfuscatedData
			) {
				obfuscationScanResultNext->addressBeginning = obfuscationScanResult->addressEnd;
			}
			
			// Fix the end of the current entry if it goes past the obfuscated data of the next.
			if (
				obfuscationScanResult->addressEnd
				&& obfuscationScanResultNext->addressBeginning
				&& obfuscationScanResultNext->addressObfuscatedData
				&& obfuscationScanResult->addressEnd > obfuscationScanResultNext->addressObfuscatedData
			) {
				obfuscationScanResult->addressEnd = obfuscationScanResultNext->addressBeginning;
			}
			
			if (obfuscationScanResult->offsetObfuscationFunction != obfuscationScanResultNext->offsetObfuscationFunction) {
				break;
			}
			
			// Combine Type 2's.
			if (
				obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2
				&& obfuscationScanResult->addressEnd == obfuscationScanResultNext->addressBeginning
			) {
				obfuscationScanResult->addressObfuscatedData = 0;
				obfuscationScanResult->addressPostRetn = 0;
				obfuscationScanResult->addressEnd = obfuscationScanResultNext->addressEnd;
				
				delete obfuscationScanResultNext;
				obfuscationScanResultNext = 0;
				obfuscation_scan_results->erase(itrObfuscationScanResult++);
			}
			// Combine Type 2b's.
			else if (
				obfuscationScanResult->offsetObfuscationFunction == obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b
				&& obfuscationScanResult->addressEnd == obfuscationScanResultNext->addressBeginning
			) {
				obfuscationScanResult->addressObfuscatedData = 0;
				obfuscationScanResult->addressPostRetn = 0;
				obfuscationScanResult->addressEnd = obfuscationScanResultNext->addressEnd;
				
				delete obfuscationScanResultNext;
				obfuscationScanResultNext = 0;
				obfuscation_scan_results->erase(itrObfuscationScanResult++);
			}
			else {
				break;
			}
		}
	}
}
