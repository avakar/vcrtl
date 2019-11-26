#include "../intrin.h"
#include "../memcpy.h"
#include "../algorithm.h"
#include "../stdint.h"
#include "eh_structs_x86.h"
#include "../bugcheck.h"
#include "../rva.h"
#include "../win32_seh.h"

namespace vcrtl::_msvc::x86 {

static x86_seh_registration * get_seh_head()
{
	return (x86_seh_registration *)__readfsdword(0);
}

#pragma warning(disable: 4733)

static void set_seh_head(x86_seh_registration * p)
{
	__writefsdword(0, (unsigned long)p);
}

struct fh3_frame_t
{
	uintptr_t frame_limit;
	x86_seh_registration seh_reg;
	int32_t state;
	uintptr_t next_frame_base;
};

struct cxx_throw_frame_t
{
	cxx_function_x86_eh_info const * eh_info;
	int32_t unwound_state;
	uint32_t primary_frame_base;

	void * exception_object_or_link;
	throw_info_t const * throw_info_if_owner;

	throw_info_t const * get_throw_info() const
	{
		if (throw_info_if_owner)
			return throw_info_if_owner;
		if (!exception_object_or_link)
			return nullptr;

		cxx_throw_frame_t * link = (cxx_throw_frame_t *)exception_object_or_link;
		return link->throw_info_if_owner;
	}

	void * get_exception_object()
	{
		if (throw_info_if_owner)
			return exception_object_or_link;

		cxx_throw_frame_t * link = (cxx_throw_frame_t *)exception_object_or_link;
		return link->exception_object_or_link;
	}
};

struct cxx_catch_frame_t
{
	x86_seh_registration seh_registration;
	cxx_throw_frame_t throw_frame;
};


extern "C" {

extern rva<x86_frame_handler_t> const __safe_se_handler_table[];
extern symbol __safe_se_handler_count;
extern symbol __ImageBase;

}

namespace {

struct _seh_verifier
{
	_seh_verifier()
	{
		_stack_base = __readfsdword(4);
		_stack_limit = __readfsdword(8);
	}

