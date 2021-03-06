#include <util/dstr.h>
#include <util/darray.h>
#include <util/crc32.h>
#include "find-font.h"
#include "text-freetype2.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

extern DARRAY(struct font_path_info) font_list;
extern void save_font_list(void);

struct mac_font_mapping {
	unsigned short encoding_id;
	unsigned short language_id;
	unsigned int code_page;
};

#define TT_MAC_LANGID_ANY 0xFFFF

static const struct mac_font_mapping mac_codes[] = {
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ENGLISH, 10000},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ICELANDIC, 10079},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_TURKISH, 10081},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_POLISH, 10029},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ROMANIAN, 10010},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_CZECH, 10029},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_SLOVAK, 10029},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ANY, 10000},
	{TT_MAC_ID_JAPANESE, TT_MAC_LANGID_JAPANESE, 932},
	{TT_MAC_ID_JAPANESE, TT_MAC_LANGID_ANY, 932},
	{TT_MAC_ID_TRADITIONAL_CHINESE, TT_MAC_LANGID_CHINESE_SIMPLIFIED, 950},
	{TT_MAC_ID_TRADITIONAL_CHINESE, TT_MAC_LANGID_ANY, 950},
	{TT_MAC_ID_KOREAN, TT_MAC_LANGID_KOREAN, 51949},
	{TT_MAC_ID_KOREAN, TT_MAC_LANGID_ANY, 51949},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_ARABIC, 10004},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_URDU, 0},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_FARSI, 0},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_ANY, 10004},
	{TT_MAC_ID_HEBREW, TT_MAC_LANGID_HEBREW, 10005},
	{TT_MAC_ID_HEBREW, TT_MAC_LANGID_ANY, 10005},
	{TT_MAC_ID_GREEK, TT_MAC_LANGID_ANY, 10006},
	{TT_MAC_ID_RUSSIAN, TT_MAC_LANGID_ANY, 10007},
	{TT_MAC_ID_DEVANAGARI, TT_MAC_LANGID_ANY, 0},
	{TT_MAC_ID_GURMUKHI, TT_MAC_LANGID_ANY, 0},
	{TT_MAC_ID_GUJARATI, TT_MAC_LANGID_ANY, 0},
	{TT_MAC_ID_SIMPLIFIED_CHINESE, TT_MAC_LANGID_CHINESE_SIMPLIFIED, 936},
	{TT_MAC_ID_SIMPLIFIED_CHINESE, TT_MAC_LANGID_ANY, 936}};

unsigned int iso_codes[] = {20127, 0, 28591};

unsigned int ms_codes[] = {1201, 1201, 932, 0, 950, 0, 0, 0, 0, 0, 1201};

static const size_t mac_code_count = sizeof(mac_codes) / sizeof(mac_codes[0]);
static const size_t iso_code_count = sizeof(iso_codes) / sizeof(iso_codes[0]);
static const size_t ms_code_count = sizeof(ms_codes) / sizeof(ms_codes[0]);

static unsigned int get_mac_code(uint16_t encoding_id, uint16_t language_id)
{
	for (size_t i = 0; i < mac_code_count; i++) {
		const struct mac_font_mapping *mac_code = &mac_codes[i];

		if (mac_code->encoding_id == encoding_id &&
		    mac_code->language_id == language_id)
			return mac_code->code_page;
	}

	return 0;
}

static unsigned int get_code_page_for_font(uint16_t platform_id,
					   uint16_t encoding_id,
					   uint16_t language_id)
{
	unsigned int ret;

	switch (platform_id) {
	case TT_PLATFORM_APPLE_UNICODE:
		return 1201;
	case TT_PLATFORM_MACINTOSH:
		ret = get_mac_code(encoding_id, language_id);
		if (!ret)
			ret = get_mac_code(encoding_id, TT_MAC_LANGID_ANY);
		return ret;
	case TT_PLATFORM_ISO:
		if (encoding_id < iso_code_count)
			return iso_codes[encoding_id];
		break;
	case TT_PLATFORM_MICROSOFT:
		if (encoding_id < ms_code_count)
			return ms_codes[encoding_id];
		break;
	}

	return 0;
}

static char *wide_to_utf8(const wchar_t *str, size_t len)
{
	size_t utf8_len;
	char *utf8_str = NULL;

	utf8_len = (size_t)WideCharToMultiByte(CP_UTF8, 0, str, (int)len, NULL,
					       0, NULL, false);
	if (utf8_len) {
		utf8_str = bzalloc(utf8_len + 1);
		utf8_len = (size_t)WideCharToMultiByte(CP_UTF8, 0, str,
						       (int)len, utf8_str,
						       (int)utf8_len + 1, NULL,
						       false);

		if (!utf8_len) {
			bfree(utf8_str);
			utf8_str = NULL;
		}
	}

	return utf8_str;
}

