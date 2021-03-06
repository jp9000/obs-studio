/******************************************************************************
    Copyright (C) 2013-2015 by Hugh Bailey <obs.jim@gmail.com>
                               Philippe Groarke <philippe.groarke@gmail.com>

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

#include "util/threading.h"
#include "util/util_uint64.h"
#include "graphics/math-defs.h"
#include "obs-scene.h"
#include "obs-internal.h"

const struct obs_source_info group_info;

static void resize_group(obs_sceneitem_t *group);
static void resize_scene(obs_scene_t *scene);
static void signal_parent(obs_scene_t *parent, const char *name,
			  calldata_t *params);
static void get_ungrouped_transform(obs_sceneitem_t *group, struct vec2 *pos,
				    struct vec2 *scale, float *rot);
static inline bool crop_enabled(const struct obs_sceneitem_crop *crop);
static inline bool item_texture_enabled(const struct obs_scene_item *item);
static void init_hotkeys(obs_scene_t *scene, obs_sceneitem_t *item,
			 const char *name);

/* NOTE: For proper mutex lock order (preventing mutual cross-locks), never
 * lock the graphics mutex inside either of the scene mutexes.
 *
 * Another thing that must be done to prevent that cross-lock (and improve
 * performance), is to not create/release/update sources within the scene
 * mutexes.
 *
 * It's okay to lock the graphics mutex before locking either of the scene
 * mutexes, but not after.
 */

static const char *obs_scene_signals[] = {
	"void item_add(ptr scene, ptr item)",
	"void item_remove(ptr scene, ptr item)",
	"void reorder(ptr scene)",
	"void refresh(ptr scene)",
	"void item_visible(ptr scene, ptr item, bool visible)",
	"void item_select(ptr scene, ptr item)",
	"void item_deselect(ptr scene, ptr item)",
	"void item_transform(ptr scene, ptr item)",
	"void item_locked(ptr scene, ptr item, bool locked)",
	NULL,
};

static inline void signal_item_remove(struct obs_scene_item *item)
{
	struct calldata params;
	uint8_t stack[128];

	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_ptr(&params, "item", item);

	signal_parent(item->parent, "item_remove", &params);
}

static const char *scene_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Scene";
}

static const char *group_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Group";
}

static void *scene_create(obs_data_t *settings, struct obs_source *source)
{
	pthread_mutexattr_t attr;
	struct obs_scene *scene = bzalloc(sizeof(struct obs_scene));
	scene->source = source;

	if (strcmp(source->info.id, group_info.id) == 0) {
		scene->is_group = true;
		scene->custom_size = true;
		scene->cx = 0;
		scene->cy = 0;
	}

	signal_handler_add_array(obs_source_get_signal_handler(source),
				 obs_scene_signals);

	if (pthread_mutexattr_init(&attr) != 0)
		goto fail;
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		goto fail;
	if (pthread_mutex_init(&scene->audio_mutex, &attr) != 0) {
		blog(LOG_ERROR, "scene_create: Couldn't initialize audio "
				"mutex");
		goto fail;
	}
	if (pthread_mutex_init(&scene->video_mutex, &attr) != 0) {
		blog(LOG_ERROR, "scene_create: Couldn't initialize video "
				"mutex");
		goto fail;
	}

	UNUSED_PARAMETER(settings);
	return scene;

fail:
	pthread_mutexattr_destroy(&attr);
	bfree(scene);
	return NULL;
}

#define audio_lock(scene) pthread_mutex_lock(&scene->audio_mutex)
#define video_lock(scene) pthread_mutex_lock(&scene->video_mutex)
#define audio_unlock(scene) pthread_mutex_unlock(&scene->audio_mutex)
#define video_unlock(scene) pthread_mutex_unlock(&scene->video_mutex)

static inline void full_lock(struct obs_scene *scene)
{
	video_lock(scene);
	audio_lock(scene);
}

static inline void full_unlock(struct obs_scene *scene)
{
	audio_unlock(scene);
	video_unlock(scene);
}

static void set_visibility(struct obs_scene_item *item, bool vis);
static inline void detach_sceneitem(struct obs_scene_item *item);

static inline void remove_without_release(struct obs_scene_item *item)
{
	item->removed = true;
	set_visibility(item, false);
	signal_item_remove(item);
	detach_sceneitem(item);
}

static void remove_all_items(struct obs_scene *scene)
{
	struct obs_scene_item *item;
	DARRAY(struct obs_scene_item *) items;

	da_init(items);

	full_lock(scene);

	item = scene->first_item;

	while (item) {
		struct obs_scene_item *del_item = item;
		item = item->next;

		remove_without_release(del_item);
		da_push_back(items, &del_item);
	}

	full_unlock(scene);

	for (size_t i = 0; i < items.num; i++)
		obs_sceneitem_release(items.array[i]);
	da_free(items);
}

static void scene_destroy(void *data)
{
	struct obs_scene *scene = data;

	remove_all_items(scene);

	pthread_mutex_destroy(&scene->video_mutex);
	pthread_mutex_destroy(&scene->audio_mutex);
	bfree(scene);
}

static void scene_enum_sources(void *data, obs_source_enum_proc_t enum_callback,
			       void *param, bool active)
{
	struct obs_scene *scene = data;
	struct obs_scene_item *item;
	struct obs_scene_item *next;

	full_lock(scene);
	item = scene->first_item;

	while (item) {
		next = item->next;

		obs_sceneitem_addref(item);
		if (!active || os_atomic_load_long(&item->active_refs) > 0)
			enum_callback(scene->source, item->source, param);
		obs_sceneitem_release(item);

		item = next;
	}

	full_unlock(scene);
}

static void scene_enum_active_sources(void *data,
				      obs_source_enum_proc_t enum_callback,
				      void *param)
{
	scene_enum_sources(data, enum_callback, param, true);
}

static void scene_enum_all_sources(void *data,
				   obs_source_enum_proc_t enum_callback,
				   void *param)
{
	scene_enum_sources(data, enum_callback, param, false);
}

static inline void detach_sceneitem(struct obs_scene_item *item)
{
	if (item->prev)
		item->prev->next = item->next;
	else
		item->parent->first_item = item->next;

	if (item->next)
		item->next->prev = item->prev;

	item->parent = NULL;
}

static inline void attach_sceneitem(struct obs_scene *parent,
				    struct obs_scene_item *item,
				    struct obs_scene_item *prev)
{
	item->prev = prev;
	item->parent = parent;

	if (prev) {
		item->next = prev->next;
		if (prev->next)
			prev->next->prev = item;
		prev->next = item;
	} else {
		item->next = parent->first_item;
		if (parent->first_item)
			parent->first_item->prev = item;
		parent->first_item = item;
	}
}

void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy)
{
	if (align & OBS_ALIGN_RIGHT)
		v->x += (float)cx;
	else if ((align & OBS_ALIGN_LEFT) == 0)
		v->x += (float)(cx / 2);

	if (align & OBS_ALIGN_BOTTOM)
		v->y += (float)cy;
	else if ((align & OBS_ALIGN_TOP) == 0)
		v->y += (float)(cy / 2);
}

static void calculate_bounds_data(struct obs_scene_item *item,
				  struct vec2 *origin, struct vec2 *scale,
				  uint32_t *cx, uint32_t *cy)
{
	float width = (float)(*cx) * fabsf(scale->x);
	float height = (float)(*cy) * fabsf(scale->y);
	float item_aspect = width / height;
	float bounds_aspect = item->bounds.x / item->bounds.y;
	uint32_t bounds_type = item->bounds_type;
	float width_diff, height_diff;

	if (item->bounds_type == OBS_BOUNDS_MAX_ONLY)
		if (width > item->bounds.x || height > item->bounds.y)
			bounds_type = OBS_BOUNDS_SCALE_INNER;

	if (bounds_type == OBS_BOUNDS_SCALE_INNER ||
	    bounds_type == OBS_BOUNDS_SCALE_OUTER) {
		bool use_width = (bounds_aspect < item_aspect);
		float mul;

		if (item->bounds_type == OBS_BOUNDS_SCALE_OUTER)
			use_width = !use_width;

		mul = use_width ? item->bounds.x / width
				: item->bounds.y / height;

		vec2_mulf(scale, scale, mul);

	} else if (bounds_type == OBS_BOUNDS_SCALE_TO_WIDTH) {
		vec2_mulf(scale, scale, item->bounds.x / width);

	} else if (bounds_type == OBS_BOUNDS_SCALE_TO_HEIGHT) {
		vec2_mulf(scale, scale, item->bounds.y / height);

	} else if (bounds_type == OBS_BOUNDS_STRETCH) {
		scale->x = item->bounds.x / (float)(*cx);
		scale->y = item->bounds.y / (float)(*cy);
	}

	width = (float)(*cx) * scale->x;
	height = (float)(*cy) * scale->y;
	width_diff = item->bounds.x - width;
	height_diff = item->bounds.y - height;
	*cx = (uint32_t)item->bounds.x;
	*cy = (uint32_t)item->bounds.y;

	add_alignment(origin, item->bounds_align, (int)-width_diff,
		      (int)-height_diff);
}

static inline uint32_t calc_cx(const struct obs_scene_item *item,
			       uint32_t width)
{
	uint32_t crop_cx = item->crop.left + item->crop.right;
	return (crop_cx > width) ? 2 : (width - crop_cx);
}

static inline uint32_t calc_cy(const struct obs_scene_item *item,
			       uint32_t height)
{
	uint32_t crop_cy = item->crop.top + item->crop.bottom;
	return (crop_cy > height) ? 2 : (height - crop_cy);
}

