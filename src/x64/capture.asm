extern __cxx_dispatch_exception: proc
extern __cxx_destroy_exception: proc
extern __cxx_seh_frame_handler: proc
extern __cxx_call_catch_frame_handler: proc

; The stack frame of `__CxxThrowException` always contains
; the catch info structure defined below. During the unwind,
; the referenced exception object may be destroyed, or reused
; in case of a rethrow. Furthermore, the `primary_frame_ptr` and
; `unwind_context` are captured and can be used by the next C++ frame handler.

catch_info_t struct
	cont_addr_0              qword ?
	cont_addr_1              qword ?
	primary_frame_ptr        qword ?
	exception_object_or_link qword ?
	throw_info_if_owner      qword ?
	unwind_context           qword ?
catch_info_t ends

; The __CxxThrowException allocates `throw_fr` as its frame. This is
; a fairly large structure containing the non-volatile context of the calling
; function. `rip` and `rsp` are layed out in a machine frame format, so that
; they can be applied by the `iretq` instruction.
;
; The context starts out marked by `.allocstack`. However, once the context is
; unwound, the execution falls through to different funclet, in which the same
; frame structure is marked with `.pushframe` and a bunch of `.pushreg`s.
; At that point, it is safe to modify non-volatile registers -- this is the
; point at which the context is applied, save the machine frame.
;
; After that, through another fallthrough, the frame is transformed into
; a `catch_fr`, which is significantly smaller. `catch_info` and the machine
; frame stay in the same place. The exception object's copy constructor and
; then the catch handler funclet are called on top of this smaller stack.
;
; The stack looks like this.
;
;     /---------------------------\
;     | catch funclet             |
;     +---------------------------+
;     | __cxx_call_catch_handler  |
;     |                           |
;     | catch_fr:                 |
;     | 0x00: red zone            |
;     | 0x20: machine frame       |
;     | 0x48: catch info          |
;     | 0x68: contents of the     |
;     |       unwound frames,     |
;     |       including the live  |
;     |       exception object    |
;     +---------------------------+
;     | some other funclet        |
;     +---------------------------+
;     |                           |
;
; The catch funclet returns the address at which the execution should continue
; in the lower frame. We therefore update the `rip` in the machine frame
; and then `iretq`. This leaves the stack in the following state.
;
;     /---------------------------\
;     | some other funclet        |
;     +---------------------------+
;     |                           |

throw_fr struct
	p1 qword ?
	p2 qword ?
	p3 qword ?
	p4 qword ?

	$xmm6  oword ?   ; 0x40
	$xmm7  oword ?   ; 0x50
	$xmm8  oword ?   ; 0x60
	$xmm9  oword ?   ; 0x70
	$xmm10 oword ?   ; 0x80
	$xmm11 oword ?   ; 0x90
	$xmm12 oword ?   ; 0xa0
	$xmm13 oword ?   ; 0xb0
	$xmm14 oword ?   ; 0xc0
	$xmm15 oword ?   ; 0xd0

	$rbx   qword ?   ; 0x00
	$rbp   qword ?   ; 0x08
	$rsi   qword ?   ; 0x10
	$rdi   qword ?   ; 0x18
	$r12   qword ?   ; 0x20
	$r13   qword ?   ; 0x28
	$r14   qword ?   ; 0x30
	$r15   qword ?   ; 0x38

	$rip   qword ?
	$cs    qword ?
	eflags qword ?
	$rsp   qword ?
	$ss    qword ?

	catch_info catch_info_t <>
throw_fr ends

catch_fr struct
	p1 qword ?
	p2 qword ?
	p3 qword ?
	p4 qword ?

	$rip   qword ?
	$cs    qword ?
	eflags qword ?
	$rsp   qword ?
	$ss    qword ?

	catch_info catch_info_t <>
catch_fr ends

.code

__NLG_Return2 proc public
	ret
__NLG_Return2 endp

__NLG_Dispatch2 proc public
	ret
__NLG_Dispatch2 endp

