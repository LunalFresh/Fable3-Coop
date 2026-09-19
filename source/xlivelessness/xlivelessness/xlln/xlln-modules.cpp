#include <winsock2.h>
#include <Windows.h>
#include "xlln-modules.hpp"
#include "../dllmain.hpp"
#include "./xlln-config.hpp"
#include "./debug-log.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-hook.hpp"
#include <string>
#include <vector>
#include <stdint.h>

#ifdef _WIN64

bool InitXllnModules()
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "Only 32-bit / x86 builds enable XLLN-Modules."
	);
	
	return true;
}

void XllnModulesPreXLiveUninitialize()
{
	
}

bool UninitXllnModules()
{
	return true;
}

#else

typedef struct {
	HINSTANCE hInstance;
	wchar_t* moduleName;
	uint32_t lastError;
} XLLN_MODULE_INFO;

static HANDLE xlln_mutex_load_modules = 0;
static std::vector<XLLN_MODULE_INFO*> xlln_modules;

static void(*xlln_post_imports_hook)() = NULL;
static uint8_t xlln_post_imports_hook_source_data[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
static uint8_t* xlln_post_imports_hook_source_address = NULL;

static void LoadXllnModules();

static void ModuleEntryPointCodeCaveReceive()
{
	DWORD OldProtection;
	DWORD temp;

	int numBytes = 5;

	VirtualProtect((void*)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
	memcpy((void*)xlln_post_imports_hook_source_address, xlln_post_imports_hook_source_data, numBytes);
	VirtualProtect((void*)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);

	xlln_post_imports_hook();
}

static __declspec(naked) void ModuleEntryPointCodeCaveReceiveHelper(void)
{
	__asm
	{
		pushfd
		pushad

		// Modify return address to be -5 to make up for the call to this.
		sub dword ptr[esp + 20h + 4h], 5

		// This will undo the code cave and execute the desired function.
		call ModuleEntryPointCodeCaveReceive

		popad
		popfd
		retn
	}
}

static uint32_t InjectModuleEntryPointHook(HMODULE hModule)
{
	if (hModule == NULL || hModule == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "%s Invalid hModule handle."
			, __func__
		);
		return ERROR_INVALID_HANDLE;
	}
	
	{
		// Get PE Import.
		PE_HOOK_ARG pe_hack[1];
		DWORD ordinal_addrs[1];
		WORD ordinals[1] = { 0 };
		const char* ordinalNames[1] = { "_CorExeMain" };
		pe_hack->ordinals_len = 1;
		pe_hack->ordinals = ordinals;
		pe_hack->ordinal_names = ordinalNames;
		pe_hack->ordinal_addrs = ordinal_addrs;
		pe_hack->pe_name = "mscoree.dll";
		pe_hack->pe_err = ERROR_FUNCTION_FAILED;
		
		if (PEImportHack(hModule, pe_hack, 1) == ERROR_SUCCESS && pe_hack->pe_err == ERROR_SUCCESS) {
			// Dereference the function from the import table.
			xlln_post_imports_hook_source_address = (uint8_t*)*(uint32_t*)ordinal_addrs[0];
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
				, "Hooking entry point _CorExeMain of .NET Module: 0x%08x."
				, hModule
			);
			
		}
		else {
			IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)hModule;
			
			if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s Not DOS - This Module (0x%zx) is not a DOS application."
					, __func__
					, hModule
				);
				return ERROR_BAD_EXE_FORMAT;
			}
			
			IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((uint8_t*)hModule + dos_header->e_lfanew);
			
			if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
					, "%s Not Valid PE - This Module (0x%zx) is not a valid NT Portable Executable."
					, __func__
					, hModule
				);
				return ERROR_BAD_EXE_FORMAT;
			}
			
			xlln_post_imports_hook_source_address = (uint8_t*)((uint8_t*)hModule + nt_headers->OptionalHeader.AddressOfEntryPoint);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
				, "Hooking entry point of Module: 0x%zx."
				, hModule
			);
		}
	}
	
	xlln_post_imports_hook = LoadXllnModules;
	
	{
		uint8_t* offset = (uint8_t*)(((uint8_t*)&ModuleEntryPointCodeCaveReceiveHelper) - xlln_post_imports_hook_source_address) - 5;

		// Call instruction.
		uint8_t patch[5] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
		size_t numBytes = 5;
		memcpy(patch + 1, &offset, sizeof(DWORD));

		DWORD OldProtection;
		DWORD temp;

		VirtualProtect((void*)xlln_post_imports_hook_source_address, numBytes, PAGE_EXECUTE_READWRITE, &OldProtection);
		memcpy((void*)xlln_post_imports_hook_source_data, (void*)xlln_post_imports_hook_source_address, numBytes);
		memcpy((void*)xlln_post_imports_hook_source_address, patch, numBytes);
		VirtualProtect((void*)xlln_post_imports_hook_source_address, numBytes, OldProtection, &temp);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "Hooked entry point of Module: 0x%08x."
		, hModule
	);
	return ERROR_SUCCESS;
}

