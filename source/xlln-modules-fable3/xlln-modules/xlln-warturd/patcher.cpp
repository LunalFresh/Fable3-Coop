#include "../dllmain.hpp"
#include "./patcher.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "./config.hpp"
#include <list>
#include <vector>
#include <map>
#include <set>

void PatchRuntime(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	if (!obfuscatedBinaryConfig->patchRuntime) {
		return;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd is now patching runtime for binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	for (auto &obfuscationType1FuncInfo : obfuscatedBinaryConfig->obfuscatedFunctionsType1) {
		if (!obfuscationType1FuncInfo->offsetBeginning || !obfuscationType1FuncInfo->offsetEnd) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd skipped patching a Type 1 function in binary file due to null value [0x%08x, 0x%08x, 0x%08x]. '%ls' '%s'."
				, __func__
				, obfuscationType1FuncInfo->offsetObfuscatedData
				, obfuscationType1FuncInfo->offsetBeginning
				, obfuscationType1FuncInfo->offsetEnd
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			continue;
		}
		if (!obfuscationType1FuncInfo->deobfuscatedFuncBuffer) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd skipped patching a Type 1 function [0x%08x, 0x%08x, 0x%08x] as it was not deobfuscated for binary file. '%ls' '%s'."
				, __func__
				, obfuscationType1FuncInfo->offsetObfuscatedData
				, obfuscationType1FuncInfo->offsetBeginning
				, obfuscationType1FuncInfo->offsetEnd
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			continue;
		}
		
		uint32_t obfuscatedFunctionSize = obfuscationType1FuncInfo->offsetEnd - obfuscationType1FuncInfo->offsetBeginning;
		uint8_t *obfuscatedFunctionBeginningRuntime = (uint8_t*)((uint32_t)(obfuscatedBinaryConfig->binaryBaseAddress) + obfuscationType1FuncInfo->offsetBeginning);
		uint8_t *obfuscatedFunctionObfuscatedDataRuntime = (uint8_t*)((uint32_t)(obfuscatedBinaryConfig->binaryBaseAddress) + obfuscationType1FuncInfo->offsetObfuscatedData);
		uint8_t *alternativeDeobfuscatedDestinationRuntime = 0;
		
		// If it does not fit in the original area.
		if (obfuscatedBinaryConfig->patchLimitedAccuracy || obfuscationType1FuncInfo->deobfuscatedFuncBufferSize > obfuscatedFunctionSize) {
			alternativeDeobfuscatedDestinationRuntime = new uint8_t[obfuscationType1FuncInfo->deobfuscatedFuncBufferSize];
			DWORD dwBack;
			VirtualProtect(alternativeDeobfuscatedDestinationRuntime, obfuscationType1FuncInfo->deobfuscatedFuncBufferSize, PAGE_EXECUTE_READWRITE, &dwBack);
		}
		
		{
			DWORD dwBack;
			VirtualProtect(obfuscatedFunctionBeginningRuntime, obfuscatedFunctionSize, PAGE_READWRITE, &dwBack);
			
			if (alternativeDeobfuscatedDestinationRuntime) {
				if (obfuscatedBinaryConfig->patchLimitedAccuracy) {
					uint8_t *jumpInstructionRuntime = (uint8_t*)((uint32_t)obfuscatedFunctionObfuscatedDataRuntime - 5);
					if (jumpInstructionRuntime < obfuscatedFunctionBeginningRuntime) {
						// It should always be big enough for a jump instruction.
						__debugbreak();
					}
					
					// NOP all the code up until the jump instruction.
					memset(obfuscatedFunctionBeginningRuntime, 0x90, obfuscationType1FuncInfo->offsetObfuscatedData - 5 - obfuscationType1FuncInfo->offsetBeginning);
					
					// Create jump instruction to the alternative buffer.
					jumpInstructionRuntime[0] = 0xE9;
					*(uint32_t*)&jumpInstructionRuntime[1] = (uint32_t)alternativeDeobfuscatedDestinationRuntime - ((uint32_t)&jumpInstructionRuntime[5]);
				}
				else {
					if (obfuscatedFunctionSize < 5) {
						// It should always be big enough for a jump instruction.
						__debugbreak();
					}
					
					// Use INT3 interupt instruction instead of NOP since if this code is executed something has definitely gone wrong.
					memset(obfuscatedFunctionBeginningRuntime, 0xCC, obfuscatedFunctionSize);
					
					// Create jump instruction to the alternative buffer.
					obfuscatedFunctionBeginningRuntime[0] = 0xE9;
					*(uint32_t*)&obfuscatedFunctionBeginningRuntime[1] = (uint32_t)alternativeDeobfuscatedDestinationRuntime - ((uint32_t)&obfuscatedFunctionBeginningRuntime[5]);
				}
			}
			else {
				uint32_t sizePadding = obfuscatedFunctionSize - obfuscationType1FuncInfo->deobfuscatedFuncBufferSize;
				memset(&obfuscatedFunctionBeginningRuntime[obfuscationType1FuncInfo->deobfuscatedFuncBufferSize], 0xCC, sizePadding);
			}
			
			// Copy the deobfuscated function in.
			memcpy(
				alternativeDeobfuscatedDestinationRuntime ? alternativeDeobfuscatedDestinationRuntime : obfuscatedFunctionBeginningRuntime
				, obfuscationType1FuncInfo->deobfuscatedFuncBuffer
				, obfuscationType1FuncInfo->deobfuscatedFuncBufferSize
			);
			// Relocate the instructions that need it.
			for (auto &relocateAddressInfo : obfuscationType1FuncInfo->relocatableAddresses) {
				uint32_t offsetToRelocatableInstruction = relocateAddressInfo.first;
				uint8_t relocationType = relocateAddressInfo.second;
				
				uint32_t &instructionRelocatableValue = *(uint32_t*)(&(alternativeDeobfuscatedDestinationRuntime ? alternativeDeobfuscatedDestinationRuntime : obfuscatedFunctionBeginningRuntime)[offsetToRelocatableInstruction]);
				if (relocationType == 2) {
					instructionRelocatableValue = (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + instructionRelocatableValue - (uint32_t)&instructionRelocatableValue;
				}
				else if (relocationType == 3) {
					instructionRelocatableValue += (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
				}
			}
			
			VirtualProtect(obfuscatedFunctionBeginningRuntime, obfuscatedFunctionSize, dwBack, &dwBack);
		}
	}
	
	for (auto &binaryPatch : obfuscatedBinaryConfig->binaryNopRangePatches) {
		uint8_t *patchAddressRuntime = (uint8_t*)((uint32_t)(obfuscatedBinaryConfig->binaryBaseAddress) + binaryPatch->offsetBeginning);
		uint32_t patchSize = binaryPatch->offsetEnd - binaryPatch->offsetBeginning;
		
		DWORD dwBack;
		VirtualProtect(patchAddressRuntime, patchSize, PAGE_READWRITE, &dwBack);
		
		// NOP instruction.
		memset(patchAddressRuntime, 0x90, patchSize);
		
		VirtualProtect(patchAddressRuntime, patchSize, dwBack, &dwBack);
	}
	
	for (auto &binaryPatch : obfuscatedBinaryConfig->binaryPatches) {
		uint8_t *patchAddressRuntime = (uint8_t*)((uint32_t)(obfuscatedBinaryConfig->binaryBaseAddress) + binaryPatch->patchOffset);
		
		DWORD dwBack;
		VirtualProtect(patchAddressRuntime, binaryPatch->patchDataSize, PAGE_READWRITE, &dwBack);
		
		memcpy(patchAddressRuntime, binaryPatch->patchData, binaryPatch->patchDataSize);
		
		VirtualProtect(patchAddressRuntime, binaryPatch->patchDataSize, dwBack, &dwBack);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd has finished patching runtime for binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
}

static bool PatchStaticallyHelper(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, uint8_t **binary_buffer, uint32_t *binary_buffer_size)
{
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)(*binary_buffer);
	
	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd binary file is not a DOS application. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		
		return false;
	}
	
	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((uint32_t)(*binary_buffer) + dos_header->e_lfanew);
	
	if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd binary file is not a valid NT Portable Executable. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		return false;
	}
	
	// Null out the Security RVA if it exists. It is removed as it is no longer valid once the binary is modified.
	if (
		nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress
		&& nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s Warturd deobfuscated binary file removing Security RVA Data Directory. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		
		memset(&(*binary_buffer)[nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress], 0, nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size);
		
		// Shorten the buffer if it was attached to the end.
		if (nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress + nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size == *binary_buffer_size) {
			*binary_buffer_size -= nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
		}
		else {
			__debugbreak();
		}
		
		nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
		nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
	}
	
	// Find the PDB string and replace all references of "warbird" with "warturd".
	if (nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress && nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size) {
		IMAGE_DEBUG_DIRECTORY *debugDirectory = (IMAGE_DEBUG_DIRECTORY*)&(*binary_buffer)[nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress];
		if (debugDirectory->Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
			DEBUG_CODEVIEW_INFO *debugCodeviewInfo = (DEBUG_CODEVIEW_INFO*)&(*binary_buffer)[debugDirectory->AddressOfRawData];
			if (memcmp(&debugCodeviewInfo->signature, "RSDS", 4) == 0) {
				while (1) {
					char *warbirdName = strstr(debugCodeviewInfo->pdbFilepath, "warbird");
					if (warbirdName) {
						memcpy(warbirdName, "warturd", 7);
						continue;
					}
					break;
				}
			}
		}
	}
	
	if (*binary_buffer_size != nt_headers->OptionalHeader.SizeOfImage) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
			, "%s Warturd deobfuscated binary file internal SizeOfImage does not equal the read buffer size. '%ls' '%s'."
			, __func__
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
	}
	
	std::set<uint32_t> binaryRelocationAddresses;
	
	for (uint32_t i = 0; i < nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size; ) {
		uint32_t relocationTableBlockBase = *(uint32_t*)&(*binary_buffer)[nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + i];
		uint32_t relocationTableBlockSize = *(uint32_t*)&(*binary_buffer)[nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + i + 4];
		for (uint32_t rBi = 0; rBi < relocationTableBlockSize - 8; rBi += 2) {
			uint16_t relocationTableBlockElement = *(uint16_t*)&(*binary_buffer)[nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + i + 8 + rBi];
			uint8_t relocationType = (relocationTableBlockElement >> 12);
			if (relocationType == 0) {
				continue;
			}
			// Assuming all the relocation addresses are IMAGE_REL_BASED_HIGHLOW Type 3 relocation types.
			if (relocationType != IMAGE_REL_BASED_HIGHLOW) {
				__debugbreak();
			}
			uint32_t value = relocationTableBlockBase + (relocationTableBlockElement & 0x0FFF);
			binaryRelocationAddresses.insert(value);
		}
		
		i += relocationTableBlockSize;
	}
	
	IMAGE_SECTION_HEADER *imageSectionHeaderRelocation = 0;
	IMAGE_SECTION_HEADER *imageSectionHeaderDeobfuscated = 0;
	{
		const char *sectionNameDeobf = ".deobf\0\0";
		const char *sectionNameReloc = ".reloc\0\0";
		uint32_t sectionAddress = 0;
		IMAGE_SECTION_HEADER *imageSectionHeaderTable = (IMAGE_SECTION_HEADER*)((uint32_t)(*binary_buffer) + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS));
		for (uint16_t iSection = 0; iSection < nt_headers->FileHeader.NumberOfSections; iSection++) {
			if (imageSectionHeaderTable[iSection].PointerToRawData < sectionAddress) {
				__debugbreak();
			}
			sectionAddress = imageSectionHeaderTable[iSection].PointerToRawData;
			if (strncmp((char*)imageSectionHeaderTable[iSection].Name, sectionNameDeobf, 8) == 0) {
				__debugbreak();
			}
			if (strncmp((char*)imageSectionHeaderTable[iSection].Name, sectionNameReloc, 8) == 0) {
				// Must be the last section.
				if (iSection != nt_headers->FileHeader.NumberOfSections - 1) {
					__debugbreak();
				}
				// 96 is the max limit of sections.
				if (iSection + 1 >= 96) {
					// We need room to add another section.
					__debugbreak();
				}
				
				nt_headers->FileHeader.NumberOfSections++;
				
				imageSectionHeaderTable[iSection + 1] = imageSectionHeaderTable[iSection];
				imageSectionHeaderDeobfuscated = &imageSectionHeaderTable[iSection];
				imageSectionHeaderRelocation = &imageSectionHeaderTable[iSection + 1];
				
				memset(imageSectionHeaderDeobfuscated, 0, sizeof(IMAGE_SECTION_HEADER));
				memcpy(imageSectionHeaderDeobfuscated->Name, sectionNameDeobf, 8);
				imageSectionHeaderDeobfuscated->PointerToRawData = imageSectionHeaderRelocation->PointerToRawData;
				imageSectionHeaderDeobfuscated->VirtualAddress = imageSectionHeaderDeobfuscated->PointerToRawData;
				imageSectionHeaderDeobfuscated->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE;
				
				*binary_buffer_size -= imageSectionHeaderRelocation->SizeOfRawData;
				
				break;
			}
		}
	}
	
	if (!imageSectionHeaderRelocation) {
		// Relocation Section not found.
		__debugbreak();
	}
	
	uint8_t *bufferSectionDeobf = 0;
	uint32_t bufferSectionDeobfSize = 0;
	do {
		if (bufferSectionDeobfSize) {
			bufferSectionDeobf = new uint8_t[bufferSectionDeobfSize];
		}
		
		uint32_t iPosBufferSectionDeobf = 0;
		
		for (auto &obfuscationType1FuncInfo : obfuscatedBinaryConfig->obfuscatedFunctionsType1) {
			if (!obfuscationType1FuncInfo->offsetBeginning || !obfuscationType1FuncInfo->offsetEnd) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd skipped patching a Type 1 function in binary file due to null value [0x%08x, 0x%08x, 0x%08x]. '%ls' '%s'."
					, __func__
					, obfuscationType1FuncInfo->offsetObfuscatedData
					, obfuscationType1FuncInfo->offsetBeginning
					, obfuscationType1FuncInfo->offsetEnd
					, obfuscatedBinaryConfig->binaryName
					, obfuscatedBinaryConfig->binarySha256
				);
				continue;
			}
			if (!obfuscationType1FuncInfo->deobfuscatedFuncBuffer) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd skipped patching a Type 1 function [0x%08x, 0x%08x, 0x%08x] as it was not deobfuscated for binary file. '%ls' '%s'."
					, __func__
					, obfuscationType1FuncInfo->offsetObfuscatedData
					, obfuscationType1FuncInfo->offsetBeginning
					, obfuscationType1FuncInfo->offsetEnd
					, obfuscatedBinaryConfig->binaryName
					, obfuscatedBinaryConfig->binarySha256
				);
				continue;
			}
			
			uint32_t obfuscatedFunctionSize = obfuscationType1FuncInfo->offsetEnd - obfuscationType1FuncInfo->offsetBeginning;
			bool deobfuscatedFitsInObfuscatedLocation = !obfuscatedBinaryConfig->patchLimitedAccuracy && obfuscationType1FuncInfo->deobfuscatedFuncBufferSize <= obfuscatedFunctionSize;
			
			if (bufferSectionDeobf) {
				uint8_t *obfuscatedFunctionBeginningRuntime = (uint8_t*)((uint32_t)(*binary_buffer) + obfuscationType1FuncInfo->offsetBeginning);
				uint8_t *alternativeDeobfuscatedDestinationRuntime = 0;
				
				// Erase any relocation addresses within the area to overwrite.
				for (uint32_t iPos = obfuscationType1FuncInfo->offsetBeginning; iPos < obfuscationType1FuncInfo->offsetEnd; iPos++) {
					binaryRelocationAddresses.erase(iPos);
				}
				
				if (deobfuscatedFitsInObfuscatedLocation) {
					uint32_t sizePadding = obfuscatedFunctionSize - obfuscationType1FuncInfo->deobfuscatedFuncBufferSize;
					memset(&obfuscatedFunctionBeginningRuntime[obfuscationType1FuncInfo->deobfuscatedFuncBufferSize], 0xCC, sizePadding);
				}
				else {
					alternativeDeobfuscatedDestinationRuntime = &bufferSectionDeobf[iPosBufferSectionDeobf];
					
					if (obfuscatedBinaryConfig->patchLimitedAccuracy) {
						uint8_t *jumpInstructionRuntime = (uint8_t*)((uint32_t)(*binary_buffer) + obfuscationType1FuncInfo->offsetObfuscatedData - 5);
						if (jumpInstructionRuntime < obfuscatedFunctionBeginningRuntime) {
							// It should always be big enough for a jump instruction.
							__debugbreak();
						}
						
						// NOP all the code up until the jump instruction.
						memset(obfuscatedFunctionBeginningRuntime, 0x90, obfuscationType1FuncInfo->offsetObfuscatedData - 5 - obfuscationType1FuncInfo->offsetBeginning);
						
						// Create jump instruction to the alternative buffer.
						jumpInstructionRuntime[0] = 0xE9;
						*(uint32_t*)&jumpInstructionRuntime[1] = (imageSectionHeaderDeobfuscated->PointerToRawData + iPosBufferSectionDeobf) - (obfuscationType1FuncInfo->offsetObfuscatedData);
					}
					else {
						if (obfuscatedFunctionSize < 5) {
							// It should always be big enough for a jump instruction.
							__debugbreak();
						}
						
						// Use INT3 interupt instruction instead of NOP since if this code is executed something has definitely gone wrong.
						memset(obfuscatedFunctionBeginningRuntime, 0xCC, obfuscatedFunctionSize);
						
						// Create jump instruction to the alternative buffer.
						obfuscatedFunctionBeginningRuntime[0] = 0xE9;
						*(uint32_t*)&obfuscatedFunctionBeginningRuntime[1] = (imageSectionHeaderDeobfuscated->PointerToRawData + iPosBufferSectionDeobf) - (obfuscationType1FuncInfo->offsetBeginning + 5);
					}
				}
				
				// Copy the deobfuscated function in.
				memcpy(
					deobfuscatedFitsInObfuscatedLocation ? obfuscatedFunctionBeginningRuntime : alternativeDeobfuscatedDestinationRuntime
					, obfuscationType1FuncInfo->deobfuscatedFuncBuffer
					, obfuscationType1FuncInfo->deobfuscatedFuncBufferSize
				);
				// Relocate the instructions that need it.
				for (auto &relocateAddressInfo : obfuscationType1FuncInfo->relocatableAddresses) {
					uint32_t offsetToRelocatableInstruction = relocateAddressInfo.first;
					uint8_t relocationType = relocateAddressInfo.second;
					
					uint32_t &instructionRelocatableValue = *(uint32_t*)(&(alternativeDeobfuscatedDestinationRuntime ? alternativeDeobfuscatedDestinationRuntime : obfuscatedFunctionBeginningRuntime)[offsetToRelocatableInstruction]);
					uint32_t offsetRelocationValue = (
						deobfuscatedFitsInObfuscatedLocation
						? obfuscationType1FuncInfo->offsetBeginning
						: imageSectionHeaderDeobfuscated->PointerToRawData + iPosBufferSectionDeobf
					);
					if (relocationType == 2) {
						instructionRelocatableValue -= offsetRelocationValue + offsetToRelocatableInstruction;
					}
					else if (relocationType == 3) {
						instructionRelocatableValue += nt_headers->OptionalHeader.ImageBase;
						binaryRelocationAddresses.insert(offsetRelocationValue + offsetToRelocatableInstruction);
					}
				}
			}
			
			if (!deobfuscatedFitsInObfuscatedLocation) {
				uint32_t paddingFunc = (obfuscationType1FuncInfo->deobfuscatedFuncBufferSize) % 0x10;
				if (paddingFunc) {
					paddingFunc = 0x10 - paddingFunc;
					
					if (bufferSectionDeobf) {
						memset(&bufferSectionDeobf[iPosBufferSectionDeobf + obfuscationType1FuncInfo->deobfuscatedFuncBufferSize], 0xCC, paddingFunc);
					}
				}
				
				iPosBufferSectionDeobf += obfuscationType1FuncInfo->deobfuscatedFuncBufferSize + paddingFunc;
			}
		}
		
		imageSectionHeaderDeobfuscated->Misc.VirtualSize = iPosBufferSectionDeobf;
		
		uint32_t paddingSection = iPosBufferSectionDeobf % nt_headers->OptionalHeader.FileAlignment;
		if (paddingSection) {
			paddingSection = nt_headers->OptionalHeader.FileAlignment - paddingSection;
			
			if (bufferSectionDeobf) {
				memset(&bufferSectionDeobf[iPosBufferSectionDeobf], 0xCC, paddingSection);
			}
			
			iPosBufferSectionDeobf += paddingSection;
		}
		
		if (bufferSectionDeobfSize && iPosBufferSectionDeobf != bufferSectionDeobfSize) {
			// Buffer size does not match the pre-calculated one.
			__debugbreak();
		}
		
		bufferSectionDeobfSize = iPosBufferSectionDeobf;
		imageSectionHeaderDeobfuscated->SizeOfRawData = bufferSectionDeobfSize;
	}
	while (!bufferSectionDeobf && bufferSectionDeobfSize);
	
	if (!bufferSectionDeobf) {
		*imageSectionHeaderDeobfuscated = *imageSectionHeaderRelocation;
		memset(imageSectionHeaderRelocation, 0, sizeof(IMAGE_SECTION_HEADER));
		imageSectionHeaderRelocation = imageSectionHeaderDeobfuscated;
		nt_headers->FileHeader.NumberOfSections--;
	}
	
	for (auto &binaryPatch : obfuscatedBinaryConfig->binaryNopRangePatches) {
		if (binaryPatch->offsetEnd > *binary_buffer_size) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd skipped patching a NOP Range in binary file due to location being on or past the relocation table [0x%08x, 0x%08x]. '%ls' '%s'."
				, __func__
				, binaryPatch->offsetBeginning
				, binaryPatch->offsetEnd
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			continue;
		}
		
		uint8_t *patchAddressRuntime = (uint8_t*)((uint32_t)(*binary_buffer) + binaryPatch->offsetBeginning);
		uint32_t patchSize = binaryPatch->offsetEnd - binaryPatch->offsetBeginning;
		
		// NOP instruction.
		memset(patchAddressRuntime, 0x90, patchSize);
		
		// Erase any relocation addresses within the area to overwrite.
		for (uint32_t iPos = binaryPatch->offsetBeginning; iPos < binaryPatch->offsetEnd; iPos++) {
			binaryRelocationAddresses.erase(iPos);
		}
	}
	
	for (auto &binaryPatch : obfuscatedBinaryConfig->binaryPatches) {
		if (binaryPatch->patchOffset + binaryPatch->patchDataSize > *binary_buffer_size) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd skipped a patch in binary file due to location being on or past the relocation table [0x%08x, 0x%08x]. '%ls' '%s'."
				, __func__
				, binaryPatch->patchOffset
				, binaryPatch->patchDataSize
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			continue;
		}
		
		uint8_t *patchAddressRuntime = (uint8_t*)((uint32_t)(*binary_buffer) + binaryPatch->patchOffset);
		
		memcpy(patchAddressRuntime, binaryPatch->patchData, binaryPatch->patchDataSize);
		
		// Erase any relocation addresses within the area to overwrite.
		for (uint32_t iPos = binaryPatch->patchOffset; iPos < binaryPatch->patchOffset + binaryPatch->patchDataSize; iPos++) {
			binaryRelocationAddresses.erase(iPos);
		}
	}
	
	uint8_t *bufferSectionReloc = 0;
	uint32_t bufferSectionRelocSize = 0;
	do {
		if (bufferSectionRelocSize) {
			bufferSectionReloc = new uint8_t[bufferSectionRelocSize];
		}
		
		std::set<uint32_t> binaryRelocationAddressesCopy;
		for (auto &relocationAddress : binaryRelocationAddresses) {
			binaryRelocationAddressesCopy.insert(relocationAddress);
		}
		
		uint32_t iPosBufferSectionReloc = 0;
		
		while (binaryRelocationAddressesCopy.size() > 0) {
			uint32_t blockBaseOffset = *binaryRelocationAddressesCopy.begin() & 0xFFFFF000;
			uint32_t blockCount = 0;
			if (bufferSectionReloc) {
				*(uint32_t*)&bufferSectionReloc[iPosBufferSectionReloc] = blockBaseOffset;
			}
			while (binaryRelocationAddressesCopy.begin() != binaryRelocationAddressesCopy.end() && (*binaryRelocationAddressesCopy.begin() & 0xFFFFF000) == blockBaseOffset) {
				uint32_t blockElement = *binaryRelocationAddressesCopy.begin();
				binaryRelocationAddressesCopy.erase(blockElement);
				
				if (bufferSectionReloc) {
					uint16_t relocValue = (blockElement & 0x0FFF) + (3 << 12);
					*(uint16_t*)&bufferSectionReloc[iPosBufferSectionReloc + 8 + (blockCount * 2)] = relocValue;
				}
				
				blockCount++;
			}
			// if it's an odd number then add an empty element to align it to a 32-bit boundary.
			if (blockCount % 2) {
				if (bufferSectionReloc) {
					*(uint16_t*)&bufferSectionReloc[iPosBufferSectionReloc + 8 + (blockCount * 2)] = 0;
				}
				blockCount++;
			}
			if (bufferSectionReloc) {
				*(uint32_t*)&bufferSectionReloc[iPosBufferSectionReloc + 4] = (blockCount*2) + 8;
			}
			iPosBufferSectionReloc += 8 + (blockCount * 2);
		}
		
		imageSectionHeaderRelocation->Misc.VirtualSize = iPosBufferSectionReloc;
		
		uint32_t paddingSection = iPosBufferSectionReloc % nt_headers->OptionalHeader.FileAlignment;
		if (paddingSection) {
			paddingSection = nt_headers->OptionalHeader.FileAlignment - paddingSection;
			
			if (bufferSectionReloc) {
				memset(&bufferSectionReloc[iPosBufferSectionReloc], 0x00, paddingSection);
			}
			
			iPosBufferSectionReloc += paddingSection;
		}
		
		if (bufferSectionRelocSize && iPosBufferSectionReloc != bufferSectionRelocSize) {
			// Buffer size does not match the pre-calculated one.
			__debugbreak();
		}
		
		bufferSectionRelocSize = iPosBufferSectionReloc;
		imageSectionHeaderRelocation->SizeOfRawData = bufferSectionRelocSize;
	}
	while (!bufferSectionReloc && bufferSectionRelocSize);
	
	imageSectionHeaderRelocation->VirtualAddress = imageSectionHeaderDeobfuscated->VirtualAddress + bufferSectionDeobfSize;
	imageSectionHeaderRelocation->PointerToRawData = imageSectionHeaderDeobfuscated->PointerToRawData + bufferSectionDeobfSize;
	
	nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = imageSectionHeaderRelocation->VirtualAddress;
	nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = imageSectionHeaderRelocation->Misc.VirtualSize;
	
	uint8_t *binaryBufferOld = (*binary_buffer);
	uint32_t binaryBufferOldSize = (*binary_buffer_size);
	
	(*binary_buffer_size) = binaryBufferOldSize + bufferSectionDeobfSize + bufferSectionRelocSize;
	nt_headers->OptionalHeader.SizeOfImage = (*binary_buffer_size);
	
	// Combine all the parts into the whole binary buffer.
	(*binary_buffer) = new uint8_t[(*binary_buffer_size)];
	memcpy((*binary_buffer), binaryBufferOld, binaryBufferOldSize);
	if (bufferSectionDeobf) {
		memcpy(&(*binary_buffer)[binaryBufferOldSize], bufferSectionDeobf, bufferSectionDeobfSize);
	}
	memcpy(&(*binary_buffer)[binaryBufferOldSize + bufferSectionDeobfSize], bufferSectionReloc, bufferSectionRelocSize);
	
	delete[] binaryBufferOld;
	binaryBufferOld = 0;
	if (bufferSectionDeobf) {
		delete[] bufferSectionDeobf;
		bufferSectionDeobf = 0;
	}
	delete[] bufferSectionReloc;
	bufferSectionReloc = 0;
	
	dos_header = (IMAGE_DOS_HEADER*)(*binary_buffer);
	nt_headers = (IMAGE_NT_HEADERS*)((uint32_t)(*binary_buffer) + dos_header->e_lfanew);
	nt_headers->OptionalHeader.CheckSum = GetPEChecksum(*binary_buffer, *binary_buffer_size, (uint32_t)&(nt_headers->OptionalHeader.CheckSum) - (uint32_t)(*binary_buffer));
	
	return true;
}

