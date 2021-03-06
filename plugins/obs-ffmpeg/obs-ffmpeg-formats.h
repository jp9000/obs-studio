#pragma once

static inline int64_t rescale_ts(int64_t val, AVCodecContext *context,
				 AVRational new_base)
{
	return av_rescale_q_rnd(val, context->time_base, new_base,
				AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
}

static inline enum AVPixelFormat
obs_to_ffmpeg_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_I444:
		return AV_PIX_FMT_YUV444P;
	case VIDEO_FORMAT_I420:
		return AV_PIX_FMT_YUV420P;
	case VIDEO_FORMAT_NV12:
		return AV_PIX_FMT_NV12;
	case VIDEO_FORMAT_YUY2:
		return AV_PIX_FMT_YUYV422;
	case VIDEO_FORMAT_UYVY:
		return AV_PIX_FMT_UYVY422;
	case VIDEO_FORMAT_RGBA:
		return AV_PIX_FMT_RGBA;
	case VIDEO_FORMAT_BGRA:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_BGRX:
		return AV_PIX_FMT_BGRA;
	case VIDEO_FORMAT_Y800:
		return AV_PIX_FMT_GRAY8;
	case VIDEO_FORMAT_BGR3:
		return AV_PIX_FMT_BGR24;
	case VIDEO_FORMAT_I422:
		return AV_PIX_FMT_YUV422P;
	case VIDEO_FORMAT_I40A:
		return AV_PIX_FMT_YUVA420P;
	case VIDEO_FORMAT_I42A:
		return AV_PIX_FMT_YUVA422P;
	case VIDEO_FORMAT_YUVA:
		return AV_PIX_FMT_YUVA444P;
	case VIDEO_FORMAT_NONE:
	case VIDEO_FORMAT_YVYU:
	case VIDEO_FORMAT_AYUV:
		/* not supported by FFmpeg */
		return AV_PIX_FMT_NONE;
	}

	return AV_PIX_FMT_NONE;
}

static inline enum video_format
ffmpeg_to_obs_video_format(enum AVPixelFormat format)
{
	switch (format) {
	case AV_PIX_FMT_YUV444P:
		return VIDEO_FORMAT_I444;
	case AV_PIX_FMT_YUV420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case AV_PIX_FMT_YUYV422:
		return VIDEO_FORMAT_YUY2;
	case AV_PIX_FMT_UYVY422:
		return VIDEO_FORMAT_UYVY;
	case AV_PIX_FMT_RGBA:
		return VIDEO_FORMAT_RGBA;
	case AV_PIX_FMT_BGRA:
		return VIDEO_FORMAT_BGRA;
	case AV_PIX_FMT_GRAY8:
		return VIDEO_FORMAT_Y800;
	case AV_PIX_FMT_BGR24:
		return VIDEO_FORMAT_BGR3;
	case AV_PIX_FMT_YUV422P:
		return VIDEO_FORMAT_I422;
	case AV_PIX_FMT_YUVA420P:
		return VIDEO_FORMAT_I40A;
	case AV_PIX_FMT_YUVA422P:
		return VIDEO_FORMAT_I42A;
	case AV_PIX_FMT_YUVA444P:
		return VIDEO_FORMAT_YUVA;
	case AV_PIX_FMT_NONE:
	default:
		return VIDEO_FORMAT_NONE;
	}
}

static inline enum audio_format
convert_ffmpeg_sample_format(enum AVSampleFormat format)
{
	switch ((uint32_t)format) {
	case AV_SAMPLE_FMT_U8:
		return AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_S16:
		return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:
		return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_FLT:
		return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:
		return AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P:
		return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P:
		return AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP:
		return AUDIO_FORMAT_FLOAT_PLANAR;
	}

	/* shouldn't get here */
	return AUDIO_FORMAT_16BIT;
}