static void update_item_transform(struct obs_scene_item *item, bool update_tex)
{
	uint32_t width;
	uint32_t height;
	uint32_t cx;
	uint32_t cy;
	struct vec2 base_origin;
	struct vec2 origin;
	struct vec2 scale;
	struct calldata params;
	uint8_t stack[128];

	if (os_atomic_load_long(&item->defer_update) > 0)
		return;

	width = obs_source_get_width(item->source);
	height = obs_source_get_height(item->source);
	cx = calc_cx(item, width);
	cy = calc_cy(item, height);
	scale = item->scale;
	item->last_width = width;
	item->last_height = height;

	width = cx;
	height = cy;

	vec2_zero(&base_origin);
	vec2_zero(&origin);

	/* ----------------------- */

	if (item->bounds_type != OBS_BOUNDS_NONE) {
		calculate_bounds_data(item, &origin, &scale, &cx, &cy);
	} else {
		cx = (uint32_t)((float)cx * scale.x);
		cy = (uint32_t)((float)cy * scale.y);
	}

	add_alignment(&origin, item->align, (int)cx, (int)cy);

	matrix4_identity(&item->draw_transform);
	matrix4_scale3f(&item->draw_transform, &item->draw_transform, scale.x,
			scale.y, 1.0f);
	matrix4_translate3f(&item->draw_transform, &item->draw_transform,
			    -origin.x, -origin.y, 0.0f);
	matrix4_rotate_aa4f(&item->draw_transform, &item->draw_transform, 0.0f,
			    0.0f, 1.0f, RAD(item->rot));
	matrix4_translate3f(&item->draw_transform, &item->draw_transform,
			    item->pos.x, item->pos.y, 0.0f);

	item->output_scale = scale;

	/* ----------------------- */

	if (item->bounds_type != OBS_BOUNDS_NONE) {
		vec2_copy(&scale, &item->bounds);
	} else {
		scale.x = (float)width * item->scale.x;
		scale.y = (float)height * item->scale.y;
	}

	item->box_scale = scale;

	add_alignment(&base_origin, item->align, (int)scale.x, (int)scale.y);

	matrix4_identity(&item->box_transform);
	matrix4_scale3f(&item->box_transform, &item->box_transform, scale.x,
			scale.y, 1.0f);
	matrix4_translate3f(&item->box_transform, &item->box_transform,
			    -base_origin.x, -base_origin.y, 0.0f);
	matrix4_rotate_aa4f(&item->box_transform, &item->box_transform, 0.0f,
			    0.0f, 1.0f, RAD(item->rot));
	matrix4_translate3f(&item->box_transform, &item->box_transform,
			    item->pos.x, item->pos.y, 0.0f);

	/* ----------------------- */

	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_ptr(&params, "item", item);
	signal_parent(item->parent, "item_transform", &params);

	if (!update_tex)
		return;

	if (item->item_render && !item_texture_enabled(item)) {
		obs_enter_graphics();
		gs_texrender_destroy(item->item_render);
		item->item_render = NULL;
		obs_leave_graphics();

	} else if (!item->item_render && item_texture_enabled(item)) {
		obs_enter_graphics();
		item->item_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		obs_leave_graphics();
	}

	os_atomic_set_bool(&item->update_transform, false);
}

static inline bool source_size_changed(struct obs_scene_item *item)
{
	uint32_t width = obs_source_get_width(item->source);
	uint32_t height = obs_source_get_height(item->source);

	return item->last_width != width || item->last_height != height;
}

static inline bool crop_enabled(const struct obs_sceneitem_crop *crop)
{
	return crop->left || crop->right || crop->top || crop->bottom;
}

static inline bool scale_filter_enabled(const struct obs_scene_item *item)
{
	return item->scale_filter != OBS_SCALE_DISABLE;
}

static inline bool item_is_scene(const struct obs_scene_item *item)
{
	return item->source && item->source->info.type == OBS_SOURCE_TYPE_SCENE;
}

static inline bool item_texture_enabled(const struct obs_scene_item *item)
{
	return crop_enabled(&item->crop) || scale_filter_enabled(item) ||
	       (item_is_scene(item) && !item->is_group);
}

static void render_item_texture(struct obs_scene_item *item)
{
	gs_texture_t *tex = gs_texrender_get_texture(item->item_render);
	if (!tex) {
		return;
	}

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_ITEM_TEXTURE,
			      "render_item_texture");

	gs_effect_t *effect = obs->video.default_effect;
	enum obs_scale_type type = item->scale_filter;
	uint32_t cx = gs_texture_get_width(tex);
	uint32_t cy = gs_texture_get_height(tex);
	const char *tech = "Draw";

	if (type != OBS_SCALE_DISABLE) {
		if (type == OBS_SCALE_POINT) {
			gs_eparam_t *image =
				gs_effect_get_param_by_name(effect, "image");
			gs_effect_set_next_sampler(image,
						   obs->video.point_sampler);

		} else if (!close_float(item->output_scale.x, 1.0f, EPSILON) ||
			   !close_float(item->output_scale.y, 1.0f, EPSILON)) {
			gs_eparam_t *scale_param;
			gs_eparam_t *scale_i_param;

			if (item->output_scale.x < 0.5f ||
			    item->output_scale.y < 0.5f) {
				effect = obs->video.bilinear_lowres_effect;
			} else if (type == OBS_SCALE_BICUBIC) {
				effect = obs->video.bicubic_effect;
			} else if (type == OBS_SCALE_LANCZOS) {
				effect = obs->video.lanczos_effect;
			} else if (type == OBS_SCALE_AREA) {
				effect = obs->video.area_effect;
				if ((item->output_scale.x >= 1.0f) &&
				    (item->output_scale.y >= 1.0f))
					tech = "DrawUpscale";
			}

			scale_param = gs_effect_get_param_by_name(
				effect, "base_dimension");
			if (scale_param) {
				struct vec2 base_res = {(float)cx, (float)cy};

				gs_effect_set_vec2(scale_param, &base_res);
			}

			scale_i_param = gs_effect_get_param_by_name(
				effect, "base_dimension_i");
			if (scale_i_param) {
				struct vec2 base_res_i = {1.0f / (float)cx,
							  1.0f / (float)cy};

				gs_effect_set_vec2(scale_i_param, &base_res_i);
			}
		}
	}

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	while (gs_effect_loop(effect, tech))
		obs_source_draw(tex, 0, 0, 0, 0, 0);

	gs_blend_state_pop();

	GS_DEBUG_MARKER_END();
}

static inline void render_item(struct obs_scene_item *item)
{
	GS_DEBUG_MARKER_BEGIN_FORMAT(GS_DEBUG_COLOR_ITEM, "Item: %s",
				     obs_source_get_name(item->source));

	if (item->item_render) {
		uint32_t width = obs_source_get_width(item->source);
		uint32_t height = obs_source_get_height(item->source);

		if (!width || !height) {
			goto cleanup;
		}

		uint32_t cx = calc_cx(item, width);
		uint32_t cy = calc_cy(item, height);

		if (cx && cy && gs_texrender_begin(item->item_render, cx, cy)) {
			float cx_scale = (float)width / (float)cx;
			float cy_scale = (float)height / (float)cy;
			struct vec4 clear_color;

			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, (float)width, 0.0f, (float)height,
				 -100.0f, 100.0f);

			gs_matrix_scale3f(cx_scale, cy_scale, 1.0f);
			gs_matrix_translate3f(-(float)item->crop.left,
					      -(float)item->crop.top, 0.0f);

			obs_source_video_render(item->source);

			gs_texrender_end(item->item_render);
		}
	}

	const bool previous = gs_set_linear_srgb(true);
	gs_matrix_push();
	gs_matrix_mul(&item->draw_transform);
	if (item->item_render) {
		render_item_texture(item);
	} else {
		obs_source_video_render(item->source);
	}
	gs_matrix_pop();
	gs_set_linear_srgb(previous);

cleanup:
	GS_DEBUG_MARKER_END();
}

static void scene_video_tick(void *data, float seconds)
{
	struct obs_scene *scene = data;
	struct obs_scene_item *item;

	video_lock(scene);
	item = scene->first_item;
	while (item) {
		if (item->item_render)
			gs_texrender_reset(item->item_render);
		item = item->next;
	}
	video_unlock(scene);

	UNUSED_PARAMETER(seconds);
}

/* assumes video lock */
static void
update_transforms_and_prune_sources(obs_scene_t *scene,
				    struct darray *remove_items,
				    obs_sceneitem_t *group_sceneitem)
{
	struct obs_scene_item *item = scene->first_item;
	bool rebuild_group =
		group_sceneitem &&
		os_atomic_load_bool(&group_sceneitem->update_group_resize);

	while (item) {
		if (obs_source_removed(item->source)) {
			struct obs_scene_item *del_item = item;
			item = item->next;

			remove_without_release(del_item);
			darray_push_back(sizeof(struct obs_scene_item *),
					 remove_items, &del_item);
			rebuild_group = true;
			continue;
		}

		if (item->is_group) {
			obs_scene_t *group_scene = item->source->context.data;

			video_lock(group_scene);
			update_transforms_and_prune_sources(group_scene,
							    remove_items, item);
			video_unlock(group_scene);
		}

		if (os_atomic_load_bool(&item->update_transform) ||
		    source_size_changed(item)) {

			update_item_transform(item, true);
			rebuild_group = true;
		}

		item = item->next;
	}

	if (rebuild_group && group_sceneitem)
		resize_group(group_sceneitem);
}

static void scene_video_render(void *data, gs_effect_t *effect)
{
	DARRAY(struct obs_scene_item *) remove_items;
	struct obs_scene *scene = data;
	struct obs_scene_item *item;

	da_init(remove_items);

	video_lock(scene);

	if (!scene->is_group) {
		update_transforms_and_prune_sources(scene, &remove_items.da,
						    NULL);
	}

	gs_blend_state_push();
	gs_reset_blend_state();

	item = scene->first_item;
	while (item) {
		if (item->user_visible)
			render_item(item);

		item = item->next;
	}

	gs_blend_state_pop();

	video_unlock(scene);

	for (size_t i = 0; i < remove_items.num; i++)
		obs_sceneitem_release(remove_items.array[i]);
	da_free(remove_items);

	UNUSED_PARAMETER(effect);
}

static void set_visibility(struct obs_scene_item *item, bool vis)
{
	pthread_mutex_lock(&item->actions_mutex);

	da_resize(item->audio_actions, 0);

	if (os_atomic_load_long(&item->active_refs) > 0) {
		if (!vis)
			obs_source_remove_active_child(item->parent->source,
						       item->source);
	} else if (vis) {
		obs_source_add_active_child(item->parent->source, item->source);
	}

	os_atomic_set_long(&item->active_refs, vis ? 1 : 0);
	item->visible = vis;
	item->user_visible = vis;

	pthread_mutex_unlock(&item->actions_mutex);
}

