#pragma once
#include "xdefs.hpp"
#include <map>
#include <vector>

extern bool xlive_debug_pause;

typedef struct _LOCAL_USER_INFO {
	XUID xuid = 0;
	char username[XUSER_NAME_SIZE] = {0};
	bool live_enabled = false;
	bool online_enabled = false;
	bool auto_login = false;
	XUSER_SIGNIN_STATE signin_state = eXUserSigninState_NotSignedIn;
	// 0 means not guest.
	uint32_t guest_number = 0;
	// if (guest_number) then this is the index of the local user sponsoring this guest online.
	uint32_t sponsor_user_index = XUSER_INDEX_NONE;
	wchar_t* audio_input_device_name = 0;
	// Range: 0 to XHV_AUDIO_SETTING_VOLUME_MAX_BOOST. 0 being Muted.
	uint16_t audio_input_device_volume = 400;
	// Range: 0 to 100. 0 being always capturing and 100 is about impossible.
	uint8_t audio_input_device_threshold = 10;
	wchar_t* audio_output_device_name = 0;
	// Range: 0 to 100. 0 being Muted.
	uint8_t audio_output_device_volume = 100;
	// Value range: 0 to XHV_AUDIO_SETTING_VOLUME_MAX_BOOST. 0 being Muted.
	std::map<XUID, uint16_t> xhv_voice_remote_volume;
} LOCAL_USER_INFO;

extern CRITICAL_SECTION xlive_critsec_local_user;
extern LOCAL_USER_INFO xlive_local_users[XLIVE_LOCAL_USER_COUNT];

extern CRITICAL_SECTION xlive_critsec_remote_user;
extern std::map<XUID, char*> xlln_remote_user_usernames;

void Check_Overlapped(XOVERLAPPED* xoverlapped);

typedef struct _ELIGIBLE_NETWORK_INTERFACE {
	// Can be null.
	char* name = 0;
	// Can be null.
	wchar_t* description = 0;
	SOCKADDR_STORAGE addressGateway = {AF_UNSPEC};
	SOCKADDR_STORAGE addressUnicast = {AF_UNSPEC};
	uint8_t prefixLength = 0;
	SOCKADDR_STORAGE addressBroadcast = {AF_UNSPEC};
	SOCKADDR_STORAGE addressMulticast = {AF_UNSPEC};
	uint64_t minLinkSpeed;
	bool hasDnsServer;
	
	~_ELIGIBLE_NETWORK_INTERFACE()
	{
		if (name) {
			delete[] name;
			name = 0;
		}
		if (description) {
			delete[] description;
			description = 0;
		}
	}
	
} ELIGIBLE_NETWORK_INTERFACE;

typedef struct _DIRECT_IP_CONNECT {
	uint32_t joinRequestSignature = 0;
	__time64_t timeoutAt = 0;
	uint32_t localPlayerId = 0;
	uint32_t remoteInstanceId = 0;
	XUID remoteXuid = 0;
	uint32_t remoteTitleId = 0;
	XNKID remoteSessionId;
	XNKEY remoteKeyExchangeKey;
} DIRECT_IP_CONNECT;

extern DIRECT_IP_CONNECT xlln_direct_ip_connect;

extern CRITICAL_SECTION xlive_critsec_xfriends_enumerators;
extern std::map<HANDLE, std::vector<XONLINE_FRIEND>> xlive_xfriends_enumerators;

extern uint32_t xlive_title_id;
extern uint32_t xlive_title_version;
extern CRITICAL_SECTION xlive_critsec_network_adapter;
extern char* xlive_init_specific_network_adapter_name;
extern char* xlive_config_specific_network_adapter_name;
extern bool xlive_ignore_title_network_adapter;
extern ELIGIBLE_NETWORK_INTERFACE* xlive_specific_network_adapter;
extern std::vector<ELIGIBLE_NETWORK_INTERFACE*> xlive_eligible_network_adapters;
extern bool xlive_online_initialized;

extern CRITICAL_SECTION xlive_critsec_title_server_enumerators;

int32_t RefreshNetworkAdapters();
void XllnDirectIpConnectCancel();
void XllnDirectIpConnectTo(uint32_t local_player_id, SOCKADDR_STORAGE* remote_sockaddr, const char* remote_password, const XNKID* requested_session = nullptr);
