// `size_t` is predefined

namespace vcrtl {

using size_t = ::size_t;
using nullptr_t = decltype(nullptr);

enum class byte: unsigned char {};

#if __INTELLISENSE__
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#else
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#define container_of(ptr, type, member) reinterpret_cast<type *>((uintptr_t)(ptr) - offsetof(type, member))

}

#pragma once
