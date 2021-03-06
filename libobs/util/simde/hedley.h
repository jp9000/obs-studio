/* Hedley - https://nemequ.github.io/hedley
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.
 *
 * For details, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 * SPDX-License-Identifier: CC0-1.0
 */

#if !defined(HEDLEY_VERSION) || (HEDLEY_VERSION < 14)
#if defined(HEDLEY_VERSION)
#undef HEDLEY_VERSION
#endif
#define HEDLEY_VERSION 14

#if defined(HEDLEY_STRINGIFY_EX)
#undef HEDLEY_STRINGIFY_EX
#endif
#define HEDLEY_STRINGIFY_EX(x) #x

#if defined(HEDLEY_STRINGIFY)
#undef HEDLEY_STRINGIFY
#endif
#define HEDLEY_STRINGIFY(x) HEDLEY_STRINGIFY_EX(x)

#if defined(HEDLEY_CONCAT_EX)
#undef HEDLEY_CONCAT_EX
#endif
#define HEDLEY_CONCAT_EX(a, b) a##b

#if defined(HEDLEY_CONCAT)
#undef HEDLEY_CONCAT
#endif
#define HEDLEY_CONCAT(a, b) HEDLEY_CONCAT_EX(a, b)

#if defined(HEDLEY_CONCAT3_EX)
#undef HEDLEY_CONCAT3_EX
#endif
#define HEDLEY_CONCAT3_EX(a, b, c) a##b##c

#if defined(HEDLEY_CONCAT3)
#undef HEDLEY_CONCAT3
#endif
#define HEDLEY_CONCAT3(a, b, c) HEDLEY_CONCAT3_EX(a, b, c)

#if defined(HEDLEY_VERSION_ENCODE)
#undef HEDLEY_VERSION_ENCODE
#endif
#define HEDLEY_VERSION_ENCODE(major, minor, revision) \
	(((major)*1000000) + ((minor)*1000) + (revision))

#if defined(HEDLEY_VERSION_DECODE_MAJOR)
#undef HEDLEY_VERSION_DECODE_MAJOR
#endif
#define HEDLEY_VERSION_DECODE_MAJOR(version) ((version) / 1000000)

#if defined(HEDLEY_VERSION_DECODE_MINOR)
#undef HEDLEY_VERSION_DECODE_MINOR
#endif
#define HEDLEY_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)

#if defined(HEDLEY_VERSION_DECODE_REVISION)
#undef HEDLEY_VERSION_DECODE_REVISION
#endif
#define HEDLEY_VERSION_DECODE_REVISION(version) ((version) % 1000)

#if defined(HEDLEY_GNUC_VERSION)
#undef HEDLEY_GNUC_VERSION
#endif
#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#define HEDLEY_GNUC_VERSION \
	HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__)
#define HEDLEY_GNUC_VERSION HEDLEY_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)
#endif

#if defined(HEDLEY_GNUC_VERSION_CHECK)
#undef HEDLEY_GNUC_VERSION_CHECK
#endif
#if defined(HEDLEY_GNUC_VERSION)
#define HEDLEY_GNUC_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_GNUC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_GNUC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_MSVC_VERSION)
#undef HEDLEY_MSVC_VERSION
#endif
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
#define HEDLEY_MSVC_VERSION                                        \
	HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 10000000,            \
			      (_MSC_FULL_VER % 10000000) / 100000, \
			      (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER) && !defined(__ICL)
#define HEDLEY_MSVC_VERSION                                      \
	HEDLEY_VERSION_ENCODE(_MSC_FULL_VER / 1000000,           \
			      (_MSC_FULL_VER % 1000000) / 10000, \
			      (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER) && !defined(__ICL)
#define HEDLEY_MSVC_VERSION \
	HEDLEY_VERSION_ENCODE(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif

#if defined(HEDLEY_MSVC_VERSION_CHECK)
#undef HEDLEY_MSVC_VERSION_CHECK
#endif
#if !defined(HEDLEY_MSVC_VERSION)
#define HEDLEY_MSVC_VERSION_CHECK(major, minor, patch) (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
#define HEDLEY_MSVC_VERSION_CHECK(major, minor, patch) \
	(_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#define HEDLEY_MSVC_VERSION_CHECK(major, minor, patch) \
	(_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#define HEDLEY_MSVC_VERSION_CHECK(major, minor, patch) \
	(_MSC_VER >= ((major * 100) + (minor)))
#endif

#if defined(HEDLEY_INTEL_VERSION)
#undef HEDLEY_INTEL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && \
	!defined(__ICL)
#define HEDLEY_INTEL_VERSION                                                  \
	HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, \
			      __INTEL_COMPILER_UPDATE)
#elif defined(__INTEL_COMPILER) && !defined(__ICL)
#define HEDLEY_INTEL_VERSION \
	HEDLEY_VERSION_ENCODE(__INTEL_COMPILER / 100, __INTEL_COMPILER % 100, 0)
#endif

#if defined(HEDLEY_INTEL_VERSION_CHECK)
#undef HEDLEY_INTEL_VERSION_CHECK
#endif
#if defined(HEDLEY_INTEL_VERSION)
#define HEDLEY_INTEL_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_INTEL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_INTEL_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_INTEL_CL_VERSION)
#undef HEDLEY_INTEL_CL_VERSION
#endif
#if defined(__INTEL_COMPILER) && defined(__INTEL_COMPILER_UPDATE) && \
	defined(__ICL)
#define HEDLEY_INTEL_CL_VERSION \
	HEDLEY_VERSION_ENCODE(__INTEL_COMPILER, __INTEL_COMPILER_UPDATE, 0)
#endif

#if defined(HEDLEY_INTEL_CL_VERSION_CHECK)
#undef HEDLEY_INTEL_CL_VERSION_CHECK
#endif
#if defined(HEDLEY_INTEL_CL_VERSION)
#define HEDLEY_INTEL_CL_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_INTEL_CL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_INTEL_CL_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_PGI_VERSION)
#undef HEDLEY_PGI_VERSION
#endif
#if defined(__PGI) && defined(__PGIC__) && defined(__PGIC_MINOR__) && \
	defined(__PGIC_PATCHLEVEL__)
#define HEDLEY_PGI_VERSION \
	HEDLEY_VERSION_ENCODE(__PGIC__, __PGIC_MINOR__, __PGIC_PATCHLEVEL__)
#endif

#if defined(HEDLEY_PGI_VERSION_CHECK)
#undef HEDLEY_PGI_VERSION_CHECK
#endif
#if defined(HEDLEY_PGI_VERSION)
#define HEDLEY_PGI_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_PGI_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_PGI_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_SUNPRO_VERSION)
#undef HEDLEY_SUNPRO_VERSION
#endif
#if defined(__SUNPRO_C) && (__SUNPRO_C > 0x1000)
#define HEDLEY_SUNPRO_VERSION                                     \
	HEDLEY_VERSION_ENCODE((((__SUNPRO_C >> 16) & 0xf) * 10) + \
				      ((__SUNPRO_C >> 12) & 0xf), \
			      (((__SUNPRO_C >> 8) & 0xf) * 10) +  \
				      ((__SUNPRO_C >> 4) & 0xf),  \
			      (__SUNPRO_C & 0xf) * 10)
#elif defined(__SUNPRO_C)
#define HEDLEY_SUNPRO_VERSION                          \
	HEDLEY_VERSION_ENCODE((__SUNPRO_C >> 8) & 0xf, \
			      (__SUNPRO_C >> 4) & 0xf, (__SUNPRO_C)&0xf)
#elif defined(__SUNPRO_CC) && (__SUNPRO_CC > 0x1000)
#define HEDLEY_SUNPRO_VERSION                                      \
	HEDLEY_VERSION_ENCODE((((__SUNPRO_CC >> 16) & 0xf) * 10) + \
				      ((__SUNPRO_CC >> 12) & 0xf), \
			      (((__SUNPRO_CC >> 8) & 0xf) * 10) +  \
				      ((__SUNPRO_CC >> 4) & 0xf),  \
			      (__SUNPRO_CC & 0xf) * 10)
#elif defined(__SUNPRO_CC)
#define HEDLEY_SUNPRO_VERSION                           \
	HEDLEY_VERSION_ENCODE((__SUNPRO_CC >> 8) & 0xf, \
			      (__SUNPRO_CC >> 4) & 0xf, (__SUNPRO_CC)&0xf)
#endif

#if defined(HEDLEY_SUNPRO_VERSION_CHECK)
#undef HEDLEY_SUNPRO_VERSION_CHECK
#endif
#if defined(HEDLEY_SUNPRO_VERSION)
#define HEDLEY_SUNPRO_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_SUNPRO_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_SUNPRO_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_EMSCRIPTEN_VERSION)
#undef HEDLEY_EMSCRIPTEN_VERSION
#endif
#if defined(__EMSCRIPTEN__)
#define HEDLEY_EMSCRIPTEN_VERSION                                         \
	HEDLEY_VERSION_ENCODE(__EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, \
			      __EMSCRIPTEN_tiny__)
#endif

#if defined(HEDLEY_EMSCRIPTEN_VERSION_CHECK)
#undef HEDLEY_EMSCRIPTEN_VERSION_CHECK
#endif
#if defined(HEDLEY_EMSCRIPTEN_VERSION)
#define HEDLEY_EMSCRIPTEN_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_EMSCRIPTEN_VERSION >=                        \
	 HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_EMSCRIPTEN_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_ARM_VERSION)
#undef HEDLEY_ARM_VERSION
#endif
#if defined(__CC_ARM) && defined(__ARMCOMPILER_VERSION)
#define HEDLEY_ARM_VERSION                                               \
	HEDLEY_VERSION_ENCODE(__ARMCOMPILER_VERSION / 1000000,           \
			      (__ARMCOMPILER_VERSION % 1000000) / 10000, \
			      (__ARMCOMPILER_VERSION % 10000) / 100)
#elif defined(__CC_ARM) && defined(__ARMCC_VERSION)
#define HEDLEY_ARM_VERSION                                         \
	HEDLEY_VERSION_ENCODE(__ARMCC_VERSION / 1000000,           \
			      (__ARMCC_VERSION % 1000000) / 10000, \
			      (__ARMCC_VERSION % 10000) / 100)
#endif

#if defined(HEDLEY_ARM_VERSION_CHECK)
#undef HEDLEY_ARM_VERSION_CHECK
#endif
#if defined(HEDLEY_ARM_VERSION)
#define HEDLEY_ARM_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_ARM_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_ARM_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_IBM_VERSION)
#undef HEDLEY_IBM_VERSION
#endif
#if defined(__ibmxl__)
#define HEDLEY_IBM_VERSION                                          \
	HEDLEY_VERSION_ENCODE(__ibmxl_version__, __ibmxl_release__, \
			      __ibmxl_modification__)
#elif defined(__xlC__) && defined(__xlC_ver__)
#define HEDLEY_IBM_VERSION                                  \
	HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, \
			      (__xlC_ver__ >> 8) & 0xff)
#elif defined(__xlC__)
#define HEDLEY_IBM_VERSION \
	HEDLEY_VERSION_ENCODE(__xlC__ >> 8, __xlC__ & 0xff, 0)
#endif

#if defined(HEDLEY_IBM_VERSION_CHECK)
#undef HEDLEY_IBM_VERSION_CHECK
#endif
#if defined(HEDLEY_IBM_VERSION)
#define HEDLEY_IBM_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_IBM_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_IBM_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_VERSION)
#undef HEDLEY_TI_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) &&                                       \
	(defined(__TMS470__) || defined(__TI_ARM__) || defined(__MSP430__) || \
	 defined(__TMS320C2000__))
#if (__TI_COMPILER_VERSION__ >= 16000000)
#define HEDLEY_TI_VERSION                                                 \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif
#endif

#if defined(HEDLEY_TI_VERSION_CHECK)
#undef HEDLEY_TI_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_VERSION)
#define HEDLEY_TI_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_CL2000_VERSION)
#undef HEDLEY_TI_CL2000_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C2000__)
#define HEDLEY_TI_CL2000_VERSION                                          \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL2000_VERSION_CHECK)
#undef HEDLEY_TI_CL2000_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL2000_VERSION)
#define HEDLEY_TI_CL2000_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_CL2000_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_CL2000_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_CL430_VERSION)
#undef HEDLEY_TI_CL430_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__MSP430__)
#define HEDLEY_TI_CL430_VERSION                                           \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL430_VERSION_CHECK)
#undef HEDLEY_TI_CL430_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL430_VERSION)
#define HEDLEY_TI_CL430_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_CL430_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_CL430_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_ARMCL_VERSION)
#undef HEDLEY_TI_ARMCL_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && \
	(defined(__TMS470__) || defined(__TI_ARM__))
#define HEDLEY_TI_ARMCL_VERSION                                           \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_ARMCL_VERSION_CHECK)
#undef HEDLEY_TI_ARMCL_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_ARMCL_VERSION)
#define HEDLEY_TI_ARMCL_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_ARMCL_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_ARMCL_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_CL6X_VERSION)
#undef HEDLEY_TI_CL6X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__TMS320C6X__)
#define HEDLEY_TI_CL6X_VERSION                                            \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL6X_VERSION_CHECK)
#undef HEDLEY_TI_CL6X_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL6X_VERSION)
#define HEDLEY_TI_CL6X_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_CL6X_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_CL6X_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_CL7X_VERSION)
#undef HEDLEY_TI_CL7X_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__C7000__)
#define HEDLEY_TI_CL7X_VERSION                                            \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CL7X_VERSION_CHECK)
#undef HEDLEY_TI_CL7X_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CL7X_VERSION)
#define HEDLEY_TI_CL7X_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_CL7X_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_CL7X_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TI_CLPRU_VERSION)
#undef HEDLEY_TI_CLPRU_VERSION
#endif
#if defined(__TI_COMPILER_VERSION__) && defined(__PRU__)
#define HEDLEY_TI_CLPRU_VERSION                                           \
	HEDLEY_VERSION_ENCODE(__TI_COMPILER_VERSION__ / 1000000,          \
			      (__TI_COMPILER_VERSION__ % 1000000) / 1000, \
			      (__TI_COMPILER_VERSION__ % 1000))
#endif

#if defined(HEDLEY_TI_CLPRU_VERSION_CHECK)
#undef HEDLEY_TI_CLPRU_VERSION_CHECK
#endif
#if defined(HEDLEY_TI_CLPRU_VERSION)
#define HEDLEY_TI_CLPRU_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TI_CLPRU_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TI_CLPRU_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_CRAY_VERSION)
#undef HEDLEY_CRAY_VERSION
#endif
#if defined(_CRAYC)
#if defined(_RELEASE_PATCHLEVEL)
#define HEDLEY_CRAY_VERSION                                   \
	HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, \
			      _RELEASE_PATCHLEVEL)
