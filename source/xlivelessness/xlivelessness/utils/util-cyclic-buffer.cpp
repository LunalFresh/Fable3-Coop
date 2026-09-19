#include <memory.h>
#include "./util-cyclic-buffer.hpp"

CyclicBuffer::CyclicBuffer(const size_t buffer_size)
{
	if (this->cyclic_buffer) {
		__debugbreak();
	}
	this->cyclic_buffer_size = buffer_size;
	if (this->cyclic_buffer_size == 0) {
		return;
	}
	this->cyclic_buffer = new uint8_t[this->cyclic_buffer_size];
	this->cyclic_buffer_filled_start = this->cyclic_buffer;
	this->cyclic_buffer_empty_start = this->cyclic_buffer;
}

CyclicBuffer::~CyclicBuffer()
{
	if (!this->cyclic_buffer) {
		return;
	}
	delete[] this->cyclic_buffer;
	this->cyclic_buffer = 0;
	this->cyclic_buffer_filled_start = 0;
	this->cyclic_buffer_empty_start = 0;
	this->cyclic_buffer_size = 0;
}

size_t CyclicBuffer::GetTotalSize()
{
	return this->cyclic_buffer_size;
}

size_t CyclicBuffer::GetFilledSize()
{
	if (!this->cyclic_buffer) {
		return 0;
	}
	
	if (this->cyclic_buffer_empty_start == 0) {
		return this->cyclic_buffer_size;
	}
	
	const uint8_t* cyclic_buffer_end = &this->cyclic_buffer[this->cyclic_buffer_size];
	
	size_t resultSize = 0;
	
	if (this->cyclic_buffer_filled_start > this->cyclic_buffer_empty_start) {
		resultSize += cyclic_buffer_end - this->cyclic_buffer_filled_start;
		resultSize += this->cyclic_buffer_empty_start - cyclic_buffer;
	}
	else {
		resultSize += this->cyclic_buffer_empty_start - this->cyclic_buffer_filled_start;
	}
	
	return resultSize;
}

size_t CyclicBuffer::GetEmptySize()
{
	if (!this->cyclic_buffer) {
		return 0;
	}
	
	if (this->cyclic_buffer_empty_start == 0) {
		return 0;
	}
	
	const uint8_t* cyclic_buffer_end = &this->cyclic_buffer[this->cyclic_buffer_size];
	
	size_t resultSize = 0;
	
	if (this->cyclic_buffer_empty_start >= this->cyclic_buffer_filled_start) {
		resultSize += cyclic_buffer_end - this->cyclic_buffer_empty_start;
		resultSize += this->cyclic_buffer_filled_start - cyclic_buffer;
	}
	else {
		resultSize += this->cyclic_buffer_filled_start - this->cyclic_buffer_empty_start;
	}
	
	return resultSize;
}

bool CyclicBuffer::ClearBufferOfData()
{
	if (!this->cyclic_buffer) {
		return false;
	}
	
	this->cyclic_buffer_filled_start = this->cyclic_buffer;
	this->cyclic_buffer_empty_start = this->cyclic_buffer;
	
	return true;
}

bool CyclicBuffer::PushData(const uint8_t* data_buffer, const size_t data_size)
{
	if (!this->cyclic_buffer) {
		return false;
	}
	
	size_t emptySize = this->GetEmptySize();
	if (data_size > emptySize) {
		return false;
	}
	
	const uint8_t* cyclic_buffer_end = &this->cyclic_buffer[this->cyclic_buffer_size];
	
	size_t dataRemaining = data_size;
	if (this->cyclic_buffer_empty_start >= this->cyclic_buffer_filled_start) {
		size_t cyclicBufferToEndSize = cyclic_buffer_end - this->cyclic_buffer_empty_start;
		if (dataRemaining > cyclicBufferToEndSize) {
			memcpy_s(
				this->cyclic_buffer_empty_start
				, cyclicBufferToEndSize
				, data_buffer
				, cyclicBufferToEndSize
			);
			dataRemaining -= cyclicBufferToEndSize;
			size_t cyclicBufferFromStartSize = this->cyclic_buffer_filled_start - this->cyclic_buffer;
			memcpy_s(
				this->cyclic_buffer
				, cyclicBufferFromStartSize
				, &data_buffer[cyclicBufferToEndSize]
				, dataRemaining
			);
			this->cyclic_buffer_empty_start = &this->cyclic_buffer[dataRemaining];
			dataRemaining = 0;
		}
		else {
			memcpy_s(
				this->cyclic_buffer_empty_start
				, cyclicBufferToEndSize
				, data_buffer
				, dataRemaining
			);
			this->cyclic_buffer_empty_start = &this->cyclic_buffer_empty_start[dataRemaining];
			dataRemaining = 0;
			
			if (this->cyclic_buffer_empty_start == cyclic_buffer_end) {
				this->cyclic_buffer_empty_start = this->cyclic_buffer;
			}
		}
	}
	else {
		memcpy_s(
			this->cyclic_buffer_empty_start
			, this->cyclic_buffer_filled_start - this->cyclic_buffer_empty_start
			, data_buffer
			, dataRemaining
		);
		this->cyclic_buffer_empty_start = &this->cyclic_buffer_empty_start[dataRemaining];
		dataRemaining = 0;
	}
	
	if (this->cyclic_buffer_empty_start == this->cyclic_buffer_filled_start) {
		this->cyclic_buffer_empty_start = 0;
	}
	
	return true;
}

