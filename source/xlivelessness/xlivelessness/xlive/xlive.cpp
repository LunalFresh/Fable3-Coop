#include <winsock2.h>
#include "../xlln/social.hpp"
#include "xdefs.hpp"
#include "xfuncs.hpp"
#include "../xlln/debug-log.hpp"
#include "xlive.hpp"
#include "xrender.hpp"
#include "xnet.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-network.hpp"
#include "../xlln/xlln-config.hpp"
#include "../xlln/wnd-main.hpp"
#include "../xlln/wnd-debug-log.hpp"
#include "../xlln/wnd-sockets.hpp"
#include "../xlln/wnd-connections.hpp"
#include "../xlln/wnd-achievements.hpp"
#include "../xlln/wnd-stats.hpp"
#include "../xlln/wnd-message-box.hpp"
#include "../xlln/wnd-input-box.hpp"
#include "../xlln/wnd-user-custom-list.hpp"
#include "../xlln/wnd-user-card.hpp"
#include "../xlln/xlln-modules.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../utils/sha256.hpp"
#include "xsocket.hpp"
#include "xlocator.hpp"
#include "xsession.hpp"
#include "xpresence.hpp"
#include "xmarketplace.hpp"
#include "xcontent.hpp"
#include "live-over-lan.hpp"
#include "xuser.hpp"
#include "xnotify.hpp"
#include "xhv-engine.hpp"
#include <time.h>
#include <string>
#include <vector>
#include <CommCtrl.h>
// Link with iphlpapi.lib
#include <iphlpapi.h>

bool xlive_debug_pause = false;

CRITICAL_SECTION xlive_critsec_local_user;
LOCAL_USER_INFO xlive_local_users[XLIVE_LOCAL_USER_COUNT];

CRITICAL_SECTION xlive_critsec_remote_user;
std::map<XUID, char*> xlln_remote_user_usernames;

uint32_t xlive_title_id = 0;
uint32_t xlive_title_version = 0;

CRITICAL_SECTION xlive_critsec_xfriends_enumerators;
// Key: enumerator handle (id).
// Value: Remaining entries in a friends snapshot.
std::map<HANDLE, std::vector<XONLINE_FRIEND>> xlive_xfriends_enumerators;

CRITICAL_SECTION xlive_critsec_network_adapter;
char* xlive_init_specific_network_adapter_name = 0;
char* xlive_config_specific_network_adapter_name = 0;
bool xlive_ignore_title_network_adapter = true;
ELIGIBLE_NETWORK_INTERFACE* xlive_specific_network_adapter = 0;
std::vector<ELIGIBLE_NETWORK_INTERFACE*> xlive_eligible_network_adapters;

bool xlive_online_initialized = false;
static bool xlive_initialised = false;

CRITICAL_SECTION xlive_critsec_title_server_enumerators;
// Key: enumerator handle (id).
// Value: Remaining entries in a friends snapshot.
static std::map<HANDLE, std::vector<uint32_t>> xlive_title_server_enumerators;

DIRECT_IP_CONNECT xlln_direct_ip_connect;

static XLIVE_DEBUG_LEVEL xlive_debug_level = XLIVE_DEBUG_LEVEL_OFF;

int32_t RefreshNetworkAdapters()
{
	int32_t result = ERROR_UNIDENTIFIED_ERROR;
	DWORD dwRetVal = 0;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	
	// Allocate a 15 KB buffer to start with.
	ULONG outBufLen = 15000;
	ULONG Iterations = 0;
	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s memory allocation failed for IP_ADAPTER_ADDRESSES struct.", __func__);
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
		result = ERROR_UNIDENTIFIED_ERROR;
		XLLN_DEBUG_LOG_ECODE(dwRetVal, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s GetAdaptersAddresses(AF_INET, ...) failed with error:", __func__);
	}
	
	{
		EnterCriticalSection(&xlive_critsec_network_adapter);
		
		xlive_specific_network_adapter = 0;
		for (ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface : xlive_eligible_network_adapters) {
			delete eligibleNetworkInterface;
		}
		xlive_eligible_network_adapters.clear();
		
		if (dwRetVal == NO_ERROR) {
			for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
				if (pCurrAddresses->OperStatus == 1) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Checking elegibility of adapter: %s - %ls."
						, __func__
						, pCurrAddresses->AdapterName
						, pCurrAddresses->Description
					);
					
					ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface = new ELIGIBLE_NETWORK_INTERFACE;
					eligibleNetworkInterface->name = CloneString(pCurrAddresses->AdapterName);
					eligibleNetworkInterface->description = CloneString(pCurrAddresses->Description);
					
					eligibleNetworkInterface->minLinkSpeed = pCurrAddresses->ReceiveLinkSpeed;
					if (pCurrAddresses->TransmitLinkSpeed < pCurrAddresses->ReceiveLinkSpeed) {
						eligibleNetworkInterface->minLinkSpeed = pCurrAddresses->TransmitLinkSpeed;
					}
					eligibleNetworkInterface->hasDnsServer = !!pCurrAddresses->FirstDnsServerAddress;
					
					//TODO IPv6 support.
					
					for (IP_ADAPTER_UNICAST_ADDRESS* interfaceUnicastAddress = pCurrAddresses->FirstUnicastAddress; interfaceUnicastAddress; interfaceUnicastAddress = interfaceUnicastAddress->Next) {
						if (interfaceUnicastAddress->Address.lpSockaddr->sa_family == AF_INET) {
							uint32_t dwMask = 0;
							dwRetVal = IPHLPAPI_ConvertLengthToIpv4Mask(interfaceUnicastAddress->OnLinkPrefixLength, &dwMask);
							if (dwRetVal == NO_ERROR) {
								eligibleNetworkInterface->prefixLength = interfaceUnicastAddress->OnLinkPrefixLength;
								memset(&eligibleNetworkInterface->addressUnicast, 0, sizeof(eligibleNetworkInterface->addressUnicast));
								eligibleNetworkInterface->addressUnicast.ss_family = AF_INET;
								((sockaddr_in*)&eligibleNetworkInterface->addressUnicast)->sin_addr.s_addr = ((sockaddr_in*)interfaceUnicastAddress->Address.lpSockaddr)->sin_addr.s_addr;
								memset(&eligibleNetworkInterface->addressBroadcast, 0, sizeof(eligibleNetworkInterface->addressBroadcast));
								eligibleNetworkInterface->addressBroadcast.ss_family = AF_INET;
								((sockaddr_in*)&eligibleNetworkInterface->addressBroadcast)->sin_addr.s_addr = ~(dwMask) | ((sockaddr_in*)&eligibleNetworkInterface->addressUnicast)->sin_addr.s_addr;
								break;
							}
						}
					}
					
					for (IP_ADAPTER_GATEWAY_ADDRESS_LH* interfaceGatewayAddress = pCurrAddresses->FirstGatewayAddress; interfaceGatewayAddress; interfaceGatewayAddress = interfaceGatewayAddress->Next) {
						if (interfaceGatewayAddress->Address.lpSockaddr->sa_family == AF_INET) {
							memset(&eligibleNetworkInterface->addressGateway, 0, sizeof(eligibleNetworkInterface->addressGateway));
							eligibleNetworkInterface->addressGateway.ss_family = AF_INET;
							((sockaddr_in*)&eligibleNetworkInterface->addressGateway)->sin_addr.s_addr = ((sockaddr_in*)interfaceGatewayAddress->Address.lpSockaddr)->sin_addr.s_addr;
							break;
						}
					}
					
					for (IP_ADAPTER_MULTICAST_ADDRESS_XP* interfaceMulticastAddress = pCurrAddresses->FirstMulticastAddress; interfaceMulticastAddress; interfaceMulticastAddress = interfaceMulticastAddress->Next) {
						if (interfaceMulticastAddress->Address.lpSockaddr->sa_family == AF_INET) {
							memset(&eligibleNetworkInterface->addressMulticast, 0, sizeof(eligibleNetworkInterface->addressMulticast));
							eligibleNetworkInterface->addressMulticast.ss_family = AF_INET;
							((sockaddr_in*)&eligibleNetworkInterface->addressMulticast)->sin_addr.s_addr = ((sockaddr_in*)interfaceMulticastAddress->Address.lpSockaddr)->sin_addr.s_addr;
							break;
						}
					}
					
					if (
						!eligibleNetworkInterface->addressUnicast.ss_family
						|| !eligibleNetworkInterface->addressBroadcast.ss_family
					) {
						delete eligibleNetworkInterface;
						eligibleNetworkInterface = 0;
						continue;
					}
					
					xlive_eligible_network_adapters.push_back(eligibleNetworkInterface);
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
						, "%s Adding eligible adapter: %s - %ls."
						, __func__
						, eligibleNetworkInterface->name
						, eligibleNetworkInterface->description
					);
				}
			}
			
			for (ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface : xlive_eligible_network_adapters) {
				if (xlive_config_specific_network_adapter_name && strncmp(eligibleNetworkInterface->name, xlive_config_specific_network_adapter_name, MAX_ADAPTER_NAME_LENGTH + 1) == 0) {
					xlive_specific_network_adapter = eligibleNetworkInterface;
					break;
				}
				if (!xlive_ignore_title_network_adapter && xlive_init_specific_network_adapter_name && strncmp(eligibleNetworkInterface->name, xlive_init_specific_network_adapter_name, MAX_ADAPTER_NAME_LENGTH + 1) == 0) {
					xlive_specific_network_adapter = eligibleNetworkInterface;
					break;
				}
			}
			
			result = ERROR_SUCCESS;
		}
		
		LeaveCriticalSection(&xlive_critsec_network_adapter);
	}
	
	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, pAddresses);
	}
	
	XllnWndMainUpdateNetworkAdapters();
	XllnNetworkInterfaceChanged();
	
	return result;
}

