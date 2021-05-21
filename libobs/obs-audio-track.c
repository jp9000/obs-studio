#include "obs-internal.h"

void obs_audio_track_set_source(size_t track, obs_source_t *source)
{
	if (track > MAX_AUDIO_MIXES - 1)
		return;

	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_AUDIO_TRACK))
		return;

	obs->audio.audio_mixes.tracks[track] = source;
}

void obs_audio_track_check_feedback(obs_source_t *source, bool activating)
{
	if (!(source->info.output_flags & OBS_SOURCE_DO_NOT_SELF_MONITOR))
		return;

	obs_data_t *s = obs_source_get_settings(source);
	const char *s_dev_id = obs_data_get_string(s, "device_id");
	obs_data_release(s);

	const char *id = obs->audio.monitoring_device_id;

	if (!s_dev_id || !*s_dev_id || !id || !*id)
		return;

	bool match = audio_devices_match(s_dev_id, id);

	if (match) {
		if (activating) {
			os_atomic_inc_long(
				&obs->audio.audio_mixes.feedback_detection);

			blog(LOG_WARNING,
			     "Prevented feedback loop between '%s' and the master audio monitor",
			     obs_source_get_name(source));
		} else {
			os_atomic_dec_long(
				&obs->audio.audio_mixes.feedback_detection);
		}
	}
}

obs_source_t *obs_audio_track_get_source(size_t track)
{
	if (track > MAX_AUDIO_MIXES - 1)
		return NULL;

	return obs->audio.audio_mixes.tracks[track];
}

int obs_audio_track_get_index(obs_source_t *source)
{
	if (!source)
		return -1;

	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_AUDIO_TRACK))
		return -1;

	int track = -1;

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		if (source == obs_audio_track_get_source(i)) {
			track = (int)i;
			break;
		}
	}

	return track;
}

obs_data_array_t *obs_save_audio_track_sources()
{
	obs_data_array_t *array = obs_data_array_create();

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		obs_source_t *source = obs_audio_track_get_source(i);
		obs_data_t *track_data = obs_save_source(source);
		obs_data_array_push_back(array, track_data);
		obs_data_release(track_data);
	}

	return array;
}

void obs_load_audio_track_sources(obs_data_array_t *array)
{
	size_t count = obs_data_array_count(array);
	if (count > MAX_AUDIO_MIXES)
		count = MAX_AUDIO_MIXES;

	for (size_t i = 0; i < count; i++) {
		obs_data_t *source_data = obs_data_array_item(array, i);

		obs_source_t *source = obs_audio_track_get_source(i);

		if (source) {
			obs_source_release(source);
			source = NULL;
		}

		if (!source) {
			source = obs_load_source(source_data);
			obs_source_load(source);
			source->context.private = true;
			obs_audio_track_set_source(i, source);
		}
		obs_data_release(source_data);
	}
}

static const char *track_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Audio track (internal use only)";
}

const struct obs_source_info audio_track_info = {
	.id = "audio_track",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_CAP_DISABLED |
			OBS_SOURCE_AUDIO_TRACK,
	.get_name = track_name,
};
