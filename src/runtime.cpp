#include "../include/vcrtl/bugcheck.h"
#include "stddef.h"
using namespace vcrtl;

void __cdecl operator delete(void *, size_t)
{
	on_bug_check(bug_check_reason::forbidden_call);
}

extern "C" void __cdecl __std_terminate()
{
	on_bug_check(bug_check_reason::std_terminate);
}
