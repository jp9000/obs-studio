/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
#define SUPPORTS_FRACTIONAL_SCALING
#endif

#include <graphics/vec4.h>
#include <graphics/matrix4.h>

static inline void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX,
					int windowCY, int &x, int &y,
					float &scale)
{
	double windowAspect, baseAspect;
	int newCX, newCY;

	windowAspect = double(windowCX) / double(windowCY);
	baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}

static inline void GetCenterPosFromFixedScale(int baseCX, int baseCY,
					      int windowCX, int windowCY,
					      int &x, int &y, float scale)
{
	x = (float(windowCX) - float(baseCX) * scale) / 2.0f;
	y = (float(windowCY) - float(baseCY) * scale) / 2.0f;
}

static inline QSize GetPixelSize(QWidget *widget)
{
#ifdef SUPPORTS_FRACTIONAL_SCALING
	return widget->size() * widget->devicePixelRatioF();
#else
	return widget->size() * widget->devicePixelRatio();
#endif
}

#define OUTLINE_COLOR 0xFFD0D0D0
#define LINE_LENGTH 0.1f

// Rec. ITU-R BT.1848-1 / EBU R 95
#define ACTION_SAFE_PERCENT 0.035f       // 3.5%
#define GRAPHICS_SAFE_PERCENT 0.05f      // 5.0%
#define FOURBYTHREE_SAFE_PERCENT 0.1625f // 16.25%

static inline void InitSafeAreas(gs_vertbuffer_t **actionSafeMargin,
				 gs_vertbuffer_t **graphicsSafeMargin,
				 gs_vertbuffer_t **fourByThreeSafeMargin,
				 gs_vertbuffer_t **leftLine,
				 gs_vertbuffer_t **topLine,
				 gs_vertbuffer_t **rightLine)
{
	obs_enter_graphics();

	// All essential action should be placed inside this area
	gs_render_start(true);
	gs_vertex2f(ACTION_SAFE_PERCENT, ACTION_SAFE_PERCENT);
	gs_vertex2f(ACTION_SAFE_PERCENT, 1 - ACTION_SAFE_PERCENT);
	gs_vertex2f(1 - ACTION_SAFE_PERCENT, 1 - ACTION_SAFE_PERCENT);
	gs_vertex2f(1 - ACTION_SAFE_PERCENT, ACTION_SAFE_PERCENT);
	gs_vertex2f(ACTION_SAFE_PERCENT, ACTION_SAFE_PERCENT);
	*actionSafeMargin = gs_render_save();

	// All graphics should be placed inside this area
	gs_render_start(true);
	gs_vertex2f(GRAPHICS_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(GRAPHICS_SAFE_PERCENT, 1 - GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(1 - GRAPHICS_SAFE_PERCENT, 1 - GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(1 - GRAPHICS_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(GRAPHICS_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	*graphicsSafeMargin = gs_render_save();

	// 4:3 safe area for widescreen
	gs_render_start(true);
	gs_vertex2f(FOURBYTHREE_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(1 - FOURBYTHREE_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(1 - FOURBYTHREE_SAFE_PERCENT, 1 - GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(FOURBYTHREE_SAFE_PERCENT, 1 - GRAPHICS_SAFE_PERCENT);
	gs_vertex2f(FOURBYTHREE_SAFE_PERCENT, GRAPHICS_SAFE_PERCENT);
	*fourByThreeSafeMargin = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.5f);
	gs_vertex2f(LINE_LENGTH, 0.5f);
	*leftLine = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.5f, 0.0f);
	gs_vertex2f(0.5f, LINE_LENGTH);
	*topLine = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(1.0f, 0.5f);
	gs_vertex2f(1 - LINE_LENGTH, 0.5f);
	*rightLine = gs_render_save();

	obs_leave_graphics();
}

static inline void RenderSafeAreas(gs_vertbuffer_t *vb, int cx, int cy)
{
	if (!vb)
		return;

	matrix4 transform;
	matrix4_identity(&transform);
	transform.x.x = cx;
	transform.y.y = cy;

	gs_load_vertexbuffer(vb);

	gs_matrix_push();
	gs_matrix_mul(&transform);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");

	gs_effect_set_color(color, OUTLINE_COLOR);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw(GS_LINESTRIP, 0, 0);

	gs_matrix_pop();
}
