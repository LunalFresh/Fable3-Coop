#include "social-controller.hpp"
#include <Xinput.h>
#include "debug-log.hpp"
#include <atomic>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace {
using GetState = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
std::atomic<GetState> originalState(nullptr);
void** stateImport = nullptr;
std::atomic<bool> capturing(false), observedPoll(false);
std::mutex controllerGate;
const WORD Directions = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN |
    XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
struct ControllerSlot {
    bool armed = false, release = false, filtered = false;
    WORD previous = 0, repeating = 0;
    ULONGLONG repeatAt = 0;
    ULONGLONG nextProbe = 0;
    DWORD packetBias = 0;
};
ControllerSlot controllers[XUSER_MAX_COUNT];

bool Neutral(const XINPUT_GAMEPAD& pad) {
    return !pad.wButtons && pad.bLeftTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD &&
        pad.bRightTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD &&
        abs((int)pad.sThumbLX) <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
        abs((int)pad.sThumbLY) <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
        abs((int)pad.sThumbRX) <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE &&
        abs((int)pad.sThumbRY) <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
}
WORD Buttons(const XINPUT_GAMEPAD& pad) {
    WORD buttons = pad.wButtons;
    if(pad.sThumbLX < -16000) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
    if(pad.sThumbLX > 16000) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    if(pad.sThumbLY < -16000) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
    if(pad.sThumbLY > 16000) buttons |= XINPUT_GAMEPAD_DPAD_UP;
    return buttons;
}
DWORD WINAPI FilteredGetState(DWORD user, XINPUT_STATE* state) {
    const auto original = originalState.load();
    if(!original) return ERROR_DEVICE_NOT_CONNECTED;
    DWORD result = original(user, state);
    if(!observedPoll.exchange(true))
        XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO,
            "SocialController: title called the controller.1 XInput filter.");
    if(result != ERROR_SUCCESS || !state || user >= XUSER_MAX_COUNT) return result;
    std::lock_guard<std::mutex> lock(controllerGate);
    auto& slot = controllers[user];
    if(capturing) slot.release = true;
    // Do not let the closing B/A press, triggers or held sticks reach the game.
    // Keep reporting a connected controller, with a changing packet on transitions.
    if(!capturing && slot.release && Neutral(state->Gamepad)) slot.release = false;
    const bool filter = capturing || slot.release;
    if(filter != slot.filtered) {++slot.packetBias; slot.filtered = filter;}
    if(filter) state->Gamepad = {};
    state->dwPacketNumber += slot.packetBias;
    return result;
}

bool ReplaceImport(void** slot, void* expected, void* replacement) {
    DWORD previous = 0;
    if(!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &previous)) return false;
    bool changed = InterlockedCompareExchangePointer(slot, replacement, expected) == expected;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), previous, &ignored);
    return changed;
}

// Inspect the loaded image, never the on-disk executable. The supported Fable
// build imports XInputGetState by ordinal 2; accept its named equivalent too.
bool InstallControllerImport(HMODULE module) {
    if(stateImport) return true;
    auto base = reinterpret_cast<BYTE*>(module);
    if(!base) return false;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000) return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if(nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) return false;
    const DWORD size = nt->OptionalHeader.SizeOfImage;
    const auto imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!imports.VirtualAddress || imports.VirtualAddress >= size || imports.Size > size - imports.VirtualAddress) return false;
    auto descriptors = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
    for(size_t i = 0; i < imports.Size / sizeof(*descriptors) && descriptors[i].Name; ++i) {
        const auto& descriptor = descriptors[i];
        if(descriptor.Name >= size || !memchr(base + descriptor.Name, 0, size - descriptor.Name)) return false;
        if(_stricmp(reinterpret_cast<char*>(base + descriptor.Name), "xinput1_3.dll")) continue;
        if(!descriptor.OriginalFirstThunk || descriptor.OriginalFirstThunk >= size || descriptor.FirstThunk >= size) return false;
        const auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor.OriginalFirstThunk);
        const auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor.FirstThunk);
        const size_t count = (size - (std::max)(descriptor.FirstThunk, descriptor.OriginalFirstThunk)) / sizeof(*names);
        for(size_t j = 0; j < count && names[j].u1.AddressOfData; ++j) {
            bool match = IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal) && IMAGE_ORDINAL(names[j].u1.Ordinal) == 2;
            if(!IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal)) {
                auto rva = names[j].u1.AddressOfData;
                if(rva >= size || size - rva < sizeof(IMAGE_IMPORT_BY_NAME)) return false;
                const auto name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + rva)->Name;
                if(!memchr(name, 0, size - rva - sizeof(WORD))) return false;
                match = !strcmp(reinterpret_cast<const char*>(name), "XInputGetState");
            }
            if(!match) continue;
            auto slot = reinterpret_cast<void**>(&slots[j].u1.Function);
            auto previous = reinterpret_cast<GetState>(*slot);
            if(!previous || previous == FilteredGetState) return false;
            originalState = previous;
            if(!ReplaceImport(slot, reinterpret_cast<void*>(previous), reinterpret_cast<void*>(FilteredGetState))) return false;
            stateImport = slot;
            return true;
        }
    }
    return false;
}
}

