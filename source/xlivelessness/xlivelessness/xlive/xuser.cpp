#include <winsock2.h>
#include "../xlln/social.hpp"
#include "xdefs.hpp"
#include "xuser.hpp"
#include "xlive.hpp"
#include "xsession.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/wnd-achievements.hpp"
#include "../xlln/xlln-config.hpp"
#include "../utils/utils.hpp"
#include <stdio.h>
#include <set>

CRITICAL_SECTION xlive_critsec_xuser_achievement_enumerators;
// Key: enumerator handle (id).
std::map<HANDLE, ACHIEVEMENT_ENUMERATION_DETAILS*> xlive_xuser_achievement_enumerators;

CRITICAL_SECTION xlive_critsec_xuser_stats;
// Key: enumerator handle (id).
std::map<HANDLE, STATS_ENUMERATION_DETAILS*> xlive_xuser_stats_enumerators;
// Stats the title wrote via XSessionWriteStats, kept so XUserReadStats can serve them back, the
// Stats window can show and edit them, and they can persist to stats.dat. Guarded by
// xlive_critsec_xuser_stats. Key: user xuid -> view id -> column id. A column the title never
// wrote is absent and reads back as XUSER_DATA_TYPE_NULL - how Live reports an un-earned stat,
// so the title skips it instead of trusting a fabricated value.
std::map<XUID, std::map<uint32_t, std::map<uint32_t, XUSER_DATA>>> xlive_written_stats;

CRITICAL_SECTION xlive_critsec_xuser_context_properties;
// Key: contextId
// Value: contextValue
std::map<uint32_t, uint32_t> xlive_user_contexts[XLIVE_LOCAL_USER_COUNT];
// Key: propertyId
// Value: propertyValue
std::map<uint32_t, XUSER_DATA> xlive_user_properties[XLIVE_LOCAL_USER_COUNT];

static const char* GetXProfileSettingName(uint32_t setting_id)
{
	switch (setting_id) {
		case XPROFILE_GAMER_YAXIS_INVERSION: {
			return "XPROFILE_GAMER_YAXIS_INVERSION";
		}
		case XPROFILE_OPTION_CONTROLLER_VIBRATION: {
			return "XPROFILE_OPTION_CONTROLLER_VIBRATION";
		}
		case XPROFILE_GAMERCARD_ZONE: {
			return "XPROFILE_GAMERCARD_ZONE";
		}
		case XPROFILE_GAMERCARD_REGION: {
			return "XPROFILE_GAMERCARD_REGION";
		}
		case XPROFILE_GAMERCARD_CRED: {
			return "XPROFILE_GAMERCARD_CRED";
		}
		case XPROFILE_GAMERCARD_REP: {
			return "XPROFILE_GAMERCARD_REP";
		}
		case XPROFILE_OPTION_VOICE_MUTED: {
			return "XPROFILE_OPTION_VOICE_MUTED";
		}
		case XPROFILE_OPTION_VOICE_THRU_SPEAKERS: {
			return "XPROFILE_OPTION_VOICE_THRU_SPEAKERS";
		}
		case XPROFILE_OPTION_VOICE_VOLUME: {
			return "XPROFILE_OPTION_VOICE_VOLUME";
		}
		case XPROFILE_GAMERCARD_PICTURE_KEY: {
			return "XPROFILE_GAMERCARD_PICTURE_KEY";
		}
		case XPROFILE_GAMERCARD_MOTTO: {
			return "XPROFILE_GAMERCARD_MOTTO";
		}
		case XPROFILE_GAMERCARD_TITLES_PLAYED: {
			return "XPROFILE_GAMERCARD_TITLES_PLAYED";
		}
		case XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED: {
			return "XPROFILE_GAMERCARD_ACHIEVEMENTS_EARNED";
		}
		case XPROFILE_GAMER_DIFFICULTY: {
			return "XPROFILE_GAMER_DIFFICULTY";
		}
		case XPROFILE_GAMER_CONTROL_SENSITIVITY: {
			return "XPROFILE_GAMER_CONTROL_SENSITIVITY";
		}
		case XPROFILE_GAMER_PREFERRED_COLOR_FIRST: {
			return "XPROFILE_GAMER_PREFERRED_COLOR_FIRST";
		}
		case XPROFILE_GAMER_PREFERRED_COLOR_SECOND: {
			return "XPROFILE_GAMER_PREFERRED_COLOR_SECOND";
		}
		case XPROFILE_GAMER_ACTION_AUTO_AIM: {
			return "XPROFILE_GAMER_ACTION_AUTO_AIM";
		}
		case XPROFILE_GAMER_ACTION_AUTO_CENTER: {
			return "XPROFILE_GAMER_ACTION_AUTO_CENTER";
		}
		case XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL: {
			return "XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL";
		}
		case XPROFILE_GAMER_RACE_TRANSMISSION: {
			return "XPROFILE_GAMER_RACE_TRANSMISSION";
		}
		case XPROFILE_GAMER_RACE_CAMERA_LOCATION: {
			return "XPROFILE_GAMER_RACE_CAMERA_LOCATION";
		}
		case XPROFILE_GAMER_RACE_BRAKE_CONTROL: {
			return "XPROFILE_GAMER_RACE_BRAKE_CONTROL";
		}
		case XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL: {
			return "XPROFILE_GAMER_RACE_ACCELERATOR_CONTROL";
		}
		case XPROFILE_GAMERCARD_TITLE_CRED_EARNED: {
			return "XPROFILE_GAMERCARD_TITLE_CRED_EARNED";
		}
		case XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED: {
			return "XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED";
		}
		case XPROFILE_TITLE_SPECIFIC1: {
			return "XPROFILE_TITLE_SPECIFIC1";
		}
		case XPROFILE_TITLE_SPECIFIC2: {
			return "XPROFILE_TITLE_SPECIFIC2";
		}
		case XPROFILE_TITLE_SPECIFIC3: {
			return "XPROFILE_TITLE_SPECIFIC3";
		}
	}
	
	return "";
}

static BOOL XLivepIsPropertyIdValid(uint32_t property_id, BOOL a2)
{
	return !(property_id & X_PROPERTY_SCOPE_MASK)
		|| property_id == X_PROPERTY_RANK
		|| property_id == X_PROPERTY_SESSION_ID
		|| property_id == X_PROPERTY_GAMER_ZONE
		|| property_id == X_PROPERTY_GAMER_COUNTRY
		|| property_id == X_PROPERTY_GAMER_LANGUAGE
		|| property_id == X_PROPERTY_GAMER_RATING
		|| property_id == X_PROPERTY_GAMER_MU
		|| property_id == X_PROPERTY_GAMER_SIGMA
		|| property_id == X_PROPERTY_GAMER_PUID
		|| property_id == X_PROPERTY_AFFILIATE_SCORE
		|| property_id == X_PROPERTY_RELATIVE_SCORE
		|| property_id == X_PROPERTY_SESSION_TEAM
		|| !a2 && property_id == X_PROPERTY_GAMER_HOSTNAME;
}

static void GetXProfileSettingInfo(uint32_t setting_id, uint8_t* data_type, uint16_t* data_size, bool* is_title_setting, bool* can_write)
{
	switch (setting_id) {
	case XPROFILE_TITLE_SPECIFIC1:
	case XPROFILE_TITLE_SPECIFIC2:
	case XPROFILE_TITLE_SPECIFIC3:
	case XPROFILE_GAMERCARD_TITLE_ACHIEVEMENTS_EARNED:
	case XPROFILE_GAMERCARD_TITLE_CRED_EARNED:
	case XPROFILE_OPTION_CONTROLLER_VIBRATION:
		if (is_title_setting) {
			*is_title_setting = true;
		}
		if (can_write) {
			*can_write = true;
		}
		break;
	default:
		if (is_title_setting) {
			*is_title_setting = false;
		}
		if (can_write) {
			*can_write = false;
		}
		break;
	}
	SETTING_ID* setting_id_info = (SETTING_ID*)&setting_id;
	if (data_type) {
		*data_type = setting_id_info->data_type;
	}
	if (data_size) {
		*data_size = setting_id_info->data_size;
	}
}

