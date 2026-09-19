#include "../dllmain.hpp"
#include "./deobfuscator.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "./config.hpp"
#include "../third-party/zydis/Zydis.h"
#include <list>
#include <vector>
#include <map>
#include <set>

std::vector<OBFUSCATED_BINARY_CONFIG*> obfuscated_binary_configurations;
// Key: relocatable_address.
// Value: relocation_type.
//  relocation_type == 2 : Immediate offset.
//  relocation_type == 3 : Displacement offset.
static std::map<uint32_t, uint8_t> warbird_obfuscation_type_1_relocatable_addresses;
static uint32_t warbird_obfuscation_type_1_branching_type;
static uint8_t *warbird_obfuscation_type_1_address_of_fragment_end;
static uint32_t warbird_obfuscation_type_1_obfuscated_data_end;
static uint32_t warbird_obfuscation_type_1_base_address;

void ProcessObfuscationType1Entry(
	OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig
	, uint32_t offset_obfuscated_data
	, uint8_t **deobfuscated_function_buffer
	, uint32_t *deobfuscated_function_buffer_size
	, std::map<uint32_t, uint8_t> *relocatable_addresses
	, uint32_t *offset_obfuscated_data_end
)
{
	if (!offset_obfuscated_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s offset_obfuscated_data was not specified."
			, __func__
		);
		return;
	}
	if (
		!(
			(deobfuscated_function_buffer && deobfuscated_function_buffer_size && relocatable_addresses)
			|| (!deobfuscated_function_buffer && !deobfuscated_function_buffer_size && !relocatable_addresses)
		)
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s deobfuscated_function_buffer or deobfuscated_function_buffer_size, or relocatable_addresses is null."
			, __func__
		);
		return;
	}
	if ((deobfuscated_function_buffer && *deobfuscated_function_buffer) || (deobfuscated_function_buffer_size && *deobfuscated_function_buffer_size)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s func has already been deobfuscated."
			, __func__
		);
		return;
	}
	if (!obfuscatedBinaryConfig->offsetFuncDeobfuscateType1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s offsetFuncDeobfuscateType1 has not been specified."
			, __func__
		);
		return;
	}
	
	std::map<uint32_t, OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO*> obfuscationType1FuncFragments;
	std::list<uint32_t> obfuscationType1FuncFragmentsRemaining;
	std::list<uint32_t> obfuscationType1FuncFragmentOrder;
	
	{
		OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO *fragmentInfo = new OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO;
		fragmentInfo->obfuscatedDataOffset = offset_obfuscated_data;
		
		obfuscationType1FuncFragments[fragmentInfo->obfuscatedDataOffset] = fragmentInfo;
		obfuscationType1FuncFragmentsRemaining.push_back(fragmentInfo->obfuscatedDataOffset);
		obfuscationType1FuncFragmentOrder.push_back(fragmentInfo->obfuscatedDataOffset);
	}
	
	uint32_t dataDeobfuscateType1[] = { 0, 0, 0, 0 };
	uint32_t(__fastcall* FuncDeobfuscateType1)(uint32_t*) = (uint32_t(__fastcall*)(uint32_t*))((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType1);
	
	// Deobfuscate all fragments by leveraging the existing deobfuscation algorithm within the (partially) obfuscated binary.
	while (obfuscationType1FuncFragmentsRemaining.size() > 0) {
		OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO *fragmentInfo;
		{
			uint32_t obfuscatedDataOffset = *obfuscationType1FuncFragmentsRemaining.begin();
			obfuscationType1FuncFragmentsRemaining.pop_front();
			fragmentInfo = obfuscationType1FuncFragments[obfuscatedDataOffset];
		}
		
		{
			if (fragmentInfo->fragmentAddress || fragmentInfo->fragmentAddressSize) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s fragmentAddress or fragmentAddressSize has already been set."
					, __func__
				);
				__debugbreak();
			}
			fragmentInfo->fragmentAddress = 0;
			fragmentInfo->fragmentAddressSize = 0;
		}
		
		// Setup for the call to the deobfuscation algorithm within the target binary.
		{
			warbird_obfuscation_type_1_relocatable_addresses.clear();
			warbird_obfuscation_type_1_branching_type = 0;
			warbird_obfuscation_type_1_address_of_fragment_end = 0;
			warbird_obfuscation_type_1_obfuscated_data_end = fragmentInfo->obfuscatedDataOffset;
			fragmentInfo->obfuscatedDataRuntimeAddress = (uint8_t*)((uint32_t)(obfuscatedBinaryConfig->binaryBaseAddress) + fragmentInfo->obfuscatedDataOffset);
			dataDeobfuscateType1[1] = (uint32_t)fragmentInfo->obfuscatedDataRuntimeAddress;
		}
		uint8_t *fragmentAddressWarbird = (uint8_t*)FuncDeobfuscateType1(dataDeobfuscateType1);

		if (!fragmentAddressWarbird) {
			// If it cannot deobfuscate the address then from what I was able to figure out it is actually the address to jump to for a JMP instruction.
			// This fragment does not branch.
			fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tIF;
			{
				INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = new INSTRUCTION_CONSTRUCTION_HELPER;
				instructionHelper->instructionBufferSize = 5;
				instructionHelper->instructionBuffer = new uint8_t[instructionHelper->instructionBufferSize];
				memset(instructionHelper->instructionBuffer, 0, instructionHelper->instructionBufferSize);
				instructionHelper->instructionBuffer[0] = 0xE9;
				instructionHelper->instructionValueDisplacementOffset = 1;
				// TODO figure out why -sizeof(uint32_t) is needed.
				instructionHelper->instructionRelocationDestinationValueOffset = fragmentInfo->obfuscatedDataOffset - sizeof(uint32_t);
				fragmentInfo->branchTypeIfInstructions.push_back(instructionHelper);
			}
		}
		else {
			if (!warbird_obfuscation_type_1_address_of_fragment_end) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s warbird_obfuscation_type_1_address_of_fragment_end has not been set."
					, __func__
				);
				__debugbreak();
			}
			if (!warbird_obfuscation_type_1_branching_type) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s warbird_obfuscation_type_1_branching_type has not been set."
					, __func__
				);
				__debugbreak();
			}
			if (!warbird_obfuscation_type_1_obfuscated_data_end) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s warbird_obfuscation_type_1_obfuscated_data_end has not been set."
					, __func__
				);
				__debugbreak();
			}
			
			if (offset_obfuscated_data_end) {
				*offset_obfuscated_data_end = warbird_obfuscation_type_1_obfuscated_data_end;
			}
			
			fragmentInfo->fragmentAddressSize = (uint32_t)warbird_obfuscation_type_1_address_of_fragment_end - (uint32_t)fragmentAddressWarbird;
			
			if (!fragmentInfo->fragmentAddressSize) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s fragmentAddressSize is 0."
					, __func__
				);
				__debugbreak();
			}
			
			// Make a copy of Warbird's deobfuscated fragment.
			fragmentInfo->fragmentAddress = new uint8_t[fragmentInfo->fragmentAddressSize];
			memcpy(fragmentInfo->fragmentAddress, fragmentAddressWarbird, fragmentInfo->fragmentAddressSize);
			
			// Convert all the relocatable addresses and store them in the fragment info.
			for (auto &warbirdRelocatableAddress : warbird_obfuscation_type_1_relocatable_addresses) {
				uint32_t relocatableAddressWarbird = warbirdRelocatableAddress.first;
				uint8_t relocationTypeWarbird = warbirdRelocatableAddress.second;
				uint32_t relocatableAddressOffset = relocatableAddressWarbird - (uint32_t)fragmentAddressWarbird;
				uint8_t relocationType = relocationTypeWarbird;
				if (fragmentInfo->fragmentAddressSize < 4 || relocatableAddressOffset > fragmentInfo->fragmentAddressSize - sizeof(uint32_t)) {
					// Appears to be 3 addresses before fragment that needed relocating by the algorithm for some reason? Ignore anything not in range.
					continue;
				}
				
				fragmentInfo->relocatableAddresses[relocatableAddressOffset] = relocationType;
				
				uint32_t &instructionRelocatableValue = *(uint32_t*)(&fragmentInfo->fragmentAddress[relocatableAddressOffset]);
				const uint32_t &instructionRelocatableValueWarbird = *(const uint32_t*)(&fragmentAddressWarbird[relocatableAddressOffset]);
				if (relocationType == 2) {
					instructionRelocatableValue += (uint32_t)&instructionRelocatableValueWarbird - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
				}
				else if (relocationType == 3) {
					instructionRelocatableValue -= (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
				}
			}
			
			// Skip the obfuscation callback instruction at the end of the deobfuscated fragment.
			uint8_t *warbirdBranchData = (uint8_t*)((uint32_t)warbird_obfuscation_type_1_address_of_fragment_end + 5);
			if (warbird_obfuscation_type_1_branching_type == obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeIf) {
				// branch count 3
				fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tIF;
				
				uint32_t jumpTo = 0;
				
				switch (warbirdBranchData[2 * sizeof(uint32_t)] & 0x7F) {
					case 35:
					case 42:
					case 61: {
						// JMP to 1st branch. Therefore there is no need to deobfuscate the 2nd branch.
						jumpTo = ((uint32_t*)warbirdBranchData)[0];
						break;
					}
					case 2:
					case 33:
					case 46:
					case 53: {
						// JMP to 2nd branch. Therefore there is no need to deobfuscate the 1st branch.
						jumpTo = ((uint32_t*)warbirdBranchData)[1];
						break;
					}
					default: {
						break;
					}
				}
				
				if (jumpTo) {
					// Ensure the branch gets queued for deobfuscation.
					if (obfuscationType1FuncFragments.count(jumpTo) <= 0) {
						OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO *fragmentInfoBranch = new OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO;
						fragmentInfoBranch->obfuscatedDataOffset = jumpTo;
						
						obfuscationType1FuncFragments[fragmentInfoBranch->obfuscatedDataOffset] = fragmentInfoBranch;
						obfuscationType1FuncFragmentsRemaining.push_back(fragmentInfoBranch->obfuscatedDataOffset);
						obfuscationType1FuncFragmentOrder.push_back(fragmentInfoBranch->obfuscatedDataOffset);
					}
					
					// Create fragment branch to jump to another obfuscated fragment.
					INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = new INSTRUCTION_CONSTRUCTION_HELPER;
					instructionHelper->instructionBufferSize = 5;
					instructionHelper->instructionBuffer = new uint8_t[instructionHelper->instructionBufferSize];
					memset(instructionHelper->instructionBuffer, 0, instructionHelper->instructionBufferSize);
					instructionHelper->instructionBuffer[0] = 0xE9;
					instructionHelper->instructionValueDisplacementOffset = 1;
					instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset = jumpTo;
					fragmentInfo->branchTypeIfInstructions.push_back(instructionHelper);
				}
				else {
					// For all the single instruction if type branches.
					uint16_t firstJump = 0;
					uint8_t *firstJumpData = (uint8_t*)&firstJump;
					// If the condition requires another instruction.
					uint16_t secondJump = 0;
					uint8_t *secondJumpData = (uint8_t*)&secondJump;
					switch (warbirdBranchData[2 * sizeof(uint32_t)] & 0x7F) {
						case 0:
						case 52:
						case 90:
						case 108: {
							//bool shouldJump = ~(unsigned __int8)(EFLAGS >> 7) & 1;
							//JNS (SF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x89;
							break;
						}
						case 1:
						case 32:
						case 45:
						case 66: {
							//if ( !(EFLAGS & 0x40) && ((EFLAGS ^ (EFLAGS >> 4)) & 0x80u) == 0 )
							//JLNE or JG (ZF=0) && (SF=OF)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8F;
							break;
						}
						case 3:
						case 15:
						case 74:
						case 103: {
							//if ( (EFLAGS & 0x80u) == 0 ) && (EFLAGS & 0x800)
							// (SF=0) && (OF=1)
							__debugbreak();
							break;
						}
						case 5:
						case 17:
						case 93:
						case 120: {
							//bool shouldJump = ~(unsigned __int8)(EFLAGS >> 2) & 1;
							//JNP or JPO (PF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8B;
							break;
						}
						case 6:
						case 77:
						case 84:
						case 109: {
							__debugbreak();
							break;
						}
						case 7:
						case 14:
						case 60:
						case 83: {
							//bool shouldJump = ~(unsigned __int8)(EFLAGS >> 11) & 1;
							//JNO (OF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x81;
							break;
						}
						case 8:
						case 13:
						case 31:
						case 91: {
							// if (EFLAGS & 0x40) || ((EFLAGS ^ (EFLAGS >> 4)) & 0x80u) != 0
							// JLE or JG (ZF=1) || (SF!=OF)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8E;
							break;
						}
						case 9:
						case 34:
						case 37:
						case 49: {
							//bool shouldJump = (EFLAGS >> 4) & 1;
							//??? (AF=1)
							__debugbreak();
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x00;
							break;
						}
						case 10:
						case 23:
						case 36:
						case 50: {
							//if !( EFLAGS & 0x40 ) && (EFLAGS & 0x800)
							// (ZF=0) && (OF=1)
							__debugbreak();
							break;
						}
						case 12:
						case 75:
						case 86:
						case 101: {
							//*(this - 7) == 0
							__debugbreak();
							break;
						}
						case 18:
						case 24:
						case 29:
						case 81: {
							//bool shouldJump = ((unsigned int)(EFLAGS ^ (EFLAGS >> 4)) >> 7) & 1;
							//JL or JNGE (SF!=OF)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8C;
							break;
						}
						case 20:
						case 59:
						case 79:
						case 95: {
							//(EFLAGS & 0x800) == 0x800
							//JO (OF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x80;
							break;
						}
						case 21:
						case 30:
						case 65:
						case 85: {
							__debugbreak();
							break;
						}
						case 26:
						case 62:
						case 96:
						case 114: {
							//(EFLAGS & 4) == 4
							//JP or JPE (PF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8A;
							break;
						}
						case 27:
						case 68:
						case 72:
						case 126: {
							//bool shouldJump = ~(unsigned __int8)(EFLAGS >> 6) & 1;
							//JNZ or JNE (ZF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x85;
							break;
						}
						case 28:
						case 38:
						case 43:
						case 117: {
							//if ( (EFLAGS & 0x40) && !(EFLAGS & 4) )
							// (ZF=1) && (PF=0)
							__debugbreak();
							break;
						}
						case 39:
						case 67:
						case 100:
						case 107: {
							//bool shouldJump = (EFLAGS & 0x80) == 0x80;
							//JS (SF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x88;
							break;
						}
						case 40:
						case 64:
						case 104:
						case 123: {
							//if ( EFLAGS & 1 ) && (EFLAGS & 0x40)
							//JBE or JNA (CF=1) && (ZF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x86;
							break;
						}
						case 41:
						case 58:
						case 113:
						case 118: {
							//if !( EFLAGS & 0x40 || ((unsigned __int8)EFLAGS ^ (unsigned __int8)(EFLAGS >> 2)) & 1 )
							// (PF=CF) && (ZF=0)
							__debugbreak();
							break;
						}
						case 44:
						case 48:
						case 63:
						case 124: {
							//bool shouldJump = ~(unsigned __int8)(EFLAGS >> 10) & 1;
							//??? (DF=0)
							__debugbreak();
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x00;
							break;
						}
						case 54:
						case 89:
						case 97:
						case 125: {
							//bool shouldJump = ~(unsigned __int8)EFLAGS & 1;
							//JNB or JAE or JNC (CF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x83;
							break;
						}
						case 55:
						case 82:
						case 94:
						case 121: {
							//if ( EFLAGS & 1 || EFLAGS & 0x40 ) //(CF=1)(ZF=1)
							// goto [0]
							// else goto [1]
							// this implementation branches opposite way.
							// JNBE or JA (CF=0 AND ZF=0)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x87;
							break;
						}
						case 57:
						case 78:
						case 111:
						case 122: {
							//bool shouldJump = ~(unsigned __int8)((unsigned int)(EFLAGS ^ (EFLAGS >> 4)) >> 7) & 1;
							//JNL or JGE (SF==OF)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8D;
							break;
						}
						case 70:
						case 106:
						case 112:
						case 119: {
							// if ( EFLAGS & 1 ) || (EFLAGS & 0x800)
							// (CF=1) || (OF=1)
							//FIXME need 2 instructions.
							__debugbreak();
							break;
						}
						case 71:
						case 73:
						case 98:
						case 115: {
							//bool shouldJump = (EFLAGS & 0x40) == 0x40;
							//JZ or JE (ZF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x84;
							break;
						}
						case 76:
						case 102:
						case 105:
						case 127: {
							//if ( EFLAGS & 4 )
							//JP or JPE (PF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x8A;
							break;
						}
						case 80:
						case 87:
						case 92:
						case 110: {
							//bool shouldJump = (EFLAGS & 1) == 1;
							//JB or JNAE or JC (CF=1)
							firstJumpData[0] = 0x0F;
							firstJumpData[1] = 0x82;
							break;
						}
						default: {
							__debugbreak();
							break;
						}
					}
					
					if (firstJump) {
						for (uint8_t i = 0; i < 2; i++) {
							jumpTo = ((uint32_t*)warbirdBranchData)[i];
							// Ensure the branch gets queued for deobfuscation.
							if (obfuscationType1FuncFragments.count(jumpTo) <= 0) {
								OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO *fragmentInfoBranch = new OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO;
								fragmentInfoBranch->obfuscatedDataOffset = jumpTo;
								
								obfuscationType1FuncFragments[fragmentInfoBranch->obfuscatedDataOffset] = fragmentInfoBranch;
								obfuscationType1FuncFragmentsRemaining.push_back(fragmentInfoBranch->obfuscatedDataOffset);
								obfuscationType1FuncFragmentOrder.push_back(fragmentInfoBranch->obfuscatedDataOffset);
							}
						}
						
						{
							// Create fragment branch to conditionally jump to another obfuscated fragment.
							INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = new INSTRUCTION_CONSTRUCTION_HELPER;
							instructionHelper->instructionBufferSize = 6;
							instructionHelper->instructionBuffer = new uint8_t[instructionHelper->instructionBufferSize];
							memset(instructionHelper->instructionBuffer, 0, instructionHelper->instructionBufferSize);
							instructionHelper->instructionBuffer[0] = firstJumpData[0];
							instructionHelper->instructionBuffer[1] = firstJumpData[1];
							instructionHelper->instructionValueDisplacementOffset = 2;
							instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset = ((uint32_t*)warbirdBranchData)[1];
							fragmentInfo->branchTypeIfInstructions.push_back(instructionHelper);
						}
						
						if (secondJump) {
							// Create fragment branch to conditionally jump to another obfuscated fragment.
							INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = new INSTRUCTION_CONSTRUCTION_HELPER;
							instructionHelper->instructionBufferSize = 6;
							instructionHelper->instructionBuffer = new uint8_t[instructionHelper->instructionBufferSize];
							memset(instructionHelper->instructionBuffer, 0, instructionHelper->instructionBufferSize);
							instructionHelper->instructionBuffer[0] = secondJumpData[0];
							instructionHelper->instructionBuffer[1] = secondJumpData[1];
							instructionHelper->instructionValueDisplacementOffset = 2;
							instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset = ((uint32_t*)warbirdBranchData)[1];
							fragmentInfo->branchTypeIfInstructions.push_back(instructionHelper);
						}
						
						{
							// Create fragment branch to jump to the 2nd obfuscated fragment.
							INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = new INSTRUCTION_CONSTRUCTION_HELPER;
							instructionHelper->instructionBufferSize = 5;
							instructionHelper->instructionBuffer = new uint8_t[instructionHelper->instructionBufferSize];
							memset(instructionHelper->instructionBuffer, 0, instructionHelper->instructionBufferSize);
							instructionHelper->instructionBuffer[0] = 0xE9;
							instructionHelper->instructionValueDisplacementOffset = 1;
							instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset = ((uint32_t*)warbirdBranchData)[0];
							fragmentInfo->branchTypeIfInstructions.push_back(instructionHelper);
						}
					}
					else {
						fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tUNKNOWN;
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
							, "%s Unknown IF branch type."
							, __func__
						);
						__debugbreak();
					}
				}
			}
			else if (warbird_obfuscation_type_1_branching_type == obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeReturn) {
				// branch count 1
				fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tRETURN;
				fragmentInfo->branchTypeReturnPopFromStack = (warbirdBranchData[0] & 0x0F) * 4;
			}
			else if (warbird_obfuscation_type_1_branching_type == obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeSwitch) {
				// branch count 2
				uint8_t switchBranchType = warbirdBranchData[0] & 0x7F;
				switch (switchBranchType) {
					case 11:
					case 16:
					case 22:
					case 116: {
						uint32_t branchInfo = ((uint32_t*)warbirdBranchData)[0];
						
						if (branchInfo & 0x80) {
							// Instead of using the weird range keep it sane.
							//uint8_t iRegister = ((branchInfo & 0x700) >> 8) - 9; // Range -9 to -2.
							uint8_t iRegister = (branchInfo & 0x700) >> 8; // Range 0 to 7.
							
							__debugbreak();
						}
						
						uint8_t thirdInstructionByte = 0x00;
						thirdInstructionByte |= 0b101;
						{
							// Instead of using the weird range keep it sane.
							//uint8_t iRegister = ((branchInfo & 0x38000) >> 15) - 9; // Range -9 to -2.
							uint8_t iRegister = (branchInfo & 0x38000) >> 15; // Range 0 to 7.
							
							if (!(branchInfo & 0x4000)) {
								iRegister = 8;// JMP ADDRESS VALUE.
							}
							
							switch (iRegister) {
								case 5:// EAX
								case 0:// ECX
									thirdInstructionByte |= 0b000000;
									break;
								case 7:// EDX
								case 2:// EBX
									thirdInstructionByte |= 0b010000;
									break;
								case 8:// JMP ADDRESS VALUE.
								case 3:// EBP
									thirdInstructionByte |= 0b100000;
									break;
								case 6:// ESI
								case 1:// EDI
									thirdInstructionByte |= 0b110000;
									break;
								case 4:// STACK POINTER
								default: {
									__debugbreak();
									break;
								}
							}
							
							switch (iRegister) {
								case 5:// EAX
								case 7:// EDX
								case 8:// JMP ADDRESS VALUE.
								case 6:// ESI
									thirdInstructionByte |= 0b0000;
									break;
								case 0:// ECX
								case 2:// EBX
								case 3:// EBP
								case 1:// EDI
									thirdInstructionByte |= 0b1000;
									break;
								case 4:// STACK POINTER
								default: {
									__debugbreak();
									break;
								}
							}
							
							// Value is: 0 to 3.
							uint8_t iMultiplyRegisterBy = (branchInfo & 0x3000) >> 12;
							//uint32_t multiplyRegisterBy[] = {2, 8, 1, 4};
							//multiplyRegisterBy[iMultiplyRegister];
							
							if (!(branchInfo & 0x4000)) {
								iMultiplyRegisterBy = 2;// JMP ADDRESS VALUE.
							}
							
							switch (iMultiplyRegisterBy) {
								case 2:// *1
									thirdInstructionByte |= 0b00000000;
									break;
								case 0:// *2
									thirdInstructionByte |= 0b01000000;
									break;
								case 3:// *4
									thirdInstructionByte |= 0b10000000;
									break;
								case 1:// *8
									thirdInstructionByte |= 0b11000000;
									break;
								default: {
									__debugbreak();
									break;
								}
							}
						}
						
						uint32_t decodedOffset = ((uint32_t*)warbirdBranchData)[1];
						
						if (branchInfo & 0x80000) {
							//decodedOffset += (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
						}
						else {
							__debugbreak();
						}
						
						fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tSWITCH;
						fragmentInfo->branchTypeSwitchThirdInstructionByte = thirdInstructionByte;
						fragmentInfo->branchTypeSwitchJumpTableOffset = decodedOffset;
						break;
					}
					case 19:
					case 51:
					case 56:
					case 69: {
						fragmentInfo->branchType = OBFUSCATION_TYPE_1_BRANCH_TYPE::tCALL_EAX;
						break;
					}
					default: {
						__debugbreak();
						break;
					}
				}
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s Unknown branching type of 0x%08x."
					, __func__
					, warbird_obfuscation_type_1_branching_type
				);
				__debugbreak();
			}
		}
	}
	
	if (obfuscationType1FuncFragments.size() != obfuscationType1FuncFragmentOrder.size()) {
		__debugbreak();
	}
	
	// TODO re-order obfuscationType1FuncFragmentOrder to improve layout of result contiguous block.
	obfuscationType1FuncFragmentOrder;
	
	// Connect all the deobfuscated fragments together into one contiguous block.
	// Create a buffer exactly big enough to fit them all in with the joining instructions included.
	if (deobfuscated_function_buffer)
	{
		uint32_t posDeobfuscatedFunctionBuffer = 0;
		uint8_t *deobfuscatedFunctionBuffer = 0;
		
		uint32_t *obfuscationType1FuncFragmentOrderArray = new uint32_t[obfuscationType1FuncFragmentOrder.size()];
		auto itrObfuscationType1FuncFragmentOrder = obfuscationType1FuncFragmentOrder.begin();
		for (uint32_t iOrder = 0; iOrder < obfuscationType1FuncFragmentOrder.size(); iOrder++, itrObfuscationType1FuncFragmentOrder++) {
			obfuscationType1FuncFragmentOrderArray[iOrder] = *itrObfuscationType1FuncFragmentOrder;
		}
		
		std::map<uint32_t, uint32_t> fragmentPosInFunc;
		
		while (obfuscationType1FuncFragmentOrder.size() > 0 && !deobfuscatedFunctionBuffer) {
			if (posDeobfuscatedFunctionBuffer) {
				deobfuscatedFunctionBuffer = new uint8_t[posDeobfuscatedFunctionBuffer];
				*deobfuscated_function_buffer = deobfuscatedFunctionBuffer;
				*deobfuscated_function_buffer_size = posDeobfuscatedFunctionBuffer;
				
				posDeobfuscatedFunctionBuffer = 0;
			}
			
			for (uint32_t iOrder = 0; iOrder < obfuscationType1FuncFragmentOrder.size(); iOrder++) {
				OBFUSCATION_TYPE_1_FUNC_FRAGMENT_INFO *fragmentInfo = obfuscationType1FuncFragments[obfuscationType1FuncFragmentOrderArray[iOrder]];
				
				// Copy the deobfuscated fragment over.
				if (deobfuscatedFunctionBuffer && fragmentInfo->fragmentAddressSize) {
					memcpy(&deobfuscatedFunctionBuffer[posDeobfuscatedFunctionBuffer], fragmentInfo->fragmentAddress, fragmentInfo->fragmentAddressSize);
				}
				
				if (deobfuscatedFunctionBuffer) {
					if (posDeobfuscatedFunctionBuffer != fragmentPosInFunc[fragmentInfo->obfuscatedDataOffset]) {
						// Should match the dry/calc run.
						__debugbreak();
					}
				}
				else {
					fragmentPosInFunc[fragmentInfo->obfuscatedDataOffset] = posDeobfuscatedFunctionBuffer;
				}
				
				// Merge the fragment relocation addresses into the function's map of relocatable addresses.
				// Only do it once.
				if (deobfuscatedFunctionBuffer) {
					for (auto &relocateAddressInfo : fragmentInfo->relocatableAddresses) {
						uint32_t offsetToRelocatableInstructionFragment = relocateAddressInfo.first;
						uint8_t relocationType = relocateAddressInfo.second;
						uint32_t offsetToRelocatableInstructionFunction = (uint32_t)offsetToRelocatableInstructionFragment + (uint32_t)posDeobfuscatedFunctionBuffer;
						
						// Make sure the offset is within range.
						if (*deobfuscated_function_buffer_size < 4 || offsetToRelocatableInstructionFunction > *deobfuscated_function_buffer_size - sizeof(uint32_t)) {
							__debugbreak();
						}
						
						(*relocatable_addresses)[offsetToRelocatableInstructionFunction] = relocationType;
					}
				}
				
				posDeobfuscatedFunctionBuffer += fragmentInfo->fragmentAddressSize;
				
				// Handle the instructions to join all the fragments together.
				switch (fragmentInfo->branchType) {
					case OBFUSCATION_TYPE_1_BRANCH_TYPE::tIF: {
						
						for (uint32_t iInstruction = 0; iInstruction < fragmentInfo->branchTypeIfInstructions.size(); iInstruction++) {
							INSTRUCTION_CONSTRUCTION_HELPER *instructionHelper = fragmentInfo->branchTypeIfInstructions[iInstruction];
							
							// If it's the last instruction and it is only a jump to the very next fragment then ignore adding the instruction.
							if (
								// If it is the last instruction.
								iInstruction == fragmentInfo->branchTypeIfInstructions.size() - 1
								// If the instruction is a jump to a fragment.
								&& instructionHelper->instructionBufferSize == 5
								&& instructionHelper->instructionBuffer[0] == 0xE9
								&& instructionHelper->instructionValueDisplacementOffset == 1
								// If the desired fragment to jump to is actually the very next one.
								&& iOrder + 1 < obfuscationType1FuncFragmentOrder.size()
								&& instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset == obfuscationType1FuncFragmentOrderArray[iOrder + 1]
							) {
								break;
							}
							
							if (deobfuscatedFunctionBuffer) {
								uint8_t *instruction = &deobfuscatedFunctionBuffer[posDeobfuscatedFunctionBuffer];
								memcpy(instruction, instructionHelper->instructionBuffer, instructionHelper->instructionBufferSize);
								
								if (instructionHelper->instructionValueDisplacementOffset != -1) {
									
									if (instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset) {
										uint32_t &instructionRelocatableValue = *(uint32_t*)(&instruction[instructionHelper->instructionValueDisplacementOffset]);
										instructionRelocatableValue = fragmentPosInFunc[instructionHelper->instructionJumpsToObfuscatedFragmentDataOffset] - (posDeobfuscatedFunctionBuffer + instructionHelper->instructionBufferSize);
									}
									
									// TODO haven't needed it so haven't figured out the displacement offset.
									if (instructionHelper->instructionValueRelocatableOffsetOffset != -1) {
										uint32_t &instructionRelocatableValue = *(uint32_t*)(&instruction[instructionHelper->instructionValueDisplacementOffset]);
										__debugbreak();
										instructionRelocatableValue = instructionRelocatableValue + 0;
									}
									
									if (instructionHelper->instructionRelocationDestinationValueOffset != -1) {
										uint32_t &instructionRelocatableValue = *(uint32_t*)(&instruction[instructionHelper->instructionValueDisplacementOffset]);
										instructionRelocatableValue = instructionHelper->instructionRelocationDestinationValueOffset;
										
										(*relocatable_addresses)[posDeobfuscatedFunctionBuffer + instructionHelper->instructionValueDisplacementOffset] = 2;
									}
								}
							}
							
							posDeobfuscatedFunctionBuffer += instructionHelper->instructionBufferSize;
						}
						
						break;
					}
					case OBFUSCATION_TYPE_1_BRANCH_TYPE::tRETURN: {
						if (deobfuscatedFunctionBuffer) {
							uint8_t *instruction = &deobfuscatedFunctionBuffer[posDeobfuscatedFunctionBuffer];
							
							instruction[0] = 0xC2;
							*(uint16_t*)(&instruction[1]) = fragmentInfo->branchTypeReturnPopFromStack;
						}
						
						posDeobfuscatedFunctionBuffer += 3;
						
						break;
					}
					case OBFUSCATION_TYPE_1_BRANCH_TYPE::tSWITCH: {
						if (deobfuscatedFunctionBuffer) {
							uint8_t *instruction = &deobfuscatedFunctionBuffer[posDeobfuscatedFunctionBuffer];
							
							if (!fragmentInfo->branchTypeSwitchThirdInstructionByte) {
								__debugbreak();
							}
							
							instruction[0] = 0xFF;
							instruction[1] = 0x24;
							instruction[2] = fragmentInfo->branchTypeSwitchThirdInstructionByte;
							*(uint32_t*)(&instruction[3]) = fragmentInfo->branchTypeSwitchJumpTableOffset;
							
							(*relocatable_addresses)[posDeobfuscatedFunctionBuffer + 3] = 3;
						}
						
						posDeobfuscatedFunctionBuffer += 7;
						
						break;
					}
					case OBFUSCATION_TYPE_1_BRANCH_TYPE::tCALL_EAX: {
						if (deobfuscatedFunctionBuffer) {
							uint8_t *instruction = &deobfuscatedFunctionBuffer[posDeobfuscatedFunctionBuffer];
							
							instruction[0] = 0xFF;
							instruction[1] = 0xD0;
						}
						
						posDeobfuscatedFunctionBuffer += 2;
						
						break;
					}
					default: {
						__debugbreak();
						break;
					}
				}
				
			}
			
			if (deobfuscatedFunctionBuffer && posDeobfuscatedFunctionBuffer != *deobfuscated_function_buffer_size) {
				__debugbreak();
			}
		}
		
		delete[] obfuscationType1FuncFragmentOrderArray;
	}
	
	// Clean up.
	{
		obfuscationType1FuncFragmentOrder.clear();
		
		for (auto &fragmentInfo : obfuscationType1FuncFragments) {
			delete fragmentInfo.second;
		}
		obfuscationType1FuncFragments.clear();
	}
}

