#pragma once
#include "xlive.hpp"
#include <map>
#include <set>

typedef struct _ACHIEVEMENT_ENUMERATION_DETAILS {
	HANDLE enumeratorHandle = 0;
	uint32_t titleId = 0;
	uint32_t detailFlags = 0;
	size_t index = 0;
	std::vector<XACHIEVEMENT_DETAILS> achievementDetails = {};
} ACHIEVEMENT_ENUMERATION_DETAILS;

typedef struct _STATS_ENUMERATION_DETAILS {
	HANDLE enumeratorHandle = 0;
	
	// Option #1. Only return results from and above this rank.
	uint32_t enumerationStartRank = 0;
	// Option #2. Only return results from and above this rating.
	int64_t enumerationStartRating = 0;
	// Option #3. Return results roughly putting this user in the middle row.
	XUID enumerationPivotXuid = 0;
	
	uint32_t titleId = 0;
	// Only return this max number of achievements per enumeration.
	size_t maxRowCount = 0;
	//
	size_t statsSpecCount = 0;
	//
	XUSER_STATS_SPEC* statsSpecs = 0;
} STATS_ENUMERATION_DETAILS;

extern CRITICAL_SECTION xlive_critsec_xuser_achievement_enumerators;
extern std::map<HANDLE, ACHIEVEMENT_ENUMERATION_DETAILS*> xlive_xuser_achievement_enumerators;

extern CRITICAL_SECTION xlive_critsec_xuser_stats;
extern std::map<HANDLE, STATS_ENUMERATION_DETAILS*> xlive_xuser_stats_enumerators;
extern std::map<XUID, std::map<uint32_t, std::map<uint32_t, XUSER_DATA>>> xlive_written_stats;

extern CRITICAL_SECTION xlive_critsec_xuser_context_properties;
extern std::map<uint32_t, uint32_t> xlive_user_contexts[XLIVE_LOCAL_USER_COUNT];
extern std::map<uint32_t, XUSER_DATA> xlive_user_properties[XLIVE_LOCAL_USER_COUNT];;

size_t XUserStatsReadResultsMinSize(const STATS_ENUMERATION_DETAILS* statsEnumerationDetails, bool only_one_row);
XUSER_DATA XllnStatsCloneUserData(const XUSER_DATA& source);
void XllnStatsFreeUserData(XUSER_DATA& value);
void XllnStatsStoreValue(XUID xuid, uint32_t view_id, uint32_t column_id, const XUSER_DATA& value);
uint32_t XllnStatsResolveColumnId(uint32_t view_id, uint16_t column_id);
uint32_t XllnStatsSaveUserToDisk(uint32_t user_index);
uint32_t XllnStatsLoadUserFromDisk(uint32_t user_index);
uint32_t XUserContextResetDefaults(uint32_t user_index);
uint32_t XUserPropertyResetDefaults(uint32_t user_index);
uint32_t XUserEraseAchievements(size_t achievement_count, const XUSER_ACHIEVEMENT* achievements, XOVERLAPPED* xoverlapped);

bool InitXUser();
bool UninitXUser();
