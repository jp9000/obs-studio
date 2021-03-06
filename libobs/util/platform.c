/*
 * Copyright (c) 2013-2014 Hugh Bailey <obs.jim@gmail.com>
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

#define _FILE_OFFSET_BITS 64

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <locale.h>
#include "c99defs.h"
#include "platform.h"
#include "bmem.h"
#include "utf8.h"
#include "dstr.h"
#include "obs.h"

FILE *os_wfopen(const wchar_t *path, const char *mode)
{
	FILE *file = NULL;

	if (path) {
#ifdef _MSC_VER
		wchar_t *wcs_mode;

		os_utf8_to_wcs_ptr(mode, 0, &wcs_mode);
		file = _wfopen(path, wcs_mode);
		bfree(wcs_mode);
#else
		char *mbs_path;

		os_wcs_to_utf8_ptr(path, 0, &mbs_path);
		file = fopen(mbs_path, mode);
		bfree(mbs_path);
#endif
	}

	return file;
}

FILE *os_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
	wchar_t *wpath = NULL;
	FILE *file = NULL;

	if (path) {
		os_utf8_to_wcs_ptr(path, 0, &wpath);
		file = os_wfopen(wpath, mode);
		bfree(wpath);
	}

	return file;
#else
	return path ? fopen(path, mode) : NULL;
#endif
}

int64_t os_fgetsize(FILE *file)
{
	int64_t cur_offset = os_ftelli64(file);
	int64_t size;
	int errval = 0;

	if (fseek(file, 0, SEEK_END) == -1)
		return -1;

	size = os_ftelli64(file);
	if (size == -1)
		errval = errno;

	if (os_fseeki64(file, cur_offset, SEEK_SET) != 0 && errval != 0)
		errno = errval;

	return size;
}

#ifdef _WIN32
int os_stat(const char *file, struct stat *st)
{
	if (file) {
		wchar_t w_file[512];
		size_t size = os_utf8_to_wcs(file, 0, w_file, sizeof(w_file));
		if (size > 0) {
			struct _stat st_w32;
			int ret = _wstat(w_file, &st_w32);
			if (ret == 0) {
				st->st_dev = st_w32.st_dev;
				st->st_ino = st_w32.st_ino;
				st->st_mode = st_w32.st_mode;
				st->st_nlink = st_w32.st_nlink;
				st->st_uid = st_w32.st_uid;
				st->st_gid = st_w32.st_gid;
				st->st_rdev = st_w32.st_rdev;
				st->st_size = st_w32.st_size;
				st->st_atime = st_w32.st_atime;
				st->st_mtime = st_w32.st_mtime;
				st->st_ctime = st_w32.st_ctime;
			}

			return ret;
		}
	}

	return -1;
}
#endif

int os_fseeki64(FILE *file, int64_t offset, int origin)
{
#ifdef _MSC_VER
	return _fseeki64(file, offset, origin);
#else
	return fseeko(file, offset, origin);
#endif
}

int64_t os_ftelli64(FILE *file)
{
#ifdef _MSC_VER
	return _ftelli64(file);
#else
	return ftello(file);
#endif
}

size_t os_fread_mbs(FILE *file, char **pstr)
{
	size_t size = 0;
	size_t len = 0;

	fseek(file, 0, SEEK_END);
	size = (size_t)os_ftelli64(file);
	*pstr = NULL;

	if (size > 0) {
		char *mbstr = bmalloc(size + 1);

		fseek(file, 0, SEEK_SET);
		size = fread(mbstr, 1, size, file);
		if (size == 0) {
			bfree(mbstr);
			return 0;
		}

		mbstr[size] = 0;
		len = os_mbs_to_utf8_ptr(mbstr, size, pstr);
		bfree(mbstr);
	}

	return len;
}

size_t os_fread_utf8(FILE *file, char **pstr)
{
	size_t size = 0;
	size_t len = 0;

	*pstr = NULL;

	fseek(file, 0, SEEK_END);
	size = (size_t)os_ftelli64(file);

	if (size > 0) {
		char bom[3];
		char *utf8str;
		off_t offset;

		bom[0] = 0;
		bom[1] = 0;
		bom[2] = 0;

		/* remove the ghastly BOM if present */
		fseek(file, 0, SEEK_SET);
		size_t size_read = fread(bom, 1, 3, file);
		(void)size_read;

		offset = (astrcmp_n(bom, "\xEF\xBB\xBF", 3) == 0) ? 3 : 0;

		size -= offset;
		if (size == 0)
			return 0;

		utf8str = bmalloc(size + 1);
		fseek(file, offset, SEEK_SET);

		size = fread(utf8str, 1, size, file);
		if (size == 0) {
			bfree(utf8str);
			return 0;
		}

		utf8str[size] = 0;

		*pstr = utf8str;
	}

	return len;
}

