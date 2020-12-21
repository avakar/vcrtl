namespace vcrtl {

enum class bug_check_reason
{
	unwind_on_unsafe_exception,
	invalid_target_unwind,
	corrupted_function_unwind_state,
	corrupted_eh_unwind_data,
	corrupted_exception_state,

	unwinding_non_cxx_frame,

	seh_handler_not_in_safeseh,
	destructor_threw_during_unwind,
	corrupted_exception_registration_chain,
	noexcept_violation,
	exception_specification_not_supported,
	no_matching_exception_handler,

	assertion_failure,
	forbidden_call,
	std_terminate,

	rtc_esp_corruption,
	rtc_canary_corruption,
};

[[noreturn]] void __fastcall on_bug_check(bug_check_reason reason);

}

#pragma once
