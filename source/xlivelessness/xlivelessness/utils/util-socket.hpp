#pragma once
#include <stdio.h>
#include <stdint.h>
#include <ws2tcpip.h>

uint16_t GetSockAddrPort(const SOCKADDR_STORAGE* sockAddrStorage);
bool SetSockAddrPort(SOCKADDR_STORAGE* sockAddrStorage, uint16_t portHBO);
char* GetSockAddrInfo(const SOCKADDR_STORAGE* sockAddrStorage);
bool SockAddrsMatch(const SOCKADDR_STORAGE* sockAddr1, const SOCKADDR_STORAGE* sockAddr2);
bool SockAddrsMatch(const SOCKADDR_STORAGE* sockAddr1, const SOCKADDR_STORAGE* sockAddr2, bool only_family_and_address);
DWORD IPHLPAPI_ConvertLengthToIpv4Mask(uint32_t mask_length, uint32_t* mask);
int WS2_32_inet_pton(int af, const char* src, void* dst);
const char* WS2_32_inet_ntop(int af, const void* src, char* dst, socklen_t size);
