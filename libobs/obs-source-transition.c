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

#include "obs-internal.h"
#include "util/util_uint64.h"
#include "graphics/math-extra.h"

#define lock_transition(transition) \
	pthread_mutex_lock(&transition->transition_mutex);
#define unlock_transition(transition) \
	pthread_mutex_unlock(&transition->transition_mutex);

#define trylock_textures(transition) \
	pthread_mutex_trylock(&transition->transition_tex_mutex)
#define lock_textures(transition) \
	pthread_mutex_lock(&transition->transition_tex_mutex)
#define unlock_textures(transition) \
	pthread_mutex_unlock(&transition->transition_tex_mutex)

static inline bool transition_valid(const obs_source_t *transition,
				    const char *func)
{
	if (!obs_ptr_valid(transition, func))
		return false;
	else if (transition->info.type != OBS_SOURCE_TYPE_TRANSITION)
		return false;

	return true;
}

bool obs_transition_init(obs_source_t *transition)
{
	pthread_mutex_init_value(&transition->transition_mutex);
	pthread_mutex_init_value(&transition->transition_tex_mutex);
	if (pthread_mutex_init(&transition->transition_mutex, NULL) != 0)
		return false;
	if (pthread_mutex_init(&transition->transition_tex_mutex, NULL) != 0)
		return false;

	transition->transition_alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	transition->transition_texrender[0] =
		gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	transition->transition_texrender[1] =
		gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	transition->transition_source_active[0] = true;

	return transition->transition_texrender[0] != NULL &&
	       transition->transition_texrender[1] != NULL;
}

void obs_transition_free(obs_source_t *transition)
{
	pthread_mutex_destroy(&transition->transition_mutex);
	pthread_mutex_destroy(&transition->transition_tex_mutex);

	gs_enter_context(obs->video.graphics);
	gs_texrender_destroy(transition->transition_texrender[0]);
	gs_texrender_destroy(transition->transition_texrender[1]);
	gs_leave_context();
}

void obs_transition_clear(obs_source_t *transition)
{
	obs_source_t *s[2];
	bool active[2];

	if (!transition_valid(transition, "obs_transition_clear"))
		return;

	lock_transition(transition);
	for (size_t i = 0; i < 2; i++) {
		s[i] = transition->transition_sources[i];
		active[i] = transition->transition_source_active[i];
		transition->transition_sources[i] = NULL;
		transition->transition_source_active[i] = false;
	}
	transition->transitioning_video = false;
	transition->transitioning_audio = false;
	unlock_transition(transition);

	for (size_t i = 0; i < 2; i++) {
		if (s[i] && active[i])
			obs_source_remove_active_child(transition, s[i]);
		obs_source_release(s[i]);
	}
}

void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy);

static inline uint32_t get_cx(obs_source_t *tr)
{
	return tr->transition_cx ? tr->transition_cx : tr->transition_actual_cx;
}

static inline uint32_t get_cy(obs_source_t *tr)
{
	return tr->transition_cy ? tr->transition_cy : tr->transition_actual_cy;
}

