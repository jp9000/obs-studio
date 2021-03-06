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

#include <time.h>
#include <stdlib.h>

#include "obs.h"
#include "obs-internal.h"
#include "graphics/vec4.h"
#include "media-io/format-conversion.h"
#include "media-io/video-frame.h"

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#endif

static uint64_t tick_sources(uint64_t cur_time, uint64_t last_time)
{
	struct obs_core_data *data = &obs->data;
	struct obs_source *source;
	uint64_t delta_time;
	float seconds;

	if (!last_time)
		last_time = cur_time -
			    video_output_get_frame_time(obs->video.video);

	delta_time = cur_time - last_time;
	seconds = (float)((double)delta_time / 1000000000.0);

	/* ------------------------------------- */
	/* call tick callbacks                   */

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);

	for (size_t i = obs->data.tick_callbacks.num; i > 0; i--) {
		struct tick_callback *callback;
		callback = obs->data.tick_callbacks.array + (i - 1);
		callback->tick(callback->param, seconds);
	}

	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);

	/* ------------------------------------- */
	/* call the tick function of each source */

	pthread_mutex_lock(&data->sources_mutex);

	source = data->first_source;
	while (source) {
		struct obs_source *cur_source = obs_source_get_ref(source);
		source = (struct obs_source *)source->context.next;

		if (cur_source) {
			obs_source_video_tick(cur_source, seconds);
			obs_source_release(cur_source);
		}
	}

	pthread_mutex_unlock(&data->sources_mutex);

	return cur_time;
}

/* in obs-display.c */
extern void render_display(struct obs_display *display);

static inline void render_displays(void)
{
	struct obs_display *display;

	if (!obs->data.valid)
		return;

	gs_enter_context(obs->video.graphics);

	/* render extra displays/swaps */
	pthread_mutex_lock(&obs->data.displays_mutex);

	display = obs->data.first_display;
	while (display) {
		render_display(display);
		display = display->next;
	}

	pthread_mutex_unlock(&obs->data.displays_mutex);

	gs_leave_context();
}

static inline void set_render_size(uint32_t width, uint32_t height)
{
	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
	gs_set_viewport(0, 0, width, height);
}

static inline void unmap_last_surface(struct obs_core_video *video)
{
	for (int c = 0; c < NUM_CHANNELS; ++c) {
		if (video->mapped_surfaces[c]) {
			gs_stagesurface_unmap(video->mapped_surfaces[c]);
			video->mapped_surfaces[c] = NULL;
		}
	}
}

static const char *render_main_texture_name = "render_main_texture";
static inline void render_main_texture(struct obs_core_video *video)
{
	profile_start(render_main_texture_name);
	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_MAIN_TEXTURE,
			      render_main_texture_name);

	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(video->render_texture, NULL);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	set_render_size(video->base_width, video->base_height);

	pthread_mutex_lock(&obs->data.draw_callbacks_mutex);

	for (size_t i = obs->data.draw_callbacks.num; i > 0; i--) {
		struct draw_callback *callback;
		callback = obs->data.draw_callbacks.array + (i - 1);

		callback->draw(callback->param, video->base_width,
			       video->base_height);
	}

	pthread_mutex_unlock(&obs->data.draw_callbacks_mutex);

	obs_view_render(&obs->data.main_view);

	video->texture_rendered = true;

	GS_DEBUG_MARKER_END();
	profile_end(render_main_texture_name);
}

static inline gs_effect_t *
get_scale_effect_internal(struct obs_core_video *video)
{
	/* if the dimension is under half the size of the original image,
	 * bicubic/lanczos can't sample enough pixels to create an accurate
	 * image, so use the bilinear low resolution effect instead */
	if (video->output_width < (video->base_width / 2) &&
	    video->output_height < (video->base_height / 2)) {
		return video->bilinear_lowres_effect;
	}

	switch (video->scale_type) {
	case OBS_SCALE_BILINEAR:
		return video->default_effect;
	case OBS_SCALE_LANCZOS:
		return video->lanczos_effect;
	case OBS_SCALE_AREA:
		return video->area_effect;
	case OBS_SCALE_BICUBIC:
	default:;
	}

	return video->bicubic_effect;
}

static inline bool resolution_close(struct obs_core_video *video,
				    uint32_t width, uint32_t height)
{
	long width_cmp = (long)video->base_width - (long)width;
	long height_cmp = (long)video->base_height - (long)height;

	return labs(width_cmp) <= 16 && labs(height_cmp) <= 16;
}

