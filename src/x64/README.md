# C++ Exception Handling on AMD64

Here we implement `__CxxThrowException`, a call to which is
emitted by MSVC when a `throw` expression is evaluated.

## Overview

The function
receives two arguments, a pointer to the exception object and a pointer
to a *throw info*, a read-only structure describing the type of the thrown
exception. The function is expected not to return, instead, control
is transferred to the appropriate exception handler.

    [[noreturn]]
    void __CxxThrowException(void *, __s_ThrowInfo const *);

On x64, most function frames on the call stack have an associated entry
in the `.pdata` and `.xdata` sections, which contains enough information
to find the function's caller and may contain a pointer to a frame handler
routine and arbitrary data available to the routine.

In general, functions which require the destruction of local variables,
contain `try` statements, or are marked as `noexcept` will have a frame
handler. Depending on the settings of the `/d2FH4` flag, the Microsoft's
compiler will use either

* `__CxxFrameHandler3`, if the flag is disabled, or
* `__CxxFrameHandler4`, otherwise.

Furthermore, when `/GS` is in effect, function deemed worthy of a security
cookie will be associate to

* `__GSHandlerCheck_EH`, or
* `__GSHandlerCheck_EH4`.

Both of the above functions will perform a GS check and then delegate
the work to the non-GS variant. When a function doesn't need
exception handling, but has a security cookie, the compiler will use
`__GSHandlerCheck`.

## Exception Objects

Recall that C++ exceptions can be caught by reference and then rethrown
using the nullary `throw` expression. When that happens and the rethrown
exception is caught again by reference, the C++ standard currently mandates
that the address of the exception object not change.

We generally want the exception objects to be stack-allocated for performance
reasons. However, once thrown, the stack space cannot be reclaimed until
the exception object -- which as explained above can't move -- is destroyed.
Therefore, the catch handler can't really execute in the frame of it's
containing function. Instead, the msvc compiler will emit a separate function
for each catch handler. When a catch handler is active, the call stack
might look like this.

    /--------------------\
    | Catch handler in F |
    +--------------------+
    |                    |
    | Zero or more       |
    | unwound frames,    |
    | one of which       |
    | contains the live  |
    | exception object   |
    |                    |
    +--------------------+
    | F                  |
    +--------------------+
    | caller of F        |

To avoid confusion, we use the term *funclet* to refer to the piece of code
that can be called and creates its own stack frame. We use the term
*function* to refer to C++ functions.

When a function is compiled, one *primary funclet* and zero or more
*catch funclets* are generated for it. Since code in a catch handler
can access variables outside its scope, the compiler will keep all
variables in the primary funclet's frame, with catch funclets only
maintaining a pointer to it. The pointer is passed to the catch funclet
in `rdx`, which would suggest that `rcx` contains an argument too, however,
it doesn't seem to be used. Microsoft's implementation passes the pointer
to the funclet itself in `rcx`.

The catch variable (i.e. the `e` in `catch (type e)`) also lives in
the primary frame.

## The Microsoft's Implementation

The implementation of `__CxxThrowExpression` in the Microsoft's CRT will
simply call `RaiseException`, which walks the stack and calls the associated
frame handlers. The `EXCEPTION_UNWINDING` flag passed to the routine
is unset, which informs the routines to not perform variable destruction
and to only search for `catch` handlers. If one is not found, the stack walk
continues.

When a matching catch handler is found, the frame handler calls
`RtlUnwindEx`, which starts the stack walk again, this time with the
`EXCEPTION_UNWIND` set, indicating to the frame handlers to destroy local
variables.

This approach is compatible with the Windows' SEH handling: `__try/__except`
statements will see C++ exceptions as they pass and can even handle them.
Similarly, `catch (...)` block can catch a SEH exception (an access
violation, for example), if the compiler is instructed to do so.

There are downsides however.

During the stack walk, the frame handlers can't pass any data to their
caller's frame handlers. Since C++ functions can be split into multiple
funclets, some things are unnecessarily computed multiple times.
Furthermore, a funclet containing a `try` statement decides to
handle the exception, it could get partially unwound (everything in the
handling `try` block must be destroyed). If another exception is then
thrown from the catch handler, the unwound part of the frame must not be
unwound again. Various tricks are used to communicate whether and which
catch handler was active to enable the frame to unwind correctly. This
includes a reserved 8-byte slot in each funclet's frame and thread-local
variables (ugh).

Additionally, since the exception handling is performed in two phases (handler
search, then unwind), the frame handlers have to compute some things twice.
FH4 in particular requires a some amount of decompressing and would be
more performant if the search and unwind was done in a single pass.

Finally, the SEH mechanism allows the execution to resume at the point,
where the exception was thrown. This is used for transparent stack enlargement,
for example. However, since a function can throw and then be resumed
at any point, the whole CPU context must be captured and then restored.
C++ exceptions can't resume and non-volatile context is sufficient.

Note that t he `CONTEXT` structure is *huge*. On AMD64 with a hypervisor
running, it may even have variable-sized extension fields that require
the use of `_alloca`. Several copies of `CONTEXT` are present on the stack
during the unwind. When the catch handler is eventually executed, the stack
space used by the `CONTEXT` structures is not reclaimed, it becomes
part of the consolidated frame between the funclet and its catch handler.

As such, each catch handler on the stack can easily take 8kB of space.
This makes the default implementation unsuitable for use in kernel mode,
where stack space is precious.

## Our implementation

We perform the exception handling ourselves, bypassing SEH, with particular
emphasis on low stack usage. Only one copy of the context is kept
during unwinding, and only its non-volatile part. Once the handler is found,
the context is freed from stack.

The unwinding is performed in a single pass. No thread-local storage is used.

The behavior differs slightly from Microsoft's.

* We do not support throwing across module boundaries. This makes type
  matching a bit faster, since we don't have to compare class names.
  It also makes the unwind faster, as we don't have to lookup image
  base address for each function on the stack. If a function from another
  image is encountered, our implementation bug-checks.
* There is no interaction with SEH. You can't catch a C++ exception
  by a `__try/__except` statement, nor can you catch a SEH exception by
  `catch (...)`. We bug-check if a C++ exception passes through a `__try`
  statement or if SEH passes through a function with a C++ frame handler.
* The debugger will not see the exception raised. No logs will be emitted
  to the output window.

Additionally, we violate the C++ standard in that we don't call
`std::terminate` if an exception is caught by value and the copy constructor
throws. Instead, we let the exception propagate. This means that the following
two pieces of code are equivalent in our implementation.

    try
    {
        throw exc();
    }
    // If copy of e throws, std::terminate should be called
    catch (exc e)
    {
    }

    try
    {
        throw exc();
    }
    catch (exc & e_ref)
    {
        // Propagates exception as expected
        exc e(e_ref);
    }

The `__CxxThrowException` function is written in assembly, as we depend on
the format of its stack frame to

* perform rethrows, and to
* destroy the exception object if another exception is thrown.
