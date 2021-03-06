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

#include <psapi.h>

#include <util/windows/CoTaskMemPtr.hpp>

#include <future>
#include <vector>
#include <string>
#include <mutex>

using namespace std;
using namespace json11;

/* ----------------------------------------------------------------------- */

HANDLE cancelRequested = nullptr;
HANDLE updateThread = nullptr;
HINSTANCE hinstMain = nullptr;
HWND hwndMain = nullptr;
HCRYPTPROV hProvider = 0;

static bool bExiting = false;
static bool updateFailed = false;
static bool is32bit = false;

static bool downloadThreadFailure = false;

int totalFileSize = 0;
int completedFileSize = 0;
static int completedUpdates = 0;

struct LastError {
	DWORD code;
	inline LastError() { code = GetLastError(); }
};

void FreeWinHttpHandle(HINTERNET handle)
{
	WinHttpCloseHandle(handle);
}

/* ----------------------------------------------------------------------- */

static inline bool is_64bit_windows(void);

static inline bool HasVS2019Redist2()
{
	wchar_t base[MAX_PATH];
	wchar_t path[MAX_PATH];
	WIN32_FIND_DATAW wfd;
	HANDLE handle;
	int folder = (is32bit && is_64bit_windows()) ? CSIDL_SYSTEMX86
						     : CSIDL_SYSTEM;

	SHGetFolderPathW(NULL, folder, NULL, SHGFP_TYPE_CURRENT, base);

#define check_dll_installed(dll)                                    \
	do {                                                        \
		StringCbCopyW(path, sizeof(path), base);            \
		StringCbCatW(path, sizeof(path), L"\\" dll ".dll"); \
		handle = FindFirstFileW(path, &wfd);                \
		if (handle == INVALID_HANDLE_VALUE) {               \
			return false;                               \
		} else {                                            \
			FindClose(handle);                          \
		}                                                   \
	} while (false)

	check_dll_installed(L"msvcp140");
	check_dll_installed(L"vcruntime140");
	if (!is32bit) {
		check_dll_installed(L"vcruntime140_1");
	}

#undef check_dll_installed

	return true;
}

static bool HasVS2019Redist()
{
	PVOID old = nullptr;
	bool redirect = !!Wow64DisableWow64FsRedirection(&old);
	bool success = HasVS2019Redist2();
	if (redirect)
		Wow64RevertWow64FsRedirection(old);
	return success;
}

static void Status(const wchar_t *fmt, ...)
{
	wchar_t str[512];

	va_list argptr;
	va_start(argptr, fmt);

	StringCbVPrintf(str, sizeof(str), fmt, argptr);

	SetDlgItemText(hwndMain, IDC_STATUS, str);

	va_end(argptr);
}

static void CreateFoldersForPath(const wchar_t *path)
{
	wchar_t *p = (wchar_t *)path;

	while (*p) {
		if (*p == '\\' || *p == '/') {
			*p = 0;
			CreateDirectory(path, nullptr);
			*p = '\\';
		}
		p++;
	}
}