static char *convert_utf16_be_to_utf8(FT_SfntName *sfnt_name)
{
	size_t utf16_len = sfnt_name->string_len / 2;
	wchar_t *utf16_str = malloc((utf16_len + 1) * sizeof(wchar_t));
	char *utf8_str = NULL;

	utf16_str[utf16_len] = 0;

	/* convert to little endian */
	for (size_t i = 0; i < utf16_len; i++) {
		size_t pos = i * 2;
		wchar_t ch = *(wchar_t *)&sfnt_name->string[pos];

		utf16_str[i] = ((ch >> 8) & 0xFF) | ((ch << 8) & 0xFF00);
	}

	utf8_str = wide_to_utf8(utf16_str, utf16_len);

	free(utf16_str);
	return utf8_str;
}

char *sfnt_name_to_utf8(FT_SfntName *sfnt_name)
{
	unsigned int code_page = get_code_page_for_font(sfnt_name->platform_id,
							sfnt_name->encoding_id,
							sfnt_name->language_id);

	char *utf8_str = NULL;
	wchar_t *utf16_str;
	size_t utf16_len;

	if (code_page == 1201)
		return convert_utf16_be_to_utf8(sfnt_name);
	else if (code_page == 0)
		return NULL;

	utf16_len = MultiByteToWideChar(code_page, 0, (char *)sfnt_name->string,
					sfnt_name->string_len, NULL, 0);
	if (utf16_len) {
		utf16_str = malloc((utf16_len + 1) * sizeof(wchar_t));
		utf16_len = MultiByteToWideChar(code_page, 0,
						(char *)sfnt_name->string,
						sfnt_name->string_len,
						utf16_str, (int)utf16_len);

		if (utf16_len) {
			utf16_str[utf16_len] = 0;
			utf8_str = wide_to_utf8(utf16_str, utf16_len);
		}

		free(utf16_str);
	}

	return utf8_str;
}

uint32_t get_font_checksum(void)
{
	uint32_t checksum = 0;
	struct dstr path = {0};
	HANDLE handle;
	WIN32_FIND_DATAA wfd;

	dstr_reserve(&path, MAX_PATH);

	HRESULT res = SHGetFolderPathA(NULL, CSIDL_FONTS, NULL,
				       SHGFP_TYPE_CURRENT, path.array);
	if (res != S_OK) {
		blog(LOG_WARNING, "Error finding windows font folder");
		return 0;
	}

	path.len = strlen(path.array);
	dstr_cat(&path, "\\*.*");

	handle = FindFirstFileA(path.array, &wfd);
	if (handle == INVALID_HANDLE_VALUE)
		goto free_string;

	dstr_resize(&path, path.len - 4);

	do {
		checksum = calc_crc32(checksum, &wfd.ftLastWriteTime,
				      sizeof(FILETIME));
		checksum = calc_crc32(checksum, wfd.cFileName,
				      strlen(wfd.cFileName));
	} while (FindNextFileA(handle, &wfd));

	FindClose(handle);

free_string:
	dstr_free(&path);
	return checksum;
}

void load_os_font_list(void)
{
	struct dstr path = {0};
	HANDLE handle;
	WIN32_FIND_DATAA wfd;

	dstr_reserve(&path, MAX_PATH);

	HRESULT res = SHGetFolderPathA(NULL, CSIDL_FONTS, NULL,
				       SHGFP_TYPE_CURRENT, path.array);
	if (res != S_OK) {
		blog(LOG_WARNING, "Error finding windows font folder");
		return;
	}

	path.len = strlen(path.array);
	dstr_cat(&path, "\\*.*");

	handle = FindFirstFileA(path.array, &wfd);
	if (handle == INVALID_HANDLE_VALUE)
		goto free_string;

	dstr_resize(&path, path.len - 4);

	do {
		struct dstr full_path = {0};
		FT_Face face;
		FT_Long idx = 0;
		FT_Long max_faces = 1;

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		dstr_copy_dstr(&full_path, &path);
		dstr_cat(&full_path, "\\");
		dstr_cat(&full_path, wfd.cFileName);

		while (idx < max_faces) {
			FT_Error ret = FT_New_Face(ft2_lib, full_path.array,
						   idx, &face);
			if (ret != 0)
				break;

			build_font_path_info(face, idx++, full_path.array);
			max_faces = face->num_faces;
			FT_Done_Face(face);
		}

		dstr_free(&full_path);
	} while (FindNextFileA(handle, &wfd));

	FindClose(handle);

	save_font_list();

free_string:
	dstr_free(&path);
}
