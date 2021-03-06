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

#include <inttypes.h>
#include <math.h>

#include "media-io/format-conversion.h"
#include "media-io/video-frame.h"
#include "media-io/audio-io.h"
#include "util/threading.h"
#include "util/platform.h"
#include "util/util_uint64.h"
#include "callback/calldata.h"
#include "graphics/matrix3.h"
#include "graphics/vec3.h"

#include "obs.h"
#include "obs-internal.h"

static bool filter_compatible(obs_source_t *source, obs_source_t *filter);

static inline bool data_valid(const struct obs_source *source, const char *f)
{
	return obs_source_valid(source, f) && source->context.data;
}

static inline bool deinterlacing_enabled(const struct obs_source *source)
{
	return source->deinterlace_mode != OBS_DEINTERLACE_MODE_DISABLE;
}

struct obs_source_info *get_source_info(const char *id)
{
	for (size_t i = 0; i < obs->source_types.num; i++) {
		struct obs_source_info *info = &obs->source_types.array[i];
		if (strcmp(info->id, id) == 0)
			return info;
	}

	return NULL;
}

struct obs_source_info *get_source_info2(const char *unversioned_id,
					 uint32_t ver)
{
	for (size_t i = 0; i < obs->source_types.num; i++) {
		struct obs_source_info *info = &obs->source_types.array[i];
		if (strcmp(info->unversioned_id, unversioned_id) == 0 &&
		    info->version == ver)
			return info;
	}

	return NULL;
}

static const char *source_signals[] = {
	"void destroy(ptr source)",
	"void remove(ptr source)",
	"void save(ptr source)",
	"void load(ptr source)",
	"void activate(ptr source)",
	"void deactivate(ptr source)",
	"void show(ptr source)",
	"void hide(ptr source)",
	"void mute(ptr source, bool muted)",
	"void push_to_mute_changed(ptr source, bool enabled)",
	"void push_to_mute_delay(ptr source, int delay)",
	"void push_to_talk_changed(ptr source, bool enabled)",
	"void push_to_talk_delay(ptr source, int delay)",
	"void enable(ptr source, bool enabled)",
	"void rename(ptr source, string new_name, string prev_name)",
	"void volume(ptr source, in out float volume)",
	"void update_properties(ptr source)",
	"void update_flags(ptr source, int flags)",
	"void audio_sync(ptr source, int out int offset)",
	"void audio_mixers(ptr source, in out int mixers)",
	"void audio_activate(ptr source)",
	"void audio_deactivate(ptr source)",
	"void filter_add(ptr source, ptr filter)",
	"void filter_remove(ptr source, ptr filter)",
	"void reorder_filters(ptr source)",
	"void transition_start(ptr source)",
	"void transition_video_stop(ptr source)",
	"void transition_stop(ptr source)",
	"void media_play(ptr source)",
	"void media_pause(ptr source)",
	"void media_restart(ptr source)",
	"void media_stopped(ptr source)",
	"void media_next(ptr source)",
	"void media_previous(ptr source)",
	"void media_started(ptr source)",
	"void media_ended(ptr source)",
	NULL,
};

bool obs_source_init_context(struct obs_source *source, obs_data_t *settings,
			     const char *name, obs_data_t *hotkey_data,
			     bool private)
{
	if (!obs_context_data_init(&source->context, OBS_OBJ_TYPE_SOURCE,
				   settings, name, hotkey_data, private))
		return false;

	return signal_handler_add_array(source->context.signals,
					source_signals);
}

const char *obs_source_get_display_name(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return (info != NULL) ? info->get_name(info->type_data) : NULL;
}

static void allocate_audio_output_buffer(struct obs_source *source)
{
	size_t size = sizeof(float) * AUDIO_OUTPUT_FRAMES * MAX_AUDIO_CHANNELS *
		      MAX_AUDIO_MIXES;
	float *ptr = bzalloc(size);

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		size_t mix_pos = mix * AUDIO_OUTPUT_FRAMES * MAX_AUDIO_CHANNELS;

		for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
			source->audio_output_buf[mix][i] =
				ptr + mix_pos + AUDIO_OUTPUT_FRAMES * i;
		}
	}
}

static void allocate_audio_mix_buffer(struct obs_source *source)
{
	size_t size = sizeof(float) * AUDIO_OUTPUT_FRAMES * MAX_AUDIO_CHANNELS;
	float *ptr = bzalloc(size);

	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		source->audio_mix_buf[i] = ptr + AUDIO_OUTPUT_FRAMES * i;
	}
}

static inline bool is_async_video_source(const struct obs_source *source)
{
	return (source->info.output_flags & OBS_SOURCE_ASYNC_VIDEO) ==
	       OBS_SOURCE_ASYNC_VIDEO;
}

static inline bool is_audio_source(const struct obs_source *source)
{
	return source->info.output_flags & OBS_SOURCE_AUDIO;
}

static inline bool is_composite_source(const struct obs_source *source)
{
	return source->info.output_flags & OBS_SOURCE_COMPOSITE;
}

extern char *find_libobs_data_file(const char *file);

/* internal initialization */
static bool obs_source_init(struct obs_source *source)
{
	pthread_mutexattr_t attr;

	source->user_volume = 1.0f;
	source->volume = 1.0f;
	source->sync_offset = 0;
	source->balance = 0.5f;
	source->audio_active = true;
	pthread_mutex_init_value(&source->filter_mutex);
	pthread_mutex_init_value(&source->async_mutex);
	pthread_mutex_init_value(&source->audio_mutex);
	pthread_mutex_init_value(&source->audio_buf_mutex);
	pthread_mutex_init_value(&source->audio_cb_mutex);
	pthread_mutex_init_value(&source->caption_cb_mutex);

	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		return false;
	if (pthread_mutex_init(&source->filter_mutex, &attr) != 0)
		return false;
	if (pthread_mutex_init(&source->audio_buf_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&source->audio_actions_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&source->audio_cb_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&source->audio_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&source->async_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&source->caption_cb_mutex, NULL) != 0)
		return false;

	if (is_audio_source(source) || is_composite_source(source))
		allocate_audio_output_buffer(source);
	if (source->info.audio_mix)
		allocate_audio_mix_buffer(source);

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION) {
		if (!obs_transition_init(source))
			return false;
	}

	source->control = bzalloc(sizeof(obs_weak_source_t));
	source->deinterlace_top_first = true;
	source->control->source = source;
	source->audio_mixers = 0xFF;

	source->private_settings = obs_data_create();
	return true;
}

static void obs_source_init_finalize(struct obs_source *source)
{
	if (is_audio_source(source)) {
		pthread_mutex_lock(&obs->data.audio_sources_mutex);

		source->next_audio_source = obs->data.first_audio_source;
		source->prev_next_audio_source = &obs->data.first_audio_source;
		if (obs->data.first_audio_source)
			obs->data.first_audio_source->prev_next_audio_source =
				&source->next_audio_source;
		obs->data.first_audio_source = source;

		pthread_mutex_unlock(&obs->data.audio_sources_mutex);
	}

	obs_context_data_insert(&source->context, &obs->data.sources_mutex,
				&obs->data.first_source);
}

static bool obs_source_hotkey_mute(void *data, obs_hotkey_pair_id id,
				   obs_hotkey_t *key, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);

	struct obs_source *source = data;

	if (!pressed || obs_source_muted(source))
		return false;

	obs_source_set_muted(source, true);
	return true;
}

static bool obs_source_hotkey_unmute(void *data, obs_hotkey_pair_id id,
				     obs_hotkey_t *key, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);

	struct obs_source *source = data;

	if (!pressed || !obs_source_muted(source))
		return false;

	obs_source_set_muted(source, false);
	return true;
}

static void obs_source_hotkey_push_to_mute(void *data, obs_hotkey_id id,
					   obs_hotkey_t *key, bool pressed)
{
	struct audio_action action = {.timestamp = os_gettime_ns(),
				      .type = AUDIO_ACTION_PTM,
				      .set = pressed};

	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);

	struct obs_source *source = data;

	pthread_mutex_lock(&source->audio_actions_mutex);
	da_push_back(source->audio_actions, &action);
	pthread_mutex_unlock(&source->audio_actions_mutex);

	source->user_push_to_mute_pressed = pressed;
}

static void obs_source_hotkey_push_to_talk(void *data, obs_hotkey_id id,
					   obs_hotkey_t *key, bool pressed)
{
	struct audio_action action = {.timestamp = os_gettime_ns(),
				      .type = AUDIO_ACTION_PTT,
				      .set = pressed};

	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);

	struct obs_source *source = data;

	pthread_mutex_lock(&source->audio_actions_mutex);
	da_push_back(source->audio_actions, &action);
	pthread_mutex_unlock(&source->audio_actions_mutex);

	source->user_push_to_talk_pressed = pressed;
}

static void obs_source_init_audio_hotkeys(struct obs_source *source)
{
	if (!(source->info.output_flags & OBS_SOURCE_AUDIO) ||
	    source->info.type != OBS_SOURCE_TYPE_INPUT) {
		source->mute_unmute_key = OBS_INVALID_HOTKEY_ID;
		source->push_to_talk_key = OBS_INVALID_HOTKEY_ID;
		return;
	}

	source->mute_unmute_key = obs_hotkey_pair_register_source(
		source, "libobs.mute", obs->hotkeys.mute, "libobs.unmute",
		obs->hotkeys.unmute, obs_source_hotkey_mute,
		obs_source_hotkey_unmute, source, source);

	source->push_to_mute_key = obs_hotkey_register_source(
		source, "libobs.push-to-mute", obs->hotkeys.push_to_mute,
		obs_source_hotkey_push_to_mute, source);

	source->push_to_talk_key = obs_hotkey_register_source(
		source, "libobs.push-to-talk", obs->hotkeys.push_to_talk,
		obs_source_hotkey_push_to_talk, source);
}

static obs_source_t *
obs_source_create_internal(const char *id, const char *name,
			   obs_data_t *settings, obs_data_t *hotkey_data,
			   bool private, uint32_t last_obs_ver)
{
	struct obs_source *source = bzalloc(sizeof(struct obs_source));

	const struct obs_source_info *info = get_source_info(id);
	if (!info) {
		blog(LOG_ERROR, "Source ID '%s' not found", id);

		source->info.id = bstrdup(id);
		source->owns_info_id = true;
		source->info.unversioned_id = bstrdup(source->info.id);
	} else {
		source->info = *info;

		/* Always mark filters as private so they aren't found by
		 * source enum/search functions.
		 *
		 * XXX: Fix design flaws with filters */
		if (info->type == OBS_SOURCE_TYPE_FILTER)
		private
		= true;
	}

	source->mute_unmute_key = OBS_INVALID_HOTKEY_PAIR_ID;
	source->push_to_mute_key = OBS_INVALID_HOTKEY_ID;
	source->push_to_talk_key = OBS_INVALID_HOTKEY_ID;
	source->last_obs_ver = last_obs_ver;

	if (!obs_source_init_context(source, settings, name, hotkey_data,
				     private))
		goto fail;

	if (info) {
		if (info->get_defaults) {
			info->get_defaults(source->context.settings);
		}
		if (info->get_defaults2) {
			info->get_defaults2(info->type_data,
					    source->context.settings);
		}
	}

	if (!obs_source_init(source))
		goto fail;

	if (!private)
		obs_source_init_audio_hotkeys(source);

	/* allow the source to be created even if creation fails so that the
	 * user's data doesn't become lost */
	if (info && info->create)
		source->context.data =
			info->create(source->context.settings, source);
	if ((!info || info->create) && !source->context.data)
		blog(LOG_ERROR, "Failed to create source '%s'!", name);

	blog(LOG_DEBUG, "%ssource '%s' (%s) created", private ? "private " : "",
	     name, id);

	source->flags = source->default_flags;
	source->enabled = true;

	if (!private) {
		obs_source_dosignal(source, "source_create", NULL);
	}

	obs_source_init_finalize(source);
	return source;

fail:
	blog(LOG_ERROR, "obs_source_create failed");
	obs_source_destroy(source);
	return NULL;
}

obs_source_t *obs_source_create(const char *id, const char *name,
				obs_data_t *settings, obs_data_t *hotkey_data)
{
	return obs_source_create_internal(id, name, settings, hotkey_data,
					  false, LIBOBS_API_VER);
}

obs_source_t *obs_source_create_private(const char *id, const char *name,
					obs_data_t *settings)
{
	return obs_source_create_internal(id, name, settings, NULL, true,
					  LIBOBS_API_VER);
}

obs_source_t *obs_source_create_set_last_ver(const char *id, const char *name,
					     obs_data_t *settings,
					     obs_data_t *hotkey_data,
					     uint32_t last_obs_ver)
{
	return obs_source_create_internal(id, name, settings, hotkey_data,
					  false, last_obs_ver);
}

static char *get_new_filter_name(obs_source_t *dst, const char *name)
{
	struct dstr new_name = {0};
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		obs_source_t *existing_filter =
			obs_source_get_filter_by_name(dst, new_name.array);
		if (!existing_filter)
			break;

		obs_source_release(existing_filter);

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}

static void duplicate_filters(obs_source_t *dst, obs_source_t *src,
			      bool private)
{
	DARRAY(obs_source_t *) filters;

	da_init(filters);

	pthread_mutex_lock(&src->filter_mutex);
	for (size_t i = 0; i < src->filters.num; i++)
		obs_source_addref(src->filters.array[i]);
	da_copy(filters, src->filters);
	pthread_mutex_unlock(&src->filter_mutex);

	for (size_t i = filters.num; i > 0; i--) {
		obs_source_t *src_filter = filters.array[i - 1];
		char *new_name =
			get_new_filter_name(dst, src_filter->context.name);
		bool enabled = obs_source_enabled(src_filter);

		obs_source_t *dst_filter =
			obs_source_duplicate(src_filter, new_name, private);
		obs_source_set_enabled(dst_filter, enabled);

		bfree(new_name);
		obs_source_filter_add(dst, dst_filter);
		obs_source_release(dst_filter);
		obs_source_release(src_filter);
	}

	da_free(filters);
}

void obs_source_copy_filters(obs_source_t *dst, obs_source_t *src)
{
	if (!obs_source_valid(dst, "obs_source_copy_filters"))
		return;
	if (!obs_source_valid(src, "obs_source_copy_filters"))
		return;

	duplicate_filters(dst, src, dst->context.private);
}

static void duplicate_filter(obs_source_t *dst, obs_source_t *filter)
{
	if (!filter_compatible(dst, filter))
		return;

	char *new_name = get_new_filter_name(dst, filter->context.name);
	bool enabled = obs_source_enabled(filter);

	obs_source_t *dst_filter = obs_source_duplicate(filter, new_name, true);
	obs_source_set_enabled(dst_filter, enabled);

	bfree(new_name);
	obs_source_filter_add(dst, dst_filter);
	obs_source_release(dst_filter);
}

void obs_source_copy_single_filter(obs_source_t *dst, obs_source_t *filter)
{
	if (!obs_source_valid(dst, "obs_source_copy_single_filter"))
		return;
	if (!obs_source_valid(filter, "obs_source_copy_single_filter"))
		return;

	duplicate_filter(dst, filter);
}

obs_source_t *obs_source_duplicate(obs_source_t *source, const char *new_name,
				   bool create_private)
{
	obs_source_t *new_source;
	obs_data_t *settings;

	if (!obs_source_valid(source, "obs_source_duplicate"))
		return NULL;

	if (source->info.type == OBS_SOURCE_TYPE_SCENE) {
		obs_scene_t *scene = obs_scene_from_source(source);
		if (scene && !create_private) {
			obs_source_addref(source);
			return source;
		}
		if (!scene)
			scene = obs_group_from_source(source);
		if (!scene)
			return NULL;

		obs_scene_t *new_scene = obs_scene_duplicate(
			scene, new_name,
			create_private ? OBS_SCENE_DUP_PRIVATE_COPY
				       : OBS_SCENE_DUP_COPY);
		obs_source_t *new_source = obs_scene_get_source(new_scene);
		return new_source;
	}

	if ((source->info.output_flags & OBS_SOURCE_DO_NOT_DUPLICATE) != 0) {
		obs_source_addref(source);
		return source;
	}

	settings = obs_data_create();
	obs_data_apply(settings, source->context.settings);

	new_source = create_private
			     ? obs_source_create_private(source->info.id,
							 new_name, settings)
			     : obs_source_create(source->info.id, new_name,
						 settings, NULL);

	new_source->audio_mixers = source->audio_mixers;
	new_source->sync_offset = source->sync_offset;
	new_source->user_volume = source->user_volume;
	new_source->user_muted = source->user_muted;
	new_source->volume = source->volume;
	new_source->muted = source->muted;
	new_source->flags = source->flags;

	obs_data_apply(new_source->private_settings, source->private_settings);

	if (source->info.type != OBS_SOURCE_TYPE_FILTER)
		duplicate_filters(new_source, source, create_private);

	obs_data_release(settings);
	return new_source;
}

void obs_source_frame_init(struct obs_source_frame *frame,
			   enum video_format format, uint32_t width,
			   uint32_t height)
{
	struct video_frame vid_frame;

	if (!obs_ptr_valid(frame, "obs_source_frame_init"))
		return;

	video_frame_init(&vid_frame, format, width, height);
	frame->format = format;
	frame->width = width;
	frame->height = height;

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		frame->data[i] = vid_frame.data[i];
		frame->linesize[i] = vid_frame.linesize[i];
	}
}

static inline void obs_source_frame_decref(struct obs_source_frame *frame)
{
	if (os_atomic_dec_long(&frame->refs) == 0)
		obs_source_frame_destroy(frame);
}

static bool obs_source_filter_remove_refless(obs_source_t *source,
					     obs_source_t *filter);