static inline gs_effect_t *get_scale_effect(struct obs_core_video *video,
					    uint32_t width, uint32_t height)
{
	if (resolution_close(video, width, height)) {
		return video->default_effect;
	} else {
		/* if the scale method couldn't be loaded, use either bicubic
		 * or bilinear by default */
		gs_effect_t *effect = get_scale_effect_internal(video);
		if (!effect)
			effect = !!video->bicubic_effect
					 ? video->bicubic_effect
					 : video->default_effect;
		return effect;
	}
}

static const char *render_output_texture_name = "render_output_texture";
static inline gs_texture_t *render_output_texture(struct obs_core_video *video)
{
	gs_texture_t *texture = video->render_texture;
	gs_texture_t *target = video->output_texture;
	uint32_t width = gs_texture_get_width(target);
	uint32_t height = gs_texture_get_height(target);

	gs_effect_t *effect = get_scale_effect(video, width, height);
	gs_technique_t *tech;

	if (video->ovi.output_format == VIDEO_FORMAT_RGBA) {
		tech = gs_effect_get_technique(effect, "DrawAlphaDivide");
	} else {
		if ((effect == video->default_effect) &&
		    (width == video->base_width) &&
		    (height == video->base_height))
			return texture;

		tech = gs_effect_get_technique(effect, "Draw");
	}

	profile_start(render_output_texture_name);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_eparam_t *bres =
		gs_effect_get_param_by_name(effect, "base_dimension");
	gs_eparam_t *bres_i =
		gs_effect_get_param_by_name(effect, "base_dimension_i");
	size_t passes, i;

	gs_set_render_target(target, NULL);
	set_render_size(width, height);

	if (bres) {
		struct vec2 base;
		vec2_set(&base, (float)video->base_width,
			 (float)video->base_height);
		gs_effect_set_vec2(bres, &base);
	}

	if (bres_i) {
		struct vec2 base_i;
		vec2_set(&base_i, 1.0f / (float)video->base_width,
			 1.0f / (float)video->base_height);
		gs_effect_set_vec2(bres_i, &base_i);
	}

	gs_effect_set_texture_srgb(image, texture);

	gs_enable_framebuffer_srgb(true);
	gs_enable_blending(false);
	passes = gs_technique_begin(tech);
	for (i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		gs_draw_sprite(texture, 0, width, height);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
	gs_enable_blending(true);
	gs_enable_framebuffer_srgb(false);

	profile_end(render_output_texture_name);

	return target;
}

static void render_convert_plane(gs_effect_t *effect, gs_texture_t *target,
				 const char *tech_name)
{
	gs_technique_t *tech = gs_effect_get_technique(effect, tech_name);

	const uint32_t width = gs_texture_get_width(target);
	const uint32_t height = gs_texture_get_height(target);

	gs_set_render_target(target, NULL);
	set_render_size(width, height);

	size_t passes = gs_technique_begin(tech);
	for (size_t i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		gs_draw(GS_TRIS, 0, 3);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
}

static const char *render_convert_texture_name = "render_convert_texture";
static void render_convert_texture(struct obs_core_video *video,
				   gs_texture_t *texture)
{
	profile_start(render_convert_texture_name);

	gs_effect_t *effect = video->conversion_effect;
	gs_eparam_t *color_vec0 =
		gs_effect_get_param_by_name(effect, "color_vec0");
	gs_eparam_t *color_vec1 =
		gs_effect_get_param_by_name(effect, "color_vec1");
	gs_eparam_t *color_vec2 =
		gs_effect_get_param_by_name(effect, "color_vec2");
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_eparam_t *width_i = gs_effect_get_param_by_name(effect, "width_i");

	struct vec4 vec0, vec1, vec2;
	vec4_set(&vec0, video->color_matrix[4], video->color_matrix[5],
		 video->color_matrix[6], video->color_matrix[7]);
	vec4_set(&vec1, video->color_matrix[0], video->color_matrix[1],
		 video->color_matrix[2], video->color_matrix[3]);
	vec4_set(&vec2, video->color_matrix[8], video->color_matrix[9],
		 video->color_matrix[10], video->color_matrix[11]);

	gs_enable_blending(false);

	if (video->convert_textures[0]) {
		gs_effect_set_texture(image, texture);
		gs_effect_set_vec4(color_vec0, &vec0);
		render_convert_plane(effect, video->convert_textures[0],
				     video->conversion_techs[0]);

		if (video->convert_textures[1]) {
			gs_effect_set_texture(image, texture);
			gs_effect_set_vec4(color_vec1, &vec1);
			if (!video->convert_textures[2])
				gs_effect_set_vec4(color_vec2, &vec2);
			gs_effect_set_float(width_i, video->conversion_width_i);
			render_convert_plane(effect, video->convert_textures[1],
					     video->conversion_techs[1]);

			if (video->convert_textures[2]) {
				gs_effect_set_texture(image, texture);
				gs_effect_set_vec4(color_vec2, &vec2);
				gs_effect_set_float(width_i,
						    video->conversion_width_i);
				render_convert_plane(
					effect, video->convert_textures[2],
					video->conversion_techs[2]);
			}
		}
	}

	gs_enable_blending(true);

	video->texture_converted = true;

	profile_end(render_convert_texture_name);
}

static const char *stage_output_texture_name = "stage_output_texture";
static inline void stage_output_texture(struct obs_core_video *video,
					int cur_texture)
{
	profile_start(stage_output_texture_name);

	unmap_last_surface(video);

	if (!video->gpu_conversion) {
		gs_stagesurf_t *copy = video->copy_surfaces[cur_texture][0];
		if (copy)
			gs_stage_texture(copy, video->output_texture);

		video->textures_copied[cur_texture] = true;
	} else if (video->texture_converted) {
		for (int i = 0; i < NUM_CHANNELS; i++) {
			gs_stagesurf_t *copy =
				video->copy_surfaces[cur_texture][i];
			if (copy)
				gs_stage_texture(copy,
						 video->convert_textures[i]);
		}

		video->textures_copied[cur_texture] = true;
	}

	profile_end(stage_output_texture_name);
}

#ifdef _WIN32
static inline bool queue_frame(struct obs_core_video *video, bool raw_active,
			       struct obs_vframe_info *vframe_info)
{
	bool duplicate =
		!video->gpu_encoder_avail_queue.size ||
		(video->gpu_encoder_queue.size && vframe_info->count > 1);

	if (duplicate) {
		struct obs_tex_frame *tf = circlebuf_data(
			&video->gpu_encoder_queue,
			video->gpu_encoder_queue.size - sizeof(*tf));

		/* texture-based encoding is stopping */
		if (!tf) {
			return false;
		}

		tf->count++;
		os_sem_post(video->gpu_encode_semaphore);
		goto finish;
	}

	struct obs_tex_frame tf;
	circlebuf_pop_front(&video->gpu_encoder_avail_queue, &tf, sizeof(tf));

	if (tf.released) {
		gs_texture_acquire_sync(tf.tex, tf.lock_key, GS_WAIT_INFINITE);
		tf.released = false;
	}

	/* the vframe_info->count > 1 case causing a copy can only happen if by
	 * some chance the very first frame has to be duplicated for whatever
	 * reason.  otherwise, it goes to the 'duplicate' case above, which
	 * will ensure better performance. */
	if (raw_active || vframe_info->count > 1) {
		gs_copy_texture(tf.tex, video->convert_textures[0]);
	} else {
		gs_texture_t *tex = video->convert_textures[0];
		gs_texture_t *tex_uv = video->convert_textures[1];

		video->convert_textures[0] = tf.tex;
		video->convert_textures[1] = tf.tex_uv;

		tf.tex = tex;
		tf.tex_uv = tex_uv;
	}

	tf.count = 1;
	tf.timestamp = vframe_info->timestamp;
	tf.released = true;
	tf.handle = gs_texture_get_shared_handle(tf.tex);
	gs_texture_release_sync(tf.tex, ++tf.lock_key);
	circlebuf_push_back(&video->gpu_encoder_queue, &tf, sizeof(tf));

	os_sem_post(video->gpu_encode_semaphore);

finish:
	return --vframe_info->count;
}

extern void full_stop(struct obs_encoder *encoder);

static inline void encode_gpu(struct obs_core_video *video, bool raw_active,
			      struct obs_vframe_info *vframe_info)
{
	while (queue_frame(video, raw_active, vframe_info))
		;
}

static const char *output_gpu_encoders_name = "output_gpu_encoders";
static void output_gpu_encoders(struct obs_core_video *video, bool raw_active)
{
	profile_start(output_gpu_encoders_name);

	if (!video->texture_converted)
		goto end;
	if (!video->vframe_info_buffer_gpu.size)
		goto end;

	struct obs_vframe_info vframe_info;
	circlebuf_pop_front(&video->vframe_info_buffer_gpu, &vframe_info,
			    sizeof(vframe_info));

	pthread_mutex_lock(&video->gpu_encoder_mutex);
	encode_gpu(video, raw_active, &vframe_info);
	pthread_mutex_unlock(&video->gpu_encoder_mutex);

end:
	profile_end(output_gpu_encoders_name);
}
#endif

static inline void render_video(struct obs_core_video *video, bool raw_active,
				const bool gpu_active, int cur_texture)
{
	gs_begin_scene();

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);

	render_main_texture(video);

	if (raw_active || gpu_active) {
		gs_texture_t *texture = render_output_texture(video);

#ifdef _WIN32
		if (gpu_active)
			gs_flush();
#endif

		if (video->gpu_conversion)
			render_convert_texture(video, texture);

#ifdef _WIN32
		if (gpu_active) {
			gs_flush();
			output_gpu_encoders(video, raw_active);
		}
#endif

		if (raw_active)
			stage_output_texture(video, cur_texture);
	}

	gs_set_render_target(NULL, NULL);
	gs_enable_blending(true);

	gs_end_scene();
}

static inline bool download_frame(struct obs_core_video *video,
				  int prev_texture, struct video_data *frame)
{
	if (!video->textures_copied[prev_texture])
		return false;

	for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
		gs_stagesurf_t *surface =
			video->copy_surfaces[prev_texture][channel];
		if (surface) {
			if (!gs_stagesurface_map(surface, &frame->data[channel],
						 &frame->linesize[channel]))
				return false;

			video->mapped_surfaces[channel] = surface;
		}
	}
	return true;
}