static void recalculate_transition_matrix(obs_source_t *tr, size_t idx)
{
	obs_source_t *child;
	struct matrix4 mat;
	struct vec2 pos;
	struct vec2 scale;
	float tr_cx = (float)get_cx(tr);
	float tr_cy = (float)get_cy(tr);
	float source_cx;
	float source_cy;
	float tr_aspect = tr_cx / tr_cy;
	float source_aspect;
	enum obs_transition_scale_type scale_type = tr->transition_scale_type;

	lock_transition(tr);

	child = tr->transition_sources[idx];
	if (!child) {
		unlock_transition(tr);
		return;
	}

	source_cx = (float)obs_source_get_width(child);
	source_cy = (float)obs_source_get_height(child);
	unlock_transition(tr);

	if (source_cx == 0.0f || source_cy == 0.0f)
		return;

	source_aspect = source_cx / source_cy;

	if (scale_type == OBS_TRANSITION_SCALE_MAX_ONLY) {
		if (source_cx > tr_cx || source_cy > tr_cy) {
			scale_type = OBS_TRANSITION_SCALE_ASPECT;
		} else {
			scale.x = 1.0f;
			scale.y = 1.0f;
		}
	}

	if (scale_type == OBS_TRANSITION_SCALE_ASPECT) {
		bool use_width = tr_aspect < source_aspect;
		scale.x = scale.y = use_width ? tr_cx / source_cx
					      : tr_cy / source_cy;

	} else if (scale_type == OBS_TRANSITION_SCALE_STRETCH) {
		scale.x = tr_cx / source_cx;
		scale.y = tr_cy / source_cy;
	}

	source_cx *= scale.x;
	source_cy *= scale.y;

	vec2_zero(&pos);
	add_alignment(&pos, tr->transition_alignment, (int)(tr_cx - source_cx),
		      (int)(tr_cy - source_cy));

	matrix4_identity(&mat);
	matrix4_scale3f(&mat, &mat, scale.x, scale.y, 1.0f);
	matrix4_translate3f(&mat, &mat, pos.x, pos.y, 0.0f);
	matrix4_copy(&tr->transition_matrices[idx], &mat);
}

static inline void recalculate_transition_matrices(obs_source_t *transition)
{
	recalculate_transition_matrix(transition, 0);
	recalculate_transition_matrix(transition, 1);
}

static void recalculate_transition_size(obs_source_t *transition)
{
	uint32_t cx = 0, cy = 0;
	obs_source_t *child;

	lock_transition(transition);

	for (size_t i = 0; i < 2; i++) {
		child = transition->transition_sources[i];
		if (child) {
			uint32_t new_cx = obs_source_get_width(child);
			uint32_t new_cy = obs_source_get_height(child);
			if (new_cx > cx)
				cx = new_cx;
			if (new_cy > cy)
				cy = new_cy;
		}
	}

	unlock_transition(transition);

	transition->transition_actual_cx = cx;
	transition->transition_actual_cy = cy;
}

void obs_transition_tick(obs_source_t *transition, float t)
{
	recalculate_transition_size(transition);
	recalculate_transition_matrices(transition);

	if (transition->transition_mode == OBS_TRANSITION_MODE_MANUAL) {
		if (transition->transition_manual_torque == 0.0f) {
			transition->transition_manual_val =
				transition->transition_manual_target;
		} else {
			transition->transition_manual_val = calc_torquef(
				transition->transition_manual_val,
				transition->transition_manual_target,
				transition->transition_manual_torque,
				transition->transition_manual_clamp, t);
		}
	}

	if (trylock_textures(transition) == 0) {
		gs_texrender_reset(transition->transition_texrender[0]);
		gs_texrender_reset(transition->transition_texrender[1]);
		unlock_textures(transition);
	}
}

static void
set_source(obs_source_t *transition, enum obs_transition_target target,
	   obs_source_t *new_child,
	   bool (*callback)(obs_source_t *t, size_t idx, obs_source_t *c))
{
	size_t idx = (size_t)target;
	obs_source_t *old_child;
	bool add_success = true;
	bool already_active;

	if (new_child)
		obs_source_addref(new_child);

	lock_transition(transition);

	old_child = transition->transition_sources[idx];

	if (new_child == old_child) {
		unlock_transition(transition);
		obs_source_release(new_child);
		return;
	}

	already_active = transition->transition_source_active[idx];

	if (already_active) {
		if (new_child)
			add_success = obs_source_add_active_child(transition,
								  new_child);
		if (old_child && add_success)
			obs_source_remove_active_child(transition, old_child);
	}

	if (callback && add_success)
		add_success = callback(transition, idx, new_child);

	transition->transition_sources[idx] = add_success ? new_child : NULL;

	unlock_transition(transition);

	if (add_success) {
		if (transition->transition_cx == 0 ||
		    transition->transition_cy == 0) {
			recalculate_transition_size(transition);
			recalculate_transition_matrices(transition);
		}
	} else {
		obs_source_release(new_child);
	}

	obs_source_release(old_child);
}

