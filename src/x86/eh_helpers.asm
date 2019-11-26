.model flat
assume fs:nothing

public __EH_prolog3
public __EH_prolog3_catch
public __EH_epilog3

extern ___security_cookie: dword

.code

__EH_prolog3_catch proc
__EH_prolog3::
	; This is a special helper routine that is called at the beginning
	; of a function when optimizing for size. The caller's prolog might look
	; like this.
	;
	;     push 8
	;     mov eax, 6C2CB4h
	;     call _EH_prolog3_catch
	;
	; In `eax`, we receive the address of the caller's frame handler, which
	; we are expected to register. Upon entry, the stack looks as follows.
	;
	;     esp    : our return address
	;     esp+04h: the size of the stack allocation
	;     esp+08h: caller's return address
	;
	; The caller expects us to setup its FH3 stack frame, allocate space
	; for its local variables, save nonvolatile registers, and store
	; the frame cookie at the top of the frame.
	;
	; We must not touch `ecx` and `edx`, since they may contain caller's
	; parameters. This leave `eax` as our only volatile register.
	;
	; Upon return, the stack should look like this.
	;
	;     esp    : frame cookie
	;     esp+04h: saved edi
	;     esp+08h: saved esi
	;     esp+0ch: saved ebx
	;     esp+10h: stack allocation
	;     ...    : 
	;     ebp-10h: frame limit (i.e. esp after return)
	;     ebp-0ch: next frame handler registration
	;     ebp-08h: frame handler
	;     ebp-04h: 0xffffffff (current FH3 function state)
	;     ebp    : saved ebp
	;     ebp+04h: caller's return address

	push eax
	push dword ptr fs:[0]

	;     esp    : next frame handler registration
	;     esp+04h: frame handler
	;     esp+08h: our return address
	;     esp+0ch: the size of the stack allocation
	;     esp+10h: caller's return address

	mov eax, [esp+0ch]
	mov [esp+0ch], ebp
	lea ebp, [esp+0ch]

	; At this point the frame looks this way.
	;
	;     ebp-0ch: next frame handler registration
	;     ebp-08h: frame handler
	;     ebp-04h: our return address
	;     ebp    : saved ebp
	;     ebp+04h: caller's return address
	;
	; The stack allocation size is now in `eax`. We now extend the stack
	; to include the stack allocation, which frees `eax` for further use.

	sub esp, 4
	sub esp, eax

	; Save non-volatile registers.

	push ebx
	push esi
	push edi

	; Setup the frame cookie.

	mov eax, [___security_cookie]
	xor eax, ebp
	push eax

	mov [ebp-10h], esp

	push dword ptr [ebp-04h]
	mov dword ptr [ebp-04h], -1

	lea eax, [ebp+0ch]
	mov fs:[0], eax

	ret
__EH_prolog3_catch endp

__EH_epilog3 proc
	; This is called right before `ret` to tear down a stack frame.
	; We are supposed to unregister the frame handler and restore
	; non-volatile registers to pre-prolog state (including `ebp` and `esp`).
	;
	; We must not touch `eax` as it contains the caller's return value.

	mov ecx, [ebp-0ch]
	mov fs:[0], ecx

	pop ecx
	add esp, 4
	pop edi
	pop esi
	pop ebx

	mov esp, ebp
	pop ebp

	jmp ecx
__EH_epilog3 endp

end
