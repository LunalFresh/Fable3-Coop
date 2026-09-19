#include "../xlivelessness/xlln/network-instance-id.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <crtdbg.h>
unsigned calls=0;
NTSTATUS WINAPI ZeroThenValid(BCRYPT_ALG_HANDLE provider,PUCHAR out,ULONG size,ULONG flags) {
    assert(provider==nullptr && size==4 && flags==BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    uint32_t sample=++calls==1?0xab000000:0xff3968cd;memcpy(out,&sample,4);return 0;
}
NTSTATUS WINAPI Failure(BCRYPT_ALG_HANDLE,PUCHAR,ULONG,ULONG) {++calls;return (NTSTATUS)0xc0000001;}
NTSTATUS WINAPI Zero(BCRYPT_ALG_HANDLE,PUCHAR out,ULONG size,ULONG) {++calls;memset(out,0,size);return 0;}
int main() {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    // Reproduce the actual duplicate ID from the recorded startup second.
    srand(1789840553);uint32_t first=rand(),second=rand();
    assert((((first&0x7fff)<<15)|(second&0x7fff))%0x1000000==0x003968cd);
    uint32_t result=99;calls=0;assert(XllnGenerateNetworkInstanceId(result,ZeroThenValid) && result==0x003968cd && calls==2);
    result=99;calls=0;assert(!XllnGenerateNetworkInstanceId(result,Failure) && result==0 && calls==1);
    result=99;calls=0;assert(!XllnGenerateNetworkInstanceId(result,Zero) && result==0 && calls==8);
    std::set<uint32_t> values;
    for(unsigned i=0;i<128;++i) {
        srand(1789840553); // Identical startup-second seeds must not dictate network identity.
        assert(XllnGenerateNetworkInstanceId(result));assert(result>0 && result<=0x00ffffff);values.insert(result);
    }
    assert(values.size()>100); // Allows chance collisions in the required 24-bit namespace.
    puts("PASS: recorded duplicate reproduced; OS entropy independent of time seed; 24-bit range, zero retry, bounded failure");
}
