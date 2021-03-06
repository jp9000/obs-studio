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

#if !defined(SIMDE_X86_MMX_H)
#define SIMDE_X86_MMX_H

#include "../simde-common.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS

#if defined(SIMDE_X86_MMX_NATIVE)
#define SIMDE_X86_MMX_USE_NATIVE_TYPE
#elif defined(SIMDE_X86_SSE_NATIVE)
#define SIMDE_X86_MMX_USE_NATIVE_TYPE
#endif

#if defined(SIMDE_X86_MMX_USE_NATIVE_TYPE)
#include <mmintrin.h>
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
#include <arm_neon.h>
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
#include <loongson-mmiintrin.h>
#endif

#include <stdint.h>
#include <limits.h>

SIMDE_BEGIN_DECLS_

typedef union {
#if defined(SIMDE_VECTOR_SUBSCRIPT)
	SIMDE_ALIGN_TO_8 int8_t i8 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 int16_t i16 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 int32_t i32 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 int64_t i64 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 uint8_t u8 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 uint16_t u16 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 uint32_t u32 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 uint64_t u64 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 simde_float32 f32 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 int_fast32_t i32f SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
	SIMDE_ALIGN_TO_8 uint_fast32_t u32f SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
#else
	SIMDE_ALIGN_TO_8 int8_t i8[8];
	SIMDE_ALIGN_TO_8 int16_t i16[4];
	SIMDE_ALIGN_TO_8 int32_t i32[2];
	SIMDE_ALIGN_TO_8 int64_t i64[1];
	SIMDE_ALIGN_TO_8 uint8_t u8[8];
	SIMDE_ALIGN_TO_8 uint16_t u16[4];
	SIMDE_ALIGN_TO_8 uint32_t u32[2];
	SIMDE_ALIGN_TO_8 uint64_t u64[1];
	SIMDE_ALIGN_TO_8 simde_float32 f32[2];
	SIMDE_ALIGN_TO_8 int_fast32_t i32f[8 / sizeof(int_fast32_t)];
	SIMDE_ALIGN_TO_8 uint_fast32_t u32f[8 / sizeof(uint_fast32_t)];
#endif

#if defined(SIMDE_X86_MMX_USE_NATIVE_TYPE)
	__m64 n;
#endif
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	int8x8_t neon_i8;
	int16x4_t neon_i16;
	int32x2_t neon_i32;
	int64x1_t neon_i64;
	uint8x8_t neon_u8;
	uint16x4_t neon_u16;
	uint32x2_t neon_u32;
	uint64x1_t neon_u64;
	float32x2_t neon_f32;
#endif
#if defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	int8x8_t mmi_i8;
	int16x4_t mmi_i16;
	int32x2_t mmi_i32;
	int64_t mmi_i64;
	uint8x8_t mmi_u8;
	uint16x4_t mmi_u16;
	uint32x2_t mmi_u32;
	uint64_t mmi_u64;
#endif
} simde__m64_private;

#if defined(SIMDE_X86_MMX_USE_NATIVE_TYPE)
typedef __m64 simde__m64;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
typedef int32x2_t simde__m64;
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
typedef int32x2_t simde__m64;
#elif defined(SIMDE_VECTOR_SUBSCRIPT)
typedef int32_t simde__m64 SIMDE_ALIGN_TO_8 SIMDE_VECTOR(8) SIMDE_MAY_ALIAS;
#else
typedef simde__m64_private simde__m64;
#endif

#if !defined(SIMDE_X86_MMX_USE_NATIVE_TYPE) && \
	defined(SIMDE_ENABLE_NATIVE_ALIASES)
#define SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES
typedef simde__m64 __m64;
#endif

HEDLEY_STATIC_ASSERT(8 == sizeof(simde__m64), "__m64 size incorrect");
HEDLEY_STATIC_ASSERT(8 == sizeof(simde__m64_private), "__m64 size incorrect");
#if defined(SIMDE_CHECK_ALIGNMENT) && defined(SIMDE_ALIGN_OF)
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m64) == 8,
		     "simde__m64 is not 8-byte aligned");
HEDLEY_STATIC_ASSERT(SIMDE_ALIGN_OF(simde__m64_private) == 8,
		     "simde__m64_private is not 8-byte aligned");
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde__m64_from_private(simde__m64_private v)
{
	simde__m64 r;
	simde_memcpy(&r, &v, sizeof(r));
	return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64_private simde__m64_to_private(simde__m64 v)
{
	simde__m64_private r;
	simde_memcpy(&r, &v, sizeof(r));
	return r;
}

#define SIMDE_X86_GENERATE_CONVERSION_FUNCTION(simde_type, source_type, isax, \
					       fragment)                      \
	SIMDE_FUNCTION_ATTRIBUTES                                             \
	simde__##simde_type simde__##simde_type##_from_##isax##_##fragment(   \
		source_type value)                                            \
	{                                                                     \
		simde__##simde_type##_private r_;                             \
		r_.isax##_##fragment = value;                                 \
		return simde__##simde_type##_from_private(r_);                \
	}                                                                     \
                                                                              \
	SIMDE_FUNCTION_ATTRIBUTES                                             \
	source_type simde__##simde_type##_to_##isax##_##fragment(             \
		simde__##simde_type value)                                    \
	{                                                                     \
		simde__##simde_type##_private r_ =                            \
			simde__##simde_type##_to_private(value);              \
		return r_.isax##_##fragment;                                  \
	}

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int8x8_t, neon, i8)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int16x4_t, neon, i16)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int32x2_t, neon, i32)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int64x1_t, neon, i64)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint8x8_t, neon, u8)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint16x4_t, neon, u16)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint32x2_t, neon, u32)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint64x1_t, neon, u64)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, float32x2_t, neon, f32)
#endif /* defined(SIMDE_ARM_NEON_A32V7_NATIVE) */

