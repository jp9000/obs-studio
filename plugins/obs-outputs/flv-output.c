/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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
******************************************************************************/

#include <stdio.h>
#include <obs-module.h>
#include <obs-avc.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <inttypes.h>
#include "flv-mux.h"

#define do_log(level, format, ...)                \
	blog(level, "[flv output: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

struct flv_output {
	obs_output_t *output;
	struct dstr path;
	FILE *file;
	volatile bool active;
	volatile bool stopping;
	uint64_t stop_ts;
	bool sent_headers;
	int64_t last_packet_ts;

	pthread_mutex_t mutex;

	bool got_first_video;
	int32_t start_dts_offset;
};

static inline bool stopping(struct flv_output *stream)
{
	return os_atomic_load_bool(&stream->stopping);
}

static inline bool active(struct flv_output *stream)
{
	return os_atomic_load_bool(&stream->active);
}

static const char *flv_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FLVOutput");
}

static void flv_output_stop(void *data, uint64_t ts);

static void flv_output_destroy(void *data)
{
	struct flv_output *stream = data;

	pthread_mutex_destroy(&stream->mutex);
	dstr_free(&stream->path);
	bfree(stream);
}

static void *flv_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct flv_output *stream = bzalloc(sizeof(struct flv_output));
	stream->output = output;
	pthread_mutex_init(&stream->mutex, NULL);

	UNUSED_PARAMETER(settings);
	return stream;
}

static int write_packet(struct flv_output *stream,
			struct encoder_packet *packet, bool is_header)
{
	uint8_t *data;
	size_t size;
	int ret = 0;

	stream->last_packet_ts = get_ms_time(packet, packet->dts);

	flv_packet_mux(packet, is_header ? 0 : stream->start_dts_offset, &data,
		       &size, is_header);
	fwrite(data, 1, size, stream->file);
	bfree(data);

	return ret;
}

static void write_meta_data(struct flv_output *stream)
{
	uint8_t *meta_data;
	size_t meta_data_size;

	flv_meta_data(stream->output, &meta_data, &meta_data_size, true);
	fwrite(meta_data, 1, meta_data_size, stream->file);
	bfree(meta_data);
}

static void write_audio_header(struct flv_output *stream)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *aencoder = obs_output_get_audio_encoder(context, 0);

	struct encoder_packet packet = {.type = OBS_ENCODER_AUDIO,
					.timebase_den = 1};

	obs_encoder_get_extra_data(aencoder, &packet.data, &packet.size);
	write_packet(stream, &packet, true);
}

static void write_video_header(struct flv_output *stream)
{
	obs_output_t *context = stream->output;
	obs_encoder_t *vencoder = obs_output_get_video_encoder(context);
	uint8_t *header;
	size_t size;

	struct encoder_packet packet = {
		.type = OBS_ENCODER_VIDEO, .timebase_den = 1, .keyframe = true};

	obs_encoder_get_extra_data(vencoder, &header, &size);
	packet.size = obs_parse_avc_header(&packet.data, header, size);
	write_packet(stream, &packet, true);
	bfree(packet.data);
}

static void write_headers(struct flv_output *stream)
{
	write_meta_data(stream);
	write_video_header(stream);
	write_audio_header(stream);
}

static bool flv_output_start(void *data)
{
	struct flv_output *stream = data;
	obs_data_t *settings;
	const char *path;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	stream->got_first_video = false;
	stream->sent_headers = false;
	os_atomic_set_bool(&stream->stopping, false);

	/* get path */
	settings = obs_output_get_settings(stream->output);
	path = obs_data_get_string(settings, "path");
	dstr_copy(&stream->path, path);
	obs_data_release(settings);

	stream->file = os_fopen(stream->path.array, "wb");
	if (!stream->file) {
		warn("Unable to open FLV file '%s'", stream->path.array);
		return false;
	}

	/* write headers and start capture */
	os_atomic_set_bool(&stream->active, true);
	obs_output_begin_data_capture(stream->output, 0);

	info("Writing FLV file '%s'...", stream->path.array);
	return true;
}

static void flv_output_stop(void *data, uint64_t ts)
{
	struct flv_output *stream = data;
	stream->stop_ts = ts / 1000;
	os_atomic_set_bool(&stream->stopping, true);
}

static void flv_output_actual_stop(struct flv_output *stream, int code)
{
	os_atomic_set_bool(&stream->active, false);

	if (stream->file) {
		write_file_info(stream->file, stream->last_packet_ts,
				os_ftelli64(stream->file));

		fclose(stream->file);
	}
	if (code) {
		obs_output_signal_stop(stream->output, code);
	} else {
		obs_output_end_data_capture(stream->output);
	}

	info("FLV file output complete");
}

static void flv_output_data(void *data, struct encoder_packet *packet)
{
	struct flv_output *stream = data;
	struct encoder_packet parsed_packet;

	pthread_mutex_lock(&stream->mutex);

	if (!active(stream))
		goto unlock;

	if (!packet) {
		flv_output_actual_stop(stream, OBS_OUTPUT_ENCODE_ERROR);
		goto unlock;
	}

	if (stopping(stream)) {
		if (packet->sys_dts_usec >= (int64_t)stream->stop_ts) {
			flv_output_actual_stop(stream, 0);
			goto unlock;
		}
	}

	if (!stream->sent_headers) {
		write_headers(stream);
		stream->sent_headers = true;
	}

	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->got_first_video) {
			stream->start_dts_offset =
				get_ms_time(packet, packet->dts);
			stream->got_first_video = true;
		}

		obs_parse_avc_packet(&parsed_packet, packet);
		write_packet(stream, &parsed_packet, false);
		obs_encoder_packet_release(&parsed_packet);
	} else {
		write_packet(stream, packet, false);
	}

unlock:
	pthread_mutex_unlock(&stream->mutex);
}

static obs_properties_t *flv_output_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "path",
				obs_module_text("FLVOutput.FilePath"),
				OBS_TEXT_DEFAULT);
	return props;
}

struct obs_output_info flv_output_info = {
	.id = "flv_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED,
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac",
	.get_name = flv_output_getname,
	.create = flv_output_create,
	.destroy = flv_output_destroy,
	.start = flv_output_start,
	.stop = flv_output_stop,
	.encoded_packet = flv_output_data,
	.get_properties = flv_output_properties,
};