static void scene_load(void *data, obs_data_t *settings);

static void scene_load_item(struct obs_scene *scene, obs_data_t *item_data)
{
	const char *name = obs_data_get_string(item_data, "name");
	obs_source_t *source;
	const char *scale_filter_str;
	struct obs_scene_item *item;
	bool visible;
	bool lock;

	if (obs_data_get_bool(item_data, "group_item_backup"))
		return;

	source = obs_get_source_by_name(name);
	if (!source) {
		blog(LOG_WARNING,
		     "[scene_load_item] Source %s not "
		     "found!",
		     name);
		return;
	}

	item = obs_scene_add(scene, source);
	if (!item) {
		blog(LOG_WARNING,
		     "[scene_load_item] Could not add source '%s' "
		     "to scene '%s'!",
		     name, obs_source_get_name(scene->source));

		obs_source_release(source);
		return;
	}

	item->is_group = strcmp(source->info.id, group_info.id) == 0;

	obs_data_set_default_int(item_data, "align",
				 OBS_ALIGN_TOP | OBS_ALIGN_LEFT);

	if (obs_data_has_user_value(item_data, "id"))
		item->id = obs_data_get_int(item_data, "id");

	item->rot = (float)obs_data_get_double(item_data, "rot");
	item->align = (uint32_t)obs_data_get_int(item_data, "align");
	visible = obs_data_get_bool(item_data, "visible");
	lock = obs_data_get_bool(item_data, "locked");
	obs_data_get_vec2(item_data, "pos", &item->pos);
	obs_data_get_vec2(item_data, "scale", &item->scale);

	obs_data_release(item->private_settings);
	item->private_settings =
		obs_data_get_obj(item_data, "private_settings");
	if (!item->private_settings)
		item->private_settings = obs_data_create();

	set_visibility(item, visible);
	obs_sceneitem_set_locked(item, lock);

	item->bounds_type = (enum obs_bounds_type)obs_data_get_int(
		item_data, "bounds_type");
	item->bounds_align =
		(uint32_t)obs_data_get_int(item_data, "bounds_align");
	obs_data_get_vec2(item_data, "bounds", &item->bounds);

	item->crop.left = (uint32_t)obs_data_get_int(item_data, "crop_left");
	item->crop.top = (uint32_t)obs_data_get_int(item_data, "crop_top");
	item->crop.right = (uint32_t)obs_data_get_int(item_data, "crop_right");
	item->crop.bottom =
		(uint32_t)obs_data_get_int(item_data, "crop_bottom");

	scale_filter_str = obs_data_get_string(item_data, "scale_filter");
	item->scale_filter = OBS_SCALE_DISABLE;

	if (scale_filter_str) {
		if (astrcmpi(scale_filter_str, "point") == 0)
			item->scale_filter = OBS_SCALE_POINT;
		else if (astrcmpi(scale_filter_str, "bilinear") == 0)
			item->scale_filter = OBS_SCALE_BILINEAR;
		else if (astrcmpi(scale_filter_str, "bicubic") == 0)
			item->scale_filter = OBS_SCALE_BICUBIC;
		else if (astrcmpi(scale_filter_str, "lanczos") == 0)
			item->scale_filter = OBS_SCALE_LANCZOS;
		else if (astrcmpi(scale_filter_str, "area") == 0)
			item->scale_filter = OBS_SCALE_AREA;
	}

	if (item->item_render && !item_texture_enabled(item)) {
		obs_enter_graphics();
		gs_texrender_destroy(item->item_render);
		item->item_render = NULL;
		obs_leave_graphics();

	} else if (!item->item_render && item_texture_enabled(item)) {
		obs_enter_graphics();
		item->item_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		obs_leave_graphics();
	}

	obs_source_release(source);

	update_item_transform(item, false);
}

static void scene_load(void *data, obs_data_t *settings)
{
	struct obs_scene *scene = data;
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count, i;

	remove_all_items(scene);

	if (!items)
		return;

	count = obs_data_array_count(items);

	for (i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		scene_load_item(scene, item_data);
		obs_data_release(item_data);
	}

	if (obs_data_has_user_value(settings, "id_counter"))
		scene->id_counter = obs_data_get_int(settings, "id_counter");

	if (obs_data_get_bool(settings, "custom_size")) {
		scene->cx = (uint32_t)obs_data_get_int(settings, "cx");
		scene->cy = (uint32_t)obs_data_get_int(settings, "cy");
		scene->custom_size = true;
	}

	obs_data_array_release(items);
}

static void scene_save(void *data, obs_data_t *settings);

static void scene_save_item(obs_data_array_t *array,
			    struct obs_scene_item *item,
			    struct obs_scene_item *backup_group)
{
	obs_data_t *item_data = obs_data_create();
	const char *name = obs_source_get_name(item->source);
	const char *scale_filter;
	struct vec2 pos = item->pos;
	struct vec2 scale = item->scale;
	float rot = item->rot;

	if (backup_group) {
		get_ungrouped_transform(backup_group, &pos, &scale, &rot);
	}

	obs_data_set_string(item_data, "name", name);
	obs_data_set_bool(item_data, "visible", item->user_visible);
	obs_data_set_bool(item_data, "locked", item->locked);
	obs_data_set_double(item_data, "rot", rot);
	obs_data_set_vec2(item_data, "pos", &pos);
	obs_data_set_vec2(item_data, "scale", &scale);
	obs_data_set_int(item_data, "align", (int)item->align);
	obs_data_set_int(item_data, "bounds_type", (int)item->bounds_type);
	obs_data_set_int(item_data, "bounds_align", (int)item->bounds_align);
	obs_data_set_vec2(item_data, "bounds", &item->bounds);
	obs_data_set_int(item_data, "crop_left", (int)item->crop.left);
	obs_data_set_int(item_data, "crop_top", (int)item->crop.top);
	obs_data_set_int(item_data, "crop_right", (int)item->crop.right);
	obs_data_set_int(item_data, "crop_bottom", (int)item->crop.bottom);
	obs_data_set_int(item_data, "id", item->id);
	obs_data_set_bool(item_data, "group_item_backup", !!backup_group);

	if (item->is_group) {
		obs_scene_t *group_scene = item->source->context.data;
		obs_sceneitem_t *group_item;

		/* save group items as part of main scene, but ignored.
		 * causes an automatic ungroup if scene collection file
		 * is loaded in previous versions. */
		full_lock(group_scene);

		group_item = group_scene->first_item;
		while (group_item) {
			scene_save_item(array, group_item, item);
			group_item = group_item->next;
		}

		full_unlock(group_scene);
	}

	if (item->scale_filter == OBS_SCALE_POINT)
		scale_filter = "point";
	else if (item->scale_filter == OBS_SCALE_BILINEAR)
		scale_filter = "bilinear";
	else if (item->scale_filter == OBS_SCALE_BICUBIC)
		scale_filter = "bicubic";
	else if (item->scale_filter == OBS_SCALE_LANCZOS)
		scale_filter = "lanczos";
	else if (item->scale_filter == OBS_SCALE_AREA)
		scale_filter = "area";
	else
		scale_filter = "disable";

	obs_data_set_string(item_data, "scale_filter", scale_filter);

	obs_data_set_obj(item_data, "private_settings", item->private_settings);

	obs_data_array_push_back(array, item_data);
	obs_data_release(item_data);
}

static void scene_save(void *data, obs_data_t *settings)
{
	struct obs_scene *scene = data;
	obs_data_array_t *array = obs_data_array_create();
	struct obs_scene_item *item;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		scene_save_item(array, item, NULL);
		item = item->next;
	}

	obs_data_set_int(settings, "id_counter", scene->id_counter);
	obs_data_set_bool(settings, "custom_size", scene->custom_size);
	if (scene->custom_size) {
		obs_data_set_int(settings, "cx", scene->cx);
		obs_data_set_int(settings, "cy", scene->cy);
	}

	full_unlock(scene);

	obs_data_set_array(settings, "items", array);
	obs_data_array_release(array);
}

static uint32_t scene_getwidth(void *data)
{
	obs_scene_t *scene = data;
	return scene->custom_size ? scene->cx : obs->video.base_width;
}

static uint32_t scene_getheight(void *data)
{
	obs_scene_t *scene = data;
	return scene->custom_size ? scene->cy : obs->video.base_height;
}

static void apply_scene_item_audio_actions(struct obs_scene_item *item,
					   float *buf, uint64_t ts,
					   size_t sample_rate)
{
	bool cur_visible = item->visible;
	uint64_t frame_num = 0;
	size_t deref_count = 0;

	pthread_mutex_lock(&item->actions_mutex);

	for (size_t i = 0; i < item->audio_actions.num; i++) {
		struct item_action action = item->audio_actions.array[i];
		uint64_t timestamp = action.timestamp;
		uint64_t new_frame_num;

		if (timestamp < ts)
			timestamp = ts;

		new_frame_num = util_mul_div64(timestamp - ts, sample_rate,
					       1000000000ULL);

		if (ts && new_frame_num >= AUDIO_OUTPUT_FRAMES)
			break;

		da_erase(item->audio_actions, i--);

		item->visible = action.visible;
		if (!item->visible)
			deref_count++;

		if (buf && new_frame_num > frame_num) {
			for (; frame_num < new_frame_num; frame_num++)
				buf[frame_num] = cur_visible ? 1.0f : 0.0f;
		}

		cur_visible = item->visible;
	}

	if (buf) {
		for (; frame_num < AUDIO_OUTPUT_FRAMES; frame_num++)
			buf[frame_num] = cur_visible ? 1.0f : 0.0f;
	}

	pthread_mutex_unlock(&item->actions_mutex);

	while (deref_count--) {
		if (os_atomic_dec_long(&item->active_refs) == 0) {
			obs_source_remove_active_child(item->parent->source,
						       item->source);
		}
	}
}

static bool apply_scene_item_volume(struct obs_scene_item *item, float *buf,
				    uint64_t ts, size_t sample_rate)
{
	bool actions_pending;
	struct item_action action;

	pthread_mutex_lock(&item->actions_mutex);

	actions_pending = item->audio_actions.num > 0;
	if (actions_pending)
		action = item->audio_actions.array[0];

	pthread_mutex_unlock(&item->actions_mutex);

	if (actions_pending) {
		uint64_t duration = util_mul_div64(AUDIO_OUTPUT_FRAMES,
						   1000000000ULL, sample_rate);

		if (!ts || action.timestamp < (ts + duration)) {
			apply_scene_item_audio_actions(item, buf, ts,
						       sample_rate);
			return true;
		}
	}

	return false;
}

