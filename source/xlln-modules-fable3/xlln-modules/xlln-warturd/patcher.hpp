#pragma once
#include "./deobfuscator.hpp"

typedef struct _DEBUG_CODEVIEW_INFO {
	uint32_t signature;
	uint8_t guid[16];
	uint32_t age;
	char pdbFilepath[1];
} DEBUG_CODEVIEW_INFO;

void PatchRuntime(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig);
void PatchStatically(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig);