void Check_Overlapped(XOVERLAPPED* xoverlapped)
{
	if (!xoverlapped) {
		return;
	}
	
	if (xoverlapped->hEvent) {
		SetEvent(xoverlapped->hEvent);
	}
	
	if (xoverlapped->pCompletionRoutine) {
		xoverlapped->pCompletionRoutine(xoverlapped->InternalLow, xoverlapped->InternalHigh, xoverlapped);
	}
}

// #1082
size_t WINAPI XGetOverlappedExtendedError(XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xoverlapped) {
		return GetLastError();
	}
	if (xoverlapped->InternalLow != ERROR_IO_PENDING) {
		return xoverlapped->dwExtendedError;
	}
	return ERROR_IO_INCOMPLETE;
}

// #1083
size_t WINAPI XGetOverlappedResult(XOVERLAPPED* xoverlapped, size_t* result_code, BOOL wait_for_completion)
{
	TRACE_FX();
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (xoverlapped->InternalLow == ERROR_IO_PENDING) {
		DWORD waitResult;
		if (wait_for_completion && xoverlapped->hEvent != 0) {
			waitResult = WaitForSingleObject(xoverlapped->hEvent, 0xFFFFFFFF);
		}
		else {
			waitResult = WAIT_TIMEOUT;
		}
		if (waitResult == WAIT_TIMEOUT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Handle 0x%zx is incomplete.", __func__, xoverlapped->hEvent);
			return ERROR_IO_INCOMPLETE;
		}
		if (waitResult) {
			return GetLastError();
		}
	}
	
	if (result_code) {
		*result_code = xoverlapped->InternalHigh;
	}
	
	return xoverlapped->InternalLow;
}

// #5000
HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO* xlive_initialise_info)
{
	TRACE_FX();
	return XLiveInitializeEx(xlive_initialise_info, 0);
}

// #5001
HRESULT WINAPI XLiveInput(XLIVE_INPUT_INFO* xlive_input_info)
{
	TRACE_FX();
	if (!xlive_input_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlive_input_info is NULL.", __func__);
		return E_POINTER;
	}
	xlive_input_info->fHandled = FALSE;
	return S_OK;
}

// #5003
void WINAPI XLiveUninitialize()
{
    SocialSetReady(false);
	TRACE_FX();
	
	XllnModulesPreXLiveUninitialize();
	
	xlive_initialised = false;
	
	bool errorXhvEngine = UninitXhvEngine();
	bool errorXNotify = UninitXNotify();
	int32_t errorXSession = UninitXSession();
	bool errorXUser = UninitXUser();
	int32_t errorXRender = UninitXRender();
	bool errorXNet = UninitXNet();
	bool errorXSocket = UninitXSocket();
	
	if (xlive_net_initialized) {
		int32_t errorXNetCleanup = XNetCleanup();
	}
	if (xlive_online_initialized) {
		uint32_t errorXOnlineCleanup = XOnlineCleanup();
	}
	
	uint32_t errorXllnWndUserCard = UninitXllnWndUserCard();
	uint32_t errorXllnWndUserCustomList = UninitXllnWndUserCustomList();
	uint32_t errorXllnWndInputBox = UninitXllnWndInputBox();
	uint32_t errorXllnWndMessageBox = UninitXllnWndMessageBox();
	uint32_t errorXllnWndAchievements = UninitXllnWndAchievements();
	uint32_t errorXllnWndStats = UninitXllnWndStats();
	uint32_t errorXllnWndConnections = UninitXllnWndConnections();
	uint32_t errorXllnWndSockets = UninitXllnWndSockets();
	uint32_t errorXllnWndDebugLog = UninitXllnWndDebugLog();
	uint32_t errorXllnWndMain = UninitXllnWndMain();
	
	bool errorXllnNetwork = UninitXllnNetwork();
	
	if (xlln_network_instance_port_mutex && xlln_network_instance_port_mutex != INVALID_HANDLE_VALUE) {
		CloseHandle(xlln_network_instance_port_mutex);
		xlln_network_instance_port_mutex = INVALID_HANDLE_VALUE;
	}
	
	EnterCriticalSection(&xlive_critsec_remote_user);
	
	for (auto &itrRemoteUser : xlln_remote_user_usernames) {
		if (itrRemoteUser.second) {
			delete[] itrRemoteUser.second;
		}
	}
	xlln_remote_user_usernames.clear();
	
	LeaveCriticalSection(&xlive_critsec_remote_user);
}

// #5010: This function is deprecated.
HRESULT WINAPI XLiveRegisterDataSection(const wchar_t* section_name, uint8_t* section_data, uint32_t section_size)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(
		XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
		, "%s TODO. \"%ls\" size 0x%08x."
		, __func__
		, section_name
		, section_size
	);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5011: This function is deprecated.
HRESULT WINAPI XLiveUnregisterDataSection(const wchar_t* section_name)
{
	TRACE_FX();
	
	XLLN_DEBUG_LOG(
		XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
		, "%s TODO. \"%ls\"."
		, __func__
		, section_name
	);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5012: This function is deprecated.
HRESULT WINAPI XLiveUpdateHashes(uint32_t a1, uint32_t a2)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);// 0x80070032;
}

// #5022
HRESULT WINAPI XLiveGetUpdateInformation(XLIVEUPDATE_INFORMATION* xlive_update_info)
{
	TRACE_FX();
	if (!xlive_update_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xlive_update_info is NULL.", __func__);
		return E_POINTER;
	}
	if (xlive_update_info->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (xlive_update_info->cbSize != sizeof(XLIVEUPDATE_INFORMATION)) Invalid buffer.", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	
	xlive_update_info->bSystemUpdate = FALSE;
	xlive_update_info->dwFromVersion = xlive_title_version;
	xlive_update_info->dwToVersion = 0;
	xlive_update_info->szUpdateDownloadPath[0] = 0;
	
	return S_OK;
}

// #5024
HRESULT WINAPI XLiveUpdateSystem(const wchar_t* relaunch_cmd_line)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(
		XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
		, "%s TODO. \"%ls\"."
		, __func__
		, relaunch_cmd_line ? relaunch_cmd_line : L"NULL"
	);
	return S_OK;
}

// #5025
HRESULT WINAPI XLiveGetLiveIdError(HRESULT* authentication_state, HRESULT* request_state, wchar_t* complete_auth_web_url, size_t* complete_auth_web_url_size)
{
	TRACE_FX();
	if (!authentication_state) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s authentication_state is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!request_state) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s request_state is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!complete_auth_web_url) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s complete_auth_web_url is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!complete_auth_web_url_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s complete_auth_web_url_size is NULL.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	*authentication_state = 0;
	*request_state = 0;
	if (*complete_auth_web_url_size > 0) {
		*complete_auth_web_url = 0;
		*complete_auth_web_url_size = 1;
	}
	return S_OK;
	return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	return E_UNEXPECTED;
}

// #5026
HRESULT WINAPI XLiveSetSponsorToken(const wchar_t* sponsor_token, uint32_t title_id)
{
	TRACE_FX();
	if (!sponsor_token) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s sponsor_token is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (wcsnlen_s(sponsor_token, 30) != 29) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (wcsnlen_s(sponsor_token, 30) != 29) Invalid string length.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5027
HRESULT WINAPI XLiveUninstallTitle(uint32_t title_id)
{
	TRACE_FX();
	if (!title_id) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s title_id is 0.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
}

// #5028
uint32_t WINAPI XLiveLoadLibraryEx(const wchar_t* module_pathname, HINSTANCE* module_handle, uint32_t flags)
{
	TRACE_FX();
	if (!module_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s module_pathname is NULL.", __func__);
		return E_POINTER;
	}
	if (!*module_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *module_pathname is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!module_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s module_handle is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
		, "XLiveLoadLibraryEx \"%ls\"."
		, module_pathname
	);
	
	HINSTANCE resultInstance = LoadLibraryExW(module_pathname, NULL, flags);
	if (!resultInstance) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s resultInstance is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveLoadLibraryEx 0x%zx \"%ls\"."
		, resultInstance
		, module_pathname
	);
	
	*module_handle = resultInstance;
	
	return S_OK;
}

