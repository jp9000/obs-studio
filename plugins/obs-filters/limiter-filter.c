#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include <obs-module.h>
#include <media-io/audio-math.h>
#include <util/platform.h>

/* -------------------------------------------------------- */

#define do_log(level, format, ...)             \
	blog(level, "[limiter: '%s'] " format, \
	     obs_source_get_name(cd->context), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

#ifdef _DEBUG
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* -------------------------------------------------------- */

/* clang-format off */

#define S_THRESHOLD                     "threshold"
#define S_RELEASE_TIME                  "release_time"

#define MT_ obs_module_text
#define TEXT_THRESHOLD                  MT_("Limiter.Threshold")
#define TEXT_RELEASE_TIME               MT_("Limiter.ReleaseTime")

#define MIN_THRESHOLD_DB                -60.0
#define MAX_THRESHOLD_DB                0.0f
#define MIN_ATK_RLS_MS                  1
#define MAX_RLS_MS                      1000
#define DEFAULT_AUDIO_BUF_MS            10
#define ATK_TIME                        0.001f
#define MS_IN_S                         1000
#define MS_IN_S_F                       ((float)MS_IN_S)

/* clang-format on */

/* -------------------------------------------------------- */

struct limiter_data {
	obs_source_t *context;
	float *envelope_buf;
	size_t envelope_buf_len;

	float threshold;
	float attack_gain;
	float release_gain;
	float output_gain;

	size_t num_channels;
	size_t sample_rate;
	float envelope;
	float slope;
};

/* -------------------------------------------------------- */

static void resize_env_buffer(struct limiter_data *cd, size_t len)
{
	cd->envelope_buf_len = len;
	cd->envelope_buf = brealloc(cd->envelope_buf, len * sizeof(float));
}

static inline float gain_coefficient(uint32_t sample_rate, float time)
{
	return (float)exp(-1.0f / (sample_rate * time));
}

static const char *limiter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Limiter");
}

static void limiter_update(void *data, obs_data_t *s)
{
	struct limiter_data *cd = data;

	const uint32_t sample_rate =
		audio_output_get_sample_rate(obs_get_audio());
	const size_t num_channels = audio_output_get_channels(obs_get_audio());
	float attack_time_ms = ATK_TIME;

	const float release_time_ms =
		(float)obs_data_get_int(s, S_RELEASE_TIME);
	const float output_gain_db = 0;

	cd->threshold = (float)obs_data_get_double(s, S_THRESHOLD);

	cd->attack_gain =
		gain_coefficient(sample_rate, attack_time_ms / MS_IN_S_F);
	cd->release_gain =
		gain_coefficient(sample_rate, release_time_ms / MS_IN_S_F);
	cd->output_gain = db_to_mul(output_gain_db);
	cd->num_channels = num_channels;
	cd->sample_rate = sample_rate;
	cd->slope = 1.0f;

	size_t sample_len = sample_rate * DEFAULT_AUDIO_BUF_MS / MS_IN_S;
	if (cd->envelope_buf_len == 0)
		resize_env_buffer(cd, sample_len);
}

static void *limiter_create(obs_data_t *settings, obs_source_t *filter)
{
	struct limiter_data *cd = bzalloc(sizeof(struct limiter_data));
	cd->context = filter;

	limiter_update(cd, settings);
	return cd;
}

static void limiter_destroy(void *data)
{
	struct limiter_data *cd = data;

	bfree(cd->envelope_buf);
	bfree(cd);
}

static void analyze_envelope(struct limiter_data *cd, float **samples,
			     const uint32_t num_samples)
{
	if (cd->envelope_buf_len < num_samples) {
		resize_env_buffer(cd, num_samples);
	}

	const float attack_gain = cd->attack_gain;
	const float release_gain = cd->release_gain;

	memset(cd->envelope_buf, 0, num_samples * sizeof(cd->envelope_buf[0]));
	for (size_t chan = 0; chan < cd->num_channels; ++chan) {
		if (!samples[chan])
			continue;

		float *envelope_buf = cd->envelope_buf;
		float env = cd->envelope;
		for (uint32_t i = 0; i < num_samples; ++i) {
			const float env_in = fabsf(samples[chan][i]);
			if (env < env_in) {
				env = env_in + attack_gain * (env - env_in);
			} else {
				env = env_in + release_gain * (env - env_in);
			}
			envelope_buf[i] = fmaxf(envelope_buf[i], env);
		}
	}
	cd->envelope = cd->envelope_buf[num_samples - 1];
}

static inline void process_compression(const struct limiter_data *cd,
				       float **samples, uint32_t num_samples)
{
	for (size_t i = 0; i < num_samples; ++i) {
		const float env_db = mul_to_db(cd->envelope_buf[i]);
		float gain = cd->slope * (cd->threshold - env_db);
		gain = db_to_mul(fminf(0, gain));

		for (size_t c = 0; c < cd->num_channels; ++c) {
			if (samples[c]) {
				samples[c][i] *= gain * cd->output_gain;
			}
		}
	}
}

static struct obs_audio_data *limiter_filter_audio(void *data,
						   struct obs_audio_data *audio)
{
	struct limiter_data *cd = data;

	const uint32_t num_samples = audio->frames;
	if (num_samples == 0)
		return audio;

	float **samples = (float **)audio->data;
	analyze_envelope(cd, samples, num_samples);
	process_compression(cd, samples, num_samples);
	return audio;
}

static void limiter_defaults(obs_data_t *s)
{
	obs_data_set_default_double(s, S_THRESHOLD, -6.0f);
	obs_data_set_default_int(s, S_RELEASE_TIME, 60);
}

static obs_properties_t *limiter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_float_slider(props, S_THRESHOLD, TEXT_THRESHOLD,
					    MIN_THRESHOLD_DB, MAX_THRESHOLD_DB,
					    0.1);
	obs_property_float_set_suffix(p, " dB");
	p = obs_properties_add_int_slider(props, S_RELEASE_TIME,
					  TEXT_RELEASE_TIME, MIN_ATK_RLS_MS,
					  MAX_RLS_MS, 1);
	obs_property_int_set_suffix(p, " ms");

	UNUSED_PARAMETER(data);
	return props;
}

struct obs_source_info limiter_filter = {
	.id = "limiter_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = limiter_name,
	.create = limiter_create,
	.destroy = limiter_destroy,
	.update = limiter_update,
	.filter_audio = limiter_filter_audio,
	.get_defaults = limiter_defaults,
	.get_properties = limiter_properties,
};
