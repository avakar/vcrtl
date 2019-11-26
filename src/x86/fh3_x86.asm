.model flat
assume fs:nothing

public __CxxThrowException@8
public ___CxxFrameHandler3
public @__cxx_call_funclet@8

public __NLG_Return2

extern _NLG_Notify: proc
extern ___fh3_primary_handler: proc
extern @__cxx_destroy_throw_frame@4: proc
extern @__cxx_dispatch_exception@4: proc
extern ___cxx_catch_frame_handler: proc

.safeseh ___fh3_primary_handler
.safeseh ___cxx_catch_frame_handler

fh3_fr struct
	frame_limit dword ?

	next_frame_reg dword ?
	frame_handler dword ?

	state dword ?
	next_frame_base dword ?
fh3_fr ends

.code

__CxxThrowException@8 proc
	; This function is called implicitly by the compiler whenever a throw
	; statement is executed. It receives two arguments,
	;
	; * the pointer to the exception object, and
	; * the pointer to the `cxx_throw_info_t` structure corresponding to
	;   the type of the exception object.
	;
	; We take ownership of the exception object here and are responsible for
	; destroying it. The pointer to the destructor routine is in the throw info.
	;
	; If the throw is a rethrow, both of the arguments are `nullptr`.
	;
	; This function should never return, instead, it should
	;
	; * locate the closest function call frame with a catch handler that matches
	;   the type of the exception object,
	; * unwind all frames above it,
	; * partially unwind the target frame so that the local variables in the
	;   try block that caught the exception are destroyed,
	; * copy construct the catch variable from the exception object, or if
	;   the variable is of reference type, bind it to the exception object,
	; * call the catch handler funclet,
	; * destroy the exception object, and
	; * resume execution at the address returned by the catch handler funclet.
	;
	; In the process, a nested C++ exception can be thrown from one of the
	; following places:
	;
	; 1. the destructor of a local variable during the unwind,
	; 2. the copy constructor of the catch variable,
	; 3. the catch handler funclet, or
	; 4. the destructor of the exception object.
	;
	; If a nested exception is thrown, we must ensure that the destruction
	; of local variables and of this frame's exception object are properly
	; sequenced. In particular, the order of destruction should be as follows.
	;
	; 1. Local variables in the catch handler funclet should be destroyed, then
	; 2. the exception object owned by this frame, and then
	; 3. the rest of the local variables.
	;
	; Note that the catch handler funclet doesn't register a frame handler,
	; which means that we must register one and take care of unwinding
	; both the funclet's frame and our own. As such, our frame must contain
	; all the information needed to perfom the unwind:
	;
	; * the pointer to the base of the function's frame,
	; * the function's unwind graph, accessible from the function's eh info, and
	; * the target state to which the frame should be unwound.

throw_fr struct
	eh_info dword ?
	unwound_state dword ?
	primary_frame_base dword ?

	exception_object dword ?
	throw_info dword ?
throw_fr ends

	; Here we construct the throw frame, fill in the exception object pointer
	; and the throw info. We also register `__cxx_throw_frame_handler`
	; as our frame handler.

	push ebp
	mov ebp, esp

	push [ebp+0ch]
	push [ebp+8]

	push 0
	push 0
	push 0

	; Our frame is setup, we now call `__cxx_dispatch_exception`, which
	; takes our frame, the _throw frame_, as its only argument.
	; This call is responsible for most of the work: it unwinds the frames,
	; locates the appropriate catch handler, and constructs its catch
	; variable. It also stores the catch handler's unwind info into
	; the throw frame so that we can properly handle nested exceptions.
	; It then returns the catch handler funclet's address.
	;
	; The function may throw a nested C++ exception from the catch variable's
	; constructor, in which case we destroy the exception object and
	; let the exception propagate. The unwind itself should not leak any
	; exceptions, `std::terminate` should be called instead.

	lea ecx, [esp]
	call @__cxx_dispatch_exception@4

	; The catch handler's address is now in `eax`. The funclet doesn't
	; expect any arguments, but expects to run in the primary funclet's frame.
	; Here we update the `ebp`.

	mov ebp, [esp + throw_fr.primary_frame_base]

	push 100h
	call _NLG_Notify

	; The following call will not respect non-volatility of registers,
	; we can only assume that `ebp` and `esp` will survive.
	;
	; The handler may throw.

	push offset ___cxx_catch_frame_handler
	push fs:[0]
	mov fs:[0], esp

	call eax
__NLG_Return2::
	mov ebx, eax

	pop ecx
	mov fs:[0], ecx

	mov ecx, 2
	mov [esp], ecx
	call _NLG_Notify

	; From this point on, nothing may throw. If an exception leaks from
	; the exception object's destructor, we bug check before the exception
	; reaches us. We therefore unregister the frame handler.

	lea ecx, [esp]
	call @__cxx_destroy_throw_frame@4

	mov esp, [ebp - fh3_fr.next_frame_base + fh3_fr.frame_limit]
	jmp ebx
__CxxThrowException@8 endp

@__cxx_call_funclet@8 proc
	push ebp
	mov ebp, edx
	call ecx
	pop ebp
	ret
@__cxx_call_funclet@8 endp

___CxxFrameHandler3 proc
	mov [esp+0ch], eax
	jmp ___fh3_primary_handler
___CxxFrameHandler3 endp

end
