#include <obs-internal.h>
#include "pulseaudio-wrapper.h"

static void pulseaudio_output_info(pa_context *c, const pa_source_info *i,
				   int eol, void *userdata)
{
	UNUSED_PARAMETER(c);
	if (eol != 0 || i->monitor_of_sink == PA_INVALID_INDEX)
		goto skip;

	struct enum_cb *ecb = (struct enum_cb *)userdata;
	if (ecb->cont)
		ecb->cont = ecb->cb(ecb->data, i->description, i->name);

skip:
	pulseaudio_signal(0);
}

void obs_enum_audio_monitoring_devices(obs_enum_audio_device_cb cb, void *data)
{
	struct enum_cb *ecb = bzalloc(sizeof(struct enum_cb));
	ecb->cb = cb;
	ecb->data = data;
	ecb->cont = 1;

	pulseaudio_init();
	pa_source_info_cb_t pa_cb = pulseaudio_output_info;
	pulseaudio_get_source_info_list(pa_cb, (void *)ecb);
	pulseaudio_unref();

	bfree(ecb);
}
