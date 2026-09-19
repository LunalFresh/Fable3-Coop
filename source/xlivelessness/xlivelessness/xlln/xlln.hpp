#pragma once
#include "../xlive/xdefs.hpp"
#include <stdint.h>
#include <map>
#include <set>
#include <vector>
#include <string>

#define RT_IMAGE L"IMAGE"
#define XLLN_LOGO_COMPACT_SQUARE_ICON L"XLLN_LOGO_COMPACT_SQUARE_ICON"
#define XLLN_LOGO_COMPACT_SQUARE_192_DPI L"XLLN_LOGO_COMPACT_SQUARE_192_DPI"

typedef enum : uint32_t {
	EVENT_DEBUG_TBX_LOG_UPDATE = WM_APP,
	EVENT_DEBUG_TBX_BLACKLIST_REFRESH,
	EVENT_USER_CARD_ACTIONS_UPDATE,
	EVENT_USER_CARD_USER_UPDATE,
	EVENT_USER_CARD_USERS_UPDATE,
	EVENT_USER_CARD_TALKING_UPDATE,
	EVENT_ACHIEVEMENTS_USERS_UPDATE,
	EVENT_STATS_USERS_UPDATE,
	MAIN_MENU_VIEW_USER_CARD,
	MAIN_MENU_SAVE_CONFIG,
	MAIN_MENU_EXIT,
	MAIN_MENU_LOGIN_P1,
	MAIN_MENU_LOGIN_PX_MAX = (MAIN_MENU_LOGIN_P1 - 1) + XLIVE_LOCAL_USER_COUNT,
	MAIN_MENU_DEBUG_DEBUG_LOG,
	MAIN_MENU_DEBUG_SOCKETS,
	MAIN_MENU_DEBUG_CONNECTIONS,
	MAIN_MENU_DEBUG_ACHIEVEMENTS,
	MAIN_MENU_DEBUG_STATS,
	MAIN_MENU_NETWORK_ADAPTER_MENU_FIRST_ITEM, // Required for counting network adapters.
	MAIN_MENU_NETWORK_ADAPTER_IGNORE_TITLE = MAIN_MENU_NETWORK_ADAPTER_MENU_FIRST_ITEM,
	MAIN_MENU_NETWORK_ADAPTER_UNSPECIFIED,
	MAIN_MENU_NETWORK_ADAPTER_REFRESH,
	MAIN_MENU_NETWORK_ADAPTER_SEPARATOR,
	MAIN_MENU_NETWORK_ADAPTERS_FIRST,
	MAIN_MENU_NETWORK_ADAPTERS_LAST = (MAIN_MENU_NETWORK_ADAPTERS_FIRST + 40 - 1), // Allocate space for max 40 network adaptor controls.
	MAIN_MENU_ALWAYS_TOP,
	MAIN_MENU_ABOUT,
	MAIN_TBX_USERNAME,
	MAIN_CMB_MICROPHONE_DEVICE,
	MAIN_TBR_MICROPHONE_VOLUME,
	MAIN_TBR_MICROPHONE_THRESHOLD,
	MAIN_CMB_SPEAKER_DEVICE,
	MAIN_TBR_SPEAKER_VOLUME,
	MAIN_CHK_LIVE_ENABLE,
	MAIN_CHK_ONLINE_ENABLE,
	MAIN_CHK_AUTO_LOGIN,
	MAIN_BTN_LOGIN,
	MAIN_BTN_LOGOUT,
	MAIN_TBX_DIRECT_IP_CONNECT_PASSWORD,
	MAIN_TBX_DIRECT_IP_CONNECT_IP_PORT,
	MAIN_BTN_DIRECT_IP_CONNECT,
	MAIN_TBX_BROADCAST,
	DEBUG_BTN_PAUSE,
	DEBUG_BTN_UNPAUSE,
	DEBUG_CHK_DBG_CTX_XLIVE,
	DEBUG_CHK_DBG_CTX_XLLN,
	DEBUG_CHK_DBG_CTX_XLLN_MOD,
	DEBUG_CHK_DBG_CTX_OTHER,
	DEBUG_CHK_DBG_CTX_TITLE,
	DEBUG_CHK_DBG_LVL_TRACE,
	DEBUG_CHK_DBG_LVL_DEBUG,
	DEBUG_CHK_DBG_LVL_INFO,
	DEBUG_CHK_DBG_LVL_WARN,
	DEBUG_CHK_DBG_LVL_ERROR,
	DEBUG_CHK_DBG_LVL_FATAL,
	DEBUG_BTN_BLACKLIST_UPDATE,
	DEBUG_TBX_BLACKLIST,
	DEBUG_TBX_LOG,
	SOCKETS_LST_SOCKETS,
	CONNS_LST_CONNECTIONS,
	CONNS_BTN_REFRESH_CONNS,
	CONNS_BTN_DELETE_CONNS,
	MSG_BOX_TBX_TITLE,
	MSG_BOX_TBX_DESCRIPTION,
	MSG_BOX_BTN_BTN_0,
	MSG_BOX_BTN_BTN_1,
	MSG_BOX_BTN_BTN_2,
	INPUT_BOX_TBX_TITLE,
	INPUT_BOX_TBX_DESCRIPTION,
	INPUT_BOX_TBX_RESULT,
	INPUT_BOX_BTN_BTN_SUBMIT,
	INPUT_BOX_BTN_BTN_CANCEL,
	USER_CUSTOM_LIST_TBX_TITLE,
	USER_CUSTOM_LIST_TBX_DESCRIPTION,
	USER_CUSTOM_LIST_LST_USERS,
	USER_CARD_CMB_USERS,
	USER_CARD_TBX_XUID,
	USER_CARD_TBX_USERNAME,
	USER_CARD_TBR_VOLUME,
	USER_CARD_CHK_IS_TALKING,
	USER_CARD_BTN_ACTION_0,
	USER_CARD_BTN_ACTION_1,
	USER_CARD_BTN_ACTION_2,
	USER_CARD_BTN_ACTION_MAX = USER_CARD_BTN_ACTION_2,
	ACHIEVEMENTS_LST_ACHIEVEMENTS,
	ACHIEVEMENTS_CMB_USERS,
	STATS_LST_STATS,
	STATS_CMB_USERS,
	STATS_TBX_VALUE,
	STATS_BTN_SET_VALUE,
	STATS_CHK_ONLY_SET,
} XLLNControlsMessageNumbers;

