#pragma once

#include <cstdint>
#include <iostream>

#include "element_register.hpp"
#include "element_size.hpp"
#include "element_type.hpp"

namespace ebml {

struct Element {
	const ElementRegister reg;
	const uint64_t pos;

	virtual void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\"";
	}

	Element(const ElementRegister p_reg, const uint64_t p_pos) :
			reg(p_reg), pos(p_pos) {}
	virtual ~Element() {}
};

struct ElementMaster : public Element {
	const uint64_t from;
	const uint64_t to;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"from\": " << from << ", \"to\": " << to;
	}

	ElementMaster(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const uint64_t p_from,
			const uint64_t p_to) :
			Element(p_reg, p_pos), from(p_from), to(p_to) {}

	~ElementMaster() {}
};

struct ElementUint : public Element {
	const uint64_t value;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"value\": " << value;
	}

	ElementUint(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const uint64_t p_value) :
			Element(p_reg, p_pos), value(p_value) {}

	~ElementUint() {}
};

struct ElementInt : public Element {
	const int64_t value;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"value\": " << value;
	}

	ElementInt(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const int64_t p_value) :
			Element(p_reg, p_pos), value(p_value) {}

	~ElementInt() {}
};

struct ElementString : public Element {
	const std::string value;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"value\": \"" << value << "\"";
	}

	ElementString(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const std::string p_value) :
			Element(p_reg, p_pos), value(p_value) {}

	~ElementString() {}
};

struct ElementBinary : public Element {
	const uint8_t *const data;
	const uint64_t size;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"length\": " << size;
	}

	ElementBinary(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const uint8_t *const p_data,
			const uint64_t p_size) :
			Element(p_reg, p_pos), data(p_data), size(p_size) {}

	~ElementBinary() {
		delete[] data;
	}
};

struct ElementFloat : public Element {
	const double value;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"value\": " << value;
	}

	ElementFloat(
			const ElementRegister p_reg,
			const uint64_t p_pos,
			const double p_value) :
			Element(p_reg, p_pos), value(p_value) {}

	~ElementFloat() {}
};

struct ElementDate : public Element {
	const int64_t value;

	void debug_print() const {
		std::cout << "\"name\": \"" << reg.name << "\", \"value\": " << value;
	}

	ElementDate(
			const ElementRegister p_reg,
			const int64_t p_value,
			const uint64_t p_pos) :
			Element(p_reg, p_pos), value(p_value) {}

	~ElementDate() {}
};

}; // namespace ebml
