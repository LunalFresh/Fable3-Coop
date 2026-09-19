#include <winsock2.h>
#include <Windows.h>
#include <unordered_map>
#include <string>
#include <algorithm>
#include "xlln.hpp"
#include "network-instance-id.hpp"
#include "../dllmain.hpp"
#include "./debug-log.hpp"
#include "xlln-config.hpp"
#include "wnd-main.hpp"
#include "../utils/utils.hpp"
#include "../utils/util-socket.hpp"
#include "../utils/util-checksum.hpp"
#include "../xlive/xhv-engine.hpp"
#include "../xlive/xdefs.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xlocator.hpp"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xuser.hpp"
#include "../xlive/xsession.hpp"
#include "../xlive/xnet.hpp"
#include "../xlive/xnetqos.hpp"
#include "../xlive/xcustom.hpp"
#include "../xlive/xpresence.hpp"
#include "../xlive/xmarketplace.hpp"
#include "../xlive/xcontent.hpp"
#include "../xlive/xnotify.hpp"
#include "../xlln/xlln-network.hpp"
#include "./wnd-achievements.hpp"
#include "./wnd-stats.hpp"
#include "./wnd-user-custom-list.hpp"
#include "./wnd-user-card.hpp"
#include <ws2tcpip.h>
#include <stdlib.h>
#include "rapidxml.hpp"
extern "C" {
#include "../utils/puff.h"
}
#include "../third-party/fantasyname/namegen.h"
#include <time.h>

HINSTANCE xlln_hModule = NULL;
// 0 - unassigned. Counts from 1.
uint32_t xlln_local_instance_id = 0;
uint32_t xlln_global_instance_id = 0;
HMENU hMenu_network_adapters = 0;
uint32_t xlln_login_player = 0;

bool xlln_debug = false;

char* broadcastAddrInput = 0;
char* xlln_direct_ip_connect_password = 0;
char* xlln_direct_ip_connect_ip_port = 0;

CRITICAL_SECTION xlln_critsec_callbacks_network_interface_changed;
std::set<XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED> xlln_callbacks_network_interface_changed;
CRITICAL_SECTION xlln_critsec_callbacks_network_socket_bind;
std::set<XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND> xlln_callbacks_network_socket_bind;

std::vector<XACHIEVEMENT_DETAILS> xlive_achievement_details = {};
std::map<uint32_t, XLLN_STAT_VIEW_DEF> xlive_stat_view_defs = {};
std::map<uint32_t, XLLN_XLAST_QUERY> xlive_xlast_queries = {};
// The XLAST's matchmaking Constants, keyed by constant id. Read-only after single-threaded init.
static std::map<uint32_t, double> xlln_xlast_constants = {};
uint8_t xlive_locale = XLANGUAGE_ENGLISH;

// #41140
uint32_t WINAPI XLLNLogin(uint32_t user_index, BOOL lsb_8_live_enabled_8_online_disabled_msb_16_reserved, uint32_t user_id, const char* user_username)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		return ERROR_NO_SUCH_USER;
	}
	if (user_username && (!*user_username || strlen(user_username) > XUSER_MAX_NAME_LENGTH)) {
		return ERROR_INVALID_ACCOUNT_NAME;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_NotSignedIn) {
		return ERROR_ALREADY_ASSIGNED;
	}
	
	if (user_username) {
		strcpy_s(xlive_local_users[user_index].username, sizeof(xlive_local_users[user_index].username), user_username);
	}
	else {
		unsigned long seedNamegen = (unsigned long)rand();
		int resultNamegen;
		do {
			resultNamegen = namegen(xlive_local_users[user_index].username, sizeof(xlive_local_users[user_index].username), "<!DdM|!DdV|!Dd|!m|!BVC|!BdC !DdM|!DdV|!Dd|!m|!BVC|!BdC>|<!DdM|!DdV|!Dd|!m|!BVC|!BdC>", &seedNamegen);
		} while (resultNamegen == NAMEGEN_TRUNCATED || xlive_local_users[user_index].username[0] == 0);
	}
	
	if (!user_id) {
		size_t usernameLength = strlen(xlive_local_users[user_index].username);
		user_id = crc32buf((uint8_t*)xlive_local_users[user_index].username, usernameLength);
	}
	
	xlive_local_users[user_index].live_enabled = !!(lsb_8_live_enabled_8_online_disabled_msb_16_reserved & 0xFF);
	xlive_local_users[user_index].online_enabled = xlive_local_users[user_index].live_enabled && !(lsb_8_live_enabled_8_online_disabled_msb_16_reserved & 0xFF00);
	xlive_local_users[user_index].signin_state = xlive_local_users[user_index].live_enabled ? eXUserSigninState_SignedInToLive : eXUserSigninState_SignedInLocally;
	xlive_local_users[user_index].guest_number = 0;
	xlive_local_users[user_index].sponsor_user_index = XUSER_INDEX_NONE;
	xlive_local_users[user_index].xuid = BuildXUID(user_id, xlive_local_users[user_index].live_enabled, xlive_local_users[user_index].online_enabled, xlive_local_users[user_index].guest_number);
	
	XUserContextResetDefaults(user_index);
	XUserPropertyResetDefaults(user_index);
	
	// Restore this user's persisted leaderboard stats; only a LIVE account has a profile directory.
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_SignedInToLive) {
		XllnStatsLoadUserFromDisk(user_index);
	}

	xlive_local_users[user_index].auto_login = false;
	XLiveNotifyAddEvent(XN_SYS_SIGNINCHANGED, (size_t)1 << user_index);
	
	bool othersSignedIntoLive = false;
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (iUser != user_index && xlive_local_users[iUser].signin_state == eXUserSigninState_SignedInToLive) {
			othersSignedIntoLive = true;
			break;
		}
	}
	if (!othersSignedIntoLive && xlive_local_users[user_index].signin_state == eXUserSigninState_SignedInToLive) {
		XLiveNotifyAddEvent(XN_LIVE_CONNECTIONCHANGED, XONLINE_S_LOGON_CONNECTION_ESTABLISHED);
	}
	
	PostMessageW(xlln_hwnd_achievements, XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE, 0, 0);
	PostMessageW(xlln_hwnd_stats, XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE, 0, 0);
	
	if (user_index == xlln_login_player) {
		SetDlgItemTextA(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_TBX_USERNAME, xlive_local_users[user_index].username);
		CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_LIVE_ENABLE, xlive_local_users[user_index].live_enabled ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_ONLINE_ENABLE, xlive_local_users[user_index].online_enabled ? BST_CHECKED : BST_UNCHECKED);
		
		CheckDlgButton(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_CHK_AUTO_LOGIN, BST_UNCHECKED);
		
		bool checked = true;
		ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}
	
	return ERROR_SUCCESS;
}

// #41141
uint32_t WINAPI XLLNLogout(uint32_t user_index)
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
	
	bool wasSignedIntoLive = xlive_local_users[user_index].signin_state == eXUserSigninState_SignedInToLive;
	xlive_local_users[user_index].signin_state = eXUserSigninState_NotSignedIn;
	XLiveNotifyAddEvent(XN_SYS_SIGNINCHANGED, (size_t)1 << user_index);
	
	bool othersSignedIntoLive = false;
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (iUser != user_index && xlive_local_users[iUser].signin_state == eXUserSigninState_SignedInToLive) {
			othersSignedIntoLive = true;
			break;
		}
	}
	if (!othersSignedIntoLive && wasSignedIntoLive) {
		XLiveNotifyAddEvent(XN_LIVE_CONNECTIONCHANGED, XONLINE_S_LOGON_DISCONNECTED);
	}
	
	PostMessageW(xlln_hwnd_achievements, XLLNControlsMessageNumbers::EVENT_ACHIEVEMENTS_USERS_UPDATE, 0, 0);
	PostMessageW(xlln_hwnd_stats, XLLNControlsMessageNumbers::EVENT_STATS_USERS_UPDATE, 0, 0);
	
	if (user_index == xlln_login_player) {
		bool checked = false;
		ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGIN), checked ? SW_HIDE : SW_SHOWNORMAL);
		ShowWindow(GetDlgItem(xlln_window_hwnd, XLLNControlsMessageNumbers::MAIN_BTN_LOGOUT), checked ? SW_SHOWNORMAL : SW_HIDE);
	}
	
	return ERROR_SUCCESS;
}

