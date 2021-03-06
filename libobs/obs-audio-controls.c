/*
Copyright (C) 2014 by Leonhard Oelke <leonhard@in-verted.de>

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

#include <math.h>

#include "util/sse-intrin.h"

#include "util/threading.h"
#include "util/bmem.h"
#include "media-io/audio-math.h"
#include "obs.h"
#include "obs-internal.h"

#include "obs-audio-controls.h"

/* These are pointless warnings generated not by our code, but by a standard
 * library macro, INFINITY */
#ifdef _MSC_VER
#pragma warning(disable : 4056)
#pragma warning(disable : 4756)
#endif

#define CLAMP(x, min, max) ((x) < min ? min : ((x) > max ? max : (x)))

typedef float (*obs_fader_conversion_t)(const float val);

struct fader_cb {
	obs_fader_changed_t callback;
	void *param;
};

struct obs_fader {
	pthread_mutex_t mutex;
	obs_fader_conversion_t def_to_db;
	obs_fader_conversion_t db_to_def;
	obs_source_t *source;
	enum obs_fader_type type;
	float max_db;
	float min_db;
	float cur_db;
	bool ignore_next_signal;

	pthread_mutex_t callback_mutex;
	DARRAY(struct fader_cb) callbacks;
};

struct meter_cb {
	obs_volmeter_updated_t callback;
	void *param;
};

struct obs_volmeter {
	pthread_mutex_t mutex;
	obs_source_t *source;
	enum obs_fader_type type;
	float cur_db;

	pthread_mutex_t callback_mutex;
	DARRAY(struct meter_cb) callbacks;

	enum obs_peak_meter_type peak_meter_type;
	unsigned int update_ms;
	float prev_samples[MAX_AUDIO_CHANNELS][4];

	float magnitude[MAX_AUDIO_CHANNELS];
	float peak[MAX_AUDIO_CHANNELS];
};

static float cubic_def_to_db(const float def)
{
	if (def == 1.0f)
		return 0.0f;
	else if (def <= 0.0f)
		return -INFINITY;

	return mul_to_db(def * def * def);
}

static float cubic_db_to_def(const float db)
{
	if (db == 0.0f)
		return 1.0f;
	else if (db == -INFINITY)
		return 0.0f;

	return cbrtf(db_to_mul(db));
}

static float iec_def_to_db(const float def)
{
	if (def == 1.0f)
		return 0.0f;
	else if (def <= 0.0f)
		return -INFINITY;

	float db;

	if (def >= 0.75f)
		db = (def - 1.0f) / 0.25f * 9.0f;
	else if (def >= 0.5f)
		db = (def - 0.75f) / 0.25f * 11.0f - 9.0f;
	else if (def >= 0.3f)
		db = (def - 0.5f) / 0.2f * 10.0f - 20.0f;
	else if (def >= 0.15f)
		db = (def - 0.3f) / 0.15f * 10.0f - 30.0f;
	else if (def >= 0.075f)
		db = (def - 0.15f) / 0.075f * 10.0f - 40.0f;
	else if (def >= 0.025f)
		db = (def - 0.075f) / 0.05f * 10.0f - 50.0f;
	else if (def >= 0.001f)
		db = (def - 0.025f) / 0.025f * 90.0f - 60.0f;
	else
		db = -INFINITY;

	return db;
}

static float iec_db_to_def(const float db)
{
	if (db == 0.0f)
		return 1.0f;
	else if (db == -INFINITY)
		return 0.0f;

	float def;

	if (db >= -9.0f)
		def = (db + 9.0f) / 9.0f * 0.25f + 0.75f;
	else if (db >= -20.0f)
		def = (db + 20.0f) / 11.0f * 0.25f + 0.5f;
	else if (db >= -30.0f)
		def = (db + 30.0f) / 10.0f * 0.2f + 0.3f;
	else if (db >= -40.0f)
		def = (db + 40.0f) / 10.0f * 0.15f + 0.15f;
	else if (db >= -50.0f)
		def = (db + 50.0f) / 10.0f * 0.075f + 0.075f;
	else if (db >= -60.0f)
		def = (db + 60.0f) / 10.0f * 0.05f + 0.025f;
	else if (db >= -114.0f)
		def = (db + 150.0f) / 90.0f * 0.025f;
	else
		def = 0.0f;

	return def;
}

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f
/* equals -log10f(LOG_OFFSET_DB) */
#define LOG_OFFSET_VAL -0.77815125038364363f
/* equals -log10f(-LOG_RANGE_DB + LOG_OFFSET_DB) */
#define LOG_RANGE_VAL -2.00860017176191756f

