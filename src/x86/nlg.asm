.model flat
.code

public __NLG_Dispatch

_NLG_Notify proc public
	push ebp
	push [esp+8]
	push eax
__NLG_Dispatch::
	add esp, 0ch
	ret 4
_NLG_Notify endp

end
