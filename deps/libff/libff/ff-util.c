/*
 * Copyright (c) 2015 John R. Bradley <jrb@turrettech.com>
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

#include "ff-util.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <stdbool.h>

struct ff_format_desc {
	const char *name;
	const char *long_name;
	const char *mime_type;
	const char *extensions;
	enum AVCodecID audio_codec;
	enum AVCodecID video_codec;
	const struct AVCodecTag *const *codec_tags;
	const struct ff_format_desc *next;
};

struct ff_codec_desc {
	const char *name;
	const char *long_name;
	int id;
	bool alias;
	const char *base_name;
	enum ff_codec_type type;
	const struct ff_codec_desc *next;
};

void ff_init()
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
	//avdevice_register_all();
	avcodec_register_all();
#endif
	avformat_network_init();
}

const char *ff_codec_name_from_id(int codec_id)
{
	AVCodec *codec = avcodec_find_encoder(codec_id);
	if (codec != NULL)
		return codec->name;
	else
		return NULL;
}

static bool get_codecs(const AVCodecDescriptor ***descs, unsigned int *size)
{
	const AVCodecDescriptor *desc = NULL;
	const AVCodecDescriptor **codecs;
	unsigned int codec_count = 0;
	unsigned int i = 0;

	while ((desc = avcodec_descriptor_next(desc)) != NULL)
		codec_count++;

	codecs = av_calloc(codec_count, sizeof(AVCodecDescriptor *));

	if (codecs == NULL) {
		av_log(NULL, AV_LOG_ERROR,
		       "unable to allocate sorted codec "
		       "array with size %d",
		       codec_count);
		return false;
	}

	while ((desc = avcodec_descriptor_next(desc)) != NULL)
		codecs[i++] = desc;

	*size = codec_count;
	*descs = codecs;
	return true;
}

static const AVCodec *next_codec_for_id(enum AVCodecID id, const AVCodec *prev)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
	const AVCodec *cur = NULL;
	void *i = 0;
	bool found_prev = false;

	while ((cur = av_codec_iterate(&i)) != NULL) {
		if (cur->id == id && av_codec_is_encoder(cur)) {
			if (!prev) {
				return cur;
			} else if (!found_prev) {
				if (cur == prev) {
					found_prev = true;
				}
			} else {
				return cur;
			}
		}
	}
#else
	while ((prev = av_codec_next(prev)) != NULL) {
		if (prev->id == id && av_codec_is_encoder(prev))
			return prev;
	}
#endif
	return NULL;
}

static void add_codec_to_list(const struct ff_format_desc *format_desc,
                              struct ff_codec_desc **first,
                              struct ff_codec_desc **current, enum AVCodecID id,
                              const AVCodec *codec, bool ignore_compatability)
{
	if (codec == NULL)
		codec = avcodec_find_encoder(id);

	// No codec, or invalid id
	if (codec == NULL)
		return;

	// Not an encoding codec
	if (!av_codec_is_encoder(codec))
		return;

	if (!ignore_compatability) {
		// Format doesn't support this codec
		unsigned int tag =
		        av_codec_get_tag(format_desc->codec_tags, codec->id);
		if (tag == 0)
			return;
	}

	struct ff_codec_desc *d = av_mallocz(sizeof(struct ff_codec_desc));

	d->name = codec->name;
	d->long_name = codec->long_name;
	d->id = codec->id;
	AVCodec *base_codec = avcodec_find_encoder(codec->id);
	if (strcmp(base_codec->name, codec->name) != 0) {
		d->alias = true;
		d->base_name = base_codec->name;
	}

	switch (codec->type) {
	case AVMEDIA_TYPE_AUDIO:
		d->type = FF_CODEC_AUDIO;
		break;
	case AVMEDIA_TYPE_VIDEO:
		d->type = FF_CODEC_VIDEO;
		break;
	default:
		d->type = FF_CODEC_UNKNOWN;
	}

	if (*current != NULL)
		(*current)->next = d;
	else
		*first = d;

	*current = d;
}

static void get_codecs_for_id(const struct ff_format_desc *format_desc,
                              struct ff_codec_desc **first,
                              struct ff_codec_desc **current, enum AVCodecID id,
                              bool ignore_compatability)
{
	const AVCodec *codec = NULL;
	while ((codec = next_codec_for_id(id, codec)) != NULL)
		add_codec_to_list(format_desc, first, current, codec->id, codec,
		                  ignore_compatability);
}

const struct ff_codec_desc *
ff_codec_supported(const struct ff_format_desc *format_desc,
                   bool ignore_compatability)
{
	const AVCodecDescriptor **codecs;
	unsigned int size;
	unsigned int i;
	struct ff_codec_desc *current = NULL;
	struct ff_codec_desc *first = NULL;

	if (!get_codecs(&codecs, &size))
		return NULL;

	for (i = 0; i < size; i++) {
		const AVCodecDescriptor *codec = codecs[i];
		get_codecs_for_id(format_desc, &first, &current, codec->id,
		                  ignore_compatability);
	}

	av_free((void *)codecs);

	return first;
}

const char *ff_codec_desc_name(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->name;
	else
		return NULL;
}

const char *ff_codec_desc_long_name(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->long_name;
	else
		return NULL;
}

bool ff_codec_desc_is_alias(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->alias;
	else
		return false;
}

const char *ff_codec_desc_base_name(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->base_name;
	else
		return NULL;
}

enum ff_codec_type ff_codec_desc_type(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->type;
	else
		return FF_CODEC_UNKNOWN;
}

const struct ff_codec_desc *
ff_codec_desc_next(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->next;
	else
		return NULL;
}

int ff_codec_desc_id(const struct ff_codec_desc *codec_desc)
{
	if (codec_desc != NULL)
		return codec_desc->id;
	else
		return AV_CODEC_ID_NONE;
}

void ff_codec_desc_free(const struct ff_codec_desc *codec_desc)
{
	const struct ff_codec_desc *desc = codec_desc;
	while (desc != NULL) {
		const struct ff_codec_desc *next = desc->next;
		av_free((void *)desc);
		desc = next;
	}
}

static inline bool is_output_device(const AVClass *avclass)
{
	if (!avclass)
		return 0;

	switch (avclass->category) {
	case AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT:
	case AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT:
	case AV_CLASS_CATEGORY_DEVICE_OUTPUT:
		return true;
	default:
		return false;
	}
}

const struct ff_format_desc *ff_format_supported()
{
	const AVOutputFormat *output_format = NULL;
	struct ff_format_desc *desc = NULL;
	struct ff_format_desc *current = NULL;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
	void *i = 0;

	while ((output_format = av_muxer_iterate(&i)) != NULL) {
#else
	while ((output_format = av_oformat_next(output_format)) != NULL) {
#endif
		struct ff_format_desc *d;
		if (is_output_device(output_format->priv_class))
			continue;

		d = av_mallocz(sizeof(struct ff_format_desc));

		d->audio_codec = output_format->audio_codec;
		d->video_codec = output_format->video_codec;
		d->name = output_format->name;
		d->long_name = output_format->long_name;
		d->mime_type = output_format->mime_type;
		d->extensions = output_format->extensions;
		d->codec_tags = output_format->codec_tag;

		if (current != NULL) {
			current->next = d;
			current = d;
		} else {
			desc = current = d;
		}
	}

	return desc;
}

const char *ff_format_desc_name(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->name;
	else
		return NULL;
}

const char *ff_format_desc_long_name(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->long_name;
	else
		return NULL;
}

const char *ff_format_desc_mime_type(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->mime_type;
	else
		return NULL;
}

const char *ff_format_desc_extensions(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->extensions;
	else
		return NULL;
}

bool ff_format_desc_has_audio(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->audio_codec != AV_CODEC_ID_NONE;
	else
		return false;
}

bool ff_format_desc_has_video(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->video_codec != AV_CODEC_ID_NONE;
	else
		return false;
}

int ff_format_desc_audio(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->audio_codec;
	else
		return false;
}

int ff_format_desc_video(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->video_codec;
	else
		return false;
}

const struct ff_format_desc *
ff_format_desc_next(const struct ff_format_desc *format_desc)
{
	if (format_desc != NULL)
		return format_desc->next;
	else
		return NULL;
}

static const char *get_encoder_name(const struct ff_format_desc *format_desc,
                                    enum AVCodecID codec_id)
{
	AVCodec *codec = avcodec_find_encoder(codec_id);
	if (codec == NULL && codec_id == AV_CODEC_ID_NONE)
		return NULL;
	else if (codec == NULL)
		return format_desc->name;
	else
		return codec->name;
}

const char *
ff_format_desc_get_default_name(const struct ff_format_desc *format_desc,
                                enum ff_codec_type codec_type)
{
	switch (codec_type) {
	case FF_CODEC_AUDIO:
		return get_encoder_name(format_desc, format_desc->audio_codec);
	case FF_CODEC_VIDEO:
		return get_encoder_name(format_desc, format_desc->video_codec);
	default:
		return NULL;
	}
}

void ff_format_desc_free(const struct ff_format_desc *format_desc)
{
	const struct ff_format_desc *desc = format_desc;
	while (desc != NULL) {
		const struct ff_format_desc *next = desc->next;
		av_free((void *)desc);
		desc = next;
	}
}
