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
 *   2020      Evan Nemerson <evan@nemerson.com>
 */

/* Constify macros.  For internal use only.
 *
 * These are used to make it possible to call a function which takes
 * an Integer Constant Expression (ICE) using a compile time constant.
 * Technically it would also be possible to use a value not trivially
 * known by the compiler, but there would be a siginficant performance
 * hit (a switch switch is used).
 *
 * The basic idea is pretty simple; we just emit a do while loop which
 * contains a switch with a case for every possible value of the
 * constant.
 *
 * As long as the value you pass to the function in constant, pretty
 * much any copmiler shouldn't have a problem generating exactly the
 * same code as if you had used an ICE.
 *
 * This is intended to be used in the SIMDe implementations of
 * functions the compilers require to be an ICE, but the other benefit
 * is that if we also disable the warnings from
 * SIMDE_REQUIRE_CONSTANT_RANGE we can actually just allow the tests
 * to use non-ICE parameters
 */

#if !defined(SIMDE_CONSTIFY_H)
#define SIMDE_CONSTIFY_H

#include "simde-diagnostic.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DIAGNOSTIC_DISABLE_VARIADIC_MACROS_
SIMDE_DIAGNOSTIC_DISABLE_CPP98_COMPAT_PEDANTIC_

#define SIMDE_CONSTIFY_2_(func_name, result, default_case, imm, ...) \
	do {                                                         \
		switch (imm) {                                       \
		case 0:                                              \
			result = func_name(__VA_ARGS__, 0);          \
			break;                                       \
		case 1:                                              \
			result = func_name(__VA_ARGS__, 1);          \
			break;                                       \
		default:                                             \
			result = default_case;                       \
			break;                                       \
		}                                                    \
	} while (0)

#define SIMDE_CONSTIFY_4_(func_name, result, default_case, imm, ...) \
	do {                                                         \
		switch (imm) {                                       \
		case 0:                                              \
			result = func_name(__VA_ARGS__, 0);          \
			break;                                       \
		case 1:                                              \
			result = func_name(__VA_ARGS__, 1);          \
			break;                                       \
		case 2:                                              \
			result = func_name(__VA_ARGS__, 2);          \
			break;                                       \
		case 3:                                              \
			result = func_name(__VA_ARGS__, 3);          \
			break;                                       \
		default:                                             \
			result = default_case;                       \
			break;                                       \
		}                                                    \
	} while (0)

#define SIMDE_CONSTIFY_8_(func_name, result, default_case, imm, ...) \
	do {                                                         \
		switch (imm) {                                       \
		case 0:                                              \
			result = func_name(__VA_ARGS__, 0);          \
			break;                                       \
		case 1:                                              \
			result = func_name(__VA_ARGS__, 1);          \
			break;                                       \
		case 2:                                              \
			result = func_name(__VA_ARGS__, 2);          \
			break;                                       \
		case 3:                                              \
			result = func_name(__VA_ARGS__, 3);          \
			break;                                       \
		case 4:                                              \
			result = func_name(__VA_ARGS__, 4);          \
			break;                                       \
		case 5:                                              \
			result = func_name(__VA_ARGS__, 5);          \
			break;                                       \
		case 6:                                              \
			result = func_name(__VA_ARGS__, 6);          \
			break;                                       \
		case 7:                                              \
			result = func_name(__VA_ARGS__, 7);          \
			break;                                       \
		default:                                             \
			result = default_case;                       \
			break;                                       \
		}                                                    \
	} while (0)

