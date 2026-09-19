#include <winsock2.h>
#include "../xlive/xdefs.hpp"
#include "xlln-config.hpp"
#include "./debug-log.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-debug-log.hpp"
#include "../xlln/xlln-network.hpp"
#include "../resource.h"
#include "../xlive/xrender.hpp"
#include "../xlive/xsocket.hpp"
#include "../xlive/xlive.hpp"
#include "../xlive/xhv-engine.hpp"
#include <stdio.h>
#include <vector>
// Link with iphlpapi.lib
#include <iphlpapi.h>

wchar_t* xlln_file_config_path = 0;

static const char XllnConfigHeader[] = "XLLN-Config-Version";
static const char XllnConfigVersion[] = DLL_VERSION_STR;

typedef struct _INTERPRET_CONFIG_CONTEXT {
	bool keepInvalidLines = false;
	bool saveValuesRead = true;
	uint32_t readConfigFromOtherInstance = 0;
	std::vector<char*> badConfigEntries;
	std::vector<char*> otherConfigHeaderEntries;
	std::vector<char*> readSettings;
} INTERPRET_CONFIG_CONTEXT;

static int interpretConfigSetting(const char* fileLine, const char* version, size_t lineNumber, void* interpretationContext)
{
	if (!version) {
		return 0;
	}
	INTERPRET_CONFIG_CONTEXT &configContext = *(INTERPRET_CONFIG_CONTEXT*)interpretationContext;
	
	bool unrecognised = false;
	bool duplicated = false;
	bool incorrect = false;
	size_t fileLineLen = strlen(fileLine);
	
	if (fileLine[0] == '#' || fileLine[0] == ';' || fileLineLen <= 2) {
		unrecognised = true;
	}
	// When the version of the header has changed.
	else if (version[-1] != 0) {
		unrecognised = true;
	}
	else {
		const char* equals = strchr(fileLine, '=');
		
		if (!equals || equals == fileLine) {
			unrecognised = true;
		}
		else {
			size_t iValueSearch = 0;
			const char* value = equals + 1;
			while (*value == '\t' || *value == ' ') {
				value++;
			}
			
			const char* whitespace = equals;
			while (&whitespace[-1] != fileLine && (whitespace[-1] == '\t' || whitespace[-1] == ' ')) {
				whitespace--;
			}
			size_t settingStrLen = whitespace - fileLine;
			
			char* settingName = new char[settingStrLen + 1];
			memcpy(settingName, fileLine, settingStrLen);
			settingName[settingStrLen] = 0;
			
			for (char* &readSettingName : configContext.readSettings) {
				if (_stricmp(readSettingName, settingName) == 0) {
					duplicated = true;
					break;
				}
			}
			
#define SettingNameMatches(x) _stricmp(x, settingName) == 0
			
			if (!unrecognised && !duplicated && !incorrect) {
				
				if (SettingNameMatches("xlln_debug_log_trace_blacklist")) {
					if (configContext.saveValuesRead) {
						XllnParseDebugLogTraceBlacklist(value);
						
						PostMessageW(xlln_hwnd_debug_log, XLLNControlsMessageNumbers::EVENT_DEBUG_TBX_BLACKLIST_REFRESH, 0, 0);
					}
				}
				else if (SettingNameMatches("xlln_debug_log_level")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "0x%x", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlln_debug_log_level = tempuint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_hotkey_id_guide")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "0x%hx", &tempuint16) == 1) {
						if (configContext.saveValuesRead) {
							xlive_hotkey_id_guide = tempuint16;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_fps_limit")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							SetFPSLimit(tempuint32);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlln_network_instance_base_port")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "%hu", &tempuint16) == 1 && tempuint16 > 0) {
						if (configContext.saveValuesRead) {
							xlln_network_instance_base_port = tempuint16;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlln_network_instance_port")) {
					uint16_t tempuint16;
					if (sscanf_s(value, "%hu", &tempuint16) == 1) {
						if (configContext.saveValuesRead) {
							if (configContext.readConfigFromOtherInstance) {
								// Get the other instance's base port then add our id offset to it.
								xlln_network_instance_port = tempuint16 - configContext.readConfigFromOtherInstance + xlln_local_instance_id;
							}
							else {
								xlln_network_instance_port = tempuint16;
							}
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_net_disable")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead && !xlive_netsocket_abort) {
							xlive_netsocket_abort = (tempuint32 > 0);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_xhv_engine_enabled")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_xhv_engine_enabled = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_xhv_engine_voice_encoded_bitrate")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1 && tempuint32 > 0) {
						if (configContext.saveValuesRead) {
							xlive_xhv_engine_voice_encoded_bitrate = tempuint32;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_specific_network_adapter")) {
					size_t adapterNameLen = strlen(value);
					if (adapterNameLen == 0) {
						if (configContext.saveValuesRead) {
							if (xlive_config_specific_network_adapter_name) {
								delete[] xlive_config_specific_network_adapter_name;
								xlive_config_specific_network_adapter_name = NULL;
							}
						}
					}
					else if (adapterNameLen > 0 && adapterNameLen <= MAX_ADAPTER_NAME_LENGTH) {
						if (configContext.saveValuesRead) {
							if (xlive_config_specific_network_adapter_name) {
								delete[] xlive_config_specific_network_adapter_name;
							}
							xlive_config_specific_network_adapter_name = CloneString(value);
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_ignore_title_network_adapter")) {
					uint32_t tempuint32;
					if (sscanf_s(value, "%u", &tempuint32) == 1) {
						if (configContext.saveValuesRead) {
							xlive_ignore_title_network_adapter = tempuint32 > 0;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (SettingNameMatches("xlive_broadcast_address")) {
					if (configContext.saveValuesRead) {
						// TODO ParseBroadcastAddrInput(temp);
						// do this later when config saving can be done dynamically (also need to update UI).
						if (broadcastAddrInput) {
							delete[] broadcastAddrInput;
						}
						broadcastAddrInput = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlln_direct_ip_connect_password")) {
					if (configContext.saveValuesRead) {
						if (xlln_direct_ip_connect_password) {
							delete[] xlln_direct_ip_connect_password;
						}
						xlln_direct_ip_connect_password = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlln_direct_ip_connect_ip_port")) {
					if (configContext.saveValuesRead) {
						if (xlln_direct_ip_connect_ip_port) {
							delete[] xlln_direct_ip_connect_ip_port;
						}
						xlln_direct_ip_connect_ip_port = CloneString(value);
					}
				}
				else if (SettingNameMatches("xlive_locale")) {
					uint8_t tempuint8;
					if (sscanf_s(value, "%hhu", &tempuint8) == 1 && tempuint8 >= XLANGUAGE_ENGLISH && tempuint8 <= XLANGUAGE_RUSSIAN) {
						if (configContext.saveValuesRead) {
							xlive_locale = tempuint8;
						}
					}
					else {
						incorrect = true;
					}
				}
				else if (_strnicmp("xlive_username_p", settingName, 16) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[16], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						char* username = CloneString(value);
						if (TrimRemoveConsecutiveSpaces(username) <= XUSER_MAX_NAME_LENGTH) {
							if (configContext.saveValuesRead) {
								strcpy_s(xlive_local_users[iUser].username, sizeof(xlive_local_users[iUser].username), username);
							}
						}
						else {
							incorrect = true;
						}
						delete[] username;
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_live_enabled_p", settingName, 25) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[25], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								bool enabled = (tempuint32 > 0);
								xlive_local_users[iUser].live_enabled = enabled;
								if (!enabled) {
									xlive_local_users[iUser].online_enabled = enabled;
								}
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_online_enabled_p", settingName, 27) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[27], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								bool enabled = (tempuint32 > 0);
								xlive_local_users[iUser].online_enabled = enabled;
								if (enabled) {
									xlive_local_users[iUser].live_enabled = enabled;
								}
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_auto_login_p", settingName, 23) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[23], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint32_t tempuint32;
						if (sscanf_s(value, "%u", &tempuint32) == 1) {
							if (configContext.saveValuesRead) {
								xlive_local_users[iUser].auto_login = (tempuint32 > 0);
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_audio_input_device_name_p", settingName, 36) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[36], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						if (configContext.saveValuesRead) {
							if (xlive_local_users[iUser].audio_input_device_name) {
								delete[] xlive_local_users[iUser].audio_input_device_name;
								xlive_local_users[iUser].audio_input_device_name = 0;
							}
							size_t valueSize = strlen(value);
							if (valueSize) {
								valueSize++;
								xlive_local_users[iUser].audio_input_device_name = new wchar_t[valueSize];
								swprintf_s(xlive_local_users[iUser].audio_input_device_name, valueSize, L"%hs", value);
							}
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_audio_input_device_volume_p", settingName, 38) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[38], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint16_t tempuint16;
						if (sscanf_s(value, "%hu", &tempuint16) == 1 && tempuint16 <= XHV_AUDIO_SETTING_VOLUME_MAX_BOOST) {
							if (configContext.saveValuesRead) {
								xlive_local_users[iUser].audio_input_device_volume = tempuint16;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_audio_input_device_threshold_p", settingName, 41) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[41], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint8_t tempuint8;
						if (sscanf_s(value, "%hhu", &tempuint8) == 1 && tempuint8 <= XHV_AUDIO_SETTING_VOLUME_MAX) {
							if (configContext.saveValuesRead) {
								xlive_local_users[iUser].audio_input_device_threshold = tempuint8;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_audio_output_device_name_p", settingName, 37) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[37], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						if (configContext.saveValuesRead) {
							if (xlive_local_users[iUser].audio_output_device_name) {
								delete[] xlive_local_users[iUser].audio_output_device_name;
								xlive_local_users[iUser].audio_output_device_name = 0;
							}
							size_t valueSize = strlen(value);
							if (valueSize) {
								valueSize++;
								xlive_local_users[iUser].audio_output_device_name = new wchar_t[valueSize];
								swprintf_s(xlive_local_users[iUser].audio_output_device_name, valueSize, L"%hs", value);
							}
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_audio_output_device_volume_p", settingName, 39) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[39], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						uint8_t tempuint8;
						if (sscanf_s(value, "%hhu", &tempuint8) == 1 && tempuint8 <= XHV_AUDIO_SETTING_VOLUME_MAX) {
							if (configContext.saveValuesRead) {
								xlive_local_users[iUser].audio_output_device_volume = tempuint8;
							}
						}
						else {
							incorrect = true;
						}
					}
					else {
						unrecognised = true;
					}
				}
				else if (_strnicmp("xlive_user_voice_remote_volume_p", settingName, 32) == 0) {
					uint32_t iUser;
					if (sscanf_s(&settingName[32], "%u", &iUser) == 1 || iUser == 0 || iUser > XLIVE_LOCAL_USER_COUNT) {
						iUser--;
						
						auto voiceRemoteVolumes = configContext.saveValuesRead ? &xlive_local_users[iUser].xhv_voice_remote_volume : 0;
						
						if (voiceRemoteVolumes) {
							voiceRemoteVolumes->clear();
						}
						
						if (value && value[0] != 0) {
							const char* valuePrevious = value;
							const char* valueCurrent = value;
							while (1) {
								bool isEnd = (*valueCurrent == 0);
								if (isEnd || *valueCurrent == ',') {
									size_t functionNameLength = valueCurrent - valuePrevious;
									if (functionNameLength > 0) {
										uint64_t tempuint64;
										uint16_t tempuint16;
										if (_snscanf_s(valuePrevious, functionNameLength, "0x%016I64x:%hu", &tempuint64, &tempuint16) == 2 && tempuint64 != 0 && tempuint16 <= XHV_AUDIO_SETTING_VOLUME_MAX_BOOST) {
											if (voiceRemoteVolumes) {
												(*voiceRemoteVolumes)[tempuint64] = tempuint16;
											}
										}
										else {
											incorrect = true;
										}
									}
									
									if (isEnd) {
										break;
									}
									valuePrevious = &valueCurrent[1];
								}
								valueCurrent = &valueCurrent[1];
							}
						}
					}
					else {
						unrecognised = true;
					}
				}
				else {
					unrecognised = true;
				}
				
				if (!unrecognised && !duplicated && !incorrect) {
					configContext.readSettings.push_back(CloneString(settingName));
				}
			}
#undef SettingNameMatches
			delete[] settingName;
			settingName = 0;
		}
	}
	
	if (unrecognised || duplicated || incorrect) {
		
		if (configContext.keepInvalidLines) {
			// When the version of the header has changed.
			if (version[-1] != 0) {
				configContext.otherConfigHeaderEntries.push_back(CloneString(fileLine));
			}
			else {
				configContext.badConfigEntries.push_back(CloneString(fileLine));
			}
		}
	}
	return 0;
}

static uint32_t SaveXllnConfig(const wchar_t* file_config_path, INTERPRET_CONFIG_CONTEXT* configContext)
{
	FILE* fileConfig = 0;
	
	uint32_t err = _wfopen_s(&fileConfig, file_config_path, L"wb");
	if (err) {
		return err;
	}

#define WriteText(text) fputs(text, fileConfig)
#define WriteTextF(format, ...) fprintf_s(fileConfig, format, __VA_ARGS__)

	WriteText("#--- XLiveLessNess Configuration File ---");
	WriteText("\n");
	
	WriteText("\n# xlln_debug_log_trace_blacklist:");
	WriteText("\n# A comma separated list of trace function names to ignore in the logging to reduce spam.");
	WriteText("\n");
	
	WriteText("\n# xlln_debug_log_level:");
	WriteText("\n# Saves the log level from last use (stored in Hexadecimal).");
	WriteText("\n# DEFAULT value: 0x873F.");
	WriteText("\n# Binary to Hexadecimal:");
	WriteText("\n# 0b1000011100111111 == 0x873F.");
	WriteText("\n# 0bO0000MXG00FEWIDT - Bit Mask Legend.");
	WriteText("\n# Context Options:");
	WriteText("\n# - O - Other - Logs related to functionality from other areas of the application.");
	WriteText("\n# - M - XLLN-Module - Logs related to XLLN-Module functionality.");
	WriteText("\n# - X - XLiveLessNess - Logs related to XLiveLessNess functionality.");
	WriteText("\n# - G - XLive - Logs related to XLive(GFWL) functionality.");
	WriteText("\n# Log Level Options:");
	WriteText("\n# - F - Fatal - Errors that will terminate the application.");
	WriteText("\n# - E - Error - Any error which is fatal to the operation, but not the service or application (can't open a required file, missing data, etc.).");
	WriteText("\n# - W - Warning - Anything that can potentially cause application oddities, but is being handled adequately.");
	WriteText("\n# - I - Info - Generally useful information to log (service start/stop, configuration assumptions, etc).");
	WriteText("\n# - D - Debug - Function, variable and operation logging.");
	WriteText("\n# - T - Trace - Function call tracing.");
	WriteText("\n");
	
	WriteText("\n# xlive_hotkey_id_guide:");
	WriteText("\n# Valid values: 0x0 to 0xFFFF.");
	WriteText("\n#   0x0 - Turns off the hotkey.");
	WriteText("\n#   0x24 - (DEFAULT) - VK_HOME");
	WriteText("\n# This hotkey will open the Guide menu when pressed on (usually) the active Title screen/window.");
	WriteText("\n# If the Title does not use Direct3D (such as in dedicated servers) then the Guide hotkey will not work.");
	WriteText("\n# The following link documents the keyboard Virtual-Key (VK) codes available for use:");
	WriteText("\n# https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes");
	WriteText("\n");
	
	WriteText("\n# xlive_fps_limit:");
	WriteText("\n# Valid values: 0 to (2^32 - 1).");
	WriteText("\n#   0 - Disables the built in limiter.");
	WriteText("\n#   60 - (DEFAULT)");
	WriteText("\n# The frequency (times per second) that XLiveRender is permitted to execute.");
	WriteText("\n# XLiveRender is typically used in Title render loops so this value can be considered as an FPS limit.");
	WriteText("\n");
	
	WriteText("\n# xlln_network_instance_base_port:");
	WriteText("\n# Valid values: 1 to 65535.");
	WriteText("\n#   39000 - (DEFAULT).");
	WriteText("\n# The shared port between all local XLLN instances used for broadcasting/communicating available lobbies/games/matches.");
	WriteText("\n# This port also serves as the base port used to calculate the default xlln_network_instance_port value if it has not already been explicitly defined.");
	WriteText("\n");
	
	WriteText("\n# xlln_network_instance_port:");
	WriteText("\n# Valid values: 0 to 65535.");
	WriteText("\n#   0 - Disables ALL network access (the Title will still think it has network access when it actually does not).");
	WriteText("\n#   xlln_network_instance_base_port + X - (DEFAULT) X is the Local Instance ID (usually 1).");
	WriteText("\n");

	WriteText("\n# xlive_net_disable:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) All network functionality related to xlive functions is enabled.");
	WriteText("\n#   1 - All network functionality related to xlive functions is disabled.");
	WriteText("\n# Allows turning off xlive network functionality.");
	WriteText("\n");
	
	WriteText("\n# xlive_xhv_engine_enabled:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - (DEFAULT) The XHV Engine will not run.");
	WriteText("\n#   1 - The XHV Engine is enabled for use.");
	WriteText("\n# Allows activating the XHV Engine which is used for voice chat.");
	WriteText("\n");
	
	WriteText("\n# xlive_xhv_engine_voice_encoded_bitrate:");
	WriteText("\n# The bitrate per second of voice data to be sent over the network per local speaker. Note that this will not affect other player's voice quality. Set this too high and all the voice data you send may not make it to the other players.");
	WriteText("\n");
	
	WriteText("\n# xlive_specific_network_adapter:");
	WriteText("\n# Example value: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}");
	WriteText("\n# The network adapter to only use if it is available. If it is not available or this setting is blank then all network adapters will be used.");
	WriteText("\n");
	
	WriteText("\n# xlive_ignore_title_network_adapter:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Limits XLLN to only that network adapter when broadcasting and searching for available games.");
	WriteText("\n#   1 - (DEFAULT) Ignores setting a specific network adapter from the Title.");
	WriteText("\n# On startup in XLiveInitialize(...) the title specifies a specific network adapter to use (if any). This setting can prevent that from happening.");
	WriteText("\n");
	
	WriteText("\n# xlive_broadcast_address:");
	WriteText("\n# A comma separated list of network addresses to send broadcast packets to.");
	WriteText("\n# All addresses are comma (,) separated. Do not use spaces at all.");
	WriteText("\n# The IP and corresponding Port are (and must be to be considered valid) separated with a colon ':'. Since IPv6 uses colons in its format, IPv6 addresses need to be enclosed in square brackets '[' and ']'.");
	WriteText("\n# If an IP address is specified with port number 0, it will use the value of xlln_network_instance_base_port as the port to broadcast/send to.");
	WriteText("\n# If the network address is specified of a local subnet, it will actually prevent automatic broadcasting to that subnet specifically. If an IP on the subnet is specified then only that address in that subnet will receive broadcasts.");
	WriteText("\n# So an example of a comma separated list of a domain name, IPv4 (local address broadcast) and an IPv6 address is as follows:");
	WriteText("\n# glitchyscripts.com:1100,127.0.0.0:0,192.168.0.255:0,[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:39000");
	WriteText("\n");
	
	WriteText("\n# xlln_direct_ip_connect_password:");
	WriteText("\n# The last used password used to host or connect with via Direct IP.");
	WriteText("\n");
	
	WriteText("\n# xlln_direct_ip_connect_ip_port:");
	WriteText("\n# The saved IP address and Port of a lobby to connect to.");
	WriteText("\n# The IP and Port are separated with a colon ':'. Since IPv6 uses colons in its format, IPv6 addresses need to be enclosed in square brackets '[' and ']'.");
	WriteText("\n");

	WriteText("\n# xlive_locale:");
	WriteText("\n# Valid values:");
	WriteText("\n#    1 - English");
	WriteText("\n#    2 - Japanese");
	WriteText("\n#    3 - German");
	WriteText("\n#    4 - French");
	WriteText("\n#    5 - Spanish");
	WriteText("\n#    6 - Italian");
	WriteText("\n#    7 - Korean");
	WriteText("\n#    8 - Traditional Chinese");
	WriteText("\n#    9 - Portuguese");
	WriteText("\n#   10 - Simplified Chinese");
	WriteText("\n#   11 - Polish");
	WriteText("\n#   12 - Russian");
	WriteText("\n");
	
	WriteText("\n# xlive_username_p1...:");
	WriteText("\n# Max username length is 15 characters.");
	WriteText("\n# The username for each local profile to use.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_live_enabled_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - The user will be signed in to a local offline account.");
	WriteText("\n#   1 - The user will be signed in to an online LIVE account.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n# This option assumes xlive_user_online_enabled_p? is also disabled if this is too (and will set it so).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_online_enabled_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - The user will be signed in as 'Offline' on a LIVE account.");
	WriteText("\n#   1 - The user will be signed in as 'Online' on a LIVE account.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n# This option assumes xlive_user_live_enabled_p? is also enabled if this is too (and will set it so).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_auto_login_p1...:");
	WriteText("\n# Valid values:");
	WriteText("\n#   0 - Do not sign in automatically.");
	WriteText("\n#   1 - Auto sign in.");
	WriteText("\n# Whether the user will be automatically signed in when the Title calls XShowSigninUI(...) and there are no other users already logged in.");
	WriteText("\n# This option does not apply when logging in from XLLN-Modules via XLLNLogin(...).");
	WriteText("\n");
	
	WriteText("\n# xlive_user_audio_input_device_name_p1...:");
	WriteText("\n# The name of the audio input device used as the microphone for voice chat.");
	WriteText("\n# Leaving this blank is to disable the microphone.");
	WriteText("\n");
	
	WriteText("\n# xlive_user_audio_input_device_volume_p1...:");
	WriteTextF("\n# Valid values: 0 to %hu.", XHV_AUDIO_SETTING_VOLUME_MAX_BOOST);
	WriteTextF("\n#   %hu - (DEFAULT)", XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteText("\n# This can be used to reduce or increase the loudness of the captured audio from the microphone that others can hear in voice chat.");
	WriteText("\n");
	
	WriteText("\n# xlive_user_audio_input_device_threshold_p1...:");
	WriteTextF("\n# Valid values: 0 to %hu.", XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteText("\n#   0   - Always capturing audio.");
	WriteTextF("\n#   %hhu  - (DEFAULT) fairly sensitive.", XHV_AUDIO_SETTING_INPUT_THRESHOLD_DEFAULT);
	WriteText("\n#   100 - About impossible to capture audio.");
	WriteText("\n# This is used to change the microphone volume activation sensitivity in voice chat.");
	WriteText("\n");
	
	WriteText("\n# xlive_user_audio_output_device_name_p1...:");
	WriteText("\n# The name of the audio input device used as the playback speaker for voice chat.");
	WriteText("\n# Leaving this blank will prevent you from being able to hear anyone.");
	WriteText("\n");
	
	WriteText("\n# xlive_user_audio_output_device_volume_p1...:");
	WriteTextF("\n# Valid values: 0 to %hu.", XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteTextF("\n#   %hu - (DEFAULT)", XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteText("\n# This can be used to reduce the loudness all other players on voice chat.");
	WriteText("\n");
	
	WriteText("\n# xlive_user_voice_remote_volume_p1...:");
	WriteText("\n# Comma separated list with items in the form of XUID:VOLUME.");
	WriteTextF("\n# Example item 0x%016I64x:%hu.", BuildXUID(0, true, true, 0), XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteText("\n# XUID formatted as padded 64 bit value in hexadecimal. It however cannot be of value 0.");
	WriteTextF("\n# Valid VOLUME values: 0 to %hu.", XHV_AUDIO_SETTING_VOLUME_MAX_BOOST);
	WriteTextF("\n#   :%hhu - (DEFAULT)", XHV_AUDIO_SETTING_VOLUME_MAX);
	WriteText("\n# This can be used to reduce the loudness or mute a specific player in voice chat.");
	WriteText("\n");
	
	WriteText("\n");
	WriteTextF("\n[%s:%s]", XllnConfigHeader, XllnConfigVersion);
	{
		char* blacklist = XllnGetDebugLogTraceBlacklist();
		WriteTextF("\nxlln_debug_log_trace_blacklist = %s", blacklist ? blacklist : "");
		if (blacklist) {
			free(blacklist);
			blacklist = 0;
		}
	}
	WriteTextF("\nxlln_debug_log_level = 0x%08x", xlln_debug_log_level);
	WriteTextF("\nxlive_hotkey_id_guide = 0x%04hx", xlive_hotkey_id_guide);
	WriteTextF("\nxlive_fps_limit = %u", xlive_fps_limit);
	WriteTextF("\nxlln_network_instance_base_port = %hu", xlln_network_instance_base_port);
	WriteTextF("\nxlln_network_instance_port = %hu", xlln_network_instance_port);
	WriteTextF("\nxlive_net_disable = %u", xlive_netsocket_abort ? 1 : 0);
	WriteTextF("\nxlive_xhv_engine_enabled = %u", xlive_xhv_engine_enabled ? 1 : 0);
	WriteTextF("\nxlive_xhv_engine_voice_encoded_bitrate = %u", xlive_xhv_engine_voice_encoded_bitrate);
	WriteTextF("\nxlive_specific_network_adapter = %s", xlive_config_specific_network_adapter_name ? xlive_config_specific_network_adapter_name : "");
	WriteTextF("\nxlive_ignore_title_network_adapter = %u", xlive_ignore_title_network_adapter ? 1 : 0);
	WriteTextF("\nxlive_broadcast_address = %s", broadcastAddrInput);
	WriteTextF("\nxlln_direct_ip_connect_password = %s", xlln_direct_ip_connect_password);
	WriteTextF("\nxlln_direct_ip_connect_ip_port = %s", xlln_direct_ip_connect_ip_port);
	WriteTextF("\nxlive_locale = %hhu", xlive_locale);
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		WriteTextF("\nxlive_username_p%u = %s", iUser + 1, xlive_local_users[iUser].username);
		WriteTextF("\nxlive_user_live_enabled_p%u = %u", iUser + 1, xlive_local_users[iUser].live_enabled ? 1 : 0);
		WriteTextF("\nxlive_user_online_enabled_p%u = %u", iUser + 1, xlive_local_users[iUser].online_enabled ? 1 : 0);
		WriteTextF("\nxlive_user_auto_login_p%u = %u", iUser + 1, xlive_local_users[iUser].auto_login ? 1 : 0);
		WriteTextF("\nxlive_user_audio_input_device_name_p%u = %ls", iUser + 1, (!xlive_local_users[iUser].audio_input_device_name ? L"" : xlive_local_users[iUser].audio_input_device_name));
		WriteTextF("\nxlive_user_audio_input_device_volume_p%u = %hu", iUser + 1, (uint16_t)xlive_local_users[iUser].audio_input_device_volume);
		WriteTextF("\nxlive_user_audio_input_device_threshold_p%u = %hhu", iUser + 1, (uint8_t)xlive_local_users[iUser].audio_input_device_threshold);
		WriteTextF("\nxlive_user_audio_output_device_name_p%u = %ls", iUser + 1, (!xlive_local_users[iUser].audio_output_device_name ? L"" : xlive_local_users[iUser].audio_output_device_name));
		WriteTextF("\nxlive_user_audio_output_device_volume_p%u = %hhu", iUser + 1, (uint8_t)xlive_local_users[iUser].audio_output_device_volume);
		
		WriteTextF("\nxlive_user_voice_remote_volume_p%u = ", iUser + 1);
		auto &voiceRemoteVolumes = xlive_local_users[iUser].xhv_voice_remote_volume;
		bool isFirst = true;
		for (const auto &itrVoiceRemoteVolume : voiceRemoteVolumes) {
			if (isFirst) {
				isFirst = false;
			}
			else {
				WriteText(",");
			}
			
			WriteTextF("0x%016I64x:%hu", itrVoiceRemoteVolume.first, itrVoiceRemoteVolume.second);
		}
	}
	WriteText("\n\n");
	
	if (configContext) {
		for (char* &badEntry : configContext->badConfigEntries) {
			WriteText(badEntry);
			WriteText("\n");
		}
		if (configContext->badConfigEntries.size()) {
			WriteText("\n\n");
		}
		for (char* &otherEntry : configContext->otherConfigHeaderEntries) {
			WriteText(otherEntry);
			WriteText("\n");
		}
		if (configContext->otherConfigHeaderEntries.size()) {
			WriteText("\n");
		}
	}
	
	fclose(fileConfig);
	
	return ERROR_SUCCESS;
}

uint32_t XllnConfig(bool save_config)
{
	// Always read the/a config file before saving.
	uint32_t result = ERROR_FUNCTION_FAILED;
	errno_t errorFopen = ERROR_SUCCESS;
	FILE* fileConfig = 0;
	
	// Use exec argument path if provided.
	if (xlln_file_config_path) {
		errorFopen = _wfopen_s(&fileConfig, xlln_file_config_path, L"rb");
		if (!fileConfig) {
			if (errorFopen == ENOENT) {
				result = ERROR_FILE_NOT_FOUND;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file not found: \"%ls\".", xlln_file_config_path);
			}
			else {
				XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Config file \"%ls\" read error:", xlln_file_config_path);
				return ERROR_FUNCTION_FAILED;
			}
		}
	}
	
	wchar_t* configAutoPath = 0;
	wchar_t* configAutoFallbackPathAppdata = 0;
	wchar_t* configAutoFallbackPathWorkingDir = 0;
	bool saveConfigFileToWorkingDirectoryOverride = false;
	uint32_t readConfigFromOtherInstance = 0;
	// Do not use auto paths if an exec arg path was provided.
	if (!fileConfig && !xlln_file_config_path) {
		wchar_t* appdataPath = 0;
		errno_t errorEnvVar = _wdupenv_s(&appdataPath, NULL, L"LOCALAPPDATA");
		if (errorEnvVar) {
			XLLN_DEBUG_LOG_ECODE(errorEnvVar, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s %%LOCALAPPDATA%% path unable to be resolved with error:", __func__);
		}
		
		for (uint32_t searchInstanceId = xlln_local_instance_id; searchInstanceId != 0; searchInstanceId--) {
			if (searchInstanceId != xlln_local_instance_id) {
				readConfigFromOtherInstance = searchInstanceId;
			}
			bool appdataDirectory = false;
			do {
				if (configAutoPath) {
					free(configAutoPath);
				}
				if (appdataDirectory) {
					configAutoPath = FormMallocString(L"%s/XLiveLessNess/xlln-config-%u.ini", appdataPath, searchInstanceId);
					if (!configAutoFallbackPathAppdata) {
						configAutoFallbackPathAppdata = CloneString(configAutoPath);
					}
				}
				else {
					configAutoPath = FormMallocString(L"./XLiveLessNess/xlln-config-%u.ini", searchInstanceId);
					if (!configAutoFallbackPathWorkingDir) {
						configAutoFallbackPathWorkingDir = CloneString(configAutoPath);
					}
				}
				errorFopen = _wfopen_s(&fileConfig, configAutoPath, L"rb");
				if (fileConfig) {
					fseek(fileConfig, (long)0, SEEK_END);
					uint32_t fileSize = ftell(fileConfig);
					fseek(fileConfig, (long)0, SEEK_SET);
					fileSize -= ftell(fileConfig);
					
					if (fileSize == 0) {
						fclose(fileConfig);
						fileConfig = 0;
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file is empty: \"%ls\".", configAutoPath);
						if (!appdataDirectory && searchInstanceId == xlln_local_instance_id) {
							saveConfigFileToWorkingDirectoryOverride = true;
						}
					}
					else {
						searchInstanceId = 0;
						break;
					}
				}
				else {
					if (errorFopen == ENOENT) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file not found: \"%ls\".", configAutoPath);
					}
					else {
						XLLN_DEBUG_LOG_ECODE(errorFopen, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "Auto config file \"%ls\" read error:", configAutoPath);
					}
				}
			} while (!errorEnvVar && (appdataDirectory = !appdataDirectory));
			if (searchInstanceId == 0) {
				break;
			}
		}
		
		if (appdataPath) {
			free(appdataPath);
		}
	}
	
	INTERPRET_CONFIG_CONTEXT* interpretationContext = 0;
	if (fileConfig) {
		interpretationContext = new INTERPRET_CONFIG_CONTEXT;
		interpretationContext->keepInvalidLines = true;
		interpretationContext->saveValuesRead = !save_config;
		interpretationContext->readConfigFromOtherInstance = readConfigFromOtherInstance;
		
		if (interpretationContext->saveValuesRead) {
			EnterCriticalSection(&xlive_critsec_local_user);
		}
		
		ReadIniFile(fileConfig, true, XllnConfigHeader, XllnConfigVersion, interpretConfigSetting, (void*)interpretationContext);
		
		if (interpretationContext->saveValuesRead) {
			LeaveCriticalSection(&xlive_critsec_local_user);
		}
		
		for (char* &readSettingName : interpretationContext->readSettings) {
			delete[] readSettingName;
		}
		interpretationContext->readSettings.clear();
		
		fclose(fileConfig);
		fileConfig = 0;
	}
	
	if (configAutoPath) {
		free(configAutoPath);
		configAutoPath = 0;
	}
	
	for (uint8_t i = 0; i < 2; i++) {
		wchar_t* saveToConfigFilePath = 0;
		if (xlln_file_config_path) {
			saveToConfigFilePath = xlln_file_config_path;
			i = 2;
		}
		else if (saveConfigFileToWorkingDirectoryOverride) {
			saveToConfigFilePath = configAutoFallbackPathWorkingDir;
			i = 2;
		}
		else if (i == 0) {
			saveToConfigFilePath = configAutoFallbackPathAppdata;
		}
		else if (i == 1) {
			saveToConfigFilePath = configAutoFallbackPathWorkingDir;
		}
		
		if (saveToConfigFilePath) {
			wchar_t* saveToConfigPath = PathFromFilename(saveToConfigFilePath);
			uint32_t errorMkdir = EnsureDirectoryExists(saveToConfigPath);
			delete[] saveToConfigPath;
			if (errorMkdir) {
				result = ERROR_DIRECTORY;
				XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, saveToConfigFilePath);
				break;
			}
			else {
				uint32_t errorSaveConfig = SaveXllnConfig(saveToConfigFilePath, interpretationContext);
				if (errorSaveConfig) {
					result = errorSaveConfig;
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVELESSNESS | XLLN_LOG_LEVEL_WARN, "SaveXllnConfig(...) error: %u, \"%ls\".", errorSaveConfig, saveToConfigFilePath);
				}
				else {
					result = ERROR_SUCCESS;
					if (!xlln_file_config_path) {
						xlln_file_config_path = CloneString(saveToConfigFilePath);
					}
				}
				break;
			}
		}
	}
	
	if (configAutoFallbackPathAppdata) {
		delete[] configAutoFallbackPathAppdata;
		configAutoFallbackPathAppdata = 0;
	}
	if (configAutoFallbackPathWorkingDir) {
		delete[] configAutoFallbackPathWorkingDir;
		configAutoFallbackPathWorkingDir = 0;
	}
	
	if (interpretationContext) {
		for (char* &badEntry : interpretationContext->badConfigEntries) {
			delete[] badEntry;
		}
		interpretationContext->badConfigEntries.clear();
		for (char* &otherEntry : interpretationContext->otherConfigHeaderEntries) {
			delete[] otherEntry;
		}
		interpretationContext->otherConfigHeaderEntries.clear();
		delete interpretationContext;
		interpretationContext = 0;
	}
	
	return result;
}

uint32_t InitXllnConfig()
{
	XllnConfig(false);
	return ERROR_SUCCESS;
}

uint32_t UninitXllnConfig()
{
	XllnConfig(true);
	
	//XXX: Note this variable is used to hold the execution argument config path and doing this will erase that preference. Fine for now but fix in any config feature enhancement involving initialising the config a second time.
	if (xlln_file_config_path) {
		free(xlln_file_config_path);
		xlln_file_config_path = 0;
	}
	
	return ERROR_SUCCESS;
}
