#include "cpu_context.h"
#include "../utils.h"
#include "eh_structs_x64.h"

namespace vcrtl::_msvc::x64 {

struct cxx_catch_info_t
{
	byte const * continuation_address[2];
	byte * primary_frame_ptr;
	void * exception_object_or_link;
	cxx_throw_info const * throw_info_if_owner;
	bytes<uint64_t> unwind_context;

	void * get_exception_object() const
	{
		if (this->throw_info_if_owner)
		{
			return this->exception_object_or_link;
		}
		else if (this->exception_object_or_link)
		{
			cxx_catch_info_t const * other = (cxx_catch_info_t const *)this->exception_object_or_link;
			return other->exception_object_or_link;
		}
		else
		{
			return nullptr;
		}
	}

	cxx_throw_info const * get_throw_info() const
	{
		if (this->exception_object_or_link)
		{
			if (!this->throw_info_if_owner)
			{
				cxx_catch_info_t const * other = (cxx_catch_info_t const *)this->exception_object_or_link;
				return other->throw_info_if_owner;
			}

			return this->throw_info_if_owner;
		}
		else
		{
			return nullptr;
		}
	}
};

struct cxx_throw_frame_t
{
	uint64_t red_zone[4];

	frame_walk_context_t ctx;
	mach_frame_t mach;
	cxx_catch_info_t catch_info;
};

void probe_for_exception_object(frame_walk_pdata_t const & pdata, cxx_throw_frame_t & frame);

bool process_catch_block(byte const * image_base, flags<cxx_catch_flag> adjectives,
	type_info const * match_type, void * catch_var, void * exception_object, cxx_throw_info const & throw_info);

extern symbol unwind_cookie;
extern symbol rethrow_probe_cookie;

struct cxx_dispatcher_context_t
{
	symbol * cookie;
	cxx_throw_frame_t * throw_frame;
	frame_walk_pdata_t const * pdata;
	pe_function const * fnent;
	void const * extra_data;
	byte const * handler;
};

}

#pragma once
