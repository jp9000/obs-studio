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

#include "obs.h"
#include "obs-internal.h"
#include "util/util_uint64.h"

#define encoder_active(encoder) os_atomic_load_bool(&encoder->active)
#define set_encoder_active(encoder, val) \
	os_atomic_set_bool(&encoder->active, val)

struct obs_encoder_info *find_encoder(const char *id)
{
	for (size_t i = 0; i < obs->encoder_types.num; i++) {
		struct obs_encoder_info *info = obs->encoder_types.array + i;

		if (strcmp(info->id, id) == 0)
			return info;
	}

	return NULL;
}

const char *obs_encoder_get_display_name(const char *id)
{
	struct obs_encoder_info *ei = find_encoder(id);
	return ei ? ei->get_name(ei->type_data) : NULL;
}

static bool init_encoder(struct obs_encoder *encoder, const char *name,
			 obs_data_t *settings, obs_data_t *hotkey_data)
{
	pthread_mutexattr_t attr;

	pthread_mutex_init_value(&encoder->init_mutex);
	pthread_mutex_init_value(&encoder->callbacks_mutex);
	pthread_mutex_init_value(&encoder->outputs_mutex);
	pthread_mutex_init_value(&encoder->pause.mutex);

	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return false;
	if (!obs_context_data_init(&encoder->context, OBS_OBJ_TYPE_ENCODER,
				   settings, name, hotkey_data, false))
		return false;
	if (pthread_mutex_init(&encoder->init_mutex, &attr) != 0)
		return false;
	if (pthread_mutex_init(&encoder->callbacks_mutex, &attr) != 0)
		return false;
	if (pthread_mutex_init(&encoder->outputs_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&encoder->pause.mutex, NULL) != 0)
		return false;

	if (encoder->orig_info.get_defaults) {
		encoder->orig_info.get_defaults(encoder->context.settings);
	}
	if (encoder->orig_info.get_defaults2) {
		encoder->orig_info.get_defaults2(encoder->context.settings,
						 encoder->orig_info.type_data);
	}

	return true;
}

static struct obs_encoder *
create_encoder(const char *id, enum obs_encoder_type type, const char *name,
	       obs_data_t *settings, size_t mixer_idx, obs_data_t *hotkey_data)
{
	struct obs_encoder *encoder;
	struct obs_encoder_info *ei = find_encoder(id);
	bool success;

	if (ei && ei->type != type)
		return NULL;

	encoder = bzalloc(sizeof(struct obs_encoder));
	encoder->mixer_idx = mixer_idx;

	if (!ei) {
		blog(LOG_ERROR, "Encoder ID '%s' not found", id);

		encoder->info.id = bstrdup(id);
		encoder->info.type = type;
		encoder->owns_info_id = true;
		encoder->orig_info = encoder->info;
	} else {
		encoder->info = *ei;
		encoder->orig_info = *ei;
	}

	success = init_encoder(encoder, name, settings, hotkey_data);
	if (!success) {
		blog(LOG_ERROR, "creating encoder '%s' (%s) failed", name, id);
		obs_encoder_destroy(encoder);
		return NULL;
	}

	encoder->control = bzalloc(sizeof(obs_weak_encoder_t));
	encoder->control->encoder = encoder;

	obs_context_data_insert(&encoder->context, &obs->data.encoders_mutex,
				&obs->data.first_encoder);

	blog(LOG_DEBUG, "encoder '%s' (%s) created", name, id);
	return encoder;
}

obs_encoder_t *obs_video_encoder_create(const char *id, const char *name,
					obs_data_t *settings,
					obs_data_t *hotkey_data)
{
	if (!name || !id)
		return NULL;
	return create_encoder(id, OBS_ENCODER_VIDEO, name, settings, 0,
			      hotkey_data);
}

obs_encoder_t *obs_audio_encoder_create(const char *id, const char *name,
					obs_data_t *settings, size_t mixer_idx,
					obs_data_t *hotkey_data)
{
	if (!name || !id)
		return NULL;
	return create_encoder(id, OBS_ENCODER_AUDIO, name, settings, mixer_idx,
			      hotkey_data);
}

static void receive_video(void *param, struct video_data *frame);
static void receive_audio(void *param, size_t mix_idx, struct audio_data *data);

static inline void get_audio_info(const struct obs_encoder *encoder,
				  struct audio_convert_info *info)
{
	const struct audio_output_info *aoi;
	aoi = audio_output_get_info(encoder->media);

	if (info->format == AUDIO_FORMAT_UNKNOWN)
		info->format = aoi->format;
	if (!info->samples_per_sec)
		info->samples_per_sec = aoi->samples_per_sec;
	if (info->speakers == SPEAKERS_UNKNOWN)
		info->speakers = aoi->speakers;

	if (encoder->info.get_audio_info)
		encoder->info.get_audio_info(encoder->context.data, info);
}

static inline void get_video_info(struct obs_encoder *encoder,
				  struct video_scale_info *info)
{
	const struct video_output_info *voi;
	voi = video_output_get_info(encoder->media);

	info->format = voi->format;
	info->colorspace = voi->colorspace;
	info->range = voi->range;
	info->width = obs_encoder_get_width(encoder);
	info->height = obs_encoder_get_height(encoder);

	if (encoder->info.get_video_info)
		encoder->info.get_video_info(encoder->context.data, info);

