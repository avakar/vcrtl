#include "throw.h"
#include "../win32_seh.h"
#include "../assert.h"
#include "../memcpy.h"
using namespace vcrtl;
using namespace vcrtl::_msvc;
using namespace vcrtl::_msvc::x64;

extern "C" void __cxx_call_catch_handler();
extern "C" symbol __ImageBase;

namespace {

struct _catch_frame_t
{
	uint64_t red_zone[4];
	mach_frame_t mach;
	cxx_catch_info_t catch_info;
};

}

namespace vcrtl::_msvc::x64 {

symbol unwind_cookie;
symbol rethrow_probe_cookie;

bool process_catch_block(byte const * image_base, flags<cxx_catch_flag> adjectives,
	type_info const * match_type, void * catch_var, void * exception_object, cxx_throw_info const & throw_info)
{
	if (adjectives.has_any_of(cxx_catch_flag::is_ellipsis))
		return true;

	if (throw_info.attributes.has_any_of(cxx_throw_flag::is_const)
		&& !adjectives.has_any_of(cxx_catch_flag::is_const))
	{
		return false;
	}

	if (throw_info.attributes.has_any_of(cxx_throw_flag::is_unaligned)
		&& !adjectives.has_any_of(cxx_catch_flag::is_unaligned))
	{
		return false;
	}

	if (throw_info.attributes.has_any_of(cxx_throw_flag::is_volatile)
		&& !adjectives.has_any_of(cxx_catch_flag::is_volatile))
	{
		return false;
	}

	cxx_catchable_type_list const * catchable_list = image_base + throw_info.catchables;
	rva<cxx_catchable_type const> const * catchables = catchable_list->types;

	for (uint32_t i = 0; i != catchable_list->count; ++i)
	{
		cxx_catchable_type const * catchable = image_base + catchables[i];
		type_info const * catchable_type_desc = image_base + catchable->desc;
		if (catchable_type_desc == match_type)
		{
			if (!catchable->properties.has_any_of(cxx_catchable_property::by_reference_only)
				|| adjectives.has_any_of(cxx_catch_flag::is_reference))
			{
				if (adjectives.has_any_of(cxx_catch_flag::is_reference))
				{
					*(void **)catch_var = exception_object;
				}
				else
				{
					if (catchable->properties.has_any_of(cxx_catchable_property::is_simple_type))
					{
						if (catchable->size == sizeof(uintptr_t))
						{
							memcpy((void *)catch_var, (void *)exception_object, sizeof(uintptr_t));
							uintptr_t & ptr = *(uintptr_t *)catch_var;
							if (ptr)
								ptr = catchable->offset.apply(ptr);
						}
						else
						{
							memcpy((void *)catch_var, (void *)exception_object, catchable->size);
						}
					}
					else if (!catchable->copy_fn)
					{
						memcpy((void *)catch_var, (void *)catchable->offset.apply((uintptr_t)exception_object), catchable->size);
					}
					else if (catchable->properties.has_any_of(cxx_catchable_property::has_virtual_base))
					{
						using copy_fn_vb_t = void(void * self, void * other, int is_most_derived);
						copy_fn_vb_t * fn = (copy_fn_vb_t *)(image_base + catchable->copy_fn);
						fn(catch_var, exception_object, 1);
					}
					else
					{
						using copy_fn_t = void(void * self, void * other);
						copy_fn_t * fn = (copy_fn_t *)(image_base + catchable->copy_fn);
						fn(catch_var, exception_object);
					}
				}

				return true;
			}
		}
	}

	return false;
}

extern "C" void __cxx_destroy_exception(cxx_catch_info_t & ci) noexcept
{
	if (ci.throw_info_if_owner && ci.throw_info_if_owner->destroy_exc_obj)
		(__ImageBase + ci.throw_info_if_owner->destroy_exc_obj)(ci.exception_object_or_link);
}

static pe_unwind_info const * _execute_handler(
	cxx_dispatcher_context_t & ctx,
	frame_walk_context_t & cpu_ctx,
	mach_frame_t & mach)
{
	frame_walk_pdata_t const & pdata = *ctx.pdata;
	byte const * image_base = pdata.image_base();

	ctx.fnent = pdata.find_function_entry(mach.rip);
	pe_unwind_info const * unwind_info = image_base + ctx.fnent->unwind_info;

	if (unwind_info->flags & 3)
	{
		size_t unwind_slots = ((size_t)unwind_info->unwind_code_count + 1) & ~(size_t)1;

		auto frame_handler = image_base
			+ *(rva<x64_frame_handler_t> const *)&unwind_info->data[unwind_slots];
		verify(frame_handler);

		ctx.extra_data = &unwind_info->data[unwind_slots + 2];

		byte * frame_ptr = reinterpret_cast<byte *>(
			unwind_info->frame_reg? cpu_ctx.gp(unwind_info->frame_reg): mach.rsp);
		verify(
			frame_handler(nullptr, frame_ptr, nullptr, &ctx)
			== win32_exception_disposition::cxx_handler);
	}

	return unwind_info;
}

extern "C" byte const * __cxx_dispatch_exception(void * exception_object, cxx_throw_info const * throw_info,
	cxx_throw_frame_t & tf) noexcept
{
	frame_walk_pdata_t pdata = frame_walk_pdata_t::for_this_image();

	cxx_dispatcher_context_t ctx;
	ctx.cookie = &unwind_cookie;
	ctx.throw_frame = &tf;
	ctx.pdata = &pdata;
	ctx.handler = nullptr;

	auto & cpu_ctx = tf.ctx;
	auto & mach = tf.mach;
	auto & ci = tf.catch_info;

	ci.exception_object_or_link = exception_object;
	ci.throw_info_if_owner = throw_info;
	ci.primary_frame_ptr = 0;

	for (;;)
	{
		pe_unwind_info const * unwind_info = _execute_handler(ctx, cpu_ctx, mach);
		if (ctx.handler)
			return ctx.handler;
		pdata.unwind(*unwind_info, cpu_ctx, mach);
	}
}

void probe_for_exception_object(
	frame_walk_pdata_t const & pdata,
	cxx_throw_frame_t & frame)
{
	cxx_dispatcher_context_t ctx;
	ctx.cookie = &rethrow_probe_cookie;
	ctx.throw_frame = &frame;
	ctx.pdata = &pdata;
	ctx.extra_data = nullptr;

	frame_walk_context_t probe_ctx = frame.ctx;
	mach_frame_t probe_mach = frame.mach;
	cxx_catch_info_t & catch_info = frame.catch_info;

	for (;;)
	{
		pe_unwind_info const * unwind_info = _execute_handler(ctx, probe_ctx, probe_mach);
		if (catch_info.exception_object_or_link)
			break;
		pdata.unwind(*unwind_info, probe_ctx, probe_mach);
	}
}

}

