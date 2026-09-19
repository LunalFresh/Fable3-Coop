#pragma once
#include <Windows.h>
#include <cstdint>
struct IDirect3DDevice9;

// Device calls run on the title's render/device thread. Opening and invite
// notifications may originate on the game or network threads.
void SocialOverlayAttach(IDirect3DDevice9* device, HWND window);
void SocialOverlayDetach();
void SocialOverlayReset(HWND window);
void SocialOverlayRender();
bool SocialOverlayShow(uint32_t user);
bool SocialOverlayIsOpen();
bool SocialOverlayClose();
bool SocialOverlayNotifyInvite(uint32_t user);
bool SocialOverlayNotifyFriend(uint32_t user);
