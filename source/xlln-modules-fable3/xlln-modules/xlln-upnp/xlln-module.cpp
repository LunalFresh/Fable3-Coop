#include <winsock2.h>
#include "../dllmain.hpp"
#include "xlln-module.hpp"
#include "../xlivelessness.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../third-party/miniupnpc/miniupnpc.h"
#include "../third-party/miniupnpc/upnpcommands.h"
#include <set>
#include <map>
#include <WS2tcpip.h>
// Link with iphlpapi.lib
#include <iphlpapi.h>

// 59 minutes.
static const uint32_t milliseconds_between_remaps = 1000*60*59;
// 60 minutes.
static const uint32_t lease_duration_seconds = 60*60;
static const char lease_duration_seconds_str[] = "3600";

// <protocol, port>.
typedef std::pair<int32_t, uint16_t> ProtocolPort;

static const char* GetSocketProtocolAsText(int32_t protocol)
{
	switch (protocol) {
		case IPPROTO_UDP:
			return "UDP";
		case IPPROTO_TCP:
			return "TCP";
	}
	return "?";
}

static CRITICAL_SECTION xmod_critsec_api;
static const ADDRESS_FAMILY XMOD_ADDRESS_FAMILY_INVALID = (ADDRESS_FAMILY)-1;
static SOCKADDR_STORAGE xmod_preferred_interface_address_gateway = {AF_UNSPEC};
static std::set<ProtocolPort> xmod_ports_in_use;

typedef enum : uint8_t {
	UNKNOWN = 0,
	UPnP,
	NAT_PMP,
	//TODO implement PCP.
	PCP,
} FORWARDED_ON_SERVICE_TYPE;
typedef struct _XMOD_NETWORK_INTERFACE {
	// Can be null.
	char* name = 0;
	// Can be null.
	wchar_t* description = 0;
	SOCKADDR_STORAGE addressGateway = {AF_UNSPEC};
	SOCKADDR_STORAGE addressUnicast = {AF_UNSPEC};
	SOCKADDR_STORAGE addressBroadcast = {AF_UNSPEC};
	SOCKADDR_STORAGE addressMulticast = {AF_UNSPEC};
	
	HANDLE threadEventUpdate = INVALID_HANDLE_VALUE;
	HANDLE threadEventExited = INVALID_HANDLE_VALUE;
	bool threadExit = false;
	std::map<ProtocolPort, FORWARDED_ON_SERVICE_TYPE> forwardedPorts;
	std::set<ProtocolPort> failedPorts;
	char* upnpIgdControlUrl = 0;
	char* upnpIgdServiceType = 0;
	bool upnpIgdSupportsLeaseDuration = true;
	
	_XMOD_NETWORK_INTERFACE()
	{
		this->threadEventUpdate = CreateEventA(NULL, FALSE, FALSE, NULL);
		this->threadEventExited = CreateEventA(NULL, FALSE, FALSE, NULL);
	}
	
	~_XMOD_NETWORK_INTERFACE()
	{
		if (name) {
			delete[] name;
			name = 0;
		}
		if (description) {
			delete[] description;
			description = 0;
		}
		
		CloseHandle(this->threadEventUpdate);
		CloseHandle(this->threadEventExited);
	}
	
} XMOD_NETWORK_INTERFACE;
static std::set<XMOD_NETWORK_INTERFACE*> xmod_network_interfaces;

static HANDLE xmod_upnp_igd_search_complete = INVALID_HANDLE_VALUE;
static SOCKET xmod_network_socket_gateway = INVALID_SOCKET;
static HANDLE xmod_network_thread_event_exited = INVALID_HANDLE_VALUE;

static DWORD WINAPI XModThreadSearchUpnpIgd(void* lpParam)
{
	int errorUpnp = 0;
	UPNPDev* discoveredUpnpDevices = upnpDiscover(2000, 0, 0, UPNP_LOCAL_PORT_ANY, FALSE, 2, &errorUpnp);
	if (errorUpnp != UPNPDISCOVER_SUCCESS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s failed to discover with error (%d)."
			, __func__
			, errorUpnp
		);
		
		EnterCriticalSection(&xmod_critsec_api);
		
		for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
			if (networkInterface->upnpIgdControlUrl) {
				delete[] networkInterface->upnpIgdControlUrl;
				networkInterface->upnpIgdControlUrl = 0;
			}
			if (networkInterface->upnpIgdServiceType) {
				delete[] networkInterface->upnpIgdServiceType;
				networkInterface->upnpIgdServiceType = 0;
			}
			networkInterface->upnpIgdSupportsLeaseDuration = true;
		}
		
		LeaveCriticalSection(&xmod_critsec_api);
		
		SetEvent(xmod_upnp_igd_search_complete);
		return ERROR_SUCCESS;
	}
	
	size_t countDevices = 0;
	for (UPNPDev* iDevice = discoveredUpnpDevices; iDevice; iDevice = iDevice->pNext) {
		countDevices++;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG,
			"[UPnP] Found UPnP IGD: Desc URL (%s), st (%s), usn (%s), scope id (%d)."
			, iDevice->descURL
			, iDevice->st
			, iDevice->usn
			, iDevice->scope_id
		);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "[UPnP] UPnP IGD Count (%zu)."
		, countDevices
	);
	
	EnterCriticalSection(&xmod_critsec_api);
	
	std::set<XMOD_NETWORK_INTERFACE*> networkInterfacesMatched;
	
	// Search for the IGD (if any) on the target interface.
	for (UPNPDev* iDevice = discoveredUpnpDevices; iDevice;) {
		IGDdatas igdDatas;
		char addrLan[INET6_ADDRSTRLEN];
		char addrWan[INET6_ADDRSTRLEN];
		
		UPNPUrls discoveredIgdUrls;
		memset(&discoveredIgdUrls, 0, sizeof(discoveredIgdUrls));
		
		errorUpnp = UPNP_GetValidIGD(iDevice, &discoveredIgdUrls, &igdDatas, addrLan, sizeof(addrLan), addrWan, sizeof(addrWan));
		if (errorUpnp != 1) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
				, "[UPnP] %s UPNP_GetValidIGD failed with (%d)."
				, __func__
				, errorUpnp
			);
			FreeUPNPUrls(&discoveredIgdUrls);
			break;
		}
		
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG,
		//	"[UPnP] UPnP IGD: controlURL (%s), controlURL_6FC (%s), controlURL_CIF (%s), ipcondescURL (%s), rootdescURL (%s)."
		//	, discoveredIgdUrls.controlURL
		//	, discoveredIgdUrls.controlURL_6FC
		//	, discoveredIgdUrls.controlURL_CIF
		//	, discoveredIgdUrls.ipcondescURL
		//	, discoveredIgdUrls.rootdescURL
		//);
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG,
		//	"[UPnP] UPnP IGD: cureltname (%s), urlbase (%s), presentationurl (%s)."
		//	, igdDatas.cureltname
		//	, igdDatas.urlbase
		//	, igdDatas.presentationurl
		//);
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG,
		//	"[UPnP] IPnP IGD: addrLan (%s), addrWan (%s)."
		//	, addrLan
		//	, addrWan
		//);
		
		SOCKADDR_STORAGE addressIdgLanUnicast;
		int32_t addressIdgLanUnicastSize = (int32_t)sizeof(addressIdgLanUnicast);
		memset(&addressIdgLanUnicast, 0, addressIdgLanUnicastSize);
		size_t addrLanSize = strlen(addrLan) + 1;
		char* addrLanCopy = new char[addrLanSize];
		memcpy(addrLanCopy, addrLan, addrLanSize);
		if (WSAStringToAddressA(addrLanCopy, AF_INET, NULL, (sockaddr*)&addressIdgLanUnicast, &addressIdgLanUnicastSize) != 0) {
			memcpy(addrLanCopy, addrLan, addrLanSize);
			if (WSAStringToAddressA(addrLanCopy, AF_INET6, NULL, (sockaddr*)&addressIdgLanUnicast, &addressIdgLanUnicastSize) != 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[UPnP] %s Failed to parse returned local network interface address (%s)."
					, __func__
					, addrLan
				);
				((sockaddr_in*)&addressIdgLanUnicast)->sin_family = AF_UNSPEC;
			}
		}
		
		delete[] addrLanCopy;
		addrLanCopy = 0;
		
		for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
			if (!SockAddrsMatch(&networkInterface->addressUnicast, &addressIdgLanUnicast, true)) {
				continue;
			}
			
			if (
				(networkInterface->upnpIgdControlUrl && strcmp(networkInterface->upnpIgdControlUrl, discoveredIgdUrls.controlURL) != 0)
				|| (networkInterface->upnpIgdServiceType && strcmp(networkInterface->upnpIgdServiceType, igdDatas.first.servicetype) != 0)
			) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN,
					"[UPnP] UPnP IGD changed for interface name (%s) description (%ls): addrLan (%s), addrWan (%s), old controlURL (%s), old servicetype (%s), new controlURL (%s), new servicetype (%s)."
					, (!networkInterface->name ? "" : networkInterface->name)
					, (!networkInterface->description ? L"" : networkInterface->description)
					, addrLan
					, addrWan
					, (!networkInterface->upnpIgdControlUrl ? "" : networkInterface->upnpIgdControlUrl)
					, (!networkInterface->upnpIgdServiceType ? "" : networkInterface->upnpIgdServiceType)
					, discoveredIgdUrls.controlURL
					, igdDatas.first.servicetype
				);
			}
			
			if (networkInterface->upnpIgdControlUrl) {
				delete[] networkInterface->upnpIgdControlUrl;
				networkInterface->upnpIgdControlUrl = 0;
			}
			if (networkInterface->upnpIgdServiceType) {
				delete[] networkInterface->upnpIgdServiceType;
				networkInterface->upnpIgdServiceType = 0;
			}
			
			networkInterface->upnpIgdControlUrl = CloneString(discoveredIgdUrls.controlURL);
			networkInterface->upnpIgdServiceType = CloneString(igdDatas.first.servicetype);
			networkInterface->upnpIgdSupportsLeaseDuration = true;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO,
				"[UPnP] UPnP IGD found for interface name (%s) description (%ls): addrLan (%s), addrWan (%s), controlURL (%s), servicetype (%s)."
				, (!networkInterface->name ? "" : networkInterface->name)
				, (!networkInterface->description ? L"" : networkInterface->description)
				, addrLan
				, addrWan
				, networkInterface->upnpIgdControlUrl
				, networkInterface->upnpIgdServiceType
			);
			
			networkInterfacesMatched.insert(networkInterface);
			break;
		}
		
		// Skip until we find the current one in the list.
		while (iDevice && strcmp(iDevice->descURL, discoveredIgdUrls.rootdescURL) != 0) {
			iDevice = iDevice->pNext;
		}
		// Then skip that one too.
		if (iDevice) {
			iDevice = iDevice->pNext;
		}
		
		FreeUPNPUrls(&discoveredIgdUrls);
	}
	
	for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
		auto itrNetworkInterfaceExisting = networkInterfacesMatched.find(networkInterface);
		if (itrNetworkInterfaceExisting != networkInterfacesMatched.end()) {
			continue;
		}
		
		if (networkInterface->upnpIgdControlUrl) {
			delete[] networkInterface->upnpIgdControlUrl;
			networkInterface->upnpIgdControlUrl = 0;
		}
		if (networkInterface->upnpIgdServiceType) {
			delete[] networkInterface->upnpIgdServiceType;
			networkInterface->upnpIgdServiceType = 0;
		}
		networkInterface->upnpIgdSupportsLeaseDuration = true;
	}
	
	LeaveCriticalSection(&xmod_critsec_api);
	
	freeUPNPDevlist(discoveredUpnpDevices);
	
	if (!networkInterfacesMatched.size()) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO,
			"[UPnP] No UPnP IGDs found on any local network interface."
		);
	}
	networkInterfacesMatched.clear();
	
	SetEvent(xmod_upnp_igd_search_complete);
	
	return ERROR_SUCCESS;
}

