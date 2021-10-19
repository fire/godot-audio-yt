// https://matroska-org.github.io/libebml/specs.html
// https://github.com/quadrifoglio/libmkv
// https://www.webmproject.org/docs/container/

#include "stream.hpp"

#include <memory.h>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <iostream>

ebml::ElementRange::Iterator::reference &ebml::ElementRange::Iterator::operator*() const {
	return element;
}

ebml::ElementRange::Iterator::value_type ebml::ElementRange::Iterator::operator->() const {
	return **this;
}

ebml::ElementRange::Iterator &ebml::ElementRange::Iterator::operator++() {
	pos = next;

	if (element != nullptr) {
		delete element;
	}
	element = nullptr;

	if (pos < end) {
		stream->read_element(next, element);
	}

	return *this;
}

ebml::ElementRange::Iterator ebml::ElementRange::Iterator::operator++(int) {
	Iterator tmp = *this;
	++(*this);
	return tmp;
}

ebml::ElementRange::Iterator::Iterator(
		Stream *const p_stream,
		const uint64_t p_pos,
		const uint64_t p_end) :
		stream(p_stream), pos(p_pos), end(p_end) {
	if (pos < end) {
		stream->read_element(next, element);
	}
}

ebml::ElementRange::Iterator::~Iterator() {
	delete element;
}

ebml::ElementRange::Iterator ebml::ElementRange::begin() const {
	return Iterator(stream, from, to);
}

ebml::ElementRange::Iterator ebml::ElementRange::end() const {
	return Iterator(stream, to, to);
}

ebml::Searcher::Searcher(const ElementRange p_range) :
		range(p_range) {
}

ebml::Searcher::~Searcher() {
	for (const Element *const element : element_list) {
		delete element;
	}
}

ebml::Searcher ebml::ElementRange::search() const {
	return Searcher(*this);
}

ebml::ElementRange::ElementRange(
		Stream *const p_stream,
		const uint64_t p_from,
		const uint64_t p_to) :
		stream(p_stream), from(p_from), to(p_to) {
}

std::string ebml::Stream::ebml_read_string(uint64_t &p_pos, const uint64_t p_size) {
	uint8_t *const data = new uint8_t[p_size + 1];
	data[p_size] = '\0';
	read(data, p_pos, p_size);
	std::string result((char *)data);
	delete[] data;
	return result;
}

void ebml::Stream::read_int(uint64_t &p_pos, int64_t &r_result) {
	read_num<int64_t, true>(p_pos, r_result);
}

void ebml::Stream::read_id(uint64_t &p_pos, ElementID &r_result) {
	read_num<ElementID, false>(p_pos, r_result);
}

void ebml::Stream::read_size(uint64_t &p_pos, ElementSize &r_result) {
	read_num<ElementSize, true>(p_pos, r_result);
}

void ebml::Stream::read_element(uint64_t &p_pos, const Element *&r_element) {
	uint64_t pos = p_pos;

	ElementID id;
	read_id(pos, id);

	ElementSize size;
	read_size(pos, size);

	const ElementRegister &reg = get_register(id);
	Element *element;
	switch (reg.type) {
		case ELEMENT_TYPE_MASTER: {
			element = new ElementMaster(reg, p_pos, pos, pos + size);
			pos += size;
		} break;
		case ELEMENT_TYPE_UINT: {
			const uint64_t result = ebml_read_copy_reverse<uint64_t>(pos, size);
			element = new ElementUint(reg, p_pos, result);
		} break;
		case ELEMENT_TYPE_INT: {
			const int64_t result = ebml_read_copy_reverse<int64_t>(pos, size);
			element = new ElementInt(reg, p_pos, result);
		} break;
		case ELEMENT_TYPE_STRING:
		case ELEMENT_TYPE_UNICODE: {
			const std::string result = ebml_read_string(pos, size);
			element = new ElementString(reg, p_pos, result);
		} break;
		case ELEMENT_TYPE_BINARY: {
			uint8_t *const data = new uint8_t[size];
			read(data, pos, size);
			element = new ElementBinary(reg, p_pos, data, size);
		} break;
		case ELEMENT_TYPE_FLOAT: {
			double value;
			switch (size) {
				case 4: {
					value = ebml_read_copy_reverse<float>(pos, 4);
				} break;
				case 8: {
					value = ebml_read_copy_reverse<double>(pos, 8);
				} break;
				default: {
					value = 0.0;
				}
			}
			element = new ElementFloat(reg, p_pos, value);
		} break;
		case ELEMENT_TYPE_DATE: {
			const int64_t result = ebml_read_copy_reverse<int64_t>(pos, size);
			element = new ElementDate(reg, p_pos, result);
		} break;
		default: {
			element = new Element(reg, p_pos);
			pos += size;
		} break;
	}

	p_pos = pos;
	r_element = element;
}

ebml::ElementRange ebml::Stream::range(const ElementMaster *const p_element) {
	return ElementRange(this, p_element->from, p_element->to);
}

ebml::ElementRange ebml::Stream::range() {
	return ElementRange(this, 0, get_length());
}

ebml::Stream::~Stream() {
}
