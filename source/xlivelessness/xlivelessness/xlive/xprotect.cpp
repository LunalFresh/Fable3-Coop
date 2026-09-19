#include <winsock2.h>
#include <Windows.h>
#include "xdefs.hpp"
#include "xprotect.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"

// #5034
HRESULT WINAPI XLiveProtectData(uint8_t* data_unprotected, size_t data_unprotected_size, uint8_t* data_protected, size_t* data_protected_size, HANDLE protected_data_handle)
{
	TRACE_FX();
	if (!data_unprotected) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_unprotected is NULL.", __func__);
		return E_POINTER;
	}
	if (data_unprotected_size == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_unprotected_size is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!data_protected_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_protected_size is NULL.", __func__);
		return E_POINTER;
	}
	if (!data_protected && *data_protected_size != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_protected is NULL and *data_protected_size != 0.", __func__);
		return E_INVALIDARG;
	}
	if (!protected_data_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_data_handle == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}
	
	if (!data_protected || *data_protected_size < data_unprotected_size) {
		*data_protected_size = data_unprotected_size;
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	}
	
	XLIVE_PROTECTED_DATA_INFORMATION* protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)protected_data_handle;
	// protectedDataInfo is not really used for much.
	memcpy(data_protected, data_unprotected, data_unprotected_size);
	*data_protected_size = data_unprotected_size;
	
	return S_OK;
}

// #5035
HRESULT WINAPI XLiveUnprotectData(uint8_t* data_protected, size_t data_protected_size, uint8_t* data_unprotected, size_t* data_unprotected_size, HANDLE* protected_data_handle)
{
	TRACE_FX();
	if (!protected_data_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is NULL.", __func__);
		return E_POINTER;
	}
	*protected_data_handle = 0;
	if (!data_protected) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_protected is NULL.", __func__);
		return E_POINTER;
	}
	if (data_protected_size == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_protected_size is 0.", __func__);
		return E_INVALIDARG;
	}
	if (!data_unprotected_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_unprotected_size is NULL.", __func__);
		return E_POINTER;
	}
	if (!data_unprotected && *data_unprotected_size != 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s data_unprotected is NULL and *data_unprotected_size != 0.", __func__);
		return E_INVALIDARG;
	}
	
	if (!data_unprotected || *data_unprotected_size < data_protected_size) {
		*data_unprotected_size = data_protected_size;
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
	}
	
	XLIVE_PROTECTED_DATA_INFORMATION* protectedDataInfo = new XLIVE_PROTECTED_DATA_INFORMATION;
	protectedDataInfo->cbSize = 0;
	protectedDataInfo->dwFlags = XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY;
	*protected_data_handle = protectedDataInfo;
	
	memcpy(data_unprotected, data_protected, data_protected_size);
	
	return S_OK;
}

// #5036
HRESULT WINAPI XLiveCreateProtectedDataContext(XLIVE_PROTECTED_DATA_INFORMATION* protected_data_info, HANDLE* protected_data_handle)
{
	TRACE_FX();
	if (!protected_data_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is NULL.", __func__);
		return E_POINTER;
	}
	*protected_data_handle = 0;
	if (!protected_data_info) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_info is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_data_info->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_info->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION).", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	if (protected_data_info->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_info->dwFlags & ~(XLIVE_PROTECTED_DATA_FLAG_OFFLINE_ONLY).", __func__);
		return E_INVALIDARG;
	}
	
	XLIVE_PROTECTED_DATA_INFORMATION* protectedDataInfo = new XLIVE_PROTECTED_DATA_INFORMATION;
	protectedDataInfo->cbSize = 0;
	protectedDataInfo->dwFlags = protected_data_info->dwFlags;
	*protected_data_handle = protectedDataInfo;
	
	return S_OK;
}

// #5037
HRESULT WINAPI XLiveQueryProtectedDataInformation(HANDLE protected_data_handle, XLIVE_PROTECTED_DATA_INFORMATION* protected_data_info)
{
	TRACE_FX();
	if (!protected_data_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is NULL.", __func__);
		return E_HANDLE;
	}
	if (protected_data_handle == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}
	if (protected_data_info->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_info->cbSize != sizeof(XLIVE_PROTECTED_DATA_INFORMATION).", __func__);
		return HRESULT_FROM_WIN32(ERROR_INVALID_USER_BUFFER);//0x800706F8;
	}
	
	XLIVE_PROTECTED_DATA_INFORMATION* protectedDataInfo = (XLIVE_PROTECTED_DATA_INFORMATION*)protected_data_handle;
	
	protected_data_info->cbSize = sizeof(XLIVE_PROTECTED_DATA_INFORMATION);
	protected_data_info->dwFlags = protectedDataInfo->dwFlags;
	
	return S_OK;
}

