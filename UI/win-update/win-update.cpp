#include "win-update-helpers.hpp"
#include "update-window.hpp"
#include "remote-text.hpp"
#include "qt-wrappers.hpp"
#include "win-update.hpp"
#include "obs-app.hpp"

#include <QMessageBox>

#include <string>
#include <mutex>

#include <util/windows/WinHandle.hpp>
#include <util/util.hpp>
#include <json11.hpp>
#include <blake2.h>

#include <time.h>
#include <strsafe.h>
#include <winhttp.h>
#include <shellapi.h>

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

using namespace std;
using namespace json11;

struct QCef;
extern QCef *cef;

/* ------------------------------------------------------------------------ */

#ifndef WIN_MANIFEST_URL
#define WIN_MANIFEST_URL "https://obsproject.com/update_studio/manifest.json"
#endif

#ifndef WIN_WHATSNEW_URL
#define WIN_WHATSNEW_URL "https://obsproject.com/update_studio/whatsnew.json"
#endif

#ifndef WIN_UPDATER_URL
#define WIN_UPDATER_URL "https://obsproject.com/update_studio/updater.exe"
#endif

static __declspec(thread) HCRYPTPROV provider = 0;

#pragma pack(push, r1, 1)

typedef struct {
	BLOBHEADER blobheader;
	RSAPUBKEY rsapubkey;
} PUBLICKEYHEADER;

#pragma pack(pop, r1)

#define BLAKE2_HASH_LENGTH 20
#define BLAKE2_HASH_STR_LENGTH ((BLAKE2_HASH_LENGTH * 2) + 1)

#define TEST_BUILD

