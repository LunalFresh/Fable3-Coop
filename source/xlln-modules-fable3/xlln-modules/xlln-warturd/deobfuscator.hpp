#pragma once
#include <stdint.h>
#include <map>
#include <list>
#include <vector>

typedef struct _BINARY_NOP_RANGE {
	uint32_t offsetBeginning = 0;
	uint32_t offsetEnd = 0;
} BINARY_NOP_RANGE;

typedef struct _OBFUSCATION_TYPE_1_FUNC_INFO {
	uint32_t offsetObfuscatedData = 0;
	uint32_t offsetBeginning = 0;
	uint32_t offsetEnd = 0;
	uint8_t *deobfuscatedFuncBuffer = 0;
	uint32_t deobfuscatedFuncBufferSize = 0;
	// Key: relocatable_address_offset_from_deobfuscated_function_beginning
	// Value: relocation_type.
	//  relocation_type == 2 : Immediate offset.
	//  relocation_type == 3 : Displacement offset.
	// Note the address to be relocated's value is currently set to the offset from the beginning of the module.
	std::map<uint32_t, uint8_t> relocatableAddresses;
	
	std::list<BINARY_NOP_RANGE*> nopRangePatches;
	
	~_OBFUSCATION_TYPE_1_FUNC_INFO() {
		if (deobfuscatedFuncBuffer) {
			delete[] deobfuscatedFuncBuffer;
			deobfuscatedFuncBuffer = 0;
		}
		
		deobfuscatedFuncBufferSize = 0;
		
		relocatableAddresses.clear();
		
		for (auto &binaryPatch : nopRangePatches) {
			delete binaryPatch;
		}
		nopRangePatches.clear();
	}
} OBFUSCATION_TYPE_1_FUNC_INFO;

typedef enum : uint8_t {
	tUNKNOWN = 0,
	tIF,
	tRETURN,
	tSWITCH,
	tCALL_EAX,
} OBFUSCATION_TYPE_1_BRANCH_TYPE;

typedef struct _INSTRUCTION_CONSTRUCTION_HELPER {
	uint8_t *instructionBuffer = 0;
	uint32_t instructionBufferSize = 0;
	uint32_t instructionValueDisplacementOffset = -1;
	uint32_t instructionJumpsToObfuscatedFragmentDataOffset = 0;
	uint32_t instructionValueRelocatableOffsetOffset = -1;
	uint32_t instructionRelocationDestinationValueOffset = -1;
	~_INSTRUCTION_CONSTRUCTION_HELPER() {
		if (instructionBuffer) {
			delete[] instructionBuffer;
			instructionBuffer = 0;
		}
	}
} INSTRUCTION_CONSTRUCTION_HELPER;

typedef struct _OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO {
	uint32_t obfuscatedDataOffset = 0;
	uint8_t *obfuscatedDataRuntimeAddress = 0;
	uint8_t *fragmentAddress = 0;
	uint32_t fragmentAddressSize = 0;
	// Key: relocatable_address_offset_from_fragment_beginning
	// Value: relocation_type.
	//  relocation_type == 2 : Immediate offset.
	//  relocation_type == 3 : Displacement offset.
	// Note the address to be relocated's value is currently set to the offset from the beginning of the module.
	std::map<uint32_t, uint8_t> relocatableAddresses;
	
	OBFUSCATION_TYPE_1_BRANCH_TYPE branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tUNKNOWN;
	
	std::vector<INSTRUCTION_CONSTRUCTION_HELPER*> branchTypeIfInstructions;
	
	uint16_t branchTypeReturnPopFromStack = 0;
	
	uint8_t branchTypeSwitchThirdInstructionByte = 0x00;
	uint32_t branchTypeSwitchJumpTableOffset = 0;
	
	~_OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO() {
		if (fragmentAddress) {
			delete[] fragmentAddress;
			fragmentAddress = 0;
		}
		
		relocatableAddresses.clear();
		
		for (auto &instruction : branchTypeIfInstructions) {
			delete instruction;
		}
		branchTypeIfInstructions.clear();
	}
	
} OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO;