static float log_def_to_db(const float def)
{
	if (def >= 1.0f)
		return 0.0f;
	else if (def <= 0.0f)
		return -INFINITY;

	return -(LOG_RANGE_DB + LOG_OFFSET_DB) *
		       powf((LOG_RANGE_DB + LOG_OFFSET_DB) / LOG_OFFSET_DB,
			    -def) +
	       LOG_OFFSET_DB;
}

static float log_db_to_def(const float db)
{
	if (db >= 0.0f)
		return 1.0f;
	else if (db <= -96.0f)
		return 0.0f;

	return (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) /
	       (LOG_OFFSET_VAL - LOG_RANGE_VAL);
}

static void signal_volume_changed(struct obs_fader *fader, const float db)
{
	pthread_mutex_lock(&fader->callback_mutex);
	for (size_t i = fader->callbacks.num; i > 0; i--) {
		struct fader_cb cb = fader->callbacks.array[i - 1];
		cb.callback(cb.param, db);
	}
	pthread_mutex_unlock(&fader->callback_mutex);
}

static void signal_levels_updated(struct obs_volmeter *volmeter,
				  const float magnitude[MAX_AUDIO_CHANNELS],
				  const float peak[MAX_AUDIO_CHANNELS],
				  const float input_peak[MAX_AUDIO_CHANNELS])
{
	pthread_mutex_lock(&volmeter->callback_mutex);
	for (size_t i = volmeter->callbacks.num; i > 0; i--) {
		struct meter_cb cb = volmeter->callbacks.array[i - 1];
		cb.callback(cb.param, magnitude, peak, input_peak);
	}
	pthread_mutex_unlock(&volmeter->callback_mutex);
}

static void fader_source_volume_changed(void *vptr, calldata_t *calldata)
{
	struct obs_fader *fader = (struct obs_fader *)vptr;

	pthread_mutex_lock(&fader->mutex);

	if (fader->ignore_next_signal) {
		fader->ignore_next_signal = false;
		pthread_mutex_unlock(&fader->mutex);
		return;
	}

	const float mul = (float)calldata_float(calldata, "volume");
	const float db = mul_to_db(mul);
	fader->cur_db = db;

	pthread_mutex_unlock(&fader->mutex);

	signal_volume_changed(fader, db);
}

static void volmeter_source_volume_changed(void *vptr, calldata_t *calldata)
{
	struct obs_volmeter *volmeter = (struct obs_volmeter *)vptr;

	pthread_mutex_lock(&volmeter->mutex);

	float mul = (float)calldata_float(calldata, "volume");
	volmeter->cur_db = mul_to_db(mul);

	pthread_mutex_unlock(&volmeter->mutex);
}

static void fader_source_destroyed(void *vptr, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	struct obs_fader *fader = (struct obs_fader *)vptr;

	obs_fader_detach_source(fader);
}

static void volmeter_source_destroyed(void *vptr, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	struct obs_volmeter *volmeter = (struct obs_volmeter *)vptr;

	obs_volmeter_detach_source(volmeter);
}

static int get_nr_channels_from_audio_data(const struct audio_data *data)
{
	int nr_channels = 0;
	for (int i = 0; i < MAX_AV_PLANES; i++) {
		if (data->data[i])
			nr_channels++;
	}
	return CLAMP(nr_channels, 0, MAX_AUDIO_CHANNELS);
}

/* msb(h, g, f, e) lsb(d, c, b, a)   -->  msb(h, h, g, f) lsb(e, d, c, b)
 */
#define SHIFT_RIGHT_2PS(msb, lsb)                                          \
	{                                                                  \
		__m128 tmp =                                               \
			_mm_shuffle_ps(lsb, msb, _MM_SHUFFLE(0, 0, 3, 3)); \
		lsb = _mm_shuffle_ps(lsb, tmp, _MM_SHUFFLE(2, 1, 2, 1));   \
		msb = _mm_shuffle_ps(msb, msb, _MM_SHUFFLE(3, 3, 2, 1));   \
	}