char *os_quick_read_mbs_file(const char *path)
{
	FILE *f = os_fopen(path, "rb");
	char *file_string = NULL;

	if (!f)
		return NULL;

	os_fread_mbs(f, &file_string);
	fclose(f);

	return file_string;
}

char *os_quick_read_utf8_file(const char *path)
{
	FILE *f = os_fopen(path, "rb");
	char *file_string = NULL;

	if (!f)
		return NULL;

	os_fread_utf8(f, &file_string);
	fclose(f);

	return file_string;
}

bool os_quick_write_mbs_file(const char *path, const char *str, size_t len)
{
	FILE *f = os_fopen(path, "wb");
	char *mbs = NULL;
	size_t mbs_len = 0;
	if (!f)
		return false;

	mbs_len = os_utf8_to_mbs_ptr(str, len, &mbs);
	if (mbs_len)
		fwrite(mbs, 1, mbs_len, f);
	bfree(mbs);
	fflush(f);
	fclose(f);

	return true;
}

bool os_quick_write_utf8_file(const char *path, const char *str, size_t len,
			      bool marker)
{
	FILE *f = os_fopen(path, "wb");
	if (!f)
		return false;

	if (marker) {
		if (fwrite("\xEF\xBB\xBF", 3, 1, f) != 1) {
			fclose(f);
			return false;
		}
	}

	if (len) {
		if (fwrite(str, len, 1, f) != 1) {
			fclose(f);
			return false;
		}
	}
	fflush(f);
	fclose(f);

	return true;
}

bool os_quick_write_utf8_file_safe(const char *path, const char *str,
				   size_t len, bool marker,
				   const char *temp_ext, const char *backup_ext)
{
	struct dstr backup_path = {0};
	struct dstr temp_path = {0};
	bool success = false;

	if (!temp_ext || !*temp_ext) {
		blog(LOG_ERROR, "os_quick_write_utf8_file_safe: invalid "
				"temporary extension specified");
		return false;
	}

	dstr_copy(&temp_path, path);
	if (*temp_ext != '.')
		dstr_cat(&temp_path, ".");
	dstr_cat(&temp_path, temp_ext);

	if (!os_quick_write_utf8_file(temp_path.array, str, len, marker)) {
		blog(LOG_ERROR,
		     "os_quick_write_utf8_file_safe: failed to "
		     "write to %s",
		     temp_path.array);
		goto cleanup;
	}

	if (backup_ext && *backup_ext) {
		dstr_copy(&backup_path, path);
		if (*backup_ext != '.')
			dstr_cat(&backup_path, ".");
		dstr_cat(&backup_path, backup_ext);
	}

	if (os_safe_replace(path, temp_path.array, backup_path.array) == 0)
		success = true;

cleanup:
	dstr_free(&backup_path);
	dstr_free(&temp_path);
	return success;
}

int64_t os_get_file_size(const char *path)
{
	FILE *f = os_fopen(path, "rb");
	if (!f)
		return -1;

	int64_t sz = os_fgetsize(f);
	fclose(f);

	return sz;
}

