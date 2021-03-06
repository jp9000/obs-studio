/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <jim@obsproject.com>

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
#include "util/bmem.h"
#include "util/profiler.h"
#include "util/text-lookup.h"
#include "graphics/graphics.h"
#include "graphics/vec2.h"
#include "graphics/vec3.h"
#include "media-io/audio-io.h"
#include "media-io/video-io.h"
#include "callback/signal.h"
#include "callback/proc.h"

#include "obs-config.h"
#include "obs-defs.h"
#include "obs-data.h"
#include "obs-ui.h"
#include "obs-properties.h"
#include "obs-interaction.h"

struct matrix4;

/* opaque types */
struct obs_display;
struct obs_view;
struct obs_source;
struct obs_scene;
struct obs_scene_item;
struct obs_output;
struct obs_encoder;
struct obs_service;
struct obs_module;
struct obs_fader;
struct obs_volmeter;

typedef struct obs_display obs_display_t;
typedef struct obs_view obs_view_t;
typedef struct obs_source obs_source_t;
typedef struct obs_scene obs_scene_t;
typedef struct obs_scene_item obs_sceneitem_t;
typedef struct obs_output obs_output_t;
typedef struct obs_encoder obs_encoder_t;
typedef struct obs_service obs_service_t;
typedef struct obs_module obs_module_t;
typedef struct obs_fader obs_fader_t;
typedef struct obs_volmeter obs_volmeter_t;

typedef struct obs_weak_source obs_weak_source_t;
typedef struct obs_weak_output obs_weak_output_t;
typedef struct obs_weak_encoder obs_weak_encoder_t;
typedef struct obs_weak_service obs_weak_service_t;

#include "obs-missing-files.h"
#include "obs-source.h"
#include "obs-encoder.h"
#include "obs-output.h"
#include "obs-service.h"
#include "obs-audio-controls.h"
#include "obs-hotkey.h"

/**
 * @file
 * @brief Main libobs header used by applications.
 *
 * @mainpage
 *
 * @section intro_sec Introduction
 *
 * This document describes the api for libobs to be used by applications as well
 * as @ref modules_page implementing some kind of functionality.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Used for changing the order of items (for example, filters in a source,
 * or items in a scene) */
enum obs_order_movement {
	OBS_ORDER_MOVE_UP,
	OBS_ORDER_MOVE_DOWN,
	OBS_ORDER_MOVE_TOP,
	OBS_ORDER_MOVE_BOTTOM,
};

/**
 * Used with obs_source_process_filter to specify whether the filter should
 * render the source directly with the specified effect, or whether it should
 * render it to a texture
 */
enum obs_allow_direct_render {
	OBS_NO_DIRECT_RENDERING,
	OBS_ALLOW_DIRECT_RENDERING,
};

enum obs_scale_type {
	OBS_SCALE_DISABLE,
	OBS_SCALE_POINT,
	OBS_SCALE_BICUBIC,
	OBS_SCALE_BILINEAR,
	OBS_SCALE_LANCZOS,
	OBS_SCALE_AREA,
};

/**
 * Used with scene items to indicate the type of bounds to use for scene items.
 * Mostly determines how the image will be scaled within those bounds, or
 * whether to use bounds at all.
 */
enum obs_bounds_type {
	OBS_BOUNDS_NONE,            /**< no bounds */
	OBS_BOUNDS_STRETCH,         /**< stretch (ignores base scale) */
	OBS_BOUNDS_SCALE_INNER,     /**< scales to inner rectangle */
	OBS_BOUNDS_SCALE_OUTER,     /**< scales to outer rectangle */
	OBS_BOUNDS_SCALE_TO_WIDTH,  /**< scales to the width  */
	OBS_BOUNDS_SCALE_TO_HEIGHT, /**< scales to the height */
	OBS_BOUNDS_MAX_ONLY,        /**< no scaling, maximum size only */
};

struct obs_transform_info {
	struct vec2 pos;
	float rot;
	struct vec2 scale;
	uint32_t alignment;

	enum obs_bounds_type bounds_type;
	uint32_t bounds_alignment;
	struct vec2 bounds;
};

/**
 * Video initialization structure
 */
struct obs_video_info {
#ifndef SWIG
	/**
	 * Graphics module to use (usually "libobs-opengl" or "libobs-d3d11")
	 */
	const char *graphics_module;
#endif

	uint32_t fps_num; /**< Output FPS numerator */
	uint32_t fps_den; /**< Output FPS denominator */

	uint32_t base_width;  /**< Base compositing width */
	uint32_t base_height; /**< Base compositing height */

	uint32_t output_width;           /**< Output width */
	uint32_t output_height;          /**< Output height */
	enum video_format output_format; /**< Output format */

	/** Video adapter index to use (NOTE: avoid for optimus laptops) */
	uint32_t adapter;

	/** Use shaders to convert to different color formats */
	bool gpu_conversion;

	enum video_colorspace colorspace; /**< YUV type (if YUV) */
	enum video_range_type range;      /**< YUV range (if YUV) */

	enum obs_scale_type scale_type; /**< How to scale if scaling */
};

/**
 * Audio initialization structure
 */
struct obs_audio_info {
	uint32_t samples_per_sec;
	enum speaker_layout speakers;
};

/**
 * Sent to source filters via the filter_audio callback to allow filtering of
 * audio data
 */
struct obs_audio_data {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t frames;
	uint64_t timestamp;
};

/**
 * Source audio output structure.  Used with obs_source_output_audio to output
 * source audio.  Audio is automatically resampled and remixed as necessary.
 */
struct obs_source_audio {
	const uint8_t *data[MAX_AV_PLANES];
	uint32_t frames;

	enum speaker_layout speakers;
	enum audio_format format;
	uint32_t samples_per_sec;

	uint64_t timestamp;
};

struct obs_source_cea_708 {
	const uint8_t *data;
	uint32_t packets;
	uint64_t timestamp;
};

/**
 * Source asynchronous video output structure.  Used with
 * obs_source_output_video to output asynchronous video.  Video is buffered as
 * necessary to play according to timestamps.  When used with audio output,
 * audio is synced to video as it is played.
 *
 * If a YUV format is specified, it will be automatically upsampled and
 * converted to RGB via shader on the graphics processor.
 *
 * NOTE: Non-YUV formats will always be treated as full range with this
 * structure!  Use obs_source_frame2 along with obs_source_output_video2
 * instead if partial range support is desired for non-YUV video formats.
 */
struct obs_source_frame {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
	uint32_t width;
	uint32_t height;
	uint64_t timestamp;

	enum video_format format;
	float color_matrix[16];
	bool full_range;
	float color_range_min[3];
	float color_range_max[3];
	bool flip;

	/* used internally by libobs */
	volatile long refs;
	bool prev_frame;
};

struct obs_source_frame2 {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
	uint32_t width;
	uint32_t height;
	uint64_t timestamp;

	enum video_format format;
	enum video_range_type range;
	float color_matrix[16];
	float color_range_min[3];
	float color_range_max[3];
	bool flip;
};

/** Access to the argc/argv used to start OBS. What you see is what you get. */
struct obs_cmdline_args {
	int argc;
	char **argv;
};

/* ------------------------------------------------------------------------- */
/* OBS context */

/**
 * Find a core libobs data file
 * @param path name of the base file
 * @return A string containing the full path to the file.
 *          Use bfree after use.
 */
EXPORT char *obs_find_data_file(const char *file);

/**
 * Add a path to search libobs data files in.
 * @param path Full path to directory to look in.
 *             The string is copied.
 */
EXPORT void obs_add_data_path(const char *path);

/**
 * Remove a path from libobs core data paths.
 * @param path The path to compare to currently set paths.
 *             It does not need to be the same pointer, but
 *             the path string must match an entry fully.
 * @return Whether or not the path was successfully removed.
 *         If false, the path could not be found.
 */
EXPORT bool obs_remove_data_path(const char *path);

/**
 * Initializes OBS
 *
 * @param  locale              The locale to use for modules
 * @param  module_config_path  Path to module config storage directory
 *                             (or NULL if none)
 * @param  store               The profiler name store for OBS to use or NULL
 */
EXPORT bool obs_startup(const char *locale, const char *module_config_path,
			profiler_name_store_t *store);

/** Releases all data associated with OBS and terminates the OBS context */
EXPORT void obs_shutdown(void);

/** @return true if the main OBS context has been initialized */
EXPORT bool obs_initialized(void);

/** @return The current core version */
EXPORT uint32_t obs_get_version(void);

/** @return The current core version string */
EXPORT const char *obs_get_version_string(void);

/**
 * Sets things up for calls to obs_get_cmdline_args. Called only once at startup
 * and safely copies argv/argc from main(). Subsequent calls do nothing.
 *
 * @param  argc  The count of command line arguments, from main()
 * @param  argv  An array of command line arguments, copied from main() and ends
 *               with NULL.
 */
EXPORT void obs_set_cmdline_args(int argc, const char *const *argv);

/**
 * Get the argc/argv used to start OBS
 *
 * @return  The command line arguments used for main(). Don't modify this or
 *          you'll mess things up for other callers.
 */
EXPORT struct obs_cmdline_args obs_get_cmdline_args(void);

/**
 * Sets a new locale to use for modules.  This will call obs_module_set_locale
 * for each module with the new locale.
 *
 * @param  locale  The locale to use for modules
 */
EXPORT void obs_set_locale(const char *locale);

/** @return the current locale */
EXPORT const char *obs_get_locale(void);

/** Initialize the Windows-specific crash handler */

#ifdef _WIN32
EXPORT void obs_init_win32_crash_handler(void);
#endif

/**
 * Returns the profiler name store (see util/profiler.h) used by OBS, which is
 * either a name store passed to obs_startup, an internal name store, or NULL
 * in case obs_initialized() returns false.
 */
EXPORT profiler_name_store_t *obs_get_profiler_name_store(void);

/**
 * Sets base video output base resolution/fps/format.
 *
 * @note This data cannot be changed if an output is currently active.
 * @note The graphics module cannot be changed without fully destroying the
 *       OBS context.
 *
 * @param   ovi  Pointer to an obs_video_info structure containing the
 *               specification of the graphics subsystem,
 * @return       OBS_VIDEO_SUCCESS if successful
 *               OBS_VIDEO_NOT_SUPPORTED if the adapter lacks capabilities
 *               OBS_VIDEO_INVALID_PARAM if a parameter is invalid
 *               OBS_VIDEO_CURRENTLY_ACTIVE if video is currently active
 *               OBS_VIDEO_MODULE_NOT_FOUND if the graphics module is not found
 *               OBS_VIDEO_FAIL for generic failure
 */