#define SIMDE_CONSTIFY_16_(func_name, result, default_case, imm, ...) \
	do {                                                          \
		switch (imm) {                                        \
		case 0:                                               \
			result = func_name(__VA_ARGS__, 0);           \
			break;                                        \
		case 1:                                               \
			result = func_name(__VA_ARGS__, 1);           \
			break;                                        \
		case 2:                                               \
			result = func_name(__VA_ARGS__, 2);           \
			break;                                        \
		case 3:                                               \
			result = func_name(__VA_ARGS__, 3);           \
			break;                                        \
		case 4:                                               \
			result = func_name(__VA_ARGS__, 4);           \
			break;                                        \
		case 5:                                               \
			result = func_name(__VA_ARGS__, 5);           \
			break;                                        \
		case 6:                                               \
			result = func_name(__VA_ARGS__, 6);           \
			break;                                        \
		case 7:                                               \
			result = func_name(__VA_ARGS__, 7);           \
			break;                                        \
		case 8:                                               \
			result = func_name(__VA_ARGS__, 8);           \
			break;                                        \
		case 9:                                               \
			result = func_name(__VA_ARGS__, 9);           \
			break;                                        \
		case 10:                                              \
			result = func_name(__VA_ARGS__, 10);          \
			break;                                        \
		case 11:                                              \
			result = func_name(__VA_ARGS__, 11);          \
			break;                                        \
		case 12:                                              \
			result = func_name(__VA_ARGS__, 12);          \
			break;                                        \
		case 13:                                              \
			result = func_name(__VA_ARGS__, 13);          \
			break;                                        \
		case 14:                                              \
			result = func_name(__VA_ARGS__, 14);          \
			break;                                        \
		case 15:                                              \
			result = func_name(__VA_ARGS__, 15);          \
			break;                                        \
		default:                                              \
			result = default_case;                        \
			break;                                        \
		}                                                     \
	} while (0)

#define SIMDE_CONSTIFY_32_(func_name, result, default_case, imm, ...) \
	do {                                                          \
		switch (imm) {                                        \
		case 0:                                               \
			result = func_name(__VA_ARGS__, 0);           \
			break;                                        \
		case 1:                                               \
			result = func_name(__VA_ARGS__, 1);           \
			break;                                        \
		case 2:                                               \
			result = func_name(__VA_ARGS__, 2);           \
			break;                                        \
		case 3:                                               \
			result = func_name(__VA_ARGS__, 3);           \
			break;                                        \
		case 4:                                               \
			result = func_name(__VA_ARGS__, 4);           \
			break;                                        \
		case 5:                                               \
			result = func_name(__VA_ARGS__, 5);           \
			break;                                        \
		case 6:                                               \
			result = func_name(__VA_ARGS__, 6);           \
			break;                                        \
		case 7:                                               \
			result = func_name(__VA_ARGS__, 7);           \
			break;                                        \
		case 8:                                               \
			result = func_name(__VA_ARGS__, 8);           \
			break;                                        \
		case 9:                                               \
			result = func_name(__VA_ARGS__, 9);           \
			break;                                        \
		case 10:                                              \
			result = func_name(__VA_ARGS__, 10);          \
			break;                                        \
		case 11:                                              \
			result = func_name(__VA_ARGS__, 11);          \
			break;                                        \
		case 12:                                              \
			result = func_name(__VA_ARGS__, 12);          \
			break;                                        \
		case 13:                                              \
			result = func_name(__VA_ARGS__, 13);          \
			break;                                        \
		case 14:                                              \
			result = func_name(__VA_ARGS__, 14);          \
			break;                                        \
		case 15:                                              \
			result = func_name(__VA_ARGS__, 15);          \
			break;                                        \
		case 16:                                              \
			result = func_name(__VA_ARGS__, 16);          \
			break;                                        \
		case 17:                                              \
			result = func_name(__VA_ARGS__, 17);          \
			break;                                        \
		case 18:                                              \
			result = func_name(__VA_ARGS__, 18);          \
			break;                                        \
		case 19:                                              \
			result = func_name(__VA_ARGS__, 19);          \
			break;                                        \
		case 20:                                              \
			result = func_name(__VA_ARGS__, 20);          \
			break;                                        \
		case 21:                                              \
			result = func_name(__VA_ARGS__, 21);          \
			break;                                        \
		case 22:                                              \
			result = func_name(__VA_ARGS__, 22);          \
			break;                                        \
		case 23:                                              \
			result = func_name(__VA_ARGS__, 23);          \
			break;                                        \
		case 24:                                              \
			result = func_name(__VA_ARGS__, 24);          \
			break;                                        \
		case 25:                                              \
			result = func_name(__VA_ARGS__, 25);          \
			break;                                        \
		case 26:                                              \
			result = func_name(__VA_ARGS__, 26);          \
			break;                                        \
		case 27:                                              \
			result = func_name(__VA_ARGS__, 27);          \
			break;                                        \
		case 28:                                              \
			result = func_name(__VA_ARGS__, 28);          \
			break;                                        \
		case 29:                                              \
			result = func_name(__VA_ARGS__, 29);          \
			break;                                        \
		case 30:                                              \
			result = func_name(__VA_ARGS__, 30);          \
			break;                                        \
		case 31:                                              \
			result = func_name(__VA_ARGS__, 31);          \
			break;                                        \
		default:                                              \
			result = default_case;                        \
			break;                                        \
		}                                                     \
	} while (0)