static bool MyCopyFile(const wchar_t *src, const wchar_t *dest)
try {
	WinHandle hSrc;
	WinHandle hDest;

	hSrc = CreateFile(src, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
			  FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (!hSrc.Valid())
		throw LastError();

	hDest = CreateFile(dest, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0,
			   nullptr);
	if (!hDest.Valid())
		throw LastError();

	BYTE buf[65536];
	DWORD read, wrote;

	for (;;) {
		if (!ReadFile(hSrc, buf, sizeof(buf), &read, nullptr))
			throw LastError();

		if (read == 0)
			break;

		if (!WriteFile(hDest, buf, read, &wrote, nullptr))
			throw LastError();

		if (wrote != read)
			return false;
	}

	return true;

} catch (LastError error) {
	SetLastError(error.code);
	return false;
}

static bool IsSafeFilename(const wchar_t *path)
{
	const wchar_t *p = path;

	if (!*p)
		return false;

	if (wcsstr(path, L".."))
		return false;

	if (*p == '/')
		return false;

	while (*p) {
		if (!isalnum(*p) && *p != '.' && *p != '/' && *p != '_' &&
		    *p != '-')
			return false;
		p++;
	}

	return true;
}

static string QuickReadFile(const wchar_t *path)
{
	string data;

	WinHandle handle = CreateFileW(path, GENERIC_READ, 0, nullptr,
				       OPEN_EXISTING, 0, nullptr);
	if (!handle.Valid()) {
		return string();
	}

	LARGE_INTEGER size;

	if (!GetFileSizeEx(handle, &size)) {
		return string();
	}

	data.resize((size_t)size.QuadPart);

	DWORD read;
	if (!ReadFile(handle, &data[0], (DWORD)data.size(), &read, nullptr)) {
		return string();
	}
	if (read != size.QuadPart) {
		return string();
	}

	return data;
}

/* ----------------------------------------------------------------------- */

enum state_t {
	STATE_INVALID,
	STATE_PENDING_DOWNLOAD,
	STATE_DOWNLOADING,
	STATE_DOWNLOADED,
	STATE_INSTALL_FAILED,
	STATE_INSTALLED,
};

struct update_t {
	wstring sourceURL;
	wstring outputPath;
	wstring tempPath;
	wstring previousFile;
	wstring basename;
	string packageName;

	DWORD fileSize = 0;
	BYTE hash[BLAKE2_HASH_LENGTH];
	BYTE downloadhash[BLAKE2_HASH_LENGTH];
	BYTE my_hash[BLAKE2_HASH_LENGTH];
	state_t state = STATE_INVALID;
	bool has_hash = false;
	bool patchable = false;

	inline update_t() {}
	inline update_t(const update_t &from)
		: sourceURL(from.sourceURL),
		  outputPath(from.outputPath),
		  tempPath(from.tempPath),
		  previousFile(from.previousFile),
		  basename(from.basename),
		  packageName(from.packageName),
		  fileSize(from.fileSize),
		  state(from.state),
		  has_hash(from.has_hash),
		  patchable(from.patchable)
	{
		memcpy(hash, from.hash, sizeof(hash));
		memcpy(downloadhash, from.downloadhash, sizeof(downloadhash));
		memcpy(my_hash, from.my_hash, sizeof(my_hash));
	}

	inline update_t(update_t &&from)
		: sourceURL(std::move(from.sourceURL)),
		  outputPath(std::move(from.outputPath)),
		  tempPath(std::move(from.tempPath)),
		  previousFile(std::move(from.previousFile)),
		  basename(std::move(from.basename)),
		  packageName(std::move(from.packageName)),
		  fileSize(from.fileSize),
		  state(from.state),
		  has_hash(from.has_hash),
		  patchable(from.patchable)
	{
		from.state = STATE_INVALID;

		memcpy(hash, from.hash, sizeof(hash));
		memcpy(downloadhash, from.downloadhash, sizeof(downloadhash));
		memcpy(my_hash, from.my_hash, sizeof(my_hash));
	}

	void CleanPartialUpdate()
	{
		if (state == STATE_INSTALL_FAILED || state == STATE_INSTALLED) {
			if (!previousFile.empty()) {
				DeleteFile(outputPath.c_str());
				MyCopyFile(previousFile.c_str(),
					   outputPath.c_str());
				DeleteFile(previousFile.c_str());
			} else {
				DeleteFile(outputPath.c_str());
			}
			if (state == STATE_INSTALL_FAILED)
				DeleteFile(tempPath.c_str());
		} else if (state == STATE_DOWNLOADED) {
			DeleteFile(tempPath.c_str());
		}
	}

	inline update_t &operator=(const update_t &from)
	{
		sourceURL = from.sourceURL;
		outputPath = from.outputPath;
		tempPath = from.tempPath;
		previousFile = from.previousFile;
		basename = from.basename;
		packageName = from.packageName;
		fileSize = from.fileSize;
		state = from.state;
		has_hash = from.has_hash;
		patchable = from.patchable;

		memcpy(hash, from.hash, sizeof(hash));
		memcpy(downloadhash, from.downloadhash, sizeof(downloadhash));
		memcpy(my_hash, from.my_hash, sizeof(my_hash));

		return *this;
	}
};

static vector<update_t> updates;
static mutex updateMutex;

static inline void CleanupPartialUpdates()
{
	for (update_t &update : updates)
		update.CleanPartialUpdate();
}

/* ----------------------------------------------------------------------- */

bool DownloadWorkerThread()
{
	const DWORD tlsProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 |
				   WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;

	const DWORD enableHTTP2Flag = WINHTTP_PROTOCOL_FLAG_HTTP2;

	HttpHandle hSession = WinHttpOpen(L"OBS Studio Updater/2.1",
					  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
					  WINHTTP_NO_PROXY_NAME,
					  WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		downloadThreadFailure = true;
		Status(L"Update failed: Couldn't open obsproject.com");
		return false;
	}

	WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
			 (LPVOID)&tlsProtocols, sizeof(tlsProtocols));

	WinHttpSetOption(hSession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
			 (LPVOID)&enableHTTP2Flag, sizeof(enableHTTP2Flag));

	HttpHandle hConnect = WinHttpConnect(hSession,
					     L"cdn-fastly.obsproject.com",
					     INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) {
		downloadThreadFailure = true;
		Status(L"Update failed: Couldn't connect to cdn-fastly.obsproject.com");
		return false;
	}

	for (;;) {
		bool foundWork = false;

		unique_lock<mutex> ulock(updateMutex);

		for (update_t &update : updates) {
			int responseCode;

			DWORD waitResult =
				WaitForSingleObject(cancelRequested, 0);
			if (waitResult == WAIT_OBJECT_0) {
				return false;
			}

			if (update.state != STATE_PENDING_DOWNLOAD)
				continue;

			update.state = STATE_DOWNLOADING;

			ulock.unlock();

			foundWork = true;

			if (downloadThreadFailure) {
				return false;
			}

			Status(L"Downloading %s", update.outputPath.c_str());

			if (!HTTPGetFile(hConnect, update.sourceURL.c_str(),
					 update.tempPath.c_str(),
					 L"Accept-Encoding: gzip",
					 &responseCode)) {

				downloadThreadFailure = true;
				DeleteFile(update.tempPath.c_str());
				Status(L"Update failed: Could not download "
				       L"%s (error code %d)",
				       update.outputPath.c_str(), responseCode);
				return 1;
			}

			if (responseCode != 200) {
				downloadThreadFailure = true;
				DeleteFile(update.tempPath.c_str());
				Status(L"Update failed: Could not download "
				       L"%s (error code %d)",
				       update.outputPath.c_str(), responseCode);
				return 1;
			}

			BYTE downloadHash[BLAKE2_HASH_LENGTH];
			if (!CalculateFileHash(update.tempPath.c_str(),
					       downloadHash)) {
				downloadThreadFailure = true;
				DeleteFile(update.tempPath.c_str());
				Status(L"Update failed: Couldn't verify "
				       L"integrity of %s",
				       update.outputPath.c_str());
				return 1;
			}

			if (memcmp(update.downloadhash, downloadHash, 20)) {
				downloadThreadFailure = true;
				DeleteFile(update.tempPath.c_str());
				Status(L"Update failed: Integrity check "
				       L"failed on %s",
				       update.outputPath.c_str());
				return 1;
			}

			ulock.lock();

			update.state = STATE_DOWNLOADED;
			completedUpdates++;
		}

		if (!foundWork) {
			break;
		}
		if (downloadThreadFailure) {
			return false;
		}
	}

	return true;
}

static bool RunDownloadWorkers(int num)
try {
	vector<future<bool>> thread_success_results;
	thread_success_results.resize(num);

	for (future<bool> &result : thread_success_results) {
		result = async(DownloadWorkerThread);
	}
	for (future<bool> &result : thread_success_results) {
		if (!result.get()) {
			return false;
		}
	}

	return true;

} catch (...) {
	return false;
}

/* ----------------------------------------------------------------------- */

#define WAITIFOBS_SUCCESS 0
#define WAITIFOBS_WRONG_PROCESS 1
#define WAITIFOBS_CANCELLED 2

static inline DWORD WaitIfOBS(DWORD id, const wchar_t *expected)
{
	wchar_t path[MAX_PATH];
	wchar_t *name;
	*path = 0;

	WinHandle proc = OpenProcess(PROCESS_QUERY_INFORMATION |
					     PROCESS_VM_READ | SYNCHRONIZE,
				     false, id);
	if (!proc.Valid())
		return WAITIFOBS_WRONG_PROCESS;

	if (!GetProcessImageFileName(proc, path, _countof(path)))
		return WAITIFOBS_WRONG_PROCESS;

	name = wcsrchr(path, L'\\');
	if (name)
		name += 1;
	else
		name = path;

	if (_wcsnicmp(name, expected, 5) == 0) {
		HANDLE hWait[2];
		hWait[0] = proc;
		hWait[1] = cancelRequested;

		int i = WaitForMultipleObjects(2, hWait, false, INFINITE);
		if (i == WAIT_OBJECT_0 + 1)
			return WAITIFOBS_CANCELLED;

		return WAITIFOBS_SUCCESS;
	}

	return WAITIFOBS_WRONG_PROCESS;
}

