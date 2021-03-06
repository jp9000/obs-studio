/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2017-2020 Evan Nemerson <evan@nemerson.com>
 */

#if !defined(SIMDE_COMMON_H)
#define SIMDE_COMMON_H

#include "hedley.h"

#define SIMDE_VERSION_MAJOR 0
#define SIMDE_VERSION_MINOR 7
#define SIMDE_VERSION_MICRO 1
#define SIMDE_VERSION                                                   \
	HEDLEY_VERSION_ENCODE(SIMDE_VERSION_MAJOR, SIMDE_VERSION_MINOR, \
			      SIMDE_VERSION_MICRO)

#include <stddef.h>
#include <stdint.h>

#include "simde-detect-clang.h"
#include "simde-arch.h"
#include "simde-features.h"
#include "simde-diagnostic.h"
#include "simde-math.h"
#include "simde-constify.h"
#include "simde-align.h"

/* In some situations, SIMDe has to make large performance sacrifices
 * for small increases in how faithfully it reproduces an API, but
 * only a relatively small number of users will actually need the API
 * to be completely accurate.  The SIMDE_FAST_* options can be used to
 * disable these trade-offs.
 *
 * They can be enabled by passing -DSIMDE_FAST_MATH to the compiler, or
 * the individual defines (e.g., -DSIMDE_FAST_NANS) if you only want to
 * enable some optimizations.  Using -ffast-math and/or
 * -ffinite-math-only will also enable the relevant options.  If you
 * don't want that you can pass -DSIMDE_NO_FAST_* to disable them. */

/* Most programs avoid NaNs by never passing values which can result in
 * a NaN; for example, if you only pass non-negative values to the sqrt
 * functions, it won't generate a NaN.  On some platforms, similar
 * functions handle NaNs differently; for example, the _mm_min_ps SSE
 * function will return 0.0 if you pass it (0.0, NaN), but the NEON
 * vminq_f32 function will return NaN.  Making them behave like one
 * another is expensive; it requires generating a mask of all lanes
 * with NaNs, then performing the operation (e.g., vminq_f32), then
 * blending together the result with another vector using the mask.
 *
 * If you don't want SIMDe to worry about the differences between how
 * NaNs are handled on the two platforms, define this (or pass
 * -ffinite-math-only) */
#if !defined(SIMDE_FAST_MATH) && !defined(SIMDE_NO_FAST_MATH) && \
	defined(__FAST_MATH__)
#define SIMDE_FAST_MATH
#endif

#if !defined(SIMDE_FAST_NANS) && !defined(SIMDE_NO_FAST_NANS)
#if defined(SIMDE_FAST_MATH)
#define SIMDE_FAST_NANS
#elif defined(__FINITE_MATH_ONLY__)
#if __FINITE_MATH_ONLY__
#define SIMDE_FAST_NANS
#endif
#endif
#endif

/* Many functions are defined as using the current rounding mode
 * (i.e., the SIMD version of fegetround()) when converting to
 * an integer.  For example, _mm_cvtpd_epi32.  Unfortunately,
 * on some platforms (such as ARMv8+ where round-to-nearest is
 * always used, regardless of the FPSCR register) this means we
 * have to first query the current rounding mode, then choose
 * the proper function (rounnd
 , ceil, floor, etc.) */
#if !defined(SIMDE_FAST_ROUND_MODE) && !defined(SIMDE_NO_FAST_ROUND_MODE) && \
	defined(SIMDE_FAST_MATH)
#define SIMDE_FAST_ROUND_MODE
#endif

/* This controls how ties are rounded.  For example, does 10.5 round to
 * 10 or 11?  IEEE 754 specifies round-towards-even, but ARMv7 (for
 * example) doesn't support it and it must be emulated (which is rather
 * slow).  If you're okay with just using the default for whatever arch
 * you're on, you should definitely define this.
 *
 * Note that we don't use this macro to avoid correct implementations
 * in functions which are explicitly about rounding (such as vrnd* on
 * NEON, _mm_round_* on x86, etc.); it is only used for code where
 * rounding is a component in another function, and even then it isn't
 * usually a problem since such functions will use the current rounding
 * mode. */
#if !defined(SIMDE_FAST_ROUND_TIES) && !defined(SIMDE_NO_FAST_ROUND_TIES) && \
	defined(SIMDE_FAST_MATH)
#define SIMDE_FAST_ROUND_TIES
#endif

/* For functions which convert from one type to another (mostly from
 * floating point to integer types), sometimes we need to do a range
 * check and potentially return a different result if the value
 * falls outside that range.  Skipping this check can provide a
 * performance boost, at the expense of faithfulness to the API we're
 * emulating. */
#if !defined(SIMDE_FAST_CONVERSION_RANGE) && \
	!defined(SIMDE_NO_FAST_CONVERSION_RANGE) && defined(SIMDE_FAST_MATH)
#define SIMDE_FAST_CONVERSION_RANGE
#endif