static bool IsValidSettingId(uint32_t title_id, uint32_t setting_id)
{
	uint8_t dataType;
	uint16_t dataSize;
	GetXProfileSettingInfo(setting_id, &dataType, &dataSize, 0, 0);
	uint32_t requiredReadSizeMin = 0;
	switch (dataType) {
	case XUSER_DATA_TYPE_INT32:
		if (dataSize != sizeof(LONG)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_INT64:
		if (dataSize != sizeof(LONGLONG)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_DOUBLE:
		if (dataSize != sizeof(double)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_FLOAT:
		if (dataSize != sizeof(FLOAT)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_DATETIME:
		if (dataSize != sizeof(FILETIME)) {
			return false;
		}
		break;
	case XUSER_DATA_TYPE_UNICODE:
		if (dataSize < sizeof(wchar_t)) {
			return false;
		}
		break;
	}

	if (
		(*(SETTING_ID*)&setting_id).id < 0x50
		|| (
			setting_id == XPROFILE_TITLE_SPECIFIC1
			|| setting_id == XPROFILE_TITLE_SPECIFIC2
			|| setting_id == XPROFILE_TITLE_SPECIFIC3
			)
		&& title_id != DASHBOARD_TITLE_ID
		) {
		return true;
	}
	return false;
}

static uint32_t ValidateSettings(uint32_t title_id, size_t num_settings, const XUSER_PROFILE_SETTING* settings)
{
	if (num_settings == 0) {
		return ERROR_SUCCESS;
	}
	for (size_t i = 0; i < num_settings; i++) {
		if (!IsValidSettingId(title_id, settings[i].dwSettingId)) {
			return ERROR_INVALID_PARAMETER;
		}
	}
	return ERROR_SUCCESS;
}

static uint32_t ValidateSettingIds(uint32_t title_id, size_t num_settings, const uint32_t* setting_ids)
{
	if (num_settings == 0) {
		return ERROR_SUCCESS;
	}
	for (size_t i = 0; i < num_settings; i++) {
		if (!IsValidSettingId(title_id, setting_ids[i])) {
			return ERROR_INVALID_PARAMETER;
		}
	}
	return ERROR_SUCCESS;
}

bool InitXUser()
{
	return true;
}

bool UninitXUser()
{
	EnterCriticalSection(&xlive_critsec_xuser_context_properties);
	
	for (uint32_t iUser = 0; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		xlive_user_contexts[iUser].clear();
		
		auto &userProperties = xlive_user_properties[iUser];
		for (auto &itrUserProperty : userProperties) {
			if (itrUserProperty.second.type == XUSER_DATA_TYPE_UNICODE && itrUserProperty.second.string.pwszData) {
				delete[] itrUserProperty.second.string.pwszData;
				itrUserProperty.second.string.pwszData = 0;
			}
			else if (itrUserProperty.second.type == XUSER_DATA_TYPE_BINARY && itrUserProperty.second.binary.pbData) {
				delete[] itrUserProperty.second.binary.pbData;
				itrUserProperty.second.binary.pbData = 0;
			}
		}
		userProperties.clear();
	}
	
	LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	
	EnterCriticalSection(&xlive_critsec_xuser_stats);
	for (auto &itrUser : xlive_written_stats) {
		for (auto &itrView : itrUser.second) {
			for (auto &itrColumn : itrView.second) {
				XllnStatsFreeUserData(itrColumn.second);
			}
		}
	}
	xlive_written_stats.clear();
	LeaveCriticalSection(&xlive_critsec_xuser_stats);

	return true;
}

// Deep-copy an XUSER_DATA so the stats store owns any UNICODE/BINARY buffer instead of aliasing the
// title's transient one. cbData counts bytes (UNICODE includes the null-terminator); mirror the
// new[]/delete[] element types used for xlive_user_properties so XllnStatsFreeUserData matches.
// Defined with the stats file helpers below; the clone needs it to copy only the bytes a type covers.
static uint32_t XllnStatsScalarWidth(uint8_t type);

XUSER_DATA XllnStatsCloneUserData(const XUSER_DATA& source)
{
	// A title only fills the union member its type names, so copying the struct would carry its
	// leftover stack bytes into the store; the disk round-trip zero-fills them and would then
	// disagree with the in-memory value. Start clean and copy only what the type covers.
	XUSER_DATA copy = {};
	copy.type = source.type;
	{
		uint32_t scalarWidth = XllnStatsScalarWidth(source.type);
		if (scalarWidth) {
			memcpy(&copy.nData, &source.nData, scalarWidth);
		}
	}
	if (source.type == XUSER_DATA_TYPE_UNICODE) {
		if (source.string.pwszData && source.string.cbData >= sizeof(wchar_t)) {
			size_t wcharCount = source.string.cbData / sizeof(wchar_t);
			copy.string.pwszData = new wchar_t[wcharCount];
			memcpy(copy.string.pwszData, source.string.pwszData, wcharCount * sizeof(wchar_t));
			copy.string.cbData = wcharCount * sizeof(wchar_t);
		}
		else {
			copy.string.pwszData = 0;
			copy.string.cbData = 0;
		}
	}
	else if (source.type == XUSER_DATA_TYPE_BINARY) {
		if (source.binary.pbData && source.binary.cbData) {
			copy.binary.pbData = new uint8_t[source.binary.cbData];
			memcpy(copy.binary.pbData, source.binary.pbData, source.binary.cbData);
			copy.binary.cbData = source.binary.cbData;
		}
		else {
			copy.binary.pbData = 0;
			copy.binary.cbData = 0;
		}
	}
	return copy;
}

void XllnStatsFreeUserData(XUSER_DATA& value)
{
	if (value.type == XUSER_DATA_TYPE_UNICODE && value.string.pwszData) {
		delete[] value.string.pwszData;
		value.string.pwszData = 0;
		value.string.cbData = 0;
	}
	else if (value.type == XUSER_DATA_TYPE_BINARY && value.binary.pbData) {
		delete[] value.binary.pbData;
		value.binary.pbData = 0;
		value.binary.cbData = 0;
	}
}

// The single mutation point for xlive_written_stats: frees any prior value before overwriting so an
// owned string/binary buffer never leaks, and treats a NULL value as "unset" by removing the cell
// (absence is how XUserReadStats reports an un-set stat) - pruning now-empty parent maps too.
void XllnStatsStoreValue(XUID xuid, uint32_t view_id, uint32_t column_id, const XUSER_DATA& value)
{
	EnterCriticalSection(&xlive_critsec_xuser_stats);

	auto itrUser = xlive_written_stats.find(xuid);
	if (itrUser != xlive_written_stats.end()) {
		auto itrView = itrUser->second.find(view_id);
		if (itrView != itrUser->second.end()) {
			auto itrColumn = itrView->second.find(column_id);
			if (itrColumn != itrView->second.end()) {
				XllnStatsFreeUserData(itrColumn->second);
				itrView->second.erase(itrColumn);
			}
			if (itrView->second.empty()) {
				itrUser->second.erase(itrView);
			}
		}
		if (itrUser->second.empty()) {
			xlive_written_stats.erase(itrUser);
		}
	}

	if (value.type != XUSER_DATA_TYPE_NULL) {
		XUSER_DATA cloned = XllnStatsCloneUserData(value);
		// An empty UNICODE/BINARY clones to {NULL, 0}; storing that would make every reader
		// special-case a set-but-pointerless value, so treat it like a NULL-typed (unset) write.
		bool emptyClone = (cloned.type == XUSER_DATA_TYPE_UNICODE && !cloned.string.pwszData)
			|| (cloned.type == XUSER_DATA_TYPE_BINARY && !cloned.binary.pbData);
		if (!emptyClone) {
			xlive_written_stats[xuid][view_id][column_id] = cloned;
		}
	}

	LeaveCriticalSection(&xlive_critsec_xuser_stats);
}

// Titles read leaderboard columns by the field's attribute id, while XSessionWriteStats keys the store
// by the property id's low bits (columnId). Map a requested attribute id back to that stored column id
// via the SPA view definition; pass the id through unchanged when it is not a known attribute id (e.g.
// a title that already reads by the property-id column, or a view with no SPA schema).
uint32_t XllnStatsResolveColumnId(uint32_t view_id, uint16_t column_id)
{
	auto itrView = xlive_stat_view_defs.find(view_id);
	if (itrView != xlive_stat_view_defs.end()) {
		for (const auto &column : itrView->second.columns) {
			// System-scope columns share the low id bits with a title column, and the store only ever
			// holds title columns, so matching one would resolve onto an unrelated cell.
			if (!column.isSystem && column.attributeId == column_id) {
				return column.columnId;
			}
		}
	}
	return column_id;
}

// stats.dat on-disk format. Native little-endian and pack(1) - deliberately distinct from the
// big-endian XDBF/SPA (XVC2) parse the schema comes from. Each value is stored as a fixed-width
// scalar or, for UNICODE/BINARY, a byte blob whose length is the record's own length field
// (XUSER_DATA.cbData is size_t, so the width is pinned to uint32 on disk).
#pragma pack(push, 1)
typedef struct _XLLN_STATS_FILE_HEADER {
	uint32_t magic;
	uint32_t version;
} XLLN_STATS_FILE_HEADER;
typedef struct _XLLN_STATS_FILE_RECORD {
	uint32_t viewId;
	uint32_t columnId;
	uint8_t type;
	uint32_t length;
} XLLN_STATS_FILE_RECORD;
#pragma pack(pop)

static const uint32_t XLLN_STATS_FILE_MAGIC = 0x54534C58; // 'XLST' as it appears byte-for-byte on disk.
static const uint32_t XLLN_STATS_FILE_VERSION = 1;
// Refuse an implausibly large record so a corrupt file cannot drive a huge allocation.
static const uint32_t XLLN_STATS_MAX_RECORD_BYTES = 0x10000;

// The fixed on-disk width of a scalar stat value; 0 for the variable-length UNICODE/BINARY types,
// which are handled from the union's pointer members instead of the overlaid scalar bytes.
static uint32_t XllnStatsScalarWidth(uint8_t type)
{
	switch (type) {
		case XUSER_DATA_TYPE_CONTEXT:
		case XUSER_DATA_TYPE_INT32:
		case XUSER_DATA_TYPE_FLOAT:
			return 4;
		case XUSER_DATA_TYPE_INT64:
		case XUSER_DATA_TYPE_DOUBLE:
		case XUSER_DATA_TYPE_DATETIME:
			return 8;
		default:
			return 0;
	}
}

uint32_t XllnStatsSaveUserToDisk(uint32_t user_index)
{
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		return ERROR_INVALID_OPERATION;
	}
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	XUID xuid = xlive_local_users[user_index].xuid;

	wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);
	wchar_t* pathTitle = FormNewString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_local_users[user_index].username);
	wchar_t* pathStats = FormNewString(L"%sstats.dat", pathTitle);
	// Write to a temp file and swap it in: rewriting stats.dat in place truncates the only
	// persisted copy first, so a crash mid-save would wipe every stat.
	// The name carries the thread id because the title thread (XSessionWriteStats) and the stats
	// window can save the same user at once: on one shared name they would open the same file with
	// "wb", truncate each other and interleave their records.
	wchar_t* pathStatsTemp = FormNewString(L"%sstats.dat.%08x.tmp", pathTitle, GetCurrentThreadId());
	delete[] pathConfig;
	pathConfig = 0;

	uint32_t errorMkdir = EnsureDirectoryExists(pathTitle);
	delete[] pathTitle;
	pathTitle = 0;
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, pathStats);
		delete[] pathStats;
		delete[] pathStatsTemp;
		return errorMkdir;
	}

	FILE* fpStats;
	errno_t errorFileOpen = _wfopen_s(&fpStats, pathStatsTemp, L"wb");
	if (errorFileOpen) {
		XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"wb\") error:", __func__, pathStatsTemp);
		delete[] pathStats;
		delete[] pathStatsTemp;
		return errorFileOpen;
	}

	XLLN_STATS_FILE_HEADER header = { XLLN_STATS_FILE_MAGIC, XLLN_STATS_FILE_VERSION };
	// A short write must not reach MoveFileExW below: replacing the only complete copy with a
	// truncated one is the exact loss the temp file exists to prevent, so track every write.
	bool writeFailed = fwrite(&header, sizeof(header), 1, fpStats) != 1;

	uint32_t recordsWritten = 0;
	EnterCriticalSection(&xlive_critsec_xuser_stats);
	auto itrUser = xlive_written_stats.find(xuid);
	if (itrUser != xlive_written_stats.end()) {
		for (auto &itrView : itrUser->second) {
			for (auto &itrColumn : itrView.second) {
				const XUSER_DATA &value = itrColumn.second;
				const void* payload = 0;
				uint32_t length = 0;
				if (value.type == XUSER_DATA_TYPE_UNICODE) {
					payload = value.string.pwszData;
					length = (uint32_t)value.string.cbData;
				}
				else if (value.type == XUSER_DATA_TYPE_BINARY) {
					payload = value.binary.pbData;
					length = (uint32_t)value.binary.cbData;
				}
				else {
					length = XllnStatsScalarWidth(value.type);
					if (!length) {
						// The store accepts whatever type byte a title passed, so a value with no on-disk width
						// still serves from memory; say it is being dropped or memory and disk differ silently.
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s cannot store view 0x%08x column %u of type %hhu; omitted from stats.dat for user %u.", __func__, itrView.first, itrColumn.first, value.type, user_index);
						continue;
					}
					// Every scalar member overlays the union start, so the fixed-width bytes read the same.
					payload = &value.nData;
				}
				XLLN_STATS_FILE_RECORD record = { itrView.first, itrColumn.first, value.type, length };
				if (fwrite(&record, sizeof(record), 1, fpStats) != 1) {
					writeFailed = true;
					break;
				}
				if (length && payload && fwrite(payload, 1, length, fpStats) != length) {
					writeFailed = true;
					break;
				}
				recordsWritten++;
			}
			if (writeFailed) {
				break;
			}
		}
	}
	LeaveCriticalSection(&xlive_critsec_xuser_stats);

	// fclose flushes, so a full buffer can only surface its error here.
	if (fclose(fpStats) != 0) {
		writeFailed = true;
	}

	if (writeFailed) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s failed writing \"%ls\"; keeping the previous stats.dat for user %u.", __func__, pathStatsTemp, user_index);
		DeleteFileW(pathStatsTemp);
		delete[] pathStats;
		delete[] pathStatsTemp;
		return ERROR_WRITE_FAULT;
	}

	if (!MoveFileExW(pathStatsTemp, pathStats, MOVEFILE_REPLACE_EXISTING)) {
		uint32_t errorMove = GetLastError();
		XLLN_DEBUG_LOG_ECODE(errorMove, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s MoveFileExW(\"%ls\", \"%ls\") error:", __func__, pathStatsTemp, pathStats);
		DeleteFileW(pathStatsTemp);
		delete[] pathStats;
		delete[] pathStatsTemp;
		return errorMove;
	}
	delete[] pathStats;
	delete[] pathStatsTemp;

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s wrote %u stat record(s) for user %u (xuid 0x%016I64x).", __func__, recordsWritten, user_index, xuid);
	return ERROR_SUCCESS;
}

uint32_t XllnStatsLoadUserFromDisk(uint32_t user_index)
{
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	XUID xuid = xlive_local_users[user_index].xuid;

	// Sign-out does not release the store, so it can still hold columns from the previous session. The
	// file wins where both have one; a store-only column is kept because a failed save left memory as
	// its only copy, and is reported because the next save persists it silently.
	std::set<std::pair<uint32_t, uint32_t>> unpersistedColumns;
	EnterCriticalSection(&xlive_critsec_xuser_stats);
	auto itrPreviousUser = xlive_written_stats.find(xuid);
	if (itrPreviousUser != xlive_written_stats.end()) {
		for (const auto &itrView : itrPreviousUser->second) {
			for (const auto &itrColumn : itrView.second) {
				unpersistedColumns.emplace(itrView.first, itrColumn.first);
			}
		}
	}
	LeaveCriticalSection(&xlive_critsec_xuser_stats);

	wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);
	wchar_t* pathTitle = FormNewString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_local_users[user_index].username);
	wchar_t* pathStats = FormNewString(L"%sstats.dat", pathTitle);
	delete[] pathConfig;
	pathConfig = 0;
	delete[] pathTitle;
	pathTitle = 0;

	FILE* fpStats;
	errno_t errorFileOpen = _wfopen_s(&fpStats, pathStats, L"rb");
	if (errorFileOpen) {
		// A user with no saved stats simply has no file yet - not an error.
		for (const auto &column : unpersistedColumns) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s view 0x%08x column %u is set in memory but there is no stats.dat for user %u; it survives the load and the next save persists it.", __func__, column.first, column.second, user_index);
		}
		delete[] pathStats;
		return ERROR_SUCCESS;
	}
	delete[] pathStats;
	pathStats = 0;

	XLLN_STATS_FILE_HEADER header = {};
	if (fread(&header, sizeof(header), 1, fpStats) != 1 || header.magic != XLLN_STATS_FILE_MAGIC || header.version != XLLN_STATS_FILE_VERSION) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats.dat header invalid (magic 0x%08x version %u) for user %u.", __func__, header.magic, header.version, user_index);
		fclose(fpStats);
		return ERROR_FILE_CORRUPT;
	}

	uint32_t recordsLoaded = 0;
	XLLN_STATS_FILE_RECORD record;
	while (fread(&record, sizeof(record), 1, fpStats) == 1) {
		if (record.length > XLLN_STATS_MAX_RECORD_BYTES) {
			// The cap only guards the allocation below; the length field still frames the record, so
			// step over the payload and keep the records after it instead of dropping them all.
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s record length %u exceeds cap; skipping record view 0x%08x column %u for user %u.", __func__, record.length, record.viewId, record.columnId, user_index);
			if (_fseeki64(fpStats, (int64_t)record.length, SEEK_CUR)) {
				break;
			}
			continue;
		}
		uint8_t* payload = 0;
		if (record.length) {
			payload = (uint8_t*)malloc(record.length);
			if (!payload) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s malloc(%u) failed; stopping load for user %u.", __func__, record.length, user_index);
				break;
			}
			if (fread(payload, 1, record.length, fpStats) != record.length) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s truncated payload for user %u; stopping load.", __func__, user_index);
				free(payload);
				break;
			}
		}

		XUSER_DATA value = {};
		value.type = record.type;
		bool store = true;
		if (record.type == XUSER_DATA_TYPE_UNICODE) {
			value.string.cbData = record.length;
			value.string.pwszData = (LPWSTR)payload;
		}
		else if (record.type == XUSER_DATA_TYPE_BINARY) {
			value.binary.cbData = record.length;
			value.binary.pbData = payload;
		}
		else {
			uint32_t width = XllnStatsScalarWidth(record.type);
			if (!width || record.length < width) {
				store = false;
			}
			else {
				memcpy(&value.nData, payload, width);
			}
		}

		if (store) {
			// Deep-copies into the store, so the temporary payload buffer is safe to free after.
			XllnStatsStoreValue(xuid, record.viewId, record.columnId, value);
			unpersistedColumns.erase(std::make_pair(record.viewId, record.columnId));
			recordsLoaded++;
		}
		else {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s skipped record view 0x%08x column %u type %hhu length %u for user %u.", __func__, record.viewId, record.columnId, record.type, record.length, user_index);
		}

		if (payload) {
			free(payload);
			payload = 0;
		}
	}

	fclose(fpStats);

	for (const auto &column : unpersistedColumns) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s view 0x%08x column %u is set in memory but absent from stats.dat for user %u; it survives the load and the next save persists it.", __func__, column.first, column.second, user_index);
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s loaded %u stat record(s) for user %u (xuid 0x%016I64x).", __func__, recordsLoaded, user_index, xuid);
	return ERROR_SUCCESS;
}