static bool WaitForOBS()
{
	DWORD proc_ids[1024], needed, count;
	const wchar_t *name = is32bit ? L"obs32" : L"obs64";

	if (!EnumProcesses(proc_ids, sizeof(proc_ids), &needed)) {
		return true;
	}

	count = needed / sizeof(DWORD);

	for (DWORD i = 0; i < count; i++) {
		DWORD id = proc_ids[i];
		if (id != 0) {
			switch (WaitIfOBS(id, name)) {
			case WAITIFOBS_SUCCESS:
				return true;
			case WAITIFOBS_WRONG_PROCESS:
				break;
			case WAITIFOBS_CANCELLED:
				return false;
			}
		}
	}

	return true;
}

/* ----------------------------------------------------------------------- */

static inline bool UTF8ToWide(wchar_t *wide, int wideSize, const char *utf8)
{
	return !!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wideSize);
}

static inline bool WideToUTF8(char *utf8, int utf8Size, const wchar_t *wide)
{
	return !!WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size,
				     nullptr, nullptr);
}

static inline bool FileExists(const wchar_t *path)
{
	WIN32_FIND_DATAW wfd;
	HANDLE hFind;

	hFind = FindFirstFileW(path, &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);

	return hFind != INVALID_HANDLE_VALUE;
}

static bool NonCorePackageInstalled(const char *name)
{
	if (is32bit) {
		if (strcmp(name, "obs-browser") == 0) {
			return FileExists(
				L"obs-plugins\\32bit\\obs-browser.dll");
		} else if (strcmp(name, "realsense") == 0) {
			return FileExists(L"obs-plugins\\32bit\\win-ivcam.dll");
		}
	} else {
		if (strcmp(name, "obs-browser") == 0) {
			return FileExists(
				L"obs-plugins\\64bit\\obs-browser.dll");
		} else if (strcmp(name, "realsense") == 0) {
			return FileExists(L"obs-plugins\\64bit\\win-ivcam.dll");
		}
	}

	return false;
}

static inline bool is_64bit_windows(void)
{
#ifdef _WIN64
	return true;
#else
	BOOL x86 = false;
	bool success = !!IsWow64Process(GetCurrentProcess(), &x86);
	return success && !!x86;
#endif
}

static inline bool is_64bit_file(const char *file)
{
	if (!file)
		return false;

	return strstr(file, "64bit") != nullptr ||
	       strstr(file, "64.dll") != nullptr ||
	       strstr(file, "64.exe") != nullptr;
}

#define UTF8ToWideBuf(wide, utf8) UTF8ToWide(wide, _countof(wide), utf8)
#define WideToUTF8Buf(utf8, wide) WideToUTF8(utf8, _countof(utf8), wide)

#define UPDATE_URL L"https://cdn-fastly.obsproject.com/update_studio"

static bool AddPackageUpdateFiles(const Json &root, size_t idx,
				  const wchar_t *tempPath)
{
	const Json &package = root[idx];
	const Json &name = package["name"];
	const Json &files = package["files"];

	bool isWin64 = is_64bit_windows();

	if (!files.is_array())
		return true;
	if (!name.is_string())
		return true;

	wchar_t wPackageName[512];
	const string &packageName = name.string_value();
	size_t fileCount = files.array_items().size();

	if (!UTF8ToWideBuf(wPackageName, packageName.c_str()))
		return false;

	if (packageName != "core" &&
	    !NonCorePackageInstalled(packageName.c_str()))
		return true;

	for (size_t j = 0; j < fileCount; j++) {
		const Json &file = files[j];
		const Json &fileName = file["name"];
		const Json &hash = file["hash"];
		const Json &size = file["size"];

		if (!fileName.is_string())
			continue;
		if (!hash.is_string())
			continue;
		if (!size.is_number())
			continue;

		const string &fileUTF8 = fileName.string_value();
		const string &hashUTF8 = hash.string_value();
		int fileSize = size.int_value();

		if (hashUTF8.size() != BLAKE2_HASH_LENGTH * 2)
			continue;

		if (!isWin64 && is_64bit_file(fileUTF8.c_str()))
			continue;

		/* ignore update files of opposite arch to reduce download */

		if ((is32bit && fileUTF8.find("/64bit/") != string::npos) ||
		    (!is32bit && fileUTF8.find("/32bit/") != string::npos))
			continue;

		/* convert strings to wide */

		wchar_t sourceURL[1024];
		wchar_t updateFileName[MAX_PATH];
		wchar_t updateHashStr[BLAKE2_HASH_STR_LENGTH];
		wchar_t tempFilePath[MAX_PATH];

		if (!UTF8ToWideBuf(updateFileName, fileUTF8.c_str()))
			continue;
		if (!UTF8ToWideBuf(updateHashStr, hashUTF8.c_str()))
			continue;

		/* make sure paths are safe */

		if (!IsSafeFilename(updateFileName)) {
			Status(L"Update failed: Unsafe path '%s' found in "
			       L"manifest",
			       updateFileName);
			return false;
		}

		StringCbPrintf(sourceURL, sizeof(sourceURL), L"%s/%s/%s",
			       UPDATE_URL, wPackageName, updateFileName);
		StringCbPrintf(tempFilePath, sizeof(tempFilePath), L"%s\\%s",
			       tempPath, updateHashStr);

		/* Check file hash */

		BYTE existingHash[BLAKE2_HASH_LENGTH];
		wchar_t fileHashStr[BLAKE2_HASH_STR_LENGTH];
		bool has_hash;

		/* We don't really care if this fails, it's just to avoid
		 * wasting bandwidth by downloading unmodified files */
		if (CalculateFileHash(updateFileName, existingHash)) {

			HashToString(existingHash, fileHashStr);
			if (wcscmp(fileHashStr, updateHashStr) == 0)
				continue;

			has_hash = true;
		} else {
			has_hash = false;
		}

		/* Add update file */

		update_t update;
		update.fileSize = fileSize;
		update.basename = updateFileName;
		update.outputPath = updateFileName;
		update.tempPath = tempFilePath;
		update.sourceURL = sourceURL;
		update.packageName = packageName;
		update.state = STATE_PENDING_DOWNLOAD;
		update.patchable = false;

		StringToHash(updateHashStr, update.downloadhash);
		memcpy(update.hash, update.downloadhash, sizeof(update.hash));

		update.has_hash = has_hash;
		if (has_hash)
			StringToHash(fileHashStr, update.my_hash);

		updates.push_back(move(update));

		totalFileSize += fileSize;
	}

	return true;
}

