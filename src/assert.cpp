#include "assert.h"
#include "bugcheck.h"

#include <ntddk.h>

void vcrtl::_verify(bool cond)
{
	if (!cond)
		on_bug_check(bug_check_reason::assertion_failure);
}

void vcrtl::on_bug_check(bug_check_reason reason)
{
	KeBugCheckEx(DRIVER_VIOLATION, (ULONG_PTR)reason, 0, 0, 0);
}