static const uint8_t *set_gpu_converted_plane(uint32_t width, uint32_t height,
					      uint32_t linesize_input,
					      uint32_t linesize_output,
					      const uint8_t *in, uint8_t *out)
{
	if ((width == linesize_input) && (width == linesize_output)) {
		size_t total = (size_t)width * (size_t)height;
		memcpy(out, in, total);
		in += total;
	} else {
		for (size_t y = 0; y < height; y++) {
			memcpy(out, in, width);
			out += linesize_output;
			in += linesize_input;
		}
	}

	return in;
}

static void set_gpu_converted_data(struct obs_core_video *video,
				   struct video_frame *output,
				   const struct video_data *input,
				   const struct video_output_info *info)
{
	if (video->using_nv12_tex) {
		const uint32_t width = info->width;
		const uint32_t height = info->height;

		const uint8_t *const in_uv = set_gpu_converted_plane(
			width, height, input->linesize[0], output->linesize[0],
			input->data[0], output->data[0]);

		const uint32_t height_d2 = height / 2;
		set_gpu_converted_plane(width, height_d2, input->linesize[0],
					output->linesize[1], in_uv,
					output->data[1]);
	} else {
		switch (info->format) {
		case VIDEO_FORMAT_I420: {
			const uint32_t width = info->width;
			const uint32_t height = info->height;

			set_gpu_converted_plane(width, height,
						input->linesize[0],
						output->linesize[0],
						input->data[0],
						output->data[0]);

			const uint32_t width_d2 = width / 2;
			const uint32_t height_d2 = height / 2;

			set_gpu_converted_plane(width_d2, height_d2,
						input->linesize[1],
						output->linesize[1],
						input->data[1],
						output->data[1]);

			set_gpu_converted_plane(width_d2, height_d2,
						input->linesize[2],
						output->linesize[2],
						input->data[2],
						output->data[2]);

			break;
		}
		case VIDEO_FORMAT_NV12: {
			const uint32_t width = info->width;
			const uint32_t height = info->height;

			set_gpu_converted_plane(width, height,
						input->linesize[0],
						output->linesize[0],
						input->data[0],
						output->data[0]);

			const uint32_t height_d2 = height / 2;
			set_gpu_converted_plane(width, height_d2,
						input->linesize[1],
						output->linesize[1],
						input->data[1],
						output->data[1]);

			break;
		}
		case VIDEO_FORMAT_I444: {
			const uint32_t width = info->width;
			const uint32_t height = info->height;

			set_gpu_converted_plane(width, height,
						input->linesize[0],
						output->linesize[0],
						input->data[0],
						output->data[0]);

			set_gpu_converted_plane(width, height,
						input->linesize[1],
						output->linesize[1],
						input->data[1],
						output->data[1]);

			set_gpu_converted_plane(width, height,
						input->linesize[2],
						output->linesize[2],
						input->data[2],
						output->data[2]);

			break;
		}

		case VIDEO_FORMAT_NONE:
		case VIDEO_FORMAT_YVYU:
		case VIDEO_FORMAT_YUY2:
		case VIDEO_FORMAT_UYVY:
		case VIDEO_FORMAT_RGBA:
		case VIDEO_FORMAT_BGRA:
		case VIDEO_FORMAT_BGRX:
		case VIDEO_FORMAT_Y800:
		case VIDEO_FORMAT_BGR3:
		case VIDEO_FORMAT_I422:
		case VIDEO_FORMAT_I40A:
		case VIDEO_FORMAT_I42A:
		case VIDEO_FORMAT_YUVA:
		case VIDEO_FORMAT_AYUV:
			/* unimplemented */
			;
		}
	}
}