bool SocialControllerAttach() {
    wchar_t executable[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    const auto slash = wcsrchr(executable, L'\\');
    if(_wcsicmp(slash ? slash + 1 : executable, L"Fable3.exe")) return false;
    bool attached = InstallControllerImport(GetModuleHandleW(nullptr));
    XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_INFO,
        "SocialController: controller.1 XInputGetState import %s; keyboard/mouse remain available.",
        attached ? "attached" : "unavailable (controller navigation disabled)");
    return attached;
}
void SocialControllerCapture(bool capture) {
    std::lock_guard<std::mutex> lock(controllerGate);
    if(capturing.exchange(capture) == capture) return;
    for(auto& slot : controllers) {
        slot.armed = false; slot.previous = slot.repeating = 0; slot.repeatAt = 0;
        slot.nextProbe = 0;
        if(capture) slot.release = true;
    }
}
void SocialControllerDetach() {
    SocialControllerCapture(false);
    if(stateImport && ReplaceImport(stateImport, reinterpret_cast<void*>(FilteredGetState),
            reinterpret_cast<void*>(originalState.load()))) stateImport = nullptr;
    // Retain the original function for a concurrent call already inside the hook.
    std::lock_guard<std::mutex> lock(controllerGate);
    for(auto& slot : controllers) slot = ControllerSlot{};
}
void SocialControllerSuspend() {
    std::lock_guard<std::mutex> lock(controllerGate);
    for(auto& slot : controllers) {
        slot.armed = false; slot.previous = slot.repeating = 0; slot.repeatAt = 0;
    }
}
SocialControllerFrame SocialControllerPoll(bool foreground, ULONGLONG now) {
    SocialControllerFrame frame;
    auto original = originalState.load();
    if(!stateImport || !original) return frame;
    for(DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
        {
            std::lock_guard<std::mutex> lock(controllerGate);
            if(now < controllers[user].nextProbe) continue;
        }
        XINPUT_STATE state = {};
        DWORD result = original(user, &state);
        std::lock_guard<std::mutex> lock(controllerGate);
        auto& slot = controllers[user];
        if(result != ERROR_SUCCESS || !foreground || !capturing) {
            slot.armed = false; slot.previous = slot.repeating = 0; slot.repeatAt = 0;
            if(result == ERROR_SUCCESS) frame.connected = true;
            else slot.nextProbe = now + 1000; // Avoid polling absent devices every render frame.
            continue;
        }
        frame.connected = true;
        if(!slot.armed) {
            if(Neutral(state.Gamepad)) slot.armed = true;
            continue;
        }
        WORD buttons = Buttons(state.Gamepad);
        WORD pressed = buttons & ~slot.previous;
        WORD direction = buttons & Directions;
        if(direction != slot.repeating) {slot.repeating = direction; slot.repeatAt = now + 400;}
        else if(direction && now >= slot.repeatAt) {pressed |= direction; slot.repeatAt = now + 125;}
        slot.previous = buttons;
        // One controller action per frame; all four slots can own the menu.
        if(!frame.pressed) frame.pressed = pressed;
    }
    return frame;
}
