#include "vlc-video-plugin.h"
#include <media-io/video-frame.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/dstr.h>

#define do_log(level, format, ...)                \
	blog(level, "[vlc_source: '%s'] " format, \
	     obs_source_get_name(ss->source), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

/* clang-format off */

#define S_PLAYLIST                     "playlist"
#define S_LOOP                         "loop"
#define S_SHUFFLE                      "shuffle"
#define S_BEHAVIOR                     "playback_behavior"
#define S_BEHAVIOR_STOP_RESTART        "stop_restart"
#define S_BEHAVIOR_PAUSE_UNPAUSE       "pause_unpause"
#define S_BEHAVIOR_ALWAYS_PLAY         "always_play"
#define S_NETWORK_CACHING              "network_caching"
#define S_TRACK                        "track"
#define S_SUBTITLE_ENABLE              "subtitle_enable"
#define S_SUBTITLE_TRACK               "subtitle"

#define T_(text) obs_module_text(text)
#define T_PLAYLIST                     T_("Playlist")
#define T_LOOP                         T_("LoopPlaylist")
#define T_SHUFFLE                      T_("shuffle")
#define T_BEHAVIOR                     T_("PlaybackBehavior")
#define T_BEHAVIOR_STOP_RESTART        T_("PlaybackBehavior.StopRestart")
#define T_BEHAVIOR_PAUSE_UNPAUSE       T_("PlaybackBehavior.PauseUnpause")
#define T_BEHAVIOR_ALWAYS_PLAY         T_("PlaybackBehavior.AlwaysPlay")
#define T_NETWORK_CACHING              T_("NetworkCaching")
#define T_TRACK                        T_("AudioTrack")
#define T_SUBTITLE_ENABLE              T_("SubtitleEnable")
#define T_SUBTITLE_TRACK               T_("SubtitleTrack")

/* clang-format on */

/* ------------------------------------------------------------------------- */

struct media_file_data {
	char *path;
	libvlc_media_t *media;
};

enum behavior {
	BEHAVIOR_STOP_RESTART,
	BEHAVIOR_PAUSE_UNPAUSE,
	BEHAVIOR_ALWAYS_PLAY,
};

struct vlc_source {
	obs_source_t *source;

	libvlc_media_player_t *media_player;
	libvlc_media_list_player_t *media_list_player;

	struct obs_source_frame frame;
	struct obs_source_audio audio;
	size_t audio_capacity;

	pthread_mutex_t mutex;
	DARRAY(struct media_file_data) files;
	enum behavior behavior;
	bool loop;
	bool shuffle;

	obs_hotkey_id play_pause_hotkey;
	obs_hotkey_id restart_hotkey;
	obs_hotkey_id stop_hotkey;
	obs_hotkey_id playlist_next_hotkey;
	obs_hotkey_id playlist_prev_hotkey;
};

static libvlc_media_t *get_media(struct darray *array, const char *path)
{
	DARRAY(struct media_file_data) files;
	libvlc_media_t *media = NULL;

	files.da = *array;

	for (size_t i = 0; i < files.num; i++) {
		const char *cur_path = files.array[i].path;

		if (strcmp(path, cur_path) == 0) {
			media = files.array[i].media;
			libvlc_media_retain_(media);
			break;
		}
	}

	return media;
}

static inline libvlc_media_t *create_media_from_file(const char *file)
{
	return (file && strstr(file, "://") != NULL)
		       ? libvlc_media_new_location_(libvlc, file)
		       : libvlc_media_new_path_(libvlc, file);
}

static void free_files(struct darray *array)
{
	DARRAY(struct media_file_data) files;
	files.da = *array;

	for (size_t i = 0; i < files.num; i++) {
		bfree(files.array[i].path);
		libvlc_media_release_(files.array[i].media);
	}

	da_free(files);
}

#define MAKEFORMAT(ch0, ch1, ch2, ch3)                                \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
	 ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))

static inline bool chroma_is(const char *chroma, const char *val)
{
	return *(uint32_t *)chroma == *(uint32_t *)val;
}

static enum video_format convert_vlc_video_format(char *chroma, bool *full)
{
	*full = false;

#define CHROMA_TEST(val, ret)       \
	if (chroma_is(chroma, val)) \
	return ret
#define CHROMA_CONV(val, new_val, ret)                           \
	do {                                                     \
		if (chroma_is(chroma, val)) {                    \
			*(uint32_t *)chroma = (uint32_t)new_val; \
			return ret;                              \
		}                                                \
	} while (false)
#define CHROMA_CONV_FULL(val, new_val, ret)     \
	do {                                    \
		*full = true;                   \
		CHROMA_CONV(val, new_val, ret); \
	} while (false)

	CHROMA_TEST("RGBA", VIDEO_FORMAT_RGBA);
	CHROMA_TEST("BGRA", VIDEO_FORMAT_BGRA);