void obs_source_destroy(struct obs_source *source)
{
	size_t i;

	if (!obs_source_valid(source, "obs_source_destroy"))
		return;

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_clear(source);

	pthread_mutex_lock(&obs->data.audio_sources_mutex);
	if (source->prev_next_audio_source) {
		*source->prev_next_audio_source = source->next_audio_source;
		if (source->next_audio_source)
			source->next_audio_source->prev_next_audio_source =
				source->prev_next_audio_source;
	}
	pthread_mutex_unlock(&obs->data.audio_sources_mutex);

	if (source->filter_parent)
		obs_source_filter_remove_refless(source->filter_parent, source);

	while (source->filters.num)
		obs_source_filter_remove(source, source->filters.array[0]);

	obs_context_data_remove(&source->context);

	blog(LOG_DEBUG, "%ssource '%s' destroyed",
	     source->context.private ? "private " : "", source->context.name);

	obs_source_dosignal(source, "source_destroy", "destroy");

	if (source->context.data) {
		source->info.destroy(source->context.data);
		source->context.data = NULL;
	}

	audio_monitor_destroy(source->monitor);

	obs_hotkey_unregister(source->push_to_talk_key);
	obs_hotkey_unregister(source->push_to_mute_key);
	obs_hotkey_pair_unregister(source->mute_unmute_key);

	for (i = 0; i < source->async_cache.num; i++)
		obs_source_frame_decref(source->async_cache.array[i].frame);

	gs_enter_context(obs->video.graphics);
	if (source->async_texrender)
		gs_texrender_destroy(source->async_texrender);
	if (source->async_prev_texrender)
		gs_texrender_destroy(source->async_prev_texrender);
	for (size_t c = 0; c < MAX_AV_PLANES; c++) {
		gs_texture_destroy(source->async_textures[c]);
		gs_texture_destroy(source->async_prev_textures[c]);
	}
	if (source->filter_texrender)
		gs_texrender_destroy(source->filter_texrender);
	gs_leave_context();

	for (i = 0; i < MAX_AV_PLANES; i++)
		bfree(source->audio_data.data[i]);
	for (i = 0; i < MAX_AUDIO_CHANNELS; i++)
		circlebuf_free(&source->audio_input_buf[i]);
	audio_resampler_destroy(source->resampler);
	bfree(source->audio_output_buf[0][0]);
	bfree(source->audio_mix_buf[0]);

	obs_source_frame_destroy(source->async_preload_frame);

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_free(source);

	da_free(source->audio_actions);
	da_free(source->audio_cb_list);
	da_free(source->caption_cb_list);
	da_free(source->async_cache);
	da_free(source->async_frames);
	da_free(source->filters);
	pthread_mutex_destroy(&source->filter_mutex);
	pthread_mutex_destroy(&source->audio_actions_mutex);
	pthread_mutex_destroy(&source->audio_buf_mutex);
	pthread_mutex_destroy(&source->audio_cb_mutex);
	pthread_mutex_destroy(&source->audio_mutex);
	pthread_mutex_destroy(&source->caption_cb_mutex);
	pthread_mutex_destroy(&source->async_mutex);
	obs_data_release(source->private_settings);
	obs_context_data_free(&source->context);

	if (source->owns_info_id) {
		bfree((void *)source->info.id);
		bfree((void *)source->info.unversioned_id);
	}

	bfree(source);
}

void obs_source_addref(obs_source_t *source)
{
	if (!source)
		return;

	obs_ref_addref(&source->control->ref);
}

void obs_source_release(obs_source_t *source)
{
	if (!obs) {
		blog(LOG_WARNING, "Tried to release a source when the OBS "
				  "core is shut down!");
		return;
	}

	if (!source)
		return;

	obs_weak_source_t *control = source->control;
	if (obs_ref_release(&control->ref)) {
		obs_source_destroy(source);
		obs_weak_source_release(control);
	}
}

void obs_weak_source_addref(obs_weak_source_t *weak)
{
	if (!weak)
		return;

	obs_weak_ref_addref(&weak->ref);
}

void obs_weak_source_release(obs_weak_source_t *weak)
{
	if (!weak)
		return;

	if (obs_weak_ref_release(&weak->ref))
		bfree(weak);
}

obs_source_t *obs_source_get_ref(obs_source_t *source)
{
	if (!source)
		return NULL;

	return obs_weak_source_get_source(source->control);
}

obs_weak_source_t *obs_source_get_weak_source(obs_source_t *source)
{
	if (!source)
		return NULL;

	obs_weak_source_t *weak = source->control;
	obs_weak_source_addref(weak);
	return weak;
}

obs_source_t *obs_weak_source_get_source(obs_weak_source_t *weak)
{
	if (!weak)
		return NULL;

	if (obs_weak_ref_get_ref(&weak->ref))
		return weak->source;

	return NULL;
}

bool obs_weak_source_references_source(obs_weak_source_t *weak,
				       obs_source_t *source)
{
	return weak && source && weak->source == source;
}

void obs_source_remove(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_remove"))
		return;

	if (!source->removed) {
		source->removed = true;
		obs_source_dosignal(source, "source_remove", "remove");
	}
}

bool obs_source_removed(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_removed") ? source->removed
							      : true;
}

static inline obs_data_t *get_defaults(const struct obs_source_info *info)
{
	obs_data_t *settings = obs_data_create();
	if (info->get_defaults2)
		info->get_defaults2(info->type_data, settings);
	else if (info->get_defaults)
		info->get_defaults(settings);
	return settings;
}

obs_data_t *obs_source_settings(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return (info) ? get_defaults(info) : NULL;
}

obs_data_t *obs_get_source_defaults(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return info ? get_defaults(info) : NULL;
}

obs_properties_t *obs_get_source_properties(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	if (info && (info->get_properties || info->get_properties2)) {
		obs_data_t *defaults = get_defaults(info);
		obs_properties_t *props;

		if (info->get_properties2)
			props = info->get_properties2(NULL, info->type_data);
		else
			props = info->get_properties(NULL);

		obs_properties_apply_settings(props, defaults);
		obs_data_release(defaults);
		return props;
	}
	return NULL;
}

obs_missing_files_t *obs_source_get_missing_files(const obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_get_missing_files"))
		return obs_missing_files_create();

	if (source->info.missing_files) {
		return source->info.missing_files(source->context.data);
	}

	return obs_missing_files_create();
}

void obs_source_replace_missing_file(obs_missing_file_cb cb,
				     obs_source_t *source, const char *new_path,
				     void *data)
{
	if (!obs_source_valid(source, "obs_source_replace_missing_file"))
		return;

	cb(source->context.data, new_path, data);
}

bool obs_is_source_configurable(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return info && (info->get_properties || info->get_properties2);
}

bool obs_source_configurable(const obs_source_t *source)
{
	return data_valid(source, "obs_source_configurable") &&
	       (source->info.get_properties || source->info.get_properties2);
}

obs_properties_t *obs_source_properties(const obs_source_t *source)
{
	if (!data_valid(source, "obs_source_properties"))
		return NULL;

	if (source->info.get_properties2) {
		obs_properties_t *props;
		props = source->info.get_properties2(source->context.data,
						     source->info.type_data);
		obs_properties_apply_settings(props, source->context.settings);
		return props;

	} else if (source->info.get_properties) {
		obs_properties_t *props;
		props = source->info.get_properties(source->context.data);
		obs_properties_apply_settings(props, source->context.settings);
		return props;
	}

	return NULL;
}

uint32_t obs_source_get_output_flags(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_output_flags")
		       ? source->info.output_flags
		       : 0;
}

uint32_t obs_get_source_output_flags(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return info ? info->output_flags : 0;
}

static void obs_source_deferred_update(obs_source_t *source)
{
	if (source->context.data && source->info.update) {
		long count = os_atomic_load_long(&source->defer_update_count);
		source->info.update(source->context.data,
				    source->context.settings);
		os_atomic_compare_swap_long(&source->defer_update_count, count,
					    0);
	}
}

void obs_source_update(obs_source_t *source, obs_data_t *settings)
{
	if (!obs_source_valid(source, "obs_source_update"))
		return;

	if (settings)
		obs_data_apply(source->context.settings, settings);

	if (source->info.output_flags & OBS_SOURCE_VIDEO) {
		os_atomic_inc_long(&source->defer_update_count);
	} else if (source->context.data && source->info.update) {
		source->info.update(source->context.data,
				    source->context.settings);
	}
}

void obs_source_update_properties(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_update_properties"))
		return;

	obs_source_dosignal(source, NULL, "update_properties");
}

void obs_source_send_mouse_click(obs_source_t *source,
				 const struct obs_mouse_event *event,
				 int32_t type, bool mouse_up,
				 uint32_t click_count)
{
	if (!obs_source_valid(source, "obs_source_send_mouse_click"))
		return;

	if (source->info.output_flags & OBS_SOURCE_INTERACTION) {
		if (source->info.mouse_click) {
			source->info.mouse_click(source->context.data, event,
						 type, mouse_up, click_count);
		}
	}
}

void obs_source_send_mouse_move(obs_source_t *source,
				const struct obs_mouse_event *event,
				bool mouse_leave)
{
	if (!obs_source_valid(source, "obs_source_send_mouse_move"))
		return;

	if (source->info.output_flags & OBS_SOURCE_INTERACTION) {
		if (source->info.mouse_move) {
			source->info.mouse_move(source->context.data, event,
						mouse_leave);
		}
	}
}

void obs_source_send_mouse_wheel(obs_source_t *source,
				 const struct obs_mouse_event *event,
				 int x_delta, int y_delta)
{
	if (!obs_source_valid(source, "obs_source_send_mouse_wheel"))
		return;

	if (source->info.output_flags & OBS_SOURCE_INTERACTION) {
		if (source->info.mouse_wheel) {
			source->info.mouse_wheel(source->context.data, event,
						 x_delta, y_delta);
		}
	}
}

void obs_source_send_focus(obs_source_t *source, bool focus)
{
	if (!obs_source_valid(source, "obs_source_send_focus"))
		return;

	if (source->info.output_flags & OBS_SOURCE_INTERACTION) {
		if (source->info.focus) {
			source->info.focus(source->context.data, focus);
		}
	}
}

void obs_source_send_key_click(obs_source_t *source,
			       const struct obs_key_event *event, bool key_up)
{
	if (!obs_source_valid(source, "obs_source_send_key_click"))
		return;

	if (source->info.output_flags & OBS_SOURCE_INTERACTION) {
		if (source->info.key_click) {
			source->info.key_click(source->context.data, event,
					       key_up);
		}
	}
}

static void activate_source(obs_source_t *source)
{
	if (source->context.data && source->info.activate)
		source->info.activate(source->context.data);
	obs_source_dosignal(source, "source_activate", "activate");
}

static void deactivate_source(obs_source_t *source)
{
	if (source->context.data && source->info.deactivate)
		source->info.deactivate(source->context.data);
	obs_source_dosignal(source, "source_deactivate", "deactivate");
}

static void show_source(obs_source_t *source)
{
	if (source->context.data && source->info.show)
		source->info.show(source->context.data);
	obs_source_dosignal(source, "source_show", "show");
}

static void hide_source(obs_source_t *source)
{
	if (source->context.data && source->info.hide)
		source->info.hide(source->context.data);
	obs_source_dosignal(source, "source_hide", "hide");
}

static void activate_tree(obs_source_t *parent, obs_source_t *child,
			  void *param)
{
	os_atomic_inc_long(&child->activate_refs);

	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(param);
}

static void deactivate_tree(obs_source_t *parent, obs_source_t *child,
			    void *param)
{
	os_atomic_dec_long(&child->activate_refs);

	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(param);
}

static void show_tree(obs_source_t *parent, obs_source_t *child, void *param)
{
	os_atomic_inc_long(&child->show_refs);

	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(param);
}

static void hide_tree(obs_source_t *parent, obs_source_t *child, void *param)
{
	os_atomic_dec_long(&child->show_refs);

	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(param);
}

void obs_source_activate(obs_source_t *source, enum view_type type)
{
	if (!obs_source_valid(source, "obs_source_activate"))
		return;

	os_atomic_inc_long(&source->show_refs);
	obs_source_enum_active_tree(source, show_tree, NULL);

	if (type == MAIN_VIEW) {
		os_atomic_inc_long(&source->activate_refs);
		obs_source_enum_active_tree(source, activate_tree, NULL);
	}
}

void obs_source_deactivate(obs_source_t *source, enum view_type type)
{
	if (!obs_source_valid(source, "obs_source_deactivate"))
		return;

	if (os_atomic_load_long(&source->show_refs) > 0) {
		os_atomic_dec_long(&source->show_refs);
		obs_source_enum_active_tree(source, hide_tree, NULL);
	}

	if (type == MAIN_VIEW) {
		if (os_atomic_load_long(&source->activate_refs) > 0) {
			os_atomic_dec_long(&source->activate_refs);
			obs_source_enum_active_tree(source, deactivate_tree,
						    NULL);
		}
	}
}

static inline struct obs_source_frame *get_closest_frame(obs_source_t *source,
							 uint64_t sys_time);
bool set_async_texture_size(struct obs_source *source,
			    const struct obs_source_frame *frame);

static void async_tick(obs_source_t *source)
{
	uint64_t sys_time = obs->video.video_time;

	pthread_mutex_lock(&source->async_mutex);

	if (deinterlacing_enabled(source)) {
		deinterlace_process_last_frame(source, sys_time);
	} else {
		if (source->cur_async_frame) {
			remove_async_frame(source, source->cur_async_frame);
			source->cur_async_frame = NULL;
		}

		source->cur_async_frame = get_closest_frame(source, sys_time);
	}

	source->last_sys_timestamp = sys_time;
	pthread_mutex_unlock(&source->async_mutex);

	if (source->cur_async_frame)
		source->async_update_texture =
			set_async_texture_size(source, source->cur_async_frame);
}

void obs_source_video_tick(obs_source_t *source, float seconds)
{
	bool now_showing, now_active;

	if (!obs_source_valid(source, "obs_source_video_tick"))
		return;

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_tick(source, seconds);

	if ((source->info.output_flags & OBS_SOURCE_ASYNC) != 0)
		async_tick(source);

	if (os_atomic_load_long(&source->defer_update_count) > 0)
		obs_source_deferred_update(source);

	/* reset the filter render texture information once every frame */
	if (source->filter_texrender)
		gs_texrender_reset(source->filter_texrender);

	/* call show/hide if the reference changed */
	now_showing = !!source->show_refs;
	if (now_showing != source->showing) {
		if (now_showing) {
			show_source(source);
		} else {
			hide_source(source);
		}

		if (source->filters.num) {
			for (size_t i = source->filters.num; i > 0; i--) {
				obs_source_t *filter =
					source->filters.array[i - 1];
				if (now_showing) {
					show_source(filter);
				} else {
					hide_source(filter);
				}
			}
		}

		source->showing = now_showing;
	}

	/* call activate/deactivate if the reference changed */
	now_active = !!source->activate_refs;
	if (now_active != source->active) {
		if (now_active) {
			activate_source(source);
		} else {
			deactivate_source(source);
		}

		if (source->filters.num) {
			for (size_t i = source->filters.num; i > 0; i--) {
				obs_source_t *filter =
					source->filters.array[i - 1];
				if (now_active) {
					activate_source(filter);
				} else {
					deactivate_source(filter);
				}
			}
		}

		source->active = now_active;
	}

	if (source->context.data && source->info.video_tick)
		source->info.video_tick(source->context.data, seconds);

	source->async_rendered = false;
	source->deinterlace_rendered = false;
}

/* unless the value is 3+ hours worth of frames, this won't overflow */
static inline uint64_t conv_frames_to_time(const size_t sample_rate,
					   const size_t frames)
{
	if (!sample_rate)
		return 0;

	return util_mul_div64(frames, 1000000000ULL, sample_rate);
}

static inline size_t conv_time_to_frames(const size_t sample_rate,
					 const uint64_t duration)
{
	return (size_t)util_mul_div64(duration, sample_rate, 1000000000ULL);
}

/* maximum buffer size */
#define MAX_BUF_SIZE (1000 * AUDIO_OUTPUT_FRAMES * sizeof(float))

/* time threshold in nanoseconds to ensure audio timing is as seamless as
 * possible */
#define TS_SMOOTHING_THRESHOLD 70000000ULL

static inline void reset_audio_timing(obs_source_t *source, uint64_t timestamp,
				      uint64_t os_time)
{
	source->timing_set = true;
	source->timing_adjust = os_time - timestamp;
}

static void reset_audio_data(obs_source_t *source, uint64_t os_time)
{
	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (source->audio_input_buf[i].size)
			circlebuf_pop_front(&source->audio_input_buf[i], NULL,
					    source->audio_input_buf[i].size);
	}

	source->last_audio_input_buf_size = 0;
	source->audio_ts = os_time;
	source->next_audio_sys_ts_min = os_time;
}

static void handle_ts_jump(obs_source_t *source, uint64_t expected, uint64_t ts,
			   uint64_t diff, uint64_t os_time)
{
	blog(LOG_DEBUG,
	     "Timestamp for source '%s' jumped by '%" PRIu64 "', "
	     "expected value %" PRIu64 ", input value %" PRIu64,
	     source->context.name, diff, expected, ts);

	pthread_mutex_lock(&source->audio_buf_mutex);
	reset_audio_timing(source, ts, os_time);
	reset_audio_data(source, os_time);
	pthread_mutex_unlock(&source->audio_buf_mutex);
}

static void source_signal_audio_data(obs_source_t *source,
				     const struct audio_data *in, bool muted)
{
	pthread_mutex_lock(&source->audio_cb_mutex);

	for (size_t i = source->audio_cb_list.num; i > 0; i--) {
		struct audio_cb_info info = source->audio_cb_list.array[i - 1];
		info.callback(info.param, source, in, muted);
	}

	pthread_mutex_unlock(&source->audio_cb_mutex);
}

