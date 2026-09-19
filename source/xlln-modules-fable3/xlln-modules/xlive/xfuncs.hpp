#pragma once
#include <stdint.h>
#include "xdefs.hpp"

// #1
int32_t WINAPI XWSAStartup(uint16_t version_requested, WSADATA* wsa_data);
// #2
int32_t WINAPI XWSACleanup();
// #3
SOCKET WINAPI XSocketCreate(int32_t address_family, int32_t type, int32_t protocol);
// #4
int32_t WINAPI XSocketClose(SOCKET title_socket_handle);
// #5
int32_t WINAPI XSocketShutdown(SOCKET title_socket_handle, int32_t how);
// #6
int32_t WINAPI XSocketIOCTLSocket(SOCKET title_socket_handle, int32_t command, unsigned long* command_value);
// #7
int32_t WINAPI XSocketSetSockOpt(SOCKET title_socket_handle, int32_t option_level, int32_t option_name, const char* option_value, int32_t option_size);
// #8
int32_t WINAPI XSocketGetSockOpt(SOCKET title_socket_handle, int32_t option_level, int32_t option_name, char* option_value, int32_t* option_size);
// #9
int32_t WINAPI XSocketGetSockName(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size);
// #10
int32_t WINAPI XSocketGetPeerName(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size);
// #11
SOCKET WINAPI XSocketBind(SOCKET title_socket_handle, const sockaddr* name, int32_t name_size);
// #12
int32_t WINAPI XSocketConnect(SOCKET title_socket_handle, const sockaddr* sock_addr_xlive, int32_t sock_addr_xlive_size);
// #13
int32_t WINAPI XSocketListen(SOCKET title_socket_handle, int32_t backlog);
// #14
SOCKET WINAPI XSocketAccept(SOCKET title_socket_handle, sockaddr* sock_addr_xlive, int32_t* sock_addr_xlive_size);
// #15
int32_t WINAPI XSocketSelect(int32_t title_socket_count, fd_set* title_sockets_read, fd_set* title_sockets_write, fd_set* title_sockets_except, const timeval* select_timeout);
// #16
BOOL WINAPI XWSAGetOverlappedResult(SOCKET title_socket_handle, WSAOVERLAPPED* wsa_overlapped, uint32_t* transferred_size, BOOL wait_for_completion, uint32_t* flags);
// #17
int32_t WINAPI XWSACancelOverlappedIO(SOCKET title_socket_handle);
// #18
int32_t WINAPI XSocketRecv(SOCKET title_socket_handle, char* data_buffer, int32_t data_buffer_size, int32_t flags);
// #19
int32_t WINAPI XWSARecv(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* received_size
	, uint32_t* flags
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
);
// #20
int32_t WINAPI XSocketRecvFrom(SOCKET title_socket_handle, char* data_buffer, int32_t data_buffer_size, int32_t flags, sockaddr* address_from, int32_t* address_from_size);
// #21
int32_t WINAPI XWSARecvFrom(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* received_size
	, uint32_t* flags
	, sockaddr* address_from
	, int32_t* address_from_size
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
);
// #22
int32_t WINAPI XSocketSend(SOCKET title_socket_handle, const char* data_buffer, int32_t data_buffer_size, int32_t flags);
// #23
int32_t WINAPI XWSASend(
	SOCKET title_socket_handle
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* sent_size
	, uint32_t flags
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
);
// #24
int32_t WINAPI XSocketSendTo(SOCKET title_socket_handle, const char* data_buffer, int32_t data_buffer_size, int32_t flags, sockaddr* address_to, int32_t address_to_size);
// #25
int32_t WINAPI XWSASendTo(
	SOCKET title_socket_handle
	// each buffer is essentially its own message afaik. so wrap each one.
	, WSABUF* wsa_buffers
	, uint32_t wsa_buffer_count
	, uint32_t* sent_size
	, uint32_t flags
	, const sockaddr* address_to
	, int32_t address_to_size
	, WSAOVERLAPPED* wsa_overlapped
	, LPWSAOVERLAPPED_COMPLETION_ROUTINE wsa_completion_routine
);
// #26
uint32_t WINAPI XSocketInet_Addr(const char* ipv4_address);
// #27
int32_t WINAPI XSocketWSAGetLastError();
// #28
void WINAPI XWSASetLastError(int32_t error_code);
// #29
WSAEVENT WINAPI XWSACreateEvent();
// #30
BOOL WINAPI XWSACloseEvent(WSAEVENT wsa_event);
// #31
BOOL WINAPI XWSASetEvent(WSAEVENT wsa_event);
// #32
BOOL WINAPI XWSAResetEvent(WSAEVENT wsa_event);
// #33
uint32_t WINAPI XWSAWaitForMultipleEvents(uint32_t event_count, const WSAEVENT* wsa_events, BOOL wait_on_all, uint32_t timeout, BOOL alertable);
// #34
int32_t WINAPI XWSAFDIsSet(SOCKET title_socket_handle, fd_set* title_socket_set);
// #35
int32_t WINAPI XWSAEventSelect(SOCKET title_socket_handle, WSAEVENT wsa_event, uint32_t network_event_flags);
// #37
unsigned long WINAPI XSocketHTONL(unsigned long host_order);
// #38
unsigned short WINAPI XSocketNTOHS(unsigned short network_order);
// #39
unsigned long WINAPI XSocketNTOHL(unsigned long network_order);
// #40
unsigned short WINAPI XSocketHTONS(unsigned short host_order);
// #51
int32_t WINAPI XNetStartup(const XNetStartupParams* xnet_startup_parameters);
// #52
int32_t WINAPI XNetCleanup();
// #53
int32_t WINAPI XNetRandom(uint8_t* buffer, size_t buffer_size);
// #54
int32_t WINAPI XNetCreateKey(XNKID* xnkid, XNKEY* xnkey);
// #55
int32_t WINAPI XNetRegisterKey(const XNKID* xnkid, const XNKEY* xnkey);
// #56
int32_t WINAPI XNetUnregisterKey(const XNKID* xnkid);
// #57
int32_t WINAPI XNetXnAddrToInAddr(XNADDR* xnaddr, XNKID* xnkid, IN_ADDR* in_addr);
// #58
int32_t WINAPI XNetServerToInAddr(const IN_ADDR in_addr_server, uint32_t service_id, IN_ADDR* in_addr);
// #59
int32_t WINAPI XNetTsAddrToInAddr(const TSADDR* ts_addr, uint32_t service_id, const XNKID* xnkid, IN_ADDR* in_addr);
// #60
int32_t WINAPI XNetInAddrToXnAddr(const IN_ADDR in_addr, XNADDR* xnaddr, XNKID* xnkid);
// #61
int32_t WINAPI XNetInAddrToServer(const IN_ADDR in_addr, IN_ADDR* in_addr_server);
// #62
int32_t WINAPI XNetInAddrToString(const IN_ADDR in_addr, char* address, size_t address_size);
// #63
int32_t WINAPI XNetUnregisterInAddr(const IN_ADDR in_addr);
// #64
int32_t WINAPI XNetXnAddrToMachineId(const XNADDR* xnaddr, uint64_t* machine_id);
// #65
int32_t WINAPI XNetConnect(const IN_ADDR in_addr);
// #66
int32_t WINAPI XNetGetConnectStatus(const IN_ADDR in_addr);
// #67
int32_t WINAPI XNetDnsLookup(const char* hostname, WSAEVENT wsa_event, XNDNS** xndns);
// #68
int32_t WINAPI XNetDnsRelease(XNDNS* xndns);
// #69
// Calling XNetUnregisterKey with the session Id / xnkid will release this QoS listener automatically.
// Therefore the session Id needs to be registered via XNetRegisterKey before use here.
int32_t WINAPI XNetQosListen(XNKID* xnkid, uint8_t* data, uint32_t data_size, uint32_t bits_per_second, uint32_t flags);
// #70
uint32_t WINAPI XNetQosLookup(
	size_t lookup_instance_count
	, XNADDR* xnaddrs[]
	, XNKID* xnkids[]
	, XNKEY* xnkeys[]
	, size_t lookup_service_count
	, IN_ADDR in_addrs[]
	, uint32_t service_ids[]
	, uint32_t probe_count
	, uint32_t bits_per_second
	, uint32_t flags
	, WSAEVENT wsa_event
	, XNQOS** xnqos
);
// #71
int32_t WINAPI XNetQosServiceLookup(uint32_t flags, WSAEVENT wsa_event, XNQOS** xnqos);
// #72
int32_t WINAPI XNetQosRelease(XNQOS* xnqos);
// #73
uint32_t WINAPI XNetGetTitleXnAddr(XNADDR* xnaddr);
// #74
uint32_t WINAPI XNetGetDebugXnAddr(XNADDR* xnaddr);
// #75
uint32_t WINAPI XNetGetEthernetLinkStatus();
// #76
uint32_t WINAPI XNetGetBroadcastVersionStatus(BOOL reset_status);
// #77
int32_t WINAPI XNetQosGetListenStats(XNKID* xnkid, XNQOSLISTENSTATS* qos_listen_stats);
// #78
int32_t WINAPI XNetGetOpt(uint32_t option_id, uint8_t* option_value, size_t* option_value_size);
// #79
int32_t WINAPI XNetSetOpt(uint32_t option_id, uint8_t* option_value, size_t* option_value_size);
// #80
int32_t WINAPI XNetStartupEx(const XNetStartupParams* xnet_startup_parameters, uint32_t version_required);
// #81
int32_t WINAPI XNetReplaceKey(const XNKID* xnkid_unregister, const XNKID* pxnkid_new);
// #82
int32_t WINAPI XNetGetXnAddrPlatform(const XNADDR* xnaddr, uint32_t* platform);
// #83
// system_link_port_NBO - network byte (big-endian) order
int32_t WINAPI XNetGetSystemLinkPort(uint16_t* system_link_port_NBO);
// #84
// system_link_port_NBO - network byte (big-endian) order
int32_t WINAPI XNetSetSystemLinkPort(uint16_t system_link_port_NBO);
// #472
void WINAPI XCustomSetAction(uint32_t action_index, const wchar_t* action_text, uint32_t flags);
// #473
BOOL WINAPI XCustomGetLastActionPress(uint32_t* user_index, uint32_t* action_index, XUID* xuid);
// #474
uint32_t WINAPI XCustomSetDynamicActions(uint32_t user_index, XUID xuid, const XCUSTOMACTION* custom_actions, uint16_t custom_action_count);
// #476
uint32_t WINAPI XCustomGetLastActionPressEx(uint32_t* user_index, uint32_t* action_id, XUID* xuid, uint8_t* payload, uint16_t* payload_size);
// #477
void WINAPI XCustomRegisterDynamicActions();
// #478
void WINAPI XCustomUnregisterDynamicActions();
// #479
BOOL WINAPI XCustomGetCurrentGamercard(uint32_t* user_index, XUID* xuid);
// #651
BOOL WINAPI XNotifyGetNext(HANDLE notification_listener, uint32_t notification_id_filter, uint32_t* notification_id, void* notification_value);
// #652
void WINAPI XNotifyPositionUI(uint32_t position_flags);
// #653
uint32_t WINAPI XNotifyDelayUI(uint32_t milliseconds);
// #1082
size_t WINAPI XGetOverlappedExtendedError(XOVERLAPPED* xoverlapped);
// #1083
size_t WINAPI XGetOverlappedResult(XOVERLAPPED* xoverlapped, size_t* result_code, BOOL wait_for_completion);
// #5000
HRESULT WINAPI XLiveInitialize(XLIVE_INITIALIZE_INFO* xlive_initialise_info);
// #5001
HRESULT WINAPI XLiveInput(XLIVE_INPUT_INFO* xlive_input_info);
// #5002
int32_t WINAPI XLiveRender();
// #5003
void WINAPI XLiveUninitialize();
// #5005
HRESULT WINAPI XLiveOnCreateDevice(IUnknown* d3d_device, void* d3d_presentation_parameters);
// #5006
HRESULT WINAPI XLiveOnDestroyDevice();
// #5007
HRESULT WINAPI XLiveOnResetDevice(void* d3d_presentation_parameters);
// #5008
int32_t WINAPI XHVCreateEngine(XHV_INIT_PARAMS* init_params, HANDLE* worker_thread_handle, IXHVEngine** result_xhv_engine);
// #5010: This function is deprecated.
HRESULT WINAPI XLiveRegisterDataSection(const wchar_t* section_name, uint8_t* section_data, uint32_t section_size);
// #5011: This function is deprecated.
HRESULT WINAPI XLiveUnregisterDataSection(const wchar_t* section_name);
// #5012: This function is deprecated.
HRESULT WINAPI XLiveUpdateHashes(uint32_t a1, uint32_t a2);
// #5016
HRESULT WINAPI XLivePBufferAllocate(size_t protected_buffer_size, XLIVE_PROTECTED_BUFFER** protected_buffer);
// #5017
HRESULT WINAPI XLivePBufferFree(XLIVE_PROTECTED_BUFFER* protected_buffer);
// #5018
HRESULT WINAPI XLivePBufferGetByte(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* result_value);
// #5019
HRESULT WINAPI XLivePBufferSetByte(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t value);
// #5020
HRESULT WINAPI XLivePBufferGetDWORD(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint32_t* result_value);
// #5021
HRESULT WINAPI XLivePBufferSetDWORD(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint32_t value);
// #5022
HRESULT WINAPI XLiveGetUpdateInformation(XLIVEUPDATE_INFORMATION* xlive_update_info);
// #5023
HRESULT WINAPI XNetGetCurrentAdapter(char* network_adapter, size_t* network_adapter_size);
// #5024
HRESULT WINAPI XLiveUpdateSystem(const wchar_t* relaunch_cmd_line);
// #5025
HRESULT WINAPI XLiveGetLiveIdError(HRESULT* authentication_state, HRESULT* request_state, wchar_t* complete_auth_web_url, size_t* complete_auth_web_url_size);
// #5026
HRESULT WINAPI XLiveSetSponsorToken(const wchar_t* sponsor_token, uint32_t title_id);
// #5027
HRESULT WINAPI XLiveUninstallTitle(uint32_t title_id);
// #5028
uint32_t WINAPI XLiveLoadLibraryEx(const wchar_t* module_pathname, HINSTANCE* module_handle, uint32_t flags);
// #5029
HRESULT WINAPI XLiveFreeLibrary(HMODULE module_handle);
// #5030
BOOL WINAPI XLivePreTranslateMessage(const MSG* procedure_message);
// #5031
HRESULT WINAPI XLiveSetDebugLevel(XLIVE_DEBUG_LEVEL debug_level_new, XLIVE_DEBUG_LEVEL* debug_level_old);
// #5032
HRESULT WINAPI XLiveVerifyArcadeLicense(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset);
// #5034
HRESULT WINAPI XLiveProtectData(uint8_t* data_unprotected, size_t data_unprotected_size, uint8_t* data_protected, size_t* data_protected_size, HANDLE protected_data_handle);
// #5035
HRESULT WINAPI XLiveUnprotectData(uint8_t* data_protected, size_t data_protected_size, uint8_t* data_unprotected, size_t* data_unprotected_size, HANDLE* protected_data_handle);
// #5036
HRESULT WINAPI XLiveCreateProtectedDataContext(XLIVE_PROTECTED_DATA_INFORMATION* protected_data_info, HANDLE* protected_data_handle);
// #5037
HRESULT WINAPI XLiveQueryProtectedDataInformation(HANDLE protected_data_handle, XLIVE_PROTECTED_DATA_INFORMATION* protected_data_info);
// #5038
HRESULT WINAPI XLiveCloseProtectedDataContext(HANDLE protected_data_handle);
// #5039: This function is deprecated. Use XLiveProtectedVerifyFile.
HRESULT WINAPI XLiveVerifyDataFile(wchar_t* file_pathname);
// #5206
uint32_t WINAPI XShowMessagesUI(uint32_t user_index);
// #5208
uint32_t WINAPI XShowGameInviteUI(uint32_t user_index, const XUID* recipient_xuids, size_t recipient_xuid_count, void* reserved);
// #5209
uint32_t WINAPI XShowMessageComposeUI(uint32_t user_index, const XUID* recipient_xuids, size_t recipient_xuid_count, const wchar_t* message_text);
// #5210
uint32_t WINAPI XShowFriendRequestUI(uint32_t user_index, XUID xuid_user);
// #5212
uint32_t WINAPI XShowCustomPlayerListUI(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* title_text
	, const wchar_t* description_text
	, const uint8_t* image_data
	, size_t image_data_size
	, const XPLAYERLIST_USER* list_players
	, size_t list_player_count
	, const XPLAYERLIST_BUTTON* x_button_action
	, const XPLAYERLIST_BUTTON* y_button_action
	, XPLAYERLIST_RESULT* result_player_list_action
	, XOVERLAPPED* xoverlapped
);
// #5214
uint32_t WINAPI XShowPlayerReviewUI(uint32_t user_index, XUID xuid_feedback_target);
// #5215
uint32_t WINAPI XShowGuideUI(uint32_t user_index);
// #5216
uint32_t WINAPI XShowKeyboardUI(
	uint32_t user_index
	, uint32_t flags
	, const wchar_t* default_text
	, const wchar_t* title_text
	, const wchar_t* description_text
	, wchar_t* result_text
	, size_t result_text_size
	, XOVERLAPPED* xoverlapped
);
// #5218
uint32_t WINAPI XShowArcadeUI(uint32_t user_index);
// #5230
HRESULT WINAPI XLocatorServerAdvertise(
	uint32_t user_index
	, uint32_t server_type
	, XNKID xnkid
	, XNKEY xnkey
	, uint32_t max_public_slots
	, uint32_t max_private_slots
	, uint32_t filled_public_slots
	, uint32_t filled_private_slots
	, size_t server_property_count
	, XUSER_PROPERTY* server_properties
	, XOVERLAPPED* xoverlapped
);
// #5231
HRESULT WINAPI XLocatorServerUnAdvertise(uint32_t user_index, XOVERLAPPED* xoverlapped);
// #5233
HRESULT WINAPI XLocatorGetServiceProperty(uint32_t user_index, size_t server_property_count, XUSER_PROPERTY* server_properties, XOVERLAPPED* xoverlapped);
// #5234
uint32_t WINAPI XLocatorCreateServerEnumerator(
	uint32_t user_index
	, size_t enumeration_item_count
	, size_t required_property_id_count
	, const uint32_t* required_property_ids
	, size_t filter_group_count
	, const XLOCATOR_FILTER_GROUP* filter_groups
	, size_t sorter_count
	, const XLOCATOR_SORTER* sorters
	, size_t* result_buffer_size
	, HANDLE* enumerator_handle
);
// #5235
uint32_t WINAPI XLocatorCreateServerEnumeratorByIDs(
	uint32_t user_index
	, size_t enumeration_item_count
	, size_t required_property_id_count
	, const uint32_t* required_property_ids
	, size_t server_id_count
	, const uint64_t* server_ids
	, size_t* result_buffer_size
	, HANDLE* enumerator_handle
);
// #5236
HRESULT WINAPI XLocatorServiceInitialize(XLOCATOR_INIT_INFO* init_info, HANDLE* xlocator_service_handle);
// #5237
HRESULT WINAPI XLocatorServiceUnInitialize(HANDLE xlocator_service_handle);
// #5238
HRESULT WINAPI XLocatorCreateKey(XNKID* xnkid, XNKEY* xnkey);
// #5250
uint32_t WINAPI XShowAchievementsUI(uint32_t user_index);
// #5251
BOOL WINAPI XCloseHandle(HANDLE object_handle);
// #5252
uint32_t WINAPI XShowGamerCardUI(uint32_t user_index, XUID xuid_player);
// #5254
uint32_t WINAPI XCancelOverlapped(XOVERLAPPED* xoverlapped);
// #5255
uint32_t WINAPI XEnumerateBack(HANDLE enumerator_handle, void* data_buffer, size_t data_buffer_size, size_t* result_item_count, XOVERLAPPED* xoverlapped);
// #5256
uint32_t WINAPI XEnumerate(HANDLE enumerator_handle, void* data_buffer, size_t data_buffer_size, size_t* result_item_count, XOVERLAPPED* xoverlapped);
// #5257
HRESULT WINAPI XLiveManageCredentials(const wchar_t* live_id_name, const wchar_t* live_id_password, uint32_t flags, XOVERLAPPED* xoverlapped);
// #5258
HRESULT WINAPI XLiveSignout(XOVERLAPPED* xoverlapped);
// #5259
HRESULT WINAPI XLiveSignin(wchar_t* live_id_name, wchar_t* live_id_password, uint32_t flags, XOVERLAPPED* xoverlapped);
// #5260
// pane_count - Number of users to sign in.
uint32_t WINAPI XShowSigninUI(uint32_t pane_count, uint32_t flags);
// #5261
uint32_t WINAPI XUserGetXUID(uint32_t user_index, XUID* xuid);
// #5262
XUSER_SIGNIN_STATE WINAPI XUserGetSigninState(uint32_t user_index);
// #5263
uint32_t WINAPI XUserGetName(uint32_t user_index, char* result_username, size_t result_username_size);
// #5264
uint32_t WINAPI XUserAreUsersFriends(uint32_t user_index, XUID* user_xuids, size_t user_xuid_count, BOOL* result_are_all_friends, XOVERLAPPED* xoverlapped);
// #5265
uint32_t WINAPI XUserCheckPrivilege(uint32_t user_index, XPRIVILEGE_TYPE privilege_type, BOOL* result_is_privileged);
// #5266
uint32_t WINAPI XShowMessageBoxUI(
	uint32_t user_index
	, const wchar_t* title_text
	, const wchar_t* message_text
	, uint32_t button_count
	, const wchar_t** button_texts
	, uint32_t focused_button_index
	, uint32_t flags
	, MESSAGEBOX_RESULT* result_message_box_action
	, XOVERLAPPED* xoverlapped
);
// #5267
uint32_t WINAPI XUserGetSigninInfo(uint32_t user_index, uint32_t flags, XUSER_SIGNIN_INFO* result_signin_info);
// #5270: Requires XNotifyGetNext to process the listener.
HANDLE WINAPI XNotifyCreateListener(uint64_t notification_area);
// #5271
uint32_t WINAPI XShowPlayersUI(uint32_t user_index);
// #5273
uint32_t WINAPI XUserReadGamerPictureByKey(const XUSER_DATA* gamercard_picture_key, BOOL small_picture, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped);
// #5274
uint32_t WINAPI XUserAwardGamerPicture(uint32_t user_index, uint32_t picture_id, void* reserved, XOVERLAPPED* xoverlapped);
// #5275
uint32_t WINAPI XShowFriendsUI(uint32_t user_index);
// #5276
void WINAPI XUserSetProperty(uint32_t user_index, uint32_t property_id, size_t property_value_buffer_size, const void* property_value_buffer);
// #5277
void WINAPI XUserSetContext(uint32_t user_index, uint32_t context_id, uint32_t context_value);
// #5278
uint32_t WINAPI XUserWriteAchievements(size_t achievement_count, const XUSER_ACHIEVEMENT* achievements, XOVERLAPPED* xoverlapped);
// #5279
uint32_t WINAPI XUserReadAchievementPicture(uint32_t user_index, uint32_t title_id, uint32_t image_id, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped);
// #5280
uint32_t WINAPI XUserCreateAchievementEnumerator(uint32_t title_id, uint32_t user_index, XUID xuid, uint32_t achievement_detail_flags, size_t starting_index, size_t result_item_count, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5281
uint32_t WINAPI XUserReadStats(uint32_t title_id, size_t user_xuid_count, const XUID* user_xuids, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, XUSER_STATS_READ_RESULTS* result_stats, XOVERLAPPED* xoverlapped);
// #5282
uint32_t WINAPI XUserReadGamerPicture(uint32_t user_index, BOOL small_picture, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped);
// #5284
uint32_t WINAPI XUserCreateStatsEnumeratorByRank(uint32_t title_id, uint32_t enumeration_rank_start, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5285
uint32_t WINAPI XUserCreateStatsEnumeratorByRating(uint32_t title_id, uint64_t enumeration_rating, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5286
uint32_t WINAPI XUserCreateStatsEnumeratorByXuid(uint32_t title_id, XUID enumeration_xuid_pivot, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5287
uint32_t WINAPI XUserResetStatsView(uint32_t user_index, uint32_t view_id, XOVERLAPPED* xoverlapped);
// #5288
uint32_t WINAPI XUserGetProperty(uint32_t user_index, size_t* result_property_buffer_size, XUSER_PROPERTY* result_property_buffer, XOVERLAPPED* xoverlapped);
// #5289
uint32_t WINAPI XUserGetContext(uint32_t user_index, XUSER_CONTEXT* result_context, XOVERLAPPED* xoverlapped);
// #5290
float WINAPI XUserGetReputationStars(float gamer_rating);
// #5291
uint32_t WINAPI XUserResetStatsViewAllUsers(uint32_t view_id, XOVERLAPPED* xoverlapped);
// #5292
uint32_t WINAPI XUserSetContextEx(uint32_t user_index, uint32_t context_id, uint32_t context_value, XOVERLAPPED* xoverlapped);
// #5293
uint32_t WINAPI XUserSetPropertyEx(uint32_t user_index, uint32_t property_id, size_t property_value_buffer_size, const void* property_value_buffer, XOVERLAPPED* xoverlapped);
// #5294
HRESULT WINAPI XLivePBufferGetByteArray(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* result_data, size_t result_data_size);
// #5295
HRESULT WINAPI XLivePBufferSetByteArray(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* data, size_t data_size);
// #5296
// assuming online_port_NBO - network byte (big-endian) order
HRESULT WINAPI XLiveGetLocalOnlinePort(uint16_t* online_port_NBO);
// #5297
HRESULT WINAPI XLiveInitializeEx(XLIVE_INITIALIZE_INFO* xlive_initialise_info, uint32_t title_xlive_version);
// #5298
uint32_t WINAPI XLiveGetGuideKey(XINPUT_KEYSTROKE* input_keystroke);
// #5299
uint32_t WINAPI XShowGuideKeyRemapUI(uint32_t user_index);
// #5300
uint32_t WINAPI XSessionCreate(uint32_t create_flags, uint32_t user_index, size_t max_public_slots, size_t max_private_slots, uint64_t* session_nonce, XSESSION_INFO* session_info, XOVERLAPPED* xoverlapped, HANDLE* session_handle);
// #5303
uint32_t WINAPI XStringVerify(uint32_t flags, const char* locale, size_t string_data_count, const STRING_DATA* string_data, size_t result_response_size, STRING_VERIFY_RESPONSE* result_response, XOVERLAPPED* xoverlapped);
// #5304
uint32_t WINAPI XStorageUploadFromMemoryGetProgress(XOVERLAPPED* xoverlapped, uint32_t* result_percent_complete, uint64_t* result_numerator, uint64_t* result_denominator);
// #5305
uint32_t WINAPI XStorageUploadFromMemory(uint32_t user_index, const wchar_t* server_path, size_t data_buffer_size, const uint8_t* data_buffer, XOVERLAPPED* xoverlapped);
// #5306
uint32_t WINAPI XStorageEnumerate(
	uint32_t user_index
	, const wchar_t* server_path
	, size_t starting_index
	, size_t max_result_count
	, size_t result_info_size
	, XSTORAGE_ENUMERATE_RESULTS* result_info
	, XOVERLAPPED* xoverlapped
);
// #5307
uint32_t WINAPI XStorageDownloadToMemoryGetProgress(XOVERLAPPED* xoverlapped, uint32_t* result_percent_complete, uint64_t* result_numerator, uint64_t* result_denominator);
// #5308
uint32_t WINAPI XStorageDelete(uint32_t user_index, const wchar_t* server_path, XOVERLAPPED* xoverlapped);
// #5309
uint32_t WINAPI XStorageBuildServerPathByXuid(
	XUID user_xuid
	, XSTORAGE_FACILITY xstorage_facility
	, const void* xstorage_facility_info
	, size_t xstorage_facility_info_size
	, const wchar_t* item_name
	, wchar_t* result_server_path
	, size_t* result_server_path_size
);
// #5310
uint32_t WINAPI XOnlineStartup();
// #5311
uint32_t WINAPI XOnlineCleanup();
// #5312
uint32_t WINAPI XFriendsCreateEnumerator(uint32_t user_index, size_t starting_index, size_t returned_friends_count, size_t* buffer_size, HANDLE* enumerator_handle);
// #5313
uint32_t WINAPI XPresenceInitialize(size_t peer_subscriptions_max_count);
// #5314
int32_t WINAPI XUserMuteListQuery(uint32_t user_index, XUID xuid_remote_talker, BOOL* result_is_muted);
// #5315
uint32_t WINAPI XInviteGetAcceptedInfo(uint32_t user_index, XINVITE_INFO* xinvite_info);
// #5316
uint32_t WINAPI XInviteSend(uint32_t user_index, size_t invite_xuid_count, const XUID* invite_xuids, const wchar_t* invite_message, XOVERLAPPED* xoverlapped);
// #5317
uint32_t WINAPI XSessionWriteStats(HANDLE session_handle, XUID user_xuid, size_t session_view_count, const XSESSION_VIEW_PROPERTIES* session_views, XOVERLAPPED* xoverlapped);
// #5318
uint32_t WINAPI XSessionStart(HANDLE session_handle, uint32_t start_flags, XOVERLAPPED* xoverlapped);
// #5319
uint32_t WINAPI XSessionSearchEx(
	uint32_t procedure_index
	, uint32_t user_index
	, size_t result_count
	, uint32_t local_user_count
	, uint16_t search_property_count
	, uint16_t search_context_count
	, XUSER_PROPERTY* search_properties
	, XUSER_CONTEXT* search_contexts
	, size_t* result_buffer_size
	, XSESSION_SEARCHRESULT_HEADER* result_search_sessions
	, XOVERLAPPED* xoverlapped
);
// #5320
uint32_t WINAPI XSessionSearchByID(XNKID session_id, uint32_t user_index, size_t* result_buffer_size, XSESSION_SEARCHRESULT_HEADER* result_search_sessions, XOVERLAPPED* xoverlapped);
// #5321
uint32_t WINAPI XSessionSearch(
	uint32_t procedure_index
	, uint32_t user_index
	, size_t result_count
	, uint16_t search_property_count
	, uint16_t search_context_count
	, XUSER_PROPERTY* search_properties
	, XUSER_CONTEXT* search_contexts
	, size_t* result_buffer_size
	, XSESSION_SEARCHRESULT_HEADER* result_search_sessions
	, XOVERLAPPED* xoverlapped
);
// #5322
uint32_t WINAPI XSessionModify(HANDLE session_handle, uint32_t modify_flags, size_t max_public_slots, size_t max_private_slots, XOVERLAPPED* xoverlapped);
// #5323
uint32_t WINAPI XSessionMigrateHost(HANDLE session_handle, uint32_t user_index, XSESSION_INFO* session_info, XOVERLAPPED* xoverlapped);
// #5324
XONLINE_NAT_TYPE WINAPI XOnlineGetNatType();
// #5325
uint32_t WINAPI XSessionLeaveLocal(HANDLE session_handle, uint32_t local_user_index_count, const uint32_t* local_user_indexes, XOVERLAPPED* xoverlapped);
// #5326
uint32_t WINAPI XSessionJoinRemote(HANDLE session_handle, size_t user_count, const XUID* user_xuids, const BOOL* user_private_slots, XOVERLAPPED* xoverlapped);
// #5327
uint32_t WINAPI XSessionJoinLocal(HANDLE session_handle, uint32_t local_user_count, const uint32_t* local_user_indexes, const BOOL* user_private_slots, XOVERLAPPED* xoverlapped);
// #5328
uint32_t WINAPI XSessionGetDetails(HANDLE session_handle, size_t* result_buffer_size, XSESSION_LOCAL_DETAILS* result_session_details, XOVERLAPPED* xoverlapped);
// #5329
uint32_t WINAPI XSessionFlushStats(HANDLE session_handle, XOVERLAPPED* xoverlapped);
// #5330
uint32_t WINAPI XSessionDelete(HANDLE session_handle, XOVERLAPPED* xoverlapped);
// #5331
uint32_t WINAPI XUserReadProfileSettings(
	uint32_t title_id
	, uint32_t user_index
	, size_t setting_id_count
	, const uint32_t* setting_ids
	, size_t* result_buffer_size
	, XUSER_READ_PROFILE_SETTING_RESULT* result_profile_setting
	, XOVERLAPPED* xoverlapped
);
// #5332
uint32_t WINAPI XSessionEnd(HANDLE session_handle, XOVERLAPPED* xoverlapped);
// #5333
uint32_t WINAPI XSessionArbitrationRegister(HANDLE session_handle, uint32_t arbitration_flags, uint64_t session_nonce, size_t* result_buffer_size, XSESSION_REGISTRATION_RESULTS* result_registration, XOVERLAPPED* xoverlapped);
// #5334
uint32_t WINAPI XOnlineGetServiceInfo(uint32_t service_id, XONLINE_SERVICE_INFO* service_info);
// #5335
uint32_t WINAPI XTitleServerCreateEnumerator(const char* server_info, size_t result_item_count, size_t* buffer_size, HANDLE* enumerator_handle);
// #5336
uint32_t WINAPI XSessionLeaveRemote(HANDLE session_handle, size_t user_xuid_count, const XUID* user_xuids, XOVERLAPPED* xoverlapped);
// #5337
uint32_t WINAPI XUserWriteProfileSettings(uint32_t user_index, size_t profile_setting_count, const XUSER_PROFILE_SETTING* profile_settings, XOVERLAPPED* xoverlapped);
// #5338
uint32_t WINAPI XPresenceSubscribe(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids);
// #5339
uint32_t WINAPI XUserReadProfileSettingsByXuid(
	uint32_t title_id
	, uint32_t user_index_requester
	, size_t read_for_num_of_xuids
	, const XUID* read_for_xuids
	, size_t setting_id_count
	, const uint32_t* setting_ids
	, size_t* result_buffer_size
	, XUSER_READ_PROFILE_SETTING_RESULT* result_profile_setting
	, XOVERLAPPED* xoverlapped
);
// #5340
uint32_t WINAPI XPresenceCreateEnumerator(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids, size_t starting_index, size_t result_peer_count, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5341
uint32_t WINAPI XPresenceUnsubscribe(uint32_t user_index, size_t peer_xuid_count, const XUID* peer_xuids);
// #5342
uint32_t WINAPI XSessionModifySkill(HANDLE session_handle, size_t user_xuid_count, const XUID* user_xuids, XOVERLAPPED* xoverlapped);
// #5343
uint32_t WINAPI XSessionCalculateSkill(size_t skill_count, double* skill_mus, double* skill_sigmas, double* skill_aggregate_mus, double* skill_aggregate_sigmas);
// #5344
uint32_t WINAPI XStorageBuildServerPath(
	uint32_t user_index
	, XSTORAGE_FACILITY xstorage_facility
	, const void* xstorage_facility_info
	, size_t xstorage_facility_info_size
	, const wchar_t* item_name
	, wchar_t* result_server_path
	, size_t* result_server_path_size
);
// #5345
uint32_t WINAPI XStorageDownloadToMemory(
	uint32_t user_index
	, const wchar_t* server_path
	, size_t result_data_buffer_size
	, const uint8_t* result_data_buffer
	, size_t result_info_size
	, XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS* result_info
	, XOVERLAPPED* xoverlapped
);
// #5346
uint32_t WINAPI XUserEstimateRankForRating(size_t rank_request_count, const XUSER_RANK_REQUEST* rank_requests, size_t result_estimate_rank_size, XUSER_ESTIMATE_RANK_RESULTS* result_estimate_rank, XOVERLAPPED* xoverlapped);
// #5347
HRESULT WINAPI XLiveProtectedLoadLibrary(HANDLE content_access_handle, void* reserved, wchar_t* module_pathname, uint32_t load_library_flags, HMODULE* result_module);
// #5348
HRESULT WINAPI XLiveProtectedCreateFile(
	HANDLE content_access_handle
	, void* reserved
	, wchar_t* file_pathname
	, uint32_t desired_access
	, uint32_t share_mode
	, SECURITY_ATTRIBUTES* security_attributes
	, uint32_t creation_disposition
	, uint32_t flags_and_attributes
	, HANDLE* result_file_handle
);
// #5349
HRESULT WINAPI XLiveProtectedVerifyFile(HANDLE content_access_handle, void* reserved, wchar_t* file_pathname);
// #5350
HRESULT WINAPI XLiveContentCreateAccessHandle(uint32_t user_index, XLIVE_CONTENT_INFO* content_info, uint32_t license_info_version, XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, HANDLE* content_access_handle, XOVERLAPPED* xoverlapped);
// #5351
HRESULT WINAPI XLiveContentInstallPackage(XLIVE_CONTENT_INFO* content_info, const wchar_t* cab_file_pathname, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS* callback_install_params);
// #5352
HRESULT WINAPI XLiveContentUninstall(XLIVE_CONTENT_INFO* content_info, XUID* uninstall_for_xuid, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS* callback_install_params);
// #5354
HRESULT WINAPI XLiveContentVerifyInstalledPackage(XLIVE_CONTENT_INFO* content_info, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS* callback_install_params);
// #5355
HRESULT WINAPI XLiveContentGetPath(uint32_t user_index, XLIVE_CONTENT_INFO* content_info, wchar_t* result_content_pathname, size_t* result_content_pathname_size);
// #5356
HRESULT WINAPI XLiveContentGetDisplayName(uint32_t user_index, XLIVE_CONTENT_INFO* content_info, wchar_t* result_display_name, size_t* result_display_name_size);
// #5357
HRESULT WINAPI XLiveContentGetThumbnail(uint32_t user_index, XLIVE_CONTENT_INFO* content_info, uint8_t* result_thumbnail, size_t* result_thumbnail_size);
// #5358
HRESULT WINAPI XLiveContentInstallLicense(XLIVE_CONTENT_INFO* content_info, const wchar_t* license_file_pathname, XLIVE_CONTENT_INSTALL_CALLBACK_PARAMS* callback_install_params);
// #5359
HRESULT WINAPI XLiveGetUPnPState(XONLINE_NAT_TYPE* nat_type);
// #5360
uint32_t WINAPI XLiveContentCreateEnumerator(size_t item_count, XLIVE_CONTENT_RETRIEVAL_INFO* content_retrieval_info, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5361
HRESULT WINAPI XLiveContentRetrieveOffersByDate(uint32_t user_index, uint32_t offer_info_version, SYSTEMTIME* offers_start_date, XLIVE_OFFER_INFO* result_offer_infos, size_t* result_offer_info_count, XOVERLAPPED* xoverlapped);
// #5362
BOOL WINAPI XLiveMarketplaceDoesContentIdMatch(const uint8_t* content_id, const XLIVE_CONTENT_INFO* content_data);
// #5363
HRESULT WINAPI XLiveContentGetLicensePath(uint32_t user_index, XLIVE_CONTENT_INFO* content_info, wchar_t* result_license_pathname, size_t* result_license_pathname_size);
// #5365
uint32_t WINAPI XShowMarketplaceUI(uint32_t user_index, uint32_t entry_point_flag, uint64_t offer_id, uint32_t content_category_flags);
// #5366
uint32_t WINAPI XShowMarketplaceDownloadItemsUI(uint32_t user_index, uint32_t entry_point_flag, const uint64_t* offer_ids, size_t offer_id_count, HRESULT* result_status, XOVERLAPPED* xoverlapped);
// #5367
uint32_t WINAPI XContentGetMarketplaceCounts(uint32_t user_index, uint32_t content_categories, size_t result_content_offers_size, XOFFERING_CONTENTAVAILABLE_RESULT* result_content_offers, XOVERLAPPED* xoverlapped);
// #5370
uint32_t WINAPI XMarketplaceConsumeAssets(uint32_t user_index, size_t consumable_asset_count, const XMARKETPLACE_ASSET* consumable_assets, XOVERLAPPED* xoverlapped);
// #5371
uint32_t WINAPI XMarketplaceCreateAssetEnumerator(uint32_t user_index, size_t asset_count, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5372
uint32_t WINAPI XMarketplaceCreateOfferEnumerator(uint32_t user_index, uint32_t offer_type, uint32_t content_categories, size_t offer_count, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5374
uint32_t WINAPI XMarketplaceGetDownloadStatus(uint32_t user_index, uint64_t offer_id, uint32_t* result_download_status);
// #5375
void WINAPI XMarketplaceGetImageUrl(uint32_t title_id, uint64_t offer_id, size_t result_image_url_size, wchar_t* result_image_url);
// #5376
uint32_t WINAPI XMarketplaceCreateOfferEnumeratorByOffering(uint32_t user_index, size_t offer_count, const uint64_t* offer_ids, uint16_t offer_id_count, size_t* result_buffer_size, HANDLE* enumerator_handle);
// #5377
uint32_t WINAPI XUserFindUsers(XUID user_xuid_requester, size_t find_user_count, const FIND_USER_INFO* find_users, size_t result_find_users_size, FIND_USERS_RESPONSE* result_find_users, XOVERLAPPED* xoverlapped);
