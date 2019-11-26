# C++ Exception Handling on Intel x86

Here we implement `_CxxThrowException` and `___CxxFrameHandler3` for
the x86 platform. These functions are implicitly referenced by MSVC
when C++ exception handling is used.

The former function is called whenever the C++ throw statement is executed.
It has the following signature.

    void __stdcall _CxxThrowException(void * exception_object, cxx_throw_info_t const * throw_info);

If a new exception object is to be thrown, it is allocated on the thrower's
stack frame, then `_CxxThrowException` is called with the pointer to the
exception object as the first argument. The second argument is a pointer
to a read-only structure describing the exception object's type. It contains,
in particular,

* the pointer to a function responsible for destroying the exception object, and
* the list of _catchables_, each of which specifies one of the types that
  can appear in a matching catch handler, along with information needed to
  initialize the catch variable.

If an existing exception is to be rethrown, both arguments are `nullptr`.

The function is supposed to walk the stack frames until a matching catch
handler is found, destroying the visited stack frames in the process.

To understand `___CxxFrameHandler3`, a short introduction to [SEH][1] is needed.

## Structured Exception Handling



## Calling conventions

The peculiarity of the x86 platform is that multiple competing calling
conventions have evolved over time. Most of the time, the convention need not
be specified explicitly, the compiler will invoke routines as it sees fit.

For functions with external linkage, the calling convention is selected
by compiler options and defaults to cdecl. For the rest, the compiler
can be quite creative and invents its own conventions.

The functions for which the convention must to be specified explicitly include

* declarations Windows API functions,
* the functions called back by Windows (like frame handlers), and
* functions written in assembly.

For the latter, we use fastcall, as it is best in all aspects. We also humbly
suggest that you set it as your [default calling convention][1].

All standard calling conventions agree that `ebx`, `esi`, `edi` and `ebp`
are non-volatile (aka. callee-saved) and that results are returned in `eax`.

Some compiler-called routines, including `___CxxFrameHandler3`, and
unwind funclets, use a custom calling convention.

### cdecl

This is the default calling convention for the MSVC compiler, unless you use
change the default with a compiler option. It is also the only
calling convention that supports variable number of arguments; the compiler
will fall back to cdecl for vararg functions.

Arguments are pushed onto the stack in right-to-left order and are popped
by the caller. This differs from other calling conventions, but allows
the callee to be unaware of the parameter count, thus allowing varargs.

`extern "C"` cdecl function names are mangled by prefixing them with
an underscore.

Ironically, frame handlers use cdecl convention, but `_CxxThrowException`
uses stdcall.

### stdcall

Unlike cdecl, the callee is responsible for cleaning up the stack, which leads
to smaller executables in overall (fewer cleanup sequences in generated code),
but prohibits varargs.

When mangled, `extern "C"` functions are prefixed by an underscore and suffixed
by `@` followed by a decimal number specifying the amount of stack the function
frees. This is actually an ingenious way to prevent stack corruptions due to
ABI changes.

Note that stdcall is superior to cdecl for non-vararg functions.

`_CxxThrowException` uses stdcall and is therefore mangled
to `__CxxThrowException@8`.

 [1]: https://docs.microsoft.com/en-us/cpp/build/reference/gd-gr-gv-gz-calling-convention

### fastcall

In fastcall, the first two arguments are passed in `ecx` and `edx` respectively.
The rest are passed as in stdcall. This makes the code faster and with smaller
footprint.

### thiscall

By default, member functions use thiscall, where the implicit `this` argument
is passed in `ecx`. Otherwise, thiscall is the same as stdcall.

Unfortunately, you can't form a pointer to a thiscall function with MSVC.
However, you can work around this problem by declaring your function pointer
as fastcall and adding a dummy `int` parameter. For example, you can call
a copy constructor through a pointer with this signature.

    void (__fastcall * fn)(void * this_, int dummy_, void * other);