static inline uint64_t uint64_diff(uint64_t ts1, uint64_t ts2)
{
	return (ts1 < ts2) ? (ts2 - ts1) : (ts1 - ts2);
}

static inline size_t get_buf_placement(audio_t *audio, uint64_t offset)
{
	uint32_t sample_rate = audio_output_get_sample_rate(audio);
	return (size_t)util_mul_div64(offset, sample_rate, 1000000000ULL);
}

static void source_output_audio_place(obs_source_t *source,
				      const struct audio_data *in)
{
	audio_t *audio = obs->audio.audio;
	size_t buf_placement;
	size_t channels = audio_output_get_channels(audio);
	size_t size = in->frames * sizeof(float);

	if (!source->audio_ts || in->timestamp < source->audio_ts)
		reset_audio_data(source, in->timestamp);

	buf_placement =
		get_buf_placement(audio, in->timestamp - source->audio_ts) *
		sizeof(float);

#if DEBUG_AUDIO == 1
	blog(LOG_DEBUG,
	     "frames: %lu, size: %lu, placement: %lu, base_ts: %llu, ts: %llu",
	     (unsigned long)in->frames,
	     (unsigned long)source->audio_input_buf[0].size,
	     (unsigned long)buf_placement, source->audio_ts, in->timestamp);
#endif

	/* do not allow the circular buffers to become too big */
	if ((buf_placement + size) > MAX_BUF_SIZE)
		return;

	for (size_t i = 0; i < channels; i++) {
		circlebuf_place(&source->audio_input_buf[i], buf_placement,
				in->data[i], size);
		circlebuf_pop_back(&source->audio_input_buf[i], NULL,
				   source->audio_input_buf[i].size -
					   (buf_placement + size));
	}

	source->last_audio_input_buf_size = 0;
}

static inline void source_output_audio_push_back(obs_source_t *source,
						 const struct audio_data *in)
{
	audio_t *audio = obs->audio.audio;
	size_t channels = audio_output_get_channels(audio);
	size_t size = in->frames * sizeof(float);

	/* do not allow the circular buffers to become too big */
	if ((source->audio_input_buf[0].size + size) > MAX_BUF_SIZE)
		return;

	for (size_t i = 0; i < channels; i++)
		circlebuf_push_back(&source->audio_input_buf[i], in->data[i],
				    size);

	/* reset audio input buffer size to ensure that audio doesn't get
	 * perpetually cut */
	source->last_audio_input_buf_size = 0;
}

static inline bool source_muted(obs_source_t *source, uint64_t os_time)
{
	if (source->push_to_mute_enabled && source->user_push_to_mute_pressed)
		source->push_to_mute_stop_time =
			os_time + source->push_to_mute_delay * 1000000;

	if (source->push_to_talk_enabled && source->user_push_to_talk_pressed)
		source->push_to_talk_stop_time =
			os_time + source->push_to_talk_delay * 1000000;

	bool push_to_mute_active = source->user_push_to_mute_pressed ||
				   os_time < source->push_to_mute_stop_time;
	bool push_to_talk_active = source->user_push_to_talk_pressed ||
				   os_time < source->push_to_talk_stop_time;

	return !source->enabled || source->user_muted ||
	       (source->push_to_mute_enabled && push_to_mute_active) ||
	       (source->push_to_talk_enabled && !push_to_talk_active);
}

static void source_output_audio_data(obs_source_t *source,
				     const struct audio_data *data)
{
	size_t sample_rate = audio_output_get_sample_rate(obs->audio.audio);
	struct audio_data in = *data;
	uint64_t diff;
	uint64_t os_time = os_gettime_ns();
	int64_t sync_offset;
	bool using_direct_ts = false;
	bool push_back = false;

	/* detects 'directly' set timestamps as long as they're within
	 * a certain threshold */
	if (uint64_diff(in.timestamp, os_time) < MAX_TS_VAR) {
		source->timing_adjust = 0;
		source->timing_set = true;
		using_direct_ts = true;
	}

	if (!source->timing_set) {
		reset_audio_timing(source, in.timestamp, os_time);

	} else if (source->next_audio_ts_min != 0) {
		diff = uint64_diff(source->next_audio_ts_min, in.timestamp);

		/* smooth audio if within threshold */
		if (diff > MAX_TS_VAR && !using_direct_ts)
			handle_ts_jump(source, source->next_audio_ts_min,
				       in.timestamp, diff, os_time);
		else if (diff < TS_SMOOTHING_THRESHOLD) {
			if (source->async_unbuffered && source->async_decoupled)
				source->timing_adjust = os_time - in.timestamp;
			in.timestamp = source->next_audio_ts_min;
		}
	}

	source->last_audio_ts = in.timestamp;
	source->next_audio_ts_min =
		in.timestamp + conv_frames_to_time(sample_rate, in.frames);

	in.timestamp += source->timing_adjust;

	pthread_mutex_lock(&source->audio_buf_mutex);

	if (source->next_audio_sys_ts_min == in.timestamp) {
		push_back = true;

	} else if (source->next_audio_sys_ts_min) {
		diff = uint64_diff(source->next_audio_sys_ts_min, in.timestamp);

		if (diff < TS_SMOOTHING_THRESHOLD) {
			push_back = true;

			/* This typically only happens if used with async video when
		 * audio/video start transitioning in to a timestamp jump.
		 * Audio will typically have a timestamp jump, and then video
		 * will have a timestamp jump.  If that case is encountered,
		 * just clear the audio data in that small window and force a
		 * resync.  This handles all cases rather than just looping. */
		} else if (diff > MAX_TS_VAR) {
			reset_audio_timing(source, data->timestamp, os_time);
			in.timestamp = data->timestamp + source->timing_adjust;
		}
	}

	sync_offset = source->sync_offset;
	in.timestamp += sync_offset;
	in.timestamp -= source->resample_offset;

	source->next_audio_sys_ts_min =
		source->next_audio_ts_min + source->timing_adjust;

	if (source->last_sync_offset != sync_offset) {
		if (source->last_sync_offset)
			push_back = false;
		source->last_sync_offset = sync_offset;
	}

	if (source->monitoring_type != OBS_MONITORING_TYPE_MONITOR_ONLY) {
		if (push_back && source->audio_ts)
			source_output_audio_push_back(source, &in);
		else
			source_output_audio_place(source, &in);
	}

	pthread_mutex_unlock(&source->audio_buf_mutex);

	source_signal_audio_data(source, data, source_muted(source, os_time));
}

enum convert_type {
	CONVERT_NONE,
	CONVERT_NV12,
	CONVERT_420,
	CONVERT_420_A,
	CONVERT_422,
	CONVERT_422_A,
	CONVERT_422_PACK,
	CONVERT_444,
	CONVERT_444_A,
	CONVERT_444_A_PACK,
	CONVERT_800,
	CONVERT_RGB_LIMITED,
	CONVERT_BGR3,
};

static inline enum convert_type get_convert_type(enum video_format format,
						 bool full_range)
{
	switch (format) {
	case VIDEO_FORMAT_I420:
		return CONVERT_420;
	case VIDEO_FORMAT_NV12:
		return CONVERT_NV12;
	case VIDEO_FORMAT_I444:
		return CONVERT_444;
	case VIDEO_FORMAT_I422:
		return CONVERT_422;

	case VIDEO_FORMAT_YVYU:
	case VIDEO_FORMAT_YUY2:
	case VIDEO_FORMAT_UYVY:
		return CONVERT_422_PACK;

	case VIDEO_FORMAT_Y800:
		return CONVERT_800;

	case VIDEO_FORMAT_NONE:
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_BGRX:
		return full_range ? CONVERT_NONE : CONVERT_RGB_LIMITED;

	case VIDEO_FORMAT_BGR3:
		return CONVERT_BGR3;

	case VIDEO_FORMAT_I40A:
		return CONVERT_420_A;

	case VIDEO_FORMAT_I42A:
		return CONVERT_422_A;

	case VIDEO_FORMAT_YUVA:
		return CONVERT_444_A;

	case VIDEO_FORMAT_AYUV:
		return CONVERT_444_A_PACK;
	}

	return CONVERT_NONE;
}

static inline bool set_packed422_sizes(struct obs_source *source,
				       const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width / 2;
	source->async_convert_height[0] = frame->height;
	source->async_texture_formats[0] = GS_BGRA;
	source->async_channel_count = 1;
	return true;
}

static inline bool
set_packed444_alpha_sizes(struct obs_source *source,
			  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_texture_formats[0] = GS_BGRA;
	source->async_channel_count = 1;
	return true;
}

static inline bool set_planar444_sizes(struct obs_source *source,
				       const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width;
	source->async_convert_width[2] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height;
	source->async_convert_height[2] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_channel_count = 3;
	return true;
}

static inline bool
set_planar444_alpha_sizes(struct obs_source *source,
			  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width;
	source->async_convert_width[2] = frame->width;
	source->async_convert_width[3] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height;
	source->async_convert_height[2] = frame->height;
	source->async_convert_height[3] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_texture_formats[3] = GS_R8;
	source->async_channel_count = 4;
	return true;
}

static inline bool set_planar420_sizes(struct obs_source *source,
				       const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width / 2;
	source->async_convert_width[2] = frame->width / 2;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height / 2;
	source->async_convert_height[2] = frame->height / 2;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_channel_count = 3;
	return true;
}

static inline bool
set_planar420_alpha_sizes(struct obs_source *source,
			  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width / 2;
	source->async_convert_width[2] = frame->width / 2;
	source->async_convert_width[3] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height / 2;
	source->async_convert_height[2] = frame->height / 2;
	source->async_convert_height[3] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_texture_formats[3] = GS_R8;
	source->async_channel_count = 4;
	return true;
}

static inline bool set_planar422_sizes(struct obs_source *source,
				       const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width / 2;
	source->async_convert_width[2] = frame->width / 2;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height;
	source->async_convert_height[2] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_channel_count = 3;
	return true;
}

static inline bool
set_planar422_alpha_sizes(struct obs_source *source,
			  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width / 2;
	source->async_convert_width[2] = frame->width / 2;
	source->async_convert_width[3] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height;
	source->async_convert_height[2] = frame->height;
	source->async_convert_height[3] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8;
	source->async_texture_formats[2] = GS_R8;
	source->async_texture_formats[3] = GS_R8;
	source->async_channel_count = 4;
	return true;
}

static inline bool set_nv12_sizes(struct obs_source *source,
				  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_width[1] = frame->width / 2;
	source->async_convert_height[0] = frame->height;
	source->async_convert_height[1] = frame->height / 2;
	source->async_texture_formats[0] = GS_R8;
	source->async_texture_formats[1] = GS_R8G8;
	source->async_channel_count = 2;
	return true;
}

static inline bool set_y800_sizes(struct obs_source *source,
				  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_channel_count = 1;
	return true;
}

static inline bool set_rgb_limited_sizes(struct obs_source *source,
					 const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width;
	source->async_convert_height[0] = frame->height;
	source->async_texture_formats[0] = convert_video_format(frame->format);
	source->async_channel_count = 1;
	return true;
}

static inline bool set_bgr3_sizes(struct obs_source *source,
				  const struct obs_source_frame *frame)
{
	source->async_convert_width[0] = frame->width * 3;
	source->async_convert_height[0] = frame->height;
	source->async_texture_formats[0] = GS_R8;
	source->async_channel_count = 1;
	return true;
}

static inline bool init_gpu_conversion(struct obs_source *source,
				       const struct obs_source_frame *frame)
{
	switch (get_convert_type(frame->format, frame->full_range)) {
	case CONVERT_422_PACK:
		return set_packed422_sizes(source, frame);

	case CONVERT_420:
		return set_planar420_sizes(source, frame);

	case CONVERT_422:
		return set_planar422_sizes(source, frame);

	case CONVERT_NV12:
		return set_nv12_sizes(source, frame);

	case CONVERT_444:
		return set_planar444_sizes(source, frame);

	case CONVERT_800:
		return set_y800_sizes(source, frame);

	case CONVERT_RGB_LIMITED:
		return set_rgb_limited_sizes(source, frame);

	case CONVERT_BGR3:
		return set_bgr3_sizes(source, frame);

	case CONVERT_420_A:
		return set_planar420_alpha_sizes(source, frame);

	case CONVERT_422_A:
		return set_planar422_alpha_sizes(source, frame);

	case CONVERT_444_A:
		return set_planar444_alpha_sizes(source, frame);

	case CONVERT_444_A_PACK:
		return set_packed444_alpha_sizes(source, frame);

	case CONVERT_NONE:
		assert(false && "No conversion requested");
		break;
	}
	return false;
}

bool set_async_texture_size(struct obs_source *source,
			    const struct obs_source_frame *frame)
{
	enum convert_type cur =
		get_convert_type(frame->format, frame->full_range);

	if (source->async_width == frame->width &&
	    source->async_height == frame->height &&
	    source->async_format == frame->format &&
	    source->async_full_range == frame->full_range)
		return true;

	source->async_width = frame->width;
	source->async_height = frame->height;
	source->async_format = frame->format;
	source->async_full_range = frame->full_range;

	gs_enter_context(obs->video.graphics);

	for (size_t c = 0; c < MAX_AV_PLANES; c++) {
		gs_texture_destroy(source->async_textures[c]);
		source->async_textures[c] = NULL;
		gs_texture_destroy(source->async_prev_textures[c]);
		source->async_prev_textures[c] = NULL;
	}

	gs_texrender_destroy(source->async_texrender);
	gs_texrender_destroy(source->async_prev_texrender);
	source->async_texrender = NULL;
	source->async_prev_texrender = NULL;

	const enum gs_color_format format = convert_video_format(frame->format);
	const bool async_gpu_conversion = (cur != CONVERT_NONE) &&
					  init_gpu_conversion(source, frame);
	source->async_gpu_conversion = async_gpu_conversion;
	if (async_gpu_conversion) {
		source->async_texrender =
			gs_texrender_create(format, GS_ZS_NONE);

		for (int c = 0; c < source->async_channel_count; ++c)
			source->async_textures[c] = gs_texture_create(
				source->async_convert_width[c],
				source->async_convert_height[c],
				source->async_texture_formats[c], 1, NULL,
				GS_DYNAMIC);
	} else {
		source->async_textures[0] =
			gs_texture_create(frame->width, frame->height, format,
					  1, NULL, GS_DYNAMIC);
	}

	if (deinterlacing_enabled(source))
		set_deinterlace_texture_size(source);

	gs_leave_context();

	return source->async_textures[0] != NULL;
}

static void upload_raw_frame(gs_texture_t *tex[MAX_AV_PLANES],
			     const struct obs_source_frame *frame)
{
	switch (get_convert_type(frame->format, frame->full_range)) {
	case CONVERT_422_PACK:
	case CONVERT_800:
	case CONVERT_RGB_LIMITED:
	case CONVERT_BGR3:
	case CONVERT_420:
	case CONVERT_422:
	case CONVERT_NV12:
	case CONVERT_444:
	case CONVERT_420_A:
	case CONVERT_422_A:
	case CONVERT_444_A:
	case CONVERT_444_A_PACK:
		for (size_t c = 0; c < MAX_AV_PLANES; c++) {
			if (tex[c])
				gs_texture_set_image(tex[c], frame->data[c],
						     frame->linesize[c], false);
		}
		break;

	case CONVERT_NONE:
		assert(false && "No conversion requested");
		break;
	}
}

static const char *select_conversion_technique(enum video_format format,
					       bool full_range)
{
	switch (format) {
	case VIDEO_FORMAT_UYVY:
		return "UYVY_Reverse";

	case VIDEO_FORMAT_YUY2:
		return "YUY2_Reverse";

	case VIDEO_FORMAT_YVYU:
		return "YVYU_Reverse";

	case VIDEO_FORMAT_I420:
		return "I420_Reverse";

	case VIDEO_FORMAT_NV12:
		return "NV12_Reverse";

	case VIDEO_FORMAT_I444:
		return "I444_Reverse";

	case VIDEO_FORMAT_Y800:
		return full_range ? "Y800_Full" : "Y800_Limited";

	case VIDEO_FORMAT_BGR3:
		return full_range ? "BGR3_Full" : "BGR3_Limited";

	case VIDEO_FORMAT_I422:
		return "I422_Reverse";

	case VIDEO_FORMAT_I40A:
		return "I40A_Reverse";

	case VIDEO_FORMAT_I42A:
		return "I42A_Reverse";

	case VIDEO_FORMAT_YUVA:
		return "YUVA_Reverse";

	case VIDEO_FORMAT_AYUV:
		return "AYUV_Reverse";

	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_BGRX:
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_NONE:
		if (full_range)
			assert(false && "No conversion requested");
		else
			return "RGB_Limited";
		break;
	}
	return NULL;
}

static inline void set_eparam(gs_effect_t *effect, const char *name, float val)
{
	gs_eparam_t *param = gs_effect_get_param_by_name(effect, name);
	gs_effect_set_float(param, val);
}

static inline void set_eparami(gs_effect_t *effect, const char *name, int val)
{
	gs_eparam_t *param = gs_effect_get_param_by_name(effect, name);
	gs_effect_set_int(param, val);
}