EXPORT int obs_reset_video(struct obs_video_info *ovi);

/**
 * Sets base audio output format/channels/samples/etc
 *
 * @note Cannot reset base audio if an output is currently active.
 */
EXPORT bool obs_reset_audio(const struct obs_audio_info *oai);

/** Gets the current video settings, returns false if no video */
EXPORT bool obs_get_video_info(struct obs_video_info *ovi);

/** Gets the current audio settings, returns false if no audio */
EXPORT bool obs_get_audio_info(struct obs_audio_info *oai);

/**
 * Opens a plugin module directly from a specific path.
 *
 * If the module already exists then the function will return successful, and
 * the module parameter will be given the pointer to the existing module.
 *
 * This does not initialize the module, it only loads the module image.  To
 * initialize the module, call obs_init_module.
 *
 * @param  module     The pointer to the created module.
 * @param  path       Specifies the path to the module library file.  If the
 *                    extension is not specified, it will use the extension
 *                    appropriate to the operating system.
 * @param  data_path  Specifies the path to the directory where the module's
 *                    data files are stored.
 * @returns           MODULE_SUCCESS if successful
 *                    MODULE_ERROR if a generic error occurred
 *                    MODULE_FILE_NOT_FOUND if the module was not found
 *                    MODULE_MISSING_EXPORTS if required exports are missing
 *                    MODULE_INCOMPATIBLE_VER if incompatible version
 */
EXPORT int obs_open_module(obs_module_t **module, const char *path,
			   const char *data_path);

/**
 * Initializes the module, which calls its obs_module_load export.  If the
 * module is already loaded, then this function does nothing and returns
 * successful.
 */
EXPORT bool obs_init_module(obs_module_t *module);

/** Returns a module based upon its name, or NULL if not found */
EXPORT obs_module_t *obs_get_module(const char *name);

/** Gets library of module */
EXPORT void *obs_get_module_lib(obs_module_t *module);

/** Returns locale text from a specific module */
EXPORT bool obs_module_get_locale_string(const obs_module_t *mod,
					 const char *lookup_string,
					 const char **translated_string);

EXPORT const char *obs_module_get_locale_text(const obs_module_t *mod,
					      const char *text);

/** Logs loaded modules */
EXPORT void obs_log_loaded_modules(void);

/** Returns the module file name */
EXPORT const char *obs_get_module_file_name(obs_module_t *module);

/** Returns the module full name */
EXPORT const char *obs_get_module_name(obs_module_t *module);

/** Returns the module author(s) */
EXPORT const char *obs_get_module_author(obs_module_t *module);

/** Returns the module description */
EXPORT const char *obs_get_module_description(obs_module_t *module);

/** Returns the module binary path */
EXPORT const char *obs_get_module_binary_path(obs_module_t *module);

/** Returns the module data path */
EXPORT const char *obs_get_module_data_path(obs_module_t *module);

/**
 * Adds a module search path to be used with obs_find_modules.  If the search
 * path strings contain %module%, that text will be replaced with the module
 * name when used.
 *
 * @param  bin   Specifies the module's binary directory search path.
 * @param  data  Specifies the module's data directory search path.
 */
EXPORT void obs_add_module_path(const char *bin, const char *data);

/** Automatically loads all modules from module paths (convenience function) */
EXPORT void obs_load_all_modules(void);

/** Notifies modules that all modules have been loaded.  This function should
 * be called after all modules have been loaded. */
EXPORT void obs_post_load_modules(void);

#ifndef SWIG
struct obs_module_info {
	const char *bin_path;
	const char *data_path;
};

typedef void (*obs_find_module_callback_t)(void *param,
					   const struct obs_module_info *info);

/** Finds all modules within the search paths added by obs_add_module_path. */
EXPORT void obs_find_modules(obs_find_module_callback_t callback, void *param);
#endif

typedef void (*obs_enum_module_callback_t)(void *param, obs_module_t *module);

/** Enumerates all loaded modules */
EXPORT void obs_enum_modules(obs_enum_module_callback_t callback, void *param);

/** Helper function for using default module locale */
EXPORT lookup_t *obs_module_load_locale(obs_module_t *module,
					const char *default_locale,
					const char *locale);

/**
 * Returns the location of a plugin module data file.
 *
 * @note   Modules should use obs_module_file function defined in obs-module.h
 *         as a more elegant means of getting their files without having to
 *         specify the module parameter.
 *
 * @param  module  The module associated with the file to locate
 * @param  file    The file to locate
 * @return         Path string, or NULL if not found.  Use bfree to free string.
 */
EXPORT char *obs_find_module_file(obs_module_t *module, const char *file);

/**
 * Returns the path of a plugin module config file (whether it exists or not)
 *
 * @note   Modules should use obs_module_config_path function defined in
 *         obs-module.h as a more elegant means of getting their files without
 *         having to specify the module parameter.
 *
 * @param  module  The module associated with the path
 * @param  file    The file to get a path to
 * @return         Path string, or NULL if not found.  Use bfree to free string.
 */
EXPORT char *obs_module_get_config_path(obs_module_t *module, const char *file);

/** Enumerates all source types (inputs, filters, transitions, etc).  */
EXPORT bool obs_enum_source_types(size_t idx, const char **id);

/**
 * Enumerates all available inputs source types.
 *
 *   Inputs are general source inputs (such as capture sources, device sources,
 * etc).
 */
EXPORT bool obs_enum_input_types(size_t idx, const char **id);
EXPORT bool obs_enum_input_types2(size_t idx, const char **id,
				  const char **unversioned_id);

EXPORT const char *obs_get_latest_input_type_id(const char *unversioned_id);

/**
 * Enumerates all available filter source types.
 *
 *   Filters are sources that are used to modify the video/audio output of
 * other sources.
 */
EXPORT bool obs_enum_filter_types(size_t idx, const char **id);

/**
 * Enumerates all available transition source types.
 *
 *   Transitions are sources used to transition between two or more other
 * sources.
 */
EXPORT bool obs_enum_transition_types(size_t idx, const char **id);

/** Enumerates all available output types. */
EXPORT bool obs_enum_output_types(size_t idx, const char **id);

/** Enumerates all available encoder types. */
EXPORT bool obs_enum_encoder_types(size_t idx, const char **id);

/** Enumerates all available service types. */
EXPORT bool obs_enum_service_types(size_t idx, const char **id);

/** Helper function for entering the OBS graphics context */
EXPORT void obs_enter_graphics(void);

/** Helper function for leaving the OBS graphics context */
EXPORT void obs_leave_graphics(void);

/** Gets the main audio output handler for this OBS context */
EXPORT audio_t *obs_get_audio(void);

/** Gets the main video output handler for this OBS context */
EXPORT video_t *obs_get_video(void);

/** Returns true if video is active, false otherwise */
EXPORT bool obs_video_active(void);

/** Sets the primary output source for a channel. */
EXPORT void obs_set_output_source(uint32_t channel, obs_source_t *source);

/**
 * Gets the primary output source for a channel and increments the reference
 * counter for that source.  Use obs_source_release to release.
 */
EXPORT obs_source_t *obs_get_output_source(uint32_t channel);

/**
 * Enumerates all input sources
 *
 *   Callback function returns true to continue enumeration, or false to end
 * enumeration.
 *
 *   Use obs_source_get_ref or obs_source_get_weak_source if you want to retain
 * a reference after obs_enum_sources finishes
 */
EXPORT void obs_enum_sources(bool (*enum_proc)(void *, obs_source_t *),
			     void *param);

/** Enumerates scenes */
EXPORT void obs_enum_scenes(bool (*enum_proc)(void *, obs_source_t *),
			    void *param);

/** Enumerates outputs */
EXPORT void obs_enum_outputs(bool (*enum_proc)(void *, obs_output_t *),
			     void *param);

/** Enumerates encoders */
EXPORT void obs_enum_encoders(bool (*enum_proc)(void *, obs_encoder_t *),
			      void *param);

/** Enumerates encoders */
EXPORT void obs_enum_services(bool (*enum_proc)(void *, obs_service_t *),
			      void *param);

/**
 * Gets a source by its name.
 *
 *   Increments the source reference counter, use obs_source_release to
 * release it when complete.
 */
EXPORT obs_source_t *obs_get_source_by_name(const char *name);

/** Gets an output by its name. */
EXPORT obs_output_t *obs_get_output_by_name(const char *name);

/** Gets an encoder by its name. */
EXPORT obs_encoder_t *obs_get_encoder_by_name(const char *name);

/** Gets an service by its name. */
EXPORT obs_service_t *obs_get_service_by_name(const char *name);

enum obs_base_effect {
	OBS_EFFECT_DEFAULT,         /**< RGB/YUV */
	OBS_EFFECT_DEFAULT_RECT,    /**< RGB/YUV (using texture_rect) */
	OBS_EFFECT_OPAQUE,          /**< RGB/YUV (alpha set to 1.0) */
	OBS_EFFECT_SOLID,           /**< RGB/YUV (solid color only) */
	OBS_EFFECT_BICUBIC,         /**< Bicubic downscale */
	OBS_EFFECT_LANCZOS,         /**< Lanczos downscale */
	OBS_EFFECT_BILINEAR_LOWRES, /**< Bilinear low resolution downscale */
	OBS_EFFECT_PREMULTIPLIED_ALPHA, /**< Premultiplied alpha */
	OBS_EFFECT_REPEAT,              /**< RGB/YUV (repeating) */
	OBS_EFFECT_AREA,                /**< Area rescale */
};

/** Returns a commonly used base effect */
EXPORT gs_effect_t *obs_get_base_effect(enum obs_base_effect effect);

#ifndef SWIG
/* DEPRECATED: gets texture_rect default effect */
OBS_DEPRECATED
EXPORT gs_effect_t *obs_get_default_rect_effect(void);
#endif

/** Returns the primary obs signal handler */
EXPORT signal_handler_t *obs_get_signal_handler(void);

/** Returns the primary obs procedure handler */
EXPORT proc_handler_t *obs_get_proc_handler(void);

#ifndef SWIG
/** Renders the main view */
OBS_DEPRECATED
EXPORT void obs_render_main_view(void);
#endif

/** Renders the last main output texture */
EXPORT void obs_render_main_texture(void);