static inline void copy_rgbx_frame(struct video_frame *output,
				   const struct video_data *input,
				   const struct video_output_info *info)
{
	uint8_t *in_ptr = input->data[0];
	uint8_t *out_ptr = output->data[0];

	/* if the line sizes match, do a single copy */
	if (input->linesize[0] == output->linesize[0]) {
		memcpy(out_ptr, in_ptr,
		       (size_t)input->linesize[0] * (size_t)info->height);
	} else {
		const size_t copy_size = (size_t)info->width * 4;
		for (size_t y = 0; y < info->height; y++) {
			memcpy(out_ptr, in_ptr, copy_size);
			in_ptr += input->linesize[0];
			out_ptr += output->linesize[0];
		}
	}
}

static inline void output_video_data(struct obs_core_video *video,
				     struct video_data *input_frame, int count)
{
	const struct video_output_info *info;
	struct video_frame output_frame;
	bool locked;

	info = video_output_get_info(video->video);

	locked = video_output_lock_frame(video->video, &output_frame, count,
					 input_frame->timestamp);
	if (locked) {
		if (video->gpu_conversion) {
			set_gpu_converted_data(video, &output_frame,
					       input_frame, info);
		} else {
			copy_rgbx_frame(&output_frame, input_frame, info);
		}

		video_output_unlock_frame(video->video);
	}
}

