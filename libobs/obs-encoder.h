/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#pragma once

/**
 * @file
 * @brief header for modules implementing encoders.
 *
 * Encoders are modules that implement some codec that can be used by libobs
 * to process output data.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define OBS_ENCODER_CAP_DEPRECATED (1 << 0)
#define OBS_ENCODER_CAP_PASS_TEXTURE (1 << 1)
#define OBS_ENCODER_CAP_DYN_BITRATE (1 << 2)
#define OBS_ENCODER_CAP_INTERNAL (1 << 3)

/** Specifies the encoder type */
enum obs_encoder_type {
	OBS_ENCODER_AUDIO, /**< The encoder provides an audio codec */
	OBS_ENCODER_VIDEO  /**< The encoder provides a video codec */
};

/** Encoder output packet */
struct encoder_packet {
	uint8_t *data; /**< Packet data */
	size_t size;   /**< Packet size */

	int64_t pts; /**< Presentation timestamp */
	int64_t dts; /**< Decode timestamp */

	int32_t timebase_num; /**< Timebase numerator */
	int32_t timebase_den; /**< Timebase denominator */

	enum obs_encoder_type type; /**< Encoder type */

	bool keyframe; /**< Is a keyframe */

	/* ---------------------------------------------------------------- */
	/* Internal video variables (will be parsed automatically) */

	/* DTS in microseconds */
	int64_t dts_usec;

	/* System DTS in microseconds */
	int64_t sys_dts_usec;

	/**
	 * Packet priority
	 *
	 * This is generally use by video encoders to specify the priority
	 * of the packet.
	 */
	int priority;

	/**
	 * Dropped packet priority
	 *
	 * If this packet needs to be dropped, the next packet must be of this
	 * priority or higher to continue transmission.
	 */
	int drop_priority;

	/** Audio track index (used with outputs) */
	size_t track_idx;

	/** Encoder from which the track originated from */
	obs_encoder_t *encoder;
};

/** Encoder input frame */
struct encoder_frame {
	/** Data for the frame/audio */
	uint8_t *data[MAX_AV_PLANES];

	/** size of each plane */
	uint32_t linesize[MAX_AV_PLANES];

	/** Number of frames (audio only) */
	uint32_t frames;

	/** Presentation timestamp */
	int64_t pts;
};

/**
 * Encoder interface
 *
 * Encoders have a limited usage with OBS.  You are not generally supposed to
 * implement every encoder out there.  Generally, these are limited or specific
 * encoders for h264/aac for streaming and recording.  It doesn't have to be
 * *just* h264 or aac of course, but generally those are the expected encoders.
 *
 * That being said, other encoders will be kept in mind for future use.
 */
struct obs_encoder_info {
	/* ----------------------------------------------------------------- */
	/* Required implementation*/

	/** Specifies the named identifier of this encoder */
	const char *id;

	/** Specifies the encoder type (video or audio) */
	enum obs_encoder_type type;

	/** Specifies the codec */
	const char *codec;

	/**
	 * Gets the full translated name of this encoder
	 *
	 * @param  type_data  The type_data variable of this structure
	 * @return            Translated name of the encoder
	 */
	const char *(*get_name)(void *type_data);

	/**
	 * Creates the encoder with the specified settings
	 *
	 * @param  settings  Settings for the encoder
	 * @param  encoder   OBS encoder context
	 * @return           Data associated with this encoder context, or
	 *                   NULL if initialization failed.
	 */
	void *(*create)(obs_data_t *settings, obs_encoder_t *encoder);

	/**
	 * Destroys the encoder data
	 *
	 * @param  data  Data associated with this encoder context
	 */
	void (*destroy)(void *data);

	/**
	 * Encodes frame(s), and outputs encoded packets as they become
	 * available.
	 *
	 * @param       data             Data associated with this encoder
	 *                               context
	 * @param[in]   frame            Raw audio/video data to encode
	 * @param[out]  packet           Encoder packet output, if any
	 * @param[out]  received_packet  Set to true if a packet was received,
	 *                               false otherwise
	 * @return                       true if successful, false otherwise.
	 */
	bool (*encode)(void *data, struct encoder_frame *frame,
		       struct encoder_packet *packet, bool *received_packet);

	/** Audio encoder only:  Returns the frame size for this encoder */
	size_t (*get_frame_size)(void *data);

	/* ----------------------------------------------------------------- */
	/* Optional implementation */

	/**
	 * Gets the default settings for this encoder
	 *
	 * @param[out]  settings  Data to assign default settings to
	 */
	void (*get_defaults)(obs_data_t *settings);

	/**
	 * Gets the property information of this encoder
	 *
	 * @return         The properties data
	 */
	obs_properties_t *(*get_properties)(void *data);

	/**
	 * Updates the settings for this encoder (usually used for things like
	 * changing bitrate while active)
	 *
	 * @param  data      Data associated with this encoder context
	 * @param  settings  New settings for this encoder
	 * @return           true if successful, false otherwise
	 */
	bool (*update)(void *data, obs_data_t *settings);

	/**
	 * Returns extra data associated with this encoder (usually header)
	 *
	 * @param  data             Data associated with this encoder context
	 * @param[out]  extra_data  Pointer to receive the extra data
	 * @param[out]  size        Pointer to receive the size of the extra
	 *                          data
	 * @return                  true if extra data available, false
	 *                          otherwise
	 */
	bool (*get_extra_data)(void *data, uint8_t **extra_data, size_t *size);

	/**
	 * Gets the SEI data, if any
	 *
	 * @param       data      Data associated with this encoder context
	 * @param[out]  sei_data  Pointer to receive the SEI data
	 * @param[out]  size      Pointer to receive the SEI data size
	 * @return                true if SEI data available, false otherwise
	 */
	bool (*get_sei_data)(void *data, uint8_t **sei_data, size_t *size);

	/**
	 * Returns desired audio format and sample information
	 *
	 * @param          data  Data associated with this encoder context
	 * @param[in/out]  info  Audio format information
	 */
	void (*get_audio_info)(void *data, struct audio_convert_info *info);

	/**
	 * Returns desired video format information
	 *
	 * @param          data  Data associated with this encoder context
	 * @param[in/out]  info  Video format information
	 */
	void (*get_video_info)(void *data, struct video_scale_info *info);

	void *type_data;
	void (*free_type_data)(void *type_data);

	uint32_t caps;

	/**
	 * Gets the default settings for this encoder
	 * 
	 * If get_defaults is also defined both will be called, and the first
	 * call will be to get_defaults, then to get_defaults2.
	 *
	 * @param[out]  settings  Data to assign default settings to
	 * @param[in]   typedata  Type Data
	 */
	void (*get_defaults2)(obs_data_t *settings, void *type_data);

	/**
	 * Gets the property information of this encoder
	 *
	 * @param[in]   data      Pointer from create (or null)
	 * @param[in]   typedata  Type Data
	 * @return                The properties data
	 */
	obs_properties_t *(*get_properties2)(void *data, void *type_data);

	bool (*encode_texture)(void *data, uint32_t handle, int64_t pts,
			       uint64_t lock_key, uint64_t *next_key,
			       struct encoder_packet *packet,
			       bool *received_packet);
};

EXPORT void obs_register_encoder_s(const struct obs_encoder_info *info,
				   size_t size);

/**
 * Register an encoder definition to the current obs context.  This should be
 * used in obs_module_load.
 *
 * @param  info  Pointer to the source definition structure.
 */
#define obs_register_encoder(info) \
	obs_register_encoder_s(info, sizeof(struct obs_encoder_info))

#ifdef __cplusplus
}
#endif
