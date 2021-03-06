/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
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

#include <stdarg.h>

#include "c99defs.h"

/*
 * Just contains logging/crash related stuff
 */

#ifdef __cplusplus
extern "C" {
#endif

#define STRINGIFY(x) #x
#define STRINGIFY_(x) STRINGIFY(x)
#define S__LINE__ STRINGIFY_(__LINE__)

#define INT_CUR_LINE __LINE__
#define FILE_LINE __FILE__ " (" S__LINE__ "): "

enum {
	/**
	 * Use if there's a problem that can potentially affect the program,
	 * but isn't enough to require termination of the program.
	 *
	 * Use in creation functions and core subsystem functions.  Places that
	 * should definitely not fail.
	 */
	LOG_ERROR = 100,

	/**
	 * Use if a problem occurs that doesn't affect the program and is
	 * recoverable.
	 *
	 * Use in places where failure isn't entirely unexpected, and can
	 * be handled safely.
	 */
	LOG_WARNING = 200,

	/**
	 * Informative message to be displayed in the log.
	 */
	LOG_INFO = 300,

	/**
	 * Debug message to be used mostly by developers.
	 */
	LOG_DEBUG = 400
};

typedef void (*log_handler_t)(int lvl, const char *msg, va_list args, void *p);

EXPORT void base_get_log_handler(log_handler_t *handler, void **param);
EXPORT void base_set_log_handler(log_handler_t handler, void *param);

EXPORT void base_set_crash_handler(void (*handler)(const char *, va_list,
						   void *),
				   void *param);

EXPORT void blogva(int log_level, const char *format, va_list args);

#if !defined(_MSC_VER) && !defined(SWIG)
#define PRINTFATTR(f, a) __attribute__((__format__(__printf__, f, a)))
#else
#define PRINTFATTR(f, a)
#endif

PRINTFATTR(2, 3)
EXPORT void blog(int log_level, const char *format, ...);
PRINTFATTR(1, 2)
EXPORT void bcrash(const char *format, ...);

#undef PRINTFATTR

#ifdef __cplusplus
}
#endif