static void __cdecl HelperDeobfuscateType1RecordRelocatableAddress(uint32_t relocatable_address, uint32_t relocation_type)
{
	relocation_type &= 0xFF;
	if (relocation_type != 2 && relocation_type != 3) {
		return;
	}
	warbird_obfuscation_type_1_relocatable_addresses[relocatable_address] = relocation_type;
}

static __declspec(naked) void CodecaveDeobfuscateType1RecordRelocatableAddress(void)
{
	__asm
	{
		// Execute the overwriten instructions first.
		and ecx, 3
		xor edx, edx
		
		pushfd
		pushad
		
		// Get EAX (the relocation type).
		mov edx, [esp + 0x1c]
		push edx
		
		// Get EBX then add ESI (the relocatable address offset).
		mov edx, [esp + 0x04 + 0x10]
		add edx, [esp + 0x04 + 0x04]
		add edx, warbird_obfuscation_type_1_base_address
		push edx
		
		call HelperDeobfuscateType1RecordRelocatableAddress
		
		add esp, 0x08
		
		popad
		popfd
		
		retn
	}
}

static void __cdecl HelperDeobfuscateType1RecordBranchingType(uint32_t branching_type, uint32_t address_of_fragment_end)
{
	warbird_obfuscation_type_1_branching_type = branching_type;
	warbird_obfuscation_type_1_address_of_fragment_end = (uint8_t*)address_of_fragment_end;
}