#else
#define HEDLEY_CRAY_VERSION \
	HEDLEY_VERSION_ENCODE(_RELEASE_MAJOR, _RELEASE_MINOR, 0)
#endif
#endif

#if defined(HEDLEY_CRAY_VERSION_CHECK)
#undef HEDLEY_CRAY_VERSION_CHECK
#endif
#if defined(HEDLEY_CRAY_VERSION)
#define HEDLEY_CRAY_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_CRAY_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_CRAY_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_IAR_VERSION)
#undef HEDLEY_IAR_VERSION
#endif
#if defined(__IAR_SYSTEMS_ICC__)
#if __VER__ > 1000
#define HEDLEY_IAR_VERSION                                                    \
	HEDLEY_VERSION_ENCODE((__VER__ / 1000000), ((__VER__ / 1000) % 1000), \
			      (__VER__ % 1000))
#else
#define HEDLEY_IAR_VERSION HEDLEY_VERSION_ENCODE(VER / 100, __VER__ % 100, 0)
#endif
#endif

#if defined(HEDLEY_IAR_VERSION_CHECK)
#undef HEDLEY_IAR_VERSION_CHECK
#endif
#if defined(HEDLEY_IAR_VERSION)
#define HEDLEY_IAR_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_IAR_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_IAR_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_TINYC_VERSION)
#undef HEDLEY_TINYC_VERSION
#endif
#if defined(__TINYC__)
#define HEDLEY_TINYC_VERSION                                            \
	HEDLEY_VERSION_ENCODE(__TINYC__ / 1000, (__TINYC__ / 100) % 10, \
			      __TINYC__ % 100)
#endif

#if defined(HEDLEY_TINYC_VERSION_CHECK)
#undef HEDLEY_TINYC_VERSION_CHECK
#endif
#if defined(HEDLEY_TINYC_VERSION)
#define HEDLEY_TINYC_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_TINYC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_TINYC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_DMC_VERSION)
#undef HEDLEY_DMC_VERSION
#endif
#if defined(__DMC__)
#define HEDLEY_DMC_VERSION \
	HEDLEY_VERSION_ENCODE(__DMC__ >> 8, (__DMC__ >> 4) & 0xf, __DMC__ & 0xf)
#endif

#if defined(HEDLEY_DMC_VERSION_CHECK)
#undef HEDLEY_DMC_VERSION_CHECK
#endif
#if defined(HEDLEY_DMC_VERSION)
#define HEDLEY_DMC_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_DMC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_DMC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_COMPCERT_VERSION)
#undef HEDLEY_COMPCERT_VERSION
#endif
#if defined(__COMPCERT_VERSION__)
#define HEDLEY_COMPCERT_VERSION                                   \
	HEDLEY_VERSION_ENCODE(__COMPCERT_VERSION__ / 10000,       \
			      (__COMPCERT_VERSION__ / 100) % 100, \
			      __COMPCERT_VERSION__ % 100)
#endif

#if defined(HEDLEY_COMPCERT_VERSION_CHECK)
#undef HEDLEY_COMPCERT_VERSION_CHECK
#endif
#if defined(HEDLEY_COMPCERT_VERSION)
#define HEDLEY_COMPCERT_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_COMPCERT_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_COMPCERT_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_PELLES_VERSION)
#undef HEDLEY_PELLES_VERSION
#endif
#if defined(__POCC__)
#define HEDLEY_PELLES_VERSION \
	HEDLEY_VERSION_ENCODE(__POCC__ / 100, __POCC__ % 100, 0)
#endif

#if defined(HEDLEY_PELLES_VERSION_CHECK)
#undef HEDLEY_PELLES_VERSION_CHECK
#endif
#if defined(HEDLEY_PELLES_VERSION)
#define HEDLEY_PELLES_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_PELLES_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_PELLES_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_GCC_VERSION)
#undef HEDLEY_GCC_VERSION
#endif
#if defined(HEDLEY_GNUC_VERSION) && !defined(__clang__) &&                \
	!defined(HEDLEY_INTEL_VERSION) && !defined(HEDLEY_PGI_VERSION) && \
	!defined(HEDLEY_ARM_VERSION) && !defined(HEDLEY_TI_VERSION) &&    \
	!defined(HEDLEY_TI_ARMCL_VERSION) &&                              \
	!defined(HEDLEY_TI_CL430_VERSION) &&                              \
	!defined(HEDLEY_TI_CL2000_VERSION) &&                             \
	!defined(HEDLEY_TI_CL6X_VERSION) &&                               \
	!defined(HEDLEY_TI_CL7X_VERSION) &&                               \
	!defined(HEDLEY_TI_CLPRU_VERSION) && !defined(__COMPCERT__)
