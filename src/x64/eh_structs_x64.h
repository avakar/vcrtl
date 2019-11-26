#include "../stdint.h"
#include "../ptr_to_member.h"
#include "../rva.h"

namespace vcrtl::_msvc::x64 {

struct gs_handler_data_t
{
	static constexpr uint32_t EHandler = 1;
	static constexpr uint32_t UHandler = 2;
	static constexpr uint32_t HasAlignment = 4;
	static constexpr uint32_t CookieOffsetMask = ~7u;

	uint32_t cookie_offset;
	int32_t aligned_base_offset;
	int32_t alignment;
};

struct c_handler_entry_t
{
	uint32_t try_rva_low;
	uint32_t try_rva_high;
	union
	{
		rva<int32_t(void * ptrs, uint64_t frame_ptr)> filter;
		rva<void(bool, uint64_t frame_ptr)> unwinder;
	};
	uint32_t target_rva;
};

struct c_handler_data_t
{
	uint32_t entry_count;
	c_handler_entry_t entries[1];
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
	rva<type_info const> desc;
	cxx_ptr_to_member offset;
	uint32_t size;
	rva<byte const> copy_fn;
};

struct cxx_catchable_type_list
{
	uint32_t count;
	rva<cxx_catchable_type const> types[1];
};

enum class cxx_throw_flag: uint32_t
{
	is_const = 0x01,
	is_volatile = 0x02,
	is_unaligned = 0x04,
	is_pure = 0x08,
	is_winrt = 0x10,
};

struct cxx_throw_info
{
	flags<cxx_throw_flag> attributes;
	rva<void __fastcall(void *)> destroy_exc_obj;
	rva<int(...)> compat_fn;
	rva<cxx_catchable_type_list const> catchables;
};

enum class cxx_catch_flag: uint32_t
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

struct cxx_eh_node_catch
{
	byte * primary_frame_ptr;
};

struct cxx_catch_handler
{
	flags<cxx_catch_flag> adjectives;
	rva<type_info const> type_desc;
	rva<void> catch_object_offset;
	rva<byte const> handler;
	rva<cxx_eh_node_catch const> node_offset;
};

struct cxx_try_block
{
	/* 0x00 */ int32_t try_low;
	/* 0x04 */ int32_t try_high;
	/* 0x08 */ int32_t catch_high;
	/* 0x0c */ int32_t catch_count;
	/* 0x10 */ rva<cxx_catch_handler const> catch_handlers;
};

struct cxx_eh_region
{
	rva<byte const> first_ip;
	int32_t state;
};

struct alignas(8) cxx_eh_node
{
	int32_t state;
	int32_t _unused;
};

struct cxx_unwind_graph_edge
{
	int32_t next;
	rva<byte const> cleanup_handler;
};

enum class cxx_eh_flag : uint32_t
{
	compiled_with_ehs = 1,
	is_noexcept = 4,
};

struct cxx_function_eh_info
{
	/* 0x00 */ uint32_t _magic;
	/* 0x04 */ uint32_t state_count;
	/* 0x08 */ rva<cxx_unwind_graph_edge const> unwind_graph;
	/* 0x0c */ int32_t try_block_count;
	/* 0x10 */ rva<cxx_try_block const> try_blocks;
	/* 0x14 */ uint32_t region_count;
	/* 0x18 */ rva<cxx_eh_region const> regions;
	/* 0x1c */ rva<cxx_eh_node> eh_node_offset;

	// `magic_num >= 0x19930521`
	/* 0x20 */ uint32_t es_types;

	// `magic_num >= 0x19930522`
	/* 0x24 */ flags<cxx_eh_flag> eh_flags;
};

struct cxx_eh_handler_data
{
	rva<cxx_function_eh_info const> eh_info;
};

}

#pragma once
