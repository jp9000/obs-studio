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
 * Signal handler
 *
 *   This is used to create a signal handler which can broadcast events
 * to one or more callbacks connected to a signal.
 */

struct signal_handler;
typedef struct signal_handler signal_handler_t;
typedef void (*global_signal_callback_t)(void *, const char *, calldata_t *);
typedef void (*signal_callback_t)(void *, calldata_t *);

EXPORT signal_handler_t *signal_handler_create(void);
EXPORT void signal_handler_destroy(signal_handler_t *handler);

EXPORT bool signal_handler_add(signal_handler_t *handler,
			       const char *signal_decl);

static inline bool signal_handler_add_array(signal_handler_t *handler,
					    const char **signal_decls)
{
	bool success = true;
	if (!signal_decls)
		return false;

	while (*signal_decls)
		if (!signal_handler_add(handler, *(signal_decls++)))
			success = false;

	return success;
}

EXPORT void signal_handler_connect(signal_handler_t *handler,
				   const char *signal,
				   signal_callback_t callback, void *data);
EXPORT void signal_handler_connect_ref(signal_handler_t *handler,
				       const char *signal,
				       signal_callback_t callback, void *data);
EXPORT void signal_handler_disconnect(signal_handler_t *handler,
				      const char *signal,
				      signal_callback_t callback, void *data);

EXPORT void signal_handler_connect_global(signal_handler_t *handler,
					  global_signal_callback_t callback,
					  void *data);
EXPORT void signal_handler_disconnect_global(signal_handler_t *handler,
					     global_signal_callback_t callback,
					     void *data);

EXPORT void signal_handler_remove_current(void);

EXPORT void signal_handler_signal(signal_handler_t *handler, const char *signal,
				  calldata_t *params);

#ifdef __cplusplus
}
#endif