static void process_all_audio_actions(struct obs_scene_item *item,
				      size_t sample_rate)
{
	while (apply_scene_item_volume(item, NULL, 0, sample_rate))
		;
}

static void mix_audio_with_buf(float *p_out, float *p_in, float *buf_in,
			       size_t pos, size_t count)
{
	register float *out = p_out;
	register float *buf = buf_in + pos;
	register float *in = p_in + pos;
	register float *end = in + count;

	while (in < end)
		*(out++) += *(in++) * *(buf++);
}

static inline void mix_audio(float *p_out, float *p_in, size_t pos,
			     size_t count)
{
	register float *out = p_out;
	register float *in = p_in + pos;
	register float *end = in + count;

	while (in < end)
		*(out++) += *(in++);
}

static bool scene_audio_render(void *data, uint64_t *ts_out,
			       struct obs_source_audio_mix *audio_output,
			       uint32_t mixers, size_t channels,
			       size_t sample_rate)
{
	uint64_t timestamp = 0;
	float buf[AUDIO_OUTPUT_FRAMES];
	struct obs_source_audio_mix child_audio;
	struct obs_scene *scene = data;
	struct obs_scene_item *item;

	audio_lock(scene);

	item = scene->first_item;
	while (item) {
		if (!obs_source_audio_pending(item->source) && item->visible) {
			uint64_t source_ts =
				obs_source_get_audio_timestamp(item->source);

			if (source_ts && (!timestamp || source_ts < timestamp))
				timestamp = source_ts;
		}

		item = item->next;
	}

	if (!timestamp) {
		/* just process all pending audio actions if no audio playing,
		 * otherwise audio actions will just never be processed */
		item = scene->first_item;
		while (item) {
			process_all_audio_actions(item, sample_rate);
			item = item->next;
		}

		audio_unlock(scene);
		return false;
	}

	item = scene->first_item;
	while (item) {
		uint64_t source_ts;
		size_t pos, count;
		bool apply_buf;

		apply_buf = apply_scene_item_volume(item, buf, timestamp,
						    sample_rate);

		if (obs_source_audio_pending(item->source)) {
			item = item->next;
			continue;
		}

		source_ts = obs_source_get_audio_timestamp(item->source);
		if (!source_ts) {
			item = item->next;
			continue;
		}

		pos = (size_t)ns_to_audio_frames(sample_rate,
						 source_ts - timestamp);
		count = AUDIO_OUTPUT_FRAMES - pos;

		if (!apply_buf && !item->visible) {
			item = item->next;
			continue;
		}

		obs_source_get_audio_mix(item->source, &child_audio);
		for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
			if ((mixers & (1 << mix)) == 0)
				continue;

			for (size_t ch = 0; ch < channels; ch++) {
				float *out = audio_output->output[mix].data[ch];
				float *in = child_audio.output[mix].data[ch];

				if (apply_buf)
					mix_audio_with_buf(out, in, buf, pos,
							   count);
				else
					mix_audio(out, in, pos, count);
			}
		}

		item = item->next;
	}

	*ts_out = timestamp;
	audio_unlock(scene);

	return true;
}

const struct obs_source_info scene_info = {
	.id = "scene",
	.type = OBS_SOURCE_TYPE_SCENE,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_COMPOSITE | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = scene_getname,
	.create = scene_create,
	.destroy = scene_destroy,
	.video_tick = scene_video_tick,
	.video_render = scene_video_render,
	.audio_render = scene_audio_render,
	.get_width = scene_getwidth,
	.get_height = scene_getheight,
	.load = scene_load,
	.save = scene_save,
	.enum_active_sources = scene_enum_active_sources,
	.enum_all_sources = scene_enum_all_sources};

const struct obs_source_info group_info = {
	.id = "group",
	.type = OBS_SOURCE_TYPE_SCENE,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_COMPOSITE,
	.get_name = group_getname,
	.create = scene_create,
	.destroy = scene_destroy,
	.video_tick = scene_video_tick,
	.video_render = scene_video_render,
	.audio_render = scene_audio_render,
	.get_width = scene_getwidth,
	.get_height = scene_getheight,
	.load = scene_load,
	.save = scene_save,
	.enum_active_sources = scene_enum_active_sources,
	.enum_all_sources = scene_enum_all_sources};

static inline obs_scene_t *create_id(const char *id, const char *name)
{
	struct obs_source *source = obs_source_create(id, name, NULL, NULL);
	return source->context.data;
}

static inline obs_scene_t *create_private_id(const char *id, const char *name)
{
	struct obs_source *source = obs_source_create_private(id, name, NULL);
	return source->context.data;
}

obs_scene_t *obs_scene_create(const char *name)
{
	return create_id("scene", name);
}

obs_scene_t *obs_scene_create_private(const char *name)
{
	return create_private_id("scene", name);
}

static obs_source_t *get_child_at_idx(obs_scene_t *scene, size_t idx)
{
	struct obs_scene_item *item = scene->first_item;

	while (item && idx--)
		item = item->next;
	return item ? item->source : NULL;
}

static inline obs_source_t *dup_child(struct darray *array, size_t idx,
				      obs_scene_t *new_scene, bool private)
{
	DARRAY(struct obs_scene_item *) old_items;
	obs_source_t *source;

	old_items.da = *array;

	source = old_items.array[idx]->source;

	/* if the old item is referenced more than once in the old scene,
	 * make sure they're referenced similarly in the new scene to reduce
	 * load times */
	for (size_t i = 0; i < idx; i++) {
		struct obs_scene_item *item = old_items.array[i];
		if (item->source == source) {
			source = get_child_at_idx(new_scene, i);
			obs_source_addref(source);
			return source;
		}
	}

	return obs_source_duplicate(
		source, private ? obs_source_get_name(source) : NULL, private);
}

static inline obs_source_t *new_ref(obs_source_t *source)
{
	obs_source_addref(source);
	return source;
}

static inline void duplicate_item_data(struct obs_scene_item *dst,
				       struct obs_scene_item *src,
				       bool defer_texture_update,
				       bool duplicate_hotkeys,
				       bool duplicate_private_data)
{
	struct obs_scene *dst_scene = dst->parent;

	if (!src->user_visible)
		set_visibility(dst, false);

	dst->selected = src->selected;
	dst->pos = src->pos;
	dst->rot = src->rot;
	dst->scale = src->scale;
	dst->align = src->align;
	dst->last_width = src->last_width;
	dst->last_height = src->last_height;
	dst->output_scale = src->output_scale;
	dst->scale_filter = src->scale_filter;
	dst->box_transform = src->box_transform;
	dst->box_scale = src->box_scale;
	dst->draw_transform = src->draw_transform;
	dst->bounds_type = src->bounds_type;
	dst->bounds_align = src->bounds_align;
	dst->bounds = src->bounds;

	if (duplicate_hotkeys && !dst_scene->source->context.private) {
		obs_data_array_t *data0 = NULL;
		obs_data_array_t *data1 = NULL;

		obs_hotkey_pair_save(src->toggle_visibility, &data0, &data1);
		obs_hotkey_pair_load(dst->toggle_visibility, data0, data1);

		obs_data_array_release(data0);
		obs_data_array_release(data1);
	}

	obs_sceneitem_set_crop(dst, &src->crop);
	obs_sceneitem_set_locked(dst, src->locked);

	if (defer_texture_update) {
		os_atomic_set_bool(&dst->update_transform, true);
	} else {
		if (!dst->item_render && item_texture_enabled(dst)) {
			obs_enter_graphics();
			dst->item_render =
				gs_texrender_create(GS_RGBA, GS_ZS_NONE);
			obs_leave_graphics();
		}
	}

	if (duplicate_private_data) {
		obs_data_apply(dst->private_settings, src->private_settings);
	}
}

obs_scene_t *obs_scene_duplicate(obs_scene_t *scene, const char *name,
				 enum obs_scene_duplicate_type type)
{
	bool make_unique = ((int)type & (1 << 0)) != 0;
	bool make_private = ((int)type & (1 << 1)) != 0;
	DARRAY(struct obs_scene_item *) items;
	struct obs_scene *new_scene;
	struct obs_scene_item *item;
	struct obs_source *source;

	da_init(items);

	if (!obs_ptr_valid(scene, "obs_scene_duplicate"))
		return NULL;

	/* --------------------------------- */

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		da_push_back(items, &item);
		obs_sceneitem_addref(item);
		item = item->next;
	}

	full_unlock(scene);

	/* --------------------------------- */

	new_scene = make_private
			    ? create_private_id(scene->source->info.id, name)
			    : create_id(scene->source->info.id, name);

	obs_source_copy_filters(new_scene->source, scene->source);

	obs_data_apply(new_scene->source->private_settings,
		       scene->source->private_settings);

	/* never duplicate sub-items for groups */
	if (scene->is_group)
		make_unique = false;

	for (size_t i = 0; i < items.num; i++) {
		item = items.array[i];
		source = make_unique ? dup_child(&items.da, i, new_scene,
						 make_private)
				     : new_ref(item->source);

		if (source) {
			struct obs_scene_item *new_item =
				obs_scene_add(new_scene, source);

			if (!new_item) {
				obs_source_release(source);
				continue;
			}

			duplicate_item_data(new_item, item, false, false,
					    false);

			obs_source_release(source);
		}
	}

	for (size_t i = 0; i < items.num; i++)
		obs_sceneitem_release(items.array[i]);

	if (new_scene->is_group)
		resize_scene(new_scene);

	da_free(items);
	return new_scene;
}

void obs_scene_addref(obs_scene_t *scene)
{
	if (scene)
		obs_source_addref(scene->source);
}

void obs_scene_release(obs_scene_t *scene)
{
	if (scene)
		obs_source_release(scene->source);
}

obs_source_t *obs_scene_get_source(const obs_scene_t *scene)
{
	return scene ? scene->source : NULL;
}

