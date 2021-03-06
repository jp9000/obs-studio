/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
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

#include "c99defs.h"

struct os_process_pipe;
typedef struct os_process_pipe os_process_pipe_t;

EXPORT os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
						 const char *type);
EXPORT int os_process_pipe_destroy(os_process_pipe_t *pp);

EXPORT size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data,
				   size_t len);
EXPORT size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data,
				       size_t len);
EXPORT size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data,
				    size_t len);
