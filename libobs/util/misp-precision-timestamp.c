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

#include <stdlib.h>

#include "bmem.h"
#include "misp-precision-timestamp.h"
#include "platform.h"

#define NSEC_IN_USEC (1000ULL)
#define MISP_TAI_OFFSET_NSEC (8000082000ULL) /* MISB ST 0603 */
#define MAX_JITTER_TIME_NSEC (5000000ULL)    /* 5 milliseconds */

enum misp_precision_timestamp_discontinuity_type {
	MISP_PRECISION_TIMESTAMP_NO_DISCONTINUITY,
	MISP_PRECISION_TIMESTAMP_FORWARD_DISCONTINUITY,
	MISP_PRECISION_TIMESTAMP_REVERSE_DISCONTINUITY
};

struct misp_precision_timestamp {
	uint64_t utc_to_tai;
	uint64_t ref_timestamp;
	uint64_t ref_misp_time;
};

static const uint8_t payload[] = {
	0x4d, 0x49, 0x53, 0x50, /*  1 -  4 ("MISP") */
	0x6d, 0x69, 0x63, 0x72, /*  5 -  8 ("micr") */
	0x6f, 0x73, 0x65, 0x63, /*  9 - 12 ("osec") */
	0x74, 0x69, 0x6d, 0x65, /*  9 - 16 ("time") */
	0x1f, 0x00, 0x00, 0xff, /* 17 - 20 */
	0x00, 0x00, 0xff, 0x00, /* 21 - 24 */
	0x00, 0xff, 0x00, 0x00  /* 25 - 28 */
};

misp_precision_timestamp_t *misp_precision_timestamp_create(void)
{
	return bzalloc(sizeof(misp_precision_timestamp_t));
}

EXPORT void
misp_precision_timestamp_set_utc_to_tai(misp_precision_timestamp_t *ts,
					uint64_t diff)
{

	ts->utc_to_tai = diff;
}

uint8_t *
misp_precision_timestamp_get_sei_payload(misp_precision_timestamp_t *ts,
					 uint64_t timestamp)
{
	enum misp_precision_timestamp_discontinuity_type disco =
		MISP_PRECISION_TIMESTAMP_NO_DISCONTINUITY;

	/* we can trust that `timestamp <= os_gettime_ns()` */
	uint64_t timestamp_diff = os_gettime_ns() - timestamp;

	uint64_t misp_time = os_getunixtime_ns();

	if (UINT64_MAX - ts->utc_to_tai < misp_time)
		return NULL;

	/* UTC -> TAI */
	misp_time += ts->utc_to_tai;

	if (misp_time < MISP_TAI_OFFSET_NSEC)
		return NULL;

	/* TAI -> MISP */
	misp_time -= MISP_TAI_OFFSET_NSEC;

	if (misp_time < timestamp_diff)
		return NULL;

	/* correct by frame offset */
	misp_time -= timestamp_diff;

	if (ts->ref_timestamp == 0 || ts->ref_misp_time == 0) {

		/* not a discontinuity if we don't have valid reference data */

		ts->ref_timestamp = timestamp;
		ts->ref_misp_time = misp_time;

	} else {

		/*
		 * the logic in here is simple;  there is just some overflow protection
		 * that makes this a bit convoluted to read:
		 *
		 * ```c
		 * diff = (timestamp - ref_timestamp) - (misp_time - ref_misp_time)
		 *
		 * if (diff > MAX_JITTER)
		 *   // reverse discontinuity
		 * else if (diff < -1 * MAX_JITTER)
		 *   // forward discontinuity
		 * ```
		 */

		/* we can trust `timestamp` to increase monotonically */
		uint64_t ref_timestamp_diff = timestamp - ts->ref_timestamp;
		uint64_t ref_misp_time_diff = misp_time - ts->ref_misp_time;
		bool ref_misp_time_diff_reverse;

		if (misp_time < ts->ref_misp_time) {
			ref_misp_time_diff = ts->ref_misp_time - misp_time;
			ref_misp_time_diff_reverse = true;
		} else {
			ref_misp_time_diff = misp_time - ts->ref_misp_time;
			ref_misp_time_diff_reverse = false;
		}

		if (ref_misp_time_diff_reverse) {

			/* overflow protection ... */
			if (ref_timestamp_diff > MAX_JITTER_TIME_NSEC ||
			    ref_misp_time_diff > MAX_JITTER_TIME_NSEC ||
			    ref_timestamp_diff + ref_misp_time_diff >
				    MAX_JITTER_TIME_NSEC)

				disco = MISP_PRECISION_TIMESTAMP_REVERSE_DISCONTINUITY;

		} else if (ref_timestamp_diff > ref_misp_time_diff) {

			if (ref_timestamp_diff - ref_misp_time_diff >
			    MAX_JITTER_TIME_NSEC)
				disco = MISP_PRECISION_TIMESTAMP_REVERSE_DISCONTINUITY;

		} else {

			if (ref_misp_time_diff - ref_timestamp_diff >
			    MAX_JITTER_TIME_NSEC)
				disco = MISP_PRECISION_TIMESTAMP_FORWARD_DISCONTINUITY;
		}

		if (disco != MISP_PRECISION_TIMESTAMP_NO_DISCONTINUITY) {
			ts->ref_timestamp = timestamp;
			ts->ref_misp_time = misp_time;
		}
	}

	uint64_t diff = timestamp - ts->ref_timestamp;

	if (UINT64_MAX - diff < ts->ref_misp_time)
		return NULL;

	uint64_t t = ts->ref_misp_time + diff;
	t = (t + NSEC_IN_USEC / 2) / NSEC_IN_USEC; /* rounding */

	uint8_t *out = bzalloc(sizeof(payload));
	memcpy(out, payload, sizeof(payload));

	if (disco == MISP_PRECISION_TIMESTAMP_FORWARD_DISCONTINUITY) {
		out[16] = 0x5F; /* MISB ST 0603:  0b01011111 */
	} else if (disco == MISP_PRECISION_TIMESTAMP_REVERSE_DISCONTINUITY) {
		out[16] = 0x7F; /* MISB ST 0603:  0b01111111 */
	}

	out[17] = (uint8_t)(t >> 56);
	out[18] = (uint8_t)(t >> 48);
	out[20] = (uint8_t)(t >> 40);
	out[21] = (uint8_t)(t >> 32);
	out[23] = (uint8_t)(t >> 24);
	out[24] = (uint8_t)(t >> 16);
	out[26] = (uint8_t)(t >> 8);
	out[27] = (uint8_t)(t);

	return out;
}
