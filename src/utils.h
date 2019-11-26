#include "stdint.h"
#include "type_traits.h"

namespace vcrtl {

template <typename T>
struct bytes
{
	template <typename U>
	static constexpr bool can_store_v
		= is_trivially_destructible_v<U>
		&& is_trivially_constructible_v<U>
		&& sizeof(U) <= sizeof(T)
		&& alignof(U) <= alignof(T);

	void * get()
	{
		return &_storage;
	}

	template <typename U>
	enable_if_t<can_store_v<U>, U &> as()
	{
		return reinterpret_cast<U &>(_storage);
	}

	template <typename U>
	enable_if_t<can_store_v<U>, U const &> as() const
	{
		return reinterpret_cast<U const &>(_storage);
	}

private:
	T _storage;
};

}

#pragma once