static __declspec(naked) void CodecaveDeobfuscateType1RecordBranchingType(void)
{
	__asm
	{
		pushfd
		pushad
		
		// Get EAX (end of fragment address).
		mov edx, [esp + 0x1c]
		push edx
		
		// Get EDX (the branching type function).
		mov edx, [esp + 0x04 + 0x14]
		sub edx, warbird_obfuscation_type_1_base_address
		push edx
		
		call HelperDeobfuscateType1RecordBranchingType
		
		add esp, 0x08
		
		popad
		popfd
		
		// Execute the overwriten instructions before returning.
		sub edx, eax
		sub edx, 5
		
		retn
	}
}

static void __cdecl HelperDeobfuscateType1RecordObfuscatedDataEnd(uint32_t offset_obfuscated_data_end)
{
	if (offset_obfuscated_data_end > warbird_obfuscation_type_1_obfuscated_data_end) {
		warbird_obfuscation_type_1_obfuscated_data_end = offset_obfuscated_data_end;
	}
}

static __declspec(naked) void CodecaveDeobfuscateType1RecordObfuscatedDataEnd(void)
{
	__asm
	{
		pushfd
		pushad
		
		// Get EDX (the offset_obfuscated_data_end).
		mov edx, [esp + 0x14]
		push edx
		
		call HelperDeobfuscateType1RecordObfuscatedDataEnd
		
		add esp, 0x04
		
		popad
		popfd
		
		// Execute the overwriten instructions before returning.
		xor edi, edi
		add esi, 0xFFFFFFFF
		
		retn
	}
}