obs_source_t *obs_transition_get_source(obs_source_t *transition,
					enum obs_transition_target target)
{
	size_t idx = (size_t)target;
	obs_source_t *ret;

	if (!transition_valid(transition, "obs_transition_get_source"))
		return NULL;

	lock_transition(transition);
	ret = transition->transition_sources[idx];
	obs_source_addref(ret);
	unlock_transition(transition);

	return ret;
}

obs_source_t *obs_transition_get_active_source(obs_source_t *transition)
{
	obs_source_t *ret;

	if (!transition_valid(transition, "obs_transition_get_source"))
		return NULL;

	lock_transition(transition);
	if (transition->transitioning_audio || transition->transitioning_video)
		ret = transition->transition_sources[1];
	else
		ret = transition->transition_sources[0];
	obs_source_addref(ret);
	unlock_transition(transition);

	return ret;
}

static inline bool activate_child(obs_source_t *transition, size_t idx)
{
	bool success = true;

	lock_transition(transition);

	if (transition->transition_sources[idx] &&
	    !transition->transition_source_active[idx]) {

		success = obs_source_add_active_child(
			transition, transition->transition_sources[idx]);
		if (success)
			transition->transition_source_active[idx] = true;
	}

	unlock_transition(transition);

	return success;
}

static bool activate_transition(obs_source_t *transition, size_t idx,
				obs_source_t *child)
{
	if (!transition->transition_source_active[idx]) {
		if (!obs_source_add_active_child(transition, child))
			return false;

		transition->transition_source_active[idx] = true;
	}

	transition->transitioning_video = true;
	transition->transitioning_audio = true;
	return true;
}

static inline bool transition_active(obs_source_t *transition)
{
	return transition->transitioning_audio ||
	       transition->transitioning_video;
}

bool obs_transition_start(obs_source_t *transition,
			  enum obs_transition_mode mode, uint32_t duration_ms,
			  obs_source_t *dest)
{
	bool active;
	bool same_as_source;
	bool same_as_dest;
	bool same_mode;

	if (!transition_valid(transition, "obs_transition_start"))
		return false;

	lock_transition(transition);
	same_as_source = dest == transition->transition_sources[0];
	same_as_dest = dest == transition->transition_sources[1];
	same_mode = mode == transition->transition_mode;
	active = transition_active(transition);
	unlock_transition(transition);

	if (same_as_source && !active)
		return false;
	if (active && mode == OBS_TRANSITION_MODE_MANUAL && same_mode &&
	    same_as_dest)
		return true;

	lock_transition(transition);
	transition->transition_mode = mode;
	transition->transition_manual_val = 0.0f;
	transition->transition_manual_target = 0.0f;
	unlock_transition(transition);

	if (transition->info.transition_start)
		transition->info.transition_start(transition->context.data);

	if (transition->transition_use_fixed_duration)
		duration_ms = transition->transition_fixed_duration;

	if (!active || (!same_as_dest && !same_as_source)) {
		transition->transition_start_time = os_gettime_ns();
		transition->transition_duration =
			(uint64_t)duration_ms * 1000000ULL;
	}

	set_source(transition, OBS_TRANSITION_SOURCE_B, dest,
		   activate_transition);
	if (dest == NULL && same_as_dest && !same_as_source) {
		transition->transitioning_video = true;
		transition->transitioning_audio = true;
	}

	obs_source_dosignal(transition, "source_transition_start",
			    "transition_start");

	recalculate_transition_size(transition);
	recalculate_transition_matrices(transition);

	return true;
}

void obs_transition_set_manual_torque(obs_source_t *transition, float torque,
				      float clamp)
{
	lock_transition(transition);
	transition->transition_manual_torque = torque;
	transition->transition_manual_clamp = clamp;
	unlock_transition(transition);
}

