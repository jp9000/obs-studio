/*
Copyright (C) 2014 by Leonhard Oelke <leonhard@in-verted.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <linux/videodev2.h>
#include <libv4l2.h>

#include <obs-module.h>
#include <media-io/video-io.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Data structure for mapped buffers
 */
struct v4l2_mmap_info {
	/** length of the mapped buffer */
	size_t length;
	/** start address of the mapped buffer */
	void *start;
};

/**
 * Data structure for buffer info
 */
struct v4l2_buffer_data {
	/** number of mapped buffers */
	uint_fast32_t count;
	/** memory info for mapped buffers */
	struct v4l2_mmap_info *info;
};

/**
 * Convert v4l2 pixel format to obs video format
 *
 * @param format v4l2 format id
 *
 * @return obs video_format id
 */
static inline enum video_format v4l2_to_obs_video_format(uint_fast32_t format)
{
	switch (format) {
	case V4L2_PIX_FMT_YVYU:
		return VIDEO_FORMAT_YVYU;
	case V4L2_PIX_FMT_YUYV:
		return VIDEO_FORMAT_YUY2;
	case V4L2_PIX_FMT_UYVY:
		return VIDEO_FORMAT_UYVY;
	case V4L2_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case V4L2_PIX_FMT_YUV420:
		return VIDEO_FORMAT_I420;
	case V4L2_PIX_FMT_YVU420:
		return VIDEO_FORMAT_I420;
#ifdef V4L2_PIX_FMT_XBGR32
	case V4L2_PIX_FMT_XBGR32:
		return VIDEO_FORMAT_BGRX;
#endif
#ifdef V4L2_PIX_FMT_ABGR32
	case V4L2_PIX_FMT_ABGR32:
		return VIDEO_FORMAT_BGRA;
#endif
	case V4L2_PIX_FMT_BGR24:
		return VIDEO_FORMAT_BGR3;
	default:
		return VIDEO_FORMAT_NONE;
	}
}

/**
 * Fixed framesizes for devices that don't support enumerating discrete values.
 *
 * The framesizes in this array are packed, the width encoded in the high word
 * and the height in the low word.
 * The array is terminated with a zero.
 */
static const int v4l2_framesizes[] = {
	/* 4:3 */
	160 << 16 | 120, 320 << 16 | 240, 480 << 16 | 320, 640 << 16 | 480,
	800 << 16 | 600, 1024 << 16 | 768, 1280 << 16 | 960, 1440 << 16 | 1050,
	1440 << 16 | 1080, 1600 << 16 | 1200,

	/* 16:9 */
	640 << 16 | 360, 960 << 16 | 540, 1280 << 16 | 720, 1600 << 16 | 900,
	1920 << 16 | 1080, 1920 << 16 | 1200, 2560 << 16 | 1440,
	3840 << 16 | 2160,

	/* 21:9 */
	2560 << 16 | 1080, 3440 << 16 | 1440, 5120 << 16 | 2160,

	/* tv */
	432 << 16 | 520, 480 << 16 | 320, 480 << 16 | 530, 486 << 16 | 440,
	576 << 16 | 310, 576 << 16 | 520, 576 << 16 | 570, 720 << 16 | 576,
	1024 << 16 | 576,

	0};

/**
 * Fixed framerates for devices that don't support enumerating discrete values.
 *
 * The framerates in this array are packed, the numerator encoded in the high
 * word and the denominator in the low word.
 * The array is terminated with a zero.
 */
static const int v4l2_framerates[] = {1 << 16 | 60,
				      1 << 16 | 50,
				      1 << 16 | 30,
				      1 << 16 | 25,
				      1 << 16 | 20,
				      1 << 16 | 15,
				      1 << 16 | 10,
				      1 << 16 | 5,

				      0};

/**
 * Pack two integer values into one
 *
 * Obviously the input integers have to be truncated in order to fit into
 * one. The effective 16bits left are still enough to handle resolutions and
 * framerates just fine.
 *
 * @param a integer one
 * @param b integer two
 *
 * @return the packed integer
 */
static inline int v4l2_pack_tuple(int a, int b)
{
	return (a << 16) | (b & 0xffff);
}

/**
 * Unpack two integer values from one
 *
 * @see v4l2_pack_tuple
 *
 * @param a pointer to integer a
 * @param b pointer to integer b
 * @param packed the packed integer
 */
static void v4l2_unpack_tuple(int *a, int *b, int packed)
{
	*a = packed >> 16;
	*b = packed & 0xffff;
}

