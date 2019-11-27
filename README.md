# C++ Exceptions in Windows Drivers

This project implements parts of the Visual Studio runtime library
that are needed for C++ exception handling. Currently, x86 and x64
platforms are supported.

## Getting started

To use exceptions in your kernel-mode driver, first

* [download the binaries](https://github.com/avakar/vcrtl/releases), and then
* add `vcrtl_driver.props` to your driver project.

C++ exceptions will magically work.

## Features

The exception handling code was optimized to significantly reduce
the required stack space. On x86, the stack usage is negligible,
on x64, approximately 300 bytes are used during handler search,
these are, however, reclaimed before the catch handler is called.

No dynamic allocations or thread-local storage is used, everything
happens on the stack.

On x64, both FH3 and FH4 C++ exception ABI is supported. FH4 is
much better than FH3, prefer it.

No string comparisons are done during exception dispatch.

## Limitations

An exception must not leave the module in which it was thrown,
otherwise the dispatcher will bug-check.

No SEH exception may pass through frames in which you do C++
exception handling (this includes functions with `try/catch` blocks
or functions marked as `noexcept`, or functions with automatic variables
with non-trivial destructors). This will be detected and
you'll get a bug-check.

## TODO

Although `/GS` is supported, the frame cookies aren't checked yet.