static uint32_t SearchDeobfuscationStackSetupFuncion(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, const char *target_name, uint32_t target_stack_setup_func)
{
	uint8_t iOffsetCalls = 0;
	uint32_t offsetCalls[] = { 0, 0, 0 };
	
	const uint8_t *assemblySearch = (const uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + target_stack_setup_func);
	uint8_t maxInstructionCount = 50;
	
	// Iterate through the instructions until a return instruction is found. Record each CALL instruction. There should be 3 and the 2nd one is the deobfuscate function.
	while (1) {
		ZydisDecoder decoder;
		ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_STACK_WIDTH_32);
		
		ZydisDecodedInstruction instruction;
		ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
		ZyanStatus zStat = ZydisDecoderDecodeFull(&decoder, assemblySearch, -1, &instruction, operands);
		if (!ZYAN_SUCCESS(zStat)) {
			return 0;
		}
		
		if (instruction.length == 0) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd unable parse assembly instructions when searching for %s in binary file. '%ls' '%s'."
				, __func__
				, target_name
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return 0;
		}
		
		if (instruction.mnemonic == ZYDIS_MNEMONIC_CALL) {
			if (iOffsetCalls >= (sizeof(offsetCalls) / sizeof(offsetCalls[0]))) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd found too many CALL instructions when searching for %s in binary file. '%ls' '%s'."
					, __func__
					, target_name
					, obfuscatedBinaryConfig->binaryName
					, obfuscatedBinaryConfig->binarySha256
				);
				return 0;
			}
			
			// TODO redo with operands instead of segments.
			ZydisInstructionSegments segments;
			ZyanStatus zStat2 = ZydisGetInstructionSegments(&instruction, &segments);
			if (!ZYAN_SUCCESS(zStat2)) {
				return 0;
			}
			
			uint32_t functionAddressOffset = 0;
			
			for (unsigned int i = 0; i < segments.count; i++) {
				// Only going to try and relocate 32-bit values.
				if (segments.segments[i].size == 4) {
					if (functionAddressOffset) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
							, "%s Warturd found too many 32-bit segments in CALL instruction when searching for %s in binary file. '%ls' '%s'."
							, __func__
							, target_name
							, obfuscatedBinaryConfig->binaryName
							, obfuscatedBinaryConfig->binarySha256
						);
						return 0;
					}
					uint32_t &assemblyValue = *(uint32_t*)((uint32_t)assemblySearch + segments.segments[i].offset);
					
					if (segments.segments[i].type == ZYDIS_INSTR_SEGMENT_IMMEDIATE) {
						functionAddressOffset = (uint32_t)assemblySearch + instruction.length + assemblyValue - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
					}
					else if (segments.segments[i].type == ZYDIS_INSTR_SEGMENT_DISPLACEMENT) {
						functionAddressOffset = assemblyValue - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
					}
					else {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
							, "%s Warturd found a different segment type than expected in CALL instruction when searching for %s in binary file. '%ls' '%s'."
							, __func__
							, target_name
							, obfuscatedBinaryConfig->binaryName
							, obfuscatedBinaryConfig->binarySha256
						);
						return 0;
					}
				}
			}
			
			if (!functionAddressOffset) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd found a different CALL instruction than expected (such as CALL to a register value) when searching for %s in binary file. '%ls' '%s'."
					, __func__
					, target_name
					, obfuscatedBinaryConfig->binaryName
					, obfuscatedBinaryConfig->binarySha256
				);
				return 0;
			}
			
			offsetCalls[iOffsetCalls++] = functionAddressOffset;
		}
		else if (instruction.mnemonic == ZYDIS_MNEMONIC_RET) {
			break;
		}
		
		assemblySearch += instruction.length;
		
		maxInstructionCount--;
		if (maxInstructionCount == 0) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd searched too far looking for %s in binary file. '%ls' '%s'."
				, __func__
				, target_name
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return 0;
		}
	}
	
	if (iOffsetCalls != 3) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "%s Warturd did not find 3 CALLs when looking for %s in binary file. '%ls' '%s'."
			, __func__, target_name
			, obfuscatedBinaryConfig->binaryName
			, obfuscatedBinaryConfig->binarySha256
		);
		return 0;
	}
	
	return offsetCalls[1];
}

