#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include <obs-module.h>
#include <media-io/audio-math.h>
#include <util/platform.h>
#include <util/circlebuf.h>
#include <util/threading.h>

/* -------------------------------------------------------- */

#define do_log(level, format, ...)              \
	blog(level, "[expander: '%s'] " format, \
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

#define S_RATIO                         "ratio"
#define S_THRESHOLD                     "threshold"
#define S_ATTACK_TIME                   "attack_time"
#define S_RELEASE_TIME                  "release_time"
#define S_OUTPUT_GAIN                   "output_gain"
#define S_DETECTOR                      "detector"
#define S_PRESETS                       "presets"

#define MT_ obs_module_text
#define TEXT_RATIO                      MT_("expander.Ratio")
#define TEXT_THRESHOLD                  MT_("expander.Threshold")
#define TEXT_ATTACK_TIME                MT_("expander.AttackTime")
#define TEXT_RELEASE_TIME               MT_("expander.ReleaseTime")
#define TEXT_OUTPUT_GAIN                MT_("expander.OutputGain")
#define TEXT_DETECTOR                   MT_("expander.Detector")
#define TEXT_PEAK                       MT_("expander.Peak")
#define TEXT_RMS                        MT_("expander.RMS")
#define TEXT_NONE                       MT_("expander.None")
#define TEXT_PRESETS                    MT_("expander.Presets")
#define TEXT_PRESETS_EXP                MT_("expander.Presets.Expander")
#define TEXT_PRESETS_GATE               MT_("expander.Presets.Gate")

#define MIN_RATIO                       1.0f
#define MAX_RATIO                       20.0f
#define MIN_THRESHOLD_DB                -60.0f
#define MAX_THRESHOLD_DB                0.0f
#define MIN_OUTPUT_GAIN_DB              -32.0f
#define MAX_OUTPUT_GAIN_DB              32.0f
#define MIN_ATK_RLS_MS                  1
#define MAX_RLS_MS                      1000
#define MAX_ATK_MS                      100
#define DEFAULT_AUDIO_BUF_MS            10

#define MS_IN_S                         1000
#define MS_IN_S_F                       ((float)MS_IN_S)

/* clang-format on */

/* -------------------------------------------------------- */

struct expander_data {
	obs_source_t *context;
	float *envelope_buf[MAX_AUDIO_CHANNELS];
	size_t envelope_buf_len;

	float ratio;
	float threshold;
	float attack_gain;
	float release_gain;
	float output_gain;

	size_t num_channels;
	size_t sample_rate;
	float envelope[MAX_AUDIO_CHANNELS];
	float slope;
	int detector;
	float runave[MAX_AUDIO_CHANNELS];
	bool is_gate;
	float *runaverage[MAX_AUDIO_CHANNELS];
	size_t runaverage_len;
	float *gaindB[MAX_AUDIO_CHANNELS];
	size_t gaindB_len;
	float gaindB_buf[MAX_AUDIO_CHANNELS];
	float *env_in;
	size_t env_in_len;
};

enum { RMS_DETECT,
       RMS_STILLWELL_DETECT,
       PEAK_DETECT,
       NO_DETECT,
};
/* -------------------------------------------------------- */

static void resize_env_buffer(struct expander_data *cd, size_t len)
{
	cd->envelope_buf_len = len;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
		cd->envelope_buf[i] =
			brealloc(cd->envelope_buf[i],
				 cd->envelope_buf_len * sizeof(float));
}

static void resize_runaverage_buffer(struct expander_data *cd, size_t len)
{
	cd->runaverage_len = len;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
		cd->runaverage[i] = brealloc(
			cd->runaverage[i], cd->runaverage_len * sizeof(float));
}

static void resize_env_in_buffer(struct expander_data *cd, size_t len)
{
	cd->env_in_len = len;
	cd->env_in = brealloc(cd->env_in, cd->env_in_len * sizeof(float));
}

static void resize_gaindB_buffer(struct expander_data *cd, size_t len)
{
	cd->gaindB_len = len;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
		cd->gaindB[i] =
			brealloc(cd->gaindB[i], cd->gaindB_len * sizeof(float));
}

static inline float gain_coefficient(uint32_t sample_rate, float time)
{
	return expf(-1.0f / (sample_rate * time));
}

static const char *expander_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Expander");
}

static void expander_defaults(obs_data_t *s)
{
	const char *presets = obs_data_get_string(s, S_PRESETS);
	bool is_expander_preset = true;
	if (strcmp(presets, "gate") == 0)
		is_expander_preset = false;
	obs_data_set_default_string(s, S_PRESETS,
				    is_expander_preset ? "expander" : "gate");
	obs_data_set_default_double(s, S_RATIO,
				    is_expander_preset ? 2.0 : 10.0);
	obs_data_set_default_double(s, S_THRESHOLD, -40.0f);
	obs_data_set_default_int(s, S_ATTACK_TIME, 10);
	obs_data_set_default_int(s, S_RELEASE_TIME,
				 is_expander_preset ? 50 : 125);
	obs_data_set_default_double(s, S_OUTPUT_GAIN, 0.0);
	obs_data_set_default_string(s, S_DETECTOR, "RMS");
}

