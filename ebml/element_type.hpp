#pragma once

#include <cstdint>

namespace ebml {
typedef uint8_t ElementType;
};

#define ELEMENT_TYPE_UNKNOWN 0x0
#define ELEMENT_TYPE_MASTER 0x1
#define ELEMENT_TYPE_UINT 0x2
#define ELEMENT_TYPE_INT 0x3
#define ELEMENT_TYPE_STRING 0x4
#define ELEMENT_TYPE_UNICODE 0x5
#define ELEMENT_TYPE_BINARY 0x6
#define ELEMENT_TYPE_FLOAT 0x7
#define ELEMENT_TYPE_DATE 0x8