	if (info->width != voi->width || info->height != voi->height)
		obs_encoder_set_scaled_size(encoder, info->width, info->height);
}

static inline bool has_scaling(const struct obs_encoder *encoder)
{
	uint32_t video_width = video_output_get_width(encoder->media);
	uint32_t video_height = video_output_get_height(encoder->media);

	return encoder->scaled_width && encoder->scaled_height &&
	       (video_width != encoder->scaled_width ||
		video_height != encoder->scaled_height);
}

static inline bool gpu_encode_available(const struct obs_encoder *encoder)
{
	return (encoder->info.caps & OBS_ENCODER_CAP_PASS_TEXTURE) != 0 &&
	       obs->video.using_nv12_tex;
}

static void add_connection(struct obs_encoder *encoder)
{
	if (encoder->info.type == OBS_ENCODER_AUDIO) {
		struct audio_convert_info audio_info = {0};
		get_audio_info(encoder, &audio_info);

		audio_output_connect(encoder->media, encoder->mixer_idx,
				     &audio_info, receive_audio, encoder);
	} else {
		struct video_scale_info info = {0};
		get_video_info(encoder, &info);

		if (gpu_encode_available(encoder)) {
			start_gpu_encode(encoder);
		} else {
			start_raw_video(encoder->media, &info, receive_video,
					encoder);
		}
	}

	set_encoder_active(encoder, true);
}

static void remove_connection(struct obs_encoder *encoder, bool shutdown)
{
	if (encoder->info.type == OBS_ENCODER_AUDIO) {
		audio_output_disconnect(encoder->media, encoder->mixer_idx,
					receive_audio, encoder);
	} else {
		if (gpu_encode_available(encoder)) {
			stop_gpu_encode(encoder);
		} else {
			stop_raw_video(encoder->media, receive_video, encoder);
		}
	}

	/* obs_encoder_shutdown locks init_mutex, so don't call it on encode
	 * errors, otherwise you can get a deadlock with outputs when they end
	 * data capture, which will lock init_mutex and the video callback
	 * mutex in the reverse order.  instead, call shutdown before starting
	 * up again */
	if (shutdown)
		obs_encoder_shutdown(encoder);
	set_encoder_active(encoder, false);
}

static inline void free_audio_buffers(struct obs_encoder *encoder)
{
	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		circlebuf_free(&encoder->audio_input_buffer[i]);
		bfree(encoder->audio_output_buffer[i]);
		encoder->audio_output_buffer[i] = NULL;
	}
}

static void obs_encoder_actually_destroy(obs_encoder_t *encoder)
{
	if (encoder) {
		pthread_mutex_lock(&encoder->outputs_mutex);
		for (size_t i = 0; i < encoder->outputs.num; i++) {
			struct obs_output *output = encoder->outputs.array[i];
			obs_output_remove_encoder(output, encoder);
		}
		da_free(encoder->outputs);
		pthread_mutex_unlock(&encoder->outputs_mutex);

		blog(LOG_DEBUG, "encoder '%s' destroyed",
		     encoder->context.name);

		free_audio_buffers(encoder);

		if (encoder->context.data)
			encoder->info.destroy(encoder->context.data);
		da_free(encoder->callbacks);
		pthread_mutex_destroy(&encoder->init_mutex);
		pthread_mutex_destroy(&encoder->callbacks_mutex);
		pthread_mutex_destroy(&encoder->outputs_mutex);
		pthread_mutex_destroy(&encoder->pause.mutex);
		obs_context_data_free(&encoder->context);
		if (encoder->owns_info_id)
			bfree((void *)encoder->info.id);
		if (encoder->last_error_message)
			bfree(encoder->last_error_message);
		bfree(encoder);
	}
}

/* does not actually destroy the encoder until all connections to it have been
 * removed. (full reference counting really would have been superfluous) */
void obs_encoder_destroy(obs_encoder_t *encoder)
{
	if (encoder) {
		bool destroy;

		obs_context_data_remove(&encoder->context);

		pthread_mutex_lock(&encoder->init_mutex);
		pthread_mutex_lock(&encoder->callbacks_mutex);
		destroy = encoder->callbacks.num == 0;
		if (!destroy)
			encoder->destroy_on_stop = true;
		pthread_mutex_unlock(&encoder->callbacks_mutex);
		pthread_mutex_unlock(&encoder->init_mutex);

		if (destroy)
			obs_encoder_actually_destroy(encoder);
	}
}

const char *obs_encoder_get_name(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_name")
		       ? encoder->context.name
		       : NULL;
}

void obs_encoder_set_name(obs_encoder_t *encoder, const char *name)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_set_name"))
		return;

	if (name && *name && strcmp(name, encoder->context.name) != 0)
		obs_context_data_setname(&encoder->context, name);
}

static inline obs_data_t *get_defaults(const struct obs_encoder_info *info)
{
	obs_data_t *settings = obs_data_create();
	if (info->get_defaults) {
		info->get_defaults(settings);
	}
	if (info->get_defaults2) {
		info->get_defaults2(settings, info->type_data);
	}
	return settings;
}

obs_data_t *obs_encoder_defaults(const char *id)
{
	const struct obs_encoder_info *info = find_encoder(id);
	return (info) ? get_defaults(info) : NULL;
}

obs_data_t *obs_encoder_get_defaults(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_defaults"))
		return NULL;

	return get_defaults(&encoder->info);
}