/* x(d, c, b, a) --> (|d|, |c|, |b|, |a|)
 */
#define abs_ps(v) _mm_andnot_ps(_mm_set1_ps(-0.f), v)

/* Take cross product of a vector with a matrix resulting in vector.
 */
#define VECTOR_MATRIX_CROSS_PS(out, v, m0, m1, m2, m3)    \
	{                                                 \
		out = _mm_mul_ps(v, m0);                  \
		__m128 mul1 = _mm_mul_ps(v, m1);          \
		__m128 mul2 = _mm_mul_ps(v, m2);          \
		__m128 mul3 = _mm_mul_ps(v, m3);          \
                                                          \
		_MM_TRANSPOSE4_PS(out, mul1, mul2, mul3); \
                                                          \
		out = _mm_add_ps(out, mul1);              \
		out = _mm_add_ps(out, mul2);              \
		out = _mm_add_ps(out, mul3);              \
	}

/* x4(d, c, b, a)  -->  max(a, b, c, d)
 */
#define hmax_ps(r, x4)                     \
	do {                               \
		float x4_mem[4];           \
		_mm_storeu_ps(x4_mem, x4); \
		r = x4_mem[0];             \
		r = fmaxf(r, x4_mem[1]);   \
		r = fmaxf(r, x4_mem[2]);   \
		r = fmaxf(r, x4_mem[3]);   \
	} while (false)

/* Calculate the true peak over a set of samples.
 * The algorithm implements 5x oversampling by using Whittaker–Shannon
 * interpolation over four samples.
 *
 * The four samples have location t=-1.5, -0.5, +0.5, +1.5
 * The oversamples are taken at locations t=-0.3, -0.1, +0.1, +0.3
 *
 * @param previous_samples  Last 4 samples from the previous iteration.
 * @param samples           The samples to find the peak in.
 * @param nr_samples        Number of sets of 4 samples.
 * @returns 5 times oversampled true-peak from the set of samples.
 */
static float get_true_peak(__m128 previous_samples, const float *samples,
			   size_t nr_samples)
{
	/* These are normalized-sinc parameters for interpolating over sample
	 * points which are located at x-coords: -1.5, -0.5, +0.5, +1.5.
	 * And oversample points at x-coords: -0.3, -0.1, 0.1, 0.3. */
	const __m128 m3 =
		_mm_set_ps(-0.155915f, 0.935489f, 0.233872f, -0.103943f);
	const __m128 m1 =
		_mm_set_ps(-0.216236f, 0.756827f, 0.504551f, -0.189207f);
	const __m128 p1 =
		_mm_set_ps(-0.189207f, 0.504551f, 0.756827f, -0.216236f);
	const __m128 p3 =
		_mm_set_ps(-0.103943f, 0.233872f, 0.935489f, -0.155915f);

	__m128 work = previous_samples;
	__m128 peak = previous_samples;
	for (size_t i = 0; (i + 3) < nr_samples; i += 4) {
		__m128 new_work = _mm_load_ps(&samples[i]);
		__m128 intrp_samples;

		/* Include the actual sample values in the peak. */
		__m128 abs_new_work = abs_ps(new_work);
		peak = _mm_max_ps(peak, abs_new_work);

		/* Shift in the next point. */
		SHIFT_RIGHT_2PS(new_work, work);
		VECTOR_MATRIX_CROSS_PS(intrp_samples, work, m3, m1, p1, p3);
		peak = _mm_max_ps(peak, abs_ps(intrp_samples));

		SHIFT_RIGHT_2PS(new_work, work);
		VECTOR_MATRIX_CROSS_PS(intrp_samples, work, m3, m1, p1, p3);
		peak = _mm_max_ps(peak, abs_ps(intrp_samples));

		SHIFT_RIGHT_2PS(new_work, work);
		VECTOR_MATRIX_CROSS_PS(intrp_samples, work, m3, m1, p1, p3);
		peak = _mm_max_ps(peak, abs_ps(intrp_samples));

		SHIFT_RIGHT_2PS(new_work, work);
		VECTOR_MATRIX_CROSS_PS(intrp_samples, work, m3, m1, p1, p3);
		peak = _mm_max_ps(peak, abs_ps(intrp_samples));
	}

	float r;
	hmax_ps(r, peak);
	return r;
}

/* points contain the first four samples to calculate the sinc interpolation
 * over. They will have come from a previous iteration.
 */
