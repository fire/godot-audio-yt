#pragma once

#include <string>

template<typename... Args>
std::string sformat(const std::string &format, Args... args) {
	const size_t size = std::snprintf(nullptr, 0, format.c_str(), args...);
	char * const str = new char[size + 1];
	std::snprintf(str, size + 1, format.c_str(), args...);
	std::string result(str);
	delete[] str;
	
	return result;
}
