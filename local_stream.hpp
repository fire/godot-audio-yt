#pragma once

#include "core/io/file_access.h"
#include "ebml/stream.hpp"

class LocalStream : public ebml::Stream {
	const String path;

	Ref<FileAccess> file;

public:
	virtual void read(uint8_t *const p_buffer, uint64_t &p_pos, const uint64_t p_bytes);
	virtual uint64_t get_length();

	LocalStream(const String p_path);
	virtual ~LocalStream();
};
