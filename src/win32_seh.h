#include "flags.h"
#include "stddef.h"
#include "stdint.h"

namespace vcrtl::_msvc {

enum class win32_exception_flag: uint32_t
{
	non_continuable = 0x01,
	unwinding = 0x02,
	exit_unwind = 0x04,
	stack_invalid = 0x08,
	nested_call = 0x10,
	target_unwind = 0x20,
	collided_unwind = 0x40,
};

struct win32_exception_record
{
	uint32_t code;
	flags<win32_exception_flag> flags;
	win32_exception_record * next;
	byte const * address;
	uint32_t parameter_count;
};

enum class win32_exception_disposition: uint32_t
{
	continue_execution = 0,
	continue_search = 1,
	nested = 2,
	collided = 3,
	cxx_handler = 0x154d3c64,
};

struct x86_cpu_context;
struct x64_cpu_context;

struct x86_seh_registration;

using x86_frame_handler_t = win32_exception_disposition __cdecl(
	win32_exception_record * exception_record, x86_seh_registration * registration,
	x86_cpu_context * cpu_context, void * dispatcher_context);

struct x86_seh_registration
{
	x86_seh_registration * next;
	x86_frame_handler_t * handler;
};

using x64_frame_handler_t = win32_exception_disposition(win32_exception_record * exception_record,
	byte * frame_ptr, x64_cpu_context *, void * dispatcher_context);

}

#pragma once
