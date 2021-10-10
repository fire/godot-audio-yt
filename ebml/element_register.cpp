#include "element_register.hpp"

#include <iostream>

namespace ebml {
// TODO: Binary search, or just a map.
ElementRegister get_register(const ElementID &p_id) {
	for (ElementRegister reg : ELEMENT_REGISTERS) {
		if (reg.id == p_id) {
			return reg;
		}
	}
	std::cerr << "WARNING: Unknown register: " << p_id << "." << std::endl;
	return ELEMENT_REGISTER_UNKNOWN;
}
}; // namespace ebml
