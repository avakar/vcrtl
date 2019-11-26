#include "stddef.h"

extern "C" void * memcpy(void * dest, void const * src, size_t size);
#pragma intrinsic(memcpy)

#pragma once
