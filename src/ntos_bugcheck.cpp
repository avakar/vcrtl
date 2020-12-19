#include "../include/vcrtl/bugcheck.h"
#include <ntddk.h>

void vcrtl::on_bug_check(bug_check_reason reason)
{
	KeBugCheckEx(DRIVER_VIOLATION, (ULONG_PTR)reason, 0, 0, 0);
}
