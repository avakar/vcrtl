.model flat
public _memcpy

.code

memcpy_fr struct
	ret_addr dword ?
	dest dword ?
	src dword ?
	sz dword ?
memcpy_fr ends

_memcpy proc
	mov eax, esi
	mov edx, edi

	mov edi, [esp + memcpy_fr.dest]
	mov esi, [esp + memcpy_fr.src]
	mov ecx, [esp + memcpy_fr.sz]
	rep movsb

	mov edi, edx
	mov esi, eax

	mov eax, [esp + memcpy_fr.dest]
	ret
_memcpy endp

end
