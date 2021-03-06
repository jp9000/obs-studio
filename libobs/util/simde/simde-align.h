/* Alignment
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 **********************************************************************
 *
 * This is portability layer which should help iron out some
 * differences across various compilers, as well as various verisons of
 * C and C++.
 *
 * It was originally developed for SIMD Everywhere
 * (<https://github.com/simd-everywhere/simde>), but since its only
 * dependency is Hedley (<https://nemequ.github.io/hedley>, also CC0)
 * it can easily be used in other projects, so please feel free to do
 * so.
 *
 * If you do use this in your project, please keep a link to SIMDe in
 * your code to remind you where to report any bugs and/or check for
 * updated versions.
 *
 * # API Overview
 *
 * The API has several parts, and most macros have a few variations.
 * There are APIs for declaring aligned fields/variables, optimization
 * hints, and run-time alignment checks.
 *
 * Briefly, macros ending with "_TO" take numeric values and are great
 * when you know the value you would like to use.  Macros ending with
 * "_LIKE", on the other hand, accept a type and are used when you want
 * to use the alignment of a type instead of hardcoding a value.
 *
 * Documentation for each section of the API is inline.
 *
 * True to form, MSVC is the main problem and imposes several
 * limitations on the effectiveness of the APIs.  Detailed descriptions
 * of the limitations of each macro are inline, but in general:
 *
 *  * On C11+ or C++11+ code written using this API will work.  The
 *    ASSUME macros may or may not generate a hint to the compiler, but
 *    that is only an optimization issue and will not actually cause
 *    failures.
 *  * If you're using pretty much any compiler other than MSVC,
 *    everything should basically work as well as in C11/C++11.
 */

#if !defined(SIMDE_ALIGN_H)
#define SIMDE_ALIGN_H

#include "hedley.h"

/* I know this seems a little silly, but some non-hosted compilers
 * don't have stddef.h, so we try to accomodate them. */
#if !defined(SIMDE_ALIGN_SIZE_T_)
#if defined(__SIZE_TYPE__)
#define SIMDE_ALIGN_SIZE_T_ __SIZE_TYPE__
#elif defined(__SIZE_T_TYPE__)
#define SIMDE_ALIGN_SIZE_T_ __SIZE_TYPE__
#elif defined(__cplusplus)
#include <cstddef>
#define SIMDE_ALIGN_SIZE_T_ size_t
#else
#include <stddef.h>
#define SIMDE_ALIGN_SIZE_T_ size_t
#endif
#endif

#if !defined(SIMDE_ALIGN_INTPTR_T_)
#if defined(__INTPTR_TYPE__)
#define SIMDE_ALIGN_INTPTR_T_ __INTPTR_TYPE__
#elif defined(__PTRDIFF_TYPE__)
#define SIMDE_ALIGN_INTPTR_T_ __PTRDIFF_TYPE__
#elif defined(__PTRDIFF_T_TYPE__)
#define SIMDE_ALIGN_INTPTR_T_ __PTRDIFF_T_TYPE__
#elif defined(__cplusplus)
#include <cstddef>
#define SIMDE_ALIGN_INTPTR_T_ ptrdiff_t
#else
#include <stddef.h>
#define SIMDE_ALIGN_INTPTR_T_ ptrdiff_t
#endif
#endif

#if defined(SIMDE_ALIGN_DEBUG)
#if defined(__cplusplus)
#include <cstdio>
#else
#include <stdio.h>
#endif
#endif

/* SIMDE_ALIGN_OF(Type)
 *
 * The SIMDE_ALIGN_OF macro works like alignof, or _Alignof, or
 * __alignof, or __alignof__, or __ALIGNOF__, depending on the compiler.
 * It isn't defined everywhere (only when the compiler has some alignof-
 * like feature we can use to implement it), but it should work in most
 * modern compilers, as well as C11 and C++11.
 *
 * If we can't find an implementation for SIMDE_ALIGN_OF then the macro
 * will not be defined, so if you can handle that situation sensibly
 * you may need to sprinkle some ifdefs into your code.
 */
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
	(0 && HEDLEY_HAS_FEATURE(c_alignof))
#define SIMDE_ALIGN_OF(Type) _Alignof(Type)
#elif (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
	(0 && HEDLEY_HAS_FEATURE(cxx_alignof))