obs_scene_t *obs_scene_from_source(const obs_source_t *source)
{
	if (!source || strcmp(source->info.id, scene_info.id) != 0)
		return NULL;

	return source->context.data;
}

obs_scene_t *obs_group_from_source(const obs_source_t *source)
{
	if (!source || strcmp(source->info.id, group_info.id) != 0)
		return NULL;

	return source->context.data;
}

obs_sceneitem_t *obs_scene_find_source(obs_scene_t *scene, const char *name)
{
	struct obs_scene_item *item;

	if (!scene)
		return NULL;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		if (strcmp(item->source->context.name, name) == 0)
			break;

		item = item->next;
	}

	full_unlock(scene);

	return item;
}

obs_sceneitem_t *obs_scene_find_source_recursive(obs_scene_t *scene,
						 const char *name)
{
	struct obs_scene_item *item;

	if (!scene)
		return NULL;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		if (strcmp(item->source->context.name, name) == 0)
			break;

		if (item->is_group) {
			obs_scene_t *group = item->source->context.data;
			obs_sceneitem_t *child =
				obs_scene_find_source(group, name);
			if (child) {
				item = child;
				break;
			}
		}

		item = item->next;
	}

	full_unlock(scene);

	return item;
}

obs_sceneitem_t *obs_scene_find_sceneitem_by_id(obs_scene_t *scene, int64_t id)
{
	struct obs_scene_item *item;

	if (!scene)
		return NULL;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		if (item->id == id)
			break;

		item = item->next;
	}

	full_unlock(scene);

	return item;
}

void obs_scene_enum_items(obs_scene_t *scene,
			  bool (*callback)(obs_scene_t *, obs_sceneitem_t *,
					   void *),
			  void *param)
{
	struct obs_scene_item *item;

	if (!scene || !callback)
		return;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		struct obs_scene_item *next = item->next;

		obs_sceneitem_addref(item);

		if (!callback(scene, item, param)) {
			obs_sceneitem_release(item);
			break;
		}

		obs_sceneitem_release(item);

		item = next;
	}

	full_unlock(scene);
}

static obs_sceneitem_t *sceneitem_get_ref(obs_sceneitem_t *si)
{
	long owners = os_atomic_load_long(&si->ref);
	while (owners > 0) {
		if (os_atomic_compare_exchange_long(&si->ref, &owners,
						    owners + 1)) {
			return si;
		}
	}
	return NULL;
}

static bool hotkey_show_sceneitem(void *data, obs_hotkey_pair_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	obs_sceneitem_t *si = sceneitem_get_ref(data);
	if (pressed && si && !si->user_visible) {
		obs_sceneitem_set_visible(si, true);
		obs_sceneitem_release(si);
		return true;
	}

	obs_sceneitem_release(si);
	return false;
}

static bool hotkey_hide_sceneitem(void *data, obs_hotkey_pair_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	obs_sceneitem_t *si = sceneitem_get_ref(data);
	if (pressed && si && si->user_visible) {
		obs_sceneitem_set_visible(si, false);
		obs_sceneitem_release(si);
		return true;
	}

	obs_sceneitem_release(si);
	return false;
}

static void init_hotkeys(obs_scene_t *scene, obs_sceneitem_t *item,
			 const char *name)
{
	struct dstr show = {0};
	struct dstr hide = {0};
	struct dstr show_desc = {0};
	struct dstr hide_desc = {0};

	dstr_copy(&show, "libobs.show_scene_item.%1");
	dstr_replace(&show, "%1", name);
	dstr_copy(&hide, "libobs.hide_scene_item.%1");
	dstr_replace(&hide, "%1", name);

	dstr_copy(&show_desc, obs->hotkeys.sceneitem_show);
	dstr_replace(&show_desc, "%1", name);
	dstr_copy(&hide_desc, obs->hotkeys.sceneitem_hide);
	dstr_replace(&hide_desc, "%1", name);

	item->toggle_visibility = obs_hotkey_pair_register_source(
		scene->source, show.array, show_desc.array, hide.array,
		hide_desc.array, hotkey_show_sceneitem, hotkey_hide_sceneitem,
		item, item);

	dstr_free(&show);
	dstr_free(&hide);
	dstr_free(&show_desc);
	dstr_free(&hide_desc);
}

static void sceneitem_rename_hotkey(const obs_sceneitem_t *scene_item,
				    const char *new_name)
{
	struct dstr show = {0};
	struct dstr hide = {0};
	struct dstr show_desc = {0};
	struct dstr hide_desc = {0};

	dstr_copy(&show, "libobs.show_scene_item.%1");
	dstr_replace(&show, "%1", new_name);
	dstr_copy(&hide, "libobs.hide_scene_item.%1");
	dstr_replace(&hide, "%1", new_name);

	obs_hotkey_pair_set_names(scene_item->toggle_visibility, show.array,
				  hide.array);

	dstr_copy(&show_desc, obs->hotkeys.sceneitem_show);
	dstr_replace(&show_desc, "%1", new_name);
	dstr_copy(&hide_desc, obs->hotkeys.sceneitem_hide);
	dstr_replace(&hide_desc, "%1", new_name);

	obs_hotkey_pair_set_descriptions(scene_item->toggle_visibility,
					 show_desc.array, hide_desc.array);

	dstr_free(&show);
	dstr_free(&hide);
	dstr_free(&show_desc);
	dstr_free(&hide_desc);
}

static void sceneitem_renamed(void *param, calldata_t *data)
{
	obs_sceneitem_t *scene_item = param;
	const char *name = calldata_string(data, "new_name");

	sceneitem_rename_hotkey(scene_item, name);
}

static inline bool source_has_audio(obs_source_t *source)
{
	return (source->info.output_flags &
		(OBS_SOURCE_AUDIO | OBS_SOURCE_COMPOSITE)) != 0;
}

static obs_sceneitem_t *obs_scene_add_internal(obs_scene_t *scene,
					       obs_source_t *source,
					       obs_sceneitem_t *insert_after)
{
	struct obs_scene_item *last;
	struct obs_scene_item *item;
	pthread_mutex_t mutex;

	struct item_action action = {.visible = true,
				     .timestamp = os_gettime_ns()};

	if (!scene)
		return NULL;

	if (!source) {
		blog(LOG_ERROR, "Tried to add a NULL source to a scene");
		return NULL;
	}

	if (pthread_mutex_init(&mutex, NULL) != 0) {
		blog(LOG_WARNING, "Failed to create scene item mutex");
		return NULL;
	}

	if (!obs_source_add_active_child(scene->source, source)) {
		blog(LOG_WARNING, "Failed to add source to scene due to "
				  "infinite source recursion");
		pthread_mutex_destroy(&mutex);
		return NULL;
	}

	item = bzalloc(sizeof(struct obs_scene_item));
	item->source = source;
	item->id = ++scene->id_counter;
	item->parent = scene;
	item->ref = 1;
	item->align = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	item->actions_mutex = mutex;
	item->user_visible = true;
	item->locked = false;
	item->is_group = strcmp(source->info.id, group_info.id) == 0;
	item->private_settings = obs_data_create();
	item->toggle_visibility = OBS_INVALID_HOTKEY_PAIR_ID;
	os_atomic_set_long(&item->active_refs, 1);
	vec2_set(&item->scale, 1.0f, 1.0f);
	matrix4_identity(&item->draw_transform);
	matrix4_identity(&item->box_transform);

	obs_source_addref(source);

	if (source_has_audio(source)) {
		item->visible = false;
		da_push_back(item->audio_actions, &action);
	} else {
		item->visible = true;
	}

	if (item_texture_enabled(item)) {
		obs_enter_graphics();
		item->item_render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		obs_leave_graphics();
	}

	full_lock(scene);

	if (insert_after) {
		obs_sceneitem_t *next = insert_after->next;
		if (next)
			next->prev = item;
		item->next = insert_after->next;
		item->prev = insert_after;
		insert_after->next = item;
	} else {
		last = scene->first_item;
		if (!last) {
			scene->first_item = item;
		} else {
			while (last->next)
				last = last->next;

			last->next = item;
			item->prev = last;
		}
	}

	full_unlock(scene);

	if (!scene->source->context.private)
		init_hotkeys(scene, item, obs_source_get_name(source));

	signal_handler_connect(obs_source_get_signal_handler(source), "rename",
			       sceneitem_renamed, item);

	return item;
}

obs_sceneitem_t *obs_scene_add(obs_scene_t *scene, obs_source_t *source)
{
	obs_sceneitem_t *item = obs_scene_add_internal(scene, source, NULL);
	struct calldata params;
	uint8_t stack[128];

	if (!item)
		return NULL;

	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_ptr(&params, "scene", scene);
	calldata_set_ptr(&params, "item", item);
	signal_handler_signal(scene->source->context.signals, "item_add",
			      &params);
	return item;
}

static void obs_sceneitem_destroy(obs_sceneitem_t *item)
{
	if (item) {
		if (item->item_render) {
			obs_enter_graphics();
			gs_texrender_destroy(item->item_render);
			obs_leave_graphics();
		}
		obs_data_release(item->private_settings);
		obs_hotkey_pair_unregister(item->toggle_visibility);
		pthread_mutex_destroy(&item->actions_mutex);
		signal_handler_disconnect(
			obs_source_get_signal_handler(item->source), "rename",
			sceneitem_renamed, item);
		if (item->source)
			obs_source_release(item->source);
		da_free(item->audio_actions);
		bfree(item);
	}
}

void obs_sceneitem_addref(obs_sceneitem_t *item)
{
	if (item)
		os_atomic_inc_long(&item->ref);
}

void obs_sceneitem_release(obs_sceneitem_t *item)
{
	if (!item)
		return;

	if (os_atomic_dec_long(&item->ref) == 0)
		obs_sceneitem_destroy(item);
}

void obs_sceneitem_remove(obs_sceneitem_t *item)
{
	obs_scene_t *scene;

	if (!item)
		return;

	scene = item->parent;

	full_lock(scene);

	if (item->removed) {
		if (scene)
			full_unlock(scene);
		return;
	}

	item->removed = true;

	assert(scene != NULL);
	assert(scene->source != NULL);

	set_visibility(item, false);

	signal_item_remove(item);
	detach_sceneitem(item);

	full_unlock(scene);

	obs_sceneitem_release(item);
}

