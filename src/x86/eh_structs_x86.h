#include "../ptr_to_member.h"
#include "../flags.h"
#include "../stdint.h"

namespace vcrtl::_msvc::x86 {

enum class cxx_throw_flag: uint32_t
{
	is_const = 0x01,
	is_volatile = 0x02,
	is_unaligned = 0x04,
	is_pure = 0x08,
	is_winrt = 0x10,
};

enum class cxx_catchable_property: uint32_t
{
	is_simple_type = 0x01,
	by_reference_only = 0x02,
	has_virtual_base = 0x04,
	is_winrt_handle = 0x08,
	is_std_bad_alloc = 0x10,
};

struct cxx_catchable_type
{
	flags<cxx_catchable_property> properties;
	type_info const * desc;
	cxx_ptr_to_member offset;
	uint32_t size;
	uint32_t copy_fn_rva;
};

struct cxx_catchable_type_list
{
	uint32_t count;
#pragma warning(suppress: 4200)
	cxx_catchable_type const * types[];
};

struct throw_info_t
{
	flags<cxx_throw_flag> attributes;
	void (__fastcall * destroy_exc_obj)(void *);
	int (__cdecl * compat_fn)(...);
	cxx_catchable_type_list const * catchables;
};

struct cxx_eh_funclet;

struct cxx_unwind_graph_edge
{
	int32_t next;
	cxx_eh_funclet * cleanup_handler;
};

enum class cxx_catch_flag : uint32_t
{
	is_const = 1,
	is_volatile = 2,
	is_unaligned = 4,
	is_reference = 8,
	is_resumable = 0x10,
	is_ellipsis = 0x40,
	is_bad_alloc_compat = 0x80,
	is_complus_eh = 0x80000000,
};

struct cxx_catch_handler
{
	flags<cxx_catch_flag> adjectives;
	type_info const * type_desc;
	int32_t catch_object_offset;
	cxx_eh_funclet * handler;
};

struct cxx_try_block
{
	/* 0x00 */ int32_t try_low;
	/* 0x04 */ int32_t try_high;
	/* 0x08 */ int32_t catch_high;
	/* 0x0c */ int32_t catch_count;
	/* 0x10 */ cxx_catch_handler const * catch_handlers;
};

enum class cxx_eh_flag : uint32_t
{
	compiled_with_ehs = 1,
	is_noexcept = 4,
};

struct cxx_function_x86_eh_info
{
	/* 0x00 */ uint32_t _magic;
	/* 0x04 */ uint32_t state_count;
	/* 0x08 */ cxx_unwind_graph_edge const * unwind_graph;
	/* 0x0c */ int32_t try_block_count;
	/* 0x10 */ cxx_try_block const * try_blocks;
	/* 0x14 */ uint32_t _14;
	/* 0x18 */ uint32_t _18;

	// `_magic >= 0x19930521`
	/* 0x1c */ uint32_t es_types;

	// `_magic >= 0x19930522`
	/* 0x20 */ flags<cxx_eh_flag> eh_flags;
	/* 0x24 */
};

}

#pragma once
