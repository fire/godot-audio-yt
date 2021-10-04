#include "buffer_stream.hpp"

#include "typedefs.hpp"

namespace ebml {

void BufferStream::read(uint8_t * const p_buffer, uint64_t &p_pos, const uint64_t p_bytes) {
	if(p_pos < 0 || p_pos + p_bytes > size) {
#ifdef __EXCEPTIONS
		throw std::runtime_error(sformat(
			"Access out of bounds: Position: %ld, Buffer Size: %ld, Total Size: %ld.",
			p_pos,
			p_bytes,
			size
		));
#else
		return;
#endif
	}
	
	for(uint64_t i = 0; i < p_bytes; ++i) {
		p_buffer[i] = data[p_pos + i];
	}
	
	p_pos += p_bytes;
}

uint64_t BufferStream::get_length() {
	return size;
}

BufferStream::BufferStream(const uint8_t * const p_data, const uint64_t p_size) : data(p_data), size(p_size) {
}

BufferStream::~BufferStream() {
}

};
