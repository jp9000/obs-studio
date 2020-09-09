/*
 * Copyright (c) 2017 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <obs.h>
#include "decode.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <util/threading.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef void (*mp_video_cb)(void *opaque, struct obs_source_frame *frame);
typedef void (*mp_audio_cb)(void *opaque, struct obs_source_audio *audio);
typedef void (*mp_stop_cb)(void *opaque);

typedef struct mp_media_queue_node mp_media_queue_node_t;
typedef struct mp_media_state_message mp_media_state_message_t;
typedef struct mp_media_queue mp_media_queue_t;
typedef struct mp_media mp_media_t;

typedef bool (*mp_state_handler)(mp_media_t *m);

/* Used internally by mp_media to track current state.
 * to add new event:
 * 1. Declare event in mp_media_state enumerator
 * 2. Define event handler function.
 * 3. Register event handler reference to queue.
 * 4. Register any transition actions to the state change method.
 * Then use an invoke function to push the message to queue.
 * Note: States must be a power of 2 in order to map properly to
 * the flag table.
 */
enum mp_media_state {
	mstate_uninit = 1,
	mstate_playing = 2,
	mstate_paused = 4,
	mstate_resume = 8,
	mstate_seeking = 16,
	mstate_resetting = 32,
	mstate_setloop = 64,
	mstate_setreconnect = 128,
	mstate_stopping = 256,
	mstate_stopped = 512,
	mstate_killing = 1024
};

/* Queued messages from event handlers to change media state. */
struct mp_media_state_message {
	/* Function for media loop to process. */
	mp_state_handler state_handler;
	/* State representation of the invoked function.
	 * Generally, this is the state the media will change to.
	 */
	enum mp_media_state state;
};

/* Queue node. */
 struct mp_media_queue_node {
	/* The message object. */
	mp_media_state_message_t message;
	/* Pointer to the next message in queue. */
	mp_media_queue_node_t *next;
};

/* Standard queue object, EXCEPT we include a bit flag integer.
 * The flag allows us to ensure any given message only appears once.
 */
struct mp_media_queue {
	mp_media_queue_node_t *first;
	mp_media_queue_node_t *last;
	int flags;
};

struct mp_media {
	AVFormatContext *fmt;

	mp_video_cb v_preload_cb;
	mp_video_cb v_seek_cb;
	mp_stop_cb stop_cb;
	mp_video_cb v_cb;
	mp_audio_cb a_cb;
	void *opaque;

	char *path;
	char *format_name;
	int buffering;
	int speed;

	enum AVPixelFormat scale_format;
	struct SwsContext *swscale;
	int scale_linesizes[4];
	uint8_t *scale_pic[4];

	struct mp_decode v;
	struct mp_decode a;
	bool is_local_file;
	bool reconnecting;
	bool has_video;
	bool has_audio;
	bool is_file;
	bool eof;
	bool hw;

	struct obs_source_frame obsframe;
	enum video_colorspace cur_space;
	enum video_range_type cur_range;
	enum video_range_type force_range;

	int64_t play_sys_ts;
	int64_t next_pts_ns;
	uint64_t next_ns;
	int64_t start_ts;
	int64_t base_ts;

	uint64_t interrupt_poll_ts;

	pthread_mutex_t mutex;
	os_sem_t *sem;
	bool looping;
	bool killing;

	enum mp_media_state mp_state;
	mp_media_queue_t mp_queue;

	bool thread_valid;
	pthread_t thread;

	bool seek_next_ts;
	int64_t seek_pos;
};

struct mp_media_info {
	void *opaque;

	mp_video_cb v_cb;
	mp_video_cb v_preload_cb;
	mp_video_cb v_seek_cb;
	mp_audio_cb a_cb;
	mp_stop_cb stop_cb;

	const char *path;
	const char *format;
	int buffering;
	int speed;
	enum video_range_type force_range;
	bool hardware_decoding;
	bool is_local_file;
	bool reconnecting;
};

extern bool mp_media_init(mp_media_t *media, const struct mp_media_info *info);
extern void mp_media_free(mp_media_t *media);
extern void mp_media_play(mp_media_t *media, bool loop, bool reconnecting);
extern void mp_media_stop(mp_media_t *media);
extern void mp_media_play_pause(mp_media_t *media, bool pause);
extern void mp_media_seek_to(mp_media_t *m, int64_t pos);

extern int64_t mp_get_current_time(mp_media_t *m);

static bool mp_media_push_message(mp_media_t *m, enum mp_media_state state);
static bool mp_media_pop_message(mp_media_t *m);

/* #define DETAILED_DEBUG_INFO */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
#define USE_NEW_FFMPEG_DECODE_API
#endif

#ifdef __cplusplus
}
#endif