static void LoadXllnModules()
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_TRACE | XLLN_LOG_LEVEL_DEBUG
		, "Hooked PE entry point invoked %s()."
		, __func__
	);
	
	if (xlln_mutex_load_modules && xlln_file_config_path) {
		wchar_t* configPath = PathFromFilename(xlln_file_config_path);
		wchar_t* modulesPath = FormMallocString(L"%smodules/", configPath);
		delete[] configPath;
		
		uint32_t errorMkdir = EnsureDirectoryExists(modulesPath);
		if (errorMkdir) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
				, "%s EnsureDirectoryExists(...) error on path \"%ls\"."
				, __func__
				, modulesPath
			);
		}
		
		// Load all additional modules.
		WIN32_FIND_DATAW data;
		wchar_t* modulesSearch = FormMallocString(L"%s*.dll", modulesPath);
		HANDLE hFind = FindFirstFileW(modulesSearch, &data);
		free(modulesSearch);
		modulesSearch = 0;
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if (!EndsWithCaseInsensitive(data.cFileName, L".dll")) {
					continue;
				}
				wchar_t* xllnModuleFilePath = FormMallocString(L"%s%s", modulesPath, data.cFileName);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
					, "Loading XLLN-Module: \"%ls\"."
					, xllnModuleFilePath
				);
				HINSTANCE hInstanceXllnModule = LoadLibraryW(xllnModuleFilePath);
				if (hInstanceXllnModule) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
						, "XLLN-Module loaded: \"%ls\"."
						, xllnModuleFilePath
					);
					XLLN_MODULE_INFO* xllnModuleInfo = (XLLN_MODULE_INFO*)malloc(sizeof(XLLN_MODULE_INFO));
					memset(xllnModuleInfo, 0, sizeof(XLLN_MODULE_INFO));
					xllnModuleInfo->hInstance = hInstanceXllnModule;
					xllnModuleInfo->moduleName = xllnModuleFilePath;
					xlln_modules.push_back(xllnModuleInfo);
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
						, "XLLN-Module was not loaded: \"%ls\"."
						, xllnModuleFilePath
					);
					free(xllnModuleFilePath);
					xllnModuleFilePath = 0;
				}
			} while (FindNextFileW(hFind, &data));
			FindClose(hFind);
		}
		
		free(modulesPath);
		
		typedef uint32_t(WINAPI* tXllnModulePostInit)();
		for (unsigned int i = 0; i < xlln_modules.size(); i++) {
			if (!xlln_modules[i]->hInstance) {
				continue;
			}
			// XLLNModulePostInit@41101
			tXllnModulePostInit xllnModulePostInit = (tXllnModulePostInit)GetProcAddress(xlln_modules[i]->hInstance, (PCSTR)41101);
			if (xllnModulePostInit) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
					, "Invoking XLLN-Module Post Init for: \"%ls\"."
					, xlln_modules[i]->moduleName
				);
				xlln_modules[i]->lastError = xllnModulePostInit();
				if (xlln_modules[i]->lastError == ERROR_SUCCESS) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
						, "XLLN-Module Post Init invoked for: \"%ls\"."
						, xlln_modules[i]->moduleName
					);
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "XLLN-Module Post Init invoked and returned error 0x%08x \"%ls\"."
						, xlln_modules[i]->lastError
						, xlln_modules[i]->moduleName
					);
					FreeLibrary(xlln_modules[i]->hInstance);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
						, "Unloaded XLLN-Module: \"%ls\"."
						, xlln_modules[i]->moduleName
					);
					xlln_modules[i]->hInstance = 0;
				}
			}
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "Returning from hooked PE entry point %s()."
		, __func__
	);
}

