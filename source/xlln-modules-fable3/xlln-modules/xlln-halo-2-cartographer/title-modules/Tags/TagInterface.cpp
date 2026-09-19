#include <Windows.h>
#include "TagInterface.hpp"
#include "../../xlln-module.hpp"
#include "../../../utils/utils.hpp"
#include <map>
#include <vector>

char *tags::get_tag_data()
{
	return *GetOffsetAddressCS<char**>(0x47CD54, 0x4A29BC);
}

char *tags::get_matg_globals_ptr()
{
	return *GetOffsetAddressCS<char**>(0x479E70, 0x4A642C);
}

HANDLE tags::get_cache_handle()
{
	return *GetOffsetAddressCS<HANDLE*>(0x4AE8A8, 0x4CF128);
}

bool tags::cache_file_loaded()
{
	return *GetOffsetAddressCS<bool*>(0x47CD60, 0x4A29C8);
}