void obs_transition_set_manual_time(obs_source_t *transition, float t)
{
	lock_transition(transition);
	transition->transition_manual_target = t;
	unlock_transition(transition);
}

void obs_transition_set(obs_source_t *transition, obs_source_t *source)
{
	obs_source_t *s[2];
	bool active[2];

	if (!transition_valid(transition, "obs_transition_clear"))
		return;

	obs_source_addref(source);

	lock_transition(transition);
	for (size_t i = 0; i < 2; i++) {
		s[i] = transition->transition_sources[i];
		active[i] = transition->transition_source_active[i];
		transition->transition_sources[i] = NULL;
		transition->transition_source_active[i] = false;
	}
	transition->transition_source_active[0] = true;
	transition->transition_sources[0] = source;
	transition->transitioning_video = false;
	transition->transitioning_audio = false;
	transition->transition_manual_val = 0.0f;
	transition->transition_manual_target = 0.0f;
	unlock_transition(transition);

	for (size_t i = 0; i < 2; i++) {
		if (s[i] && active[i])
			obs_source_remove_active_child(transition, s[i]);
		obs_source_release(s[i]);
	}

	if (source)
		obs_source_add_active_child(transition, source);
}

static float calc_time(obs_source_t *transition, uint64_t ts)
{
	if (transition->transition_mode == OBS_TRANSITION_MODE_MANUAL)
		return transition->transition_manual_val;

	uint64_t end;

	if (ts <= transition->transition_start_time)
		return 0.0f;

	end = transition->transition_duration;
	ts -= transition->transition_start_time;
	if (ts >= end || end == 0)
		return 1.0f;

	return (float)((long double)ts / (long double)end);
}

static inline float get_video_time(obs_source_t *transition)
{
	uint64_t ts = obs->video.video_time;
	return calc_time(transition, ts);
}

float obs_transition_get_time(obs_source_t *transition)
{
	return get_video_time(transition);
}

static inline gs_texture_t *get_texture(obs_source_t *transition,
					enum obs_transition_target target)
{
	size_t idx = (size_t)target;
	return gs_texrender_get_texture(transition->transition_texrender[idx]);
}

void obs_transition_set_scale_type(obs_source_t *transition,
				   enum obs_transition_scale_type type)
{
	if (!transition_valid(transition, "obs_transition_set_scale_type"))
		return;

	transition->transition_scale_type = type;
}

enum obs_transition_scale_type
obs_transition_get_scale_type(const obs_source_t *transition)
{
	return transition_valid(transition, "obs_transition_get_scale_type")
		       ? transition->transition_scale_type
		       : OBS_TRANSITION_SCALE_MAX_ONLY;
}

void obs_transition_set_alignment(obs_source_t *transition, uint32_t alignment)
{
	if (!transition_valid(transition, "obs_transition_set_alignment"))
		return;

	transition->transition_alignment = alignment;
}

uint32_t obs_transition_get_alignment(const obs_source_t *transition)
{
	return transition_valid(transition, "obs_transition_get_alignment")
		       ? transition->transition_alignment
		       : 0;
}

void obs_transition_set_size(obs_source_t *transition, uint32_t cx, uint32_t cy)
{
	if (!transition_valid(transition, "obs_transition_set_size"))
		return;

	transition->transition_cx = cx;
	transition->transition_cy = cy;
}

void obs_transition_get_size(const obs_source_t *transition, uint32_t *cx,
			     uint32_t *cy)
{
	if (!transition_valid(transition, "obs_transition_set_size")) {
		*cx = 0;
		*cy = 0;
		return;
	}

	*cx = transition->transition_cx;
	*cy = transition->transition_cy;
}

