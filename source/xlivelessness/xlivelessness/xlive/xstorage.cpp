#include <winsock2.h>
#include "xdefs.hpp"
#include "xstorage.hpp"
#include "xlive.hpp"
#include "../xlln/debug-log.hpp"
#include "../utils/utils.hpp"
#include "../xlln/xlln.hpp"
#include "../xlln/xlln-config.hpp"

static uint32_t ToXStorageError(uint32_t error)
{
	uint32_t result = error;
	
	switch (result) {
		case ERROR_ACCESS_DENIED:
			result = XONLINE_E_STORAGE_ACCESS_DENIED;
			break;
		case ERROR_FILE_TOO_LARGE:
			result = XONLINE_E_STORAGE_FILE_IS_TOO_BIG;
			break;
		case ERROR_INSUFFICIENT_BUFFER:
			result = XONLINE_E_STORAGE_FILE_IS_TOO_BIG;
			break;
		case ERROR_FILE_NOT_FOUND:
			result = XONLINE_E_STORAGE_FILE_NOT_FOUND;
			break;
		case ERROR_FILE_EXISTS:
			result = XONLINE_E_STORAGE_FILE_ALREADY_EXISTS;
			break;
	}
	return result;
}

// #5304
uint32_t WINAPI XStorageUploadFromMemoryGetProgress(XOVERLAPPED* xoverlapped, uint32_t* result_percent_complete, uint64_t* result_numerator, uint64_t* result_denominator)
{
	TRACE_FX();
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_numerator != !result_denominator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_numerator and result_denominator must both be defined or undefined.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (result_percent_complete) {
		*result_percent_complete = 100;
	}
	if (result_numerator) {
		*result_numerator = 100;
	}
	if (result_denominator) {
		*result_denominator = 100;
	}
	
	return ERROR_SUCCESS;
}

// #5305
uint32_t WINAPI XStorageUploadFromMemory(uint32_t user_index, const wchar_t* server_path, size_t data_buffer_size, const uint8_t* data_buffer, XOVERLAPPED* xoverlapped)
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
	if (!server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (data_buffer_size == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_buffer_size is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!data_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	wchar_t* configPath = PathFromFilename(xlln_file_config_path);
	wchar_t* storageFilePath = FormMallocString(L"%s%s", configPath, server_path);
	delete[] configPath;
	wchar_t* storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		result = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	
	if (!errorMkdir) {
		FILE* fp;
		errno_t errorFileOpen = _wfopen_s(&fp, storageFilePath, L"wb");
		if (errorFileOpen) {
			result = errorFileOpen;
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"wb\") error:", __func__, storageFilePath);
		}
		else {
			fwrite(data_buffer, 1, data_buffer_size, fp);
			fclose(fp);
		}
	}
	delete[] storageFilePath;
	
	result = ToXStorageError(result);
	
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

// #5306
uint32_t WINAPI XStorageEnumerate(
	uint32_t user_index
	, const wchar_t* server_path
	, size_t starting_index
	, size_t max_result_count
	, size_t result_info_size
	, XSTORAGE_ENUMERATE_RESULTS* result_info
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
	if (!server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (max_result_count > XSTORAGE_MAX_RESULTS_TO_RETURN) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (max_result_count > XSTORAGE_MAX_RESULTS_TO_RETURN) (%zu >= %u).", __func__, max_result_count, XSTORAGE_MAX_RESULTS_TO_RETURN);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index >= XSTORAGE_MAX_RESULTS_TO_RETURN) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index >= XSTORAGE_MAX_RESULTS_TO_RETURN) (%zu >= %u).", __func__, starting_index, XSTORAGE_MAX_RESULTS_TO_RETURN);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index + max_result_count < starting_index) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index + max_result_count < starting_index) (%zu + %zu >= %zu).", __func__, starting_index, max_result_count, starting_index);
		return ERROR_INVALID_PARAMETER;
	}
	if (starting_index + max_result_count > XSTORAGE_MAX_RESULTS_TO_RETURN) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (starting_index + max_result_count > XSTORAGE_MAX_RESULTS_TO_RETURN) (%zu + %zu > %u).", __func__, starting_index, max_result_count, XSTORAGE_MAX_RESULTS_TO_RETURN);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_info_size < sizeof(XSTORAGE_ENUMERATE_RESULTS) + (max_result_count * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(wchar_t))))) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR
			, "%s (result_info_size < sizeof(XSTORAGE_ENUMERATE_RESULTS) + (max_result_count * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(wchar_t))))) (0x%zx < 0x%zx)."
			, __func__
			, result_info_size
			, (size_t)sizeof(XSTORAGE_ENUMERATE_RESULTS) + (max_result_count * (sizeof(XSTORAGE_FILE_INFO) + (XONLINE_MAX_PATHNAME_LENGTH * sizeof(wchar_t))))
		);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	
	result_info->dwNumItemsReturned = 0;
	result_info->dwTotalNumItems = 0;
	
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