static float get_sample_peak(__m128 previous_samples, const float *samples,
			     size_t nr_samples)
{
	__m128 peak = previous_samples;
	for (size_t i = 0; (i + 3) < nr_samples; i += 4) {
		__m128 new_work = _mm_load_ps(&samples[i]);
		peak = _mm_max_ps(peak, abs_ps(new_work));
	}

	float r;
	hmax_ps(r, peak);
	return r;
}

static void volmeter_process_peak_last_samples(obs_volmeter_t *volmeter,
					       int channel_nr, float *samples,
					       size_t nr_samples)
{
	/* Take the last 4 samples that need to be used for the next peak
	 * calculation. If there are less than 4 samples in total the new
	 * samples shift out the old samples. */

	switch (nr_samples) {
	case 0:
		break;
	case 1:
		volmeter->prev_samples[channel_nr][0] =
			volmeter->prev_samples[channel_nr][1];
		volmeter->prev_samples[channel_nr][1] =
			volmeter->prev_samples[channel_nr][2];
		volmeter->prev_samples[channel_nr][2] =
			volmeter->prev_samples[channel_nr][3];
		volmeter->prev_samples[channel_nr][3] = samples[nr_samples - 1];
		break;
	case 2:
		volmeter->prev_samples[channel_nr][0] =
			volmeter->prev_samples[channel_nr][2];
		volmeter->prev_samples[channel_nr][1] =
			volmeter->prev_samples[channel_nr][3];
		volmeter->prev_samples[channel_nr][2] = samples[nr_samples - 2];
		volmeter->prev_samples[channel_nr][3] = samples[nr_samples - 1];
		break;
	case 3:
		volmeter->prev_samples[channel_nr][0] =
			volmeter->prev_samples[channel_nr][3];
		volmeter->prev_samples[channel_nr][1] = samples[nr_samples - 3];
		volmeter->prev_samples[channel_nr][2] = samples[nr_samples - 2];
		volmeter->prev_samples[channel_nr][3] = samples[nr_samples - 1];
		break;
	default:
		volmeter->prev_samples[channel_nr][0] = samples[nr_samples - 4];
		volmeter->prev_samples[channel_nr][1] = samples[nr_samples - 3];
		volmeter->prev_samples[channel_nr][2] = samples[nr_samples - 2];
		volmeter->prev_samples[channel_nr][3] = samples[nr_samples - 1];
	}
}

static void volmeter_process_peak(obs_volmeter_t *volmeter,
				  const struct audio_data *data,
				  int nr_channels)
{
	int nr_samples = data->frames;
	int channel_nr = 0;
	for (int plane_nr = 0; channel_nr < nr_channels; plane_nr++) {
		float *samples = (float *)data->data[plane_nr];
		if (!samples) {
			continue;
		}
		if (((uintptr_t)samples & 0xf) > 0) {
			printf("Audio plane %i is not aligned %p skipping "
			       "peak volume measurement.\n",
			       plane_nr, samples);
			volmeter->peak[channel_nr] = 1.0;
			channel_nr++;
			continue;
		}

		/* volmeter->prev_samples may not be aligned to 16 bytes;
		 * use unaligned load. */
		__m128 previous_samples =
			_mm_loadu_ps(volmeter->prev_samples[channel_nr]);

		float peak;
		switch (volmeter->peak_meter_type) {
		case TRUE_PEAK_METER:
			peak = get_true_peak(previous_samples, samples,
					     nr_samples);
			break;

		case SAMPLE_PEAK_METER:
		default:
			peak = get_sample_peak(previous_samples, samples,
					       nr_samples);
			break;
		}

		volmeter_process_peak_last_samples(volmeter, channel_nr,
						   samples, nr_samples);

		volmeter->peak[channel_nr] = peak;

		channel_nr++;
	}

	/* Clear the peak of the channels that have not been handled. */
	for (; channel_nr < MAX_AUDIO_CHANNELS; channel_nr++) {
		volmeter->peak[channel_nr] = 0.0;
	}
}

