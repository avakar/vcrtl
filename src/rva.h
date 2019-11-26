#include "assert.h"
#include "limits.h"
#include "stdint.h"
#include "type_traits.h"

namespace vcrtl {

template <typename T, typename U>
constexpr T narrow(U value) noexcept
{
	verify(numeric_limits<T>::min() <= value);
	verify(value <= numeric_limits<T>::max());
	return static_cast<T>(value);
}

template <typename T>
struct rva
{
	rva(nullptr_t = nullptr)
		: _offset(0)
	{
	}

	static rva<T> from_displacement(uint32_t disp)
	{
		return rva<T>(disp);
	}

	static rva<T> make(T * ptr, void const * base)
	{
		return rva<T>(narrow<uint32_t>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(base)));
	}

	uint32_t value() const
	{
		return _offset;
	}

	explicit operator bool() const
	{
		return _offset != 0;
	}

	template <
		typename U,
		enable_if_t<is_convertible_v<T *, U *>, int> = 0
		>
	operator rva<U>() const
	{
		return rva<U>::from_displacement(_offset);
	}

	friend T * operator+(void const * base, rva<T> rva)
	{
		return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(base) + rva._offset);
	}

	friend byte const * operator-(T * p, rva<T> r)
	{
		return reinterpret_cast<byte const *>(reinterpret_cast<uintptr_t>(p) - r._offset);
	}

	friend rva operator+(rva lhs, uint32_t rhs)
	{
		return rva(lhs._offset + rhs);
	}

	rva & operator+=(uint32_t rhs)
	{
		_offset += rhs;
		return *this;
	}

	friend bool operator==(rva const & lhs, rva const & rhs)
	{
		return lhs._offset == rhs._offset;
	}

	friend bool operator!=(rva const & lhs, rva const & rhs)
	{
		return !(lhs == rhs);
	}

	friend bool operator<(rva const & lhs, rva const & rhs)
	{
		return lhs._offset < rhs._offset;
	}

	friend bool operator<=(rva const & lhs, rva const & rhs)
	{
		return !(rhs < lhs);
	}

	friend bool operator>(rva const & lhs, rva const & rhs)
	{
		return rhs < lhs;
	}

	friend bool operator>=(rva const & lhs, rva const & rhs)
	{
		return !(lhs < rhs);
	}

private:
	explicit rva(uint32_t offset)
		: _offset(offset)
	{
	}

	uint32_t _offset;
};

template <typename T>
rva<T> make_rva(T * ptr, void const * base)
{
	return rva<T>::make(ptr, base);
}

struct symbol
{
	operator byte const *() const
	{
		return reinterpret_cast<byte const *>(this);
	}

	operator uintptr_t() const
	{
		return reinterpret_cast<uintptr_t>(this);
	}
};

}

#pragma once