// #5038
HRESULT WINAPI XLiveCloseProtectedDataContext(HANDLE protected_data_handle)
{
	TRACE_FX();
	if (!protected_data_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_data_handle == INVALID_HANDLE_VALUE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_data_handle is INVALID_HANDLE_VALUE.", __func__);
		return E_HANDLE;
	}
	
	delete protected_data_handle;
	
	return S_OK;
}

// #5347
HRESULT WINAPI XLiveProtectedLoadLibrary(HANDLE content_access_handle, void* reserved, wchar_t* module_pathname, uint32_t load_library_flags, HMODULE* result_module)
{
	TRACE_FX();
	if (reserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s reserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!module_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s module_pathname is NULL.", __func__);
		return E_POINTER;
	}
	if (!*module_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *module_pathname is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (load_library_flags & LOAD_WITH_ALTERED_SEARCH_PATH) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s load_library_flags must not include LOAD_WITH_ALTERED_SEARCH_PATH.", __func__);
		return E_INVALIDARG;
	}
	if (load_library_flags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s load_library_flags must not include LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE.", __func__);
		return E_INVALIDARG;
	}
	if (!result_module) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_module is NULL.", __func__);
		return E_POINTER;
	}
	*result_module = 0;
	
	// Verify the file is in the signed catalog. Note content_access_handle from XLiveContentCreateAccessHandle(...) affects path.
	//HANDLE hCreateFile = CreateFileW(module_pathname, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	//if (hCreateFile == INVALID_HANDLE_VALUE) {
	//	return HRESULT_FROM_WIN32(GetLastError());
	//}
	
	HMODULE hModule = LoadLibraryExW(module_pathname, 0, load_library_flags);
	if (!hModule) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	
	*result_module = hModule;
	
	return S_OK;
}

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
)
{
	TRACE_FX();
	if (reserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s reserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s file_pathname is NULL.", __func__);
		return E_POINTER;
	}
	if (!*file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *file_pathname is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (desired_access != GENERIC_READ && desired_access != GENERIC_EXECUTE && desired_access != (GENERIC_READ | GENERIC_EXECUTE)) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s desired_access (0x%08x) is not GENERIC_READ (0x%08x) and/or GENERIC_EXECUTE (0x%08x).", __func__, desired_access, GENERIC_READ, GENERIC_EXECUTE);
		return E_INVALIDARG;
	}
	if (share_mode && share_mode != FILE_SHARE_READ) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s share_mode (0x%08x) must be 0 or FILE_SHARE_READ (0x%08x).", __func__, share_mode, FILE_SHARE_READ);
		return E_INVALIDARG;
	}
	if (creation_disposition != OPEN_EXISTING && creation_disposition != TRUNCATE_EXISTING) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s creation_disposition (0x%08x) is not OPEN_EXISTING (0x%08x) or TRUNCATE_EXISTING (0x%08x).", __func__, creation_disposition, OPEN_EXISTING, TRUNCATE_EXISTING);
		return E_INVALIDARG;
	}
	if (!result_file_handle) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_file_handle is NULL.", __func__);
		return E_POINTER;
	}
	*result_file_handle = 0;
	
	HANDLE hCreateFile = CreateFileW(
		file_pathname
		, desired_access
		, share_mode
		, security_attributes
		, creation_disposition
		, flags_and_attributes
		, 0
	);
	if (hCreateFile == INVALID_HANDLE_VALUE) {
		return HRESULT_FROM_WIN32(GetLastError());
	}
	*result_file_handle = hCreateFile;
	
	return S_OK;
	return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

// #5039: This function is deprecated. Use XLiveProtectedVerifyFile.
HRESULT WINAPI XLiveVerifyDataFile(wchar_t* file_pathname)
{
	TRACE_FX();
	if (!file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s file_pathname is NULL.", __func__);
		return E_POINTER;
	}
	if (!*file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *file_pathname is NULL.", __func__);
		return E_INVALIDARG;
	}
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s TODO.", __func__);
	return S_OK;
	//if (XLivepGetTitleXLiveVersion() < 0x20000000)
	return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

// #5349
HRESULT WINAPI XLiveProtectedVerifyFile(HANDLE content_access_handle, void* reserved, wchar_t* file_pathname)
{
	TRACE_FX();
	if (reserved) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s reserved is not NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s file_pathname is NULL.", __func__);
		return E_POINTER;
	}
	if (!*file_pathname) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *file_pathname is NULL.", __func__);
		return E_INVALIDARG;
	}
	
	// handle from XLiveContentCreateAccessHandle.
	content_access_handle;
	
	return S_OK;
}