uint32_t XUserContextResetDefaults(uint32_t user_index)
{
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	
	EnterCriticalSection(&xlive_critsec_xuser_context_properties);
	
	xlive_user_contexts[user_index].clear();
	//xlive_user_contexts[user_index][X_CONTEXT_PRESENCE] = 0;
	
	LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	
	return true;
}

uint32_t XUserPropertyResetDefaults(uint32_t user_index)
{
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	
	EnterCriticalSection(&xlive_critsec_xuser_context_properties);
	
	auto &userProperties = xlive_user_properties[user_index];
	for (auto &itrUserProperty : userProperties) {
		if (itrUserProperty.second.type == XUSER_DATA_TYPE_UNICODE && itrUserProperty.second.string.pwszData) {
			delete[] itrUserProperty.second.string.pwszData;
			itrUserProperty.second.string.pwszData = 0;
		}
		else if (itrUserProperty.second.type == XUSER_DATA_TYPE_BINARY && itrUserProperty.second.binary.pbData) {
			delete[] itrUserProperty.second.binary.pbData;
			itrUserProperty.second.binary.pbData = 0;
		}
	}
	userProperties.clear();
	
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_NotSignedIn) {
		XUSER_DATA &propertyGamerPuid = userProperties[X_PROPERTY_GAMER_PUID];
		propertyGamerPuid.type = XPROPERTYTYPEFROMID(X_PROPERTY_GAMER_PUID);
		propertyGamerPuid.i64Data = xlive_local_users[user_index].xuid;
		
		XUSER_DATA &propertyGamerHostName = userProperties[X_PROPERTY_GAMER_HOSTNAME];
		propertyGamerHostName.type = XPROPERTYTYPEFROMID(X_PROPERTY_GAMER_HOSTNAME);
		size_t usernameLength = strnlen(xlive_local_users[user_index].username, sizeof(xlive_local_users[user_index].username) - 1) + 1;
		propertyGamerHostName.string.cbData = (uint32_t)(usernameLength * sizeof(wchar_t));
		propertyGamerHostName.string.pwszData = new wchar_t[usernameLength];
		swprintf_s(propertyGamerHostName.string.pwszData, usernameLength, L"%hs", xlive_local_users[user_index].username);
	}
	
	LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	
	return true;
}

// #5261
uint32_t WINAPI XUserGetXUID(uint32_t user_index, XUID* xuid)
{
	TRACE_FX();
	if (!xuid) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xuid is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	
	*xuid = xlive_local_users[user_index].xuid;
	return ERROR_SUCCESS;
}

// #5262
XUSER_SIGNIN_STATE WINAPI XUserGetSigninState(uint32_t user_index)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return eXUserSigninState_NotSignedIn;
	}
	return xlive_local_users[user_index].signin_state;
}

// #5263
uint32_t WINAPI XUserGetName(uint32_t user_index, char* result_username, size_t result_username_size)
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
	if (!result_username) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_username is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_username_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_username_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (result_username_size > XUSER_NAME_SIZE) {
		result_username_size = XUSER_NAME_SIZE;
	}
	
	memcpy(result_username, xlive_local_users[user_index].username, result_username_size);
	result_username[result_username_size - 1] = 0;
	return ERROR_SUCCESS;
}

// #5264
uint32_t WINAPI XUserAreUsersFriends(uint32_t user_index, XUID* user_xuids, size_t user_xuid_count, BOOL* result_are_all_friends, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to a LIVE account.", __func__, user_index);
		return ERROR_NOT_LOGGED_ON;
	}
	if (!user_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_xuid_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is not NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!result_are_all_friends) return ERROR_INVALID_PARAMETER;
    *result_are_all_friends = TRUE;
    for (size_t i = 0; i < user_xuid_count; ++i) if (!SocialIsFriend(user_xuids[i])) *result_are_all_friends = FALSE;
	
	return ERROR_SUCCESS;
}

// #5265
uint32_t WINAPI XUserCheckPrivilege(uint32_t user_index, XPRIVILEGE_TYPE privilege_type, BOOL* result_is_privileged)
{
	TRACE_FX();
	if (!result_is_privileged) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_is_privileged is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	*result_is_privileged = FALSE;
	if (privilege_type != XPRIVILEGE_MULTIPLAYER_SESSIONS
		&& privilege_type != XPRIVILEGE_COMMUNICATIONS
		&& privilege_type != XPRIVILEGE_COMMUNICATIONS_FRIENDS_ONLY
		&& privilege_type != XPRIVILEGE_PROFILE_VIEWING
		&& privilege_type != XPRIVILEGE_PROFILE_VIEWING_FRIENDS_ONLY
		&& privilege_type != XPRIVILEGE_USER_CREATED_CONTENT
		&& privilege_type != XPRIVILEGE_USER_CREATED_CONTENT_FRIENDS_ONLY
		&& privilege_type != XPRIVILEGE_PURCHASE_CONTENT
		&& privilege_type != XPRIVILEGE_PRESENCE
		&& privilege_type != XPRIVILEGE_PRESENCE_FRIENDS_ONLY
		&& privilege_type != XPRIVILEGE_TRADE_CONTENT
		&& privilege_type != XPRIVILEGE_VIDEO_COMMUNICATIONS
		&& privilege_type != XPRIVILEGE_VIDEO_COMMUNICATIONS_FRIENDS_ONLY
		&& privilege_type != XPRIVILEGE_MULTIPLAYER_DEDICATED_SERVER
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s privilege_type (%u) does not exist.", __func__, privilege_type);
		return ERROR_INVALID_PARAMETER;
	}
	// XUSER_INDEX_ANY names no seat, so rejecting it as out of range answers "not privileged".
	if (user_index == XUSER_INDEX_ANY) {
		uint32_t iUser = 0;
		for (; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
			if (xlive_local_users[iUser].signin_state != eXUserSigninState_NotSignedIn) {
				break;
			}
		}
		if (iUser == XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s privilege_type (%u) asked of any user while none is signed in.", __func__, privilege_type);
			return ERROR_NOT_LOGGED_ON;
		}
		user_index = iUser;
	}
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist (privilege_type (%u)).", __func__, user_index, privilege_type);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in (privilege_type (%u)).", __func__, user_index, privilege_type);
		return ERROR_NOT_LOGGED_ON;
	}
	
	*result_is_privileged = TRUE;
	
	return ERROR_SUCCESS;
}

// #5267
uint32_t WINAPI XUserGetSigninInfo(uint32_t user_index, uint32_t flags, XUSER_SIGNIN_INFO* result_signin_info)
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
	if (!result_signin_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_signin_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (flags & ~(XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY | XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY | XUSER_GET_SIGNIN_INFO_UNKNOWN_XUID_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags (0x%08x) is invalid.", __func__, flags);
		return ERROR_INVALID_PARAMETER;
	}
	if ((flags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY) && (flags & XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s flags cannot be XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY and XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	result_signin_info->xuid = xlive_local_users[user_index].xuid;
	memcpy_s(result_signin_info->szUserName, sizeof(result_signin_info->szUserName), xlive_local_users[user_index].username, sizeof(xlive_local_users[user_index].username));
	result_signin_info->UserSigninState = xlive_local_users[user_index].signin_state;
	result_signin_info->dwInfoFlags = xlive_local_users[user_index].live_enabled ? XUSER_INFO_FLAG_LIVE_ENABLED : 0;
	result_signin_info->dwGuestNumber = xlive_local_users[user_index].guest_number;
	result_signin_info->dwSponsorUserIndex = xlive_local_users[user_index].sponsor_user_index;
	
	// If the profile is a local/offline account which is not live enabled then there is no valid online variation of the XUID.
	if ((flags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY) && !xlive_local_users[user_index].live_enabled) {
		result_signin_info->xuid = INVALID_XUID;
	}
	else if ((flags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY) || (flags & XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY)) {
		result_signin_info->xuid = BuildXUID((result_signin_info->xuid & 0xFFFFFFFF), (flags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY), (flags & XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY), 0);
	}
	
	return ERROR_SUCCESS;
}

// #5274
uint32_t WINAPI XUserAwardGamerPicture(uint32_t user_index, uint32_t picture_id, void* reserved, XOVERLAPPED* xoverlapped)
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
	if (reserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s reserved is not 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
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

// #5278
uint32_t WINAPI XUserWriteAchievements(size_t achievement_count, const XUSER_ACHIEVEMENT* achievements, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (achievement_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievement_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!achievements) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievements is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	// Track which dwUserIds have achievements in pAchievements
	bool users_to_update[XLIVE_LOCAL_USER_COUNT] = {};
	for (size_t iAchievement = 0; iAchievement < achievement_count; iAchievement++) {
		if (achievements[iAchievement].dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, achievements[iAchievement].dwUserIndex);
			return ERROR_NO_SUCH_USER;
		}
		if (xlive_local_users[achievements[iAchievement].dwUserIndex].signin_state != eXUserSigninState_SignedInToLive) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to a LIVE account.", __func__, achievements[iAchievement].dwUserIndex);
			return ERROR_INVALID_OPERATION;
		}
		users_to_update[achievements[iAchievement].dwUserIndex] = true;
	}
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	uint32_t result = ERROR_SUCCESS;

	FILETIME ftAchieved;
	GetSystemTimeAsFileTime(&ftAchieved);

	wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);

	EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);

	// Do it one user at a time instead of potentially opening and closing the same files over and over
	for (DWORD iLocalUser = 0; iLocalUser < XLIVE_LOCAL_USER_COUNT; ++iLocalUser) {
		if (!users_to_update[iLocalUser]) {
			continue;
		}

		DWORD dwFlags = XACHIEVEMENT_DETAILS_ACHIEVED | XACHIEVEMENT_DETAILS_UNKNOWN;
		if (xlive_local_users[iLocalUser].online_enabled) {
			dwFlags |= XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE;
		}

		wchar_t* pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_local_users[iLocalUser].username);
		wchar_t* pathCow = FormMallocString(L"%sachievements.dat", pathTitle);
		uint32_t errorMkdir = EnsureDirectoryExists(pathTitle);
		if (errorMkdir) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, pathTitle);
			result = errorMkdir;
			free(pathCow);
			pathCow = 0;
			free(pathTitle);
			pathTitle = 0;
			break;
		}
		free(pathTitle);
		pathTitle = 0;

		FILE* fpCow;
		errno_t errorFileOpen = _wfopen_s(&fpCow, pathCow, L"a+b");
		if (errorFileOpen) {
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"a+b\") error:", __func__, pathCow);
			result = errorFileOpen;
			free(pathCow);
			pathCow = 0;
			break;
		}
		free(pathCow);
		pathCow = 0;
		fseek(fpCow, (long)0, SEEK_END);
		uint32_t fileSize = ftell(fpCow);
		fseek(fpCow, (long)0, SEEK_SET);
		fileSize -= ftell(fpCow);

		uint32_t cows_len = (sizeof(uint8_t) * fileSize) / sizeof(XLLN_XACHIEVEMENT_DETAILS_COW);
		XLLN_XACHIEVEMENT_DETAILS_COW* cows = (XLLN_XACHIEVEMENT_DETAILS_COW*)malloc(fileSize);
		size_t readC = fread(cows, sizeof(uint8_t), fileSize, fpCow);
		
		// Must set file position after fread if intending to follow with an fwrite.
		fseek(fpCow, fileSize, SEEK_SET);
		
		for (size_t iAchievement = 0; iAchievement < achievement_count; ++iAchievement) {
			// Not for this user
			if (achievements[iAchievement].dwUserIndex != iLocalUser) {
				continue;
			}

			// Check if we already achieved it
			size_t iCow = 0;
			for (; iCow < cows_len; ++iCow) {
				if (cows[iCow].dwAchievementId == achievements[iAchievement].dwAchievementId) {
					break;
				}
			}
			if (iCow != cows_len) {
				continue;
			}

			// Achievement get!
			XLLN_XACHIEVEMENT_DETAILS_COW xlln_xuser_achievement_cow = {
				achievements[iAchievement].dwAchievementId,
				ftAchieved,
				dwFlags,
			};
			fwrite(&xlln_xuser_achievement_cow, sizeof(xlln_xuser_achievement_cow), 1, fpCow);
		}
		free(cows);
		cows = 0;
		fclose(fpCow);
		fpCow = 0;
	}

	LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	delete[] pathConfig;
	pathConfig = 0;

	XllnWndAchievementsInvalidateAchievements();

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