// #41142
uint32_t WINAPI XLLNModifyProperty(XLLNModifyPropertyTypes::TYPE property_id, void** new_value, void** old_value)
{
	TRACE_FX();
	switch (property_id) {
		case XLLNModifyPropertyTypes::tUNKNOWN: {
			return ERROR_INVALID_PARAMETER;
		}
		case XLLNModifyPropertyTypes::tFPS_LIMIT: {
			if (old_value && !new_value) {
				*(uint32_t*)old_value = xlive_fps_limit;
			}
			else if (new_value) {
				uint32_t old_value2 = SetFPSLimit(*(uint32_t*)new_value);
				if (old_value) {
					*(uint32_t*)old_value = old_value2;
				}
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::tBASE_PORT: {
			return ERROR_NOT_SUPPORTED;
		}
		case XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_REGISTER: {
			return ERROR_NOT_SUPPORTED;
		}
		case XLLNModifyPropertyTypes::tRECVFROM_CUSTOM_HANDLER_UNREGISTER: {
			return ERROR_NOT_SUPPORTED;
		}
		case XLLNModifyPropertyTypes::tGUIDE_UI_HANDLER: {
			if (old_value && !new_value) {
				// TODO
				*old_value = 0;
				return ERROR_NOT_SUPPORTED;
			}
			else if (new_value) {
				GUIDE_UI_HANDLER_INFO handlerInfoNew;
				handlerInfoNew.xllnModule = 0;
				handlerInfoNew.guideUiHandler = (tGuideUiHandler)*new_value;
				
				EnterCriticalSection(&xlln_critsec_guide_ui_handlers);
				GUIDE_UI_HANDLER_INFO* handlerInfo = 0;
				for (auto itrGuideUiHandlerInfo = xlln_guide_ui_handlers.begin(); itrGuideUiHandlerInfo != xlln_guide_ui_handlers.end(); itrGuideUiHandlerInfo++) {
					if (itrGuideUiHandlerInfo->xllnModule == handlerInfoNew.xllnModule) {
						if (!handlerInfoNew.guideUiHandler) {
							xlln_guide_ui_handlers.erase(itrGuideUiHandlerInfo);
							break;
						}
						handlerInfo = &(*itrGuideUiHandlerInfo);
						break;
					}
				}
				if (old_value) {
					// TODO
					*old_value = 0;
				}
				if (handlerInfo) {
					*handlerInfo = handlerInfoNew;
				}
				else if (handlerInfoNew.guideUiHandler) {
					xlln_guide_ui_handlers.push_back(handlerInfoNew);
				}
				LeaveCriticalSection(&xlln_critsec_guide_ui_handlers);
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::tXHV_ENGINE_ENABLED: {
			if (old_value && !new_value) {
				*(bool*)old_value = xlive_xhv_engine_enabled;
			}
			else if (new_value) {
				if (old_value) {
					*(bool*)old_value = xlive_xhv_engine_enabled;
				}
				xlive_xhv_engine_enabled = *(bool*)new_value;
			}
			else {
				return ERROR_NOT_SUPPORTED;
			}
			return ERROR_SUCCESS;
		}
		case XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_INTERFACE_CHANGED: {
			if (!new_value && !old_value) {
				return ERROR_NOT_SUPPORTED;
			}
			
			uint32_t result = ERROR_SUCCESS;
			EnterCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
			
			if (old_value) {
				size_t resultErase = xlln_callbacks_network_interface_changed.erase((XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED)old_value);
				result = resultErase ? ERROR_SUCCESS : ERROR_NOT_FOUND;
			}
			if (new_value) {
				auto resultInsert = xlln_callbacks_network_interface_changed.insert((XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED)new_value);
				result = resultInsert.second ? (result == ERROR_SUCCESS ? ERROR_SUCCESS : result) : (result == ERROR_SUCCESS ? ERROR_ALREADY_REGISTERED : result);
			}
			
			LeaveCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
			
			return result;
		}
		case XLLNModifyPropertyTypes::TYPE::tCALLBACK_NETWORK_SOCKET_BIND: {
			if (!new_value && !old_value) {
				return ERROR_NOT_SUPPORTED;
			}
			
			uint32_t result = ERROR_SUCCESS;
			EnterCriticalSection(&xlln_critsec_callbacks_network_socket_bind);
			
			if (old_value) {
				size_t resultErase = xlln_callbacks_network_socket_bind.erase((XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND)old_value);
				result = resultErase ? ERROR_SUCCESS : ERROR_NOT_FOUND;
			}
			if (new_value) {
				auto resultInsert = xlln_callbacks_network_socket_bind.insert((XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND)new_value);
				result = resultInsert.second ? (result == ERROR_SUCCESS ? ERROR_SUCCESS : result) : (result == ERROR_SUCCESS ? ERROR_ALREADY_REGISTERED : result);
			}
			
			LeaveCriticalSection(&xlln_critsec_callbacks_network_socket_bind);
			
			return result;
		}
	}
	
	return ERROR_UNKNOWN_PROPERTY;
}

// #41145
uint32_t WINAPI XLLNGetXLLNStoragePath(uint32_t module_handle, uint32_t* result_local_instance_id, wchar_t* result_storage_path_buffer, size_t* result_storage_path_buffer_size)
{
	if (!result_local_instance_id && !result_storage_path_buffer && !result_storage_path_buffer_size) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_storage_path_buffer && !result_storage_path_buffer_size) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_storage_path_buffer) {
		result_storage_path_buffer[0] = 0;
	}
	if (result_local_instance_id) {
		*result_local_instance_id = 0;
	}
	if (!module_handle) {
		return ERROR_INVALID_PARAMETER;
	}
	if (result_local_instance_id) {
		*result_local_instance_id = xlln_local_instance_id;
	}
	if (result_storage_path_buffer_size) {
		wchar_t* configPath = PathFromFilename(xlln_file_config_path);
		if (!configPath) {
			*result_storage_path_buffer_size = 0;
			return ERROR_PATH_NOT_FOUND;
		}
		
		size_t configPathLen = wcslen(configPath);
		size_t configPathBufSize = (configPathLen + 1) * sizeof(wchar_t);
		
		if (configPathBufSize > *result_storage_path_buffer_size) {
			*result_storage_path_buffer_size = configPathBufSize;
			delete[] configPath;
			return ERROR_INSUFFICIENT_BUFFER;
		}
		if (*result_storage_path_buffer_size == 0) {
			*result_storage_path_buffer_size = configPathBufSize;
		}
		if (result_storage_path_buffer) {
			memcpy(result_storage_path_buffer, configPath, configPathBufSize);
		}
		delete[] configPath;
	}
	return ERROR_SUCCESS;
}

// #41146
uint32_t WINAPI XLLNSetBasePortOffsetMapping(uint8_t* port_offsets, uint16_t* port_originals, uint8_t port_mappings_size)
{
	return ERROR_NOT_SUPPORTED;
}

void XllnNetworkInterfaceChanged()
{
	char* interfaceName = 0;
	SOCKADDR_STORAGE addressGateway = {AF_UNSPEC};
	SOCKADDR_STORAGE addressUnicast = {AF_UNSPEC};
	SOCKADDR_STORAGE addressBroadcast = {AF_UNSPEC};
	
	EnterCriticalSection(&xlive_critsec_network_adapter);
	if (xlive_specific_network_adapter) {
		interfaceName = CloneString(xlive_specific_network_adapter->name);
		addressGateway = xlive_specific_network_adapter->addressGateway;
		addressUnicast = xlive_specific_network_adapter->addressUnicast;
		addressBroadcast = xlive_specific_network_adapter->addressBroadcast;
	}
	LeaveCriticalSection(&xlive_critsec_network_adapter);
	
	EnterCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
	for (const auto callback : xlln_callbacks_network_interface_changed) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_TRACE,
			"%s xlln_callbacks_network_interface_changed invoking function (0x%zx)."
			, __func__
			, (size_t)callback
		);
		if (interfaceName) {
			callback(interfaceName, &addressGateway, &addressUnicast, &addressBroadcast);
		}
		else {
			callback(0, 0, 0, 0);
		}
	}
	LeaveCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
	
	if (interfaceName) {
		delete[] interfaceName;
		interfaceName = 0;
	}
}

static bool IsBroadcastAddress(const SOCKADDR_STORAGE* sockaddr)
{
	if (sockaddr->ss_family != AF_INET) {
		return false;
	}
	
	const uint32_t ipv4NBO = ((sockaddr_in*)sockaddr)->sin_addr.s_addr;
	const uint32_t ipv4HBO = ntohl(ipv4NBO);
	
	if (ipv4HBO == INADDR_BROADCAST) {
		return true;
	}
	if (ipv4HBO == INADDR_ANY) {
		return true;
	}
	
	//TODO IPv6 support.
	for (ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface : xlive_eligible_network_adapters) {
		if (eligibleNetworkInterface->addressBroadcast.ss_family == AF_INET && ((sockaddr_in*)&eligibleNetworkInterface->addressBroadcast)->sin_addr.s_addr == ipv4NBO) {
			return true;
		}
	}
	
	return false;
}