obs_scene_t *obs_sceneitem_get_scene(const obs_sceneitem_t *item)
{
	return item ? item->parent : NULL;
}

obs_source_t *obs_sceneitem_get_source(const obs_sceneitem_t *item)
{
	return item ? item->source : NULL;
}

static void signal_parent(obs_scene_t *parent, const char *command,
			  calldata_t *params)
{
	calldata_set_ptr(params, "scene", parent);
	signal_handler_signal(parent->source->context.signals, command, params);
}

void obs_sceneitem_select(obs_sceneitem_t *item, bool select)
{
	struct calldata params;
	uint8_t stack[128];
	const char *command = select ? "item_select" : "item_deselect";

	if (!item || item->selected == select || !item->parent)
		return;

	item->selected = select;

	calldata_init_fixed(&params, stack, sizeof(stack));
	calldata_set_ptr(&params, "item", item);

	signal_parent(item->parent, command, &params);
}

bool obs_sceneitem_selected(const obs_sceneitem_t *item)
{
	return item ? item->selected : false;
}

#define do_update_transform(item)                                          \
	do {                                                               \
		if (!item->parent || item->parent->is_group)               \
			os_atomic_set_bool(&item->update_transform, true); \
		else                                                       \
			update_item_transform(item, false);                \
	} while (false)

void obs_sceneitem_set_pos(obs_sceneitem_t *item, const struct vec2 *pos)
{
	if (item) {
		vec2_copy(&item->pos, pos);
		do_update_transform(item);
	}
}

void obs_sceneitem_set_rot(obs_sceneitem_t *item, float rot)
{
	if (item) {
		item->rot = rot;
		do_update_transform(item);
	}
}

void obs_sceneitem_set_scale(obs_sceneitem_t *item, const struct vec2 *scale)
{
	if (item) {
		vec2_copy(&item->scale, scale);
		do_update_transform(item);
	}
}

void obs_sceneitem_set_alignment(obs_sceneitem_t *item, uint32_t alignment)
{
	if (item) {
		item->align = alignment;
		do_update_transform(item);
	}
}

static inline void signal_reorder(struct obs_scene_item *item)
{
	const char *command = NULL;
	struct calldata params;
	uint8_t stack[128];

	command = "reorder";

	calldata_init_fixed(&params, stack, sizeof(stack));
	signal_parent(item->parent, command, &params);
}

static inline void signal_refresh(obs_scene_t *scene)
{
	const char *command = NULL;
	struct calldata params;
	uint8_t stack[128];

	command = "refresh";

	calldata_init_fixed(&params, stack, sizeof(stack));
	signal_parent(scene, command, &params);
}

void obs_sceneitem_set_order(obs_sceneitem_t *item,
			     enum obs_order_movement movement)
{
	if (!item)
		return;

	struct obs_scene_item *next, *prev;
	struct obs_scene *scene = item->parent;

	obs_scene_addref(scene);
	full_lock(scene);

	next = item->next;
	prev = item->prev;

	detach_sceneitem(item);

	if (movement == OBS_ORDER_MOVE_DOWN) {
		attach_sceneitem(scene, item, prev ? prev->prev : NULL);

	} else if (movement == OBS_ORDER_MOVE_UP) {
		attach_sceneitem(scene, item, next ? next : prev);

	} else if (movement == OBS_ORDER_MOVE_TOP) {
		struct obs_scene_item *last = next;
		if (!last) {
			last = prev;
		} else {
			while (last->next)
				last = last->next;
		}

		attach_sceneitem(scene, item, last);

	} else if (movement == OBS_ORDER_MOVE_BOTTOM) {
		attach_sceneitem(scene, item, NULL);
	}

	full_unlock(scene);

	signal_reorder(item);
	obs_scene_release(scene);
}

void obs_sceneitem_set_order_position(obs_sceneitem_t *item, int position)
{
	if (!item)
		return;

	struct obs_scene *scene = item->parent;
	struct obs_scene_item *next;

	obs_scene_addref(scene);
	full_lock(scene);

	detach_sceneitem(item);
	next = scene->first_item;

	if (position == 0) {
		attach_sceneitem(scene, item, NULL);
	} else {
		for (int i = position; i > 1; --i) {
			if (next->next == NULL)
				break;
			next = next->next;
		}

		attach_sceneitem(scene, item, next);
	}

	full_unlock(scene);

	signal_reorder(item);
	obs_scene_release(scene);
}

void obs_sceneitem_set_bounds_type(obs_sceneitem_t *item,
				   enum obs_bounds_type type)
{
	if (item) {
		item->bounds_type = type;
		do_update_transform(item);
	}
}

void obs_sceneitem_set_bounds_alignment(obs_sceneitem_t *item,
					uint32_t alignment)
{
	if (item) {
		item->bounds_align = alignment;
		do_update_transform(item);
	}
}

void obs_sceneitem_set_bounds(obs_sceneitem_t *item, const struct vec2 *bounds)
{
	if (item) {
		item->bounds = *bounds;
		do_update_transform(item);
	}
}

void obs_sceneitem_get_pos(const obs_sceneitem_t *item, struct vec2 *pos)
{
	if (item)
		vec2_copy(pos, &item->pos);
}

float obs_sceneitem_get_rot(const obs_sceneitem_t *item)
{
	return item ? item->rot : 0.0f;
}

void obs_sceneitem_get_scale(const obs_sceneitem_t *item, struct vec2 *scale)
{
	if (item)
		vec2_copy(scale, &item->scale);
}

uint32_t obs_sceneitem_get_alignment(const obs_sceneitem_t *item)
{
	return item ? item->align : 0;
}

enum obs_bounds_type obs_sceneitem_get_bounds_type(const obs_sceneitem_t *item)
{
	return item ? item->bounds_type : OBS_BOUNDS_NONE;
}

uint32_t obs_sceneitem_get_bounds_alignment(const obs_sceneitem_t *item)
{
	return item ? item->bounds_align : 0;
}

void obs_sceneitem_get_bounds(const obs_sceneitem_t *item, struct vec2 *bounds)
{
	if (item)
		*bounds = item->bounds;
}

void obs_sceneitem_get_info(const obs_sceneitem_t *item,
			    struct obs_transform_info *info)
{
	if (item && info) {
		info->pos = item->pos;
		info->rot = item->rot;
		info->scale = item->scale;
		info->alignment = item->align;
		info->bounds_type = item->bounds_type;
		info->bounds_alignment = item->bounds_align;
		info->bounds = item->bounds;
	}
}

void obs_sceneitem_set_info(obs_sceneitem_t *item,
			    const struct obs_transform_info *info)
{
	if (item && info) {
		item->pos = info->pos;
		item->rot = info->rot;
		item->scale = info->scale;
		item->align = info->alignment;
		item->bounds_type = info->bounds_type;
		item->bounds_align = info->bounds_alignment;
		item->bounds = info->bounds;
		do_update_transform(item);
	}
}

void obs_sceneitem_get_draw_transform(const obs_sceneitem_t *item,
				      struct matrix4 *transform)
{
	if (item)
		matrix4_copy(transform, &item->draw_transform);
}

void obs_sceneitem_get_box_transform(const obs_sceneitem_t *item,
				     struct matrix4 *transform)
{
	if (item)
		matrix4_copy(transform, &item->box_transform);
}

void obs_sceneitem_get_box_scale(const obs_sceneitem_t *item,
				 struct vec2 *scale)
{
	if (item)
		*scale = item->box_scale;
}

bool obs_sceneitem_visible(const obs_sceneitem_t *item)
{
	return item ? item->user_visible : false;
}

bool obs_sceneitem_set_visible(obs_sceneitem_t *item, bool visible)
{
	struct calldata cd;
	uint8_t stack[256];
	struct item_action action = {.visible = visible,
				     .timestamp = os_gettime_ns()};

	if (!item)
		return false;

	if (item->user_visible == visible)
		return false;

	if (!item->parent)
		return false;

	if (visible) {
		if (os_atomic_inc_long(&item->active_refs) == 1) {
			if (!obs_source_add_active_child(item->parent->source,
							 item->source)) {
				os_atomic_dec_long(&item->active_refs);
				return false;
			}
		}
	}

	item->user_visible = visible;

	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "item", item);
	calldata_set_bool(&cd, "visible", visible);

	signal_parent(item->parent, "item_visible", &cd);

	if (source_has_audio(item->source)) {
		pthread_mutex_lock(&item->actions_mutex);
		da_push_back(item->audio_actions, &action);
		pthread_mutex_unlock(&item->actions_mutex);
	} else {
		set_visibility(item, visible);
	}
	return true;
}

bool obs_sceneitem_locked(const obs_sceneitem_t *item)
{
	return item ? item->locked : false;
}

bool obs_sceneitem_set_locked(obs_sceneitem_t *item, bool lock)
{
	struct calldata cd;
	uint8_t stack[256];

	if (!item)
		return false;

	if (item->locked == lock)
		return false;

	if (!item->parent)
		return false;

	item->locked = lock;

	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "item", item);
	calldata_set_bool(&cd, "locked", lock);

	signal_parent(item->parent, "item_locked", &cd);

	return true;
}

static bool sceneitems_match(obs_scene_t *scene, obs_sceneitem_t *const *items,
			     size_t size, bool *order_matches)
{
	obs_sceneitem_t *item = scene->first_item;

	size_t count = 0;
	while (item) {
		bool found = false;
		for (size_t i = 0; i < size; i++) {
			if (items[i] != item)
				continue;

			if (count != i)
				*order_matches = false;

			found = true;
			break;
		}

		if (!found)
			return false;

		item = item->next;
		count += 1;
	}

	return count == size;
}

bool obs_scene_reorder_items(obs_scene_t *scene,
			     obs_sceneitem_t *const *item_order,
			     size_t item_order_size)
{
	if (!scene || !item_order_size)
		return false;

	obs_scene_addref(scene);
	full_lock(scene);

	bool order_matches = true;
	if (!sceneitems_match(scene, item_order, item_order_size,
			      &order_matches) ||
	    order_matches) {
		full_unlock(scene);
		obs_scene_release(scene);
		return false;
	}

	scene->first_item = item_order[0];

	obs_sceneitem_t *prev = NULL;
	for (size_t i = 0; i < item_order_size; i++) {
		item_order[i]->prev = prev;
		item_order[i]->next = NULL;

		if (prev)
			prev->next = item_order[i];

		prev = item_order[i];
	}

	full_unlock(scene);

	signal_reorder(scene->first_item);
	obs_scene_release(scene);
	return true;
}

