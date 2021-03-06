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

#include "../util/c99defs.h"

#include "calldata.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Procedure handler
 *
 *   This handler is used to allow access to one or more procedures that can be
 *   added and called without having to have direct access to declarations or
 *   procedure callback pointers.
 */

struct proc_handler;
typedef struct proc_handler proc_handler_t;
typedef void (*proc_handler_proc_t)(void *, calldata_t *);

EXPORT proc_handler_t *proc_handler_create(void);
EXPORT void proc_handler_destroy(proc_handler_t *handler);

EXPORT void proc_handler_add(proc_handler_t *handler, const char *decl_string,
			     proc_handler_proc_t proc, void *data);

/**
 * Calls a function in a procedure handler.  Returns false if the named
 * procedure is not found.
 */
EXPORT bool proc_handler_call(proc_handler_t *handler, const char *name,
			      calldata_t *params);

#ifdef __cplusplus
}
#endif