bool ScanForBinaryAndWarbirdOffsets(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	if (!obfuscatedBinaryConfig->manuallyLocatedDeobfuscateStackSetupFunctions) {
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
		
		IMAGE_SECTION_HEADER *imageSectionHeaderData = 0;
		
		for (uint16_t iSection = 0; iSection < imageSectionHeaderTableSectionCount; iSection++) {
			if (strncmp((char*)imageSectionHeaderTable[iSection].Name, ".data", 8) == 0) {
				imageSectionHeaderData = &imageSectionHeaderTable[iSection];
			}
		}
		
		if (!imageSectionHeaderData) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd unable to find '.Data' section header in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		uint8_t *sectionDataAddressRuntime = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + imageSectionHeaderData->PointerToRawData);
		
		// The Warbird version / date string. This specific string is common between all (known) binaries containing this type of obfuscation.
		const char warbirdVersion[] = "Jul 18 2006 19:22:48";
		
		// Scan the .data section for it.
		const uint8_t *warbirdSentinel = FindBytes(sectionDataAddressRuntime, imageSectionHeaderData->SizeOfRawData, (const uint8_t*)warbirdVersion, sizeof(warbirdVersion), true);
		if (!warbirdSentinel) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd unable to find Warbird version / date string in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// The 2nd 32-bit integer before the warbirdVersion string appears to always be the stack setup/manipulation function before the actual deobfuscation takes place in runtime by the original algorithm.
		obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1 = *(uint32_t*)(warbirdSentinel - sizeof(uint32_t) - sizeof(uint32_t)) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2 = *(uint32_t*)(warbirdSentinel + 0x218) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b = *(uint32_t*)(warbirdSentinel + 0x218 + 4) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	}
	
	obfuscatedBinaryConfig->offsetFuncDeobfuscateType1 = SearchDeobfuscationStackSetupFuncion(obfuscatedBinaryConfig, "offsetFuncDeobfuscateStackSetupType1", obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1);
	if (!obfuscatedBinaryConfig->offsetFuncDeobfuscateType1) {
		return false;
	}
	obfuscatedBinaryConfig->offsetFuncDeobfuscateType2 = SearchDeobfuscationStackSetupFuncion(obfuscatedBinaryConfig, "offsetFuncDeobfuscateStackSetupType2", obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2);
	if (!obfuscatedBinaryConfig->offsetFuncDeobfuscateType2) {
		return false;
	}
	if (obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b) {
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType2b = SearchDeobfuscationStackSetupFuncion(obfuscatedBinaryConfig, "offsetFuncDeobfuscateStackSetupType2b", obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b);
		if (!obfuscatedBinaryConfig->offsetFuncDeobfuscateType2b) {
			return false;
		}
	}
	
	{
		// Inside the deobf function offsetFuncDeobfuscateType1 there are the following instructions near the bottom:
		//mov ecx, eax
		//add eax, eax
		//add eax, eax
		//sar eax, 1Eh
		//sar ecx, 1Eh
		//and eax, 3
		//and ecx, 3
		//xor edx, edx
		//sub eax, 1
		const uint8_t signatureOpCodesDeobfuscateType1RecordRelocatableAddress[] = { 0x8b, 0xc8, 0x03, 0xc0, 0x03, 0xc0, 0xc1, 0xf8, 0x1e, 0xc1, 0xf9, 0x1e, 0x83, 0xe0, 0x03, 0x83, 0xe1, 0x03, 0x33, 0xd2, 0x83, 0xe8, 0x01 };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x4000;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType1);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordRelocatableAddress, sizeof(signatureOpCodesDeobfuscateType1RecordRelocatableAddress), true);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetCodecaveDeobfuscateType1RecordRelocatableAddress in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the start of these instructions for use in a codecave:
		//and ecx, 3
		//xor edx, edx
		obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordRelocatableAddress = (uint32_t)assemblySearchResult - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + 15;
	}
	
	{
		// Inside the deobf function offsetFuncDeobfuscateType1 there are the following instructions at the bottom:
		//sub edx, eax
		//sub edx, 5
		//mov byte ptr [eax], 0E8h
		//mov [eax+1], edx
		const uint8_t signatureOpCodesDeobfuscateType1RecordBranchingType[] = { 0x2b, 0xd0, 0x83, 0xea, 0x05, 0xc6, 0x00, 0xe8, 0x89, 0x50, 0x01 };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x4000;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType1);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordBranchingType, sizeof(signatureOpCodesDeobfuscateType1RecordBranchingType), true);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetCodecaveDeobfuscateType1RecordBranchingType in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the start of the signature for use in a codecave.
		obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType = (uint32_t)assemblySearchResult - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	}
	
	{
		// Inside the deobf function offsetFuncDeobfuscateType1 there are the following instructions near the bottom:
		//lea edx, [eax+ecx]
		//xor edi, edi
		//add esi, 0xFFFFFFFF
		//mov [ebp-40h], edx
		const uint8_t signatureOpCodesDeobfuscateType1RecordObfuscatedDataEnd[] = { 0x8d, 0x14, 0x08, 0x33, 0xff, 0x83, 0xc6, 0xff, 0x89, 0x55, 0xc0 };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x4000;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetFuncDeobfuscateType1);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordObfuscatedDataEnd, sizeof(signatureOpCodesDeobfuscateType1RecordObfuscatedDataEnd), true);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetCodecaveDeobfuscateType1RecordObfuscatedDataEnd in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the start of these instructions for use in a codecave:
		//xor edi, edi
		//add esi, 0xFFFFFFFF
		obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordObfuscatedDataEnd = (uint32_t)assemblySearchResult - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + 3;
	}
	
	// There are 3 branching deobfuscation helper functions.
	{
		//and ecx, 0FFFFFF80h
		//or ecx, esi
		//mov [eax+9], edx
		//mov [eax+0Dh], ecx
		const uint8_t signatureOpCodesDeobfuscateType1RecordBranchTypeIf[] = { 0x83, 0xE1, 0x80, 0x0B, 0xCE, 0x89, 0x50, 0x09, 0x89, 0x48, 0x0D };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x150;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType - maxBytesOfAssemblyToSearchIn);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordBranchTypeIf, sizeof(signatureOpCodesDeobfuscateType1RecordBranchTypeIf), false);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetFuncDeobfuscateType1BranchTypeIf in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the end of the signature.
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeIf = *(uint32_t*)((uint32_t)assemblySearchResult + sizeof(signatureOpCodesDeobfuscateType1RecordBranchTypeIf) + 1) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	}
	{
		//and ecx, 3F00h
		//or edi, ecx
		//mov ecx, [ebp+var_7C]
		//mov [eax+5], edi
		//mov edi, [ebp+var_68]
		//mov [eax+9], ecx
		const uint8_t signatureOpCodesDeobfuscateType1RecordBranchTypeSwitch[] = { 0x81, 0xE1, 0x00, 0x3F, 0x00, 0x00, 0x0B, 0xF9, 0x8B, 0x4D, 0x84, 0x89, 0x78, 0x05, 0x8B, 0x7D, 0x98, 0x89, 0x48, 0x09 };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x150;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType - maxBytesOfAssemblyToSearchIn);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordBranchTypeSwitch, sizeof(signatureOpCodesDeobfuscateType1RecordBranchTypeSwitch), false);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetFuncDeobfuscateType1BranchTypeSwitch in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the end of the signature.
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeSwitch = *(uint32_t*)((uint32_t)assemblySearchResult + sizeof(signatureOpCodesDeobfuscateType1RecordBranchTypeSwitch) + 1) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	}
	{
		
		//and cl, 0Fh
		//xor [eax+5], cl
		const uint8_t signatureOpCodesDeobfuscateType1RecordBranchTypeReturn[] = { 0x80, 0xE1, 0x0F, 0x30, 0x48, 0x05 };
		const uint32_t maxBytesOfAssemblyToSearchIn = 0x150;
		
		const uint8_t *assemblySearch = (uint8_t*)((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType - maxBytesOfAssemblyToSearchIn);
		const uint8_t *assemblySearchResult = FindBytes(assemblySearch, maxBytesOfAssemblyToSearchIn, signatureOpCodesDeobfuscateType1RecordBranchTypeReturn, sizeof(signatureOpCodesDeobfuscateType1RecordBranchTypeReturn), false);
		if (!assemblySearchResult) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "%s Warturd did not find a match for the signature of offsetFuncDeobfuscateType1BranchTypeReturn in binary file. '%ls' '%s'."
				, __func__
				, obfuscatedBinaryConfig->binaryName
				, obfuscatedBinaryConfig->binarySha256
			);
			return false;
		}
		
		// Get the offset at the start of the signature.
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeReturn = *(uint32_t*)((uint32_t)assemblySearchResult - 4) - (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	}
	
	return true;
}

