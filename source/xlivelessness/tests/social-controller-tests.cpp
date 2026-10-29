// Patch a disposable loaded-PE fixture and call through its real IAT slot.
// The original function supplies controlled raw states; no game is launched.
#include "../xlivelessness/xlln/social-controller.cpp"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>

XINPUT_STATE raw[XUSER_MAX_COUNT] = {};
DWORD results[XUSER_MAX_COUNT] = {1167,1167,1167,1167};
int calls[XUSER_MAX_COUNT] = {};
uint32_t WINAPI XLLNDebugLogF(uint32_t,const char* const,...) {return 0;}
DWORD WINAPI FakeState(DWORD user,XINPUT_STATE* state) {
    if(user>=XUSER_MAX_COUNT)return ERROR_BAD_ARGUMENTS;
    ++calls[user];
    if(results[user]==ERROR_SUCCESS && state)*state=raw[user];
    return results[user];
}
BYTE* Image(bool named) {
    auto base=(BYTE*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(base);
    auto dos=(IMAGE_DOS_HEADER*)base;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto nt=(IMAGE_NT_HEADERS*)(base+0x80);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR_MAGIC;nt->OptionalHeader.SizeOfImage=4096;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={0x200,2*sizeof(IMAGE_IMPORT_DESCRIPTOR)};
    auto desc=(IMAGE_IMPORT_DESCRIPTOR*)(base+0x200);
    desc->Name=0x240;strcpy_s((char*)base+0x240,32,"XINPUT1_3.dll");
    desc->OriginalFirstThunk=0x280;desc->FirstThunk=0x300;
    auto names=(IMAGE_THUNK_DATA*)(base+0x280);
    names[0].u1.Ordinal=named?0x340:IMAGE_ORDINAL_FLAG|2;
    auto name=(IMAGE_IMPORT_BY_NAME*)(base+0x340);strcpy_s((char*)name->Name,32,"XInputGetState");
    *(GetState*)(base+0x300)=FakeState;
    return base;
}
int main() {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    for(bool named : {false,true}) {
        auto image=Image(named);assert(InstallControllerImport((HMODULE)image));
        assert(*(GetState*)(image+0x300)==FilteredGetState);
        results[1]=ERROR_SUCCESS;raw[1].dwPacketNumber=42;raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;
        XINPUT_STATE game={};
        auto invoke=[&](DWORD slot) {return (*(GetState*)(image+0x300))(slot,&game);};
        assert(invoke(1)==ERROR_SUCCESS && game.Gamepad.wButtons==XINPUT_GAMEPAD_A && game.dwPacketNumber==42);
        SocialControllerCapture(true);
        assert(invoke(1)==ERROR_SUCCESS && game.Gamepad.wButtons==0 && game.dwPacketNumber!=42);
        auto neutralPacket=game.dwPacketNumber;
        assert(invoke(0)==ERROR_DEVICE_NOT_CONNECTED);assert(invoke(4)==ERROR_BAD_ARGUMENTS);
        // A held when opening cannot select anything; slot 1 works without slot 0.
        assert(!SocialControllerPoll(true,100).pressed);
        raw[1].Gamepad={};assert(SocialControllerPoll(true,110).connected);
        raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;
        assert(SocialControllerPoll(true,120).pressed==XINPUT_GAMEPAD_A);
        assert(!SocialControllerPoll(true,130).pressed);
        raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;
        assert(SocialControllerPoll(true,140).pressed==XINPUT_GAMEPAD_DPAD_DOWN);
        assert(!SocialControllerPoll(true,539).pressed);
        assert(SocialControllerPoll(true,540).pressed==XINPUT_GAMEPAD_DPAD_DOWN);
        assert(!SocialControllerPoll(true,664).pressed);
        assert(SocialControllerPoll(true,665).pressed==XINPUT_GAMEPAD_DPAD_DOWN);
        // Stick drift is neutral; deliberate movement has the same repeat logic.
        raw[1].Gamepad={};raw[1].Gamepad.sThumbLX=5000;
        assert(!SocialControllerPoll(true,680).pressed);
        raw[1].Gamepad.sThumbLX=20000;
        assert(SocialControllerPoll(true,690).pressed==XINPUT_GAMEPAD_DPAD_RIGHT);
        // Alt-tab/re-entry must observe a neutral controller before accepting A.
        raw[1].Gamepad={};raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;
        assert(!SocialControllerPoll(false,700).pressed);
        assert(!SocialControllerPoll(true,710).pressed);
        raw[1].Gamepad={};SocialControllerPoll(true,720);
        raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_B;
        assert(SocialControllerPoll(true,730).pressed==XINPUT_GAMEPAD_B);
        // WM_KILLFOCUS can arrive while D3D9 is lost and rendering has stopped.
        SocialControllerSuspend();raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;
        assert(!SocialControllerPoll(true,740).pressed);
        SocialControllerCapture(false);
        assert(invoke(1)==ERROR_SUCCESS && !game.Gamepad.wButtons);
        raw[1].Gamepad={};raw[1].Gamepad.bRightTrigger=255;raw[1].Gamepad.sThumbRY=22000;
        assert(invoke(1)==ERROR_SUCCESS && !game.Gamepad.bRightTrigger && !game.Gamepad.sThumbRY);
        raw[1].Gamepad={};assert(invoke(1)==ERROR_SUCCESS && game.dwPacketNumber!=neutralPacket);
        raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;assert(invoke(1)==ERROR_SUCCESS && game.Gamepad.wButtons==XINPUT_GAMEPAD_A);
        // Unplug/replug, even while A is held, cannot activate the menu.
        SocialControllerCapture(true);results[1]=ERROR_DEVICE_NOT_CONNECTED;
        assert(!SocialControllerPoll(true,800).connected);
        auto before=calls[1];assert(!SocialControllerPoll(true,801).connected && calls[1]==before);
        results[1]=ERROR_SUCCESS;assert(!SocialControllerPoll(true,1800).pressed);
        raw[1].Gamepad={};SocialControllerPoll(true,1810);
        raw[1].Gamepad.wButtons=XINPUT_GAMEPAD_A;assert(SocialControllerPoll(true,1820).pressed==XINPUT_GAMEPAD_A);
        // Opening captures all connected controllers, not only the menu operator.
        results[3]=ERROR_SUCCESS;raw[3].Gamepad.wButtons=XINPUT_GAMEPAD_Y;
        assert(invoke(3)==ERROR_SUCCESS && !game.Gamepad.wButtons);
        SocialControllerDetach();assert(*(GetState*)(image+0x300)==FakeState);
        assert(invoke(1)==ERROR_SUCCESS && game.Gamepad.wButtons==XINPUT_GAMEPAD_A && game.dwPacketNumber==42);
        VirtualFree(image,0,MEM_RELEASE);results[3]=ERROR_DEVICE_NOT_CONNECTED;
    }
    auto bad=Image(false);auto names=(IMAGE_THUNK_DATA*)(bad+0x280);names[0].u1.Ordinal=IMAGE_ORDINAL_FLAG|3;
    assert(!InstallControllerImport((HMODULE)bad));assert(*(GetState*)(bad+0x300)==FakeState);
    VirtualFree(bad,0,MEM_RELEASE);
    puts("PASS: named/ordinal IAT interception and restoration, passthrough, all-slot capture, packet transitions, release gating, repeat/deadzone, focus loss, disconnect/reconnect, absent-slot throttle");
}
