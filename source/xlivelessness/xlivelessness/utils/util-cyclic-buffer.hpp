#pragma once
#include <stdint.h>

class CyclicBuffer
{
	public:
		CyclicBuffer(const size_t buffer_size);
		~CyclicBuffer();
		
		size_t GetTotalSize();
		size_t GetFilledSize();
		size_t GetEmptySize();
		bool ClearBufferOfData();
		
		bool PushData(const uint8_t* data_buffer, const size_t data_size);
		// data_buffer is optional.
		bool PopData(const size_t data_buffer_size, uint8_t* data_buffer, size_t* data_size);
		// data_buffer is optional.
		// peek_only: peek at the data and do not dequeue/remove it from the buffer.
		bool PopData(const size_t data_buffer_size, uint8_t* data_buffer, size_t* data_size, bool peek_only);
	
	private:
		uint8_t* cyclic_buffer = 0;
		size_t cyclic_buffer_size = 0;
		uint8_t* cyclic_buffer_filled_start = 0;
		// Null when buffer is full.
		uint8_t* cyclic_buffer_empty_start = 0;
};