static bool update_async_texrender(struct obs_source *source,
				   const struct obs_source_frame *frame,
				   gs_texture_t *tex[MAX_AV_PLANES],
				   gs_texrender_t *texrender)
{
	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_CONVERT_FORMAT, "Convert Format");

	gs_texrender_reset(texrender);

	upload_raw_frame(tex, frame);

	uint32_t cx = source->async_width;
	uint32_t cy = source->async_height;

	gs_effect_t *conv = obs->video.conversion_effect;
	const char *tech_name =
		select_conversion_technique(frame->format, frame->full_range);
	gs_technique_t *tech = gs_effect_get_technique(conv, tech_name);

	const bool success = gs_texrender_begin(texrender, cx, cy);

	if (success) {
		gs_enable_blending(false);

		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);

		if (tex[0])
			gs_effect_set_texture(
				gs_effect_get_param_by_name(conv, "image"),
				tex[0]);
		if (tex[1])
			gs_effect_set_texture(
				gs_effect_get_param_by_name(conv, "image1"),
				tex[1]);
		if (tex[2])
			gs_effect_set_texture(
				gs_effect_get_param_by_name(conv, "image2"),
				tex[2]);
		if (tex[3])
			gs_effect_set_texture(
				gs_effect_get_param_by_name(conv, "image3"),
				tex[3]);
		set_eparam(conv, "width", (float)cx);
		set_eparam(conv, "height", (float)cy);
		set_eparam(conv, "width_d2", (float)cx * 0.5f);
		set_eparam(conv, "height_d2", (float)cy * 0.5f);
		set_eparam(conv, "width_x2_i", 0.5f / (float)cx);

		struct vec4 vec0, vec1, vec2;
		vec4_set(&vec0, frame->color_matrix[0], frame->color_matrix[1],
			 frame->color_matrix[2], frame->color_matrix[3]);
		vec4_set(&vec1, frame->color_matrix[4], frame->color_matrix[5],
			 frame->color_matrix[6], frame->color_matrix[7]);
		vec4_set(&vec2, frame->color_matrix[8], frame->color_matrix[9],
			 frame->color_matrix[10], frame->color_matrix[11]);
		gs_effect_set_vec4(
			gs_effect_get_param_by_name(conv, "color_vec0"), &vec0);
		gs_effect_set_vec4(
			gs_effect_get_param_by_name(conv, "color_vec1"), &vec1);
		gs_effect_set_vec4(
			gs_effect_get_param_by_name(conv, "color_vec2"), &vec2);
		if (!frame->full_range) {
			gs_eparam_t *min_param = gs_effect_get_param_by_name(
				conv, "color_range_min");
			gs_effect_set_val(min_param, frame->color_range_min,
					  sizeof(float) * 3);
			gs_eparam_t *max_param = gs_effect_get_param_by_name(
				conv, "color_range_max");
			gs_effect_set_val(max_param, frame->color_range_max,
					  sizeof(float) * 3);
		}

		gs_draw(GS_TRIS, 0, 3);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);

		gs_enable_blending(true);

		gs_texrender_end(texrender);
	}

	GS_DEBUG_MARKER_END();
	return success;
}

bool update_async_texture(struct obs_source *source,
			  const struct obs_source_frame *frame,
			  gs_texture_t *tex, gs_texrender_t *texrender)
{
	gs_texture_t *tex3[MAX_AV_PLANES] = {tex,  NULL, NULL, NULL,
					     NULL, NULL, NULL, NULL};
	return update_async_textures(source, frame, tex3, texrender);
}

bool update_async_textures(struct obs_source *source,
			   const struct obs_source_frame *frame,
			   gs_texture_t *tex[MAX_AV_PLANES],
			   gs_texrender_t *texrender)
{
	enum convert_type type;

	source->async_flip = frame->flip;

	if (source->async_gpu_conversion && texrender)
		return update_async_texrender(source, frame, tex, texrender);

	type = get_convert_type(frame->format, frame->full_range);
	if (type == CONVERT_NONE) {
		gs_texture_set_image(tex[0], frame->data[0], frame->linesize[0],
				     false);
		return true;
	}

	return false;
}

static inline void obs_source_draw_texture(struct obs_source *source,
					   gs_effect_t *effect)
{
	gs_texture_t *tex = source->async_textures[0];
	gs_eparam_t *param;

	if (source->async_texrender)
		tex = gs_texrender_get_texture(source->async_texrender);

	param = gs_effect_get_param_by_name(effect, "image");

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	if (linear_srgb) {
		gs_effect_set_texture_srgb(param, tex);
	} else {
		gs_effect_set_texture(param, tex);
	}

	gs_draw_sprite(tex, source->async_flip ? GS_FLIP_V : 0, 0, 0);

	gs_enable_framebuffer_srgb(previous);
}

static void obs_source_draw_async_texture(struct obs_source *source)
{
	gs_effect_t *effect = gs_get_effect();
	bool def_draw = (!effect);
	gs_technique_t *tech = NULL;

	if (def_draw) {
		effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		tech = gs_effect_get_technique(effect, "Draw");
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
	}

	obs_source_draw_texture(source, effect);

	if (def_draw) {
		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}
}

static void recreate_async_texture(obs_source_t *source,
				   enum gs_color_format format)
{
	uint32_t cx = gs_texture_get_width(source->async_textures[0]);
	uint32_t cy = gs_texture_get_height(source->async_textures[0]);
	gs_texture_destroy(source->async_textures[0]);
	source->async_textures[0] =
		gs_texture_create(cx, cy, format, 1, NULL, GS_DYNAMIC);
}

static inline void check_to_swap_bgrx_bgra(obs_source_t *source,
					   struct obs_source_frame *frame)
{
	enum gs_color_format format =
		gs_texture_get_color_format(source->async_textures[0]);
	if (format == GS_BGRX && frame->format == VIDEO_FORMAT_BGRA) {
		recreate_async_texture(source, GS_BGRA);
	} else if (format == GS_BGRA && frame->format == VIDEO_FORMAT_BGRX) {
		recreate_async_texture(source, GS_BGRX);
	}
}

static void obs_source_update_async_video(obs_source_t *source)
{
	if (!source->async_rendered) {
		struct obs_source_frame *frame = obs_source_get_frame(source);

		if (frame)
			frame = filter_async_video(source, frame);

		source->async_rendered = true;
		if (frame) {
			check_to_swap_bgrx_bgra(source, frame);

			if (!source->async_decoupled ||
			    !source->async_unbuffered) {
				source->timing_adjust = obs->video.video_time -
							frame->timestamp;
				source->timing_set = true;
			}

			if (source->async_update_texture) {
				update_async_textures(source, frame,
						      source->async_textures,
						      source->async_texrender);
				source->async_update_texture = false;
			}

			obs_source_release_frame(source, frame);
		}
	}
}

static void rotate_async_video(obs_source_t *source, long rotation)
{
	float x = 0;
	float y = 0;

	switch (rotation) {
	case 90:
		y = (float)source->async_width;
		break;
	case 270:
	case -90:
		x = (float)source->async_height;
		break;
	case 180:
		x = (float)source->async_width;
		y = (float)source->async_height;
	}

	gs_matrix_translate3f(x, y, 0);
	gs_matrix_rotaa4f(0.0f, 0.0f, -1.0f, RAD((float)rotation));
}

static inline void obs_source_render_async_video(obs_source_t *source)
{
	if (source->async_textures[0] && source->async_active) {
		long rotation = source->async_rotation;
		if (rotation) {
			gs_matrix_push();
			rotate_async_video(source, rotation);
		}
		obs_source_draw_async_texture(source);
		if (rotation) {
			gs_matrix_pop();
		}
	}
}

static inline void obs_source_render_filters(obs_source_t *source)
{
	obs_source_t *first_filter;

	pthread_mutex_lock(&source->filter_mutex);
	first_filter = source->filters.array[0];
	obs_source_addref(first_filter);
	pthread_mutex_unlock(&source->filter_mutex);

	source->rendering_filter = true;
	obs_source_video_render(first_filter);
	source->rendering_filter = false;

	obs_source_release(first_filter);
}

void obs_source_default_render(obs_source_t *source)
{
	gs_effect_t *effect = obs->video.default_effect;
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	size_t passes, i;

	passes = gs_technique_begin(tech);
	for (i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		if (source->context.data)
			source->info.video_render(source->context.data, effect);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
}

static inline void obs_source_main_render(obs_source_t *source)
{
	uint32_t flags = source->info.output_flags;
	bool custom_draw = (flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
	bool default_effect = !source->filter_parent &&
			      source->filters.num == 0 && !custom_draw;

	if (default_effect)
		obs_source_default_render(source);
	else if (source->context.data)
		source->info.video_render(source->context.data,
					  custom_draw ? NULL : gs_get_effect());
}

static bool ready_async_frame(obs_source_t *source, uint64_t sys_time);

#if GS_USE_DEBUG_MARKERS
static const char *get_type_format(enum obs_source_type type)
{
	switch (type) {
	case OBS_SOURCE_TYPE_INPUT:
		return "Input: %s";
	case OBS_SOURCE_TYPE_FILTER:
		return "Filter: %s";
	case OBS_SOURCE_TYPE_TRANSITION:
		return "Transition: %s";
	case OBS_SOURCE_TYPE_SCENE:
		return "Scene: %s";
	default:
		return "[Unknown]: %s";
	}
}
#endif

static inline void render_video(obs_source_t *source)
{
	if (source->info.type != OBS_SOURCE_TYPE_FILTER &&
	    (source->info.output_flags & OBS_SOURCE_VIDEO) == 0) {
		if (source->filter_parent)
			obs_source_skip_video_filter(source);
		return;
	}

	if (source->info.type == OBS_SOURCE_TYPE_INPUT &&
	    (source->info.output_flags & OBS_SOURCE_ASYNC) != 0 &&
	    !source->rendering_filter) {
		if (deinterlacing_enabled(source))
			deinterlace_update_async_video(source);
		obs_source_update_async_video(source);
	}

	if (!source->context.data || !source->enabled) {
		if (source->filter_parent)
			obs_source_skip_video_filter(source);
		return;
	}

	GS_DEBUG_MARKER_BEGIN_FORMAT(GS_DEBUG_COLOR_SOURCE,
				     get_type_format(source->info.type),
				     obs_source_get_name(source));

	if (source->filters.num && !source->rendering_filter)
		obs_source_render_filters(source);

	else if (source->info.video_render)
		obs_source_main_render(source);

	else if (source->filter_target)
		obs_source_video_render(source->filter_target);

	else if (deinterlacing_enabled(source))
		deinterlace_render(source);

	else
		obs_source_render_async_video(source);

	GS_DEBUG_MARKER_END();
}

void obs_source_video_render(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_video_render"))
		return;

	obs_source_addref(source);
	render_video(source);
	obs_source_release(source);
}

static inline uint32_t get_async_width(const obs_source_t *source)
{
	return ((source->async_rotation % 180) == 0) ? source->async_width
						     : source->async_height;
}

static inline uint32_t get_async_height(const obs_source_t *source)
{
	return ((source->async_rotation % 180) == 0) ? source->async_height
						     : source->async_width;
}

static uint32_t get_base_width(const obs_source_t *source)
{
	bool is_filter = !!source->filter_parent;
	bool func_valid = source->context.data && source->info.get_width;

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION) {
		return source->enabled ? source->transition_actual_cx : 0;

	} else if (func_valid && (!is_filter || source->enabled)) {
		return source->info.get_width(source->context.data);

	} else if (is_filter) {
		return get_base_width(source->filter_target);
	}

	return source->async_active ? get_async_width(source) : 0;
}

static uint32_t get_base_height(const obs_source_t *source)
{
	bool is_filter = !!source->filter_parent;
	bool func_valid = source->context.data && source->info.get_height;

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION) {
		return source->enabled ? source->transition_actual_cy : 0;

	} else if (func_valid && (!is_filter || source->enabled)) {
		return source->info.get_height(source->context.data);

	} else if (is_filter) {
		return get_base_height(source->filter_target);
	}

	return source->async_active ? get_async_height(source) : 0;
}

static uint32_t get_recurse_width(obs_source_t *source)
{
	uint32_t width;

	pthread_mutex_lock(&source->filter_mutex);

	width = (source->filters.num) ? get_base_width(source->filters.array[0])
				      : get_base_width(source);

	pthread_mutex_unlock(&source->filter_mutex);

	return width;
}

static uint32_t get_recurse_height(obs_source_t *source)
{
	uint32_t height;

	pthread_mutex_lock(&source->filter_mutex);

	height = (source->filters.num)
			 ? get_base_height(source->filters.array[0])
			 : get_base_height(source);

	pthread_mutex_unlock(&source->filter_mutex);

	return height;
}

uint32_t obs_source_get_width(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_get_width"))
		return 0;

	return (source->info.type != OBS_SOURCE_TYPE_FILTER)
		       ? get_recurse_width(source)
		       : get_base_width(source);
}

uint32_t obs_source_get_height(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_get_height"))
		return 0;

	return (source->info.type != OBS_SOURCE_TYPE_FILTER)
		       ? get_recurse_height(source)
		       : get_base_height(source);
}

uint32_t obs_source_get_base_width(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_get_base_width"))
		return 0;

	return get_base_width(source);
}

uint32_t obs_source_get_base_height(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_get_base_height"))
		return 0;

	return get_base_height(source);
}

obs_source_t *obs_filter_get_parent(const obs_source_t *filter)
{
	return obs_ptr_valid(filter, "obs_filter_get_parent")
		       ? filter->filter_parent
		       : NULL;
}

obs_source_t *obs_filter_get_target(const obs_source_t *filter)
{
	return obs_ptr_valid(filter, "obs_filter_get_target")
		       ? filter->filter_target
		       : NULL;
}

#define OBS_SOURCE_AV (OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO)

static bool filter_compatible(obs_source_t *source, obs_source_t *filter)
{
	uint32_t s_caps = source->info.output_flags & OBS_SOURCE_AV;
	uint32_t f_caps = filter->info.output_flags & OBS_SOURCE_AV;

	if ((f_caps & OBS_SOURCE_AUDIO) != 0 &&
	    (f_caps & OBS_SOURCE_VIDEO) == 0)
		f_caps &= ~OBS_SOURCE_ASYNC;

	return (s_caps & f_caps) == f_caps;
}

void obs_source_filter_add(obs_source_t *source, obs_source_t *filter)
{
	struct calldata cd;
	uint8_t stack[128];

	if (!obs_source_valid(source, "obs_source_filter_add"))
		return;
	if (!obs_ptr_valid(filter, "obs_source_filter_add"))
		return;

	pthread_mutex_lock(&source->filter_mutex);

	if (da_find(source->filters, &filter, 0) != DARRAY_INVALID) {
		blog(LOG_WARNING, "Tried to add a filter that was already "
				  "present on the source");
		pthread_mutex_unlock(&source->filter_mutex);
		return;
	}

	if (!source->owns_info_id && !filter_compatible(source, filter)) {
		pthread_mutex_unlock(&source->filter_mutex);
		return;
	}

	obs_source_addref(filter);

	filter->filter_parent = source;
	filter->filter_target = !source->filters.num ? source
						     : source->filters.array[0];

	da_insert(source->filters, 0, &filter);

	pthread_mutex_unlock(&source->filter_mutex);

	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "source", source);
	calldata_set_ptr(&cd, "filter", filter);

	signal_handler_signal(source->context.signals, "filter_add", &cd);

	blog(LOG_DEBUG, "- filter '%s' (%s) added to source '%s'",
	     filter->context.name, filter->info.id, source->context.name);
}

static bool obs_source_filter_remove_refless(obs_source_t *source,
					     obs_source_t *filter)
{
	struct calldata cd;
	uint8_t stack[128];
	size_t idx;

	pthread_mutex_lock(&source->filter_mutex);

	idx = da_find(source->filters, &filter, 0);
	if (idx == DARRAY_INVALID) {
		pthread_mutex_unlock(&source->filter_mutex);
		return false;
	}

	if (idx > 0) {
		obs_source_t *prev = source->filters.array[idx - 1];
		prev->filter_target = filter->filter_target;
	}

	da_erase(source->filters, idx);

	pthread_mutex_unlock(&source->filter_mutex);

	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "source", source);
	calldata_set_ptr(&cd, "filter", filter);

	signal_handler_signal(source->context.signals, "filter_remove", &cd);

	blog(LOG_DEBUG, "- filter '%s' (%s) removed from source '%s'",
	     filter->context.name, filter->info.id, source->context.name);

	if (filter->info.filter_remove)
		filter->info.filter_remove(filter->context.data,
					   filter->filter_parent);

	filter->filter_parent = NULL;
	filter->filter_target = NULL;
	return true;
}

void obs_source_filter_remove(obs_source_t *source, obs_source_t *filter)
{
	if (!obs_source_valid(source, "obs_source_filter_remove"))
		return;
	if (!obs_ptr_valid(filter, "obs_source_filter_remove"))
		return;

	if (obs_source_filter_remove_refless(source, filter))
		obs_source_release(filter);
}

static size_t find_next_filter(obs_source_t *source, obs_source_t *filter,
			       size_t cur_idx)
{
	bool curAsync = (filter->info.output_flags & OBS_SOURCE_ASYNC) != 0;
	bool nextAsync;
	obs_source_t *next;

	if (cur_idx == source->filters.num - 1)
		return DARRAY_INVALID;

	next = source->filters.array[cur_idx + 1];
	nextAsync = (next->info.output_flags & OBS_SOURCE_ASYNC);

	if (nextAsync == curAsync)
		return cur_idx + 1;
	else
		return find_next_filter(source, filter, cur_idx + 1);
}

static size_t find_prev_filter(obs_source_t *source, obs_source_t *filter,
			       size_t cur_idx)
{
	bool curAsync = (filter->info.output_flags & OBS_SOURCE_ASYNC) != 0;
	bool prevAsync;
	obs_source_t *prev;

	if (cur_idx == 0)
		return DARRAY_INVALID;

	prev = source->filters.array[cur_idx - 1];
	prevAsync = (prev->info.output_flags & OBS_SOURCE_ASYNC);

	if (prevAsync == curAsync)
		return cur_idx - 1;
	else
		return find_prev_filter(source, filter, cur_idx - 1);
}

/* moves filters above/below matching filter types */
static bool move_filter_dir(obs_source_t *source, obs_source_t *filter,
			    enum obs_order_movement movement)
{
	size_t idx;

	idx = da_find(source->filters, &filter, 0);
	if (idx == DARRAY_INVALID)
		return false;