static void volmeter_process_magnitude(obs_volmeter_t *volmeter,
				       const struct audio_data *data,
				       int nr_channels)
{
	size_t nr_samples = data->frames;

	int channel_nr = 0;
	for (int plane_nr = 0; channel_nr < nr_channels; plane_nr++) {
		float *samples = (float *)data->data[plane_nr];
		if (!samples) {
			continue;
		}

		float sum = 0.0;
		for (size_t i = 0; i < nr_samples; i++) {
			float sample = samples[i];
			sum += sample * sample;
		}
		volmeter->magnitude[channel_nr] = sqrtf(sum / nr_samples);

		channel_nr++;
	}
}

static void volmeter_process_audio_data(obs_volmeter_t *volmeter,
					const struct audio_data *data)
{
	int nr_channels = get_nr_channels_from_audio_data(data);

	volmeter_process_peak(volmeter, data, nr_channels);
	volmeter_process_magnitude(volmeter, data, nr_channels);
}

static void volmeter_source_data_received(void *vptr, obs_source_t *source,
					  const struct audio_data *data,
					  bool muted)
{
	struct obs_volmeter *volmeter = (struct obs_volmeter *)vptr;
	float mul;
	float magnitude[MAX_AUDIO_CHANNELS];
	float peak[MAX_AUDIO_CHANNELS];
	float input_peak[MAX_AUDIO_CHANNELS];

	pthread_mutex_lock(&volmeter->mutex);

	volmeter_process_audio_data(volmeter, data);

	// Adjust magnitude/peak based on the volume level set by the user.
	// And convert to dB.
	mul = muted ? 0.0f : db_to_mul(volmeter->cur_db);
	for (int channel_nr = 0; channel_nr < MAX_AUDIO_CHANNELS;
	     channel_nr++) {
		magnitude[channel_nr] =
			mul_to_db(volmeter->magnitude[channel_nr] * mul);
		peak[channel_nr] = mul_to_db(volmeter->peak[channel_nr] * mul);

		/* The input-peak is NOT adjusted with volume, so that the user
		 * can check the input-gain. */
		input_peak[channel_nr] = mul_to_db(volmeter->peak[channel_nr]);
	}

	pthread_mutex_unlock(&volmeter->mutex);

	signal_levels_updated(volmeter, magnitude, peak, input_peak);

	UNUSED_PARAMETER(source);
}

obs_fader_t *obs_fader_create(enum obs_fader_type type)
{
	struct obs_fader *fader = bzalloc(sizeof(struct obs_fader));
	if (!fader)
		return NULL;

	pthread_mutex_init_value(&fader->mutex);
	pthread_mutex_init_value(&fader->callback_mutex);
	if (pthread_mutex_init(&fader->mutex, NULL) != 0)
		goto fail;
	if (pthread_mutex_init(&fader->callback_mutex, NULL) != 0)
		goto fail;

	switch (type) {
	case OBS_FADER_CUBIC:
		fader->def_to_db = cubic_def_to_db;
		fader->db_to_def = cubic_db_to_def;
		fader->max_db = 0.0f;
		fader->min_db = -INFINITY;
		break;
	case OBS_FADER_IEC:
		fader->def_to_db = iec_def_to_db;
		fader->db_to_def = iec_db_to_def;
		fader->max_db = 0.0f;
		fader->min_db = -INFINITY;
		break;
	case OBS_FADER_LOG:
		fader->def_to_db = log_def_to_db;
		fader->db_to_def = log_db_to_def;
		fader->max_db = 0.0f;
		fader->min_db = -96.0f;
		break;
	default:
		goto fail;
		break;
	}
	fader->type = type;

	return fader;
fail:
	obs_fader_destroy(fader);
	return NULL;
}

void obs_fader_destroy(obs_fader_t *fader)
{
	if (!fader)
		return;

	obs_fader_detach_source(fader);
	da_free(fader->callbacks);
	pthread_mutex_destroy(&fader->callback_mutex);
	pthread_mutex_destroy(&fader->mutex);

	bfree(fader);
}

bool obs_fader_set_db(obs_fader_t *fader, const float db)
{
	if (!fader)
		return false;

	pthread_mutex_lock(&fader->mutex);

	bool clamped = false;
	fader->cur_db = db;

	if (fader->cur_db > fader->max_db) {
		fader->cur_db = fader->max_db;
		clamped = true;
	}
	if (fader->cur_db < fader->min_db) {
		fader->cur_db = -INFINITY;
		clamped = true;
	}

	fader->ignore_next_signal = true;
	obs_source_t *src = fader->source;
	const float mul = db_to_mul(fader->cur_db);

	pthread_mutex_unlock(&fader->mutex);

	if (src)
		obs_source_set_volume(src, mul);

	return !clamped;
}