#define HEDLEY_GCC_VERSION HEDLEY_GNUC_VERSION
#endif

#if defined(HEDLEY_GCC_VERSION_CHECK)
#undef HEDLEY_GCC_VERSION_CHECK
#endif
#if defined(HEDLEY_GCC_VERSION)
#define HEDLEY_GCC_VERSION_CHECK(major, minor, patch) \
	(HEDLEY_GCC_VERSION >= HEDLEY_VERSION_ENCODE(major, minor, patch))
#else
#define HEDLEY_GCC_VERSION_CHECK(major, minor, patch) (0)
#endif

#if defined(HEDLEY_HAS_ATTRIBUTE)
#undef HEDLEY_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#define HEDLEY_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)
#else
#define HEDLEY_HAS_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_ATTRIBUTE)
#undef HEDLEY_GNUC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#define HEDLEY_GNUC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
	__has_attribute(attribute)
#else
#define HEDLEY_GNUC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_ATTRIBUTE)
#undef HEDLEY_GCC_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#define HEDLEY_GCC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
	__has_attribute(attribute)
#else
#define HEDLEY_GCC_HAS_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_CPP_ATTRIBUTE)
#undef HEDLEY_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus) && \
	(!defined(HEDLEY_SUNPRO_VERSION) ||                 \
	 HEDLEY_SUNPRO_VERSION_CHECK(5, 15, 0))
#define HEDLEY_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)
#else
#define HEDLEY_HAS_CPP_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_HAS_CPP_ATTRIBUTE_NS)
#undef HEDLEY_HAS_CPP_ATTRIBUTE_NS
#endif
#if !defined(__cplusplus) || !defined(__has_cpp_attribute)
#define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns, attribute) (0)
#elif !defined(HEDLEY_PGI_VERSION) && !defined(HEDLEY_IAR_VERSION) && \
	(!defined(HEDLEY_SUNPRO_VERSION) ||                           \
	 HEDLEY_SUNPRO_VERSION_CHECK(5, 15, 0)) &&                    \
	(!defined(HEDLEY_MSVC_VERSION) ||                             \
	 HEDLEY_MSVC_VERSION_CHECK(19, 20, 0))
#define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns, attribute) \
	HEDLEY_HAS_CPP_ATTRIBUTE(ns::attribute)
#else
#define HEDLEY_HAS_CPP_ATTRIBUTE_NS(ns, attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_CPP_ATTRIBUTE)
#undef HEDLEY_GNUC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#define HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute, major, minor, patch) \
	__has_cpp_attribute(attribute)
#else
#define HEDLEY_GNUC_HAS_CPP_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_CPP_ATTRIBUTE)
#undef HEDLEY_GCC_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#define HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute, major, minor, patch) \
	__has_cpp_attribute(attribute)
#else
#define HEDLEY_GCC_HAS_CPP_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_BUILTIN)
#undef HEDLEY_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define HEDLEY_HAS_BUILTIN(builtin) __has_builtin(builtin)
#else
#define HEDLEY_HAS_BUILTIN(builtin) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_BUILTIN)
#undef HEDLEY_GNUC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define HEDLEY_GNUC_HAS_BUILTIN(builtin, major, minor, patch) \
	__has_builtin(builtin)
#else
#define HEDLEY_GNUC_HAS_BUILTIN(builtin, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_BUILTIN)
#undef HEDLEY_GCC_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#define HEDLEY_GCC_HAS_BUILTIN(builtin, major, minor, patch) \
	__has_builtin(builtin)
#else
#define HEDLEY_GCC_HAS_BUILTIN(builtin, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_FEATURE)
#undef HEDLEY_HAS_FEATURE
#endif
#if defined(__has_feature)
#define HEDLEY_HAS_FEATURE(feature) __has_feature(feature)
#else
#define HEDLEY_HAS_FEATURE(feature) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_FEATURE)
#undef HEDLEY_GNUC_HAS_FEATURE
#endif
#if defined(__has_feature)
#define HEDLEY_GNUC_HAS_FEATURE(feature, major, minor, patch) \
	__has_feature(feature)
#else
#define HEDLEY_GNUC_HAS_FEATURE(feature, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_FEATURE)
#undef HEDLEY_GCC_HAS_FEATURE
#endif
#if defined(__has_feature)
#define HEDLEY_GCC_HAS_FEATURE(feature, major, minor, patch) \
	__has_feature(feature)
#else
#define HEDLEY_GCC_HAS_FEATURE(feature, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_EXTENSION)
#undef HEDLEY_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define HEDLEY_HAS_EXTENSION(extension) __has_extension(extension)
#else
#define HEDLEY_HAS_EXTENSION(extension) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_EXTENSION)
#undef HEDLEY_GNUC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define HEDLEY_GNUC_HAS_EXTENSION(extension, major, minor, patch) \
	__has_extension(extension)
#else
#define HEDLEY_GNUC_HAS_EXTENSION(extension, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_EXTENSION)
#undef HEDLEY_GCC_HAS_EXTENSION
#endif
#if defined(__has_extension)
#define HEDLEY_GCC_HAS_EXTENSION(extension, major, minor, patch) \
	__has_extension(extension)
#else
#define HEDLEY_GCC_HAS_EXTENSION(extension, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_DECLSPEC_ATTRIBUTE)
#undef HEDLEY_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) \
	__has_declspec_attribute(attribute)
#else
#define HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE)
#undef HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute, major, minor, patch) \
	__has_declspec_attribute(attribute)
#else
#define HEDLEY_GNUC_HAS_DECLSPEC_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE)
#undef HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE
#endif
#if defined(__has_declspec_attribute)
#define HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute, major, minor, patch) \
	__has_declspec_attribute(attribute)
#else
#define HEDLEY_GCC_HAS_DECLSPEC_ATTRIBUTE(attribute, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_HAS_WARNING)
#undef HEDLEY_HAS_WARNING
#endif
#if defined(__has_warning)
#define HEDLEY_HAS_WARNING(warning) __has_warning(warning)
#else
#define HEDLEY_HAS_WARNING(warning) (0)
#endif

#if defined(HEDLEY_GNUC_HAS_WARNING)
#undef HEDLEY_GNUC_HAS_WARNING
#endif
#if defined(__has_warning)
#define HEDLEY_GNUC_HAS_WARNING(warning, major, minor, patch) \
	__has_warning(warning)
#else
#define HEDLEY_GNUC_HAS_WARNING(warning, major, minor, patch) \
	HEDLEY_GNUC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_GCC_HAS_WARNING)
#undef HEDLEY_GCC_HAS_WARNING
#endif
#if defined(__has_warning)
#define HEDLEY_GCC_HAS_WARNING(warning, major, minor, patch) \
	__has_warning(warning)
#else
#define HEDLEY_GCC_HAS_WARNING(warning, major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
	defined(__clang__) || HEDLEY_GCC_VERSION_CHECK(3, 0, 0) ||  \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                     \
	HEDLEY_IAR_VERSION_CHECK(8, 0, 0) ||                        \
	HEDLEY_PGI_VERSION_CHECK(18, 4, 0) ||                       \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                        \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                       \
	HEDLEY_TI_ARMCL_VERSION_CHECK(4, 7, 0) ||                   \
	HEDLEY_TI_CL430_VERSION_CHECK(2, 0, 1) ||                   \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 1, 0) ||                  \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 0, 0) ||                    \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                    \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0) ||                   \
	HEDLEY_CRAY_VERSION_CHECK(5, 0, 0) ||                       \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 17) ||                     \
	HEDLEY_SUNPRO_VERSION_CHECK(8, 0, 0) ||                     \
	(HEDLEY_IBM_VERSION_CHECK(10, 1, 0) && defined(__C99_PRAGMA_OPERATOR))
#define HEDLEY_PRAGMA(value) _Pragma(#value)
#elif HEDLEY_MSVC_VERSION_CHECK(15, 0, 0)
#define HEDLEY_PRAGMA(value) __pragma(value)
#else
#define HEDLEY_PRAGMA(value)
#endif

#if defined(HEDLEY_DIAGNOSTIC_PUSH)
#undef HEDLEY_DIAGNOSTIC_PUSH
#endif
#if defined(HEDLEY_DIAGNOSTIC_POP)
#undef HEDLEY_DIAGNOSTIC_POP
#endif
#if defined(__clang__)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#elif HEDLEY_GCC_VERSION_CHECK(4, 6, 0)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif HEDLEY_MSVC_VERSION_CHECK(15, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DIAGNOSTIC_PUSH __pragma(warning(push))
#define HEDLEY_DIAGNOSTIC_POP __pragma(warning(pop))
#elif HEDLEY_ARM_VERSION_CHECK(5, 6, 0)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("push")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("pop")
#elif HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||       \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) || \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 4, 0) || \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 1, 0) ||  \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||  \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("diag_push")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("diag_pop")
#elif HEDLEY_PELLES_VERSION_CHECK(2, 90, 0)
#define HEDLEY_DIAGNOSTIC_PUSH _Pragma("warning(push)")
#define HEDLEY_DIAGNOSTIC_POP _Pragma("warning(pop)")
#else
#define HEDLEY_DIAGNOSTIC_PUSH
#define HEDLEY_DIAGNOSTIC_POP
#endif

/* HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#undef HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_
#endif
#if defined(__cplusplus)
#if HEDLEY_HAS_WARNING("-Wc++98-compat")
#if HEDLEY_HAS_WARNING("-Wc++17-extensions")
#if HEDLEY_HAS_WARNING("-Wc++1z-extensions")
#define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr)                  \
	HEDLEY_DIAGNOSTIC_PUSH                                             \
	_Pragma("clang diagnostic ignored \"-Wc++98-compat\"") _Pragma(    \
		"clang diagnostic ignored \"-Wc++17-extensions\"")         \
		_Pragma("clang diagnostic ignored \"-Wc++1z-extensions\"") \
			xpr HEDLEY_DIAGNOSTIC_POP
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr)                  \
	HEDLEY_DIAGNOSTIC_PUSH                                             \
	_Pragma("clang diagnostic ignored \"-Wc++98-compat\"")             \
		_Pragma("clang diagnostic ignored \"-Wc++17-extensions\"") \
			xpr HEDLEY_DIAGNOSTIC_POP
#endif
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(xpr)      \
	HEDLEY_DIAGNOSTIC_PUSH                                 \
	_Pragma("clang diagnostic ignored \"-Wc++98-compat\"") \
		xpr HEDLEY_DIAGNOSTIC_POP
#endif
#endif
#endif
#if !defined(HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_)
#define HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(x) x
#endif

#if defined(HEDLEY_CONST_CAST)
#undef HEDLEY_CONST_CAST
#endif
#if defined(__cplusplus)
#define HEDLEY_CONST_CAST(T, expr) (const_cast<T>(expr))
#elif HEDLEY_HAS_WARNING("-Wcast-qual") ||   \
	HEDLEY_GCC_VERSION_CHECK(4, 6, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_CONST_CAST(T, expr)                              \
	(__extension__({                                        \
		HEDLEY_DIAGNOSTIC_PUSH                          \
		HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL((T)(expr)); \
		HEDLEY_DIAGNOSTIC_POP                           \
	}))
#else
#define HEDLEY_CONST_CAST(T, expr) ((T)(expr))
#endif

#if defined(HEDLEY_REINTERPRET_CAST)
#undef HEDLEY_REINTERPRET_CAST
#endif
#if defined(__cplusplus)
#define HEDLEY_REINTERPRET_CAST(T, expr) (reinterpret_cast<T>(expr))
#else
#define HEDLEY_REINTERPRET_CAST(T, expr) ((T)(expr))
#endif

#if defined(HEDLEY_STATIC_CAST)
#undef HEDLEY_STATIC_CAST
#endif
#if defined(__cplusplus)
#define HEDLEY_STATIC_CAST(T, expr) (static_cast<T>(expr))
#else
#define HEDLEY_STATIC_CAST(T, expr) ((T)(expr))
#endif

#if defined(HEDLEY_CPP_CAST)
#undef HEDLEY_CPP_CAST
#endif
#if defined(__cplusplus)
#if HEDLEY_HAS_WARNING("-Wold-style-cast")
#define HEDLEY_CPP_CAST(T, expr)                                            \
	HEDLEY_DIAGNOSTIC_PUSH                                              \
	_Pragma("clang diagnostic ignored \"-Wold-style-cast\"")((T)(expr)) \
		HEDLEY_DIAGNOSTIC_POP
#elif HEDLEY_IAR_VERSION_CHECK(8, 3, 0)
#define HEDLEY_CPP_CAST(T, expr) \
	HEDLEY_DIAGNOSTIC_PUSH   \
	_Pragma("diag_suppress=Pe137") HEDLEY_DIAGNOSTIC_POP
#else
#define HEDLEY_CPP_CAST(T, expr) ((T)(expr))
#endif
#else
#define HEDLEY_CPP_CAST(T, expr) (expr)
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED)
#undef HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif
#if HEDLEY_HAS_WARNING("-Wdeprecated-declarations")
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("warning(disable:1478 1786)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	__pragma(warning(disable : 1478 1786))
#elif HEDLEY_PGI_VERSION_CHECK(20, 7, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("diag_suppress 1215,1216,1444,1445")
#elif HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1215,1444")
#elif HEDLEY_GCC_VERSION_CHECK(4, 3, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_MSVC_VERSION_CHECK(15, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED __pragma(warning(disable : 4996))
#elif HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||         \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&  \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||   \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) && \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||  \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&  \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||   \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&   \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||    \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||    \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("diag_suppress 1291,1718")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 13, 0) && !defined(__cplusplus)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("error_messages(off,E_DEPRECATED_ATT,E_DEPRECATED_ATT_MESS)")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 13, 0) && defined(__cplusplus)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("error_messages(off,symdeprecated,symdeprecated2)")
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED \
	_Pragma("diag_suppress=Pe1444,Pe1215")
#elif HEDLEY_PELLES_VERSION_CHECK(2, 90, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED _Pragma("warn(disable:2241)")
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS)
#undef HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	_Pragma("clang diagnostic ignored \"-Wunknown-pragmas\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	_Pragma("warning(disable:161)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	__pragma(warning(disable : 161))
#elif HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 1675")
#elif HEDLEY_GCC_VERSION_CHECK(4, 3, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	_Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")
#elif HEDLEY_MSVC_VERSION_CHECK(15, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	__pragma(warning(disable : 4068))
#elif HEDLEY_TI_VERSION_CHECK(16, 9, 0) ||       \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 0, 0) || \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) || \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 3, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif HEDLEY_TI_CL6X_VERSION_CHECK(8, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress 163")
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS _Pragma("diag_suppress=Pe161")
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES)
#undef HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-attributes")
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("clang diagnostic ignored \"-Wunknown-attributes\"")
#elif HEDLEY_GCC_VERSION_CHECK(4, 6, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif HEDLEY_INTEL_VERSION_CHECK(17, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("warning(disable:1292)")
#elif HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	__pragma(warning(disable : 1292))
#elif HEDLEY_MSVC_VERSION_CHECK(19, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	__pragma(warning(disable : 5030))
#elif HEDLEY_PGI_VERSION_CHECK(20, 7, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("diag_suppress 1097,1098")
#elif HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("diag_suppress 1097")
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 14, 0) && defined(__cplusplus)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("error_messages(off,attrskipunsup)")
#elif HEDLEY_TI_VERSION_CHECK(18, 1, 0) ||       \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 3, 0) || \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("diag_suppress 1173")
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES \
	_Pragma("diag_suppress=Pe1097")
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_CPP_ATTRIBUTES
#endif

#if defined(HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL)
#undef HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif
#if HEDLEY_HAS_WARNING("-Wcast-qual")
#define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
	_Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
	_Pragma("warning(disable:2203 2331)")
#elif HEDLEY_GCC_VERSION_CHECK(3, 0, 0)
#define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL \
	_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
#define HEDLEY_DIAGNOSTIC_DISABLE_CAST_QUAL
#endif

#if defined(HEDLEY_DEPRECATED)
#undef HEDLEY_DEPRECATED
#endif
#if defined(HEDLEY_DEPRECATED_FOR)
#undef HEDLEY_DEPRECATED_FOR
#endif
#if HEDLEY_MSVC_VERSION_CHECK(14, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DEPRECATED(since) __declspec(deprecated("Since " #since))
#define HEDLEY_DEPRECATED_FOR(since, replacement) \
	__declspec(deprecated("Since " #since "; use " #replacement))
#elif HEDLEY_HAS_EXTENSION(attribute_deprecated_with_message) || \
	HEDLEY_GCC_VERSION_CHECK(4, 5, 0) ||                     \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                  \
	HEDLEY_ARM_VERSION_CHECK(5, 6, 0) ||                     \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 13, 0) ||                 \
	HEDLEY_PGI_VERSION_CHECK(17, 10, 0) ||                   \
	HEDLEY_TI_VERSION_CHECK(18, 1, 0) ||                     \
	HEDLEY_TI_ARMCL_VERSION_CHECK(18, 1, 0) ||               \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 3, 0) ||                 \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                 \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 3, 0)
#define HEDLEY_DEPRECATED(since) \
	__attribute__((__deprecated__("Since " #since)))
#define HEDLEY_DEPRECATED_FOR(since, replacement) \
	__attribute__((__deprecated__("Since " #since "; use " #replacement)))
#elif defined(__cplusplus) && (__cplusplus >= 201402L)
#define HEDLEY_DEPRECATED(since)                      \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_( \
		[[deprecated("Since " #since)]])
#define HEDLEY_DEPRECATED_FOR(since, replacement)     \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_( \
		[[deprecated("Since " #since "; use " #replacement)]])
#elif HEDLEY_HAS_ATTRIBUTE(deprecated) || HEDLEY_GCC_VERSION_CHECK(3, 1, 0) || \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                                   \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                                  \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                             \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                             \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                              \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                            \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                             \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                             \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                             \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                             \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                              \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                              \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                             \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                               \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                               \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_DEPRECATED(since) __attribute__((__deprecated__))
#define HEDLEY_DEPRECATED_FOR(since, replacement) \
	__attribute__((__deprecated__))
#elif HEDLEY_MSVC_VERSION_CHECK(13, 10, 0) ||    \
	HEDLEY_PELLES_VERSION_CHECK(6, 50, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_DEPRECATED(since) __declspec(deprecated)
#define HEDLEY_DEPRECATED_FOR(since, replacement) __declspec(deprecated)
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_DEPRECATED(since) _Pragma("deprecated")
#define HEDLEY_DEPRECATED_FOR(since, replacement) _Pragma("deprecated")
#else
#define HEDLEY_DEPRECATED(since)
#define HEDLEY_DEPRECATED_FOR(since, replacement)
#endif

#if defined(HEDLEY_UNAVAILABLE)
#undef HEDLEY_UNAVAILABLE
#endif
#if HEDLEY_HAS_ATTRIBUTE(warning) || HEDLEY_GCC_VERSION_CHECK(4, 3, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_UNAVAILABLE(available_since) \
	__attribute__((__warning__("Not available until " #available_since)))
#else
#define HEDLEY_UNAVAILABLE(available_since)
#endif

#if defined(HEDLEY_WARN_UNUSED_RESULT)
#undef HEDLEY_WARN_UNUSED_RESULT
#endif
#if defined(HEDLEY_WARN_UNUSED_RESULT_MSG)
#undef HEDLEY_WARN_UNUSED_RESULT_MSG
#endif
#if HEDLEY_HAS_ATTRIBUTE(warn_unused_result) ||                            \
	HEDLEY_GCC_VERSION_CHECK(3, 4, 0) ||                               \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                              \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                          \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                        \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                         \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                          \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                          \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0) ||                          \
	(HEDLEY_SUNPRO_VERSION_CHECK(5, 15, 0) && defined(__cplusplus)) || \
	HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) \
	__attribute__((__warn_unused_result__))
#elif (HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard) >= 201907L)
#define HEDLEY_WARN_UNUSED_RESULT \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard(msg)]])
#elif HEDLEY_HAS_CPP_ATTRIBUTE(nodiscard)
#define HEDLEY_WARN_UNUSED_RESULT \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[nodiscard]])
#elif defined(_Check_return_) /* SAL */
#define HEDLEY_WARN_UNUSED_RESULT _Check_return_
#define HEDLEY_WARN_UNUSED_RESULT_MSG(msg) _Check_return_
#else
#define HEDLEY_WARN_UNUSED_RESULT
#define HEDLEY_WARN_UNUSED_RESULT_MSG(msg)
#endif