	if (movement == OBS_ORDER_MOVE_UP) {
		size_t next_id = find_next_filter(source, filter, idx);
		if (next_id == DARRAY_INVALID)
			return false;
		da_move_item(source->filters, idx, next_id);

	} else if (movement == OBS_ORDER_MOVE_DOWN) {
		size_t prev_id = find_prev_filter(source, filter, idx);
		if (prev_id == DARRAY_INVALID)
			return false;
		da_move_item(source->filters, idx, prev_id);

	} else if (movement == OBS_ORDER_MOVE_TOP) {
		if (idx == source->filters.num - 1)
			return false;
		da_move_item(source->filters, idx, source->filters.num - 1);

	} else if (movement == OBS_ORDER_MOVE_BOTTOM) {
		if (idx == 0)
			return false;
		da_move_item(source->filters, idx, 0);
	}

	/* reorder filter targets, not the nicest way of dealing with things */
	for (size_t i = 0; i < source->filters.num; i++) {
		obs_source_t *next_filter =
			(i == source->filters.num - 1)
				? source
				: source->filters.array[i + 1];

		source->filters.array[i]->filter_target = next_filter;
	}

	return true;
}

void obs_source_filter_set_order(obs_source_t *source, obs_source_t *filter,
				 enum obs_order_movement movement)
{
	bool success;

	if (!obs_source_valid(source, "obs_source_filter_set_order"))
		return;
	if (!obs_ptr_valid(filter, "obs_source_filter_set_order"))
		return;

	pthread_mutex_lock(&source->filter_mutex);
	success = move_filter_dir(source, filter, movement);
	pthread_mutex_unlock(&source->filter_mutex);

	if (success)
		obs_source_dosignal(source, NULL, "reorder_filters");
}

obs_data_t *obs_source_get_settings(const obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_get_settings"))
		return NULL;

	obs_data_addref(source->context.settings);
	return source->context.settings;
}

struct obs_source_frame *filter_async_video(obs_source_t *source,
					    struct obs_source_frame *in)
{
	size_t i;

	pthread_mutex_lock(&source->filter_mutex);

	for (i = source->filters.num; i > 0; i--) {
		struct obs_source *filter = source->filters.array[i - 1];

		if (!filter->enabled)
			continue;

		if (filter->context.data && filter->info.filter_video) {
			in = filter->info.filter_video(filter->context.data,
						       in);
			if (!in)
				break;
		}
	}

	pthread_mutex_unlock(&source->filter_mutex);

	return in;
}

static inline void copy_frame_data_line(struct obs_source_frame *dst,
					const struct obs_source_frame *src,
					uint32_t plane, uint32_t y)
{
	uint32_t pos_src = y * src->linesize[plane];
	uint32_t pos_dst = y * dst->linesize[plane];
	uint32_t bytes = dst->linesize[plane] < src->linesize[plane]
				 ? dst->linesize[plane]
				 : src->linesize[plane];

	memcpy(dst->data[plane] + pos_dst, src->data[plane] + pos_src, bytes);
}

static inline void copy_frame_data_plane(struct obs_source_frame *dst,
					 const struct obs_source_frame *src,
					 uint32_t plane, uint32_t lines)
{
	if (dst->linesize[plane] != src->linesize[plane]) {
		for (uint32_t y = 0; y < lines; y++)
			copy_frame_data_line(dst, src, plane, y);
	} else {
		memcpy(dst->data[plane], src->data[plane],
		       (size_t)dst->linesize[plane] * (size_t)lines);
	}
}

static void copy_frame_data(struct obs_source_frame *dst,
			    const struct obs_source_frame *src)
{
	dst->flip = src->flip;
	dst->full_range = src->full_range;
	dst->timestamp = src->timestamp;
	memcpy(dst->color_matrix, src->color_matrix, sizeof(float) * 16);
	if (!dst->full_range) {
		size_t const size = sizeof(float) * 3;
		memcpy(dst->color_range_min, src->color_range_min, size);
		memcpy(dst->color_range_max, src->color_range_max, size);
	}

	switch (src->format) {
	case VIDEO_FORMAT_I420:
		copy_frame_data_plane(dst, src, 0, dst->height);
		copy_frame_data_plane(dst, src, 1, dst->height / 2);
		copy_frame_data_plane(dst, src, 2, dst->height / 2);
		break;

	case VIDEO_FORMAT_NV12:
		copy_frame_data_plane(dst, src, 0, dst->height);
		copy_frame_data_plane(dst, src, 1, dst->height / 2);
		break;

	case VIDEO_FORMAT_I444:
	case VIDEO_FORMAT_I422:
		copy_frame_data_plane(dst, src, 0, dst->height);
		copy_frame_data_plane(dst, src, 1, dst->height);
		copy_frame_data_plane(dst, src, 2, dst->height);
		break;

	case VIDEO_FORMAT_YVYU:
	case VIDEO_FORMAT_YUY2:
	case VIDEO_FORMAT_UYVY:
	case VIDEO_FORMAT_NONE:
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_BGRX:
	case VIDEO_FORMAT_Y800:
	case VIDEO_FORMAT_BGR3:
	case VIDEO_FORMAT_AYUV:
		copy_frame_data_plane(dst, src, 0, dst->height);
		break;

	case VIDEO_FORMAT_I40A:
		copy_frame_data_plane(dst, src, 0, dst->height);
		copy_frame_data_plane(dst, src, 1, dst->height / 2);
		copy_frame_data_plane(dst, src, 2, dst->height / 2);
		copy_frame_data_plane(dst, src, 3, dst->height);
		break;

	case VIDEO_FORMAT_I42A:
	case VIDEO_FORMAT_YUVA:
		copy_frame_data_plane(dst, src, 0, dst->height);
		copy_frame_data_plane(dst, src, 1, dst->height);
		copy_frame_data_plane(dst, src, 2, dst->height);
		copy_frame_data_plane(dst, src, 3, dst->height);
		break;
	}
}

void obs_source_frame_copy(struct obs_source_frame *dst,
			   const struct obs_source_frame *src)
{
	copy_frame_data(dst, src);
}

static inline bool async_texture_changed(struct obs_source *source,
					 const struct obs_source_frame *frame)
{
	enum convert_type prev, cur;
	prev = get_convert_type(source->async_cache_format,
				source->async_cache_full_range);
	cur = get_convert_type(frame->format, frame->full_range);

	return source->async_cache_width != frame->width ||
	       source->async_cache_height != frame->height || prev != cur;
}

static inline void free_async_cache(struct obs_source *source)
{
	for (size_t i = 0; i < source->async_cache.num; i++)
		obs_source_frame_decref(source->async_cache.array[i].frame);

	da_resize(source->async_cache, 0);
	da_resize(source->async_frames, 0);
	source->cur_async_frame = NULL;
	source->prev_async_frame = NULL;
}

#define MAX_UNUSED_FRAME_DURATION 5

/* frees frame allocations if they haven't been used for a specific period
 * of time */
static void clean_cache(obs_source_t *source)
{
	for (size_t i = source->async_cache.num; i > 0; i--) {
		struct async_frame *af = &source->async_cache.array[i - 1];
		if (!af->used) {
			if (++af->unused_count == MAX_UNUSED_FRAME_DURATION) {
				obs_source_frame_destroy(af->frame);
				da_erase(source->async_cache, i - 1);
			}
		}
	}
}

#define MAX_ASYNC_FRAMES 30
//if return value is not null then do (os_atomic_dec_long(&output->refs) == 0) && obs_source_frame_destroy(output)
static inline struct obs_source_frame *
cache_video(struct obs_source *source, const struct obs_source_frame *frame)
{
	struct obs_source_frame *new_frame = NULL;

	pthread_mutex_lock(&source->async_mutex);

	if (source->async_frames.num >= MAX_ASYNC_FRAMES) {
		free_async_cache(source);
		source->last_frame_ts = 0;
		pthread_mutex_unlock(&source->async_mutex);
		return NULL;
	}

	if (async_texture_changed(source, frame)) {
		free_async_cache(source);
		source->async_cache_width = frame->width;
		source->async_cache_height = frame->height;
	}

	const enum video_format format = frame->format;
	source->async_cache_format = format;
	source->async_cache_full_range = frame->full_range;

	for (size_t i = 0; i < source->async_cache.num; i++) {
		struct async_frame *af = &source->async_cache.array[i];
		if (!af->used) {
			new_frame = af->frame;
			new_frame->format = format;
			af->used = true;
			af->unused_count = 0;
			break;
		}
	}

	clean_cache(source);

	if (!new_frame) {
		struct async_frame new_af;

		new_frame = obs_source_frame_create(format, frame->width,
						    frame->height);
		new_af.frame = new_frame;
		new_af.used = true;
		new_af.unused_count = 0;
		new_frame->refs = 1;

		da_push_back(source->async_cache, &new_af);
	}

	os_atomic_inc_long(&new_frame->refs);

	pthread_mutex_unlock(&source->async_mutex);

	copy_frame_data(new_frame, frame);

	return new_frame;
}

static void
obs_source_output_video_internal(obs_source_t *source,
				 const struct obs_source_frame *frame)
{
	if (!obs_source_valid(source, "obs_source_output_video"))
		return;

	if (!frame) {
		source->async_active = false;
		return;
	}

	struct obs_source_frame *output = cache_video(source, frame);

	/* ------------------------------------------- */
	pthread_mutex_lock(&source->async_mutex);
	if (output) {
		if (os_atomic_dec_long(&output->refs) == 0) {
			obs_source_frame_destroy(output);
			output = NULL;
		} else {
			da_push_back(source->async_frames, &output);
			source->async_active = true;
		}
	}
	pthread_mutex_unlock(&source->async_mutex);
}