/**
 * Start the video capture on the device.
 *
 * This enqueues the memory mapped buffers and instructs the device to start
 * the video stream.
 *
 * @param dev handle for the v4l2 device
 * @param buf buffer data
 *
 * @return negative on failure
 */
int_fast32_t v4l2_start_capture(int_fast32_t dev, struct v4l2_buffer_data *buf);

/**
 * Stop the video capture on the device.
 *
 * @param dev handle for the v4l2 device
 *
 * @return negative on failure
 */
int_fast32_t v4l2_stop_capture(int_fast32_t dev);

/**
 * Resets video capture on the device.
 *
 * This runs stop and start capture again. Stop dequeues the buffers and start
 * enqueues the memory mapped buffers and instructs the device to start
 * the video stream.
 *
 * @param dev handle for the v4l2 device
 * @param buf buffer data
 *
 * @return negative on failure
 */
int_fast32_t v4l2_reset_capture(int_fast32_t dev, struct v4l2_buffer_data *buf);

#ifdef _DEBUG
/**
 * Query the status of all buffers.
 * Only used for debug purposes.
 *
 * @param dev handle for the v4l2 device
 * @param buf_data buffer data
 *
 * @return negative on failure
 */
int_fast32_t v4l2_query_all_buffers(int_fast32_t dev,
				    struct v4l2_buffer_data *buf_data);
#endif

/**
 * Create memory mapping for buffers
 *
 * This tries to map at least 2, preferably 4, buffers to application memory.
 *
 * @param dev handle for the v4l2 device
 * @param buf buffer data
 *
 * @return negative on failure
 */
int_fast32_t v4l2_create_mmap(int_fast32_t dev, struct v4l2_buffer_data *buf);

/**
 * Destroy the memory mapping for buffers
 *
 * @param buf buffer data
 *
 * @return negative on failure
 */
int_fast32_t v4l2_destroy_mmap(struct v4l2_buffer_data *buf);

/**
 * Set the video input on the device.
 *
 * If the action succeeds input is set to the currently selected input.
 *
 * @param dev handle for the v4l2 device
 * @param input index of the input or -1 to leave it as is
 *
 * @return negative on failure
 */
int_fast32_t v4l2_set_input(int_fast32_t dev, int *input);

/**
 * Get capabilities for an input.
 *
 * @param dev handle for the v4l2 device
 * @param input index of the input or -1 to use the currently selected
 * @param caps capabilities for the input
 *
 * @return negative on failure
 */
int_fast32_t v4l2_get_input_caps(int_fast32_t dev, int input, uint32_t *caps);

/**
 * Set the video format on the device.
 *
 * If the action succeeds resolution, pixelformat and bytesperline are set
 * to the used values.
 *
 * @param dev handle for the v4l2 device
 * @param resolution packed value of the resolution or -1 to leave as is
 * @param pixelformat index of the pixelformat or -1 to leave as is
 * @param bytesperline this will be set accordingly on success
 *
 * @return negative on failure
 */
int_fast32_t v4l2_set_format(int_fast32_t dev, int *resolution,
			     int *pixelformat, int *bytesperline);

/**
 * Set the framerate on the device.
 *
 * If the action succeeds framerate is set to the used value.
 *
 * @param dev handle to the v4l2 device
 * @param framerate packed value of the framerate or -1 to leave as is
 *
 * @return negative on failure
 */
int_fast32_t v4l2_set_framerate(int_fast32_t dev, int *framerate);

/**
 * Set a video standard on the device.
 *
 * If the action succeeds standard is set to the used video standard id.
 *
 * @param dev handle to the v4l2 device
 * @param standard id of the standard to use or -1 to leave as is
 *
 * @return negative on failure
 */
int_fast32_t v4l2_set_standard(int_fast32_t dev, int *standard);

/**
 * Get the dv timing for an input with a specified index
 *
 * @param dev handle to the v4l2 device
 * @param dvt pointer to the timing structure to fill
 * @param index index of the dv timing to fetch
 *
 * @return negative on failure
 */
int_fast32_t v4l2_enum_dv_timing(int_fast32_t dev, struct v4l2_dv_timings *dvt,
				 int index);
/**
 * Set a dv timing on the device
 *
 * Currently standard will not be changed on success or error.
 *
 * @param dev handle to the v4l2 device
 * @param timing index of the timing to use or -1 to leave as is
 *
 * @return negative on failure
 */
int_fast32_t v4l2_set_dv_timing(int_fast32_t dev, int *timing);

#ifdef __cplusplus
}
#endif