static void UpdateWithPatchIfAvailable(const char *name, const char *hash,
				       const char *source, int size)
{
	wchar_t widePatchableFilename[MAX_PATH];
	wchar_t widePatchHash[MAX_PATH];
	wchar_t sourceURL[1024];
	wchar_t patchHashStr[BLAKE2_HASH_STR_LENGTH];

	if (strncmp(source, "https://cdn-fastly.obsproject.com/", 34) != 0)
		return;

	string patchPackageName = name;

	const char *slash = strchr(name, '/');
	if (!slash)
		return;

	patchPackageName.resize(slash - name);
	name = slash + 1;

	if (!UTF8ToWideBuf(widePatchableFilename, name))
		return;
	if (!UTF8ToWideBuf(widePatchHash, hash))
		return;
	if (!UTF8ToWideBuf(sourceURL, source))
		return;
	if (!UTF8ToWideBuf(patchHashStr, hash))
		return;

	for (update_t &update : updates) {
		if (update.packageName != patchPackageName)
			continue;
		if (update.basename != widePatchableFilename)
			continue;

		StringToHash(patchHashStr, update.downloadhash);

		/* Replace the source URL with the patch file, mark it as
		 * patchable, and re-calculate download size */
		totalFileSize -= (update.fileSize - size);
		update.sourceURL = sourceURL;
		update.fileSize = size;
		update.patchable = true;
		break;
	}
}

static bool MoveInUseFileAway(update_t &file)
{
	_TCHAR deleteMeName[MAX_PATH];
	_TCHAR randomStr[MAX_PATH];

	BYTE junk[40];
	BYTE hash[BLAKE2_HASH_LENGTH];

	CryptGenRandom(hProvider, sizeof(junk), junk);
	blake2b(hash, sizeof(hash), junk, sizeof(junk), NULL, 0);
	HashToString(hash, randomStr);
	randomStr[8] = 0;

	StringCbCopy(deleteMeName, sizeof(deleteMeName),
		     file.outputPath.c_str());

	StringCbCat(deleteMeName, sizeof(deleteMeName), L".");
	StringCbCat(deleteMeName, sizeof(deleteMeName), randomStr);
	StringCbCat(deleteMeName, sizeof(deleteMeName), L".deleteme");

	if (MoveFile(file.outputPath.c_str(), deleteMeName)) {

		if (MyCopyFile(deleteMeName, file.outputPath.c_str())) {
			MoveFileEx(deleteMeName, NULL,
				   MOVEFILE_DELAY_UNTIL_REBOOT);

			return true;
		} else {
			MoveFile(deleteMeName, file.outputPath.c_str());
		}
	}

	return false;
}

static bool UpdateFile(update_t &file)
{
	wchar_t oldFileRenamedPath[MAX_PATH];

	if (file.patchable)
		Status(L"Updating %s...", file.outputPath.c_str());
	else
		Status(L"Installing %s...", file.outputPath.c_str());

	/* Check if we're replacing an existing file or just installing a new
	 * one */
	DWORD attribs = GetFileAttributes(file.outputPath.c_str());

	if (attribs != INVALID_FILE_ATTRIBUTES) {
		wchar_t *curFileName = nullptr;
		wchar_t baseName[MAX_PATH];

		StringCbCopy(baseName, sizeof(baseName),
			     file.outputPath.c_str());

		curFileName = wcsrchr(baseName, '/');
		if (curFileName) {
			curFileName[0] = '\0';
			curFileName++;
		} else
			curFileName = baseName;

		/* Backup the existing file in case a rollback is needed */
		StringCbCopy(oldFileRenamedPath, sizeof(oldFileRenamedPath),
			     file.outputPath.c_str());
		StringCbCat(oldFileRenamedPath, sizeof(oldFileRenamedPath),
			    L".old");

		if (!MyCopyFile(file.outputPath.c_str(), oldFileRenamedPath)) {
			int is_sharing_violation =
				(GetLastError() == ERROR_SHARING_VIOLATION);

			if (is_sharing_violation)
				Status(L"Update failed: %s is still in use.  "
				       L"Close all programs and try again.",
				       curFileName);
			else
				Status(L"Update failed: Couldn't backup %s "
				       L"(error %d)",
				       curFileName, GetLastError());
			return false;
		}

		file.previousFile = oldFileRenamedPath;

		int error_code;
		bool installed_ok;
		bool already_tried_to_move = false;

	retryAfterMovingFile:

		if (file.patchable) {
			error_code = ApplyPatch(file.tempPath.c_str(),
						file.outputPath.c_str());
			installed_ok = (error_code == 0);

			if (installed_ok) {
				BYTE patchedFileHash[BLAKE2_HASH_LENGTH];
				if (!CalculateFileHash(file.outputPath.c_str(),
						       patchedFileHash)) {
					Status(L"Update failed: Couldn't "
					       L"verify integrity of patched %s",
					       curFileName);

					file.state = STATE_INSTALL_FAILED;
					return false;
				}

				if (memcmp(file.hash, patchedFileHash,
					   BLAKE2_HASH_LENGTH) != 0) {
					Status(L"Update failed: Integrity "
					       L"check of patched "
					       L"%s failed",
					       curFileName);

					file.state = STATE_INSTALL_FAILED;
					return false;
				}
			}
		} else {
			installed_ok = MyCopyFile(file.tempPath.c_str(),
						  file.outputPath.c_str());
			error_code = GetLastError();
		}

		if (!installed_ok) {
			int is_sharing_violation =
				(error_code == ERROR_SHARING_VIOLATION);

			if (is_sharing_violation) {
				if (!already_tried_to_move) {
					already_tried_to_move = true;

					if (MoveInUseFileAway(file))
						goto retryAfterMovingFile;
				}

				Status(L"Update failed: %s is still in use.  "
				       L"Close all "
				       L"programs and try again.",
				       curFileName);
			} else {
				Status(L"Update failed: Couldn't update %s "
				       L"(error %d)",
				       curFileName, GetLastError());
			}

			file.state = STATE_INSTALL_FAILED;
			return false;
		}

		file.state = STATE_INSTALLED;
	} else {
		if (file.patchable) {
			/* Uh oh, we thought we could patch something but it's
			 * no longer there! */
			Status(L"Update failed: Source file %s not found",
			       file.outputPath.c_str());
			return false;
		}

		/* We may be installing into new folders,
		 * make sure they exist */
		CreateFoldersForPath(file.outputPath.c_str());

		file.previousFile = L"";

		bool success = !!MyCopyFile(file.tempPath.c_str(),
					    file.outputPath.c_str());
		if (!success) {
			Status(L"Update failed: Couldn't install %s (error %d)",
			       file.outputPath.c_str(), GetLastError());
			file.state = STATE_INSTALL_FAILED;
			return false;
		}

		file.state = STATE_INSTALLED;
	}

	return true;
}