/** Renders the last main output texture ignoring background color */
EXPORT void obs_render_main_texture_src_color_only(void);

/** Returns the last main output texture.  This can return NULL if the texture
 * is unavailable. */
EXPORT gs_texture_t *obs_get_main_texture(void);

/** Sets the master user volume */
EXPORT void obs_set_master_volume(float volume);

/** Gets the master user volume */
EXPORT float obs_get_master_volume(void);

/** Saves a source to settings data */
EXPORT obs_data_t *obs_save_source(obs_source_t *source);

/** Loads a source from settings data */
EXPORT obs_source_t *obs_load_source(obs_data_t *data);

/** Send a save signal to sources */
EXPORT void obs_source_save(obs_source_t *source);

/** Send a load signal to sources */
EXPORT void obs_source_load(obs_source_t *source);

typedef void (*obs_load_source_cb)(void *private_data, obs_source_t *source);

/** Loads sources from a data array */
EXPORT void obs_load_sources(obs_data_array_t *array, obs_load_source_cb cb,
			     void *private_data);

/** Saves sources to a data array */
EXPORT obs_data_array_t *obs_save_sources(void);

typedef bool (*obs_save_source_filter_cb)(void *data, obs_source_t *source);
EXPORT obs_data_array_t *obs_save_sources_filtered(obs_save_source_filter_cb cb,
						   void *data);

enum obs_obj_type {
	OBS_OBJ_TYPE_INVALID,
	OBS_OBJ_TYPE_SOURCE,
	OBS_OBJ_TYPE_OUTPUT,
	OBS_OBJ_TYPE_ENCODER,
	OBS_OBJ_TYPE_SERVICE,
};

EXPORT enum obs_obj_type obs_obj_get_type(void *obj);
EXPORT const char *obs_obj_get_id(void *obj);
EXPORT bool obs_obj_invalid(void *obj);
EXPORT void *obs_obj_get_data(void *obj);

typedef bool (*obs_enum_audio_device_cb)(void *data, const char *name,
					 const char *id);

EXPORT void obs_enum_audio_monitoring_devices(obs_enum_audio_device_cb cb,
					      void *data);

EXPORT bool obs_set_audio_monitoring_device(const char *name, const char *id);
EXPORT void obs_get_audio_monitoring_device(const char **name, const char **id);

EXPORT void obs_add_tick_callback(void (*tick)(void *param, float seconds),
				  void *param);
EXPORT void obs_remove_tick_callback(void (*tick)(void *param, float seconds),
				     void *param);

EXPORT void obs_add_main_render_callback(void (*draw)(void *param, uint32_t cx,
						      uint32_t cy),
					 void *param);
EXPORT void obs_remove_main_render_callback(
	void (*draw)(void *param, uint32_t cx, uint32_t cy), void *param);

EXPORT void obs_add_raw_video_callback(
	const struct video_scale_info *conversion,
	void (*callback)(void *param, struct video_data *frame), void *param);
EXPORT void obs_remove_raw_video_callback(
	void (*callback)(void *param, struct video_data *frame), void *param);

EXPORT uint64_t obs_get_video_frame_time(void);

EXPORT double obs_get_active_fps(void);
EXPORT uint64_t obs_get_average_frame_time_ns(void);
EXPORT uint64_t obs_get_frame_interval_ns(void);

EXPORT uint32_t obs_get_total_frames(void);
EXPORT uint32_t obs_get_lagged_frames(void);

EXPORT bool obs_nv12_tex_active(void);

EXPORT void obs_apply_private_data(obs_data_t *settings);
EXPORT void obs_set_private_data(obs_data_t *settings);
EXPORT obs_data_t *obs_get_private_data(void);

typedef void (*obs_task_t)(void *param);

enum obs_task_type {
	OBS_TASK_UI,
	OBS_TASK_GRAPHICS,
};

EXPORT void obs_queue_task(enum obs_task_type type, obs_task_t task,
			   void *param, bool wait);

typedef void (*obs_task_handler_t)(obs_task_t task, void *param, bool wait);
EXPORT void obs_set_ui_task_handler(obs_task_handler_t handler);

/* ------------------------------------------------------------------------- */
/* View context */

/**
 * Creates a view context.
 *
 *   A view can be used for things like separate previews, or drawing
 * sources separately.
 */
EXPORT obs_view_t *obs_view_create(void);

/** Destroys this view context */
EXPORT void obs_view_destroy(obs_view_t *view);

/** Sets the source to be used for this view context. */
EXPORT void obs_view_set_source(obs_view_t *view, uint32_t channel,
				obs_source_t *source);

/** Gets the source currently in use for this view context */
EXPORT obs_source_t *obs_view_get_source(obs_view_t *view, uint32_t channel);

/** Renders the sources of this view context */
EXPORT void obs_view_render(obs_view_t *view);

/* ------------------------------------------------------------------------- */
/* Display context */

/**
 * Adds a new window display linked to the main render pipeline.  This creates
 * a new swap chain which updates every frame.
 *
 * @param  graphics_data  The swap chain initialization data.
 * @return                The new display context, or NULL if failed.
 */
EXPORT obs_display_t *
obs_display_create(const struct gs_init_data *graphics_data,
		   uint32_t backround_color);

/** Destroys a display context */
EXPORT void obs_display_destroy(obs_display_t *display);

/** Changes the size of this display */
EXPORT void obs_display_resize(obs_display_t *display, uint32_t cx,
			       uint32_t cy);

/**
 * Adds a draw callback for this display context
 *
 * @param  display  The display context.
 * @param  draw     The draw callback which is called each time a frame
 *                  updates.
 * @param  param    The user data to be associated with this draw callback.
 */
EXPORT void obs_display_add_draw_callback(obs_display_t *display,
					  void (*draw)(void *param, uint32_t cx,
						       uint32_t cy),
					  void *param);

/** Removes a draw callback for this display context */
EXPORT void obs_display_remove_draw_callback(
	obs_display_t *display,
	void (*draw)(void *param, uint32_t cx, uint32_t cy), void *param);

EXPORT void obs_display_set_enabled(obs_display_t *display, bool enable);
EXPORT bool obs_display_enabled(obs_display_t *display);

EXPORT void obs_display_set_background_color(obs_display_t *display,
					     uint32_t color);

EXPORT void obs_display_size(obs_display_t *display, uint32_t *width,
			     uint32_t *height);

/* ------------------------------------------------------------------------- */
/* Sources */

/** Returns the translated display name of a source */
EXPORT const char *obs_source_get_display_name(const char *id);

/**
 * Creates a source of the specified type with the specified settings.
 *
 *   The "source" context is used for anything related to presenting
 * or modifying video/audio.  Use obs_source_release to release it.
 */
EXPORT obs_source_t *obs_source_create(const char *id, const char *name,
				       obs_data_t *settings,
				       obs_data_t *hotkey_data);

EXPORT obs_source_t *obs_source_create_private(const char *id, const char *name,
					       obs_data_t *settings);

/* if source has OBS_SOURCE_DO_NOT_DUPLICATE output flag set, only returns a
 * reference */
EXPORT obs_source_t *obs_source_duplicate(obs_source_t *source,
					  const char *desired_name,
					  bool create_private);
/**
 * Adds/releases a reference to a source.  When the last reference is
 * released, the source is destroyed.
 */
EXPORT void obs_source_addref(obs_source_t *source);
EXPORT void obs_source_release(obs_source_t *source);

EXPORT void obs_weak_source_addref(obs_weak_source_t *weak);
EXPORT void obs_weak_source_release(obs_weak_source_t *weak);

EXPORT obs_source_t *obs_source_get_ref(obs_source_t *source);
EXPORT obs_weak_source_t *obs_source_get_weak_source(obs_source_t *source);
EXPORT obs_source_t *obs_weak_source_get_source(obs_weak_source_t *weak);

EXPORT bool obs_weak_source_references_source(obs_weak_source_t *weak,
					      obs_source_t *source);

/** Notifies all references that the source should be released */
EXPORT void obs_source_remove(obs_source_t *source);

/** Returns true if the source should be released */
EXPORT bool obs_source_removed(const obs_source_t *source);

/** Returns capability flags of a source */
EXPORT uint32_t obs_source_get_output_flags(const obs_source_t *source);

/** Returns capability flags of a source type */
EXPORT uint32_t obs_get_source_output_flags(const char *id);

/** Gets the default settings for a source type */
EXPORT obs_data_t *obs_get_source_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t *obs_get_source_properties(const char *id);

EXPORT obs_missing_files_t *
obs_source_get_missing_files(const obs_source_t *source);

EXPORT void obs_source_replace_missing_file(obs_missing_file_cb cb,
					    obs_source_t *source,
					    const char *new_path, void *data);

/** Returns whether the source has custom properties or not */
EXPORT bool obs_is_source_configurable(const char *id);

EXPORT bool obs_source_configurable(const obs_source_t *source);

/**
 * Returns the properties list for a specific existing source.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t *obs_source_properties(const obs_source_t *source);

/** Updates settings for this source */
EXPORT void obs_source_update(obs_source_t *source, obs_data_t *settings);

/** Renders a video source. */
EXPORT void obs_source_video_render(obs_source_t *source);

/** Gets the width of a source (if it has video) */
EXPORT uint32_t obs_source_get_width(obs_source_t *source);

/** Gets the height of a source (if it has video) */
EXPORT uint32_t obs_source_get_height(obs_source_t *source);

/**
 * If the source is a filter, returns the parent source of the filter.  Only
 * guaranteed to be valid inside of the video_render, filter_audio,
 * filter_video, and filter_remove callbacks.
 */
EXPORT obs_source_t *obs_filter_get_parent(const obs_source_t *filter);

/**
 * If the source is a filter, returns the target source of the filter.  Only
 * guaranteed to be valid inside of the video_render, filter_audio,
 * filter_video, and filter_remove callbacks.
 */
EXPORT obs_source_t *obs_filter_get_target(const obs_source_t *filter);

/** Used to directly render a non-async source without any filter processing */
EXPORT void obs_source_default_render(obs_source_t *source);

/** Adds a filter to the source (which is used whenever the source is used) */
EXPORT void obs_source_filter_add(obs_source_t *source, obs_source_t *filter);

/** Removes a filter from the source */
EXPORT void obs_source_filter_remove(obs_source_t *source,
				     obs_source_t *filter);

/** Modifies the order of a specific filter */
EXPORT void obs_source_filter_set_order(obs_source_t *source,
					obs_source_t *filter,
					enum obs_order_movement movement);

