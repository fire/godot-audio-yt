#pragma once

#include "element.hpp"
#include "element_id.hpp"
#include "element_register.hpp"
#include "element_size.hpp"
#include "typedefs.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ebml {

class Stream;
class Searcher;

/**
 * A range of bytes inside the stream. This is intended for iterating through elements.
 * 
 * @see Stream::range
 */
class ElementRange {
public:
	Stream *const stream;
	const uint64_t from;
	const uint64_t to;

public:
	/**
	 * Iterator to iterate through the elements in this range.
	 * 
	 * Elements allocated by this iterator are automatically deleted when the iterator is destroyed.
	 */
	class Iterator {
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = const Element *;
		using pointer = value_type *const;
		using reference = value_type const &;

		Stream *const stream;
		uint64_t pos;
		uint64_t end;
		uint64_t next = pos;
		value_type element = nullptr;

	public:
		reference &operator*() const;
		value_type operator->() const;

		Iterator &operator++();
		Iterator operator++(int);

		friend bool operator==(const Iterator &a, const Iterator &b) { return a.stream == b.stream && a.pos == b.pos; };
		friend bool operator!=(const Iterator &a, const Iterator &b) { return !(a == b); };

		Iterator(Stream *const p_stream, const uint64_t p_pos, const uint64_t p_end);
		~Iterator();
	};

	Iterator begin() const;
	Iterator end() const;

	/**
	 * Create a new searcher object. This can be used to simplify the element searching process.
	 * 
	 * @see Searcher
	 * @see Searcher::get
	 * 
	 * @returns A new searcher object.
	 */
	Searcher search() const;

	ElementRange(Stream *const p_stream, const uint64_t p_from, const uint64_t p_to);
};

/**
 * Allows directly searching for an element in this range based on their IDs.
 * 
 * @see ElementRange::search
 * @see get
 */
class Searcher {
public:
	const ElementRange range;

	std::vector<const Element *> element_list;
	uint64_t pos = range.from;

public:
	/**
	 * Search for an element represented by its ID.
	 * 
	 * If the element is not found, an exception is thrown.
	 * 
	 * If the Searcher object is destroyed, all elements discovered with this method will be deleted.
	 * 
	 * @tparam ID The ID of the element to search for.
	 * @tparam Type The type to cast the found element to.
	 * @returns The found element.
	 * @throws std::runtime_error If the element was not found.
	 */
	template <uint64_t ID, typename Type>
	const Type *get();

	Searcher(const ElementRange p_range);
	~Searcher();
};

/**
 * Virtual stream class for parsing EBML.
 * 
 * All methods used for reading EBML information have a parameter for position and a reference parameter for the result.
 */
class Stream {
	template <class T, bool strip_leading>
	void read_num(uint64_t &p_pos, T &r_result);

	template <class T>
	T ebml_read_copy_reverse(uint64_t &p_pos, const uint64_t p_size);

	template <class T, class Cast = uint8_t *>
	T ebml_read_construct(uint64_t &p_pos, const uint64_t p_size);

protected:
	/**
	 * Virtual method to read an arbitrary amount of data from the input.
	 * 
	 * Implementations should fill each value of `p_buffer` to avoid undefined behavior.
	 * Implementations should increment `p_pos` by `p_bytes` when the write is successful.
	 * Implementations may throw an exception if something goes wrong.
	 * 
	 * @param[in] p_buffer Pointer of the array to write into.
	 * @param[in, out] p_pos Position to read from in the input data. Will be incremented if the read is successful.
	 * @param[in] p_bytes Number of bytes to read from the input data.
	 */
	virtual void read(uint8_t *const p_buffer, uint64_t &p_pos, const uint64_t p_bytes) = 0;

public:
	/**
	 * Virtual method to get the total length of the input data.
	 * 
	 * @returns The total amount of bytes available in the input data.
	 */
	virtual uint64_t get_length() = 0;