obs_properties_t *obs_get_encoder_properties(const char *id)
{
	const struct obs_encoder_info *ei = find_encoder(id);
	if (ei && (ei->get_properties || ei->get_properties2)) {
		obs_data_t *defaults = get_defaults(ei);
		obs_properties_t *properties = NULL;

		if (ei->get_properties2) {
			properties = ei->get_properties2(NULL, ei->type_data);
		} else if (ei->get_properties) {
			properties = ei->get_properties(NULL);
		}

		obs_properties_apply_settings(properties, defaults);
		obs_data_release(defaults);
		return properties;
	}
	return NULL;
}

obs_properties_t *obs_encoder_properties(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_properties"))
		return NULL;

	if (encoder->orig_info.get_properties2) {
		obs_properties_t *props;
		props = encoder->orig_info.get_properties2(
			encoder->context.data, encoder->orig_info.type_data);
		obs_properties_apply_settings(props, encoder->context.settings);
		return props;

	} else if (encoder->orig_info.get_properties) {
		obs_properties_t *props;
		props = encoder->orig_info.get_properties(
			encoder->context.data);
		obs_properties_apply_settings(props, encoder->context.settings);
		return props;
	}

	return NULL;
}

void obs_encoder_update(obs_encoder_t *encoder, obs_data_t *settings)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_update"))
		return;

	obs_data_apply(encoder->context.settings, settings);

	if (encoder->info.update && encoder->context.data)
		encoder->info.update(encoder->context.data,
				     encoder->context.settings);
}

bool obs_encoder_get_extra_data(const obs_encoder_t *encoder,
				uint8_t **extra_data, size_t *size)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_extra_data"))
		return false;

	if (encoder->info.get_extra_data && encoder->context.data)
		return encoder->info.get_extra_data(encoder->context.data,
						    extra_data, size);

	return false;
}

obs_data_t *obs_encoder_get_settings(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_settings"))
		return NULL;

	obs_data_addref(encoder->context.settings);
	return encoder->context.settings;
}

static inline void reset_audio_buffers(struct obs_encoder *encoder)
{
	free_audio_buffers(encoder);

	for (size_t i = 0; i < encoder->planes; i++)
		encoder->audio_output_buffer[i] =
			bmalloc(encoder->framesize_bytes);
}

static void intitialize_audio_encoder(struct obs_encoder *encoder)
{
	struct audio_convert_info info = {0};
	get_audio_info(encoder, &info);

	encoder->samplerate = info.samples_per_sec;
	encoder->planes = get_audio_planes(info.format, info.speakers);
	encoder->blocksize = get_audio_size(info.format, info.speakers, 1);
	encoder->framesize =
		encoder->info.get_frame_size(encoder->context.data);

	encoder->framesize_bytes = encoder->blocksize * encoder->framesize;
	reset_audio_buffers(encoder);
}

static THREAD_LOCAL bool can_reroute = false;

static inline bool obs_encoder_initialize_internal(obs_encoder_t *encoder)
{
	if (encoder_active(encoder))
		return true;
	if (encoder->initialized)
		return true;

	obs_encoder_shutdown(encoder);

	if (encoder->orig_info.create) {
		can_reroute = true;
		encoder->info = encoder->orig_info;
		encoder->context.data = encoder->orig_info.create(
			encoder->context.settings, encoder);
		can_reroute = false;
	}
	if (!encoder->context.data)
		return false;

	if (encoder->orig_info.type == OBS_ENCODER_AUDIO)
		intitialize_audio_encoder(encoder);

	encoder->initialized = true;
	return true;
}

void *obs_encoder_create_rerouted(obs_encoder_t *encoder,
				  const char *reroute_id)
{
	if (!obs_ptr_valid(encoder, "obs_encoder_reroute"))
		return NULL;
	if (!obs_ptr_valid(reroute_id, "obs_encoder_reroute"))
		return NULL;
	if (!can_reroute)
		return NULL;

	const struct obs_encoder_info *ei = find_encoder(reroute_id);
	if (ei) {
		if (ei->type != encoder->orig_info.type ||
		    astrcmpi(ei->codec, encoder->orig_info.codec) != 0) {
			return NULL;
		}
		encoder->info = *ei;
		return encoder->info.create(encoder->context.settings, encoder);
	}

	return NULL;
}

bool obs_encoder_initialize(obs_encoder_t *encoder)
{
	bool success;

	if (!encoder)
		return false;

	pthread_mutex_lock(&encoder->init_mutex);
	success = obs_encoder_initialize_internal(encoder);
	pthread_mutex_unlock(&encoder->init_mutex);

	return success;
}

void obs_encoder_shutdown(obs_encoder_t *encoder)
{
	pthread_mutex_lock(&encoder->init_mutex);
	if (encoder->context.data) {
		encoder->info.destroy(encoder->context.data);
		encoder->context.data = NULL;
		encoder->paired_encoder = NULL;
		encoder->first_received = false;
		encoder->offset_usec = 0;
		encoder->start_ts = 0;
	}
	obs_encoder_set_last_error(encoder, NULL);
	pthread_mutex_unlock(&encoder->init_mutex);
}

static inline size_t
get_callback_idx(const struct obs_encoder *encoder,
		 void (*new_packet)(void *param, struct encoder_packet *packet),
		 void *param)
{
	for (size_t i = 0; i < encoder->callbacks.num; i++) {
		struct encoder_callback *cb = encoder->callbacks.array + i;

		if (cb->new_packet == new_packet && cb->param == param)
			return i;
	}

	return DARRAY_INVALID;
}