/** Gets the settings string for a source */
EXPORT obs_data_t *obs_source_get_settings(const obs_source_t *source);

/** Gets the name of a source */
EXPORT const char *obs_source_get_name(const obs_source_t *source);

/** Sets the name of a source */
EXPORT void obs_source_set_name(obs_source_t *source, const char *name);

/** Gets the source type */
EXPORT enum obs_source_type obs_source_get_type(const obs_source_t *source);

/** Gets the source identifier */
EXPORT const char *obs_source_get_id(const obs_source_t *source);
EXPORT const char *obs_source_get_unversioned_id(const obs_source_t *source);

/** Returns the signal handler for a source */
EXPORT signal_handler_t *
obs_source_get_signal_handler(const obs_source_t *source);

/** Returns the procedure handler for a source */
EXPORT proc_handler_t *obs_source_get_proc_handler(const obs_source_t *source);

/** Sets the user volume for a source that has audio output */
EXPORT void obs_source_set_volume(obs_source_t *source, float volume);

/** Gets the user volume for a source that has audio output */
EXPORT float obs_source_get_volume(const obs_source_t *source);

/* Gets speaker layout of a source */
EXPORT enum speaker_layout obs_source_get_speaker_layout(obs_source_t *source);

/** Sets the balance value for a stereo audio source */
EXPORT void obs_source_set_balance_value(obs_source_t *source, float balance);

/** Gets the balance value for a stereo audio source */
EXPORT float obs_source_get_balance_value(const obs_source_t *source);

/** Sets the audio sync offset (in nanoseconds) for a source */
EXPORT void obs_source_set_sync_offset(obs_source_t *source, int64_t offset);

/** Gets the audio sync offset (in nanoseconds) for a source */
EXPORT int64_t obs_source_get_sync_offset(const obs_source_t *source);

/** Enumerates active child sources used by this source */
EXPORT void obs_source_enum_active_sources(obs_source_t *source,
					   obs_source_enum_proc_t enum_callback,
					   void *param);

/** Enumerates the entire active child source tree used by this source */
EXPORT void obs_source_enum_active_tree(obs_source_t *source,
					obs_source_enum_proc_t enum_callback,
					void *param);

/** Returns true if active, false if not */
EXPORT bool obs_source_active(const obs_source_t *source);

/**
 * Returns true if currently displayed somewhere (active or not), false if not
 */
EXPORT bool obs_source_showing(const obs_source_t *source);

/** Unused flag */
#define OBS_SOURCE_FLAG_UNUSED_1 (1 << 0)
/** Specifies to force audio to mono */
#define OBS_SOURCE_FLAG_FORCE_MONO (1 << 1)

/**
 * Sets source flags.  Note that these are different from the main output
 * flags.  These are generally things that can be set by the source or user,
 * while the output flags are more used to determine capabilities of a source.
 */
EXPORT void obs_source_set_flags(obs_source_t *source, uint32_t flags);

/** Gets source flags. */
EXPORT uint32_t obs_source_get_flags(const obs_source_t *source);

/**
 * Sets audio mixer flags.  These flags are used to specify which mixers
 * the source's audio should be applied to.
 */
EXPORT void obs_source_set_audio_mixers(obs_source_t *source, uint32_t mixers);

/** Gets audio mixer flags */
EXPORT uint32_t obs_source_get_audio_mixers(const obs_source_t *source);

/**
 * Increments the 'showing' reference counter to indicate that the source is
 * being shown somewhere.  If the reference counter was 0, will call the 'show'
 * callback.
 */
EXPORT void obs_source_inc_showing(obs_source_t *source);

/**
 * Increments the 'active' reference counter to indicate that the source is
 * fully active.  If the reference counter was 0, will call the 'activate'
 * callback.
 *
 * Unlike obs_source_inc_showing, this will cause children of this source to be
 * considered showing as well (currently used by transition previews to make
 * the stinger transition show correctly).  obs_source_inc_showing should
 * generally be used instead.
 */
EXPORT void obs_source_inc_active(obs_source_t *source);

/**
 * Decrements the 'showing' reference counter to indicate that the source is
 * no longer being shown somewhere.  If the reference counter is set to 0,
 * will call the 'hide' callback
 */
EXPORT void obs_source_dec_showing(obs_source_t *source);

/**
 * Decrements the 'active' reference counter to indicate that the source is no
 * longer fully active.  If the reference counter is set to 0, will call the
 * 'deactivate' callback
 *
 * Unlike obs_source_dec_showing, this will cause children of this source to be
 * considered not showing as well.  obs_source_dec_showing should generally be
 * used instead.
 */
EXPORT void obs_source_dec_active(obs_source_t *source);

/** Enumerates filters assigned to the source */
EXPORT void obs_source_enum_filters(obs_source_t *source,
				    obs_source_enum_proc_t callback,
				    void *param);

/** Gets a filter of a source by its display name. */
EXPORT obs_source_t *obs_source_get_filter_by_name(obs_source_t *source,
						   const char *name);

EXPORT void obs_source_copy_filters(obs_source_t *dst, obs_source_t *src);
EXPORT void obs_source_copy_single_filter(obs_source_t *dst,
					  obs_source_t *filter);

EXPORT bool obs_source_enabled(const obs_source_t *source);
EXPORT void obs_source_set_enabled(obs_source_t *source, bool enabled);

EXPORT bool obs_source_muted(const obs_source_t *source);
EXPORT void obs_source_set_muted(obs_source_t *source, bool muted);

EXPORT bool obs_source_push_to_mute_enabled(obs_source_t *source);
EXPORT void obs_source_enable_push_to_mute(obs_source_t *source, bool enabled);

EXPORT uint64_t obs_source_get_push_to_mute_delay(obs_source_t *source);
EXPORT void obs_source_set_push_to_mute_delay(obs_source_t *source,
					      uint64_t delay);

EXPORT bool obs_source_push_to_talk_enabled(obs_source_t *source);
EXPORT void obs_source_enable_push_to_talk(obs_source_t *source, bool enabled);

EXPORT uint64_t obs_source_get_push_to_talk_delay(obs_source_t *source);
EXPORT void obs_source_set_push_to_talk_delay(obs_source_t *source,
					      uint64_t delay);

typedef void (*obs_source_audio_capture_t)(void *param, obs_source_t *source,
					   const struct audio_data *audio_data,
					   bool muted);

EXPORT void obs_source_add_audio_capture_callback(
	obs_source_t *source, obs_source_audio_capture_t callback, void *param);
EXPORT void obs_source_remove_audio_capture_callback(
	obs_source_t *source, obs_source_audio_capture_t callback, void *param);

typedef void (*obs_source_caption_t)(void *param, obs_source_t *source,
				     const struct obs_source_cea_708 *captions);

EXPORT void obs_source_add_caption_callback(obs_source_t *source,
					    obs_source_caption_t callback,
					    void *param);
EXPORT void obs_source_remove_caption_callback(obs_source_t *source,
					       obs_source_caption_t callback,
					       void *param);

enum obs_deinterlace_mode {
	OBS_DEINTERLACE_MODE_DISABLE,
	OBS_DEINTERLACE_MODE_DISCARD,
	OBS_DEINTERLACE_MODE_RETRO,
	OBS_DEINTERLACE_MODE_BLEND,
	OBS_DEINTERLACE_MODE_BLEND_2X,
	OBS_DEINTERLACE_MODE_LINEAR,
	OBS_DEINTERLACE_MODE_LINEAR_2X,
	OBS_DEINTERLACE_MODE_YADIF,
	OBS_DEINTERLACE_MODE_YADIF_2X,
};

enum obs_deinterlace_field_order {
	OBS_DEINTERLACE_FIELD_ORDER_TOP,
	OBS_DEINTERLACE_FIELD_ORDER_BOTTOM,
};

EXPORT void obs_source_set_deinterlace_mode(obs_source_t *source,
					    enum obs_deinterlace_mode mode);
EXPORT enum obs_deinterlace_mode
obs_source_get_deinterlace_mode(const obs_source_t *source);
EXPORT void obs_source_set_deinterlace_field_order(
	obs_source_t *source, enum obs_deinterlace_field_order field_order);
EXPORT enum obs_deinterlace_field_order
obs_source_get_deinterlace_field_order(const obs_source_t *source);

enum obs_monitoring_type {
	OBS_MONITORING_TYPE_NONE,
	OBS_MONITORING_TYPE_MONITOR_ONLY,
	OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT,
};

EXPORT void obs_source_set_monitoring_type(obs_source_t *source,
					   enum obs_monitoring_type type);
EXPORT enum obs_monitoring_type
obs_source_get_monitoring_type(const obs_source_t *source);

/** Gets private front-end settings data.  This data is saved/loaded
 * automatically.  Returns an incremented reference. */
EXPORT obs_data_t *obs_source_get_private_settings(obs_source_t *item);

/* ------------------------------------------------------------------------- */
/* Functions used by sources */

EXPORT void *obs_source_get_type_data(obs_source_t *source);

/**
 * Helper function to set the color matrix information when drawing the source.
 *
 * @param  color_matrix     The color matrix.  Assigns to the 'color_matrix'
 *                          effect variable.
 * @param  color_range_min  The minimum color range.  Assigns to the
 *                          'color_range_min' effect variable.  If NULL,
 *                          {0.0f, 0.0f, 0.0f} is used.
 * @param  color_range_max  The maximum color range.  Assigns to the
 *                          'color_range_max' effect variable.  If NULL,
 *                          {1.0f, 1.0f, 1.0f} is used.
 */
EXPORT void
obs_source_draw_set_color_matrix(const struct matrix4 *color_matrix,
				 const struct vec3 *color_range_min,
				 const struct vec3 *color_range_max);

/**
 * Helper function to draw sprites for a source (synchronous video).
 *
 * @param  image   The sprite texture to draw.  Assigns to the 'image' variable
 *                 of the current effect.
 * @param  x       X position of the sprite.
 * @param  y       Y position of the sprite.
 * @param  cx      Width of the sprite.  If 0, uses the texture width.
 * @param  cy      Height of the sprite.  If 0, uses the texture height.
 * @param  flip    Specifies whether to flip the image vertically.
 */
EXPORT void obs_source_draw(gs_texture_t *image, int x, int y, uint32_t cx,
			    uint32_t cy, bool flip);

/**
 * Outputs asynchronous video data.  Set to NULL to deactivate the texture
 *
 * NOTE: Non-YUV formats will always be treated as full range with this
 * function!  Use obs_source_output_video2 instead if partial range support is
 * desired for non-YUV video formats.
 */
