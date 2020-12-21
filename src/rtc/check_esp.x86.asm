.model flat
.code

extern ?_on_rtc_esp_corruption@vcrtl@@YAXXZ: proc

__RTC_CheckEsp proc
	bnd jne ?_on_rtc_esp_corruption@vcrtl@@YAXXZ
	bnd ret
__RTC_CheckEsp endp

end
