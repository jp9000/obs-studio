/******************************************************************************
    Copyright (C) 2020 by Hugh Bailey <obs.jim@gmail.com>

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

#include "window-basic-main.hpp"
#include "screenshot-obj.hpp"
#include "qt-wrappers.hpp"

static void ScreenshotTick(void *param, float);

/* ========================================================================= */

ScreenshotObj::ScreenshotObj(obs_source_t *source)
	: weakSource(OBSGetWeakRef(source))
{
	obs_add_tick_callback(ScreenshotTick, this);
}

ScreenshotObj::~ScreenshotObj()
{
	obs_enter_graphics();
	gs_stagesurface_destroy(stagesurf);
	gs_texrender_destroy(texrender);
	obs_leave_graphics();

	obs_remove_tick_callback(ScreenshotTick, this);
	if (th.joinable())
		th.join();
}

void ScreenshotObj::Screenshot()
{
	OBSSource source = OBSGetStrongRef(weakSource);

	if (source) {
		cx = obs_source_get_base_width(source);
		cy = obs_source_get_base_height(source);
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		cx = ovi.base_width;
		cy = ovi.base_height;
	}

	if (!cx || !cy) {
		blog(LOG_WARNING, "Cannot screenshot, invalid target size");
		obs_remove_tick_callback(ScreenshotTick, this);
		deleteLater();
		return;
	}

	texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);

	gs_texrender_reset(texrender);
	if (gs_texrender_begin(texrender, cx, cy)) {
		vec4 zero;
		vec4_zero(&zero);

		gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (source) {
			obs_source_inc_showing(source);
			obs_source_video_render(source);
			obs_source_dec_showing(source);
		} else {
			obs_render_main_texture();
		}

		gs_blend_state_pop();
		gs_texrender_end(texrender);
	}
}

void ScreenshotObj::Download()
{
	gs_stage_texture(stagesurf, gs_texrender_get_texture(texrender));
}

void ScreenshotObj::Copy()
{
	uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;

	image = QImage(cx, cy, QImage::Format::Format_RGBX8888);

	if (gs_stagesurface_map(stagesurf, &videoData, &videoLinesize)) {
		int linesize = image.bytesPerLine();
		for (int y = 0; y < (int)cy; y++)
			memcpy(image.scanLine(y),
			       videoData + (y * videoLinesize), linesize);

		gs_stagesurface_unmap(stagesurf);
	}
}

void ScreenshotObj::Save()
{
	OBSBasic *main = OBSBasic::Get();
	config_t *config = main->Config();

	const char *mode = config_get_string(config, "Output", "Mode");
	const char *type = config_get_string(config, "AdvOut", "RecType");
	const char *adv_path =
		strcmp(type, "Standard")
			? config_get_string(config, "AdvOut", "FFFilePath")
			: config_get_string(config, "AdvOut", "RecFilePath");
	const char *rec_path =
		strcmp(mode, "Advanced")
			? config_get_string(config, "SimpleOutput", "FilePath")
			: adv_path;

	bool noSpace =
		config_get_bool(config, "SimpleOutput", "FileNameWithoutSpace");
	const char *filenameFormat =
		config_get_string(config, "Output", "FilenameFormatting");
	bool overwriteIfExists =
		config_get_bool(config, "Output", "OverwriteIfExists");

	path = GetOutputFilename(
		rec_path, "png", noSpace, overwriteIfExists,
		GetFormatString(filenameFormat, "Screenshot", nullptr).c_str());

	th = std::thread([this] { MuxAndFinish(); });
}

void ScreenshotObj::MuxAndFinish()
{
	image.save(QT_UTF8(path.c_str()));
	blog(LOG_INFO, "Saved screenshot to '%s'", path.c_str());
	deleteLater();
}

/* ========================================================================= */

#define STAGE_SCREENSHOT 0
#define STAGE_DOWNLOAD 1
#define STAGE_COPY_AND_SAVE 2
#define STAGE_FINISH 3

static void ScreenshotTick(void *param, float)
{
	ScreenshotObj *data = reinterpret_cast<ScreenshotObj *>(param);

	if (data->stage == STAGE_FINISH) {
		return;
	}

	obs_enter_graphics();

	switch (data->stage) {
	case STAGE_SCREENSHOT:
		data->Screenshot();
		break;
	case STAGE_DOWNLOAD:
		data->Download();
		break;
	case STAGE_COPY_AND_SAVE:
		data->Copy();
		QMetaObject::invokeMethod(data, "Save");
		obs_remove_tick_callback(ScreenshotTick, data);
		break;
	}

	obs_leave_graphics();

	data->stage++;
}

void OBSBasic::Screenshot(OBSSource source)
{
	if (!!screenshotData) {
		blog(LOG_WARNING, "Cannot take new screenshot, "
				  "screenshot currently in progress");
		return;
	}

	screenshotData = new ScreenshotObj(source);
}

void OBSBasic::ScreenshotSelectedSource()
{
	OBSSceneItem item = GetCurrentSceneItem();
	if (item) {
		Screenshot(obs_sceneitem_get_source(item));
	} else {
		blog(LOG_INFO, "Could not take a source screenshot: "
			       "no source selected");
	}
}

void OBSBasic::ScreenshotProgram()
{
	Screenshot(GetProgramSource());
}

void OBSBasic::ScreenshotScene()
{
	Screenshot(GetCurrentSceneSource());
}