EXPORT void obs_source_output_video(obs_source_t *source,
				    const struct obs_source_frame *frame);
EXPORT void obs_source_output_video2(obs_source_t *source,
				     const struct obs_source_frame2 *frame);

EXPORT void obs_source_set_async_rotation(obs_source_t *source, long rotation);

EXPORT void obs_source_output_cea708(obs_source_t *source,
				     const struct obs_source_cea_708 *captions);

/**
 * Preloads asynchronous video data to allow instantaneous playback
 *
 * NOTE: Non-YUV formats will always be treated as full range with this
 * function!  Use obs_source_preload_video2 instead if partial range support is
 * desired for non-YUV video formats.
 */
EXPORT void obs_source_preload_video(obs_source_t *source,
				     const struct obs_source_frame *frame);
EXPORT void obs_source_preload_video2(obs_source_t *source,
				      const struct obs_source_frame2 *frame);

/** Shows any preloaded video data */
EXPORT void obs_source_show_preloaded_video(obs_source_t *source);

/**
 * Sets current async video frame immediately
 *
 * NOTE: Non-YUV formats will always be treated as full range with this
 * function!  Use obs_source_preload_video2 instead if partial range support is
 * desired for non-YUV video formats.
 */
EXPORT void obs_source_set_video_frame(obs_source_t *source,
				       const struct obs_source_frame *frame);
EXPORT void obs_source_set_video_frame2(obs_source_t *source,
					const struct obs_source_frame2 *frame);

/** Outputs audio data (always asynchronous) */
EXPORT void obs_source_output_audio(obs_source_t *source,
				    const struct obs_source_audio *audio);

/** Signal an update to any currently used properties via 'update_properties' */
EXPORT void obs_source_update_properties(obs_source_t *source);

/** Gets the current async video frame */
EXPORT struct obs_source_frame *obs_source_get_frame(obs_source_t *source);

/** Releases the current async video frame */
EXPORT void obs_source_release_frame(obs_source_t *source,
				     struct obs_source_frame *frame);

/**
 * Default RGB filter handler for generic effect filters.  Processes the
 * filter chain and renders them to texture if needed, then the filter is
 * drawn with
 *
 * After calling this, set your parameters for the effect, then call
 * obs_source_process_filter_end to draw the filter.
 *
 * Returns true if filtering should continue, false if the filter is bypassed
 * for whatever reason.
 */
EXPORT bool
obs_source_process_filter_begin(obs_source_t *filter,
				enum gs_color_format format,
				enum obs_allow_direct_render allow_direct);

/**
 * Draws the filter.
 *
 * Before calling this function, first call obs_source_process_filter_begin and
 * then set the effect parameters, and then call this function to finalize the
 * filter.
 */
EXPORT void obs_source_process_filter_end(obs_source_t *filter,
					  gs_effect_t *effect, uint32_t width,
					  uint32_t height);

/**
 * Draws the filter with a specific technique.
 *
 * Before calling this function, first call obs_source_process_filter_begin and
 * then set the effect parameters, and then call this function to finalize the
 * filter.
 */
EXPORT void obs_source_process_filter_tech_end(obs_source_t *filter,
					       gs_effect_t *effect,
					       uint32_t width, uint32_t height,
					       const char *tech_name);

/** Skips the filter if the filter is invalid and cannot be rendered */
EXPORT void obs_source_skip_video_filter(obs_source_t *filter);

/**
 * Adds an active child source.  Must be called by parent sources on child
 * sources when the child is added and active.  This ensures that the source is
 * properly activated if the parent is active.
 *
 * @returns true if source can be added, false if it causes recursion
 */
EXPORT bool obs_source_add_active_child(obs_source_t *parent,
					obs_source_t *child);

/**
 * Removes an active child source.  Must be called by parent sources on child
 * sources when the child is removed or inactive.  This ensures that the source
 * is properly deactivated if the parent is no longer active.
 */
EXPORT void obs_source_remove_active_child(obs_source_t *parent,
					   obs_source_t *child);

/** Sends a mouse down/up event to a source */
EXPORT void obs_source_send_mouse_click(obs_source_t *source,
					const struct obs_mouse_event *event,
					int32_t type, bool mouse_up,
					uint32_t click_count);

/** Sends a mouse move event to a source. */
EXPORT void obs_source_send_mouse_move(obs_source_t *source,
				       const struct obs_mouse_event *event,
				       bool mouse_leave);

/** Sends a mouse wheel event to a source */
EXPORT void obs_source_send_mouse_wheel(obs_source_t *source,
					const struct obs_mouse_event *event,
					int x_delta, int y_delta);

/** Sends a got-focus or lost-focus event to a source */
EXPORT void obs_source_send_focus(obs_source_t *source, bool focus);

/** Sends a key up/down event to a source */
EXPORT void obs_source_send_key_click(obs_source_t *source,
				      const struct obs_key_event *event,
				      bool key_up);

/** Sets the default source flags. */
EXPORT void obs_source_set_default_flags(obs_source_t *source, uint32_t flags);

/** Gets the base width for a source (not taking in to account filtering) */
EXPORT uint32_t obs_source_get_base_width(obs_source_t *source);

/** Gets the base height for a source (not taking in to account filtering) */
EXPORT uint32_t obs_source_get_base_height(obs_source_t *source);

EXPORT bool obs_source_audio_pending(const obs_source_t *source);
EXPORT uint64_t obs_source_get_audio_timestamp(const obs_source_t *source);
EXPORT void obs_source_get_audio_mix(const obs_source_t *source,
				     struct obs_source_audio_mix *audio);

EXPORT void obs_source_set_async_unbuffered(obs_source_t *source,
					    bool unbuffered);
EXPORT bool obs_source_async_unbuffered(const obs_source_t *source);

/** Used to decouple audio from video so that audio doesn't attempt to sync up
 * with video.  I.E. Audio acts independently.  Only works when in unbuffered
 * mode. */
EXPORT void obs_source_set_async_decoupled(obs_source_t *source, bool decouple);
EXPORT bool obs_source_async_decoupled(const obs_source_t *source);

EXPORT void obs_source_set_audio_active(obs_source_t *source, bool show);
EXPORT bool obs_source_audio_active(const obs_source_t *source);

EXPORT uint32_t obs_source_get_last_obs_version(const obs_source_t *source);

/** Media controls */
EXPORT void obs_source_media_play_pause(obs_source_t *source, bool pause);
EXPORT void obs_source_media_restart(obs_source_t *source);
EXPORT void obs_source_media_stop(obs_source_t *source);
EXPORT void obs_source_media_next(obs_source_t *source);
EXPORT void obs_source_media_previous(obs_source_t *source);
EXPORT int64_t obs_source_media_get_duration(obs_source_t *source);
EXPORT int64_t obs_source_media_get_time(obs_source_t *source);
EXPORT void obs_source_media_set_time(obs_source_t *source, int64_t ms);
EXPORT enum obs_media_state obs_source_media_get_state(obs_source_t *source);
EXPORT void obs_source_media_started(obs_source_t *source);
EXPORT void obs_source_media_ended(obs_source_t *source);

/* ------------------------------------------------------------------------- */
/* Transition-specific functions */
enum obs_transition_target {
	OBS_TRANSITION_SOURCE_A,
	OBS_TRANSITION_SOURCE_B,
};

EXPORT obs_source_t *
obs_transition_get_source(obs_source_t *transition,
			  enum obs_transition_target target);
EXPORT void obs_transition_clear(obs_source_t *transition);

EXPORT obs_source_t *obs_transition_get_active_source(obs_source_t *transition);

enum obs_transition_mode {
	OBS_TRANSITION_MODE_AUTO,
	OBS_TRANSITION_MODE_MANUAL,
};

EXPORT bool obs_transition_start(obs_source_t *transition,
				 enum obs_transition_mode mode,
				 uint32_t duration_ms, obs_source_t *dest);

EXPORT void obs_transition_set(obs_source_t *transition, obs_source_t *source);

EXPORT void obs_transition_set_manual_time(obs_source_t *transition, float t);
EXPORT void obs_transition_set_manual_torque(obs_source_t *transition,
					     float torque, float clamp);

enum obs_transition_scale_type {
	OBS_TRANSITION_SCALE_MAX_ONLY,
	OBS_TRANSITION_SCALE_ASPECT,
	OBS_TRANSITION_SCALE_STRETCH,
};

EXPORT void obs_transition_set_scale_type(obs_source_t *transition,
					  enum obs_transition_scale_type type);
EXPORT enum obs_transition_scale_type
obs_transition_get_scale_type(const obs_source_t *transition);

EXPORT void obs_transition_set_alignment(obs_source_t *transition,
					 uint32_t alignment);
EXPORT uint32_t obs_transition_get_alignment(const obs_source_t *transition);

EXPORT void obs_transition_set_size(obs_source_t *transition, uint32_t cx,
				    uint32_t cy);
EXPORT void obs_transition_get_size(const obs_source_t *transition,
				    uint32_t *cx, uint32_t *cy);

/* function used by transitions */

/**
 * Enables fixed transitions (videos or specific types of transitions that
 * are of fixed duration and linearly interpolated
 */
EXPORT void obs_transition_enable_fixed(obs_source_t *transition, bool enable,
					uint32_t duration_ms);
EXPORT bool obs_transition_fixed(obs_source_t *transition);

typedef void (*obs_transition_video_render_callback_t)(void *data,
						       gs_texture_t *a,
						       gs_texture_t *b, float t,
						       uint32_t cx,
						       uint32_t cy);
typedef float (*obs_transition_audio_mix_callback_t)(void *data, float t);

EXPORT float obs_transition_get_time(obs_source_t *transition);

EXPORT void obs_transition_force_stop(obs_source_t *transition);

EXPORT void
obs_transition_video_render(obs_source_t *transition,
			    obs_transition_video_render_callback_t callback);

/** Directly renders its sub-source instead of to texture.  Returns false if no
 * longer transitioning */
EXPORT bool
obs_transition_video_render_direct(obs_source_t *transition,
				   enum obs_transition_target target);

EXPORT bool
obs_transition_audio_render(obs_source_t *transition, uint64_t *ts_out,
			    struct obs_source_audio_mix *audio, uint32_t mixers,
			    size_t channels, size_t sample_rate,
			    obs_transition_audio_mix_callback_t mix_a_callback,
			    obs_transition_audio_mix_callback_t mix_b_callback);