void obs_transition_save(obs_source_t *tr, obs_data_t *data)
{
	obs_source_t *child;

	lock_transition(tr);
	child = transition_active(tr) ? tr->transition_sources[1]
				      : tr->transition_sources[0];

	obs_data_set_string(data, "transition_source_a",
			    child ? child->context.name : "");
	obs_data_set_int(data, "transition_alignment",
			 tr->transition_alignment);
	obs_data_set_int(data, "transition_mode", (int64_t)tr->transition_mode);
	obs_data_set_int(data, "transition_scale_type",
			 (int64_t)tr->transition_scale_type);
	obs_data_set_int(data, "transition_cx", tr->transition_cx);
	obs_data_set_int(data, "transition_cy", tr->transition_cy);
	unlock_transition(tr);
}

void obs_transition_load(obs_source_t *tr, obs_data_t *data)
{
	const char *name = obs_data_get_string(data, "transition_source_a");
	int64_t alignment = obs_data_get_int(data, "transition_alignment");
	int64_t mode = obs_data_get_int(data, "transition_mode");
	int64_t scale_type = obs_data_get_int(data, "transition_scale_type");
	int64_t cx = obs_data_get_int(data, "transition_cx");
	int64_t cy = obs_data_get_int(data, "transition_cy");
	obs_source_t *source = NULL;

	if (name) {
		source = obs_get_source_by_name(name);
		if (source) {
			if (!obs_source_add_active_child(tr, source)) {
				blog(LOG_WARNING,
				     "Cannot set transition '%s' "
				     "to source '%s' due to "
				     "infinite recursion",
				     tr->context.name, name);
				obs_source_release(source);
				source = NULL;
			}
		} else {
			blog(LOG_WARNING,
			     "Failed to find source '%s' for "
			     "transition '%s'",
			     name, tr->context.name);
		}
	}

	lock_transition(tr);
	tr->transition_sources[0] = source;
	tr->transition_source_active[0] = true;
	tr->transition_alignment = (uint32_t)alignment;
	tr->transition_mode = (enum obs_transition_mode)mode;
	tr->transition_scale_type = (enum obs_transition_scale_type)scale_type;
	tr->transition_cx = (uint32_t)cx;
	tr->transition_cy = (uint32_t)cy;
	unlock_transition(tr);

	recalculate_transition_size(tr);
	recalculate_transition_matrices(tr);
}

struct transition_state {
	obs_source_t *s[2];
	bool transitioning_video;
	bool transitioning_audio;
};

static inline void copy_transition_state(obs_source_t *transition,
					 struct transition_state *state)
{
	state->s[0] = transition->transition_sources[0];
	state->s[1] = transition->transition_sources[1];
	obs_source_addref(state->s[0]);
	obs_source_addref(state->s[1]);

	state->transitioning_video = transition->transitioning_video;
	state->transitioning_audio = transition->transitioning_audio;
}

static inline void enum_child(obs_source_t *tr, obs_source_t *child,
			      obs_source_enum_proc_t enum_callback, void *param)
{
	if (!child)
		return;

	if (child->context.data && child->info.enum_active_sources)
		child->info.enum_active_sources(child->context.data,
						enum_callback, param);

	enum_callback(tr, child, param);
}

void obs_transition_enum_sources(obs_source_t *transition,
				 obs_source_enum_proc_t cb, void *param)
{
	lock_transition(transition);
	for (size_t i = 0; i < 2; i++) {
		if (transition->transition_sources[i])
			cb(transition, transition->transition_sources[i],
			   param);
	}
	unlock_transition(transition);
}