#if defined(HEDLEY_SENTINEL)
#undef HEDLEY_SENTINEL
#endif
#if HEDLEY_HAS_ATTRIBUTE(sentinel) || HEDLEY_GCC_VERSION_CHECK(4, 0, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_ARM_VERSION_CHECK(5, 4, 0)
#define HEDLEY_SENTINEL(position) __attribute__((__sentinel__(position)))
#else
#define HEDLEY_SENTINEL(position)
#endif

#if defined(HEDLEY_NO_RETURN)
#undef HEDLEY_NO_RETURN
#endif
#if HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_NO_RETURN __noreturn
#elif HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define HEDLEY_NO_RETURN _Noreturn
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
#define HEDLEY_NO_RETURN \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[noreturn]])
#elif HEDLEY_HAS_ATTRIBUTE(noreturn) || HEDLEY_GCC_VERSION_CHECK(3, 2, 0) || \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                             \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                                 \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                                \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                                \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                           \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                           \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                            \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                          \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                           \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                           \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                           \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                           \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                            \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                            \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                           \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                             \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                             \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_NO_RETURN __attribute__((__noreturn__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 10, 0)
#define HEDLEY_NO_RETURN _Pragma("does_not_return")
#elif HEDLEY_MSVC_VERSION_CHECK(13, 10, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_NO_RETURN __declspec(noreturn)
#elif HEDLEY_TI_CL6X_VERSION_CHECK(6, 0, 0) && defined(__cplusplus)
#define HEDLEY_NO_RETURN _Pragma("FUNC_NEVER_RETURNS;")
#elif HEDLEY_COMPCERT_VERSION_CHECK(3, 2, 0)
#define HEDLEY_NO_RETURN __attribute((noreturn))
#elif HEDLEY_PELLES_VERSION_CHECK(9, 0, 0)
#define HEDLEY_NO_RETURN __declspec(noreturn)
#else
#define HEDLEY_NO_RETURN
#endif

#if defined(HEDLEY_NO_ESCAPE)
#undef HEDLEY_NO_ESCAPE
#endif
#if HEDLEY_HAS_ATTRIBUTE(noescape)
#define HEDLEY_NO_ESCAPE __attribute__((__noescape__))
#else
#define HEDLEY_NO_ESCAPE
#endif

#if defined(HEDLEY_UNREACHABLE)
#undef HEDLEY_UNREACHABLE
#endif
#if defined(HEDLEY_UNREACHABLE_RETURN)
#undef HEDLEY_UNREACHABLE_RETURN
#endif
#if defined(HEDLEY_ASSUME)
#undef HEDLEY_ASSUME
#endif
#if HEDLEY_MSVC_VERSION_CHECK(13, 10, 0) ||     \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_ASSUME(expr) __assume(expr)
#elif HEDLEY_HAS_BUILTIN(__builtin_assume)
#define HEDLEY_ASSUME(expr) __builtin_assume(expr)
#elif HEDLEY_TI_CL2000_VERSION_CHECK(6, 2, 0) || \
	HEDLEY_TI_CL6X_VERSION_CHECK(4, 0, 0)
#if defined(__cplusplus)
#define HEDLEY_ASSUME(expr) std::_nassert(expr)
#else
#define HEDLEY_ASSUME(expr) _nassert(expr)
#endif
#endif
#if (HEDLEY_HAS_BUILTIN(__builtin_unreachable) && \
     (!defined(HEDLEY_ARM_VERSION))) ||           \
	HEDLEY_GCC_VERSION_CHECK(4, 5, 0) ||      \
	HEDLEY_PGI_VERSION_CHECK(18, 10, 0) ||    \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||   \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 5)
#define HEDLEY_UNREACHABLE() __builtin_unreachable()
#elif defined(HEDLEY_ASSUME)
#define HEDLEY_UNREACHABLE() HEDLEY_ASSUME(0)
#endif
#if !defined(HEDLEY_ASSUME)
#if defined(HEDLEY_UNREACHABLE)
#define HEDLEY_ASSUME(expr) \
	HEDLEY_STATIC_CAST(void, ((expr) ? 1 : (HEDLEY_UNREACHABLE(), 1)))
#else
#define HEDLEY_ASSUME(expr) HEDLEY_STATIC_CAST(void, expr)
#endif
#endif
#if defined(HEDLEY_UNREACHABLE)
#if HEDLEY_TI_CL2000_VERSION_CHECK(6, 2, 0) || \
	HEDLEY_TI_CL6X_VERSION_CHECK(4, 0, 0)
#define HEDLEY_UNREACHABLE_RETURN(value) \
	return (HEDLEY_STATIC_CAST(void, HEDLEY_ASSUME(0)), (value))
#else
#define HEDLEY_UNREACHABLE_RETURN(value) HEDLEY_UNREACHABLE()
#endif
#else
#define HEDLEY_UNREACHABLE_RETURN(value) return (value)
#endif
#if !defined(HEDLEY_UNREACHABLE)
#define HEDLEY_UNREACHABLE() HEDLEY_ASSUME(0)
#endif

HEDLEY_DIAGNOSTIC_PUSH
#if HEDLEY_HAS_WARNING("-Wpedantic")
#pragma clang diagnostic ignored "-Wpedantic"
#endif
#if HEDLEY_HAS_WARNING("-Wc++98-compat-pedantic") && defined(__cplusplus)
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif
#if HEDLEY_GCC_HAS_WARNING("-Wvariadic-macros", 4, 0, 0)
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wvariadic-macros"
#elif defined(HEDLEY_GCC_VERSION)
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif
#endif
#if defined(HEDLEY_NON_NULL)
#undef HEDLEY_NON_NULL
#endif
#if HEDLEY_HAS_ATTRIBUTE(nonnull) || HEDLEY_GCC_VERSION_CHECK(3, 3, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                           \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0)
#define HEDLEY_NON_NULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
#define HEDLEY_NON_NULL(...)
#endif
HEDLEY_DIAGNOSTIC_POP

#if defined(HEDLEY_PRINTF_FORMAT)
#undef HEDLEY_PRINTF_FORMAT
#endif
#if defined(__MINGW32__) && HEDLEY_GCC_HAS_ATTRIBUTE(format, 4, 4, 0) && \
	!defined(__USE_MINGW_ANSI_STDIO)
#define HEDLEY_PRINTF_FORMAT(string_idx, first_to_check) \
	__attribute__((__format__(ms_printf, string_idx, first_to_check)))
#elif defined(__MINGW32__) && HEDLEY_GCC_HAS_ATTRIBUTE(format, 4, 4, 0) && \
	defined(__USE_MINGW_ANSI_STDIO)
#define HEDLEY_PRINTF_FORMAT(string_idx, first_to_check) \
	__attribute__((__format__(gnu_printf, string_idx, first_to_check)))
#elif HEDLEY_HAS_ATTRIBUTE(format) || HEDLEY_GCC_VERSION_CHECK(3, 1, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_ARM_VERSION_CHECK(5, 6, 0) ||                               \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                              \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                              \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                          \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                        \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                         \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                          \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                          \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_PRINTF_FORMAT(string_idx, first_to_check) \
	__attribute__((__format__(__printf__, string_idx, first_to_check)))
#elif HEDLEY_PELLES_VERSION_CHECK(6, 0, 0)
#define HEDLEY_PRINTF_FORMAT(string_idx, first_to_check) \
	__declspec(vaformat(printf, string_idx, first_to_check))
#else
#define HEDLEY_PRINTF_FORMAT(string_idx, first_to_check)
#endif

#if defined(HEDLEY_CONSTEXPR)
#undef HEDLEY_CONSTEXPR
#endif
#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define HEDLEY_CONSTEXPR HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(constexpr)
#endif
#endif
#if !defined(HEDLEY_CONSTEXPR)
#define HEDLEY_CONSTEXPR
#endif