// Hard coded 4096 bit RSA public key for obsproject.com in PEM format
static const unsigned char obs_pub[] = {
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
	0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x43, 0x49, 0x6a, 0x41, 0x4e, 0x42,
	0x67, 0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41,
	0x51, 0x45, 0x46, 0x41, 0x41, 0x4f, 0x43, 0x41, 0x67, 0x38, 0x41, 0x4d,
	0x49, 0x49, 0x43, 0x43, 0x67, 0x4b, 0x43, 0x41, 0x67, 0x45, 0x41, 0x6c,
	0x33, 0x73, 0x76, 0x65, 0x72, 0x77, 0x39, 0x48, 0x51, 0x2b, 0x72, 0x59,
	0x51, 0x4e, 0x6e, 0x39, 0x43, 0x61, 0x37, 0x0a, 0x39, 0x4c, 0x55, 0x36,
	0x32, 0x6e, 0x47, 0x36, 0x4e, 0x6f, 0x7a, 0x45, 0x2f, 0x46, 0x73, 0x49,
	0x56, 0x4e, 0x65, 0x72, 0x2b, 0x57, 0x2f, 0x68, 0x75, 0x65, 0x45, 0x38,
	0x57, 0x51, 0x31, 0x6d, 0x72, 0x46, 0x50, 0x2b, 0x32, 0x79, 0x41, 0x2b,
	0x69, 0x59, 0x52, 0x75, 0x74, 0x59, 0x50, 0x65, 0x45, 0x67, 0x70, 0x78,
	0x74, 0x6f, 0x64, 0x48, 0x68, 0x67, 0x6b, 0x52, 0x34, 0x70, 0x45, 0x4b,
	0x0a, 0x56, 0x6e, 0x72, 0x72, 0x31, 0x38, 0x71, 0x34, 0x73, 0x7a, 0x6c,
	0x76, 0x38, 0x39, 0x51, 0x49, 0x37, 0x74, 0x38, 0x6c, 0x4d, 0x6f, 0x4c,
	0x54, 0x6c, 0x46, 0x2b, 0x74, 0x31, 0x49, 0x52, 0x30, 0x56, 0x34, 0x77,
	0x4a, 0x56, 0x33, 0x34, 0x49, 0x33, 0x43, 0x2b, 0x33, 0x35, 0x39, 0x4b,
	0x69, 0x78, 0x6e, 0x7a, 0x4c, 0x30, 0x42, 0x6c, 0x39, 0x61, 0x6a, 0x2f,
	0x7a, 0x44, 0x63, 0x72, 0x58, 0x0a, 0x57, 0x6c, 0x35, 0x70, 0x48, 0x54,
	0x69, 0x6f, 0x4a, 0x77, 0x59, 0x4f, 0x67, 0x4d, 0x69, 0x42, 0x47, 0x4c,
	0x79, 0x50, 0x65, 0x69, 0x74, 0x4d, 0x46, 0x64, 0x6a, 0x6a, 0x54, 0x49,
	0x70, 0x43, 0x4d, 0x2b, 0x6d, 0x78, 0x54, 0x57, 0x58, 0x43, 0x72, 0x5a,
	0x39, 0x64, 0x50, 0x55, 0x4b, 0x76, 0x5a, 0x74, 0x67, 0x7a, 0x6a, 0x64,
	0x2b, 0x49, 0x7a, 0x6c, 0x48, 0x69, 0x64, 0x48, 0x74, 0x4f, 0x0a, 0x4f,
	0x52, 0x42, 0x4e, 0x35, 0x6d, 0x52, 0x73, 0x38, 0x4c, 0x4e, 0x4f, 0x35,
	0x38, 0x6b, 0x37, 0x39, 0x72, 0x37, 0x37, 0x44, 0x63, 0x67, 0x51, 0x59,
	0x50, 0x4e, 0x69, 0x69, 0x43, 0x74, 0x57, 0x67, 0x43, 0x2b, 0x59, 0x34,
	0x4b, 0x37, 0x75, 0x53, 0x5a, 0x58, 0x33, 0x48, 0x76, 0x65, 0x6f, 0x6d,
	0x32, 0x74, 0x48, 0x62, 0x56, 0x58, 0x79, 0x30, 0x4c, 0x2f, 0x43, 0x6c,
	0x37, 0x66, 0x4d, 0x0a, 0x48, 0x4b, 0x71, 0x66, 0x63, 0x51, 0x47, 0x75,
	0x79, 0x72, 0x76, 0x75, 0x64, 0x34, 0x32, 0x4f, 0x72, 0x57, 0x61, 0x72,
	0x41, 0x73, 0x6e, 0x32, 0x70, 0x32, 0x45, 0x69, 0x36, 0x4b, 0x7a, 0x78,
	0x62, 0x33, 0x47, 0x36, 0x45, 0x53, 0x43, 0x77, 0x31, 0x35, 0x6e, 0x48,
	0x41, 0x67, 0x4c, 0x61, 0x6c, 0x38, 0x7a, 0x53, 0x71, 0x37, 0x2b, 0x72,
	0x61, 0x45, 0x2f, 0x78, 0x6b, 0x4c, 0x70, 0x43, 0x0a, 0x62, 0x59, 0x67,
	0x35, 0x67, 0x6d, 0x59, 0x36, 0x76, 0x62, 0x6d, 0x57, 0x6e, 0x71, 0x39,
	0x64, 0x71, 0x57, 0x72, 0x55, 0x7a, 0x61, 0x71, 0x4f, 0x66, 0x72, 0x5a,
	0x50, 0x67, 0x76, 0x67, 0x47, 0x30, 0x57, 0x76, 0x6b, 0x42, 0x53, 0x68,
	0x66, 0x61, 0x45, 0x4f, 0x42, 0x61, 0x49, 0x55, 0x78, 0x41, 0x33, 0x51,
	0x42, 0x67, 0x7a, 0x41, 0x5a, 0x68, 0x71, 0x65, 0x65, 0x64, 0x46, 0x39,
	0x68, 0x0a, 0x61, 0x66, 0x4d, 0x47, 0x4d, 0x4d, 0x39, 0x71, 0x56, 0x62,
	0x66, 0x77, 0x75, 0x75, 0x7a, 0x4a, 0x32, 0x75, 0x68, 0x2b, 0x49, 0x6e,
	0x61, 0x47, 0x61, 0x65, 0x48, 0x32, 0x63, 0x30, 0x34, 0x6f, 0x56, 0x63,
	0x44, 0x46, 0x66, 0x65, 0x4f, 0x61, 0x44, 0x75, 0x78, 0x52, 0x6a, 0x43,
	0x43, 0x62, 0x71, 0x72, 0x35, 0x73, 0x4c, 0x53, 0x6f, 0x31, 0x43, 0x57,
	0x6f, 0x6b, 0x79, 0x6e, 0x6a, 0x4e, 0x0a, 0x43, 0x42, 0x2b, 0x62, 0x32,
	0x72, 0x51, 0x46, 0x37, 0x44, 0x50, 0x50, 0x62, 0x44, 0x34, 0x73, 0x2f,
	0x6e, 0x54, 0x39, 0x4e, 0x73, 0x63, 0x6b, 0x2f, 0x4e, 0x46, 0x7a, 0x72,
	0x42, 0x58, 0x52, 0x4f, 0x2b, 0x64, 0x71, 0x6b, 0x65, 0x42, 0x77, 0x44,
	0x55, 0x43, 0x76, 0x37, 0x62, 0x5a, 0x67, 0x57, 0x37, 0x4f, 0x78, 0x75,
	0x4f, 0x58, 0x30, 0x37, 0x4c, 0x54, 0x71, 0x66, 0x70, 0x35, 0x73, 0x0a,
	0x4f, 0x65, 0x47, 0x67, 0x75, 0x62, 0x75, 0x62, 0x69, 0x77, 0x59, 0x33,
	0x55, 0x64, 0x48, 0x59, 0x71, 0x2b, 0x4c, 0x39, 0x4a, 0x71, 0x49, 0x53,
	0x47, 0x31, 0x74, 0x4d, 0x34, 0x48, 0x65, 0x4b, 0x6a, 0x61, 0x48, 0x6a,
	0x75, 0x31, 0x4d, 0x44, 0x6a, 0x76, 0x48, 0x5a, 0x32, 0x44, 0x62, 0x6d,
	0x4c, 0x77, 0x55, 0x78, 0x75, 0x59, 0x61, 0x36, 0x4a, 0x5a, 0x44, 0x4b,
	0x57, 0x73, 0x37, 0x72, 0x0a, 0x49, 0x72, 0x64, 0x44, 0x77, 0x78, 0x33,
	0x4a, 0x77, 0x61, 0x63, 0x46, 0x36, 0x36, 0x68, 0x33, 0x59, 0x55, 0x57,
	0x36, 0x74, 0x7a, 0x55, 0x5a, 0x68, 0x7a, 0x74, 0x63, 0x6d, 0x51, 0x65,
	0x70, 0x50, 0x2f, 0x75, 0x37, 0x42, 0x67, 0x47, 0x72, 0x6b, 0x4f, 0x50,
	0x50, 0x70, 0x59, 0x41, 0x30, 0x4e, 0x45, 0x4a, 0x38, 0x30, 0x53, 0x65,
	0x41, 0x78, 0x37, 0x68, 0x69, 0x4e, 0x34, 0x76, 0x61, 0x0a, 0x65, 0x45,
	0x51, 0x4b, 0x6e, 0x52, 0x6e, 0x2b, 0x45, 0x70, 0x42, 0x4e, 0x36, 0x55,
	0x42, 0x61, 0x35, 0x66, 0x37, 0x4c, 0x6f, 0x4b, 0x38, 0x43, 0x41, 0x77,
	0x45, 0x41, 0x41, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x45, 0x4e, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b,
	0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a};
