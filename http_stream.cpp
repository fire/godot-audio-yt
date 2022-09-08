#include "http_stream.hpp"

#include <iostream>
#include <stdint.h>

void HttpStream::_poll_request() {
	if (!has_parsed) {
		{
			int port;
			const Error err = url.parse_url(scheme, host, port, path);
			if (err != OK) {
				ERR_FAIL_MSG("Failed to parse URL.");
			}
		}

		has_parsed = true;
	}
	while (true) {
		if (has_content_length && cache_pos >= content_length) {
			ERR_FAIL_MSG("Request position out of bounds.");
		}
		HTTPClient::Status status = client->get_status();
		switch (status) {
			case HTTPClient::STATUS_DISCONNECTED: {
				Error err = client->connect_to_host(scheme + host);
				if (err != OK) {
					ERR_FAIL_MSG("Failed to connect to host.");
				}
			} break;
			case HTTPClient::STATUS_RESOLVING: {
				OS::get_singleton()->delay_usec(1);
				client->poll();
			} break;
			case HTTPClient::STATUS_CANT_RESOLVE: {
				ERR_FAIL_MSG("Cannot resolve host.");
			} break;
			case HTTPClient::STATUS_CONNECTING: {
				OS::get_singleton()->delay_usec(1);
				client->poll();
			} break;
			case HTTPClient::STATUS_CANT_CONNECT: {
				ERR_FAIL_MSG("Cannot connect.");
			} break;
			case HTTPClient::STATUS_CONNECTED: {
				Vector<String> headers;
				headers.push_back(vformat("Range: bytes=%s-", cache_pos + cache_buffer.size()));
				Error err = client->request(HTTPClient::Method::METHOD_GET, path, headers, cache_buffer.ptrw(), cache_buffer.size());
				if (err != OK) {
					ERR_FAIL_MSG("Failed to read from server.");
				}
			} break;
			case HTTPClient::STATUS_REQUESTING: {
				OS::get_singleton()->delay_usec(1);
				client->poll();
			} break;
			case HTTPClient::STATUS_BODY: {
				if (client->get_response_body_length() == 0) {
					List<String> headers;
					const Error headers_err = client->get_response_headers(&headers);
					if (headers_err != OK) {
						ERR_FAIL_MSG("Failed to read headers.");
					}

					String redirect;
					for (int i = 0; i < headers.size(); ++i) {
						const String header = headers[i];
						if (header.begins_with("Location: ")) {
							redirect = header.substr(10);
							client->close();
							break;
						}
					}

					if(redirect.is_empty()) {
						for(int i = 0; i < headers.size(); ++i) {
							print_error(headers[i]);
						}
						ERR_FAIL_MSG("No redirect header given.");
					}

					if (redirect.begins_with("//")) {
						redirect = redirect.insert(0, "https:");
					} else if (redirect.begins_with("/")) {
						redirect = redirect.insert(0, host);
					}

					int port;
					const Error url_err = redirect.parse_url(scheme, host, port, path);
					if (url_err != OK) {
						ERR_FAIL_MSG("Failed to parse redirect url.");
					}

					if (!redirect.is_empty()) {
						continue;
					}

					ERR_FAIL_MSG("Response body length was zero.");
				}

				if (!has_content_length) {
					has_content_length = true;
					content_length = cache_pos + client->get_response_body_length();
				}

				OS::get_singleton()->delay_usec(1);
				client->poll();

				return;
			} break;
			case HTTPClient::STATUS_CONNECTION_ERROR: {
				client->close();
			} break;
			default: {
				ERR_FAIL_MSG("Invalid connection status.");
			} break;
		}
	}
}

void HttpStream::read(uint8_t *const p_buffer, uint64_t &p_pos, const uint64_t p_bytes) {
	// If keeping the old request would involve receiving more than 50KB, make a new request.
	static const uint64_t RESET_IF_AHEAD_BY = 50000;

	// If the cache is more than 10MB behind, trim it.
	static const uint64_t TRIM_CACHE_AFTER = 10000000;

	int64_t offset = p_pos - cache_pos;
	if (offset < 0 || offset - int64_t(cache_buffer.size()) > int64_t(RESET_IF_AHEAD_BY)) {
		// Start the request from scratch.
		client->close();
		cache_pos = p_pos;
		cache_buffer.resize(0);
		offset = 0;
	}

	// Fill up the buffer using content from the request.
	while (uint64_t(cache_buffer.size()) < offset + p_bytes) {
		if (client->get_status() != HTTPClient::STATUS_BODY) {
			_poll_request();
		}
		const PackedByteArray &chunk = client->read_response_body_chunk();
		cache_buffer.append_array(chunk);
	}

	// Copy data from cache to input buffer.
	for (uint64_t i = 0; i < p_bytes; ++i) {
		p_buffer[i] = cache_buffer[offset + i];
	}

	const int64_t trim_amount = offset - TRIM_CACHE_AFTER;
	if (trim_amount > 0) {
		cache_buffer = cache_buffer.slice(trim_amount);
	}

	// Since the read was successful, move the position.
	p_pos += p_bytes;
}

uint64_t HttpStream::get_length() {
	if (!has_content_length) {
		_poll_request();
	}

	return content_length;
}

HttpStream::HttpStream(const String p_url) :
		url(p_url) {
}

HttpStream::~HttpStream() {
}