/// Mutates the input buffer.
void ParseBroadcastAddrInput(char* jlbuffer)
{
	EnterCriticalSection(&xlln_critsec_network_broadcast_addresses);
	xlln_network_broadcast_addresses.clear();
	XllnNetworkBroadcastEntity::BROADCAST_ENTITY broadcastEntity;
	char* current = jlbuffer;
	while (1) {
		char* comma = strchr(current, ',');
		if (comma) {
			comma[0] = 0;
		}
		
		char* colon = strrchr(current, ':');
		if (colon) {
			colon[0] = 0;
			
			if (current[0] == '[') {
				current = &current[1];
				if (colon[-1] == ']') {
					colon[-1] = 0;
				}
			}
			
			uint16_t portHBO = 0;
			if (sscanf_s(&colon[1], "%hu", &portHBO) == 1) {
				if (current[0] == 0) {
					broadcastEntity.lastComm = 0;
					broadcastEntity.entityType = XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_BROADCAST_ADDR;
					memset(&broadcastEntity.sockaddr, 0, sizeof(broadcastEntity.sockaddr));
					(*(sockaddr_in*)&broadcastEntity.sockaddr).sin_family = AF_INET;
					(*(sockaddr_in*)&broadcastEntity.sockaddr).sin_addr.s_addr = htonl(INADDR_BROADCAST);
					(*(sockaddr_in*)&broadcastEntity.sockaddr).sin_port = htons(portHBO);
					xlln_network_broadcast_addresses.push_back(broadcastEntity);
				}
				else {
					addrinfo hints;
					memset(&hints, 0, sizeof(hints));
					
					hints.ai_family = PF_UNSPEC;
					hints.ai_socktype = SOCK_DGRAM;
					hints.ai_protocol = IPPROTO_UDP;
					
					in6_addr serveraddr;
					int rc = WS2_32_inet_pton(AF_INET, current, &serveraddr);
					if (rc == 1) {
						hints.ai_family = AF_INET;
						hints.ai_flags |= AI_NUMERICHOST;
					}
					else {
						rc = WS2_32_inet_pton(AF_INET6, current, &serveraddr);
						if (rc == 1) {
							hints.ai_family = AF_INET6;
							hints.ai_flags |= AI_NUMERICHOST;
						}
					}
					
					addrinfo* res;
					int error = getaddrinfo(current, NULL, &hints, &res);
					if (!error) {
						memset(&broadcastEntity.sockaddr, 0, sizeof(broadcastEntity.sockaddr));
						broadcastEntity.entityType = XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_UNKNOWN;
						broadcastEntity.lastComm = 0;
						
						addrinfo* nextRes = res;
						while (nextRes) {
							if (nextRes->ai_family == AF_INET) {
								memcpy(&broadcastEntity.sockaddr, res->ai_addr, res->ai_addrlen);
								(*(sockaddr_in*)&broadcastEntity.sockaddr).sin_port = htons(portHBO);
								broadcastEntity.entityType = IsBroadcastAddress(&broadcastEntity.sockaddr) ? XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_BROADCAST_ADDR : XllnNetworkBroadcastEntity::TYPE::XLLN_NBE_UNKNOWN;
								xlln_network_broadcast_addresses.push_back(broadcastEntity);
								break;
							}
							else if (nextRes->ai_family == AF_INET6) {
								memcpy(&broadcastEntity.sockaddr, res->ai_addr, res->ai_addrlen);
								(*(sockaddr_in6*)&broadcastEntity.sockaddr).sin6_port = htons(portHBO);
								xlln_network_broadcast_addresses.push_back(broadcastEntity);
								break;
							}
							else {
								nextRes = nextRes->ai_next;
							}
						}
						
						freeaddrinfo(res);
					}
				}
			}
		}
		
		if (comma) {
			current = &comma[1];
		}
		else {
			break;
		}
	}
	LeaveCriticalSection(&xlln_critsec_network_broadcast_addresses);
}

static void ReadTitleConfig(const wchar_t* titleExecutableFilePath)
{
	wchar_t* liveConfig = FormMallocString(L"%s.cfg", titleExecutableFilePath);
	FILE* fpLiveConfig;
	errno_t errorFileOpen = _wfopen_s(&fpLiveConfig, liveConfig, L"r");
	if (errorFileOpen) {
		XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Live Config fopen(\"%ls\", \"r\") error:", __func__, liveConfig);
		free(liveConfig);
		liveConfig = 0;
	}
	else {
		free(liveConfig);
		liveConfig = 0;
		
		fseek(fpLiveConfig, (long)0, SEEK_END);
		uint32_t fileSize = ftell(fpLiveConfig);
		fseek(fpLiveConfig, (long)0, SEEK_SET);
		fileSize -= ftell(fpLiveConfig);
		// Add a null sentinel to make the buffer a valid c string.
		fileSize += 1;
		
		uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * fileSize);
		size_t readC = fread(buffer, sizeof(uint8_t), fileSize / sizeof(uint8_t), fpLiveConfig);
		
		buffer[readC] = 0;
		
		fclose(fpLiveConfig);
		fpLiveConfig = 0;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Parsing TITLE.exe.cfg.");
		rapidxml::xml_document<> liveConfigXml;
		liveConfigXml.parse<0>((char*)buffer);
		
		rapidxml::xml_node<>* rootNode = liveConfigXml.first_node();
		while (rootNode) {
			if (strcmp(rootNode->name(), "Liveconfig") == 0) {
				rapidxml::xml_node<>* configNode = rootNode->first_node();
				while (configNode) {
					if (strcmp(configNode->name(), "titleid") == 0) {
						if (sscanf_s(configNode->value(), "%x", &xlive_title_id) == 1) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Title ID: %X.", xlive_title_id);
						}
					}
					else if (strcmp(configNode->name(), "titleversion") == 0) {
						if (sscanf_s(configNode->value(), "%x", &xlive_title_version) == 1) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "Title Version: 0x%08x.", xlive_title_version);
						}
					}
					configNode = configNode->next_sibling();
				}
				break;
			}
			rootNode = rootNode->next_sibling();
		}
		
		liveConfigXml.clear();
		
		free(buffer);
		buffer = 0;
	}
}

std::unordered_map<WORD, std::wstring> spa_file_strings;

LPWSTR GetStringFromIdOrDefault(WORD id)
{
	if (spa_file_strings.count(id)) {
		return (LPWSTR)spa_file_strings[id].c_str();
	}
	return (LPWSTR)L"";
}

bool XllnXlastConstantValue(uint32_t constant_id, double* result_value)
{
	auto itrConstant = xlln_xlast_constants.find(constant_id);
	if (itrConstant == xlln_xlast_constants.end()) {
		return false;
	}
	*result_value = itrConstant->second;
	return true;
}

static std::wstring XllnUtf8ToWide(const char* utf8)
{
	if (!utf8 || !*utf8) {
		return std::wstring();
	}
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (wlen <= 1) {
		return std::wstring();
	}
	std::wstring wide((size_t)wlen - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wide[0], wlen);
	return wide;
}

// Everything one XLAST document yields.
struct XLLN_XLAST_PARSE {
	std::map<uint32_t, std::wstring> viewNames;
	std::map<uint32_t, std::wstring> propertyNames;
	std::map<uint32_t, double> constants;
	std::map<uint32_t, XLLN_XLAST_QUERY> queries;
};

static XLLN_XLAST_COMPARISON XllnXlastComparisonFromText(const char* comparison)
{
	if (comparison) {
		if (strcmp(comparison, "!=") == 0) { return XLLN_XLAST_COMPARISON_NOT_EQUAL; }
		if (strcmp(comparison, "<") == 0) { return XLLN_XLAST_COMPARISON_LESS; }
		if (strcmp(comparison, "<=") == 0) { return XLLN_XLAST_COMPARISON_LESS_EQUAL; }
		if (strcmp(comparison, ">") == 0) { return XLLN_XLAST_COMPARISON_GREATER; }
		if (strcmp(comparison, ">=") == 0) { return XLLN_XLAST_COMPARISON_GREATER_EQUAL; }
	}
	return XLLN_XLAST_COMPARISON_EQUAL;
}