uint32_t XUserEraseAchievements(size_t achievement_count, const XUSER_ACHIEVEMENT* achievements, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (achievement_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievement_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!achievements) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievements is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	// Track which dwUserIds have achievements in pAchievements
	bool users_to_update[XLIVE_LOCAL_USER_COUNT] = {};
	for (size_t iAchievement = 0; iAchievement < achievement_count; iAchievement++) {
		if (achievements[iAchievement].dwUserIndex >= XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, achievements[iAchievement].dwUserIndex);
			return ERROR_NO_SUCH_USER;
		}
		if (xlive_local_users[achievements[iAchievement].dwUserIndex].signin_state != eXUserSigninState_SignedInToLive) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in to a LIVE account.", __func__, achievements[iAchievement].dwUserIndex);
			return ERROR_INVALID_OPERATION;
		}
		users_to_update[achievements[iAchievement].dwUserIndex] = true;
	}
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}

	uint32_t result = ERROR_SUCCESS;

	wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);

	EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);

	// Do it one user at a time instead of potentially opening and closing the same files over and over
	for (DWORD iLocalUser = 0; iLocalUser < XLIVE_LOCAL_USER_COUNT; ++iLocalUser) {
		if (!users_to_update[iLocalUser]) {
			continue;
		}

		DWORD dwFlags = XACHIEVEMENT_DETAILS_ACHIEVED | XACHIEVEMENT_DETAILS_UNKNOWN;
		if (xlive_local_users[iLocalUser].online_enabled) {
			dwFlags |= XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE;
		}

		wchar_t* pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_local_users[iLocalUser].username);
		wchar_t* pathCow = FormMallocString(L"%sachievements.dat", pathTitle);
		uint32_t errorMkdir = EnsureDirectoryExists(pathTitle);
		if (errorMkdir) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, pathTitle);
			result = errorMkdir;
			free(pathCow);
			pathCow = 0;
			free(pathTitle);
			pathTitle = 0;
			break;
		}
		free(pathTitle);
		pathTitle = 0;

		FILE* fpCowR;
		errno_t errorFileOpen = _wfopen_s(&fpCowR, pathCow, L"rb");
		if (errorFileOpen) {
			if (errorFileOpen == ERROR_FILE_NOT_FOUND) {
				// If file doesn't exist then there are no achievements to be erased for this user.
				XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s fopen(\"%ls\", \"rb\") error:", __func__, pathCow);
				free(pathCow);
				pathCow = 0;
				continue;
			}
			else {
				XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s fopen(\"%ls\", \"rb\") error:", __func__, pathCow);
				result = errorFileOpen;
				free(pathCow);
				pathCow = 0;
				break;
			}
		}
		fseek(fpCowR, (long)0, SEEK_END);
		uint32_t fileSize = ftell(fpCowR);
		fseek(fpCowR, (long)0, SEEK_SET);
		fileSize -= ftell(fpCowR);

		uint32_t cows_len = (sizeof(uint8_t) * fileSize) / sizeof(XLLN_XACHIEVEMENT_DETAILS_COW);
		XLLN_XACHIEVEMENT_DETAILS_COW* cows = (XLLN_XACHIEVEMENT_DETAILS_COW*)malloc(fileSize);
		size_t readC = fread(cows, sizeof(uint8_t), fileSize, fpCowR);
		fclose(fpCowR);
		fpCowR = 0;

		FILE* fpCowW;
		errorFileOpen = _wfopen_s(&fpCowW, pathCow, L"wb");
		if (errorFileOpen) {
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"wb\") error:", __func__, pathCow);
			result = errorFileOpen;
			free(cows);
			cows = 0;
			free(pathCow);
			pathCow = 0;
			break;
		}
		free(pathCow);
		pathCow = 0;

		for (size_t iCow = 0; iCow < cows_len; ++iCow) {
			bool found = false;
			for (size_t iAchievement = 0; iAchievement < achievement_count; ++iAchievement) {
				// Not for this user
				if (achievements[iAchievement].dwUserIndex != iLocalUser) {
					continue;
				}

				if (cows[iCow].dwAchievementId == achievements[iAchievement].dwAchievementId) {
					found = true;
					break;
				}
			}

			if (found) {
				continue;
			}

			fwrite(&cows[iCow], sizeof(cows[iCow]), 1, fpCowW);
		}
		free(cows);
		cows = 0;
		fclose(fpCowW);
		fpCowW = 0;
	}

	LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	delete[] pathConfig;
	pathConfig = 0;

	XllnWndAchievementsInvalidateAchievements();

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

// #5279
uint32_t WINAPI XUserReadAchievementPicture(uint32_t user_index, uint32_t title_id, uint32_t image_id, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped)
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
	if (!texture_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s texture_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	return result;
}

// #5280
uint32_t WINAPI XUserCreateAchievementEnumerator(uint32_t title_id, uint32_t user_index, XUID user_xuid, uint32_t achievement_detail_flags, size_t starting_index, size_t result_item_count, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (user_index >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
		return ERROR_NO_SUCH_USER;
	}
	if (result_item_count > XACHIEVEMENT_MAX_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_item_count > XACHIEVEMENT_MAX_COUNT) (%zu > %u).", __func__, result_item_count, XACHIEVEMENT_MAX_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index >= XACHIEVEMENT_MAX_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s starting_index >= XACHIEVEMENT_MAX_COUNT) (%zu >= %u).", __func__, starting_index, XACHIEVEMENT_MAX_COUNT);
		return ERROR_INVALID_PARAMETER;
	}
	if (achievement_detail_flags == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievement_detail_flags is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (achievement_detail_flags != XACHIEVEMENT_DETAILS_ALL && achievement_detail_flags & ~(XACHIEVEMENT_DETAILS_LABEL | XACHIEVEMENT_DETAILS_DESCRIPTION | XACHIEVEMENT_DETAILS_UNACHIEVED | XACHIEVEMENT_DETAILS_TFC)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s achievement_detail_flags (0x%08x) is invalid.", __func__, achievement_detail_flags);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumerator_handle is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!title_id) {
		title_id = xlive_title_id;
	}
	
	char username[XUSER_NAME_SIZE] = {0};
	
	if (user_xuid == INVALID_XUID) {
		// Enumerate the local signed-in gamer's achievements.
		if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
			return ERROR_NOT_LOGGED_ON;
		}
		if (xlive_local_users[user_index].signin_state != eXUserSigninState_SignedInToLive) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u must be signed in to LIVE.", __func__, user_index);
			return ERROR_NOT_LOGGED_ON;
		}
		
		memcpy(username, xlive_local_users[user_index].username, XUSER_NAME_SIZE);
		username[XUSER_NAME_SIZE - 1] = 0;
	}
	else {
		if (!IsOnlineXUID(user_xuid) || IsGuestXUID(user_xuid)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid (0x%016I64x) must be an online account and also not a guest.", __func__, user_xuid);
			return ERROR_INVALID_PARAMETER;
		}
		
		// A signed-in user is never in the remote map, so an explicit XUID has to try the local ones.
		bool resolvedLocalUser = false;
		for (uint32_t iLocalUser = 0; iLocalUser < XLIVE_LOCAL_USER_COUNT; iLocalUser++) {
			if (xlive_local_users[iLocalUser].signin_state == eXUserSigninState_NotSignedIn) {
				continue;
			}
			if (xlive_local_users[iLocalUser].xuid != user_xuid) {
				continue;
			}

			memcpy(username, xlive_local_users[iLocalUser].username, XUSER_NAME_SIZE);
			username[XUSER_NAME_SIZE - 1] = 0;
			resolvedLocalUser = true;
			break;
		}

		if (!resolvedLocalUser) {
			EnterCriticalSection(&xlive_critsec_remote_user);

			auto itrRemoteUserUsername = xlln_remote_user_usernames.find(user_xuid);
			// An entry can exist with a NULL username before anything resolves the gamertag; copying from
			// it would crash the caller.
			if (itrRemoteUserUsername != xlln_remote_user_usernames.end() && itrRemoteUserUsername->second) {
				memcpy(username, itrRemoteUserUsername->second, XUSER_NAME_SIZE);
				username[XUSER_NAME_SIZE - 1] = 0;
			}

			LeaveCriticalSection(&xlive_critsec_remote_user);

			// A peer's unlocks are not held here, so an unresolved gamertag can only mean nothing earnt.
			// Failing instead strands titles that gate a lobby on reading every player.
			if (!username[0]) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
					, "%s user_xuid (0x%016I64x) has no resolved gamertag; enumerating with nothing unlocked."
					, __func__
					, user_xuid
				);
			}
		}
	}
	
	EnterCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	
	ACHIEVEMENT_ENUMERATION_DETAILS* achievementEnumerationDetails = new ACHIEVEMENT_ENUMERATION_DETAILS;
	achievementEnumerationDetails->enumeratorHandle = CreateMutex(NULL, NULL, NULL);
	achievementEnumerationDetails->titleId = title_id;
	achievementEnumerationDetails->detailFlags = achievement_detail_flags;
	achievementEnumerationDetails->index = 0;

	if (title_id == xlive_title_id) {
		size_t remaining_item_count = 0;
		if (starting_index < xlive_achievement_details.size()) {
			remaining_item_count = xlive_achievement_details.size() - starting_index;
		}
		result_item_count = min(remaining_item_count, result_item_count);
		achievementEnumerationDetails->achievementDetails = std::vector<XACHIEVEMENT_DETAILS>(xlive_achievement_details.begin() + starting_index, xlive_achievement_details.begin() + starting_index + result_item_count);

		// No gamertag means no per-user file, and the path would carry an empty directory segment.
		wchar_t* pathCow = 0;
		if (username[0]) {
			wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);
			pathCow = FormNewString(L"%sprofile/title/%08X/%hs/achievements.dat", pathConfig, xlive_title_id, username);
			delete[] pathConfig;
			pathConfig = 0;
		}

		FILE* fpCow = 0;
		errno_t errorFileOpen = pathCow ? _wfopen_s(&fpCow, pathCow, L"rb") : ENOENT;
		if (errorFileOpen) {
			if (pathCow) {
				XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s fopen(\"%ls\", \"r\") error:", __func__, pathCow);
				delete[] pathCow;
				pathCow = 0;
			}
		}
		else {
			delete[] pathCow;
			pathCow = 0;

			XLLN_XACHIEVEMENT_DETAILS_COW cow;
			while (fread(&cow, sizeof(cow), 1, fpCow) == 1) {
				for (DWORD i = 0; i < result_item_count; ++i) {
					if (achievementEnumerationDetails->achievementDetails[i].dwId == cow.dwAchievementId) {
						achievementEnumerationDetails->achievementDetails[i].dwFlags |= cow.dwFlags;
						achievementEnumerationDetails->achievementDetails[i].ftAchieved = cow.ftAchieved;
						break;
					}
				}
			}

			fclose(fpCow);
			fpCow = 0;
		}
	}

	uint32_t achievementDetailSize = sizeof(XACHIEVEMENT_DETAILS);
	if (achievement_detail_flags & XACHIEVEMENT_DETAILS_LABEL) {
		achievementDetailSize += XACHIEVEMENT_MAX_LABEL_LENGTH;
	}
	if (achievement_detail_flags & XACHIEVEMENT_DETAILS_DESCRIPTION) {
		achievementDetailSize += XACHIEVEMENT_MAX_DESC_LENGTH;
	}
	if (achievement_detail_flags & XACHIEVEMENT_DETAILS_UNACHIEVED) {
		achievementDetailSize += XACHIEVEMENT_MAX_UNACH_LENGTH;
	}
	
	*result_buffer_size = achievementDetailSize * result_item_count;
	*enumerator_handle = achievementEnumerationDetails->enumeratorHandle;
	xlive_xuser_achievement_enumerators[achievementEnumerationDetails->enumeratorHandle] = achievementEnumerationDetails;
	LeaveCriticalSection(&xlive_critsec_xuser_achievement_enumerators);
	
	return ERROR_SUCCESS;
}