void pause_reset(struct pause_data *pause)
{
	pthread_mutex_lock(&pause->mutex);
	pause->last_video_ts = 0;
	pause->ts_start = 0;
	pause->ts_end = 0;
	pause->ts_offset = 0;
	pthread_mutex_unlock(&pause->mutex);
}

static inline void obs_encoder_start_internal(
	obs_encoder_t *encoder,
	void (*new_packet)(void *param, struct encoder_packet *packet),
	void *param)
{
	struct encoder_callback cb = {false, new_packet, param};
	bool first = false;

	if (!encoder->context.data)
		return;

	pthread_mutex_lock(&encoder->callbacks_mutex);

	first = (encoder->callbacks.num == 0);

	size_t idx = get_callback_idx(encoder, new_packet, param);
	if (idx == DARRAY_INVALID)
		da_push_back(encoder->callbacks, &cb);

	pthread_mutex_unlock(&encoder->callbacks_mutex);

	if (first) {
		os_atomic_set_bool(&encoder->paused, false);
		pause_reset(&encoder->pause);

		encoder->cur_pts = 0;
		add_connection(encoder);
	}
}

void obs_encoder_start(obs_encoder_t *encoder,
		       void (*new_packet)(void *param,
					  struct encoder_packet *packet),
		       void *param)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_start"))
		return;
	if (!obs_ptr_valid(new_packet, "obs_encoder_start"))
		return;

	pthread_mutex_lock(&encoder->init_mutex);
	obs_encoder_start_internal(encoder, new_packet, param);
	pthread_mutex_unlock(&encoder->init_mutex);
}

static inline bool obs_encoder_stop_internal(
	obs_encoder_t *encoder,
	void (*new_packet)(void *param, struct encoder_packet *packet),
	void *param)
{
	bool last = false;
	size_t idx;

	pthread_mutex_lock(&encoder->callbacks_mutex);

	idx = get_callback_idx(encoder, new_packet, param);
	if (idx != DARRAY_INVALID) {
		da_erase(encoder->callbacks, idx);
		last = (encoder->callbacks.num == 0);
	}

	pthread_mutex_unlock(&encoder->callbacks_mutex);

	if (last) {
		remove_connection(encoder, true);
		encoder->initialized = false;

		if (encoder->destroy_on_stop) {
			pthread_mutex_unlock(&encoder->init_mutex);
			obs_encoder_actually_destroy(encoder);
			return true;
		}
	}

	return false;
}

void obs_encoder_stop(obs_encoder_t *encoder,
		      void (*new_packet)(void *param,
					 struct encoder_packet *packet),
		      void *param)
{
	bool destroyed;

	if (!obs_encoder_valid(encoder, "obs_encoder_stop"))
		return;
	if (!obs_ptr_valid(new_packet, "obs_encoder_stop"))
		return;

	pthread_mutex_lock(&encoder->init_mutex);
	destroyed = obs_encoder_stop_internal(encoder, new_packet, param);
	if (!destroyed)
		pthread_mutex_unlock(&encoder->init_mutex);
}

const char *obs_encoder_get_codec(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_codec")
		       ? encoder->info.codec
		       : NULL;
}

const char *obs_get_encoder_codec(const char *id)
{
	struct obs_encoder_info *info = find_encoder(id);
	return info ? info->codec : NULL;
}

enum obs_encoder_type obs_encoder_get_type(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_type")
		       ? encoder->info.type
		       : OBS_ENCODER_AUDIO;
}

enum obs_encoder_type obs_get_encoder_type(const char *id)
{
	struct obs_encoder_info *info = find_encoder(id);
	return info ? info->type : OBS_ENCODER_AUDIO;
}

void obs_encoder_set_scaled_size(obs_encoder_t *encoder, uint32_t width,
				 uint32_t height)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_set_scaled_size"))
		return;
	if (encoder->info.type != OBS_ENCODER_VIDEO) {
		blog(LOG_WARNING,
		     "obs_encoder_set_scaled_size: "
		     "encoder '%s' is not a video encoder",
		     obs_encoder_get_name(encoder));
		return;
	}
	if (encoder_active(encoder)) {
		blog(LOG_WARNING,
		     "encoder '%s': Cannot set the scaled "
		     "resolution while the encoder is active",
		     obs_encoder_get_name(encoder));
		return;
	}

	encoder->scaled_width = width;
	encoder->scaled_height = height;
}

bool obs_encoder_scaling_enabled(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_scaling_enabled"))
		return false;

	return encoder->scaled_width || encoder->scaled_height;
}

uint32_t obs_encoder_get_width(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_width"))
		return 0;
	if (encoder->info.type != OBS_ENCODER_VIDEO) {
		blog(LOG_WARNING,
		     "obs_encoder_get_width: "
		     "encoder '%s' is not a video encoder",
		     obs_encoder_get_name(encoder));
		return 0;
	}
	if (!encoder->media)
		return 0;

	return encoder->scaled_width != 0
		       ? encoder->scaled_width
		       : video_output_get_width(encoder->media);
}

uint32_t obs_encoder_get_height(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_height"))
		return 0;
	if (encoder->info.type != OBS_ENCODER_VIDEO) {
		blog(LOG_WARNING,
		     "obs_encoder_get_height: "
		     "encoder '%s' is not a video encoder",
		     obs_encoder_get_name(encoder));
		return 0;
	}
	if (!encoder->media)
		return 0;

	return encoder->scaled_height != 0
		       ? encoder->scaled_height
		       : video_output_get_height(encoder->media);
}