static wchar_t tempPath[MAX_PATH] = {};

#define PATCH_MANIFEST_URL \
	L"https://obsproject.com/update_studio/getpatchmanifest"
#define HASH_NULL L"0000000000000000000000000000000000000000"

static bool UpdateVS2019Redists(const Json &root)
{
	/* ------------------------------------------ *
	 * Initialize session                         */

	const DWORD tlsProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

	HttpHandle hSession = WinHttpOpen(L"OBS Studio Updater/2.1",
					  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
					  WINHTTP_NO_PROXY_NAME,
					  WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		Status(L"Update failed: Couldn't open obsproject.com");
		return false;
	}

	WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
			 (LPVOID)&tlsProtocols, sizeof(tlsProtocols));

	HttpHandle hConnect = WinHttpConnect(hSession,
					     L"cdn-fastly.obsproject.com",
					     INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) {
		Status(L"Update failed: Couldn't connect to cdn-fastly.obsproject.com");
		return false;
	}

	int responseCode;

	DWORD waitResult = WaitForSingleObject(cancelRequested, 0);
	if (waitResult == WAIT_OBJECT_0) {
		return false;
	}

	/* ------------------------------------------ *
	 * Download redist                            */

	Status(L"Downloading %s", L"Visual C++ 2019 Redistributable");

	const wchar_t *file = (is32bit) ? L"VC_redist.x86.exe"
					: L"VC_redist.x64.exe";

	wstring sourceURL;
	sourceURL += L"https://cdn-fastly.obsproject.com/downloads/";
	sourceURL += file;

	wstring destPath;
	destPath += tempPath;
	destPath += L"\\";
	destPath += file;

	if (!HTTPGetFile(hConnect, sourceURL.c_str(), destPath.c_str(),
			 L"Accept-Encoding: gzip", &responseCode)) {

		DeleteFile(destPath.c_str());
		Status(L"Update failed: Could not download "
		       L"%s (error code %d)",
		       L"Visual C++ 2019 Redistributable", responseCode);
		return false;
	}

	/* ------------------------------------------ *
	 * Get expected hash                          */

	const char *which = is32bit ? "vc2019_redist_x86" : "vc2019_redist_x64";
	const Json &redistJson = root[which];
	if (!redistJson.is_string()) {
		Status(L"Update failed: Could not parse VC2019 redist json");
		return false;
	}

	const string &expectedHashUTF8 = redistJson.string_value();
	wchar_t expectedHashWide[BLAKE2_HASH_STR_LENGTH];
	BYTE expectedHash[BLAKE2_HASH_LENGTH];

	if (!UTF8ToWideBuf(expectedHashWide, expectedHashUTF8.c_str())) {
		DeleteFile(destPath.c_str());
		Status(L"Update failed: Couldn't convert Json for redist hash");
		return false;
	}

	StringToHash(expectedHashWide, expectedHash);

	wchar_t downloadHashWide[BLAKE2_HASH_STR_LENGTH];
	BYTE downloadHash[BLAKE2_HASH_LENGTH];

	/* ------------------------------------------ *
	 * Get download hash                          */

	if (!CalculateFileHash(destPath.c_str(), downloadHash)) {
		DeleteFile(destPath.c_str());
		Status(L"Update failed: Couldn't verify integrity of %s",
		       L"Visual C++ 2019 Redistributable");
		return false;
	}

	/* ------------------------------------------ *
	 * If hashes do not match, integrity failed   */

	HashToString(downloadHash, downloadHashWide);
	if (wcscmp(expectedHashWide, downloadHashWide) != 0) {
		DeleteFile(destPath.c_str());
		Status(L"Update failed: Couldn't verify integrity of %s",
		       L"Visual C++ 2019 Redistributable");
		return false;
	}

	/* ------------------------------------------ *
	 * If hashes match, install redist            */

	wchar_t commandline[MAX_PATH + MAX_PATH];
	StringCbPrintf(commandline, sizeof(commandline),
		       L"%s /install /quiet /norestart", destPath.c_str());

	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof(si);

	bool success = !!CreateProcessW(destPath.c_str(), commandline, nullptr,
					nullptr, false, CREATE_NO_WINDOW,
					nullptr, nullptr, &si, &pi);
	if (success) {
		Status(L"Installing %s...", L"Visual C++ 2019 Redistributable");

		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
	} else {
		Status(L"Update failed: Could not execute "
		       L"%s (error code %d)",
		       L"Visual C++ 2019 Redistributable", (int)GetLastError());
	}

	DeleteFile(destPath.c_str());

	waitResult = WaitForSingleObject(cancelRequested, 0);
	if (waitResult == WAIT_OBJECT_0) {
		return false;
	}

	return success;
}

extern "C" void UpdateHookFiles(void);

