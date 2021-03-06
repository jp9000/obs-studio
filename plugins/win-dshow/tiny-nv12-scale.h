#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum target_format {
	TARGET_FORMAT_NV12,
	TARGET_FORMAT_I420,
	TARGET_FORMAT_YUY2,
};

struct nv12_scale {
	enum target_format format;

	int src_cx;
	int src_cy;

	int dst_cx;
	int dst_cy;
};

typedef struct nv12_scale nv12_scale_t;

extern void nv12_scale_init(nv12_scale_t *s, enum target_format format,
			    int dst_cx, int dst_cy, int src_cx, int src_cy);
extern void nv12_do_scale(nv12_scale_t *s, uint8_t *dst, const uint8_t *src);

#ifdef __cplusplus
}
#endif
