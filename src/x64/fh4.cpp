#include "cpu_context.h"
#include "throw.h"
#include "../utils.h"
#include "../assert.h"

using namespace vcrtl;
using namespace vcrtl::_msvc;
using namespace vcrtl::_msvc::x64;

struct gs_check_data_t
{
	uint32_t _00;
};

struct gs4_data_t
{
	rva<uint8_t> func_info;
	gs_check_data_t gs_check_data;
};

enum class eh4_flag: uint8_t
{
	is_catch_funclet = 0x01,
	has_multiple_funclets = 0x02,
	bbt = 0x04,  // Flags set by Basic Block Transformations
	has_unwind_map = 0x08,
	has_try_block_map = 0x10,
	ehs = 0x20,
	is_noexcept = 0x40,
};

struct fh4_info
{
	flags<eh4_flag> flags;
	uint32_t bbt_flags;
	rva<uint8_t const> unwind_graph;
	rva<uint8_t const> try_blocks;
	rva<uint8_t const> regions;
	rva<byte *> primary_frame_ptr;
};

static uint32_t read_unsigned(uint8_t const ** data)
{
	uint8_t enc_type = (*data)[0] & 0xf;

	static constexpr uint8_t lengths[] = {
		1, 2, 1, 3, 1, 2, 1, 4,
		1, 2, 1, 3, 1, 2, 1, 5,
	};
	static constexpr uint8_t shifts[] = {
		0x19, 0x12, 0x19, 0x0b, 0x19, 0x12, 0x19, 0x04,
		0x19, 0x12, 0x19, 0x0b, 0x19, 0x12, 0x19, 0x00,
	};

	uint8_t length = lengths[enc_type];

	// XXX we're in UB land here
	uint32_t r = *(uint32_t *)(*data + length - 4);
	r >>= shifts[enc_type];

	*data += length;
	return r;
}

static int32_t read_int(uint8_t const ** data)
{
	// XXX alignment

	int32_t r = *(int32_t *)(*data);
	*data += 4;
	return r;
}

template <typename T = void>
static rva<T> read_rva(uint8_t const ** data)
{
	uint32_t offset = read_int(data);
	return rva<T>::from_displacement(offset);
}

static void _load_eh_info(fh4_info & eh_info, uint8_t const * data, byte const * image_base, pe_function const & fnent)
{
	flags<eh4_flag> flags = (eh4_flag)*data++;
	eh_info.flags = flags;

	if (flags.has_any_of(eh4_flag::bbt))
		eh_info.bbt_flags = read_unsigned(&data);

	if (flags.has_any_of(eh4_flag::has_unwind_map))
		eh_info.unwind_graph = read_rva<uint8_t const>(&data);

	if (flags.has_any_of(eh4_flag::has_try_block_map))
		eh_info.try_blocks = read_rva<uint8_t const>(&data);

	if (flags.has_any_of(eh4_flag::has_multiple_funclets))
	{
		uint8_t const * funclet_map = image_base + read_rva<uint8_t const>(&data);

		uint32_t count = read_unsigned(&funclet_map);
		eh_info.regions = nullptr;

		for (uint32_t idx = 0; idx != count; ++idx)
		{
			rva<void const> fn_rva = read_rva<void const>(&funclet_map);
			rva<uint8_t const> regions = read_rva<uint8_t const>(&funclet_map);

			if (fn_rva == fnent.begin)
			{
				eh_info.regions = regions;
				break;
			}
		}
	}
	else
	{
		eh_info.regions = read_rva<uint8_t const>(&data);
	}

	if (flags.has_any_of(eh4_flag::is_catch_funclet))
		eh_info.primary_frame_ptr = rva<byte *>::from_displacement(read_unsigned(&data));
}

static int32_t _lookup_region(fh4_info const * eh_info, byte const * image_base, rva<byte const> fn, byte const * control_pc)
{
	if (!eh_info->regions)
		return -1;

	rva pc = make_rva(control_pc, image_base + fn);
	uint8_t const * p = image_base + eh_info->regions;

	int32_t state = -1;
	uint32_t region_count = read_unsigned(&p);
	rva<byte const> fn_rva = nullptr;
	for (uint32_t idx = 0; idx != region_count; ++idx)
	{
		fn_rva += read_unsigned(&p);
		if (pc < fn_rva)
			break;

		state = read_unsigned(&p) - 1;
	}

	return state;
}