size_t os_mbs_to_wcs(const char *str, size_t len, wchar_t *dst, size_t dst_size)
{
	size_t out_len;

	if (!str)
		return 0;

	out_len = dst ? (dst_size - 1) : mbstowcs(NULL, str, len);

	if (dst) {
		if (!dst_size)
			return 0;

		if (out_len)
			out_len = mbstowcs(dst, str, out_len + 1);

		dst[out_len] = 0;
	}

	return out_len;
}

size_t os_utf8_to_wcs(const char *str, size_t len, wchar_t *dst,
		      size_t dst_size)
{
	size_t in_len;
	size_t out_len;

	if (!str)
		return 0;

	in_len = len ? len : strlen(str);
	out_len = dst ? (dst_size - 1) : utf8_to_wchar(str, in_len, NULL, 0, 0);

	if (dst) {
		if (!dst_size)
			return 0;

		if (out_len)
			out_len =
				utf8_to_wchar(str, in_len, dst, out_len + 1, 0);

		dst[out_len] = 0;
	}

	return out_len;
}

size_t os_wcs_to_mbs(const wchar_t *str, size_t len, char *dst, size_t dst_size)
{
	size_t out_len;

	if (!str)
		return 0;

	out_len = dst ? (dst_size - 1) : wcstombs(NULL, str, 0);

	if (dst) {
		if (!dst_size)
			return 0;

		if (out_len)
			out_len = wcstombs(dst, str, out_len + 1);

		dst[out_len] = 0;
	}

	return out_len;
}

size_t os_wcs_to_utf8(const wchar_t *str, size_t len, char *dst,
		      size_t dst_size)
{
	size_t in_len;
	size_t out_len;

	if (!str)
		return 0;

	in_len = (len != 0) ? len : wcslen(str);
	out_len = dst ? (dst_size - 1) : wchar_to_utf8(str, in_len, NULL, 0, 0);

	if (dst) {
		if (!dst_size)
			return 0;

		if (out_len)
			out_len =
				wchar_to_utf8(str, in_len, dst, out_len + 1, 0);

		dst[out_len] = 0;
	}

	return out_len;
}

size_t os_mbs_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr)
{
	if (str) {
		size_t out_len = os_mbs_to_wcs(str, len, NULL, 0);

		*pstr = bmalloc((out_len + 1) * sizeof(wchar_t));
		return os_mbs_to_wcs(str, len, *pstr, out_len + 1);
	} else {
		*pstr = NULL;
		return 0;
	}
}

size_t os_utf8_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr)
{
	if (str) {
		size_t out_len = os_utf8_to_wcs(str, len, NULL, 0);

		*pstr = bmalloc((out_len + 1) * sizeof(wchar_t));
		return os_utf8_to_wcs(str, len, *pstr, out_len + 1);
	} else {
		*pstr = NULL;
		return 0;
	}
}

size_t os_wcs_to_mbs_ptr(const wchar_t *str, size_t len, char **pstr)
{
	if (str) {
		size_t out_len = os_wcs_to_mbs(str, len, NULL, 0);

		*pstr = bmalloc((out_len + 1) * sizeof(char));
		return os_wcs_to_mbs(str, len, *pstr, out_len + 1);
	} else {
		*pstr = NULL;
		return 0;
	}
}

size_t os_wcs_to_utf8_ptr(const wchar_t *str, size_t len, char **pstr)
{
	if (str) {
		size_t out_len = os_wcs_to_utf8(str, len, NULL, 0);

		*pstr = bmalloc((out_len + 1) * sizeof(char));
		return os_wcs_to_utf8(str, len, *pstr, out_len + 1);
	} else {
		*pstr = NULL;
		return 0;
	}
}

size_t os_utf8_to_mbs_ptr(const char *str, size_t len, char **pstr)
{
	char *dst = NULL;
	size_t out_len = 0;

	if (str) {
		wchar_t *wstr = NULL;
		size_t wlen = os_utf8_to_wcs_ptr(str, len, &wstr);
		out_len = os_wcs_to_mbs_ptr(wstr, wlen, &dst);
		bfree(wstr);
	}
	*pstr = dst;

	return out_len;
}