static inline void render_child(obs_source_t *transition, obs_source_t *child,
				size_t idx)
{
	uint32_t cx = get_cx(transition);
	uint32_t cy = get_cy(transition);
	struct vec4 blank;
	if (!child)
		return;

	if (gs_texrender_begin(transition->transition_texrender[idx], cx, cy)) {
		vec4_zero(&blank);
		gs_clear(GS_CLEAR_COLOR, &blank, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

		gs_matrix_push();
		gs_matrix_mul(&transition->transition_matrices[idx]);
		obs_source_video_render(child);
		gs_matrix_pop();

		gs_texrender_end(transition->transition_texrender[idx]);
	}
}

static void obs_transition_stop(obs_source_t *transition)
{
	obs_source_t *old_child = transition->transition_sources[0];

	if (old_child && transition->transition_source_active[0])
		obs_source_remove_active_child(transition, old_child);
	obs_source_release(old_child);

	transition->transition_source_active[0] = true;
	transition->transition_source_active[1] = false;
	transition->transition_sources[0] = transition->transition_sources[1];
	transition->transition_sources[1] = NULL;
}

static inline void handle_stop(obs_source_t *transition)
{
	if (transition->info.transition_stop)
		transition->info.transition_stop(transition->context.data);
	obs_source_dosignal(transition, "source_transition_stop",
			    "transition_stop");
}

void obs_transition_force_stop(obs_source_t *transition)
{
	handle_stop(transition);
}

void obs_transition_video_render(obs_source_t *transition,
				 obs_transition_video_render_callback_t callback)
{
	struct transition_state state;
	struct matrix4 matrices[2];
	bool locked = false;
	bool stopped = false;
	bool video_stopped = false;
	float t;

	if (!transition_valid(transition, "obs_transition_video_render"))
		return;

	t = get_video_time(transition);

	lock_transition(transition);

	if (t >= 1.0f && transition->transitioning_video) {
		transition->transitioning_video = false;
		video_stopped = true;

		if (!transition->transitioning_audio) {
			obs_transition_stop(transition);
			stopped = true;
		}
	}
	copy_transition_state(transition, &state);
	matrices[0] = transition->transition_matrices[0];
	matrices[1] = transition->transition_matrices[1];

	unlock_transition(transition);

	if (state.transitioning_video)
		locked = trylock_textures(transition) == 0;

	if (state.transitioning_video && locked && callback) {
		gs_texture_t *tex[2];
		uint32_t cx;
		uint32_t cy;

		for (size_t i = 0; i < 2; i++) {
			if (state.s[i]) {
				render_child(transition, state.s[i], i);
				tex[i] = get_texture(transition, i);
				if (!tex[i])
					tex[i] = obs->video.transparent_texture;
			} else {
				tex[i] = obs->video.transparent_texture;
			}
		}

		cx = get_cx(transition);
		cy = get_cy(transition);
		if (cx && cy) {
			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

			callback(transition->context.data, tex[0], tex[1], t,
				 cx, cy);

			gs_blend_state_pop();
		}

	} else if (state.transitioning_audio) {
		if (state.s[1]) {
			gs_matrix_push();
			gs_matrix_mul(&matrices[1]);
			obs_source_video_render(state.s[1]);
			gs_matrix_pop();
		}
	} else {
		if (state.s[0]) {
			gs_matrix_push();
			gs_matrix_mul(&matrices[0]);
			obs_source_video_render(state.s[0]);
			gs_matrix_pop();
		}
	}

	if (locked)
		unlock_textures(transition);

	obs_source_release(state.s[0]);
	obs_source_release(state.s[1]);

	if (video_stopped)
		obs_source_dosignal(transition, "source_transition_video_stop",
				    "transition_video_stop");
	if (stopped)
		handle_stop(transition);
}

bool obs_transition_video_render_direct(obs_source_t *transition,
					enum obs_transition_target target)
{
	struct transition_state state;
	struct matrix4 matrices[2];
	bool stopped = false;
	bool video_stopped = false;
	bool render_b = target == OBS_TRANSITION_SOURCE_B;
	bool transitioning;
	float t;

	if (!transition_valid(transition, "obs_transition_video_render"))
		return false;

	t = get_video_time(transition);

	lock_transition(transition);

	if (t >= 1.0f && transition->transitioning_video) {
		transition->transitioning_video = false;
		video_stopped = true;

		if (!transition->transitioning_audio) {
			obs_transition_stop(transition);
			stopped = true;
		}
	}
	copy_transition_state(transition, &state);
	transitioning = state.transitioning_audio || state.transitioning_video;
	matrices[0] = transition->transition_matrices[0];
	matrices[1] = transition->transition_matrices[1];

	unlock_transition(transition);

	int idx = (transitioning && render_b) ? 1 : 0;
	if (state.s[idx]) {
		gs_matrix_push();
		gs_matrix_mul(&matrices[idx]);
		obs_source_video_render(state.s[idx]);
		gs_matrix_pop();
	}

	obs_source_release(state.s[0]);
	obs_source_release(state.s[1]);

	if (video_stopped)
		obs_source_dosignal(transition, "source_transition_video_stop",
				    "transition_video_stop");
	if (stopped)
		handle_stop(transition);

	return transitioning;
}

static inline float get_sample_time(obs_source_t *transition,
				    size_t sample_rate, size_t sample,
				    uint64_t ts)
{
	uint64_t sample_ts_offset =
		util_mul_div64(sample, 1000000000ULL, sample_rate);
	uint64_t i_ts = ts + sample_ts_offset;
	return calc_time(transition, i_ts);
}

static inline void mix_child(obs_source_t *transition, float *out, float *in,
			     size_t count, size_t sample_rate, uint64_t ts,
			     obs_transition_audio_mix_callback_t mix)
{
	void *context_data = transition->context.data;

	for (size_t i = 0; i < count; i++) {
		float t = get_sample_time(transition, sample_rate, i, ts);
		out[i] += in[i] * mix(context_data, t);
	}
}

static void process_audio(obs_source_t *transition, obs_source_t *child,
			  struct obs_source_audio_mix *audio, uint64_t min_ts,
			  uint32_t mixers, size_t channels, size_t sample_rate,
			  obs_transition_audio_mix_callback_t mix)
{
	bool valid = child && !child->audio_pending && child->audio_ts;
	struct obs_source_audio_mix child_audio;
	uint64_t ts;
	size_t pos;

	if (!valid)
		return;

	ts = child->audio_ts;
	obs_source_get_audio_mix(child, &child_audio);
	pos = (size_t)ns_to_audio_frames(sample_rate, ts - min_ts);

	if (pos > AUDIO_OUTPUT_FRAMES)
		return;

	for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; mix_idx++) {
		struct audio_output_data *output = &audio->output[mix_idx];
		struct audio_output_data *input = &child_audio.output[mix_idx];

		if ((mixers & (1 << mix_idx)) == 0)
			continue;

		for (size_t ch = 0; ch < channels; ch++) {
			float *out = output->data[ch];
			float *in = input->data[ch];

			mix_child(transition, out + pos, in,
				  AUDIO_OUTPUT_FRAMES - pos, sample_rate, ts,
				  mix);
		}
	}
}

