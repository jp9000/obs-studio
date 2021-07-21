#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-transitions", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS core transitions";
}

extern struct obs_source_info cut_transition;
extern struct obs_source_info fade_transition;
extern struct obs_source_info swipe_transition;
extern struct obs_source_info slide_transition;
extern struct obs_source_info stinger_transition;
extern struct obs_source_info fade_to_color_transition;
extern struct obs_source_info luma_wipe_transition;
extern struct obs_source_info blackout_transition;

bool obs_module_load(void)
{
	obs_register_source(&cut_transition);
	obs_register_source(&fade_transition);
	obs_register_source(&swipe_transition);
	obs_register_source(&slide_transition);
	obs_register_source(&stinger_transition);
	obs_register_source(&fade_to_color_transition);
	obs_register_source(&luma_wipe_transition);
	obs_register_source(&blackout_transition);
	return true;
}
