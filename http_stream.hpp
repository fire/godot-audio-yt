#pragma once

#include "core/io/http_client.h"
#include "core/variant/variant.h"
#include "ebml/stream.hpp"
#include "core/io/http_client_tcp.h"

class HttpStream : public ebml::Stream {
	const String url;

	bool has_parsed = false;
	String scheme;
	String host;
	String path;

	Ref<HTTPClientTCP> client;

	uint64_t cache_pos = 0;
	PackedByteArray cache_buffer;

	bool has_content_length = false;
	uint64_t content_length = 0;

protected:
	virtual void _poll_request();

public:
	virtual void read(uint8_t *const p_buffer, uint64_t &p_pos, const uint64_t p_bytes);
	virtual uint64_t get_length();

	HttpStream(const String p_url);
	virtual ~HttpStream();
};