static void expander_update(void *data, obs_data_t *s)
{
	struct expander_data *cd = data;
	const char *presets = obs_data_get_string(s, S_PRESETS);
	if (strcmp(presets, "expander") == 0 && cd->is_gate) {
		obs_data_clear(s);
		obs_data_set_string(s, S_PRESETS, "expander");
		expander_defaults(s);
		cd->is_gate = false;
	}
	if (strcmp(presets, "gate") == 0 && !cd->is_gate) {
		obs_data_clear(s);
		obs_data_set_string(s, S_PRESETS, "gate");
		expander_defaults(s);
		cd->is_gate = true;
	}

	const uint32_t sample_rate =
		audio_output_get_sample_rate(obs_get_audio());
	const size_t num_channels = audio_output_get_channels(obs_get_audio());
	const float attack_time_ms = (float)obs_data_get_int(s, S_ATTACK_TIME);
	const float release_time_ms =
		(float)obs_data_get_int(s, S_RELEASE_TIME);
	const float output_gain_db =
		(float)obs_data_get_double(s, S_OUTPUT_GAIN);

	cd->ratio = (float)obs_data_get_double(s, S_RATIO);

	cd->threshold = (float)obs_data_get_double(s, S_THRESHOLD);
	cd->attack_gain =
		gain_coefficient(sample_rate, attack_time_ms / MS_IN_S_F);
	cd->release_gain =
		gain_coefficient(sample_rate, release_time_ms / MS_IN_S_F);
	cd->output_gain = db_to_mul(output_gain_db);
	cd->num_channels = num_channels;
	cd->sample_rate = sample_rate;
	cd->slope = 1.0f - cd->ratio;

	const char *detect_mode = obs_data_get_string(s, S_DETECTOR);
	if (strcmp(detect_mode, "RMS") == 0)
		cd->detector = RMS_DETECT;
	if (strcmp(detect_mode, "peak") == 0)
		cd->detector = PEAK_DETECT;

	size_t sample_len = sample_rate * DEFAULT_AUDIO_BUF_MS / MS_IN_S;
	if (cd->envelope_buf_len == 0)
		resize_env_buffer(cd, sample_len);
	if (cd->runaverage_len == 0)
		resize_runaverage_buffer(cd, sample_len);
	if (cd->env_in_len == 0)
		resize_env_in_buffer(cd, sample_len);
	if (cd->gaindB_len == 0)
		resize_gaindB_buffer(cd, sample_len);
}

static void *expander_create(obs_data_t *settings, obs_source_t *filter)
{
	struct expander_data *cd = bzalloc(sizeof(struct expander_data));
	cd->context = filter;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		cd->runave[i] = 0;
		cd->envelope[i] = 0;
		cd->gaindB_buf[i] = 0;
	}
	cd->is_gate = false;
	const char *presets = obs_data_get_string(settings, S_PRESETS);
	if (strcmp(presets, "gate") == 0)
		cd->is_gate = true;

	expander_update(cd, settings);
	return cd;
}

static void expander_destroy(void *data)
{
	struct expander_data *cd = data;

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		bfree(cd->envelope_buf[i]);
		bfree(cd->runaverage[i]);
		bfree(cd->gaindB[i]);
	}
	bfree(cd->env_in);
	bfree(cd);
}

// detection stage
static void analyze_envelope(struct expander_data *cd, float **samples,
			     const uint32_t num_samples)
{
	if (cd->envelope_buf_len < num_samples)
		resize_env_buffer(cd, num_samples);
	if (cd->runaverage_len < num_samples)
		resize_runaverage_buffer(cd, num_samples);
	if (cd->env_in_len < num_samples)
		resize_env_in_buffer(cd, num_samples);

	// 10 ms RMS window
	const float rmscoef = exp2f(-100.0f / cd->sample_rate);

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		memset(cd->envelope_buf[i], 0,
		       num_samples * sizeof(cd->envelope_buf[i][0]));
		memset(cd->runaverage[i], 0,
		       num_samples * sizeof(cd->runaverage[i][0]));
	}
	memset(cd->env_in, 0, num_samples * sizeof(cd->env_in[0]));

	for (size_t chan = 0; chan < cd->num_channels; ++chan) {
		if (!samples[chan])
			continue;

		float *envelope_buf = cd->envelope_buf[chan];
		float *runave = cd->runaverage[chan];
		float *env_in = cd->env_in;

		if (cd->detector == RMS_DETECT) {
			runave[0] =
				rmscoef * cd->runave[chan] +
				(1 - rmscoef) * powf(samples[chan][0], 2.0f);
			env_in[0] = sqrtf(fmaxf(runave[0], 0));
			for (uint32_t i = 1; i < num_samples; ++i) {
				runave[i] =
					rmscoef * runave[i - 1] +
					(1 - rmscoef) *
						powf(samples[chan][i], 2.0f);
				env_in[i] = sqrtf(runave[i]);
			}
		} else if (cd->detector == PEAK_DETECT) {
			for (uint32_t i = 0; i < num_samples; ++i) {
				runave[i] = powf(samples[chan][i], 2);
				env_in[i] = fabsf(samples[chan][i]);
			}
		}

		cd->runave[chan] = runave[num_samples - 1];
		for (uint32_t i = 0; i < num_samples; ++i)
			envelope_buf[i] = fmaxf(envelope_buf[i], env_in[i]);
		cd->envelope[chan] = cd->envelope_buf[chan][num_samples - 1];
	}
}

