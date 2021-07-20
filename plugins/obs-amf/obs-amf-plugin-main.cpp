
#include <obs-module.h>
#include "amf-encoder.hpp"
#include "obs-amf.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-amf", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "AMF encoder support";
}

extern struct obs_encoder_info obs_amf_h264_encoder;
extern struct obs_encoder_info obs_amf_hevc_encoder;

bool obs_module_load(void)
{
	AMF::Initialize();
	AMF::Instance()->FillCaps();
	register_amf_h264_encoder();
	//register_amf_hevc_encoder();
	return true;
}

void obs_module_unload(void)
{
	AMF::Finalize();
}
