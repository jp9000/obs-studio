#include <iconv.h>
#include <errno.h>
#include "find-font.h"

struct mac_font_mapping {
	unsigned short encoding_id;
	unsigned short language_id;
	const char *code_page;
};

#define TT_MAC_LANGID_ANY 0xFFFF

static const struct mac_font_mapping mac_codes[] = {
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ENGLISH, "macintosh"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ICELANDIC, "x-mac-icelandic"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_TURKISH, "x-mac-ce"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_POLISH, "x-mac-ce"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ROMANIAN, "x-mac-romanian"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_CZECH, "x-mac-ce"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_SLOVAK, "x-mac-ce"},
	{TT_MAC_ID_ROMAN, TT_MAC_LANGID_ANY, "macintosh"},
	{TT_MAC_ID_JAPANESE, TT_MAC_LANGID_JAPANESE, "Shift_JIS"},
	{TT_MAC_ID_JAPANESE, TT_MAC_LANGID_ANY, "Shift_JIS"},
	{TT_MAC_ID_KOREAN, TT_MAC_LANGID_KOREAN, "EUC-KR"},
	{TT_MAC_ID_KOREAN, TT_MAC_LANGID_ANY, "EUC-KR"},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_ARABIC, "x-mac-arabic"},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_URDU, "x-mac-farsi"},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_FARSI, "x-mac-farsi"},
	{TT_MAC_ID_ARABIC, TT_MAC_LANGID_ANY, "x-mac-arabic"},
	{TT_MAC_ID_HEBREW, TT_MAC_LANGID_HEBREW, "x-mac-hebrew"},
	{TT_MAC_ID_HEBREW, TT_MAC_LANGID_ANY, "x-mac-hebrew"},
	{TT_MAC_ID_GREEK, TT_MAC_LANGID_ANY, "x-mac-greek"},
	{TT_MAC_ID_RUSSIAN, TT_MAC_LANGID_ANY, "x-mac-cyrillic"},
	{TT_MAC_ID_DEVANAGARI, TT_MAC_LANGID_ANY, "x-mac-devanagari"},
	{TT_MAC_ID_GURMUKHI, TT_MAC_LANGID_ANY, "x-mac-gurmukhi"},
	{TT_MAC_ID_GUJARATI, TT_MAC_LANGID_ANY, "x-mac-gujarati"},
	{TT_MAC_ID_TRADITIONAL_CHINESE, TT_MAC_LANGID_CHINESE_SIMPLIFIED,
	 "Big5"},
	{TT_MAC_ID_TRADITIONAL_CHINESE, TT_MAC_LANGID_ANY, "Big5"},
	{TT_MAC_ID_SIMPLIFIED_CHINESE, TT_MAC_LANGID_CHINESE_SIMPLIFIED,
	 "GB2312"},
	{TT_MAC_ID_SIMPLIFIED_CHINESE, TT_MAC_LANGID_ANY, "GB2312"}};

const char *iso_codes[] = {"us-ascii", NULL, "iso-8859-1"};

const char *ms_codes[] = {"UTF-16BE", "UTF-16BE", "Shift_JIS", NULL,
			  "Big5",     NULL,       NULL,        NULL,
			  NULL,       NULL,       "UTF-16BE"};

static const size_t mac_code_count = sizeof(mac_codes) / sizeof(mac_codes[0]);
static const size_t iso_code_count = sizeof(iso_codes) / sizeof(iso_codes[0]);
static const size_t ms_code_count = sizeof(ms_codes) / sizeof(ms_codes[0]);

static const char *get_mac_code(uint16_t encoding_id, uint16_t language_id)
{
	for (size_t i = 0; i < mac_code_count; i++) {
		const struct mac_font_mapping *mac_code = &mac_codes[i];

		if (mac_code->encoding_id == encoding_id &&
		    mac_code->language_id == language_id)
			return mac_code->code_page;
	}

	return NULL;
}

static const char *get_code_page_for_font(uint16_t platform_id,
					  uint16_t encoding_id,
					  uint16_t language_id)
{
	const char *ret;

	switch (platform_id) {
	case TT_PLATFORM_APPLE_UNICODE:
		return "UTF-16BE";
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

	return NULL;
}

char *sfnt_name_to_utf8(FT_SfntName *sfnt_name)
{
	const char *charset = get_code_page_for_font(sfnt_name->platform_id,
						     sfnt_name->encoding_id,
						     sfnt_name->language_id);
	char utf8[256];
	char *conv_in, *conv_out;
	size_t in_len, out_len;

	if (!charset) {
		blog(LOG_DEBUG,
		     "invalid character set found, "
		     "platform_id: %d, encoding_id: %d, "
		     "language_id: %d",
		     sfnt_name->platform_id, sfnt_name->encoding_id,
		     sfnt_name->language_id);
		return NULL;
	}

	iconv_t ic = iconv_open("UTF-8", charset);
	if (ic == (iconv_t)-1) {
		blog(LOG_DEBUG,
		     "couldn't intialize font code page "
		     "conversion:  '%s' to 'utf-8': errno = %d",
		     charset, (int)errno);
		return NULL;
	}

	conv_in = (char *)sfnt_name->string;
	conv_out = utf8;
	in_len = sfnt_name->string_len;
	out_len = 256;

	size_t n = iconv(ic, &conv_in, &in_len, &conv_out, &out_len);
	if (n == (size_t)-1) {
		blog(LOG_WARNING, "couldn't convert font name text: errno = %d",
		     (int)errno);
		iconv_close(ic);
		return NULL;
	}

	iconv_close(ic);
	*conv_out = 0;
	return bstrdup(utf8);
}