// #5029
HRESULT WINAPI XLiveFreeLibrary(HMODULE module_handle)
{
	TRACE_FX();
	if (!module_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s module_handle is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "XLiveFreeLibrary 0x%08x."
		, module_handle
	);
	
	if (!FreeLibrary(module_handle)) {
		HRESULT errorFreeLibrary = GetLastError();
		if (errorFreeLibrary > 0) {
			errorFreeLibrary = HRESULT_FROM_WIN32(errorFreeLibrary);
		}
		return errorFreeLibrary;
	}
	return S_OK;
}

// #5030
BOOL WINAPI XLivePreTranslateMessage(const MSG* procedure_message)
{
	TRACE_FX();
	if (!procedure_message) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s procedure_message is NULL.", __func__);
		return FALSE;
	}
	
	bool isSystemUIOpen = XllnIsXliveSystemUIOpen();
	XllnUpdateXliveSystemUIOpenState(isSystemUIOpen);
	if (isSystemUIOpen) {
		if (!((procedure_message->message == WM_KEYDOWN || procedure_message->message == WM_SYSKEYDOWN) && procedure_message->wParam == VK_PROCESSKEY)) {
			return FALSE;
		}
		// The message has been consumed.
		return TRUE;
	}
	return FALSE;
}

// #5031
HRESULT WINAPI XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL debug_level_new, XLIVE_DEBUG_LEVEL* debug_level_old)
{
	TRACE_FX();
	if (debug_level_new != XLIVE_DEBUG_LEVEL_OFF
		&& debug_level_new != XLIVE_DEBUG_LEVEL_ERROR
		&& debug_level_new != XLIVE_DEBUG_LEVEL_WARNING
		&& debug_level_new != XLIVE_DEBUG_LEVEL_INFO
		&& debug_level_new != XLIVE_DEBUG_LEVEL_DEFAULT
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s debug_level_new value (0x%08x) does not exist.", __func__, debug_level_new);
		return E_INVALIDARG;
	}
	if (debug_level_old) {
		*debug_level_old = xlive_debug_level;
	}
	xlive_debug_level = debug_level_new;
	return S_OK;
}

// #5032
HRESULT WINAPI XLiveVerifyArcadeLicense(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	//XLIVE_LICENSE_INFO_V1 licenseInfo;
	//HRESULT result = XLivePBufferSetByteArray(protected_buffer, protected_buffer_offset, &licenseInfo, XLIVE_LICENSE_ID_SIZE);
	//return result;
	
	return S_OK;
}

