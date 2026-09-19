#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include "../utils/util-checksum.hpp"
#include "./config.hpp"
#include "./deobfuscator.hpp"
#include "./scanner.hpp"
#include "./patcher.hpp"
#include <stdint.h>
#include <tchar.h>
#include <vector>

static bool ProcessWarbirdBinary(OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig, uint8_t *binary_base_address, const wchar_t *binary_filepath_runtime)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd is now processing the binary: '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	// Reset it if we are processing it again (the module could have been loaded again).
	if (obfuscatedBinaryConfig->binaryBaseAddress) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
			, "%s Resetting the config as this binary file match has been processed before."
			, __func__
		);
		
		obfuscatedBinaryConfig->binaryBaseAddress = 0;
		if (obfuscatedBinaryConfig->binaryFilepathRuntime) {
			delete[] obfuscatedBinaryConfig->binaryFilepathRuntime;
			obfuscatedBinaryConfig->binaryFilepathRuntime = 0;
		}
		if (!obfuscatedBinaryConfig->manuallyLocatedDeobfuscateStackSetupFunctions) {
			obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType1 = 0;
			obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2 = 0;
			obfuscatedBinaryConfig->offsetFuncDeobfuscateStackSetupType2b = 0;
		}
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1 = 0;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType2 = 0;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType2b = 0;
		obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordRelocatableAddress = 0;
		obfuscatedBinaryConfig->offsetCodecaveDeobfuscateType1RecordBranchingType = 0;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeIf = 0;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeSwitch = 0;
		obfuscatedBinaryConfig->offsetFuncDeobfuscateType1BranchTypeReturn = 0;
		
		for (auto &obfuscatedFunctionType1 : obfuscatedBinaryConfig->obfuscatedFunctionsType1) {
			obfuscatedFunctionType1->~_OBFUSCATION_TYPE_1_FUNC_INFO();
		}
	}
	
	obfuscatedBinaryConfig->binaryBaseAddress = binary_base_address;
	obfuscatedBinaryConfig->binaryFilepathRuntime = CloneString(binary_filepath_runtime);
	
	if (!ScanForBinaryAndWarbirdOffsets(obfuscatedBinaryConfig)) {
		return false;
	}
	
	ScanForObfuscationInBinary(obfuscatedBinaryConfig);
	
	DeobfuscateFunctionsInBinary(obfuscatedBinaryConfig, &obfuscatedBinaryConfig->obfuscatedFunctionsType1);
	
	PatchRuntime(obfuscatedBinaryConfig);
	
	PatchStatically(obfuscatedBinaryConfig);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Warturd has finished processing the binary: '%ls' '%s'."
		, __func__
		, obfuscatedBinaryConfig->binaryName
		, obfuscatedBinaryConfig->binarySha256
	);
	
	return true;
}

static DWORD ManuallyLoadLibrary(const wchar_t *library_file_path, HINSTANCE *phModule, DWORD dwFlags, OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfig)
{
	DWORD result = 0;
	
	// What this does is load the dll without running dllmain (also consequently without resolving references) in order to hack some of the code in it before it is executed.
	// It is done this way so there is no need for file I/O and is probably less detectable.
	// Therefore once loaded we need to manually resolve the dll references (import dlls and each of their ordinals).
	// At this point the library is ready for use and we can choose when to call dllmain with DLL_PROCESS_ATTACH.
	// Note that we must also call DLL_PROCESS_DETACH and probably DLL_THREAD_ATTACH and DLL_THREAD_DETACH too.
	HINSTANCE hInstance = LoadLibraryExW(library_file_path, NULL, DONT_RESOLVE_DLL_REFERENCES);
	HMODULE hmodLibrary = hInstance;
	
	DWORD resultResolveImports = PEResolveImports(hmodLibrary);
	if (resultResolveImports != ERROR_SUCCESS) {
		return resultResolveImports;
	}
	
	{
		IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hmodLibrary;
		
		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "Not DOS - This file is not a DOS application."
			);
			return ERROR_BAD_EXE_FORMAT;
		}
		
		IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((DWORD)hmodLibrary + dos_header->e_lfanew);
		
		if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "Not Valid PE - This file is not a valid NT Portable Executable."
			);
			return ERROR_BAD_EXE_FORMAT;
		}
		
		DWORD entryPointAddress = (DWORD)((DWORD)hmodLibrary + (DWORD)nt_headers->OptionalHeader.AddressOfEntryPoint);
		
		BOOL (APIENTRY *entryPointFunc)(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) = (BOOL (APIENTRY *)(HMODULE, DWORD, LPVOID))entryPointAddress;
		
		ProcessWarbirdBinary(obfuscatedBinaryConfig, (uint8_t*)hmodLibrary, library_file_path);
		
		entryPointFunc(hmodLibrary, DLL_PROCESS_ATTACH, 0);
	}
	
	*phModule = hInstance;
	
	return S_OK;
}