namespace XLLNModifyPropertyTypes {
	const char* const TypeNames[] {
		"UNKNOWN",
		"FPS_LIMIT",
		"LiveOverLan_BROADCAST_HANDLER",
		"RECVFROM_CUSTOM_HANDLER_REGISTER",
		"RECVFROM_CUSTOM_HANDLER_UNREGISTER",
		"BASE_PORT",
		"GUIDE_UI_HANDLER",
		"XHV_ENGINE_ENABLED",
		"CALLBACK_NETWORK_INTERFACE_CHANGED",
		"CALLBACK_NETWORK_SOCKET_BIND",
	};
	typedef enum : uint8_t {
		tUNKNOWN = 0,
		tFPS_LIMIT,
		tLiveOverLan_BROADCAST_HANDLER,
		tRECVFROM_CUSTOM_HANDLER_REGISTER,
		tRECVFROM_CUSTOM_HANDLER_UNREGISTER,
		tBASE_PORT,
		tGUIDE_UI_HANDLER,
		tXHV_ENGINE_ENABLED,
		tCALLBACK_NETWORK_INTERFACE_CHANGED,
		tCALLBACK_NETWORK_SOCKET_BIND,
	} TYPE;
	
	typedef void (WINAPI* XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED)(const char* interface_name, const SOCKADDR_STORAGE* interface_address_gateway, const SOCKADDR_STORAGE* interface_address_unicast, const SOCKADDR_STORAGE* interface_address_broadcast);
	typedef void (WINAPI* XLLN_CALLBACK_NETWORK_SOCKET_BIND)(bool has_binded, int32_t port_protocol, uint16_t port_binded, uint16_t port_requested, int16_t port_base_offset);
	
}

struct BASE_PORT_OFFSET_MAPPING {
	int8_t priority = 0;
	uint8_t offset = 0;
	uint16_t original = 0;
};

#pragma pack(push, 1) // Save then set byte alignment setting.

typedef struct _XLLN_XACHIEVEMENT_DETAILS_COW {
	DWORD dwAchievementId;
	FILETIME ftAchieved;
	DWORD dwFlags;
} XLLN_XACHIEVEMENT_DETAILS_COW, *PXLLN_XACHIEVEMENT_DETAILS_COW;

#pragma pack(pop) // Return to original alignment setting.

