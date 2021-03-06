#include <obs-module.h>

#include "xcompcap-main.hpp"

static void *xcompcap_create(obs_data_t *settings, obs_source_t *source)
{
	return new XCompcapMain(settings, source);
}

static void xcompcap_destroy(void *data)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	delete cc;
}

static void xcompcap_video_tick(void *data, float seconds)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	cc->tick(seconds);
}

static void xcompcap_video_render(void *data, gs_effect_t *effect)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	cc->render(effect);
}

static uint32_t xcompcap_getwidth(void *data)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	return cc->width();
}

static uint32_t xcompcap_getheight(void *data)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	return cc->height();
}

static obs_properties_t *xcompcap_props(void *unused)
{
	UNUSED_PARAMETER(unused);

	return XCompcapMain::properties();
}

void xcompcap_defaults(obs_data_t *settings)
{
	XCompcapMain::defaults(settings);
}

void xcompcap_update(void *data, obs_data_t *settings)
{
	XCompcapMain *cc = (XCompcapMain *)data;
	cc->updateSettings(settings);
}

static const char *xcompcap_getname(void *)
{
	return obs_module_text("XCCapture");
}

extern "C" void xcomposite_load(void)
{
	if (!XCompcapMain::init())
		return;

	obs_source_info sinfo;
	memset(&sinfo, 0, sizeof(obs_source_info));

	sinfo.id = "xcomposite_input";
	sinfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			     OBS_SOURCE_DO_NOT_DUPLICATE;

	sinfo.get_name = xcompcap_getname;
	sinfo.create = xcompcap_create;
	sinfo.destroy = xcompcap_destroy;
	sinfo.get_properties = xcompcap_props;
	sinfo.get_defaults = xcompcap_defaults;
	sinfo.update = xcompcap_update;
	sinfo.video_tick = xcompcap_video_tick;
	sinfo.video_render = xcompcap_video_render;
	sinfo.get_width = xcompcap_getwidth;
	sinfo.get_height = xcompcap_getheight;
	sinfo.icon_type = OBS_ICON_TYPE_WINDOW_CAPTURE,

	obs_register_source(&sinfo);
}

extern "C" void xcomposite_unload(void)
{
	XCompcapMain::deinit();
}