void obs_source_output_video(obs_source_t *source,
			     const struct obs_source_frame *frame)
{
	if (!frame) {
		obs_source_output_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame = *frame;
	new_frame.full_range =
		format_is_yuv(frame->format) ? new_frame.full_range : true;

	obs_source_output_video_internal(source, &new_frame);
}

void obs_source_output_video2(obs_source_t *source,
			      const struct obs_source_frame2 *frame)
{
	if (!frame) {
		obs_source_output_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame;
	enum video_range_type range =
		resolve_video_range(frame->format, frame->range);

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		new_frame.data[i] = frame->data[i];
		new_frame.linesize[i] = frame->linesize[i];
	}

	new_frame.width = frame->width;
	new_frame.height = frame->height;
	new_frame.timestamp = frame->timestamp;
	new_frame.format = frame->format;
	new_frame.full_range = range == VIDEO_RANGE_FULL;
	new_frame.flip = frame->flip;

	memcpy(&new_frame.color_matrix, &frame->color_matrix,
	       sizeof(frame->color_matrix));
	memcpy(&new_frame.color_range_min, &frame->color_range_min,
	       sizeof(frame->color_range_min));
	memcpy(&new_frame.color_range_max, &frame->color_range_max,
	       sizeof(frame->color_range_max));

	obs_source_output_video_internal(source, &new_frame);
}

void obs_source_set_async_rotation(obs_source_t *source, long rotation)
{
	if (source)
		source->async_rotation = rotation;
}

void obs_source_output_cea708(obs_source_t *source,
			      const struct obs_source_cea_708 *captions)
{
	if (!captions) {
		return;
	}

	pthread_mutex_lock(&source->caption_cb_mutex);

	for (size_t i = source->caption_cb_list.num; i > 0; i--) {
		struct caption_cb_info info =
			source->caption_cb_list.array[i - 1];
		info.callback(info.param, source, captions);
	}

	pthread_mutex_unlock(&source->caption_cb_mutex);
}

void obs_source_add_caption_callback(obs_source_t *source,
				     obs_source_caption_t callback, void *param)
{
	struct caption_cb_info info = {callback, param};

	if (!obs_source_valid(source, "obs_source_add_caption_callback"))
		return;

	pthread_mutex_lock(&source->caption_cb_mutex);
	da_push_back(source->caption_cb_list, &info);
	pthread_mutex_unlock(&source->caption_cb_mutex);
}

void obs_source_remove_caption_callback(obs_source_t *source,
					obs_source_caption_t callback,
					void *param)
{
	struct caption_cb_info info = {callback, param};

	if (!obs_source_valid(source, "obs_source_remove_caption_callback"))
		return;

	pthread_mutex_lock(&source->caption_cb_mutex);
	da_erase_item(source->caption_cb_list, &info);
	pthread_mutex_unlock(&source->caption_cb_mutex);
}

static inline bool preload_frame_changed(obs_source_t *source,
					 const struct obs_source_frame *in)
{
	if (!source->async_preload_frame)
		return true;

	return in->width != source->async_preload_frame->width ||
	       in->height != source->async_preload_frame->height ||
	       in->format != source->async_preload_frame->format;
}

static void
obs_source_preload_video_internal(obs_source_t *source,
				  const struct obs_source_frame *frame)
{
	if (!obs_source_valid(source, "obs_source_preload_video"))
		return;
	if (!frame)
		return;

	if (preload_frame_changed(source, frame)) {
		obs_source_frame_destroy(source->async_preload_frame);
		source->async_preload_frame = obs_source_frame_create(
			frame->format, frame->width, frame->height);
	}

	copy_frame_data(source->async_preload_frame, frame);

	source->last_frame_ts = frame->timestamp;
}

void obs_source_preload_video(obs_source_t *source,
			      const struct obs_source_frame *frame)
{
	if (!frame) {
		obs_source_preload_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame = *frame;
	new_frame.full_range =
		format_is_yuv(frame->format) ? new_frame.full_range : true;

	obs_source_preload_video_internal(source, &new_frame);
}

void obs_source_preload_video2(obs_source_t *source,
			       const struct obs_source_frame2 *frame)
{
	if (!frame) {
		obs_source_preload_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame;
	enum video_range_type range =
		resolve_video_range(frame->format, frame->range);

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		new_frame.data[i] = frame->data[i];
		new_frame.linesize[i] = frame->linesize[i];
	}

	new_frame.width = frame->width;
	new_frame.height = frame->height;
	new_frame.timestamp = frame->timestamp;
	new_frame.format = frame->format;
	new_frame.full_range = range == VIDEO_RANGE_FULL;
	new_frame.flip = frame->flip;

	memcpy(&new_frame.color_matrix, &frame->color_matrix,
	       sizeof(frame->color_matrix));
	memcpy(&new_frame.color_range_min, &frame->color_range_min,
	       sizeof(frame->color_range_min));
	memcpy(&new_frame.color_range_max, &frame->color_range_max,
	       sizeof(frame->color_range_max));

	obs_source_preload_video_internal(source, &new_frame);
}

void obs_source_show_preloaded_video(obs_source_t *source)
{
	uint64_t sys_ts;

	if (!obs_source_valid(source, "obs_source_show_preloaded_video"))
		return;

	if (!source->async_preload_frame)
		return;

	obs_enter_graphics();

	set_async_texture_size(source, source->async_preload_frame);
	update_async_textures(source, source->async_preload_frame,
			      source->async_textures, source->async_texrender);
	source->async_active = true;

	obs_leave_graphics();

	pthread_mutex_lock(&source->audio_buf_mutex);
	sys_ts = (source->monitoring_type != OBS_MONITORING_TYPE_MONITOR_ONLY)
			 ? os_gettime_ns()
			 : 0;
	reset_audio_timing(source, source->last_frame_ts, sys_ts);
	reset_audio_data(source, sys_ts);
	pthread_mutex_unlock(&source->audio_buf_mutex);
}

static void
obs_source_set_video_frame_internal(obs_source_t *source,
				    const struct obs_source_frame *frame)
{
	if (!obs_source_valid(source, "obs_source_set_video_frame"))
		return;
	if (!frame)
		return;

	obs_enter_graphics();

	if (preload_frame_changed(source, frame)) {
		obs_source_frame_destroy(source->async_preload_frame);
		source->async_preload_frame = obs_source_frame_create(
			frame->format, frame->width, frame->height);
	}

	copy_frame_data(source->async_preload_frame, frame);
	set_async_texture_size(source, source->async_preload_frame);
	update_async_textures(source, source->async_preload_frame,
			      source->async_textures, source->async_texrender);

	source->last_frame_ts = frame->timestamp;

	obs_leave_graphics();
}

void obs_source_set_video_frame(obs_source_t *source,
				const struct obs_source_frame *frame)
{
	if (!frame) {
		obs_source_preload_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame = *frame;
	new_frame.full_range =
		format_is_yuv(frame->format) ? new_frame.full_range : true;

	obs_source_set_video_frame_internal(source, &new_frame);
}

void obs_source_set_video_frame2(obs_source_t *source,
				 const struct obs_source_frame2 *frame)
{
	if (!frame) {
		obs_source_preload_video_internal(source, NULL);
		return;
	}

	struct obs_source_frame new_frame;
	enum video_range_type range =
		resolve_video_range(frame->format, frame->range);

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		new_frame.data[i] = frame->data[i];
		new_frame.linesize[i] = frame->linesize[i];
	}

	new_frame.width = frame->width;
	new_frame.height = frame->height;
	new_frame.timestamp = frame->timestamp;
	new_frame.format = frame->format;
	new_frame.full_range = range == VIDEO_RANGE_FULL;
	new_frame.flip = frame->flip;

	memcpy(&new_frame.color_matrix, &frame->color_matrix,
	       sizeof(frame->color_matrix));
	memcpy(&new_frame.color_range_min, &frame->color_range_min,
	       sizeof(frame->color_range_min));
	memcpy(&new_frame.color_range_max, &frame->color_range_max,
	       sizeof(frame->color_range_max));

	obs_source_set_video_frame_internal(source, &new_frame);
}

static inline struct obs_audio_data *
filter_async_audio(obs_source_t *source, struct obs_audio_data *in)
{
	size_t i;
	for (i = source->filters.num; i > 0; i--) {
		struct obs_source *filter = source->filters.array[i - 1];

		if (!filter->enabled)
			continue;

		if (filter->context.data && filter->info.filter_audio) {
			in = filter->info.filter_audio(filter->context.data,
						       in);
			if (!in)
				return NULL;
		}
	}

	return in;
}

static inline void reset_resampler(obs_source_t *source,
				   const struct obs_source_audio *audio)
{
	const struct audio_output_info *obs_info;
	struct resample_info output_info;

	obs_info = audio_output_get_info(obs->audio.audio);

	output_info.format = obs_info->format;
	output_info.samples_per_sec = obs_info->samples_per_sec;
	output_info.speakers = obs_info->speakers;

	source->sample_info.format = audio->format;
	source->sample_info.samples_per_sec = audio->samples_per_sec;
	source->sample_info.speakers = audio->speakers;

	audio_resampler_destroy(source->resampler);
	source->resampler = NULL;
	source->resample_offset = 0;

	if (source->sample_info.samples_per_sec == obs_info->samples_per_sec &&
	    source->sample_info.format == obs_info->format &&
	    source->sample_info.speakers == obs_info->speakers) {
		source->audio_failed = false;
		return;
	}

	source->resampler =
		audio_resampler_create(&output_info, &source->sample_info);

	source->audio_failed = source->resampler == NULL;
	if (source->resampler == NULL)
		blog(LOG_ERROR, "creation of resampler failed");
}

static void copy_audio_data(obs_source_t *source, const uint8_t *const data[],
			    uint32_t frames, uint64_t ts)
{
	size_t planes = audio_output_get_planes(obs->audio.audio);
	size_t blocksize = audio_output_get_block_size(obs->audio.audio);
	size_t size = (size_t)frames * blocksize;
	bool resize = source->audio_storage_size < size;

	source->audio_data.frames = frames;
	source->audio_data.timestamp = ts;

	for (size_t i = 0; i < planes; i++) {
		/* ensure audio storage capacity */
		if (resize) {
			bfree(source->audio_data.data[i]);
			source->audio_data.data[i] = bmalloc(size);
		}

		memcpy(source->audio_data.data[i], data[i], size);
	}

	if (resize)
		source->audio_storage_size = size;
}

/* TODO: SSE optimization */
static void downmix_to_mono_planar(struct obs_source *source, uint32_t frames)
{
	size_t channels = audio_output_get_channels(obs->audio.audio);
	const float channels_i = 1.0f / (float)channels;
	float **data = (float **)source->audio_data.data;

	for (size_t channel = 1; channel < channels; channel++) {
		for (uint32_t frame = 0; frame < frames; frame++)
			data[0][frame] += data[channel][frame];
	}

	for (uint32_t frame = 0; frame < frames; frame++)
		data[0][frame] *= channels_i;

	for (size_t channel = 1; channel < channels; channel++) {
		for (uint32_t frame = 0; frame < frames; frame++)
			data[channel][frame] = data[0][frame];
	}
}

static void process_audio_balancing(struct obs_source *source, uint32_t frames,
				    float balance, enum obs_balance_type type)
{
	float **data = (float **)source->audio_data.data;

	switch (type) {
	case OBS_BALANCE_TYPE_SINE_LAW:
		for (uint32_t frame = 0; frame < frames; frame++) {
			data[0][frame] = data[0][frame] *
					 sinf((1.0f - balance) * (M_PI / 2.0f));
			data[1][frame] =
				data[1][frame] * sinf(balance * (M_PI / 2.0f));
		}
		break;
	case OBS_BALANCE_TYPE_SQUARE_LAW:
		for (uint32_t frame = 0; frame < frames; frame++) {
			data[0][frame] = data[0][frame] * sqrtf(1.0f - balance);
			data[1][frame] = data[1][frame] * sqrtf(balance);
		}
		break;
	case OBS_BALANCE_TYPE_LINEAR:
		for (uint32_t frame = 0; frame < frames; frame++) {
			data[0][frame] = data[0][frame] * (1.0f - balance);
			data[1][frame] = data[1][frame] * balance;
		}
		break;
	default:
		break;
	}
}

/* resamples/remixes new audio to the designated main audio output format */
static void process_audio(obs_source_t *source,
			  const struct obs_source_audio *audio)
{
	uint32_t frames = audio->frames;
	bool mono_output;

	if (source->sample_info.samples_per_sec != audio->samples_per_sec ||
	    source->sample_info.format != audio->format ||
	    source->sample_info.speakers != audio->speakers)
		reset_resampler(source, audio);

	if (source->audio_failed)
		return;

	if (source->resampler) {
		uint8_t *output[MAX_AV_PLANES];

		memset(output, 0, sizeof(output));

		audio_resampler_resample(source->resampler, output, &frames,
					 &source->resample_offset, audio->data,
					 audio->frames);

		copy_audio_data(source, (const uint8_t *const *)output, frames,
				audio->timestamp);
	} else {
		copy_audio_data(source, audio->data, audio->frames,
				audio->timestamp);
	}

	mono_output = audio_output_get_channels(obs->audio.audio) == 1;

	if (!mono_output && source->sample_info.speakers == SPEAKERS_STEREO &&
	    (source->balance > 0.51f || source->balance < 0.49f)) {
		process_audio_balancing(source, frames, source->balance,
					OBS_BALANCE_TYPE_SINE_LAW);
	}

	if (!mono_output && (source->flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0)
		downmix_to_mono_planar(source, frames);
}

void obs_source_output_audio(obs_source_t *source,
			     const struct obs_source_audio *audio)
{
	struct obs_audio_data *output;

	if (!obs_source_valid(source, "obs_source_output_audio"))
		return;
	if (!obs_ptr_valid(audio, "obs_source_output_audio"))
		return;

	process_audio(source, audio);

	pthread_mutex_lock(&source->filter_mutex);
	output = filter_async_audio(source, &source->audio_data);

	if (output) {
		struct audio_data data;

		for (int i = 0; i < MAX_AV_PLANES; i++)
			data.data[i] = output->data[i];

		data.frames = output->frames;
		data.timestamp = output->timestamp;

		pthread_mutex_lock(&source->audio_mutex);
		source_output_audio_data(source, &data);
		pthread_mutex_unlock(&source->audio_mutex);
	}

	pthread_mutex_unlock(&source->filter_mutex);
}

void remove_async_frame(obs_source_t *source, struct obs_source_frame *frame)
{
	if (frame)
		frame->prev_frame = false;

	for (size_t i = 0; i < source->async_cache.num; i++) {
		struct async_frame *f = &source->async_cache.array[i];

		if (f->frame == frame) {
			f->used = false;
			break;
		}
	}
}

/* #define DEBUG_ASYNC_FRAMES 1 */

static bool ready_async_frame(obs_source_t *source, uint64_t sys_time)
{
	struct obs_source_frame *next_frame = source->async_frames.array[0];
	struct obs_source_frame *frame = NULL;
	uint64_t sys_offset = sys_time - source->last_sys_timestamp;
	uint64_t frame_time = next_frame->timestamp;
	uint64_t frame_offset = 0;

	if (source->async_unbuffered) {
		while (source->async_frames.num > 1) {
			da_erase(source->async_frames, 0);
			remove_async_frame(source, next_frame);
			next_frame = source->async_frames.array[0];
		}

		source->last_frame_ts = next_frame->timestamp;
		return true;
	}

#if DEBUG_ASYNC_FRAMES
	blog(LOG_DEBUG,
	     "source->last_frame_ts: %llu, frame_time: %llu, "
	     "sys_offset: %llu, frame_offset: %llu, "
	     "number of frames: %lu",
	     source->last_frame_ts, frame_time, sys_offset,
	     frame_time - source->last_frame_ts,
	     (unsigned long)source->async_frames.num);
#endif

	/* account for timestamp invalidation */
	if (frame_out_of_bounds(source, frame_time)) {
#if DEBUG_ASYNC_FRAMES
		blog(LOG_DEBUG, "timing jump");
#endif
		source->last_frame_ts = next_frame->timestamp;
		return true;
	} else {
		frame_offset = frame_time - source->last_frame_ts;
		source->last_frame_ts += sys_offset;
	}

	while (source->last_frame_ts > next_frame->timestamp) {

		/* this tries to reduce the needless frame duplication, also
		 * helps smooth out async rendering to frame boundaries.  In
		 * other words, tries to keep the framerate as smooth as
		 * possible */
		if ((source->last_frame_ts - next_frame->timestamp) < 2000000)
			break;

		if (frame)
			da_erase(source->async_frames, 0);

#if DEBUG_ASYNC_FRAMES
		blog(LOG_DEBUG,
		     "new frame, "
		     "source->last_frame_ts: %llu, "
		     "next_frame->timestamp: %llu",
		     source->last_frame_ts, next_frame->timestamp);
#endif

		remove_async_frame(source, frame);

		if (source->async_frames.num == 1)
			return true;

		frame = next_frame;
		next_frame = source->async_frames.array[1];

		/* more timestamp checking and compensating */
		if ((next_frame->timestamp - frame_time) > MAX_TS_VAR) {
#if DEBUG_ASYNC_FRAMES
			blog(LOG_DEBUG, "timing jump");
#endif
			source->last_frame_ts =
				next_frame->timestamp - frame_offset;
		}

		frame_time = next_frame->timestamp;
		frame_offset = frame_time - source->last_frame_ts;
	}

#if DEBUG_ASYNC_FRAMES
	if (!frame)
		blog(LOG_DEBUG, "no frame!");
#endif

	return frame != NULL;
}

static inline struct obs_source_frame *get_closest_frame(obs_source_t *source,
							 uint64_t sys_time)
{
	if (!source->async_frames.num)
		return NULL;

	if (!source->last_frame_ts || ready_async_frame(source, sys_time)) {
		struct obs_source_frame *frame = source->async_frames.array[0];
		da_erase(source->async_frames, 0);

		if (!source->last_frame_ts)
			source->last_frame_ts = frame->timestamp;

		return frame;
	}

	return NULL;
}

/*
 * Ensures that cached frames are displayed on time.  If multiple frames
 * were cached between renders, then releases the unnecessary frames and uses
 * the frame with the closest timing to ensure sync.  Also ensures that timing
 * with audio is synchronized.
 */
struct obs_source_frame *obs_source_get_frame(obs_source_t *source)
{
	struct obs_source_frame *frame = NULL;

	if (!obs_source_valid(source, "obs_source_get_frame"))
		return NULL;

	pthread_mutex_lock(&source->async_mutex);

	frame = source->cur_async_frame;
	source->cur_async_frame = NULL;

	if (frame) {
		os_atomic_inc_long(&frame->refs);
	}

	pthread_mutex_unlock(&source->async_mutex);

	return frame;
}

void obs_source_release_frame(obs_source_t *source,
			      struct obs_source_frame *frame)
{
	if (!frame)
		return;

	if (!source) {
		obs_source_frame_destroy(frame);
	} else {
		pthread_mutex_lock(&source->async_mutex);

		if (os_atomic_dec_long(&frame->refs) == 0)
			obs_source_frame_destroy(frame);
		else
			remove_async_frame(source, frame);

		pthread_mutex_unlock(&source->async_mutex);
	}
}

const char *obs_source_get_name(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_name")
		       ? source->context.name
		       : NULL;
}

void obs_source_set_name(obs_source_t *source, const char *name)
{
	if (!obs_source_valid(source, "obs_source_set_name"))
		return;

	if (!name || !*name || !source->context.name ||
	    strcmp(name, source->context.name) != 0) {
		struct calldata data;
		char *prev_name = bstrdup(source->context.name);
		obs_context_data_setname(&source->context, name);

		calldata_init(&data);
		calldata_set_ptr(&data, "source", source);
		calldata_set_string(&data, "new_name", source->context.name);
		calldata_set_string(&data, "prev_name", prev_name);
		if (!source->context.private)
			signal_handler_signal(obs->signals, "source_rename",
					      &data);
		signal_handler_signal(source->context.signals, "rename", &data);
		calldata_free(&data);
		bfree(prev_name);
	}
}

enum obs_source_type obs_source_get_type(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_type")
		       ? source->info.type
		       : OBS_SOURCE_TYPE_INPUT;
}

const char *obs_source_get_id(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_id") ? source->info.id
							     : NULL;
}

const char *obs_source_get_unversioned_id(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_unversioned_id")
		       ? source->info.unversioned_id
		       : NULL;
}

static inline void render_filter_bypass(obs_source_t *target,
					gs_effect_t *effect,
					const char *tech_name)
{
	gs_technique_t *tech = gs_effect_get_technique(effect, tech_name);
	size_t passes, i;

	passes = gs_technique_begin(tech);
	for (i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		obs_source_video_render(target);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
}

static inline void render_filter_tex(gs_texture_t *tex, gs_effect_t *effect,
				     uint32_t width, uint32_t height,
				     const char *tech_name)
{
	gs_technique_t *tech = gs_effect_get_technique(effect, tech_name);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	size_t passes, i;

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	if (linear_srgb)
		gs_effect_set_texture_srgb(image, tex);
	else
		gs_effect_set_texture(image, tex);

	passes = gs_technique_begin(tech);
	for (i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		gs_draw_sprite(tex, 0, width, height);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);

	gs_enable_framebuffer_srgb(previous);
}

static inline bool can_bypass(obs_source_t *target, obs_source_t *parent,
			      uint32_t parent_flags,
			      enum obs_allow_direct_render allow_direct)
{
	return (target == parent) &&
	       (allow_direct == OBS_ALLOW_DIRECT_RENDERING) &&
	       ((parent_flags & OBS_SOURCE_CUSTOM_DRAW) == 0) &&
	       ((parent_flags & OBS_SOURCE_ASYNC) == 0);
}

bool obs_source_process_filter_begin(obs_source_t *filter,
				     enum gs_color_format format,
				     enum obs_allow_direct_render allow_direct)
{
	obs_source_t *target, *parent;
	uint32_t parent_flags;
	int cx, cy;

	if (!obs_ptr_valid(filter, "obs_source_process_filter_begin"))
		return false;

	target = obs_filter_get_target(filter);
	parent = obs_filter_get_parent(filter);

	if (!target) {
		blog(LOG_INFO, "filter '%s' being processed with no target!",
		     filter->context.name);
		return false;
	}
	if (!parent) {
		blog(LOG_INFO, "filter '%s' being processed with no parent!",
		     filter->context.name);
		return false;
	}

	parent_flags = parent->info.output_flags;
	cx = get_base_width(target);
	cy = get_base_height(target);

	filter->allow_direct = allow_direct;

	/* if the parent does not use any custom effects, and this is the last
	 * filter in the chain for the parent, then render the parent directly
	 * using the filter effect instead of rendering to texture to reduce
	 * the total number of passes */
	if (can_bypass(target, parent, parent_flags, allow_direct)) {
		return true;
	}

	if (!cx || !cy) {
		obs_source_skip_video_filter(filter);
		return false;
	}

	if (!filter->filter_texrender)
		filter->filter_texrender =
			gs_texrender_create(format, GS_ZS_NONE);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(filter->filter_texrender, cx, cy)) {
		bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

		if (target == parent && !custom_draw && !async)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(filter->filter_texrender);
	}

	gs_blend_state_pop();
	return true;
}

void obs_source_process_filter_tech_end(obs_source_t *filter,
					gs_effect_t *effect, uint32_t width,
					uint32_t height, const char *tech_name)
{
	obs_source_t *target, *parent;
	gs_texture_t *texture;
	uint32_t parent_flags;

	if (!filter)
		return;

	target = obs_filter_get_target(filter);
	parent = obs_filter_get_parent(filter);

	if (!target || !parent)
		return;

	parent_flags = parent->info.output_flags;

	const char *tech = tech_name ? tech_name : "Draw";

	if (can_bypass(target, parent, parent_flags, filter->allow_direct)) {
		render_filter_bypass(target, effect, tech);
	} else {
		texture = gs_texrender_get_texture(filter->filter_texrender);
		if (texture) {
			render_filter_tex(texture, effect, width, height, tech);
		}
	}
}

void obs_source_process_filter_end(obs_source_t *filter, gs_effect_t *effect,
				   uint32_t width, uint32_t height)
{
	if (!obs_ptr_valid(filter, "obs_source_process_filter_end"))
		return;

	obs_source_process_filter_tech_end(filter, effect, width, height,
					   "Draw");
}

void obs_source_skip_video_filter(obs_source_t *filter)
{
	obs_source_t *target, *parent;
	bool custom_draw, async;
	uint32_t parent_flags;

	if (!obs_ptr_valid(filter, "obs_source_skip_video_filter"))
		return;

	target = obs_filter_get_target(filter);
	parent = obs_filter_get_parent(filter);
	parent_flags = parent->info.output_flags;
	custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
	async = (parent_flags & OBS_SOURCE_ASYNC) != 0;

	if (target == parent) {
		if (!custom_draw && !async)
			obs_source_default_render(target);
		else if (target->info.video_render)
			obs_source_main_render(target);
		else if (deinterlacing_enabled(target))
			deinterlace_render(target);
		else
			obs_source_render_async_video(target);

	} else {
		obs_source_video_render(target);
	}
}

signal_handler_t *obs_source_get_signal_handler(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_signal_handler")
		       ? source->context.signals
		       : NULL;
}

proc_handler_t *obs_source_get_proc_handler(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_proc_handler")
		       ? source->context.procs
		       : NULL;
}

void obs_source_set_volume(obs_source_t *source, float volume)
{
	if (obs_source_valid(source, "obs_source_set_volume")) {
		struct audio_action action = {.timestamp = os_gettime_ns(),
					      .type = AUDIO_ACTION_VOL,
					      .vol = volume};

		struct calldata data;
		uint8_t stack[128];

		calldata_init_fixed(&data, stack, sizeof(stack));
		calldata_set_ptr(&data, "source", source);
		calldata_set_float(&data, "volume", volume);

		signal_handler_signal(source->context.signals, "volume", &data);
		if (!source->context.private)
			signal_handler_signal(obs->signals, "source_volume",
					      &data);

		volume = (float)calldata_float(&data, "volume");

		pthread_mutex_lock(&source->audio_actions_mutex);
		da_push_back(source->audio_actions, &action);
		pthread_mutex_unlock(&source->audio_actions_mutex);

		source->user_volume = volume;
	}
}

float obs_source_get_volume(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_volume")
		       ? source->user_volume
		       : 0.0f;
}

void obs_source_set_sync_offset(obs_source_t *source, int64_t offset)
{
	if (obs_source_valid(source, "obs_source_set_sync_offset")) {
		struct calldata data;
		uint8_t stack[128];

		calldata_init_fixed(&data, stack, sizeof(stack));
		calldata_set_ptr(&data, "source", source);
		calldata_set_int(&data, "offset", offset);

		signal_handler_signal(source->context.signals, "audio_sync",
				      &data);

		source->sync_offset = calldata_int(&data, "offset");
	}
}

int64_t obs_source_get_sync_offset(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_sync_offset")
		       ? source->sync_offset
		       : 0;
}

struct source_enum_data {
	obs_source_enum_proc_t enum_callback;
	void *param;
};

static void enum_source_active_tree_callback(obs_source_t *parent,
					     obs_source_t *child, void *param)
{
	struct source_enum_data *data = param;
	bool is_transition = child->info.type == OBS_SOURCE_TYPE_TRANSITION;

	if (is_transition)
		obs_transition_enum_sources(
			child, enum_source_active_tree_callback, param);
	if (child->info.enum_active_sources) {
		if (child->context.data) {
			child->info.enum_active_sources(
				child->context.data,
				enum_source_active_tree_callback, data);
		}
	}

	data->enum_callback(parent, child, data->param);
}

void obs_source_enum_active_sources(obs_source_t *source,
				    obs_source_enum_proc_t enum_callback,
				    void *param)
{
	bool is_transition;
	if (!data_valid(source, "obs_source_enum_active_sources"))
		return;

	is_transition = source->info.type == OBS_SOURCE_TYPE_TRANSITION;
	if (!is_transition && !source->info.enum_active_sources)
		return;

	obs_source_addref(source);

	if (is_transition)
		obs_transition_enum_sources(source, enum_callback, param);
	if (source->info.enum_active_sources)
		source->info.enum_active_sources(source->context.data,
						 enum_callback, param);

	obs_source_release(source);
}

void obs_source_enum_active_tree(obs_source_t *source,
				 obs_source_enum_proc_t enum_callback,
				 void *param)
{
	struct source_enum_data data = {enum_callback, param};
	bool is_transition;

	if (!data_valid(source, "obs_source_enum_active_tree"))
		return;

	is_transition = source->info.type == OBS_SOURCE_TYPE_TRANSITION;
	if (!is_transition && !source->info.enum_active_sources)
		return;

	obs_source_addref(source);

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_enum_sources(
			source, enum_source_active_tree_callback, &data);
	if (source->info.enum_active_sources)
		source->info.enum_active_sources(
			source->context.data, enum_source_active_tree_callback,
			&data);

	obs_source_release(source);
}

static void enum_source_full_tree_callback(obs_source_t *parent,
					   obs_source_t *child, void *param)
{
	struct source_enum_data *data = param;
	bool is_transition = child->info.type == OBS_SOURCE_TYPE_TRANSITION;

	if (is_transition)
		obs_transition_enum_sources(
			child, enum_source_full_tree_callback, param);
	if (child->info.enum_all_sources) {
		if (child->context.data) {
			child->info.enum_all_sources(
				child->context.data,
				enum_source_full_tree_callback, data);
		}
	} else if (child->info.enum_active_sources) {
		if (child->context.data) {
			child->info.enum_active_sources(
				child->context.data,
				enum_source_full_tree_callback, data);
		}
	}

	data->enum_callback(parent, child, data->param);
}

static void obs_source_enum_full_tree(obs_source_t *source,
				      obs_source_enum_proc_t enum_callback,
				      void *param)
{
	struct source_enum_data data = {enum_callback, param};
	bool is_transition;

	if (!data_valid(source, "obs_source_enum_active_tree"))
		return;

	is_transition = source->info.type == OBS_SOURCE_TYPE_TRANSITION;
	if (!is_transition && !source->info.enum_active_sources)
		return;

	obs_source_addref(source);

	if (source->info.type == OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_enum_sources(
			source, enum_source_full_tree_callback, &data);

	if (source->info.enum_all_sources) {
		source->info.enum_all_sources(source->context.data,
					      enum_source_full_tree_callback,
					      &data);

	} else if (source->info.enum_active_sources) {
		source->info.enum_active_sources(source->context.data,
						 enum_source_full_tree_callback,
						 &data);
	}

	obs_source_release(source);
}

struct descendant_info {
	bool exists;
	obs_source_t *target;
};

static void check_descendant(obs_source_t *parent, obs_source_t *child,
			     void *param)
{
	struct descendant_info *info = param;
	if (child == info->target || parent == info->target)
		info->exists = true;
}

bool obs_source_add_active_child(obs_source_t *parent, obs_source_t *child)
{
	struct descendant_info info = {false, parent};

	if (!obs_ptr_valid(parent, "obs_source_add_active_child"))
		return false;
	if (!obs_ptr_valid(child, "obs_source_add_active_child"))
		return false;
	if (parent == child) {
		blog(LOG_WARNING, "obs_source_add_active_child: "
				  "parent == child");
		return false;
	}

	obs_source_enum_full_tree(child, check_descendant, &info);
	if (info.exists)
		return false;

	for (int i = 0; i < parent->show_refs; i++) {
		enum view_type type;
		type = (i < parent->activate_refs) ? MAIN_VIEW : AUX_VIEW;
		obs_source_activate(child, type);
	}

	return true;
}

void obs_source_remove_active_child(obs_source_t *parent, obs_source_t *child)
{
	if (!obs_ptr_valid(parent, "obs_source_remove_active_child"))
		return;
	if (!obs_ptr_valid(child, "obs_source_remove_active_child"))
		return;

	for (int i = 0; i < parent->show_refs; i++) {
		enum view_type type;
		type = (i < parent->activate_refs) ? MAIN_VIEW : AUX_VIEW;
		obs_source_deactivate(child, type);
	}
}

void obs_source_save(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_save"))
		return;

	obs_source_dosignal(source, "source_save", "save");

	if (source->info.save)
		source->info.save(source->context.data,
				  source->context.settings);
}

void obs_source_load(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_load"))
		return;
	if (source->info.load)
		source->info.load(source->context.data,
				  source->context.settings);

	obs_source_dosignal(source, "source_load", "load");
}

bool obs_source_active(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_active")
		       ? source->activate_refs != 0
		       : false;
}

bool obs_source_showing(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_showing")
		       ? source->show_refs != 0
		       : false;
}

static inline void signal_flags_updated(obs_source_t *source)
{
	struct calldata data;
	uint8_t stack[128];

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_int(&data, "flags", source->flags);

	signal_handler_signal(source->context.signals, "update_flags", &data);
}

void obs_source_set_flags(obs_source_t *source, uint32_t flags)
{
	if (!obs_source_valid(source, "obs_source_set_flags"))
		return;

	if (flags != source->flags) {
		source->flags = flags;
		signal_flags_updated(source);
	}
}

void obs_source_set_default_flags(obs_source_t *source, uint32_t flags)
{
	if (!obs_source_valid(source, "obs_source_set_default_flags"))
		return;

	source->default_flags = flags;
}

uint32_t obs_source_get_flags(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_flags") ? source->flags
								: 0;
}

void obs_source_set_audio_mixers(obs_source_t *source, uint32_t mixers)
{
	struct calldata data;
	uint8_t stack[128];

	if (!obs_source_valid(source, "obs_source_set_audio_mixers"))
		return;
	if ((source->info.output_flags & OBS_SOURCE_AUDIO) == 0)
		return;

	if (source->audio_mixers == mixers)
		return;

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_int(&data, "mixers", mixers);

	signal_handler_signal(source->context.signals, "audio_mixers", &data);

	mixers = (uint32_t)calldata_int(&data, "mixers");

	source->audio_mixers = mixers;
}

uint32_t obs_source_get_audio_mixers(const obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_get_audio_mixers"))
		return 0;
	if ((source->info.output_flags & OBS_SOURCE_AUDIO) == 0)
		return 0;

	return source->audio_mixers;
}

void obs_source_draw_set_color_matrix(const struct matrix4 *color_matrix,
				      const struct vec3 *color_range_min,
				      const struct vec3 *color_range_max)
{
	struct vec3 color_range_min_def;
	struct vec3 color_range_max_def;

	vec3_set(&color_range_min_def, 0.0f, 0.0f, 0.0f);
	vec3_set(&color_range_max_def, 1.0f, 1.0f, 1.0f);

	gs_effect_t *effect = gs_get_effect();
	gs_eparam_t *matrix;
	gs_eparam_t *range_min;
	gs_eparam_t *range_max;

	if (!effect) {
		blog(LOG_WARNING, "obs_source_draw_set_color_matrix: no "
				  "active effect!");
		return;
	}

	if (!obs_ptr_valid(color_matrix, "obs_source_draw_set_color_matrix"))
		return;

	if (!color_range_min)
		color_range_min = &color_range_min_def;
	if (!color_range_max)
		color_range_max = &color_range_max_def;

	matrix = gs_effect_get_param_by_name(effect, "color_matrix");
	range_min = gs_effect_get_param_by_name(effect, "color_range_min");
	range_max = gs_effect_get_param_by_name(effect, "color_range_max");

	gs_effect_set_matrix4(matrix, color_matrix);
	gs_effect_set_val(range_min, color_range_min, sizeof(float) * 3);
	gs_effect_set_val(range_max, color_range_max, sizeof(float) * 3);
}

void obs_source_draw(gs_texture_t *texture, int x, int y, uint32_t cx,
		     uint32_t cy, bool flip)
{
	if (!obs_ptr_valid(texture, "obs_source_draw"))
		return;

	gs_effect_t *effect = gs_get_effect();
	if (!effect) {
		blog(LOG_WARNING, "obs_source_draw: no active effect!");
		return;
	}

	const bool linear_srgb = gs_get_linear_srgb();

	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	if (linear_srgb)
		gs_effect_set_texture_srgb(image, texture);
	else
		gs_effect_set_texture(image, texture);

	const bool change_pos = (x != 0 || y != 0);
	if (change_pos) {
		gs_matrix_push();
		gs_matrix_translate3f((float)x, (float)y, 0.0f);
	}

	gs_draw_sprite(texture, flip ? GS_FLIP_V : 0, cx, cy);

	if (change_pos)
		gs_matrix_pop();

	gs_enable_framebuffer_srgb(previous);
}

void obs_source_inc_showing(obs_source_t *source)
{
	if (obs_source_valid(source, "obs_source_inc_showing"))
		obs_source_activate(source, AUX_VIEW);
}

void obs_source_inc_active(obs_source_t *source)
{
	if (obs_source_valid(source, "obs_source_inc_active"))
		obs_source_activate(source, MAIN_VIEW);
}

void obs_source_dec_showing(obs_source_t *source)
{
	if (obs_source_valid(source, "obs_source_dec_showing"))
		obs_source_deactivate(source, AUX_VIEW);
}

void obs_source_dec_active(obs_source_t *source)
{
	if (obs_source_valid(source, "obs_source_dec_active"))
		obs_source_deactivate(source, MAIN_VIEW);
}

void obs_source_enum_filters(obs_source_t *source,
			     obs_source_enum_proc_t callback, void *param)
{
	if (!obs_source_valid(source, "obs_source_enum_filters"))
		return;
	if (!obs_ptr_valid(callback, "obs_source_enum_filters"))
		return;

	pthread_mutex_lock(&source->filter_mutex);

	for (size_t i = source->filters.num; i > 0; i--) {
		struct obs_source *filter = source->filters.array[i - 1];
		callback(source, filter, param);
	}

	pthread_mutex_unlock(&source->filter_mutex);
}

obs_source_t *obs_source_get_filter_by_name(obs_source_t *source,
					    const char *name)
{
	obs_source_t *filter = NULL;

	if (!obs_source_valid(source, "obs_source_get_filter_by_name"))
		return NULL;
	if (!obs_ptr_valid(name, "obs_source_get_filter_by_name"))
		return NULL;

	pthread_mutex_lock(&source->filter_mutex);

	for (size_t i = 0; i < source->filters.num; i++) {
		struct obs_source *cur_filter = source->filters.array[i];
		if (strcmp(cur_filter->context.name, name) == 0) {
			filter = cur_filter;
			obs_source_addref(filter);
			break;
		}
	}

	pthread_mutex_unlock(&source->filter_mutex);

	return filter;
}

bool obs_source_enabled(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_enabled") ? source->enabled
							      : false;
}

void obs_source_set_enabled(obs_source_t *source, bool enabled)
{
	struct calldata data;
	uint8_t stack[128];

	if (!obs_source_valid(source, "obs_source_set_enabled"))
		return;

	source->enabled = enabled;

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_bool(&data, "enabled", enabled);

	signal_handler_signal(source->context.signals, "enable", &data);
}

bool obs_source_muted(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_muted") ? source->user_muted
							    : false;
}

void obs_source_set_muted(obs_source_t *source, bool muted)
{
	struct calldata data;
	uint8_t stack[128];
	struct audio_action action = {.timestamp = os_gettime_ns(),
				      .type = AUDIO_ACTION_MUTE,
				      .set = muted};

	if (!obs_source_valid(source, "obs_source_set_muted"))
		return;

	source->user_muted = muted;

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_bool(&data, "muted", muted);

	signal_handler_signal(source->context.signals, "mute", &data);

	pthread_mutex_lock(&source->audio_actions_mutex);
	da_push_back(source->audio_actions, &action);
	pthread_mutex_unlock(&source->audio_actions_mutex);
}

static void source_signal_push_to_changed(obs_source_t *source,
					  const char *signal, bool enabled)
{
	struct calldata data;
	uint8_t stack[128];

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_bool(&data, "enabled", enabled);

	signal_handler_signal(source->context.signals, signal, &data);
}

static void source_signal_push_to_delay(obs_source_t *source,
					const char *signal, uint64_t delay)
{
	struct calldata data;
	uint8_t stack[128];

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	calldata_set_bool(&data, "delay", delay);

	signal_handler_signal(source->context.signals, signal, &data);
}

bool obs_source_push_to_mute_enabled(obs_source_t *source)
{
	bool enabled;
	if (!obs_source_valid(source, "obs_source_push_to_mute_enabled"))
		return false;

	pthread_mutex_lock(&source->audio_mutex);
	enabled = source->push_to_mute_enabled;
	pthread_mutex_unlock(&source->audio_mutex);

	return enabled;
}

void obs_source_enable_push_to_mute(obs_source_t *source, bool enabled)
{
	if (!obs_source_valid(source, "obs_source_enable_push_to_mute"))
		return;

	pthread_mutex_lock(&source->audio_mutex);
	bool changed = source->push_to_mute_enabled != enabled;
	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO && changed)
		blog(LOG_INFO, "source '%s' %s push-to-mute",
		     obs_source_get_name(source),
		     enabled ? "enabled" : "disabled");

	source->push_to_mute_enabled = enabled;

	if (changed)
		source_signal_push_to_changed(source, "push_to_mute_changed",
					      enabled);
	pthread_mutex_unlock(&source->audio_mutex);
}

uint64_t obs_source_get_push_to_mute_delay(obs_source_t *source)
{
	uint64_t delay;
	if (!obs_source_valid(source, "obs_source_get_push_to_mute_delay"))
		return 0;

	pthread_mutex_lock(&source->audio_mutex);
	delay = source->push_to_mute_delay;
	pthread_mutex_unlock(&source->audio_mutex);

	return delay;
}

void obs_source_set_push_to_mute_delay(obs_source_t *source, uint64_t delay)
{
	if (!obs_source_valid(source, "obs_source_set_push_to_mute_delay"))
		return;

	pthread_mutex_lock(&source->audio_mutex);
	source->push_to_mute_delay = delay;

	source_signal_push_to_delay(source, "push_to_mute_delay", delay);
	pthread_mutex_unlock(&source->audio_mutex);
}

bool obs_source_push_to_talk_enabled(obs_source_t *source)
{
	bool enabled;
	if (!obs_source_valid(source, "obs_source_push_to_talk_enabled"))
		return false;

	pthread_mutex_lock(&source->audio_mutex);
	enabled = source->push_to_talk_enabled;
	pthread_mutex_unlock(&source->audio_mutex);

	return enabled;
}

void obs_source_enable_push_to_talk(obs_source_t *source, bool enabled)
{
	if (!obs_source_valid(source, "obs_source_enable_push_to_talk"))
		return;

	pthread_mutex_lock(&source->audio_mutex);
	bool changed = source->push_to_talk_enabled != enabled;
	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO && changed)
		blog(LOG_INFO, "source '%s' %s push-to-talk",
		     obs_source_get_name(source),
		     enabled ? "enabled" : "disabled");

	source->push_to_talk_enabled = enabled;

	if (changed)
		source_signal_push_to_changed(source, "push_to_talk_changed",
					      enabled);
	pthread_mutex_unlock(&source->audio_mutex);
}

uint64_t obs_source_get_push_to_talk_delay(obs_source_t *source)
{
	uint64_t delay;
	if (!obs_source_valid(source, "obs_source_get_push_to_talk_delay"))
		return 0;

	pthread_mutex_lock(&source->audio_mutex);
	delay = source->push_to_talk_delay;
	pthread_mutex_unlock(&source->audio_mutex);

	return delay;
}

void obs_source_set_push_to_talk_delay(obs_source_t *source, uint64_t delay)
{
	if (!obs_source_valid(source, "obs_source_set_push_to_talk_delay"))
		return;

	pthread_mutex_lock(&source->audio_mutex);
	source->push_to_talk_delay = delay;

	source_signal_push_to_delay(source, "push_to_talk_delay", delay);
	pthread_mutex_unlock(&source->audio_mutex);
}

void *obs_source_get_type_data(obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_type_data")
		       ? source->info.type_data
		       : NULL;
}

static float get_source_volume(obs_source_t *source, uint64_t os_time)
{
	if (source->push_to_mute_enabled && source->push_to_mute_pressed)
		source->push_to_mute_stop_time =
			os_time + source->push_to_mute_delay * 1000000;

	if (source->push_to_talk_enabled && source->push_to_talk_pressed)
		source->push_to_talk_stop_time =
			os_time + source->push_to_talk_delay * 1000000;

	bool push_to_mute_active = source->push_to_mute_pressed ||
				   os_time < source->push_to_mute_stop_time;
	bool push_to_talk_active = source->push_to_talk_pressed ||
				   os_time < source->push_to_talk_stop_time;

	bool muted = !source->enabled || source->muted ||
		     (source->push_to_mute_enabled && push_to_mute_active) ||
		     (source->push_to_talk_enabled && !push_to_talk_active);

	if (muted || close_float(source->volume, 0.0f, 0.0001f))
		return 0.0f;
	if (close_float(source->volume, 1.0f, 0.0001f))
		return 1.0f;

	return source->volume;
}

static inline void multiply_output_audio(obs_source_t *source, size_t mix,
					 size_t channels, float vol)
{
	register float *out = source->audio_output_buf[mix][0];
	register float *end = out + AUDIO_OUTPUT_FRAMES * channels;

	while (out < end)
		*(out++) *= vol;
}

static inline void multiply_vol_data(obs_source_t *source, size_t mix,
				     size_t channels, float *vol_data)
{
	for (size_t ch = 0; ch < channels; ch++) {
		register float *out = source->audio_output_buf[mix][ch];
		register float *end = out + AUDIO_OUTPUT_FRAMES;
		register float *vol = vol_data;

		while (out < end)
			*(out++) *= *(vol++);
	}
}

static inline void apply_audio_action(obs_source_t *source,
				      const struct audio_action *action)
{
	switch (action->type) {
	case AUDIO_ACTION_VOL:
		source->volume = action->vol;
		break;
	case AUDIO_ACTION_MUTE:
		source->muted = action->set;
		break;
	case AUDIO_ACTION_PTT:
		source->push_to_talk_pressed = action->set;
		break;
	case AUDIO_ACTION_PTM:
		source->push_to_mute_pressed = action->set;
		break;
	}
}

static void apply_audio_actions(obs_source_t *source, size_t channels,
				size_t sample_rate)
{
	float vol_data[AUDIO_OUTPUT_FRAMES];
	float cur_vol = get_source_volume(source, source->audio_ts);
	size_t frame_num = 0;

	pthread_mutex_lock(&source->audio_actions_mutex);

	for (size_t i = 0; i < source->audio_actions.num; i++) {
		struct audio_action action = source->audio_actions.array[i];
		uint64_t timestamp = action.timestamp;
		size_t new_frame_num;

		if (timestamp < source->audio_ts)
			timestamp = source->audio_ts;

		new_frame_num = conv_time_to_frames(
			sample_rate, timestamp - source->audio_ts);

		if (new_frame_num >= AUDIO_OUTPUT_FRAMES)
			break;

		da_erase(source->audio_actions, i--);

		apply_audio_action(source, &action);

		if (new_frame_num > frame_num) {
			for (; frame_num < new_frame_num; frame_num++)
				vol_data[frame_num] = cur_vol;
		}

		cur_vol = get_source_volume(source, timestamp);
	}

	for (; frame_num < AUDIO_OUTPUT_FRAMES; frame_num++)
		vol_data[frame_num] = cur_vol;

	pthread_mutex_unlock(&source->audio_actions_mutex);

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		if ((source->audio_mixers & (1 << mix)) != 0)
			multiply_vol_data(source, mix, channels, vol_data);
	}
}

static void apply_audio_volume(obs_source_t *source, uint32_t mixers,
			       size_t channels, size_t sample_rate)
{
	struct audio_action action;
	bool actions_pending;
	float vol;

	pthread_mutex_lock(&source->audio_actions_mutex);

	actions_pending = source->audio_actions.num > 0;
	if (actions_pending)
		action = source->audio_actions.array[0];

	pthread_mutex_unlock(&source->audio_actions_mutex);

	if (actions_pending) {
		uint64_t duration =
			conv_frames_to_time(sample_rate, AUDIO_OUTPUT_FRAMES);

		if (action.timestamp < (source->audio_ts + duration)) {
			apply_audio_actions(source, channels, sample_rate);
			return;
		}
	}

	vol = get_source_volume(source, source->audio_ts);
	if (vol == 1.0f)
		return;

	if (vol == 0.0f || mixers == 0) {
		memset(source->audio_output_buf[0][0], 0,
		       AUDIO_OUTPUT_FRAMES * sizeof(float) *
			       MAX_AUDIO_CHANNELS * MAX_AUDIO_MIXES);
		return;
	}

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		uint32_t mix_and_val = (1 << mix);
		if ((source->audio_mixers & mix_and_val) != 0 &&
		    (mixers & mix_and_val) != 0)
			multiply_output_audio(source, mix, channels, vol);
	}
}

