#pragma once
#include "./deobfuscator.hpp"
#include "../third-party/zydis/Zydis.h"
#include <list>

typedef struct _OBFUSCATION_BLOCK_SCAN {
	uint32_t offsetObfuscationFunction = 0;
	const uint8_t *addressObfuscatedData = 0;
	const uint8_t *addressPostRetn = 0;
	ZydisRegister secondRegister = ZYDIS_REGISTER_NONE;
	std::list<const uint8_t*> assemblyInstructions;
	
	~_OBFUSCATION_BLOCK_SCAN() {
		assemblyInstructions.clear();
	}
} OBFUSCATION_BLOCK_SCAN;

typedef struct _OBFUSCATION_SCAN_RESULT {
	const uint8_t *addressBeginning = 0;
	const uint8_t *addressEnd = 0;
	const uint8_t *addressPostRetn = 0;
	const uint8_t *addressObfuscatedData = 0;
	uint32_t offsetObfuscationFunction = 0;
	
	std::list<BINARY_NOP_RANGE*> nopRangePatches;
	
	~_OBFUSCATION_SCAN_RESULT() {
		for (auto &binaryPatch : nopRangePatches) {
			delete binaryPatch;
		}
		nopRangePatches.clear();
	}
} OBFUSCATION_SCAN_RESULT;

bool ScanForObfuscationInBinary(OBFUSCATED_BINARY_CONFIG *config);
void ScanForObfuscation(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, const uint8_t *assembly_search_start, const uint32_t assembly_search_size, std::list<OBFUSCATION_SCAN_RESULT*> *obfuscation_scan_results);
