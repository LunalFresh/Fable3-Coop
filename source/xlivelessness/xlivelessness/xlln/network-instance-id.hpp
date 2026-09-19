#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <cstdint>

// Fable requires a nonzero 24-bit virtual address. The process ID must not be
// drawn from srand(time(NULL)): peers started in the same second then collide.
using XllnRandomFunction = NTSTATUS (WINAPI*)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);
inline bool XllnGenerateNetworkInstanceId(uint32_t& result, XllnRandomFunction random = BCryptGenRandom)
{
    result=0;
    for(unsigned attempt=0;attempt<8;++attempt) {
        uint32_t sample=0;
        if(random(nullptr,reinterpret_cast<PUCHAR>(&sample),sizeof(sample),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return false;
        sample &= 0x00ffffff;
        if(sample) {result=sample;return true;}
    }
    return false;
}