// #5251
BOOL WINAPI XCloseHandle(HANDLE object_handle)
{
	TRACE_FX();
	if (!object_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s object_handle is NULL.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	if (object_handle == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s object_handle is INVALID_HANDLE_VALUE.", __func__);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	
	bool foundEnumerator = false;
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
		if (xlive_xlocator_enumerators.count(object_handle)) {
			foundEnumerator = true;
			xlive_xlocator_enumerators.erase(object_handle);
		}
		LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xsession);
		auto const &itrXSessions = xlive_xsession_local_sessions.find(object_handle);
		if (itrXSessions != xlive_xsession_local_sessions.end()) {
			foundEnumerator = true;
			LIVE_SESSION_XSESSION* xsessionDetails = itrXSessions->second;
			xlive_xsession_local_sessions.erase(object_handle);
			if (xsessionDetails->liveSession) {
				if (xsessionDetails->liveSession->pContexts) {
					delete[] xsessionDetails->liveSession->pContexts;
					xsessionDetails->liveSession->pContexts = 0;
				}
				if (xsessionDetails->liveSession->pProperties) {
					for (uint32_t iProp = 0; iProp < xsessionDetails->liveSession->propertiesCount; iProp++) {
						XUSER_DATA &xuserData = xsessionDetails->liveSession->pProperties[iProp].value;
						if (xuserData.type == XUSER_DATA_TYPE_UNICODE && xuserData.string.pwszData) {
							delete[] xuserData.string.pwszData;
							xuserData.string.pwszData = 0;
						}
						if (xuserData.type == XUSER_DATA_TYPE_BINARY && xuserData.binary.pbData) {
							delete[] xuserData.binary.pbData;
							xuserData.binary.pbData = 0;
						}
					}
					delete[] xsessionDetails->liveSession->pProperties;
					xsessionDetails->liveSession->pProperties = 0;
				}
				delete xsessionDetails->liveSession;
				xsessionDetails->liveSession = 0;
			}
			delete xsessionDetails;
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
		auto const &itrAchievementEnumerationDetails = xlive_xuser_achievement_enumerators.find(object_handle);
		if (itrAchievementEnumerationDetails != xlive_xuser_achievement_enumerators.end()) {
			foundEnumerator = true;
			ACHIEVEMENT_ENUMERATION_DETAILS* achievementEnumerationDetails = itrAchievementEnumerationDetails->second;
			xlive_xuser_achievement_enumerators.erase(object_handle);
			delete achievementEnumerationDetails;
		}
		LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_stats);
		auto const &itrStatsEnumerationDetails = xlive_xuser_stats_enumerators.find(object_handle);
		if (itrStatsEnumerationDetails != xlive_xuser_stats_enumerators.end()) {
			foundEnumerator = true;
			STATS_ENUMERATION_DETAILS* statsEnumerationDetails = itrStatsEnumerationDetails->second;
			xlive_xuser_stats_enumerators.erase(object_handle);
			if (statsEnumerationDetails->statsSpecs) {
				delete[] statsEnumerationDetails->statsSpecs;
				statsEnumerationDetails->statsSpecs = 0;
			}
			delete statsEnumerationDetails;
		}
		LeaveCriticalSection(&xlive_critsec_xuser_stats);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
		if (xlive_xfriends_enumerators.count(object_handle)) {
			foundEnumerator = true;
			xlive_xfriends_enumerators.erase(object_handle);
		}
		LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_title_server_enumerators);
		if (xlive_title_server_enumerators.count(object_handle)) {
			foundEnumerator = true;
			xlive_title_server_enumerators.erase(object_handle);
		}
		LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_presence_enumerators);
		if (xlive_presence_enumerators.count(object_handle)) {
			foundEnumerator = true;
			xlive_presence_enumerators.erase(object_handle);
		}
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xmarketplace);
		if (xlive_xmarketplace_enumerators.count(object_handle)) {
			foundEnumerator = true;
			xlive_xmarketplace_enumerators.erase(object_handle);
		}
		LeaveCriticalSection(&xlive_critsec_xmarketplace);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xcontent);
		auto const &itrXContentEnumerator = xlive_xcontent_enumerators.find(object_handle);
		if (itrXContentEnumerator != xlive_xcontent_enumerators.end()) {
			foundEnumerator = true;
			XCONTENT_ENUMERATION_DETAILS* xcontentEnumerationDetails = itrXContentEnumerator->second;
			xlive_xcontent_enumerators.erase(itrXContentEnumerator);
			delete xcontentEnumerationDetails;
		}
		if (!foundEnumerator) {
			auto const &itrXContentAccessHandle = xlive_xcontent_access_handles.find(object_handle);
			if (itrXContentAccessHandle != xlive_xcontent_access_handles.end()) {
				foundEnumerator = true;
				xlive_xcontent_access_handles.erase(itrXContentAccessHandle);
			}
		}
		LeaveCriticalSection(&xlive_critsec_xcontent);
	}
	
	if (!foundEnumerator) {
		if (XLiveNotifyDeleteListener(object_handle)) {
			foundEnumerator = true;
		}
	}
	
	if (!foundEnumerator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown handle 0x%zx.", __func__, (size_t)object_handle);
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	
	if (!CloseHandle(object_handle)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Failed to close handle 0x%zx.", __func__, (size_t)object_handle);
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	return TRUE;
}

// #5254
uint32_t WINAPI XCancelOverlapped(XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped->hEvent (0x%zx) TODO.", __func__, xoverlapped->hEvent);
	SetEvent(xoverlapped->hEvent);
	SetLastError(ERROR_SUCCESS);
	return ERROR_SUCCESS;
}

// #5255
uint32_t WINAPI XEnumerateBack(HANDLE enumerator_handle, void* data_buffer, size_t data_buffer_size, size_t* result_item_count, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	uint32_t resultCode = XEnumerate(enumerator_handle, data_buffer, data_buffer_size, result_item_count, xoverlapped);
	return resultCode;
}

// #5256
uint32_t WINAPI XEnumerate(HANDLE enumerator_handle, void* data_buffer, size_t data_buffer_size, size_t* result_item_count, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!data_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!data_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_item_count && xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (result_item_count && xoverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_item_count && !xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!result_item_count && !xoverlapped).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	bool foundEnumerator = false;
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xlocator_enumerators);
		auto const &itrXlocatorEnumerationDetails = xlive_xlocator_enumerators.find(enumerator_handle);
		if (itrXlocatorEnumerationDetails != xlive_xlocator_enumerators.end()) {
			foundEnumerator = true;
			auto &xlocatorEnumerationDetails = itrXlocatorEnumerationDetails->second;
			
			bool triedToReturnSomething = false;
			size_t totalSessionCount = 0;
			XLOCATOR_SEARCHRESULT* searchResults = (XLOCATOR_SEARCHRESULT*)data_buffer;
			uint8_t* searchResultsData = ((uint8_t*)data_buffer) + data_buffer_size;
			
			EnterCriticalSection(&xlln_critsec_liveoverlan_sessions);
			for (auto const &session : liveoverlan_remote_sessions_xlocator) {
				// Ensure this Live Session has not already been returned in this enumerator.
				if (std::find(xlocatorEnumerationDetails.begin(), xlocatorEnumerationDetails.end(), session.first) != xlocatorEnumerationDetails.end()) {
					continue;
				}
				
				// Ensure this is an XLocator item.
				if (session.second->liveSession->sessionType != XLLN_LIVEOVERLAN_SESSION_TYPE_XLOCATOR) {
					continue;
				}
				
				if ((uint8_t*)&searchResults[totalSessionCount + 1] > searchResultsData) {
					// Not enough room left in the buffer to store this object's minimum size.
					break;
				}
				
				triedToReturnSomething = true;
				
				{
					EnterCriticalSection(&xlive_critsec_remote_user);
					
					// Make an entry if one didn't already exist for this user.
					xlln_remote_user_usernames[session.second->liveSession->xuid];
					PostMessageW(xlln_hwnd_user_card, XLLNControlsMessageNumbers::EVENT_USER_CARD_USERS_UPDATE, 0, 0);
					
					LeaveCriticalSection(&xlive_critsec_remote_user);
				}
				
				uint8_t* searchResultsDataPrev = searchResultsData;
				
				// Calculate the space required for the extra data.
				searchResultsData -= session.second->liveSession->propertiesCount * sizeof(*session.second->liveSession->pProperties);
				for (uint32_t iProperty = 0; iProperty < session.second->liveSession->propertiesCount; iProperty++) {
					XUSER_PROPERTY &property = session.second->liveSession->pProperties[iProperty];
					switch (property.value.type) {
						case XUSER_DATA_TYPE_BINARY: {
							searchResultsData -= property.value.binary.cbData;
							break;
						}
						case XUSER_DATA_TYPE_UNICODE: {
							searchResultsData -= property.value.string.cbData;
							break;
						}
					}
				}
				
				// Copy over all the memory into the buffer.
				XLOCATOR_SEARCHRESULT &searchResult = searchResults[totalSessionCount++];
				searchResult.serverID = session.second->liveSession->xuid;
				searchResult.dwServerType = session.second->liveSession->sessionFlags;
				searchResult.serverAddress = session.second->xnAddr;
				searchResult.xnkid = session.second->liveSession->xnkid;
				searchResult.xnkey = session.second->liveSession->xnkey;
				searchResult.dwMaxPublicSlots = session.second->liveSession->slotsPublicMaxCount;
				searchResult.dwMaxPrivateSlots = session.second->liveSession->slotsPrivateMaxCount;
				searchResult.dwFilledPublicSlots = session.second->liveSession->slotsPublicFilledCount;
				searchResult.dwFilledPrivateSlots = session.second->liveSession->slotsPrivateFilledCount;
				searchResult.cProperties = 0;
				searchResult.pProperties = 0;
				
				if ((uint8_t*)&searchResults[totalSessionCount] > searchResultsData) {
					// Not enough room left in the buffer to store this object and all the extra data.
					searchResultsData = searchResultsDataPrev;
					
					// Ignore this item completely if there is already at least one being returned (if only returning 1 item, keep it just without any properties).
					if (totalSessionCount > 1) {
						totalSessionCount--;
					}
					else {
						// Record that this Live Session has been returned in the enumerator.
						xlocatorEnumerationDetails.push_back(session.first);
					}
					
					break;
				}
				
				// Record that this Live Session has been returned in the enumerator.
				xlocatorEnumerationDetails.push_back(session.first);
				
				searchResult.cProperties = session.second->liveSession->propertiesCount;
				
				uint8_t* searchResultData = searchResultsData + (session.second->liveSession->propertiesCount * sizeof(*session.second->liveSession->pProperties));
				
				if (searchResult.cProperties) {
					searchResult.pProperties = (XUSER_PROPERTY*)searchResultsData;
					memcpy(searchResult.pProperties, session.second->liveSession->pProperties, session.second->liveSession->propertiesCount * sizeof(*session.second->liveSession->pProperties));
					for (uint32_t iProperty = 0; iProperty < session.second->liveSession->propertiesCount; iProperty++) {
						XUSER_PROPERTY &propertyOrig = session.second->liveSession->pProperties[iProperty];
						XUSER_PROPERTY &propertyCopy = searchResult.pProperties[iProperty];
						switch (propertyCopy.value.type) {
							case XUSER_DATA_TYPE_BINARY: {
								propertyCopy.value.binary.pbData = searchResultData;
								memcpy(propertyCopy.value.binary.pbData, propertyOrig.value.binary.pbData, propertyCopy.value.binary.cbData);
								searchResultData += propertyCopy.value.binary.cbData;
								break;
							}
							case XUSER_DATA_TYPE_UNICODE: {
								propertyCopy.value.string.pwszData = (wchar_t*)searchResultData;
								memcpy(propertyCopy.value.string.pwszData, propertyOrig.value.string.pwszData, propertyCopy.value.string.cbData);
								searchResultData += propertyCopy.value.string.cbData;
								break;
							}
						}
					}
				}
				
				if (searchResultData != searchResultsDataPrev) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
						, "%s the end result of searchResultData (0x%zx) should not be different from searchResultsDataPrev (0x%zx)."
						, __func__
						, searchResultData
						, searchResultsDataPrev
					);
					__debugbreak();
					break;
				}
			}
			LeaveCriticalSection(&xlln_critsec_liveoverlan_sessions);
			
			LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
			
			if (xoverlapped) {
				if (totalSessionCount) {
					xoverlapped->InternalHigh = totalSessionCount;
					xoverlapped->InternalLow = ERROR_SUCCESS;
					xoverlapped->dwExtendedError = ERROR_SUCCESS;
				}
				else if (triedToReturnSomething) {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_INSUFFICIENT_BUFFER;
					xoverlapped->dwExtendedError = ERROR_INSUFFICIENT_BUFFER;
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XLocator enumerator (0x%zx) was passed a buffer of insufficient size (0x%zx) to hold at least one partial (without properies) result (0x%zx)."
						, __func__
						, enumerator_handle
						, data_buffer_size
						, sizeof(XLOCATOR_SEARCHRESULT)
					);
				}
				else {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_NO_MORE_FILES;
					xoverlapped->dwExtendedError = ERROR_NO_MORE_FILES;
				}
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = totalSessionCount;
				if (totalSessionCount == 0 && triedToReturnSomething) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s XLocator enumerator (0x%zx) was passed a buffer of insufficient size (0x%zx) to hold at least one partial (without properies) result (0x%zx)."
						, __func__
						, enumerator_handle
						, data_buffer_size
						, sizeof(XLOCATOR_SEARCHRESULT)
					);
					return ERROR_INSUFFICIENT_BUFFER;
				}
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xlocator_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
		auto const &itrAchievementEnumerationDetails = xlive_xuser_achievement_enumerators.find(enumerator_handle);
		if (itrAchievementEnumerationDetails != xlive_xuser_achievement_enumerators.end()) {
			foundEnumerator = true;
			ACHIEVEMENT_ENUMERATION_DETAILS* achievementEnumerationDetails = itrAchievementEnumerationDetails->second;
			
			bool triedToReturnSomething = false;
			size_t numberOfResults = 0;
			XACHIEVEMENT_DETAILS* resultAchievements = (XACHIEVEMENT_DETAILS*)data_buffer;
			uint8_t* resultAdditionalData = ((uint8_t*)data_buffer) + data_buffer_size;
			
			size_t achivementDetailsMaxSize = sizeof(XACHIEVEMENT_DETAILS);
			if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_LABEL) {
				achivementDetailsMaxSize += XACHIEVEMENT_MAX_LABEL_LENGTH;
			}
			if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_DESCRIPTION) {
				achivementDetailsMaxSize += XACHIEVEMENT_MAX_DESC_LENGTH;
			}
			if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_UNACHIEVED) {
				achivementDetailsMaxSize += XACHIEVEMENT_MAX_UNACH_LENGTH;
			}
			
			while (true) {
				if (achievementEnumerationDetails->index >= achievementEnumerationDetails->achievementDetails.size()) {
					break;
				}
				
				triedToReturnSomething = true;
				
				if ((uint8_t*)&resultAchievements[numberOfResults + 1] > resultAdditionalData) {
					// Not enough room left in the buffer to store this object's minimum size.
					break;
				}
				
				LPWSTR pwszLabel = 0;
				size_t pwszLabelSize = 0;
				LPWSTR pwszDescription = 0;
				size_t pwszDescriptionSize = 0;
				LPWSTR pwszUnachieved = 0;
				size_t pwszUnachievedSize = 0;
				XACHIEVEMENT_DETAILS & achievementDetails = resultAchievements[numberOfResults];
				XACHIEVEMENT_DETAILS & achievementEnumerationDetailsAchievementDetails = achievementEnumerationDetails->achievementDetails[achievementEnumerationDetails->index];
				achievementDetails = achievementEnumerationDetailsAchievementDetails;

				pwszLabel = achievementEnumerationDetailsAchievementDetails.pwszLabel;
				pwszLabelSize = (wcslen(pwszLabel) + 1) * sizeof(*pwszLabel);
				pwszDescription = achievementEnumerationDetailsAchievementDetails.pwszDescription;
				pwszDescriptionSize = (wcslen(pwszDescription) + 1) * sizeof(*pwszDescription);
				pwszUnachieved = achievementEnumerationDetailsAchievementDetails.pwszUnachieved;
				pwszUnachievedSize = (wcslen(pwszUnachieved) + 1) * sizeof(*pwszUnachieved);
				
				uint8_t* resultAdditionalDataPrev = resultAdditionalData;
				
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_LABEL) {
					resultAdditionalData -= pwszLabelSize;
				}
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_DESCRIPTION) {
					resultAdditionalData -= pwszDescriptionSize;
				}
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_UNACHIEVED) {
					resultAdditionalData -= pwszUnachievedSize;
				}
				
				if ((uint8_t*)&resultAchievements[numberOfResults + 1] >= resultAdditionalData) {
					// Not enough room left in the buffer to store all the extra data.
					resultAdditionalData = resultAdditionalDataPrev;
					
					break;
				}
				
				numberOfResults++;
				achievementEnumerationDetails->index++;
				
				uint8_t* achievementDetailsAdditionalData = resultAdditionalData;
				
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_LABEL) {
					//TODO: Verify that XACHIEVEMENT_MAX_LABEL_LENGTH
					memcpy(achievementDetailsAdditionalData, pwszLabel, pwszLabelSize);
					achievementDetails.pwszLabel = (wchar_t*)achievementDetailsAdditionalData;
					achievementDetailsAdditionalData += pwszLabelSize;
				}
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_DESCRIPTION) {
					//TODO: Verify that XACHIEVEMENT_MAX_DESC_LENGTH
					memcpy(achievementDetailsAdditionalData, pwszDescription, pwszDescriptionSize);
					achievementDetails.pwszDescription = (wchar_t*)achievementDetailsAdditionalData;
					achievementDetailsAdditionalData += pwszDescriptionSize;
				}
				if (achievementEnumerationDetails->detailFlags & XACHIEVEMENT_DETAILS_UNACHIEVED) {
					//TODO: Verify that XACHIEVEMENT_MAX_UNACH_LENGTH
					memcpy(achievementDetailsAdditionalData, pwszUnachieved, pwszUnachievedSize);
					achievementDetails.pwszUnachieved = (wchar_t*)achievementDetailsAdditionalData;
					achievementDetailsAdditionalData += pwszUnachievedSize;
				}
				
				if (achievementDetailsAdditionalData != resultAdditionalDataPrev) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
						, "%s the end result of achievementDetailsAdditionalData (0x%zx) should not be different from resultAdditionalDataPrev (0x%zx)."
						, __func__
						, achievementDetailsAdditionalData
						, resultAdditionalDataPrev
					);
					__debugbreak();
					break;
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
			
			if (xoverlapped) {
				if (numberOfResults) {
					xoverlapped->InternalHigh = numberOfResults;
					xoverlapped->InternalLow = ERROR_SUCCESS;
					xoverlapped->dwExtendedError = ERROR_SUCCESS;
				}
				else if (numberOfResults == 0 && triedToReturnSomething) {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_INSUFFICIENT_BUFFER;
					xoverlapped->dwExtendedError = ERROR_INSUFFICIENT_BUFFER;
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Achievement enumerator (0x%zx) was passed a buffer of insufficient size (0x%zx) to hold at least one result (up to 0x%zx)."
						, __func__
						, enumerator_handle
						, data_buffer_size
						, achivementDetailsMaxSize
					);
				}
				else {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_NO_MORE_FILES;
					xoverlapped->dwExtendedError = ERROR_NO_MORE_FILES;
				}
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = numberOfResults;
				if (numberOfResults == 0 && triedToReturnSomething) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Achievement enumerator (0x%zx) was passed a buffer of insufficient size (0x%zx) to hold at least one result (up to 0x%zx)."
						, __func__
						, enumerator_handle
						, data_buffer_size
						, achivementDetailsMaxSize
					);
					return ERROR_INSUFFICIENT_BUFFER;
				}
				if (numberOfResults == 0) {
					// Exhaustion must be non-success, or titles that loop on the return code never stop.
					return ERROR_NO_MORE_FILES;
				}
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xuser_stats);
		auto const &itrStatsEnumerationDetails = xlive_xuser_stats_enumerators.find(enumerator_handle);
		if (itrStatsEnumerationDetails != xlive_xuser_stats_enumerators.end()) {
			foundEnumerator = true;
			STATS_ENUMERATION_DETAILS* statsEnumerationDetails = itrStatsEnumerationDetails->second;
			
			uint32_t resultCode = ERROR_FUNCTION_FAILED;
			
			//TODO: Perhaps change this function so it returns the maximum number of rows that could fit in the buffer size we have here? And if result is 0 then ERROR_INSUFFICIENT_BUFFER.
			size_t statsResultsSize = XUserStatsReadResultsMinSize(statsEnumerationDetails, true);
			if (data_buffer_size < statsResultsSize) {
				resultCode = ERROR_INSUFFICIENT_BUFFER;
			}
			else {
				resultCode = ERROR_SUCCESS;
				resultCode = ERROR_NO_MORE_FILES;
				XUSER_STATS_READ_RESULTS* resultStats = (XUSER_STATS_READ_RESULTS*)data_buffer;
				uint8_t* resultAdditionalData = ((uint8_t*)data_buffer) + sizeof(XUSER_STATS_READ_RESULTS);
				resultStats->dwNumViews = statsEnumerationDetails->statsSpecCount;
				resultStats->pViews = (XUSER_STATS_VIEW*)resultAdditionalData;
				resultAdditionalData += sizeof(XUSER_STATS_VIEW) * statsEnumerationDetails->statsSpecCount;
				for (uint32_t iSpec = 0; iSpec < statsEnumerationDetails->statsSpecCount; iSpec++) {
					XUSER_STATS_SPEC &statsSpec = statsEnumerationDetails->statsSpecs[iSpec];
					XUSER_STATS_VIEW &statsView = resultStats->pViews[iSpec];
					statsView.dwViewId = statsSpec.dwViewId;
					//TODO: Stats data.
					statsView.dwTotalViewRows = 0;
					statsView.dwNumRows = 0;
					statsView.pRows = 0;
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_xuser_stats);
			
			if (xoverlapped) {
				xoverlapped->InternalHigh = 0;
				xoverlapped->InternalLow = resultCode;
				xoverlapped->dwExtendedError = resultCode;
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = 0;
				return resultCode;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xuser_stats);
	}
	
	if (!foundEnumerator) {
        EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
        auto it = xlive_xfriends_enumerators.find(enumerator_handle);
        if (it != xlive_xfriends_enumerators.end()) {
            size_t count = 0;
            DWORD status = SocialReadSnapshot(it->second, data_buffer, data_buffer_size, &count);
            LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);
            if (xoverlapped) {
                xoverlapped->InternalHigh = count;
                xoverlapped->InternalLow = status;
                xoverlapped->dwExtendedError = status;
                Check_Overlapped(xoverlapped);
                return ERROR_IO_PENDING;
            }
            *result_item_count = count;
            return status;
        }
        LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);
    }

	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_title_server_enumerators);
		if (xlive_title_server_enumerators.count(enumerator_handle)) {
			foundEnumerator = true;
			
			LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
			
			if (xoverlapped) {
				if (false) {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_SUCCESS;
					xoverlapped->dwExtendedError = ERROR_SUCCESS;
				}
				else {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_NO_MORE_FILES;
					xoverlapped->dwExtendedError = ERROR_NO_MORE_FILES;
				}
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_presence_enumerators);
		if (xlive_presence_enumerators.count(enumerator_handle)) {
			foundEnumerator = true;
			auto &xuidSet = xlive_presence_enumerators[enumerator_handle];
			size_t maxResults = data_buffer_size / sizeof(XONLINE_PRESENCE);
			size_t numberOfResults = 0;
			
			while (xuidSet.size() && numberOfResults < maxResults) {
				XUID xuid = *xuidSet.begin();
				xuidSet.erase(xuid);
				
				XONLINE_PRESENCE* userPresence = &((XONLINE_PRESENCE*)data_buffer)[numberOfResults++];
				userPresence->xuid = xuid;
                memset(userPresence, 0, sizeof(*userPresence));
                userPresence->xuid = xuid;
                for (const auto &friendInfo : SocialFriends()) if (friendInfo.xuid == xuid) {
                    userPresence->dwState = friendInfo.dwFriendState;
                    userPresence->dwTitleID = friendInfo.dwTitleID;
                    userPresence->sessionID = friendInfo.sessionID;
                    userPresence->ftUserTime = friendInfo.ftUserTime;
                    break;
                }
			}
			
			LeaveCriticalSection(&xlive_critsec_presence_enumerators);
			
			if (xoverlapped) {
				if (numberOfResults) {
					xoverlapped->InternalHigh = numberOfResults;
					xoverlapped->InternalLow = ERROR_SUCCESS;
					xoverlapped->dwExtendedError = ERROR_SUCCESS;
				}
				else {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_NO_MORE_FILES;
					xoverlapped->dwExtendedError = ERROR_NO_MORE_FILES;
				}
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = numberOfResults;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_presence_enumerators);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xmarketplace);
		if (xlive_xmarketplace_enumerators.count(enumerator_handle)) {
			foundEnumerator = true;
			
			LeaveCriticalSection(&xlive_critsec_xmarketplace);
			
			if (xoverlapped) {
				if (false) {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_SUCCESS;
					xoverlapped->dwExtendedError = ERROR_SUCCESS;
				}
				else {
					xoverlapped->InternalHigh = 0;
					xoverlapped->InternalLow = ERROR_NO_MORE_FILES;
					xoverlapped->dwExtendedError = ERROR_NO_MORE_FILES;
				}
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = 0;
				return ERROR_SUCCESS;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xmarketplace);
	}
	
	if (!foundEnumerator) {
		EnterCriticalSection(&xlive_critsec_xcontent);
		auto const &itrXContentEnumerator = xlive_xcontent_enumerators.find(enumerator_handle);
		if (itrXContentEnumerator != xlive_xcontent_enumerators.end()) {
			foundEnumerator = true;
			
			XCONTENT_ENUMERATION_DETAILS* xcontentEnumerationDetails = itrXContentEnumerator->second;
			
			uint32_t resultCode = ERROR_FUNCTION_FAILED;
			size_t numberOfResults = 0;
			size_t maxResults = data_buffer_size / sizeof(XLIVE_CONTENT_INFO);
			
			if (maxResults <= 0) {
				resultCode = ERROR_INSUFFICIENT_BUFFER;
			}
			else {
				for (auto itrContentDetails = xcontentEnumerationDetails->contentDetails.begin(); itrContentDetails != xcontentEnumerationDetails->contentDetails.end() && numberOfResults < maxResults;) {
					CONTENT_DETAILS* contentDetails = itrContentDetails->second;
					xcontentEnumerationDetails->contentDetails.erase(itrContentDetails++);
					
					XLIVE_CONTENT_INFO* contentInfo = &((XLIVE_CONTENT_INFO*)data_buffer)[numberOfResults++];
					
					contentInfo->dwContentAPIVersion = XLIVE_CONTENT_API_VERSION;
					contentInfo->dwTitleID = contentDetails->titleId;
					contentInfo->dwContentType = contentDetails->contentPackageType;
					memcpy(contentInfo->abContentID, contentDetails->contentId, sizeof(contentDetails->contentId));
				}
				
				resultCode = numberOfResults ? ERROR_SUCCESS : ERROR_NO_MORE_FILES;
			}
			
			LeaveCriticalSection(&xlive_critsec_xcontent);
			
			if (xoverlapped) {
				xoverlapped->InternalHigh = numberOfResults;
				xoverlapped->InternalLow = resultCode;
				xoverlapped->dwExtendedError = resultCode;
				
				Check_Overlapped(xoverlapped);
				
				return ERROR_IO_PENDING;
			}
			else {
				*result_item_count = numberOfResults;
				return resultCode;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xcontent);
	}
	
	if (!foundEnumerator) {
		if (result_item_count) {
			*result_item_count = 0;
		}
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s unknown enumerator handle (0x%zx).", __func__, enumerator_handle);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalHigh = 0;
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return ERROR_SUCCESS;
}

// #5257
HRESULT WINAPI XLiveManageCredentials(const wchar_t* live_id_name, const wchar_t* live_id_password, uint32_t flags, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (flags & XLMGRCREDS_FLAG_SAVE && flags & XLMGRCREDS_FLAG_DELETE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (flags & XLMGRCREDS_FLAG_SAVE && flags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!(flags & XLMGRCREDS_FLAG_SAVE) && !(flags & XLMGRCREDS_FLAG_DELETE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!(flags & XLMGRCREDS_FLAG_SAVE) && !(flags & XLMGRCREDS_FLAG_DELETE).", __func__);
		return E_INVALIDARG;
	}
	if (!live_id_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s live_id_name is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*live_id_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *live_id_name is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (flags & XLMGRCREDS_FLAG_SAVE) {
		if (!live_id_password) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (flags & XLMGRCREDS_FLAG_SAVE) and live_id_password is NULL.", __func__);
			return E_INVALIDARG;
		}
		if (!*live_id_password) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (flags & XLMGRCREDS_FLAG_SAVE) and *live_id_password is NULL.", __func__);
			return E_INVALIDARG;
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalHigh = 0;
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return S_OK;
}

// #5258
HRESULT WINAPI XLiveSignout(XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	
	XLLNLogout(0);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalHigh = 0;
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return S_OK;
}

// #5259
HRESULT WINAPI XLiveSignin(wchar_t* live_id_name, wchar_t* live_id_password, uint32_t flags, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!live_id_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s live_id_name is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!*live_id_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *live_id_name is NULL.", __func__);
		return E_INVALIDARG;
	}
	//TODO password may not get passed in.
	//if (!live_id_password || !*live_id_password)
	//	return E_INVALIDARG;
	
	if (flags & XLSIGNIN_FLAG_SAVECREDS) {
		
	}
	//XLSIGNIN_FLAG_ALLOWTITLEUPDATES XLSIGNIN_FLAG_ALLOWSYSTEMUPDATES
	
	size_t usernameLenSize = wcslen(live_id_name) + 1;

	char* username = new char[usernameLenSize];
	wcstombs2(username, live_id_name, usernameLenSize);
	ReplaceFilePathSensitiveChars(username);
	
	memset(live_id_name, 0, usernameLenSize * sizeof(wchar_t));
	
	if (live_id_password) {
		size_t passwordLenSize = wcslen(live_id_name) + 1;
		memset(live_id_password, 0, passwordLenSize * sizeof(wchar_t));
	}
	
	uint32_t result = XLLNLogin(0, TRUE, 0, username);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLLNLogin(..., \"%s\") failed with error:", __func__, username);
		result = E_INVALIDARG;
	}
	
	delete[] username;
	username = 0;
	
	if (result) {
		return result;
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	else {
		//synchronous
		//return result;
	}
	return S_OK;
}

// #5297
HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO* xlive_initialise_info, uint32_t title_xlive_version)
{
	TRACE_FX();
	
	// if (IsDebuggerPresent())
	// return E_DEBUGGER_PRESENT;
	
	while (xlive_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}
	
	srand((unsigned int)time(NULL));
	
	if (xlive_initialise_info->wLivePortOverride > 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO xlive_initialise_info->wLivePortOverride.", __func__);
	}
	
	if ((xlive_initialise_info->dwFlags & ~(XLIVE_INITFLAG_USE_ADAPTER_NAME | XLIVE_INITFLAG_NO_AUTO_LOGON))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
			, "(RESEARCH) %s xlive_initialise_info->dwFlags contains extra flag(s) (0x%08x)."
			, __func__
			, (xlive_initialise_info->dwFlags & ~(XLIVE_INITFLAG_USE_ADAPTER_NAME | XLIVE_INITFLAG_NO_AUTO_LOGON))
		);
	}
	
	if (xlln_network_instance_port) {
		if (xlln_network_instance_port_mutex && xlln_network_instance_port_mutex != INVALID_HANDLE_VALUE) {
			CloseHandle(xlln_network_instance_port_mutex);
			xlln_network_instance_port_mutex = INVALID_HANDLE_VALUE;
		}
		
		char* mutexName = FormMallocString("Global\\XLiveLessNessInstancePort#%hu", xlln_network_instance_port);
		xlln_network_instance_port_mutex = CreateMutexA(0, TRUE, mutexName);
		free(mutexName);
		mutexName = 0;
		if (!xlln_network_instance_port_mutex || xlln_network_instance_port_mutex == INVALID_HANDLE_VALUE) {
			uint32_t errorMutex = GetLastError();
			XLLN_DEBUG_LOG_ECODE(errorMutex, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_FATAL
				, "%s Failed to obtain Instance Port mutex for port %hu with error:"
				, __func__
				, xlln_network_instance_port
			);
			
			return E_ABORT;
		}
	}
	
	EnterCriticalSection(&xlive_critsec_network_adapter);
	if ((xlive_initialise_info->dwFlags & XLIVE_INITFLAG_USE_ADAPTER_NAME) && xlive_initialise_info->pszAdapterName && xlive_initialise_info->pszAdapterName[0]) {
		if (xlive_init_specific_network_adapter_name) {
			delete[] xlive_init_specific_network_adapter_name;
		}
		xlive_init_specific_network_adapter_name = CloneString(xlive_initialise_info->pszAdapterName);
	}
	LeaveCriticalSection(&xlive_critsec_network_adapter);
	
	int32_t errorNetworkAdapter = RefreshNetworkAdapters();
	
	bool successXllnNetwork = InitXllnNetwork();
	if (!successXllnNetwork) {
		return E_ABORT;
	}
	
	InitCommonControls();
	
	uint32_t errorXllnWndMain = InitXllnWndMain();
	uint32_t errorXllnWndDebugLog = InitXllnWndDebugLog();
	uint32_t errorXllnWndSockets = InitXllnWndSockets();
	uint32_t errorXllnWndConnections = InitXllnWndConnections();
	uint32_t errorXllnWndMessageBox = InitXllnWndMessageBox();
	uint32_t errorXllnWndMessageAchievements = InitXllnWndAchievements();
	uint32_t errorXllnWndStats = InitXllnWndStats();
	uint32_t errorXllnWndInputBox = InitXllnWndInputBox();
	uint32_t errorXllnWndUserCustomList = InitXllnWndUserCustomList();
	uint32_t errorXllnWndUserCard = InitXllnWndUserCard();
	
	if (broadcastAddrInput) {
		char* temp = CloneString(broadcastAddrInput);
		ParseBroadcastAddrInput(temp);
		delete[] temp;
	}
	
	bool errorXSocket = InitXSocket();
	bool errorXNet = InitXNet();
	int32_t errorXRender = InitXRender();
	bool errorXUser = InitXUser();
	int32_t errorXSession = InitXSession();
	bool errorXNotify = InitXNotify();
	bool errorXhvEngine = InitXhvEngine();
	
	HRESULT errorD3D = S_OK;
	if (xlive_initialise_info->pD3D) {
		errorD3D = D3DOnCreateDevice(xlive_initialise_info->pD3D, xlive_initialise_info->pD3DPP);
	}
	if (FAILED(errorD3D)) {
		return errorD3D;
	}
	
	if (!(xlive_initialise_info->dwFlags & XLIVE_INITFLAG_NO_AUTO_LOGON)) {
		XllnShowWindow(XllnShowType::MAIN_SHOW_LOGIN);
	}
	
	xlive_initialised = true;
    SocialSetReady(true);
	return S_OK;
}

// #5303
uint32_t WINAPI XStringVerify(uint32_t flags, const char* locale, size_t string_data_count, const STRING_DATA* string_data, size_t result_response_size, STRING_VERIFY_RESPONSE* result_response, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (flags) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags is not officially implemented and should be NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!locale) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s locale is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (strlen(locale) >= XSTRING_MAX_LENGTH) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (strlen(locale) >= XSTRING_MAX_LENGTH) (%u >= %u).", __func__, (uint32_t)strlen(locale), XSTRING_MAX_LENGTH);
		return ERROR_INVALID_PARAMETER;
	}
	if (string_data_count > XSTRING_MAX_STRINGS) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (string_data_count > XSTRING_MAX_STRINGS) (%zu > %u).", __func__, string_data_count, XSTRING_MAX_STRINGS);
		return ERROR_INVALID_PARAMETER;
	}
	if (!string_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s string_data is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_response) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_response is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	size_t responseSizeRequired = sizeof(STRING_VERIFY_RESPONSE) + (string_data_count * sizeof(HRESULT));
	if (result_response_size < responseSizeRequired) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s result_response_size (%zu) is insufficient (%zu)."
			, __func__
			, result_response_size
			, responseSizeRequired
		);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	
	result_response->wNumStrings = (uint16_t)string_data_count;
	result_response->pStringResult = (HRESULT*)((uint8_t*)result_response + sizeof(STRING_VERIFY_RESPONSE));
	
	for (size_t iString = 0; iString < string_data_count; iString++) {
		result_response->pStringResult[iString] = S_OK;
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	return ERROR_SUCCESS;
}