#if HEDLEY_HAS_BUILTIN(__builtin_constant_p) ||                             \
	HEDLEY_GCC_VERSION_CHECK(3, 4, 0) ||                                \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                             \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 19) ||                             \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                                \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 0) ||                               \
	HEDLEY_TI_CL6X_VERSION_CHECK(6, 1, 0) ||                            \
	(HEDLEY_SUNPRO_VERSION_CHECK(5, 10, 0) && !defined(__cplusplus)) || \
	HEDLEY_CRAY_VERSION_CHECK(8, 1, 0)
#define SIMDE_CHECK_CONSTANT_(expr) (__builtin_constant_p(expr))
#elif defined(__cplusplus) && (__cplusplus > 201703L)
#include <type_traits>
#define SIMDE_CHECK_CONSTANT_(expr) (std::is_constant_evaluated())
#endif

#if !defined(SIMDE_NO_CHECK_IMMEDIATE_CONSTANT)
#if defined(SIMDE_CHECK_CONSTANT_) &&                \
	SIMDE_DETECT_CLANG_VERSION_CHECK(9, 0, 0) && \
	(!defined(__apple_build_version__) ||        \
	 ((__apple_build_version__ < 11000000) ||    \
	  (__apple_build_version__ >= 12000000)))
#define SIMDE_REQUIRE_CONSTANT(arg)                    \
	HEDLEY_REQUIRE_MSG(SIMDE_CHECK_CONSTANT_(arg), \
			   "`" #arg "' must be constant")
#else
#define SIMDE_REQUIRE_CONSTANT(arg)
#endif
#else
#define SIMDE_REQUIRE_CONSTANT(arg)
#endif

#define SIMDE_REQUIRE_RANGE(arg, min, max)                         \
	HEDLEY_REQUIRE_MSG((((arg) >= (min)) && ((arg) <= (max))), \
			   "'" #arg "' must be in [" #min ", " #max "]")

#define SIMDE_REQUIRE_CONSTANT_RANGE(arg, min, max) \
	SIMDE_REQUIRE_CONSTANT(arg)                 \
	SIMDE_REQUIRE_RANGE(arg, min, max)

/* A copy of HEDLEY_STATIC_ASSERT, except we don't define an empty
 * fallback if we can't find an implementation; instead we have to
 * check if SIMDE_STATIC_ASSERT is defined before using it. */
#if !defined(__cplusplus) &&                                             \
	((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
	 HEDLEY_HAS_FEATURE(c_static_assert) ||                          \
	 HEDLEY_GCC_VERSION_CHECK(6, 0, 0) ||                            \
	 HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) || defined(_Static_assert))
#define SIMDE_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
	HEDLEY_MSVC_VERSION_CHECK(16, 0, 0)
#define SIMDE_STATIC_ASSERT(expr, message)            \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_( \
		static_assert(expr, message))
#endif

#if (HEDLEY_HAS_ATTRIBUTE(may_alias) && !defined(HEDLEY_SUNPRO_VERSION)) || \
	HEDLEY_GCC_VERSION_CHECK(3, 3, 0) ||                                \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                             \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 0)
#define SIMDE_MAY_ALIAS __attribute__((__may_alias__))
#else
#define SIMDE_MAY_ALIAS
#endif

/*  Lots of compilers support GCC-style vector extensions, but many
    don't support all the features.  Define different macros depending
    on support for

    * SIMDE_VECTOR - Declaring a vector.
    * SIMDE_VECTOR_OPS - basic operations (binary and unary).
    * SIMDE_VECTOR_NEGATE - negating a vector
    * SIMDE_VECTOR_SCALAR - For binary operators, the second argument
        can be a scalar, in which case the result is as if that scalar
        had been broadcast to all lanes of a vector.
    * SIMDE_VECTOR_SUBSCRIPT - Supports array subscript notation for
        extracting/inserting a single element.=

    SIMDE_VECTOR can be assumed if any others are defined, the
    others are independent. */
#if !defined(SIMDE_NO_VECTOR)
#if HEDLEY_GCC_VERSION_CHECK(4, 8, 0)
#define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#define SIMDE_VECTOR_OPS
#define SIMDE_VECTOR_NEGATE
#define SIMDE_VECTOR_SCALAR
#define SIMDE_VECTOR_SUBSCRIPT
#elif HEDLEY_INTEL_VERSION_CHECK(16, 0, 0)
#define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#define SIMDE_VECTOR_OPS
#define SIMDE_VECTOR_NEGATE
/* ICC only supports SIMDE_VECTOR_SCALAR for constants */
#define SIMDE_VECTOR_SUBSCRIPT
#elif HEDLEY_GCC_VERSION_CHECK(4, 1, 0) || HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#define SIMDE_VECTOR_OPS
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 12, 0)
#define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#elif HEDLEY_HAS_ATTRIBUTE(vector_size)
#define SIMDE_VECTOR(size) __attribute__((__vector_size__(size)))
#define SIMDE_VECTOR_OPS
#define SIMDE_VECTOR_NEGATE
#define SIMDE_VECTOR_SUBSCRIPT
#if SIMDE_DETECT_CLANG_VERSION_CHECK(5, 0, 0)
#define SIMDE_VECTOR_SCALAR
#endif
#endif

