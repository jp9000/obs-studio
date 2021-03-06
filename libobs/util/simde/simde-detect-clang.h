/* Detect Clang Version
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

/* This file was originally part of SIMDe
 * (<https://github.com/simd-everywhere/simde>).  You're free to do with it as
 * you please, but I do have a few small requests:
 *
 *  * If you make improvements, please submit them back to SIMDe
 *    (at <https://github.com/simd-everywhere/simde/issues>) so others can
 *    benefit from them.
 *  * Please keep a link to SIMDe intact so people know where to submit
 *    improvements.
 *  * If you expose it publicly, please change the SIMDE_ prefix to
 *    something specific to your project.
 *
 * The version numbers clang exposes (in the ___clang_major__,
 * __clang_minor__, and __clang_patchlevel__ macros) are unreliable.
 * Vendors such as Apple will define these values to their version
 * numbers; for example, "Apple Clang 4.0" is really clang 3.1, but
 * __clang_major__ and __clang_minor__ are defined to 4 and 0
 * respectively, instead of 3 and 1.
 *
 * The solution is *usually* to use clang's feature detection macros
 * (<https://clang.llvm.org/docs/LanguageExtensions.html#feature-checking-macros>)
 * to determine if the feature you're interested in is available.  This
 * generally works well, and it should probably be the first thing you
 * try.  Unfortunately, it's not possible to check for everything.  In
 * particular, compiler bugs.
 *
 * This file just uses the feature checking macros to detect features
 * added in specific versions of clang to identify which version of
 * clang the compiler is based on.
 *
 * Right now it only goes back to 3.6, but I'm happy to accept patches
 * to go back further.  And, of course, newer versions are welcome if
 * they're not already present, and if you find a way to detect a point
 * release that would be great, too!
 */

#if !defined(SIMDE_DETECT_CLANG_H)
#define SIMDE_DETECT_CLANG_H 1

/* Attempt to detect the upstream clang version number.  I usually only
 * worry about major version numbers (at least for 4.0+), but if you
 * need more resolution I'm happy to accept patches that are able to
 * detect minor versions as well.  That said, you'll probably have a
 * hard time with detection since AFAIK most minor releases don't add
 * anything we can detect. */

#if defined(__clang__) && !defined(SIMDE_DETECT_CLANG_VERSION)
#if __has_warning("-Wformat-insufficient-args")
#define SIMDE_DETECT_CLANG_VERSION 120000
#elif __has_warning("-Wimplicit-const-int-float-conversion")
#define SIMDE_DETECT_CLANG_VERSION 110000
#elif __has_warning("-Wmisleading-indentation")
#define SIMDE_DETECT_CLANG_VERSION 100000
#elif defined(__FILE_NAME__)
#define SIMDE_DETECT_CLANG_VERSION 90000
#elif __has_warning("-Wextra-semi-stmt") || \
	__has_builtin(__builtin_rotateleft32)
#define SIMDE_DETECT_CLANG_VERSION 80000
#elif __has_warning("-Wc++98-compat-extra-semi")
#define SIMDE_DETECT_CLANG_VERSION 70000
#elif __has_warning("-Wpragma-pack")
#define SIMDE_DETECT_CLANG_VERSION 60000
#elif __has_warning("-Wbitfield-enum-conversion")
#define SIMDE_DETECT_CLANG_VERSION 50000
#elif __has_attribute(diagnose_if)
#define SIMDE_DETECT_CLANG_VERSION 40000
#elif __has_warning("-Wcast-calling-convention")
#define SIMDE_DETECT_CLANG_VERSION 30900
#elif __has_warning("-WCL4")
#define SIMDE_DETECT_CLANG_VERSION 30800
#elif __has_warning("-WIndependentClass-attribute")
#define SIMDE_DETECT_CLANG_VERSION 30700
#elif __has_warning("-Wambiguous-ellipsis")
#define SIMDE_DETECT_CLANG_VERSION 30600
#else
#define SIMDE_DETECT_CLANG_VERSION 1
#endif
#endif /* defined(__clang__) && !defined(SIMDE_DETECT_CLANG_VERSION) */

/* The SIMDE_DETECT_CLANG_VERSION_CHECK macro is pretty
 * straightforward; it returns true if the compiler is a derivative
 * of clang >= the specified version.
 *
 * Since this file is often (primarily?) useful for working around bugs
 * it is also helpful to have a macro which returns true if only if the
 * compiler is a version of clang *older* than the specified version to
 * make it a bit easier to ifdef regions to add code for older versions,
 * such as pragmas to disable a specific warning. */

#if defined(SIMDE_DETECT_CLANG_VERSION)
#define SIMDE_DETECT_CLANG_VERSION_CHECK(major, minor, revision) \
	(SIMDE_DETECT_CLANG_VERSION >=                           \
	 ((major * 10000) + (minor * 1000) + (revision)))
#define SIMDE_DETECT_CLANG_VERSION_NOT(major, minor, revision) \
	(SIMDE_DETECT_CLANG_VERSION <                          \
	 ((major * 10000) + (minor * 1000) + (revision)))
#else
#define SIMDE_DETECT_CLANG_VERSION_CHECK(major, minor, revision) (0)
#define SIMDE_DETECT_CLANG_VERSION_NOT(major, minor, revision) (1)
#endif

#endif /* !defined(SIMDE_DETECT_CLANG_H) */