#if defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int8x8_t, mmi, i8)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int16x4_t, mmi, i16)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int32x2_t, mmi, i32)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, int64_t, mmi, i64)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint8x8_t, mmi, u8)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint16x4_t, mmi, u16)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint32x2_t, mmi, u32)
SIMDE_X86_GENERATE_CONVERSION_FUNCTION(m64, uint64_t, mmi, u64)
#endif /* defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE) */

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_add_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_add_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i8 = vadd_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = paddb_s(a_.mmi_i8, b_.mmi_i8);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i8 = a_.i8 + b_.i8;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		r_.i8[i] = a_.i8[i] + b_.i8[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddb(a, b) simde_mm_add_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_add_pi8(a, b) simde_mm_add_pi8(a, b)
#define _m_paddb(a, b) simde_m_paddb(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_add_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_add_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vadd_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = paddh_s(a_.mmi_i16, b_.mmi_i16);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i16 = a_.i16 + b_.i16;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = a_.i16[i] + b_.i16[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddw(a, b) simde_mm_add_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_add_pi16(a, b) simde_mm_add_pi16(a, b)
#define _m_paddw(a, b) simde_mm_add_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_add_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_add_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vadd_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = paddw_s(a_.mmi_i32, b_.mmi_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i32 = a_.i32 + b_.i32;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = a_.i32[i] + b_.i32[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddd(a, b) simde_mm_add_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_add_pi32(a, b) simde_mm_add_pi32(a, b)
#define _m_paddd(a, b) simde_mm_add_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_adds_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_adds_pi8(a, b);
#else
	simde__m64_private r_, a_ = simde__m64_to_private(a),
			       b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i8 = vqadd_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = paddsb(a_.mmi_i8, b_.mmi_i8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		if ((((b_.i8[i]) > 0) &&
		     ((a_.i8[i]) > (INT8_MAX - (b_.i8[i]))))) {
			r_.i8[i] = INT8_MAX;
		} else if ((((b_.i8[i]) < 0) &&
			    ((a_.i8[i]) < (INT8_MIN - (b_.i8[i]))))) {
			r_.i8[i] = INT8_MIN;
		} else {
			r_.i8[i] = (a_.i8[i]) + (b_.i8[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddsb(a, b) simde_mm_adds_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_adds_pi8(a, b) simde_mm_adds_pi8(a, b)
#define _m_paddsb(a, b) simde_mm_adds_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_adds_pu8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_adds_pu8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u8 = vqadd_u8(a_.neon_u8, b_.neon_u8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_u8 = paddusb(a_.mmi_u8, b_.mmi_u8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u8) / sizeof(r_.u8[0])); i++) {
		const uint_fast16_t x =
			HEDLEY_STATIC_CAST(uint_fast16_t, a_.u8[i]) +
			HEDLEY_STATIC_CAST(uint_fast16_t, b_.u8[i]);
		if (x > UINT8_MAX)
			r_.u8[i] = UINT8_MAX;
		else
			r_.u8[i] = HEDLEY_STATIC_CAST(uint8_t, x);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddusb(a, b) simde_mm_adds_pu8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_adds_pu8(a, b) simde_mm_adds_pu8(a, b)
#define _m_paddusb(a, b) simde_mm_adds_pu8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_adds_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_adds_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vqadd_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = paddsh(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if ((((b_.i16[i]) > 0) &&
		     ((a_.i16[i]) > (INT16_MAX - (b_.i16[i]))))) {
			r_.i16[i] = INT16_MAX;
		} else if ((((b_.i16[i]) < 0) &&
			    ((a_.i16[i]) < (SHRT_MIN - (b_.i16[i]))))) {
			r_.i16[i] = SHRT_MIN;
		} else {
			r_.i16[i] = (a_.i16[i]) + (b_.i16[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddsw(a, b) simde_mm_adds_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_adds_pi16(a, b) simde_mm_adds_pi16(a, b)
#define _m_paddsw(a, b) simde_mm_adds_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_adds_pu16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_adds_pu16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vqadd_u16(a_.neon_u16, b_.neon_u16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_u16 = paddush(a_.mmi_u16, b_.mmi_u16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		const uint32_t x = a_.u16[i] + b_.u16[i];
		if (x > UINT16_MAX)
			r_.u16[i] = UINT16_MAX;
		else
			r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, x);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_paddusw(a, b) simde_mm_adds_pu16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_adds_pu16(a, b) simde_mm_adds_pu16(a, b)
#define _m_paddusw(a, b) simde_mm_adds_pu16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_and_si64(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_and_si64(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vand_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i64 = a_.i64 & b_.i64;
#else
	r_.i64[0] = a_.i64[0] & b_.i64[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pand(a, b) simde_mm_and_si64(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_and_si64(a, b) simde_mm_and_si64(a, b)
#define _m_pand(a, b) simde_mm_and_si64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_andnot_si64(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_andnot_si64(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vbic_s32(b_.neon_i32, a_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = pandn_sw(a_.mmi_i32, b_.mmi_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i32f = ~a_.i32f & b_.i32f;
#else
	r_.u64[0] = (~(a_.u64[0])) & (b_.u64[0]);
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pandn(a, b) simde_mm_andnot_si64(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_andnot_si64(a, b) simde_mm_andnot_si64(a, b)
#define _m_pandn(a, b) simde_mm_andnot_si64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpeq_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpeq_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u8 = vceq_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = pcmpeqb_s(a_.mmi_i8, b_.mmi_i8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		r_.i8[i] = (a_.i8[i] == b_.i8[i]) ? ~INT8_C(0) : INT8_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpeqb(a, b) simde_mm_cmpeq_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpeq_pi8(a, b) simde_mm_cmpeq_pi8(a, b)
#define _m_pcmpeqb(a, b) simde_mm_cmpeq_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpeq_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpeq_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vceq_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = pcmpeqh_s(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = (a_.i16[i] == b_.i16[i]) ? ~INT16_C(0) : INT16_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpeqw(a, b) simde_mm_cmpeq_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpeq_pi16(a, b) simde_mm_cmpeq_pi16(a, b)
#define _m_pcmpeqw(a, b) simde_mm_cmpeq_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpeq_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpeq_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u32 = vceq_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = pcmpeqw_s(a_.mmi_i32, b_.mmi_i32);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = (a_.i32[i] == b_.i32[i]) ? ~INT32_C(0) : INT32_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpeqd(a, b) simde_mm_cmpeq_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpeq_pi32(a, b) simde_mm_cmpeq_pi32(a, b)
#define _m_pcmpeqd(a, b) simde_mm_cmpeq_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpgt_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpgt_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u8 = vcgt_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = pcmpgtb_s(a_.mmi_i8, b_.mmi_i8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		r_.i8[i] = (a_.i8[i] > b_.i8[i]) ? ~INT8_C(0) : INT8_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpgtb(a, b) simde_mm_cmpgt_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpgt_pi8(a, b) simde_mm_cmpgt_pi8(a, b)
#define _m_pcmpgtb(a, b) simde_mm_cmpgt_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpgt_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpgt_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vcgt_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = pcmpgth_s(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = (a_.i16[i] > b_.i16[i]) ? ~INT16_C(0) : INT16_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpgtw(a, b) simde_mm_cmpgt_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpgt_pi16(a, b) simde_mm_cmpgt_pi16(a, b)
#define _m_pcmpgtw(a, b) simde_mm_cmpgt_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cmpgt_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cmpgt_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u32 = vcgt_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = pcmpgtw_s(a_.mmi_i32, b_.mmi_i32);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = (a_.i32[i] > b_.i32[i]) ? ~INT32_C(0) : INT32_C(0);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pcmpgtd(a, b) simde_mm_cmpgt_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cmpgt_pi32(a, b) simde_mm_cmpgt_pi32(a, b)
#define _m_pcmpgtd(a, b) simde_mm_cmpgt_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int64_t simde_mm_cvtm64_si64(simde__m64 a)
{
#if defined(SIMDE_X86_MMX_NATIVE) && defined(SIMDE_ARCH_AMD64) && \
	!defined(__PGI)
	return _mm_cvtm64_si64(a);
#else
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(10, 0, 0)
#pragma clang diagnostic ignored "-Wvector-conversion"
#endif
	return vget_lane_s64(a_.neon_i64, 0);
	HEDLEY_DIAGNOSTIC_POP
#else
	return a_.i64[0];
#endif
#endif
}
#define simde_m_to_int64(a) simde_mm_cvtm64_si64(a)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cvtm64_si64(a) simde_mm_cvtm64_si64(a)
#define _m_to_int64(a) simde_mm_cvtm64_si64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cvtsi32_si64(int32_t a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cvtsi32_si64(a);
#else
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int32_t av[sizeof(r_.neon_i32) / sizeof(r_.neon_i32[0])] = {a, 0};
	r_.neon_i32 = vld1_s32(av);
#else
	r_.i32[0] = a;
	r_.i32[1] = 0;
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_from_int(a) simde_mm_cvtsi32_si64(a)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cvtsi32_si64(a) simde_mm_cvtsi32_si64(a)
#define _m_from_int(a) simde_mm_cvtsi32_si64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_cvtsi64_m64(int64_t a)
{
#if defined(SIMDE_X86_MMX_NATIVE) && defined(SIMDE_ARCH_AMD64) && \
	!defined(__PGI)
	return _mm_cvtsi64_m64(a);
#else
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i64 = vld1_s64(&a);
#else
	r_.i64[0] = a;
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_from_int64(a) simde_mm_cvtsi64_m64(a)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cvtsi64_m64(a) simde_mm_cvtsi64_m64(a)
#define _m_from_int64(a) simde_mm_cvtsi64_m64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t simde_mm_cvtsi64_si32(simde__m64 a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_cvtsi64_si32(a);
#else
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(10, 0, 0)
#pragma clang diagnostic ignored "-Wvector-conversion"
#endif
	return vget_lane_s32(a_.neon_i32, 0);
	HEDLEY_DIAGNOSTIC_POP
#else
	return a_.i32[0];
#endif
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_cvtsi64_si32(a) simde_mm_cvtsi64_si32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void simde_mm_empty(void)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	_mm_empty();
#else
	/* noop */
#endif
}
#define simde_m_empty() simde_mm_empty()
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_empty() simde_mm_empty()
#define _m_empty() simde_mm_empty()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_madd_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_madd_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	int32x4_t i1 = vmull_s16(a_.neon_i16, b_.neon_i16);
	r_.neon_i32 = vpadd_s32(vget_low_s32(i1), vget_high_s32(i1));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = pmaddhw(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i += 2) {
		r_.i32[i / 2] = (a_.i16[i] * b_.i16[i]) +
				(a_.i16[i + 1] * b_.i16[i + 1]);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pmaddwd(a, b) simde_mm_madd_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_madd_pi16(a, b) simde_mm_madd_pi16(a, b)
#define _m_pmaddwd(a, b) simde_mm_madd_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_mulhi_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_mulhi_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int32x4_t t1 = vmull_s16(a_.neon_i16, b_.neon_i16);
	const uint32x4_t t2 = vshrq_n_u32(vreinterpretq_u32_s32(t1), 16);
	const uint16x4_t t3 = vmovn_u32(t2);
	r_.neon_u16 = t3;
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = pmulhh(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = HEDLEY_STATIC_CAST(int16_t,
					       ((a_.i16[i] * b_.i16[i]) >> 16));
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pmulhw(a, b) simde_mm_mulhi_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_mulhi_pi16(a, b) simde_mm_mulhi_pi16(a, b)
#define _m_pmulhw(a, b) simde_mm_mulhi_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_mullo_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_mullo_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int32x4_t t1 = vmull_s16(a_.neon_i16, b_.neon_i16);
	const uint16x4_t t2 = vmovn_u32(vreinterpretq_u32_s32(t1));
	r_.neon_u16 = t2;
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = pmullh(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = HEDLEY_STATIC_CAST(
			int16_t, ((a_.i16[i] * b_.i16[i]) & 0xffff));
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pmullw(a, b) simde_mm_mullo_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_mullo_pi16(a, b) simde_mm_mullo_pi16(a, b)
#define _m_pmullw(a, b) simde_mm_mullo_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_or_si64(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_or_si64(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vorr_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i64 = a_.i64 | b_.i64;
#else
	r_.i64[0] = a_.i64[0] | b_.i64[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_por(a, b) simde_mm_or_si64(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_or_si64(a, b) simde_mm_or_si64(a, b)
#define _m_por(a, b) simde_mm_or_si64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_packs_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_packs_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i8 = vqmovn_s16(vcombine_s16(a_.neon_i16, b_.neon_i16));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = packsshb(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if (a_.i16[i] < INT8_MIN) {
			r_.i8[i] = INT8_MIN;
		} else if (a_.i16[i] > INT8_MAX) {
			r_.i8[i] = INT8_MAX;
		} else {
			r_.i8[i] = HEDLEY_STATIC_CAST(int8_t, a_.i16[i]);
		}
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if (b_.i16[i] < INT8_MIN) {
			r_.i8[i + 4] = INT8_MIN;
		} else if (b_.i16[i] > INT8_MAX) {
			r_.i8[i + 4] = INT8_MAX;
		} else {
			r_.i8[i + 4] = HEDLEY_STATIC_CAST(int8_t, b_.i16[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_packsswb(a, b) simde_mm_packs_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_packs_pi16(a, b) simde_mm_packs_pi16(a, b)
#define _m_packsswb(a, b) simde_mm_packs_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_packs_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_packs_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vqmovn_s32(vcombine_s32(a_.neon_i32, b_.neon_i32));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = packsswh(a_.mmi_i32, b_.mmi_i32);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (8 / sizeof(a_.i32[0])); i++) {
		if (a_.i32[i] < SHRT_MIN) {
			r_.i16[i] = SHRT_MIN;
		} else if (a_.i32[i] > INT16_MAX) {
			r_.i16[i] = INT16_MAX;
		} else {
			r_.i16[i] = HEDLEY_STATIC_CAST(int16_t, a_.i32[i]);
		}
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < (8 / sizeof(b_.i32[0])); i++) {
		if (b_.i32[i] < SHRT_MIN) {
			r_.i16[i + 2] = SHRT_MIN;
		} else if (b_.i32[i] > INT16_MAX) {
			r_.i16[i + 2] = INT16_MAX;
		} else {
			r_.i16[i + 2] = HEDLEY_STATIC_CAST(int16_t, b_.i32[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_packssdw(a, b) simde_mm_packs_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_packs_pi32(a, b) simde_mm_packs_pi32(a, b)
#define _m_packssdw(a, b) simde_mm_packs_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_packs_pu16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_packs_pu16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	const int16x8_t t1 = vcombine_s16(a_.neon_i16, b_.neon_i16);

	/* Set elements which are < 0 to 0 */
	const int16x8_t t2 =
		vandq_s16(t1, vreinterpretq_s16_u16(vcgezq_s16(t1)));

	/* Vector with all s16 elements set to UINT8_MAX */
	const int16x8_t vmax =
		vmovq_n_s16(HEDLEY_STATIC_CAST(int16_t, UINT8_MAX));

	/* Elements which are within the acceptable range */
	const int16x8_t le_max =
		vandq_s16(t2, vreinterpretq_s16_u16(vcleq_s16(t2, vmax)));
	const int16x8_t gt_max =
		vandq_s16(vmax, vreinterpretq_s16_u16(vcgtq_s16(t2, vmax)));

	/* Final values as 16-bit integers */
	const int16x8_t values = vorrq_s16(le_max, gt_max);

	r_.neon_u8 = vmovn_u16(vreinterpretq_u16_s16(values));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_u8 = packushb(a_.mmi_u16, b_.mmi_u16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if (a_.i16[i] > UINT8_MAX) {
			r_.u8[i] = UINT8_MAX;
		} else if (a_.i16[i] < 0) {
			r_.u8[i] = 0;
		} else {
			r_.u8[i] = HEDLEY_STATIC_CAST(uint8_t, a_.i16[i]);
		}
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if (b_.i16[i] > UINT8_MAX) {
			r_.u8[i + 4] = UINT8_MAX;
		} else if (b_.i16[i] < 0) {
			r_.u8[i + 4] = 0;
		} else {
			r_.u8[i + 4] = HEDLEY_STATIC_CAST(uint8_t, b_.i16[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_packuswb(a, b) simde_mm_packs_pu16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_packs_pu16(a, b) simde_mm_packs_pu16(a, b)
#define _m_packuswb(a, b) simde_mm_packs_pu16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set_pi8(int8_t e7, int8_t e6, int8_t e5, int8_t e4,
			    int8_t e3, int8_t e2, int8_t e1, int8_t e0)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_set_pi8(e7, e6, e5, e4, e3, e2, e1, e0);
#else
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int8_t v[sizeof(r_.i8) / sizeof(r_.i8[0])] = {e0, e1, e2, e3,
							    e4, e5, e6, e7};
	r_.neon_i8 = vld1_s8(v);
#else
	r_.i8[0] = e0;
	r_.i8[1] = e1;
	r_.i8[2] = e2;
	r_.i8[3] = e3;
	r_.i8[4] = e4;
	r_.i8[5] = e5;
	r_.i8[6] = e6;
	r_.i8[7] = e7;
#endif

	return simde__m64_from_private(r_);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set_pi8(e7, e6, e5, e4, e3, e2, e1, e0) \
	simde_mm_set_pi8(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_set_pu8(uint8_t e7, uint8_t e6, uint8_t e5, uint8_t e4,
			      uint8_t e3, uint8_t e2, uint8_t e1, uint8_t e0)
{
	simde__m64_private r_;

#if defined(SIMDE_X86_MMX_NATIVE)
	r_.n = _mm_set_pi8(
		HEDLEY_STATIC_CAST(int8_t, e7), HEDLEY_STATIC_CAST(int8_t, e6),
		HEDLEY_STATIC_CAST(int8_t, e5), HEDLEY_STATIC_CAST(int8_t, e4),
		HEDLEY_STATIC_CAST(int8_t, e3), HEDLEY_STATIC_CAST(int8_t, e2),
		HEDLEY_STATIC_CAST(int8_t, e1), HEDLEY_STATIC_CAST(int8_t, e0));
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const uint8_t v[sizeof(r_.u8) / sizeof(r_.u8[0])] = {e0, e1, e2, e3,
							     e4, e5, e6, e7};
	r_.neon_u8 = vld1_u8(v);
#else
	r_.u8[0] = e0;
	r_.u8[1] = e1;
	r_.u8[2] = e2;
	r_.u8[3] = e3;
	r_.u8[4] = e4;
	r_.u8[5] = e5;
	r_.u8[6] = e6;
	r_.u8[7] = e7;
#endif

	return simde__m64_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set_pi16(int16_t e3, int16_t e2, int16_t e1, int16_t e0)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_set_pi16(e3, e2, e1, e0);
#else
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int16_t v[sizeof(r_.i16) / sizeof(r_.i16[0])] = {e0, e1, e2, e3};
	r_.neon_i16 = vld1_s16(v);
#else
	r_.i16[0] = e0;
	r_.i16[1] = e1;
	r_.i16[2] = e2;
	r_.i16[3] = e3;
#endif

	return simde__m64_from_private(r_);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set_pi16(e3, e2, e1, e0) simde_mm_set_pi16(e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_set_pu16(uint16_t e3, uint16_t e2, uint16_t e1,
			       uint16_t e0)
{
	simde__m64_private r_;

#if defined(SIMDE_X86_MMX_NATIVE)
	r_.n = _mm_set_pi16(HEDLEY_STATIC_CAST(int16_t, e3),
			    HEDLEY_STATIC_CAST(int16_t, e2),
			    HEDLEY_STATIC_CAST(int16_t, e1),
			    HEDLEY_STATIC_CAST(int16_t, e0));
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const uint16_t v[sizeof(r_.u16) / sizeof(r_.u16[0])] = {e0, e1, e2, e3};
	r_.neon_u16 = vld1_u16(v);
#else
	r_.u16[0] = e0;
	r_.u16[1] = e1;
	r_.u16[2] = e2;
	r_.u16[3] = e3;
#endif

	return simde__m64_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_set_pu32(uint32_t e1, uint32_t e0)
{
	simde__m64_private r_;

#if defined(SIMDE_X86_MMX_NATIVE)
	r_.n = _mm_set_pi32(HEDLEY_STATIC_CAST(int32_t, e1),
			    HEDLEY_STATIC_CAST(int32_t, e0));
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const uint32_t v[sizeof(r_.u32) / sizeof(r_.u32[0])] = {e0, e1};
	r_.neon_u32 = vld1_u32(v);
#else
	r_.u32[0] = e0;
	r_.u32[1] = e1;
#endif

	return simde__m64_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set_pi32(int32_t e1, int32_t e0)
{
	simde__m64_private r_;

#if defined(SIMDE_X86_MMX_NATIVE)
	r_.n = _mm_set_pi32(e1, e0);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int32_t v[sizeof(r_.i32) / sizeof(r_.i32[0])] = {e0, e1};
	r_.neon_i32 = vld1_s32(v);
#else
	r_.i32[0] = e0;
	r_.i32[1] = e1;
#endif

	return simde__m64_from_private(r_);
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set_pi32(e1, e0) simde_mm_set_pi32(e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_set_pi64(int64_t e0)
{
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const int64_t v[sizeof(r_.i64) / sizeof(r_.i64[0])] = {e0};
	r_.neon_i64 = vld1_s64(v);
#else
	r_.i64[0] = e0;
#endif

	return simde__m64_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_set_f32x2(simde_float32 e1, simde_float32 e0)
{
	simde__m64_private r_;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	const simde_float32 v[sizeof(r_.f32) / sizeof(r_.f32[0])] = {e0, e1};
	r_.neon_f32 = vld1_f32(v);
#else
	r_.f32[0] = e0;
	r_.f32[1] = e1;
#endif

	return simde__m64_from_private(r_);
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set1_pi8(int8_t a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_set1_pi8(a);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	simde__m64_private r_;
	r_.neon_i8 = vmov_n_s8(a);
	return simde__m64_from_private(r_);
#else
	return simde_mm_set_pi8(a, a, a, a, a, a, a, a);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set1_pi8(a) simde_mm_set1_pi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set1_pi16(int16_t a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_set1_pi16(a);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	simde__m64_private r_;
	r_.neon_i16 = vmov_n_s16(a);
	return simde__m64_from_private(r_);
#else
	return simde_mm_set_pi16(a, a, a, a);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set1_pi16(a) simde_mm_set1_pi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_set1_pi32(int32_t a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_set1_pi32(a);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	simde__m64_private r_;
	r_.neon_i32 = vmov_n_s32(a);
	return simde__m64_from_private(r_);
#else
	return simde_mm_set_pi32(a, a);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_set1_pi32(a) simde_mm_set1_pi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_setr_pi8(int8_t e7, int8_t e6, int8_t e5, int8_t e4,
			     int8_t e3, int8_t e2, int8_t e1, int8_t e0)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_setr_pi8(e7, e6, e5, e4, e3, e2, e1, e0);
#else
	return simde_mm_set_pi8(e0, e1, e2, e3, e4, e5, e6, e7);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_setr_pi8(e7, e6, e5, e4, e3, e2, e1, e0) \
	simde_mm_setr_pi8(e7, e6, e5, e4, e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_setr_pi16(int16_t e3, int16_t e2, int16_t e1, int16_t e0)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_setr_pi16(e3, e2, e1, e0);
#else
	return simde_mm_set_pi16(e0, e1, e2, e3);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_setr_pi16(e3, e2, e1, e0) simde_mm_setr_pi16(e3, e2, e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_setr_pi32(int32_t e1, int32_t e0)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_setr_pi32(e1, e0);
#else
	return simde_mm_set_pi32(e0, e1);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_setr_pi32(e1, e0) simde_mm_setr_pi32(e1, e0)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_setzero_si64(void)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_setzero_si64();
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	simde__m64_private r_;
	r_.neon_u32 = vmov_n_u32(0);
	return simde__m64_from_private(r_);
#else
	return simde_mm_set_pi32(0, 0);
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_setzero_si64() simde_mm_setzero_si64()
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_load_si64(const void *mem_addr)
{
	simde__m64 r;
	simde_memcpy(&r, SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m64),
		     sizeof(r));
	return r;
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_loadu_si64(const void *mem_addr)
{
	simde__m64 r;
	simde_memcpy(&r, mem_addr, sizeof(r));
	return r;
}

SIMDE_FUNCTION_ATTRIBUTES
void simde_x_mm_store_si64(void *mem_addr, simde__m64 value)
{
	simde_memcpy(SIMDE_ALIGN_ASSUME_LIKE(mem_addr, simde__m64), &value,
		     sizeof(value));
}

SIMDE_FUNCTION_ATTRIBUTES
void simde_x_mm_storeu_si64(void *mem_addr, simde__m64 value)
{
	simde_memcpy(mem_addr, &value, sizeof(value));
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_x_mm_setone_si64(void)
{
	return simde_mm_set1_pi32(~INT32_C(0));
}

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sll_pi16(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sll_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(10, 0, 0)
#pragma clang diagnostic ignored "-Wvector-conversion"
#endif
	r_.neon_i16 =
		vshl_s16(a_.neon_i16,
			 vmov_n_s16(HEDLEY_STATIC_CAST(
				 int16_t, vget_lane_u64(count_.neon_u64, 0))));
	HEDLEY_DIAGNOSTIC_POP
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && \
	defined(SIMDE_BUG_CLANG_POWER9_16x4_BAD_SHIFT)
	if (HEDLEY_UNLIKELY(count_.u64[0] > 15))
		return simde_mm_setzero_si64();

	r_.i16 = a_.i16 << HEDLEY_STATIC_CAST(int16_t, count_.u64[0]);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i16 = a_.i16 << count_.u64[0];
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 15)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++) {
		r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t,
					       a_.u16[i] << count_.u64[0]);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psllw(a, count) simde_mm_sll_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sll_pi16(a, count) simde_mm_sll_pi16(a, count)
#define _m_psllw(a, count) simde_mm_sll_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sll_pi32(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sll_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(10, 0, 0)
#pragma clang diagnostic ignored "-Wvector-conversion"
#endif
	r_.neon_i32 =
		vshl_s32(a_.neon_i32,
			 vmov_n_s32(HEDLEY_STATIC_CAST(
				 int32_t, vget_lane_u64(count_.neon_u64, 0))));
	HEDLEY_DIAGNOSTIC_POP
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i32 = a_.i32 << count_.u64[0];
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 31)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++) {
		r_.u32[i] = a_.u32[i] << count_.u64[0];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pslld(a, count) simde_mm_sll_pi32(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sll_pi32(a, count) simde_mm_sll_pi32(a, count)
#define _m_pslld(a, count) simde_mm_sll_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_slli_pi16(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_slli_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && \
	defined(SIMDE_BUG_CLANG_POWER9_16x4_BAD_SHIFT)
	if (HEDLEY_UNLIKELY(count > 15))
		return simde_mm_setzero_si64();

	r_.i16 = a_.i16 << HEDLEY_STATIC_CAST(int16_t, count);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i16 = a_.i16 << count;
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vshl_s16(a_.neon_i16, vmov_n_s16((int16_t)count));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = psllh_s(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++) {
		r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, a_.u16[i] << count);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psllwi(a, count) simde_mm_slli_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_slli_pi16(a, count) simde_mm_slli_pi16(a, count)
#define _m_psllwi(a, count) simde_mm_slli_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_slli_pi32(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_slli_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i32 = a_.i32 << count;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vshl_s32(a_.neon_i32, vmov_n_s32((int32_t)count));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = psllw_s(a_.mmi_i32, b_.mmi_i32);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++) {
		r_.u32[i] = a_.u32[i] << count;
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pslldi(a, b) simde_mm_slli_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_slli_pi32(a, count) simde_mm_slli_pi32(a, count)
#define _m_pslldi(a, count) simde_mm_slli_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_slli_si64(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_slli_si64(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i64 = a_.i64 << count;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i64 = vshl_s64(a_.neon_i64, vmov_n_s64((int64_t)count));
#else
	r_.u64[0] = a_.u64[0] << count;
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psllqi(a, count) simde_mm_slli_si64(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_slli_si64(a, count) simde_mm_slli_si64(a, count)
#define _m_psllqi(a, count) simde_mm_slli_si64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sll_si64(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sll_si64(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i64 = vshl_s64(a_.neon_i64, count_.neon_i64);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i64 = a_.i64 << count_.i64;
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 63)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	r_.u64[0] = a_.u64[0] << count_.u64[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psllq(a, count) simde_mm_sll_si64(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sll_si64(a, count) simde_mm_sll_si64(a, count)
#define _m_psllq(a, count) simde_mm_sll_si64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srl_pi16(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_srl_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR) && \
	defined(SIMDE_BUG_CLANG_POWER9_16x4_BAD_SHIFT)
	if (HEDLEY_UNLIKELY(count_.u64[0] > 15))
		return simde_mm_setzero_si64();

	r_.i16 = a_.i16 >> HEDLEY_STATIC_CAST(int16_t, count_.u64[0]);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.u16 = a_.u16 >> count_.u64[0];
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vshl_u16(
		a_.neon_u16,
		vmov_n_s16(-((int16_t)vget_lane_u64(count_.neon_u64, 0))));
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 15)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < sizeof(r_.u16) / sizeof(r_.u16[0]); i++) {
		r_.u16[i] = a_.u16[i] >> count_.u64[0];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrlw(a, count) simde_mm_srl_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srl_pi16(a, count) simde_mm_srl_pi16(a, count)
#define _m_psrlw(a, count) simde_mm_srl_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srl_pi32(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_srl_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.u32 = a_.u32 >> count_.u64[0];
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u32 = vshl_u32(
		a_.neon_u32,
		vmov_n_s32(-((int32_t)vget_lane_u64(count_.neon_u64, 0))));
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 31)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	SIMDE_VECTORIZE
	for (size_t i = 0; i < sizeof(r_.u32) / sizeof(r_.u32[0]); i++) {
		r_.u32[i] = a_.u32[i] >> count_.u64[0];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrld(a, count) simde_mm_srl_pi32(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srl_pi32(a, count) simde_mm_srl_pi32(a, count)
#define _m_psrld(a, count) simde_mm_srl_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srli_pi16(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_srli_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.u16 = a_.u16 >> count;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vshl_u16(a_.neon_u16, vmov_n_s16(-((int16_t)count)));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = psrlh_s(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++) {
		r_.u16[i] = a_.u16[i] >> count;
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrlwi(a, count) simde_mm_srli_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srli_pi16(a, count) simde_mm_srli_pi16(a, count)
#define _m_psrlwi(a, count) simde_mm_srli_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srli_pi32(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_srli_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.u32 = a_.u32 >> count;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u32 = vshl_u32(a_.neon_u32, vmov_n_s32(-((int32_t)count)));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = psrlw_s(a_.mmi_i32, b_.mmi_i32);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u32) / sizeof(r_.u32[0])); i++) {
		r_.u32[i] = a_.u32[i] >> count;
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrldi(a, count) simde_mm_srli_pi32(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srli_pi32(a, count) simde_mm_srli_pi32(a, count)
#define _m_psrldi(a, count) simde_mm_srli_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srli_si64(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_srli_si64(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u64 = vshl_u64(a_.neon_u64, vmov_n_s64(-count));
#elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.u64 = a_.u64 >> count;
#else
	r_.u64[0] = a_.u64[0] >> count;
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrlqi(a, count) simde_mm_srli_si64(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srli_si64(a, count) simde_mm_srli_si64(a, count)
#define _m_psrlqi(a, count) simde_mm_srli_si64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srl_si64(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_srl_si64(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_u64 = vshl_u64(a_.neon_u64, vneg_s64(count_.neon_i64));
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.u64 = a_.u64 >> count_.u64;
#else
	if (HEDLEY_UNLIKELY(count_.u64[0] > 63)) {
		simde_memset(&r_, 0, sizeof(r_));
		return simde__m64_from_private(r_);
	}

	r_.u64[0] = a_.u64[0] >> count_.u64[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrlq(a, count) simde_mm_srl_si64(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srl_si64(a, count) simde_mm_srl_si64(a, count)
#define _m_psrlq(a, count) simde_mm_srl_si64(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srai_pi16(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_srai_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i16 = a_.i16 >> (count & 0xff);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vshl_s16(a_.neon_i16,
			       vmov_n_s16(-HEDLEY_STATIC_CAST(int16_t, count)));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = psrah_s(a_.mmi_i16, count);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = a_.i16[i] >> (count & 0xff);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrawi(a, count) simde_mm_srai_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srai_pi16(a, count) simde_mm_srai_pi16(a, count)
#define _m_psrawi(a, count) simde_mm_srai_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_srai_pi32(simde__m64 a, int count)
{
#if defined(SIMDE_X86_MMX_NATIVE) && !defined(__PGI)
	return _mm_srai_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i32 = a_.i32 >> (count & 0xff);
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vshl_s32(a_.neon_i32,
			       vmov_n_s32(-HEDLEY_STATIC_CAST(int32_t, count)));
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = psraw_s(a_.mmi_i32, count);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = a_.i32[i] >> (count & 0xff);
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psradi(a, count) simde_mm_srai_pi32(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_srai_pi32(a, count) simde_mm_srai_pi32(a, count)
#define _m_psradi(a, count) simde_mm_srai_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sra_pi16(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sra_pi16(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);
	const int cnt = HEDLEY_STATIC_CAST(
		int, (count_.i64[0] > 15 ? 15 : count_.i64[0]));

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i16 = a_.i16 >> cnt;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 =
		vshl_s16(a_.neon_i16,
			 vmov_n_s16(-HEDLEY_STATIC_CAST(
				 int16_t, vget_lane_u64(count_.neon_u64, 0))));
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = a_.i16[i] >> cnt;
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psraw(a, count) simde_mm_sra_pi16(a, count)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sra_pi16(a, count) simde_mm_sra_pi16(a, count)
#define _m_psraw(a, count) simde_mm_sra_pi16(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sra_pi32(simde__m64 a, simde__m64 count)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sra_pi32(a, count);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private count_ = simde__m64_to_private(count);
	const int32_t cnt =
		(count_.u64[0] > 31)
			? 31
			: HEDLEY_STATIC_CAST(int32_t, count_.u64[0]);

#if defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
	r_.i32 = a_.i32 >> cnt;
#elif defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 =
		vshl_s32(a_.neon_i32,
			 vmov_n_s32(-HEDLEY_STATIC_CAST(
				 int32_t, vget_lane_u64(count_.neon_u64, 0))));
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = a_.i32[i] >> cnt;
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psrad(a, b) simde_mm_sra_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sra_pi32(a, count) simde_mm_sra_pi32(a, count)
#define _m_psrad(a, count) simde_mm_sra_pi32(a, count)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sub_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sub_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i8 = vsub_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = psubb_s(a_.mmi_i8, b_.mmi_i8);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i8 = a_.i8 - b_.i8;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		r_.i8[i] = a_.i8[i] - b_.i8[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubb(a, b) simde_mm_sub_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sub_pi8(a, b) simde_mm_sub_pi8(a, b)
#define _m_psubb(a, b) simde_mm_sub_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sub_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sub_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vsub_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = psubh_s(a_.mmi_i16, b_.mmi_i16);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i16 = a_.i16 - b_.i16;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		r_.i16[i] = a_.i16[i] - b_.i16[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubw(a, b) simde_mm_sub_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sub_pi16(a, b) simde_mm_sub_pi16(a, b)
#define _m_psubw(a, b) simde_mm_sub_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_sub_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_sub_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = vsub_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = psubw_s(a_.mmi_i32, b_.mmi_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i32 = a_.i32 - b_.i32;
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i32) / sizeof(r_.i32[0])); i++) {
		r_.i32[i] = a_.i32[i] - b_.i32[i];
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubd(a, b) simde_mm_sub_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_sub_pi32(a, b) simde_mm_sub_pi32(a, b)
#define _m_psubd(a, b) simde_mm_sub_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_subs_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_subs_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i8 = vqsub_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = psubsb(a_.mmi_i8, b_.mmi_i8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i8) / sizeof(r_.i8[0])); i++) {
		if (((b_.i8[i]) > 0 && (a_.i8[i]) < INT8_MIN + (b_.i8[i]))) {
			r_.i8[i] = INT8_MIN;
		} else if ((b_.i8[i]) < 0 &&
			   (a_.i8[i]) > INT8_MAX + (b_.i8[i])) {
			r_.i8[i] = INT8_MAX;
		} else {
			r_.i8[i] = (a_.i8[i]) - (b_.i8[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubsb(a, b) simde_mm_subs_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_subs_pi8(a, b) simde_mm_subs_pi8(a, b)
#define _m_psubsb(a, b) simde_mm_subs_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_subs_pu8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_subs_pu8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u8 = vqsub_u8(a_.neon_u8, b_.neon_u8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_u8 = psubusb(a_.mmi_u8, b_.mmi_u8);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u8) / sizeof(r_.u8[0])); i++) {
		const int32_t x = a_.u8[i] - b_.u8[i];
		if (x < 0) {
			r_.u8[i] = 0;
		} else if (x > UINT8_MAX) {
			r_.u8[i] = UINT8_MAX;
		} else {
			r_.u8[i] = HEDLEY_STATIC_CAST(uint8_t, x);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubusb(a, b) simde_mm_subs_pu8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_subs_pu8(a, b) simde_mm_subs_pu8(a, b)
#define _m_psubusb(a, b) simde_mm_subs_pu8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_subs_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_subs_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i16 = vqsub_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = psubsh(a_.mmi_i16, b_.mmi_i16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.i16) / sizeof(r_.i16[0])); i++) {
		if (((b_.i16[i]) > 0 && (a_.i16[i]) < SHRT_MIN + (b_.i16[i]))) {
			r_.i16[i] = SHRT_MIN;
		} else if ((b_.i16[i]) < 0 &&
			   (a_.i16[i]) > INT16_MAX + (b_.i16[i])) {
			r_.i16[i] = INT16_MAX;
		} else {
			r_.i16[i] = (a_.i16[i]) - (b_.i16[i]);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubsw(a, b) simde_mm_subs_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_subs_pi16(a, b) simde_mm_subs_pi16(a, b)
#define _m_psubsw(a, b) simde_mm_subs_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_subs_pu16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_subs_pu16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_u16 = vqsub_u16(a_.neon_u16, b_.neon_u16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_u16 = psubush(a_.mmi_u16, b_.mmi_u16);
#else
	SIMDE_VECTORIZE
	for (size_t i = 0; i < (sizeof(r_.u16) / sizeof(r_.u16[0])); i++) {
		const int x = a_.u16[i] - b_.u16[i];
		if (x < 0) {
			r_.u16[i] = 0;
		} else if (x > UINT16_MAX) {
			r_.u16[i] = UINT16_MAX;
		} else {
			r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, x);
		}
	}
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_psubusw(a, b) simde_mm_subs_pu16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_subs_pu16(a, b) simde_mm_subs_pu16(a, b)
#define _m_psubusw(a, b) simde_mm_subs_pu16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpackhi_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpackhi_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i8 = vzip2_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i8 = SIMDE_SHUFFLE_VECTOR_(8, 8, a_.i8, b_.i8, 4, 12, 5, 13, 6, 14,
				      7, 15);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = punpckhbh_s(a_.mmi_i8, b_.mmi_i8);
#else
	r_.i8[0] = a_.i8[4];
	r_.i8[1] = b_.i8[4];
	r_.i8[2] = a_.i8[5];
	r_.i8[3] = b_.i8[5];
	r_.i8[4] = a_.i8[6];
	r_.i8[5] = b_.i8[6];
	r_.i8[6] = a_.i8[7];
	r_.i8[7] = b_.i8[7];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpckhbw(a, b) simde_mm_unpackhi_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpackhi_pi8(a, b) simde_mm_unpackhi_pi8(a, b)
#define _m_punpckhbw(a, b) simde_mm_unpackhi_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpackhi_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpackhi_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i16 = vzip2_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = punpckhhw_s(a_.mmi_i16, b_.mmi_i16);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i16 = SIMDE_SHUFFLE_VECTOR_(16, 8, a_.i16, b_.i16, 2, 6, 3, 7);
#else
	r_.i16[0] = a_.i16[2];
	r_.i16[1] = b_.i16[2];
	r_.i16[2] = a_.i16[3];
	r_.i16[3] = b_.i16[3];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpckhwd(a, b) simde_mm_unpackhi_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpackhi_pi16(a, b) simde_mm_unpackhi_pi16(a, b)
#define _m_punpckhwd(a, b) simde_mm_unpackhi_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpackhi_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpackhi_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i32 = vzip2_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = punpckhwd_s(a_.mmi_i32, b_.mmi_i32);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i32 = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.i32, b_.i32, 1, 3);
#else
	r_.i32[0] = a_.i32[1];
	r_.i32[1] = b_.i32[1];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpckhdq(a, b) simde_mm_unpackhi_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpackhi_pi32(a, b) simde_mm_unpackhi_pi32(a, b)
#define _m_punpckhdq(a, b) simde_mm_unpackhi_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpacklo_pi8(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpacklo_pi8(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i8 = vzip1_s8(a_.neon_i8, b_.neon_i8);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i8 = punpcklbh_s(a_.mmi_i8, b_.mmi_i8);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i8 = SIMDE_SHUFFLE_VECTOR_(8, 8, a_.i8, b_.i8, 0, 8, 1, 9, 2, 10, 3,
				      11);
#else
	r_.i8[0] = a_.i8[0];
	r_.i8[1] = b_.i8[0];
	r_.i8[2] = a_.i8[1];
	r_.i8[3] = b_.i8[1];
	r_.i8[4] = a_.i8[2];
	r_.i8[5] = b_.i8[2];
	r_.i8[6] = a_.i8[3];
	r_.i8[7] = b_.i8[3];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpcklbw(a, b) simde_mm_unpacklo_pi8(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpacklo_pi8(a, b) simde_mm_unpacklo_pi8(a, b)
#define _m_punpcklbw(a, b) simde_mm_unpacklo_pi8(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpacklo_pi16(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpacklo_pi16(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i16 = vzip1_s16(a_.neon_i16, b_.neon_i16);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i16 = punpcklhw_s(a_.mmi_i16, b_.mmi_i16);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i16 = SIMDE_SHUFFLE_VECTOR_(16, 8, a_.i16, b_.i16, 0, 4, 1, 5);
#else
	r_.i16[0] = a_.i16[0];
	r_.i16[1] = b_.i16[0];
	r_.i16[2] = a_.i16[1];
	r_.i16[3] = b_.i16[1];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpcklwd(a, b) simde_mm_unpacklo_pi16(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpacklo_pi16(a, b) simde_mm_unpacklo_pi16(a, b)
#define _m_punpcklwd(a, b) simde_mm_unpacklo_pi16(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_unpacklo_pi32(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_unpacklo_pi32(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
	r_.neon_i32 = vzip1_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_MIPS_LOONGSON_MMI_NATIVE)
	r_.mmi_i32 = punpcklwd_s(a_.mmi_i32, b_.mmi_i32);
#elif defined(SIMDE_SHUFFLE_VECTOR_)
	r_.i32 = SIMDE_SHUFFLE_VECTOR_(32, 8, a_.i32, b_.i32, 0, 2);
#else
	r_.i32[0] = a_.i32[0];
	r_.i32[1] = b_.i32[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_punpckldq(a, b) simde_mm_unpacklo_pi32(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_unpacklo_pi32(a, b) simde_mm_unpacklo_pi32(a, b)
#define _m_punpckldq(a, b) simde_mm_unpacklo_pi32(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m64 simde_mm_xor_si64(simde__m64 a, simde__m64 b)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _mm_xor_si64(a, b);
#else
	simde__m64_private r_;
	simde__m64_private a_ = simde__m64_to_private(a);
	simde__m64_private b_ = simde__m64_to_private(b);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	r_.neon_i32 = veor_s32(a_.neon_i32, b_.neon_i32);
#elif defined(SIMDE_VECTOR_SUBSCRIPT_OPS)
	r_.i32f = a_.i32f ^ b_.i32f;
#else
	r_.u64[0] = a_.u64[0] ^ b_.u64[0];
#endif

	return simde__m64_from_private(r_);
#endif
}
#define simde_m_pxor(a, b) simde_mm_xor_si64(a, b)
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _mm_xor_si64(a, b) simde_mm_xor_si64(a, b)
#define _m_pxor(a, b) simde_mm_xor_si64(a, b)
#endif

SIMDE_FUNCTION_ATTRIBUTES
int32_t simde_m_to_int(simde__m64 a)
{
#if defined(SIMDE_X86_MMX_NATIVE)
	return _m_to_int(a);
#else
	simde__m64_private a_ = simde__m64_to_private(a);

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
	HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wvector-conversion") && \
	SIMDE_DETECT_CLANG_VERSION_NOT(10, 0, 0)
#pragma clang diagnostic ignored "-Wvector-conversion"
#endif
	return vget_lane_s32(a_.neon_i32, 0);
	HEDLEY_DIAGNOSTIC_POP
#else
	return a_.i32[0];
#endif
#endif
}
#if defined(SIMDE_X86_MMX_ENABLE_NATIVE_ALIASES)
#define _m_to_int(a) simde_m_to_int(a)
#endif

SIMDE_END_DECLS_

HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_MMX_H) */
