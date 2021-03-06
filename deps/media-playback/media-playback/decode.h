/*
 * Copyright (c) 2017 Hugh Bailey <obs.jim@gmail.com>
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

#ifdef __cplusplus
extern "C" {
#endif

#include <util/circlebuf.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <util/threading.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 58
#define CODEC_CAP_TRUNC AV_CODEC_CAP_TRUNCATED
#define CODEC_FLAG_TRUNC AV_CODEC_FLAG_TRUNCATED
#else
#define CODEC_CAP_TRUNC CODEC_CAP_TRUNCATED
#define CODEC_FLAG_TRUNC CODEC_FLAG_TRUNCATED
#endif

struct mp_media;

struct mp_decode {
	struct mp_media *m;
	AVStream *stream;
	bool audio;

	AVCodecContext *decoder;
	AVBufferRef *hw_ctx;
	AVCodec *codec;

	int64_t last_duration;
	int64_t frame_pts;
	int64_t next_pts;
	AVFrame *in_frame;
	AVFrame *sw_frame;
	AVFrame *hw_frame;
	AVFrame *frame;
	enum AVPixelFormat hw_format;
	bool got_first_keyframe;
	bool frame_ready;
	bool eof;
	bool hw;

	AVPacket orig_pkt;
	AVPacket pkt;
	bool packet_pending;
	struct circlebuf packets;
};

extern bool mp_decode_init(struct mp_media *media, enum AVMediaType type,
			   bool hw);
extern void mp_decode_free(struct mp_decode *decode);

extern void mp_decode_clear_packets(struct mp_decode *decode);

extern void mp_decode_push_packet(struct mp_decode *decode, AVPacket *pkt);
extern bool mp_decode_next(struct mp_decode *decode);
extern void mp_decode_flush(struct mp_decode *decode);

#ifdef __cplusplus
}
#endif