/* swaps transition sources and textures as an optimization and to reduce
 * memory usage when switching between transitions */
EXPORT void obs_transition_swap_begin(obs_source_t *tr_dest,
				      obs_source_t *tr_source);
EXPORT void obs_transition_swap_end(obs_source_t *tr_dest,
				    obs_source_t *tr_source);

/* ------------------------------------------------------------------------- */
/* Scenes */

/**
 * Creates a scene.
 *
 *   A scene is a source which is a container of other sources with specific
 * display orientations.  Scenes can also be used like any other source.
 */
EXPORT obs_scene_t *obs_scene_create(const char *name);

EXPORT obs_scene_t *obs_scene_create_private(const char *name);

enum obs_scene_duplicate_type {
	OBS_SCENE_DUP_REFS,         /**< Source refs only */
	OBS_SCENE_DUP_COPY,         /**< Fully duplicate */
	OBS_SCENE_DUP_PRIVATE_REFS, /**< Source refs only (as private) */
	OBS_SCENE_DUP_PRIVATE_COPY, /**< Fully duplicate (as private) */
};

/**
 * Duplicates a scene.
 */
EXPORT obs_scene_t *obs_scene_duplicate(obs_scene_t *scene, const char *name,
					enum obs_scene_duplicate_type type);

EXPORT void obs_scene_addref(obs_scene_t *scene);
EXPORT void obs_scene_release(obs_scene_t *scene);

/** Gets the scene's source context */
EXPORT obs_source_t *obs_scene_get_source(const obs_scene_t *scene);

/** Gets the scene from its source, or NULL if not a scene */
EXPORT obs_scene_t *obs_scene_from_source(const obs_source_t *source);

/** Determines whether a source is within a scene */
EXPORT obs_sceneitem_t *obs_scene_find_source(obs_scene_t *scene,
					      const char *name);

EXPORT obs_sceneitem_t *obs_scene_find_source_recursive(obs_scene_t *scene,
							const char *name);

EXPORT obs_sceneitem_t *obs_scene_find_sceneitem_by_id(obs_scene_t *scene,
						       int64_t id);

/** Enumerates sources within a scene */
EXPORT void obs_scene_enum_items(obs_scene_t *scene,
				 bool (*callback)(obs_scene_t *,
						  obs_sceneitem_t *, void *),
				 void *param);

EXPORT bool obs_scene_reorder_items(obs_scene_t *scene,
				    obs_sceneitem_t *const *item_order,
				    size_t item_order_size);

struct obs_sceneitem_order_info {
	obs_sceneitem_t *group;
	obs_sceneitem_t *item;
};

EXPORT bool
obs_scene_reorder_items2(obs_scene_t *scene,
			 struct obs_sceneitem_order_info *item_order,
			 size_t item_order_size);

/** Adds/creates a new scene item for a source */
EXPORT obs_sceneitem_t *obs_scene_add(obs_scene_t *scene, obs_source_t *source);

typedef void (*obs_scene_atomic_update_func)(void *, obs_scene_t *scene);
EXPORT void obs_scene_atomic_update(obs_scene_t *scene,
				    obs_scene_atomic_update_func func,
				    void *data);

EXPORT void obs_sceneitem_addref(obs_sceneitem_t *item);
EXPORT void obs_sceneitem_release(obs_sceneitem_t *item);

/** Removes a scene item. */
EXPORT void obs_sceneitem_remove(obs_sceneitem_t *item);

/** Gets the scene parent associated with the scene item. */
EXPORT obs_scene_t *obs_sceneitem_get_scene(const obs_sceneitem_t *item);

/** Gets the source of a scene item. */
EXPORT obs_source_t *obs_sceneitem_get_source(const obs_sceneitem_t *item);

/* FIXME: The following functions should be deprecated and replaced with a way
 * to specify saveable private user data. -Jim */
EXPORT void obs_sceneitem_select(obs_sceneitem_t *item, bool select);
EXPORT bool obs_sceneitem_selected(const obs_sceneitem_t *item);
EXPORT bool obs_sceneitem_locked(const obs_sceneitem_t *item);
EXPORT bool obs_sceneitem_set_locked(obs_sceneitem_t *item, bool lock);

/* Functions for getting/setting specific orientation of a scene item */
EXPORT void obs_sceneitem_set_pos(obs_sceneitem_t *item,
				  const struct vec2 *pos);
EXPORT void obs_sceneitem_set_rot(obs_sceneitem_t *item, float rot_deg);
EXPORT void obs_sceneitem_set_scale(obs_sceneitem_t *item,
				    const struct vec2 *scale);
EXPORT void obs_sceneitem_set_alignment(obs_sceneitem_t *item,
					uint32_t alignment);
EXPORT void obs_sceneitem_set_order(obs_sceneitem_t *item,
				    enum obs_order_movement movement);
EXPORT void obs_sceneitem_set_order_position(obs_sceneitem_t *item,
					     int position);
EXPORT void obs_sceneitem_set_bounds_type(obs_sceneitem_t *item,
					  enum obs_bounds_type type);
EXPORT void obs_sceneitem_set_bounds_alignment(obs_sceneitem_t *item,
					       uint32_t alignment);
EXPORT void obs_sceneitem_set_bounds(obs_sceneitem_t *item,
				     const struct vec2 *bounds);

EXPORT int64_t obs_sceneitem_get_id(const obs_sceneitem_t *item);

EXPORT void obs_sceneitem_get_pos(const obs_sceneitem_t *item,
				  struct vec2 *pos);
EXPORT float obs_sceneitem_get_rot(const obs_sceneitem_t *item);
EXPORT void obs_sceneitem_get_scale(const obs_sceneitem_t *item,
				    struct vec2 *scale);
EXPORT uint32_t obs_sceneitem_get_alignment(const obs_sceneitem_t *item);

EXPORT enum obs_bounds_type
obs_sceneitem_get_bounds_type(const obs_sceneitem_t *item);
EXPORT uint32_t obs_sceneitem_get_bounds_alignment(const obs_sceneitem_t *item);
EXPORT void obs_sceneitem_get_bounds(const obs_sceneitem_t *item,
				     struct vec2 *bounds);

EXPORT void obs_sceneitem_get_info(const obs_sceneitem_t *item,
				   struct obs_transform_info *info);
EXPORT void obs_sceneitem_set_info(obs_sceneitem_t *item,
				   const struct obs_transform_info *info);

EXPORT void obs_sceneitem_get_draw_transform(const obs_sceneitem_t *item,
					     struct matrix4 *transform);
EXPORT void obs_sceneitem_get_box_transform(const obs_sceneitem_t *item,
					    struct matrix4 *transform);
EXPORT void obs_sceneitem_get_box_scale(const obs_sceneitem_t *item,
					struct vec2 *scale);

EXPORT bool obs_sceneitem_visible(const obs_sceneitem_t *item);
EXPORT bool obs_sceneitem_set_visible(obs_sceneitem_t *item, bool visible);

struct obs_sceneitem_crop {
	int left;
	int top;
	int right;
	int bottom;
};

EXPORT void obs_sceneitem_set_crop(obs_sceneitem_t *item,
				   const struct obs_sceneitem_crop *crop);
EXPORT void obs_sceneitem_get_crop(const obs_sceneitem_t *item,
				   struct obs_sceneitem_crop *crop);

EXPORT void obs_sceneitem_set_scale_filter(obs_sceneitem_t *item,
					   enum obs_scale_type filter);
EXPORT enum obs_scale_type
obs_sceneitem_get_scale_filter(obs_sceneitem_t *item);

EXPORT void obs_sceneitem_force_update_transform(obs_sceneitem_t *item);

EXPORT void obs_sceneitem_defer_update_begin(obs_sceneitem_t *item);
EXPORT void obs_sceneitem_defer_update_end(obs_sceneitem_t *item);

/** Gets private front-end settings data.  This data is saved/loaded
 * automatically.  Returns an incremented reference. */
EXPORT obs_data_t *obs_sceneitem_get_private_settings(obs_sceneitem_t *item);

EXPORT obs_sceneitem_t *obs_scene_add_group(obs_scene_t *scene,
					    const char *name);
EXPORT obs_sceneitem_t *obs_scene_insert_group(obs_scene_t *scene,
					       const char *name,
					       obs_sceneitem_t **items,
					       size_t count);

EXPORT obs_sceneitem_t *obs_scene_add_group2(obs_scene_t *scene,
					     const char *name, bool signal);
EXPORT obs_sceneitem_t *obs_scene_insert_group2(obs_scene_t *scene,
						const char *name,
						obs_sceneitem_t **items,
						size_t count, bool signal);

EXPORT obs_sceneitem_t *obs_scene_get_group(obs_scene_t *scene,
					    const char *name);

EXPORT bool obs_sceneitem_is_group(obs_sceneitem_t *item);

EXPORT obs_scene_t *obs_sceneitem_group_get_scene(const obs_sceneitem_t *group);

EXPORT void obs_sceneitem_group_ungroup(obs_sceneitem_t *group);
EXPORT void obs_sceneitem_group_ungroup2(obs_sceneitem_t *group, bool signal);

EXPORT void obs_sceneitem_group_add_item(obs_sceneitem_t *group,
					 obs_sceneitem_t *item);
EXPORT void obs_sceneitem_group_remove_item(obs_sceneitem_t *group,
					    obs_sceneitem_t *item);

EXPORT obs_sceneitem_t *obs_sceneitem_get_group(obs_scene_t *scene,
						obs_sceneitem_t *item);

EXPORT bool obs_source_is_group(const obs_source_t *source);
EXPORT bool obs_scene_is_group(const obs_scene_t *scene);

EXPORT void obs_sceneitem_group_enum_items(obs_sceneitem_t *group,
					   bool (*callback)(obs_scene_t *,
							    obs_sceneitem_t *,
							    void *),
					   void *param);

/** Gets the group from its source, or NULL if not a group */
EXPORT obs_scene_t *obs_group_from_source(const obs_source_t *source);

EXPORT void obs_sceneitem_defer_group_resize_begin(obs_sceneitem_t *item);
EXPORT void obs_sceneitem_defer_group_resize_end(obs_sceneitem_t *item);

/* ------------------------------------------------------------------------- */
/* Outputs */

EXPORT const char *obs_output_get_display_name(const char *id);

/**
 * Creates an output.
 *
 *   Outputs allow outputting to file, outputting to network, outputting to
 * directshow, or other custom outputs.
 */