	/**
	 * Read a variable size integer from the stream according to the EBML specification.
	 * 
	 * @param[in, out] p_pos Position to read from in the input data. Will be incremented if the read is successful.
	 * @param[out] r_result The resulting integer.
	 */
	void read_int(uint64_t &p_pos, int64_t &r_result);

	/**
	 * Read an ID from the stream according to the EBML specification.
	 * 
	 * @param[in, out] p_pos Position to read from in the input data. Will be incremented if the read is successful.
	 * @param[out] r_result The resulting id.
	 */
	void read_id(uint64_t &p_pos, ElementID &r_result);

	/**
	 * Read the element data size from the stream according to the EBML specification.
	 * 
	 * @param[in, out] p_pos Position to read from in the input data. Will be incremented if the read is successful.
	 * @param[out] r_result The resulting size.
	 */
	void read_size(uint64_t &p_pos, ElementSize &r_result);

	/**
	 * Read an EBML element from the stream according to the EBML specification.
	 * 
	 * This method allocates a new pointer to the resulting element. If is up to the caller to later free this element
	 * with `delete` to avoid a memory leak.
	 * 
	 * @param[in, out] p_pos Position to read from in the input data. Will be incremented if the read is successful.
	 * @param[out] r_result The resulting element.
	 */
	void read_element(uint64_t &p_pos, const Element *&r_element);

	/**
	 * Helper function that creates an ElementRange that corresponds to this element's children.
	 * 
	 * @param p_element A master element
	 * @returns A range that corresponds to the children of a master element.
	 */
	ElementRange range(const ElementMaster *const p_element);

	/**
	 * Helper function that creates an ElementRange that corresponds to the entire stream.
	 * 
	 * @returns A range that corresponds to all primary elements.
	 */
	ElementRange range();

	virtual ~Stream();
};

}; // namespace ebml

template <class T>
T ebml::Stream::ebml_read_copy_reverse(uint64_t &p_pos, const uint64_t p_size) {
	uint8_t *const data = new uint8_t[p_size];
	read((uint8_t *)&data, p_pos, p_size);

	T result = default(T);
	uint8_t *const result_ptr = (uint8_t *)&result;
	for (uint64_t i = 0; i < p_size; ++i) {
		result_ptr[i] = data[p_size - 1 - i];
	}
	delete[] data;

	return result;
}

template <class T, class Cast>
T ebml::Stream::ebml_read_construct(uint64_t &p_pos, const uint64_t p_size) {
	uint8_t *const data = new uint8_t[p_size];
	read((uint8_t *)&data, p_pos, p_size);
	delete[] data;
	T result((Cast)data);
	return result;
}

template <class T, bool strip_leading>
void ebml::Stream::read_num(uint64_t &p_pos, T &r_result) {
	uint64_t pos = p_pos;

	uint8_t first;

	read(&first, pos, 1);

	T full = 0;
	for (uint8_t octets = 0; octets < sizeof(T); ++octets) {
		uint64_t mask = 0b10000000 >> octets;
		if (first & mask) {
			if (strip_leading) {
				first ^= mask;
			}
			full |= first << octets * 8;
			break;
		}

		uint8_t next;
		read(&next, pos, 1);

		full <<= 8;
		full |= next;
	}

	p_pos = pos;
	r_result = full;
}

template <uint64_t ID, typename Type>
const Type *ebml::Searcher::get() {
	for (const Element *const element : element_list) {
		if (element->reg.id == ID) {
			return (const Type *)element;
		}
	}

	while (pos < range.to) {
		const Element *element;
		range.stream->read_element(pos, element);
		element_list.push_back(element);

		if (element->reg.id == ID) {
			return (const Type *)element;
		}
	}

#ifdef __EXCEPTIONS
	throw std::runtime_error(sformat(
			"Could not find element: %s.",
			get_register(ID).name));
#else
	return nullptr;
#endif
}