static bool Update(wchar_t *cmdLine)
{
	/* ------------------------------------- *
	 * Check to make sure OBS isn't running  */

	HANDLE hObsUpdateMutex =
		OpenMutexW(SYNCHRONIZE, false, L"OBSStudioUpdateMutex");
	if (hObsUpdateMutex) {
		HANDLE hWait[2];
		hWait[0] = hObsUpdateMutex;
		hWait[1] = cancelRequested;

		int i = WaitForMultipleObjects(2, hWait, false, INFINITE);

		if (i == WAIT_OBJECT_0)
			ReleaseMutex(hObsUpdateMutex);

		CloseHandle(hObsUpdateMutex);

		if (i == WAIT_OBJECT_0 + 1)
			return false;
	}

	if (!WaitForOBS())
		return false;

	/* ------------------------------------- *
	 * Init crypt stuff                      */

	CryptProvider hProvider;
	if (!CryptAcquireContext(&hProvider, nullptr, MS_ENH_RSA_AES_PROV,
				 PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
		SetDlgItemTextW(hwndMain, IDC_STATUS,
				L"Update failed: CryptAcquireContext failure");
		return false;
	}

	::hProvider = hProvider;

	/* ------------------------------------- */

	SetDlgItemTextW(hwndMain, IDC_STATUS,
			L"Searching for available updates...");

	HWND hProgress = GetDlgItem(hwndMain, IDC_PROGRESS);
	LONG_PTR style = GetWindowLongPtr(hProgress, GWL_STYLE);
	SetWindowLongPtr(hProgress, GWL_STYLE, style | PBS_MARQUEE);

	SendDlgItemMessage(hwndMain, IDC_PROGRESS, PBM_SETMARQUEE, 1, 0);

	/* ------------------------------------- *
	 * Check if updating portable build      */

	bool bIsPortable = false;

	if (cmdLine[0]) {
		int argc;
		LPWSTR *argv = CommandLineToArgvW(cmdLine, &argc);

		if (argv) {
			for (int i = 0; i < argc; i++) {
				if (wcscmp(argv[i], L"Portable") == 0) {
					bIsPortable = true;
				}
			}

			LocalFree((HLOCAL)argv);
		}
	}

	/* ------------------------------------- *
	 * Get config path                       */

	wchar_t lpAppDataPath[MAX_PATH];
	lpAppDataPath[0] = 0;

	if (bIsPortable) {
		GetCurrentDirectory(_countof(lpAppDataPath), lpAppDataPath);
		StringCbCat(lpAppDataPath, sizeof(lpAppDataPath), L"\\config");
	} else {
		DWORD ret;
		ret = GetEnvironmentVariable(L"OBS_USER_APPDATA_PATH",
					     lpAppDataPath,
					     _countof(lpAppDataPath));

		if (ret >= _countof(lpAppDataPath)) {
			Status(L"Update failed: Could not determine AppData "
			       L"location");
			return false;
		}

		if (!ret) {
			CoTaskMemPtr<wchar_t> pOut;
			HRESULT hr = SHGetKnownFolderPath(
				FOLDERID_RoamingAppData, KF_FLAG_DEFAULT,
				nullptr, &pOut);
			if (hr != S_OK) {
				Status(L"Update failed: Could not determine AppData "
				       L"location");
				return false;
			}

			StringCbCopy(lpAppDataPath, sizeof(lpAppDataPath),
				     pOut);
		}
	}

	StringCbCat(lpAppDataPath, sizeof(lpAppDataPath), L"\\obs-studio");

	/* ------------------------------------- *
	 * Get download path                     */

	wchar_t manifestPath[MAX_PATH];
	wchar_t tempDirName[MAX_PATH];

	manifestPath[0] = 0;
	tempDirName[0] = 0;

	StringCbPrintf(manifestPath, sizeof(manifestPath),
		       L"%s\\updates\\manifest.json", lpAppDataPath);
	if (!GetTempPathW(_countof(tempDirName), tempDirName)) {
		Status(L"Update failed: Failed to get temp path: %ld",
		       GetLastError());
		return false;
	}
	if (!GetTempFileNameW(tempDirName, L"obs-studio", 0, tempPath)) {
		Status(L"Update failed: Failed to create temp dir name: %ld",
		       GetLastError());
		return false;
	}

	DeleteFile(tempPath);
	CreateDirectory(tempPath, nullptr);

	/* ------------------------------------- *
	 * Load manifest file                    */

	Json root;
	{
		string manifestFile = QuickReadFile(manifestPath);
		if (manifestFile.empty()) {
			Status(L"Update failed: Couldn't load manifest file");
			return false;
		}

		string error;
		root = Json::parse(manifestFile, error);

		if (!error.empty()) {
			Status(L"Update failed: Couldn't parse update "
			       L"manifest: %S",
			       error.c_str());
			return false;
		}
	}

	if (!root.is_object()) {
		Status(L"Update failed: Invalid update manifest");
		return false;
	}

	/* ------------------------------------- *
	 * Parse current manifest update files   */

	const Json::array &packages = root["packages"].array_items();
	for (size_t i = 0; i < packages.size(); i++) {
		if (!AddPackageUpdateFiles(packages, i, tempPath)) {
			Status(L"Update failed: Failed to process update packages");
			return false;
		}
	}

	SendDlgItemMessage(hwndMain, IDC_PROGRESS, PBM_SETMARQUEE, 0, 0);
	SetWindowLongPtr(hProgress, GWL_STYLE, style);

	/* ------------------------------------- *
	 * Exit if updates already installed     */

	if (!updates.size()) {
		Status(L"All available updates are already installed.");
		SetDlgItemText(hwndMain, IDC_BUTTON, L"Launch OBS");
		return true;
	}

	/* ------------------------------------- *
	 * Check for VS2019 redistributables     */

	if (!HasVS2019Redist()) {
		if (!UpdateVS2019Redists(root)) {
			return false;
		}
	}

	/* ------------------------------------- *
	 * Generate file hash json               */

	Json::array files;

	for (update_t &update : updates) {
		wchar_t whash_string[BLAKE2_HASH_STR_LENGTH];
		char hash_string[BLAKE2_HASH_STR_LENGTH];
		char outputPath[MAX_PATH];

		if (!update.has_hash)
			continue;

		/* check hash */
		HashToString(update.my_hash, whash_string);
		if (wcscmp(whash_string, HASH_NULL) == 0)
			continue;

		if (!WideToUTF8Buf(hash_string, whash_string))
			continue;
		if (!WideToUTF8Buf(outputPath, update.basename.c_str()))
			continue;

		string package_path;
		package_path = update.packageName;
		package_path += "/";
		package_path += outputPath;

		files.emplace_back(Json::object{
			{"name", package_path},
			{"hash", hash_string},
		});
	}

	/* ------------------------------------- *
	 * Send file hashes                      */

	string newManifest;

	if (files.size() > 0) {
		string post_body;
		Json(files).dump(post_body);

		int responseCode;

		int len = (int)post_body.size();
		uLong compressSize = compressBound(len);
		string compressedJson;

		compressedJson.resize(compressSize);
		compress2((Bytef *)&compressedJson[0], &compressSize,
			  (const Bytef *)post_body.c_str(), len,
			  Z_BEST_COMPRESSION);
		compressedJson.resize(compressSize);

		bool success = !!HTTPPostData(PATCH_MANIFEST_URL,
					      (BYTE *)&compressedJson[0],
					      (int)compressedJson.size(),
					      L"Accept-Encoding: gzip",
					      &responseCode, newManifest);

		if (!success)
			return false;

		if (responseCode != 200) {
			Status(L"Update failed: HTTP/%d while trying to "
			       L"download patch manifest",
			       responseCode);
			return false;
		}
	} else {
		newManifest = "[]";
	}

	/* ------------------------------------- *
	 * Parse new manifest                    */

	string error;
	root = Json::parse(newManifest, error);
	if (!error.empty()) {
		Status(L"Update failed: Couldn't parse patch manifest: %S",
		       error.c_str());
		return false;
	}

	if (!root.is_array()) {
		Status(L"Update failed: Invalid patch manifest");
		return false;
	}

	size_t packageCount = root.array_items().size();

	for (size_t i = 0; i < packageCount; i++) {
		const Json &patch = root[i];

		if (!patch.is_object()) {
			Status(L"Update failed: Invalid patch manifest");
			return false;
		}

		const Json &name_json = patch["name"];
		const Json &hash_json = patch["hash"];
		const Json &source_json = patch["source"];
		const Json &size_json = patch["size"];

		if (!name_json.is_string())
			continue;
		if (!hash_json.is_string())
			continue;
		if (!source_json.is_string())
			continue;
		if (!size_json.is_number())
			continue;

		const string &name = name_json.string_value();
		const string &hash = hash_json.string_value();
		const string &source = source_json.string_value();
		int size = size_json.int_value();

		UpdateWithPatchIfAvailable(name.c_str(), hash.c_str(),
					   source.c_str(), size);
	}

	/* ------------------------------------- *
	 * Download Updates                      */

	if (!RunDownloadWorkers(4))
		return false;

	if ((size_t)completedUpdates != updates.size()) {
		Status(L"Update failed to download all files.");
		return false;
	}

	/* ------------------------------------- *
	 * Install updates                       */

	int updatesInstalled = 0;
	int lastPosition = 0;

	SendDlgItemMessage(hwndMain, IDC_PROGRESS, PBM_SETPOS, 0, 0);

	for (update_t &update : updates) {
		if (!UpdateFile(update)) {
			return false;
		} else {
			updatesInstalled++;
			int position = (int)(((float)updatesInstalled /
					      (float)completedUpdates) *
					     100.0f);
			if (position > lastPosition) {
				lastPosition = position;
				SendDlgItemMessage(hwndMain, IDC_PROGRESS,
						   PBM_SETPOS, position, 0);
			}
		}
	}

	/* ------------------------------------- *
	 * Install virtual camera                */

	auto runcommand = [](wchar_t *cmd) {
		STARTUPINFO si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi;
		bool success = !!CreateProcessW(nullptr, cmd, nullptr, nullptr,
						false, CREATE_NEW_CONSOLE,
						nullptr, nullptr, &si, &pi);
		if (success) {
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
	};

	if (!bIsPortable) {
		wchar_t regsvr[MAX_PATH];
		wchar_t src[MAX_PATH];
		wchar_t tmp[MAX_PATH];
		wchar_t tmp2[MAX_PATH];

		SHGetFolderPathW(nullptr, CSIDL_SYSTEM, nullptr,
				 SHGFP_TYPE_CURRENT, regsvr);
		StringCbCat(regsvr, sizeof(regsvr), L"\\regsvr32.exe");

		GetCurrentDirectoryW(_countof(src), src);
		StringCbCat(src, sizeof(src),
			    L"\\data\\obs-plugins\\win-dshow\\");

		StringCbCopy(tmp, sizeof(tmp), L"\"");
		StringCbCat(tmp, sizeof(tmp), regsvr);
		StringCbCat(tmp, sizeof(tmp), L"\" /s \"");
		StringCbCat(tmp, sizeof(tmp), src);
		StringCbCat(tmp, sizeof(tmp), L"obs-virtualcam-module");

		StringCbCopy(tmp2, sizeof(tmp2), tmp);
		StringCbCat(tmp2, sizeof(tmp2), L"32.dll\"");
		runcommand(tmp2);

		if (is_64bit_windows()) {
			StringCbCopy(tmp2, sizeof(tmp2), tmp);
			StringCbCat(tmp2, sizeof(tmp2), L"64.dll\"");
			runcommand(tmp2);
		}
	}

	/* ------------------------------------- *
	 * Update hook files and vulkan registry */

	UpdateHookFiles();

	/* ------------------------------------- *
	 * Finish                                */

	/* If we get here, all updates installed successfully so we can purge
	 * the old versions */
	for (update_t &update : updates) {
		if (!update.previousFile.empty())
			DeleteFile(update.previousFile.c_str());

		/* We delete here not above in case of duplicate hashes */
		if (!update.tempPath.empty())
			DeleteFile(update.tempPath.c_str());
	}

	SendDlgItemMessage(hwndMain, IDC_PROGRESS, PBM_SETPOS, 100, 0);

	Status(L"Update complete.");
	SetDlgItemText(hwndMain, IDC_BUTTON, L"Launch OBS");
	return true;
}

static DWORD WINAPI UpdateThread(void *arg)
{
	wchar_t *cmdLine = (wchar_t *)arg;

	bool success = Update(cmdLine);

	if (!success) {
		/* This handles deleting temp files and rolling back and
		 * partially installed updates */
		CleanupPartialUpdates();

		if (tempPath[0])
			RemoveDirectory(tempPath);

		if (WaitForSingleObject(cancelRequested, 0) == WAIT_OBJECT_0)
			Status(L"Update aborted.");

		HWND hProgress = GetDlgItem(hwndMain, IDC_PROGRESS);
		LONG_PTR style = GetWindowLongPtr(hProgress, GWL_STYLE);
		SetWindowLongPtr(hProgress, GWL_STYLE, style & ~PBS_MARQUEE);
		SendMessage(hProgress, PBM_SETSTATE, PBST_ERROR, 0);

		SetDlgItemText(hwndMain, IDC_BUTTON, L"Exit");
		EnableWindow(GetDlgItem(hwndMain, IDC_BUTTON), true);

		updateFailed = true;
	} else {
		if (tempPath[0])
			RemoveDirectory(tempPath);
	}

	if (bExiting)
		ExitProcess(success);
	return 0;
}

static void CancelUpdate(bool quit)
{
	if (WaitForSingleObject(updateThread, 0) != WAIT_OBJECT_0) {
		bExiting = quit;
		SetEvent(cancelRequested);
	} else {
		PostQuitMessage(0);
	}
}

static void LaunchOBS()
{
	wchar_t cwd[MAX_PATH];
	wchar_t newCwd[MAX_PATH];
	wchar_t obsPath[MAX_PATH];

	GetCurrentDirectory(_countof(cwd) - 1, cwd);

	StringCbCopy(obsPath, sizeof(obsPath), cwd);
	StringCbCat(obsPath, sizeof(obsPath),
		    is32bit ? L"\\bin\\32bit" : L"\\bin\\64bit");
	SetCurrentDirectory(obsPath);
	StringCbCopy(newCwd, sizeof(newCwd), obsPath);

	StringCbCat(obsPath, sizeof(obsPath),
		    is32bit ? L"\\obs32.exe" : L"\\obs64.exe");

	if (!FileExists(obsPath)) {
		StringCbCopy(obsPath, sizeof(obsPath), cwd);
		StringCbCat(obsPath, sizeof(obsPath), L"\\bin\\32bit");
		SetCurrentDirectory(obsPath);
		StringCbCopy(newCwd, sizeof(newCwd), obsPath);

		StringCbCat(obsPath, sizeof(obsPath), L"\\obs32.exe");

		if (!FileExists(obsPath)) {
			/* TODO: give user a message maybe? */
			return;
		}
	}

	SHELLEXECUTEINFO execInfo;

	ZeroMemory(&execInfo, sizeof(execInfo));

	execInfo.cbSize = sizeof(execInfo);
	execInfo.lpFile = obsPath;
	execInfo.lpDirectory = newCwd;
	execInfo.nShow = SW_SHOWNORMAL;

	ShellExecuteEx(&execInfo);
}

static INT_PTR CALLBACK UpdateDialogProc(HWND hwnd, UINT message, WPARAM wParam,
					 LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG: {
		static HICON hMainIcon =
			LoadIcon(hinstMain, MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hMainIcon);
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hMainIcon);
		return true;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_BUTTON) {
			if (HIWORD(wParam) == BN_CLICKED) {
				DWORD result =
					WaitForSingleObject(updateThread, 0);
				if (result == WAIT_OBJECT_0) {
					if (updateFailed)
						PostQuitMessage(0);
					else
						PostQuitMessage(1);
				} else {
					EnableWindow((HWND)lParam, false);
					CancelUpdate(false);
				}
			}
		}
		return true;

	case WM_CLOSE:
		CancelUpdate(true);
		return true;
	}

	return false;
}

