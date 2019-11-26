#include "stdint.h"

template <typename T>
void * __GetExceptionInfo(T);

extern "C" long _InterlockedIncrement(long * addend);
#pragma intrinsic(_InterlockedIncrement)

extern "C" short _InterlockedIncrement16(short * addend);
#pragma intrinsic(_InterlockedIncrement16)

extern "C" long _InterlockedAdd(long volatile * addend, long value);

extern "C" char _InterlockedExchangeAdd8(char volatile * addend, char value);
#pragma intrinsic(_InterlockedExchangeAdd8)

extern "C" short _InterlockedExchangeAdd16(short volatile * addend, short value);
#pragma intrinsic(_InterlockedExchangeAdd16)

extern "C" long _InterlockedExchangeAdd(long volatile * addend, long value);
#pragma intrinsic(_InterlockedExchangeAdd)

extern "C" void * _AddressOfReturnAddress();
#pragma intrinsic(_AddressOfReturnAddress)

#ifdef _M_IX86

extern "C" unsigned long __readfsdword(unsigned long offset);
#pragma intrinsic(__readfsdword)

extern "C" void __writefsdword(unsigned long offset, unsigned long value);
#pragma intrinsic(__writefsdword)

#elif defined(_M_AMD64)

extern "C" __int64 _InterlockedIncrement64(__int64 * addend);
#pragma intrinsic(_InterlockedIncrement64)

extern "C" __int64 _InterlockedAdd64(__int64 volatile * addend, __int64 value);

extern "C" __int64 _InterlockedExchangeAdd64(__int64 volatile * addend, __int64 value);
#pragma intrinsic(_InterlockedExchangeAdd64)

#endif
#pragma once