// #5281
uint32_t WINAPI XUserReadStats(uint32_t title_id, size_t user_xuid_count, const XUID* user_xuids, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, XUSER_STATS_READ_RESULTS* result_stats, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_xuid_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (user_xuid_count > 0x65) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_count (0x%zx) is greater than 0x65.", __func__, user_xuid_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (!user_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (stats_spec_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_spec_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (stats_spec_count > XUSER_STATS_ATTRS_IN_SPEC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_spec_count (0x%zx) is greater than XUSER_STATS_ATTRS_IN_SPEC (0x%x).", __func__, stats_spec_count, XUSER_STATS_ATTRS_IN_SPEC);
		return ERROR_INVALID_PARAMETER;
	}
	if (!stats_specs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_specs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*result_buffer_size && !result_stats) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*result_buffer_size && !result_stats).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*result_buffer_size && result_stats) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!*result_buffer_size && result_stats).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t resultCode = ERROR_FUNCTION_FAILED;
	
	STATS_ENUMERATION_DETAILS statsEnumerationDetails;
	statsEnumerationDetails.enumeratorHandle = INVALID_HANDLE_VALUE;
	
	statsEnumerationDetails.enumerationStartRank = 0;
	statsEnumerationDetails.enumerationStartRating = 0;
	statsEnumerationDetails.enumerationPivotXuid = 0;
	
	statsEnumerationDetails.titleId = title_id;
	statsEnumerationDetails.maxRowCount = user_xuid_count;
	statsEnumerationDetails.statsSpecCount = stats_spec_count;
	// Only accessing in this scope. No need to allocate and copy.
	statsEnumerationDetails.statsSpecs = (XUSER_STATS_SPEC*)stats_specs;
	
	size_t statsResultsSize = XUserStatsReadResultsMinSize(&statsEnumerationDetails, false);
	if (*result_buffer_size < statsResultsSize || !result_stats) {
		if (result_stats && *result_buffer_size >= sizeof(XUSER_STATS_READ_RESULTS)) {
			result_stats->dwNumViews = 0;
			result_stats->pViews = 0;
		}
		*result_buffer_size = statsResultsSize;
		resultCode = ERROR_INSUFFICIENT_BUFFER;
	}
	else {
		resultCode = ERROR_SUCCESS;
		XUSER_STATS_READ_RESULTS* resultStats = result_stats;
		uint8_t* resultAdditionalData = ((uint8_t*)result_stats) + sizeof(XUSER_STATS_READ_RESULTS);
		resultStats->dwNumViews = statsEnumerationDetails.statsSpecCount;
		resultStats->pViews = (XUSER_STATS_VIEW*)resultAdditionalData;
		resultAdditionalData += sizeof(XUSER_STATS_VIEW) * resultStats->dwNumViews;
		for (uint32_t iSpec = 0; iSpec < resultStats->dwNumViews; iSpec++) {
			XUSER_STATS_SPEC &statsSpec = statsEnumerationDetails.statsSpecs[iSpec];
			XUSER_STATS_VIEW &statsView = resultStats->pViews[iSpec];
			
			statsView.dwViewId = statsSpec.dwViewId;
			statsView.dwTotalViewRows = statsEnumerationDetails.maxRowCount;
			statsView.dwNumRows = 0;
			statsView.pRows = (XUSER_STATS_ROW*)resultAdditionalData;
			resultAdditionalData += sizeof(XUSER_STATS_ROW) * statsView.dwTotalViewRows;
			
			for (uint8_t iXuid = 0; iXuid < statsView.dwTotalViewRows; iXuid++) {
				XUSER_STATS_ROW &statsRow = statsView.pRows[statsView.dwNumRows];
				
				statsRow.xuid = user_xuids[iXuid];
				statsRow.dwRank = 0;
				statsRow.i64Rating = 0;
				memset(statsRow.szGamertag, 0, sizeof(statsRow.szGamertag));
				if (statsRow.xuid == xlive_local_users[0].xuid) {
					memcpy_s(statsRow.szGamertag, sizeof(statsRow.szGamertag), xlive_local_users[0].username, sizeof(xlive_local_users[0].username));
				}
				statsRow.dwNumColumns = statsSpec.dwNumColumnIds;
				statsRow.pColumns = (XUSER_STATS_COLUMN*)resultAdditionalData;
				resultAdditionalData += sizeof(XUSER_STATS_COLUMN) * statsRow.dwNumColumns;
				
				for (uint8_t iColumn = 0; iColumn < statsRow.dwNumColumns; iColumn++) {
					XUSER_STATS_COLUMN &statsColumn = statsRow.pColumns[iColumn];
					
					statsColumn.wColumnId = statsSpec.rgwColumnIds[iColumn];
					memset(&statsColumn.Value, 0, sizeof(XUSER_DATA));
					
					// A value the title actually wrote is authoritative, so consult the store BEFORE the
					// TrueSkill defaults below: those defaults are keyed by bare columnId (61-64) and a title
					// is free to reuse those ids for its own stats.
					bool foundStat = false;
					EnterCriticalSection(&xlive_critsec_xuser_stats);
					auto itrStatUser = xlive_written_stats.find(statsRow.xuid);
					if (itrStatUser != xlive_written_stats.end()) {
						auto itrStatView = itrStatUser->second.find(statsSpec.dwViewId);
						if (itrStatView != itrStatUser->second.end()) {
							uint32_t storeColumnId = XllnStatsResolveColumnId(statsSpec.dwViewId, statsColumn.wColumnId);
							auto itrStatColumn = itrStatView->second.find(storeColumnId);
							// Serve scalar types only: the result buffer reserves no payload space for
							// UNICODE/BINARY (see XUserStatsReadResultsMinSize), and serving a pointer into
							// the store would dangle once the next write to that cell frees the buffer.
							if (itrStatColumn != itrStatView->second.end()
								&& itrStatColumn->second.type != XUSER_DATA_TYPE_UNICODE
								&& itrStatColumn->second.type != XUSER_DATA_TYPE_BINARY) {
								statsColumn.Value = itrStatColumn->second;
								foundStat = true;
							}
						}
					}
					LeaveCriticalSection(&xlive_critsec_xuser_stats);

					if (foundStat) {
						XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG, "%s view 0x%08x column %hu served stored value 0x%016I64x (type %hhu).", __func__, statsSpec.dwViewId, statsColumn.wColumnId, statsColumn.Value.i64Data, statsColumn.Value.type);
					}
					else {
						// Nothing stored: seed the standard TrueSkill leaderboard columns with the values Live
						// gives a new player (a title never writes these itself), and report any other unwritten
						// column "not set" (NULL), which the title skips like an un-earned stat.
						// Each case must set its own type: the memset leaves 0, which is a valid CONTEXT type
						// rather than an unset marker, so a title switching on it would read the wrong union member.
						switch (statsColumn.wColumnId) {
							case X_STATS_COLUMN_SKILL_SKILL: {
								statsColumn.Value.i64Data = X_STATS_SKILL_SKILL_DEFAULT;
								statsColumn.Value.type = XUSER_DATA_TYPE_INT64;
								break;
							}
							case X_STATS_COLUMN_SKILL_GAMESPLAYED: {
								statsColumn.Value.i64Data = 0;
								statsColumn.Value.type = XUSER_DATA_TYPE_INT64;
								break;
							}
							case X_STATS_COLUMN_SKILL_MU: {
								statsColumn.Value.dblData = X_STATS_SKILL_MU_DEFAULT;
								statsColumn.Value.type = XUSER_DATA_TYPE_DOUBLE;
								break;
							}
							case X_STATS_COLUMN_SKILL_SIGMA: {
								statsColumn.Value.dblData = X_STATS_SKILL_SIGMA_DEFAULT;
								statsColumn.Value.type = XUSER_DATA_TYPE_DOUBLE;
								break;
							}
							default: {
								statsColumn.Value.type = XUSER_DATA_TYPE_NULL;
								break;
							}
						}
					}
				}
				statsView.dwNumRows++;
			}
		}
		
		if (resultAdditionalData > ((uint8_t*)result_stats) + *result_buffer_size) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL, "%s buffer overflow. Allocated (0x%zx) bytes, consumed (0x%zx) bytes.", __func__, *result_buffer_size, (uint32_t)(resultAdditionalData - ((uint8_t*)result_stats)));
			__debugbreak();
		}
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = resultCode;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}

// #5273
uint32_t WINAPI XUserReadGamerPictureByKey(const XUSER_DATA* gamercard_picture_key, BOOL small_picture, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!gamercard_picture_key) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s gamercard_picture_key is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!texture_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s texture_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_FUNCTION_FAILED;
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	//synchronous
	return ERROR_SUCCESS;
}

// #5282
uint32_t WINAPI XUserReadGamerPicture(uint32_t user_index, BOOL small_picture, uint8_t* texture_buffer, uint32_t texture_pitch, uint32_t texture_height, XOVERLAPPED* xoverlapped)
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
	if (!texture_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s texture_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return ERROR_FUNCTION_FAILED;
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = ERROR_SUCCESS;
		xoverlapped->InternalHigh = ERROR_SUCCESS;
		xoverlapped->dwExtendedError = ERROR_SUCCESS;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	//synchronous
	return ERROR_SUCCESS;
}

size_t XUserStatsReadResultsMinSize(const STATS_ENUMERATION_DETAILS* statsEnumerationDetails, bool only_one_row)
{
	size_t statsResultsSize = 
		sizeof(XUSER_STATS_READ_RESULTS)
		+ (
			statsEnumerationDetails->statsSpecCount * (
				sizeof(XUSER_STATS_VIEW)
				+ (
					sizeof(XUSER_STATS_ROW) * (only_one_row ? 1 : statsEnumerationDetails->maxRowCount)
				)
			)
		);
	
	for (size_t iSpec = 0; iSpec < statsEnumerationDetails->statsSpecCount; iSpec++) {
		XUSER_STATS_SPEC &statsSpec = statsEnumerationDetails->statsSpecs[iSpec];
		// XUSER_DATA_TYPE_UNICODE && XUSER_DATA_TYPE_BINARY not supported so no need to allocate for them.
		//FIXME: GFWL disassembly disagrees with this guess of `sizeof(XUSER_STATS_COLUMN)` which is 24 not 28. We may be missing something.
		statsResultsSize += sizeof(XUSER_STATS_COLUMN) * statsSpec.dwNumColumnIds * (only_one_row ? 1 : statsEnumerationDetails->maxRowCount);
	}
	
	return statsResultsSize;
}

uint32_t __stdcall XUserCreateStatsEnumerator(uint32_t enumeration_rank_start, uint64_t enumeration_rating, XUID enumeration_xuid_pivot, uint32_t title_id, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (stats_row_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_row_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (stats_row_count > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_row_count (0x%zx) is greater than 0x64.", __func__, stats_row_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (stats_spec_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_spec_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (stats_spec_count > XUSER_STATS_ATTRS_IN_SPEC) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_spec_count (0x%zx) is greater than XUSER_STATS_ATTRS_IN_SPEC (0x%x).", __func__, stats_spec_count, XUSER_STATS_ATTRS_IN_SPEC);
		return ERROR_INVALID_PARAMETER;
	}
	if (!stats_specs) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s stats_specs is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!enumerator_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ph is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!title_id) {
		title_id = xlive_title_id;
	}
	
	STATS_ENUMERATION_DETAILS* statsEnumerationDetails = new STATS_ENUMERATION_DETAILS;
	statsEnumerationDetails->enumeratorHandle = CreateMutex(NULL, NULL, NULL);
	
	statsEnumerationDetails->enumerationStartRank = enumeration_rank_start;
	statsEnumerationDetails->enumerationStartRating = enumeration_rating;
	statsEnumerationDetails->enumerationPivotXuid = enumeration_xuid_pivot;
	
	statsEnumerationDetails->titleId = title_id;
	statsEnumerationDetails->maxRowCount = stats_row_count;
	statsEnumerationDetails->statsSpecCount = stats_spec_count;
	statsEnumerationDetails->statsSpecs = new XUSER_STATS_SPEC[statsEnumerationDetails->statsSpecCount];
	memcpy(statsEnumerationDetails->statsSpecs, stats_specs, sizeof(XUSER_STATS_SPEC) * statsEnumerationDetails->statsSpecCount);
	
	size_t statsResultsSize = XUserStatsReadResultsMinSize(statsEnumerationDetails, false);
	
	*result_buffer_size = statsResultsSize;
	*enumerator_handle = statsEnumerationDetails->enumeratorHandle;
	EnterCriticalSection(&xlive_critsec_xuser_stats);
	xlive_xuser_stats_enumerators[statsEnumerationDetails->enumeratorHandle] = statsEnumerationDetails;
	LeaveCriticalSection(&xlive_critsec_xuser_stats);
	
	return ERROR_SUCCESS;
}