enum class catch_block_flag: uint8_t
{
	has_type_flags = 0x01,
	has_type_info = 0x02,
	has_catch_var = 0x04,
	image_rva = 0x08,
	continuation_addr_count = 0x30,
};

enum class unwind_edge_type
{
	trivial = 0,
	object_offset = 1,
	object_ptr_offset = 2,
	function = 3,
};

struct unwind_edge_t
{
	uint32_t target_offset;
	unwind_edge_type type;
	rva<void()> destroy_fn;
	rva<void> object;

	static unwind_edge_t read(uint8_t const ** p)
	{
		unwind_edge_t r;

		uint32_t target_offset_and_type = read_unsigned(p);
		unwind_edge_type type = static_cast<unwind_edge_type>(target_offset_and_type & 3);
		r.type = type;
		r.target_offset = target_offset_and_type >> 2;
		r.destroy_fn = type != unwind_edge_type::trivial? read_rva<void()>(p): 0;
		r.object = rva<void>::from_displacement(
			type == unwind_edge_type::object_offset || type == unwind_edge_type::object_ptr_offset? read_unsigned(p): 0);

		return r;
	}

	static void skip(uint8_t const ** p)
	{
		(void)read(p);
	}
};

static win32_exception_disposition _frame_handler(byte * frame_ptr, cxx_dispatcher_context_t * dispatcher_context)
{
	if (dispatcher_context->cookie == &rethrow_probe_cookie)
		return win32_exception_disposition::cxx_handler;
	if (dispatcher_context->cookie != &unwind_cookie)
		return win32_exception_disposition::continue_search;

	byte const * image_base = dispatcher_context->pdata->image_base();
	cxx_throw_frame_t * throw_frame = dispatcher_context->throw_frame;
	cxx_catch_info_t & catch_info = throw_frame->catch_info;

	gs4_data_t const * handler_data = (gs4_data_t const *)dispatcher_context->extra_data;
	uint8_t const * compressed_data = image_base + handler_data->func_info;

	fh4_info eh_info = {};
	_load_eh_info(eh_info, compressed_data, image_base, *dispatcher_context->fnent);

	byte * primary_frame_ptr;
	int32_t state;

	if (catch_info.primary_frame_ptr >= frame_ptr)
	{
		primary_frame_ptr = catch_info.primary_frame_ptr;
		state = catch_info.unwind_context.as<int32_t>();
	}
	else
	{
		state = _lookup_region(&eh_info, image_base, dispatcher_context->fnent->begin, throw_frame->mach.rip);
		if (eh_info.flags.has_any_of(eh4_flag::is_catch_funclet))
			primary_frame_ptr = *(frame_ptr + eh_info.primary_frame_ptr);
		else
			primary_frame_ptr = frame_ptr;
	}

	int32_t target_state = -1;

	if (eh_info.try_blocks && state >= 0)
	{
		cxx_throw_info const * throw_info = catch_info.get_throw_info();

		uint8_t const * p = image_base + eh_info.try_blocks;
		uint32_t try_block_count = read_unsigned(&p);
		for (uint32_t i = 0; i != try_block_count; ++i)
		{
			uint32_t try_low = read_unsigned(&p);
			uint32_t try_high = read_unsigned(&p);

			uint32_t _catch_high = read_unsigned(&p);
			(void)_catch_high;

			rva<uint8_t const> handlers = read_rva<uint8_t const>(&p);

			if (try_low > (uint32_t)state || (uint32_t)state > try_high)
				continue;

			if (!throw_info)
			{
				probe_for_exception_object(*dispatcher_context->pdata, *throw_frame);
				throw_info = throw_frame->catch_info.get_throw_info();
			}

			uint8_t const * q = image_base + handlers;
			uint32_t handler_count = read_unsigned(&q);

			for (uint32_t j = 0; j != handler_count; ++j)
			{
				flags<catch_block_flag> handler_flags = flags<catch_block_flag>(*q++);

				flags<cxx_catch_flag> type_flags
					= handler_flags.has_any_of(catch_block_flag::has_type_flags)
					? flags<cxx_catch_flag>(read_unsigned(&q))
					: nullflag;

				rva<type_info const> type
					= handler_flags.has_any_of(catch_block_flag::has_type_info)
					? read_rva<type_info const>(&q)
					: 0;

				uint32_t continuation_addr_count = handler_flags.get<catch_block_flag::continuation_addr_count>();

				rva<void> catch_var
					= handler_flags.has_any_of(catch_block_flag::has_catch_var)
					? rva<void>::from_displacement(read_unsigned(&q))
					: 0;

				rva<byte const> handler = read_rva<byte const>(&q);

				if (handler_flags.has_any_of(catch_block_flag::image_rva))
				{
					for (uint32_t k = 0; k != continuation_addr_count; ++k)
						catch_info.continuation_address[k] = image_base + read_rva<byte const>(&q);
				}
				else
				{
					byte const * fn = image_base + dispatcher_context->fnent->begin;
					for (uint32_t k = 0; k != continuation_addr_count; ++k)
						catch_info.continuation_address[k] = fn + read_unsigned(&q);
				}

				if (process_catch_block(image_base, type_flags, image_base + type, primary_frame_ptr + catch_var,
					throw_frame->catch_info.get_exception_object(), *throw_info))
				{
					dispatcher_context->handler = image_base + handler;
					catch_info.primary_frame_ptr = primary_frame_ptr;
					target_state = try_low - 1;
					catch_info.unwind_context.as<int32_t>() = target_state;
					break;
				}
			}

			if (dispatcher_context->handler)
				break;
		}
	}

	if (target_state >= state)
		return win32_exception_disposition::cxx_handler;

	uint8_t const * p = image_base + eh_info.unwind_graph;
	uint32_t unwind_nodes = read_unsigned(&p);
	verify(state >= 0 && (uint32_t)state < unwind_nodes);

	uint8_t const * target_edge_last = p;
	for (int32_t i = 0; i != state; ++i)
	{
		unwind_edge_t::skip(&p);
		if (target_state + 1 == i)
			target_edge_last = p;
	}

	if (target_state + 1 == state)
		target_edge_last = p;

	for (;;)
	{
		uint8_t const * orig = p;

		uint32_t target_offset_and_type = read_unsigned(&p);
		uint32_t target_offset = target_offset_and_type >> 2;
		verify(target_offset != 0);

		unwind_edge_type edge_type = static_cast<unwind_edge_type>(target_offset_and_type & 3);

		switch (edge_type)
		{
		case unwind_edge_type::trivial:
			break;
		case unwind_edge_type::object_offset:
		{
			auto destroy_fn = image_base + read_rva<void(byte *)>(&p);
			byte * obj = frame_ptr + read_unsigned(&p);
			destroy_fn(obj);
			break;
		}
		case unwind_edge_type::object_ptr_offset:
		{
			auto destroy_fn = image_base + read_rva<void(byte *)>(&p);
			byte * obj = *reinterpret_cast<byte **>(frame_ptr + read_unsigned(&p));
			destroy_fn(obj);
			break;
		}
		case unwind_edge_type::function:
		{
			auto destroy_fn = image_base + read_rva<void(void *, byte *)>(&p);
			destroy_fn(destroy_fn, frame_ptr);
			break;
		}
		}

		p = orig - target_offset;
		if (orig - target_edge_last < target_offset)
			break;
	}

	return win32_exception_disposition::cxx_handler;
}

extern "C" x64_frame_handler_t __CxxFrameHandler4;
extern "C" win32_exception_disposition __CxxFrameHandler4(
	win32_exception_record * exception_record,
	byte * frame_ptr,
	x64_cpu_context * _cpu_ctx,
	void * ctx
)
{
	(void)_cpu_ctx;
	if (exception_record)
	{
		verify(exception_record->flags.has_any_of(win32_exception_flag::unwinding));
		return win32_exception_disposition::continue_search;
	}

	return _frame_handler(frame_ptr, static_cast<cxx_dispatcher_context_t *>(ctx));
}

extern "C" x64_frame_handler_t __GSHandlerCheck_EH4;
extern "C" win32_exception_disposition __GSHandlerCheck_EH4(
	win32_exception_record * exception_record,
	byte * frame_ptr,
	x64_cpu_context * _cpu_ctx,
	void * ctx
)
{
	(void)_cpu_ctx;
	if (exception_record)
	{
		verify(exception_record->flags.has_any_of(win32_exception_flag::unwinding));
		return win32_exception_disposition::continue_search;
	}

	return _frame_handler(frame_ptr, static_cast<cxx_dispatcher_context_t *>(ctx));
}
