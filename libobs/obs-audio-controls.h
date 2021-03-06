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

#pragma once

#include "obs.h"

/**
 * @file
 * @brief header for audio controls
 *
 * @brief Audio controls for use in GUIs
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fader types
 */
enum obs_fader_type {
	/**
	 * @brief A simple cubic fader for controlling audio levels
	 *
	 * This is a very common type of software fader since it yields good
	 * results while being quite performant.
	 * The input value is mapped to mul values with the simple formula x^3.
	 */
	OBS_FADER_CUBIC,
	/**
	 * @brief A fader compliant to IEC 60-268-18
	 *
	 * This type of fader has several segments with different slopes that
	 * map deflection linearly to dB values. The segments are defined as
	 * in the following table:
	 *
	@code
	Deflection           | Volume
	------------------------------------------
	[ 100   %, 75   % ]  | [   0 dB,   -9 dB ]
	[  75   %, 50   % ]  | [  -9 dB,  -20 dB ]
	[  50   %, 30   % ]  | [ -20 dB,  -30 dB ]
	[  30   %, 15   % ]  | [ -30 dB,  -40 dB ]
	[  15   %,  7.5 % ]  | [ -40 dB,  -50 dB ]
	[   7.5 %,  2.5 % ]  | [ -50 dB,  -60 dB ]
	[   2.5 %,  0   % ]  | [ -60 dB, -inf dB ]
	@endcode
	 */
	OBS_FADER_IEC,
	/**
	 * @brief Logarithmic fader
	 */
	OBS_FADER_LOG
};

/**
 * @brief Peak meter types
 */
enum obs_peak_meter_type {
	/**
	 * @brief A simple peak meter measuring the maximum of all samples.
	 *
	 * This was a very common type of peak meter used for audio, but
	 * is not very accurate with regards to further audio processing.
	 */
	SAMPLE_PEAK_METER,

	/**
	 * @brief An accurate peak meter measure the maximum of inter-samples.
	 *
	 * This meter is more computational intensive due to 4x oversampling
	 * to determine the true peak to an accuracy of +/- 0.5 dB.
	 */
	TRUE_PEAK_METER
};

/**
 * @brief Create a fader
 * @param type the type of the fader
 * @return pointer to the fader object
 *
 * A fader object is used to map input values from a gui element to dB and
 * subsequently multiplier values used by libobs to mix audio.
 * The current "position" of the fader is internally stored as dB value.
 */
EXPORT obs_fader_t *obs_fader_create(enum obs_fader_type type);

/**
 * @brief Destroy a fader
 * @param fader pointer to the fader object
 *
 * Destroy the fader and free all related data
 */
EXPORT void obs_fader_destroy(obs_fader_t *fader);

/**
 * @brief Set the fader dB value
 * @param fader pointer to the fader object
 * @param db new dB value
 * @return true if value was set without clamping
 */
EXPORT bool obs_fader_set_db(obs_fader_t *fader, const float db);

/**
 * @brief Get the current fader dB value
 * @param fader pointer to the fader object
 * @return current fader dB value
 */
EXPORT float obs_fader_get_db(obs_fader_t *fader);

/**
 * @brief Set the fader value from deflection
 * @param fader pointer to the fader object
 * @param def new deflection
 * @return true if value was set without clamping
 *
 * This sets the new fader value from the supplied deflection, in case the
 * resulting value was clamped due to limits this function will return false.
 * The deflection is typically in the range [0.0, 1.0] but may be higher in
 * order to provide some amplification. In order for this to work the high dB
 * limit has to be set.
 */
EXPORT bool obs_fader_set_deflection(obs_fader_t *fader, const float def);

/**
 * @brief Get the current fader deflection
 * @param fader pointer to the fader object
 * @return current fader deflection
 */
EXPORT float obs_fader_get_deflection(obs_fader_t *fader);

/**
 * @brief Set the fader value from multiplier
 * @param fader pointer to the fader object
 * @return true if the value was set without clamping
 */
EXPORT bool obs_fader_set_mul(obs_fader_t *fader, const float mul);

/**
 * @brief Get the current fader multiplier value
 * @param fader pointer to the fader object
 * @return current fader multiplier
 */
EXPORT float obs_fader_get_mul(obs_fader_t *fader);