// #5284
uint32_t WINAPI XUserCreateStatsEnumeratorByRank(uint32_t title_id, uint32_t enumeration_rank_start, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (enumeration_rank_start == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_rank_start is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	return XUserCreateStatsEnumerator(enumeration_rank_start, 0, 0, title_id, stats_row_count, stats_spec_count, stats_specs, result_buffer_size, enumerator_handle);
}

// #5285
uint32_t WINAPI XUserCreateStatsEnumeratorByRating(uint32_t title_id, uint64_t enumeration_rating, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	
	return XUserCreateStatsEnumerator(0, enumeration_rating, 0, title_id, stats_row_count, stats_spec_count, stats_specs, result_buffer_size, enumerator_handle);
}

// #5286
uint32_t WINAPI XUserCreateStatsEnumeratorByXuid(uint32_t title_id, XUID enumeration_xuid_pivot, size_t stats_row_count, size_t stats_spec_count, const XUSER_STATS_SPEC* stats_specs, size_t* result_buffer_size, HANDLE* enumerator_handle)
{
	TRACE_FX();
	if (!enumeration_xuid_pivot) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s enumeration_xuid_pivot is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	return XUserCreateStatsEnumerator(0, 0, enumeration_xuid_pivot, title_id, stats_row_count, stats_spec_count, stats_specs, result_buffer_size, enumerator_handle);
}

// #5287
uint32_t WINAPI XUserResetStatsView(uint32_t user_index, uint32_t view_id, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (user_index != XUSER_INDEX_NONE) {
		if (user_index >= XLIVE_LOCAL_USER_COUNT) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index);
			return ERROR_NO_SUCH_USER;
		}
		if (xlive_local_users[user_index].signin_state == eXUserSigninState_NotSignedIn) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index);
			return ERROR_NOT_LOGGED_ON;
		}
	}
	
	//TODO: if user_index == XUSER_INDEX_NONE then delete ALL user stats in specified view.
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s is only used in development to delete stats views.", __func__);
	uint32_t result = ERROR_FUNCTION_FAILED;
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	//synchronous
	return result;
}

// #5288
uint32_t WINAPI XUserGetProperty(uint32_t user_index, size_t* result_property_buffer_size, XUSER_PROPERTY* result_property_buffer, XOVERLAPPED* xoverlapped)
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
	if (!result_property_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_property_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t resultCode = ERROR_FUNCTION_FAILED;
	
	if (*result_property_buffer_size < sizeof(XUSER_PROPERTY) || !result_property_buffer) {
		*result_property_buffer_size = sizeof(XUSER_PROPERTY);
		resultCode = ERROR_INSUFFICIENT_BUFFER;
	}
	else {
		uint8_t dataType = XPROPERTYTYPEFROMID(result_property_buffer->dwPropertyId);
		bool isSystemProperty = !!XISSYSTEMPROPERTY(result_property_buffer->dwPropertyId);
		
		memset(&result_property_buffer->value, 0, sizeof(result_property_buffer->value));
		result_property_buffer->value.type = dataType;
		
		{
			EnterCriticalSection(&xlive_critsec_xuser_context_properties);
			
			const auto &userProperties = xlive_user_properties[user_index];
			const auto &itrProperties = userProperties.find(result_property_buffer->dwPropertyId);
			if (itrProperties != userProperties.end()) {
				result_property_buffer->value = itrProperties->second;
				
				resultCode = ERROR_SUCCESS;
				if (dataType == XUSER_DATA_TYPE_BINARY || dataType == XUSER_DATA_TYPE_BINARY) {
					size_t sizeRequiredData = ((dataType == XUSER_DATA_TYPE_BINARY) ? result_property_buffer->value.binary.cbData : result_property_buffer->value.string.cbData);
					size_t sizeRequiredTotal = sizeof(XUSER_PROPERTY) + sizeRequiredData;
					
					if (sizeRequiredTotal < *result_property_buffer_size) {
						*result_property_buffer_size = sizeRequiredTotal;
						resultCode = ERROR_INSUFFICIENT_BUFFER;
					}
					else {
						uint8_t* additionalData = ((uint8_t*)result_property_buffer) + sizeof(XUSER_PROPERTY);
						memcpy(additionalData, (dataType == XUSER_DATA_TYPE_BINARY ? itrProperties->second.binary.pbData : (const uint8_t*)itrProperties->second.string.pwszData), sizeRequiredData);
					}
				}
			}
			else {
				if (isSystemProperty) {
					*result_property_buffer_size = 0;
					resultCode = S_FALSE;
				}
				else {
					resultCode = ERROR_NOT_FOUND;
				}
			}
			
			LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
		}
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = resultCode;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}

// #5289
uint32_t WINAPI XUserGetContext(uint32_t user_index, XUSER_CONTEXT* result_context, XOVERLAPPED* xoverlapped)
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
	if (!result_context) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_context is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t resultCode = ERROR_NOT_FOUND;
	
	{
		EnterCriticalSection(&xlive_critsec_xuser_context_properties);
		
		const auto &userContexts = xlive_user_contexts[user_index];
		const auto &itrContexts = userContexts.find(result_context->dwContextId);
		if (itrContexts != userContexts.end()) {
			result_context->dwValue = itrContexts->second;
			resultCode = ERROR_SUCCESS;
		}
		
		LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	}
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = resultCode;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}

// #5290
float WINAPI XUserGetReputationStars(float gamer_rating)
{
	TRACE_FX();
	if (gamer_rating >= 100.0) {
		return 5.0;
	}
	if (gamer_rating > 0.0) {
		return (float)(ceil((double)gamer_rating / 5.0) * 0.25);
	}
	return 0.0;
}

// #5291
uint32_t WINAPI XUserResetStatsViewAllUsers(uint32_t view_id, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	return XUserResetStatsView(XUSER_INDEX_NONE, view_id, xoverlapped);
}

// #5292
uint32_t WINAPI XUserSetContextEx(uint32_t user_index, uint32_t context_id, uint32_t context_value, XOVERLAPPED* xoverlapped)
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
	if ((context_id & X_PROPERTY_SCOPE_MASK)
		&& context_id != X_CONTEXT_PRESENCE
		&& context_id != X_CONTEXT_GAME_TYPE
		&& context_id != X_CONTEXT_GAME_MODE
		&& context_id != X_CONTEXT_SESSION_JOINABLE
		&& context_id != X_CONTEXT_GAME_TYPE_RANKED
		&& context_id != X_CONTEXT_GAME_TYPE_STANDARD
	) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s context_id X_CONTEXT (0x%08x) is invalid.", __func__, context_id);
		return ERROR_INVALID_PARAMETER;
	}
	
	{
		EnterCriticalSection(&xlive_critsec_xuser_context_properties);

		xlive_user_contexts[user_index][context_id] = context_value;

		LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	}

	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
		, "%s user (%u) context (0x%08x) value (0x%08x)."
		, __func__
		, user_index
		, context_id
		, context_value
	);
	
	// Game type and game mode fix what a session is when XSessionCreate copies them, and must not
	// follow the user's later changes. A title running a party session alongside a matchmaking session
	// gives each its own game mode by setting the context between the two creates; carrying a later
	// change into both would collapse that separation and expose the party session to searches.
	if (context_id != X_CONTEXT_GAME_TYPE && context_id != X_CONTEXT_GAME_MODE) {
		EnterCriticalSection(&xlive_critsec_xsession);
		// Every session the user is in, not just the first: a title may hold several at once, and which
		// one a map keyed by handle yields first says nothing about which one wanted the change.
		for (auto const &xsession : xlive_xsession_local_sessions) {
			LIVE_SESSION_XSESSION* xsessionDetails = xsession.second;
			
			// Attributes set between XSessionCreate and the local join would otherwise be dropped.
			const bool isSessionMember = xsessionDetails->sessionMembers.find(xlive_local_users[user_index].xuid) != xsessionDetails->sessionMembers.end();
			if (!isSessionMember && !(xsessionDetails->liveSession->sessionFlags & XSESSION_CREATE_HOST)) {
				continue;
			}
			
			uint32_t iContext = 0;
			// Update the context if it already exits.
			for (; iContext < xsessionDetails->liveSession->contextsCount; iContext++) {
				if (xsessionDetails->liveSession->pContexts[iContext].dwContextId == context_id) {
					xsessionDetails->liveSession->pContexts[iContext].dwValue = context_value;
					break;
				}
			}
			// Add the context if it does not exist.
			if (iContext == xsessionDetails->liveSession->contextsCount) {
				XUSER_CONTEXT* contextsOld = xsessionDetails->liveSession->pContexts;
				xsessionDetails->liveSession->pContexts = new XUSER_CONTEXT[++xsessionDetails->liveSession->contextsCount];
				
				uint32_t iContext = 0;
				for (; iContext < xsessionDetails->liveSession->contextsCount - 1; iContext++) {
					xsessionDetails->liveSession->pContexts[iContext] = contextsOld[iContext];
				}
				xsessionDetails->liveSession->pContexts[iContext].dwContextId = context_id;
				xsessionDetails->liveSession->pContexts[iContext].dwValue = context_value;
				
				delete[] contextsOld;
				contextsOld = 0;
			}
		}
		LeaveCriticalSection(&xlive_critsec_xsession);
	}
	
	uint32_t resultCode = ERROR_SUCCESS;
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = resultCode;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}

// #5277
void WINAPI XUserSetContext(uint32_t user_index, uint32_t context_id, uint32_t context_value)
{
	TRACE_FX();
	XUserSetContextEx(user_index, context_id, context_value, NULL);
}

