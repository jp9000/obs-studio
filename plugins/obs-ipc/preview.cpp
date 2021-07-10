#include "preview.hpp"

Preview::Preview()
{
	SetSize(w, h);
	obs_add_main_render_callback(DrawCallback, this);
	texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
}

Preview::~Preview()
{
	obs_remove_main_render_callback(DrawCallback, this);

	for (auto iter = texCallbacks.begin(); iter != texCallbacks.end();) {
		iter = texCallbacks.erase(iter);
	}
	for (auto iter = presCallbacks.begin(); iter != presCallbacks.end();) {
		iter = presCallbacks.erase(iter);
	}

	obs_enter_graphics();
	gs_texture_destroy(tex);
	obs_leave_graphics();
}

uint32_t Preview::GetHandle()
{
	return handle;
}

void Preview::SignalTexChange()
{
	for (auto iter = texCallbacks.begin(); iter != texCallbacks.end();
	     ++iter) {
		iter->first(handle, iter->second);
	}
}

void Preview::SignalDraw()
{
	for (auto iter = presCallbacks.begin(); iter != presCallbacks.end();
	     ++iter) {
		iter->first(iter->second);
	}
}

void Preview::AddTexChangeCallback(texchange_cb cb, void *param)
{
	texCallbacks[cb] = param;
}

void Preview::AddPresentCallback(present_cb cb, void *param)
{
	presCallbacks[cb] = param;
}

void Preview::SetSize(uint32_t width, uint32_t height)
{
	uint32_t oldTexW = texW;
	uint32_t oldTexH = texH;

	w = width;
	h = height;

	texW = 128;
	for (int i = 0; texW < w && i < 5; i++) {
		texW = texSizes[i];
	}
	texH = 128;
	for (int i = 0; texH < h && i < 5; i++) {
		texH = texSizes[i];
	}

	if (texW != oldTexW || texH != oldTexH) {
		obs_enter_graphics();

		if (tex)
			gs_texture_destroy(tex);

		tex = NULL;
		tex = gs_texture_create(texW, texH, GS_BGRA, 1, NULL,
					GS_SHARED_TEX | GS_RENDER_TARGET);

		handle = gs_texture_get_shared_handle(tex);
		obs_leave_graphics();

		SignalTexChange();
	}
}

void Preview::DrawCallback(void *param, uint32_t cx, uint32_t cy)
{
	Preview *prev = (Preview *)param;

	if (!prev->tex || !cx || !cy)
		return;

	struct vec4 background;
	vec4_zero(&background);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_BILINEAR_LOWRES);
	gs_eparam_t *p_image = gs_effect_get_param_by_name(effect, "image");
	gs_eparam_t *p_dimension =
		gs_effect_get_param_by_name(effect, "base_dimension");
	gs_eparam_t *p_dimension_i =
		gs_effect_get_param_by_name(effect, "base_dimension_i");

	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");

	vec2 dimension;
	vec2 dimension_i;

	obs_source_t *output = obs_get_output_source(0);

	float width = (float)obs_source_get_width(output);
	float height = (float)obs_source_get_height(output);

	gs_texrender_reset(prev->texrender);
	gs_texrender_begin(prev->texrender, (uint32_t)width, (uint32_t)height);

	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);

	obs_source_video_render(output);

	gs_texrender_end(prev->texrender);

	obs_source_release(output);

	gs_viewport_push();
	gs_projection_push();

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_matrix_push();
	gs_matrix_identity();

	gs_texture_t *prevTarget = gs_get_render_target();
	gs_zstencil_t *prevZs = gs_get_zstencil_target();

	float offX = 0;
	float offY = 0;

	float scaleW = (float)prev->w / width;
	float scaleH = (float)prev->h / height;
	float scale = scaleW < scaleH ? scaleW : scaleH;

	if (scale == scaleW) {
		offY = ((float)prev->h - scale * height) / 2.0f;
	} else {
		offX = ((float)prev->w - scale * width) / 2.0f;
	}

	vec2_set(&dimension, width, height);
	vec2_set(&dimension_i, 1.0f / width, 1.0f / height);

	gs_effect_set_vec2(p_dimension, &dimension);
	gs_effect_set_vec2(p_dimension_i, &dimension_i);

	gs_set_render_target(prev->tex, NULL);
	gs_set_viewport((int)offX, (int)offY,
			(int)((float)prev->w - offX * 2.0f),
			(int)((float)prev->h - offY * 2.0f));

	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	gs_texture_t *tex = gs_texrender_get_texture(prev->texrender);
	gs_effect_set_texture(p_image, tex);
	gs_draw_sprite(tex, 0, (uint32_t)width, (uint32_t)height);

	gs_blend_state_pop();

	gs_set_render_target(prevTarget, prevZs);

	gs_matrix_pop();

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_projection_pop();
	gs_viewport_pop();

	prev->SignalDraw();
}
