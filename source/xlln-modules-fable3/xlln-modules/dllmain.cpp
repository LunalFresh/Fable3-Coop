// dllmain.cpp : Defines the entry point for the DLL application.
#include "dllmain.hpp"
#include "xlivelessness.hpp"

HMODULE xlln_hmod_xlivelessness = 0;
HMODULE xlln_hmod_xlln_module = 0;
HMODULE xlln_hmod_title = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		xlln_hmod_xlln_module = hModule;

		xlln_hmod_title = GetModuleHandle(NULL);
		if (!xlln_hmod_title) {
			return FALSE;
		}

		xlln_hmod_xlivelessness = GetModuleHandle(L"xlive.dll");
		if (!xlln_hmod_xlivelessness) {
			return FALSE;
		}

		if (!InitXLiveLessNess()) {
			return FALSE;
		}

		extern bool InitXllnModule();
		if (!InitXllnModule()) {
			return FALSE;
		}

	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		extern bool UninitXllnModule();
		UninitXllnModule();

		UninitXLiveLessNess();
	}
	else if (ul_reason_for_call == DLL_THREAD_ATTACH) {

	}
	else if (ul_reason_for_call == DLL_THREAD_DETACH) {

	}
	return TRUE;
}
