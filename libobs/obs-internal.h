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

#include "util/c99defs.h"
#include "util/darray.h"
#include "util/circlebuf.h"
#include "util/dstr.h"
#include "util/threading.h"
#include "util/platform.h"
#include "util/profiler.h"
#include "callback/signal.h"
#include "callback/proc.h"

#include "graphics/graphics.h"
#include "graphics/matrix4.h"

#include "media-io/audio-resampler.h"
#include "media-io/video-io.h"
#include "media-io/audio-io.h"

#include "obs.h"

#include <caption/caption.h>

#define NUM_TEXTURES 2
#define NUM_CHANNELS 3
#define MICROSECOND_DEN 1000000
#define NUM_ENCODE_TEXTURES 3
#define NUM_ENCODE_TEXTURE_FRAMES_TO_WAIT 1

static inline int64_t packet_dts_usec(struct encoder_packet *packet)
{
	return packet->dts * MICROSECOND_DEN / packet->timebase_den;
}

struct tick_callback {
	void (*tick)(void *param, float seconds);
	void *param;
};

struct draw_callback {
	void (*draw)(void *param, uint32_t cx, uint32_t cy);
	void *param;
};

/* ------------------------------------------------------------------------- */
/* validity checks */

static inline bool obs_object_valid(const void *obj, const char *f,
				    const char *t)
{
	if (!obj) {
		blog(LOG_DEBUG, "%s: Null '%s' parameter", f, t);
		return false;
	}

	return true;
}

#define obs_ptr_valid(ptr, func) obs_object_valid(ptr, func, #ptr)
#define obs_source_valid obs_ptr_valid
#define obs_output_valid obs_ptr_valid
#define obs_encoder_valid obs_ptr_valid
#define obs_service_valid obs_ptr_valid

/* ------------------------------------------------------------------------- */
/* modules */

struct obs_module {
	char *mod_name;
	const char *file;
	char *bin_path;
	char *data_path;
	void *module;
	bool loaded;

	bool (*load)(void);
	void (*unload)(void);
	void (*post_load)(void);
	void (*set_locale)(const char *locale);
	bool (*get_string)(const char *lookup_string,
			   const char **translated_string);
	void (*free_locale)(void);
	uint32_t (*ver)(void);
	void (*set_pointer)(obs_module_t *module);
	const char *(*name)(void);
	const char *(*description)(void);
	const char *(*author)(void);

	struct obs_module *next;
};

extern void free_module(struct obs_module *mod);

struct obs_module_path {
	char *bin;
	char *data;
};

static inline void free_module_path(struct obs_module_path *omp)
{
	if (omp) {
		bfree(omp->bin);
		bfree(omp->data);
	}
}

static inline bool check_path(const char *data, const char *path,
			      struct dstr *output)
{
	dstr_copy(output, path);
	dstr_cat(output, data);

	return os_file_exists(output->array);
}

/* ------------------------------------------------------------------------- */
/* hotkeys */

struct obs_hotkey {
	obs_hotkey_id id;
	char *name;
	char *description;

	obs_hotkey_func func;
	void *data;
	int pressed;

	obs_hotkey_registerer_t registerer_type;
	void *registerer;

	obs_hotkey_id pair_partner_id;
};

struct obs_hotkey_pair {
	obs_hotkey_pair_id pair_id;
	obs_hotkey_id id[2];
	obs_hotkey_active_func func[2];
	bool pressed0;
	bool pressed1;
	void *data[2];
};

typedef struct obs_hotkey_pair obs_hotkey_pair_t;

typedef struct obs_hotkeys_platform obs_hotkeys_platform_t;

void *obs_hotkey_thread(void *param);

struct obs_core_hotkeys;
bool obs_hotkeys_platform_init(struct obs_core_hotkeys *hotkeys);
void obs_hotkeys_platform_free(struct obs_core_hotkeys *hotkeys);
bool obs_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *context,
				     obs_key_t key);

const char *obs_get_hotkey_translation(obs_key_t key, const char *def);

struct obs_context_data;
void obs_hotkeys_context_release(struct obs_context_data *context);

void obs_hotkeys_free(void);

struct obs_hotkey_binding {
	obs_key_combination_t key;
	bool pressed;
	bool modifiers_match;

	obs_hotkey_id hotkey_id;
	obs_hotkey_t *hotkey;
};

struct obs_hotkey_name_map;
void obs_hotkey_name_map_free(void);