float obs_fader_get_db(obs_fader_t *fader)
{
	if (!fader)
		return 0.0f;

	pthread_mutex_lock(&fader->mutex);
	const float db = fader->cur_db;
	pthread_mutex_unlock(&fader->mutex);

	return db;
}

bool obs_fader_set_deflection(obs_fader_t *fader, const float def)
{
	if (!fader)
		return false;

	return obs_fader_set_db(fader, fader->def_to_db(def));
}

float obs_fader_get_deflection(obs_fader_t *fader)
{
	if (!fader)
		return 0.0f;

	pthread_mutex_lock(&fader->mutex);
	const float def = fader->db_to_def(fader->cur_db);
	pthread_mutex_unlock(&fader->mutex);

	return def;
}

bool obs_fader_set_mul(obs_fader_t *fader, const float mul)
{
	if (!fader)
		return false;

	return obs_fader_set_db(fader, mul_to_db(mul));
}

float obs_fader_get_mul(obs_fader_t *fader)
{
	if (!fader)
		return 0.0f;

	pthread_mutex_lock(&fader->mutex);
	const float mul = db_to_mul(fader->cur_db);
	pthread_mutex_unlock(&fader->mutex);

	return mul;
}

bool obs_fader_attach_source(obs_fader_t *fader, obs_source_t *source)
{
	signal_handler_t *sh;
	float vol;

	if (!fader || !source)
		return false;

	obs_fader_detach_source(fader);

	sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "volume", fader_source_volume_changed,
			       fader);
	signal_handler_connect(sh, "destroy", fader_source_destroyed, fader);
	vol = obs_source_get_volume(source);

	pthread_mutex_lock(&fader->mutex);

	fader->source = source;
	fader->cur_db = mul_to_db(vol);

	pthread_mutex_unlock(&fader->mutex);

	return true;
}

void obs_fader_detach_source(obs_fader_t *fader)
{
	signal_handler_t *sh;
	obs_source_t *source;

	if (!fader)
		return;

	pthread_mutex_lock(&fader->mutex);
	source = fader->source;
	fader->source = NULL;
	pthread_mutex_unlock(&fader->mutex);

	if (!source)
		return;

	sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "volume", fader_source_volume_changed,
				  fader);
	signal_handler_disconnect(sh, "destroy", fader_source_destroyed, fader);
}

void obs_fader_add_callback(obs_fader_t *fader, obs_fader_changed_t callback,
			    void *param)
{
	struct fader_cb cb = {callback, param};

	if (!obs_ptr_valid(fader, "obs_fader_add_callback"))
		return;

	pthread_mutex_lock(&fader->callback_mutex);
	da_push_back(fader->callbacks, &cb);
	pthread_mutex_unlock(&fader->callback_mutex);
}

void obs_fader_remove_callback(obs_fader_t *fader, obs_fader_changed_t callback,
			       void *param)
{
	struct fader_cb cb = {callback, param};

	if (!obs_ptr_valid(fader, "obs_fader_remove_callback"))
		return;

	pthread_mutex_lock(&fader->callback_mutex);
	da_erase_item(fader->callbacks, &cb);
	pthread_mutex_unlock(&fader->callback_mutex);
}

obs_volmeter_t *obs_volmeter_create(enum obs_fader_type type)
{
	struct obs_volmeter *volmeter = bzalloc(sizeof(struct obs_volmeter));
	if (!volmeter)
		return NULL;

	pthread_mutex_init_value(&volmeter->mutex);
	pthread_mutex_init_value(&volmeter->callback_mutex);
	if (pthread_mutex_init(&volmeter->mutex, NULL) != 0)
		goto fail;
	if (pthread_mutex_init(&volmeter->callback_mutex, NULL) != 0)
		goto fail;

	volmeter->type = type;

	obs_volmeter_set_update_interval(volmeter, 50);

	return volmeter;
fail:
	obs_volmeter_destroy(volmeter);
	return NULL;
}

void obs_volmeter_destroy(obs_volmeter_t *volmeter)
{
	if (!volmeter)
		return;

	obs_volmeter_detach_source(volmeter);
	da_free(volmeter->callbacks);
	pthread_mutex_destroy(&volmeter->callback_mutex);
	pthread_mutex_destroy(&volmeter->mutex);

	bfree(volmeter);
}