void obs_scene_atomic_update(obs_scene_t *scene,
			     obs_scene_atomic_update_func func, void *data)
{
	if (!scene)
		return;

	obs_scene_addref(scene);
	full_lock(scene);
	func(data, scene);
	full_unlock(scene);
	obs_scene_release(scene);
}

static inline bool crop_equal(const struct obs_sceneitem_crop *crop1,
			      const struct obs_sceneitem_crop *crop2)
{
	return crop1->left == crop2->left && crop1->right == crop2->right &&
	       crop1->top == crop2->top && crop1->bottom == crop2->bottom;
}

void obs_sceneitem_set_crop(obs_sceneitem_t *item,
			    const struct obs_sceneitem_crop *crop)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_set_crop"))
		return;
	if (!obs_ptr_valid(crop, "obs_sceneitem_set_crop"))
		return;
	if (crop_equal(crop, &item->crop))
		return;

	memcpy(&item->crop, crop, sizeof(*crop));

	if (item->crop.left < 0)
		item->crop.left = 0;
	if (item->crop.right < 0)
		item->crop.right = 0;
	if (item->crop.top < 0)
		item->crop.top = 0;
	if (item->crop.bottom < 0)
		item->crop.bottom = 0;

	os_atomic_set_bool(&item->update_transform, true);
}

void obs_sceneitem_get_crop(const obs_sceneitem_t *item,
			    struct obs_sceneitem_crop *crop)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_get_crop"))
		return;
	if (!obs_ptr_valid(crop, "obs_sceneitem_get_crop"))
		return;

	memcpy(crop, &item->crop, sizeof(*crop));
}

void obs_sceneitem_set_scale_filter(obs_sceneitem_t *item,
				    enum obs_scale_type filter)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_set_scale_filter"))
		return;

	item->scale_filter = filter;

	os_atomic_set_bool(&item->update_transform, true);
}

enum obs_scale_type obs_sceneitem_get_scale_filter(obs_sceneitem_t *item)
{
	return obs_ptr_valid(item, "obs_sceneitem_get_scale_filter")
		       ? item->scale_filter
		       : OBS_SCALE_DISABLE;
}

void obs_sceneitem_defer_update_begin(obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_defer_update_begin"))
		return;

	os_atomic_inc_long(&item->defer_update);
}

void obs_sceneitem_defer_update_end(obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_defer_update_end"))
		return;

	if (os_atomic_dec_long(&item->defer_update) == 0)
		do_update_transform(item);
}

void obs_sceneitem_defer_group_resize_begin(obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_defer_group_resize_begin"))
		return;

	os_atomic_inc_long(&item->defer_group_resize);
}

void obs_sceneitem_defer_group_resize_end(obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_defer_group_resize_end"))
		return;

	if (os_atomic_dec_long(&item->defer_group_resize) == 0)
		os_atomic_set_bool(&item->update_group_resize, true);
}

int64_t obs_sceneitem_get_id(const obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_get_id"))
		return 0;

	return item->id;
}

obs_data_t *obs_sceneitem_get_private_settings(obs_sceneitem_t *item)
{
	if (!obs_ptr_valid(item, "obs_sceneitem_get_private_settings"))
		return NULL;

	obs_data_addref(item->private_settings);
	return item->private_settings;
}

static inline void transform_val(struct vec2 *v2, struct matrix4 *transform)
{
	struct vec3 v;
	vec3_set(&v, v2->x, v2->y, 0.0f);
	vec3_transform(&v, &v, transform);
	v2->x = v.x;
	v2->y = v.y;
}

static void get_ungrouped_transform(obs_sceneitem_t *group, struct vec2 *pos,
				    struct vec2 *scale, float *rot)
{
	struct matrix4 transform;
	struct matrix4 mat;
	struct vec4 x_base;

	vec4_set(&x_base, 1.0f, 0.0f, 0.0f, 0.0f);

	matrix4_copy(&transform, &group->draw_transform);

	transform_val(pos, &transform);
	vec4_set(&transform.t, 0.0f, 0.0f, 0.0f, 1.0f);

