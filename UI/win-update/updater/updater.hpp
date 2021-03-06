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

#pragma once

#define WINVER 0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#define ZLIB_CONST

#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>
#include <Wincrypt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <malloc.h>
#include <stdlib.h>
#include <tchar.h>
#include <strsafe.h>
#include <zlib.h>
#include <ctype.h>
#include <blake2.h>

#include <string>

#include "../win-update-helpers.hpp"

#define BLAKE2_HASH_LENGTH 20
#define BLAKE2_HASH_STR_LENGTH ((BLAKE2_HASH_LENGTH * 2) + 1)

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' "       \
			"name='Microsoft.Windows.Common-Controls' " \
			"version='6.0.0.0' "                        \
			"processorArchitecture='x86' "              \
			"publicKeyToken='6595b64144ccf1df' "        \
			"language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' "       \
			"name='Microsoft.Windows.Common-Controls' " \
			"version='6.0.0.0' "                        \
			"processorArchitecture='ia64' "             \
			"publicKeyToken='6595b64144ccf1df' "        \
			"language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' "       \
			"name='Microsoft.Windows.Common-Controls' " \
			"version='6.0.0.0' "                        \
			"processorArchitecture='amd64' "            \
			"publicKeyToken='6595b64144ccf1df' "        \
			"language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' "          \
			"name='Microsoft.Windows.Common-Controls' "    \
			"version='6.0.0.0' processorArchitecture='*' " \
			"publicKeyToken='6595b64144ccf1df' "           \
			"language='*'\"")
#endif

#include <util/windows/WinHandle.hpp>
#include <json11.hpp>
#include "resource.h"

bool HTTPGetFile(HINTERNET hConnect, const wchar_t *url,
		 const wchar_t *outputPath, const wchar_t *extraHeaders,
		 int *responseCode);
bool HTTPPostData(const wchar_t *url, const BYTE *data, int dataLen,
		  const wchar_t *extraHeaders, int *responseCode,
		  std::string &response);

void HashToString(const BYTE *in, wchar_t *out);
void StringToHash(const wchar_t *in, BYTE *out);

bool CalculateFileHash(const wchar_t *path, BYTE *hash);

int ApplyPatch(LPCTSTR patchFile, LPCTSTR targetFile);

extern HWND hwndMain;
extern HCRYPTPROV hProvider;
extern int totalFileSize;
extern int completedFileSize;
extern HANDLE cancelRequested;

#pragma pack(push, r1, 1)

typedef struct {
	BLOBHEADER blobheader;
	RSAPUBKEY rsapubkey;
} PUBLICKEYHEADER;

#pragma pack(pop, r1)

void FreeWinHttpHandle(HINTERNET handle);
using HttpHandle = CustomHandle<HINTERNET, FreeWinHttpHandle>;
