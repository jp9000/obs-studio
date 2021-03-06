/* Check (assertions)
 * Portable Snippets - https://gitub.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#if !defined(SIMDE_CHECK_H)
#define SIMDE_CHECK_H

#if !defined(SIMDE_NDEBUG) && !defined(SIMDE_DEBUG)
#define SIMDE_NDEBUG 1
#endif

#include "hedley.h"
#include "simde-diagnostic.h"
#include <stdint.h>

#if !defined(_WIN32)
#define SIMDE_SIZE_MODIFIER "z"
#define SIMDE_CHAR_MODIFIER "hh"
#define SIMDE_SHORT_MODIFIER "h"
#else
#if defined(_M_X64) || defined(__amd64__)
#define SIMDE_SIZE_MODIFIER "I64"
#else
#define SIMDE_SIZE_MODIFIER ""
#endif
#define SIMDE_CHAR_MODIFIER ""
#define SIMDE_SHORT_MODIFIER ""
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#define SIMDE_PUSH_DISABLE_MSVC_C4127_ \
	__pragma(warning(push)) __pragma(warning(disable : 4127))
#define SIMDE_POP_DISABLE_MSVC_C4127_ __pragma(warning(pop))
#else
#define SIMDE_PUSH_DISABLE_MSVC_C4127_
#define SIMDE_POP_DISABLE_MSVC_C4127_
#endif

#if !defined(simde_errorf)
#if defined(__has_include)
#if __has_include(<stdio.h>)
#include <stdio.h>
#endif
#elif defined(SIMDE_STDC_HOSTED)
#if SIMDE_STDC_HOSTED == 1
#include <stdio.h>
#endif
#elif defined(__STDC_HOSTED__)
#if __STDC_HOSTETD__ == 1
#include <stdio.h>
#endif
#endif

#include "debug-trap.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_
#if defined(EOF)
#define simde_errorf(format, ...) \
	(fprintf(stderr, format, __VA_ARGS__), abort())
#else
#define simde_errorf(format, ...) (simde_trap())
#endif
HEDLEY_DIAGNOSTIC_POP
#endif

#define simde_error(msg) simde_errorf("%s", msg)

#if defined(SIMDE_NDEBUG) ||                                 \
	(defined(__cplusplus) && (__cplusplus < 201103L)) || \
	(defined(__STDC__) && (__STDC__ < 199901L))
#if defined(SIMDE_CHECK_FAIL_DEFINED)
#define simde_assert(expr)
#else
#if defined(HEDLEY_ASSUME)
#define simde_assert(expr) HEDLEY_ASSUME(expr)
#elif HEDLEY_GCC_VERSION_CHECK(4, 5, 0)
#define simde_assert(expr) ((void)(!!(expr) ? 1 : (__builtin_unreachable(), 1)))
#elif HEDLEY_MSVC_VERSION_CHECK(13, 10, 0)
#define simde_assert(expr) __assume(expr)
#else
#define simde_assert(expr)
#endif
#endif
#define simde_assert_true(expr) simde_assert(expr)
#define simde_assert_false(expr) simde_assert(!(expr))
#define simde_assert_type_full(prefix, suffix, T, fmt, a, op, b) \
	simde_assert(((a)op(b)))
#define simde_assert_double_equal(a, b, precision)
#define simde_assert_string_equal(a, b)
#define simde_assert_string_not_equal(a, b)
#define simde_assert_memory_equal(size, a, b)
#define simde_assert_memory_not_equal(size, a, b)
#else
#define simde_assert(expr)                                            \
	do {                                                          \
		if (!HEDLEY_LIKELY(expr)) {                           \
			simde_error("assertion failed: " #expr "\n"); \
		}                                                     \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                        \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_true(expr)                                \
	do {                                                   \
		if (!HEDLEY_LIKELY(expr)) {                    \
			simde_error("assertion failed: " #expr \
				    " is not true\n");         \
		}                                              \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                 \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_false(expr)                               \
	do {                                                   \
		if (!HEDLEY_LIKELY(!(expr))) {                 \
			simde_error("assertion failed: " #expr \
				    " is not false\n");        \
		}                                              \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                 \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_type_full(prefix, suffix, T, fmt, a, op, b)           \
	do {                                                               \
		T simde_tmp_a_ = (a);                                      \
		T simde_tmp_b_ = (b);                                      \
		if (!(simde_tmp_a_ op simde_tmp_b_)) {                     \
			simde_errorf("assertion failed: %s %s %s (" prefix \
				     "%" fmt suffix " %s " prefix          \
				     "%" fmt suffix ")\n",                 \
				     #a, #op, #b, simde_tmp_a_, #op,       \
				     simde_tmp_b_);                        \
		}                                                          \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                             \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_double_equal(a, b, precision)                           \
	do {                                                                 \
		const double simde_tmp_a_ = (a);                             \
		const double simde_tmp_b_ = (b);                             \
		const double simde_tmp_diff_ =                               \
			((simde_tmp_a_ - simde_tmp_b_) < 0)                  \
				? -(simde_tmp_a_ - simde_tmp_b_)             \
				: (simde_tmp_a_ - simde_tmp_b_);             \
		if (HEDLEY_UNLIKELY(simde_tmp_diff_ > 1e-##precision)) {     \
			simde_errorf(                                        \
				"assertion failed: %s == %s (%0." #precision \
				"g == %0." #precision "g)\n",                \
				#a, #b, simde_tmp_a_, simde_tmp_b_);         \
		}                                                            \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                               \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#include <string.h>
#define simde_assert_string_equal(a, b)                                                   \
	do {                                                                              \
		const char *simde_tmp_a_ = a;                                             \
		const char *simde_tmp_b_ = b;                                             \
		if (HEDLEY_UNLIKELY(strcmp(simde_tmp_a_, simde_tmp_b_) !=                 \
				    0)) {                                                 \
			simde_errorf(                                                     \
				"assertion failed: string %s == %s (\"%s\" == \"%s\")\n", \
				#a, #b, simde_tmp_a_, simde_tmp_b_);                      \
		}                                                                         \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                                            \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_string_not_equal(a, b)                                               \
	do {                                                                              \
		const char *simde_tmp_a_ = a;                                             \
		const char *simde_tmp_b_ = b;                                             \
		if (HEDLEY_UNLIKELY(strcmp(simde_tmp_a_, simde_tmp_b_) ==                 \
				    0)) {                                                 \
			simde_errorf(                                                     \
				"assertion failed: string %s != %s (\"%s\" == \"%s\")\n", \
				#a, #b, simde_tmp_a_, simde_tmp_b_);                      \
		}                                                                         \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                                            \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_memory_equal(size, a, b)                                                                        \
	do {                                                                                                         \
		const unsigned char *simde_tmp_a_ =                                                                  \
			(const unsigned char *)(a);                                                                  \
		const unsigned char *simde_tmp_b_ =                                                                  \
			(const unsigned char *)(b);                                                                  \
		const size_t simde_tmp_size_ = (size);                                                               \
		if (HEDLEY_UNLIKELY(memcmp(simde_tmp_a_, simde_tmp_b_,                                               \
					   simde_tmp_size_)) != 0) {                                                 \
			size_t simde_tmp_pos_;                                                                       \
			for (simde_tmp_pos_ = 0;                                                                     \
			     simde_tmp_pos_ < simde_tmp_size_;                                                       \
			     simde_tmp_pos_++) {                                                                     \
				if (simde_tmp_a_[simde_tmp_pos_] !=                                                  \
				    simde_tmp_b_[simde_tmp_pos_]) {                                                  \
					simde_errorf(                                                                \
						"assertion failed: memory %s == %s, at offset %" SIMDE_SIZE_MODIFIER \
						"u\n",                                                               \
						#a, #b, simde_tmp_pos_);                                             \
					break;                                                                       \
				}                                                                                    \
			}                                                                                            \
		}                                                                                                    \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                                                                       \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_

#define simde_assert_memory_not_equal(size, a, b)                                          \
	do {                                                                               \
		const unsigned char *simde_tmp_a_ =                                        \
			(const unsigned char *)(a);                                        \
		const unsigned char *simde_tmp_b_ =                                        \
			(const unsigned char *)(b);                                        \
		const size_t simde_tmp_size_ = (size);                                     \
		if (HEDLEY_UNLIKELY(memcmp(simde_tmp_a_, simde_tmp_b_,                     \
					   simde_tmp_size_)) == 0) {                       \
			simde_errorf(                                                      \
				"assertion failed: memory %s != %s (%" SIMDE_SIZE_MODIFIER \
				"u bytes)\n",                                              \
				#a, #b, simde_tmp_size_);                                  \
		}                                                                          \
		SIMDE_PUSH_DISABLE_MSVC_C4127_                                             \
	} while (0) SIMDE_POP_DISABLE_MSVC_C4127_
#endif

#define simde_assert_type(T, fmt, a, op, b) \
	simde_assert_type_full("", "", T, fmt, a, op, b)

#define simde_assert_char(a, op, b)               \
	simde_assert_type_full("'\\x", "'", char, \
			       "02" SIMDE_CHAR_MODIFIER "x", a, op, b)
#define simde_assert_uchar(a, op, b)                       \
	simde_assert_type_full("'\\x", "'", unsigned char, \
			       "02" SIMDE_CHAR_MODIFIER "x", a, op, b)
#define simde_assert_short(a, op, b) \
	simde_assert_type(short, SIMDE_SHORT_MODIFIER "d", a, op, b)
#define simde_assert_ushort(a, op, b) \
	simde_assert_type(unsigned short, SIMDE_SHORT_MODIFIER "u", a, op, b)
#define simde_assert_int(a, op, b) simde_assert_type(int, "d", a, op, b)
#define simde_assert_uint(a, op, b) \
	simde_assert_type(unsigned int, "u", a, op, b)
#define simde_assert_long(a, op, b) simde_assert_type(long int, "ld", a, op, b)
#define simde_assert_ulong(a, op, b) \
	simde_assert_type(unsigned long int, "lu", a, op, b)
#define simde_assert_llong(a, op, b) \
	simde_assert_type(long long int, "lld", a, op, b)
#define simde_assert_ullong(a, op, b) \
	simde_assert_type(unsigned long long int, "llu", a, op, b)

#define simde_assert_size(a, op, b) \
	simde_assert_type(size_t, SIMDE_SIZE_MODIFIER "u", a, op, b)

#define simde_assert_float(a, op, b) simde_assert_type(float, "f", a, op, b)
#define simde_assert_double(a, op, b) simde_assert_type(double, "g", a, op, b)
#define simde_assert_ptr(a, op, b) \
	simde_assert_type(const void *, "p", a, op, b)

#define simde_assert_int8(a, op, b) simde_assert_type(int8_t, PRIi8, a, op, b)
#define simde_assert_uint8(a, op, b) simde_assert_type(uint8_t, PRIu8, a, op, b)
#define simde_assert_int16(a, op, b) \
	simde_assert_type(int16_t, PRIi16, a, op, b)
#define simde_assert_uint16(a, op, b) \
	simde_assert_type(uint16_t, PRIu16, a, op, b)
#define simde_assert_int32(a, op, b) \
	simde_assert_type(int32_t, PRIi32, a, op, b)
#define simde_assert_uint32(a, op, b) \
	simde_assert_type(uint32_t, PRIu32, a, op, b)
#define simde_assert_int64(a, op, b) \
	simde_assert_type(int64_t, PRIi64, a, op, b)
#define simde_assert_uint64(a, op, b) \
	simde_assert_type(uint64_t, PRIu64, a, op, b)

#define simde_assert_ptr_equal(a, b) simde_assert_ptr(a, ==, b)
#define simde_assert_ptr_not_equal(a, b) simde_assert_ptr(a, !=, b)
#define simde_assert_null(ptr) simde_assert_ptr(ptr, ==, NULL)
#define simde_assert_not_null(ptr) simde_assert_ptr(ptr, !=, NULL)
#define simde_assert_ptr_null(ptr) simde_assert_ptr(ptr, ==, NULL)
#define simde_assert_ptr_not_null(ptr) simde_assert_ptr(ptr, !=, NULL)

#endif /* !defined(SIMDE_CHECK_H) */