static inline void video_sleep(struct obs_core_video *video, bool raw_active,
			       const bool gpu_active, uint64_t *p_time,
			       uint64_t interval_ns)
{
	struct obs_vframe_info vframe_info;
	uint64_t cur_time = *p_time;
	uint64_t t = cur_time + interval_ns;
	int count;

	if (os_sleepto_ns(t)) {
		*p_time = t;
		count = 1;
	} else {
		count = (int)((os_gettime_ns() - cur_time) / interval_ns);
		*p_time = cur_time + interval_ns * count;
	}

	video->total_frames += count;
	video->lagged_frames += count - 1;

	vframe_info.timestamp = cur_time;
	vframe_info.count = count;

	if (raw_active)
		circlebuf_push_back(&video->vframe_info_buffer, &vframe_info,
				    sizeof(vframe_info));
	if (gpu_active)
		circlebuf_push_back(&video->vframe_info_buffer_gpu,
				    &vframe_info, sizeof(vframe_info));
}

static const char *output_frame_gs_context_name = "gs_context(video->graphics)";
static const char *output_frame_render_video_name = "render_video";
static const char *output_frame_download_frame_name = "download_frame";
static const char *output_frame_gs_flush_name = "gs_flush";
static const char *output_frame_output_video_data_name = "output_video_data";
static inline void output_frame(bool raw_active, const bool gpu_active)
{
	struct obs_core_video *video = &obs->video;
	int cur_texture = video->cur_texture;
	int prev_texture = cur_texture == 0 ? NUM_TEXTURES - 1
					    : cur_texture - 1;
	struct video_data frame;
	bool frame_ready = 0;

	memset(&frame, 0, sizeof(struct video_data));

	profile_start(output_frame_gs_context_name);
	gs_enter_context(video->graphics);

	profile_start(output_frame_render_video_name);
	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_RENDER_VIDEO,
			      output_frame_render_video_name);
	render_video(video, raw_active, gpu_active, cur_texture);
	GS_DEBUG_MARKER_END();
	profile_end(output_frame_render_video_name);

	if (raw_active) {
		profile_start(output_frame_download_frame_name);
		frame_ready = download_frame(video, prev_texture, &frame);
		profile_end(output_frame_download_frame_name);
	}

	profile_start(output_frame_gs_flush_name);
	gs_flush();
	profile_end(output_frame_gs_flush_name);

	gs_leave_context();
	profile_end(output_frame_gs_context_name);

	if (raw_active && frame_ready) {
		struct obs_vframe_info vframe_info;
		circlebuf_pop_front(&video->vframe_info_buffer, &vframe_info,
				    sizeof(vframe_info));

		frame.timestamp = vframe_info.timestamp;
		profile_start(output_frame_output_video_data_name);
		output_video_data(video, &frame, vframe_info.count);
		profile_end(output_frame_output_video_data_name);
	}

	if (++video->cur_texture == NUM_TEXTURES)
		video->cur_texture = 0;
}