static inline uint64_t calc_min_ts(obs_source_t *sources[2])
{
	uint64_t min_ts = 0;

	for (size_t i = 0; i < 2; i++) {
		if (sources[i] && !sources[i]->audio_pending &&
		    sources[i]->audio_ts) {
			if (!min_ts || sources[i]->audio_ts < min_ts)
				min_ts = sources[i]->audio_ts;
		}
	}

	return min_ts;
}

static inline bool stop_audio(obs_source_t *transition)
{
	transition->transitioning_audio = false;
	if (!transition->transitioning_video) {
		obs_transition_stop(transition);
		return true;
	}

	return false;
}

bool obs_transition_audio_render(obs_source_t *transition, uint64_t *ts_out,
				 struct obs_source_audio_mix *audio,
				 uint32_t mixers, size_t channels,
				 size_t sample_rate,
				 obs_transition_audio_mix_callback_t mix_a,
				 obs_transition_audio_mix_callback_t mix_b)
{
	obs_source_t *sources[2];
	struct transition_state state = {0};
	bool stopped = false;
	uint64_t min_ts;
	float t;

	if (!transition_valid(transition, "obs_transition_audio_render"))
		return false;

	lock_transition(transition);

	sources[0] = transition->transition_sources[0];
	sources[1] = transition->transition_sources[1];

	min_ts = calc_min_ts(sources);

	if (min_ts) {
		t = calc_time(transition, min_ts);

		if (t >= 1.0f && transition->transitioning_audio)
			stopped = stop_audio(transition);

		sources[0] = transition->transition_sources[0];
		sources[1] = transition->transition_sources[1];
		min_ts = calc_min_ts(sources);
		if (min_ts)
			copy_transition_state(transition, &state);

	} else if (!transition->transitioning_video &&
		   transition->transitioning_audio) {
		stopped = stop_audio(transition);
	}

	unlock_transition(transition);

	if (min_ts) {
		if (state.transitioning_audio) {
			if (state.s[0])
				process_audio(transition, state.s[0], audio,
					      min_ts, mixers, channels,
					      sample_rate, mix_a);
			if (state.s[1])
				process_audio(transition, state.s[1], audio,
					      min_ts, mixers, channels,
					      sample_rate, mix_b);
		} else if (state.s[0]) {
			memcpy(audio->output[0].data[0],
			       state.s[0]->audio_output_buf[0][0],
			       TOTAL_AUDIO_SIZE);
		}

		obs_source_release(state.s[0]);
		obs_source_release(state.s[1]);
	}

	if (stopped)
		handle_stop(transition);

	*ts_out = min_ts;
	return !!min_ts;
}

