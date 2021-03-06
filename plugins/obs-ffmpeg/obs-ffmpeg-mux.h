#pragma once

#include <obs-avc.h>
#include <obs-module.h>
#include <obs-hotkey.h>
#include <util/circlebuf.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <util/pipe.h>
#include <util/platform.h>
#include <util/threading.h>

struct ffmpeg_muxer {
	obs_output_t *output;
	os_process_pipe_t *pipe;
	int64_t stop_ts;
	uint64_t total_bytes;
	bool sent_headers;
	volatile bool active;
	volatile bool capturing;
	volatile bool stopping;
	struct dstr path;
	struct dstr printable_path;
	struct dstr muxer_settings;
	struct dstr stream_key;

	/* replay buffer */
	int64_t cur_size;
	int64_t cur_time;
	int64_t max_size;
	int64_t max_time;
	int64_t save_ts;
	int keyframes;
	obs_hotkey_id hotkey;
	volatile bool muxing;
	DARRAY(struct encoder_packet) mux_packets;

	/* these are accessed both by replay buffer and by HLS */
	pthread_t mux_thread;
	bool mux_thread_joinable;
	struct circlebuf packets;

	/* HLS only */
	int keyint_sec;
	pthread_mutex_t write_mutex;
	os_sem_t *write_sem;
	os_event_t *stop_event;
	bool is_hls;
	int dropped_frames;
	int min_priority;
	int64_t last_dts_usec;

	bool is_network;
};

bool stopping(struct ffmpeg_muxer *stream);
bool active(struct ffmpeg_muxer *stream);
void start_pipe(struct ffmpeg_muxer *stream, const char *path);
bool write_packet(struct ffmpeg_muxer *stream, struct encoder_packet *packet);
bool send_headers(struct ffmpeg_muxer *stream);
int deactivate(struct ffmpeg_muxer *stream, int code);
void ffmpeg_mux_stop(void *data, uint64_t ts);
uint64_t ffmpeg_mux_total_bytes(void *data);