/* GCC and clang have built-in functions to handle shuffling and
   converting of vectors, but the implementations are slightly
   different.  This macro is just an abstraction over them.  Note that
   elem_size is in bits but vec_size is in bytes. */
#if !defined(SIMDE_NO_SHUFFLE_VECTOR) && defined(SIMDE_VECTOR_SUBSCRIPT)
HEDLEY_DIAGNOSTIC_PUSH
/* We don't care about -Wvariadic-macros; all compilers that support
      * shufflevector/shuffle support them. */
#if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic")
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#if HEDLEY_HAS_WARNING("-Wvariadic-macros") || HEDLEY_GCC_VERSION_CHECK(4, 0, 0)
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif

#if HEDLEY_HAS_BUILTIN(__builtin_shufflevector)
#define SIMDE_SHUFFLE_VECTOR_(elem_size, vec_size, a, b, ...) \
	__builtin_shufflevector(a, b, __VA_ARGS__)
#elif HEDLEY_GCC_HAS_BUILTIN(__builtin_shuffle, 4, 7, 0) && \
	!defined(__INTEL_COMPILER)
#define SIMDE_SHUFFLE_VECTOR_(elem_size, vec_size, a, b, ...) \
	(__extension__({                                      \
		int##elem_size##_t SIMDE_VECTOR(vec_size)     \
			simde_shuffle_ = {__VA_ARGS__};       \
		__builtin_shuffle(a, b, simde_shuffle_);      \
	}))
#endif
HEDLEY_DIAGNOSTIC_POP
#endif

/* TODO: this actually works on XL C/C++ without SIMDE_VECTOR_SUBSCRIPT
   but the code needs to be refactored a bit to take advantage. */
#if !defined(SIMDE_NO_CONVERT_VECTOR) && defined(SIMDE_VECTOR_SUBSCRIPT)
#if HEDLEY_HAS_BUILTIN(__builtin_convertvector) || \
	HEDLEY_GCC_VERSION_CHECK(9, 0, 0)
#if HEDLEY_GCC_VERSION_CHECK(9, 0, 0) && !HEDLEY_GCC_VERSION_CHECK(9, 3, 0)
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93557 */
#define SIMDE_CONVERT_VECTOR_(to, from)                          \
	((to) = (__extension__({                                 \
		 __typeof__(from) from_ = (from);                \
		 ((void)from_);                                  \
		 __builtin_convertvector(from_, __typeof__(to)); \
	 })))
#else
#define SIMDE_CONVERT_VECTOR_(to, from) \
	((to) = __builtin_convertvector((from), __typeof__(to)))
#endif
#endif
#endif
#endif

/* Since we currently require SUBSCRIPT before using a vector in a
   union, we define these as dependencies of SUBSCRIPT.  They are
   likely to disappear in the future, once SIMDe learns how to make
   use of vectors without using the union members.  Do not use them
   in your code unless you're okay with it breaking when SIMDe
   changes. */
#if defined(SIMDE_VECTOR_SUBSCRIPT)
#if defined(SIMDE_VECTOR_OPS)
#define SIMDE_VECTOR_SUBSCRIPT_OPS
#endif
#if defined(SIMDE_VECTOR_SCALAR)
#define SIMDE_VECTOR_SUBSCRIPT_SCALAR
#endif
#endif

#if !defined(SIMDE_ENABLE_OPENMP) &&                   \
	((defined(_OPENMP) && (_OPENMP >= 201307L)) || \
	 (defined(_OPENMP_SIMD) && (_OPENMP_SIMD >= 201307L)))
#define SIMDE_ENABLE_OPENMP
#endif

#if !defined(SIMDE_ENABLE_CILKPLUS) && \
	(defined(__cilk) || defined(HEDLEY_INTEL_VERSION))
#define SIMDE_ENABLE_CILKPLUS
#endif

#if defined(SIMDE_ENABLE_OPENMP)
#define SIMDE_VECTORIZE HEDLEY_PRAGMA(omp simd)
#define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(omp simd safelen(l))
#if defined(__clang__)
#define SIMDE_VECTORIZE_REDUCTION(r)                              \
	HEDLEY_DIAGNOSTIC_PUSH                                    \
	_Pragma("clang diagnostic ignored \"-Wsign-conversion\"") \
		HEDLEY_PRAGMA(omp simd reduction(r)) HEDLEY_DIAGNOSTIC_POP