static bool XModSearchUpnpIgd()
{
	if (WaitForSingleObject(xmod_upnp_igd_search_complete, 0) == WAIT_TIMEOUT) {
		return false;
	}
	
	ResetEvent(xmod_upnp_igd_search_complete);
	
	HANDLE resultCreateThread = CreateThread(0, 0, XModThreadSearchUpnpIgd, (void*)0, 0, 0);
	if (!resultCreateThread) {
		uint32_t errorCreateThread = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCreateThread, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s Failed to create XModThreadSearchUpnpIgd."
			, __func__
		);
		
		SetEvent(xmod_upnp_igd_search_complete);
		return false;
	}
	
	return true;
}

static bool ChangeMappingUpnp(XMOD_NETWORK_INTERFACE* network_interface, int32_t protocol, uint16_t port, bool add_mapping)
{
	if (xmod_network_socket_gateway == INVALID_SOCKET) {
		return false;
	}
	if (network_interface->threadExit && add_mapping) {
		return false;
	}
	
	DWORD resultWait = WaitForSingleObject(xmod_upnp_igd_search_complete, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		if (resultWait == WAIT_FAILED) {
			resultWait = GetLastError();
		}
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s failed to wait for xmod_upnp_igd_search_complete."
			, __func__
		);
		return false;
	}
	
	EnterCriticalSection(&xmod_critsec_api);
	
	if (
		!network_interface->upnpIgdControlUrl
		|| !network_interface->upnpIgdServiceType
	) {
		if (!add_mapping) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
				"[UPnP] %s for protocol (%s) port (%hu) on interface name (%s) description (%ls) failed because internal service is unavailable."
				, __func__
				, GetSocketProtocolAsText(protocol)
				, port
				, (!network_interface->name ? "" : network_interface->name)
				, (!network_interface->description ? L"" : network_interface->description)
			);
		}
		
		LeaveCriticalSection(&xmod_critsec_api);
		
		return false;
	}
	
	char* upnpIgdControlUrl = CloneString(network_interface->upnpIgdControlUrl);
	char* upnpIgdServiceType = CloneString(network_interface->upnpIgdServiceType);
	ProtocolPort protocolPort = std::make_pair(protocol, port);
	
	if (!add_mapping) {
		LeaveCriticalSection(&xmod_critsec_api);
		
		char* argStringPort = FormMallocString("%hu", port);
		int errorUpnp = UPNP_DeletePortMapping(upnpIgdControlUrl, upnpIgdServiceType, argStringPort, GetSocketProtocolAsText(protocol), 0);
		free(argStringPort);
		argStringPort = 0;
		
		delete[] upnpIgdControlUrl;
		upnpIgdControlUrl = 0;
		delete[] upnpIgdServiceType;
		upnpIgdServiceType = 0;
		
		{
			EnterCriticalSection(&xmod_critsec_api);
			
			if (errorUpnp != 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
					"[UPnP] %s remove for protocol (%s) port (%hu) on interface name (%s) description (%ls) failed with error (%d)."
					, __func__
					, GetSocketProtocolAsText(protocol)
					, port
					, (!network_interface->name ? "" : network_interface->name)
					, (!network_interface->description ? L"" : network_interface->description)
					, errorUpnp
				);
				
				LeaveCriticalSection(&xmod_critsec_api);
				
				return false;
			}
			
			auto itrForwardedPort = network_interface->forwardedPorts.find(protocolPort);
			if (itrForwardedPort != network_interface->forwardedPorts.end()) {
				if (itrForwardedPort->second == FORWARDED_ON_SERVICE_TYPE::UPnP) {
					network_interface->forwardedPorts.erase(itrForwardedPort);
				}
				else {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
						, "[UPnP] %s interface name (%s) description (%ls) has protocol (%s) port (%hu) recorded as forwarded on internal service type (%hhu) instead of (%hhu). Not deleting."
						, __func__
						, (!network_interface->name ? "" : network_interface->name)
						, (!network_interface->description ? L"" : network_interface->description)
						, GetSocketProtocolAsText(protocol)
						, port
						, FORWARDED_ON_SERVICE_TYPE::UPnP
						, itrForwardedPort->second
					);
				}
			}
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO,
				"[UPnP] Successfully deleted mapping for protocol (%s) port (%hu) on UPnP IGD on interface name (%s) description (%ls)."
				, GetSocketProtocolAsText(protocol)
				, port
				, (!network_interface->name ? "" : network_interface->name)
				, (!network_interface->description ? L"" : network_interface->description)
			);
			
			LeaveCriticalSection(&xmod_critsec_api);
		}
		
		return true;
	}
	
	// Maximum length of full domain name + null terminator.
	char interfaceAddressUnicast[253 + 1] = "127.0.0.1";
	int error = getnameinfo((sockaddr*)&network_interface->addressUnicast, sizeof(network_interface->addressUnicast), interfaceAddressUnicast, sizeof(interfaceAddressUnicast), NULL, 0, NI_NUMERICHOST);
	if (error) {
		LeaveCriticalSection(&xmod_critsec_api);
		
		delete[] upnpIgdControlUrl;
		upnpIgdControlUrl = 0;
		delete[] upnpIgdServiceType;
		upnpIgdServiceType = 0;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
			"[UPnP] %s Failed to convert interface_address_unicast to string form."
			, __func__
		);
		return false;
	}
	
	bool upnpIgdSupportsLeaseDuration = network_interface->upnpIgdSupportsLeaseDuration;
	
	LeaveCriticalSection(&xmod_critsec_api);
	
	bool upnpIgdSupportsLeaseDurationOld = upnpIgdSupportsLeaseDuration;
	
	while (1) {
		char* argStringPort = FormMallocString("%hu", port);
		int errorUpnp = UPNP_AddPortMapping(
			upnpIgdControlUrl
			, upnpIgdServiceType
			, argStringPort
			, argStringPort
			, interfaceAddressUnicast
			, "XLiveLessNess UPnP"
			, GetSocketProtocolAsText(protocol)
			, 0
			, upnpIgdSupportsLeaseDuration ? lease_duration_seconds_str : "0"
		);
		free(argStringPort);
		argStringPort = 0;
		
		if (errorUpnp == 725 && upnpIgdSupportsLeaseDuration) {
			upnpIgdSupportsLeaseDuration = false;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN,
				"[UPnP] %s Failed with error 725 OnlyPermanentLeasesSupported - The NAT implementation only supports permanent lease times on port mappings. Retrying with permanent leases."
				, __func__
			);
			continue;
		}
		
		{
			EnterCriticalSection(&xmod_critsec_api);
			
			if (errorUpnp == 0 && upnpIgdSupportsLeaseDurationOld != upnpIgdSupportsLeaseDuration) {
				if (
					network_interface->upnpIgdControlUrl
					&& network_interface->upnpIgdServiceType
					&& strcmp(network_interface->upnpIgdControlUrl, upnpIgdControlUrl) == 0
					&& strcmp(network_interface->upnpIgdServiceType, upnpIgdServiceType) == 0
				) {
					network_interface->upnpIgdSupportsLeaseDuration = upnpIgdSupportsLeaseDuration;
				}
			}
			
			delete[] upnpIgdControlUrl;
			upnpIgdControlUrl = 0;
			delete[] upnpIgdServiceType;
			upnpIgdServiceType = 0;
			
			if (errorUpnp != 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
					"[UPnP] %s add for protocol (%s) port (%hu) on interface name (%s) description (%ls) failed with error (%d)."
					, __func__
					, GetSocketProtocolAsText(protocol)
					, port
					, (!network_interface->name ? "" : network_interface->name)
					, (!network_interface->description ? L"" : network_interface->description)
					, errorUpnp
				);
				
				LeaveCriticalSection(&xmod_critsec_api);
				
				return false;
			}
			
			auto itrForwardedPort = network_interface->forwardedPorts.find(protocolPort);
			if (itrForwardedPort != network_interface->forwardedPorts.end() && itrForwardedPort->second != FORWARDED_ON_SERVICE_TYPE::UPnP) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[UPnP] %s interface name (%s) description (%ls) already had forwarded protocol (%s) port (%hu) on internal service type (%hhu) now overwriting with (%hhu)."
					, __func__
					, (!network_interface->name ? "" : network_interface->name)
					, (!network_interface->description ? L"" : network_interface->description)
					, GetSocketProtocolAsText(protocol)
					, port
					, itrForwardedPort->second
					, FORWARDED_ON_SERVICE_TYPE::UPnP
				);
			}
			
			network_interface->forwardedPorts[protocolPort] = FORWARDED_ON_SERVICE_TYPE::UPnP;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO,
				"[UPnP] Successfully forwarded mapping for protocol (%s) port (%hu) on UPnP IGD on interface name (%s) description (%ls)."
				, GetSocketProtocolAsText(protocol)
				, port
				, (!network_interface->name ? "" : network_interface->name)
				, (!network_interface->description ? L"" : network_interface->description)
			);
			
			LeaveCriticalSection(&xmod_critsec_api);
		}
		
		return true;
	}
}