static XLLN_XLAST_OPERAND XllnXlastOperandFromText(const char* operand_type)
{
	if (operand_type) {
		if (strcmp(operand_type, "Constant") == 0) { return XLLN_XLAST_OPERAND_CONSTANT; }
		if (strcmp(operand_type, "ContextValue") == 0) { return XLLN_XLAST_OPERAND_CONTEXT_VALUE; }
	}
	return XLLN_XLAST_OPERAND_PARAMETER;
}

// Reads one Query, the unit a search selects by procedure index: its Filters restrict which sessions
// come back and its Returns says what each result carries. Filters are conjunctive, so their order
// does not matter; Returns is ordered by ordinal.
static void XllnCollectXlastQuery(rapidxml::xml_node<>* node, XLLN_XLAST_PARSE &parse)
{
	rapidxml::xml_attribute<>* attrId = node->first_attribute("id");
	if (!attrId) {
		return;
	}
	XLLN_XLAST_QUERY &query = parse.queries[(uint32_t)strtoul(attrId->value(), NULL, 0)];
	
	rapidxml::xml_attribute<>* attrMaxResults = node->first_attribute("maxResults");
	query.maxResults = attrMaxResults ? (uint32_t)strtoul(attrMaxResults->value(), NULL, 0) : 0;
	
	if (rapidxml::xml_node<>* nodeFilters = node->first_node("Filters")) {
		query.filters.clear();
		for (rapidxml::xml_node<>* nodeFilter = nodeFilters->first_node("Filter"); nodeFilter; nodeFilter = nodeFilter->next_sibling("Filter")) {
			rapidxml::xml_attribute<>* attrLeft = nodeFilter->first_attribute("left");
			rapidxml::xml_attribute<>* attrRight = nodeFilter->first_attribute("right");
			if (!attrLeft || !attrRight) {
				continue;
			}
			// A rule that does not start from something the advertising host set has nothing to compare
			// against a session, so drop it rather than guess at what it meant.
			rapidxml::xml_attribute<>* attrLeftType = nodeFilter->first_attribute("leftType");
			if (attrLeftType && attrLeftType->value() && strcmp(attrLeftType->value(), "Attribute") != 0) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s XLAST query (%s) filter left side is a %s, not an Attribute; ignored.", __func__, attrId->value(), attrLeftType->value());
				continue;
			}
			rapidxml::xml_attribute<>* attrComparison = nodeFilter->first_attribute("op");
			rapidxml::xml_attribute<>* attrRightType = nodeFilter->first_attribute("rightType");
			XLLN_XLAST_QUERY_FILTER filter;
			filter.attributeId = (uint32_t)strtoul(attrLeft->value(), NULL, 0);
			filter.comparison = XllnXlastComparisonFromText(attrComparison ? attrComparison->value() : 0);
			filter.operandType = XllnXlastOperandFromText(attrRightType ? attrRightType->value() : 0);
			filter.operand = (uint32_t)strtoul(attrRight->value(), NULL, 0);
			query.filters.push_back(filter);
		}
	}
	
	if (rapidxml::xml_node<>* nodeReturns = node->first_node("Returns")) {
		std::vector<std::pair<uint32_t, uint32_t>> returnsByOrdinal;
		for (rapidxml::xml_node<>* nodeReturn = nodeReturns->first_node("Return"); nodeReturn; nodeReturn = nodeReturn->next_sibling("Return")) {
			rapidxml::xml_attribute<>* attrReturnId = nodeReturn->first_attribute("id");
			if (!attrReturnId) {
				continue;
			}
			rapidxml::xml_attribute<>* attrOrdinal = nodeReturn->first_attribute("ordinal");
			uint32_t ordinal = attrOrdinal ? (uint32_t)strtoul(attrOrdinal->value(), NULL, 0) : (uint32_t)returnsByOrdinal.size();
			returnsByOrdinal.push_back(std::make_pair(ordinal, (uint32_t)strtoul(attrReturnId->value(), NULL, 0)));
		}
		if (!returnsByOrdinal.empty()) {
			std::sort(returnsByOrdinal.begin(), returnsByOrdinal.end());
			query.returns.clear();
			for (const auto &returnEntry : returnsByOrdinal) {
				query.returns.push_back(returnEntry.second);
			}
		}
	}
}

// Walk the XLAST tree collecting the friendlyName for every StatsView (keyed by decimal view id) and
// Property (keyed by full property id, e.g. 0x20000002), the matchmaking Constants table, and every
// matchmaking Query. strtoul base 0 handles both the decimal and 0x forms the XLAST mixes.
static void XllnCollectXlast(rapidxml::xml_node<>* node, XLLN_XLAST_PARSE &parse)
{
	for (rapidxml::xml_node<>* child = node->first_node(); child; child = child->next_sibling()) {
		const char* elementName = child->name();
		if (elementName && *elementName) {
			bool isView = strcmp(elementName, "StatsView") == 0;
			bool isProperty = !isView && strcmp(elementName, "Property") == 0;
			if (isView || isProperty) {
				rapidxml::xml_attribute<>* attrId = child->first_attribute("id");
				rapidxml::xml_attribute<>* attrName = child->first_attribute("friendlyName");
				if (attrId && attrName && attrName->value() && *attrName->value()) {
					uint32_t id = (uint32_t)strtoul(attrId->value(), NULL, 0);
					std::map<uint32_t, std::wstring> &target = isView ? parse.viewNames : parse.propertyNames;
					if (!target.count(id)) {
						target[id] = XllnUtf8ToWide(attrName->value());
					}
				}
			}
			else if (strcmp(elementName, "Constant") == 0) {
				rapidxml::xml_attribute<>* attrId = child->first_attribute("id");
				rapidxml::xml_attribute<>* attrValue = child->first_attribute("value");
				if (attrId && attrValue) {
					parse.constants[(uint32_t)strtoul(attrId->value(), NULL, 0)] = strtod(attrValue->value(), NULL);
				}
			}
			else if (strcmp(elementName, "Query") == 0) {
				XllnCollectXlastQuery(child, parse);
			}
		}
		XllnCollectXlast(child, parse);
	}
}

// The SPA's XVC2/XPRP sections only carry numeric string ids, which many titles leave as
// untranslated "<String text>" placeholders. The XSRC section holds the original XLAST
// project - gzip-compressed UTF-16 XML - which keeps a real friendlyName on every view and property.
// Decompress it (raw DEFLATE via puff, after stripping the gzip wrapper) and pull those names out.
static bool XllnParseSpaXsrcNames(
	const uint8_t* xsrc
	, size_t xsrc_length
	, XLLN_XLAST_PARSE &parse
)
{
	auto be32 = [](const uint8_t* p) -> uint32_t {
		return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
	};
	// XSRC (big-endian): magic, version, size, filenameLen, filename[], uncompressedSize, compressedSize, gzip[].
	if (xsrc_length < 24 || be32(xsrc) != 0x58535243 /* 'XSRC' */) {
		return false;
	}
	uint32_t filenameLength = be32(xsrc + 12);
	if (filenameLength > 1024 || (size_t)16 + filenameLength + 8 > xsrc_length) {
		return false;
	}
	const uint8_t* cursor = xsrc + 16 + filenameLength;
	uint32_t uncompressedSize = be32(cursor);
	cursor += 4;
	uint32_t compressedSize = be32(cursor);
	cursor += 4;
	// Compare by subtraction: adding compressedSize to the cursor offset can wrap 32-bit size_t.
	if (!uncompressedSize || uncompressedSize > 0x4000000 || compressedSize < 18 || compressedSize > xsrc_length - (size_t)(cursor - xsrc)) {
		return false;
	}
	// gzip member: id1 id2 = 0x1f 0x8b, method 8 (deflate).
	if (cursor[0] != 0x1f || cursor[1] != 0x8b || cursor[2] != 0x08) {
		return false;
	}
	uint8_t flags = cursor[3];
	size_t gzipHeader = 10;
	if (flags & 0x04) { // FEXTRA
		if (gzipHeader + 2 > compressedSize) { return false; }
		uint32_t extraLength = (uint32_t)cursor[gzipHeader] | ((uint32_t)cursor[gzipHeader + 1] << 8);
		gzipHeader += 2 + extraLength;
	}
	if (flags & 0x08) { // FNAME
		while (gzipHeader < compressedSize && cursor[gzipHeader]) { gzipHeader++; }
		gzipHeader++;
	}
	if (flags & 0x10) { // FCOMMENT
		while (gzipHeader < compressedSize && cursor[gzipHeader]) { gzipHeader++; }
		gzipHeader++;
	}
	if (flags & 0x02) { // FHCRC
		gzipHeader += 2;
	}
	if (gzipHeader + 8 > compressedSize) {
		return false;
	}

	unsigned long destLength = uncompressedSize;
	unsigned long sourceLength = compressedSize - gzipHeader; // deflate stream + 8-byte trailer; puff stops at the final block.
	uint8_t* dest = (uint8_t*)malloc(uncompressedSize);
	if (!dest) {
		return false;
	}
	int puffResult = puff(dest, &destLength, cursor + gzipHeader, &sourceLength);
	if (puffResult != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s XSRC inflate failed (puff rc=%d).", __func__, puffResult);
		free(dest);
		return false;
	}

	// The XLAST is UTF-16; RapidXML is byte-oriented, so convert to UTF-8 first. (A leading BOM
	// survives the conversion, but RapidXML's parse_bom step consumes it.)
	int wideCount = (int)(destLength / sizeof(wchar_t));
	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)dest, wideCount, NULL, 0, NULL, NULL);
	if (utf8Length <= 0) {
		free(dest);
		return false;
	}

	try {
		std::vector<char> utf8((size_t)utf8Length + 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)dest, wideCount, utf8.data(), utf8Length, NULL, NULL);
		free(dest);
		dest = 0;
		utf8[utf8Length] = 0;
		rapidxml::xml_document<> doc;
		doc.parse<0>(utf8.data());
		rapidxml::xml_node<>* root = doc.first_node();
		if (root) {
			XllnCollectXlast(root, parse);
		}
	}
	catch (...) {
		// Also covers bad_alloc from the buffer/name containers: this runs inside the title's
		// process at startup, so an escaping exception would take the game down with it.
		if (dest) {
			free(dest);
		}
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s XSRC XLAST XML parse failed.", __func__);
		return false;
	}
	return true;
}