#else
#define SIMDE_VECTORIZE_REDUCTION(r) HEDLEY_PRAGMA(omp simd reduction(r))
#endif
#define SIMDE_VECTORIZE_ALIGNED(a) HEDLEY_PRAGMA(omp simd aligned(a))
#elif defined(SIMDE_ENABLE_CILKPLUS)
#define SIMDE_VECTORIZE HEDLEY_PRAGMA(simd)
#define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(simd vectorlength(l))
#define SIMDE_VECTORIZE_REDUCTION(r) HEDLEY_PRAGMA(simd reduction(r))
#define SIMDE_VECTORIZE_ALIGNED(a) HEDLEY_PRAGMA(simd aligned(a))
#elif defined(__clang__) && !defined(HEDLEY_IBM_VERSION)
#define SIMDE_VECTORIZE HEDLEY_PRAGMA(clang loop vectorize(enable))
#define SIMDE_VECTORIZE_SAFELEN(l) HEDLEY_PRAGMA(clang loop vectorize_width(l))
#define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_ALIGNED(a)
#elif HEDLEY_GCC_VERSION_CHECK(4, 9, 0)
#define SIMDE_VECTORIZE HEDLEY_PRAGMA(GCC ivdep)
#define SIMDE_VECTORIZE_SAFELEN(l) SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_ALIGNED(a)
#elif HEDLEY_CRAY_VERSION_CHECK(5, 0, 0)
#define SIMDE_VECTORIZE HEDLEY_PRAGMA(_CRI ivdep)
#define SIMDE_VECTORIZE_SAFELEN(l) SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_REDUCTION(r) SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_ALIGNED(a)
#else
#define SIMDE_VECTORIZE
#define SIMDE_VECTORIZE_SAFELEN(l)
#define SIMDE_VECTORIZE_REDUCTION(r)
#define SIMDE_VECTORIZE_ALIGNED(a)
#endif

#define SIMDE_MASK_NZ_(v, mask) (((v) & (mask)) | !((v) & (mask)))

/* Intended for checking coverage, you should never use this in
   production. */
#if defined(SIMDE_NO_INLINE)
#define SIMDE_FUNCTION_ATTRIBUTES HEDLEY_NEVER_INLINE static
#else
#define SIMDE_FUNCTION_ATTRIBUTES HEDLEY_ALWAYS_INLINE static
#endif

#if HEDLEY_HAS_ATTRIBUTE(unused) || HEDLEY_GCC_VERSION_CHECK(2, 95, 0)
#define SIMDE_FUNCTION_POSSIBLY_UNUSED_ __attribute__((__unused__))
#else
#define SIMDE_FUNCTION_POSSIBLY_UNUSED_
#endif

#if HEDLEY_HAS_WARNING("-Wused-but-marked-unused")
#define SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED \
	_Pragma("clang diagnostic ignored \"-Wused-but-marked-unused\"")
#else
#define SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED
#endif

#if defined(_MSC_VER)
#define SIMDE_BEGIN_DECLS_                                            \
	HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(disable : 4996 4204)) \
		HEDLEY_BEGIN_C_DECLS
#define SIMDE_END_DECLS_ HEDLEY_DIAGNOSTIC_POP HEDLEY_END_C_DECLS
#else
#define SIMDE_BEGIN_DECLS_                              \
	HEDLEY_DIAGNOSTIC_PUSH                          \
	SIMDE_DIAGNOSTIC_DISABLE_USED_BUT_MARKED_UNUSED \
	HEDLEY_BEGIN_C_DECLS
#define SIMDE_END_DECLS_   \
	HEDLEY_END_C_DECLS \
	HEDLEY_DIAGNOSTIC_POP
#endif

#if defined(__SIZEOF_INT128__)
#define SIMDE_HAVE_INT128_
HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_PEDANTIC_
typedef __int128 simde_int128;
typedef unsigned __int128 simde_uint128;
HEDLEY_DIAGNOSTIC_POP
#endif

#if !defined(SIMDE_ENDIAN_LITTLE)
#define SIMDE_ENDIAN_LITTLE 1234
#endif
#if !defined(SIMDE_ENDIAN_BIG)
#define SIMDE_ENDIAN_BIG 4321
#endif

#if !defined(SIMDE_ENDIAN_ORDER)
/* GCC (and compilers masquerading as GCC) define  __BYTE_ORDER__. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
	(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
	(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
/* TI defines _BIG_ENDIAN or _LITTLE_ENDIAN */
#elif defined(_BIG_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#elif defined(_LITTLE_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
/* We know the endianness of some common architectures.  Common
 * architectures not listed (ARM, POWER, MIPS, etc.) here are
 * bi-endian. */
#elif defined(__amd64) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(__s390x__) || defined(__zarch__)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
/* Looks like we'll have to rely on the platform.  If we're missing a
 * platform, please let us know. */
#elif defined(_WIN32)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(sun) || defined(__sun) /* Solaris */
#include <sys/byteorder.h>
#if defined(_LITTLE_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(_BIG_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#endif
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#if defined(__LITTLE_ENDIAN__)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(__BIG_ENDIAN__)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#endif
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
	defined(__bsdi__) || defined(__DragonFly__) || defined(BSD)
