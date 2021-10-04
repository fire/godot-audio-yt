// https://matroska-org.github.io/libebml/specs.html
// https://github.com/quadrifoglio/libmkv
// https://www.webmproject.org/docs/container/

#include "stream.hpp"

#include <algorithm>
#include <cassert>
#include <bitset>
#include <iostream>
#include <memory.h>

ebml::ElementRange::Iterator::reference &ebml::ElementRange::Iterator::operator*() const {
	return element;
}

ebml::ElementRange::Iterator::value_type ebml::ElementRange::Iterator::operator->() const {
	return **this;
}

ebml::ElementRange::Iterator &ebml::ElementRange::Iterator::operator++() {
	pos = next;
	
	if(element != nullptr) {
		delete element;
	}
	element = nullptr;
	
	if(pos < end) {
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
	Stream * const p_stream,
	const uint64_t p_pos,
	const uint64_t p_end
) : stream(p_stream), pos(p_pos), end(p_end) {
	if(pos < end) {
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

ebml::ElementRange::Searcher::Searcher(const ElementRange &p_range) : range(p_range) {
}

ebml::ElementRange::Searcher::~Searcher() {
	for(const Element * const element : element_list) {
		delete element;
	}
}

ebml::ElementRange::Searcher ebml::ElementRange::search() {
	return Searcher(*this);
}

ebml::ElementRange::ElementRange(
	Stream * const p_stream,
	const uint64_t p_from,
	const uint64_t p_to
) : stream(p_stream), from(p_from), to(p_to) {
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

void reverse_copy(uint8_t * const dst, const uint8_t * const src, const uint64_t n) {
	for(uint64_t i = 0; i < n; ++i) {
		dst[i] = src[n - 1 - i];
	}
}

void ebml::Stream::read_element(uint64_t &p_pos, const Element *&r_element) {
	uint64_t pos = p_pos;
	
	ElementID id;
	read_id(pos, id);
	
	ElementSize size;
	read_size(pos, size);
	
	const ElementRegister &reg = get_register(id);
	Element *element;
	switch(reg.type) {
		case ELEMENT_TYPE_MASTER: {
			element = new ElementMaster(reg, p_pos, pos, pos + size);
			pos += size;
		} break;
		case ELEMENT_TYPE_UINT: {
			uint8_t data[size];
			uint64_t result = 0;
			read((uint8_t *)&data, pos, size);
			
			reverse_copy((uint8_t *)&result, (uint8_t *)&data, size);
			
			element = new ElementUint(reg, p_pos, result);
		} break;
		case ELEMENT_TYPE_INT: {
			uint8_t data[size];
			int64_t result = 0;
			read((uint8_t *)&data, pos, size);
			
			reverse_copy((uint8_t *)&result, (uint8_t *)&data, size);
			
			element = new ElementInt(reg, p_pos, result);
		} break;
		case ELEMENT_TYPE_STRING:
		case ELEMENT_TYPE_UNICODE: {
			uint8_t * const data = new uint8_t[size];
			read(data, pos, size);
			const std::string value((const char *)data);
			element = new ElementString(reg, p_pos, value);
		} break;
		case ELEMENT_TYPE_BINARY: {
			uint8_t * const data = new uint8_t[size];
			read(data, pos, size);
			element = new ElementBinary(reg, p_pos, data, size);
		} break;
		case ELEMENT_TYPE_FLOAT: {
			double value;
			switch(size) {
				case 4: {
					uint8_t data[4];
					float result;
					read((uint8_t *)&data, pos, 4);
					
					reverse_copy((uint8_t *)&result, (uint8_t *)&data, 4);
					
					value = result;
				} break;
				case 8: {
					uint8_t data[8];
					double result;
					read((uint8_t *)&data, pos, 8);
					
					reverse_copy((uint8_t *)&result, (uint8_t *)&data, 8);
					
					value = result;
				} break;
				default: {
					value = 0.0;
				}
			}
			element = new ElementFloat(reg, p_pos, value);
		} break;
		case ELEMENT_TYPE_DATE: {
			uint8_t data[size];
			int64_t result = 0;
			read((uint8_t *)&data, pos, size);
			
			reverse_copy((uint8_t *)&result, (uint8_t *)&data, size);
			
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

ebml::ElementRange ebml::Stream::range(const ElementMaster * const p_element) {
	return ElementRange(this, p_element->from, p_element->to);
}

ebml::ElementRange ebml::Stream::range() {
	return ElementRange(this, 0, get_length());
}

ebml::Stream::~Stream() {
}
