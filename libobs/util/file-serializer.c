/*
 * Copyright (c) 2015 Hugh Bailey <obs.jim@gmail.com>
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

#include "dstr.h"
#include "file-serializer.h"
#include "platform.h"

static size_t file_input_read(void *file, void *data, size_t size)
{
	return fread(data, 1, size, file);
}

static int64_t file_input_seek(void *file, int64_t offset,
			       enum serialize_seek_type seek_type)
{
	int origin = SEEK_SET;

	switch (seek_type) {
	case SERIALIZE_SEEK_START:
		origin = SEEK_SET;
		break;
	case SERIALIZE_SEEK_CURRENT:
		origin = SEEK_CUR;
		break;
	case SERIALIZE_SEEK_END:
		origin = SEEK_END;
		break;
	}

	if (os_fseeki64(file, offset, origin) == -1)
		return -1;

	return os_ftelli64(file);
}

static int64_t file_input_get_pos(void *file)
{
	return os_ftelli64(file);
}

bool file_input_serializer_init(struct serializer *s, const char *path)
{
	s->data = os_fopen(path, "rb");
	if (!s->data)
		return false;

	s->read = file_input_read;
	s->write = NULL;
	s->seek = file_input_seek;
	s->get_pos = file_input_get_pos;
	return true;
}

void file_input_serializer_free(struct serializer *s)
{
	if (s->data)
		fclose(s->data);
}

/* ------------------------------------------------------------------------- */

struct file_output_data {
	FILE *file;
	char *temp_name;
	char *file_name;
};

static size_t file_output_write(void *sdata, const void *data, size_t size)
{
	struct file_output_data *out = sdata;
	return fwrite(data, 1, size, out->file);
}

static int64_t file_output_seek(void *sdata, int64_t offset,
				enum serialize_seek_type seek_type)
{
	struct file_output_data *out = sdata;
	int origin = SEEK_SET;

	switch (seek_type) {
	case SERIALIZE_SEEK_START:
		origin = SEEK_SET;
		break;
	case SERIALIZE_SEEK_CURRENT:
		origin = SEEK_CUR;
		break;
	case SERIALIZE_SEEK_END:
		origin = SEEK_END;
		break;
	}

	if (os_fseeki64(out->file, offset, origin) == -1)
		return -1;

	return os_ftelli64(out->file);
}

static int64_t file_output_get_pos(void *sdata)
{
	struct file_output_data *out = sdata;
	return os_ftelli64(out->file);
}

bool file_output_serializer_init(struct serializer *s, const char *path)
{
	FILE *file = os_fopen(path, "wb");
	struct file_output_data *out;

	if (!file)
		return false;

	out = bzalloc(sizeof(*out));
	out->file = file;

	s->data = out;
	s->read = NULL;
	s->write = file_output_write;
	s->seek = file_output_seek;
	s->get_pos = file_output_get_pos;
	return true;
}

bool file_output_serializer_init_safe(struct serializer *s, const char *path,
				      const char *temp_ext)
{
	struct dstr temp_name = {0};
	struct file_output_data *out;
	FILE *file;

	if (!temp_ext || !*temp_ext)
		return false;

	dstr_copy(&temp_name, path);
	if (*temp_ext != '.')
		dstr_cat_ch(&temp_name, '.');
	dstr_cat(&temp_name, temp_ext);

	file = os_fopen(temp_name.array, "wb");
	if (!file) {
		dstr_free(&temp_name);
		return false;
	}

	out = bzalloc(sizeof(*out));
	out->file_name = bstrdup(path);
	out->temp_name = temp_name.array;
	out->file = file;

	s->data = out;
	s->read = NULL;
	s->write = file_output_write;
	s->seek = file_output_seek;
	s->get_pos = file_output_get_pos;
	return true;
}

void file_output_serializer_free(struct serializer *s)
{
	struct file_output_data *out = s->data;

	if (out) {
		fclose(out->file);

		if (out->temp_name) {
			os_unlink(out->file_name);
			os_rename(out->temp_name, out->file_name);
		}

		bfree(out->file_name);
		bfree(out->temp_name);
		bfree(out);
	}
}