void obs_transition_enable_fixed(obs_source_t *transition, bool enable,
				 uint32_t duration)
{
	if (!transition_valid(transition, "obs_transition_enable_fixed"))
		return;

	transition->transition_use_fixed_duration = enable;
	transition->transition_fixed_duration = duration;
}

bool obs_transition_fixed(obs_source_t *transition)
{
	return transition_valid(transition, "obs_transition_fixed")
		       ? transition->transition_use_fixed_duration
		       : false;
}

static inline obs_source_t *
copy_source_state(obs_source_t *tr_dest, obs_source_t *tr_source, size_t idx)
{
	obs_source_t *old_child = tr_dest->transition_sources[idx];
	obs_source_t *new_child = tr_source->transition_sources[idx];
	bool active = tr_source->transition_source_active[idx];

	if (old_child && tr_dest->transition_source_active[idx])
		obs_source_remove_active_child(tr_dest, old_child);

	tr_dest->transition_sources[idx] = new_child;
	tr_dest->transition_source_active[idx] = active;

	if (active && new_child)
		obs_source_add_active_child(tr_dest, new_child);
	obs_source_addref(new_child);

	return old_child;
}

void obs_transition_swap_begin(obs_source_t *tr_dest, obs_source_t *tr_source)
{
	obs_source_t *old_children[2];

	if (tr_dest == tr_source)
		return;

	lock_textures(tr_source);
	lock_textures(tr_dest);

	lock_transition(tr_source);
	lock_transition(tr_dest);

	for (size_t i = 0; i < 2; i++)
		old_children[i] = copy_source_state(tr_dest, tr_source, i);

	unlock_transition(tr_dest);
	unlock_transition(tr_source);

	for (size_t i = 0; i < 2; i++)
		obs_source_release(old_children[i]);
}

void obs_transition_swap_end(obs_source_t *tr_dest, obs_source_t *tr_source)
{
	if (tr_dest == tr_source)
		return;

	obs_transition_clear(tr_source);

	for (size_t i = 0; i < 2; i++) {
		gs_texrender_t *dest = tr_dest->transition_texrender[i];
		gs_texrender_t *source = tr_source->transition_texrender[i];

		tr_dest->transition_texrender[i] = source;
		tr_source->transition_texrender[i] = dest;
	}

	unlock_textures(tr_dest);
	unlock_textures(tr_source);
}