/* ------------------------------------------------------------------------- */
/* views */

struct obs_view {
	pthread_mutex_t channels_mutex;
	obs_source_t *channels[MAX_CHANNELS];
};

extern bool obs_view_init(struct obs_view *view);
extern void obs_view_free(struct obs_view *view);

/* ------------------------------------------------------------------------- */
/* displays */

struct obs_display {
	bool size_changed;
	bool enabled;
	uint32_t cx, cy;
	uint32_t background_color;
	gs_swapchain_t *swap;
	pthread_mutex_t draw_callbacks_mutex;
	pthread_mutex_t draw_info_mutex;
	DARRAY(struct draw_callback) draw_callbacks;

	struct obs_display *next;
	struct obs_display **prev_next;
};

extern bool obs_display_init(struct obs_display *display,
			     const struct gs_init_data *graphics_data);
extern void obs_display_free(struct obs_display *display);

/* ------------------------------------------------------------------------- */
/* core */

struct obs_vframe_info {
	uint64_t timestamp;
	int count;
};

struct obs_tex_frame {
	gs_texture_t *tex;
	gs_texture_t *tex_uv;
	uint32_t handle;
	uint64_t timestamp;
	uint64_t lock_key;
	int count;
	bool released;
};

struct obs_task_info {
	obs_task_t task;
	void *param;
};

struct obs_core_video {
	graphics_t *graphics;
	gs_stagesurf_t *copy_surfaces[NUM_TEXTURES][NUM_CHANNELS];
	gs_texture_t *render_texture;
	gs_texture_t *output_texture;
	gs_texture_t *convert_textures[NUM_CHANNELS];
	bool texture_rendered;
	bool textures_copied[NUM_TEXTURES];
	bool texture_converted;
	bool using_nv12_tex;
	struct circlebuf vframe_info_buffer;
	struct circlebuf vframe_info_buffer_gpu;
	gs_effect_t *default_effect;
	gs_effect_t *default_rect_effect;
	gs_effect_t *opaque_effect;
	gs_effect_t *solid_effect;
	gs_effect_t *repeat_effect;
	gs_effect_t *conversion_effect;
	gs_effect_t *bicubic_effect;
	gs_effect_t *lanczos_effect;
	gs_effect_t *area_effect;
	gs_effect_t *bilinear_lowres_effect;
	gs_effect_t *premultiplied_alpha_effect;
	gs_samplerstate_t *point_sampler;
	gs_stagesurf_t *mapped_surfaces[NUM_CHANNELS];
	int cur_texture;
	long raw_active;
	long gpu_encoder_active;
	pthread_mutex_t gpu_encoder_mutex;
	struct circlebuf gpu_encoder_queue;
	struct circlebuf gpu_encoder_avail_queue;
	DARRAY(obs_encoder_t *) gpu_encoders;
	os_sem_t *gpu_encode_semaphore;
	os_event_t *gpu_encode_inactive;
	pthread_t gpu_encode_thread;
	bool gpu_encode_thread_initialized;
	volatile bool gpu_encode_stop;

	uint64_t video_time;
	uint64_t video_frame_interval_ns;
	uint64_t video_avg_frame_time_ns;
	double video_fps;
	video_t *video;
	pthread_t video_thread;
	uint32_t total_frames;
	uint32_t lagged_frames;
	bool thread_initialized;

	bool gpu_conversion;
	const char *conversion_techs[NUM_CHANNELS];
	bool conversion_needed;
	float conversion_width_i;

	uint32_t output_width;
	uint32_t output_height;
	uint32_t base_width;
	uint32_t base_height;
	float color_matrix[16];
	enum obs_scale_type scale_type;

	gs_texture_t *transparent_texture;

	gs_effect_t *deinterlace_discard_effect;
	gs_effect_t *deinterlace_discard_2x_effect;
	gs_effect_t *deinterlace_linear_effect;
	gs_effect_t *deinterlace_linear_2x_effect;
	gs_effect_t *deinterlace_blend_effect;
	gs_effect_t *deinterlace_blend_2x_effect;
	gs_effect_t *deinterlace_yadif_effect;
	gs_effect_t *deinterlace_yadif_2x_effect;

	struct obs_video_info ovi;

	pthread_mutex_t task_mutex;
	struct circlebuf tasks;
};

struct audio_monitor;

struct obs_core_audio {
	audio_t *audio;

	DARRAY(struct obs_source *) render_order;
	DARRAY(struct obs_source *) root_nodes;

