/*
 * Copyright (c) 2017-2018 Hugh Bailey <obs.jim@gmail.com>
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

#include "updater.hpp"

#include <stdint.h>
#include <vector>

#ifdef _MSC_VER
#define restrict __restrict
#include <lzma.h>
#undef restrict
#else
#include <lzma.h>
#endif

using namespace std;

#define MAX_BUF_SIZE 262144
#define READ_BUF_SIZE 32768

/* ------------------------------------------------------------------------ */

class LZMAStream {
	lzma_stream strm = {};
	bool initialized = false;

public:
	inline ~LZMAStream()
	{
		if (initialized) {
			lzma_end(&strm);
		}
	}

	inline bool init_decoder()
	{
		lzma_ret ret = lzma_stream_decoder(&strm, 200 * 1024 * 1024, 0);
		initialized = (ret == LZMA_OK);
		return initialized;
	}

	inline operator lzma_stream *() { return &strm; }
	inline bool operator!() const { return !initialized; }

	inline lzma_stream *get() { return &strm; }
};

class File {
	FILE *f = nullptr;

public:
	inline ~File()
	{
		if (f)
			fclose(f);
	}

	inline FILE **operator&() { return &f; }
	inline operator FILE *() const { return f; }
	inline bool operator!() const { return !f; }
};

/* ------------------------------------------------------------------------ */

struct bspatch_stream {
	void *opaque;
	int (*read)(const struct bspatch_stream *stream, void *buffer,
		    int length);
};

/* ------------------------------------------------------------------------ */

static int64_t offtin(const uint8_t *buf)
{
	int64_t y;

	y = buf[7] & 0x7F;
	y = y * 256;
	y += buf[6];
	y = y * 256;
	y += buf[5];
	y = y * 256;
	y += buf[4];
	y = y * 256;
	y += buf[3];
	y = y * 256;
	y += buf[2];
	y = y * 256;
	y += buf[1];
	y = y * 256;
	y += buf[0];

	if (buf[7] & 0x80)
		y = -y;

	return y;
}

/* ------------------------------------------------------------------------ */

static int bspatch(const uint8_t *old, int64_t oldsize, uint8_t *newp,
		   int64_t newsize, struct bspatch_stream *stream)
{
	uint8_t buf[8];
	int64_t oldpos, newpos;
	int64_t ctrl[3];
	int64_t i;

	oldpos = 0;
	newpos = 0;
	while (newpos < newsize) {
		/* Read control data */
		for (i = 0; i <= 2; i++) {
			if (stream->read(stream, buf, 8))
				return -1;
			ctrl[i] = offtin(buf);
		};

		/* Sanity-check */
		if (newpos + ctrl[0] > newsize)
			return -1;

		/* Read diff string */
		if (stream->read(stream, newp + newpos, (int)ctrl[0]))
			return -1;

		/* Add old data to diff string */
		for (i = 0; i < ctrl[0]; i++)
			if ((oldpos + i >= 0) && (oldpos + i < oldsize))
				newp[newpos + i] += old[oldpos + i];

		/* Adjust pointers */
		newpos += ctrl[0];
		oldpos += ctrl[0];

		/* Sanity-check */
		if (newpos + ctrl[1] > newsize)
			return -1;

		/* Read extra string */
		if (stream->read(stream, newp + newpos, (int)ctrl[1]))
			return -1;

		/* Adjust pointers */
		newpos += ctrl[1];
		oldpos += ctrl[2];
	};

	return 0;
}

/* ------------------------------------------------------------------------ */

struct patch_data {
	HANDLE h;
	lzma_stream *strm;
	uint8_t buf[READ_BUF_SIZE];
};

static int read_lzma(const struct bspatch_stream *stream, void *buffer, int len)
{
	if (!len)
		return 0;

	patch_data *data = (patch_data *)stream->opaque;
	HANDLE h = data->h;
	lzma_stream *strm = data->strm;

	strm->avail_out = (size_t)len;
	strm->next_out = (uint8_t *)buffer;

	for (;;) {
		if (strm->avail_in == 0) {
			DWORD read_size;
			if (!ReadFile(h, data->buf, READ_BUF_SIZE, &read_size,
				      nullptr))
				return -1;
			if (read_size == 0)
				return -1;

			strm->avail_in = (size_t)read_size;
			strm->next_in = data->buf;
		}

		lzma_ret ret = lzma_code(strm, LZMA_RUN);
		if (ret == LZMA_STREAM_END)
			return 0;
		if (ret != LZMA_OK)
			return -1;
		if (strm->avail_out == 0)
			break;
	}

	return 0;
}

int ApplyPatch(const wchar_t *patchFile, const wchar_t *targetFile)
try {
	uint8_t header[24];
	int64_t newsize;
	struct bspatch_stream stream;
	bool success;

	WinHandle hPatch;
	WinHandle hTarget;
	LZMAStream strm;

	/* --------------------------------- *
	 * open patch and file to patch      */

	hPatch = CreateFile(patchFile, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
			    0, nullptr);
	if (!hPatch.Valid())
		throw int(GetLastError());

	hTarget = CreateFile(targetFile, GENERIC_READ, 0, nullptr,
			     OPEN_EXISTING, 0, nullptr);
	if (!hTarget.Valid())
		throw int(GetLastError());

	/* --------------------------------- *
	 * read patch header                 */

	DWORD read;
	success = !!ReadFile(hPatch, header, sizeof(header), &read, nullptr);
	if (success && read == sizeof(header)) {
		if (memcmp(header, "JIMSLEY/BSDIFF43", 16))
			throw int(-4);
	} else {
		throw int(GetLastError());
	}

	/* --------------------------------- *
	 * allocate new file size data       */

	newsize = offtin(header + 16);
	if (newsize < 0 || newsize >= 0x7ffffffff)
		throw int(-5);

	vector<uint8_t> newData;
	try {
		newData.resize((size_t)newsize);
	} catch (...) {
		throw int(-1);
	}

	/* --------------------------------- *
	 * read old file                     */

	DWORD targetFileSize;

	targetFileSize = GetFileSize(hTarget, nullptr);
	if (targetFileSize == INVALID_FILE_SIZE)
		throw int(GetLastError());

	vector<uint8_t> oldData;
	try {
		oldData.resize(targetFileSize);
	} catch (...) {
		throw int(-1);
	}

	if (!ReadFile(hTarget, &oldData[0], targetFileSize, &read, nullptr))
		throw int(GetLastError());
	if (read != targetFileSize)
		throw int(-1);

	/* --------------------------------- *
	 * patch to new file data            */

	if (!strm.init_decoder())
		throw int(-10);

	patch_data data;
	data.h = hPatch;
	data.strm = strm.get();

	stream.read = read_lzma;
	stream.opaque = &data;

	int ret = bspatch(oldData.data(), oldData.size(), newData.data(),
			  newData.size(), &stream);
	if (ret != 0)
		throw int(-9);

	/* --------------------------------- *
	 * write new file                    */

	hTarget = nullptr;
	hTarget = CreateFile(targetFile, GENERIC_WRITE, 0, nullptr,
			     CREATE_ALWAYS, 0, nullptr);
	if (!hTarget.Valid())
		throw int(GetLastError());

	DWORD written;

	success = !!WriteFile(hTarget, newData.data(), (DWORD)newsize, &written,
			      nullptr);
	if (!success || written != newsize)
		throw int(GetLastError());

	return 0;

} catch (int code) {
	return code;
}