#define NBSP "\xC2\xA0"

static void clear_base_frame_data(void)
{
	struct obs_core_video *video = &obs->video;
	video->texture_rendered = false;
	video->texture_converted = false;
	circlebuf_free(&video->vframe_info_buffer);
	video->cur_texture = 0;
}

static void clear_raw_frame_data(void)
{
	struct obs_core_video *video = &obs->video;
	memset(video->textures_copied, 0, sizeof(video->textures_copied));
	circlebuf_free(&video->vframe_info_buffer);
}

#ifdef _WIN32
static void clear_gpu_frame_data(void)
{
	struct obs_core_video *video = &obs->video;
	circlebuf_free(&video->vframe_info_buffer_gpu);
}
#endif

extern THREAD_LOCAL bool is_graphics_thread;

static void execute_graphics_tasks(void)
{
	struct obs_core_video *video = &obs->video;
	bool tasks_remaining = true;

	while (tasks_remaining) {
		pthread_mutex_lock(&video->task_mutex);
		if (video->tasks.size) {
			struct obs_task_info info;
			circlebuf_pop_front(&video->tasks, &info, sizeof(info));
			info.task(info.param);
		}
		tasks_remaining = !!video->tasks.size;
		pthread_mutex_unlock(&video->task_mutex);
	}
}

#ifdef _WIN32

struct winrt_exports {
	void (*winrt_initialize)();
	void (*winrt_uninitialize)();
	struct winrt_disaptcher *(*winrt_dispatcher_init)();
	void (*winrt_dispatcher_free)(struct winrt_disaptcher *dispatcher);
	void (*winrt_capture_thread_start)();
	void (*winrt_capture_thread_stop)();
};

