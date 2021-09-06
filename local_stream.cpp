#include "local_stream.hpp"

#include "core/variant.h"

void LocalStream::read(uint8_t * const p_buffer, uint64_t &p_pos, const uint64_t p_bytes) {
	if(p_pos < 0 || p_pos + p_bytes > get_length()) {
		for(uint64_t i = 0; i < p_bytes; ++i) {
			p_buffer[i] = 0;
		}
		ERR_FAIL_MSG(
			String()
				+ "Access out of bounds: Position: "
				+ Variant(p_pos)
				+ ", Buffer Size: "
				+ Variant(p_bytes)
				+ ", Total Size: "
				+ Variant(get_length())
				+ "."
		);
	}
	
	file->seek(p_pos);
	if(file->get_buffer(p_buffer, p_bytes) != p_bytes) {
		ERR_FAIL_MSG("Could not read from file.");
	}
	
	// Since the read was successful, move the position.
	p_pos += p_bytes;
}

uint64_t LocalStream::get_length() {
	return file->get_len();
}

LocalStream::LocalStream(const String p_path) : path(p_path) {
	file = FileAccess::open(path, FileAccess::READ);
}

LocalStream::~LocalStream() {
	file->close();
	memdelete(file);
}