// #5310
uint32_t WINAPI XOnlineStartup()
{
	TRACE_FX();
	if (!xlive_initialised) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s XLive not initialised.", __func__);
		return ERROR_FUNCTION_FAILED;
	}
	
	WSADATA wsaData;
	int32_t result = XWSAStartup(2, &wsaData);
	if (result != ERROR_SUCCESS) {
		return result;
	}
	
	XllnThreadLiveOverLanStart();
	
	xlive_online_initialized = true;
	return ERROR_SUCCESS;
}

// #5311
uint32_t WINAPI XOnlineCleanup()
{
	TRACE_FX();
	if (!xlive_online_initialized) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s WSANOTINITIALISED.", __func__);
		return WSANOTINITIALISED;
	}
	
	int32_t resultXWsaCleanup = XWSACleanup();
	
	XllnThreadLiveOverLanStop();
	
	xlive_online_initialized = false;
	
	return resultXWsaCleanup;
}

// #5312
uint32_t WINAPI XFriendsCreateEnumerator(uint32_t user_index, size_t starting_index, size_t returned_friends_count, size_t* buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (returned_friends_count > XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (returned_friends_count > XFRIENDS_MAX_C_RESULT) (%zu >= %u).", __func__, returned_friends_count, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index >= XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index >= XFRIENDS_MAX_C_RESULT) (%zu >= %u).", __func__, starting_index, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index + returned_friends_count < starting_index) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index + returned_friends_count < starting_index) (%zu + %zu >= %zu).", __func__, starting_index, returned_friends_count, starting_index);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index + returned_friends_count > XFRIENDS_MAX_C_RESULT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index + returned_friends_count > XFRIENDS_MAX_C_RESULT) (%zu + %zu > %u).", __func__, starting_index, returned_friends_count, XFRIENDS_MAX_C_RESULT);
		return ERROR_INVALID_PARAMETER;
	}
	if (!buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}

	*buffer_size = returned_friends_count * sizeof(XONLINE_FRIEND);
	*enumerator_handle = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_xfriends_enumerators);
    auto friends = SocialFriends();
    size_t first = (std::min)(starting_index, friends.size());
    size_t last = (std::min)(first + returned_friends_count, friends.size());
    xlive_xfriends_enumerators[*enumerator_handle] = std::vector<XONLINE_FRIEND>(friends.begin() + first, friends.begin() + last);
	LeaveCriticalSection(&xlive_critsec_xfriends_enumerators);

	return ERROR_SUCCESS;
}

