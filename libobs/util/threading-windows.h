/*
 * Copyright (c) 2015 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <intrin.h>
#include <string.h>

#if !defined(_M_IX86) && !defined(_M_X64) && !defined(_M_ARM) && \
	!defined(_M_ARM64)
#error Processor not supported
#endif

static inline long os_atomic_inc_long(volatile long *val)
{
	return _InterlockedIncrement(val);
}

static inline long os_atomic_dec_long(volatile long *val)
{
	return _InterlockedDecrement(val);
}

static inline void os_atomic_store_long(volatile long *ptr, long val)
{
#if defined(_M_ARM64)
	__stlr32((volatile unsigned *)ptr, val);
#elif defined(_M_ARM)
	__dmb(_ARM_BARRIER_ISH);
	__iso_volatile_store32((volatile __int32 *)ptr, val);
	__dmb(_ARM_BARRIER_ISH);
#else
	_InterlockedExchange(ptr, val);
#endif
}

static inline long os_atomic_set_long(volatile long *ptr, long val)
{
	return _InterlockedExchange(ptr, val);
}

static inline long os_atomic_exchange_long(volatile long *ptr, long val)
{
	return os_atomic_set_long(ptr, val);
}

static inline long os_atomic_load_long(const volatile long *ptr)
{
#if defined(_M_ARM64)
	return __ldar32((volatile unsigned *)ptr);
#else
	const long val = __iso_volatile_load32((const volatile __int32 *)ptr);
#if defined(_M_ARM)
	__dmb(_ARM_BARRIER_ISH);
#else
	_ReadWriteBarrier();
#endif
	return val;
#endif
}

static inline bool os_atomic_compare_swap_long(volatile long *val, long old_val,
					       long new_val)
{
	return _InterlockedCompareExchange(val, new_val, old_val) == old_val;
}

static inline bool os_atomic_compare_exchange_long(volatile long *val,
						   long *old_ptr, long new_val)
{
	const long old_val = *old_ptr;
	const long previous =
		_InterlockedCompareExchange(val, new_val, old_val);
	*old_ptr = previous;
	return previous == old_val;
}

static inline void os_atomic_store_bool(volatile bool *ptr, bool val)
{
#if defined(_M_ARM64)
	__stlr8((volatile unsigned char *)ptr, val);
#elif defined(_M_ARM)
	__dmb(_ARM_BARRIER_ISH);
	__iso_volatile_store8((volatile char *)ptr, val);
	__dmb(_ARM_BARRIER_ISH);
#else
	_InterlockedExchange8((volatile char *)ptr, (char)val);
#endif
}

static inline bool os_atomic_set_bool(volatile bool *ptr, bool val)
{
	const char c = _InterlockedExchange8((volatile char *)ptr, (char)val);
	bool b;

	/* Avoid unnecesary char to bool conversion. Value known 0 or 1. */
	memcpy(&b, &c, sizeof(b));

	return b;
}

static inline bool os_atomic_exchange_bool(volatile bool *ptr, bool val)
{
	return os_atomic_set_bool(ptr, val);
}

static inline bool os_atomic_load_bool(const volatile bool *ptr)
{
	bool b;

#if defined(_M_ARM64)
	const unsigned char c = __ldar8((volatile unsigned char *)ptr);
#else
	const char c = __iso_volatile_load8((const volatile char *)ptr);
#if defined(_M_ARM)
	__dmb(_ARM_BARRIER_ISH);
#else
	_ReadWriteBarrier();
#endif
#endif

	/* Avoid unnecesary char to bool conversion. Value known 0 or 1. */
	memcpy(&b, &c, sizeof(b));

	return b;
}
