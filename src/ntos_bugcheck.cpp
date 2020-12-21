#include "../include/vcrtl/bugcheck.h"
#include "stdint.h"

namespace vcrtl {

extern "C" [[noreturn]] __declspec(dllimport) void __stdcall KeBugCheckEx(
    uint32_t BugCheckCode,
    uintptr_t BugCheckParameter1,
    uintptr_t BugCheckParameter2,
    uintptr_t BugCheckParameter3,
    uintptr_t BugCheckParameter4
    );

void __fastcall on_bug_check(bug_check_reason reason)
{
    static constexpr uint32_t driver_violation = 0x121;
	KeBugCheckEx(driver_violation, (uintptr_t)reason, 0, 0, 0);
}

}
