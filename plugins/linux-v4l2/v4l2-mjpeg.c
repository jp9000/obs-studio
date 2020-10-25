/*
Copyright (C) 2020 by Morten Bøgeskov <source@kosmisk.dk>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <obs-module.h>

#include "v4l2-mjpeg.h"

#define blog(level, msg, ...) \
	blog(level, "v4l2-input: mjpeg: " msg, ##__VA_ARGS__)

int v4l2_init_mjpeg(struct v4l2_mjpeg_decoder *decoder)
{
	decoder->codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	if (!decoder->codec) {
		return -1;
	}

	decoder->context = avcodec_alloc_context3(decoder->codec);
	if (!decoder->context) {
		return -1;
	}

	decoder->packet = av_packet_alloc();
	if (!decoder->packet) {
		return -1;
	}

	decoder->frame = av_frame_alloc();
	if (!decoder->frame) {
		return -1;
	}

	decoder->context->flags2 |= AV_CODEC_FLAG2_FAST;
	decoder->context->pix_fmt = AV_PIX_FMT_YUVJ422P;

	if (avcodec_open2(decoder->context, decoder->codec, NULL) < 0) {
		blog(LOG_ERROR, "failed to open codec");
		return -1;
	}

	blog(LOG_DEBUG, "initialized avcodec");

	return 0;
}

void v4l2_destroy_mjpeg(struct v4l2_mjpeg_decoder *decoder)
{
	blog(LOG_DEBUG, "destroying avcodec");
	if (decoder->frame) {
		av_frame_free(&decoder->frame);
	}

	if (decoder->packet) {
		av_packet_free(&decoder->packet);
	}

	if (decoder->context) {
		avcodec_free_context(&decoder->context);
	}
}

int v4l2_decode_mjpeg(struct obs_source_frame *out, uint8_t *data,
		      size_t length, struct v4l2_mjpeg_decoder *decoder)
{

	decoder->packet->data = data;
	decoder->packet->size = length;
	if (avcodec_send_packet(decoder->context, decoder->packet) < 0) {
		blog(LOG_ERROR, "failed to send jpeg to codec");
		return -1;
	}

	if (avcodec_receive_frame(decoder->context, decoder->frame) < 0) {
		blog(LOG_ERROR, "failed to recieve frame from codec");
		return -1;
	}

	for (uint_fast32_t i = 0; i < MAX_AV_PLANES; ++i) {
		out->data[i] = decoder->frame->data[i];
		out->linesize[i] = decoder->frame->linesize[i];
	}

	return 0;
}