	uint64_t buffered_ts;
	struct circlebuf buffered_timestamps;
	int buffering_wait_ticks;
	int total_buffering_ticks;

	float user_volume;

	pthread_mutex_t monitoring_mutex;
	DARRAY(struct audio_monitor *) monitors;
	char *monitoring_device_name;
	char *monitoring_device_id;
};

/* user sources, output channels, and displays */
struct obs_core_data {
	struct obs_source *first_source;
	struct obs_source *first_audio_source;
	struct obs_display *first_display;
	struct obs_output *first_output;
	struct obs_encoder *first_encoder;
	struct obs_service *first_service;

	pthread_mutex_t sources_mutex;
	pthread_mutex_t displays_mutex;
	pthread_mutex_t outputs_mutex;
	pthread_mutex_t encoders_mutex;
	pthread_mutex_t services_mutex;
	pthread_mutex_t audio_sources_mutex;
	pthread_mutex_t draw_callbacks_mutex;
	DARRAY(struct draw_callback) draw_callbacks;
	DARRAY(struct tick_callback) tick_callbacks;

	struct obs_view main_view;

	long long unnamed_index;

	obs_data_t *private_data;

	volatile bool valid;
};

/* user hotkeys */
struct obs_core_hotkeys {
	pthread_mutex_t mutex;
	DARRAY(obs_hotkey_t) hotkeys;
	obs_hotkey_id next_id;
	DARRAY(obs_hotkey_pair_t) hotkey_pairs;
	obs_hotkey_pair_id next_pair_id;

	pthread_t hotkey_thread;
	bool hotkey_thread_initialized;
	os_event_t *stop_event;
	bool thread_disable_press;
	bool strict_modifiers;
	bool reroute_hotkeys;
	DARRAY(obs_hotkey_binding_t) bindings;

	obs_hotkey_callback_router_func router_func;
	void *router_func_data;

	obs_hotkeys_platform_t *platform_context;

	pthread_once_t name_map_init_token;
	struct obs_hotkey_name_map *name_map;

	signal_handler_t *signals;

	char *translations[OBS_KEY_LAST_VALUE];
	char *mute;
	char *unmute;
	char *push_to_mute;
	char *push_to_talk;
	char *sceneitem_show;
	char *sceneitem_hide;
};

struct obs_core {
	struct obs_module *first_module;
	DARRAY(struct obs_module_path) module_paths;

	DARRAY(struct obs_source_info) source_types;
	DARRAY(struct obs_source_info) input_types;
	DARRAY(struct obs_source_info) filter_types;
	DARRAY(struct obs_source_info) transition_types;
	DARRAY(struct obs_output_info) output_types;
	DARRAY(struct obs_encoder_info) encoder_types;
	DARRAY(struct obs_service_info) service_types;
	DARRAY(struct obs_modal_ui) modal_ui_callbacks;
	DARRAY(struct obs_modeless_ui) modeless_ui_callbacks;

	signal_handler_t *signals;
	proc_handler_t *procs;

	char *locale;
	char *module_config_path;
	bool name_store_owned;
	profiler_name_store_t *name_store;

	/* segmented into multiple sub-structures to keep things a bit more
	 * clean and organized */
	struct obs_core_video video;
	struct obs_core_audio audio;
	struct obs_core_data data;
	struct obs_core_hotkeys hotkeys;

	obs_task_handler_t ui_task_handler;
};

extern struct obs_core *obs;

struct obs_graphics_context {
	uint64_t last_time;
	uint64_t interval;
	uint64_t frame_time_total_ns;
	uint64_t fps_total_ns;
	uint32_t fps_total_frames;
#ifdef _WIN32
	bool gpu_was_active;
#endif
	bool raw_was_active;
	bool was_active;
	const char *video_thread_name;
};

extern void *obs_graphics_thread(void *param);
extern bool obs_graphics_thread_loop(struct obs_graphics_context *context);
#ifdef __APPLE__
extern void *obs_graphics_thread_autorelease(void *param);
extern bool
obs_graphics_thread_loop_autorelease(struct obs_graphics_context *context);
#endif

extern gs_effect_t *obs_load_effect(gs_effect_t **effect, const char *file);

extern bool audio_callback(void *param, uint64_t start_ts_in,
			   uint64_t end_ts_in, uint64_t *out_ts,
			   uint32_t mixers, struct audio_output_data *mixes);