static void custom_audio_render(obs_source_t *source, uint32_t mixers,
				size_t channels, size_t sample_rate)
{
	struct obs_source_audio_mix audio_data;
	bool success;
	uint64_t ts;

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		for (size_t ch = 0; ch < channels; ch++) {
			audio_data.output[mix].data[ch] =
				source->audio_output_buf[mix][ch];
		}

		if ((source->audio_mixers & mixers & (1 << mix)) != 0) {
			memset(source->audio_output_buf[mix][0], 0,
			       sizeof(float) * AUDIO_OUTPUT_FRAMES * channels);
		}
	}

	success = source->info.audio_render(source->context.data, &ts,
					    &audio_data, mixers, channels,
					    sample_rate);
	source->audio_ts = success ? ts : 0;
	source->audio_pending = !success;

	if (!success || !source->audio_ts || !mixers)
		return;

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		uint32_t mix_bit = 1 << mix;

		if ((mixers & mix_bit) == 0)
			continue;

		if ((source->audio_mixers & mix_bit) == 0) {
			memset(source->audio_output_buf[mix][0], 0,
			       sizeof(float) * AUDIO_OUTPUT_FRAMES * channels);
		}
	}

	apply_audio_volume(source, mixers, channels, sample_rate);
}

static void audio_submix(obs_source_t *source, size_t channels,
			 size_t sample_rate)
{
	struct audio_output_data audio_data;
	struct obs_source_audio audio = {0};
	bool success;
	uint64_t ts;

	for (size_t ch = 0; ch < channels; ch++) {
		audio_data.data[ch] = source->audio_mix_buf[ch];
	}

	memset(source->audio_mix_buf[0], 0,
	       sizeof(float) * AUDIO_OUTPUT_FRAMES * channels);

	success = source->info.audio_mix(source->context.data, &ts, &audio_data,
					 channels, sample_rate);

	if (!success)
		return;

	for (size_t i = 0; i < channels; i++)
		audio.data[i] = (const uint8_t *)audio_data.data[i];

	audio.samples_per_sec = (uint32_t)sample_rate;
	audio.frames = AUDIO_OUTPUT_FRAMES;
	audio.format = AUDIO_FORMAT_FLOAT_PLANAR;
	audio.speakers = (enum speaker_layout)channels;
	audio.timestamp = ts;

	obs_source_output_audio(source, &audio);
}