static bool ChangeMappingNATPMP(XMOD_NETWORK_INTERFACE* network_interface, int32_t protocol, uint16_t port, bool add_mapping)
{
	if (xmod_network_socket_gateway == INVALID_SOCKET) {
		return false;
	}
	if (network_interface->threadExit && add_mapping) {
		return false;
	}
	
	uint8_t packetData[12];
	// Version.
	packetData[0] = 0;
	// OP Code: UDP = 1, TCP = 2.
	packetData[1] = (protocol == IPPROTO_UDP ? 1 : 2);
	// Reserved.
	*((uint16_t*)&packetData[2]) = 0;
	// Internal Port.
	*((uint16_t*)&packetData[4]) = htons(port);
	// External Port.
	*((uint16_t*)&packetData[6]) = htons(port);
	// Port Mapping Lifetime.
	*((uint32_t*)&packetData[8]) = htonl(add_mapping ? lease_duration_seconds : 0);
	
	ProtocolPort protocolPort = std::make_pair(protocol, port);
	
	EnterCriticalSection(&xmod_critsec_api);
	
	SOCKADDR_STORAGE addressGateway = network_interface->addressGateway;
	SetSockAddrPort(&addressGateway, 5351);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
		, "[UPnP] %s requesting to %s protocol (%s) port (%hu) for interface name (%s) description (%ls)."
		, __func__
		, (add_mapping ? "add" : "remove")
		, GetSocketProtocolAsText(protocol)
		, port
		, (!network_interface->name ? "" : network_interface->name)
		, (!network_interface->description ? L"" : network_interface->description)
	);
	
	LeaveCriticalSection(&xmod_critsec_api);
	
	for (uint8_t iAttempts = 0; iAttempts < 3; iAttempts++) {
		if (network_interface->threadExit && add_mapping) {
			return false;
		}
		
		int resultSendTo = sendto(xmod_network_socket_gateway, (char*)&packetData, sizeof(packetData), 0, (sockaddr*)&addressGateway, sizeof(addressGateway));
		if (resultSendTo <= 0) {
			int32_t errorSendTo = WSAGetLastError();
			char* sockAddrInfo = GET_SOCKADDR_INFO(&addressGateway);
			XLLN_DEBUG_LOG_ECODE(errorSendTo, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
				"[UPnP] %s sendto (%s) failed with error"
				, __func__
				, sockAddrInfo
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
				sockAddrInfo = 0;
			}
			break;
		}
		
		__timeb64 timeToWaitUntilForResponse;
		_ftime64_s(&timeToWaitUntilForResponse);
		TimeAddMillis(timeToWaitUntilForResponse, 250);
		
		while (1) {
			__timeb64 currentTime;
			_ftime64_s(&currentTime);
			int64_t millisecondsUntilRemap = TimeDiffMilliseconds(currentTime, timeToWaitUntilForResponse);
			
			if (millisecondsUntilRemap > 0) {
				DWORD resultWait = WaitForSingleObject(network_interface->threadEventUpdate, (uint32_t)millisecondsUntilRemap);
			}
			
			EnterCriticalSection(&xmod_critsec_api);
			
			if ((network_interface->forwardedPorts.find(protocolPort) != network_interface->forwardedPorts.end()) == add_mapping) {
				LeaveCriticalSection(&xmod_critsec_api);
				return true;
			}
			
			LeaveCriticalSection(&xmod_critsec_api);
			
			if (network_interface->threadExit && add_mapping) {
				return false;
			}
			
			if (millisecondsUntilRemap <= 0) {
				break;
			}
		}
	}
	
	return false;
}