extern void
start_raw_video(video_t *video, const struct video_scale_info *conversion,
		void (*callback)(void *param, struct video_data *frame),
		void *param);
extern void stop_raw_video(video_t *video,
			   void (*callback)(void *param,
					    struct video_data *frame),
			   void *param);

/* ------------------------------------------------------------------------- */
/* obs shared context data */

struct obs_context_data {
	char *name;
	void *data;
	obs_data_t *settings;
	signal_handler_t *signals;
	proc_handler_t *procs;
	enum obs_obj_type type;

	DARRAY(obs_hotkey_id) hotkeys;
	DARRAY(obs_hotkey_pair_id) hotkey_pairs;
	obs_data_t *hotkey_data;

	DARRAY(char *) rename_cache;
	pthread_mutex_t rename_cache_mutex;

	pthread_mutex_t *mutex;
	struct obs_context_data *next;
	struct obs_context_data **prev_next;

	bool private;
};

extern bool obs_context_data_init(struct obs_context_data *context,
				  enum obs_obj_type type, obs_data_t *settings,
				  const char *name, obs_data_t *hotkey_data,
				  bool private);
extern void obs_context_data_free(struct obs_context_data *context);

extern void obs_context_data_insert(struct obs_context_data *context,
				    pthread_mutex_t *mutex, void *first);
extern void obs_context_data_remove(struct obs_context_data *context);

extern void obs_context_data_setname(struct obs_context_data *context,
				     const char *name);

/* ------------------------------------------------------------------------- */
/* ref-counting  */

struct obs_weak_ref {
	volatile long refs;
	volatile long weak_refs;
};

static inline void obs_ref_addref(struct obs_weak_ref *ref)
{
	os_atomic_inc_long(&ref->refs);
}

static inline bool obs_ref_release(struct obs_weak_ref *ref)
{
	return os_atomic_dec_long(&ref->refs) == -1;
}

static inline void obs_weak_ref_addref(struct obs_weak_ref *ref)
{
	os_atomic_inc_long(&ref->weak_refs);
}

static inline bool obs_weak_ref_release(struct obs_weak_ref *ref)
{
	return os_atomic_dec_long(&ref->weak_refs) == -1;
}

static inline bool obs_weak_ref_get_ref(struct obs_weak_ref *ref)
{
	long owners = os_atomic_load_long(&ref->refs);
	while (owners > -1) {
		if (os_atomic_compare_exchange_long(&ref->refs, &owners,
						    owners + 1)) {
			return true;
		}
	}

	return false;
}

/* ------------------------------------------------------------------------- */
/* sources  */

struct async_frame {
	struct obs_source_frame *frame;
	long unused_count;
	bool used;
};

enum audio_action_type {
	AUDIO_ACTION_VOL,
	AUDIO_ACTION_MUTE,
	AUDIO_ACTION_PTT,
	AUDIO_ACTION_PTM,
};

struct audio_action {
	uint64_t timestamp;
	enum audio_action_type type;
	union {
		float vol;
		bool set;
	};
};

struct obs_weak_source {
	struct obs_weak_ref ref;
	struct obs_source *source;
};

struct audio_cb_info {
	obs_source_audio_capture_t callback;
	void *param;
};

struct caption_cb_info {
	obs_source_caption_t callback;
	void *param;
};

struct obs_source {
	struct obs_context_data context;
	struct obs_source_info info;
	struct obs_weak_source *control;

	/* general exposed flags that can be set for the source */
	uint32_t flags;
	uint32_t default_flags;
	uint32_t last_obs_ver;

	/* indicates ownership of the info.id buffer */
	bool owns_info_id;

	/* signals to call the source update in the video thread */
	long defer_update_count;

	/* ensures show/hide are only called once */
	volatile long show_refs;

	/* ensures activate/deactivate are only called once */
	volatile long activate_refs;

	/* used to indicate that the source has been removed and all
	 * references to it should be released (not exactly how I would prefer
	 * to handle things but it's the best option) */
	bool removed;

	bool active;
	bool showing;

	/* used to temporarily disable sources if needed */
	bool enabled;

	/* timing (if video is present, is based upon video) */
	volatile bool timing_set;
	volatile uint64_t timing_adjust;
	uint64_t resample_offset;
	uint64_t last_audio_ts;
	uint64_t next_audio_ts_min;
	uint64_t next_audio_sys_ts_min;
	uint64_t last_frame_ts;
	uint64_t last_sys_timestamp;
	bool async_rendered;

