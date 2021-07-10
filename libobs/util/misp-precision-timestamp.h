/*
 * Copyright (c) 2021 Armin Hasitzka <armin@grabyo.com>
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

#include "c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct misp_precision_timestamp;

typedef struct misp_precision_timestamp misp_precision_timestamp_t;

EXPORT misp_precision_timestamp_t *misp_precision_timestamp_create(void);

static inline void
misp_precision_timestamp_destroy(misp_precision_timestamp_t *ts)
{
	bfree(ts);
}

/**
 * @brief Set the difference between UTC and TAI
 * @param diff expected in nanoseconds
 */
EXPORT void
misp_precision_timestamp_set_utc_to_tai(misp_precision_timestamp_t *ts,
					uint64_t diff);

/**
 * @brief Palyload type of user data unregistered SEI messages
 *
 * Specified by "H.264:  Advanced video coding for generic audiovisual services"
 * Freely available:  https://www.itu.int/rec/T-REC-H.264
 */
static inline int misp_precision_timestamp_sei_payload_type(void)
{
	return 5;
}

/**
 * @brief Payload size of MISP precision timestamps (in bytes)
 *
 * Specified by "MISB ST 0604:  Timestamps for Class 1/Class 2 Motion Imagery"
 * Freely available:  https://gwg.nga.mil/misb/docs/standards/ST0604.6.pdf
 */
static inline int misp_precision_timestamp_sei_payload_size(void)
{
	return 28;
}

/**
 * @brief Generate MISP precision timestamp payload
 * @param timestamp monotonically increasing timestamp from os_gettime_ns()
 * @return `NULL` on error, a buffer to a bzalloc() allocated buffer otherwise
 *
 * Specified by "MISB ST 0604:  Timestamps for Class 1/Class 2 Motion Imagery"
 * Freely available:  https://gwg.nga.mil/misb/docs/standards/ST0604.6.pdf
 */
EXPORT uint8_t *
misp_precision_timestamp_get_sei_payload(misp_precision_timestamp_t *ts,
					 uint64_t timestamp);

#ifdef __cplusplus
}
#endif