static bool ChangeMappingPCP(XMOD_NETWORK_INTERFACE* network_interface, int32_t protocol, uint16_t port, bool add_mapping)
{
	if (xmod_network_socket_gateway == INVALID_SOCKET) {
		return false;
	}
	if (network_interface->threadExit && add_mapping) {
		return false;
	}
	
	// TODO PCP
	
	return false;
}

static bool ParseNetworkData(uint8_t* data_buffer, size_t data_size, SOCKADDR_STORAGE* sockaddr_external)
{
	if (data_size == 0) {
		return false;
	}
	
	uint8_t &responseOpCode = (uint8_t&)data_buffer[1];
	if (
		data_size >= 4
		&& data_buffer[0] == 0
		&& (
			responseOpCode == 128
			|| responseOpCode == 129
			|| responseOpCode == 130
		)
	) {
		uint16_t responseCode = ntohs(*((uint16_t*)&data_buffer[2]));
		if (responseCode != 0) {
			char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "[UPnP] %s NAT-PMP Gateway (%s) responded to %s with error (%hu)."
				, __func__
				, sockAddrInfo
				, (responseOpCode == 128 ? "Public Address Request" : (responseOpCode == 129 ? "UDP Mapping Request" : "TCP Mapping Request"))
				, responseCode
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
				sockAddrInfo = 0;
			}
		}
		else if (responseOpCode == 128 && data_size == 12) {
			char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "[UPnP] %s NAT-PMP Gateway (%s) responded to %s."
				, __func__
				, sockAddrInfo
				, "Public Address Request"
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
				sockAddrInfo = 0;
			}
		}
		else if ((responseOpCode == 129 || responseOpCode == 130) && data_size == 16) {
			uint16_t responsePortInternal = ntohs(*((uint16_t*)&data_buffer[8]));
			uint16_t responsePortExternal = ntohs(*((uint16_t*)&data_buffer[10]));
			uint32_t responseMappingLifetime = ntohl(*((uint32_t*)&data_buffer[12]));
			if (responseMappingLifetime && responsePortInternal != responsePortExternal) {
				char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[UPnP] %s NAT-PMP Gateway (%s) responded to %s but with different internal (%hu) and external (%hu) ports."
					, __func__
					, sockAddrInfo
					, (responseOpCode == 129 ? "UDP Mapping Request" : "TCP Mapping Request")
					, responsePortInternal
					, responsePortExternal
				);
				if (sockAddrInfo) {
					free(sockAddrInfo);
					sockAddrInfo = 0;
				}
			}
			else {
				int32_t protocol = (responseOpCode == 129 ? IPPROTO_UDP : IPPROTO_TCP);
				
				EnterCriticalSection(&xmod_critsec_api);
				
				for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
					if (!SockAddrsMatch(sockaddr_external, &networkInterface->addressGateway, true)) {
						continue;
					}
					
					char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
						, "[UPnP] Successfully %s protocol (%s) port (%hu) on NAT-PMP Gateway (%s) on interface name (%s) description (%ls)."
						, (responseMappingLifetime ? "added" : "removed")
						, GetSocketProtocolAsText(protocol)
						, responsePortInternal
						, sockAddrInfo
						, (!networkInterface->name ? "" : networkInterface->name)
						, (!networkInterface->description ? L"" : networkInterface->description)
					);
					if (sockAddrInfo) {
						free(sockAddrInfo);
						sockAddrInfo = 0;
					}
					
					ProtocolPort protocolPort = std::make_pair(protocol, responsePortInternal);
					
					if (responseMappingLifetime) {
						auto itrForwardedPort = networkInterface->forwardedPorts.find(protocolPort);
						if (itrForwardedPort != networkInterface->forwardedPorts.end() && itrForwardedPort->second != FORWARDED_ON_SERVICE_TYPE::NAT_PMP) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
								, "[UPnP] %s interface name (%s) description (%ls) already had forwarded protocol (%s) port (%hu) on internal service type (%hhu) now overwriting with (%hhu)."
								, __func__
								, (!networkInterface->name ? "" : networkInterface->name)
								, (!networkInterface->description ? L"" : networkInterface->description)
								, GetSocketProtocolAsText(protocol)
								, responsePortInternal
								, itrForwardedPort->second
								, FORWARDED_ON_SERVICE_TYPE::NAT_PMP
							);
						}
						
						networkInterface->forwardedPorts[protocolPort] = FORWARDED_ON_SERVICE_TYPE::NAT_PMP;
					}
					else {
						auto itrForwardedPort = networkInterface->forwardedPorts.find(protocolPort);
						if (itrForwardedPort != networkInterface->forwardedPorts.end()) {
							if (itrForwardedPort->second == FORWARDED_ON_SERVICE_TYPE::NAT_PMP) {
								networkInterface->forwardedPorts.erase(itrForwardedPort);
							}
							else {
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
									, "[UPnP] %s Gateway on interface name (%s) description (%ls) responded to delete protocol (%s) port (%hu) on wrong internal service type (%hhu) instead of (%hhu). Ignoring."
									, __func__
									, (!networkInterface->name ? "" : networkInterface->name)
									, (!networkInterface->description ? L"" : networkInterface->description)
									, GetSocketProtocolAsText(protocol)
									, responsePortInternal
									, FORWARDED_ON_SERVICE_TYPE::NAT_PMP
									, itrForwardedPort->second
								);
							}
						}
					}
					
					SetEvent(networkInterface->threadEventUpdate);
					
					break;
				}
				
				LeaveCriticalSection(&xmod_critsec_api);
				
				return true;
			}
		}
		else {
			char* sockAddrInfo = GET_SOCKADDR_INFO(sockaddr_external);
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "[UPnP] %s NAT-PMP Gateway (%s) responded to %s with a malformed packet / size."
				, __func__
				, sockAddrInfo
				, (responseOpCode == 128 ? "Public Address Request" : (responseOpCode == 129 ? "UDP Mapping Request" : "TCP Mapping Request"))
			);
			if (sockAddrInfo) {
				free(sockAddrInfo);
				sockAddrInfo = 0;
			}
		}
	}
	
	return false;
}

typedef struct _XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR {
	XMOD_NETWORK_INTERFACE* networkInterface = 0;
} XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR;

