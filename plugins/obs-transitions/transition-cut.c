#include <obs-module.h>

struct cut_info {
	obs_source_t *source;
};

static const char *cut_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("CutTransition");
}

static void *cut_create(obs_data_t *settings, obs_source_t *source)
{
	struct cut_info *cut;

	cut = bmalloc(sizeof(*cut));
	cut->source = source;

	obs_transition_enable_fixed(source, true, 0);
	UNUSED_PARAMETER(settings);
	return cut;
}

static void cut_destroy(void *data)
{
	struct cut_info *cut = data;
	bfree(cut);
}

static void cut_video_render(void *data, gs_effect_t *effect)
{
	struct cut_info *cut = data;
	obs_transition_video_render(cut->source, NULL);
	UNUSED_PARAMETER(effect);
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

static bool cut_audio_render(void *data, uint64_t *ts_out,
			     struct obs_source_audio_mix *audio,
			     uint32_t mixers, size_t channels,
			     size_t sample_rate)
{
	struct cut_info *cut = data;
	return obs_transition_audio_render(cut->source, ts_out, audio, mixers,
					   channels, sample_rate, mix_a, mix_b);
}

struct obs_source_info cut_transition = {
	.id = "cut_transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = cut_get_name,
	.create = cut_create,
	.destroy = cut_destroy,
	.video_render = cut_video_render,
	.audio_render = cut_audio_render,
};