#include <machine/endian.h>
#if defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#endif
#elif defined(__linux__) || defined(__linux) || defined(__gnu_linux__)
#include <endian.h>
#if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
	(__BYTE_ORDER == __LITTLE_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
	(__BYTE_ORDER == __BIG_ENDIAN)
#define SIMDE_ENDIAN_ORDER SIMDE_ENDIAN_BIG
#endif
#endif
#endif

#if HEDLEY_HAS_BUILTIN(__builtin_bswap64) ||  \
	HEDLEY_GCC_VERSION_CHECK(4, 3, 0) ||  \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define simde_bswap64(v) __builtin_bswap64(v)
#elif HEDLEY_MSVC_VERSION_CHECK(13, 10, 0)
#define simde_bswap64(v) _byteswap_uint64(v)
#else
SIMDE_FUNCTION_ATTRIBUTES
uint64_t simde_bswap64(uint64_t v)
{
	return ((v & (((uint64_t)0xff) << 56)) >> 56) |
	       ((v & (((uint64_t)0xff) << 48)) >> 40) |
	       ((v & (((uint64_t)0xff) << 40)) >> 24) |
	       ((v & (((uint64_t)0xff) << 32)) >> 8) |
	       ((v & (((uint64_t)0xff) << 24)) << 8) |
	       ((v & (((uint64_t)0xff) << 16)) << 24) |
	       ((v & (((uint64_t)0xff) << 8)) << 40) |
	       ((v & (((uint64_t)0xff))) << 56);
}
#endif

#if !defined(SIMDE_ENDIAN_ORDER)
#error Unknown byte order; please file a bug
#else
#if SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_LITTLE
#define simde_endian_bswap64_be(value) simde_bswap64(value)
#define simde_endian_bswap64_le(value) (value)
#elif SIMDE_ENDIAN_ORDER == SIMDE_ENDIAN_BIG
#define simde_endian_bswap64_be(value) (value)
#define simde_endian_bswap64_le(value) simde_bswap64(value)
#endif
#endif

/* TODO: we should at least make an attempt to detect the correct
   types for simde_float32/float64 instead of just assuming float and
   double. */

#if !defined(SIMDE_FLOAT32_TYPE)
#define SIMDE_FLOAT32_TYPE float
#define SIMDE_FLOAT32_C(value) value##f
#else
#define SIMDE_FLOAT32_C(value) ((SIMDE_FLOAT32_TYPE)value)
#endif
typedef SIMDE_FLOAT32_TYPE simde_float32;

#if !defined(SIMDE_FLOAT64_TYPE)
#define SIMDE_FLOAT64_TYPE double
#define SIMDE_FLOAT64_C(value) value
#else
#define SIMDE_FLOAT32_C(value) ((SIMDE_FLOAT64_TYPE)value)
#endif
typedef SIMDE_FLOAT64_TYPE simde_float64;

#if HEDLEY_HAS_WARNING("-Wbad-function-cast")
#define SIMDE_CONVERT_FTOI(T, v)                                    \
	HEDLEY_DIAGNOSTIC_PUSH                                      \
	_Pragma("clang diagnostic ignored \"-Wbad-function-cast\"") \
		HEDLEY_STATIC_CAST(T, (v)) HEDLEY_DIAGNOSTIC_POP
#else
#define SIMDE_CONVERT_FTOI(T, v) ((T)(v))
#endif

/* TODO: detect compilers which support this outside of C11 mode */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define SIMDE_CHECKED_REINTERPRET_CAST(to, from, value) \
	_Generic((value), to                            \
		 : (value), default                     \
		 : (_Generic((value), from              \
			     : ((to)(value)))))
#define SIMDE_CHECKED_STATIC_CAST(to, from, value) \
	_Generic((value), to                       \
		 : (value), default                \
		 : (_Generic((value), from         \
			     : ((to)(value)))))
#else
#define SIMDE_CHECKED_REINTERPRET_CAST(to, from, value) \
	HEDLEY_REINTERPRET_CAST(to, value)
#define SIMDE_CHECKED_STATIC_CAST(to, from, value) HEDLEY_STATIC_CAST(to, value)
#endif

#if HEDLEY_HAS_WARNING("-Wfloat-equal")
#define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL \
	_Pragma("clang diagnostic ignored \"-Wfloat-equal\"")
#elif HEDLEY_GCC_VERSION_CHECK(3, 0, 0)
#define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL \
	_Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")
#else
#define SIMDE_DIAGNOSTIC_DISABLE_FLOAT_EQUAL
#endif

/* Some functions can trade accuracy for speed.  For those functions
   you can control the trade-off using this macro.  Possible values:

   0: prefer speed
   1: reasonable trade-offs
   2: prefer accuracy */
#if !defined(SIMDE_ACCURACY_PREFERENCE)
#define SIMDE_ACCURACY_PREFERENCE 1
#endif

#if defined(__STDC_HOSTED__)
#define SIMDE_STDC_HOSTED __STDC_HOSTED__
#else
#if defined(HEDLEY_PGI_VERSION) || defined(HEDLEY_MSVC_VERSION)
#define SIMDE_STDC_HOSTED 1
#else
#define SIMDE_STDC_HOSTED 0
#endif
#endif

/* Try to deal with environments without a standard library. */
#if !defined(simde_memcpy)
#if HEDLEY_HAS_BUILTIN(__builtin_memcpy)
#define simde_memcpy(dest, src, n) __builtin_memcpy(dest, src, n)
#endif
#endif
#if !defined(simde_memset)
#if HEDLEY_HAS_BUILTIN(__builtin_memset)
#define simde_memset(s, c, n) __builtin_memset(s, c, n)
#endif
#endif
#if !defined(simde_memcmp)
#if HEDLEY_HAS_BUILTIN(__builtin_memcmp)
#define simde_memcmp(s1, s2, n) __builtin_memcmp(s1, s2, n)
#endif
#endif

#if !defined(simde_memcpy) || !defined(simde_memset) || !defined(simde_memcmp)
#if !defined(SIMDE_NO_STRING_H)
#if defined(__has_include)
#if !__has_include(<string.h>)
#define SIMDE_NO_STRING_H
#endif
#elif (SIMDE_STDC_HOSTED == 0)
#define SIMDE_NO_STRING_H
#endif
#endif

#if !defined(SIMDE_NO_STRING_H)
#include <string.h>
#if !defined(simde_memcpy)
#define simde_memcpy(dest, src, n) memcpy(dest, src, n)
#endif
#if !defined(simde_memset)
#define simde_memset(s, c, n) memset(s, c, n)
#endif
#if !defined(simde_memcmp)
#define simde_memcmp(s1, s2, n) memcmp(s1, s2, n)
#endif
#else
/* These are meant to be portable, not fast.  If you're hitting them you
     * should think about providing your own (by defining the simde_memcpy
     * macro prior to including any SIMDe files) or submitting a patch to
     * SIMDe so we can detect your system-provided memcpy/memset, like by
     * adding your compiler to the checks for __builtin_memcpy and/or
     * __builtin_memset. */
#if !defined(simde_memcpy)
SIMDE_FUNCTION_ATTRIBUTES
void simde_memcpy_(void *dest, const void *src, size_t len)
{
	char *dest_ = HEDLEY_STATIC_CAST(char *, dest);
	char *src_ = HEDLEY_STATIC_CAST(const char *, src);
	for (size_t i = 0; i < len; i++) {
		dest_[i] = src_[i];
	}
}
#define simde_memcpy(dest, src, n) simde_memcpy_(dest, src, n)
#endif

#if !defined(simde_memset)
SIMDE_FUNCTION_ATTRIBUTES
void simde_memset_(void *s, int c, size_t len)
{
	char *s_ = HEDLEY_STATIC_CAST(char *, s);
	char c_ = HEDLEY_STATIC_CAST(char, c);
	for (size_t i = 0; i < len; i++) {
		s_[i] = c_[i];
	}
}
#define simde_memset(s, c, n) simde_memset_(s, c, n)
#endif

#if !defined(simde_memcmp)
SIMDE_FUCTION_ATTRIBUTES
int simde_memcmp_(const void *s1, const void *s2, size_t n)
{
	unsigned char *s1_ = HEDLEY_STATIC_CAST(unsigned char *, s1);
	unsigned char *s2_ = HEDLEY_STATIC_CAST(unsigned char *, s2);
	for (size_t i = 0; i < len; i++) {
		if (s1_[i] != s2_[i]) {
			return (int)(s1_[i] - s2_[i]);
		}
	}
	return 0;
}
#define simde_memcmp(s1, s2, n) simde_memcmp_(s1, s2, n)
#endif
#endif
#endif

#if defined(FE_ALL_EXCEPT)
#define SIMDE_HAVE_FENV_H
#elif defined(__has_include)
#if __has_include(<fenv.h>)
#include <fenv.h>
#define SIMDE_HAVE_FENV_H
#endif
#elif SIMDE_STDC_HOSTED == 1
#include <fenv.h>
#define SIMDE_HAVE_FENV_H
#endif

#if defined(EXIT_FAILURE)
#define SIMDE_HAVE_STDLIB_H
#elif defined(__has_include)
#if __has_include(<stdlib.h>)
#include <stdlib.h>
#define SIMDE_HAVE_STDLIB_H
#endif
#elif SIMDE_STDC_HOSTED == 1
#include <stdlib.h>
#define SIMDE_HAVE_STDLIB_H
#endif

#if defined(__has_include)
#if defined(__cplusplus) && (__cplusplus >= 201103L) && __has_include(<cfenv>)
#include <cfenv>
#elif __has_include(<fenv.h>)
#include <fenv.h>
#endif
#if __has_include(<stdlib.h>)
#include <stdlib.h>
#endif
#elif SIMDE_STDC_HOSTED == 1
#include <stdlib.h>
#include <fenv.h>
#endif

#include "check.h"

/* GCC/clang have a bunch of functionality in builtins which we would
 * like to access, but the suffixes indicate whether the operate on
 * int, long, or long long, not fixed width types (e.g., int32_t).
 * we use these macros to attempt to map from fixed-width to the
 * names GCC uses.  Note that you should still cast the input(s) and
 * return values (to/from SIMDE_BUILTIN_TYPE_*_) since often even if
 * types are the same size they may not be compatible according to the
 * compiler.  For example, on x86 long and long lonsg are generally
 * both 64 bits, but platforms vary on whether an int64_t is mapped
 * to a long or long long. */

#include <limits.h>

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_

#if (INT8_MAX == INT_MAX) && (INT8_MIN == INT_MIN)
#define SIMDE_BUILTIN_SUFFIX_8_
#define SIMDE_BUILTIN_TYPE_8_ int
#elif (INT8_MAX == LONG_MAX) && (INT8_MIN == LONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_8_ l
#define SIMDE_BUILTIN_TYPE_8_ long
#elif (INT8_MAX == LLONG_MAX) && (INT8_MIN == LLONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_8_ ll
#define SIMDE_BUILTIN_TYPE_8_ long long
#endif

#if (INT16_MAX == INT_MAX) && (INT16_MIN == INT_MIN)
#define SIMDE_BUILTIN_SUFFIX_16_
#define SIMDE_BUILTIN_TYPE_16_ int
#elif (INT16_MAX == LONG_MAX) && (INT16_MIN == LONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_16_ l
#define SIMDE_BUILTIN_TYPE_16_ long
#elif (INT16_MAX == LLONG_MAX) && (INT16_MIN == LLONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_16_ ll
#define SIMDE_BUILTIN_TYPE_16_ long long
#endif

#if (INT32_MAX == INT_MAX) && (INT32_MIN == INT_MIN)
#define SIMDE_BUILTIN_SUFFIX_32_
#define SIMDE_BUILTIN_TYPE_32_ int
#elif (INT32_MAX == LONG_MAX) && (INT32_MIN == LONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_32_ l
#define SIMDE_BUILTIN_TYPE_32_ long
#elif (INT32_MAX == LLONG_MAX) && (INT32_MIN == LLONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_32_ ll
#define SIMDE_BUILTIN_TYPE_32_ long long
#endif

#if (INT64_MAX == INT_MAX) && (INT64_MIN == INT_MIN)
#define SIMDE_BUILTIN_SUFFIX_64_
#define SIMDE_BUILTIN_TYPE_64_ int
#elif (INT64_MAX == LONG_MAX) && (INT64_MIN == LONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_64_ l
#define SIMDE_BUILTIN_TYPE_64_ long
#elif (INT64_MAX == LLONG_MAX) && (INT64_MIN == LLONG_MIN)
#define SIMDE_BUILTIN_SUFFIX_64_ ll
#define SIMDE_BUILTIN_TYPE_64_ long long
#endif

#if defined(SIMDE_BUILTIN_SUFFIX_8_)
#define SIMDE_BUILTIN_8_(name) \
	HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_8_)