static const unsigned int obs_pub_len = 800;

/* ------------------------------------------------------------------------ */

static bool QuickWriteFile(const char *file, const void *data, size_t size)
try {
	BPtr<wchar_t> w_file;
	if (os_utf8_to_wcs_ptr(file, 0, &w_file) == 0)
		return false;

	WinHandle handle = CreateFileW(w_file, GENERIC_WRITE, 0, nullptr,
				       CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH,
				       nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", file,
				GetLastError());

	DWORD written;
	if (!WriteFile(handle, data, (DWORD)size, &written, nullptr))
		throw strprintf("Failed to write file '%s': %lu", file,
				GetLastError());

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

static bool QuickReadFile(const char *file, string &data)
try {
	BPtr<wchar_t> w_file;
	if (os_utf8_to_wcs_ptr(file, 0, &w_file) == 0)
		return false;

	WinHandle handle = CreateFileW(w_file, GENERIC_READ, FILE_SHARE_READ,
				       nullptr, OPEN_EXISTING, 0, nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", file,
				GetLastError());

	DWORD size = GetFileSize(handle, nullptr);
	data.resize(size);

	DWORD read;
	if (!ReadFile(handle, &data[0], size, &read, nullptr))
		throw strprintf("Failed to write file '%s': %lu", file,
				GetLastError());

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

static void HashToString(const uint8_t *in, char *out)
{
	const char alphabet[] = "0123456789abcdef";

	for (int i = 0; i != BLAKE2_HASH_LENGTH; ++i) {
		out[2 * i] = alphabet[in[i] / 16];
		out[2 * i + 1] = alphabet[in[i] % 16];
	}

	out[BLAKE2_HASH_LENGTH * 2] = 0;
}

static bool CalculateFileHash(const char *path, uint8_t *hash)
try {
	blake2b_state blake2;
	if (blake2b_init(&blake2, BLAKE2_HASH_LENGTH) != 0)
		return false;

	BPtr<wchar_t> w_path;
	if (os_utf8_to_wcs_ptr(path, 0, &w_path) == 0)
		return false;

	WinHandle handle = CreateFileW(w_path, GENERIC_READ, FILE_SHARE_READ,
				       nullptr, OPEN_EXISTING, 0, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", path,
				GetLastError());

	vector<BYTE> buf;
	buf.resize(65536);

	for (;;) {
		DWORD read = 0;
		if (!ReadFile(handle, buf.data(), (DWORD)buf.size(), &read,
			      nullptr))
			throw strprintf("Failed to read file '%s': %lu", path,
					GetLastError());

		if (!read)
			break;

		if (blake2b_update(&blake2, buf.data(), read) != 0)
			return false;
	}

	if (blake2b_final(&blake2, hash, BLAKE2_HASH_LENGTH) != 0)
		return false;

	return true;

} catch (string &text) {
	blog(LOG_DEBUG, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

static bool VerifyDigitalSignature(uint8_t *buf, size_t len, uint8_t *sig,
				   size_t sigLen)
{
	/* ASN of PEM public key */
	BYTE binaryKey[1024];
	DWORD binaryKeyLen = sizeof(binaryKey);

	/* Windows X509 public key info from ASN */
	LocalPtr<CERT_PUBLIC_KEY_INFO> publicPBLOB;
	DWORD iPBLOBSize;

	/* RSA BLOB info from X509 public key */
	LocalPtr<PUBLICKEYHEADER> rsaPublicBLOB;
	DWORD rsaPublicBLOBSize;

	/* Handle to public key */
	CryptKey keyOut;

	/* Handle to hash context */
	CryptHash hash;

	/* Signature in little-endian format */
	vector<BYTE> reversedSig;

	if (!CryptStringToBinaryA((LPCSTR)obs_pub, obs_pub_len,
				  CRYPT_STRING_BASE64HEADER, binaryKey,
				  &binaryKeyLen, nullptr, nullptr))
		return false;

	if (!CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
				 binaryKey, binaryKeyLen,
				 CRYPT_DECODE_ALLOC_FLAG, nullptr, &publicPBLOB,
				 &iPBLOBSize))
		return false;

	if (!CryptDecodeObjectEx(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB,
				 publicPBLOB->PublicKey.pbData,
				 publicPBLOB->PublicKey.cbData,
				 CRYPT_DECODE_ALLOC_FLAG, nullptr,
				 &rsaPublicBLOB, &rsaPublicBLOBSize))
		return false;

	if (!CryptImportKey(provider, (const BYTE *)rsaPublicBLOB.get(),
			    rsaPublicBLOBSize, 0, 0, &keyOut))
		return false;

	if (!CryptCreateHash(provider, CALG_SHA_512, 0, 0, &hash))
		return false;

	if (!CryptHashData(hash, buf, (DWORD)len, 0))
		return false;

	/* Windows requires signature in little-endian. Every other crypto
	 * provider is big-endian of course. */
	reversedSig.resize(sigLen);
	for (size_t i = 0; i < sigLen; i++)
		reversedSig[i] = sig[sigLen - i - 1];

	if (!CryptVerifySignature(hash, reversedSig.data(), (DWORD)sigLen,
				  keyOut, nullptr, 0))
		return false;

	return true;
}

static inline void HexToByteArray(const char *hexStr, size_t hexLen,
				  vector<uint8_t> &out)
{
	char ptr[3];

	ptr[2] = 0;

	for (size_t i = 0; i < hexLen; i += 2) {
		ptr[0] = hexStr[i];
		ptr[1] = hexStr[i + 1];
		out.push_back((uint8_t)strtoul(ptr, nullptr, 16));
	}
}

static bool CheckDataSignature(const string &data, const char *name,
			       const char *hexSig, size_t sigLen)
try {
	if (sigLen == 0 || sigLen > 0xFFFF || (sigLen & 1) != 0)
		throw strprintf("Missing or invalid signature for %s", name);

	/* Convert TCHAR signature to byte array */
	vector<uint8_t> signature;
	signature.reserve(sigLen);
	HexToByteArray(hexSig, sigLen, signature);

	if (!VerifyDigitalSignature((uint8_t *)data.data(), data.size(),
				    signature.data(), signature.size()))
		throw strprintf("Signature check failed for %s", name);

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

static bool FetchUpdaterModule(const char *url)
try {
	long responseCode;
	uint8_t updateFileHash[BLAKE2_HASH_LENGTH];
	vector<string> extraHeaders;

	BPtr<char> updateFilePath =
		GetConfigPathPtr("obs-studio\\updates\\updater.exe");

	if (CalculateFileHash(updateFilePath, updateFileHash)) {
		char hashString[BLAKE2_HASH_STR_LENGTH];
		HashToString(updateFileHash, hashString);

		string header = "If-None-Match: ";
		header += hashString;
		extraHeaders.push_back(move(header));
	}

	string signature;
	string error;
	string data;

	bool success = GetRemoteFile(url, data, error, &responseCode, nullptr,
				     nullptr, extraHeaders, &signature);

	if (!success || (responseCode != 200 && responseCode != 304)) {
		if (responseCode == 404)
			return false;

		throw strprintf("Could not fetch '%s': %s", url, error.c_str());
	}

	/* A new file must be digitally signed */
	if (responseCode == 200) {
		bool valid = CheckDataSignature(data, url, signature.data(),
						signature.size());
		if (!valid)
			throw string("Invalid updater module signature");

		if (!QuickWriteFile(updateFilePath, data.data(), data.size()))
			return false;
	}

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

static bool ParseUpdateManifest(const char *manifest, bool *updatesAvailable,
				string &notes_str, int &updateVer)
try {

	string error;
	Json root = Json::parse(manifest, error);
	if (!error.empty())
		throw strprintf("Failed reading json string: %s",
				error.c_str());

	if (!root.is_object())
		throw string("Root of manifest is not an object");

	int major = root["version_major"].int_value();
	int minor = root["version_minor"].int_value();
	int patch = root["version_patch"].int_value();

	if (major == 0)
		throw strprintf("Invalid version number: %d.%d.%d", major,
				minor, patch);

	const Json &notes = root["notes"];
	if (!notes.is_string())
		throw string("'notes' value invalid");

	notes_str = notes.string_value();

	const Json &packages = root["packages"];
	if (!packages.is_array())
		throw string("'packages' value invalid");

	int cur_ver = LIBOBS_API_VER;
	int new_ver = MAKE_SEMANTIC_VERSION(major, minor, patch);

	updateVer = new_ver;
	*updatesAvailable = new_ver > cur_ver;

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

void GenerateGUID(string &guid)
{
	BYTE junk[20];

	if (!CryptGenRandom(provider, sizeof(junk), junk))
		return;

	guid.resize(41);
	HashToString(junk, &guid[0]);
}

string GetProgramGUID()
{
	static mutex m;
	lock_guard<mutex> lock(m);

	/* NOTE: this is an arbitrary random number that we use to count the
	 * number of unique OBS installations and is not associated with any
	 * kind of identifiable information */
	const char *pguid =
		config_get_string(GetGlobalConfig(), "General", "InstallGUID");
	string guid;
	if (pguid)
		guid = pguid;

	if (guid.empty()) {
		GenerateGUID(guid);

		if (!guid.empty())
			config_set_string(GetGlobalConfig(), "General",
					  "InstallGUID", guid.c_str());
	}

	return guid;
}

void AutoUpdateThread::infoMsg(const QString &title, const QString &text)
{
	OBSMessageBox::information(App()->GetMainWindow(), title, text);
}

void AutoUpdateThread::info(const QString &title, const QString &text)
{
	QMetaObject::invokeMethod(this, "infoMsg", Qt::BlockingQueuedConnection,
				  Q_ARG(QString, title), Q_ARG(QString, text));
}

int AutoUpdateThread::queryUpdateSlot(bool localManualUpdate,
				      const QString &text)
{
	OBSUpdate updateDlg(App()->GetMainWindow(), localManualUpdate, text);
	return updateDlg.exec();
}

int AutoUpdateThread::queryUpdate(bool localManualUpdate, const char *text_utf8)
{
	int ret = OBSUpdate::No;
	QString text = text_utf8;
	QMetaObject::invokeMethod(this, "queryUpdateSlot",
				  Qt::BlockingQueuedConnection,
				  Q_RETURN_ARG(int, ret),
				  Q_ARG(bool, localManualUpdate),
				  Q_ARG(QString, text));
	return ret;
}

void AutoUpdateThread::run()
try {
	long responseCode;
	vector<string> extraHeaders;
	string text;
	string error;
	string signature;
	CryptProvider localProvider;
	BYTE manifestHash[BLAKE2_HASH_LENGTH];
	bool updatesAvailable = false;
	bool success;

	struct FinishedTrigger {
		inline ~FinishedTrigger()
		{
			QMetaObject::invokeMethod(App()->GetMainWindow(),
						  "updateCheckFinished");
		}
	} finishedTrigger;

	BPtr<char> manifestPath =
		GetConfigPathPtr("obs-studio\\updates\\manifest.json");

	/* ----------------------------------- *
	 * create signature provider           */

	if (!CryptAcquireContext(&localProvider, nullptr, MS_ENH_RSA_AES_PROV,
				 PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		throw strprintf("CryptAcquireContext failed: %lu",
				GetLastError());

	provider = localProvider;

	/* ----------------------------------- *
	 * avoid downloading manifest again    */

	if (CalculateFileHash(manifestPath, manifestHash)) {
		char hashString[BLAKE2_HASH_STR_LENGTH];
		HashToString(manifestHash, hashString);

		string header = "If-None-Match: ";
		header += hashString;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get current install GUID            */

	string guid = GetProgramGUID();
	if (!guid.empty()) {
		string header = "X-OBS2-GUID: ";
		header += guid;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get manifest from server            */

	success = GetRemoteFile(WIN_MANIFEST_URL, text, error, &responseCode,
				nullptr, nullptr, extraHeaders, &signature);

	if (!success || (responseCode != 200 && responseCode != 304)) {
		if (responseCode == 404)
			return;

		throw strprintf("Failed to fetch manifest file: %s",
				error.c_str());
	}

	/* ----------------------------------- *
	 * verify file signature               */

	/* a new file must be digitally signed */
	if (responseCode == 200) {
		success = CheckDataSignature(text, "manifest", signature.data(),
					     signature.size());
		if (!success)
			throw string("Invalid manifest signature");
	}

	/* ----------------------------------- *
	 * write or load manifest              */

	if (responseCode == 200) {
		if (!QuickWriteFile(manifestPath, text.data(), text.size()))
			throw strprintf("Could not write file '%s'",
					manifestPath.Get());
	} else {
		if (!QuickReadFile(manifestPath, text))
			throw strprintf("Could not read file '%s'",
					manifestPath.Get());
	}

	/* ----------------------------------- *
	 * check manifest for update           */

	string notes;
	int updateVer = 0;

	success = ParseUpdateManifest(text.c_str(), &updatesAvailable, notes,
				      updateVer);
	if (!success)
		throw string("Failed to parse manifest");

	if (!updatesAvailable) {
		if (manualUpdate)
			info(QTStr("Updater.NoUpdatesAvailable.Title"),
			     QTStr("Updater.NoUpdatesAvailable.Text"));
		return;
	}

	/* ----------------------------------- *
	 * skip this version if set to skip    */

	int skipUpdateVer = config_get_int(GetGlobalConfig(), "General",
					   "SkipUpdateVersion");
	if (!manualUpdate && updateVer == skipUpdateVer)
		return;

	/* ----------------------------------- *
	 * fetch updater module                */

	if (!FetchUpdaterModule(WIN_UPDATER_URL))
		return;

	/* ----------------------------------- *
	 * query user for update               */

	int queryResult = queryUpdate(manualUpdate, notes.c_str());

	if (queryResult == OBSUpdate::No) {
		if (!manualUpdate) {
			long long t = (long long)time(nullptr);
			config_set_int(GetGlobalConfig(), "General",
				       "LastUpdateCheck", t);
		}
		return;

	} else if (queryResult == OBSUpdate::Skip) {
		config_set_int(GetGlobalConfig(), "General",
			       "SkipUpdateVersion", updateVer);
		return;
	}

	/* ----------------------------------- *
	 * get working dir                     */

	wchar_t cwd[MAX_PATH];
	GetModuleFileNameW(nullptr, cwd, _countof(cwd) - 1);
	wchar_t *p = wcsrchr(cwd, '\\');
	if (p)
		*p = 0;

	/* ----------------------------------- *
	 * execute updater                     */

	BPtr<char> updateFilePath =
		GetConfigPathPtr("obs-studio\\updates\\updater.exe");
	BPtr<wchar_t> wUpdateFilePath;

	size_t size = os_utf8_to_wcs_ptr(updateFilePath, 0, &wUpdateFilePath);
	if (!size)
		throw string("Could not convert updateFilePath to wide");

	/* note, can't use CreateProcess to launch as admin. */
	SHELLEXECUTEINFO execInfo = {};

	execInfo.cbSize = sizeof(execInfo);
	execInfo.lpFile = wUpdateFilePath;
#ifndef UPDATE_CHANNEL
#define UPDATE_ARG_SUFFIX L""
#else
#define UPDATE_ARG_SUFFIX UPDATE_CHANNEL
#endif
	if (App()->IsPortableMode())
		execInfo.lpParameters = UPDATE_ARG_SUFFIX L" Portable";
	else
		execInfo.lpParameters = UPDATE_ARG_SUFFIX;

	execInfo.lpDirectory = cwd;
	execInfo.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteEx(&execInfo)) {
		QString msg = QTStr("Updater.FailedToLaunch");
		info(msg, msg);
		throw strprintf("Can't launch updater '%s': %d",
				updateFilePath.Get(), GetLastError());
	}

	/* force OBS to perform another update check immediately after updating
	 * in case of issues with the new version */
	config_set_int(GetGlobalConfig(), "General", "LastUpdateCheck", 0);
	config_set_int(GetGlobalConfig(), "General", "SkipUpdateVersion", 0);
	config_set_string(GetGlobalConfig(), "General", "InstallGUID",
			  guid.c_str());

	QMetaObject::invokeMethod(App()->GetMainWindow(), "close");

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
}

/* ------------------------------------------------------------------------ */

void WhatsNewInfoThread::run()
try {
	long responseCode;
	vector<string> extraHeaders;
	string text;
	string error;
	string signature;
	CryptProvider localProvider;
	BYTE whatsnewHash[BLAKE2_HASH_LENGTH];
	bool success;

	BPtr<char> whatsnewPath =
		GetConfigPathPtr("obs-studio\\updates\\whatsnew.json");

	/* ----------------------------------- *
	 * create signature provider           */

	if (!CryptAcquireContext(&localProvider, nullptr, MS_ENH_RSA_AES_PROV,
				 PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		throw strprintf("CryptAcquireContext failed: %lu",
				GetLastError());

	provider = localProvider;

	/* ----------------------------------- *
	 * avoid downloading json again        */

	if (CalculateFileHash(whatsnewPath, whatsnewHash)) {
		char hashString[BLAKE2_HASH_STR_LENGTH];
		HashToString(whatsnewHash, hashString);

		string header = "If-None-Match: ";
		header += hashString;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get current install GUID            */

	string guid = GetProgramGUID();

	if (!guid.empty()) {
		string header = "X-OBS2-GUID: ";
		header += guid;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get json from server                */

	success = GetRemoteFile(WIN_WHATSNEW_URL, text, error, &responseCode,
				nullptr, nullptr, extraHeaders, &signature);

	if (!success || (responseCode != 200 && responseCode != 304)) {
		if (responseCode == 404)
			return;

		throw strprintf("Failed to fetch whatsnew file: %s",
				error.c_str());
	}

	/* ----------------------------------- *
	 * verify file signature               */

	if (responseCode == 200) {
		success = CheckDataSignature(text, "whatsnew", signature.data(),
					     signature.size());
		if (!success)
			throw string("Invalid whatsnew signature");
	}

	/* ----------------------------------- *
	 * write or load json                  */

	if (responseCode == 200) {
		if (!QuickWriteFile(whatsnewPath, text.data(), text.size()))
			throw strprintf("Could not write file '%s'",
					whatsnewPath.Get());
	} else {
		if (!QuickReadFile(whatsnewPath, text))
			throw strprintf("Could not read file '%s'",
					whatsnewPath.Get());
	}

	/* ----------------------------------- *
	 * success                             */

	emit Result(QString::fromUtf8(text.c_str()));

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
}

/* ------------------------------------------------------------------------ */

void WhatsNewBrowserInitThread::run()
{
#ifdef BROWSER_AVAILABLE
	cef->wait_for_browser_init();
#endif
	emit Result(url);
}