#define SIMDE_ALIGN_OF(Type) alignof(Type)
#elif HEDLEY_GCC_VERSION_CHECK(2, 95, 0) ||                                    \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                                   \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                                \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 13, 0) ||                               \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 24) ||                                \
	HEDLEY_PGI_VERSION_CHECK(19, 10, 0) ||                                 \
	HEDLEY_CRAY_VERSION_CHECK(10, 0, 0) ||                                 \
	HEDLEY_TI_ARMCL_VERSION_CHECK(16, 9, 0) ||                             \
	HEDLEY_TI_CL2000_VERSION_CHECK(16, 9, 0) ||                            \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 0, 0) ||                               \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                               \
	HEDLEY_TI_CL430_VERSION_CHECK(16, 9, 0) ||                             \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 3, 2) || defined(__IBM__ALIGNOF__) || \
	defined(__clang__)
#define SIMDE_ALIGN_OF(Type) __alignof__(Type)
#elif HEDLEY_IAR_VERSION_CHECK(8, 40, 0)
#define SIMDE_ALIGN_OF(Type) __ALIGNOF__(Type)
#elif HEDLEY_MSVC_VERSION_CHECK(19, 0, 0)
/* Probably goes back much further, but MS takes down their old docs.
   * If you can verify that this works in earlier versions please let
   * me know! */
#define SIMDE_ALIGN_OF(Type) __alignof(Type)
#endif

/* SIMDE_ALIGN_MAXIMUM:
 *
 * This is the maximum alignment that the compiler supports.  You can
 * define the value prior to including SIMDe if necessary, but in that
 * case *please* submit an issue so we can add the platform to the
 * detection code.
 *
 * Most compilers are okay with types which are aligned beyond what
 * they think is the maximum, as long as the alignment is a power
 * of two.  MSVC is the exception (of course), so we need to cap the
 * alignment requests at values that the implementation supports.
 *
 * XL C/C++ will accept values larger than 16 (which is the alignment
 * of an AltiVec vector), but will not reliably align to the larger
 * value, so so we cap the value at 16 there.
 *
 * If the compiler accepts any power-of-two value within reason then
 * this macro should be left undefined, and the SIMDE_ALIGN_CAP
 * macro will just return the value passed to it. */
#if !defined(SIMDE_ALIGN_MAXIMUM)
#if defined(HEDLEY_MSVC_VERSION)
#if defined(_M_IX86) || defined(_M_AMD64)
#if HEDLEY_MSVC_VERSION_CHECK(19, 14, 0)
#define SIMDE_ALIGN_PLATFORM_MAXIMUM 64
#elif HEDLEY_MSVC_VERSION_CHECK(16, 0, 0)
/* VS 2010 is really a guess based on Wikipedia; if anyone can
         * test with old VS versions I'd really appreciate it. */
#define SIMDE_ALIGN_PLATFORM_MAXIMUM 32
#else
#define SIMDE_ALIGN_PLATFORM_MAXIMUM 16
#endif
#elif defined(_M_ARM) || defined(_M_ARM64)
#define SIMDE_ALIGN_PLATFORM_MAXIMUM 8
#endif
#elif defined(HEDLEY_IBM_VERSION)
#define SIMDE_ALIGN_PLATFORM_MAXIMUM 16
#endif
#endif

/* You can mostly ignore these; they're intended for internal use.
 * If you do need to use them please let me know; if they fulfill
 * a common use case I'll probably drop the trailing underscore
 * and make them part of the public API. */
#if defined(SIMDE_ALIGN_PLATFORM_MAXIMUM)
#if SIMDE_ALIGN_PLATFORM_MAXIMUM >= 64
#define SIMDE_ALIGN_64_ 64
#define SIMDE_ALIGN_32_ 32
#define SIMDE_ALIGN_16_ 16
#define SIMDE_ALIGN_8_ 8
#elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 32
#define SIMDE_ALIGN_64_ 32
#define SIMDE_ALIGN_32_ 32
#define SIMDE_ALIGN_16_ 16
#define SIMDE_ALIGN_8_ 8
#elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 16
#define SIMDE_ALIGN_64_ 16
#define SIMDE_ALIGN_32_ 16
#define SIMDE_ALIGN_16_ 16
#define SIMDE_ALIGN_8_ 8
#elif SIMDE_ALIGN_PLATFORM_MAXIMUM >= 8
#define SIMDE_ALIGN_64_ 8
#define SIMDE_ALIGN_32_ 8
#define SIMDE_ALIGN_16_ 8
#define SIMDE_ALIGN_8_ 8
#else
#error Max alignment expected to be >= 8
#endif
#else
#define SIMDE_ALIGN_64_ 64
#define SIMDE_ALIGN_32_ 32
#define SIMDE_ALIGN_16_ 16
#define SIMDE_ALIGN_8_ 8
#endif