	/* audio */
	bool audio_failed;
	bool audio_pending;
	bool pending_stop;
	bool audio_active;
	bool user_muted;
	bool muted;
	struct obs_source *next_audio_source;
	struct obs_source **prev_next_audio_source;
	uint64_t audio_ts;
	struct circlebuf audio_input_buf[MAX_AUDIO_CHANNELS];
	size_t last_audio_input_buf_size;
	DARRAY(struct audio_action) audio_actions;
	float *audio_output_buf[MAX_AUDIO_MIXES][MAX_AUDIO_CHANNELS];
	float *audio_mix_buf[MAX_AUDIO_CHANNELS];
	struct resample_info sample_info;
	audio_resampler_t *resampler;
	pthread_mutex_t audio_actions_mutex;
	pthread_mutex_t audio_buf_mutex;
	pthread_mutex_t audio_mutex;
	pthread_mutex_t audio_cb_mutex;
	DARRAY(struct audio_cb_info) audio_cb_list;
	struct obs_audio_data audio_data;
	size_t audio_storage_size;
	uint32_t audio_mixers;
	float user_volume;
	float volume;
	int64_t sync_offset;
	int64_t last_sync_offset;
	float balance;

	/* async video data */
	gs_texture_t *async_textures[MAX_AV_PLANES];
	gs_texrender_t *async_texrender;
	struct obs_source_frame *cur_async_frame;
	bool async_gpu_conversion;
	enum video_format async_format;
	bool async_full_range;
	enum video_format async_cache_format;
	bool async_cache_full_range;
	enum gs_color_format async_texture_formats[MAX_AV_PLANES];
	int async_channel_count;
	long async_rotation;
	bool async_flip;
	bool async_active;
	bool async_update_texture;
	bool async_unbuffered;
	bool async_decoupled;
	struct obs_source_frame *async_preload_frame;
	DARRAY(struct async_frame) async_cache;
	DARRAY(struct obs_source_frame *) async_frames;
	pthread_mutex_t async_mutex;
	uint32_t async_width;
	uint32_t async_height;
	uint32_t async_cache_width;
	uint32_t async_cache_height;
	uint32_t async_convert_width[MAX_AV_PLANES];
	uint32_t async_convert_height[MAX_AV_PLANES];

	pthread_mutex_t caption_cb_mutex;
	DARRAY(struct caption_cb_info) caption_cb_list;

	/* async video deinterlacing */
	uint64_t deinterlace_offset;
	uint64_t deinterlace_frame_ts;
	gs_effect_t *deinterlace_effect;
	struct obs_source_frame *prev_async_frame;
	gs_texture_t *async_prev_textures[MAX_AV_PLANES];
	gs_texrender_t *async_prev_texrender;
	uint32_t deinterlace_half_duration;
	enum obs_deinterlace_mode deinterlace_mode;
	bool deinterlace_top_first;
	bool deinterlace_rendered;

	/* filters */
	struct obs_source *filter_parent;
	struct obs_source *filter_target;
	DARRAY(struct obs_source *) filters;
	pthread_mutex_t filter_mutex;
	gs_texrender_t *filter_texrender;
	enum obs_allow_direct_render allow_direct;
	bool rendering_filter;

	/* sources specific hotkeys */
	obs_hotkey_pair_id mute_unmute_key;
	obs_hotkey_id push_to_mute_key;
	obs_hotkey_id push_to_talk_key;
	bool push_to_mute_enabled;
	bool push_to_mute_pressed;
	bool user_push_to_mute_pressed;
	bool push_to_talk_enabled;
	bool push_to_talk_pressed;
	bool user_push_to_talk_pressed;
	uint64_t push_to_mute_delay;
	uint64_t push_to_mute_stop_time;
	uint64_t push_to_talk_delay;
	uint64_t push_to_talk_stop_time;

	/* transitions */
	uint64_t transition_start_time;
	uint64_t transition_duration;
	pthread_mutex_t transition_tex_mutex;
	gs_texrender_t *transition_texrender[2];
	pthread_mutex_t transition_mutex;
	obs_source_t *transition_sources[2];
	float transition_manual_clamp;
	float transition_manual_torque;
	float transition_manual_target;
	float transition_manual_val;
	bool transitioning_video;
	bool transitioning_audio;
	bool transition_source_active[2];
	uint32_t transition_alignment;
	uint32_t transition_actual_cx;
	uint32_t transition_actual_cy;
	uint32_t transition_cx;
	uint32_t transition_cy;
	uint32_t transition_fixed_duration;
	bool transition_use_fixed_duration;
	enum obs_transition_mode transition_mode;
	enum obs_transition_scale_type transition_scale_type;
	struct matrix4 transition_matrices[2];