static int RestartAsAdmin(LPCWSTR lpCmdLine, LPCWSTR cwd)
{
	wchar_t myPath[MAX_PATH];
	if (!GetModuleFileNameW(nullptr, myPath, _countof(myPath) - 1)) {
		return 0;
	}

	SHELLEXECUTEINFO shExInfo = {0};
	shExInfo.cbSize = sizeof(shExInfo);
	shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shExInfo.hwnd = 0;
	shExInfo.lpVerb = L"runas";        /* Operation to perform */
	shExInfo.lpFile = myPath;          /* Application to start */
	shExInfo.lpParameters = lpCmdLine; /* Additional parameters */
	shExInfo.lpDirectory = cwd;
	shExInfo.nShow = SW_NORMAL;
	shExInfo.hInstApp = 0;

	/* annoyingly the actual elevated updater will disappear behind other
	 * windows :( */
	AllowSetForegroundWindow(ASFW_ANY);

	/* if the admin is a different user, save the path to the user's
	 * appdata so we can load the correct manifest */
	CoTaskMemPtr<wchar_t> pOut;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData,
					  KF_FLAG_DEFAULT, nullptr, &pOut);
	if (hr == S_OK)
		SetEnvironmentVariable(L"OBS_USER_APPDATA_PATH", pOut);

	if (ShellExecuteEx(&shExInfo)) {
		DWORD exitCode;

		WaitForSingleObject(shExInfo.hProcess, INFINITE);

		if (GetExitCodeProcess(shExInfo.hProcess, &exitCode)) {
			if (exitCode == 1) {
				return exitCode;
			}
		}
		CloseHandle(shExInfo.hProcess);
	}
	return 0;
}

