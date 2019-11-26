#include "../stdint.h"
#include "../win32_seh.h"
#include "../rva.h"

namespace vcrtl::_msvc::x64 {

enum class unwind_code_t: uint8_t
{
	push_nonvolatile_reg = 0, // 1
	alloc_large = 1, // 2-3
	alloc_small = 2, // 1
	set_frame_pointer = 3, // 1
	save_nonvolatile_reg = 4, // 2
	save_nonvolatile_reg_far = 5, // 3
	epilog = 6, // 2
	_07 = 7, // 3
	save_xmm128 = 8, // 2
	save_xmm128_far = 9, // 3
	push_machine_frame = 10, // 1
};

struct pe_unwind_entry
{
	uint8_t prolog_offset;
	unwind_code_t code: 4;
	uint8_t info: 4;
};

struct pe_unwind_info
{
	uint8_t version: 3;
	uint8_t flags: 5;
	uint8_t prolog_size;
	uint8_t unwind_code_count;
	uint8_t frame_reg: 4;
	uint8_t frame_reg_disp: 4;
	union
	{
		pe_unwind_entry entries[1];
		uint16_t data[1];
	};
};

struct pe_function
{
	/* 0x00 */ rva<byte const> begin;
	/* 0x04 */ rva<byte const> end;
	/* 0x08 */ rva<pe_unwind_info const> unwind_info;
};

/*struct frame_info_t
{
	rva<void const> function;
	rva<win32_frame_handler_t> exception_routine;
	rva<void const> extra_data;
	rva<void const> rip;
	uint64_t frame_ptr;
};*/

struct alignas(16) xmm_t
{
	char _data[16];
};

struct frame_walk_context_t
{
	xmm_t xmm6;
	xmm_t xmm7;
	xmm_t xmm8;
	xmm_t xmm9;
	xmm_t xmm10;
	xmm_t xmm11;
	xmm_t xmm12;
	xmm_t xmm13;
	xmm_t xmm14;
	xmm_t xmm15;

	uint64_t rbx;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	uint64_t & gp(uint8_t idx);
};

struct mach_frame_t
{
	byte const * rip;
	uint64_t cs;
	uint64_t eflags;
	uint64_t rsp;
	uint64_t ss;
};

struct frame_walk_pdata_t
{
	static frame_walk_pdata_t for_this_image();
	static frame_walk_pdata_t from_image_base(byte const * image_base);

	byte const * image_base() const;
	bool contains_address(byte const * addr) const;
	pe_function const * find_function_entry(byte const * addr) const;

	void unwind(pe_unwind_info const & unwind_info, frame_walk_context_t & ctx, mach_frame_t & mach) const;

private:
	explicit frame_walk_pdata_t(byte const * image_base);

	byte const * _image_base;
	pe_function const * _functions;
	uint32_t _function_count;
	uint32_t _image_size;
};


}

#pragma once