	struct audio_monitor *monitor;
	enum obs_monitoring_type monitoring_type;

	obs_data_t *private_settings;
};

extern struct obs_source_info *get_source_info(const char *id);
extern struct obs_source_info *get_source_info2(const char *unversioned_id,
						uint32_t ver);
extern bool obs_source_init_context(struct obs_source *source,
				    obs_data_t *settings, const char *name,
				    obs_data_t *hotkey_data, bool private);

extern bool obs_transition_init(obs_source_t *transition);
extern void obs_transition_free(obs_source_t *transition);
extern void obs_transition_tick(obs_source_t *transition, float t);
extern void obs_transition_enum_sources(obs_source_t *transition,
					obs_source_enum_proc_t enum_callback,
					void *param);
extern void obs_transition_save(obs_source_t *source, obs_data_t *data);
extern void obs_transition_load(obs_source_t *source, obs_data_t *data);

struct audio_monitor *audio_monitor_create(obs_source_t *source);
void audio_monitor_reset(struct audio_monitor *monitor);
extern void audio_monitor_destroy(struct audio_monitor *monitor);

extern obs_source_t *obs_source_create_set_last_ver(const char *id,
						    const char *name,
						    obs_data_t *settings,
						    obs_data_t *hotkey_data,
						    uint32_t last_obs_ver);
extern void obs_source_destroy(struct obs_source *source);

enum view_type {
	MAIN_VIEW,
	AUX_VIEW,
};

static inline void obs_source_dosignal(struct obs_source *source,
				       const char *signal_obs,
				       const char *signal_source)
{
	struct calldata data;
	uint8_t stack[128];

	calldata_init_fixed(&data, stack, sizeof(stack));
	calldata_set_ptr(&data, "source", source);
	if (signal_obs && !source->context.private)
		signal_handler_signal(obs->signals, signal_obs, &data);
	if (signal_source)
		signal_handler_signal(source->context.signals, signal_source,
				      &data);
}

/* maximum timestamp variance in nanoseconds */
#define MAX_TS_VAR 2000000000ULL

static inline bool frame_out_of_bounds(const obs_source_t *source, uint64_t ts)
{
	if (ts < source->last_frame_ts)
		return ((source->last_frame_ts - ts) > MAX_TS_VAR);
	else
		return ((ts - source->last_frame_ts) > MAX_TS_VAR);
}

static inline enum gs_color_format
convert_video_format(enum video_format format)
{
	switch (format) {
	case VIDEO_FORMAT_RGBA:
		return GS_RGBA;
	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_I40A:
	case VIDEO_FORMAT_I42A:
	case VIDEO_FORMAT_YUVA:
	case VIDEO_FORMAT_AYUV:
		return GS_BGRA;
	default:
		return GS_BGRX;
	}
}

extern void obs_source_activate(obs_source_t *source, enum view_type type);
extern void obs_source_deactivate(obs_source_t *source, enum view_type type);
extern void obs_source_video_tick(obs_source_t *source, float seconds);
extern float obs_source_get_target_volume(obs_source_t *source,
					  obs_source_t *target);

extern void obs_source_audio_render(obs_source_t *source, uint32_t mixers,
				    size_t channels, size_t sample_rate,
				    size_t size);

extern void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy);

extern struct obs_source_frame *filter_async_video(obs_source_t *source,
						   struct obs_source_frame *in);
extern bool update_async_texture(struct obs_source *source,
				 const struct obs_source_frame *frame,
				 gs_texture_t *tex, gs_texrender_t *texrender);
extern bool update_async_textures(struct obs_source *source,
				  const struct obs_source_frame *frame,
				  gs_texture_t *tex[MAX_AV_PLANES],
				  gs_texrender_t *texrender);
extern bool set_async_texture_size(struct obs_source *source,
				   const struct obs_source_frame *frame);
extern void remove_async_frame(obs_source_t *source,
			       struct obs_source_frame *frame);

extern void set_deinterlace_texture_size(obs_source_t *source);
extern void deinterlace_process_last_frame(obs_source_t *source,
					   uint64_t sys_time);