/**
 * @brief Attach the fader to a source
 * @param fader pointer to the fader object
 * @param source pointer to the source object
 * @return true on success
 *
 * When the fader is attached to a source it will automatically sync it's state
 * to the volume of the source.
 */
EXPORT bool obs_fader_attach_source(obs_fader_t *fader, obs_source_t *source);

/**
 * @brief Detach the fader from the currently attached source
 * @param fader pointer to the fader object
 */
EXPORT void obs_fader_detach_source(obs_fader_t *fader);

typedef void (*obs_fader_changed_t)(void *param, float db);

EXPORT void obs_fader_add_callback(obs_fader_t *fader,
				   obs_fader_changed_t callback, void *param);
EXPORT void obs_fader_remove_callback(obs_fader_t *fader,
				      obs_fader_changed_t callback,
				      void *param);

/**
 * @brief Create a volume meter
 * @param type the mapping type to use for the volume meter
 * @return pointer to the volume meter object
 *
 * A volume meter object is used to prepare the sound levels reported by audio
 * sources for display in a GUI.
 * It will automatically take source volume into account and map the levels
 * to a range [0.0f, 1.0f].
 */
EXPORT obs_volmeter_t *obs_volmeter_create(enum obs_fader_type type);

/**
 * @brief Destroy a volume meter
 * @param volmeter pointer to the volmeter object
 *
 * Destroy the volume meter and free all related data
 */
EXPORT void obs_volmeter_destroy(obs_volmeter_t *volmeter);

/**
 * @brief Attach the volume meter to a source
 * @param volmeter pointer to the volume meter object
 * @param source pointer to the source object
 * @return true on success
 *
 * When the volume meter is attached to a source it will start to listen to
 * volume updates on the source and after preparing the data emit its own
 * signal.
 */
EXPORT bool obs_volmeter_attach_source(obs_volmeter_t *volmeter,
				       obs_source_t *source);

/**
 * @brief Detach the volume meter from the currently attached source
 * @param volmeter pointer to the volume meter object
 */
EXPORT void obs_volmeter_detach_source(obs_volmeter_t *volmeter);

/**
 * @brief Set the peak meter type for the volume meter
 * @param volmeter pointer to the volume meter object
 * @param peak_meter_type set if true-peak needs to be measured.
 */
EXPORT void
obs_volmeter_set_peak_meter_type(obs_volmeter_t *volmeter,
				 enum obs_peak_meter_type peak_meter_type);

/**
 * @brief Set the update interval for the volume meter
 * @param volmeter pointer to the volume meter object
 * @param ms update interval in ms
 *
 * This sets the update interval in milliseconds that should be processed before
 * the resulting values are emitted by the levels_updated signal. The resulting
 * number of audio samples is rounded to an integer.
 *
 * Please note that due to way obs does receive audio data from the sources
 * this is no hard guarantee for the timing of the signal itself. When the
 * volume meter receives a chunk of data that is multiple the size of the sample
 * interval, all data will be sampled and the values updated accordingly, but
 * only the signal for the last segment is actually emitted.
 * On the other hand data might be received in a way that will cause the signal
 * to be emitted in shorter intervals than specified here under some
 * circumstances.
 */
EXPORT void obs_volmeter_set_update_interval(obs_volmeter_t *volmeter,
					     const unsigned int ms);

/**
 * @brief Get the update interval currently used for the volume meter
 * @param volmeter pointer to the volume meter object
 * @return update interval in ms
 */
EXPORT unsigned int obs_volmeter_get_update_interval(obs_volmeter_t *volmeter);

/**
 * @brief Get the number of channels which are configured for this source.
 * @param volmeter pointer to the volume meter object
 */
EXPORT int obs_volmeter_get_nr_channels(obs_volmeter_t *volmeter);

typedef void (*obs_volmeter_updated_t)(
	void *param, const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS],
	const float input_peak[MAX_AUDIO_CHANNELS]);

EXPORT void obs_volmeter_add_callback(obs_volmeter_t *volmeter,
				      obs_volmeter_updated_t callback,
				      void *param);
EXPORT void obs_volmeter_remove_callback(obs_volmeter_t *volmeter,
					 obs_volmeter_updated_t callback,
					 void *param);

EXPORT float obs_mul_to_db(float mul);
EXPORT float obs_db_to_mul(float db);

#ifdef __cplusplus
}
#endif
