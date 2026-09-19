#include <winsock2.h>
#include <Windows.h>
#include "dllmain.hpp"
#include "xlln/xlln.hpp"
#include "xlln/xlln-modules.hpp"
#include "xlln/xlln-keep-alive.hpp"
#include "xlive/live-over-lan.hpp"
#include "xlive/xnetqos.hpp"
#include "xlive/xrender.hpp"
#include <iostream>

HMODULE xlln_hmod_xlive = 0;
HMODULE xlln_hmod_title = 0;

BOOL APIENTRY DllMain(HMODULE hModule, uint32_t ul_reason_for_call, void* lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		xlln_hmod_xlive = hModule;
		
		xlln_hmod_title = GetModuleHandle(NULL);
		if (!xlln_hmod_title) {
			return FALSE;
		}
		
		InitCriticalSections();
		
		if (!InitXLLN(hModule)) {
			UninitCriticalSections();
			return FALSE;
		}
		
		// Emphasize that NOTHING else should be done after this point to cause this DLL not to load successfully.
		if (!InitXllnModules()) {
			UninitCriticalSections();
			return FALSE;
		}
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		if (lpReserved) {
			XllnThreadLiveOverLanAbandon();
			XLiveThreadQosAbandon();
			XllnThreadHotkeysAbandon();
			XllnThreadKeepAliveAbandon();
		}

		if (!UninitXllnModules()) {
			return FALSE;
		}
		
		if (!UninitXLLN()) {
			return FALSE;
		}
		
		UninitCriticalSections();
	}
	return TRUE;
}