extern HINSTANCE xlln_hModule;
extern uint32_t xlln_local_instance_id;
extern uint32_t xlln_global_instance_id;
extern HMENU hMenu_network_adapters;
extern uint32_t xlln_login_player;
extern bool xlln_debug;
extern char* broadcastAddrInput;
extern char* xlln_direct_ip_connect_password;
extern char* xlln_direct_ip_connect_ip_port;
extern CRITICAL_SECTION xlln_critsec_callbacks_network_interface_changed;
extern std::set<XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_INTERFACE_CHANGED> xlln_callbacks_network_interface_changed;
extern CRITICAL_SECTION xlln_critsec_callbacks_network_socket_bind;
extern std::set<XLLNModifyPropertyTypes::XLLN_CALLBACK_NETWORK_SOCKET_BIND> xlln_callbacks_network_socket_bind;
extern std::vector<XACHIEVEMENT_DETAILS> xlive_achievement_details;
struct XLLN_STAT_COLUMN_DEF {
	// Low bits of the property id the title WRITES this stat under (XSessionWriteStats keys the store
	// by this); also what the editor window shows.
	uint32_t columnId;
	uint8_t type;
	// A system-scope property (X_PROPERTY_SCOPE_MASK) - a platform column like Rank or Gamer Name
	// that the game fills in itself, not a title stat. It collides with a title column of the same
	// low id under X_PROPERTY_ID_MASK, so the editor hides it.
	bool isSystem;
	// The field's attribute id, which is the "column id" a title READS this stat by (XUserReadStats).
	// Distinct from columnId for aggregated leaderboard fields, so reads translate attributeId->columnId.
	uint32_t attributeId;
	std::wstring name;
};
struct XLLN_STAT_VIEW_DEF {
	std::wstring name;
	std::vector<XLLN_STAT_COLUMN_DEF> columns;
};
// Per view id (parsed from the title's SPA XVC2 section): the view's display name and its columns,
// each with a column id, XUSER_DATA_TYPE and display name. Read-only after single-threaded init, so
// UI and read paths consult it without locking.
extern std::map<uint32_t, XLLN_STAT_VIEW_DEF> xlive_stat_view_defs;
// How a matchmaking query filter compares its two sides.
typedef enum : uint8_t {
	XLLN_XLAST_COMPARISON_EQUAL,
	XLLN_XLAST_COMPARISON_NOT_EQUAL,
	XLLN_XLAST_COMPARISON_LESS,
	XLLN_XLAST_COMPARISON_LESS_EQUAL,
	XLLN_XLAST_COMPARISON_GREATER,
	XLLN_XLAST_COMPARISON_GREATER_EQUAL,
} XLLN_XLAST_COMPARISON;
// Where a filter gets the value it compares an advertised attribute against.
typedef enum : uint8_t {
	// An attribute id the searching title supplies a value for in the search's context or property array.
	XLLN_XLAST_OPERAND_PARAMETER,
	// An id into the XLAST's matchmaking Constants table.
	XLLN_XLAST_OPERAND_CONSTANT,
	// The value itself, written into the query rather than supplied by the search.
	XLLN_XLAST_OPERAND_CONTEXT_VALUE,
} XLLN_XLAST_OPERAND;
// One rule restricting what a matchmaking query returns. The left side is always an attribute the
// advertising host set on its session; only the right side varies.
struct XLLN_XLAST_QUERY_FILTER {
	uint32_t attributeId;
	XLLN_XLAST_COMPARISON comparison;
	XLLN_XLAST_OPERAND operandType;
	uint32_t operand;
};
// One matchmaking query as the title's XLAST declares it. A search selects a query by passing its id
// as the procedure index, so what a search filters on and returns is fixed here, not by the caller.
struct XLLN_XLAST_QUERY {
	uint32_t maxResults;
	std::vector<XLLN_XLAST_QUERY_FILTER> filters;
	// The attribute ids a result carries, in ordinal order.
	std::vector<uint32_t> returns;
};
// Per matchmaking query id (parsed from the title's SPA XSRC XLAST). Empty when the title ships no
// usable SPA. Read-only after init.
extern std::map<uint32_t, XLLN_XLAST_QUERY> xlive_xlast_queries;

// Looks up an entry of the XLAST's matchmaking Constants table.
bool XllnXlastConstantValue(uint32_t constant_id, double* result_value);
extern uint8_t xlive_locale;

// #41140
uint32_t WINAPI XLLNLogin(uint32_t user_index, BOOL lsb_8_live_enabled_8_online_disabled_msb_16_reserved, uint32_t user_id, const char* user_username);
// #41141
uint32_t WINAPI XLLNLogout(uint32_t user_index);

void InitCriticalSections();
void UninitCriticalSections();
bool InitXLLN(HMODULE hModule);
bool UninitXLLN();
void XllnNetworkInterfaceChanged();
void ParseBroadcastAddrInput(char* jlbuffer);