#define SIMDE_BUILTIN_HAS_8_(name) \
	HEDLEY_HAS_BUILTIN(        \
		HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_8_))
#else
#define SIMDE_BUILTIN_HAS_8_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_16_)
#define SIMDE_BUILTIN_16_(name) \
	HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_16_)
#define SIMDE_BUILTIN_HAS_16_(name) \
	HEDLEY_HAS_BUILTIN(         \
		HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_16_))
#else
#define SIMDE_BUILTIN_HAS_16_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_32_)
#define SIMDE_BUILTIN_32_(name) \
	HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_32_)
#define SIMDE_BUILTIN_HAS_32_(name) \
	HEDLEY_HAS_BUILTIN(         \
		HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_32_))
#else
#define SIMDE_BUILTIN_HAS_32_(name) 0
#endif
#if defined(SIMDE_BUILTIN_SUFFIX_64_)
#define SIMDE_BUILTIN_64_(name) \
	HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_64_)
#define SIMDE_BUILTIN_HAS_64_(name) \
	HEDLEY_HAS_BUILTIN(         \
		HEDLEY_CONCAT3(__builtin_, name, SIMDE_BUILTIN_SUFFIX_64_))
#else
#define SIMDE_BUILTIN_HAS_64_(name) 0
#endif

HEDLEY_DIAGNOSTIC_POP