size_t os_mbs_to_utf8_ptr(const char *str, size_t len, char **pstr)
{
	char *dst = NULL;
	size_t out_len = 0;

	if (str) {
		wchar_t *wstr = NULL;
		size_t wlen = os_mbs_to_wcs_ptr(str, len, &wstr);
		out_len = os_wcs_to_utf8_ptr(wstr, wlen, &dst);
		bfree(wstr);
	}

	*pstr = dst;
	return out_len;
}

/* locale independent double conversion from jansson, credit goes to them */

static inline void to_locale(char *str)
{
	const char *point;
	char *pos;

	point = localeconv()->decimal_point;
	if (*point == '.') {
		/* No conversion needed */
		return;
	}

	pos = strchr(str, '.');
	if (pos)
		*pos = *point;
}

static inline void from_locale(char *buffer)
{
	const char *point;
	char *pos;

	point = localeconv()->decimal_point;
	if (*point == '.') {
		/* No conversion needed */
		return;
	}

	pos = strchr(buffer, *point);
	if (pos)
		*pos = '.';
}

double os_strtod(const char *str)
{
	char buf[64];
	snprintf(buf, 64, "%s", str);
	to_locale(buf);
	return strtod(buf, NULL);
}

int os_dtostr(double value, char *dst, size_t size)
{
	int ret;
	char *start, *end;
	size_t length;

	ret = snprintf(dst, size, "%.17g", value);
	if (ret < 0)
		return -1;

	length = (size_t)ret;
	if (length >= size)
		return -1;

	from_locale(dst);

	/* Make sure there's a dot or 'e' in the output. Otherwise
	   a real is converted to an integer when decoding */
	if (strchr(dst, '.') == NULL && strchr(dst, 'e') == NULL) {
		if (length + 3 >= size) {
			/* No space to append ".0" */
			return -1;
		}
		dst[length] = '.';
		dst[length + 1] = '0';
		dst[length + 2] = '\0';
		length += 2;
	}

	/* Remove leading '+' from positive exponent. Also remove leading
	   zeros from exponents (added by some printf() implementations) */
	start = strchr(dst, 'e');
	if (start) {
		start++;
		end = start + 1;

		if (*start == '-')
			start++;

		while (*end == '0')
			end++;

		if (end != start) {
			memmove(start, end, length - (size_t)(end - dst));
			length -= (size_t)(end - start);
		}
	}

	return (int)length;
}

static int recursive_mkdir(char *path)
{
	char *last_slash;
	int ret;

	ret = os_mkdir(path);
	if (ret != MKDIR_ERROR)
		return ret;

	last_slash = strrchr(path, '/');
	if (!last_slash)
		return MKDIR_ERROR;

	*last_slash = 0;
	ret = recursive_mkdir(path);
	*last_slash = '/';

	if (ret == MKDIR_ERROR)
		return MKDIR_ERROR;

	ret = os_mkdir(path);
	return ret;
}

int os_mkdirs(const char *dir)
{
	struct dstr dir_str;
	int ret;

	dstr_init_copy(&dir_str, dir);
	dstr_replace(&dir_str, "\\", "/");
	ret = recursive_mkdir(dir_str.array);
	dstr_free(&dir_str);
	return ret;
}

const char *os_get_path_extension(const char *path)
{
	struct dstr temp;
	size_t pos = 0;
	char *period;
	char *slash;

	if (!path[0])
		return NULL;

	dstr_init_copy(&temp, path);
	dstr_replace(&temp, "\\", "/");

	slash = strrchr(temp.array, '/');
	period = strrchr(temp.array, '.');
	if (period)
		pos = (size_t)(period - temp.array);

	dstr_free(&temp);

	if (!period || slash > period)
		return NULL;

	return path + pos;
}

static inline bool valid_string(const char *str)
{
	while (str && *str) {
		if (*(str++) != ' ')
			return true;
	}

	return false;
}