#define SIMDE_CONSTIFY_64_(func_name, result, default_case, imm, ...) \
	do {                                                          \
		switch (imm) {                                        \
		case 0:                                               \
			result = func_name(__VA_ARGS__, 0);           \
			break;                                        \
		case 1:                                               \
			result = func_name(__VA_ARGS__, 1);           \
			break;                                        \
		case 2:                                               \
			result = func_name(__VA_ARGS__, 2);           \
			break;                                        \
		case 3:                                               \
			result = func_name(__VA_ARGS__, 3);           \
			break;                                        \
		case 4:                                               \
			result = func_name(__VA_ARGS__, 4);           \
			break;                                        \
		case 5:                                               \
			result = func_name(__VA_ARGS__, 5);           \
			break;                                        \
		case 6:                                               \
			result = func_name(__VA_ARGS__, 6);           \
			break;                                        \
		case 7:                                               \
			result = func_name(__VA_ARGS__, 7);           \
			break;                                        \
		case 8:                                               \
			result = func_name(__VA_ARGS__, 8);           \
			break;                                        \
		case 9:                                               \
			result = func_name(__VA_ARGS__, 9);           \
			break;                                        \
		case 10:                                              \
			result = func_name(__VA_ARGS__, 10);          \
			break;                                        \
		case 11:                                              \
			result = func_name(__VA_ARGS__, 11);          \
			break;                                        \
		case 12:                                              \
			result = func_name(__VA_ARGS__, 12);          \
			break;                                        \
		case 13:                                              \
			result = func_name(__VA_ARGS__, 13);          \
			break;                                        \
		case 14:                                              \
			result = func_name(__VA_ARGS__, 14);          \
			break;                                        \
		case 15:                                              \
			result = func_name(__VA_ARGS__, 15);          \
			break;                                        \
		case 16:                                              \
			result = func_name(__VA_ARGS__, 16);          \
			break;                                        \
		case 17:                                              \
			result = func_name(__VA_ARGS__, 17);          \
			break;                                        \
		case 18:                                              \
			result = func_name(__VA_ARGS__, 18);          \
			break;                                        \
		case 19:                                              \
			result = func_name(__VA_ARGS__, 19);          \
			break;                                        \
		case 20:                                              \
			result = func_name(__VA_ARGS__, 20);          \
			break;                                        \
		case 21:                                              \
			result = func_name(__VA_ARGS__, 21);          \
			break;                                        \
		case 22:                                              \
			result = func_name(__VA_ARGS__, 22);          \
			break;                                        \
		case 23:                                              \
			result = func_name(__VA_ARGS__, 23);          \
			break;                                        \
		case 24:                                              \
			result = func_name(__VA_ARGS__, 24);          \
			break;                                        \
		case 25:                                              \
			result = func_name(__VA_ARGS__, 25);          \
			break;                                        \
		case 26:                                              \
			result = func_name(__VA_ARGS__, 26);          \
			break;                                        \
		case 27:                                              \
			result = func_name(__VA_ARGS__, 27);          \
			break;                                        \
		case 28:                                              \
			result = func_name(__VA_ARGS__, 28);          \
			break;                                        \
		case 29:                                              \
			result = func_name(__VA_ARGS__, 29);          \
			break;                                        \
		case 30:                                              \
			result = func_name(__VA_ARGS__, 30);          \
			break;                                        \
		case 31:                                              \
			result = func_name(__VA_ARGS__, 31);          \
			break;                                        \
		case 32:                                              \
			result = func_name(__VA_ARGS__, 32);          \
			break;                                        \
		case 33:                                              \
			result = func_name(__VA_ARGS__, 33);          \
			break;                                        \
		case 34:                                              \
			result = func_name(__VA_ARGS__, 34);          \
			break;                                        \
		case 35:                                              \
			result = func_name(__VA_ARGS__, 35);          \
			break;                                        \
		case 36:                                              \
			result = func_name(__VA_ARGS__, 36);          \
			break;                                        \
		case 37:                                              \
			result = func_name(__VA_ARGS__, 37);          \
			break;                                        \
		case 38:                                              \
			result = func_name(__VA_ARGS__, 38);          \
			break;                                        \
		case 39:                                              \
			result = func_name(__VA_ARGS__, 39);          \
			break;                                        \
		case 40:                                              \
			result = func_name(__VA_ARGS__, 40);          \
			break;                                        \
		case 41:                                              \
			result = func_name(__VA_ARGS__, 41);          \
			break;                                        \
		case 42:                                              \
			result = func_name(__VA_ARGS__, 42);          \
			break;                                        \
		case 43:                                              \
			result = func_name(__VA_ARGS__, 43);          \
			break;                                        \
		case 44:                                              \
			result = func_name(__VA_ARGS__, 44);          \
			break;                                        \
		case 45:                                              \
			result = func_name(__VA_ARGS__, 45);          \
			break;                                        \
		case 46:                                              \
			result = func_name(__VA_ARGS__, 46);          \
			break;                                        \
		case 47:                                              \
			result = func_name(__VA_ARGS__, 47);          \
			break;                                        \
		case 48:                                              \
			result = func_name(__VA_ARGS__, 48);          \
			break;                                        \
		case 49:                                              \
			result = func_name(__VA_ARGS__, 49);          \
			break;                                        \
		case 50:                                              \
			result = func_name(__VA_ARGS__, 50);          \
			break;                                        \
		case 51:                                              \
			result = func_name(__VA_ARGS__, 51);          \
			break;                                        \
		case 52:                                              \
			result = func_name(__VA_ARGS__, 52);          \
			break;                                        \
		case 53:                                              \
			result = func_name(__VA_ARGS__, 53);          \
			break;                                        \
		case 54:                                              \
			result = func_name(__VA_ARGS__, 54);          \
			break;                                        \
		case 55:                                              \
			result = func_name(__VA_ARGS__, 55);          \
			break;                                        \
		case 56:                                              \
			result = func_name(__VA_ARGS__, 56);          \
			break;                                        \
		case 57:                                              \
			result = func_name(__VA_ARGS__, 57);          \
			break;                                        \
		case 58:                                              \
			result = func_name(__VA_ARGS__, 58);          \
			break;                                        \
		case 59:                                              \
			result = func_name(__VA_ARGS__, 59);          \
			break;                                        \
		case 60:                                              \
			result = func_name(__VA_ARGS__, 60);          \
			break;                                        \
		case 61:                                              \
			result = func_name(__VA_ARGS__, 61);          \
			break;                                        \
		case 62:                                              \
			result = func_name(__VA_ARGS__, 62);          \
			break;                                        \
		case 63:                                              \
			result = func_name(__VA_ARGS__, 63);          \
			break;                                        \
		default:                                              \
			result = default_case;                        \
			break;                                        \
		}                                                     \
	} while (0)

