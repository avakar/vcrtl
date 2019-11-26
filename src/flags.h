#include "type_traits.h"

namespace vcrtl {

struct nullflag_t
{
};

constexpr nullflag_t nullflag = {};

template <typename E>
struct flags
{
	using type = underlying_type_t<E>;

	flags() noexcept = default;

	flags(nullflag_t) noexcept
		: _value(0)
	{
	}

	flags(E o) noexcept
		: _value(static_cast<type>(o))
	{
	}

	explicit flags(type o) noexcept
		: _value(o)
	{
	}

	operator type() const
	{
		return _value;
	}

	explicit operator bool() const
	{
		return _value != 0;
	}

	bool has_any_of(flags f) const
	{
		return (_value & f._value) != 0;
	}

	template <E e>
	type get() const
	{
		static constexpr type mask = static_cast<type>(e);
		static constexpr size_t leading_zeros = _ctz(mask);

		if constexpr (((mask - 1) & mask) != 0)
		{
			return (_value & mask) >> leading_zeros;
		}
		else
		{
			return (_value & mask) != 0;
		}
	}

	friend flags operator|(E l, E r)
	{
		flags f;
		f._value = static_cast<type>(l) | static_cast<type>(r);
		return f;
	}

	friend flags operator&(flags l, E r)
	{
		flags f;
		f._value = l._value & static_cast<type>(r);
		return f;
	}

private:
	static constexpr size_t _ctz(type val)
	{
		size_t r = 0;
		while ((val & 1) == 0)
		{
			++r;
			val >>= 1;
		}

		return r;
	}

	type _value;
};

}

#pragma once