bool obs_volmeter_attach_source(obs_volmeter_t *volmeter, obs_source_t *source)
{
	signal_handler_t *sh;
	float vol;

	if (!volmeter || !source)
		return false;

	obs_volmeter_detach_source(volmeter);

	sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "volume", volmeter_source_volume_changed,
			       volmeter);
	signal_handler_connect(sh, "destroy", volmeter_source_destroyed,
			       volmeter);
	obs_source_add_audio_capture_callback(
		source, volmeter_source_data_received, volmeter);
	vol = obs_source_get_volume(source);

	pthread_mutex_lock(&volmeter->mutex);

	volmeter->source = source;
	volmeter->cur_db = mul_to_db(vol);

	pthread_mutex_unlock(&volmeter->mutex);

	return true;
}

void obs_volmeter_detach_source(obs_volmeter_t *volmeter)
{
	signal_handler_t *sh;
	obs_source_t *source;

	if (!volmeter)
		return;

	pthread_mutex_lock(&volmeter->mutex);
	source = volmeter->source;
	volmeter->source = NULL;
	pthread_mutex_unlock(&volmeter->mutex);

	if (!source)
		return;

	sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "volume", volmeter_source_volume_changed,
				  volmeter);
	signal_handler_disconnect(sh, "destroy", volmeter_source_destroyed,
				  volmeter);
	obs_source_remove_audio_capture_callback(
		source, volmeter_source_data_received, volmeter);
}

void obs_volmeter_set_peak_meter_type(obs_volmeter_t *volmeter,
				      enum obs_peak_meter_type peak_meter_type)
{
	pthread_mutex_lock(&volmeter->mutex);
	volmeter->peak_meter_type = peak_meter_type;
	pthread_mutex_unlock(&volmeter->mutex);
}

void obs_volmeter_set_update_interval(obs_volmeter_t *volmeter,
				      const unsigned int ms)
{
	if (!volmeter || !ms)
		return;

	pthread_mutex_lock(&volmeter->mutex);
	volmeter->update_ms = ms;
	pthread_mutex_unlock(&volmeter->mutex);
}

unsigned int obs_volmeter_get_update_interval(obs_volmeter_t *volmeter)
{
	if (!volmeter)
		return 0;

	pthread_mutex_lock(&volmeter->mutex);
	const unsigned int interval = volmeter->update_ms;
	pthread_mutex_unlock(&volmeter->mutex);

	return interval;
}

int obs_volmeter_get_nr_channels(obs_volmeter_t *volmeter)
{
	int source_nr_audio_channels;
	int obs_nr_audio_channels;

	if (volmeter->source) {
		source_nr_audio_channels = get_audio_channels(
			volmeter->source->sample_info.speakers);
	} else {
		source_nr_audio_channels = 1;
	}

	struct obs_audio_info audio_info;
	if (obs_get_audio_info(&audio_info)) {
		obs_nr_audio_channels = get_audio_channels(audio_info.speakers);
	} else {
		obs_nr_audio_channels = 2;
	}

	return CLAMP(source_nr_audio_channels, 1, obs_nr_audio_channels);
}

void obs_volmeter_add_callback(obs_volmeter_t *volmeter,
			       obs_volmeter_updated_t callback, void *param)
{
	struct meter_cb cb = {callback, param};

	if (!obs_ptr_valid(volmeter, "obs_volmeter_add_callback"))
		return;

	pthread_mutex_lock(&volmeter->callback_mutex);
	da_push_back(volmeter->callbacks, &cb);
	pthread_mutex_unlock(&volmeter->callback_mutex);
}

void obs_volmeter_remove_callback(obs_volmeter_t *volmeter,
				  obs_volmeter_updated_t callback, void *param)
{
	struct meter_cb cb = {callback, param};

	if (!obs_ptr_valid(volmeter, "obs_volmeter_remove_callback"))
		return;

	pthread_mutex_lock(&volmeter->callback_mutex);
	da_erase_item(volmeter->callbacks, &cb);
	pthread_mutex_unlock(&volmeter->callback_mutex);
}

float obs_mul_to_db(float mul)
{
	return mul_to_db(mul);
}

float obs_db_to_mul(float db)
{
	return db_to_mul(db);
}
