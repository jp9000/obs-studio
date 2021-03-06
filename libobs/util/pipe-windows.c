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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform.h"
#include "bmem.h"
#include "pipe.h"

struct os_process_pipe {
	bool read_pipe;
	HANDLE handle;
	HANDLE handle_err;
	HANDLE process;
};

static bool create_pipe(HANDLE *input, HANDLE *output)
{
	SECURITY_ATTRIBUTES sa = {0};

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = true;

	if (!CreatePipe(input, output, &sa, 0)) {
		return false;
	}

	return true;
}

static inline bool create_process(const char *cmd_line, HANDLE stdin_handle,
				  HANDLE stdout_handle, HANDLE stderr_handle,
				  HANDLE *process)
{
	PROCESS_INFORMATION pi = {0};
	wchar_t *cmd_line_w = NULL;
	STARTUPINFOW si = {0};
	bool success = false;

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
	si.hStdInput = stdin_handle;
	si.hStdOutput = stdout_handle;
	si.hStdError = stderr_handle;

	DWORD flags = 0;
#ifndef SHOW_SUBPROCESSES
	flags = CREATE_NO_WINDOW;
#endif

	os_utf8_to_wcs_ptr(cmd_line, 0, &cmd_line_w);
	if (cmd_line_w) {
		success = !!CreateProcessW(NULL, cmd_line_w, NULL, NULL, true,
					   flags, NULL, NULL, &si, &pi);

		if (success) {
			*process = pi.hProcess;
			CloseHandle(pi.hThread);
		}

		bfree(cmd_line_w);
	}

	return success;
}

os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
					  const char *type)
{
	os_process_pipe_t *pp = NULL;
	bool read_pipe;
	HANDLE process;
	HANDLE output;
	HANDLE err_input, err_output;
	HANDLE input;
	bool success;

	if (!cmd_line || !type) {
		return NULL;
	}
	if (*type != 'r' && *type != 'w') {
		return NULL;
	}
	if (!create_pipe(&input, &output)) {
		return NULL;
	}

	if (!create_pipe(&err_input, &err_output)) {
		return NULL;
	}

	read_pipe = *type == 'r';

	success = !!SetHandleInformation(read_pipe ? input : output,
					 HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = !!SetHandleInformation(err_input, HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = create_process(cmd_line, read_pipe ? NULL : input,
				 read_pipe ? output : NULL, err_output,
				 &process);
	if (!success) {
		goto error;
	}

	pp = bmalloc(sizeof(*pp));

	pp->handle = read_pipe ? input : output;
	pp->read_pipe = read_pipe;
	pp->process = process;
	pp->handle_err = err_input;

	CloseHandle(read_pipe ? output : input);
	CloseHandle(err_output);
	return pp;

error:
	CloseHandle(output);
	CloseHandle(input);
	return NULL;
}

int os_process_pipe_destroy(os_process_pipe_t *pp)
{
	int ret = 0;

	if (pp) {
		DWORD code;

		CloseHandle(pp->handle);
		CloseHandle(pp->handle_err);

		WaitForSingleObject(pp->process, INFINITE);
		if (GetExitCodeProcess(pp->process, &code))
			ret = (int)code;

		CloseHandle(pp->process);
		bfree(pp);
	}

	return ret;
}

size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp) {
		return 0;
	}
	if (!pp->read_pipe) {
		return 0;
	}

	success = !!ReadFile(pp->handle, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	}

	return 0;
}

size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data,
				size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp || !pp->handle_err) {
		return 0;
	}

	success =
		!!ReadFile(pp->handle_err, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	} else
		bytes_read = GetLastError();

	return 0;
}

size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data,
			     size_t len)
{
	DWORD bytes_written;
	bool success;

	if (!pp) {
		return 0;
	}
	if (pp->read_pipe) {
		return 0;
	}

	success =
		!!WriteFile(pp->handle, data, (DWORD)len, &bytes_written, NULL);
	if (success && bytes_written) {
		return bytes_written;
	}

	return 0;
}