uint32_t obs_encoder_get_sample_rate(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_sample_rate"))
		return 0;
	if (encoder->info.type != OBS_ENCODER_AUDIO) {
		blog(LOG_WARNING,
		     "obs_encoder_get_sample_rate: "
		     "encoder '%s' is not an audio encoder",
		     obs_encoder_get_name(encoder));
		return 0;
	}
	if (!encoder->media)
		return 0;

	return encoder->samplerate != 0
		       ? encoder->samplerate
		       : audio_output_get_sample_rate(encoder->media);
}

void obs_encoder_set_video(obs_encoder_t *encoder, video_t *video)
{
	const struct video_output_info *voi;

	if (!obs_encoder_valid(encoder, "obs_encoder_set_video"))
		return;
	if (encoder->info.type != OBS_ENCODER_VIDEO) {
		blog(LOG_WARNING,
		     "obs_encoder_set_video: "
		     "encoder '%s' is not a video encoder",
		     obs_encoder_get_name(encoder));
		return;
	}
	if (!video)
		return;

	voi = video_output_get_info(video);

	encoder->media = video;
	encoder->timebase_num = voi->fps_den;
	encoder->timebase_den = voi->fps_num;
}

void obs_encoder_set_audio(obs_encoder_t *encoder, audio_t *audio)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_set_audio"))
		return;
	if (encoder->info.type != OBS_ENCODER_AUDIO) {
		blog(LOG_WARNING,
		     "obs_encoder_set_audio: "
		     "encoder '%s' is not an audio encoder",
		     obs_encoder_get_name(encoder));
		return;
	}
	if (!audio)
		return;

	encoder->media = audio;
	encoder->timebase_num = 1;
	encoder->timebase_den = audio_output_get_sample_rate(audio);
}

video_t *obs_encoder_video(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_video"))
		return NULL;
	if (encoder->info.type != OBS_ENCODER_VIDEO) {
		blog(LOG_WARNING,
		     "obs_encoder_set_video: "
		     "encoder '%s' is not a video encoder",
		     obs_encoder_get_name(encoder));
		return NULL;
	}

	return encoder->media;
}

audio_t *obs_encoder_audio(const obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_audio"))
		return NULL;
	if (encoder->info.type != OBS_ENCODER_AUDIO) {
		blog(LOG_WARNING,
		     "obs_encoder_set_audio: "
		     "encoder '%s' is not an audio encoder",
		     obs_encoder_get_name(encoder));
		return NULL;
	}

	return encoder->media;
}

bool obs_encoder_active(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_active")
		       ? encoder_active(encoder)
		       : false;
}

static inline bool get_sei(const struct obs_encoder *encoder, uint8_t **sei,
			   size_t *size)
{
	if (encoder->info.get_sei_data)
		return encoder->info.get_sei_data(encoder->context.data, sei,
						  size);
	return false;
}

static void send_first_video_packet(struct obs_encoder *encoder,
				    struct encoder_callback *cb,
				    struct encoder_packet *packet)
{
	struct encoder_packet first_packet;
	DARRAY(uint8_t) data;
	uint8_t *sei;
	size_t size;

	/* always wait for first keyframe */
	if (!packet->keyframe)
		return;

	da_init(data);

	if (!get_sei(encoder, &sei, &size) || !sei || !size) {
		cb->new_packet(cb->param, packet);
		cb->sent_first_packet = true;
		return;
	}

	da_push_back_array(data, sei, size);
	da_push_back_array(data, packet->data, packet->size);

	first_packet = *packet;
	first_packet.data = data.array;
	first_packet.size = data.num;

	cb->new_packet(cb->param, &first_packet);
	cb->sent_first_packet = true;

	da_free(data);
}

static inline void send_packet(struct obs_encoder *encoder,
			       struct encoder_callback *cb,
			       struct encoder_packet *packet)
{
	/* include SEI in first video packet */
	if (encoder->info.type == OBS_ENCODER_VIDEO && !cb->sent_first_packet)
		send_first_video_packet(encoder, cb, packet);
	else
		cb->new_packet(cb->param, packet);
}

void full_stop(struct obs_encoder *encoder)
{
	if (encoder) {
		pthread_mutex_lock(&encoder->outputs_mutex);
		for (size_t i = 0; i < encoder->outputs.num; i++) {
			struct obs_output *output = encoder->outputs.array[i];
			obs_output_force_stop(output);

			pthread_mutex_lock(&output->interleaved_mutex);
			output->info.encoded_packet(output->context.data, NULL);
			pthread_mutex_unlock(&output->interleaved_mutex);
		}
		pthread_mutex_unlock(&encoder->outputs_mutex);

		pthread_mutex_lock(&encoder->callbacks_mutex);
		da_free(encoder->callbacks);
		pthread_mutex_unlock(&encoder->callbacks_mutex);

		remove_connection(encoder, false);
		encoder->initialized = false;
	}
}