/**
 * SIMDE_ALIGN_CAP(Alignment)
 *
 * Returns the minimum of Alignment or SIMDE_ALIGN_MAXIMUM.
 */
#if defined(SIMDE_ALIGN_MAXIMUM)
#define SIMDE_ALIGN_CAP(Alignment)                      \
	(((Alignment) < (SIMDE_ALIGN_PLATFORM_MAXIMUM)) \
		 ? (Alignment)                          \
		 : (SIMDE_ALIGN_PLATFORM_MAXIMUM))
#else
#define SIMDE_ALIGN_CAP(Alignment) (Alignment)
#endif

/* SIMDE_ALIGN_TO(Alignment)
 *
 * SIMDE_ALIGN_TO is used to declare types or variables.  It basically
 * maps to the align attribute in most compilers, the align declspec
 * in MSVC, or _Alignas/alignas in C11/C++11.
 *
 * Example:
 *
 *   struct i32x4 {
 *     SIMDE_ALIGN_TO(16) int32_t values[4];
 *   }
 *
 * Limitations:
 *
 * MSVC requires that the Alignment parameter be numeric; you can't do
 * something like `SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(int))`.  This is
 * unfortunate because that's really how the LIKE macros are
 * implemented, and I am not aware of a way to get anything like this
 * to work without using the C11/C++11 keywords.
 *
 * It also means that we can't use SIMDE_ALIGN_CAP to limit the
 * alignment to the value specified, which MSVC also requires, so on
 * MSVC you should use the `SIMDE_ALIGN_TO_8/16/32/64` macros instead.
 * They work like `SIMDE_ALIGN_TO(SIMDE_ALIGN_CAP(Alignment))` would,
 * but should be safe to use on MSVC.
 *
 * All this is to say that, if you want your code to work on MSVC, you
 * should use the SIMDE_ALIGN_TO_8/16/32/64 macros below instead of
 * SIMDE_ALIGN_TO(8/16/32/64).
 */
#if HEDLEY_HAS_ATTRIBUTE(aligned) || HEDLEY_GCC_VERSION_CHECK(2, 95, 0) || \
	HEDLEY_CRAY_VERSION_CHECK(8, 4, 0) ||                              \
	HEDLEY_IBM_VERSION_CHECK(11, 1, 0) ||                              \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_PGI_VERSION_CHECK(19, 4, 0) ||                              \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                               \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 24) ||                            \
	HEDLEY_TI_ARMCL_VERSION_CHECK(16, 9, 0) ||                         \
	HEDLEY_TI_CL2000_VERSION_CHECK(16, 9, 0) ||                        \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 0, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	HEDLEY_TI_CL430_VERSION_CHECK(16, 9, 0) ||                         \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 3, 2)
#define SIMDE_ALIGN_TO(Alignment) \
	__attribute__((__aligned__(SIMDE_ALIGN_CAP(Alignment))))
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))
#define SIMDE_ALIGN_TO(Alignment) _Alignas(SIMDE_ALIGN_CAP(Alignment))
#elif (defined(__cplusplus) && (__cplusplus >= 201103L))
#define SIMDE_ALIGN_TO(Alignment) alignas(SIMDE_ALIGN_CAP(Alignment))
#elif defined(HEDLEY_MSVC_VERSION)
#define SIMDE_ALIGN_TO(Alignment) __declspec(align(Alignment))
/* Unfortunately MSVC can't handle __declspec(align(__alignof(Type)));
   * the alignment passed to the declspec has to be an integer. */
#define SIMDE_ALIGN_OF_UNUSABLE_FOR_LIKE
#endif
#define SIMDE_ALIGN_TO_64 SIMDE_ALIGN_TO(SIMDE_ALIGN_64_)
#define SIMDE_ALIGN_TO_32 SIMDE_ALIGN_TO(SIMDE_ALIGN_32_)
#define SIMDE_ALIGN_TO_16 SIMDE_ALIGN_TO(SIMDE_ALIGN_16_)
#define SIMDE_ALIGN_TO_8 SIMDE_ALIGN_TO(SIMDE_ALIGN_8_)

/* SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment)
 *
 * SIMDE_ALIGN_ASSUME_TO is semantically similar to C++20's
 * std::assume_aligned, or __builtin_assume_aligned.  It tells the
 * compiler to assume that the provided pointer is aligned to an
 * `Alignment`-byte boundary.
 *
 * If you define SIMDE_ALIGN_DEBUG prior to including this header then
 * SIMDE_ALIGN_ASSUME_TO will turn into a runtime check.   We don't
 * integrate with NDEBUG in this header, but it may be a good idea to
 * put something like this in your code:
 *
 *   #if !defined(NDEBUG)
 *     #define SIMDE_ALIGN_DEBUG
 *   #endif
 *   #include <.../simde-align.h>
 */
