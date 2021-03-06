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

#include <stdio.h>
#include <sys/wait.h>

#include "bmem.h"
#include "pipe.h"

struct os_process_pipe {
	bool read_pipe;
	FILE *file;
};

os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
					  const char *type)
{
	struct os_process_pipe pipe = {0};
	struct os_process_pipe *out;

	if (!cmd_line || !type) {
		return NULL;
	}

	pipe.file = popen(cmd_line, type);
	pipe.read_pipe = *type == 'r';

	if (pipe.file == (FILE *)-1 || pipe.file == NULL) {
		return NULL;
	}

	out = bmalloc(sizeof(pipe));
	*out = pipe;
	return out;
}

int os_process_pipe_destroy(os_process_pipe_t *pp)
{
	int ret = 0;

	if (pp) {
		int status = pclose(pp->file);
		if (WIFEXITED(status))
			ret = (int)(char)WEXITSTATUS(status);
		bfree(pp);
	}

	return ret;
}

size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len)
{
	if (!pp) {
		return 0;
	}
	if (!pp->read_pipe) {
		return 0;
	}

	return fread(data, 1, len, pp->file);
}

size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data,
				size_t len)
{
	/* XXX: unsupported on posix */
	UNUSED_PARAMETER(pp);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(len);
	return 0;
}

size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data,
			     size_t len)
{
	if (!pp) {
		return 0;
	}
	if (pp->read_pipe) {
		return 0;
	}

	size_t written = 0;
	while (written < len) {
		size_t ret = fwrite(data + written, 1, len - written, pp->file);
		if (!ret)
			return written;
		written += ret;
	}
	return written;
}