_CxxThrowException proc public frame: __cxx_seh_frame_handler
	; We receive two arguments, the pointer to the exception object
	; and a const pointer to `cxx_throw_info`. In case of a rethrow,
	; both of these are null.
	;
	; We need to
	;
	;  * walk the stack in the search of a catch handler,
	;  * unwind the frames along the way,
	;  * construct the catch variable for the handler (if any),
	;  * call the handler, and
	;  * destroy the exception object, unless another frame owns it.
	;
	; Most of the work is done by `__cxx_dispatch_exception`, which unwinds
	; the stack and fills in a catch info structure containing
	; everything we need to construct the catch var, call the handler,
	; and destroy the exception object.
	;
	; We must capture our caller's non-volatile context, then reapply it
	; after it is modified by the dispatcher.
	;
	; The layout of `throw_fr` is purpusfully made to contain a machine context
	; so that we can `iretq` to free the stack while keeping the .pdata-based
	; callstack valid at every point during the exception handling.

	mov r10, [rsp]
	lea r11, [rsp + 8]

	sub rsp, (sizeof throw_fr) - throw_fr.catch_info
	.allocstack (sizeof throw_fr) - throw_fr.catch_info

	; We'll be filling the frame gradually to keep rsp offsets small
	; and thus to keep the opcodes small.

	mov eax, ss
	push rax
	.allocstack 8

	push r11
	.allocstack 8

	pushfq
	.allocstack 8

	mov eax, cs
	push rax
	.allocstack 8

	push r10
	.allocstack 8

	push r15
	.allocstack 8
	push r14
	.allocstack 8
	push r13
	.allocstack 8
	push r12
	.allocstack 8
	push rdi
	.allocstack 8
	push rsi
	.allocstack 8
	push rbp
	.allocstack 8
	push rbx
	.allocstack 8

	sub rsp, throw_fr.$rbx
	.allocstack throw_fr.$rbx
.endprolog

	; We index via `rax`, which points into the middle of the xmm context.
	; This makes the offset fit in a signed byte, making the opcodes shorter.
	base = throw_fr.$xmm10
	lea rax, [rsp + base]
	movdqa [rax - base + throw_fr.$xmm6], xmm6
	movdqa [rax - base + throw_fr.$xmm7], xmm7
	movdqa [rax - base + throw_fr.$xmm8], xmm8
	movdqa [rax - base + throw_fr.$xmm9], xmm9
	movdqa [rax - base + throw_fr.$xmm10], xmm10
	movdqa [rax - base + throw_fr.$xmm11], xmm11
	movdqa [rax - base + throw_fr.$xmm12], xmm12
	movdqa [rax - base + throw_fr.$xmm13], xmm13
	movdqa [rax - base + throw_fr.$xmm14], xmm14
	movdqa [rax - base + throw_fr.$xmm15], xmm15

	; The dispatcher walks the function frames on the stack and looks for
	; the C++ catch handler. During the walk it unwinds the frames.
	; If the handler is not found, the dispatcher MUST never return,
	; it should assert or bugcheck or whatever.
	;
	; Once the target frame is found and potentially partially unwound,
	; the dispatcher will construct the catch variable, if any.
	;
	; The function should be marked `noexcept` to make sure that a throw
	; in one of the destructors causes `std::terminate`, instead of
	; unwinding already unwound frames.
	;
	; The dispatcher receives as arguments
	;
	;  * the pointer to the exception object to throw,
	;  * the pointer to read-only `cxx_throw_info` structure,
	;  * the pointer to the throw frame object we just partially filled in.
	;
	;  Note that nothing except for the captured context is initialized.
	;
	;  Here, `rcx` and `rdx` are already filled by our caller.

	lea r8, [rsp]
	call __cxx_dispatch_exception

	; Here, `rax` contains the address of the catch handler funclet we'll be
	; calling later. We eventually need to move it to `rcx` for NLG notification
	; and it makes sense to do it here, since we need at least one more
	; instruction in this .pdata context.

	mov rcx, rax

	; Now that the dispatcher returned, the non-volatile context should be
	; modified to that of the catcher's frame and all the fields should be
	; filled in. We can now reclain some stack space, but we must keep
	; the exception object, which lives in the thrower's frame, alive.
	; Since the thrower is right below us, the best we can do is make our own
	; stack smaller.
	;
	; As such, we now immediately apply the non-volatile context and
	; free it from the stack, except for the machine frame. Given our .pdata
	; context, we mustn't modify non-volatile registers here. Instead, we fall
	; through to another funclet with a different .pdata setup.
_CxxThrowException endp

__cxx_eh_apply_context proc private frame: __cxx_seh_frame_handler
	; This function expects its frame to have already been allocated and that
	; essentially means that it shouldn't really be called.
	;
	; The .pdata for this function is setup so that the non-volatile context
	; filled in by the previous funclet is considered when walking the stack.
	; Through `.pushframe`, all stack frames above the catcher's become
	; part of our frame. All exceptions thrown from now on will not
	; unwind any unwound frames again. Futhermore, we can write
	; into the non-volatile registers without it affecting the callstack.