static DWORD WINAPI XModThreadGatewayNegotiator(void* lpParam)
{
	XMOD_NETWORK_INTERFACE* network_interface;
	{
		XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR* thread_params = (XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR*)lpParam;
		
		network_interface = thread_params->networkInterface;
		
		delete thread_params;
		thread_params = 0;
		lpParam = 0;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s starting for interface name (%s) description (%ls)."
		, __func__
		, (!network_interface->name ? "" : network_interface->name)
		, (!network_interface->description ? L"" : network_interface->description)
	);
	
	__timeb64 timeToRemapAt;
	_ftime64_s(&timeToRemapAt);
	TimeAddMillis(timeToRemapAt, milliseconds_between_remaps);
	
	bool recheck = false;
	__timeb64 currentTime;
	while (1) {
		if (!recheck) {
			_ftime64_s(&currentTime);
			int64_t millisecondsUntilRemap = TimeDiffMilliseconds(currentTime, timeToRemapAt);
			
			DWORD resultWait = WaitForSingleObject(network_interface->threadEventUpdate, millisecondsUntilRemap < 0 ? 0 : (uint32_t)millisecondsUntilRemap);
			if (resultWait != WAIT_OBJECT_0 && resultWait != WAIT_TIMEOUT) {
				if (resultWait == WAIT_FAILED) {
					resultWait = GetLastError();
				}
				XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[UPnP] %s failed to wait on network_interface->threadEventUpdate with error"
					, __func__
				);
				break;
			}
		}
		recheck = false;
		
		EnterCriticalSection(&xmod_critsec_api);
		
		bool shouldForwardOnGateway = (xmod_preferred_interface_address_gateway.ss_family == AF_UNSPEC || SockAddrsMatch(&network_interface->addressGateway, &xmod_preferred_interface_address_gateway, true));
		
		std::map<ProtocolPort, FORWARDED_ON_SERVICE_TYPE> portsToRemove;
		for (auto itrSet1 = network_interface->forwardedPorts.begin(); itrSet1 != network_interface->forwardedPorts.end(); itrSet1++) {
			if (shouldForwardOnGateway && xmod_ports_in_use.find(itrSet1->first) != xmod_ports_in_use.end()) {
				continue;
			}
			
			portsToRemove[itrSet1->first] = itrSet1->second;
		}
		
		_ftime64_s(&currentTime);
		if (!network_interface->threadExit && IsTimeLessThan(timeToRemapAt, currentTime)) {
			timeToRemapAt = currentTime;
			TimeAddMillis(timeToRemapAt, milliseconds_between_remaps);
			
			network_interface->forwardedPorts.clear();
		}
		
		std::set<ProtocolPort> portsToForward;
		if (shouldForwardOnGateway) {
			for (auto itrSet1 = xmod_ports_in_use.begin(); itrSet1 != xmod_ports_in_use.end(); itrSet1++) {
				if (network_interface->forwardedPorts.find(*itrSet1) != network_interface->forwardedPorts.end()) {
					continue;
				}
				if (network_interface->failedPorts.find(*itrSet1) != network_interface->failedPorts.end()) {
					continue;
				}
				
				portsToForward.insert(*itrSet1);
			}
		}
		
		if (portsToRemove.size() || portsToForward.size()) {
			recheck = true;
		}
		else if (network_interface->threadExit) {
			LeaveCriticalSection(&xmod_critsec_api);
			break;
		}
		
		LeaveCriticalSection(&xmod_critsec_api);
		
		for (const auto itrProtocolPort : portsToRemove) {
			switch (itrProtocolPort.second) {
				case FORWARDED_ON_SERVICE_TYPE::UPnP: {
					ChangeMappingUpnp(network_interface, itrProtocolPort.first.first, itrProtocolPort.first.second, false);
					break;
				}
				case FORWARDED_ON_SERVICE_TYPE::NAT_PMP: {
					ChangeMappingNATPMP(network_interface, itrProtocolPort.first.first, itrProtocolPort.first.second, false);
					break;
				}
				case FORWARDED_ON_SERVICE_TYPE::PCP: {
					ChangeMappingPCP(network_interface, itrProtocolPort.first.first, itrProtocolPort.first.second, false);
					break;
				}
				default: {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
						"[UPnP] %s for protocol (%s) port (%hu) failed because unknown internally recorded service type (%hhu)."
						, __func__
						, GetSocketProtocolAsText(itrProtocolPort.first.first)
						, itrProtocolPort.first.second
						, itrProtocolPort.second
					);
					break;
				}
			}
		}
		
		if (portsToRemove.size()) {
			EnterCriticalSection(&xmod_critsec_api);
			
			for (const auto itrProtocolPort : portsToRemove) {
				auto itrForwardedPort = network_interface->forwardedPorts.find(itrProtocolPort.first);
				if (itrForwardedPort == network_interface->forwardedPorts.end()) {
					continue;
				}
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
					"[UPnP] %s failed to unforward protocol (%s) port (%hu) on service type (%hhu) on interface name (%s) description (%ls)."
					, __func__
					, GetSocketProtocolAsText(itrProtocolPort.first.first)
					, itrProtocolPort.first.second
					, itrProtocolPort.second
					, (!network_interface->name ? "" : network_interface->name)
					, (!network_interface->description ? L"" : network_interface->description)
				);
				
				network_interface->forwardedPorts.erase(itrForwardedPort);
			}
			portsToRemove.clear();
			
			LeaveCriticalSection(&xmod_critsec_api);
		}
		
		if (network_interface->threadExit) {
			continue;
		}
		
		for (auto itrProtocolPort = portsToForward.begin(); itrProtocolPort != portsToForward.end();) {
			if (ChangeMappingNATPMP(network_interface, itrProtocolPort->first, itrProtocolPort->second, true)) {}
			else if (ChangeMappingPCP(network_interface, itrProtocolPort->first, itrProtocolPort->second, true)) {}
			else if (ChangeMappingUpnp(network_interface, itrProtocolPort->first, itrProtocolPort->second, true)) {}
			else {
				itrProtocolPort++;
				continue;
			}
			
			portsToForward.erase(itrProtocolPort++);
		}
		
		if (portsToForward.size()) {
			EnterCriticalSection(&xmod_critsec_api);
			
			for (ProtocolPort protocolPort : portsToForward) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN,
					"[UPnP] Failed to forward protocol (%s) port (%hu) via any service on interface name (%s) description (%ls)."
					, GetSocketProtocolAsText(protocolPort.first)
					, protocolPort.second
					, (!network_interface->name ? "" : network_interface->name)
					, (!network_interface->description ? L"" : network_interface->description)
				);
				network_interface->failedPorts.insert(protocolPort);
			}
			portsToForward.clear();
			
			LeaveCriticalSection(&xmod_critsec_api);
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s exiting for interface name (%s) description (%ls)."
		, __func__
		, (!network_interface->name ? "" : network_interface->name)
		, (!network_interface->description ? L"" : network_interface->description)
	);
	
	SetEvent(network_interface->threadEventExited);
	network_interface = 0;
	
	return ERROR_SUCCESS;
}

static bool XModDeleteGatewayNegotiators(std::set<XMOD_NETWORK_INTERFACE*>* network_interfaces)
{
	size_t threadExitedEventsCount = network_interfaces->size();
	if (!threadExitedEventsCount) {
		return true;
	}
	
	bool result = true;
	
	HANDLE* threadExitedEvents = new HANDLE[threadExitedEventsCount];
	size_t iThreadExitedEvents = 0;
	
	for (XMOD_NETWORK_INTERFACE* networkInterface : *network_interfaces) {
		
		networkInterface->threadExit = true;
		SetEvent(networkInterface->threadEventUpdate);
		
		threadExitedEvents[iThreadExitedEvents++] = networkInterface->threadEventExited;
	}
	
	if (iThreadExitedEvents != threadExitedEventsCount) {
		__debugbreak();
	}
	
	DWORD resultWait = WaitForMultipleObjects(iThreadExitedEvents, threadExitedEvents, TRUE, INFINITE);
	if (!(resultWait >= WAIT_OBJECT_0 && resultWait < WAIT_OBJECT_0 + iThreadExitedEvents)) {
		if (resultWait == WAIT_FAILED) {
			resultWait = GetLastError();
		}
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s WaitForMultipleObjects failed."
			, __func__
		);
		result = false;
	}
	
	delete[] threadExitedEvents;
	threadExitedEvents = 0;
	
	for (XMOD_NETWORK_INTERFACE* networkInterface : *network_interfaces) {
		delete networkInterface;
	}
	network_interfaces->clear();
	
	return result;
}