void XllnDirectIpConnectCancel()
{
	memset(&xlln_direct_ip_connect, 0, sizeof(xlln_direct_ip_connect));
	EnableWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_DIRECT_IP_CONNECT), true);
}

void XllnDirectIpConnectTo(uint32_t local_player_id, SOCKADDR_STORAGE* remote_sockaddr, const char* remote_password, const XNKID* requested_session)
{
	if (!xlln_network_instance_base_port) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s XLLN networking is disabled.", __func__);
		MessageBoxW(xlln_window_hwnd, L"XLLN networking is disabled.", L"XLLN Direct IP Connect Error", MB_OK);
		return;
	}
	
	if (xlln_direct_ip_connect.joinRequestSignature && xlln_direct_ip_connect.timeoutAt) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s There is already an active direct IP connection attempt.", __func__);
		MessageBoxW(xlln_window_hwnd, L"There is already an active direct IP connection attempt.", L"XLLN Direct IP Connect Error", MB_OK);
		return;
	}
	
	if (local_player_id >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, local_player_id);
		wchar_t* messageDescription = FormMallocString(L"User %d does not exist.", local_player_id + 1);
		MessageBoxW(xlln_window_hwnd, messageDescription, L"XLLN Direct IP Connect Error", MB_OK);
		free(messageDescription);
		return;
	}
	if (xlive_local_users[local_player_id].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, local_player_id);
		wchar_t* messageDescription = FormMallocString(L"User %d is not signed in.", local_player_id + 1);
		MessageBoxW(xlln_window_hwnd, messageDescription, L"XLLN Direct IP Connect Error", MB_OK);
		free(messageDescription);
		return;
	}
	
	EnableWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_DIRECT_IP_CONNECT), false);
	
	__time64_t ltime;
	_time64(&ltime);//seconds since epoch.
	
	xlln_direct_ip_connect.timeoutAt = ltime + 5;
	
	while (!(xlln_direct_ip_connect.joinRequestSignature = rand()));
	xlln_direct_ip_connect.localPlayerId = local_player_id;
	
	XLLN_NET_SEND_PACKET_INFO* sendPacket = new XLLN_NET_SEND_PACKET_INFO;
	sendPacket->destinationAddress = *remote_sockaddr;
	{
		const size_t packetSizeType = sizeof(XllnNetworkPacket::TYPE);
		const size_t packetSizeTypeDirectIpRequest = sizeof(XllnNetworkPacket::DIRECT_IP_REQUEST);
		const size_t packetSize = packetSizeType + packetSizeTypeDirectIpRequest + (requested_session ? sizeof(XNKID) : 0);
		
		uint8_t* packetBuffer = new uint8_t[packetSize];
		packetBuffer[0] = XllnNetworkPacket::TYPE::XLLN_NPT_DIRECT_IP_REQUEST;
		XllnNetworkPacket::DIRECT_IP_REQUEST &directIpRequest = *(XllnNetworkPacket::DIRECT_IP_REQUEST*)&packetBuffer[packetSizeType];
		memset(&directIpRequest, 0, sizeof(XllnNetworkPacket::DIRECT_IP_REQUEST));
		directIpRequest.joinRequestSignature = xlln_direct_ip_connect.joinRequestSignature;
		
		size_t remotePasswordLen = strlen(remote_password);
		if (remotePasswordLen) {
			mbedtls_sha256((const uint8_t*)remote_password, remotePasswordLen, directIpRequest.passwordSha256, 0);
		}
		
		if (requested_session) memcpy(packetBuffer + packetSizeType + packetSizeTypeDirectIpRequest, requested_session, sizeof(XNKID));
        sendPacket->data = packetBuffer;
		sendPacket->dataSize = packetSize;
	}
	
	if (!SendPacketToRemoteInstance(sendPacket)) {
		delete sendPacket;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR, "%s Failed to send join request.", __func__);
		MessageBoxW(xlln_window_hwnd, L"Failed to send join request.", L"XLLN Direct IP Connect Error", MB_OK);
	}
	sendPacket = 0;
}