.pushframe
.pushreg r15
.pushreg r14
.pushreg r13
.pushreg r12
.pushreg rdi
.pushreg rsi
.pushreg rbp
.pushreg rbx
.allocstack throw_fr.$rbx
.savexmm128 xmm6, throw_fr.$xmm6
.savexmm128 xmm7, throw_fr.$xmm7
.savexmm128 xmm8, throw_fr.$xmm8
.savexmm128 xmm9, throw_fr.$xmm9
.savexmm128 xmm10, throw_fr.$xmm10
.savexmm128 xmm11, throw_fr.$xmm11
.savexmm128 xmm12, throw_fr.$xmm12
.savexmm128 xmm13, throw_fr.$xmm13
.savexmm128 xmm14, throw_fr.$xmm14
.savexmm128 xmm15, throw_fr.$xmm15
.endprolog

	base = throw_fr.$xmm10
	lea rax, [rsp + base]
	movdqa xmm6, [rax - base + throw_fr.$xmm6]
	movdqa xmm7, [rax - base + throw_fr.$xmm7]
	movdqa xmm8, [rax - base + throw_fr.$xmm8]
	movdqa xmm9, [rax - base + throw_fr.$xmm9]
	movdqa xmm10, [rax - base + throw_fr.$xmm10]
	movdqa xmm11, [rax - base + throw_fr.$xmm11]
	movdqa xmm12, [rax - base + throw_fr.$xmm12]
	movdqa xmm13, [rax - base + throw_fr.$xmm13]
	movdqa xmm14, [rax - base + throw_fr.$xmm14]
	movdqa xmm15, [rax - base + throw_fr.$xmm15]

	base = throw_fr.$rdi
	lea rax, [rax + base - throw_fr.$xmm10]
	mov rbx, [rax - base + throw_fr.$rbx]
	mov rbp, [rax - base + throw_fr.$rbp]
	mov rsi, [rax - base + throw_fr.$rsi]
	mov rdi, [rax - base + throw_fr.$rdi]
	mov r12, [rax - base + throw_fr.$r12]
	mov r13, [rax - base + throw_fr.$r13]
	mov r14, [rax - base + throw_fr.$r14]
	mov r15, [rax - base + throw_fr.$r15]

	; The following instruction will turn `throw_fr` into `catch_fr`,
	; essentially removing the non-volatile context, which has already been
	; applied, from the stack. Since we're moving the frame pointer, we must
	; change .pdata again.

	lea rsp, [rax - base + (sizeof throw_fr) - (sizeof catch_fr)]
__cxx_eh_apply_context endp

__cxx_call_catch_handler proc public frame: __cxx_call_catch_frame_handler
.pushframe
.allocstack catch_fr.$rip
.endprolog
	; We now have a small frame, so we can call the handler. Everything can
	; throw now and our frame handler must be ready to free the exception
	; object if it happens.
	;
	; It's unclear why the handler expects the frame pointer in `rdx` rather
	; than `rcx`. The Microsoft's implementation leave the handler's function
	; pointer in `rcx`, so we're going to follow suit, just to be sure.

	mov rdx, [rsp + catch_fr.catch_info.primary_frame_ptr]
	mov r8, 1
	call __NLG_Dispatch2
	call rcx
	call __NLG_Return2

	; The handler returns the continuation address. Apply it to the machine
	; frame; this changes the callstack.

	cmp rax, 1
	ja _direct_continuation_address
	mov rax, [rsp + catch_fr.catch_info.cont_addr_0 + 8*rax]
_direct_continuation_address:

	mov [rsp + catch_fr.$rip], rax

	mov rcx, rax
	mov rdx, [rsp + catch_fr.catch_info.primary_frame_ptr]
	mov r8, 2
	call __NLG_Dispatch2

	; One more change of the current .pdata entry. Although
	; `__cxx_destroy_exception` is noexcept, we don't want any rethrow
	; probes to look at our frame while the exception object is being
	; destroyed.
__cxx_call_catch_handler endp

__cxx_call_exception_destructor proc private frame: __cxx_seh_frame_handler
.pushframe
.allocstack catch_fr.$rip
.endprolog
	; Now destroy the exception object.

	lea rcx, [rsp + catch_fr.catch_info]
	call __cxx_destroy_exception

	; We'd love to iretq here, but as usual, the deallocation of our frame
	; moves `rsp` and `iretq` can't be in the function's epilog.
	; One last switch of the .pdata context is necessary.

	add rsp, catch_fr.$rip
__cxx_call_exception_destructor endp

__cxx_continue_after_exception proc frame: __cxx_seh_frame_handler
.pushframe
.endprolog
	iretq
__cxx_continue_after_exception endp

end
