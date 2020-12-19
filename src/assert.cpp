#include "assert.h"
#include "../include/vcrtl/bugcheck.h"


void vcrtl::_verify(bool cond)
{
	if (!cond)
		on_bug_check(bug_check_reason::assertion_failure);
}
