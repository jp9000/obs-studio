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

#include <windows.h>

struct ipc_pipe_server {
	OVERLAPPED overlap;
	HANDLE handle;
	HANDLE ready_event;
	HANDLE thread;

	uint8_t *read_data;
	size_t size;
	size_t capacity;

	ipc_pipe_read_t read_callback;
	void *param;
};

struct ipc_pipe_client {
	HANDLE handle;
};

static inline bool ipc_pipe_client_valid(ipc_pipe_client_t *pipe)
{
	return pipe->handle != NULL && pipe->handle != INVALID_HANDLE_VALUE;
}