// #5307
uint32_t WINAPI XStorageDownloadToMemoryGetProgress(XOVERLAPPED* xoverlapped, uint32_t* result_percent_complete, uint64_t* result_numerator, uint64_t* result_denominator)
{
	TRACE_FX();
	if (!xoverlapped) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xoverlapped is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_numerator != !result_denominator) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_numerator and result_denominator must both be defined or undefined.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (result_percent_complete) {
		*result_percent_complete = 100;
	}
	if (result_numerator) {
		*result_numerator = 100;
	}
	if (result_denominator) {
		*result_denominator = 100;
	}
	
	return ERROR_SUCCESS;
}

// #5308
uint32_t WINAPI XStorageDelete(uint32_t user_index, const wchar_t* server_path, XOVERLAPPED* xoverlapped)
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
	if (!server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t result = ERROR_SUCCESS;
	
	wchar_t* configPath = PathFromFilename(xlln_file_config_path);
	wchar_t* storageFilePath = FormMallocString(L"%s%s", configPath, server_path);
	delete[] configPath;
	wchar_t* storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		result = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	
	if (!errorMkdir) {
		wchar_t* storageFilePathExt = FormMallocString(L"\\\\?\\%s", storageFilePath);
		BOOL resultDelete = DeleteFileW(storageFilePathExt);
		if (!resultDelete) {
			result = GetLastError();
			XLLN_DEBUG_LOG_ECODE(result, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s DeleteFileW(\"%ls\") error:", __func__, storageFilePathExt);
		}
		delete[] storageFilePathExt;
	}
	
	delete[] storageFilePath;
	
	result = ToXStorageError(result);
	
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

// #5344
uint32_t WINAPI XStorageBuildServerPath(
	uint32_t user_index
	, XSTORAGE_FACILITY xstorage_facility
	, const void* xstorage_facility_info
	, size_t xstorage_facility_info_size
	, const wchar_t* item_name
	, wchar_t* result_server_path
	, size_t* result_server_path_size
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
	if (!xstorage_facility_info && xstorage_facility_info_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (!xstorage_facility_info && xstorage_facility_info_size).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!item_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s item_name is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*item_name) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *item_name is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_server_path_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_server_path_size is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (xstorage_facility != XSTORAGE_FACILITY_GAME_CLIP && xstorage_facility != XSTORAGE_FACILITY_PER_TITLE && xstorage_facility != XSTORAGE_FACILITY_PER_USER_TITLE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xstorage_facility (%u) enum is unknown.", __func__, xstorage_facility);
		return ERROR_INVALID_PARAMETER;
	}
	if (xstorage_facility == XSTORAGE_FACILITY_GAME_CLIP && !xstorage_facility_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (xstorage_facility == XSTORAGE_FACILITY_GAME_CLIP && !xstorage_facility_info).", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (xstorage_facility_info && xstorage_facility_info_size < sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s xstorage_facility_info_size (%zu) must be at least %u when xstorage_facility_info is not NULL.", __func__, xstorage_facility_info_size, sizeof(XSTORAGE_FACILITY_INFO_GAME_CLIP));
		return ERROR_INVALID_PARAMETER;
	}
	
	char* username = CloneString(xlive_local_users[user_index].username);
	ReplaceFilePathSensitiveChars(username);
	wchar_t* itemName = CloneString(item_name);
	if (wcschr(itemName, L'*') != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_FATAL, "(RESEARCH) %s itemName contains wildcard *.", __func__);
	}
	ReplaceFilePathSensitiveChars(itemName); // '*' is a wildcard to allow multiple item enumeration?
	wchar_t* storageFilePath = FormMallocString(L"storage/%hs/%s", username, itemName);
	delete[] username;
	delete[] itemName;
	size_t storageFilePathBufSize = (wcslen(storageFilePath) + 1) * sizeof(wchar_t);
	
	if (storageFilePathBufSize > *result_server_path_size) {
		free(storageFilePath);
		return ERROR_INSUFFICIENT_BUFFER;
	}
	
	memcpy(result_server_path, storageFilePath, storageFilePathBufSize);
	*result_server_path_size = storageFilePathBufSize;
	
	free(storageFilePath);
	return ERROR_SUCCESS;
}