#if HEDLEY_HAS_BUILTIN(__builtin_assume_aligned) || \
	HEDLEY_GCC_VERSION_CHECK(4, 7, 0)
#define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment)                   \
	HEDLEY_REINTERPRET_CAST(                                              \
		__typeof__(Pointer),                                          \
		__builtin_assume_aligned(                                     \
			HEDLEY_CONST_CAST(                                    \
				void *, HEDLEY_REINTERPRET_CAST(const void *, \
								Pointer)),    \
			Alignment))
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment)           \
	(__extension__({                                              \
		__typeof__(v) simde_assume_aligned_t_ = (Pointer);    \
		__assume_aligned(simde_assume_aligned_t_, Alignment); \
		simde_assume_aligned_t_;                              \
	}))
#elif defined(__cplusplus) && (__cplusplus > 201703L)
#include <memory>
#define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) \
	std::assume_aligned<Alignment>(Pointer)
#else
#if defined(__cplusplus)
template<typename T>
HEDLEY_ALWAYS_INLINE static T *
simde_align_assume_to_unchecked(T *ptr, const size_t alignment)
#else
HEDLEY_ALWAYS_INLINE static void *
simde_align_assume_to_unchecked(void *ptr, const size_t alignment)
#endif
{
	HEDLEY_ASSUME((HEDLEY_REINTERPRET_CAST(size_t, (ptr)) %
		       SIMDE_ALIGN_CAP(alignment)) == 0);
	return ptr;
}
#if defined(__cplusplus)
#define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment) \
	simde_align_assume_to_unchecked((Pointer), (Alignment))
#else
#define SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment)                \
	simde_align_assume_to_unchecked(                                   \
		HEDLEY_CONST_CAST(void *, HEDLEY_REINTERPRET_CAST(         \
						  const void *, Pointer)), \
		(Alignment))
#endif
#endif

#if !defined(SIMDE_ALIGN_DEBUG)
#define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment) \
	SIMDE_ALIGN_ASSUME_TO_UNCHECKED(Pointer, Alignment)
#else
#include <stdio.h>
#if defined(__cplusplus)
template<typename T>
static HEDLEY_ALWAYS_INLINE T *
simde_align_assume_to_checked_uncapped(T *ptr, const size_t alignment,
				       const char *file, int line,
				       const char *ptrname)
#else
static HEDLEY_ALWAYS_INLINE void *
simde_align_assume_to_checked_uncapped(void *ptr, const size_t alignment,
				       const char *file, int line,
				       const char *ptrname)
#endif
{
	if (HEDLEY_UNLIKELY(
		    (HEDLEY_REINTERPRET_CAST(SIMDE_ALIGN_INTPTR_T_, (ptr)) %
		     HEDLEY_STATIC_CAST(SIMDE_ALIGN_INTPTR_T_,
					SIMDE_ALIGN_CAP(alignment))) != 0)) {
		fprintf(stderr,
			"%s:%d: alignment check failed for `%s' (%p %% %u == %u)\n",
			file, line, ptrname,
			HEDLEY_REINTERPRET_CAST(const void *, ptr),
			HEDLEY_STATIC_CAST(unsigned int,
					   SIMDE_ALIGN_CAP(alignment)),
			HEDLEY_STATIC_CAST(
				unsigned int,
				HEDLEY_REINTERPRET_CAST(SIMDE_ALIGN_INTPTR_T_,
							(ptr)) %
					HEDLEY_STATIC_CAST(
						SIMDE_ALIGN_INTPTR_T_,
						SIMDE_ALIGN_CAP(alignment))));
	}

	return ptr;
}