#if defined(HEDLEY_PREDICT)
#undef HEDLEY_PREDICT
#endif
#if defined(HEDLEY_LIKELY)
#undef HEDLEY_LIKELY
#endif
#if defined(HEDLEY_UNLIKELY)
#undef HEDLEY_UNLIKELY
#endif
#if defined(HEDLEY_UNPREDICTABLE)
#undef HEDLEY_UNPREDICTABLE
#endif
#if HEDLEY_HAS_BUILTIN(__builtin_unpredictable)
#define HEDLEY_UNPREDICTABLE(expr) __builtin_unpredictable((expr))
#endif
#if (HEDLEY_HAS_BUILTIN(__builtin_expect_with_probability) && \
     !defined(HEDLEY_PGI_VERSION)) ||                         \
	HEDLEY_GCC_VERSION_CHECK(9, 0, 0)
#define HEDLEY_PREDICT(expr, value, probability) \
	__builtin_expect_with_probability((expr), (value), (probability))
#define HEDLEY_PREDICT_TRUE(expr, probability) \
	__builtin_expect_with_probability(!!(expr), 1, (probability))
#define HEDLEY_PREDICT_FALSE(expr, probability) \
	__builtin_expect_with_probability(!!(expr), 0, (probability))
#define HEDLEY_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define HEDLEY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#elif (HEDLEY_HAS_BUILTIN(__builtin_expect) &&                             \
       !defined(HEDLEY_INTEL_CL_VERSION)) ||                               \
	HEDLEY_GCC_VERSION_CHECK(3, 0, 0) ||                               \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	(HEDLEY_SUNPRO_VERSION_CHECK(5, 15, 0) && defined(__cplusplus)) || \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                               \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                              \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                              \
	HEDLEY_TI_ARMCL_VERSION_CHECK(4, 7, 0) ||                          \
	HEDLEY_TI_CL430_VERSION_CHECK(3, 1, 0) ||                          \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 1, 0) ||                         \
	HEDLEY_TI_CL6X_VERSION_CHECK(6, 1, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0) ||                          \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 27) ||                            \
	HEDLEY_CRAY_VERSION_CHECK(8, 1, 0)
#define HEDLEY_PREDICT(expr, expected, probability)     \
	(((probability) >= 0.9)                         \
		 ? __builtin_expect((expr), (expected)) \
		 : (HEDLEY_STATIC_CAST(void, expected), (expr)))
#define HEDLEY_PREDICT_TRUE(expr, probability)                      \
	(__extension__({                                            \
		double hedley_probability_ = (probability);         \
		((hedley_probability_ >= 0.9)                       \
			 ? __builtin_expect(!!(expr), 1)            \
			 : ((hedley_probability_ <= 0.1)            \
				    ? __builtin_expect(!!(expr), 0) \
				    : !!(expr)));                   \
	}))
#define HEDLEY_PREDICT_FALSE(expr, probability)                     \
	(__extension__({                                            \
		double hedley_probability_ = (probability);         \
		((hedley_probability_ >= 0.9)                       \
			 ? __builtin_expect(!!(expr), 0)            \
			 : ((hedley_probability_ <= 0.1)            \
				    ? __builtin_expect(!!(expr), 1) \
				    : !!(expr)));                   \
	}))
#define HEDLEY_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define HEDLEY_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#define HEDLEY_PREDICT(expr, expected, probability) \
	(HEDLEY_STATIC_CAST(void, expected), (expr))
#define HEDLEY_PREDICT_TRUE(expr, probability) (!!(expr))
#define HEDLEY_PREDICT_FALSE(expr, probability) (!!(expr))
#define HEDLEY_LIKELY(expr) (!!(expr))
#define HEDLEY_UNLIKELY(expr) (!!(expr))
#endif
#if !defined(HEDLEY_UNPREDICTABLE)
#define HEDLEY_UNPREDICTABLE(expr) HEDLEY_PREDICT(expr, 1, 0.5)
#endif

#if defined(HEDLEY_MALLOC)
#undef HEDLEY_MALLOC
#endif
#if HEDLEY_HAS_ATTRIBUTE(malloc) || HEDLEY_GCC_VERSION_CHECK(3, 1, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                          \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                         \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                             \
	HEDLEY_IBM_VERSION_CHECK(12, 1, 0) ||                            \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                            \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                       \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                       \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                        \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                      \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                       \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                       \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                       \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                       \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                        \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                        \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                       \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                         \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                         \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_MALLOC __attribute__((__malloc__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 10, 0)
#define HEDLEY_MALLOC _Pragma("returns_new_memory")
#elif HEDLEY_MSVC_VERSION_CHECK(14, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_MALLOC __declspec(restrict)
#else
#define HEDLEY_MALLOC
#endif

#if defined(HEDLEY_PURE)
#undef HEDLEY_PURE
#endif
#if HEDLEY_HAS_ATTRIBUTE(pure) || HEDLEY_GCC_VERSION_CHECK(2, 96, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                         \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                        \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                            \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                           \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                           \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                      \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                       \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                     \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                      \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                      \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                       \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                       \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                        \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                        \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0) ||                       \
	HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_PURE __attribute__((__pure__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 10, 0)
#define HEDLEY_PURE _Pragma("does_not_write_global_data")
#elif defined(__cplusplus) && (HEDLEY_TI_CL430_VERSION_CHECK(2, 0, 1) || \
			       HEDLEY_TI_CL6X_VERSION_CHECK(4, 0, 0) ||  \
			       HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0))
#define HEDLEY_PURE _Pragma("FUNC_IS_PURE;")
#else
#define HEDLEY_PURE
#endif

#if defined(HEDLEY_CONST)
#undef HEDLEY_CONST
#endif
#if HEDLEY_HAS_ATTRIBUTE(const) || HEDLEY_GCC_VERSION_CHECK(2, 5, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                         \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                        \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                            \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                           \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                           \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                      \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                       \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                     \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                      \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                      \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                       \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                       \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                      \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                        \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                        \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0) ||                       \
	HEDLEY_PGI_VERSION_CHECK(17, 10, 0)
#define HEDLEY_CONST __attribute__((__const__))
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 10, 0)
#define HEDLEY_CONST _Pragma("no_side_effect")
#else
#define HEDLEY_CONST HEDLEY_PURE
#endif

#if defined(HEDLEY_RESTRICT)
#undef HEDLEY_RESTRICT
#endif
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
	!defined(__cplusplus)
#define HEDLEY_RESTRICT restrict
#elif HEDLEY_GCC_VERSION_CHECK(3, 1, 0) ||                                 \
	HEDLEY_MSVC_VERSION_CHECK(14, 0, 0) ||                             \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0) ||                       \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                               \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                              \
	HEDLEY_PGI_VERSION_CHECK(17, 10, 0) ||                             \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                          \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 2, 4) ||                         \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 1, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	(HEDLEY_SUNPRO_VERSION_CHECK(5, 14, 0) && defined(__cplusplus)) || \
	HEDLEY_IAR_VERSION_CHECK(8, 0, 0) || defined(__clang__)
#define HEDLEY_RESTRICT __restrict
#elif HEDLEY_SUNPRO_VERSION_CHECK(5, 3, 0) && !defined(__cplusplus)
#define HEDLEY_RESTRICT _Restrict
#else
#define HEDLEY_RESTRICT
#endif

#if defined(HEDLEY_INLINE)
#undef HEDLEY_INLINE
#endif
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || \
	(defined(__cplusplus) && (__cplusplus >= 199711L))
#define HEDLEY_INLINE inline
#elif defined(HEDLEY_GCC_VERSION) || HEDLEY_ARM_VERSION_CHECK(6, 2, 0)
#define HEDLEY_INLINE __inline__
#elif HEDLEY_MSVC_VERSION_CHECK(12, 0, 0) ||         \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0) || \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||         \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 1, 0) ||    \
	HEDLEY_TI_CL430_VERSION_CHECK(3, 1, 0) ||    \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 2, 0) ||   \
	HEDLEY_TI_CL6X_VERSION_CHECK(8, 0, 0) ||     \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||     \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_INLINE __inline
#else
#define HEDLEY_INLINE
#endif

#if defined(HEDLEY_ALWAYS_INLINE)
#undef HEDLEY_ALWAYS_INLINE
#endif
#if HEDLEY_HAS_ATTRIBUTE(always_inline) ||          \
	HEDLEY_GCC_VERSION_CHECK(4, 0, 0) ||        \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||     \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||    \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||        \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||       \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||       \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&  \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||   \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) && \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||  \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&  \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||   \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&   \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||  \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||    \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||    \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_ALWAYS_INLINE __attribute__((__always_inline__)) HEDLEY_INLINE
#elif HEDLEY_MSVC_VERSION_CHECK(12, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_ALWAYS_INLINE __forceinline
#elif defined(__cplusplus) && (HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||  \
			       HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||  \
			       HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) || \
			       HEDLEY_TI_CL6X_VERSION_CHECK(6, 1, 0) ||   \
			       HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||   \
			       HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0))
#define HEDLEY_ALWAYS_INLINE _Pragma("FUNC_ALWAYS_INLINE;")
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_ALWAYS_INLINE _Pragma("inline=forced")
#else
#define HEDLEY_ALWAYS_INLINE HEDLEY_INLINE
#endif