EXPORT obs_output_t *obs_output_create(const char *id, const char *name,
				       obs_data_t *settings,
				       obs_data_t *hotkey_data);

/**
 * Adds/releases a reference to an output.  When the last reference is
 * released, the output is destroyed.
 */
EXPORT void obs_output_addref(obs_output_t *output);
EXPORT void obs_output_release(obs_output_t *output);

EXPORT void obs_weak_output_addref(obs_weak_output_t *weak);
EXPORT void obs_weak_output_release(obs_weak_output_t *weak);

EXPORT obs_output_t *obs_output_get_ref(obs_output_t *output);
EXPORT obs_weak_output_t *obs_output_get_weak_output(obs_output_t *output);
EXPORT obs_output_t *obs_weak_output_get_output(obs_weak_output_t *weak);

EXPORT bool obs_weak_output_references_output(obs_weak_output_t *weak,
					      obs_output_t *output);

EXPORT const char *obs_output_get_name(const obs_output_t *output);

/** Starts the output. */
EXPORT bool obs_output_start(obs_output_t *output);

/** Stops the output. */
EXPORT void obs_output_stop(obs_output_t *output);

/**
 * On reconnection, start where it left of on reconnection.  Note however that
 * this option will consume extra memory to continually increase delay while
 * waiting to reconnect.
 */
#define OBS_OUTPUT_DELAY_PRESERVE (1 << 0)

/**
 * Sets the current output delay, in seconds (if the output supports delay).
 *
 * If delay is currently active, it will set the delay value, but will not
 * affect the current delay, it will only affect the next time the output is
 * activated.
 */
EXPORT void obs_output_set_delay(obs_output_t *output, uint32_t delay_sec,
				 uint32_t flags);

/** Gets the currently set delay value, in seconds. */
EXPORT uint32_t obs_output_get_delay(const obs_output_t *output);

/** If delay is active, gets the currently active delay value, in seconds. */
EXPORT uint32_t obs_output_get_active_delay(const obs_output_t *output);

/** Forces the output to stop.  Usually only used with delay. */
EXPORT void obs_output_force_stop(obs_output_t *output);

/** Returns whether the output is active */
EXPORT bool obs_output_active(const obs_output_t *output);

/** Returns output capability flags */
EXPORT uint32_t obs_output_get_flags(const obs_output_t *output);

/** Returns output capability flags */
EXPORT uint32_t obs_get_output_flags(const char *id);

/** Gets the default settings for an output type */
EXPORT obs_data_t *obs_output_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t *obs_get_output_properties(const char *id);

/**
 * Returns the property list of an existing output, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t *obs_output_properties(const obs_output_t *output);

/** Updates the settings for this output context */
EXPORT void obs_output_update(obs_output_t *output, obs_data_t *settings);

/** Specifies whether the output can be paused */
EXPORT bool obs_output_can_pause(const obs_output_t *output);

/** Pauses the output (if the functionality is allowed by the output */
EXPORT bool obs_output_pause(obs_output_t *output, bool pause);

/** Returns whether output is paused */
EXPORT bool obs_output_paused(const obs_output_t *output);

/* Gets the current output settings string */
EXPORT obs_data_t *obs_output_get_settings(const obs_output_t *output);

/** Returns the signal handler for an output  */
EXPORT signal_handler_t *
obs_output_get_signal_handler(const obs_output_t *output);

/** Returns the procedure handler for an output */
EXPORT proc_handler_t *obs_output_get_proc_handler(const obs_output_t *output);

/**
 * Sets the current audio/video media contexts associated with this output,
 * required for non-encoded outputs.  Can be null.
 */
EXPORT void obs_output_set_media(obs_output_t *output, video_t *video,
				 audio_t *audio);

/** Returns the video media context associated with this output */
EXPORT video_t *obs_output_video(const obs_output_t *output);

/** Returns the audio media context associated with this output */
EXPORT audio_t *obs_output_audio(const obs_output_t *output);

/** Sets the current audio mixer for non-encoded outputs */
EXPORT void obs_output_set_mixer(obs_output_t *output, size_t mixer_idx);

/** Gets the current audio mixer for non-encoded outputs */
EXPORT size_t obs_output_get_mixer(const obs_output_t *output);

/** Sets the current audio mixes (mask) for a non-encoded multi-track output */
EXPORT void obs_output_set_mixers(obs_output_t *output, size_t mixers);

/** Gets the current audio mixes (mask) for a non-encoded multi-track output */
EXPORT size_t obs_output_get_mixers(const obs_output_t *output);

/**
 * Sets the current video encoder associated with this output,
 * required for encoded outputs
 */
EXPORT void obs_output_set_video_encoder(obs_output_t *output,
					 obs_encoder_t *encoder);

/**
 * Sets the current audio encoder associated with this output,
 * required for encoded outputs.
 *
 * The idx parameter specifies the audio encoder index to set the encoder to.
 * Only used with outputs that have multiple audio outputs (RTMP typically),
 * otherwise the parameter is ignored.
 */
EXPORT void obs_output_set_audio_encoder(obs_output_t *output,
					 obs_encoder_t *encoder, size_t idx);

/** Returns the current video encoder associated with this output */
EXPORT obs_encoder_t *obs_output_get_video_encoder(const obs_output_t *output);

/**
 * Returns the current audio encoder associated with this output
 *
 * The idx parameter specifies the audio encoder index.  Only used with
 * outputs that have multiple audio outputs, otherwise the parameter is
 * ignored.
 */
EXPORT obs_encoder_t *obs_output_get_audio_encoder(const obs_output_t *output,
						   size_t idx);

/** Sets the current service associated with this output. */
EXPORT void obs_output_set_service(obs_output_t *output,
				   obs_service_t *service);

/** Gets the current service associated with this output. */
EXPORT obs_service_t *obs_output_get_service(const obs_output_t *output);

/**
 * Sets the reconnect settings.  Set retry_count to 0 to disable reconnecting.
 */
EXPORT void obs_output_set_reconnect_settings(obs_output_t *output,
					      int retry_count, int retry_sec);

EXPORT uint64_t obs_output_get_total_bytes(const obs_output_t *output);
EXPORT int obs_output_get_frames_dropped(const obs_output_t *output);
EXPORT int obs_output_get_total_frames(const obs_output_t *output);

/**
 * Sets the preferred scaled resolution for this output.  Set width and height
 * to 0 to disable scaling.
 *
 * If this output uses an encoder, it will call obs_encoder_set_scaled_size on
 * the encoder before the stream is started.  If the encoder is already active,
 * then this function will trigger a warning and do nothing.
 */
EXPORT void obs_output_set_preferred_size(obs_output_t *output, uint32_t width,
					  uint32_t height);

/** For video outputs, returns the width of the encoded image */
EXPORT uint32_t obs_output_get_width(const obs_output_t *output);

/** For video outputs, returns the height of the encoded image */
EXPORT uint32_t obs_output_get_height(const obs_output_t *output);

EXPORT const char *obs_output_get_id(const obs_output_t *output);

EXPORT void obs_output_caption(obs_output_t *output,
			       const struct obs_source_cea_708 *captions);

EXPORT void obs_output_output_caption_text1(obs_output_t *output,
					    const char *text);
EXPORT void obs_output_output_caption_text2(obs_output_t *output,
					    const char *text,
					    double display_duration);

EXPORT float obs_output_get_congestion(obs_output_t *output);
EXPORT int obs_output_get_connect_time_ms(obs_output_t *output);

EXPORT bool obs_output_reconnecting(const obs_output_t *output);

/** Pass a string of the last output error, for UI use */
EXPORT void obs_output_set_last_error(obs_output_t *output,
				      const char *message);
EXPORT const char *obs_output_get_last_error(obs_output_t *output);

EXPORT const char *
obs_output_get_supported_video_codecs(const obs_output_t *output);
EXPORT const char *
obs_output_get_supported_audio_codecs(const obs_output_t *output);

/* ------------------------------------------------------------------------- */
/* Functions used by outputs */

EXPORT void *obs_output_get_type_data(obs_output_t *output);

/** Optionally sets the video conversion info.  Used only for raw output */
EXPORT void
obs_output_set_video_conversion(obs_output_t *output,
				const struct video_scale_info *conversion);

/** Optionally sets the audio conversion info.  Used only for raw output */
EXPORT void
obs_output_set_audio_conversion(obs_output_t *output,
				const struct audio_convert_info *conversion);

/** Returns whether data capture can begin with the specified flags */
EXPORT bool obs_output_can_begin_data_capture(const obs_output_t *output,
					      uint32_t flags);

/** Initializes encoders (if any) */
EXPORT bool obs_output_initialize_encoders(obs_output_t *output,
					   uint32_t flags);

/**
 * Begins data capture from media/encoders.
 *
 * @param  output  Output context
 * @param  flags   Set this to 0 to use default output flags set in the
 *                 obs_output_info structure, otherwise set to a either
 *                 OBS_OUTPUT_VIDEO or OBS_OUTPUT_AUDIO to specify whether to
 *                 connect audio or video.  This is useful for things like
 *                 ffmpeg which may or may not always want to use both audio
 *                 and video.
 * @return         true if successful, false otherwise.
 */
EXPORT bool obs_output_begin_data_capture(obs_output_t *output, uint32_t flags);

/** Ends data capture from media/encoders */
EXPORT void obs_output_end_data_capture(obs_output_t *output);

/**
 * Signals that the output has stopped itself.
 *
 * @param  output  Output context
 * @param  code    Error code (or OBS_OUTPUT_SUCCESS if not an error)
 */
EXPORT void obs_output_signal_stop(obs_output_t *output, int code);

EXPORT uint64_t obs_output_get_pause_offset(obs_output_t *output);

/* ------------------------------------------------------------------------- */
/* Encoders */

EXPORT const char *obs_encoder_get_display_name(const char *id);

/**
 * Creates a video encoder context
 *
 * @param  id        Video encoder ID
 * @param  name      Name to assign to this context
 * @param  settings  Settings
 * @return           The video encoder context, or NULL if failed or not found.
 */
EXPORT obs_encoder_t *obs_video_encoder_create(const char *id, const char *name,
					       obs_data_t *settings,
					       obs_data_t *hotkey_data);

/**
 * Creates an audio encoder context
 *
 * @param  id        Audio Encoder ID
 * @param  name      Name to assign to this context
 * @param  settings  Settings
 * @param  mixer_idx Index of the mixer to use for this audio encoder
 * @return           The video encoder context, or NULL if failed or not found.
 */
