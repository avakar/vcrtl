namespace vcrtl {

template <bool _cond, typename T = void>
struct enable_if
{
};

template <typename T>
struct enable_if<true, T>
{
	using type = T;
};

template <bool _cond, typename T = void>
using enable_if_t = typename enable_if<_cond, T>::type;

template <typename T, T _value>
struct integral_constant
{
	using value_type = T;
	using type = integral_constant<T, _value>;
	static constexpr T value = _value;

	constexpr operator value_type() const noexcept
	{
		return _value;
	}

	constexpr value_type operator()() const noexcept
	{
		return _value;
	}
};

template <bool _value>
using bool_constant = integral_constant<bool, _value>;

using false_type = bool_constant<false>;
using true_type = bool_constant<true>;

template <typename T>
struct is_integral
	: false_type
{
};

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

template <> struct is_integral<bool> : true_type {};
template <> struct is_integral<char> : true_type {};
template <> struct is_integral<signed char> : true_type {};
template <> struct is_integral<unsigned char> : true_type {};
template <> struct is_integral<char16_t> : true_type {};
template <> struct is_integral<char32_t> : true_type {};
template <> struct is_integral<short> : true_type {};
template <> struct is_integral<unsigned short> : true_type {};
template <> struct is_integral<int> : true_type {};
template <> struct is_integral<unsigned int> : true_type {};
template <> struct is_integral<long> : true_type {};
template <> struct is_integral<unsigned long> : true_type {};
template <> struct is_integral<long long> : true_type {};
template <> struct is_integral<unsigned long long> : true_type {};

#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)
template <> struct is_integral<wchar_t> : true_type {};
#endif

#ifdef __cpp_char8_t
template <> struct is_integral<char8_t>: true_type {};
#endif

template <typename T>
struct remove_extent
{
	using type = T;
};

template <typename T>
struct remove_extent<T[]>
{
	using type = T;
};

template <typename T>
using remove_extent_t = typename remove_extent<T>::type;

template <typename T>
struct remove_reference
{
	using type = T;
};

template <typename T>
struct remove_reference<T &>
{
	using type = T;
};

template <typename T>
struct remove_reference<T &&>
{
	using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename From, typename To>
struct is_convertible
	: bool_constant<__is_convertible_to(From, To)>
{
};

template <typename From, typename To>
inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

template <typename T>
constexpr bool is_trivially_constructible_v = __is_trivially_constructible(T);

template <typename T>
constexpr bool is_trivially_destructible_v = __is_trivially_constructible(T);

template <typename T>
struct underlying_type
{
	using type = __underlying_type(T);
};

template <typename T>
using underlying_type_t = typename underlying_type<T>::type;

template <typename T>
struct remove_const
{
	using type = T;
};

template <typename T>
struct remove_const<T const>
{
	using type = T;
};

template <typename T>
using remove_const_t = typename remove_const<T>::type;

template <typename T>
struct remove_volatile
{
	using type = T;
};

template <typename T>
struct remove_volatile<T volatile>
{
	using type = T;
};

template <typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template <typename T>
using remove_cv_t = remove_const_t<remove_volatile_t<T>>;

template <typename T>
struct remove_cv
{
	using type = remove_cv_t<T>;
};

template <typename T, typename U>
struct is_same
	: bool_constant<false>
{
};

template <typename T>
struct is_same<T, T>
	: bool_constant<true>
{
};

template <typename T>
struct is_void
	: is_same<remove_cv_t<T>, void>
{
};

template <typename T>
constexpr bool is_same_v = is_void<T>::value;

}

#pragma once
