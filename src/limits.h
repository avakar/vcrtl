namespace vcrtl {

enum float_round_style
{
	round_indeterminate = -1,
	round_toward_zero = 0,
	round_to_nearest = 1,
	round_toward_infinity = 2,
	round_toward_neg_infinity = 3
};

template <typename T>
struct numeric_limits
{
	static constexpr bool is_specialized = false;
	static constexpr bool is_signed = false;
	static constexpr bool is_integer = false;
	static constexpr bool is_exact = false;
	static constexpr bool has_infinity = false;
	static constexpr bool has_quiet_NaN = false;
	static constexpr bool has_signaling_NaN = false;
	static constexpr bool has_denorm = false;
	static constexpr bool has_denorm_loss = false;
	static constexpr float_round_style round_style = round_toward_zero;
	static constexpr bool is_iec599 = false;
	static constexpr bool is_bounded = false;
	static constexpr bool is_modulo = false;
	static constexpr int digits = 0;
	static constexpr int digits10 = 0;
	static constexpr int max_digits10 = 0;
	static constexpr int radix = 0;
	static constexpr int min_exponent = 0;
	static constexpr int min_exponent10 = 0;
	static constexpr int max_exponent = 0;
	static constexpr bool traps = false;
	static constexpr bool tinyness_before = false;

	static constexpr T min() noexcept { return T(); };
	static constexpr T lowest() noexcept { return T(); };
	static constexpr T max() noexcept { return T(); };
	static constexpr T epsilon() noexcept { return T(); };
	static constexpr T round_error() noexcept { return T(); };
	static constexpr T infinity() noexcept { return T(); };
	static constexpr T quiet_NaN() noexcept { return T(); };
	static constexpr T signaling_NaN() noexcept { return T(); };
	static constexpr T denorm_min() noexcept { return T(); };
};

template <typename T>
struct numeric_limits<T const>
	: numeric_limits<T>
{
};

template <typename T>
struct numeric_limits<T volatile>
	: numeric_limits<T>
{
};

template <typename T>
struct numeric_limits<T const volatile>
	: numeric_limits<T>
{
};

template <typename T, T Min, T Max, int Digits>
struct _integer_numeric_limits
{
	static constexpr bool is_specialized = true;
	static constexpr bool is_signed = Min < 0;
	static constexpr bool is_integer = true;
	static constexpr bool is_exact = true;
	static constexpr bool has_infinity = false;
	static constexpr bool has_quiet_NaN = false;
	static constexpr bool has_signaling_NaN = false;
	static constexpr bool has_denorm = false;
	static constexpr bool has_denorm_loss = false;
	static constexpr float_round_style round_style = round_toward_zero;
	static constexpr bool is_iec599 = false;
	static constexpr bool is_bounded = true;
	static constexpr bool is_modulo = Min >= 0;
	static constexpr int digits = Digits;
	static constexpr int digits10 = (Digits * 19728) >> 16;
	static constexpr int max_digits10 = 0;
	static constexpr int radix = 2;
	static constexpr int min_exponent = 0;
	static constexpr int min_exponent10 = 0;
	static constexpr int max_exponent = 0;
	static constexpr bool traps = true;
	static constexpr bool tinyness_before = false;

	static constexpr T min() noexcept { return Min; };
	static constexpr T lowest() noexcept { return Min; };
	static constexpr T max() noexcept { return Max; };
	static constexpr T epsilon() noexcept { return 0; };
	static constexpr T round_error() noexcept { return 0; };
	static constexpr T infinity() noexcept { return 0; };
	static constexpr T quiet_NaN() noexcept { return 0; };
	static constexpr T signaling_NaN() noexcept { return 0; };
	static constexpr T denorm_min() noexcept { return 0; };
};

template <>
struct numeric_limits<unsigned int>
	: _integer_numeric_limits<unsigned int, 0, 0xffff'ffff, 32>
{
};

}

#pragma once
