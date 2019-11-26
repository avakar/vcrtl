namespace vcrtl {

using int8_t  = signed char;
using int16_t = signed short;
using int32_t = signed int;
using int64_t = signed long long;

using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

#ifdef _M_AMD64

using intptr_t = int64_t;
using uintptr_t = uint64_t;

#elif defined(_M_IX86)

using intptr_t = int32_t;
using uintptr_t = uint32_t;

#else
#error Unknown platform
#endif

}

#pragma once