// #5293
uint32_t WINAPI XUserSetPropertyEx(uint32_t user_index, uint32_t property_id, size_t property_value_buffer_size, const void* property_value_buffer, XOVERLAPPED* xoverlapped)
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
	if (property_value_buffer_size == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_value_buffer_size is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!property_value_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_value_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!XLivepIsPropertyIdValid(property_id, TRUE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_id (0x%08x) is invalid.", __func__, property_id);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t propertyType = XPROPERTYTYPEFROMID(property_id);
	{
		size_t propertySize = 0;
		switch(propertyType) {
			case XUSER_DATA_TYPE_INT32: {
				propertySize = sizeof(XUSER_DATA::nData);
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				propertySize = sizeof(XUSER_DATA::i64Data);
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				propertySize = sizeof(XUSER_DATA::dblData);
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				propertySize = sizeof(XUSER_DATA::fData);
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				propertySize = sizeof(XUSER_DATA::ftData);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				if (property_value_buffer_size % sizeof(*XUSER_DATA::string.pwszData) != 0) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_value_buffer_size (0x%zx) is not valid and must be a multiple of sizeof(*XUSER_DATA::string.pwszData) (0x%08x) for property_id (0x%08x).", __func__, property_value_buffer_size, sizeof(*XUSER_DATA::string.pwszData), property_id);
					return ERROR_INVALID_PARAMETER;
				}
				if (((wchar_t*)property_value_buffer)[(property_value_buffer_size/sizeof(*XUSER_DATA::string.pwszData)) - 1] != 0) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_id (0x%08x) string value is not null terminated.", __func__, property_id);
					return ERROR_INVALID_PARAMETER;
				}
			}
			case XUSER_DATA_TYPE_BINARY: {
				propertySize = (size_t)-1;
				break;
			}
		}
		
		if (propertySize == 0) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_id (0x%08x) is invalid.", __func__, property_id);
			return ERROR_INVALID_PARAMETER;
		}
		if (propertySize != (size_t)-1 && property_value_buffer_size != propertySize) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s property_value_buffer_size (0x%zx) is incorrect (needs 0x%zx) for property_id (0x%08x).", __func__, property_value_buffer_size, propertySize, property_id);
			return ERROR_INVALID_PARAMETER;
		}
	}
	
	uint32_t resultCode = ERROR_SUCCESS;
	
	{
		EnterCriticalSection(&xlive_critsec_xuser_context_properties);
		
		auto &userProperties = xlive_user_properties[user_index];
		const auto &itrProperties = userProperties.find(property_id);
		XUSER_DATA &userProperty = (itrProperties != userProperties.end()) ? itrProperties->second : userProperties[property_id];
		userProperty.type = propertyType;
		
		switch(propertyType) {
			case XUSER_DATA_TYPE_INT32: {
				memcpy(&userProperty.nData, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				memcpy(&userProperty.i64Data, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				memcpy(&userProperty.dblData, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				memcpy(&userProperty.fData, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				memcpy(&userProperty.ftData, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				userProperty.string.cbData = property_value_buffer_size;
				userProperty.string.pwszData = new wchar_t[property_value_buffer_size/sizeof(*XUSER_DATA::string.pwszData)];
				memcpy(userProperty.string.pwszData, property_value_buffer, property_value_buffer_size);
				break;
			}
			case XUSER_DATA_TYPE_BINARY: {
				userProperty.binary.cbData = property_value_buffer_size;
				userProperty.binary.pbData = new uint8_t[property_value_buffer_size];
				memcpy(userProperty.binary.pbData, property_value_buffer, property_value_buffer_size);
				break;
			}
		}

		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
			, "%s user (%u) property (0x%08x) type (0x%02hhx) size (0x%zx) value (0x%016llx)."
			, __func__
			, user_index
			, property_id
			, propertyType
			, property_value_buffer_size
			, (propertyType == XUSER_DATA_TYPE_INT32) ? (uint64_t)(uint32_t)userProperty.nData : (uint64_t)userProperty.i64Data
		);

		LeaveCriticalSection(&xlive_critsec_xuser_context_properties);
	}

	// A property set after XSessionCreate has to reach the advertisement too, as a context already
	// does, and every session the user is in rather than whichever one a map keyed by handle yields
	// first. Unlike game type and mode there is no creation-time rule here: a title advertises a live
	// player count and match status this way, and a search filters on them.
	{
		EnterCriticalSection(&xlive_critsec_xsession);

		for (auto const &xsession : xlive_xsession_local_sessions) {
			LIVE_SESSION_XSESSION* xsessionDetails = xsession.second;

			// Attributes set between XSessionCreate and the local join would otherwise be dropped.
			const bool isSessionMember = xsessionDetails->sessionMembers.find(xlive_local_users[user_index].xuid) != xsessionDetails->sessionMembers.end();
			if (!isSessionMember && !(xsessionDetails->liveSession->sessionFlags & XSESSION_CREATE_HOST)) {
				continue;
			}

			uint32_t iProperty = 0;
			for (; iProperty < xsessionDetails->liveSession->propertiesCount; iProperty++) {
				if (xsessionDetails->liveSession->pProperties[iProperty].dwPropertyId == property_id) {
					break;
				}
			}

			if (iProperty == xsessionDetails->liveSession->propertiesCount) {
				XUSER_PROPERTY* propertiesOld = xsessionDetails->liveSession->pProperties;
				xsessionDetails->liveSession->pProperties = new XUSER_PROPERTY[++xsessionDetails->liveSession->propertiesCount];

				for (uint32_t iPropertyCopy = 0; iPropertyCopy < xsessionDetails->liveSession->propertiesCount - 1; iPropertyCopy++) {
					xsessionDetails->liveSession->pProperties[iPropertyCopy] = propertiesOld[iPropertyCopy];
				}

				delete[] propertiesOld;
				propertiesOld = 0;

				// The release below reads the type, so an uninitialised slot must describe an empty value.
				xsessionDetails->liveSession->pProperties[iProperty].dwPropertyId = property_id;
				xsessionDetails->liveSession->pProperties[iProperty].value.type = XUSER_DATA_TYPE_NULL;
			}

			XUSER_PROPERTY &propertyLive = xsessionDetails->liveSession->pProperties[iProperty];

			// The session owns its copy; it and the per-user entry are released independently.
			if (propertyLive.value.type == XUSER_DATA_TYPE_UNICODE) {
				delete[] propertyLive.value.string.pwszData;
				propertyLive.value.string.pwszData = 0;
			}
			else if (propertyLive.value.type == XUSER_DATA_TYPE_BINARY && propertyLive.value.binary.cbData) {
				delete[] propertyLive.value.binary.pbData;
				propertyLive.value.binary.pbData = 0;
			}

			propertyLive.value.type = propertyType;

			switch (propertyType) {
				case XUSER_DATA_TYPE_INT32: {
					memcpy(&propertyLive.value.nData, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_INT64: {
					memcpy(&propertyLive.value.i64Data, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_DOUBLE: {
					memcpy(&propertyLive.value.dblData, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_FLOAT: {
					memcpy(&propertyLive.value.fData, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_DATETIME: {
					memcpy(&propertyLive.value.ftData, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_UNICODE: {
					propertyLive.value.string.cbData = property_value_buffer_size;
					propertyLive.value.string.pwszData = new wchar_t[property_value_buffer_size/sizeof(*XUSER_DATA::string.pwszData)];
					memcpy(propertyLive.value.string.pwszData, property_value_buffer, property_value_buffer_size);
					break;
				}
				case XUSER_DATA_TYPE_BINARY: {
					propertyLive.value.binary.cbData = property_value_buffer_size;
					propertyLive.value.binary.pbData = new uint8_t[property_value_buffer_size];
					memcpy(propertyLive.value.binary.pbData, property_value_buffer, property_value_buffer_size);
					break;
				}
			}
		}

		LeaveCriticalSection(&xlive_critsec_xsession);
	}

	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = resultCode;
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}

// #5276
void WINAPI XUserSetProperty(uint32_t user_index, uint32_t property_id, size_t property_value_buffer_size, const void* property_value_buffer)
{
	TRACE_FX();
	XUserSetPropertyEx(user_index, property_id, property_value_buffer_size, property_value_buffer, NULL);
}

static uint32_t GetProfileSettingsBufferSize(
	uint32_t title_id
	, size_t read_for_num_of_xuids
	, size_t num_setting_ids
	, const uint32_t* setting_ids
	, size_t* result_buffer_size
	, bool has_result_profile_settings
)
{
	TRACE_FX();
	if (num_setting_ids == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s num_setting_ids is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (num_setting_ids > 0x20) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s num_setting_ids (0x%zx) is greater than 0x20.", __func__, num_setting_ids);
		return ERROR_INVALID_PARAMETER;
	}
	if (!setting_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s setting_ids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*result_buffer_size && !has_result_profile_settings) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*result_buffer_size && !result_profile_settings).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ValidateSettingIds(title_id, num_setting_ids, setting_ids);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ValidateSettingIds(...) error:", __func__);
		return result;
	}
	
	size_t settingBufferRequiredSize = 0;
	for (uint32_t i = 0; i < num_setting_ids; i++) {
		uint8_t dataType;
		uint16_t dataSize;
		GetXProfileSettingInfo(setting_ids[i], &dataType, &dataSize, 0, 0);
		settingBufferRequiredSize += sizeof(XUSER_PROFILE_SETTING);
		if (dataType == XUSER_DATA_TYPE_UNICODE || dataType == XUSER_DATA_TYPE_BINARY) {
			settingBufferRequiredSize += dataSize;
		}
	}
	settingBufferRequiredSize *= read_for_num_of_xuids;
	settingBufferRequiredSize += sizeof(XUSER_READ_PROFILE_SETTING_RESULT);
	
	if (!has_result_profile_settings || settingBufferRequiredSize > *result_buffer_size) {
		if (has_result_profile_settings) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO, "%s ERROR_INSUFFICIENT_BUFFER Had 0x%zx, requires 0x%zx.", __func__, *result_buffer_size, settingBufferRequiredSize);
		}
		*result_buffer_size = settingBufferRequiredSize;
		return ERROR_INSUFFICIENT_BUFFER;
	}
	
	return ERROR_SUCCESS;
}

static const wchar_t* fileNameSetting = L"%s%08X.dat";

// #5337
uint32_t WINAPI XUserWriteProfileSettings(uint32_t user_index, size_t profile_setting_count, const XUSER_PROFILE_SETTING* profile_settings, XOVERLAPPED* xoverlapped)
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
	if (profile_setting_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s profile_setting_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (profile_setting_count == (size_t)-1) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s profile_setting_count is -1.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!profile_settings) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s profile_settings is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	uint32_t result = ValidateSettings(0, profile_setting_count, profile_settings);
	if (result) {
		XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s ValidateSettings(...) error:", __func__);
		return result;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}
	
	wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);
	wchar_t* pathUser = FormMallocString(L"%sprofile/user/%hs/", pathConfig, xlive_local_users[user_index].username);
	wchar_t* pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, xlive_local_users[user_index].username);
	delete[] pathConfig;
	pathConfig = 0;
	
	for (size_t iSetting = 0; iSetting < profile_setting_count; iSetting++) {
		uint8_t dataType;
		uint16_t dataSize;
		bool isTitleSetting;
		bool canWrite;
		GetXProfileSettingInfo(profile_settings[iSetting].dwSettingId, &dataType, &dataSize, &isTitleSetting, &canWrite);
		
		const wchar_t* pathForSetting = isTitleSetting ? pathTitle : pathUser;
		wchar_t* filePathSetting = FormMallocString(fileNameSetting, pathForSetting, profile_settings[iSetting].dwSettingId);
		
		uint32_t errorMkdir = EnsureDirectoryExists(pathForSetting);
		if (errorMkdir) {
			XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "%s EnsureDirectoryExists(...) error on path \"%ls\"."
				, __func__
				, pathForSetting
			);
			result = errorMkdir;
			free(filePathSetting);
			filePathSetting = 0;
			break;
		}
		
		size_t fileSize = dataSize;
		uint8_t* dataLocation = 0;
		switch (dataType) {
			case XUSER_DATA_TYPE_INT32: {
				dataLocation = (uint8_t*)&profile_settings[iSetting].data.nData;
				break;
			}
			case XUSER_DATA_TYPE_INT64: {
				dataLocation = (uint8_t*)&profile_settings[iSetting].data.i64Data;
				break;
			}
			case XUSER_DATA_TYPE_DOUBLE: {
				dataLocation = (uint8_t*)&profile_settings[iSetting].data.dblData;
				break;
			}
			case XUSER_DATA_TYPE_FLOAT: {
				dataLocation = (uint8_t*)&profile_settings[iSetting].data.fData;
				break;
			}
			case XUSER_DATA_TYPE_DATETIME: {
				dataLocation = (uint8_t*)&profile_settings[iSetting].data.ftData;
				break;
			}
			case XUSER_DATA_TYPE_UNICODE: {
				dataLocation = (uint8_t*)profile_settings[iSetting].data.string.pwszData;
				fileSize = profile_settings[iSetting].data.string.cbData;
				break;
			}
			case XUSER_DATA_TYPE_BINARY: {
				dataLocation = (uint8_t*)profile_settings[iSetting].data.binary.pbData;
				fileSize = profile_settings[iSetting].data.binary.cbData;
				break;
			}
		}
		
		if (fileSize > dataSize) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "%s Setting (0x%08x) contents (size %u) is too big and will be truncated (max size %u)."
				, __func__
				, profile_settings[iSetting].dwSettingId
				, fileSize
				, dataSize
			);
			fileSize = dataSize;
		}
		
		if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize < sizeof(wchar_t)) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Invalid Setting (0x%08x) UNICODE fileSize (%u) must be greater than 2."
				, __func__
				, profile_settings[iSetting].dwSettingId
				, fileSize
			);
			result = ERROR_FILE_INVALID;
			break;
		}
		if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize % 2) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s Invalid Setting (0x%08x) UNICODE/wchar_t fileSize is not multiple of 2 (%u)."
				, __func__
				, profile_settings[iSetting].dwSettingId
				, fileSize
			);
			result = ERROR_FILE_INVALID;
			break;
		}
		
		FILE* fp = 0;
		errno_t errorFileOpen = _wfopen_s(&fp, filePathSetting, L"wb");
		if (errorFileOpen) {
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
				, "%s fopen(\"%ls\", \"wb\") error:"
				, __func__
				, filePathSetting
			);
			result = errorFileOpen;
			free(filePathSetting);
			filePathSetting = 0;
			break;
		}
		
		if (dataType == XUSER_DATA_TYPE_UNICODE && (dataLocation[fileSize - 2] != 0 || dataLocation[fileSize - 1] != 0)) {
			dataLocation[fileSize - 2] = dataLocation[fileSize - 1] = 0;
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
				, "%s Invalid Setting (0x%08x) UNICODE/wchar_t text was not null terminated. Truncating data."
				, __func__
				, profile_settings[iSetting].dwSettingId
			);
		}
		
		fwrite(dataLocation, sizeof(uint8_t), fileSize, fp);
		
		fclose(fp);
		fp = 0;
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_INFO
			, "%s Successfully wrote setting (0x%08x) of size (%u) to file: \"%ls\"."
			, __func__
			, profile_settings[iSetting].dwSettingId
			, fileSize
			, filePathSetting
		);
		
		free(filePathSetting);
		filePathSetting = 0;
	}
	
	free(pathUser);
	pathUser = 0;
	free(pathTitle);
	pathTitle = 0;
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalHigh = 0;
		xoverlapped->InternalLow = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return result;
}

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
)
{
	TRACE_FX();
	if (user_index_requester >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User 0x%08x does not exist.", __func__, user_index_requester);
		return ERROR_NO_SUCH_USER;
	}
	if (xlive_local_users[user_index_requester].signin_state == eXUserSigninState_NotSignedIn) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s User %u is not signed in.", __func__, user_index_requester);
		return ERROR_NOT_LOGGED_ON;
	}
	if (setting_id_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s setting_id_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (setting_id_count > 0x20) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s setting_id_count (0x%zx) is greater than 0x20.", __func__, setting_id_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (!setting_ids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s setting_ids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_buffer_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (*result_buffer_size && !result_profile_setting) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (*result_buffer_size && !result_profile_setting).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (read_for_num_of_xuids == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_num_of_xuids is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (read_for_num_of_xuids > 0x10) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_num_of_xuids (0x%zx) is greater than 0x10.", __func__, read_for_num_of_xuids);
		return ERROR_INVALID_PARAMETER;
	}
	if (!read_for_xuids) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s read_for_xuids is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (result_profile_setting && *result_buffer_size >= sizeof(XUSER_READ_PROFILE_SETTING_RESULT)) {
		result_profile_setting->dwSettingsLen = 0;
	}
	
	uint32_t result = GetProfileSettingsBufferSize(title_id, read_for_num_of_xuids, setting_id_count, setting_ids, result_buffer_size, result_profile_setting != 0);
	if (result) {
		if (!(result == ERROR_INSUFFICIENT_BUFFER && !result_profile_setting)) {
			XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s GetProfileSettingsBufferSize(...) error:", __func__);
		}
		return result;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the profile settings directories cannot be determined.");
		return ERROR_FUNCTION_FAILED;
	}
	
	size_t bufferRequired = sizeof(XUSER_READ_PROFILE_SETTING_RESULT) + (read_for_num_of_xuids * setting_id_count * sizeof(XUSER_PROFILE_SETTING));
	
	if (*result_buffer_size >= bufferRequired) {
		result_profile_setting->dwSettingsLen = setting_id_count;
		result_profile_setting->pSettings = (XUSER_PROFILE_SETTING*)((uint8_t*)result_profile_setting + sizeof(XUSER_READ_PROFILE_SETTING_RESULT));
		
		wchar_t* pathConfig = PathFromFilename(xlln_file_config_path);
		wchar_t* pathDefaults = FormMallocString(L"%sprofile/defaults/", pathConfig);
		
		for (size_t iReadForXuid = 0; iReadForXuid < read_for_num_of_xuids; iReadForXuid++) {
			bool isUserOnline = false;
			char* username = 0;
			uint32_t iLocalUser = 0;
			for (; iLocalUser < XLIVE_LOCAL_USER_COUNT; iLocalUser++) {
				if (xlive_local_users[iLocalUser].signin_state == eXUserSigninState_NotSignedIn) {
					continue;
				}
				if (xlive_local_users[iLocalUser].xuid == read_for_xuids[iReadForXuid]) {
					username = CloneString(xlive_local_users[iLocalUser].username);
					isUserOnline = (xlive_local_users[iLocalUser].signin_state == eXUserSigninState_SignedInToLive);
					break;
				}
			}
			if (iLocalUser == XLIVE_LOCAL_USER_COUNT) {
				iLocalUser = XUSER_INDEX_NONE;
			}
			
			wchar_t* pathUser = 0;
			wchar_t* pathTitle = 0;
			
			if (username) {
				pathUser = FormMallocString(L"%sprofile/user/%hs/", pathConfig, username);
				pathTitle = FormMallocString(L"%sprofile/title/%08X/%hs/", pathConfig, xlive_title_id, username);
			}
			
			for (size_t iSettingId = 0; iSettingId < setting_id_count; iSettingId++) {
				uint32_t settingId = setting_ids[iSettingId];
				uint8_t dataType;
				uint16_t dataSize;
				bool isTitleSetting;
				GetXProfileSettingInfo(settingId, &dataType, &dataSize, &isTitleSetting, 0);
				
				XUSER_PROFILE_SETTING* pSetting = &result_profile_setting->pSettings[iSettingId];
				pSetting->dwSettingId = settingId;
				pSetting->source = XSOURCE_NO_VALUE;
				memset(&pSetting->data, 0, sizeof(pSetting->data));
				pSetting->data.type = dataType;
				if (isUserOnline) {
					pSetting->user.xuid = read_for_xuids[iReadForXuid];
				}
				else {
					pSetting->user.dwUserIndex = iLocalUser;
				}
				
				uint32_t requiredReadSizeMin = dataSize;
				switch (dataType) {
					case XUSER_DATA_TYPE_INT32: {
						requiredReadSizeMin = sizeof(LONG);
						break;
					}
					case XUSER_DATA_TYPE_INT64: {
						requiredReadSizeMin = sizeof(LONGLONG);
						break;
					}
					case XUSER_DATA_TYPE_DOUBLE: {
						requiredReadSizeMin = sizeof(double);
						break;
					}
					case XUSER_DATA_TYPE_FLOAT: {
						requiredReadSizeMin = sizeof(FLOAT);
						break;
					}
					case XUSER_DATA_TYPE_DATETIME: {
						requiredReadSizeMin = sizeof(FILETIME);
						break;
					}
					case XUSER_DATA_TYPE_UNICODE: {
						requiredReadSizeMin = sizeof(wchar_t);
						break;
					}
				}
				
				const char* settingName = GetXProfileSettingName(settingId);
				bool readSomething = false;
				
				for (uint8_t iPath = isTitleSetting ? 0 : 1; iPath < 3; iPath++) {
					wchar_t* filePathSetting;
					switch (iPath) {
						case 0: {
							if (pathTitle) {
								filePathSetting = FormMallocString(fileNameSetting, pathTitle, settingId);
								break;
							}
							// Skip to default case.
							iPath++;
							continue;
						}
						case 1: {
							if (!isTitleSetting && pathUser) {
								filePathSetting = FormMallocString(fileNameSetting, pathUser, settingId);
								break;
							}
							iPath++;
						}
						default: {
							filePathSetting = FormMallocString(fileNameSetting, pathDefaults, settingId);
							break;
						}
					}
					
					FILE* fp;
					errno_t errorFileOpen = _wfopen_s(&fp, filePathSetting, L"rb");
					if (errorFileOpen) {
						if (errorFileOpen == ERROR_FILE_NOT_FOUND) {
							XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
								, "%s fopen(\"%ls\", \"rb\") error:"
								, __func__
								, filePathSetting
							);
						}
						else {
							XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
								, "%s fopen(\"%ls\", \"rb\") error:"
								, __func__
								, filePathSetting
							);
						}
					}
					
					if (!errorFileOpen) {
						fseek(fp, 0, SEEK_END);
						uint32_t fileSize = ftell(fp);
						
						uint8_t* dataLocation = 0;
						switch (dataType) {
							case XUSER_DATA_TYPE_INT32: {
								dataLocation = (uint8_t*)&pSetting->data.nData;
								break;
							}
							case XUSER_DATA_TYPE_INT64: {
								dataLocation = (uint8_t*)&pSetting->data.i64Data;
								break;
							}
							case XUSER_DATA_TYPE_DOUBLE: {
								dataLocation = (uint8_t*)&pSetting->data.dblData;
								break;
							}
							case XUSER_DATA_TYPE_FLOAT: {
								dataLocation = (uint8_t*)&pSetting->data.fData;
								break;
							}
							case XUSER_DATA_TYPE_DATETIME: {
								dataLocation = (uint8_t*)&pSetting->data.ftData;
								break;
							}
							case XUSER_DATA_TYPE_UNICODE:
							case XUSER_DATA_TYPE_BINARY: {
								dataLocation = (uint8_t*)((size_t)result_profile_setting + (size_t)bufferRequired);
								break;
							}
						}
						
						if (fileSize != dataSize && dataType != XUSER_DATA_TYPE_UNICODE && dataType != XUSER_DATA_TYPE_BINARY) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s Invalid Setting (0x%08x) file contents size (%u) does not match dataSize (%u). File: \"%ls\"."
								, __func__
								, settingId
								, fileSize
								, dataSize
								, filePathSetting
							);
						}
						else if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize < sizeof(wchar_t)) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s Invalid Setting (0x%08x) UNICODE fileSize (%u) must be greater than 2. File: \"%ls\"."
								, __func__
								, settingId
								, fileSize
								, filePathSetting
							);
						}
						else if (dataType == XUSER_DATA_TYPE_UNICODE && fileSize % 2) {
							XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
								, "%s Invalid Setting (0x%08x) UNICODE/wchar_t fileSize is not multiple of 2 (%u). File: \"%ls\"."
								, __func__
								, settingId
								, fileSize
								, filePathSetting
							);
						}
						else {
							if (dataLocation && *result_buffer_size >= bufferRequired) {
								pSetting->source = iPath == 2 ? XSOURCE_DEFAULT : XSOURCE_TITLE;
								uint32_t dataSizeToRead = dataSize;
								
								if (fileSize > dataSize) {
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
										, "%s Setting (0x%08x) file contents (size %u) is too big and will be truncated (max size %u). File: \"%ls\"."
										, __func__
										, settingId
										, fileSize
										, dataSize
										, filePathSetting
									);
									
									bufferRequired += dataSizeToRead;
								}
								else if (fileSize != dataSize) { // For UNICODE and BINARY.
									dataSizeToRead = fileSize;
									bufferRequired += dataSizeToRead;
								}
								
								if (dataType == XUSER_DATA_TYPE_UNICODE) {
									pSetting->data.string.cbData = dataSizeToRead;
									pSetting->data.string.pwszData = (wchar_t*)dataLocation;
								}
								else if (dataType == XUSER_DATA_TYPE_BINARY) {
									pSetting->data.binary.cbData = dataSizeToRead;
									pSetting->data.binary.pbData = dataLocation;
								}
								fseek(fp, 0, SEEK_SET);
								fread(dataLocation, sizeof(uint8_t), dataSizeToRead, fp);
								// Make sure the string is null terminated.
								if (dataType == XUSER_DATA_TYPE_UNICODE && (dataLocation[dataSizeToRead - 2] != 0 || dataLocation[dataSizeToRead - 1] != 0)) {
									dataLocation[dataSizeToRead - 2] = dataLocation[dataSizeToRead - 1] = 0;
									XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN
										, "%s Invalid Setting (0x%08x) UNICODE/wchar_t text was not null terminated. Truncating read data. File: \"%ls\"."
										, __func__
										, settingId
										, filePathSetting
									);
								}
								
								XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_DEBUG
									, "%s Successfully read setting (0x%08x) (%s) of size (%u) from file: \"%ls\"."
									, __func__
									, settingId
									, settingName
									, fileSize
									, filePathSetting
								);
							}
						}
						
						fclose(fp);
						free(filePathSetting);
						filePathSetting = 0;
						readSomething = true;
						break;
					}
					
					free(filePathSetting);
					filePathSetting = 0;
				}
				
				if (!readSomething) {
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
						, "%s Failed to read profile setting (0x%08x) (%s). Checked %s and defaults folder."
						, __func__
						, settingId
						, settingName
						, (isTitleSetting ? "title" : "user")
					);
				}
			}
			
			if (pathUser) {
				free(pathUser);
				pathUser = 0;
			}
			if (pathTitle) {
				free(pathTitle);
				pathTitle = 0;
			}
			if (username) {
				delete[] username;
				username = 0;
			}
		}
		
		delete[] pathConfig;
		pathConfig = 0;
		free(pathDefaults);
		pathDefaults = 0;
	}
	
	if (*result_buffer_size < bufferRequired) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL
			, "%s ERROR_INSUFFICIENT_BUFFER even though the already allocated result buffer is defined."
			, __func__
		);
		__debugbreak();
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