bool CyclicBuffer::PopData(const size_t data_buffer_size, uint8_t* data_buffer, size_t* data_size)
{
	return this->PopData(data_buffer_size, data_buffer, data_size, false);
}

bool CyclicBuffer::PopData(const size_t data_buffer_size, uint8_t* data_buffer, size_t* data_size, bool peek_only)
{
	if (!this->cyclic_buffer) {
		return false;
	}
	
	*data_size = 0;
	
	const uint8_t* cyclic_buffer_end = &this->cyclic_buffer[this->cyclic_buffer_size];
	
	size_t spaceRemaining = data_buffer_size;
	if (this->cyclic_buffer_empty_start == 0 || this->cyclic_buffer_filled_start > this->cyclic_buffer_empty_start) {
		size_t cyclicBufferToEndSize = cyclic_buffer_end - this->cyclic_buffer_filled_start;
		if (spaceRemaining > cyclicBufferToEndSize) {
			if (data_buffer) {
				memcpy_s(
					data_buffer
					, spaceRemaining
					, this->cyclic_buffer_filled_start
					, cyclicBufferToEndSize
				);
			}
			spaceRemaining -= cyclicBufferToEndSize;
			*data_size = cyclicBufferToEndSize;
			size_t cyclicBufferFromStartSize = (this->cyclic_buffer_empty_start == 0 ? this->cyclic_buffer_filled_start : this->cyclic_buffer_empty_start) - this->cyclic_buffer;
			size_t copySize = (spaceRemaining > cyclicBufferFromStartSize ? cyclicBufferFromStartSize : spaceRemaining);
			if (data_buffer) {
				memcpy_s(
					&data_buffer[cyclicBufferToEndSize]
					, spaceRemaining
					, this->cyclic_buffer
					, copySize
				);
			}
			if (!peek_only) {
				if (this->cyclic_buffer_empty_start == 0) {
					this->cyclic_buffer_empty_start = this->cyclic_buffer_filled_start;
				}
				this->cyclic_buffer_filled_start = &this->cyclic_buffer[copySize];
			}
			spaceRemaining -= copySize;
			*data_size += copySize;
		}
		else {
			if (data_buffer) {
				memcpy_s(
					data_buffer
					, spaceRemaining
					, this->cyclic_buffer_filled_start
					, spaceRemaining
				);
			}
			if (!peek_only) {
				if (spaceRemaining > 0 && this->cyclic_buffer_empty_start == 0) {
					this->cyclic_buffer_empty_start = this->cyclic_buffer_filled_start;
				}
				this->cyclic_buffer_filled_start = &this->cyclic_buffer_filled_start[spaceRemaining];
				
				if (this->cyclic_buffer_filled_start == cyclic_buffer_end) {
					this->cyclic_buffer_filled_start = this->cyclic_buffer;
				}
			}
			*data_size = spaceRemaining;
			spaceRemaining = 0;
		}
	}
	else {
		size_t cyclicBufferFilledSize = this->cyclic_buffer_empty_start - this->cyclic_buffer_filled_start;
		size_t copySize = (data_buffer_size > cyclicBufferFilledSize ? cyclicBufferFilledSize : data_buffer_size);
		if (data_buffer) {
			memcpy_s(
				data_buffer
				, data_buffer_size
				, this->cyclic_buffer_filled_start
				, copySize
			);
		}
		if (!peek_only) {
			this->cyclic_buffer_filled_start = &this->cyclic_buffer_filled_start[copySize];
		}
		spaceRemaining -= copySize;
		*data_size = copySize;
	}
	
	if (*data_size == 0) {
		return false;
	}
	
	return true;
}