#define SIMDE_CONSTIFY_2_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                           \
		switch (imm) {                                         \
		case 0:                                                \
			func_name(__VA_ARGS__, 0);                     \
			break;                                         \
		case 1:                                                \
			func_name(__VA_ARGS__, 1);                     \
			break;                                         \
		default:                                               \
			default_case;                                  \
			break;                                         \
		}                                                      \
	} while (0)

#define SIMDE_CONSTIFY_4_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                           \
		switch (imm) {                                         \
		case 0:                                                \
			func_name(__VA_ARGS__, 0);                     \
			break;                                         \
		case 1:                                                \
			func_name(__VA_ARGS__, 1);                     \
			break;                                         \
		case 2:                                                \
			func_name(__VA_ARGS__, 2);                     \
			break;                                         \
		case 3:                                                \
			func_name(__VA_ARGS__, 3);                     \
			break;                                         \
		default:                                               \
			default_case;                                  \
			break;                                         \
		}                                                      \
	} while (0)

#define SIMDE_CONSTIFY_8_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                           \
		switch (imm) {                                         \
		case 0:                                                \
			func_name(__VA_ARGS__, 0);                     \
			break;                                         \
		case 1:                                                \
			func_name(__VA_ARGS__, 1);                     \
			break;                                         \
		case 2:                                                \
			func_name(__VA_ARGS__, 2);                     \
			break;                                         \
		case 3:                                                \
			func_name(__VA_ARGS__, 3);                     \
			break;                                         \
		case 4:                                                \
			func_name(__VA_ARGS__, 4);                     \
			break;                                         \
		case 5:                                                \
			func_name(__VA_ARGS__, 5);                     \
			break;                                         \
		case 6:                                                \
			func_name(__VA_ARGS__, 6);                     \
			break;                                         \
		case 7:                                                \
			func_name(__VA_ARGS__, 7);                     \
			break;                                         \
		default:                                               \
			default_case;                                  \
			break;                                         \
		}                                                      \
	} while (0)