void send_off_encoder_packet(obs_encoder_t *encoder, bool success,
			     bool received, struct encoder_packet *pkt)
{
	if (!success) {
		blog(LOG_ERROR, "Error encoding with encoder '%s'",
		     encoder->context.name);
		full_stop(encoder);
		return;
	}

	if (received) {
		if (!encoder->first_received) {
			encoder->offset_usec = packet_dts_usec(pkt);
			encoder->first_received = true;
		}

		/* we use system time here to ensure sync with other encoders,
		 * you do not want to use relative timestamps here */
		pkt->dts_usec = encoder->start_ts / 1000 +
				packet_dts_usec(pkt) - encoder->offset_usec;
		pkt->sys_dts_usec = pkt->dts_usec;

		pthread_mutex_lock(&encoder->pause.mutex);
		pkt->sys_dts_usec += encoder->pause.ts_offset / 1000;
		pthread_mutex_unlock(&encoder->pause.mutex);

		pthread_mutex_lock(&encoder->callbacks_mutex);

		for (size_t i = encoder->callbacks.num; i > 0; i--) {
			struct encoder_callback *cb;
			cb = encoder->callbacks.array + (i - 1);
			send_packet(encoder, cb, pkt);
		}

		pthread_mutex_unlock(&encoder->callbacks_mutex);
	}
}

static const char *do_encode_name = "do_encode";
bool do_encode(struct obs_encoder *encoder, struct encoder_frame *frame)
{
	profile_start(do_encode_name);
	if (!encoder->profile_encoder_encode_name)
		encoder->profile_encoder_encode_name =
			profile_store_name(obs_get_profiler_name_store(),
					   "encode(%s)", encoder->context.name);

	struct encoder_packet pkt = {0};
	bool received = false;
	bool success;

	pkt.timebase_num = encoder->timebase_num;
	pkt.timebase_den = encoder->timebase_den;
	pkt.encoder = encoder;

	profile_start(encoder->profile_encoder_encode_name);
	success = encoder->info.encode(encoder->context.data, frame, &pkt,
				       &received);
	profile_end(encoder->profile_encoder_encode_name);
	send_off_encoder_packet(encoder, success, received, &pkt);

	profile_end(do_encode_name);

	return success;
}

static inline bool video_pause_check_internal(struct pause_data *pause,
					      uint64_t ts)
{
	pause->last_video_ts = ts;
	if (!pause->ts_start) {
		return false;
	}

	if (ts == pause->ts_end) {
		pause->ts_start = 0;
		pause->ts_end = 0;

	} else if (ts >= pause->ts_start) {
		return true;
	}

	return false;
}

bool video_pause_check(struct pause_data *pause, uint64_t timestamp)
{
	bool ignore_frame;

	pthread_mutex_lock(&pause->mutex);
	ignore_frame = video_pause_check_internal(pause, timestamp);
	pthread_mutex_unlock(&pause->mutex);

	return ignore_frame;
}

static const char *receive_video_name = "receive_video";
static void receive_video(void *param, struct video_data *frame)
{
	profile_start(receive_video_name);

	struct obs_encoder *encoder = param;
	struct obs_encoder *pair = encoder->paired_encoder;
	struct encoder_frame enc_frame;

	if (!encoder->first_received && pair) {
		if (!pair->first_received ||
		    pair->first_raw_ts > frame->timestamp) {
			goto wait_for_audio;
		}
	}

	if (video_pause_check(&encoder->pause, frame->timestamp))
		goto wait_for_audio;

	memset(&enc_frame, 0, sizeof(struct encoder_frame));

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		enc_frame.data[i] = frame->data[i];
		enc_frame.linesize[i] = frame->linesize[i];
	}

	if (!encoder->start_ts)
		encoder->start_ts = frame->timestamp;

	enc_frame.frames = 1;
	enc_frame.pts = encoder->cur_pts;

	if (do_encode(encoder, &enc_frame))
		encoder->cur_pts += encoder->timebase_num;

wait_for_audio:
	profile_end(receive_video_name);
}

static void clear_audio(struct obs_encoder *encoder)
{
	for (size_t i = 0; i < encoder->planes; i++)
		circlebuf_free(&encoder->audio_input_buffer[i]);
}

static inline void push_back_audio(struct obs_encoder *encoder,
				   struct audio_data *data, size_t size,
				   size_t offset_size)
{
	size -= offset_size;

	/* push in to the circular buffer */
	if (size)
		for (size_t i = 0; i < encoder->planes; i++)
			circlebuf_push_back(&encoder->audio_input_buffer[i],
					    data->data[i] + offset_size, size);
}

static inline size_t calc_offset_size(struct obs_encoder *encoder,
				      uint64_t v_start_ts, uint64_t a_start_ts)
{
	uint64_t offset = v_start_ts - a_start_ts;
	offset = util_mul_div64(offset, encoder->samplerate, 1000000000ULL);
	return (size_t)offset * encoder->blocksize;
}

static void start_from_buffer(struct obs_encoder *encoder, uint64_t v_start_ts)
{
	size_t size = encoder->audio_input_buffer[0].size;
	struct audio_data audio = {0};
	size_t offset_size = 0;

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		audio.data[i] = encoder->audio_input_buffer[i].data;
		memset(&encoder->audio_input_buffer[i], 0,
		       sizeof(struct circlebuf));
	}

	if (encoder->first_raw_ts < v_start_ts)
		offset_size = calc_offset_size(encoder, v_start_ts,
					       encoder->first_raw_ts);

	push_back_audio(encoder, &audio, size, offset_size);

	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		bfree(audio.data[i]);
}

