// The type of the `typeid` expression is actually
// `const ::type_info` rather than `const std::type_info`
// likely for historical reasons.
//
// For non-polymorphic types, `typeid` simply returns
// a reference to a static `type_info` object for that type.
// For polymorphic types, the function `__RTtypeid`
// is called with the pointer to the object. The functiuon
// returns pointer to the `type_info` for the most-derived
// object.
//
// The objects the compiler produces have the following
// layout.
//
//     void * vtable = &`??_7type_info@@6B@`;
//     void * undecorated_name = nullptr;
//     char decorated_name[] = ...;
//
// Sadly, the reference to type_info's vtable means that we must
// make the struct polymorphic, even though we don't actually
// need it to be. Without the virtual function, the linker
// will complain about a missing symbol.
// We should, however, consider defining the vtable symbol
// to absolute zero to remove the virtual functions.
// We'll still be left with a useless field unfortunately.
//
// The `undecorated_name` field is initialized to zero
// and is used by CRT to cache the string it returns from
// `type_info::name()`. This is the reason the `type_info`
// objects are put into a *read-write* section `.data$r`.
//
// The `decorated_name` is the string CRT returns
// from `type_info::raw_name()`. We'll be returning it from
// `type_info::name()`, although as far as I'm concerned,
// the string is a waste of space too.
// All we really care about is a unique address.

class type_info final
{
	virtual ~type_info();

	void * _undecorated_name;
	char const decorated_name[1];
};

namespace vcrtl {

using type_info = ::type_info;

}

#pragma once