#define SIMDE_CONSTIFY_16_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                            \
		switch (imm) {                                          \
		case 0:                                                 \
			func_name(__VA_ARGS__, 0);                      \
			break;                                          \
		case 1:                                                 \
			func_name(__VA_ARGS__, 1);                      \
			break;                                          \
		case 2:                                                 \
			func_name(__VA_ARGS__, 2);                      \
			break;                                          \
		case 3:                                                 \
			func_name(__VA_ARGS__, 3);                      \
			break;                                          \
		case 4:                                                 \
			func_name(__VA_ARGS__, 4);                      \
			break;                                          \
		case 5:                                                 \
			func_name(__VA_ARGS__, 5);                      \
			break;                                          \
		case 6:                                                 \
			func_name(__VA_ARGS__, 6);                      \
			break;                                          \
		case 7:                                                 \
			func_name(__VA_ARGS__, 7);                      \
			break;                                          \
		case 8:                                                 \
			func_name(__VA_ARGS__, 8);                      \
			break;                                          \
		case 9:                                                 \
			func_name(__VA_ARGS__, 9);                      \
			break;                                          \
		case 10:                                                \
			func_name(__VA_ARGS__, 10);                     \
			break;                                          \
		case 11:                                                \
			func_name(__VA_ARGS__, 11);                     \
			break;                                          \
		case 12:                                                \
			func_name(__VA_ARGS__, 12);                     \
			break;                                          \
		case 13:                                                \
			func_name(__VA_ARGS__, 13);                     \
			break;                                          \
		case 14:                                                \
			func_name(__VA_ARGS__, 14);                     \
			break;                                          \
		case 15:                                                \
			func_name(__VA_ARGS__, 15);                     \
			break;                                          \
		default:                                                \
			default_case;                                   \
			break;                                          \
		}                                                       \
	} while (0)

