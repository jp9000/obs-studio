#pragma once

#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

struct ffmpeg_cfg {
	const char *url;
	const char *format_name;
	const char *format_mime_type;
	const char *muxer_settings;
	int gop_size;
	int video_bitrate;
	int audio_bitrate;
	const char *video_encoder;
	int video_encoder_id;
	const char *audio_encoder;
	int audio_encoder_id;
	const char *video_settings;
	const char *audio_settings;
	int audio_mix_count;
	int audio_tracks;
	enum AVPixelFormat format;
	enum AVColorRange color_range;
	enum AVColorPrimaries color_primaries;
	enum AVColorTransferCharacteristic color_trc;
	enum AVColorSpace colorspace;
	int scale_width;
	int scale_height;
	int width;
	int height;
};

struct ffmpeg_audio_info {
	AVStream *stream;
	AVCodecContext *ctx;
};

struct ffmpeg_data {
	AVStream *video;
	AVCodecContext *video_ctx;
	struct ffmpeg_audio_info *audio_infos;
	AVCodec *acodec;
	AVCodec *vcodec;
	AVFormatContext *output;
	struct SwsContext *swscale;

	int64_t total_frames;
	AVFrame *vframe;
	int frame_size;

	uint64_t start_timestamp;

	int64_t total_samples[MAX_AUDIO_MIXES];
	uint32_t audio_samplerate;
	enum audio_format audio_format;
	size_t audio_planes;
	size_t audio_size;
	int num_audio_streams;

	/* audio_tracks is a bitmask storing the indices of the mixes */
	int audio_tracks;
	struct circlebuf excess_frames[MAX_AUDIO_MIXES][MAX_AV_PLANES];
	uint8_t *samples[MAX_AUDIO_MIXES][MAX_AV_PLANES];
	AVFrame *aframe[MAX_AUDIO_MIXES];

	struct ffmpeg_cfg config;

	bool initialized;

	char *last_error;
};

bool ffmpeg_data_init(struct ffmpeg_data *data, struct ffmpeg_cfg *config);
void ffmpeg_data_free(struct ffmpeg_data *data);