bool ReadSpaFile(uint8_t locale)
{
	// Both the name and type need to be strings for the resource to be found.
	// Using the RT_RCDATA macro will not work.
	HRSRC spafileResource = FindResource(NULL, TEXT("SPAFILE"), TEXT("RT_RCDATA"));
	if (spafileResource) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Found SPAFILE %p.", __func__, spafileResource);
	}
	else {
		uint32_t error = GetLastError();
		XLLN_DEBUG_LOG_ECODE(error, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Could not find SPAFILE with error:", __func__);
		return false;
	}

	HGLOBAL spafileData = LoadResource(NULL, spafileResource);
	if (spafileData) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Loaded SPAFILE data %p.", __func__, spafileData);
	}
	else {
		uint32_t error = GetLastError();
		XLLN_DEBUG_LOG_ECODE(error, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Could not load SPAFILE data with error:", __func__);
		return false;
	}

	LPVOID spafileDataPtr = LockResource(spafileData);
	if (spafileDataPtr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Locked SPAFILE data %p.", __func__, spafileDataPtr);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Could not lock SPAFILE data.", __func__);
		return false;
	}

	DWORD spafileDataSize = SizeofResource(NULL, spafileResource);
	if (spafileDataSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE data size %u.", __func__, spafileDataSize);
	}
	else {
		uint32_t error = GetLastError();
		XLLN_DEBUG_LOG_ECODE(error, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Could not get SPAFILE data size with error:", __func__);
		return false;
	}

#pragma pack(push, 1)
	struct XdbfEntry {
		WORD ns;
		ULONGLONG id;
		DWORD offsetSpecifier;
		DWORD length;
	};

	struct XdbfFreeSpaceEntry {
		DWORD offsetSpecifier;
		DWORD length;
	};

	struct XdbfHeader {
		DWORD magic;
		DWORD version;
		DWORD entryTableLength;
		DWORD entryCount;
		DWORD freeSpaceTableLength;
		DWORD freeSpaceCount;
	};

	struct XachAchievement {
		WORD dwId;
		WORD wLabelId;
		WORD wDescriptionId;
		WORD wUnachievedId;
		DWORD dwImageId;
		WORD dwCred;
		CHAR _pad0[2];
		DWORD dwFlags;
		CHAR _pad1[16];
	};

	struct Xach {
		DWORD magic;
		DWORD version;
		DWORD size;
		WORD achivementCount;
	};

	struct XstrString {
		WORD id;
		WORD length;
	};

	struct Xstr {
		DWORD magic;
		DWORD version;
		DWORD size;
		WORD stringCount;
	};

	// XVC2 (stats view configuration). Big-endian like the rest of the XDBF file.
	struct Xvc2SectionHeader {
		DWORD magic;
		DWORD version;
		DWORD size;
	};

	struct Xvc2ViewFieldEntry {
		DWORD size;
		DWORD propertyId;
		DWORD flags;
		WORD attributeId;
		WORD stringId;
		WORD aggregationType;
		uint8_t ordinal;
		uint8_t fieldType;
		DWORD formatType;
		uint8_t _padding[8];
	};

	struct Xvc2StatsViewTableEntry {
		DWORD id;
		DWORD flags;
		WORD sharedIndex;
		WORD stringId;
		uint8_t _padding[4];
	};

	struct Xpbm {
		DWORD magic;
		DWORD version;
		DWORD size;
		DWORD contextCount;
		DWORD propertyCount;
	};
#pragma pack(pop)

	enum SPAFILE_MAGICS {
		XDBF_MAGIC = 0x58444246,
		XACH_MAGIC = 0x58414348,
		XSTR_MAGIC = 0x58535452,
		XVC2_MAGIC = 0x58564332,
		XSRC_MAGIC = 0x58535243,
	};

	XdbfHeader* xdbfHeader = (XdbfHeader*)spafileDataPtr;

	DWORD magic = xdbfHeader->magic;
	if (_byteswap_ulong(magic) == XDBF_MAGIC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE magic is XDBF.", __func__);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE magic is not XDBF.", __func__);
		return false;
	}

	DWORD entryCount = _byteswap_ulong(xdbfHeader->entryCount);
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE entry count %u.", __func__, entryCount);

	XdbfEntry* entries = (XdbfEntry*)((uint8_t*)xdbfHeader + sizeof(XdbfHeader));
	uint8_t* endOfHeader = (uint8_t*)xdbfHeader + sizeof(XdbfHeader) + _byteswap_ulong(xdbfHeader->entryTableLength) * sizeof(XdbfEntry) + _byteswap_ulong(xdbfHeader->freeSpaceTableLength) * sizeof(XdbfFreeSpaceEntry);
	Xach* xach = NULL;
	Xstr* xstr = NULL;
	Xstr* xstrFallback = NULL;
	Xvc2SectionHeader* xvc2 = NULL;
	for (DWORD i = 0; i < entryCount && (!xach || !xstr || !xvc2); ++i) {
		XdbfEntry* xdbfEntry = &entries[i];
		WORD ns = _byteswap_ushort(xdbfEntry->ns);
		ULONGLONG id = _byteswap_uint64(xdbfEntry->id);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Entry %hu %llu.", __func__, ns, id);
		if (ns == 1 && id == XACH_MAGIC) {
			if (xach) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s multiple SPAFILE XACH entries found, using first.", __func__);
			}
			else {
				xach = (Xach*)(endOfHeader + _byteswap_ulong(xdbfEntry->offsetSpecifier));
			}
		}
		else if (ns == 1 && id == XVC2_MAGIC) {
			if (!xvc2) {
				xvc2 = (Xvc2SectionHeader*)(endOfHeader + _byteswap_ulong(xdbfEntry->offsetSpecifier));
			}
		}
		else if (ns == 3) {
			if (!xstrFallback) {
				xstrFallback = (Xstr*)(endOfHeader + _byteswap_ulong(xdbfEntry->offsetSpecifier));
			}

			if (id == xlive_locale) {
				if (xstr) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s multiple SPAFILE XSTR entries found for locale %hhu, using first.", __func__, xlive_locale);
				}
				else {
					xstr = (Xstr*)(endOfHeader + _byteswap_ulong(xdbfEntry->offsetSpecifier));
				}
			}
		}
	}

	if (xach) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XACH found %p.", __func__, xach);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE XACH not found.", __func__);
		return false;
	}

	if (_byteswap_ulong(xach->magic) == XACH_MAGIC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XACH magic is XACH.", __func__);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE XACH magic is not XACH.", __func__);
		return false;
	}

	if (xstr) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XSTR found %p.", __func__, xstr);
	}
	else if (xstrFallback) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s SPAFILE XSTR not found for locale %hhu, using fallback.", __func__, xlive_locale);
		xstr = xstrFallback;
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE XSTR not found and no fallback.", __func__);
		return false;
	}

	if (_byteswap_ulong(xstr->magic) == XSTR_MAGIC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XSTR magic is XSTR.", __func__);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE XSTR magic is not XSTR.", __func__);
		return false;
	}

	XstrString* xstrString = (XstrString*)((uint8_t*)xstr + sizeof(Xstr));
	WORD stringCount = _byteswap_ushort(xstr->stringCount);
	for (WORD j = 0; j < stringCount; ++j) {
		WORD id = _byteswap_ushort(xstrString->id);
		WORD length = _byteswap_ushort(xstrString->length);
		char* string = (char*)((uint8_t*)xstrString + sizeof(*xstrString));
		std::wstring wstring;
		// MultiByteToWideChar reports a zero byte count as failure, and an empty entry is legal.
		if (length) {
			int wstringSize = MultiByteToWideChar(CP_UTF8, 0, string, length, NULL, 0);
			wstring.resize(wstringSize);
			if (!MultiByteToWideChar(CP_UTF8, 0, string, length, &wstring[0], wstringSize)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s MultiByteToWideChar failed.", __func__);
				return false;
			}
		}
		spa_file_strings.emplace(id, wstring);
		xstrString = (XstrString*)((uint8_t*)xstrString + sizeof(*xstrString) + length);
	}

	XachAchievement* achievements = (XachAchievement*)((uint8_t*)xach + sizeof(Xach));
	WORD achievementCount = _byteswap_ushort(xach->achivementCount);
	xlive_achievement_details.clear();
	xlive_achievement_details.reserve(achievementCount);
	for (WORD i = 0; i < achievementCount; ++i) {
		XachAchievement* xachAchievement = &achievements[i];
		WORD dwId = _byteswap_ushort(xachAchievement->dwId);
		WORD wLabelId = _byteswap_ushort(xachAchievement->wLabelId);
		WORD wDescriptionId = _byteswap_ushort(xachAchievement->wDescriptionId);
		WORD wUnachievedId = _byteswap_ushort(xachAchievement->wUnachievedId);
		DWORD dwImageId = _byteswap_ulong(xachAchievement->dwImageId);
		WORD dwCred = _byteswap_ushort(xachAchievement->dwCred);
		DWORD dwFlags = _byteswap_ulong(xachAchievement->dwFlags);
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s Achievement %hu %hu %hu %hu %u %hu %u.", __func__, dwId, wLabelId, wDescriptionId, wUnachievedId, dwImageId, dwCred, dwFlags);
		xlive_achievement_details.push_back(XACHIEVEMENT_DETAILS {
			dwId,
			GetStringFromIdOrDefault(wLabelId),
			GetStringFromIdOrDefault(wDescriptionId),
			GetStringFromIdOrDefault(wUnachievedId),
			dwImageId,
			dwCred,
			{},
			dwFlags
		});
	}

	// Parse the stats view definitions (XVC2). Each view references a "shared view" whose field
	// entries each declare a property id (top nibble = XUSER_DATA_TYPE, low id bits = stat column id)
	// and a string id for the column's name; the view table entry carries the view's own name. The
	// stored value map (xlive_written_stats) keys off (viewId, columnId), so the column id derived
	// here with the same mask XSessionWriteStats uses is what lines the two up.
	// Both section offsets below come from the title's own resource, so each is range checked before
	// anything dereferences it. The upper bound has to be a pointer comparison: past the end the
	// difference is negative, and as an unsigned length that reads as a huge size and passes.
	const uint8_t* spafileEnd = (const uint8_t*)spafileDataPtr + spafileDataSize;

	// Pull the real (friendlyName) view/column names out of the embedded XLAST (XSRC) if the title
	// ships one; falls back to the localized string ids below when a name is missing.
	XLLN_XLAST_PARSE xlastParse;
	xlive_xlast_queries.clear();
	xlln_xlast_constants.clear();
	for (DWORD i = 0; i < entryCount; ++i) {
		XdbfEntry* xdbfEntry = &entries[i];
		if (_byteswap_ushort(xdbfEntry->ns) == 1 && _byteswap_uint64(xdbfEntry->id) == XSRC_MAGIC) {
			const uint8_t* xsrcSection = endOfHeader + _byteswap_ulong(xdbfEntry->offsetSpecifier);
			size_t xsrcLength = _byteswap_ulong(xdbfEntry->length);
			if (xsrcSection < (const uint8_t*)spafileDataPtr
				|| xsrcSection > spafileEnd
				|| xsrcLength > (size_t)(spafileEnd - xsrcSection)) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s SPAFILE XSRC section (offset 0x%08x length 0x%zx) lies outside the resource; XLAST names and matchmaking queries unavailable.", __func__, _byteswap_ulong(xdbfEntry->offsetSpecifier), xsrcLength);
				break;
			}
			if (XllnParseSpaXsrcNames(xsrcSection, xsrcLength, xlastParse)) {
				xlive_xlast_queries = std::move(xlastParse.queries);
				xlln_xlast_constants = std::move(xlastParse.constants);
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XSRC XLAST parsed (%zu view names, %zu property names, %zu constants, %zu matchmaking queries).", __func__, xlastParse.viewNames.size(), xlastParse.propertyNames.size(), xlln_xlast_constants.size(), xlive_xlast_queries.size());
				for (const auto &queryEntry : xlive_xlast_queries) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s XLAST matchmaking query (%u) declares %zu filter(s) and returns %zu attribute(s).", __func__, queryEntry.first, queryEntry.second.filters.size(), queryEntry.second.returns.size());
				}
			}
			break;
		}
	}

	xlive_stat_view_defs.clear();
	// Every count below is read from the title's own resource, so a title whose XVC2 layout differs
	// from the one this parser expects desyncs the cursor on the first entry and then reads arbitrary
	// bytes as counts. Bound each step against the resource and abandon the parse instead: an empty
	// stats schema costs names in the UI, walking off the end faults the title at startup.
	bool xvc2Truncated = false;
	if (xvc2
		&& (const uint8_t*)xvc2 >= (const uint8_t*)spafileDataPtr
		&& (const uint8_t*)xvc2 <= spafileEnd
		&& (size_t)(spafileEnd - (const uint8_t*)xvc2) >= sizeof(Xvc2SectionHeader)
		&& _byteswap_ulong(xvc2->magic) == XVC2_MAGIC) {
		uint8_t* cursor = (uint8_t*)xvc2 + sizeof(Xvc2SectionHeader);
		// Returns the start of the next `bytes` and advances past them, or 0 once the resource is
		// exhausted; every later call short-circuits so one check per read site is enough.
		auto xvc2Take = [&cursor, spafileEnd, &xvc2Truncated](size_t bytes) -> uint8_t* {
			if (xvc2Truncated || (size_t)(spafileEnd - cursor) < bytes) {
				xvc2Truncated = true;
				return 0;
			}
			uint8_t* taken = cursor;
			cursor += bytes;
			return taken;
		};

		uint8_t* at = xvc2Take(sizeof(WORD));
		WORD sharedViewCount = at ? _byteswap_ushort(*(WORD*)at) : 0;
		std::vector<std::vector<XLLN_STAT_COLUMN_DEF>> sharedViewColumns;
		sharedViewColumns.reserve(sharedViewCount);
		for (WORD iSharedView = 0; iSharedView < sharedViewCount; iSharedView++) {
			at = xvc2Take(sizeof(WORD) + sizeof(WORD) + 8); // columnCount, rowCount, padding
			if (!at) {
				break;
			}
			WORD columnCount = _byteswap_ushort(*(WORD*)at);
			WORD rowCount = _byteswap_ushort(*(WORD*)(at + sizeof(WORD)));
			std::vector<XLLN_STAT_COLUMN_DEF> columns;
			columns.reserve(columnCount);
			for (WORD iColumn = 0; iColumn < columnCount; iColumn++) {
				at = xvc2Take(sizeof(Xvc2ViewFieldEntry));
				if (!at) {
					break;
				}
				Xvc2ViewFieldEntry* field = (Xvc2ViewFieldEntry*)at;
				DWORD propertyId = _byteswap_ulong(field->propertyId);
				XLLN_STAT_COLUMN_DEF column;
				column.columnId = propertyId & X_PROPERTY_ID_MASK;
				column.type = (uint8_t)((propertyId >> 28) & 0xF);
				column.isSystem = XISSYSTEMPROPERTY(propertyId) != 0;
				column.attributeId = _byteswap_ushort(field->attributeId);
				auto itrPropertyName = xlastParse.propertyNames.find(propertyId);
				if (itrPropertyName != xlastParse.propertyNames.end() && !itrPropertyName->second.empty()) {
					column.name = itrPropertyName->second;
				}
				else {
					column.name = GetStringFromIdOrDefault(_byteswap_ushort(field->stringId));
				}
				columns.push_back(std::move(column));
			}
			if (!xvc2Take((size_t)rowCount * sizeof(Xvc2ViewFieldEntry))) { // row field entries, unused here
				break;
			}
			at = xvc2Take(sizeof(Xpbm));
			if (!at) {
				break;
			}
			Xpbm* xpbm = (Xpbm*)at;
			DWORD contextCount = _byteswap_ulong(xpbm->contextCount);
			DWORD propertyCount = _byteswap_ulong(xpbm->propertyCount);
			// Widened because size_t is 32 bit here: two file-controlled counts scaled by 4 wrap, and a
			// product of zero would take nothing, advance nothing and report success, leaving the cursor
			// mid-table with no truncation recorded.
			uint64_t xpbmBytes = ((uint64_t)contextCount + propertyCount) * sizeof(DWORD);
			if (xpbmBytes > (uint64_t)(spafileEnd - cursor)) {
				xvc2Truncated = true;
				break;
			}
			if (!xvc2Take((size_t)xpbmBytes)) {
				break;
			}
			sharedViewColumns.push_back(std::move(columns));
		}
		at = xvc2Take(sizeof(WORD));
		WORD viewCount = at ? _byteswap_ushort(*(WORD*)at) : 0;
		for (WORD iView = 0; iView < viewCount; iView++) {
			at = xvc2Take(sizeof(Xvc2StatsViewTableEntry));
			if (!at) {
				break;
			}
			Xvc2StatsViewTableEntry* viewEntry = (Xvc2StatsViewTableEntry*)at;
			DWORD viewId = _byteswap_ulong(viewEntry->id);
			WORD sharedIndex = _byteswap_ushort(viewEntry->sharedIndex);
			WORD viewStringId = _byteswap_ushort(viewEntry->stringId);
			if (sharedIndex < sharedViewColumns.size()) {
				XLLN_STAT_VIEW_DEF viewDef;
				auto itrViewName = xlastParse.viewNames.find(viewId);
				if (itrViewName != xlastParse.viewNames.end() && !itrViewName->second.empty()) {
					viewDef.name = itrViewName->second;
				}
				else {
					viewDef.name = GetStringFromIdOrDefault(viewStringId);
				}
				viewDef.columns = sharedViewColumns[sharedIndex];
				xlive_stat_view_defs[viewId] = std::move(viewDef);
			}
		}
		if (xvc2Truncated) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s SPAFILE XVC2 ran past the end of the resource; the stats schema is incomplete (%zu views recovered).", __func__, xlive_stat_view_defs.size());
		}
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s SPAFILE XVC2 parsed (%hu shared views, %zu total views).", __func__, sharedViewCount, xlive_stat_view_defs.size());
	}

	return true;
}