#if defined(HEDLEY_NEVER_INLINE)
#undef HEDLEY_NEVER_INLINE
#endif
#if HEDLEY_HAS_ATTRIBUTE(noinline) || HEDLEY_GCC_VERSION_CHECK(4, 0, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                            \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                           \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                               \
	HEDLEY_IBM_VERSION_CHECK(10, 1, 0) ||                              \
	HEDLEY_TI_VERSION_CHECK(15, 12, 0) ||                              \
	(HEDLEY_TI_ARMCL_VERSION_CHECK(4, 8, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_ARMCL_VERSION_CHECK(5, 2, 0) ||                          \
	(HEDLEY_TI_CL2000_VERSION_CHECK(6, 0, 0) &&                        \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL2000_VERSION_CHECK(6, 4, 0) ||                         \
	(HEDLEY_TI_CL430_VERSION_CHECK(4, 0, 0) &&                         \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL430_VERSION_CHECK(4, 3, 0) ||                          \
	(HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&                          \
	 defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) ||                         \
	HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0) ||                           \
	HEDLEY_TI_CL7X_VERSION_CHECK(1, 2, 0) ||                           \
	HEDLEY_TI_CLPRU_VERSION_CHECK(2, 1, 0)
#define HEDLEY_NEVER_INLINE __attribute__((__noinline__))
#elif HEDLEY_MSVC_VERSION_CHECK(13, 10, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_NEVER_INLINE __declspec(noinline)
#elif HEDLEY_PGI_VERSION_CHECK(10, 2, 0)
#define HEDLEY_NEVER_INLINE _Pragma("noinline")
#elif HEDLEY_TI_CL6X_VERSION_CHECK(6, 0, 0) && defined(__cplusplus)
#define HEDLEY_NEVER_INLINE _Pragma("FUNC_CANNOT_INLINE;")
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_NEVER_INLINE _Pragma("inline=never")
#elif HEDLEY_COMPCERT_VERSION_CHECK(3, 2, 0)
#define HEDLEY_NEVER_INLINE __attribute((noinline))
#elif HEDLEY_PELLES_VERSION_CHECK(9, 0, 0)
#define HEDLEY_NEVER_INLINE __declspec(noinline)
#else
#define HEDLEY_NEVER_INLINE
#endif

#if defined(HEDLEY_PRIVATE)
#undef HEDLEY_PRIVATE
#endif
#if defined(HEDLEY_PUBLIC)
#undef HEDLEY_PUBLIC
#endif
#if defined(HEDLEY_IMPORT)
#undef HEDLEY_IMPORT
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#define HEDLEY_PRIVATE
#define HEDLEY_PUBLIC __declspec(dllexport)
#define HEDLEY_IMPORT __declspec(dllimport)
#else
#if HEDLEY_HAS_ATTRIBUTE(visibility) || HEDLEY_GCC_VERSION_CHECK(3, 3, 0) || \
	HEDLEY_SUNPRO_VERSION_CHECK(5, 11, 0) ||                             \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||                              \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0) ||                                 \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 0) ||                                \
	(defined(__TI_EABI__) && ((HEDLEY_TI_CL6X_VERSION_CHECK(7, 2, 0) &&  \
				   defined(__TI_GNU_ATTRIBUTE_SUPPORT__)) || \
				  HEDLEY_TI_CL6X_VERSION_CHECK(7, 5, 0)))
#define HEDLEY_PRIVATE __attribute__((__visibility__("hidden")))
#define HEDLEY_PUBLIC __attribute__((__visibility__("default")))
#else
#define HEDLEY_PRIVATE
#define HEDLEY_PUBLIC
#endif
#define HEDLEY_IMPORT extern
#endif

#if defined(HEDLEY_NO_THROW)
#undef HEDLEY_NO_THROW
#endif
#if HEDLEY_HAS_ATTRIBUTE(nothrow) || HEDLEY_GCC_VERSION_CHECK(3, 3, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_NO_THROW __attribute__((__nothrow__))
#elif HEDLEY_MSVC_VERSION_CHECK(13, 1, 0) ||         \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0) || \
	HEDLEY_ARM_VERSION_CHECK(4, 1, 0)
#define HEDLEY_NO_THROW __declspec(nothrow)
#else
#define HEDLEY_NO_THROW
#endif

#if defined(HEDLEY_FALL_THROUGH)
#undef HEDLEY_FALL_THROUGH
#endif
#if HEDLEY_HAS_ATTRIBUTE(fallthrough) || HEDLEY_GCC_VERSION_CHECK(7, 0, 0)
#define HEDLEY_FALL_THROUGH __attribute__((__fallthrough__))
#elif HEDLEY_HAS_CPP_ATTRIBUTE_NS(clang, fallthrough)
#define HEDLEY_FALL_THROUGH \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[clang::fallthrough]])
#elif HEDLEY_HAS_CPP_ATTRIBUTE(fallthrough)
#define HEDLEY_FALL_THROUGH \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_([[fallthrough]])
#elif defined(__fallthrough) /* SAL */
#define HEDLEY_FALL_THROUGH __fallthrough
#else
#define HEDLEY_FALL_THROUGH
#endif

#if defined(HEDLEY_RETURNS_NON_NULL)
#undef HEDLEY_RETURNS_NON_NULL
#endif
#if HEDLEY_HAS_ATTRIBUTE(returns_nonnull) || HEDLEY_GCC_VERSION_CHECK(4, 9, 0)
#define HEDLEY_RETURNS_NON_NULL __attribute__((__returns_nonnull__))
#elif defined(_Ret_notnull_) /* SAL */
#define HEDLEY_RETURNS_NON_NULL _Ret_notnull_
#else
#define HEDLEY_RETURNS_NON_NULL
#endif

#if defined(HEDLEY_ARRAY_PARAM)
#undef HEDLEY_ARRAY_PARAM
#endif
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && \
	!defined(__STDC_NO_VLA__) && !defined(__cplusplus) &&     \
	!defined(HEDLEY_PGI_VERSION) && !defined(HEDLEY_TINYC_VERSION)
#define HEDLEY_ARRAY_PARAM(name) (name)
#else
#define HEDLEY_ARRAY_PARAM(name)
#endif

#if defined(HEDLEY_IS_CONSTANT)
#undef HEDLEY_IS_CONSTANT
#endif
#if defined(HEDLEY_REQUIRE_CONSTEXPR)
#undef HEDLEY_REQUIRE_CONSTEXPR
#endif
/* HEDLEY_IS_CONSTEXPR_ is for
   HEDLEY INTERNAL USE ONLY.  API subject to change without notice. */
#if defined(HEDLEY_IS_CONSTEXPR_)
#undef HEDLEY_IS_CONSTEXPR_
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
#define HEDLEY_IS_CONSTANT(expr) __builtin_constant_p(expr)
#endif
#if !defined(__cplusplus)
#if HEDLEY_HAS_BUILTIN(__builtin_types_compatible_p) || \
	HEDLEY_GCC_VERSION_CHECK(3, 4, 0) ||            \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) ||         \
	HEDLEY_IBM_VERSION_CHECK(13, 1, 0) ||           \
	HEDLEY_CRAY_VERSION_CHECK(8, 1, 0) ||           \
	HEDLEY_ARM_VERSION_CHECK(5, 4, 0) ||            \
	HEDLEY_TINYC_VERSION_CHECK(0, 9, 24)
#if defined(__INTPTR_TYPE__)
#define HEDLEY_IS_CONSTEXPR_(expr)                                    \
	__builtin_types_compatible_p(                                 \
		__typeof__((1 ? (void *)((__INTPTR_TYPE__)((expr)*0)) \
			      : (int *)0)),                           \
		int *)
#else
#include <stdint.h>
#define HEDLEY_IS_CONSTEXPR_(expr)                                           \
	__builtin_types_compatible_p(                                        \
		__typeof__((1 ? (void *)((intptr_t)((expr)*0)) : (int *)0)), \
		int *)
#endif
#elif (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) &&      \
       !defined(HEDLEY_SUNPRO_VERSION) && !defined(HEDLEY_PGI_VERSION) && \
       !defined(HEDLEY_IAR_VERSION)) ||                                   \
	HEDLEY_HAS_EXTENSION(c_generic_selections) ||                     \
	HEDLEY_GCC_VERSION_CHECK(4, 9, 0) ||                              \
	HEDLEY_INTEL_VERSION_CHECK(17, 0, 0) ||                           \
	HEDLEY_IBM_VERSION_CHECK(12, 1, 0) ||                             \
	HEDLEY_ARM_VERSION_CHECK(5, 3, 0)
#if defined(__INTPTR_TYPE__)
#define HEDLEY_IS_CONSTEXPR_(expr)                                       \
	_Generic((1 ? (void *)((__INTPTR_TYPE__)((expr)*0)) : (int *)0), \
		 int * : 1, void * : 0)
#else
#include <stdint.h>
#define HEDLEY_IS_CONSTEXPR_(expr) \
	_Generic((1 ? (void *)((intptr_t)*0) : (int *)0), int * : 1, void * : 0)
#endif
#elif defined(HEDLEY_GCC_VERSION) || defined(HEDLEY_INTEL_VERSION) ||         \
	defined(HEDLEY_TINYC_VERSION) || defined(HEDLEY_TI_ARMCL_VERSION) ||  \
	HEDLEY_TI_CL430_VERSION_CHECK(18, 12, 0) ||                           \
	defined(HEDLEY_TI_CL2000_VERSION) ||                                  \
	defined(HEDLEY_TI_CL6X_VERSION) || defined(HEDLEY_TI_CL7X_VERSION) || \
	defined(HEDLEY_TI_CLPRU_VERSION) || defined(__clang__)
#define HEDLEY_IS_CONSTEXPR_(expr)                                       \
	(sizeof(void) != sizeof(*(1 ? ((void *)((expr)*0L)) : ((struct { \
		 char v[sizeof(void) * 2];                               \
	 } *)1))))
#endif
#endif
#if defined(HEDLEY_IS_CONSTEXPR_)
#if !defined(HEDLEY_IS_CONSTANT)
#define HEDLEY_IS_CONSTANT(expr) HEDLEY_IS_CONSTEXPR_(expr)
#endif
#define HEDLEY_REQUIRE_CONSTEXPR(expr) \
	(HEDLEY_IS_CONSTEXPR_(expr) ? (expr) : (-1))
