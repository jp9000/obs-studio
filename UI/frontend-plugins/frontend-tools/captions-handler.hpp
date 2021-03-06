#pragma once

#include <media-io/audio-resampler.h>
#include <obs-module.h>
#include <functional>
#include <string>

class resampler_obj {
	audio_resampler_t *resampler = nullptr;

public:
	inline ~resampler_obj() { audio_resampler_destroy(resampler); }

	inline bool reset(const resample_info &dst, const resample_info &src)
	{
		audio_resampler_destroy(resampler);
		resampler = audio_resampler_create(&dst, &src);
		return !!resampler;
	}

	inline operator audio_resampler_t *() { return resampler; }
};

/* ------------------------------------------------------------------------- */

typedef std::function<void(const std::string &)> captions_cb;

#define captions_error(s) std::string(obs_module_text("Captions.Error."##s))
#define CAPTIONS_ERROR_GENERIC_FAIL captions_error("GenericFail")

/* ------------------------------------------------------------------------- */

class captions_handler {
	captions_cb cb;
	resampler_obj resampler;

protected:
	inline void callback(const std::string &text) { cb(text); }

	virtual void pcm_data(const void *data, size_t frames) = 0;

	/* always resamples to 1 channel */
	bool reset_resampler(enum audio_format format, uint32_t sample_rate);

public:
	/* throw std::string for errors shown to users */
	captions_handler(captions_cb callback, enum audio_format format,
			 uint32_t sample_rate);
	virtual ~captions_handler() {}

	void push_audio(const audio_data *audio);
};

/* ------------------------------------------------------------------------- */

struct captions_handler_info {
	std::string (*name)(void);
	captions_handler *(*create)(captions_cb cb, const std::string &lang);
};
