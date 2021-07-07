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
#include <unistd.h>
#include <errno.h>
#include <spawn.h>

#include "bmem.h"
#include "pipe.h"

struct os_process_pipe {
	bool read_pipe;
	int pid;
	FILE *file;
	FILE *err_file;
};

os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
					  const char *type)
{
	struct os_process_pipe process_pipe = {0};
	struct os_process_pipe *out;
	posix_spawn_file_actions_t file_actions;

	if (!cmd_line || !type) {
		return NULL;
	}

	process_pipe.read_pipe = *type == 'r';

	int mainfds[2], errfds[2];

	if (pipe(mainfds) != 0) {
		return NULL;
	}

	if (pipe(errfds) != 0) {
		close(mainfds[0]);
		close(mainfds[1]);

		return NULL;
	}

	if (posix_spawn_file_actions_init(&file_actions) != 0) {
		close(mainfds[0]);
		close(mainfds[1]);
		close(errfds[0]);
		close(errfds[1]);

		return NULL;
	}

	if (process_pipe.read_pipe) {
		posix_spawn_file_actions_addclose(&file_actions, mainfds[0]);
		if (mainfds[1] != STDOUT_FILENO) {
			posix_spawn_file_actions_adddup2(
				&file_actions, mainfds[1], STDOUT_FILENO);
			posix_spawn_file_actions_addclose(&file_actions,
							  mainfds[0]);
		}
	} else {
		if (mainfds[0] != STDIN_FILENO) {
			posix_spawn_file_actions_adddup2(
				&file_actions, mainfds[0], STDIN_FILENO);
			posix_spawn_file_actions_addclose(&file_actions,
							  mainfds[1]);
		}
	}

	posix_spawn_file_actions_addclose(&file_actions, errfds[0]);
	posix_spawn_file_actions_adddup2(&file_actions, errfds[1],
					 STDERR_FILENO);

	int pid;
	char *argv[4];

	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = (char *)cmd_line;
	argv[3] = NULL;

	int ret = posix_spawn(&pid, "/bin/sh", &file_actions, NULL, argv, NULL);

	posix_spawn_file_actions_destroy(&file_actions);

	if (ret != 0) {
		close(mainfds[0]);
		close(mainfds[1]);
		close(errfds[0]);
		close(errfds[1]);

		return NULL;
	}

	close(errfds[1]);
	process_pipe.err_file = fdopen(errfds[0], "r");

	if (process_pipe.read_pipe) {
		close(mainfds[1]);
		process_pipe.file = fdopen(mainfds[0], "r");
	} else {
		close(mainfds[0]);
		process_pipe.file = fdopen(mainfds[1], "w");
	}

	process_pipe.pid = pid;

	out = bmalloc(sizeof(pipe));
	*out = process_pipe;
	return out;
}

int os_process_pipe_destroy(os_process_pipe_t *pp)
{
	int ret = 0;

	if (pp) {
		int status;
		fclose(pp->file);
		pp->file = NULL;
		fclose(pp->err_file);
		pp->err_file = NULL;

		do {
			ret = waitpid(pp->pid, &status, 0);
		} while (ret == -1 && errno == EINTR);

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
	if (!pp) {
		return 0;
	}

	return fread(data, 1, len, pp->file);
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