#define SIMDE_CONSTIFY_32_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                            \
		switch (imm) {                                          \
		case 0:                                                 \
			func_name(__VA_ARGS__, 0);                      \
			break;                                          \
		case 1:                                                 \
			func_name(__VA_ARGS__, 1);                      \
			break;                                          \
		case 2:                                                 \
			func_name(__VA_ARGS__, 2);                      \
			break;                                          \
		case 3:                                                 \
			func_name(__VA_ARGS__, 3);                      \
			break;                                          \
		case 4:                                                 \
			func_name(__VA_ARGS__, 4);                      \
			break;                                          \
		case 5:                                                 \
			func_name(__VA_ARGS__, 5);                      \
			break;                                          \
		case 6:                                                 \
			func_name(__VA_ARGS__, 6);                      \
			break;                                          \
		case 7:                                                 \
			func_name(__VA_ARGS__, 7);                      \
			break;                                          \
		case 8:                                                 \
			func_name(__VA_ARGS__, 8);                      \
			break;                                          \
		case 9:                                                 \
			func_name(__VA_ARGS__, 9);                      \
			break;                                          \
		case 10:                                                \
			func_name(__VA_ARGS__, 10);                     \
			break;                                          \
		case 11:                                                \
			func_name(__VA_ARGS__, 11);                     \
			break;                                          \
		case 12:                                                \
			func_name(__VA_ARGS__, 12);                     \
			break;                                          \
		case 13:                                                \
			func_name(__VA_ARGS__, 13);                     \
			break;                                          \
		case 14:                                                \
			func_name(__VA_ARGS__, 14);                     \
			break;                                          \
		case 15:                                                \
			func_name(__VA_ARGS__, 15);                     \
			break;                                          \
		case 16:                                                \
			func_name(__VA_ARGS__, 16);                     \
			break;                                          \
		case 17:                                                \
			func_name(__VA_ARGS__, 17);                     \
			break;                                          \
		case 18:                                                \
			func_name(__VA_ARGS__, 18);                     \
			break;                                          \
		case 19:                                                \
			func_name(__VA_ARGS__, 19);                     \
			break;                                          \
		case 20:                                                \
			func_name(__VA_ARGS__, 20);                     \
			break;                                          \
		case 21:                                                \
			func_name(__VA_ARGS__, 21);                     \
			break;                                          \
		case 22:                                                \
			func_name(__VA_ARGS__, 22);                     \
			break;                                          \
		case 23:                                                \
			func_name(__VA_ARGS__, 23);                     \
			break;                                          \
		case 24:                                                \
			func_name(__VA_ARGS__, 24);                     \
			break;                                          \
		case 25:                                                \
			func_name(__VA_ARGS__, 25);                     \
			break;                                          \
		case 26:                                                \
			func_name(__VA_ARGS__, 26);                     \
			break;                                          \
		case 27:                                                \
			func_name(__VA_ARGS__, 27);                     \
			break;                                          \
		case 28:                                                \
			func_name(__VA_ARGS__, 28);                     \
			break;                                          \
		case 29:                                                \
			func_name(__VA_ARGS__, 29);                     \
			break;                                          \
		case 30:                                                \
			func_name(__VA_ARGS__, 30);                     \
			break;                                          \
		case 31:                                                \
			func_name(__VA_ARGS__, 31);                     \
			break;                                          \
		default:                                                \
			default_case;                                   \
			break;                                          \
		}                                                       \
	} while (0)