	vec4_set(&mat.x, scale->x, 0.0f, 0.0f, 0.0f);
	vec4_set(&mat.y, 0.0f, scale->y, 0.0f, 0.0f);
	vec4_set(&mat.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_set(&mat.t, 0.0f, 0.0f, 0.0f, 1.0f);
	matrix4_mul(&mat, &mat, &transform);

	scale->x = vec4_len(&mat.x) * (scale->x > 0.0f ? 1.0f : -1.0f);
	scale->y = vec4_len(&mat.y) * (scale->y > 0.0f ? 1.0f : -1.0f);
	*rot += group->rot;
}

static void remove_group_transform(obs_sceneitem_t *group,
				   obs_sceneitem_t *item)
{
	obs_scene_t *parent = item->parent;
	if (!parent || !group)
		return;

	get_ungrouped_transform(group, &item->pos, &item->scale, &item->rot);

	update_item_transform(item, false);
}

static void apply_group_transform(obs_sceneitem_t *item, obs_sceneitem_t *group)
{
	struct matrix4 transform;
	struct matrix4 mat;
	struct vec4 x_base;

	vec4_set(&x_base, 1.0f, 0.0f, 0.0f, 0.0f);

	matrix4_inv(&transform, &group->draw_transform);

	transform_val(&item->pos, &transform);
	vec4_set(&transform.t, 0.0f, 0.0f, 0.0f, 1.0f);

	vec4_set(&mat.x, item->scale.x, 0.0f, 0.0f, 0.0f);
	vec4_set(&mat.y, 0.0f, item->scale.y, 0.0f, 0.0f);
	vec4_set(&mat.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_set(&mat.t, 0.0f, 0.0f, 0.0f, 1.0f);
	matrix4_mul(&mat, &mat, &transform);

	item->scale.x =
		vec4_len(&mat.x) * (item->scale.x > 0.0f ? 1.0f : -1.0f);
	item->scale.y =
		vec4_len(&mat.y) * (item->scale.y > 0.0f ? 1.0f : -1.0f);
	item->rot -= group->rot;

	update_item_transform(item, false);
}

static bool resize_scene_base(obs_scene_t *scene, struct vec2 *minv,
			      struct vec2 *maxv, struct vec2 *scale)
{
	vec2_set(minv, M_INFINITE, M_INFINITE);
	vec2_set(maxv, -M_INFINITE, -M_INFINITE);

	obs_sceneitem_t *item = scene->first_item;
	if (!item) {
		scene->cx = 0;
		scene->cy = 0;
		return false;
	}

	while (item) {
#define get_min_max(x_val, y_val)                             \
	do {                                                  \
		struct vec3 v;                                \
		vec3_set(&v, x_val, y_val, 0.0f);             \
		vec3_transform(&v, &v, &item->box_transform); \
		if (v.x < minv->x)                            \
			minv->x = v.x;                        \
		if (v.y < minv->y)                            \
			minv->y = v.y;                        \
		if (v.x > maxv->x)                            \
			maxv->x = v.x;                        \
		if (v.y > maxv->y)                            \
			maxv->y = v.y;                        \
	} while (false)

		get_min_max(0.0f, 0.0f);
		get_min_max(1.0f, 0.0f);
		get_min_max(0.0f, 1.0f);
		get_min_max(1.0f, 1.0f);
#undef get_min_max

		item = item->next;
	}

	item = scene->first_item;
	while (item) {
		vec2_sub(&item->pos, &item->pos, minv);
		update_item_transform(item, false);
		item = item->next;
	}

	vec2_sub(scale, maxv, minv);
	scene->cx = (uint32_t)ceilf(scale->x);
	scene->cy = (uint32_t)ceilf(scale->y);
	return true;
}

static void resize_scene(obs_scene_t *scene)
{
	struct vec2 minv;
	struct vec2 maxv;
	struct vec2 scale;
	resize_scene_base(scene, &minv, &maxv, &scale);
}

/* assumes group scene and parent scene is locked */
static void resize_group(obs_sceneitem_t *group)
{
	obs_scene_t *scene = group->source->context.data;
	struct vec2 minv;
	struct vec2 maxv;
	struct vec2 scale;

	if (os_atomic_load_long(&group->defer_group_resize) > 0)
		return;

	if (!resize_scene_base(scene, &minv, &maxv, &scale))
		return;

	if (group->bounds_type == OBS_BOUNDS_NONE) {
		struct vec2 new_pos;

		if ((group->align & OBS_ALIGN_LEFT) != 0)
			new_pos.x = minv.x;
		else if ((group->align & OBS_ALIGN_RIGHT) != 0)
			new_pos.x = maxv.x;
		else
			new_pos.x = (maxv.x - minv.x) * 0.5f + minv.x;

		if ((group->align & OBS_ALIGN_TOP) != 0)
			new_pos.y = minv.y;
		else if ((group->align & OBS_ALIGN_BOTTOM) != 0)
			new_pos.y = maxv.y;
		else
			new_pos.y = (maxv.y - minv.y) * 0.5f + minv.y;

		transform_val(&new_pos, &group->draw_transform);
		vec2_copy(&group->pos, &new_pos);
	}

	os_atomic_set_bool(&group->update_group_resize, false);

	update_item_transform(group, false);
}

obs_sceneitem_t *obs_scene_add_group(obs_scene_t *scene, const char *name)
{
	return obs_scene_insert_group(scene, name, NULL, 0);
}

obs_sceneitem_t *obs_scene_add_group2(obs_scene_t *scene, const char *name,
				      bool signal)
{
	return obs_scene_insert_group2(scene, name, NULL, 0, signal);
}

obs_sceneitem_t *obs_scene_insert_group(obs_scene_t *scene, const char *name,
					obs_sceneitem_t **items, size_t count)
{
	if (!scene)
		return NULL;

	/* don't allow groups or sub-items of other groups */
	for (size_t i = count; i > 0; i--) {
		obs_sceneitem_t *item = items[i - 1];
		if (item->parent != scene || item->is_group)
			return NULL;
	}

	obs_scene_t *sub_scene = create_id("group", name);
	obs_sceneitem_t *last_item = items ? items[count - 1] : NULL;

	obs_sceneitem_t *item =
		obs_scene_add_internal(scene, sub_scene->source, last_item);

	obs_scene_release(sub_scene);

	if (!items || !count)
		return item;

	/* ------------------------- */

	full_lock(scene);
	full_lock(sub_scene);
	sub_scene->first_item = items[0];

	for (size_t i = count; i > 0; i--) {
		size_t idx = i - 1;
		remove_group_transform(item, items[idx]);
		detach_sceneitem(items[idx]);
	}
	for (size_t i = 0; i < count; i++) {
		size_t idx = i;
		if (idx != (count - 1)) {
			size_t next_idx = idx + 1;
			items[idx]->next = items[next_idx];
			items[next_idx]->prev = items[idx];
		} else {
			items[idx]->next = NULL;
		}
		items[idx]->parent = sub_scene;
		apply_group_transform(items[idx], item);
	}
	items[0]->prev = NULL;
	resize_group(item);
	full_unlock(sub_scene);
	full_unlock(scene);

	/* ------------------------- */

	return item;
}

obs_sceneitem_t *obs_scene_insert_group2(obs_scene_t *scene, const char *name,
					 obs_sceneitem_t **items, size_t count,
					 bool signal)
{
	obs_sceneitem_t *item =
		obs_scene_insert_group(scene, name, items, count);
	if (signal && item)
		signal_refresh(scene);
	return item;
}

obs_sceneitem_t *obs_scene_get_group(obs_scene_t *scene, const char *name)
{
	if (!scene || !name || !*name) {
		return NULL;
	}

	obs_sceneitem_t *group = NULL;
	obs_sceneitem_t *item;

	full_lock(scene);

	item = scene->first_item;
	while (item) {
		if (item->is_group && item->source->context.name) {
			if (strcmp(item->source->context.name, name) == 0) {
				group = item;
				break;
			}
		}

		item = item->next;
	}

	full_unlock(scene);

	return group;
}

bool obs_sceneitem_is_group(obs_sceneitem_t *item)
{
	return item && item->is_group;
}

obs_scene_t *obs_sceneitem_group_get_scene(const obs_sceneitem_t *item)
{
	return (item && item->is_group) ? item->source->context.data : NULL;
}

void obs_sceneitem_group_ungroup(obs_sceneitem_t *item)
{
	if (!item || !item->is_group)
		return;

	obs_scene_t *scene = item->parent;
	obs_scene_t *subscene = item->source->context.data;
	obs_sceneitem_t *insert_after = item;
	obs_sceneitem_t *first;
	obs_sceneitem_t *last;

	full_lock(scene);

	/* ------------------------- */

	full_lock(subscene);
	first = subscene->first_item;
	last = first;
	while (last) {
		obs_sceneitem_t *dst;

		remove_group_transform(item, last);
		dst = obs_scene_add_internal(scene, last->source, insert_after);
		duplicate_item_data(dst, last, true, true, true);
		apply_group_transform(last, item);

		if (!last->next)
			break;

		insert_after = dst;
		last = last->next;
	}
	full_unlock(subscene);

	/* ------------------------- */

	detach_sceneitem(item);
	full_unlock(scene);

	obs_sceneitem_release(item);
}

void obs_sceneitem_group_ungroup2(obs_sceneitem_t *item, bool signal)
{
	obs_scene_t *scene = item->parent;
	obs_sceneitem_group_ungroup(item);
	if (signal)
		signal_refresh(scene);
}

void obs_sceneitem_group_add_item(obs_sceneitem_t *group, obs_sceneitem_t *item)
{
	if (!group || !group->is_group || !item)
		return;

	obs_scene_t *scene = group->parent;
	obs_scene_t *groupscene = group->source->context.data;

	if (item->parent != scene)
		return;

	if (item->parent == groupscene)
		return;

	/* ------------------------- */

	full_lock(scene);
	full_lock(groupscene);

	remove_group_transform(group, item);

	detach_sceneitem(item);
	attach_sceneitem(groupscene, item, NULL);

	apply_group_transform(item, group);

	resize_group(group);

	full_unlock(groupscene);
	full_unlock(scene);

	/* ------------------------- */

	signal_refresh(scene);
}

void obs_sceneitem_group_remove_item(obs_sceneitem_t *group,
				     obs_sceneitem_t *item)
{
	if (!item || !group || !group->is_group)
		return;

	obs_scene_t *groupscene = item->parent;
	obs_scene_t *scene = group->parent;

	/* ------------------------- */

	full_lock(scene);
	full_lock(groupscene);

	remove_group_transform(group, item);

	detach_sceneitem(item);
	attach_sceneitem(scene, item, NULL);

	resize_group(group);

	full_unlock(groupscene);
	full_unlock(scene);

	/* ------------------------- */

	signal_refresh(scene);
}

static void
build_current_order_info(obs_scene_t *scene,
			 struct obs_sceneitem_order_info **items_out,
			 size_t *size_out)
{
	DARRAY(struct obs_sceneitem_order_info) items;
	da_init(items);

	obs_sceneitem_t *item = scene->first_item;
	while (item) {
		da_push_back(items, &item);

		if (item->is_group) {
			obs_scene_t *sub_scene = item->source->context.data;

			full_lock(sub_scene);

			obs_sceneitem_t *sub_item = sub_scene->first_item;

			while (sub_item) {
				da_push_back(items, &item);

				sub_item = sub_item->next;
			}

			full_unlock(sub_scene);
		}

		item = item->next;
	}

	*items_out = items.array;
	*size_out = items.num;
}

static bool sceneitems_match2(obs_scene_t *scene,
			      struct obs_sceneitem_order_info *items,
			      size_t size)
{
	struct obs_sceneitem_order_info *cur_items;
	size_t cur_size;

	build_current_order_info(scene, &cur_items, &cur_size);
	if (cur_size != size) {
		bfree(cur_items);
		return false;
	}

	for (size_t i = 0; i < size; i++) {
		struct obs_sceneitem_order_info *new = &items[i];
		struct obs_sceneitem_order_info *old = &cur_items[i];

		if (new->group != old->group || new->item != old->item) {
			bfree(cur_items);
			return false;
		}
	}

	bfree(cur_items);
	return true;
}

static obs_sceneitem_t *
get_sceneitem_parent_group(obs_scene_t *scene, obs_sceneitem_t *group_subitem)
{
	if (group_subitem->is_group)
		return NULL;

	obs_sceneitem_t *item = scene->first_item;
	while (item) {
		if (item->is_group &&
		    item->source->context.data == group_subitem->parent)
			return item;
		item = item->next;
	}

	return NULL;
}

bool obs_scene_reorder_items2(obs_scene_t *scene,
			      struct obs_sceneitem_order_info *item_order,
			      size_t item_order_size)
{
	if (!scene || !item_order_size || !item_order)
		return false;

	obs_scene_addref(scene);
	full_lock(scene);

	if (sceneitems_match2(scene, item_order, item_order_size)) {
		full_unlock(scene);
		obs_scene_release(scene);
		return false;
	}

	for (size_t i = 0; i < item_order_size; i++) {
		struct obs_sceneitem_order_info *info = &item_order[i];
		if (!info->item->is_group) {
			obs_sceneitem_t *group =
				get_sceneitem_parent_group(scene, info->item);
			remove_group_transform(group, info->item);
		}
	}

	scene->first_item = item_order[0].item;

	obs_sceneitem_t *prev = NULL;
	for (size_t i = 0; i < item_order_size; i++) {
		struct obs_sceneitem_order_info *info = &item_order[i];
		obs_sceneitem_t *item = info->item;

		if (info->item->is_group) {
			obs_sceneitem_t *sub_prev = NULL;
			obs_scene_t *sub_scene =
				info->item->source->context.data;

			sub_scene->first_item = NULL;

			obs_scene_addref(sub_scene);
			full_lock(sub_scene);

			for (i++; i < item_order_size; i++) {
				struct obs_sceneitem_order_info *sub_info =
					&item_order[i];
				obs_sceneitem_t *sub_item = sub_info->item;

				if (sub_info->group != info->item) {
					i--;
					break;
				}

				if (!sub_scene->first_item)
					sub_scene->first_item = sub_item;

				sub_item->prev = sub_prev;
				sub_item->next = NULL;
				sub_item->parent = sub_scene;

				if (sub_prev)
					sub_prev->next = sub_item;

				apply_group_transform(sub_info->item,
						      sub_info->group);

				sub_prev = sub_item;
			}

			resize_group(info->item);
			full_unlock(sub_scene);
			obs_scene_release(sub_scene);
		}

		item->prev = prev;
		item->next = NULL;
		item->parent = scene;

		if (prev)
			prev->next = item;

		prev = item;
	}

	full_unlock(scene);

	signal_reorder(scene->first_item);
	obs_scene_release(scene);
	return true;
}

obs_sceneitem_t *obs_sceneitem_get_group(obs_scene_t *scene,
					 obs_sceneitem_t *group_subitem)
{
	if (!scene || !group_subitem || group_subitem->is_group)
		return NULL;

	full_lock(scene);
	obs_sceneitem_t *group =
		get_sceneitem_parent_group(scene, group_subitem);
	full_unlock(scene);

	return group;
}

bool obs_source_is_group(const obs_source_t *source)
{
	return source && strcmp(source->info.id, group_info.id) == 0;
}

bool obs_scene_is_group(const obs_scene_t *scene)
{
	return scene ? scene->is_group : false;
}

void obs_sceneitem_group_enum_items(obs_sceneitem_t *group,
				    bool (*callback)(obs_scene_t *,
						     obs_sceneitem_t *, void *),
				    void *param)
{
	if (!group || !group->is_group)
		return;

	obs_scene_t *scene = group->source->context.data;
	if (scene)
		obs_scene_enum_items(scene, callback, param);
}

void obs_sceneitem_force_update_transform(obs_sceneitem_t *item)
{
	if (!item)
		return;

	if (os_atomic_set_bool(&item->update_transform, false))
		update_item_transform(item, false);
}