static inline void process_audio_source_tick(obs_source_t *source,
					     uint32_t mixers, size_t channels,
					     size_t sample_rate, size_t size)
{
	bool audio_submix = !!(source->info.output_flags & OBS_SOURCE_SUBMIX);

	pthread_mutex_lock(&source->audio_buf_mutex);

	if (source->audio_input_buf[0].size < size) {
		source->audio_pending = true;
		pthread_mutex_unlock(&source->audio_buf_mutex);
		return;
	}

	for (size_t ch = 0; ch < channels; ch++)
		circlebuf_peek_front(&source->audio_input_buf[ch],
				     source->audio_output_buf[0][ch], size);

	pthread_mutex_unlock(&source->audio_buf_mutex);

	for (size_t mix = 1; mix < MAX_AUDIO_MIXES; mix++) {
		uint32_t mix_and_val = (1 << mix);

		if (audio_submix) {
			if (mix > 1)
				break;

			mixers = 1;
			mix_and_val = 1;
		}

		if ((source->audio_mixers & mix_and_val) == 0 ||
		    (mixers & mix_and_val) == 0) {
			memset(source->audio_output_buf[mix][0], 0,
			       size * channels);
			continue;
		}

		for (size_t ch = 0; ch < channels; ch++)
			memcpy(source->audio_output_buf[mix][ch],
			       source->audio_output_buf[0][ch], size);
	}

	if (audio_submix) {
		source->audio_pending = false;
		return;
	}

	if ((source->audio_mixers & 1) == 0 || (mixers & 1) == 0)
		memset(source->audio_output_buf[0][0], 0, size * channels);

	apply_audio_volume(source, mixers, channels, sample_rate);
	source->audio_pending = false;
}

void obs_source_audio_render(obs_source_t *source, uint32_t mixers,
			     size_t channels, size_t sample_rate, size_t size)
{
	if (!source->audio_output_buf[0][0]) {
		source->audio_pending = true;
		return;
	}

	if (source->info.audio_render) {
		if (!source->context.data) {
			source->audio_pending = true;
			return;
		}
		custom_audio_render(source, mixers, channels, sample_rate);
		return;
	}

	if (source->info.audio_mix) {
		audio_submix(source, channels, sample_rate);
	}

	if (!source->audio_ts) {
		source->audio_pending = true;
		return;
	}

	process_audio_source_tick(source, mixers, channels, sample_rate, size);
}

bool obs_source_audio_pending(const obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_audio_pending"))
		return true;

	return (is_composite_source(source) || is_audio_source(source))
		       ? source->audio_pending
		       : true;
}

uint64_t obs_source_get_audio_timestamp(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_audio_timestamp")
		       ? source->audio_ts
		       : 0;
}

void obs_source_get_audio_mix(const obs_source_t *source,
			      struct obs_source_audio_mix *audio)
{
	if (!obs_source_valid(source, "obs_source_get_audio_mix"))
		return;
	if (!obs_ptr_valid(audio, "audio"))
		return;

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		for (size_t ch = 0; ch < MAX_AUDIO_CHANNELS; ch++) {
			audio->output[mix].data[ch] =
				source->audio_output_buf[mix][ch];
		}
	}
}

void obs_source_add_audio_capture_callback(obs_source_t *source,
					   obs_source_audio_capture_t callback,
					   void *param)
{
	struct audio_cb_info info = {callback, param};

	if (!obs_source_valid(source, "obs_source_add_audio_capture_callback"))
		return;

	pthread_mutex_lock(&source->audio_cb_mutex);
	da_push_back(source->audio_cb_list, &info);
	pthread_mutex_unlock(&source->audio_cb_mutex);
}

void obs_source_remove_audio_capture_callback(
	obs_source_t *source, obs_source_audio_capture_t callback, void *param)
{
	struct audio_cb_info info = {callback, param};

	if (!obs_source_valid(source,
			      "obs_source_remove_audio_capture_callback"))
		return;

	pthread_mutex_lock(&source->audio_cb_mutex);
	da_erase_item(source->audio_cb_list, &info);
	pthread_mutex_unlock(&source->audio_cb_mutex);
}

void obs_source_set_monitoring_type(obs_source_t *source,
				    enum obs_monitoring_type type)
{
	bool was_on;
	bool now_on;

	if (!obs_source_valid(source, "obs_source_set_monitoring_type"))
		return;
	if (source->monitoring_type == type)
		return;

	was_on = source->monitoring_type != OBS_MONITORING_TYPE_NONE;
	now_on = type != OBS_MONITORING_TYPE_NONE;

	if (was_on != now_on) {
		if (!was_on) {
			source->monitor = audio_monitor_create(source);
		} else {
			audio_monitor_destroy(source->monitor);
			source->monitor = NULL;
		}
	}

	source->monitoring_type = type;
}

enum obs_monitoring_type
obs_source_get_monitoring_type(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_monitoring_type")
		       ? source->monitoring_type
		       : OBS_MONITORING_TYPE_NONE;
}

void obs_source_set_async_unbuffered(obs_source_t *source, bool unbuffered)
{
	if (!obs_source_valid(source, "obs_source_set_async_unbuffered"))
		return;

	source->async_unbuffered = unbuffered;
}

bool obs_source_async_unbuffered(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_async_unbuffered")
		       ? source->async_unbuffered
		       : false;
}

obs_data_t *obs_source_get_private_settings(obs_source_t *source)
{
	if (!obs_ptr_valid(source, "obs_source_get_private_settings"))
		return NULL;

	obs_data_addref(source->private_settings);
	return source->private_settings;
}

void obs_source_set_async_decoupled(obs_source_t *source, bool decouple)
{
	if (!obs_ptr_valid(source, "obs_source_set_async_decoupled"))
		return;

	source->async_decoupled = decouple;
	if (decouple) {
		pthread_mutex_lock(&source->audio_buf_mutex);
		source->timing_set = false;
		reset_audio_data(source, 0);
		pthread_mutex_unlock(&source->audio_buf_mutex);
	}
}

bool obs_source_async_decoupled(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_async_decoupled")
		       ? source->async_decoupled
		       : false;
}

/* hidden/undocumented export to allow source type redefinition for scripts */
EXPORT void obs_enable_source_type(const char *name, bool enable)
{
	struct obs_source_info *info = get_source_info(name);
	if (!info)
		return;

	if (enable)
		info->output_flags &= ~OBS_SOURCE_CAP_DISABLED;
	else
		info->output_flags |= OBS_SOURCE_CAP_DISABLED;
}

enum speaker_layout obs_source_get_speaker_layout(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_get_audio_channels"))
		return SPEAKERS_UNKNOWN;

	return source->sample_info.speakers;
}

void obs_source_set_balance_value(obs_source_t *source, float balance)
{
	if (!obs_source_valid(source, "obs_source_set_balance_value"))
		return;

	source->balance = balance;
}

float obs_source_get_balance_value(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_balance_value")
		       ? source->balance
		       : 0.5f;
}

void obs_source_set_audio_active(obs_source_t *source, bool active)
{
	if (!obs_source_valid(source, "obs_source_set_audio_active"))
		return;

	if (os_atomic_set_bool(&source->audio_active, active) == active)
		return;

	if (active)
		obs_source_dosignal(source, "source_audio_activate",
				    "audio_activate");
	else
		obs_source_dosignal(source, "source_audio_deactivate",
				    "audio_deactivate");
}

bool obs_source_audio_active(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_audio_active")
		       ? os_atomic_load_bool(&source->audio_active)
		       : false;
}

uint32_t obs_source_get_last_obs_version(const obs_source_t *source)
{
	return obs_source_valid(source, "obs_source_get_last_obs_version")
		       ? source->last_obs_ver
		       : 0;
}

enum obs_icon_type obs_source_get_icon_type(const char *id)
{
	const struct obs_source_info *info = get_source_info(id);
	return (info) ? info->icon_type : OBS_ICON_TYPE_UNKNOWN;
}

void obs_source_media_play_pause(obs_source_t *source, bool pause)
{
	if (!data_valid(source, "obs_source_media_play_pause"))
		return;

	if (!source->info.media_play_pause)
		return;

	source->info.media_play_pause(source->context.data, pause);

	if (pause)
		obs_source_dosignal(source, NULL, "media_pause");
	else
		obs_source_dosignal(source, NULL, "media_play");
}

void obs_source_media_restart(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_restart"))
		return;

	if (!source->info.media_restart)
		return;

	source->info.media_restart(source->context.data);

	obs_source_dosignal(source, NULL, "media_restart");
}

void obs_source_media_stop(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_stop"))
		return;

	if (!source->info.media_stop)
		return;

	source->info.media_stop(source->context.data);

	obs_source_dosignal(source, NULL, "media_stopped");
}

void obs_source_media_next(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_next"))
		return;

	if (!source->info.media_next)
		return;

	source->info.media_next(source->context.data);

	obs_source_dosignal(source, NULL, "media_next");
}

void obs_source_media_previous(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_previous"))
		return;

	if (!source->info.media_previous)
		return;

	source->info.media_previous(source->context.data);

	obs_source_dosignal(source, NULL, "media_previous");
}

int64_t obs_source_media_get_duration(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_get_duration"))
		return 0;

	if (source->info.media_get_duration)
		return source->info.media_get_duration(source->context.data);
	else
		return 0;
}

int64_t obs_source_media_get_time(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_get_time"))
		return 0;

	if (source->info.media_get_time)
		return source->info.media_get_time(source->context.data);
	else
		return 0;
}

void obs_source_media_set_time(obs_source_t *source, int64_t ms)
{
	if (!data_valid(source, "obs_source_media_set_time"))
		return;

	if (source->info.media_set_time)
		source->info.media_set_time(source->context.data, ms);
}

enum obs_media_state obs_source_media_get_state(obs_source_t *source)
{
	if (!data_valid(source, "obs_source_media_get_state"))
		return OBS_MEDIA_STATE_NONE;

	if (source->info.media_get_state)
		return source->info.media_get_state(source->context.data);
	else
		return OBS_MEDIA_STATE_NONE;
}

void obs_source_media_started(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_media_started"))
		return;

	obs_source_dosignal(source, NULL, "media_started");
}

void obs_source_media_ended(obs_source_t *source)
{
	if (!obs_source_valid(source, "obs_source_media_ended"))
		return;

	obs_source_dosignal(source, NULL, "media_ended");
}