void InitCriticalSections()
{
	InitializeCriticalSection(&xlive_critsec_network_adapter);
	InitializeCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
	InitializeCriticalSection(&xlln_critsec_callbacks_network_socket_bind);
	InitializeCriticalSection(&xlive_critsec_xlocator_enumerators);
	InitializeCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	InitializeCriticalSection(&xlive_critsec_xuser_stats);
	InitializeCriticalSection(&xlive_critsec_xuser_context_properties);
	InitializeCriticalSection(&xlive_critsec_xfriends_enumerators);
	InitializeCriticalSection(&xlive_critsec_custom_actions);
	InitializeCriticalSection(&xlive_critsec_local_user);
	InitializeCriticalSection(&xlive_critsec_remote_user);
	InitializeCriticalSection(&xlive_critsec_title_server_enumerators);
	InitializeCriticalSection(&xlive_critsec_presence_enumerators);
	InitializeCriticalSection(&xlive_critsec_xnotify);
	InitializeCriticalSection(&xlive_critsec_xsession);
	InitializeCriticalSection(&xlive_critsec_qos_listeners);
	InitializeCriticalSection(&xlive_critsec_qos_lookups);
	InitializeCriticalSection(&xlive_critsec_xmarketplace);
	InitializeCriticalSection(&xlive_critsec_xcontent);
	InitializeCriticalSection(&xlive_critsec_xhv_engines);
	InitializeCriticalSection(&xlive_critsec_hotkeys);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	InitializeCriticalSection(&xlln_critsec_liveoverlan_sessions);
	InitializeCriticalSection(&xlive_critsec_fps_limit);
	InitializeCriticalSection(&xlln_critsec_guide_ui_handlers);
	InitializeCriticalSection(&xlive_critsec_sockets);
	InitializeCriticalSection(&xlln_critsec_network_send);
	InitializeCriticalSection(&xlln_critsec_network_net_entity);
	InitializeCriticalSection(&xlive_critsec_xnet_session_keys);
	InitializeCriticalSection(&xlln_critsec_network_broadcast_addresses);
	InitializeCriticalSection(&xlln_critsec_user_custom_list);
	InitializeCriticalSection(&xlln_critsec_user_card);
	InitializeCriticalSection(&xlln_critsec_debug_log);
	InitializeCriticalSection(&xlln_critsec_achievements);
	InitializeCriticalSection(&xlln_critsec_stats);
}