static void replace_text(struct dstr *str, size_t pos, size_t len,
			 const char *new_text)
{
	struct dstr front = {0};
	struct dstr back = {0};

	dstr_left(&front, str, pos);
	dstr_right(&back, str, pos + len);
	dstr_copy_dstr(str, &front);
	dstr_cat(str, new_text);
	dstr_cat_dstr(str, &back);
	dstr_free(&front);
	dstr_free(&back);
}

static void erase_ch(struct dstr *str, size_t pos)
{
	struct dstr new_str = {0};
	dstr_left(&new_str, str, pos);
	dstr_cat(&new_str, str->array + pos + 1);
	dstr_free(str);
	*str = new_str;
}

char *os_generate_formatted_filename(const char *extension, bool space,
				     const char *format)
{
	time_t now = time(0);
	struct tm *cur_time;
	cur_time = localtime(&now);
	struct obs_video_info ovi;
	obs_get_video_info(&ovi);

	const size_t spec_count = 23;
	static const char *spec[][2] = {
		{"%CCYY", "%Y"}, {"%YY", "%y"}, {"%MM", "%m"}, {"%DD", "%d"},
		{"%hh", "%H"},   {"%mm", "%M"}, {"%ss", "%S"}, {"%%", "%%"},

		{"%a", ""},      {"%A", ""},    {"%b", ""},    {"%B", ""},
		{"%d", ""},      {"%H", ""},    {"%I", ""},    {"%m", ""},
		{"%M", ""},      {"%p", ""},    {"%S", ""},    {"%y", ""},
		{"%Y", ""},      {"%z", ""},    {"%Z", ""},
	};

	char convert[128] = {0};
	struct dstr sf;
	struct dstr c = {0};
	size_t pos = 0;

	dstr_init_copy(&sf, format);

	while (pos < sf.len) {
		const char *cmp = sf.array + pos;
		for (size_t i = 0; i < spec_count && !convert[0]; i++) {
			size_t len = strlen(spec[i][0]);

			if (astrcmp_n(cmp, spec[i][0], len) == 0) {
				if (strlen(spec[i][1]))
					strftime(convert, sizeof(convert),
						 spec[i][1], cur_time);
				else
					strftime(convert, sizeof(convert),
						 spec[i][0], cur_time);

				dstr_copy(&c, convert);
				if (c.len && valid_string(c.array))
					replace_text(&sf, pos, len, convert);
			}
		}

		if (!convert[0]) {
			if (astrcmp_n(cmp, "%FPS", 4) == 0) {
				if (ovi.fps_den <= 1) {
					sprintf(convert, "%u", ovi.fps_num);
				} else {
					const double obsFPS =
						(double)ovi.fps_num /
						(double)ovi.fps_den;
					sprintf(convert, "%.2f", obsFPS);
				}
				replace_text(&sf, pos, 4, convert);

			} else if (astrcmp_n(cmp, "%CRES", 5) == 0) {
				sprintf(convert, "%ux%u", ovi.base_width,
					ovi.base_height);
				replace_text(&sf, pos, 5, convert);

			} else if (astrcmp_n(cmp, "%ORES", 5) == 0) {
				sprintf(convert, "%ux%u", ovi.output_width,
					ovi.output_height);
				replace_text(&sf, pos, 5, convert);

			} else if (astrcmp_n(cmp, "%VF", 3) == 0) {
				strcpy(convert, get_video_format_name(
							ovi.output_format));
				replace_text(&sf, pos, 3, convert);
			}
		}

		if (convert[0]) {
			pos += strlen(convert);
			convert[0] = 0;
		} else if (!convert[0] && sf.array[pos] == '%') {
			erase_ch(&sf, pos);
		} else {
			pos++;
		}
	}

	if (!space)
		dstr_replace(&sf, " ", "_");

	dstr_cat_ch(&sf, '.');
	dstr_cat(&sf, extension);
	dstr_free(&c);

	if (sf.len > 255)
		dstr_mid(&sf, &sf, 0, 255);

	return sf.array;
}