static const char *buffer_audio_name = "buffer_audio";
static bool buffer_audio(struct obs_encoder *encoder, struct audio_data *data)
{
	profile_start(buffer_audio_name);

	size_t size = data->frames * encoder->blocksize;
	size_t offset_size = 0;
	bool success = true;

	if (!encoder->start_ts && encoder->paired_encoder) {
		uint64_t end_ts = data->timestamp;
		uint64_t v_start_ts = encoder->paired_encoder->start_ts;

		/* no video yet, so don't start audio */
		if (!v_start_ts) {
			success = false;
			goto fail;
		}

		/* audio starting point still not synced with video starting
		 * point, so don't start audio */
		end_ts += util_mul_div64(data->frames, 1000000000ULL,
					 encoder->samplerate);
		if (end_ts <= v_start_ts) {
			success = false;
			goto fail;
		}

		/* ready to start audio, truncate if necessary */
		if (data->timestamp < v_start_ts)
			offset_size = calc_offset_size(encoder, v_start_ts,
						       data->timestamp);
		if (data->timestamp <= v_start_ts)
			clear_audio(encoder);

		encoder->start_ts = v_start_ts;

		/* use currently buffered audio instead */
		if (v_start_ts < data->timestamp) {
			start_from_buffer(encoder, v_start_ts);
		}

	} else if (!encoder->start_ts && !encoder->paired_encoder) {
		encoder->start_ts = data->timestamp;
	}

fail:
	push_back_audio(encoder, data, size, offset_size);

	profile_end(buffer_audio_name);
	return success;
}

static bool send_audio_data(struct obs_encoder *encoder)
{
	struct encoder_frame enc_frame;

	memset(&enc_frame, 0, sizeof(struct encoder_frame));

	for (size_t i = 0; i < encoder->planes; i++) {
		circlebuf_pop_front(&encoder->audio_input_buffer[i],
				    encoder->audio_output_buffer[i],
				    encoder->framesize_bytes);

		enc_frame.data[i] = encoder->audio_output_buffer[i];
		enc_frame.linesize[i] = (uint32_t)encoder->framesize_bytes;
	}

	enc_frame.frames = (uint32_t)encoder->framesize;
	enc_frame.pts = encoder->cur_pts;

	if (!do_encode(encoder, &enc_frame))
		return false;

	encoder->cur_pts += encoder->framesize;
	return true;
}

static void pause_audio(struct pause_data *pause, struct audio_data *data,
			size_t sample_rate)
{
	uint64_t cutoff_frames = pause->ts_start - data->timestamp;
	cutoff_frames = ns_to_audio_frames(sample_rate, cutoff_frames);

	data->frames = (uint32_t)cutoff_frames;
}

static void unpause_audio(struct pause_data *pause, struct audio_data *data,
			  size_t sample_rate)
{
	uint64_t cutoff_frames = pause->ts_end - data->timestamp;
	cutoff_frames = ns_to_audio_frames(sample_rate, cutoff_frames);

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		if (!data->data[i])
			break;
		data->data[i] += cutoff_frames * sizeof(float);
	}

	data->timestamp = pause->ts_start;
	data->frames = data->frames - (uint32_t)cutoff_frames;
	pause->ts_start = 0;
	pause->ts_end = 0;
}

static inline bool audio_pause_check_internal(struct pause_data *pause,
					      struct audio_data *data,
					      size_t sample_rate)
{
	uint64_t end_ts;

	if (!pause->ts_start) {
		return false;
	}

	end_ts =
		data->timestamp + audio_frames_to_ns(sample_rate, data->frames);

	if (pause->ts_start >= data->timestamp) {
		if (pause->ts_start <= end_ts) {
			pause_audio(pause, data, sample_rate);
			return !data->frames;
		}

	} else {
		if (pause->ts_end >= data->timestamp &&
		    pause->ts_end <= end_ts) {
			unpause_audio(pause, data, sample_rate);
			return !data->frames;
		}

		return true;
	}

	return false;
}

bool audio_pause_check(struct pause_data *pause, struct audio_data *data,
		       size_t sample_rate)
{
	bool ignore_audio;

	pthread_mutex_lock(&pause->mutex);
	ignore_audio = audio_pause_check_internal(pause, data, sample_rate);
	data->timestamp -= pause->ts_offset;
	pthread_mutex_unlock(&pause->mutex);

	return ignore_audio;
}

static const char *receive_audio_name = "receive_audio";
static void receive_audio(void *param, size_t mix_idx, struct audio_data *in)
{
	profile_start(receive_audio_name);

	struct obs_encoder *encoder = param;
	struct audio_data audio = *in;

	if (!encoder->first_received) {
		encoder->first_raw_ts = audio.timestamp;
		encoder->first_received = true;
		clear_audio(encoder);
	}

	if (audio_pause_check(&encoder->pause, &audio, encoder->samplerate))
		goto end;

	if (!buffer_audio(encoder, &audio))
		goto end;

	while (encoder->audio_input_buffer[0].size >=
	       encoder->framesize_bytes) {
		if (!send_audio_data(encoder)) {
			break;
		}
	}

	UNUSED_PARAMETER(mix_idx);

end:
	profile_end(receive_audio_name);
}

void obs_encoder_add_output(struct obs_encoder *encoder,
			    struct obs_output *output)
{
	if (!encoder)
		return;

	pthread_mutex_lock(&encoder->outputs_mutex);
	da_push_back(encoder->outputs, &output);
	pthread_mutex_unlock(&encoder->outputs_mutex);
}