void UninitCriticalSections()
{
	DeleteCriticalSection(&xlive_critsec_network_adapter);
	DeleteCriticalSection(&xlln_critsec_callbacks_network_interface_changed);
	DeleteCriticalSection(&xlln_critsec_callbacks_network_socket_bind);
	DeleteCriticalSection(&xlive_critsec_xlocator_enumerators);
	DeleteCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	DeleteCriticalSection(&xlive_critsec_xuser_context_properties);
	DeleteCriticalSection(&xlive_critsec_xuser_stats);
	DeleteCriticalSection(&xlive_critsec_xfriends_enumerators);
	DeleteCriticalSection(&xlive_critsec_custom_actions);
	DeleteCriticalSection(&xlive_critsec_local_user);
	DeleteCriticalSection(&xlive_critsec_remote_user);
	DeleteCriticalSection(&xlive_critsec_title_server_enumerators);
	DeleteCriticalSection(&xlive_critsec_presence_enumerators);
	DeleteCriticalSection(&xlive_critsec_xnotify);
	DeleteCriticalSection(&xlive_critsec_xsession);
	DeleteCriticalSection(&xlive_critsec_qos_listeners);
	DeleteCriticalSection(&xlive_critsec_qos_lookups);
	DeleteCriticalSection(&xlive_critsec_xmarketplace);
	DeleteCriticalSection(&xlive_critsec_xcontent);
	DeleteCriticalSection(&xlive_critsec_xhv_engines);
	DeleteCriticalSection(&xlive_critsec_hotkeys);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_broadcast);
	DeleteCriticalSection(&xlln_critsec_liveoverlan_sessions);
	DeleteCriticalSection(&xlive_critsec_fps_limit);
	DeleteCriticalSection(&xlln_critsec_guide_ui_handlers);
	DeleteCriticalSection(&xlive_critsec_sockets);
	DeleteCriticalSection(&xlln_critsec_network_send);
	DeleteCriticalSection(&xlln_critsec_network_net_entity);
	DeleteCriticalSection(&xlive_critsec_xnet_session_keys);
	DeleteCriticalSection(&xlln_critsec_network_broadcast_addresses);
	DeleteCriticalSection(&xlln_critsec_user_custom_list);
	DeleteCriticalSection(&xlln_critsec_user_card);
	DeleteCriticalSection(&xlln_critsec_debug_log);
	DeleteCriticalSection(&xlln_critsec_achievements);
	DeleteCriticalSection(&xlln_critsec_stats);
}