#define SIMDE_CONSTIFY_64_NO_RESULT_(func_name, default_case, imm, ...) \
	do {                                                            \
		switch (imm) {                                          \
		case 0:                                                 \
			func_name(__VA_ARGS__, 0);                      \
			break;                                          \
		case 1:                                                 \
			func_name(__VA_ARGS__, 1);                      \
			break;                                          \
		case 2:                                                 \
			func_name(__VA_ARGS__, 2);                      \
			break;                                          \
		case 3:                                                 \
			func_name(__VA_ARGS__, 3);                      \
			break;                                          \
		case 4:                                                 \
			func_name(__VA_ARGS__, 4);                      \
			break;                                          \
		case 5:                                                 \
			func_name(__VA_ARGS__, 5);                      \
			break;                                          \
		case 6:                                                 \
			func_name(__VA_ARGS__, 6);                      \
			break;                                          \
		case 7:                                                 \
			func_name(__VA_ARGS__, 7);                      \
			break;                                          \
		case 8:                                                 \
			func_name(__VA_ARGS__, 8);                      \
			break;                                          \
		case 9:                                                 \
			func_name(__VA_ARGS__, 9);                      \
			break;                                          \
		case 10:                                                \
			func_name(__VA_ARGS__, 10);                     \
			break;                                          \
		case 11:                                                \
			func_name(__VA_ARGS__, 11);                     \
			break;                                          \
		case 12:                                                \
			func_name(__VA_ARGS__, 12);                     \
			break;                                          \
		case 13:                                                \
			func_name(__VA_ARGS__, 13);                     \
			break;                                          \
		case 14:                                                \
			func_name(__VA_ARGS__, 14);                     \
			break;                                          \
		case 15:                                                \
			func_name(__VA_ARGS__, 15);                     \
			break;                                          \
		case 16:                                                \
			func_name(__VA_ARGS__, 16);                     \
			break;                                          \
		case 17:                                                \
			func_name(__VA_ARGS__, 17);                     \
			break;                                          \
		case 18:                                                \
			func_name(__VA_ARGS__, 18);                     \
			break;                                          \
		case 19:                                                \
			func_name(__VA_ARGS__, 19);                     \
			break;                                          \
		case 20:                                                \
			func_name(__VA_ARGS__, 20);                     \
			break;                                          \
		case 21:                                                \
			func_name(__VA_ARGS__, 21);                     \
			break;                                          \
		case 22:                                                \
			func_name(__VA_ARGS__, 22);                     \
			break;                                          \
		case 23:                                                \
			func_name(__VA_ARGS__, 23);                     \
			break;                                          \
		case 24:                                                \
			func_name(__VA_ARGS__, 24);                     \
			break;                                          \
		case 25:                                                \
			func_name(__VA_ARGS__, 25);                     \
			break;                                          \
		case 26:                                                \
			func_name(__VA_ARGS__, 26);                     \
			break;                                          \
		case 27:                                                \
			func_name(__VA_ARGS__, 27);                     \
			break;                                          \
		case 28:                                                \
			func_name(__VA_ARGS__, 28);                     \
			break;                                          \
		case 29:                                                \
			func_name(__VA_ARGS__, 29);                     \
			break;                                          \
		case 30:                                                \
			func_name(__VA_ARGS__, 30);                     \
			break;                                          \
		case 31:                                                \
			func_name(__VA_ARGS__, 31);                     \
			break;                                          \
		case 32:                                                \
			func_name(__VA_ARGS__, 32);                     \
			break;                                          \
		case 33:                                                \
			func_name(__VA_ARGS__, 33);                     \
			break;                                          \
		case 34:                                                \
			func_name(__VA_ARGS__, 34);                     \
			break;                                          \
		case 35:                                                \
			func_name(__VA_ARGS__, 35);                     \
			break;                                          \
		case 36:                                                \
			func_name(__VA_ARGS__, 36);                     \
			break;                                          \
		case 37:                                                \
			func_name(__VA_ARGS__, 37);                     \
			break;                                          \
		case 38:                                                \
			func_name(__VA_ARGS__, 38);                     \
			break;                                          \
		case 39:                                                \
			func_name(__VA_ARGS__, 39);                     \
			break;                                          \
		case 40:                                                \
			func_name(__VA_ARGS__, 40);                     \
			break;                                          \
		case 41:                                                \
			func_name(__VA_ARGS__, 41);                     \
			break;                                          \
		case 42:                                                \
			func_name(__VA_ARGS__, 42);                     \
			break;                                          \
		case 43:                                                \
			func_name(__VA_ARGS__, 43);                     \
			break;                                          \
		case 44:                                                \
			func_name(__VA_ARGS__, 44);                     \
			break;                                          \
		case 45:                                                \
			func_name(__VA_ARGS__, 45);                     \
			break;                                          \
		case 46:                                                \
			func_name(__VA_ARGS__, 46);                     \
			break;                                          \
		case 47:                                                \
			func_name(__VA_ARGS__, 47);                     \
			break;                                          \
		case 48:                                                \
			func_name(__VA_ARGS__, 48);                     \
			break;                                          \
		case 49:                                                \
			func_name(__VA_ARGS__, 49);                     \
			break;                                          \
		case 50:                                                \
			func_name(__VA_ARGS__, 50);                     \
			break;                                          \
		case 51:                                                \
			func_name(__VA_ARGS__, 51);                     \
			break;                                          \
		case 52:                                                \
			func_name(__VA_ARGS__, 52);                     \
			break;                                          \
		case 53:                                                \
			func_name(__VA_ARGS__, 53);                     \
			break;                                          \
		case 54:                                                \
			func_name(__VA_ARGS__, 54);                     \
			break;                                          \
		case 55:                                                \
			func_name(__VA_ARGS__, 55);                     \
			break;                                          \
		case 56:                                                \
			func_name(__VA_ARGS__, 56);                     \
			break;                                          \
		case 57:                                                \
			func_name(__VA_ARGS__, 57);                     \
			break;                                          \
		case 58:                                                \
			func_name(__VA_ARGS__, 58);                     \
			break;                                          \
		case 59:                                                \
			func_name(__VA_ARGS__, 59);                     \
			break;                                          \
		case 60:                                                \
			func_name(__VA_ARGS__, 60);                     \
			break;                                          \
		case 61:                                                \
			func_name(__VA_ARGS__, 61);                     \
			break;                                          \
		case 62:                                                \
			func_name(__VA_ARGS__, 62);                     \
			break;                                          \
		case 63:                                                \
			func_name(__VA_ARGS__, 63);                     \
			break;                                          \
		default:                                                \
			default_case;                                   \
			break;                                          \
		}                                                       \
	} while (0)

HEDLEY_DIAGNOSTIC_POP

#endif
