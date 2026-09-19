#include <winsock2.h>
#include <Windows.h>
#include "xdefs.hpp"
#include "xpbuffer.hpp"
#include "../xlln/debug-log.hpp"
#include "../xlln/xlln.hpp"

// #5016
HRESULT WINAPI XLivePBufferAllocate(size_t protected_buffer_size, XLIVE_PROTECTED_BUFFER** protected_buffer)
{
	TRACE_FX();
	if (!protected_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer_size is NULL.", __func__);
		return E_INVALIDARG;
	}
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_size + sizeof(XLIVE_PROTECTED_BUFFER::dwSize) < protected_buffer_size) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_size + sizeof(XLIVE_PROTECTED_BUFFER::dwSize) < protected_buffer_size) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	
	*protected_buffer = (XLIVE_PROTECTED_BUFFER*)calloc(protected_buffer_size + sizeof(XLIVE_PROTECTED_BUFFER::dwSize), 1);
	if (!*protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s *protected_buffer is NULL.", __func__);
		return E_OUTOFMEMORY;
	}
	
	(*protected_buffer)->dwSize = protected_buffer_size;
	
	return S_OK;
}

// #5017
HRESULT WINAPI XLivePBufferFree(XLIVE_PROTECTED_BUFFER* protected_buffer)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	
	free(protected_buffer);
	
	return S_OK;
}

// #5018
HRESULT WINAPI XLivePBufferGetByte(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* result_value)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset >= protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset >= protected_buffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}
	if (!result_value) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_value is NULL.", __func__);
		return E_POINTER;
	}
	
	*result_value = ((uint8_t*)&protected_buffer->bData)[protected_buffer_offset];
	
	return S_OK;
}

// #5019
HRESULT WINAPI XLivePBufferSetByte(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t value)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset >= protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset >= protected_buffer->dwSize) Unexpected.", __func__);
		return E_UNEXPECTED;
	}
	
	((uint8_t*)&protected_buffer->bData)[protected_buffer_offset] = value;
	
	return S_OK;
}

// #5020
HRESULT WINAPI XLivePBufferGetDWORD(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint32_t* result_value)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset + sizeof(*result_value) < protected_buffer_offset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + sizeof(*result_value) < protected_buffer_offset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (protected_buffer_offset + sizeof(*result_value) > protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + sizeof(*result_value) > protected_buffer->dwSize) (0x%zx + 0x%zx > 0x%zx) Unexpected range.", __func__, protected_buffer_offset, sizeof(*result_value), protected_buffer->dwSize);
		return E_UNEXPECTED;
	}
	if (!result_value) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_value is NULL.", __func__);
		return E_POINTER;
	}
	
	*result_value = *(uint32_t*)&((uint8_t*)&protected_buffer->bData)[protected_buffer_offset];
	
	return S_OK;
}

// #5021
HRESULT WINAPI XLivePBufferSetDWORD(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint32_t value)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset + sizeof(value) < protected_buffer_offset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + sizeof(value) < protected_buffer_offset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (protected_buffer_offset + sizeof(value) > protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + sizeof(value) > protected_buffer->dwSize) (0x%zx + 0x%zx > 0x%zx) Unexpected range.", __func__, protected_buffer_offset, sizeof(value), protected_buffer->dwSize);
		return E_UNEXPECTED;
	}
	
	*(uint32_t*)&((uint8_t*)&protected_buffer->bData)[protected_buffer_offset] = value;
	
	return S_OK;
}

// #5294
HRESULT WINAPI XLivePBufferGetByteArray(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* result_data, size_t result_data_size)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset + result_data_size < protected_buffer_offset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + result_data_size < protected_buffer_offset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (protected_buffer_offset + result_data_size > protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + result_data_size > protected_buffer->dwSize) (0x%zx + 0x%zx > 0x%zx) Unexpected range.", __func__, protected_buffer_offset, result_data_size, protected_buffer->dwSize);
		return E_UNEXPECTED;
	}
	if (!result_data) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s result_data is NULL.", __func__);
		return E_POINTER;
	}
	
	memcpy(result_data, &((uint8_t*)&protected_buffer->bData)[protected_buffer_offset], result_data_size);
	
	return S_OK;
}

// #5295
HRESULT WINAPI XLivePBufferSetByteArray(XLIVE_PROTECTED_BUFFER* protected_buffer, size_t protected_buffer_offset, uint8_t* data, size_t data_size)
{
	TRACE_FX();
	if (!protected_buffer) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s protected_buffer is NULL.", __func__);
		return E_POINTER;
	}
	if (protected_buffer_offset + data_size < protected_buffer_offset) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + data_size < protected_buffer_offset) Overflow experienced.", __func__);
		return E_UNEXPECTED;
	}
	if (protected_buffer_offset + data_size > protected_buffer->dwSize) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLIVE | XLLN_LOG_LEVEL_ERROR, "%s (protected_buffer_offset + data_size > protected_buffer->dwSize) (0x%zx + 0x%zx > 0x%zx) Unexpected range.", __func__, protected_buffer_offset, data_size, protected_buffer->dwSize);
		return E_UNEXPECTED;
	}
	
	memcpy(&((uint8_t*)&protected_buffer->bData)[protected_buffer_offset], data, data_size);
	
	return S_OK;
}