static DWORD Import_XLiveLoadLibraryEx = 0;
static tXLiveLoadLibraryEx DetourXLiveLoadLibraryEx = NULL;
static DWORD WINAPI HookXLiveLoadLibraryEx(LPCWSTR lpwszModuleFileName, HINSTANCE *phModule, DWORD dwFlags)
{
	DWORD result = 0;
	OBFUSCATED_BINARY_CONFIG *obfuscatedBinaryConfigForLibrary = 0;
	
	char* checksumModule = 0;
	uint8_t* checksumModuleRaw = 0;
	if (GetFileSha256(lpwszModuleFileName, &checksumModuleRaw)) {
		checksumModule = Sha256ByteArrayToStr(checksumModuleRaw);
		delete[] checksumModuleRaw;
		checksumModuleRaw = 0;
	}
	
	if (checksumModule) {
		for (auto &obfuscatedBinaryConfig : obfuscated_binary_configurations) {
			if (obfuscatedBinaryConfig->binaryIsImported || !obfuscatedBinaryConfig->binaryIsLoadedProgrammatically) {
				continue;
			}
			
			if (!obfuscatedBinaryConfig->binaryName || !EndsWith(lpwszModuleFileName, obfuscatedBinaryConfig->binaryName)) {
				continue;
			}
			if (!obfuscatedBinaryConfig->binarySha256 || _strcmpi(checksumModule, obfuscatedBinaryConfig->binarySha256) != 0) {
				continue;
			}
			
			obfuscatedBinaryConfigForLibrary = obfuscatedBinaryConfig;
			
			// Break since there is only going to be a single match for the loading module.
			break;
		}
		
		delete[] checksumModule;
		checksumModule = 0;
	}
	
	if (obfuscatedBinaryConfigForLibrary) {
		result = ManuallyLoadLibrary(lpwszModuleFileName, phModule, dwFlags, obfuscatedBinaryConfigForLibrary);
		return result;
	}
	
	result = DetourXLiveLoadLibraryEx(lpwszModuleFileName, phModule, dwFlags);
	
	return result;
}

static uint32_t AddPatches()
{
	uint32_t patchAddress = 0;
	
	
	return ERROR_SUCCESS;
}

static uint32_t RemovePatches()
{
	uint32_t patchAddress = 0;
	
	return ERROR_SUCCESS;
}

// #41101
uint32_t WINAPI XLLNModulePostInit()
{
	uint32_t result = ERROR_SUCCESS;
	
	AddPatches();
	
	return result;
}

// #41102
uint32_t WINAPI XLLNModulePreUninit()
{
	uint32_t result = ERROR_SUCCESS;
	
	RemovePatches();
	
	return result;
}

bool InitXllnModule()
{
	bool enableModule = false;
	
	{
		int nArgs;
		// GetCommandLineW() does not need de-allocating but ToArgv does.
		LPWSTR* lpwszArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
		if (lpwszArglist == NULL) {
			uint32_t errorCmdLineToArgv = GetLastError();
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "%s CommandLineToArgvW(...) failed with error 0x%08x."
				, __func__
				, errorCmdLineToArgv
			);
			return false;
		}
		
		for (int i = 1; i < nArgs; i++) {
			if (wcscmp(lpwszArglist[i], L"-xllnwarturd") == 0) {
				enableModule = true;
			}
		}
		
		LocalFree(lpwszArglist);
		lpwszArglist = 0;
	}
	
	if (!enableModule) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "%s Execution flag '-xllnwarturd' not present. Not loading Warturd."
			, __func__
		);
		return false;
	}
	
	if (!ReadWarturdConfig()) {
		return false;
	}
	
	PE_HOOK_ARG pe_hack[1];
	DWORD ordinal_addrs[1];
	// #5028 XLiveLoadLibraryEx
	WORD ordinals[1] = { 5028 };
	char dll_name[] = "xlive.dll";
	pe_hack->ordinals_len = 1;
	pe_hack->ordinal_names = NULL;
	pe_hack->ordinal_addrs = ordinal_addrs;
	pe_hack->ordinals = ordinals;
	pe_hack->pe_name = dll_name;
	pe_hack->pe_err = ERROR_FUNCTION_FAILED;
	
	if (PEImportHack(xlln_hmod_title, pe_hack, 1)) {
		// TODO log
		return false;
	}
	
	HookImport(&Import_XLiveLoadLibraryEx, &DetourXLiveLoadLibraryEx, HookXLiveLoadLibraryEx, ordinal_addrs[0]);
	
	{
		wchar_t *moduleFileName = GetModuleFilePathW(xlln_hmod_title);
		if (!moduleFileName) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "%s Unable to get Title filepath."
				, __func__
			);
			return false;
		}
		
		char* checksumTitle = 0;
		uint8_t* checksumModuleRaw = 0;
		if (GetFileSha256(moduleFileName, &checksumModuleRaw)) {
			checksumTitle = Sha256ByteArrayToStr(checksumModuleRaw);
			delete[] checksumModuleRaw;
			checksumModuleRaw = 0;
		}
		if (!checksumTitle) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "%s Unable to compute Title SHA256."
				, __func__
			);
			free(moduleFileName);
			moduleFileName = 0;
			return false;
		}
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG | XLLN_LOG_LEVEL_INFO
			, "%s Warturd is enabled."
			, __func__
		);
		
		for (auto &obfuscatedBinaryConfig : obfuscated_binary_configurations) {
			if (obfuscatedBinaryConfig->binaryIsImported || obfuscatedBinaryConfig->binaryIsLoadedProgrammatically) {
				continue;
			}
			
			if (!obfuscatedBinaryConfig->binarySha256 || _strcmpi(checksumTitle, obfuscatedBinaryConfig->binarySha256) != 0) {
				continue;
			}
			
			ProcessWarbirdBinary(obfuscatedBinaryConfig, (uint8_t*)xlln_hmod_title, moduleFileName);
			
			// Break since there is only going to be a single match for the running title.
			break;
		}
		
		delete[] checksumTitle;
		checksumTitle = 0;
		free(moduleFileName);
		moduleFileName = 0;
	}
	
	return true;
}

bool UninitXllnModule()
{
	for (auto &obfuscatedBinaryConfig : obfuscated_binary_configurations) {
		delete obfuscatedBinaryConfig;
	}
	obfuscated_binary_configurations.clear();

	// Undo hooks.
	HookImport(&Import_XLiveLoadLibraryEx, &DetourXLiveLoadLibraryEx, HookXLiveLoadLibraryEx, NULL);
	
	return true;
}
