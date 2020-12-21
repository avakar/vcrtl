#include "../../include/vcrtl/bugcheck.h"
#include "../stddef.h"
#include "../stdint.h"

namespace vcrtl {

[[noreturn]] void _on_rtc_esp_corruption()
{
	on_bug_check(bug_check_reason::rtc_esp_corruption);
}

extern "C" void _RTC_InitBase()
{
}

extern "C" void _RTC_Shutdown()
{
}

struct _rtc_variable_descriptor_t
{
	int32_t offset;
	int32_t size;
	char const * name;
};

struct _rtc_frame_descriptor_t
{
	int32_t var_count;
	_rtc_variable_descriptor_t const * vars;
};

extern "C" [[noreturn]] void __fastcall _RTC_CheckStackVars(byte const * frame_ptr, _rtc_frame_descriptor_t * edx)
{
	for (int32_t i = 0; i < edx->var_count; ++i)
	{
		_rtc_variable_descriptor_t const & var = edx->vars[i];
		int32_t first = var.offset;
		int32_t last = first + var.size;

		if (*reinterpret_cast<uint32_t const *>(frame_ptr + first - 4) != 0xcccccccc
			|| *reinterpret_cast<uint32_t const *>(frame_ptr + last) != 0xcccccccc)
		{
			on_bug_check(bug_check_reason::rtc_canary_corruption);
		}
	}
}

}
