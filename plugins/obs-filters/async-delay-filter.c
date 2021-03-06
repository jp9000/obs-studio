#include <obs-module.h>
#include <util/circlebuf.h>
#include <util/util_uint64.h>

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

#ifndef MSEC_TO_NSEC
#define MSEC_TO_NSEC 1000000ULL
#endif

#define SETTING_DELAY_MS "delay_ms"

#define TEXT_DELAY_MS obs_module_text("DelayMs")

struct async_delay_data {
	obs_source_t *context;

	/* contains struct obs_source_frame* */
	struct circlebuf video_frames;

	/* stores the audio data */
	struct circlebuf audio_frames;
	struct obs_audio_data audio_output;

	uint64_t last_video_ts;
	uint64_t last_audio_ts;
	uint64_t interval;
	uint64_t samplerate;
	bool video_delay_reached;
	bool audio_delay_reached;
	bool reset_video;
	bool reset_audio;
};

static const char *async_delay_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("AsyncDelayFilter");
}

static void free_video_data(struct async_delay_data *filter,
			    obs_source_t *parent)
{
	while (filter->video_frames.size) {
		struct obs_source_frame *frame;

		circlebuf_pop_front(&filter->video_frames, &frame,
				    sizeof(struct obs_source_frame *));
		obs_source_release_frame(parent, frame);
	}
}

static inline void free_audio_packet(struct obs_audio_data *audio)
{
	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		bfree(audio->data[i]);
	memset(audio, 0, sizeof(*audio));
}

static void free_audio_data(struct async_delay_data *filter)
{
	while (filter->audio_frames.size) {
		struct obs_audio_data audio;

		circlebuf_pop_front(&filter->audio_frames, &audio,
				    sizeof(struct obs_audio_data));
		free_audio_packet(&audio);
	}
}

static void async_delay_filter_update(void *data, obs_data_t *settings)
{
	struct async_delay_data *filter = data;
	uint64_t new_interval =
		(uint64_t)obs_data_get_int(settings, SETTING_DELAY_MS) *
		MSEC_TO_NSEC;

	if (new_interval < filter->interval)
		free_video_data(filter, obs_filter_get_parent(filter->context));

	filter->reset_audio = true;
	filter->reset_video = true;
	filter->interval = new_interval;
	filter->video_delay_reached = false;
	filter->audio_delay_reached = false;
}

static void *async_delay_filter_create(obs_data_t *settings,
				       obs_source_t *context)
{
	struct async_delay_data *filter = bzalloc(sizeof(*filter));
	struct obs_audio_info oai;

	filter->context = context;
	async_delay_filter_update(filter, settings);

	obs_get_audio_info(&oai);
	filter->samplerate = oai.samples_per_sec;

	return filter;
}

static void async_delay_filter_destroy(void *data)
{
	struct async_delay_data *filter = data;

	free_audio_packet(&filter->audio_output);
	circlebuf_free(&filter->video_frames);
	circlebuf_free(&filter->audio_frames);
	bfree(data);
}

static obs_properties_t *async_delay_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_int(props, SETTING_DELAY_MS,
						   TEXT_DELAY_MS, 0, 20000, 1);
	obs_property_int_set_suffix(p, " ms");

	UNUSED_PARAMETER(data);
	return props;
}

static void async_delay_filter_remove(void *data, obs_source_t *parent)
{
	struct async_delay_data *filter = data;

	free_video_data(filter, parent);
	free_audio_data(filter);
}

/* due to the fact that we need timing information to be consistent in order to
 * measure the current interval of data, if there is an unexpected hiccup or
 * jump with the timestamps, reset the cached delay data and start again to
 * ensure that the timing is consistent */
static inline bool is_timestamp_jump(uint64_t ts, uint64_t prev_ts)
{
	return ts < prev_ts || (ts - prev_ts) > SEC_TO_NSEC;
}

static struct obs_source_frame *
async_delay_filter_video(void *data, struct obs_source_frame *frame)
{
	struct async_delay_data *filter = data;
	obs_source_t *parent = obs_filter_get_parent(filter->context);
	struct obs_source_frame *output;
	uint64_t cur_interval;

	if (filter->reset_video ||
	    is_timestamp_jump(frame->timestamp, filter->last_video_ts)) {
		free_video_data(filter, parent);
		filter->video_delay_reached = false;
		filter->reset_video = false;
	}

	filter->last_video_ts = frame->timestamp;

	circlebuf_push_back(&filter->video_frames, &frame,
			    sizeof(struct obs_source_frame *));
	circlebuf_peek_front(&filter->video_frames, &output,
			     sizeof(struct obs_source_frame *));

	cur_interval = frame->timestamp - output->timestamp;
	if (!filter->video_delay_reached && cur_interval < filter->interval)
		return NULL;

	circlebuf_pop_front(&filter->video_frames, NULL,
			    sizeof(struct obs_source_frame *));

	if (!filter->video_delay_reached)
		filter->video_delay_reached = true;

	return output;
}

/* NOTE: Delaying audio shouldn't be necessary because the audio subsystem will
 * automatically sync audio to video frames */

/* #define DELAY_AUDIO */

#ifdef DELAY_AUDIO
static struct obs_audio_data *
async_delay_filter_audio(void *data, struct obs_audio_data *audio)
{
	struct async_delay_data *filter = data;
	struct obs_audio_data cached = *audio;
	uint64_t cur_interval;
	uint64_t duration;
	uint64_t end_ts;

	if (filter->reset_audio ||
	    is_timestamp_jump(audio->timestamp, filter->last_audio_ts)) {
		free_audio_data(filter);
		filter->audio_delay_reached = false;
		filter->reset_audio = false;
	}

	filter->last_audio_ts = audio->timestamp;

	duration =
		util_mul_div64(audio->frames, SEC_TO_NSEC, filter->samplerate);
	end_ts = audio->timestamp + duration;

	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		if (!audio->data[i])
			break;

		cached.data[i] =
			bmemdup(audio->data[i], audio->frames * sizeof(float));
	}

	free_audio_packet(&filter->audio_output);

	circlebuf_push_back(&filter->audio_frames, &cached, sizeof(cached));
	circlebuf_peek_front(&filter->audio_frames, &cached, sizeof(cached));

	cur_interval = end_ts - cached.timestamp;
	if (!filter->audio_delay_reached && cur_interval < filter->interval)
		return NULL;

	circlebuf_pop_front(&filter->audio_frames, NULL, sizeof(cached));
	memcpy(&filter->audio_output, &cached, sizeof(cached));

	if (!filter->audio_delay_reached)
		filter->audio_delay_reached = true;

	return &filter->audio_output;
}
#endif

struct obs_source_info async_delay_filter = {
	.id = "async_delay_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC,
	.get_name = async_delay_filter_name,
	.create = async_delay_filter_create,
	.destroy = async_delay_filter_destroy,
	.update = async_delay_filter_update,
	.get_properties = async_delay_filter_properties,
	.filter_video = async_delay_filter_video,
#ifdef DELAY_AUDIO
	.filter_audio = async_delay_filter_audio,
#endif
	.filter_remove = async_delay_filter_remove,
};