// #5315
uint32_t WINAPI XInviteGetAcceptedInfo(uint32_t user_index, XINVITE_INFO* xinvite_info)
{
	TRACE_FX();
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO,
		"XInviteGetAcceptedInfo requested user=%u remoteInstance=0x%08x.",user_index,xlln_direct_ip_connect.remoteInstanceId);
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!xinvite_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xinvite_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (xlln_direct_ip_connect.localPlayerId >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, xlln_direct_ip_connect.localPlayerId);
		return ERROR_NOT_LOGGED_ON;
	}
	if (xlive_local_users[xlln_direct_ip_connect.localPlayerId].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, xlln_direct_ip_connect.localPlayerId);
		return ERROR_NOT_LOGGED_ON;
	}
	
	if (!xlln_direct_ip_connect.remoteInstanceId) {
		return XONLINE_E_MESSAGE_PROPERTY_NOT_FOUND;
	}
	
	xinvite_info->hostInfo.sessionID = xlln_direct_ip_connect.remoteSessionId;
	xinvite_info->hostInfo.keyExchangeKey = xlln_direct_ip_connect.remoteKeyExchangeKey;
	XllnNetEntityGetXnaddrXnkidByInstanceId(xlln_direct_ip_connect.remoteInstanceId, &xinvite_info->hostInfo.hostAddress, 0);
	xinvite_info->dwTitleID = xlln_direct_ip_connect.remoteTitleId;
	xinvite_info->fFromGameInvite = TRUE;
	xinvite_info->xuidInvitee = xlive_local_users[xlln_direct_ip_connect.localPlayerId].xuid;
	xinvite_info->xuidInviter = xlln_direct_ip_connect.remoteXuid;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO,
		"XInviteGetAcceptedInfo returned session details for title=0x%08x inviter=0x%016llx.",xinvite_info->dwTitleID,xinvite_info->xuidInviter);
	
	return ERROR_SUCCESS;
}