extern void deinterlace_update_async_video(obs_source_t *source);
extern void deinterlace_render(obs_source_t *s);

/* ------------------------------------------------------------------------- */
/* outputs  */

enum delay_msg {
	DELAY_MSG_PACKET,
	DELAY_MSG_START,
	DELAY_MSG_STOP,
};

struct delay_data {
	enum delay_msg msg;
	uint64_t ts;
	struct encoder_packet packet;
};

typedef void (*encoded_callback_t)(void *data, struct encoder_packet *packet);

struct obs_weak_output {
	struct obs_weak_ref ref;
	struct obs_output *output;
};

#define CAPTION_LINE_CHARS (32)
#define CAPTION_LINE_BYTES (4 * CAPTION_LINE_CHARS)
struct caption_text {
	char text[CAPTION_LINE_BYTES + 1];
	double display_duration;
	struct caption_text *next;
};

struct pause_data {
	pthread_mutex_t mutex;
	uint64_t last_video_ts;
	uint64_t ts_start;
	uint64_t ts_end;
	uint64_t ts_offset;
};

extern bool video_pause_check(struct pause_data *pause, uint64_t timestamp);
extern bool audio_pause_check(struct pause_data *pause, struct audio_data *data,
			      size_t sample_rate);
extern void pause_reset(struct pause_data *pause);

struct obs_output {
	struct obs_context_data context;
	struct obs_output_info info;
	struct obs_weak_output *control;

	/* indicates ownership of the info.id buffer */
	bool owns_info_id;

	bool received_video;
	bool received_audio;
	volatile bool data_active;
	volatile bool end_data_capture_thread_active;
	int64_t video_offset;
	int64_t audio_offsets[MAX_AUDIO_MIXES];
	int64_t highest_audio_ts;
	int64_t highest_video_ts;
	pthread_t end_data_capture_thread;
	os_event_t *stopping_event;
	pthread_mutex_t interleaved_mutex;
	DARRAY(struct encoder_packet) interleaved_packets;
	int stop_code;

	int reconnect_retry_sec;
	int reconnect_retry_max;
	int reconnect_retries;
	int reconnect_retry_cur_sec;
	pthread_t reconnect_thread;
	os_event_t *reconnect_stop_event;
	volatile bool reconnecting;
	volatile bool reconnect_thread_active;

	uint32_t starting_drawn_count;
	uint32_t starting_lagged_count;
	uint32_t starting_frame_count;

	int total_frames;

	volatile bool active;
	volatile bool paused;
	video_t *video;
	audio_t *audio;
	obs_encoder_t *video_encoder;
	obs_encoder_t *audio_encoders[MAX_AUDIO_MIXES];
	obs_service_t *service;
	size_t mixer_mask;

	struct pause_data pause;

	struct circlebuf audio_buffer[MAX_AUDIO_MIXES][MAX_AV_PLANES];
	uint64_t audio_start_ts;
	uint64_t video_start_ts;
	size_t audio_size;
	size_t planes;
	size_t sample_rate;
	size_t total_audio_frames;

	uint32_t scaled_width;
	uint32_t scaled_height;

	bool video_conversion_set;
	bool audio_conversion_set;
	struct video_scale_info video_conversion;
	struct audio_convert_info audio_conversion;

	pthread_mutex_t caption_mutex;
	double caption_timestamp;
	struct caption_text *caption_head;
	struct caption_text *caption_tail;

	struct circlebuf caption_data;

	bool valid;

	uint64_t active_delay_ns;
	encoded_callback_t delay_callback;
	struct circlebuf delay_data; /* struct delay_data */
	pthread_mutex_t delay_mutex;
	uint32_t delay_sec;
	uint32_t delay_flags;
	uint32_t delay_cur_flags;
	volatile long delay_restart_refs;
	volatile bool delay_active;
	volatile bool delay_capturing;

	char *last_error_message;

	float audio_data[MAX_AUDIO_CHANNELS][AUDIO_OUTPUT_FRAMES];
};

static inline void do_output_signal(struct obs_output *output,
				    const char *signal)
{
	struct calldata params = {0};
	calldata_set_ptr(&params, "output", output);
	signal_handler_signal(output->context.signals, signal, &params);
	calldata_free(&params);
}

extern void process_delay(void *data, struct encoder_packet *packet);
extern void obs_output_cleanup_delay(obs_output_t *output);
extern bool obs_output_delay_start(obs_output_t *output);
extern void obs_output_delay_stop(obs_output_t *output);
extern bool obs_output_actual_start(obs_output_t *output);
extern void obs_output_actual_stop(obs_output_t *output, bool force,
				   uint64_t ts);