static bool XModRefreshNetworkInterfaces()
{
	DWORD dwRetVal = 0;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	
	// Allocate a 15 KB buffer to start with.
	ULONG outBufLen = 15000;
	ULONG Iterations = 0;
	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "[UPnP] %s memory allocation failed for IP_ADAPTER_ADDRESSES struct."
				, __func__
			);
			dwRetVal = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}
		
		dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAddresses, &outBufLen);
		
		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}
		
		Iterations++;
		// 3 attempts max.
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3));
	
	if (dwRetVal != NO_ERROR && dwRetVal != ERROR_NO_DATA) {
		XLLN_DEBUG_LOG_ECODE(dwRetVal, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s GetAdaptersAddresses(AF_INET, ...) failed with error:"
			, __func__
		);
		
		if (pAddresses) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
		}
		
		return false;
	}
	
	{
		EnterCriticalSection(&xmod_critsec_api);
		
		std::set<XMOD_NETWORK_INTERFACE*> networkInterfacesNew;
		std::set<XMOD_NETWORK_INTERFACE*> networkInterfacesExisting;
		
		if (dwRetVal == NO_ERROR) {
			for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
				if (pCurrAddresses->OperStatus == 1) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
						, "[UPnP] %s Checking elegibility of adapter: %s - %ls."
						, __func__
						, pCurrAddresses->AdapterName
						, pCurrAddresses->Description
					);
					
					SOCKADDR_STORAGE addressGateway = {AF_UNSPEC};
					SOCKADDR_STORAGE addressUnicast = {AF_UNSPEC};
					SOCKADDR_STORAGE addressBroadcast = {AF_UNSPEC};
					SOCKADDR_STORAGE addressMulticast = {AF_UNSPEC};
					
					//TODO IPv6 support.
					
					for (IP_ADAPTER_UNICAST_ADDRESS* interfaceUnicastAddress = pCurrAddresses->FirstUnicastAddress; interfaceUnicastAddress; interfaceUnicastAddress = interfaceUnicastAddress->Next) {
						if (interfaceUnicastAddress->Address.lpSockaddr->sa_family == AF_INET) {
							uint32_t dwMask = 0;
							dwRetVal = IPHLPAPI_ConvertLengthToIpv4Mask(interfaceUnicastAddress->OnLinkPrefixLength, &dwMask);
							if (dwRetVal == NO_ERROR) {
								memset(&addressUnicast, 0, sizeof(addressUnicast));
								addressUnicast.ss_family = AF_INET;
								((sockaddr_in*)&addressUnicast)->sin_addr.s_addr = ((sockaddr_in*)interfaceUnicastAddress->Address.lpSockaddr)->sin_addr.s_addr;
								memset(&addressBroadcast, 0, sizeof(addressBroadcast));
								addressBroadcast.ss_family = AF_INET;
								((sockaddr_in*)&addressBroadcast)->sin_addr.s_addr = ~(dwMask) | ((sockaddr_in*)&addressUnicast)->sin_addr.s_addr;
								break;
							}
						}
					}
					
					for (IP_ADAPTER_GATEWAY_ADDRESS_LH* interfaceGatewayAddress = pCurrAddresses->FirstGatewayAddress; interfaceGatewayAddress; interfaceGatewayAddress = interfaceGatewayAddress->Next) {
						if (interfaceGatewayAddress->Address.lpSockaddr->sa_family == AF_INET) {
							memset(&addressGateway, 0, sizeof(addressGateway));
							addressGateway.ss_family = AF_INET;
							((sockaddr_in*)&addressGateway)->sin_addr.s_addr = ((sockaddr_in*)interfaceGatewayAddress->Address.lpSockaddr)->sin_addr.s_addr;
							break;
						}
					}
					
					for (IP_ADAPTER_MULTICAST_ADDRESS_XP* interfaceMulticastAddress = pCurrAddresses->FirstMulticastAddress; interfaceMulticastAddress; interfaceMulticastAddress = interfaceMulticastAddress->Next) {
						if (interfaceMulticastAddress->Address.lpSockaddr->sa_family == AF_INET) {
							memset(&addressMulticast, 0, sizeof(addressMulticast));
							addressMulticast.ss_family = AF_INET;
							((sockaddr_in*)&addressMulticast)->sin_addr.s_addr = ((sockaddr_in*)interfaceMulticastAddress->Address.lpSockaddr)->sin_addr.s_addr;
							break;
						}
					}
					
					if (
						addressUnicast.ss_family == AF_UNSPEC
						|| addressBroadcast.ss_family == AF_UNSPEC
						|| addressGateway.ss_family == AF_UNSPEC
					) {
						continue;
					}
					
					bool matched = false;
					for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
						if (
							!(
								(
									(!networkInterface->name && !pCurrAddresses->AdapterName)
									|| (networkInterface->name && pCurrAddresses->AdapterName && strcmp(networkInterface->name, pCurrAddresses->AdapterName) == 0)
								)
								&& (
									(!networkInterface->description && !pCurrAddresses->Description)
									|| (networkInterface->description && pCurrAddresses->Description && wcscmp(networkInterface->description, pCurrAddresses->Description) == 0)
								)
								&& SockAddrsMatch(&networkInterface->addressGateway, &addressGateway, true)
								&& SockAddrsMatch(&networkInterface->addressUnicast, &addressUnicast, true)
								&& SockAddrsMatch(&networkInterface->addressBroadcast, &addressBroadcast, true)
								&& SockAddrsMatch(&networkInterface->addressMulticast, &addressMulticast, true)
							)
						) {
							continue;
						}
						matched = true;
						networkInterfacesExisting.insert(networkInterface);
						break;
					}
					
					if (!matched) {
						XMOD_NETWORK_INTERFACE* networkInterface = new XMOD_NETWORK_INTERFACE;
						networkInterface->name = CloneString(pCurrAddresses->AdapterName);
						networkInterface->description = CloneString(pCurrAddresses->Description);
						networkInterface->addressGateway = addressGateway;
						networkInterface->addressUnicast = addressUnicast;
						networkInterface->addressBroadcast = addressBroadcast;
						networkInterface->addressMulticast = addressMulticast;
						networkInterfacesNew.insert(networkInterface);
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_DEBUG
							, "[UPnP] %s Adding eligible adapter: %s - %ls."
							, __func__
							, networkInterface->name
							, networkInterface->description
						);
					}
				}
			}
		}
		
		std::set<XMOD_NETWORK_INTERFACE*> networkInterfacesDelete;
		{
			for (auto itrNetworkInterface = xmod_network_interfaces.begin(); itrNetworkInterface != xmod_network_interfaces.end();) {
				XMOD_NETWORK_INTERFACE* networkInterface = *itrNetworkInterface;
				auto itrNetworkInterfaceExisting = networkInterfacesExisting.find(networkInterface);
				if (itrNetworkInterfaceExisting != networkInterfacesExisting.end()) {
					itrNetworkInterface++;
					continue;
				}
				
				xmod_network_interfaces.erase(itrNetworkInterface++);
				
				networkInterfacesDelete.insert(networkInterface);
			}
			networkInterfacesExisting.clear();
		}
		
		bool resultGatewayNegotiators = XModDeleteGatewayNegotiators(&networkInterfacesDelete);
		
		bool resultSearchUpnpIgd = XModSearchUpnpIgd();
		
		for (XMOD_NETWORK_INTERFACE* networkInterface : networkInterfacesNew) {
			XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR* threadParams = new XMOD_THREAD_PARAMS_GATEWAY_NEGOTIATOR;
			threadParams->networkInterface = networkInterface;
			
			HANDLE resultCreateThread = CreateThread(0, 0, XModThreadGatewayNegotiator, (void*)threadParams, 0, 0);
			if (!resultCreateThread) {
				uint32_t errorCreateThread = GetLastError();
				XLLN_DEBUG_LOG_ECODE(errorCreateThread, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
					"[UPnP] %s Failed to create XModThreadGatewayNegotiator for interface name (%s) description (%ls)."
					, __func__
					, (!networkInterface->name ? "" : networkInterface->name)
					, (!networkInterface->description ? L"" : networkInterface->description)
				);
				delete threadParams;
				threadParams = 0;
				
				delete networkInterface;
			}
			
			xmod_network_interfaces.insert(networkInterface);
		}
		networkInterfacesNew.clear();
		
		LeaveCriticalSection(&xmod_critsec_api);
	}
	
	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, pAddresses);
	}
	
	return true;
}

static void WINAPI XllnCallbackNetworkInterfaceChanged(const char* interface_name, const SOCKADDR_STORAGE* gateway, const SOCKADDR_STORAGE* unicast, const SOCKADDR_STORAGE* broadcast)
{
	if (!(!gateway && !unicast && !broadcast) && !(gateway && unicast && broadcast)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s either all set or none: gateway (0x%zx), unicast (0x%zx), broadcast (0x%zx)."
			, __func__
			, gateway
			, unicast
			, broadcast
		);
		return;
	}
	if (interface_name && !gateway) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s interface_name (%s) is set when the rest is not: gateway (0x%zx), unicast (0x%zx), broadcast (0x%zx)."
			, __func__
			, interface_name
			, gateway
			, unicast
			, broadcast
		);
		return;
	}
	
	{
		EnterCriticalSection(&xmod_critsec_api);
		
		bool resultRefreshNetworkInterfaces = XModRefreshNetworkInterfaces();
		
		if (gateway) {
			if (gateway->ss_family != AF_INET && gateway->ss_family != AF_INET6) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_WARN
					, "[UPnP] %s unsupported gateway->ss_family (%hu)."
					, __func__
					, gateway->ss_family
				);
				// Do not forward on any interface.
				xmod_preferred_interface_address_gateway.ss_family = XMOD_ADDRESS_FAMILY_INVALID;
			}
			else {
				xmod_preferred_interface_address_gateway = *gateway;
			}
		}
		else {
			// Try to forward on all available interfaces.
			xmod_preferred_interface_address_gateway.ss_family = AF_UNSPEC;
		}
		
		bool resultSearchUpnpIgd = XModSearchUpnpIgd();
		
		for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
			networkInterface->failedPorts.clear();
			SetEvent(networkInterface->threadEventUpdate);
		}
		
		LeaveCriticalSection(&xmod_critsec_api);
	}
}

