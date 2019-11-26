#include "throw.h"
#include "../assert.h"
using namespace vcrtl;
using namespace vcrtl::_msvc;
using namespace vcrtl::_msvc::x64;

struct fh3_fn_ctx_t
{
	int32_t state;
	int32_t home_block_index;
};

static win32_exception_disposition _frame_handler(
	win32_exception_record * _exception_record,
	byte * frame_ptr,
	x64_cpu_context * _cpu_ctx,
	cxx_dispatcher_context_t * dispatcher_context
)
{
	(void)_exception_record;
	(void)_cpu_ctx;

	if (dispatcher_context->cookie == &rethrow_probe_cookie)
		return win32_exception_disposition::cxx_handler;
	if (dispatcher_context->cookie != &unwind_cookie)
		return win32_exception_disposition::continue_search;

	cxx_throw_frame_t * throw_frame = dispatcher_context->throw_frame;

	cxx_catch_info_t & catch_info = throw_frame->catch_info;
	fh3_fn_ctx_t & ctx = catch_info.unwind_context.as<fh3_fn_ctx_t>();

	byte const * image_base = dispatcher_context->pdata->image_base();

	cxx_function_eh_info const * eh_info = image_base + *(rva<cxx_function_eh_info const> *)dispatcher_context->extra_data;
	cxx_try_block const * try_blocks = image_base + eh_info->try_blocks;

	byte * primary_frame_ptr = catch_info.primary_frame_ptr;
	int32_t state;
	int32_t home_block_index;

	if (primary_frame_ptr < frame_ptr)
	{
		rva pc_rva = make_rva(reinterpret_cast<byte const *>(throw_frame->mach.rip), image_base);

		auto regions = image_base + eh_info->regions;
		state = regions[eh_info->region_count - 1].state;
		for (uint32_t i = 1; i != eh_info->region_count; ++i)
		{
			if (pc_rva < regions[i].first_ip)
			{
				state = regions[i - 1].state;
				break;
			}
		}

		home_block_index = eh_info->try_block_count - 1;
	}
	else
	{
		state = ctx.state;
		home_block_index = ctx.home_block_index;
	}

	int32_t funclet_low_state = -1;
	if (primary_frame_ptr == frame_ptr)
	{
		home_block_index = -1;
	}
	else
	{
		// Identify the funclet we're currently in. If we're in a catch
		// funclet, locate the primary frame ptr and cache everything.
		for (; home_block_index > -1; --home_block_index)
		{
			cxx_try_block const & try_block = try_blocks[home_block_index];
			if (try_block.try_high < state && state <= try_block.catch_high)
			{
				if (primary_frame_ptr < frame_ptr)
				{
					cxx_catch_handler const * catch_handlers = image_base + try_block.catch_handlers;
					for (int32_t j = 0; j < try_block.catch_count; ++j)
					{
						cxx_catch_handler const & catch_handler = catch_handlers[j];
						if (catch_handler.handler == dispatcher_context->fnent->begin)
						{
							cxx_eh_node_catch const * node = frame_ptr + catch_handler.node_offset;
							primary_frame_ptr = node->primary_frame_ptr;
							verify(primary_frame_ptr >= frame_ptr);
							break;
						}
					}
				}

				funclet_low_state = try_block.try_low;
				break;
			}
		}

		if (primary_frame_ptr < frame_ptr)
			primary_frame_ptr = frame_ptr;
	}

	int32_t target_state = funclet_low_state;

	cxx_throw_info const * throw_info = catch_info.get_throw_info();

	cxx_catch_handler const * target_catch_handler = nullptr;
	for (int32_t i = home_block_index + 1; !target_catch_handler && i < eh_info->try_block_count; ++i)
	{
		auto const & try_block = try_blocks[i];

		if (try_block.try_low <= state && state <= try_block.try_high)
		{
			if (try_block.try_low < funclet_low_state)
				continue;

			if (!throw_info)
			{
				probe_for_exception_object(*dispatcher_context->pdata, *throw_frame);
				throw_info = catch_info.get_throw_info();
			}

			cxx_catch_handler const * handlers = image_base + try_block.catch_handlers;
			for (int32_t j = 0; j < try_block.catch_count; ++j)
			{
				cxx_catch_handler const & catch_block = handlers[j];

				if (!process_catch_block(image_base, catch_block.adjectives, image_base + catch_block.type_desc,
					primary_frame_ptr + catch_block.catch_object_offset, catch_info.get_exception_object(),
					*throw_info))
				{
					continue;
				}

				target_state = try_block.try_low - 1;
				target_catch_handler = &catch_block;
				dispatcher_context->handler = image_base + catch_block.handler;

				catch_info.primary_frame_ptr = primary_frame_ptr;
				ctx.home_block_index = home_block_index;
				ctx.state = try_block.try_low - 1;
				break;
			}
		}
	}

	if (home_block_index == -1 && !target_catch_handler
		&& eh_info->eh_flags.has_any_of(cxx_eh_flag::is_noexcept))
	{
		// XXX call std::terminate
		verify(false);
	}

	verify(target_state >= funclet_low_state);

	cxx_unwind_graph_edge const * unwind_graph = image_base + eh_info->unwind_graph;
	while (state > target_state)
	{
		cxx_unwind_graph_edge const & edge = unwind_graph[state];
		state = edge.next;
		if (edge.cleanup_handler)
		{
			byte const * funclet = image_base + edge.cleanup_handler;
			using fn_type = uintptr_t(byte const * funclet, byte const * frame_ptr);
			(void)((fn_type *)funclet)(funclet, frame_ptr);
		}
	}

	return win32_exception_disposition::cxx_handler;
}

extern "C" x64_frame_handler_t __CxxFrameHandler3;
extern "C" win32_exception_disposition __CxxFrameHandler3(
	win32_exception_record * exception_record,
	byte * frame_ptr,
	x64_cpu_context * cpu_ctx,
	void * ctx
)
{
	if (exception_record)
	{
		verify(!exception_record->flags.has_any_of(win32_exception_flag::unwinding));
		return win32_exception_disposition::continue_search;
	}

	return _frame_handler(exception_record, frame_ptr, cpu_ctx, static_cast<cxx_dispatcher_context_t *>(ctx));
}

extern "C" x64_frame_handler_t __GSHandlerCheck_EH;
extern "C" win32_exception_disposition __GSHandlerCheck_EH(
	win32_exception_record * exception_record,
	byte * frame_ptr,
	x64_cpu_context * cpu_ctx,
	void * ctx
)
{
	if (exception_record)
	{
		verify(!exception_record->flags.has_any_of(win32_exception_flag::unwinding));
		return win32_exception_disposition::continue_search;
	}

	return _frame_handler(exception_record, frame_ptr, cpu_ctx, static_cast<cxx_dispatcher_context_t *>(ctx));
}