void ProcessObfuscationType1Pre(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	warbird_obfuscation_type_1_base_address = (uint32_t)obfuscatedBinaryConfig->binaryBaseAddress;
	
	// Codecave over these instructions:
	//and ecx, 3
	//xor edx, edx
	uint8_t overwrittenOpCodesDeobfuscateType1RecordRelocatableAddress[] = { 0x83, 0xe1, 0x03, 0x33, 0xd2 };
	PatchWithCall((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordRelocatableAddress, &CodecaveDeobfuscateType1RecordRelocatableAddress);
	
	// Codecave over these instructions:
	//sub edx, eax
	//sub edx, 5
	uint8_t overwrittenOpCodesDeobfuscateType1RecordBranchingType[] = { 0x2b, 0xd0, 0x83, 0xea, 0x05 };
	PatchWithCall((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType, &CodecaveDeobfuscateType1RecordBranchingType);
	
	// Codecave over these instructions:
	//xor edi, edi
	//add esi, 0xFFFFFFFF
	uint8_t overwrittenOpCodesDeobfuscateType1RecordObfuscatedDataEnd[] = { 0x33, 0xff, 0x83, 0xc6, 0xff };
	PatchWithCall((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordObfuscatedDataEnd, &CodecaveDeobfuscateType1RecordObfuscatedDataEnd);
}
void ProcessObfuscationType1Post(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	// Undo Hooks and Codecaves.
	
	//and ecx, 3
	//xor edx, edx
	uint8_t overwrittenOpCodesDeobfuscateType1RecordRelocatableAddress[] = { 0x83, 0xe1, 0x03, 0x33, 0xd2 };
	WriteBytes((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordRelocatableAddress, overwrittenOpCodesDeobfuscateType1RecordRelocatableAddress, sizeof(overwrittenOpCodesDeobfuscateType1RecordRelocatableAddress));
	
	//sub edx, eax
	//sub edx, 5
	uint8_t overwrittenOpCodesDeobfuscateType1RecordBranchingType[] = { 0x2b, 0xd0, 0x83, 0xea, 0x05 };
	WriteBytes((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType, overwrittenOpCodesDeobfuscateType1RecordBranchingType, sizeof(overwrittenOpCodesDeobfuscateType1RecordBranchingType));
	
	//xor edi, edi
	//add esi, 0xFFFFFFFF
	uint8_t overwrittenOpCodesDeobfuscateType1RecordObfuscatedDataEnd[] = { 0x33, 0xff, 0x83, 0xc6, 0xff };
	WriteBytes((uint32_t)obfuscatedBinaryConfig->binaryBaseAddress + obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordObfuscatedDataEnd, overwrittenOpCodesDeobfuscateType1RecordObfuscatedDataEnd, sizeof(overwrittenOpCodesDeobfuscateType1RecordObfuscatedDataEnd));
	
	warbird_obfuscation_type_1_relocatable_addresses.clear();
	warbird_obfuscation_type_1_branching_type = 0;
	warbird_obfuscation_type_1_address_of_fragment_end = 0;
	warbird_obfuscation_type_1_obfuscated_data_end = 0;
	
	warbird_obfuscation_type_1_base_address = 0;
}

void DeobfuscateFunctionsInBinary(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, std::vector<OBFUSCATION_TYPE_1_FUNC_INFO*> *obfuscated_functions_type_1)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd is now deobfuscating functions for binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	ProcessObfuscationType1Pre(obfuscatedBinaryConfig);
	
	// Deobfuscate each entry one by one and have the OBFUSCATION_TYPE_1_FUNC_INFO saved.
	for (auto &obfuscationType1FuncInfo : *obfuscated_functions_type_1) {
		ProcessObfuscationType1Entry(
			obfuscatedBinaryConfig
			, obfuscationType1FuncInfo->offsetObfuscatedData
			, &obfuscationType1FuncInfo->deobfuscatedFuncBuffer
			, &obfuscationType1FuncInfo->deobfuscatedFuncBufferSize
			, &obfuscationType1FuncInfo->relocatableAddresses
			, 0
		);
		
		for (auto &binaryPatch : obfuscationType1FuncInfo->nopRangePatches) {
			if (binaryPatch->offsetEnd > obfuscationType1FuncInfo->deobfuscatedFuncBufferSize) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "%s Warturd skipped patching a Type 1 NOP Range in binary file due to location being past the end of the function (0x%08x) [0x%08x, 0x%08x]. '%ls' '%s'."
					, __func__
					, obfuscationType1FuncInfo->deobfuscatedFuncBufferSize
					, binaryPatch->offsetBeginning
					, binaryPatch->offsetEnd
					, obfuscatedBinaryConfig->binaryName
					, obfuscatedBinaryConfig->binarySha256
				);
				continue;
			}
			
			uint8_t *patchAddressRuntime = &obfuscationType1FuncInfo->deobfuscatedFuncBuffer[binaryPatch->offsetBeginning];
			uint32_t patchSize = binaryPatch->offsetEnd - binaryPatch->offsetBeginning;
			
			// NOP instruction.
			memset(patchAddressRuntime, 0x90, patchSize);
			
			// Erase any relocation addresses within the area to overwrite.
			for (uint32_t iPos = binaryPatch->offsetBeginning; iPos < binaryPatch->offsetEnd; iPos++) {
				obfuscationType1FuncInfo->relocatableAddresses.erase(iPos);
			}
		}
	}
	
	ProcessObfuscationType1Post(obfuscatedBinaryConfig);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd has finished deobfuscating functions for binary file. '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
}