static void WINAPI XllnCallbackNetworkSocketBind(bool has_binded, int32_t port_protocol, uint16_t port_binded, uint16_t port_requested, int16_t port_base_offset)
{
	if (port_protocol != IPPROTO_UDP && port_protocol != IPPROTO_TCP) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s unsupported port_protocol (%d)."
			, __func__
			, port_protocol
		);
		return;
	}
	if (port_binded == 0 || port_requested == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "[UPnP] %s will not handle port_protocol (%d), port_binded (%hu), port_requested (%hu)."
			, __func__
			, port_protocol
			, port_binded
			, port_requested
		);
		return;
	}
	
	{
		EnterCriticalSection(&xmod_critsec_api);
		
		ProtocolPort protocolPort = std::make_pair(port_protocol, port_binded);
		if (has_binded) {
			xmod_ports_in_use.insert(protocolPort);
		}
		else {
			xmod_ports_in_use.erase(protocolPort);
		}
		
		if (!xmod_network_interfaces.size() && xmod_preferred_interface_address_gateway.ss_family == AF_UNSPEC) {
			XllnCallbackNetworkInterfaceChanged(0, 0, 0, 0);
		}
		else {
			for (XMOD_NETWORK_INTERFACE* networkInterface : xmod_network_interfaces) {
				networkInterface->failedPorts.erase(protocolPort);
				SetEvent(networkInterface->threadEventUpdate);
			}
		}
		
		LeaveCriticalSection(&xmod_critsec_api);
	}
}

typedef struct _XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV {
	SOCKET networkSocket = INVALID_SOCKET;
	size_t networkRecvSize = 0;
} XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV;

static DWORD WINAPI XModThreadNetworkSocketRecv(void* lpParam)
{
	XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV* thread_params = (XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV*)lpParam;
	
	const SOCKET network_socket = thread_params->networkSocket;
	const size_t data_buffer_size = thread_params->networkRecvSize;
	
	delete thread_params;
	thread_params = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s starting."
		, __func__
	);
	
	uint8_t* dataBuffer = new uint8_t[data_buffer_size];
	SOCKADDR_STORAGE sockAddrExternal;
	int32_t sockAddrExternalSize = (int32_t)sizeof(sockAddrExternal);
	while (1) {
		int32_t resultRecvFromDataSize = SOCKET_ERROR;
		resultRecvFromDataSize = recvfrom(network_socket, (char*)dataBuffer, data_buffer_size, 0, (sockaddr*)&sockAddrExternal, &sockAddrExternalSize);
		if (resultRecvFromDataSize < 0) {
			int32_t resultErrorRecvFrom = WSAGetLastError();
			if (resultErrorRecvFrom == WSAEWOULDBLOCK) {
				// Normal error. No more data.
				break;
			}
			else if (resultErrorRecvFrom == WSAEMSGSIZE) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
					, "[UPnP] %s recvfrom WSAEMSGSIZE buffer too small or message received too big. Discarding."
					, __func__
				);
				continue;
			}
			
			XLLN_DEBUG_LOG_ECODE(resultErrorRecvFrom, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
				, "[UPnP] %s recvfrom failed."
				, __func__
			);
			
			break;
		}
		
		ParseNetworkData(dataBuffer, resultRecvFromDataSize, &sockAddrExternal);
	}
	
	delete[] dataBuffer;
	dataBuffer = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s exiting."
		, __func__
	);
	
	SetEvent(xmod_network_thread_event_exited);
	
	return ERROR_SUCCESS;
}


static bool XModNetworkSocketCreate(SOCKET* network_socket)
{
	// Request version 2.0.
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	int resultXWSAStartup = WSAStartup(wVersionRequested, &wsaData);
	if (resultXWSAStartup) {
		XLLN_DEBUG_LOG_ECODE(resultXWSAStartup, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s WSAStartup failed. Requested version (%hhu.%hhu)."
			, __func__
			, (uint8_t)(wVersionRequested & 0xFF)
			, (uint8_t)((wVersionRequested >> 8) & 0xFF)
		);
		return false;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s WSAStartup succeeded. Requested version (%hhu.%hhu). Result support for version (%hhu.%hhu)."
		, __func__
		, (uint8_t)(wVersionRequested & 0xFF)
		, (uint8_t)((wVersionRequested >> 8) & 0xFF)
		, (uint8_t)(wsaData.wVersion & 0xFF)
		, (uint8_t)((wsaData.wVersion >> 8) & 0xFF)
	);
	
	*network_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (*network_socket == INVALID_SOCKET) {
		int32_t errorSocketCreate = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketCreate, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s Failed to create."
			, __func__
		);
		
		int resultWSACleanup = WSACleanup();
		if (resultWSACleanup) {
			int32_t errorWsaCleanup = WSAGetLastError();
			XLLN_DEBUG_LOG_ECODE(errorWsaCleanup, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "[UPnP] %s WSACleanup failed."
				, __func__
			);
		}
		
		return false;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "[UPnP] %s (0x%zx) created."
		, __func__
		, *network_socket
	);
	
	//uint32_t sockOptValue = 1;
	//int32_t resultSetSockOpt = setsockopt(*network_socket, SOL_SOCKET, SO_BROADCAST, (const char*)&sockOptValue, sizeof(sockOptValue));
	//if (resultSetSockOpt == SOCKET_ERROR) {
	//	int32_t errorSetSockOpt = WSAGetLastError();
	//	XLLN_DEBUG_LOG_ECODE(errorSetSockOpt, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
	//		, "[UPnP] %s Failed to set SO_BROADCAST option."
	//		, __func__
	//	);
	//	return false;
	//}
	
	uint32_t socketRecvSize = 0;
	int32_t sockOptValueSize = sizeof(socketRecvSize);
	int32_t resultGetSockOpt = getsockopt(*network_socket, SOL_SOCKET, SO_RCVBUF, (char*)&socketRecvSize, &sockOptValueSize);
	if (resultGetSockOpt == SOCKET_ERROR) {
		int32_t errorGetSockOpt = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorGetSockOpt, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s Failed to getsockopt SO_RCVBUF."
			, __func__
		);
		return false;
	}
	
	uint32_t socketSendSize = 0;
	sockOptValueSize = sizeof(socketSendSize);
	resultGetSockOpt = getsockopt(*network_socket, SOL_SOCKET, SO_SNDBUF, (char*)&socketSendSize, &sockOptValueSize);
	if (resultGetSockOpt == SOCKET_ERROR) {
		int32_t errorGetSockOpt = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorGetSockOpt, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s Failed to getsockopt SO_RCVBUF."
			, __func__
		);
		return false;
	}
	
	// If iMode = 0, blocking is enabled; 
	// If iMode != 0, non-blocking mode is enabled.
	unsigned long iMode = 0;
	int32_t resultIoCtlSocket = ioctlsocket(*network_socket, FIONBIO, &iMode);
	if (resultIoCtlSocket == SOCKET_ERROR) {
		int32_t errorIoCtlSocket = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorIoCtlSocket, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s Failed to set FIONBIO option."
			, __func__
		);
		return false;
	}
	
	sockaddr_in socketBindAddress;
	socketBindAddress.sin_family = AF_INET;
	socketBindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	socketBindAddress.sin_port = htons(0);
	SOCKET resultSocketBind = bind(*network_socket, (sockaddr*)&socketBindAddress, sizeof(socketBindAddress));
	if (resultSocketBind) {
		int32_t errorSocketBind = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketBind, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s Failed to bind."
			, __func__
		);
		return false;
	}
	
	ResetEvent(xmod_network_thread_event_exited);
	
	XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV* threadParams = new XMOD_THREAD_PARAMS_NETWORK_SOCKET_RECV;
	threadParams->networkSocket = *network_socket;
	threadParams->networkRecvSize = socketRecvSize;
	
	HANDLE resultCreateThread = CreateThread(0, 0, XModThreadNetworkSocketRecv, (void*)threadParams, 0, 0);
	if (!resultCreateThread) {
		uint32_t errorCreateThread = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCreateThread, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s Failed to create XModThreadNetworkSocketRecv."
			, __func__
		);
		delete threadParams;
		threadParams = 0;
		
		SetEvent(xmod_network_thread_event_exited);
		
		return false;
	}
	
	return true;
}

