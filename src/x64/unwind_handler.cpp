#include "cpu_context.h"
#include "../../include/vcrtl/bugcheck.h"
#include "../assert.h"
#include "../stdint.h"
using namespace vcrtl;
using namespace vcrtl::_msvc::x64;

extern "C" symbol __ImageBase;

namespace {

template <typename T>
struct _pe_dir_hdr
{
	/* 0x00 */ rva<T> rva;
	/* 0x04 */ uint32_t size;
	/* 0x08 */
};


struct _pe64_header
{
	/* 0x00 */ uint32_t pe_magic;

	/* 0x04 */ uint16_t machine;
	/* 0x06 */ uint8_t _06[0x12];

	/* 0x18 */ uint16_t opt_magic;
	/* 0x1a */ uint8_t _20[0x36];
	/* 0x50 */ uint32_t image_size;
	/* 0x54 */ uint32_t headers_size;
	/* 0x58 */ uint8_t _58[0x2c];
	/* 0x84 */ uint32_t directory_count;

	/* 0x88 */ _pe_dir_hdr<void> export_table;
	/* 0x90 */ _pe_dir_hdr<void> import_table;
	/* 0x98 */ _pe_dir_hdr<void> resource_table;
	/* 0xa0 */ _pe_dir_hdr<pe_function> exception_table;
};

struct _dos_exe_header
{
	/* 0x00 */ uint16_t magic;
	/* 0x02 */ uint8_t _02[0x3a];
	/* 0x3c */ rva<_pe64_header> pe_header;
	/* 0x40 */
};

}

//frame_info_t unwind_one();


frame_walk_pdata_t frame_walk_pdata_t::for_this_image()
{
	return frame_walk_pdata_t(__ImageBase);
}

frame_walk_pdata_t frame_walk_pdata_t::from_image_base(byte const * image_base)
{
	return frame_walk_pdata_t(image_base);
}

byte const * frame_walk_pdata_t::image_base() const
{
	return _image_base;
}

bool frame_walk_pdata_t::contains_address(byte const * addr) const
{
	return _image_base <= addr && addr - _image_base < _image_size;
}

pe_function const * frame_walk_pdata_t::find_function_entry(byte const * addr) const
{
	verify(this->contains_address(addr));
	rva pc_rva = make_rva(addr, _image_base);

	uint32_t l = 0;
	uint32_t r = _function_count;
	while (l < r)
	{
		uint32_t i = l + (r - l) / 2;
		pe_function const & fn = _functions[i];

		if (pc_rva < fn.begin)
			r = i;
		else if (fn.end <= pc_rva)
			l = i + 1;
		else
			return &fn;
	}

	return nullptr;
}

frame_walk_pdata_t::frame_walk_pdata_t(byte const * image_base)
	: _image_base(image_base)
{
	_dos_exe_header const * dos_hdr = (_dos_exe_header const *)image_base;
	verify(dos_hdr->magic == 0x5a4d);

	_pe64_header const * pe_hdr = image_base + dos_hdr->pe_header;
	verify(pe_hdr->pe_magic == 0x4550);
	verify(pe_hdr->machine == 0x8664);
	verify(pe_hdr->opt_magic == 0x20b);
	verify(pe_hdr->headers_size >= dos_hdr->pe_header.value() + sizeof(_pe64_header));
	verify(pe_hdr->image_size >= pe_hdr->headers_size);

	verify(pe_hdr->directory_count >= 4);
	verify((pe_hdr->exception_table.size % sizeof(pe_function)) == 0);

	_functions = image_base + pe_hdr->exception_table.rva;
	_function_count = pe_hdr->exception_table.size / sizeof(pe_function);
	_image_size = pe_hdr->image_size;
}

uint64_t & frame_walk_context_t::gp(uint8_t idx)
{
	int8_t conv[16] = {
		-1, -1, -1,  0, -1,  1,  2,  3,
		-1, -1, -1, -1,  4,  5,  6,  7,
	};

	int8_t offs = conv[idx];
	if (offs < 0)
		on_bug_check(bug_check_reason::corrupted_eh_unwind_data);

	return (&this->rbx)[offs];
};

static xmm_t & _xmm(frame_walk_context_t & ctx, uint8_t idx)
{
	if (idx < 6 || idx >= 16)
		on_bug_check(bug_check_reason::corrupted_eh_unwind_data);

	return (&ctx.xmm6)[idx - 6];
}

void frame_walk_pdata_t::unwind(pe_unwind_info const & unwind_info, frame_walk_context_t & ctx, mach_frame_t & mach) const
{
	bool rip_updated = false;
	for (uint32_t i = 0; i != unwind_info.unwind_code_count; ++i)
	{
		auto entry = unwind_info.entries[i];
		switch (entry.code)
		{
		case unwind_code_t::push_nonvolatile_reg:
			ctx.gp(entry.info) = *(uint64_t const *)mach.rsp;
			mach.rsp += 8;
			break;

		case unwind_code_t::alloc_large:
			if (entry.info == 0)
			{
				mach.rsp += unwind_info.data[++i] * 8;
			}
			else
			{
				mach.rsp += (uint32_t const &)unwind_info.data[i+1];
				i += 2;
			}
			break;

		case unwind_code_t::alloc_small:
			mach.rsp += entry.info * 8 + 8;
			break;

		case unwind_code_t::set_frame_pointer:
			mach.rsp = ctx.gp(unwind_info.frame_reg) - unwind_info.frame_reg_disp * 16;
			break;

		case unwind_code_t::save_nonvolatile_reg:
			ctx.gp(entry.info) = *(uint64_t const *)(mach.rsp + unwind_info.data[++i] * 8);
			break;

		case unwind_code_t::save_nonvolatile_reg_far:
			ctx.gp(entry.info) = *(uint64_t const *)(mach.rsp + *(uint32_t const *)&unwind_info.data[i]);
			i += 2;
			break;

		case unwind_code_t::epilog:
			i += 1;
			break;

		case unwind_code_t::_07:
			i += 2;
			break;

		case unwind_code_t::save_xmm128:
			_xmm(ctx, entry.info) = *(xmm_t const *)(mach.rsp + unwind_info.data[++i] * 16);
			break;

		case unwind_code_t::save_xmm128_far:
			_xmm(ctx, entry.info) = *(xmm_t const *)(mach.rsp + (uint32_t const &)unwind_info.data[i+1]);
			i += 2;
			break;

		case unwind_code_t::push_machine_frame:
			mach.rsp += (uint64_t)entry.info * 8;
			mach.rip = *(byte const **)mach.rsp;
			mach.rsp = *(uint64_t const *)(mach.rsp + 24);
			rip_updated = true;
			break;
		}
	}

	if (!rip_updated)
	{
		mach.rip = *(byte const **)mach.rsp;
		mach.rsp += 8;
	}
}