/* Sometimes we run into problems with specific versions of compilers
   which make the native versions unusable for us.  Often this is due
   to missing functions, sometimes buggy implementations, etc.  These
   macros are how we check for specific bugs.  As they are fixed we'll
   start only defining them for problematic compiler versions. */

#if !defined(SIMDE_IGNORE_COMPILER_BUGS)
#if defined(HEDLEY_GCC_VERSION)
#if !HEDLEY_GCC_VERSION_CHECK(4, 9, 0)
#define SIMDE_BUG_GCC_REV_208793
#endif
#if !HEDLEY_GCC_VERSION_CHECK(5, 0, 0)
#define SIMDE_BUG_GCC_BAD_MM_SRA_EPI32 /* TODO: find relevant bug or commit */
#endif
#if !HEDLEY_GCC_VERSION_CHECK(4, 6, 0)
#define SIMDE_BUG_GCC_BAD_MM_EXTRACT_EPI8 /* TODO: find relevant bug or commit */
#endif
#if !HEDLEY_GCC_VERSION_CHECK(8, 0, 0)
#define SIMDE_BUG_GCC_REV_247851
#endif
#if !HEDLEY_GCC_VERSION_CHECK(10, 0, 0)
#define SIMDE_BUG_GCC_REV_274313
#define SIMDE_BUG_GCC_91341
#endif
#if !HEDLEY_GCC_VERSION_CHECK(9, 0, 0) && defined(SIMDE_ARCH_AARCH64)
#define SIMDE_BUG_GCC_ARM_SHIFT_SCALAR
#endif
#if defined(SIMDE_ARCH_X86) && !defined(SIMDE_ARCH_AMD64)
#define SIMDE_BUG_GCC_94482
#endif
#if (defined(SIMDE_ARCH_X86) && !defined(SIMDE_ARCH_AMD64)) || \
	defined(SIMDE_ARCH_SYSTEMZ)