#if defined(__cplusplus)
#define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment)                      \
	simde_align_assume_to_checked_uncapped((Pointer), (Alignment), \
					       __FILE__, __LINE__, #Pointer)
#else
#define SIMDE_ALIGN_ASSUME_TO(Pointer, Alignment)                          \
	simde_align_assume_to_checked_uncapped(                            \
		HEDLEY_CONST_CAST(void *, HEDLEY_REINTERPRET_CAST(         \
						  const void *, Pointer)), \
		(Alignment), __FILE__, __LINE__, #Pointer)
#endif
#endif

/* SIMDE_ALIGN_LIKE(Type)
 * SIMDE_ALIGN_LIKE_#(Type)
 *
 * The SIMDE_ALIGN_LIKE macros are similar to the SIMDE_ALIGN_TO macros
 * except instead of an integer they take a type; basically, it's just
 * a more convenient way to do something like:
 *
 *   SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(Type))
 *
 * The versions with a numeric suffix will fall back on using a numeric
 * value in the event we can't use SIMDE_ALIGN_OF(Type).  This is
 * mainly for MSVC, where __declspec(align()) can't handle anything
 * other than hard-coded numeric values.
 */
#if defined(SIMDE_ALIGN_OF) && defined(SIMDE_ALIGN_TO) && \
	!defined(SIMDE_ALIGN_OF_UNUSABLE_FOR_LIKE)
#define SIMDE_ALIGN_LIKE(Type) SIMDE_ALIGN_TO(SIMDE_ALIGN_OF(Type))
#define SIMDE_ALIGN_LIKE_64(Type) SIMDE_ALIGN_LIKE(Type)
#define SIMDE_ALIGN_LIKE_32(Type) SIMDE_ALIGN_LIKE(Type)
#define SIMDE_ALIGN_LIKE_16(Type) SIMDE_ALIGN_LIKE(Type)
#define SIMDE_ALIGN_LIKE_8(Type) SIMDE_ALIGN_LIKE(Type)
#else
#define SIMDE_ALIGN_LIKE_64(Type) SIMDE_ALIGN_TO_64
#define SIMDE_ALIGN_LIKE_32(Type) SIMDE_ALIGN_TO_32
#define SIMDE_ALIGN_LIKE_16(Type) SIMDE_ALIGN_TO_16
#define SIMDE_ALIGN_LIKE_8(Type) SIMDE_ALIGN_TO_8
#endif

/* SIMDE_ALIGN_ASSUME_LIKE(Pointer, Type)
 *
 * Tihs is similar to SIMDE_ALIGN_ASSUME_TO, except that it takes a
 * type instead of a numeric value. */
#if defined(SIMDE_ALIGN_OF) && defined(SIMDE_ALIGN_ASSUME_TO)
#define SIMDE_ALIGN_ASSUME_LIKE(Pointer, Type) \
	SIMDE_ALIGN_ASSUME_TO(Pointer, SIMDE_ALIGN_OF(Type))
#endif

/* SIMDE_ALIGN_CAST(Type, Pointer)
 *
 * SIMDE_ALIGN_CAST is like C++'s reinterpret_cast, but it will try
 * to silence warnings that some compilers may produce if you try
 * to assign to a type with increased alignment requirements.
 *
 * Note that it does *not* actually attempt to tell the compiler that
 * the pointer is aligned like the destination should be; that's the
 * job of the next macro.  This macro is necessary for stupid APIs
 * like _mm_loadu_si128 where the input is a __m128i* but the function
 * is specifically for data which isn't necessarily aligned to
 * _Alignof(__m128i).
 */
#if HEDLEY_HAS_WARNING("-Wcast-align") || defined(__clang__) || \
	HEDLEY_GCC_VERSION_CHECK(3, 4, 0)
#define SIMDE_ALIGN_CAST(Type, Pointer)                                 \
	(__extension__({                                                \
		HEDLEY_DIAGNOSTIC_PUSH                                  \
		_Pragma("GCC diagnostic ignored \"-Wcast-align\"")      \
			Type simde_r_ =                                 \
				HEDLEY_REINTERPRET_CAST(Type, Pointer); \
		HEDLEY_DIAGNOSTIC_POP                                   \
		simde_r_;                                               \
	}))
#else
#define SIMDE_ALIGN_CAST(Type, Pointer) HEDLEY_REINTERPRET_CAST(Type, Pointer)
#endif

/* SIMDE_ALIGN_ASSUME_CAST(Type, Pointer)
 *
 * This is sort of like a combination of a reinterpret_cast and a
 * SIMDE_ALIGN_ASSUME_LIKE.  It uses SIMDE_ALIGN_ASSUME_LIKE to tell
 * the compiler that the pointer is aligned like the specified type
 * and casts the pointer to the specified type while suppressing any
 * warnings from the compiler about casting to a type with greater
 * alignment requirements.
 */
#define SIMDE_ALIGN_ASSUME_CAST(Type, Pointer) \
	SIMDE_ALIGN_ASSUME_LIKE(SIMDE_ALIGN_CAST(Type, Pointer), Type)

#endif /* !defined(SIMDE_ALIGN_H) */