#else
#if !defined(HEDLEY_IS_CONSTANT)
#define HEDLEY_IS_CONSTANT(expr) (0)
#endif
#define HEDLEY_REQUIRE_CONSTEXPR(expr) (expr)
#endif

#if defined(HEDLEY_BEGIN_C_DECLS)
#undef HEDLEY_BEGIN_C_DECLS
#endif
#if defined(HEDLEY_END_C_DECLS)
#undef HEDLEY_END_C_DECLS
#endif
#if defined(HEDLEY_C_DECL)
#undef HEDLEY_C_DECL
#endif
#if defined(__cplusplus)
#define HEDLEY_BEGIN_C_DECLS extern "C" {
#define HEDLEY_END_C_DECLS }
#define HEDLEY_C_DECL extern "C"
#else
#define HEDLEY_BEGIN_C_DECLS
#define HEDLEY_END_C_DECLS
#define HEDLEY_C_DECL
#endif

#if defined(HEDLEY_STATIC_ASSERT)
#undef HEDLEY_STATIC_ASSERT
#endif
#if !defined(__cplusplus) &&                                             \
	((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)) || \
	 (HEDLEY_HAS_FEATURE(c_static_assert) &&                         \
	  !defined(HEDLEY_INTEL_CL_VERSION)) ||                          \
	 HEDLEY_GCC_VERSION_CHECK(6, 0, 0) ||                            \
	 HEDLEY_INTEL_VERSION_CHECK(13, 0, 0) || defined(_Static_assert))
#define HEDLEY_STATIC_ASSERT(expr, message) _Static_assert(expr, message)
#elif (defined(__cplusplus) && (__cplusplus >= 201103L)) || \
	HEDLEY_MSVC_VERSION_CHECK(16, 0, 0) ||              \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_STATIC_ASSERT(expr, message)           \
	HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_( \
		static_assert(expr, message))
#else
#define HEDLEY_STATIC_ASSERT(expr, message)
#endif

#if defined(HEDLEY_NULL)
#undef HEDLEY_NULL
#endif
#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define HEDLEY_NULL HEDLEY_DIAGNOSTIC_DISABLE_CPP98_COMPAT_WRAP_(nullptr)
#elif defined(NULL)
#define HEDLEY_NULL NULL
#else
#define HEDLEY_NULL HEDLEY_STATIC_CAST(void *, 0)
#endif
#elif defined(NULL)
#define HEDLEY_NULL NULL
#else
#define HEDLEY_NULL ((void *)0)
#endif

#if defined(HEDLEY_MESSAGE)
#undef HEDLEY_MESSAGE
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#define HEDLEY_MESSAGE(msg)                       \
	HEDLEY_DIAGNOSTIC_PUSH                    \
	HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	HEDLEY_PRAGMA(message msg)                \
	HEDLEY_DIAGNOSTIC_POP
#elif HEDLEY_GCC_VERSION_CHECK(4, 4, 0) || HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message msg)
#elif HEDLEY_CRAY_VERSION_CHECK(5, 0, 0)
#define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(_CRI message msg)
#elif HEDLEY_IAR_VERSION_CHECK(8, 0, 0)
#define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message(msg))
#elif HEDLEY_PELLES_VERSION_CHECK(2, 0, 0)
#define HEDLEY_MESSAGE(msg) HEDLEY_PRAGMA(message(msg))
#else
#define HEDLEY_MESSAGE(msg)
#endif

#if defined(HEDLEY_WARNING)
#undef HEDLEY_WARNING
#endif
#if HEDLEY_HAS_WARNING("-Wunknown-pragmas")
#define HEDLEY_WARNING(msg)                       \
	HEDLEY_DIAGNOSTIC_PUSH                    \
	HEDLEY_DIAGNOSTIC_DISABLE_UNKNOWN_PRAGMAS \
	HEDLEY_PRAGMA(clang warning msg)          \
	HEDLEY_DIAGNOSTIC_POP
#elif HEDLEY_GCC_VERSION_CHECK(4, 8, 0) ||    \
	HEDLEY_PGI_VERSION_CHECK(18, 4, 0) || \
	HEDLEY_INTEL_VERSION_CHECK(13, 0, 0)
#define HEDLEY_WARNING(msg) HEDLEY_PRAGMA(GCC warning msg)
#elif HEDLEY_MSVC_VERSION_CHECK(15, 0, 0) || \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_WARNING(msg) HEDLEY_PRAGMA(message(msg))
#else
#define HEDLEY_WARNING(msg) HEDLEY_MESSAGE(msg)
#endif

#if defined(HEDLEY_REQUIRE)
#undef HEDLEY_REQUIRE
#endif
#if defined(HEDLEY_REQUIRE_MSG)
#undef HEDLEY_REQUIRE_MSG
#endif
#if HEDLEY_HAS_ATTRIBUTE(diagnose_if)
#if HEDLEY_HAS_WARNING("-Wgcc-compat")
#define HEDLEY_REQUIRE(expr)                                          \
	HEDLEY_DIAGNOSTIC_PUSH                                        \
	_Pragma("clang diagnostic ignored \"-Wgcc-compat\"")          \
		__attribute__((diagnose_if(!(expr), #expr, "error"))) \
			HEDLEY_DIAGNOSTIC_POP
#define HEDLEY_REQUIRE_MSG(expr, msg)                               \
	HEDLEY_DIAGNOSTIC_PUSH                                      \
	_Pragma("clang diagnostic ignored \"-Wgcc-compat\"")        \
		__attribute__((diagnose_if(!(expr), msg, "error"))) \
			HEDLEY_DIAGNOSTIC_POP
#else
#define HEDLEY_REQUIRE(expr) \
	__attribute__((diagnose_if(!(expr), #expr, "error")))
#define HEDLEY_REQUIRE_MSG(expr, msg) \
	__attribute__((diagnose_if(!(expr), msg, "error")))
#endif
#else
#define HEDLEY_REQUIRE(expr)
#define HEDLEY_REQUIRE_MSG(expr, msg)
#endif

#if defined(HEDLEY_FLAGS)
#undef HEDLEY_FLAGS
#endif
#if HEDLEY_HAS_ATTRIBUTE(flag_enum)
#define HEDLEY_FLAGS __attribute__((__flag_enum__))
#else
#define HEDLEY_FLAGS
#endif

#if defined(HEDLEY_FLAGS_CAST)
#undef HEDLEY_FLAGS_CAST
#endif
#if HEDLEY_INTEL_VERSION_CHECK(19, 0, 0)
#define HEDLEY_FLAGS_CAST(T, expr)                          \
	(__extension__({                                    \
		HEDLEY_DIAGNOSTIC_PUSH                      \
		_Pragma("warning(disable:188)")((T)(expr)); \
		HEDLEY_DIAGNOSTIC_POP                       \
	}))
#else
#define HEDLEY_FLAGS_CAST(T, expr) HEDLEY_STATIC_CAST(T, expr)
#endif

#if defined(HEDLEY_EMPTY_BASES)
#undef HEDLEY_EMPTY_BASES
#endif
#if (HEDLEY_MSVC_VERSION_CHECK(19, 0, 23918) && \
     !HEDLEY_MSVC_VERSION_CHECK(20, 0, 0)) ||   \
	HEDLEY_INTEL_CL_VERSION_CHECK(2021, 1, 0)
#define HEDLEY_EMPTY_BASES __declspec(empty_bases)
#else
#define HEDLEY_EMPTY_BASES
#endif

/* Remaining macros are deprecated. */

#if defined(HEDLEY_GCC_NOT_CLANG_VERSION_CHECK)
#undef HEDLEY_GCC_NOT_CLANG_VERSION_CHECK
#endif
#if defined(__clang__)
#define HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major, minor, patch) (0)
#else
#define HEDLEY_GCC_NOT_CLANG_VERSION_CHECK(major, minor, patch) \
	HEDLEY_GCC_VERSION_CHECK(major, minor, patch)
#endif

#if defined(HEDLEY_CLANG_HAS_ATTRIBUTE)
#undef HEDLEY_CLANG_HAS_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_ATTRIBUTE(attribute) HEDLEY_HAS_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_CPP_ATTRIBUTE)
#undef HEDLEY_CLANG_HAS_CPP_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_CPP_ATTRIBUTE(attribute) \
	HEDLEY_HAS_CPP_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_BUILTIN)
#undef HEDLEY_CLANG_HAS_BUILTIN
#endif
#define HEDLEY_CLANG_HAS_BUILTIN(builtin) HEDLEY_HAS_BUILTIN(builtin)

#if defined(HEDLEY_CLANG_HAS_FEATURE)
#undef HEDLEY_CLANG_HAS_FEATURE
#endif
#define HEDLEY_CLANG_HAS_FEATURE(feature) HEDLEY_HAS_FEATURE(feature)

#if defined(HEDLEY_CLANG_HAS_EXTENSION)
#undef HEDLEY_CLANG_HAS_EXTENSION
#endif
#define HEDLEY_CLANG_HAS_EXTENSION(extension) HEDLEY_HAS_EXTENSION(extension)

#if defined(HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE)
#undef HEDLEY_CLANG_HAS_DECLSPEC_DECLSPEC_ATTRIBUTE
#endif
#define HEDLEY_CLANG_HAS_DECLSPEC_ATTRIBUTE(attribute) \
	HEDLEY_HAS_DECLSPEC_ATTRIBUTE(attribute)

#if defined(HEDLEY_CLANG_HAS_WARNING)
#undef HEDLEY_CLANG_HAS_WARNING
#endif
#define HEDLEY_CLANG_HAS_WARNING(warning) HEDLEY_HAS_WARNING(warning)

#endif /* !defined(HEDLEY_VERSION) || (HEDLEY_VERSION < X) */
