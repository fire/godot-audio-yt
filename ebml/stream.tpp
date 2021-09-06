template<class T, bool strip_leading>
void ebml::Stream::read_num(uint64_t &p_pos, T &r_result) {
	uint64_t pos = p_pos;
	
	uint8_t first;
	read(&first, pos, 1);
	
	T full = 0;
	for(uint8_t octets = 0; octets < sizeof(T); ++octets) {
		uint64_t mask = 0b10000000 >> octets;
		if(first & mask) {
			if(strip_leading) {
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
const Type *ebml::ElementRange::Searcher::get() {
	for(const Element * const &element : element_list) {
		if(element->reg.id == ID) {
			return (const Type *)element;
		}
	}
	
	while(pos < range.to) {
		const Element *element;
		range.stream->read_element(pos, element);
		
		element_list.push_back(element);
		
		if(element->reg.id == ID) {
			return (const Type *)element;
		}
	}
	
#ifdef __EXCEPTIONS
	char *str;
	asprintf(&str, "Could not find element: %s.", get_register(ID).name);
	std::string err(str);
	free(str);
	
	throw std::runtime_error(err);
#else
	return nullptr;
#endif
}