// gain stage and ballistics in dB domain
static inline void process_expansion(struct expander_data *cd, float **samples,
				     uint32_t num_samples)
{
	const float attack_gain = cd->attack_gain;
	const float release_gain = cd->release_gain;

	if (cd->gaindB_len < num_samples)
		resize_gaindB_buffer(cd, num_samples);
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++)
		memset(cd->gaindB[i], 0,
		       num_samples * sizeof(cd->gaindB[i][0]));

	for (size_t chan = 0; chan < cd->num_channels; chan++) {
		for (size_t i = 0; i < num_samples; ++i) {
			// gain stage of expansion
			float env_db = mul_to_db(cd->envelope_buf[chan][i]);
			float gain =
				cd->threshold - env_db > 0.0f
					? fmaxf(cd->slope * (cd->threshold -
							     env_db),
						-60.0f)
					: 0.0f;
			// ballistics (attack/release)
			if (i > 0) {
				if (gain > cd->gaindB[chan][i - 1])
					cd->gaindB[chan][i] =
						attack_gain *
							cd->gaindB[chan][i - 1] +
						(1.0f - attack_gain) * gain;
				else
					cd->gaindB[chan][i] =
						release_gain *
							cd->gaindB[chan][i - 1] +
						(1.0f - release_gain) * gain;
			} else {
				if (gain > cd->gaindB_buf[chan])
					cd->gaindB[chan][i] =
						attack_gain *
							cd->gaindB_buf[chan] +
						(1.0f - attack_gain) * gain;
				else
					cd->gaindB[chan][i] =
						release_gain *
							cd->gaindB_buf[chan] +
						(1.0f - release_gain) * gain;
			}

			gain = db_to_mul(fminf(0, cd->gaindB[chan][i]));
			if (samples[chan])
				samples[chan][i] *= gain * cd->output_gain;
		}
		cd->gaindB_buf[chan] = cd->gaindB[chan][num_samples - 1];
	}
}

static struct obs_audio_data *
expander_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct expander_data *cd = data;

	const uint32_t num_samples = audio->frames;
	if (num_samples == 0)
		return audio;

	float **samples = (float **)audio->data;

	analyze_envelope(cd, samples, num_samples);
	process_expansion(cd, samples, num_samples);
	return audio;
}

static bool presets_changed(obs_properties_t *props, obs_property_t *prop,
			    obs_data_t *settings)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);
	UNUSED_PARAMETER(settings);
	return true;
}

static obs_properties_t *expander_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_property_t *presets = obs_properties_add_list(
		props, S_PRESETS, TEXT_PRESETS, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(presets, TEXT_PRESETS_EXP, "expander");
	obs_property_list_add_string(presets, TEXT_PRESETS_GATE, "gate");
	obs_property_set_modified_callback(presets, presets_changed);
	p = obs_properties_add_float_slider(props, S_RATIO, TEXT_RATIO,
					    MIN_RATIO, MAX_RATIO, 0.1);
	obs_property_float_set_suffix(p, ":1");
	p = obs_properties_add_float_slider(props, S_THRESHOLD, TEXT_THRESHOLD,
					    MIN_THRESHOLD_DB, MAX_THRESHOLD_DB,
					    0.1);
	obs_property_float_set_suffix(p, " dB");
	p = obs_properties_add_int_slider(props, S_ATTACK_TIME,
					  TEXT_ATTACK_TIME, MIN_ATK_RLS_MS,
					  MAX_ATK_MS, 1);
	obs_property_int_set_suffix(p, " ms");
	p = obs_properties_add_int_slider(props, S_RELEASE_TIME,
					  TEXT_RELEASE_TIME, MIN_ATK_RLS_MS,
					  MAX_RLS_MS, 1);
	obs_property_int_set_suffix(p, " ms");
	p = obs_properties_add_float_slider(props, S_OUTPUT_GAIN,
					    TEXT_OUTPUT_GAIN,
					    MIN_OUTPUT_GAIN_DB,
					    MAX_OUTPUT_GAIN_DB, 0.1);
	obs_property_float_set_suffix(p, " dB");
	obs_property_t *detect = obs_properties_add_list(
		props, S_DETECTOR, TEXT_DETECTOR, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(detect, TEXT_RMS, "RMS");
	obs_property_list_add_string(detect, TEXT_PEAK, "peak");

	UNUSED_PARAMETER(data);
	return props;
}

struct obs_source_info expander_filter = {
	.id = "expander_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = expander_name,
	.create = expander_create,
	.destroy = expander_destroy,
	.update = expander_update,
	.filter_audio = expander_filter_audio,
	.get_defaults = expander_defaults,
	.get_properties = expander_properties,
};