// #5331
uint32_t WINAPI XUserReadProfileSettings(
	uint32_t title_id
	, uint32_t user_index
	, size_t setting_id_count
	, const uint32_t* setting_ids
	, size_t* result_buffer_size
	, XUSER_READ_PROFILE_SETTING_RESULT* result_profile_setting
	, XOVERLAPPED* xoverlapped
)
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
	XUID readForXuids[] = { xlive_local_users[user_index].xuid };
	uint32_t result = XUserReadProfileSettingsByXuid(title_id, user_index, 1, readForXuids, setting_id_count, setting_ids, result_buffer_size, result_profile_setting, xoverlapped);
	return result;
}

// #5346
uint32_t WINAPI XUserEstimateRankForRating(size_t rank_request_count, const XUSER_RANK_REQUEST* rank_requests, size_t result_estimate_rank_size, XUSER_ESTIMATE_RANK_RESULTS* result_estimate_rank, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!rank_request_count) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rank_request_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!rank_requests) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s rank_requests is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_estimate_rank_size && !result_estimate_rank) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_estimate_rank is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	size_t resultEstimateRankSize = (sizeof(XUSER_ESTIMATE_RANK_RESULTS) + (sizeof(*XUSER_ESTIMATE_RANK_RESULTS::pdwRanks) * rank_request_count));
	if (result_estimate_rank_size < resultEstimateRankSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_estimate_rank_size (0x%zx) < resultEstimateRankSize (0x%zx).", __func__, result_estimate_rank_size, resultEstimateRankSize);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	
	result_estimate_rank->dwNumRanks = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	//synchronous
	return result;
}

// #5377
uint32_t WINAPI XUserFindUsers(XUID user_xuid_requester, size_t find_user_count, const FIND_USER_INFO* find_users, size_t result_find_users_size, FIND_USERS_RESPONSE* result_find_users, XOVERLAPPED* xoverlapped)
{
	TRACE_FX();
	if (!user_xuid_requester) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s user_xuid_requester is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (find_user_count == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s find_user_count is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (find_user_count > 0x64) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s find_user_count (0x%zx) is greater than 0x64.", __func__, find_user_count);
		return ERROR_INVALID_PARAMETER;
	}
	if (!find_users) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s find_users is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	size_t resultFindUsersSize = sizeof(FIND_USERS_RESPONSE) + (find_user_count * (sizeof(FIND_USER_INFO::qwUserId) + XUSER_NAME_SIZE));
	if (result_find_users_size < resultFindUsersSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_find_users_size (0x%zx) < resultFindUsersSize (0x%zx).", __func__, result_find_users_size, resultFindUsersSize);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_find_users) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_find_users is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	uint32_t result = ERROR_FUNCTION_FAILED;
	
	result_find_users->dwResults = 0;
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = result;
		xoverlapped->InternalHigh = result;
		xoverlapped->dwExtendedError = result;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	//synchronous
	return result;
}
