#pragma once

// Sets the dinput/xinput version. Halo 2 uses the older 9.1.0 version and DirectInput 0x0800.
#define XINPUT_USE_9_1_0
#define DIRECTINPUT_VERSION 0x0800

#define TITLEMODULEANY 0xFFFFFFFF
#define CLIENT_11081 1
#define CLIENT_11091 2
#define CLIENT_11122 3
#define SERVER_11081 4
#define SERVER_11091 5
#define SERVER_11122 6
extern DWORD Title_Version;

extern wchar_t *lpwszTitleModulePath;
extern wchar_t* H2ProcessFilePath;
extern wchar_t* H2AppDataLocal;
extern wchar_t* FlagFilePathConfig;

DWORD GetOffsetAddress(DWORD dwTitleVersion, DWORD dwOffset);
DWORD GetOffsetAddress(DWORD dwOffsetC_11081, DWORD dwOffsetC_11091, DWORD dwOffsetC_11122);
template <typename T = void*>
T GetOffsetAddress(DWORD dwTitleVersion, DWORD dwOffset)
{
	return reinterpret_cast<T>(GetOffsetAddress(dwTitleVersion, dwOffset));
}
template <typename T = void*>
T GetOffsetAddressCS(DWORD dwOffsetC_11122, DWORD dwOffsetS_11122)
{
	if (Title_Version == CLIENT_11122) {
		return reinterpret_cast<T>(GetOffsetAddress(CLIENT_11122, dwOffsetC_11122));
	}
	else if(Title_Version == SERVER_11122) {
		return reinterpret_cast<T>(GetOffsetAddress(SERVER_11122, dwOffsetS_11122));
	}
	return NULL;
}