bool InitXllnModules()
{
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "XLLN Config is not set so no XLLN-Modules directory can be loaded."
		);
		return true;
	}
	
	char* mutexName = FormMallocString("Global\\XLiveLessNessModuleLoader0x%x", GetCurrentProcessId());
	xlln_mutex_load_modules = CreateMutexA(0, TRUE, mutexName);
	DWORD lastErr = GetLastError();
	if (lastErr == ERROR_ALREADY_EXISTS || (xlln_mutex_load_modules && xlln_mutex_load_modules == INVALID_HANDLE_VALUE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "XLLN failed to create mutex for loading XLLN Modules. Mutex: \"%s\"."
			, mutexName
		);
		if (xlln_mutex_load_modules && xlln_mutex_load_modules != INVALID_HANDLE_VALUE) {
			CloseHandle(xlln_mutex_load_modules);
		}
		xlln_mutex_load_modules = 0;
	}
	free(mutexName);
	mutexName = 0;
	
	if (!xlln_mutex_load_modules) {
		MessageBoxA(NULL, "XLiveLessNess failed to get the mutex to enable loading XLLN-Modules.", "XLLN-Modules Loader Fail", MB_OK);
		return true;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "Obtained load XLLN-Module mutex: 0x%08x."
		, xlln_mutex_load_modules
	);
	
	// Emphasize that NOTHING else should be done after this point to cause this DLL not to load successfully.
	uint32_t result = InjectModuleEntryPointHook(xlln_hmod_title);
	if (result != ERROR_SUCCESS) {
		wchar_t* messageDescription = FormMallocString(L"XLiveLessNess failed hook Title entry point for loading XLLN-Modules with error 0x%08x.", result);
		MessageBoxW(NULL, messageDescription, L"XLLN Instance ID Fail", MB_OK);
		free(messageDescription);
		return false;
	}
	return true;
}

void XllnModulesPreXLiveUninitialize()
{
	typedef uint32_t(WINAPI* tXllnModulePreUninit)();
	for (const auto xllnModule : xlln_modules) {
		if (!xllnModule->hInstance) {
			continue;
		}
		// XLLNModulePreUninit@41102
		tXllnModulePreUninit xllnModulePreUninit = (tXllnModulePreUninit)GetProcAddress(xllnModule->hInstance, (PCSTR)41102);
		if (xllnModulePreUninit) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
				, "Invoking XLLN-Module Pre Uninit for: \"%ls\"."
				, xllnModule->moduleName
			);
			xllnModule->lastError = xllnModulePreUninit();
			if (xllnModule->lastError == ERROR_SUCCESS) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
					, "XLLN-Module Pre Uninit invoked for: \"%ls\"."
					, xllnModule->moduleName
				);
			}
			else {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "XLLN-Module Pre Uninit invoked and returned error 0x%08x \"%ls\"."
					, xllnModule->lastError
					, xllnModule->moduleName
				);
			}
		}
	}
}

// All XLLN-Modules will be unloaded by the OS automatically and some may have already just been unloaded (cannot reliably call FreeLibrary from DllMain).
bool UninitXllnModules()
{
	for (const auto xllnModule : xlln_modules) {
		free(xllnModule->moduleName);
		free(xllnModule);
	}
	xlln_modules.clear();
	
	if (xlln_mutex_load_modules) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
			, "Closing load XLLN-Module mutex: 0x%zx."
			, xlln_mutex_load_modules
		);
		CloseHandle(xlln_mutex_load_modules);
		xlln_mutex_load_modules = 0;
	}
	
	return true;
}

#endif
