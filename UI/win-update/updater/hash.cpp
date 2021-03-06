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

#include <util/windows/WinHandle.hpp>
#include <vector>

using namespace std;

void HashToString(const uint8_t *in, wchar_t *out)
{
	const wchar_t alphabet[] = L"0123456789abcdef";

	for (int i = 0; i != BLAKE2_HASH_LENGTH; ++i) {
		out[2 * i] = alphabet[in[i] / 16];
		out[2 * i + 1] = alphabet[in[i] % 16];
	}

	out[BLAKE2_HASH_LENGTH * 2] = 0;
}

void StringToHash(const wchar_t *in, BYTE *out)
{
	unsigned int temp;

	for (int i = 0; i < BLAKE2_HASH_LENGTH; i++) {
		swscanf_s(in + i * 2, L"%02x", &temp);
		out[i] = (BYTE)temp;
	}
}

bool CalculateFileHash(const wchar_t *path, BYTE *hash)
{
	static __declspec(thread) vector<BYTE> hashBuffer;
	blake2b_state blake2;
	if (blake2b_init(&blake2, BLAKE2_HASH_LENGTH) != 0)
		return false;

	hashBuffer.resize(1048576);

	WinHandle handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
				       nullptr, OPEN_EXISTING, 0, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	for (;;) {
		DWORD read = 0;
		if (!ReadFile(handle, &hashBuffer[0], hashBuffer.size(), &read,
			      nullptr))
			return false;

		if (!read)
			break;

		if (blake2b_update(&blake2, &hashBuffer[0], read) != 0)
			return false;
	}

	if (blake2b_final(&blake2, hash, BLAKE2_HASH_LENGTH) != 0)
		return false;

	return true;
}
