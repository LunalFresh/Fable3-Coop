#pragma once

namespace tags
{

	struct tag_data_block
	{
		int block_count;
		DWORD block_data_offset;
	};

	/* 
		Tag Interface

		These functions shouldn't be called while a new cache is being loaded and as such it's not recommended you call them from any thread other than the main one.
		If you want to run code just after a map load register a callback using tags::on_map_load
	*/

	/* Run callbacks after the cache file has been loaded */
	void run_callbacks();

	/* Register callback on map data load */
	void on_map_load(void (*callback)());

	/* tag data in currently loaded map (merged cache and shared cache data afaik) */
	char *get_tag_data();

	/* gets the globals\globals aka matg for the current map/cache file (TODO: add the matg structure) */
	char *get_matg_globals_ptr();

	/* Returns a handle to the map file currently loaded */
	HANDLE get_cache_handle();

	/* Is a cache loaded? */
	bool cache_file_loaded();

	/* 
		Load tag names from cache file.
		Automatically called on load.
	*/
	bool load_tag_debug_name();

	/* helper function for getting a pointer to data at offset in tag data */
	template <typename T>
	T *get_at_tag_data_offset(size_t offset)
	{
		return reinterpret_cast<T*>(&get_tag_data()[offset]);
	}
}