static bool XModNetworkSocketClose(SOCKET* network_socket)
{
	if (*network_socket == INVALID_SOCKET) {
		return true;
	}
	
	bool result = true;
	
	int32_t resultSocketShutdown = shutdown(*network_socket, SD_RECEIVE);
	if (resultSocketShutdown) {
		int32_t errorSocketShutdown = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketShutdown, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s Failed to shutdown."
			, __func__
		);
		result = false;
	}
	
	int32_t resultSocketClose = closesocket(*network_socket);
	if (resultSocketClose) {
		int32_t errorSocketClose = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorSocketClose, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s Failed to close."
			, __func__
		);
		result = false;
	}
	
	if (result) {
		DWORD resultWait = WaitForSingleObject(xmod_network_thread_event_exited, INFINITE);
		if (resultWait != WAIT_OBJECT_0) {
			if (resultWait == WAIT_FAILED) {
				resultWait = GetLastError();
			}
			XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
				, "[UPnP] %s failed to wait for xmod_network_thread_event_exited."
				, __func__
			);
			result = false;
		}
	}
	
	if (result) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "[UPnP] %s (0x%zx) closed."
			, __func__
			, *network_socket
		);
	}
	
	int resultWSACleanup = WSACleanup();
	if (resultWSACleanup) {
		int32_t errorWsaCleanup = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorWsaCleanup, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR
			, "[UPnP] %s WSACleanup failed."
			, __func__
		);
		result = false;
	}
	
	*network_socket = INVALID_SOCKET;
	
	return result;
}

static bool XModDeleteHandles()
{
	bool result = true;
	
	if (xmod_network_thread_event_exited != INVALID_HANDLE_VALUE) {
		BOOL resultCloseHandle = CloseHandle(xmod_network_thread_event_exited);
		if (!resultCloseHandle) {
			uint32_t errorCloseHandle = GetLastError();
			XLLN_DEBUG_LOG_ECODE(errorCloseHandle, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
				"[UPnP] %s CloseHandle xmod_network_thread_event_exited failed with error"
				, __func__
			);
			result = false;
		}
		xmod_network_thread_event_exited = INVALID_HANDLE_VALUE;
	}
	
	bool resultGatewayNegotiators = XModDeleteGatewayNegotiators(&xmod_network_interfaces);
	
	DWORD resultWait = WaitForSingleObject(xmod_upnp_igd_search_complete, INFINITE);
	if (resultWait != WAIT_OBJECT_0) {
		if (resultWait == WAIT_FAILED) {
			resultWait = GetLastError();
		}
		XLLN_DEBUG_LOG_ECODE(resultWait, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL
			, "[UPnP] %s failed to wait for xmod_upnp_igd_search_complete."
			, __func__
		);
		result = false;
	}
	
	if (xmod_upnp_igd_search_complete != INVALID_HANDLE_VALUE) {
		BOOL resultCloseHandle = CloseHandle(xmod_upnp_igd_search_complete);
		if (!resultCloseHandle) {
			uint32_t errorCloseHandle = GetLastError();
			XLLN_DEBUG_LOG_ECODE(errorCloseHandle, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
				"[UPnP] %s CloseHandle xmod_upnp_igd_search_complete failed with error"
				, __func__
			);
			result = false;
		}
		xmod_upnp_igd_search_complete = INVALID_HANDLE_VALUE;
	}
	
	DeleteCriticalSection(&xmod_critsec_api);
	
	return result;
}

// #41101
uint32_t WINAPI XLLNModulePostInit()
{
	InitializeCriticalSection(&xmod_critsec_api);
	
	xmod_network_thread_event_exited = CreateEventA(NULL, FALSE, TRUE, NULL);
	if (xmod_network_thread_event_exited == INVALID_HANDLE_VALUE) {
		uint32_t errorCreateEvent = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCreateEvent, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s CreateEventA xmod_network_thread_event_exited failed."
			, __func__
		);
		
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	xmod_upnp_igd_search_complete = CreateEventA(NULL, TRUE, TRUE, NULL);
	if (xmod_upnp_igd_search_complete == INVALID_HANDLE_VALUE) {
		uint32_t errorCreateEvent = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorCreateEvent, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s CreateEventA xmod_upnp_igd_search_complete failed."
			, __func__
		);
		
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	bool resultSocket = XModNetworkSocketCreate(&xmod_network_socket_gateway);
	if (!resultSocket) {
		resultSocket = XModNetworkSocketClose(&xmod_network_socket_gateway);
		
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	bool resultRefreshNetworkInterfaces = XModRefreshNetworkInterfaces();
	if (!resultRefreshNetworkInterfaces) {
		resultSocket = XModNetworkSocketClose(&xmod_network_socket_gateway);
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_INTERFACE_CHANGED, (void**)CHECK_FUNC_PTR_TYPE(XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED, XllnCallbackNetworkInterfaceChanged), 0);
	if (resultXllnModifyProperty) {
		XLLN_DEBUG_LOG_ECODE(resultXllnModifyProperty, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s XLLNModifyProperty CALLBACK_NETWORK_INTERFACE_CHANGED failed."
			, __func__
		);
		
		resultSocket = XModNetworkSocketClose(&xmod_network_socket_gateway);
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_SOCKET_BIND, (void**)CHECK_FUNC_PTR_TYPE(XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND, XllnCallbackNetworkSocketBind), 0);
	if (resultXllnModifyProperty) {
		XLLN_DEBUG_LOG_ECODE(resultXllnModifyProperty, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_FATAL,
			"[UPnP] %s XLLNModifyProperty CALLBACK_NETWORK_SOCKET_BIND failed."
			, __func__
		);
		
		resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_INTERFACE_CHANGED, 0, (void**)CHECK_FUNC_PTR_TYPE(XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED, XllnCallbackNetworkInterfaceChanged));
		if (resultXllnModifyProperty) {
			XLLN_DEBUG_LOG_ECODE(resultXllnModifyProperty, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
				"[UPnP] %s XLLNModifyProperty CALLBACK_NETWORK_INTERFACE_CHANGED failed."
				, __func__
			);
		}
		
		resultSocket = XModNetworkSocketClose(&xmod_network_socket_gateway);
		bool resultXModDeleteHandles = XModDeleteHandles();
		return ERROR_FUNCTION_FAILED;
	}
	
	return ERROR_SUCCESS;
}

// #41102
uint32_t WINAPI XLLNModulePreUninit()
{
	uint32_t result = ERROR_SUCCESS;
	
	uint32_t resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_INTERFACE_CHANGED, 0, (void**)CHECK_FUNC_PTR_TYPE(XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED, XllnCallbackNetworkInterfaceChanged));
	if (resultXllnModifyProperty) {
		XLLN_DEBUG_LOG_ECODE(resultXllnModifyProperty, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
			"[UPnP] %s XLLNModifyProperty CALLBACK_NETWORK_INTERFACE_CHANGED failed."
			, __func__
		);
	}
	
	resultXllnModifyProperty = XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_SOCKET_BIND, 0, (void**)CHECK_FUNC_PTR_TYPE(XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND, XllnCallbackNetworkSocketBind));
	if (resultXllnModifyProperty) {
		XLLN_DEBUG_LOG_ECODE(resultXllnModifyProperty, XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_ERROR,
			"[UPnP] %s XLLNModifyProperty CALLBACK_NETWORK_SOCKET_BIND failed."
			, __func__
		);
	}
	
	EnterCriticalSection(&xmod_critsec_api);
	
	xmod_ports_in_use.clear();
	
	LeaveCriticalSection(&xmod_critsec_api);
	
	bool resultGatewayNegotiators = XModDeleteGatewayNegotiators(&xmod_network_interfaces);
	
	bool resultSocket = XModNetworkSocketClose(&xmod_network_socket_gateway);
	
	bool resultXModDeleteHandles = XModDeleteHandles();
	
	return result;
}

bool InitXllnModule()
{
	return true;
}

bool UninitXllnModule()
{
	return true;
}
