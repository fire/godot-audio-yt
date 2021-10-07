#pragma once

#include "stream.hpp"

namespace ebml {

class BufferStream : public Stream {
	const uint8_t *const data;
	const uint64_t size;

public:
	virtual void read(uint8_t *const p_buffer, uint64_t &p_pos, const uint64_t p_bytes);
	virtual uint64_t get_length();

	BufferStream(const uint8_t *const p_data, const uint64_t p_size);
	virtual ~BufferStream();
};

}; // namespace ebml