bool InitXLLN(HMODULE hModule)
{
	bool xlln_debug_pause = false;
	
	int nArgs;
	// GetCommandLineW() does not need de-allocating but ToArgv does.
	LPWSTR* lpwszArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (lpwszArglist == NULL) {
		uint32_t errorCmdLineToArgv = GetLastError();
		char* messageDescription = FormMallocString("CommandLineToArgvW(...) error 0x%08x.", errorCmdLineToArgv);
		MessageBoxA(NULL, messageDescription, "XLLN CommandLineToArgvW(...) Failed", MB_OK);
		free(messageDescription);
		return false;
	}
	for (int i = 1; i < nArgs; i++) {
		if (wcscmp(lpwszArglist[i], L"-xllndebug") == 0) {
			xlln_debug_pause = true;
		}
		else if (wcscmp(lpwszArglist[i], L"-xllndebuglog") == 0) {
#ifdef XLLN_DEBUG
			xlln_debug = true;
#endif
		}
	}
	
	while (xlln_debug_pause && !IsDebuggerPresent()) {
		Sleep(500L);
	}
	
	for (int i = 1; i < nArgs; i++) {
		if (wcscmp(lpwszArglist[i], L"-xlivedebug") == 0) {
			xlive_debug_pause = true;
		}
		else if (wcscmp(lpwszArglist[i], L"-xlivenetdisable") == 0) {
			xlive_netsocket_abort = true;
		}
		else if (wcsstr(lpwszArglist[i], L"-xllnconfig=") == lpwszArglist[i]) {
			wchar_t* configFilePath = &lpwszArglist[i][12];
			if (xlln_file_config_path) {
				free(xlln_file_config_path);
			}
			xlln_file_config_path = CloneString(configFilePath);
		}
		else if (wcsstr(lpwszArglist[i], L"-xlln_local_instance_id=") != NULL) {
			uint32_t tempuint32 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xlln_local_instance_id=%u", &tempuint32) == 1) {
				xlln_local_instance_id = tempuint32;
			}
		}
	}
	
	if (xlln_local_instance_id) {
		wchar_t* mutexName = FormMallocString(L"Global\\XLiveLessNessInstanceId#%u", xlln_local_instance_id);
		HANDLE mutex = CreateMutexW(0, FALSE, mutexName);
		free(mutexName);
		uint32_t mutex_last_error = GetLastError();
		if (mutex_last_error != ERROR_SUCCESS) {
			char* messageDescription = FormMallocString("Failed to get XLiveLessNess Local Instance ID %u.", xlln_local_instance_id);
			MessageBoxA(NULL, messageDescription, "XLLN Local Instance ID Fail", MB_OK);
			free(messageDescription);
			return false;
		}
	}
	else {
		uint32_t mutex_last_error;
		HANDLE mutex = NULL;
		do {
			if (mutex) {
				mutex_last_error = CloseHandle(mutex);
			}
			wchar_t* mutexName = FormMallocString(L"Global\\XLiveLessNessInstanceId#%u", ++xlln_local_instance_id);
			mutex = CreateMutexW(0, FALSE, mutexName);
			free(mutexName);
			mutex_last_error = GetLastError();
		} while (mutex_last_error != ERROR_SUCCESS);
	}
	
	// Default the appropriate value for the config.
	xlln_network_instance_port = xlln_network_instance_base_port + xlln_local_instance_id;
	
	srand((unsigned int)time(NULL));
	
	// Generated number between 1 and 0x00FFFFFF inclusive.
	// This is so it does not clash with INADDR_ANY, INADDR_LOOPBACK or INADDR_BROADCAST.
	// NOTE Titles have problems with any bits in 0xFF000000 being set.
	if (!XllnGenerateNetworkInstanceId(xlln_global_instance_id)) {
		MessageBoxW(NULL, L"Windows could not generate an XLLN network identity. Restart the game and try again.", L"XLLN Network Identity", MB_OK | MB_ICONERROR);
		return false;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s xlln_global_instance_id: 0x%08x."
		, __func__
		, xlln_global_instance_id
	);
	
	if (!broadcastAddrInput) {
		broadcastAddrInput = new char[1]{ "" };
	}
	
	if (!xlln_direct_ip_connect_password) {
		xlln_direct_ip_connect_password = new char[1]{ "" };
	}
	
	if (!xlln_direct_ip_connect_ip_port) {
		xlln_direct_ip_connect_ip_port = new char[1]{ "" };
	}
	
	uint32_t errorXllnConfig = InitXllnConfig();
	uint32_t errorXllnDebugLog = InitDebugLog();
	
	// Request version 2.0.
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	int resultWSAStartup = WSAStartup(wVersionRequested, &wsaData);
	if (resultWSAStartup) {
		XLLN_DEBUG_LOG_ECODE(resultWSAStartup, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s WSAStartup failed. Requested version (%hhu.%hhu)."
			, __func__
			, (uint8_t)(wVersionRequested & 0xFF)
			, (uint8_t)((wVersionRequested >> 8) & 0xFF)
		);
	}
	
	{
		wchar_t* titleExecutableFilePath = GetModuleFilePathW(xlln_hmod_title);
		ReadTitleConfig(titleExecutableFilePath);
		free(titleExecutableFilePath);
		titleExecutableFilePath = 0;
	}
	
	for (int i = 1; i < nArgs; i++) {
		if (wcsstr(lpwszArglist[i], L"-xlivefps=") != NULL) {
			uint32_t tempuint32 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xlivefps=%u", &tempuint32) == 1) {
				SetFPSLimit(tempuint32);
			}
		}
		else if (wcsstr(lpwszArglist[i], L"-xlln_network_instance_port=") != NULL) {
			uint16_t tempuint16 = 0;
			if (swscanf_s(lpwszArglist[i], L"-xlln_network_instance_port=%hu", &tempuint16) == 1) {
				xlln_network_instance_port = tempuint16;
			}
		}
		else if (wcsstr(lpwszArglist[i], L"-xllnbroadcastaddr=") == lpwszArglist[i]) {
			wchar_t* broadcastAddrInputTemp = &lpwszArglist[i][19];
			size_t bufferLen = wcslen(broadcastAddrInputTemp) + 1;
			if (broadcastAddrInput) {
				delete[] broadcastAddrInput;
			}
			broadcastAddrInput = new char[bufferLen];
			wcstombs2(broadcastAddrInput, broadcastAddrInputTemp, bufferLen);
		}
		else if (wcsstr(lpwszArglist[i], L"-xlivenetworkadapter=") == lpwszArglist[i]) {
			wchar_t* networkAdapterTemp = &lpwszArglist[i][21];
			size_t bufferLen = wcslen(networkAdapterTemp) + 1;
			{
				EnterCriticalSection(&xlive_critsec_network_adapter);
				if (xlive_config_specific_network_adapter_name) {
					delete[] xlive_config_specific_network_adapter_name;
				}
				xlive_config_specific_network_adapter_name = new char[bufferLen];
				wcstombs2(xlive_config_specific_network_adapter_name, networkAdapterTemp, bufferLen);
				LeaveCriticalSection(&xlive_critsec_network_adapter);
			}
		}
		else if (wcsstr(lpwszArglist[i], L"-xlivelocale=") == lpwszArglist[i]) {
			uint8_t tempuint8;
			if (swscanf_s(lpwszArglist[i], L"-xlivelocale=%hhu", &tempuint8) == 1 && tempuint8 >= XLANGUAGE_ENGLISH && tempuint8 <= XLANGUAGE_RUSSIAN) {
				xlive_locale = tempuint8;
			}
		}
	}
	
	LocalFree(lpwszArglist);
	lpwszArglist = 0;

	if (ReadSpaFile(xlive_locale)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s ReadSpaFile succeeded.", __func__);
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ReadSpaFile failed.", __func__);
	}
	
	xlln_hModule = hModule;
	
	return true;
}

bool UninitXLLN()
{
	int resultWSACleanup = WSACleanup();
	if (resultWSACleanup) {
		int32_t errorWsaCleanup = WSAGetLastError();
		XLLN_DEBUG_LOG_ECODE(errorWsaCleanup, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_ERROR
			, "%s WSACleanup failed."
			, __func__
		);
	}
	
	uint32_t errorXllnConfig = UninitXllnConfig();
	
	uint32_t errorXllnDebugLog = UninitDebugLog();
	
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (xlive_local_users[iUser].audio_input_device_name) {
			delete[] xlive_local_users[iUser].audio_input_device_name;
			xlive_local_users[iUser].audio_input_device_name = 0;
		}
		if (xlive_local_users[iUser].audio_output_device_name) {
			delete[] xlive_local_users[iUser].audio_output_device_name;
			xlive_local_users[iUser].audio_output_device_name = 0;
		}
	}

	for (ELIGIBLE_NETWORK_INTERFACE* eligibleNetworkInterface : xlive_eligible_network_adapters) {
		delete eligibleNetworkInterface;
	}
	xlive_specific_network_adapter = 0;
	xlive_eligible_network_adapters.clear();

	if (xlive_config_specific_network_adapter_name) {
		delete[] xlive_config_specific_network_adapter_name;
		xlive_config_specific_network_adapter_name = 0;
	}

	if (broadcastAddrInput) {
		delete[] broadcastAddrInput;
		broadcastAddrInput = 0;
	}
	
	if (xlln_direct_ip_connect_password) {
		delete[] xlln_direct_ip_connect_password;
		xlln_direct_ip_connect_password = 0;
	}
	
	if (xlln_direct_ip_connect_ip_port) {
		delete[] xlln_direct_ip_connect_ip_port;
		xlln_direct_ip_connect_ip_port = 0;
	}
	
	return true;
}