static bool HasElevation()
{
	SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
	PSID sid = nullptr;
	BOOL elevated = false;
	BOOL success;

	success = AllocateAndInitializeSid(&sia, 2, SECURITY_BUILTIN_DOMAIN_RID,
					   DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0,
					   0, 0, &sid);
	if (success && sid) {
		CheckTokenMembership(nullptr, sid, &elevated);
		FreeSid(sid);
	}

	return elevated;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{
	INITCOMMONCONTROLSEX icce;

	wchar_t cwd[MAX_PATH];
	wchar_t newPath[MAX_PATH];
	GetCurrentDirectoryW(_countof(cwd) - 1, cwd);

	is32bit = wcsstr(cwd, L"bin\\32bit") != nullptr;

	if (!HasElevation()) {

		WinHandle hMutex = OpenMutex(
			SYNCHRONIZE, false, L"OBSUpdaterRunningAsNonAdminUser");
		if (hMutex) {
			MessageBox(
				nullptr, L"Updater Error",
				L"OBS Studio Updater must be run as an administrator.",
				MB_ICONWARNING);
			return 2;
		}

		HANDLE hLowMutex = CreateMutexW(
			nullptr, true, L"OBSUpdaterRunningAsNonAdminUser");

		/* return code 1 =  user wanted to launch OBS */
		if (RestartAsAdmin(lpCmdLine, cwd) == 1) {
			StringCbCat(cwd, sizeof(cwd), L"\\..\\..");
			GetFullPathName(cwd, _countof(newPath), newPath,
					nullptr);
			SetCurrentDirectory(newPath);

			LaunchOBS();
		}

		if (hLowMutex) {
			ReleaseMutex(hLowMutex);
			CloseHandle(hLowMutex);
		}

		return 0;
	} else {
		StringCbCat(cwd, sizeof(cwd), L"\\..\\..");
		GetFullPathName(cwd, _countof(newPath), newPath, nullptr);
		SetCurrentDirectory(newPath);

		hinstMain = hInstance;

		icce.dwSize = sizeof(icce);
		icce.dwICC = ICC_PROGRESS_CLASS;

		InitCommonControlsEx(&icce);

		hwndMain = CreateDialog(hInstance,
					MAKEINTRESOURCE(IDD_UPDATEDIALOG),
					nullptr, UpdateDialogProc);
		if (!hwndMain) {
			return -1;
		}

		ShowWindow(hwndMain, SW_SHOWNORMAL);
		SetForegroundWindow(hwndMain);

		cancelRequested = CreateEvent(nullptr, true, false, nullptr);
		updateThread = CreateThread(nullptr, 0, UpdateThread, lpCmdLine,
					    0, nullptr);

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			if (!IsDialogMessage(hwndMain, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		/* there is no non-elevated process waiting for us if UAC is
		 * disabled */
		WinHandle hMutex = OpenMutex(
			SYNCHRONIZE, false, L"OBSUpdaterRunningAsNonAdminUser");
		if (msg.wParam == 1 && !hMutex) {
			LaunchOBS();
		}

		return (int)msg.wParam;
	}
}