// #5316
uint32_t WINAPI XInviteSend(uint32_t user_index, size_t invite_xuid_count, const XUID* invite_xuids, const wchar_t* invite_message, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!invite_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s invite_xuid_count is 0.", __func__);
		return E_INVALIDARG;
	}
	if (invite_xuid_count > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s invite_xuid_count (%u) is greater than 0x64.", __func__, invite_xuid_count);
		return E_INVALIDARG;
	}
	if (!invite_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s invite_xuids is NULL.", __func__);
		return E_POINTER;
	}
	if (invite_message && wcsnlen_s(invite_message, 0x100+1) > 0x100) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Length of invite_message is > 0x100.", __func__);
		return E_INVALIDARG;
	}
	
	uint32_t result = SocialInvite(user_index, invite_xuid_count, invite_xuids);
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	return result;
}

// #5324
XONLINE_NAT_TYPE WINAPI XOnlineGetNatType()
{
	TRACE_FX();
	//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s TODO.", __func__);
	return XONLINE_NAT_OPEN;
}

// #5334
uint32_t WINAPI XOnlineGetServiceInfo(uint32_t service_id, XONLINE_SERVICE_INFO* service_info)
{
	TRACE_FX();
	if (!service_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s service_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_SERVICE_NOT_FOUND;
	return ERROR_SUCCESS;
	return ERROR_FUNCTION_FAILED;
	return ERROR_CONNECTION_INVALID;
}

// #5335
uint32_t WINAPI XTitleServerCreateEnumerator(const char* server_info, size_t result_item_count, size_t* buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (server_info && strnlen_s(server_info, XTITLE_SERVER_MAX_SERVER_INFO_SIZE) > XTITLE_SERVER_MAX_SERVER_INFO_LEN) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s length of server_info is > XTITLE_SERVER_MAX_SERVER_INFO_LEN (%u).", __func__, XTITLE_SERVER_MAX_SERVER_INFO_LEN);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_item_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_item_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_item_count > XTITLE_SERVER_MAX_LSP_INFO) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_item_count (%u) is greater than XTITLE_SERVER_MAX_LSP_INFO (%u).", __func__, result_item_count, XTITLE_SERVER_MAX_LSP_INFO);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (buffer_size) {
		*buffer_size = sizeof(XTITLE_SERVER_INFO) * result_item_count;
	}
	
	*enumerator_handle = CreateMutex(NULL, NULL, NULL);
	EnterCriticalSection(&xlive_critsec_title_server_enumerators);
	xlive_title_server_enumerators[*enumerator_handle];
	LeaveCriticalSection(&xlive_critsec_title_server_enumerators);
	
	return ERROR_SUCCESS;
}

// #5359
HRESULT WINAPI XLiveGetUPnPState(XONLINE_NAT_TYPE* nat_type)
{
	TRACE_FX();
	if (!nat_type) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s nat_type is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	// FIXME not sure the type of this variable or the correct values.
	*nat_type = XONLINE_NAT_OPEN;
	
	return S_OK;
}