	/* 4:2:0 formats */
	CHROMA_TEST("NV12", VIDEO_FORMAT_NV12);
	CHROMA_TEST("I420", VIDEO_FORMAT_I420);
	CHROMA_TEST("IYUV", VIDEO_FORMAT_I420);
	CHROMA_CONV("NV21", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_NV12);
	CHROMA_CONV("I422", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_NV12);
	CHROMA_CONV("Y42B", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_NV12);
	CHROMA_CONV("YV12", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_NV12);
	CHROMA_CONV("yv12", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_NV12);

	CHROMA_CONV_FULL("J420", MAKEFORMAT('J', '4', '2', '0'),
			 VIDEO_FORMAT_I420);

	/* 4:2:2 formats */
	CHROMA_TEST("UYVY", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("UYNV", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("UYNY", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("Y422", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("HDYC", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("AVUI", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("uyv1", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("2vuy", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("2Vuy", VIDEO_FORMAT_UYVY);
	CHROMA_TEST("2Vu1", VIDEO_FORMAT_UYVY);

	CHROMA_TEST("YUY2", VIDEO_FORMAT_YUY2);
	CHROMA_TEST("YUYV", VIDEO_FORMAT_YUY2);
	CHROMA_TEST("YUNV", VIDEO_FORMAT_YUY2);
	CHROMA_TEST("V422", VIDEO_FORMAT_YUY2);

	CHROMA_TEST("YVYU", VIDEO_FORMAT_YVYU);

	CHROMA_CONV("v210", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("cyuv", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("CYUV", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("VYUY", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("NV16", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("NV61", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("I410", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("I422", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("Y42B", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("J422", MAKEFORMAT('U', 'Y', 'V', 'Y'), VIDEO_FORMAT_UYVY);

	/* 4:4:4 formats */
	CHROMA_TEST("I444", VIDEO_FORMAT_I444);
	CHROMA_CONV_FULL("J444", MAKEFORMAT('R', 'G', 'B', 'A'),
			 VIDEO_FORMAT_RGBA);
	CHROMA_CONV("YUVA", MAKEFORMAT('R', 'G', 'B', 'A'), VIDEO_FORMAT_RGBA);

	/* 4:4:0 formats */
	CHROMA_CONV("I440", MAKEFORMAT('I', '4', '4', '4'), VIDEO_FORMAT_I444);
	CHROMA_CONV("J440", MAKEFORMAT('I', '4', '4', '4'), VIDEO_FORMAT_I444);

	/* 4:1:0 formats */
	CHROMA_CONV("YVU9", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("I410", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_UYVY);

	/* 4:1:1 formats */
	CHROMA_CONV("I411", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_UYVY);
	CHROMA_CONV("Y41B", MAKEFORMAT('N', 'V', '1', '2'), VIDEO_FORMAT_UYVY);

	/* greyscale formats */
	CHROMA_TEST("GREY", VIDEO_FORMAT_Y800);
	CHROMA_TEST("Y800", VIDEO_FORMAT_Y800);
	CHROMA_TEST("Y8  ", VIDEO_FORMAT_Y800);
#undef CHROMA_CONV_FULL
#undef CHROMA_CONV
#undef CHROMA_TEST

	*(uint32_t *)chroma = (uint32_t)MAKEFORMAT('B', 'G', 'R', 'A');
	return VIDEO_FORMAT_BGRA;
}

static inline unsigned get_format_lines(enum video_format format,
					unsigned height, size_t plane)
{
	switch (format) {
	case VIDEO_FORMAT_I420:
	case VIDEO_FORMAT_NV12:
		return (plane == 0) ? height : height / 2;
	case VIDEO_FORMAT_YVYU:
	case VIDEO_FORMAT_YUY2:
	case VIDEO_FORMAT_UYVY:
	case VIDEO_FORMAT_I444:
	case VIDEO_FORMAT_RGBA:
	case VIDEO_FORMAT_BGRA:
	case VIDEO_FORMAT_BGRX:
	case VIDEO_FORMAT_Y800:
		return height;
	case VIDEO_FORMAT_NONE:
	default:
		break;
	}

	return 0;
}

static enum audio_format convert_vlc_audio_format(char *format)
{
#define AUDIO_TEST(val, ret)        \
	if (chroma_is(format, val)) \
	return ret
#define AUDIO_CONV(val, new_val, ret)                            \
	do {                                                     \
		if (chroma_is(format, val)) {                    \
			*(uint32_t *)format = (uint32_t)new_val; \
			return ret;                              \
		}                                                \
	} while (false)

	AUDIO_TEST("S16N", AUDIO_FORMAT_16BIT);
	AUDIO_TEST("S32N", AUDIO_FORMAT_32BIT);
	AUDIO_TEST("FL32", AUDIO_FORMAT_FLOAT);

	AUDIO_CONV("U16N", MAKEFORMAT('S', '1', '6', 'N'), AUDIO_FORMAT_16BIT);
	AUDIO_CONV("U32N", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("S24N", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("U24N", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("FL64", MAKEFORMAT('F', 'L', '3', '2'), AUDIO_FORMAT_FLOAT);

	AUDIO_CONV("S16I", MAKEFORMAT('S', '1', '6', 'N'), AUDIO_FORMAT_16BIT);
	AUDIO_CONV("U16I", MAKEFORMAT('S', '1', '6', 'N'), AUDIO_FORMAT_16BIT);
	AUDIO_CONV("S24I", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("U24I", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("S32I", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
	AUDIO_CONV("U32I", MAKEFORMAT('S', '3', '2', 'N'), AUDIO_FORMAT_32BIT);
#undef AUDIO_CONV
#undef AUDIO_TEST

	*(uint32_t *)format = (uint32_t)MAKEFORMAT('F', 'L', '3', '2');
	return AUDIO_FORMAT_FLOAT;
}

/* ------------------------------------------------------------------------- */

static void vlcs_get_metadata(void *data, calldata_t *cd)
{
	struct vlc_source *vlcs = data;
	const char *data_id = calldata_string(cd, "tag_id");

	if (!vlcs || !data_id)
		return;
	libvlc_media_t *media =
		libvlc_media_player_get_media_(vlcs->media_player);

	if (!media)
		return;

#define VLC_META(media, cd, did, tid, tag)                               \
	else if (strcmp(did, tid) == 0)                                  \
	{                                                                \
		calldata_set_string(cd, "tag_data",                      \
				    libvlc_media_get_meta_(media, tag)); \
	}

	if (strcmp(data_id, "title") == 0)
		calldata_set_string(cd, "tag_data",
				    libvlc_media_get_meta_(media,
							   libvlc_meta_Title));

	VLC_META(media, cd, data_id, "artist", libvlc_meta_Artist)
	VLC_META(media, cd, data_id, "genre", libvlc_meta_Genre)
	VLC_META(media, cd, data_id, "copyright", libvlc_meta_Copyright)
	VLC_META(media, cd, data_id, "album", libvlc_meta_Album)
	VLC_META(media, cd, data_id, "track_number", libvlc_meta_TrackNumber)
	VLC_META(media, cd, data_id, "description", libvlc_meta_Description)
	VLC_META(media, cd, data_id, "rating", libvlc_meta_Rating)
	VLC_META(media, cd, data_id, "date", libvlc_meta_Date)
	VLC_META(media, cd, data_id, "setting", libvlc_meta_Setting)
	VLC_META(media, cd, data_id, "url", libvlc_meta_URL)
	VLC_META(media, cd, data_id, "language", libvlc_meta_Language)
	VLC_META(media, cd, data_id, "now_playing", libvlc_meta_NowPlaying)
	VLC_META(media, cd, data_id, "publisher", libvlc_meta_Publisher)
	VLC_META(media, cd, data_id, "encoded_by", libvlc_meta_EncodedBy)
	VLC_META(media, cd, data_id, "artwork_url", libvlc_meta_ArtworkURL)
	VLC_META(media, cd, data_id, "track_id", libvlc_meta_TrackID)
	VLC_META(media, cd, data_id, "track_total", libvlc_meta_TrackTotal)
	VLC_META(media, cd, data_id, "director", libvlc_meta_Director)
	VLC_META(media, cd, data_id, "season", libvlc_meta_Season)
	VLC_META(media, cd, data_id, "episode", libvlc_meta_Episode)
	VLC_META(media, cd, data_id, "show_name", libvlc_meta_ShowName)
	VLC_META(media, cd, data_id, "actors", libvlc_meta_Actors)
	VLC_META(media, cd, data_id, "album_artist", libvlc_meta_AlbumArtist)
	VLC_META(media, cd, data_id, "disc_number", libvlc_meta_DiscNumber)
	VLC_META(media, cd, data_id, "disc_total", libvlc_meta_DiscTotal)

	libvlc_media_release_(media);
#undef VLC_META
}

/* ------------------------------------------------------------------------- */

static const char *vlcs_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("VLCSource");
}

static void vlcs_destroy(void *data)
{
	struct vlc_source *c = data;

	if (c->media_list_player) {
		libvlc_media_list_player_stop_(c->media_list_player);
		libvlc_media_list_player_release_(c->media_list_player);
	}
	if (c->media_player) {
		libvlc_media_player_release_(c->media_player);
	}

	bfree((void *)c->audio.data[0]);
	obs_source_frame_free(&c->frame);

	free_files(&c->files.da);
	pthread_mutex_destroy(&c->mutex);
	bfree(c);
}

static void *vlcs_video_lock(void *data, void **planes)
{
	struct vlc_source *c = data;
	for (size_t i = 0; i < MAX_AV_PLANES && c->frame.data[i] != NULL; i++)
		planes[i] = c->frame.data[i];
	return NULL;
}

static void vlcs_video_display(void *data, void *picture)
{
	struct vlc_source *c = data;
	c->frame.timestamp = (uint64_t)libvlc_clock_() * 1000ULL - time_start;
	obs_source_output_video(c->source, &c->frame);

	UNUSED_PARAMETER(picture);
}

static unsigned vlcs_video_format(void **p_data, char *chroma, unsigned *width,
				  unsigned *height, unsigned *pitches,
				  unsigned *lines)
{
	struct vlc_source *c = *p_data;
	enum video_format new_format;
	enum video_range_type range;
	bool new_range;
	unsigned new_width = 0;
	unsigned new_height = 0;
	size_t i = 0;

	new_format = convert_vlc_video_format(chroma, &new_range);

	/* This is used because VLC will by default try to use a different
	 * scaling than what the file uses (probably for optimization reasons).
	 * For example, if the file is 1920x1080, it will try to render it by
	 * 1920x1088, which isn't what we want.  Calling libvlc_video_get_size
	 * gets the actual video file's size, and thus fixes the problem.
	 * However this doesn't work with URLs, so if it returns a 0 value, it
	 * shouldn't be used. */
	libvlc_video_get_size_(c->media_player, 0, &new_width, &new_height);

	if (new_width && new_height) {
		*width = new_width;
		*height = new_height;
	}

	/* don't allocate a new frame if format/width/height hasn't changed */
	if (c->frame.format != new_format || c->frame.width != *width ||
	    c->frame.height != *height) {
		obs_source_frame_free(&c->frame);
		obs_source_frame_init(&c->frame, new_format, *width, *height);

		c->frame.format = new_format;
		c->frame.full_range = new_range;
		range = c->frame.full_range ? VIDEO_RANGE_FULL
					    : VIDEO_RANGE_PARTIAL;
		video_format_get_parameters(VIDEO_CS_DEFAULT, range,
					    c->frame.color_matrix,
					    c->frame.color_range_min,
					    c->frame.color_range_max);
	}

	while (c->frame.data[i]) {
		pitches[i] = (unsigned)c->frame.linesize[i];
		lines[i] = get_format_lines(c->frame.format, *height, i);
		i++;
	}

	return 1;
}

static void vlcs_audio_play(void *data, const void *samples, unsigned count,
			    int64_t pts)
{
	struct vlc_source *c = data;
	size_t size = get_audio_size(c->audio.format, c->audio.speakers, count);

	if (c->audio_capacity < count) {
		c->audio.data[0] = brealloc((void *)c->audio.data[0], size);
		c->audio_capacity = count;
	}

	memcpy((void *)c->audio.data[0], samples, size);
	c->audio.timestamp = (uint64_t)pts * 1000ULL - time_start;
	c->audio.frames = count;

	obs_source_output_audio(c->source, &c->audio);
}

static int vlcs_audio_setup(void **p_data, char *format, unsigned *rate,
			    unsigned *channels)
{
	struct vlc_source *c = *p_data;
	enum audio_format new_audio_format;

	new_audio_format = convert_vlc_audio_format(format);
	if (*channels > 2)
		*channels = 2;

	/* don't free audio data if the data is the same format */
	if (c->audio.format == new_audio_format &&
	    c->audio.samples_per_sec == *rate &&
	    c->audio.speakers == (enum speaker_layout) * channels)
		return 0;

	c->audio_capacity = 0;
	bfree((void *)c->audio.data[0]);

	memset(&c->audio, 0, sizeof(c->audio));
	c->audio.speakers = (enum speaker_layout) * channels;
	c->audio.samples_per_sec = *rate;
	c->audio.format = new_audio_format;
	return 0;
}

static void add_file(struct vlc_source *c, struct darray *array,
		     const char *path, int network_caching, int track_index,
		     int subtitle_index, bool subtitle_enable)
{
	DARRAY(struct media_file_data) new_files;
	struct media_file_data data;
	struct dstr new_path = {0};
	libvlc_media_t *new_media;
	bool is_url = path && strstr(path, "://") != NULL;

	new_files.da = *array;

	dstr_copy(&new_path, path);
#ifdef _WIN32
	if (!is_url)
		dstr_replace(&new_path, "/", "\\");
#endif
	path = new_path.array;

	new_media = get_media(&c->files.da, path);

	if (!new_media)
		new_media = get_media(&new_files.da, path);
	if (!new_media)
		new_media = create_media_from_file(path);

	if (new_media) {
		if (is_url) {
			struct dstr network_caching_option = {0};
			dstr_catf(&network_caching_option,
				  ":network-caching=%d", network_caching);
			libvlc_media_add_option_(new_media,
						 network_caching_option.array);
			dstr_free(&network_caching_option);
		}
		struct dstr track_option = {0};
		dstr_catf(&track_option, ":audio-track=%d", track_index - 1);
		libvlc_media_add_option_(new_media, track_option.array);
		dstr_free(&track_option);

		struct dstr sub_option = {0};
		if (subtitle_enable) {
			dstr_catf(&sub_option, ":sub-track=%d",
				  subtitle_index - 1);
		}
		libvlc_media_add_option_(new_media, sub_option.array);
		dstr_free(&sub_option);

		data.path = new_path.array;
		data.media = new_media;
		da_push_back(new_files, &data);
	} else {
		dstr_free(&new_path);
	}

	*array = new_files.da;
}

static bool valid_extension(const char *ext)
{
	struct dstr test = {0};
	bool valid = false;
	const char *b;
	const char *e;

	if (!ext || !*ext)
		return false;

	b = EXTENSIONS_MEDIA + 1;
	e = strchr(b, ';');

	for (;;) {
		if (e)
			dstr_ncopy(&test, b, e - b);
		else
			dstr_copy(&test, b);

		if (dstr_cmpi(&test, ext) == 0) {
			valid = true;
			break;
		}

		if (!e)
			break;

		b = e + 2;
		e = strchr(b, ';');
	}

	dstr_free(&test);
	return valid;
}

static void vlcs_update(void *data, obs_data_t *settings)
{
	DARRAY(struct media_file_data) new_files;
	DARRAY(struct media_file_data) old_files;
	libvlc_media_list_t *media_list;
	struct vlc_source *c = data;
	obs_data_array_t *array;
	const char *behavior;
	size_t count;
	int network_caching;
	int track_index;
	int subtitle_index;
	bool subtitle_enable;

	da_init(new_files);
	da_init(old_files);

	array = obs_data_get_array(settings, S_PLAYLIST);
	count = obs_data_array_count(array);

	c->loop = obs_data_get_bool(settings, S_LOOP);

	behavior = obs_data_get_string(settings, S_BEHAVIOR);

	network_caching = (int)obs_data_get_int(settings, S_NETWORK_CACHING);

	track_index = (int)obs_data_get_int(settings, S_TRACK);

	subtitle_index = (int)obs_data_get_int(settings, S_SUBTITLE_TRACK);

	subtitle_enable = obs_data_get_bool(settings, S_SUBTITLE_ENABLE);

	if (astrcmpi(behavior, S_BEHAVIOR_PAUSE_UNPAUSE) == 0) {
		c->behavior = BEHAVIOR_PAUSE_UNPAUSE;
	} else if (astrcmpi(behavior, S_BEHAVIOR_ALWAYS_PLAY) == 0) {
		c->behavior = BEHAVIOR_ALWAYS_PLAY;
	} else { /* S_BEHAVIOR_STOP_RESTART */
		c->behavior = BEHAVIOR_STOP_RESTART;
	}

	/* ------------------------------------- */
	/* create new list of sources */

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(item, "value");
		os_dir_t *dir = os_opendir(path);

		if (dir) {
			struct dstr dir_path = {0};
			struct os_dirent *ent;

			for (;;) {
				const char *ext;

				ent = os_readdir(dir);
				if (!ent)
					break;
				if (ent->directory)
					continue;

				ext = os_get_path_extension(ent->d_name);
				if (!valid_extension(ext))
					continue;

				dstr_copy(&dir_path, path);
				dstr_cat_ch(&dir_path, '/');
				dstr_cat(&dir_path, ent->d_name);
				add_file(c, &new_files.da, dir_path.array,
					 network_caching, track_index,
					 subtitle_index, subtitle_enable);
			}

			dstr_free(&dir_path);
			os_closedir(dir);
		} else {
			add_file(c, &new_files.da, path, network_caching,
				 track_index, subtitle_index, subtitle_enable);
		}

		obs_data_release(item);
	}

	/* ------------------------------------- */
	/* update settings data */

	libvlc_media_list_player_stop_(c->media_list_player);

	pthread_mutex_lock(&c->mutex);
	old_files.da = c->files.da;
	c->files.da = new_files.da;
	pthread_mutex_unlock(&c->mutex);

	/* ------------------------------------- */
	/* shuffle playlist */

	c->shuffle = obs_data_get_bool(settings, S_SHUFFLE);

	if (c->files.num > 1 && c->shuffle) {
		DARRAY(struct media_file_data) new_files;
		DARRAY(size_t) idxs;

		da_init(new_files);
		da_init(idxs);
		da_resize(idxs, c->files.num);
		da_reserve(new_files, c->files.num);

		for (size_t i = 0; i < c->files.num; i++) {
			idxs.array[i] = i;
		}
		for (size_t i = idxs.num; i > 0; i--) {
			size_t val = rand() % i;
			size_t idx = idxs.array[val];
			da_push_back(new_files, &c->files.array[idx]);
			da_erase(idxs, val);
		}

		da_free(c->files);
		da_free(idxs);
		c->files.da = new_files.da;
	}

	/* ------------------------------------- */
	/* clean up and restart playback */

	free_files(&old_files.da);

	media_list = libvlc_media_list_new_(libvlc);

	libvlc_media_list_lock_(media_list);
	for (size_t i = 0; i < c->files.num; i++)
		libvlc_media_list_add_media_(media_list,
					     c->files.array[i].media);
	libvlc_media_list_unlock_(media_list);

	libvlc_media_list_player_set_media_list_(c->media_list_player,
						 media_list);
	libvlc_media_list_release_(media_list);

	libvlc_media_list_player_set_playback_mode_(
		c->media_list_player, c->loop ? libvlc_playback_mode_loop
					      : libvlc_playback_mode_default);

	if (c->files.num && (c->behavior == BEHAVIOR_ALWAYS_PLAY ||
			     obs_source_active(c->source)))
		libvlc_media_list_player_play_(c->media_list_player);
	else
		obs_source_output_video(c->source, NULL);

	obs_data_array_release(array);
}

static void vlcs_started(const struct libvlc_event_t *event, void *data)
{
	struct vlc_source *c = data;
	obs_source_media_started(c->source);

	UNUSED_PARAMETER(event);
}

static void vlcs_stopped(const struct libvlc_event_t *event, void *data)
{
	struct vlc_source *c = data;
	if (!c->loop) {
		obs_source_output_video(c->source, NULL);
		obs_source_media_ended(c->source);
	}

	UNUSED_PARAMETER(event);
}

static enum obs_media_state vlcs_get_state(void *data)
{
	struct vlc_source *c = data;

	libvlc_state_t state = libvlc_media_player_get_state_(c->media_player);

	switch (state) {
	case libvlc_NothingSpecial:
		return OBS_MEDIA_STATE_NONE;
	case libvlc_Opening:
		return OBS_MEDIA_STATE_OPENING;
	case libvlc_Buffering:
		return OBS_MEDIA_STATE_BUFFERING;
	case libvlc_Playing:
		return OBS_MEDIA_STATE_PLAYING;
	case libvlc_Paused:
		return OBS_MEDIA_STATE_PAUSED;
	case libvlc_Stopped:
		return OBS_MEDIA_STATE_STOPPED;
	case libvlc_Ended:
		return OBS_MEDIA_STATE_ENDED;
	case libvlc_Error:
		return OBS_MEDIA_STATE_ERROR;
	}

	return 0;
}

static void vlcs_play_pause(void *data, bool pause)
{
	struct vlc_source *c = data;

	if (pause)
		libvlc_media_list_player_pause_(c->media_list_player);
	else
		libvlc_media_list_player_play_(c->media_list_player);
}

static void vlcs_restart(void *data)
{
	struct vlc_source *c = data;

	libvlc_media_list_player_stop_(c->media_list_player);
	libvlc_media_list_player_play_(c->media_list_player);
}

static void vlcs_stop(void *data)
{
	struct vlc_source *c = data;

	libvlc_media_list_player_stop_(c->media_list_player);
	obs_source_output_video(c->source, NULL);
}

static void vlcs_playlist_next(void *data)
{
	struct vlc_source *c = data;

	libvlc_media_list_player_next_(c->media_list_player);
}

static void vlcs_playlist_prev(void *data)
{
	struct vlc_source *c = data;

	libvlc_media_list_player_previous_(c->media_list_player);
}

static int64_t vlcs_get_duration(void *data)
{
	struct vlc_source *c = data;

	return (int64_t)libvlc_media_player_get_length_(c->media_player);
}

static int64_t vlcs_get_time(void *data)
{
	struct vlc_source *c = data;

	return (int64_t)libvlc_media_player_get_time_(c->media_player);
}

static void vlcs_set_time(void *data, int64_t ms)
{
	struct vlc_source *c = data;

	libvlc_media_player_set_time_(c->media_player, (libvlc_time_t)ms);
}

static void vlcs_play_pause_hotkey(void *data, obs_hotkey_id id,
				   obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct vlc_source *c = data;

	enum obs_media_state state = obs_source_media_get_state(c->source);

	if (pressed && obs_source_showing(c->source)) {
		if (state == OBS_MEDIA_STATE_PLAYING)
			obs_source_media_play_pause(c->source, true);
		else if (state == OBS_MEDIA_STATE_PAUSED)
			obs_source_media_play_pause(c->source, false);
	}
}

static void vlcs_restart_hotkey(void *data, obs_hotkey_id id,
				obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct vlc_source *c = data;

	if (pressed && obs_source_showing(c->source))
		obs_source_media_restart(c->source);
}

static void vlcs_stop_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			     bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct vlc_source *c = data;

	if (pressed && obs_source_showing(c->source))
		obs_source_media_stop(c->source);
}

static void vlcs_playlist_next_hotkey(void *data, obs_hotkey_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct vlc_source *c = data;

	if (pressed && obs_source_showing(c->source))
		obs_source_media_next(c->source);
}

static void vlcs_playlist_prev_hotkey(void *data, obs_hotkey_id id,
				      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct vlc_source *c = data;

	if (pressed && obs_source_showing(c->source))
		obs_source_media_previous(c->source);
}

static void *vlcs_create(obs_data_t *settings, obs_source_t *source)
{
	struct vlc_source *c = bzalloc(sizeof(*c));
	c->source = source;

	c->play_pause_hotkey = obs_hotkey_register_source(
		source, "VLCSource.PlayPause", obs_module_text("PlayPause"),
		vlcs_play_pause_hotkey, c);

	c->restart_hotkey = obs_hotkey_register_source(
		source, "VLCSource.Restart", obs_module_text("Restart"),
		vlcs_restart_hotkey, c);

	c->stop_hotkey = obs_hotkey_register_source(source, "VLCSource.Stop",
						    obs_module_text("Stop"),
						    vlcs_stop_hotkey, c);

	c->playlist_next_hotkey = obs_hotkey_register_source(
		source, "VLCSource.PlaylistNext",
		obs_module_text("PlaylistNext"), vlcs_playlist_next_hotkey, c);

	c->playlist_prev_hotkey = obs_hotkey_register_source(
		source, "VLCSource.PlaylistPrev",
		obs_module_text("PlaylistPrev"), vlcs_playlist_prev_hotkey, c);

	pthread_mutex_init_value(&c->mutex);
	if (pthread_mutex_init(&c->mutex, NULL) != 0)
		goto error;

	if (!load_libvlc())
		goto error;

	c->media_list_player = libvlc_media_list_player_new_(libvlc);
	if (!c->media_list_player)
		goto error;

	c->media_player = libvlc_media_player_new_(libvlc);
	if (!c->media_player)
		goto error;

	libvlc_media_list_player_set_media_player_(c->media_list_player,
						   c->media_player);

	libvlc_video_set_callbacks_(c->media_player, vlcs_video_lock, NULL,
				    vlcs_video_display, c);
	libvlc_video_set_format_callbacks_(c->media_player, vlcs_video_format,
					   NULL);

	libvlc_audio_set_callbacks_(c->media_player, vlcs_audio_play, NULL,
				    NULL, NULL, NULL, c);
	libvlc_audio_set_format_callbacks_(c->media_player, vlcs_audio_setup,
					   NULL);

	libvlc_event_manager_t *event_manager;
	event_manager = libvlc_media_player_event_manager_(c->media_player);
	libvlc_event_attach_(event_manager, libvlc_MediaPlayerEndReached,
			     vlcs_stopped, c);
	libvlc_event_attach_(event_manager, libvlc_MediaPlayerOpening,
			     vlcs_started, c);

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	proc_handler_add(
		ph, "void get_metadata(in string tag_id out string tag_data)",
		vlcs_get_metadata, c);

	obs_source_update(source, NULL);

	UNUSED_PARAMETER(settings);
	return c;

error:
	vlcs_destroy(c);
	return NULL;
}

static void vlcs_activate(void *data)
{
	struct vlc_source *c = data;

	if (c->behavior == BEHAVIOR_STOP_RESTART) {
		libvlc_media_list_player_play_(c->media_list_player);

	} else if (c->behavior == BEHAVIOR_PAUSE_UNPAUSE) {
		libvlc_media_list_player_play_(c->media_list_player);
	}
}

static void vlcs_deactivate(void *data)
{
	struct vlc_source *c = data;

	if (c->behavior == BEHAVIOR_STOP_RESTART) {
		libvlc_media_list_player_stop_(c->media_list_player);
		obs_source_output_video(c->source, NULL);

	} else if (c->behavior == BEHAVIOR_PAUSE_UNPAUSE) {
		libvlc_media_list_player_pause_(c->media_list_player);
	}
}

static void vlcs_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_LOOP, true);
	obs_data_set_default_bool(settings, S_SHUFFLE, false);
	obs_data_set_default_string(settings, S_BEHAVIOR,
				    S_BEHAVIOR_STOP_RESTART);
	obs_data_set_default_int(settings, S_NETWORK_CACHING, 400);
	obs_data_set_default_int(settings, S_TRACK, 1);
	obs_data_set_default_bool(settings, S_SUBTITLE_ENABLE, false);
	obs_data_set_default_int(settings, S_SUBTITLE_TRACK, 1);
}

static obs_properties_t *vlcs_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	struct vlc_source *c = data;
	struct dstr filter = {0};
	struct dstr exts = {0};
	struct dstr path = {0};
	obs_property_t *p;

	obs_properties_set_flags(ppts, OBS_PROPERTIES_DEFER_UPDATE);
	obs_properties_add_bool(ppts, S_LOOP, T_LOOP);
	obs_properties_add_bool(ppts, S_SHUFFLE, T_SHUFFLE);

	if (c) {
		pthread_mutex_lock(&c->mutex);
		if (c->files.num) {
			struct media_file_data *last = da_end(c->files);
			const char *slash;

			dstr_copy(&path, last->path);
			dstr_replace(&path, "\\", "/");
			slash = strrchr(path.array, '/');
			if (slash)
				dstr_resize(&path, slash - path.array + 1);
		}
		pthread_mutex_unlock(&c->mutex);
	}

	p = obs_properties_add_list(ppts, S_BEHAVIOR, T_BEHAVIOR,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_BEHAVIOR_STOP_RESTART,
				     S_BEHAVIOR_STOP_RESTART);
	obs_property_list_add_string(p, T_BEHAVIOR_PAUSE_UNPAUSE,
				     S_BEHAVIOR_PAUSE_UNPAUSE);
	obs_property_list_add_string(p, T_BEHAVIOR_ALWAYS_PLAY,
				     S_BEHAVIOR_ALWAYS_PLAY);

	dstr_cat(&filter, "Media Files (");
	dstr_copy(&exts, EXTENSIONS_MEDIA);
	dstr_replace(&exts, ";", " ");
	dstr_cat_dstr(&filter, &exts);

	dstr_cat(&filter, ");;Video Files (");
	dstr_copy(&exts, EXTENSIONS_VIDEO);
	dstr_replace(&exts, ";", " ");
	dstr_cat_dstr(&filter, &exts);

	dstr_cat(&filter, ");;Audio Files (");
	dstr_copy(&exts, EXTENSIONS_AUDIO);
	dstr_replace(&exts, ";", " ");
	dstr_cat_dstr(&filter, &exts);

	dstr_cat(&filter, ");;Playlist Files (");
	dstr_copy(&exts, EXTENSIONS_PLAYLIST);
	dstr_replace(&exts, ";", " ");
	dstr_cat_dstr(&filter, &exts);
	dstr_cat(&filter, ")");

	obs_properties_add_editable_list(ppts, S_PLAYLIST, T_PLAYLIST,
					 OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS,
					 filter.array, path.array);
	dstr_free(&path);
	dstr_free(&filter);
	dstr_free(&exts);

	obs_properties_add_int(ppts, S_NETWORK_CACHING, T_NETWORK_CACHING, 100,
			       60000, 10);
	obs_properties_add_int(ppts, S_TRACK, T_TRACK, 1, 10, 1);
	obs_properties_add_bool(ppts, S_SUBTITLE_ENABLE, T_SUBTITLE_ENABLE);
	obs_properties_add_int(ppts, S_SUBTITLE_TRACK, T_SUBTITLE_TRACK, 1, 10,
			       1);

	return ppts;
}

static void missing_file_callback(void *src, const char *new_path, void *data)
{
	struct vlc_source *s = src;
	const char *orig_path = data;

	obs_source_t *source = s->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_array_t *files = obs_data_get_array(settings, S_PLAYLIST);

	size_t l = obs_data_array_count(files);
	for (size_t i = 0; i < l; i++) {
		obs_data_t *file = obs_data_array_item(files, i);
		const char *path = obs_data_get_string(file, "value");

		if (strcmp(path, orig_path) == 0) {
			obs_data_set_string(file, "value", new_path);

			obs_data_release(file);
			break;
		}

		obs_data_release(file);
	}

	obs_source_update(source, settings);

	obs_data_array_release(files);
	obs_data_release(settings);
}

static obs_missing_files_t *vlcs_missingfiles(void *data)
{
	struct vlc_source *s = data;
	obs_missing_files_t *missing_files = obs_missing_files_create();

	obs_source_t *source = s->source;
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_array_t *files = obs_data_get_array(settings, S_PLAYLIST);

	size_t l = obs_data_array_count(files);
	for (size_t i = 0; i < l; i++) {
		obs_data_t *item = obs_data_array_item(files, i);
		const char *path = obs_data_get_string(item, "value");

		if (strcmp(path, "") != 0) {
			if (!os_file_exists(path)) {
				obs_missing_file_t *file =
					obs_missing_file_create(
						path, missing_file_callback,
						OBS_MISSING_FILE_SOURCE, source,
						(void *)path);

				obs_missing_files_add_file(missing_files, file);
			}
		}

		obs_data_release(item);
	}

	obs_data_array_release(files);
	obs_data_release(settings);

	return missing_files;
}

struct obs_source_info vlc_source_info = {
	.id = "vlc_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
			OBS_SOURCE_DO_NOT_DUPLICATE |
			OBS_SOURCE_CONTROLLABLE_MEDIA,
	.get_name = vlcs_get_name,
	.create = vlcs_create,
	.destroy = vlcs_destroy,
	.update = vlcs_update,
	.get_defaults = vlcs_defaults,
	.get_properties = vlcs_properties,
	.activate = vlcs_activate,
	.deactivate = vlcs_deactivate,
	.missing_files = vlcs_missingfiles,
	.icon_type = OBS_ICON_TYPE_MEDIA,
	.media_play_pause = vlcs_play_pause,
	.media_restart = vlcs_restart,
	.media_stop = vlcs_stop,
	.media_next = vlcs_playlist_next,
	.media_previous = vlcs_playlist_prev,
	.media_get_duration = vlcs_get_duration,
	.media_get_time = vlcs_get_time,
	.media_set_time = vlcs_set_time,
	.media_get_state = vlcs_get_state,
};