void PatchStatically(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	if (!obfuscatedBinaryConfig->patchStatically) {
		return;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd is now statically patching a deobfuscated version of the binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	if (!obfuscatedBinaryConfig->binaryFilepathRuntime) {
		XLLN_DEBUG_LOG(
			XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "Warbird binary filepath not specified."
		);
		return;
	}
	
	FILE* fileHandle = 0;
	errno_t errorFopen = _wfopen_s(&fileHandle, obfuscatedBinaryConfig->binaryFilepathRuntime, L"rb");
	if (!fileHandle) {
		if (errorFopen == ENOENT) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warbird binary file not found: \"%ls\"."
				, obfuscatedBinaryConfig->binaryFilepathRuntime
			);
		}
		else {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warbird binary file read error: %d, \"%ls\"."
				, __func__
				, errorFopen
				, obfuscatedBinaryConfig->binaryFilepathRuntime
			);
		}
		return;
	}
	
	fseek(fileHandle, (long)0, SEEK_END);
	size_t fileSize = ftell(fileHandle);
	fseek(fileHandle, (long)0, SEEK_SET);
	fileSize -= ftell(fileHandle);
	uint8_t *buffer = new uint8_t[fileSize];
	size_t readC = fread(buffer, sizeof(uint8_t), fileSize / sizeof(uint8_t), fileHandle);
	
	fclose(fileHandle);
	fileHandle = 0;
	
	uint32_t bufferSize = readC;
	PatchStaticallyHelper(obfuscatedBinaryConfig, &buffer, &bufferSize);
	
	wchar_t *binaryFilepathDeobfuscated = FormMallocString(
		L"%ws%wsDeobfuscated-%hs-%ws"
		, warturd_storage_filepath
		, obfuscatedBinaryConfig->resultDestinationPath
		, obfuscatedBinaryConfig->binarySha256
		, obfuscatedBinaryConfig->binaryName
	);
	
	fileHandle = 0;
	errorFopen = _wfopen_s(&fileHandle, binaryFilepathDeobfuscated, L"wb");
	if (!fileHandle) {
		if (errorFopen == EACCES || errorFopen == EIO || errorFopen == EPERM) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warturd failed to open file for writing due to insufficient permissions: \"%ls\"."
				, binaryFilepathDeobfuscated
			);
		}
		else if (errorFopen == ESRCH) {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "Warturd failed to open file for writing due to path not existing: \"%ls\"."
				, binaryFilepathDeobfuscated
			);
		}
		else {
			XLLN_DEBUG_LOG(
				XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd failed to open file for writing with error: %d, \"%ls\"."
				, __func__
				, errorFopen
				, binaryFilepathDeobfuscated
			);
		}
		
		free(binaryFilepathDeobfuscated);
		binaryFilepathDeobfuscated = 0;
		
		return;
	}
	
	fwrite(buffer, sizeof(uint8_t), bufferSize, fileHandle);
	
	fclose(fileHandle);
	fileHandle = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd exported the deobfuscated binary file: '%ls' '%ls' '%s'."
		, __func__
		, binaryFilepathDeobfuscated
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	free(binaryFilepathDeobfuscated);
	binaryFilepathDeobfuscated = 0;
	
	delete[] buffer;
	buffer = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd has finished statically patching a deobfuscated version of the binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
}