EXPORT obs_encoder_t *obs_audio_encoder_create(const char *id, const char *name,
					       obs_data_t *settings,
					       size_t mixer_idx,
					       obs_data_t *hotkey_data);

/**
 * Adds/releases a reference to an encoder.  When the last reference is
 * released, the encoder is destroyed.
 */
EXPORT void obs_encoder_addref(obs_encoder_t *encoder);
EXPORT void obs_encoder_release(obs_encoder_t *encoder);

EXPORT void obs_weak_encoder_addref(obs_weak_encoder_t *weak);
EXPORT void obs_weak_encoder_release(obs_weak_encoder_t *weak);

EXPORT obs_encoder_t *obs_encoder_get_ref(obs_encoder_t *encoder);
EXPORT obs_weak_encoder_t *obs_encoder_get_weak_encoder(obs_encoder_t *encoder);
EXPORT obs_encoder_t *obs_weak_encoder_get_encoder(obs_weak_encoder_t *weak);

EXPORT bool obs_weak_encoder_references_encoder(obs_weak_encoder_t *weak,
						obs_encoder_t *encoder);

EXPORT void obs_encoder_set_name(obs_encoder_t *encoder, const char *name);
EXPORT const char *obs_encoder_get_name(const obs_encoder_t *encoder);

/** Returns the codec of an encoder by the id */
EXPORT const char *obs_get_encoder_codec(const char *id);

/** Returns the type of an encoder by the id */
EXPORT enum obs_encoder_type obs_get_encoder_type(const char *id);

/** Returns the codec of the encoder */
EXPORT const char *obs_encoder_get_codec(const obs_encoder_t *encoder);

/** Returns the type of an encoder */
EXPORT enum obs_encoder_type obs_encoder_get_type(const obs_encoder_t *encoder);

/**
 * Sets the scaled resolution for a video encoder.  Set width and height to 0
 * to disable scaling.  If the encoder is active, this function will trigger
 * a warning, and do nothing.
 */
EXPORT void obs_encoder_set_scaled_size(obs_encoder_t *encoder, uint32_t width,
					uint32_t height);

/** For video encoders, returns true if pre-encode scaling is enabled */
EXPORT bool obs_encoder_scaling_enabled(const obs_encoder_t *encoder);

/** For video encoders, returns the width of the encoded image */
EXPORT uint32_t obs_encoder_get_width(const obs_encoder_t *encoder);

/** For video encoders, returns the height of the encoded image */
EXPORT uint32_t obs_encoder_get_height(const obs_encoder_t *encoder);

/** For audio encoders, returns the sample rate of the audio */
EXPORT uint32_t obs_encoder_get_sample_rate(const obs_encoder_t *encoder);

/**
 * Sets the preferred video format for a video encoder.  If the encoder can use
 * the format specified, it will force a conversion to that format if the
 * obs output format does not match the preferred format.
 *
 * If the format is set to VIDEO_FORMAT_NONE, will revert to the default
 * functionality of converting only when absolutely necessary.
 */
EXPORT void obs_encoder_set_preferred_video_format(obs_encoder_t *encoder,
						   enum video_format format);
EXPORT enum video_format
obs_encoder_get_preferred_video_format(const obs_encoder_t *encoder);

/** Gets the default settings for an encoder type */
EXPORT obs_data_t *obs_encoder_defaults(const char *id);
EXPORT obs_data_t *obs_encoder_get_defaults(const obs_encoder_t *encoder);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t *obs_get_encoder_properties(const char *id);

/**
 * Returns the property list of an existing encoder, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t *obs_encoder_properties(const obs_encoder_t *encoder);

/**
 * Updates the settings of the encoder context.  Usually used for changing
 * bitrate while active
 */
EXPORT void obs_encoder_update(obs_encoder_t *encoder, obs_data_t *settings);

/** Gets extra data (headers) associated with this context */
EXPORT bool obs_encoder_get_extra_data(const obs_encoder_t *encoder,
				       uint8_t **extra_data, size_t *size);

/** Returns the current settings for this encoder */
EXPORT obs_data_t *obs_encoder_get_settings(const obs_encoder_t *encoder);

/** Sets the video output context to be used with this encoder */
EXPORT void obs_encoder_set_video(obs_encoder_t *encoder, video_t *video);

/** Sets the audio output context to be used with this encoder */
EXPORT void obs_encoder_set_audio(obs_encoder_t *encoder, audio_t *audio);

/**
 * Returns the video output context used with this encoder, or NULL if not
 * a video context
 */
EXPORT video_t *obs_encoder_video(const obs_encoder_t *encoder);

/**
 * Returns the audio output context used with this encoder, or NULL if not
 * a audio context
 */
EXPORT audio_t *obs_encoder_audio(const obs_encoder_t *encoder);

/** Returns true if encoder is active, false otherwise */
EXPORT bool obs_encoder_active(const obs_encoder_t *encoder);

EXPORT void *obs_encoder_get_type_data(obs_encoder_t *encoder);

EXPORT const char *obs_encoder_get_id(const obs_encoder_t *encoder);

EXPORT uint32_t obs_get_encoder_caps(const char *encoder_id);
EXPORT uint32_t obs_encoder_get_caps(const obs_encoder_t *encoder);

#ifndef SWIG
/** Duplicates an encoder packet */
OBS_DEPRECATED
EXPORT void obs_duplicate_encoder_packet(struct encoder_packet *dst,
					 const struct encoder_packet *src);

OBS_DEPRECATED
EXPORT void obs_free_encoder_packet(struct encoder_packet *packet);
#endif

EXPORT void obs_encoder_packet_ref(struct encoder_packet *dst,
				   struct encoder_packet *src);
EXPORT void obs_encoder_packet_release(struct encoder_packet *packet);

EXPORT void *obs_encoder_create_rerouted(obs_encoder_t *encoder,
					 const char *reroute_id);

/** Returns whether encoder is paused */
EXPORT bool obs_encoder_paused(const obs_encoder_t *output);

EXPORT const char *obs_encoder_get_last_error(obs_encoder_t *encoder);
EXPORT void obs_encoder_set_last_error(obs_encoder_t *encoder,
				       const char *message);

/* ------------------------------------------------------------------------- */
/* Stream Services */

EXPORT const char *obs_service_get_display_name(const char *id);

EXPORT obs_service_t *obs_service_create(const char *id, const char *name,
					 obs_data_t *settings,
					 obs_data_t *hotkey_data);

EXPORT obs_service_t *obs_service_create_private(const char *id,
						 const char *name,
						 obs_data_t *settings);

/**
 * Adds/releases a reference to a service.  When the last reference is
 * released, the service is destroyed.
 */
EXPORT void obs_service_addref(obs_service_t *service);
EXPORT void obs_service_release(obs_service_t *service);

EXPORT void obs_weak_service_addref(obs_weak_service_t *weak);
EXPORT void obs_weak_service_release(obs_weak_service_t *weak);

EXPORT obs_service_t *obs_service_get_ref(obs_service_t *service);
EXPORT obs_weak_service_t *obs_service_get_weak_service(obs_service_t *service);
EXPORT obs_service_t *obs_weak_service_get_service(obs_weak_service_t *weak);

EXPORT bool obs_weak_service_references_service(obs_weak_service_t *weak,
						obs_service_t *service);

EXPORT const char *obs_service_get_name(const obs_service_t *service);

/** Gets the default settings for a service */
EXPORT obs_data_t *obs_service_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t *obs_get_service_properties(const char *id);

/**
 * Returns the property list of an existing service context, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t *obs_service_properties(const obs_service_t *service);

/** Gets the service type */
EXPORT const char *obs_service_get_type(const obs_service_t *service);

/** Updates the settings of the service context */
EXPORT void obs_service_update(obs_service_t *service, obs_data_t *settings);

/** Returns the current settings for this service */
EXPORT obs_data_t *obs_service_get_settings(const obs_service_t *service);

/** Returns the URL for this service context */
EXPORT const char *obs_service_get_url(const obs_service_t *service);

/** Returns the stream key (if any) for this service context */
EXPORT const char *obs_service_get_key(const obs_service_t *service);

/** Returns the username (if any) for this service context */
EXPORT const char *obs_service_get_username(const obs_service_t *service);

/** Returns the password (if any) for this service context */
EXPORT const char *obs_service_get_password(const obs_service_t *service);

/**
 * Applies service-specific video encoder settings.
 *
 * @param  video_encoder_settings  Video encoder settings.  Optional.
 * @param  audio_encoder_settings  Audio encoder settings.  Optional.
 */
EXPORT void
obs_service_apply_encoder_settings(obs_service_t *service,
				   obs_data_t *video_encoder_settings,
				   obs_data_t *audio_encoder_settings);

EXPORT void *obs_service_get_type_data(obs_service_t *service);

EXPORT const char *obs_service_get_id(const obs_service_t *service);

EXPORT void obs_service_get_supported_resolutions(
	const obs_service_t *service,
	struct obs_service_resolution **resolutions, size_t *count);
EXPORT void obs_service_get_max_fps(const obs_service_t *service, int *fps);

EXPORT void obs_service_get_max_bitrate(const obs_service_t *service,
					int *video_bitrate, int *audio_bitrate);

/* NOTE: This function is temporary and should be removed/replaced at a later
 * date. */
EXPORT const char *obs_service_get_output_type(const obs_service_t *service);

/* ------------------------------------------------------------------------- */
/* Source frame allocation functions */
EXPORT void obs_source_frame_init(struct obs_source_frame *frame,
				  enum video_format format, uint32_t width,
				  uint32_t height);

static inline void obs_source_frame_free(struct obs_source_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		memset(frame, 0, sizeof(*frame));
	}
}

static inline struct obs_source_frame *
obs_source_frame_create(enum video_format format, uint32_t width,
			uint32_t height)
{
	struct obs_source_frame *frame;

	frame = (struct obs_source_frame *)bzalloc(sizeof(*frame));
	obs_source_frame_init(frame, format, width, height);
	return frame;
}

static inline void obs_source_frame_destroy(struct obs_source_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		bfree(frame);
	}
}

EXPORT void obs_source_frame_copy(struct obs_source_frame *dst,
				  const struct obs_source_frame *src);

/* ------------------------------------------------------------------------- */
/* Get source icon type */
EXPORT enum obs_icon_type obs_source_get_icon_type(const char *id);

#ifdef __cplusplus
}
#endif