void obs_encoder_remove_output(struct obs_encoder *encoder,
			       struct obs_output *output)
{
	if (!encoder)
		return;

	pthread_mutex_lock(&encoder->outputs_mutex);
	da_erase_item(encoder->outputs, &output);
	pthread_mutex_unlock(&encoder->outputs_mutex);
}

void obs_encoder_packet_create_instance(struct encoder_packet *dst,
					const struct encoder_packet *src)
{
	long *p_refs;

	*dst = *src;
	p_refs = bmalloc(src->size + sizeof(long));
	dst->data = (void *)(p_refs + 1);
	*p_refs = 1;
	memcpy(dst->data, src->data, src->size);
}

/* OBS_DEPRECATED */
void obs_duplicate_encoder_packet(struct encoder_packet *dst,
				  const struct encoder_packet *src)
{
	obs_encoder_packet_create_instance(dst, src);
}

/* OBS_DEPRECATED */
void obs_free_encoder_packet(struct encoder_packet *packet)
{
	obs_encoder_packet_release(packet);
}

void obs_encoder_packet_ref(struct encoder_packet *dst,
			    struct encoder_packet *src)
{
	if (!src)
		return;

	if (src->data) {
		long *p_refs = ((long *)src->data) - 1;
		os_atomic_inc_long(p_refs);
	}

	*dst = *src;
}

void obs_encoder_packet_release(struct encoder_packet *pkt)
{
	if (!pkt)
		return;

	if (pkt->data) {
		long *p_refs = ((long *)pkt->data) - 1;
		if (os_atomic_dec_long(p_refs) == 0)
			bfree(p_refs);
	}

	memset(pkt, 0, sizeof(struct encoder_packet));
}

void obs_encoder_set_preferred_video_format(obs_encoder_t *encoder,
					    enum video_format format)
{
	if (!encoder || encoder->info.type != OBS_ENCODER_VIDEO)
		return;

	encoder->preferred_format = format;
}

enum video_format
obs_encoder_get_preferred_video_format(const obs_encoder_t *encoder)
{
	if (!encoder || encoder->info.type != OBS_ENCODER_VIDEO)
		return VIDEO_FORMAT_NONE;

	return encoder->preferred_format;
}

void obs_encoder_addref(obs_encoder_t *encoder)
{
	if (!encoder)
		return;

	obs_ref_addref(&encoder->control->ref);
}

void obs_encoder_release(obs_encoder_t *encoder)
{
	if (!encoder)
		return;

	obs_weak_encoder_t *control = encoder->control;
	if (obs_ref_release(&control->ref)) {
		// The order of operations is important here since
		// get_context_by_name in obs.c relies on weak refs
		// being alive while the context is listed
		obs_encoder_destroy(encoder);
		obs_weak_encoder_release(control);
	}
}

void obs_weak_encoder_addref(obs_weak_encoder_t *weak)
{
	if (!weak)
		return;

	obs_weak_ref_addref(&weak->ref);
}

void obs_weak_encoder_release(obs_weak_encoder_t *weak)
{
	if (!weak)
		return;

	if (obs_weak_ref_release(&weak->ref))
		bfree(weak);
}

obs_encoder_t *obs_encoder_get_ref(obs_encoder_t *encoder)
{
	if (!encoder)
		return NULL;

	return obs_weak_encoder_get_encoder(encoder->control);
}

obs_weak_encoder_t *obs_encoder_get_weak_encoder(obs_encoder_t *encoder)
{
	if (!encoder)
		return NULL;

	obs_weak_encoder_t *weak = encoder->control;
	obs_weak_encoder_addref(weak);
	return weak;
}

obs_encoder_t *obs_weak_encoder_get_encoder(obs_weak_encoder_t *weak)
{
	if (!weak)
		return NULL;

	if (obs_weak_ref_get_ref(&weak->ref))
		return weak->encoder;

	return NULL;
}

bool obs_weak_encoder_references_encoder(obs_weak_encoder_t *weak,
					 obs_encoder_t *encoder)
{
	return weak && encoder && weak->encoder == encoder;
}

void *obs_encoder_get_type_data(obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_type_data")
		       ? encoder->orig_info.type_data
		       : NULL;
}

const char *obs_encoder_get_id(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_id")
		       ? encoder->orig_info.id
		       : NULL;
}

uint32_t obs_get_encoder_caps(const char *encoder_id)
{
	struct obs_encoder_info *info = find_encoder(encoder_id);
	return info ? info->caps : 0;
}

uint32_t obs_encoder_get_caps(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_get_caps")
		       ? encoder->orig_info.caps
		       : 0;
}

bool obs_encoder_paused(const obs_encoder_t *encoder)
{
	return obs_encoder_valid(encoder, "obs_encoder_paused")
		       ? os_atomic_load_bool(&encoder->paused)
		       : false;
}

const char *obs_encoder_get_last_error(obs_encoder_t *encoder)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_get_last_error"))
		return NULL;

	return encoder->last_error_message;
}

void obs_encoder_set_last_error(obs_encoder_t *encoder, const char *message)
{
	if (!obs_encoder_valid(encoder, "obs_encoder_set_last_error"))
		return;

	if (encoder->last_error_message)
		bfree(encoder->last_error_message);

	if (message)
		encoder->last_error_message = bstrdup(message);
	else
		encoder->last_error_message = NULL;
}
