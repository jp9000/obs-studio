#include "captions-handler.hpp"

captions_handler::captions_handler(captions_cb callback,
				   enum audio_format format,
				   uint32_t sample_rate)
	: cb(callback)
{
	if (!reset_resampler(format, sample_rate))
		throw CAPTIONS_ERROR_GENERIC_FAIL;
}

bool captions_handler::reset_resampler(enum audio_format format,
				       uint32_t sample_rate)
try {
	obs_audio_info ai;
	if (!obs_get_audio_info(&ai))
		throw std::string("Failed to get OBS audio info");

	resample_info src = {ai.samples_per_sec, AUDIO_FORMAT_FLOAT_PLANAR,
			     ai.speakers};
	resample_info dst = {sample_rate, format, SPEAKERS_MONO};

	if (!resampler.reset(dst, src))
		throw std::string("Failed to create audio resampler");

	return true;

} catch (std::string text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

void captions_handler::push_audio(const audio_data *audio)
{
	uint8_t *out[MAX_AV_PLANES];
	uint32_t frames;
	uint64_t ts_offset;
	bool success;

	success = audio_resampler_resample(resampler, out, &frames, &ts_offset,
					   (const uint8_t *const *)audio->data,
					   audio->frames);
	if (success)
		pcm_data(out[0], frames);
}