// #5309
uint32_t WINAPI XStorageBuildServerPathByXuid(
	XUID user_xuid
	, XSTORAGE_FACILITY xstorage_facility
	, const void* xstorage_facility_info
	, size_t xstorage_facility_info_size
	, const wchar_t* item_name
	, wchar_t* result_server_path
	, size_t* result_server_path_size
)
{
	TRACE_FX();
	uint32_t iUser = 0;
	for (; iUser < XLIVE_LOCAL_USER_COUNT; iUser++) {
		if (xlive_local_users[iUser].xuid == user_xuid) {
			break;
		}
	}
	if (iUser >= XLIVE_LOCAL_USER_COUNT) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s Local XUID 0x%016I64x does not exist.", __func__, user_xuid);
		return ERROR_NO_SUCH_USER;
	}
	return XStorageBuildServerPath(iUser, xstorage_facility, xstorage_facility_info, xstorage_facility_info_size, item_name, result_server_path, result_server_path_size);
}

// #5345
uint32_t WINAPI XStorageDownloadToMemory(
	uint32_t user_index
	, const wchar_t* server_path
	, size_t result_data_buffer_size
	, const uint8_t* result_data_buffer
	, size_t result_info_size
	, XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS* result_info
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
	if (!server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!*server_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *server_path is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_data_buffer_size == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_data_buffer_size is 0.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_data_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_data_buffer is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	if (result_info_size != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (result_info_size != sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS)) (0x%zx != 0x%x).", __func__, result_info_size, sizeof(XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS));
		return ERROR_INVALID_PARAMETER;
	}
	if (!result_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_info is NULL.", __func__);
		return ERROR_INVALID_PARAMETER;
	}
	
	if (!xlln_file_config_path) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "XLLN Config is not set so the storage directory cannot be determined.");
		return XONLINE_E_STORAGE_CANNOT_FIND_PATH;
	}
	
	uint32_t resultCode = ERROR_SUCCESS;
	
	wchar_t* configPath = PathFromFilename(xlln_file_config_path);
	wchar_t* storageFilePath = FormMallocString(L"%s%s", configPath, server_path);
	delete[] configPath;
	configPath = 0;
	wchar_t* storagePath = PathFromFilename(storageFilePath);
	uint32_t errorMkdir = EnsureDirectoryExists(storagePath);
	if (errorMkdir) {
		XLLN_DEBUG_LOG_ECODE(errorMkdir, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s EnsureDirectoryExists(...) error on path \"%ls\".", __func__, storagePath);
		resultCode = XONLINE_E_STORAGE_INVALID_STORAGE_PATH;
	}
	delete[] storagePath;
	storagePath = 0;
	
	if (!errorMkdir) {
		FILE* fp = 0;
		errno_t errorFileOpen = _wfopen_s(&fp, storageFilePath, L"rb");
		if (errorFileOpen == ERROR_FILE_NOT_FOUND) {
			resultCode = errorFileOpen;
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_WARN, "%s fopen(\"%ls\", \"rb\") error:", __func__, storageFilePath);
		}
		else if (errorFileOpen) {
			resultCode = errorFileOpen;
			XLLN_DEBUG_LOG_ECODE(errorFileOpen, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s fopen(\"%ls\", \"rb\") error:", __func__, storageFilePath);
		}
		else {
			fseek(fp, 0, SEEK_END);
			int32_t fileSize = ftell(fp);
			
			result_info->dwBytesTotal = fileSize;
			result_info->xuidOwner = xlive_local_users[user_index].xuid;
			
			if (fileSize < 0 || (uint32_t)fileSize > result_data_buffer_size) {
				resultCode = ERROR_INSUFFICIENT_BUFFER;
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (fileSize > result_data_buffer_size) (0x%x > 0x%zx).", __func__, fileSize, result_data_buffer_size);
				fclose(fp);
				fp = 0;
			}
			else {
				fseek(fp, 0, SEEK_SET);
				fread((void*)result_data_buffer, 1, fileSize, fp);
				fclose(fp);
				fp = 0;
				
				HANDLE hFile = CreateFileW(storageFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					resultCode = GetLastError();
					XLLN_DEBUG_LOG_ECODE(resultCode, XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s CreateFileW(\"%ls\", GENERIC_READ, ...) error:", __func__, storageFilePath);
					GetSystemTimeAsFileTime(&result_info->ftCreated);
				}
				else {
					GetFileTime(hFile, &result_info->ftCreated, NULL, NULL);
					CloseHandle(hFile);
				}
			}
		}
	}
	free(storageFilePath);
	storageFilePath = 0;
	
	resultCode = ToXStorageError(resultCode);
	
	if (xoverlapped) {
		//asynchronous
		
		xoverlapped->InternalLow = (resultCode != 0 ? ERROR_FUNCTION_FAILED : ERROR_SUCCESS);
		xoverlapped->InternalHigh = 0;
		xoverlapped->dwExtendedError = resultCode;
		
		Check_Overlapped(xoverlapped);
		
		return ERROR_IO_PENDING;
	}
	
	//synchronous
	return resultCode;
}