	void verify_seh_registration(x86_seh_registration * p) const
	{
		uint32_t addr = reinterpret_cast<uint32_t>(p);
		if (addr < _stack_limit || _stack_base <= addr)
			on_bug_check(bug_check_reason::corrupted_exception_registration_chain);

		if (!binary_search(
			__safe_se_handler_table, __safe_se_handler_table + __safe_se_handler_count,
			make_rva(p->handler, __ImageBase)))
		{
			on_bug_check(bug_check_reason::seh_handler_not_in_safeseh);
		}
	}

private:
	uint32_t _stack_base;
	uint32_t _stack_limit;
};

struct _dispatcher_context_t
{
	symbol * cookie;
	cxx_throw_frame_t * throw_frame;
	_seh_verifier const * verifier;
	cxx_eh_funclet * handler;
};

symbol _unwind_cookie;
symbol _rethrow_probe_cookie;

}

static void _probe_for_exception_object(cxx_throw_frame_t & throw_frame, _seh_verifier const & verifier)
{
	_dispatcher_context_t ctx;
	ctx.cookie = &_rethrow_probe_cookie;
	ctx.throw_frame = &throw_frame;
	ctx.verifier = &verifier;

	x86_seh_registration * current = get_seh_head();
	while (!throw_frame.exception_object_or_link)
	{
		verifier.verify_seh_registration(current);

		win32_exception_disposition disposition = current->handler(nullptr, current, nullptr, &ctx);
		if (disposition != win32_exception_disposition::cxx_handler)
			on_bug_check(bug_check_reason::unwinding_non_cxx_frame);

		current = current->next;
	}
}


extern "C" void __fastcall __cxx_destroy_throw_frame(cxx_throw_frame_t * throw_frame) noexcept
{
	if (throw_frame->throw_info_if_owner && throw_frame->throw_info_if_owner->destroy_exc_obj)
		throw_frame->throw_info_if_owner->destroy_exc_obj(throw_frame->exception_object_or_link);
}

extern "C" uintptr_t __fastcall __cxx_call_funclet(cxx_eh_funclet * funclet, uintptr_t frame_base);

static void _unwind_frame(fh3_frame_t * frame, cxx_function_x86_eh_info const * eh_info, int32_t target_state)
{
	int32_t state = frame->state;
	while (state > target_state)
	{
		cxx_unwind_graph_edge const & edge = eh_info->unwind_graph[state];
		state = edge.next;
		frame->state = state;

		if (edge.cleanup_handler)
			(void)__cxx_call_funclet(edge.cleanup_handler, (uintptr_t)&frame->next_frame_base);
	}
}

static void _fh3_handler(
	fh3_frame_t * frame, cxx_function_x86_eh_info const * eh_info,
	int32_t unwound_state, _dispatcher_context_t & ctx) noexcept
{
	if (eh_info->_magic >= 0x19930521 && eh_info->es_types)
		on_bug_check(bug_check_reason::exception_specification_not_supported);

	cxx_throw_frame_t * throw_frame = ctx.throw_frame;
	throw_info_t const * throw_info = throw_frame->get_throw_info();
	uintptr_t frame_base = (uintptr_t)&frame->next_frame_base;

	int32_t state = frame->state;

	for (int32_t i = 0; i < eh_info->try_block_count; ++i)
	{
		cxx_try_block const & try_block = eh_info->try_blocks[i];
		if (try_block.try_low <= unwound_state)
			break;

		if (try_block.try_low > state || state > try_block.try_high)
			continue;

		cxx_catch_handler const * catch_handlers = try_block.catch_handlers;
		for (int32_t j = 0; j < try_block.catch_count; ++j)
		{
			cxx_catch_handler const & catch_handler = catch_handlers[j];

			cxx_catchable_type const * matching_catchable = nullptr;
			if (!catch_handler.adjectives.has_any_of(cxx_catch_flag::is_ellipsis))
			{
				if (!throw_info)
				{
					_probe_for_exception_object(*throw_frame, *ctx.verifier);
					throw_info = throw_frame->get_throw_info();
				}

				cxx_catchable_type_list const * catchables = throw_info->catchables;
				for (uint32_t k = 0; k < catchables->count; ++k)
				{
					cxx_catchable_type const * catchable = 0 + catchables->types[k];
					if (catch_handler.type_desc == catchable->desc)
					{
						matching_catchable = catchable;
						break;
					}
				}

				if (!matching_catchable)
					continue;
			}

			ctx.handler = catch_handler.handler;
			throw_frame->primary_frame_base = frame_base;
			throw_frame->eh_info = eh_info;
			throw_frame->unwound_state = try_block.try_low - 1;

			_unwind_frame(frame, eh_info, try_block.try_low);
			frame->state = try_block.try_high + 1;

			if (catch_handler.adjectives.has_any_of(cxx_catch_flag::is_reference))
			{
				void *& catch_object = *(void **)(frame_base + catch_handler.catch_object_offset);
				catch_object = throw_frame->get_exception_object();
			}
			else if (matching_catchable && catch_handler.catch_object_offset)
			{
				void * exception_object = throw_frame->get_exception_object();
				uintptr_t catch_var = throw_frame->primary_frame_base + catch_handler.catch_object_offset;

				if (matching_catchable->properties.has_any_of(cxx_catchable_property::is_simple_type))
				{
					memcpy((void *)catch_var, (void *)exception_object, matching_catchable->size);
					if (matching_catchable->size == sizeof(uintptr_t))
					{
						uintptr_t & ptr = *(uintptr_t *)catch_var;
						if (ptr)
							ptr = matching_catchable->offset.apply(ptr);
					}
				}
				else if (matching_catchable->copy_fn_rva == 0)
				{
					memcpy((void *)catch_var, (void *)matching_catchable->offset.apply((uintptr_t)exception_object),
						matching_catchable->size);
				}
				else if (matching_catchable->properties.has_any_of(cxx_catchable_property::has_virtual_base))
				{
					using copy_fn_vb_t = void __fastcall(uintptr_t self, uintptr_t edx, void * other, int is_most_derived);
					copy_fn_vb_t * fn = (copy_fn_vb_t *)matching_catchable->copy_fn_rva;
					fn(catch_var, reinterpret_cast<uintptr_t>(fn), exception_object, 1);
				}
				else
				{
					using copy_fn_t = void __fastcall (uintptr_t self, uintptr_t edx, void * other);
					copy_fn_t * fn = (copy_fn_t *)matching_catchable->copy_fn_rva;
					fn(catch_var, reinterpret_cast<uintptr_t>(fn), exception_object);
				}
			}

			return;
		}
	}

	_unwind_frame(frame, eh_info, unwound_state);

	if (state == -1
		&& eh_info->_magic >= 0x19930522
		&& eh_info->eh_flags.has_any_of(cxx_eh_flag::is_noexcept)
		&& !ctx.handler)
	{
		on_bug_check(bug_check_reason::noexcept_violation);
	}
}

using cxx_frame_handler_t = win32_exception_disposition __cdecl(
	win32_exception_record * exception_record, x86_seh_registration * registration_record,
	cxx_function_x86_eh_info const * eh_info, _dispatcher_context_t & ctx);

extern "C" cxx_frame_handler_t __fh3_primary_handler;
extern "C" win32_exception_disposition __cdecl __fh3_primary_handler(
	win32_exception_record * exception_record, x86_seh_registration * seh_reg,
	cxx_function_x86_eh_info const * eh_info, _dispatcher_context_t & ctx)
{
	if (exception_record)
	{
		if (exception_record->flags.has_any_of(win32_exception_flag::unwinding))
			on_bug_check(bug_check_reason::unwind_on_unsafe_exception);
		return win32_exception_disposition::continue_search;
	}

	if (eh_info->_magic < 0x19930520 || eh_info->_magic >= 0x19940000)
		on_bug_check(bug_check_reason::corrupted_eh_unwind_data);

	if (ctx.cookie == &_unwind_cookie)
	{
		fh3_frame_t * frame = container_of(seh_reg, fh3_frame_t, seh_reg);
		_fh3_handler(frame, eh_info, -1, ctx);
	}
	else if (ctx.cookie != &_rethrow_probe_cookie)
	{
		return win32_exception_disposition::continue_search;
	}

	return win32_exception_disposition::cxx_handler;
}

extern "C" x86_frame_handler_t __cxx_catch_frame_handler;
extern "C" win32_exception_disposition __cdecl __cxx_catch_frame_handler(
	win32_exception_record * exception_record, x86_seh_registration * seh_reg,
	x86_cpu_context * /*cpu_context*/, void * dispatcher_context)
{
	if (exception_record)
	{
		if (exception_record->flags.has_any_of(win32_exception_flag::unwinding))
			on_bug_check(bug_check_reason::unwind_on_unsafe_exception);
		return win32_exception_disposition::continue_search;
	}

	cxx_catch_frame_t * frame = container_of(seh_reg, cxx_catch_frame_t, seh_registration);
	_dispatcher_context_t * ctx = static_cast<_dispatcher_context_t *>(dispatcher_context);
	cxx_throw_frame_t * throw_frame = ctx->throw_frame;

	if (ctx->cookie == &_rethrow_probe_cookie)
	{
		throw_frame->exception_object_or_link = &frame->throw_frame;
		return win32_exception_disposition::cxx_handler;
	}

	if (ctx->cookie != &_unwind_cookie)
		return win32_exception_disposition::continue_search;

	if (throw_frame->exception_object_or_link == &frame->throw_frame)
	{
		throw_frame->exception_object_or_link = frame->throw_frame.exception_object_or_link;
		throw_frame->throw_info_if_owner = frame->throw_frame.throw_info_if_owner;
	}
	else if (!throw_frame->exception_object_or_link)
	{
		throw_frame->exception_object_or_link = &frame->throw_frame;
	}

	fh3_frame_t * primary_frame = container_of(frame->throw_frame.primary_frame_base, fh3_frame_t, next_frame_base);
	_fh3_handler(primary_frame, frame->throw_frame.eh_info, frame->throw_frame.unwound_state, *ctx);

	if (!ctx->handler)
	{
		if (throw_frame->exception_object_or_link == &frame->throw_frame)
		{
			throw_frame->exception_object_or_link = frame->throw_frame.exception_object_or_link;
			throw_frame->throw_info_if_owner = frame->throw_frame.throw_info_if_owner;
		}
		else if (frame->throw_frame.throw_info_if_owner && frame->throw_frame.throw_info_if_owner->destroy_exc_obj)
		{
			frame->throw_frame.throw_info_if_owner->destroy_exc_obj(frame->throw_frame.exception_object_or_link);
		}
	}

	return win32_exception_disposition::cxx_handler;
}


extern "C" cxx_eh_funclet * __fastcall __cxx_dispatch_exception(cxx_throw_frame_t * throw_frame)
{
	_seh_verifier verifier;

	_dispatcher_context_t ctx;
	ctx.cookie = &_unwind_cookie;
	ctx.throw_frame = throw_frame;
	ctx.verifier = &verifier;
	ctx.handler = nullptr;

	x86_seh_registration * seh_head = get_seh_head();
	for (;;)
	{
		verifier.verify_seh_registration(seh_head);

		win32_exception_disposition exception_disposition = seh_head->handler(nullptr, seh_head, nullptr, &ctx);
		if (exception_disposition != win32_exception_disposition::cxx_handler)
			on_bug_check(bug_check_reason::unwinding_non_cxx_frame);

		if (ctx.handler)
			return ctx.handler;

		seh_head = seh_head->next;
		set_seh_head(seh_head);
	}
}

}
