#include "../../include/vcrtl/bugcheck.h"
#include "../stdint.h"

namespace vcrtl {

// XXX register cookie in load config

#ifdef _M_IX86
extern "C" uintptr_t __security_cookie = 0xbb40e64e;
#else
extern "C" uintptr_t __security_cookie = 0x00002b99'2ddfa232;
#endif

extern "C" void __fastcall __security_check_cookie(uintptr_t p1)
{
	if (p1 != __security_cookie || (p1 & 0xffff0000'00000000) != 0)
		on_bug_check(bug_check_reason::gs_failure);
}

}
