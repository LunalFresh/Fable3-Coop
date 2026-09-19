#include <Winsock2.h>
#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/util-hook.hpp"
// Link with iphlpapi.lib
#include <iphlpapi.h>

static char* UserSelectedNetworkAdapter()
{
	char *chosenAdapterName = 0;

	// Declare and initialize variables
	DWORD dwRetVal = 0;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

	// IPv4
	ULONG family = AF_INET;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	// Allocate a 15 KB buffer to start with.
	ULONG outBufLen = 15000;
	ULONG Iterations = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			//XLLNDebugLog("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
			dwRetVal = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}

		Iterations++;
		// 3 attempts max
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3));

	if (dwRetVal == NO_ERROR) {
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {

			if (pCurrAddresses->OperStatus == 1) {

				//addDebugText("\tAdapter name: %s", pCurrAddresses->AdapterName);

				int resultMessageBox = MessageBoxW(0, pCurrAddresses->Description, L"Use this Network Adapter?", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
				if (resultMessageBox == IDYES) {
					size_t adapterNameBufLen = strlen(pCurrAddresses->AdapterName) + 1;
					chosenAdapterName = (char*)malloc(sizeof(char) * adapterNameBufLen);
					memcpy(chosenAdapterName, pCurrAddresses->AdapterName, adapterNameBufLen);

					break;
				}
			}

			pCurrAddresses = pCurrAddresses->Next;
		}
	}

	return chosenAdapterName;
}

DWORD Import_XLiveInitializeEx = 0;
tXLiveInitializeEx DetourXLiveInitializeEx = NULL;
static HRESULT WINAPI HookXLiveInitializeEx(XLIVE_INITIALIZE_INFO *pPii, DWORD dwTitleXLiveVersion)
{
	HRESULT result = SOCKET_ERROR;

	char *chosenAdapterName = UserSelectedNetworkAdapter();

	char *prevAdapterName = 0;
	if (chosenAdapterName) {
		prevAdapterName = pPii->pszAdapterName;
		pPii->pszAdapterName = chosenAdapterName;
	}
	else {
		MessageBoxA(0, "No Network Adaptor preference specified.", "HookXLiveInitialize", MB_OK | MB_TOPMOST);
	}

	result = DetourXLiveInitializeEx(pPii, dwTitleXLiveVersion);

	if (chosenAdapterName) {
		free(chosenAdapterName);
		chosenAdapterName = 0;
		// Reset the adapter name back to what the Title passed in.
		pPii->pszAdapterName = prevAdapterName;
	}

	return result;
}

DWORD Import_XLiveInitialize = 0;
tXLiveInitialize DetourXLiveInitialize = NULL;
static HRESULT WINAPI HookXLiveInitialize(XLIVE_INITIALIZE_INFO *pPii)
{
	HRESULT result = SOCKET_ERROR;

	char *chosenAdapterName = UserSelectedNetworkAdapter();

	char *prevAdapterName = 0;
	if (chosenAdapterName) {
		prevAdapterName = pPii->pszAdapterName;
		pPii->pszAdapterName = chosenAdapterName;
	}
	else {
		MessageBoxA(0, "No Network Adaptor preference specified.", "HookXLiveInitialize", MB_OK | MB_TOPMOST);
	}

	result = DetourXLiveInitialize(pPii);

	if (chosenAdapterName) {
		free(chosenAdapterName);
		chosenAdapterName = 0;
		// Reset the adapter name back to what the Title passed in.
		pPii->pszAdapterName = prevAdapterName;
	}

	return result;
}

bool InitXllnModule()
{
	PE_HOOK_ARG pe_hack[1];
	DWORD ordinal_addrs[2]{ 0, 0 };
	WORD ordinals[2] = { 5000, 5297 }; //XLiveInitialize, XLiveInitializeEx
	char dll_name[] = "xlive.dll";
	pe_hack->ordinals_len = 2;
	pe_hack->ordinal_names = NULL;
	pe_hack->ordinal_addrs = ordinal_addrs;
	pe_hack->ordinals = ordinals;
	pe_hack->pe_name = dll_name;
	pe_hack->pe_err = ERROR_FUNCTION_FAILED;

	DWORD resultImportLookup;
	if ((resultImportLookup = PEImportHack(xlln_hmod_title, pe_hack, 1)) && resultImportLookup != ERROR_NOT_FOUND) {
		return false;
	}
	if (!ordinal_addrs[0] && !ordinal_addrs[1]) {
		return false;
	}

	HookImport(&Import_XLiveInitialize, &DetourXLiveInitialize, HookXLiveInitialize, ordinal_addrs[0]);
	HookImport(&Import_XLiveInitializeEx, &DetourXLiveInitializeEx, HookXLiveInitializeEx, ordinal_addrs[1]);

	return true;
}

bool UninitXllnModule()
{
	HookImport(&Import_XLiveInitialize, &DetourXLiveInitialize, HookXLiveInitialize, NULL);
	HookImport(&Import_XLiveInitializeEx, &DetourXLiveInitializeEx, HookXLiveInitializeEx, NULL);

	return true;
}
