#include "stdint.h"

namespace vcrtl {

struct cxx_ptr_to_member
{
	int32_t member_offset;
	int32_t vbtable_ptr_offset;
	int32_t vbase_offset;

	uintptr_t apply(uintptr_t obj) const
	{
		if (vbtable_ptr_offset >= 0)
		{
			uintptr_t vbtable_ptr = obj + vbtable_ptr_offset;
			uintptr_t vbtable = *(uintptr_t const *)vbtable_ptr;
			obj = vbtable_ptr + *(int32_t const *)(vbtable + vbase_offset);
		}
		return obj + member_offset;
	}
};

}

#pragma once