#define SIMDE_BUG_GCC_53784
#endif
#if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#if HEDLEY_GCC_VERSION_CHECK(4, 3, 0) /* -Wsign-conversion */
#define SIMDE_BUG_GCC_95144
#endif
#endif
#if !HEDLEY_GCC_VERSION_CHECK(9, 4, 0) && defined(SIMDE_ARCH_AARCH64)
#define SIMDE_BUG_GCC_94488
#endif
#if defined(SIMDE_ARCH_ARM)
#define SIMDE_BUG_GCC_95399
#define SIMDE_BUG_GCC_95471
#elif defined(SIMDE_ARCH_POWER)
#define SIMDE_BUG_GCC_95227
#define SIMDE_BUG_GCC_95782
#elif defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#if !HEDLEY_GCC_VERSION_CHECK(10, 2, 0) && !defined(__OPTIMIZE__)
#define SIMDE_BUG_GCC_96174
#endif
#endif
#define SIMDE_BUG_GCC_95399
#elif defined(__clang__)
#if defined(SIMDE_ARCH_AARCH64)
#define SIMDE_BUG_CLANG_45541
#define SIMDE_BUG_CLANG_46844
#define SIMDE_BUG_CLANG_48257
#if SIMDE_DETECT_CLANG_VERSION_CHECK(10, 0, 0) && \
	SIMDE_DETECT_CLANG_VERSION_NOT(11, 0, 0)
#define SIMDE_BUG_CLANG_BAD_VI64_OPS
#endif
#endif
#if defined(SIMDE_ARCH_POWER)
#define SIMDE_BUG_CLANG_46770
#endif
#if defined(_ARCH_PWR9) && !SIMDE_DETECT_CLANG_VERSION_CHECK(12, 0, 0) && \
	!defined(__OPTIMIZE__)
#define SIMDE_BUG_CLANG_POWER9_16x4_BAD_SHIFT
#endif
#if defined(SIMDE_ARCH_X86) || defined(SIMDE_ARCH_AMD64)
#if HEDLEY_HAS_WARNING("-Wsign-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(11, 0, 0)
#define SIMDE_BUG_CLANG_45931
#endif
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(11, 0, 0)
#define SIMDE_BUG_CLANG_44589
#endif
#endif
#define SIMDE_BUG_CLANG_45959
#elif defined(HEDLEY_MSVC_VERSION)
#if defined(SIMDE_ARCH_X86)
#define SIMDE_BUG_MSVC_ROUND_EXTRACT
#endif
#elif defined(HEDLEY_INTEL_VERSION)
#define SIMDE_BUG_INTEL_857088
#endif
#endif

/* GCC and Clang both have the same issue:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95144
 * https://bugs.llvm.org/show_bug.cgi?id=45931
 * This is just an easy way to work around it.
 */
#if (HEDLEY_HAS_WARNING("-Wsign-conversion") &&   \
     SIMDE_DETECT_CLANG_VERSION_NOT(11, 0, 0)) || \
	HEDLEY_GCC_VERSION_CHECK(4, 3, 0)
#define SIMDE_BUG_IGNORE_SIGN_CONVERSION(expr)                                      \
	(__extension__({                                                            \
		HEDLEY_DIAGNOSTIC_PUSH                                              \
		HEDLEY_DIAGNOSTIC_POP                                               \
		_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") __typeof__( \
			expr) simde_bug_ignore_sign_conversion_v_ = (expr);         \
		HEDLEY_DIAGNOSTIC_PUSH                                              \
		simde_bug_ignore_sign_conversion_v_;                                \
	}))
#else
#define SIMDE_BUG_IGNORE_SIGN_CONVERSION(expr) (expr)
#endif

#endif /* !defined(SIMDE_COMMON_H) */
