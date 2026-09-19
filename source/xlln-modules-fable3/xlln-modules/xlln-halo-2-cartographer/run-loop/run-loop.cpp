#include "../../dllmain.hpp"
#include "../xlln-module.hpp"
#include "run-loop.hpp"
#include "../../utils/util-hook.hpp"
#include "../xlive.hpp"
#include "../config/config.hpp"
#include "../../xlivelessness.hpp"
#include "../menus/custom-menu-functions.hpp"
#include "../coop/coop.hpp"

static BYTE* Quit_Exit_Game;
static bool QuitRunLoop = false;

static void FadeFromBlackWhite()
{
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Coop Fading from Black_White."
		, __func__
	);
	
	gameEngine = 2;
	char(*PlayerEffects)();
	PlayerEffects = (char(*)(void))GetOffsetAddress(CLIENT_11122, 0x000a3e39);
	PlayerEffects();
	gameEngine = 1;
}

static int H2Config_hotkeyIdTestA = VK_F5;
static void hotkeyFuncTestA()
{
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	//FadeFromBlackWhite();
}

static int H2Config_hotkeyIdTestB = VK_F6;
static void hotkeyFuncTestB()
{
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	//extern void FloodSpawner1();
	//FloodSpawner1();
}

static int H2Config_hotkeyIdTestC = VK_F7;
static void hotkeyFuncTestC()
{
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	//extern void FloodSpawner2();
	//FloodSpawner2();
}

static const int hotkeyLen = 3;
static int hotkeyListenLen = 0;
static int* hotkeyId[hotkeyLen] = { &H2Config_hotkeyIdTestA, &H2Config_hotkeyIdTestB, &H2Config_hotkeyIdTestC };
static bool hotkeyPressed[hotkeyLen] = { false, false, false };
static void(*hotkeyFunc[hotkeyLen])(void) = { hotkeyFuncTestA, hotkeyFuncTestB, hotkeyFuncTestC };

static void RunLoop()
{
	if ((Title_Version == CLIENT_11122 || Title_Version == CLIENT_11081 || Title_Version == CLIENT_11091) && (GetFocus() == H2hWnd || GetForegroundWindow() == H2hWnd)) {
		
		for (int i = 0; i < hotkeyListenLen; i++) {
			//& 0x8000 is pressed
			//& 0x1 Key just transitioned from released to pressed.
			if (GetAsyncKeyState(*hotkeyId[i]) & 0x8000) {
				hotkeyPressed[i] = true;
			}
			else if (!(GetAsyncKeyState(*hotkeyId[i]) & 0x8000) && hotkeyPressed[i]) {
				hotkeyPressed[i] = false;
				hotkeyFunc[i]();
			}
		}
	}
	
	LoopCoop();
}

static signed int(*sub_287a1)();
static signed int HookedClientRandFunc()
{
	if (!QuitRunLoop) {
		RunLoop();
	}
	
	signed int result = sub_287a1();
	return result;
}

static char HookedServerShutdownCheck()
{
	if (!QuitRunLoop) {
		RunLoop();
	}
	
	if (*Quit_Exit_Game) {
		//DeinitH2Startup();
	}
	
	//original test - if game should shutdown
	return *Quit_Exit_Game;
}

bool InitRunLoop()
{
	if (Title_Version == SERVER_11122) {
		Quit_Exit_Game = (BYTE*)GetOffsetAddress(SERVER_11122, 0x004a7083);
		PatchCall(GetOffsetAddress(SERVER_11122, 0x0000c6cb), HookedServerShutdownCheck);
	}
	else if (Title_Version == CLIENT_11122) {
		Quit_Exit_Game = (BYTE*)GetOffsetAddress(CLIENT_11122, 0x0048220b);
		sub_287a1 = (signed int(*)())GetOffsetAddress(CLIENT_11122, 0x000287a1);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x000399f3), HookedClientRandFunc);
	}
	
	return true;
}

bool UninitRunLoop()
{
	return true;
}