typedef struct _BINARY_PATCH_INFO {
	uint32_t patchOffset = 0;
	uint8_t *patchData = 0;
	uint32_t patchDataSize = 0;
	
	~_BINARY_PATCH_INFO() {
		if (patchData) {
			delete[] patchData;
			patchData = 0;
		}
	}
	
} BINARY_PATCH_INFO;

typedef struct _OBFUSCATED_BINARY_CONFIG {
	wchar_t *binaryName = 0;
	char *binarySha256 = 0;
	bool binaryIsImported = false;
	bool binaryIsLoadedProgrammatically = false;
	wchar_t *resultDestinationPath = 0;
	bool scanForObfuscation = false;
	bool patchLimitedAccuracy = false;
	bool patchRuntime = false;
	bool patchStatically = false;
	
	std::vector<OBFUSCATION_TYPE_1_FUNC_INFO*> obfuscatedFunctionsType1;
	std::vector<BINARY_NOP_RANGE*> binaryNopRangePatches;
	std::vector<BINARY_PATCH_INFO*> binaryPatches;
	
	uint8_t *binaryBaseAddress = 0;
	wchar_t *binaryFilepathRuntime = 0;
	bool manuallyLocatedDeobfuscateStackSetupFunctions = false;
	uint32_t offsetFuncDeobfuscateStackSetupType1 = 0;
	uint32_t offsetFuncDeobfuscateStackSetupType2 = 0;
	uint32_t offsetFuncDeobfuscateStackSetupType2b = 0;
	uint32_t offsetFuncDeobfuscateType1 = 0;
	uint32_t offsetFuncDeobfuscateType2 = 0;
	uint32_t offsetFuncDeobfuscateType2b = 0;
	uint32_t offsetCodecaveDeobfuscateType1RecordRelocatableAddress = 0;
	uint32_t offsetCodecaveDeobfuscateType1RecordBranchingType = 0;
	uint32_t offsetCodecaveDeobfuscateType1RecordObfuscatedDataEnd = 0;
	uint32_t offsetFuncDeobfuscateType1BranchTypeIf = 0;
	uint32_t offsetFuncDeobfuscateType1BranchTypeSwitch = 0;
	uint32_t offsetFuncDeobfuscateType1BranchTypeReturn = 0;
	
	~_OBFUSCATED_BINARY_CONFIG() {
		if (binaryName) {
			delete[] binaryName;
			binaryName = 0;
		}
		
		if (binarySha256) {
			delete[] binarySha256;
			binarySha256 = 0;
		}
		
		if (resultDestinationPath) {
			delete[] resultDestinationPath;
			resultDestinationPath = 0;
		}
		
		if (binaryFilepathRuntime) {
			delete[] binaryFilepathRuntime;
			binaryFilepathRuntime = 0;
		}
		
		for (auto &obfuscatedFunctionType1 : obfuscatedFunctionsType1) {
			delete obfuscatedFunctionType1;
		}
		obfuscatedFunctionsType1.clear();
		
		for (auto &binaryPatch : binaryNopRangePatches) {
			delete binaryPatch;
		}
		binaryNopRangePatches.clear();
		
		for (auto &binaryPatch : binaryPatches) {
			delete binaryPatch;
		}
		binaryPatches.clear();
	}
} OBFUSCATED_BINARY_CONFIG;

extern std::vector<OBFUSCATED_BINARY_CONFIG*> obfuscated_binary_configurations;

void ProcessObfuscationType1Entry(
	OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig
	, uint32_t offset_obfuscated_data
	, uint8_t **deobfuscated_function_buffer
	, uint32_t *deobfuscated_function_buffer_size
	, std::map<uint32_t, uint8_t> *relocatable_addresses
	, uint32_t *offset_obfuscated_data_end
);
bool ScanForBinaryAndWarbirdOffsets(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig);
void ProcessObfuscationType1Pre(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig);
void ProcessObfuscationType1Post(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig);
void DeobfuscateFunctionsInBinary(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, std::vector<OBFUSCATION_TYPE_1_FUNC_INFO*> *obfuscated_functions_type_1);
