#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct video_queue;
struct nv12_scale;
typedef struct video_queue video_queue_t;
typedef struct nv12_scale nv12_scale_t;

enum queue_state {
	SHARED_QUEUE_STATE_INVALID,
	SHARED_QUEUE_STATE_STARTING,
	SHARED_QUEUE_STATE_READY,
	SHARED_QUEUE_STATE_STOPPING,
};

extern video_queue_t *video_queue_create(uint32_t cx, uint32_t cy,
					 uint64_t interval);
extern video_queue_t *video_queue_open();
extern void video_queue_close(video_queue_t *vq);

extern void video_queue_get_info(video_queue_t *vq, uint32_t *cx, uint32_t *cy,
				 uint64_t *interval);
extern void video_queue_write(video_queue_t *vq, uint8_t **data,
			      uint32_t *linesize, uint64_t timestamp);
extern enum queue_state video_queue_state(video_queue_t *vq);
extern bool video_queue_read(video_queue_t *vq, nv12_scale_t *scale, void *dst,
			     uint64_t *ts);

#ifdef __cplusplus
}
#endif