extern "C" x64_frame_handler_t __cxx_seh_frame_handler;
extern "C" win32_exception_disposition __cxx_seh_frame_handler(
	win32_exception_record * exception_record,
	byte * _frame_ptr,
	x64_cpu_context * _cpu_ctx,
	void * _ctx
)
{
	(void)_frame_ptr;
	(void)_cpu_ctx;
	(void)_ctx;
	if (exception_record)
		verify(exception_record->flags.has_any_of(win32_exception_flag::unwinding));

	return win32_exception_disposition::continue_search;
}

extern "C" x64_frame_handler_t __cxx_call_catch_frame_handler;
extern "C" win32_exception_disposition __cxx_call_catch_frame_handler(
	win32_exception_record * exception_record,
	byte * frame_ptr,
	x64_cpu_context * _cpu_ctx,
	void * dispatcher_context
)
{
	(void)_cpu_ctx;
	if (exception_record)
	{
		verify(exception_record->flags.has_any_of(win32_exception_flag::unwinding));
		return win32_exception_disposition::continue_search;
	}

	cxx_dispatcher_context_t * ctx = static_cast<cxx_dispatcher_context_t *>(dispatcher_context);
	cxx_throw_frame_t * throw_frame = ctx->throw_frame;
	_catch_frame_t * frame = (_catch_frame_t *)frame_ptr;
	cxx_catch_info_t & ci = throw_frame->catch_info;

	if (ctx->cookie == &rethrow_probe_cookie)
	{
		verify(frame->catch_info.exception_object_or_link);

		if (frame->catch_info.throw_info_if_owner)
			ci.exception_object_or_link = &frame->catch_info;
		else
			ci.exception_object_or_link = frame->catch_info.exception_object_or_link;
	}
	else if (ctx->cookie == &unwind_cookie)
	{
		if (!ci.exception_object_or_link || ci.exception_object_or_link == &frame->catch_info)
		{
			ci.exception_object_or_link = frame->catch_info.exception_object_or_link;
			ci.throw_info_if_owner = frame->catch_info.throw_info_if_owner;
		}
		else
		{
			__cxx_destroy_exception(frame->catch_info);
		}

		ci.primary_frame_ptr = frame->catch_info.primary_frame_ptr;
		ci.unwind_context = frame->catch_info.unwind_context;
	}
	else
	{
		return win32_exception_disposition::continue_search;
	}

	return win32_exception_disposition::cxx_handler;
}