extern const struct obs_output_info *find_output(const char *id);

extern void obs_output_remove_encoder(struct obs_output *output,
				      struct obs_encoder *encoder);

extern void
obs_encoder_packet_create_instance(struct encoder_packet *dst,
				   const struct encoder_packet *src);
void obs_output_destroy(obs_output_t *output);

/* ------------------------------------------------------------------------- */
/* encoders  */

struct obs_weak_encoder {
	struct obs_weak_ref ref;
	struct obs_encoder *encoder;
};

struct encoder_callback {
	bool sent_first_packet;
	void (*new_packet)(void *param, struct encoder_packet *packet);
	void *param;
};

struct obs_encoder {
	struct obs_context_data context;
	struct obs_encoder_info info;
	struct obs_weak_encoder *control;

	/* allows re-routing to another encoder */
	struct obs_encoder_info orig_info;

	pthread_mutex_t init_mutex;

	uint32_t samplerate;
	size_t planes;
	size_t blocksize;
	size_t framesize;
	size_t framesize_bytes;

	size_t mixer_idx;

	uint32_t scaled_width;
	uint32_t scaled_height;
	enum video_format preferred_format;

	volatile bool active;
	volatile bool paused;
	bool initialized;

	/* indicates ownership of the info.id buffer */
	bool owns_info_id;

	uint32_t timebase_num;
	uint32_t timebase_den;

	int64_t cur_pts;

	struct circlebuf audio_input_buffer[MAX_AV_PLANES];
	uint8_t *audio_output_buffer[MAX_AV_PLANES];

	/* if a video encoder is paired with an audio encoder, make it start
	 * up at the specific timestamp.  if this is the audio encoder,
	 * wait_for_video makes it wait until it's ready to sync up with
	 * video */
	bool wait_for_video;
	bool first_received;
	struct obs_encoder *paired_encoder;
	int64_t offset_usec;
	uint64_t first_raw_ts;
	uint64_t start_ts;

	pthread_mutex_t outputs_mutex;
	DARRAY(obs_output_t *) outputs;

	bool destroy_on_stop;

	/* stores the video/audio media output pointer.  video_t *or audio_t **/
	void *media;

	pthread_mutex_t callbacks_mutex;
	DARRAY(struct encoder_callback) callbacks;

	struct pause_data pause;

	const char *profile_encoder_encode_name;
	char *last_error_message;
};

extern struct obs_encoder_info *find_encoder(const char *id);

extern bool obs_encoder_initialize(obs_encoder_t *encoder);
extern void obs_encoder_shutdown(obs_encoder_t *encoder);

extern void obs_encoder_start(obs_encoder_t *encoder,
			      void (*new_packet)(void *param,
						 struct encoder_packet *packet),
			      void *param);
extern void obs_encoder_stop(obs_encoder_t *encoder,
			     void (*new_packet)(void *param,
						struct encoder_packet *packet),
			     void *param);

extern void obs_encoder_add_output(struct obs_encoder *encoder,
				   struct obs_output *output);
extern void obs_encoder_remove_output(struct obs_encoder *encoder,
				      struct obs_output *output);

extern bool start_gpu_encode(obs_encoder_t *encoder);
extern void stop_gpu_encode(obs_encoder_t *encoder);

extern bool do_encode(struct obs_encoder *encoder, struct encoder_frame *frame);
extern void send_off_encoder_packet(obs_encoder_t *encoder, bool success,
				    bool received, struct encoder_packet *pkt);

void obs_encoder_destroy(obs_encoder_t *encoder);

/* ------------------------------------------------------------------------- */
/* services */

struct obs_weak_service {
	struct obs_weak_ref ref;
	struct obs_service *service;
};

struct obs_service {
	struct obs_context_data context;
	struct obs_service_info info;
	struct obs_weak_service *control;

	/* indicates ownership of the info.id buffer */
	bool owns_info_id;

	bool active;
	bool destroy;
	struct obs_output *output;
};

extern const struct obs_service_info *find_service(const char *id);

extern void obs_service_activate(struct obs_service *service);
extern void obs_service_deactivate(struct obs_service *service, bool remove);
extern bool obs_service_initialize(struct obs_service *service,
				   struct obs_output *output);

void obs_service_destroy(obs_service_t *service);
