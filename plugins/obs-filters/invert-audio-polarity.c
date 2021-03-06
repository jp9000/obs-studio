#include <obs-module.h>

static const char *invert_polarity_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("InvertPolarity");
}

static void invert_polarity_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static void *invert_polarity_create(obs_data_t *settings, obs_source_t *filter)
{
	UNUSED_PARAMETER(settings);
	return filter;
}

static struct obs_audio_data *
invert_polarity_filter_audio(void *unused, struct obs_audio_data *audio)
{
	float **adata = (float **)audio->data;

	for (size_t c = 0; c < MAX_AV_PLANES; c++) {
		register float *channel_data = adata[c];
		register float *channel_end = channel_data + audio->frames;

		if (!channel_data)
			break;

		while (channel_data < channel_end) {
			*(channel_data++) *= -1.0f;
		}
	}

	UNUSED_PARAMETER(unused);
	return audio;
}

struct obs_source_info invert_polarity_filter = {
	.id = "invert_polarity_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = invert_polarity_name,
	.create = invert_polarity_create,
	.destroy = invert_polarity_destroy,
	.filter_audio = invert_polarity_filter_audio,
};