#define WINRT_IMPORT(func)                                        \
	do {                                                      \
		exports->func = os_dlsym(module, #func);          \
		if (!exports->func) {                             \
			success = false;                          \
			blog(LOG_ERROR,                           \
			     "Could not load function '%s' from " \
			     "module '%s'",                       \
			     #func, module_name);                 \
		}                                                 \
	} while (false)

static bool load_winrt_imports(struct winrt_exports *exports, void *module,
			       const char *module_name)
{
	bool success = true;

	WINRT_IMPORT(winrt_initialize);
	WINRT_IMPORT(winrt_uninitialize);
	WINRT_IMPORT(winrt_dispatcher_init);
	WINRT_IMPORT(winrt_dispatcher_free);
	WINRT_IMPORT(winrt_capture_thread_start);
	WINRT_IMPORT(winrt_capture_thread_stop);

	return success;
}

struct winrt_state {
	bool loaded;
	void *winrt_module;
	struct winrt_exports exports;
	struct winrt_disaptcher *dispatcher;
};

static void init_winrt_state(struct winrt_state *winrt)
{
	static const char *const module_name = "libobs-winrt";

	winrt->winrt_module = os_dlopen(module_name);
	winrt->loaded = winrt->winrt_module &&
			load_winrt_imports(&winrt->exports, winrt->winrt_module,
					   module_name);
	winrt->dispatcher = NULL;
	if (winrt->loaded) {
		winrt->exports.winrt_initialize();
		winrt->dispatcher = winrt->exports.winrt_dispatcher_init();

		gs_enter_context(obs->video.graphics);
		winrt->exports.winrt_capture_thread_start();
		gs_leave_context();
	}
}

static void uninit_winrt_state(struct winrt_state *winrt)
{
	if (winrt->winrt_module) {
		if (winrt->loaded) {
			winrt->exports.winrt_capture_thread_stop();
			if (winrt->dispatcher)
				winrt->exports.winrt_dispatcher_free(
					winrt->dispatcher);
			winrt->exports.winrt_uninitialize();
		}

		os_dlclose(winrt->winrt_module);
	}
}

#endif // #ifdef _WIN32

static const char *tick_sources_name = "tick_sources";
static const char *render_displays_name = "render_displays";
static const char *output_frame_name = "output_frame";
bool obs_graphics_thread_loop(struct obs_graphics_context *context)
{
	/* defer loop break to clean up sources */
	const bool stop_requested = video_output_stopped(obs->video.video);

	uint64_t frame_start = os_gettime_ns();
	uint64_t frame_time_ns;
	bool raw_active = obs->video.raw_active > 0;
#ifdef _WIN32
	const bool gpu_active = obs->video.gpu_encoder_active > 0;
	const bool active = raw_active || gpu_active;
#else
	const bool gpu_active = 0;
	const bool active = raw_active;
#endif

	if (!context->was_active && active)
		clear_base_frame_data();
	if (!context->raw_was_active && raw_active)
		clear_raw_frame_data();
#ifdef _WIN32
	if (!context->gpu_was_active && gpu_active)
		clear_gpu_frame_data();

	context->gpu_was_active = gpu_active;
#endif
	context->raw_was_active = raw_active;
	context->was_active = active;

	profile_start(context->video_thread_name);

	gs_enter_context(obs->video.graphics);
	gs_begin_frame();
	gs_leave_context();

	profile_start(tick_sources_name);
	context->last_time =
		tick_sources(obs->video.video_time, context->last_time);
	profile_end(tick_sources_name);

	execute_graphics_tasks();

#ifdef _WIN32
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif

	profile_start(output_frame_name);
	output_frame(raw_active, gpu_active);
	profile_end(output_frame_name);

	profile_start(render_displays_name);
	render_displays();
	profile_end(render_displays_name);

	frame_time_ns = os_gettime_ns() - frame_start;

	profile_end(context->video_thread_name);

	profile_reenable_thread();

	video_sleep(&obs->video, raw_active, gpu_active, &obs->video.video_time,
		    context->interval);

	context->frame_time_total_ns += frame_time_ns;
	context->fps_total_ns += (obs->video.video_time - context->last_time);
	context->fps_total_frames++;

	if (context->fps_total_ns >= 1000000000ULL) {
		obs->video.video_fps =
			(double)context->fps_total_frames /
			((double)context->fps_total_ns / 1000000000.0);
		obs->video.video_avg_frame_time_ns =
			context->frame_time_total_ns /
			(uint64_t)context->fps_total_frames;

		context->frame_time_total_ns = 0;
		context->fps_total_ns = 0;
		context->fps_total_frames = 0;
	}

	return !stop_requested;
}

void *obs_graphics_thread(void *param)
{
#ifdef _WIN32
	struct winrt_state winrt;
	init_winrt_state(&winrt);
#endif // #ifdef _WIN32

	is_graphics_thread = true;

	const uint64_t interval = video_output_get_frame_time(obs->video.video);

	obs->video.video_time = os_gettime_ns();
	obs->video.video_frame_interval_ns = interval;

	os_set_thread_name("libobs: graphics thread");

	const char *video_thread_name = profile_store_name(
		obs_get_profiler_name_store(),
		"obs_graphics_thread(%g" NBSP "ms)", interval / 1000000.);
	profile_register_root(video_thread_name, interval);

	srand((unsigned int)time(NULL));

	struct obs_graphics_context context;
	context.interval = video_output_get_frame_time(obs->video.video);
	context.frame_time_total_ns = 0;
	context.fps_total_ns = 0;
	context.fps_total_frames = 0;
	context.last_time = 0;
#ifdef _WIN32
	context.gpu_was_active = false;
#endif
	context.raw_was_active = false;
	context.was_active = false;
	context.video_thread_name = video_thread_name;

#ifdef __APPLE__
	while (obs_graphics_thread_loop_autorelease(&context))
#else
	while (obs_graphics_thread_loop(&context))
#endif
		;

#ifdef _WIN32
	uninit_winrt_state(&winrt);
#endif

	UNUSED_PARAMETER(param);
	return NULL;
}
