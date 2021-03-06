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

#include <sys/mman.h>

#include <util/bmem.h>

#include "v4l2-helpers.h"

#define blog(level, msg, ...) blog(level, "v4l2-helpers: " msg, ##__VA_ARGS__)

int_fast32_t v4l2_start_capture(int_fast32_t dev, struct v4l2_buffer_data *buf)
{
	enum v4l2_buf_type type;
	struct v4l2_buffer enq;

	memset(&enq, 0, sizeof(enq));
	enq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	enq.memory = V4L2_MEMORY_MMAP;

	for (enq.index = 0; enq.index < buf->count; ++enq.index) {
		if (v4l2_ioctl(dev, VIDIOC_QBUF, &enq) < 0) {
			blog(LOG_ERROR, "unable to queue buffer");
			return -1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (v4l2_ioctl(dev, VIDIOC_STREAMON, &type) < 0) {
		blog(LOG_ERROR, "unable to start stream");
		return -1;
	}

	return 0;
}

int_fast32_t v4l2_stop_capture(int_fast32_t dev)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (v4l2_ioctl(dev, VIDIOC_STREAMOFF, &type) < 0) {
		blog(LOG_ERROR, "unable to stop stream");
		return -1;
	}

	return 0;
}

int_fast32_t v4l2_reset_capture(int_fast32_t dev, struct v4l2_buffer_data *buf)
{
	blog(LOG_DEBUG, "attempting to reset capture");
	if (v4l2_stop_capture(dev) < 0)
		return -1;
	if (v4l2_start_capture(dev, buf) < 0)
		return -1;

	return 0;
}

#ifdef _DEBUG
int_fast32_t v4l2_query_all_buffers(int_fast32_t dev,
				    struct v4l2_buffer_data *buf_data)
{
	struct v4l2_buffer buf;

	blog(LOG_DEBUG, "attempting to read buffer data for %ld buffers",
	     buf_data->count);

	for (uint_fast32_t i = 0; i < buf_data->count; i++) {
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (v4l2_ioctl(dev, VIDIOC_QUERYBUF, &buf) < 0) {
			blog(LOG_DEBUG,
			     "failed to read buffer data for buffer #%ld", i);
		} else {
			blog(LOG_DEBUG,
			     "query buf #%ld info: ts: %06ld buf id #%d, flags 0x%08X, seq #%d, len %d, used %d",
			     i, buf.timestamp.tv_usec, buf.index, buf.flags,
			     buf.sequence, buf.length, buf.bytesused);
		}
	}

	return 0;
}
#endif

int_fast32_t v4l2_create_mmap(int_fast32_t dev, struct v4l2_buffer_data *buf)
{
	struct v4l2_requestbuffers req;
	struct v4l2_buffer map;

	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (v4l2_ioctl(dev, VIDIOC_REQBUFS, &req) < 0) {
		blog(LOG_ERROR, "Request for buffers failed !");
		return -1;
	}

	if (req.count < 2) {
		blog(LOG_ERROR, "Device returned less than 2 buffers");
		return -1;
	}

	buf->count = req.count;
	buf->info = bzalloc(req.count * sizeof(struct v4l2_mmap_info));

	memset(&map, 0, sizeof(map));
	map.type = req.type;
	map.memory = req.memory;

	for (map.index = 0; map.index < req.count; ++map.index) {
		if (v4l2_ioctl(dev, VIDIOC_QUERYBUF, &map) < 0) {
			blog(LOG_ERROR, "Failed to query buffer details");
			return -1;
		}

		buf->info[map.index].length = map.length;
		buf->info[map.index].start =
			v4l2_mmap(NULL, map.length, PROT_READ | PROT_WRITE,
				  MAP_SHARED, dev, map.m.offset);

		if (buf->info[map.index].start == MAP_FAILED) {
			blog(LOG_ERROR, "mmap for buffer failed");
			return -1;
		}
	}

	return 0;
}

int_fast32_t v4l2_destroy_mmap(struct v4l2_buffer_data *buf)
{
	for (uint_fast32_t i = 0; i < buf->count; ++i) {
		if (buf->info[i].start != MAP_FAILED && buf->info[i].start != 0)
			v4l2_munmap(buf->info[i].start, buf->info[i].length);
	}

	if (buf->count) {
		bfree(buf->info);
		buf->count = 0;
	}

	return 0;
}

int_fast32_t v4l2_set_input(int_fast32_t dev, int *input)
{
	if (!dev || !input)
		return -1;

	return (*input == -1) ? v4l2_ioctl(dev, VIDIOC_G_INPUT, input)
			      : v4l2_ioctl(dev, VIDIOC_S_INPUT, input);
}

int_fast32_t v4l2_get_input_caps(int_fast32_t dev, int input, uint32_t *caps)
{
	if (!dev || !caps)
		return -1;

	if (input == -1) {
		if (v4l2_ioctl(dev, VIDIOC_G_INPUT, &input) < 0)
			return -1;
	}

	struct v4l2_input in;
	memset(&in, 0, sizeof(in));
	in.index = input;

	if (v4l2_ioctl(dev, VIDIOC_ENUMINPUT, &in) < 0)
		return -1;

	*caps = in.capabilities;
	return 0;
}

int_fast32_t v4l2_set_format(int_fast32_t dev, int *resolution,
			     int *pixelformat, int *bytesperline)
{
	bool set = false;
	int width, height;
	struct v4l2_format fmt;

	if (!dev || !resolution || !pixelformat || !bytesperline)
		return -1;

	/* We need to set the type in order to query the settings */
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (v4l2_ioctl(dev, VIDIOC_G_FMT, &fmt) < 0)
		return -1;

	if (*resolution != -1) {
		v4l2_unpack_tuple(&width, &height, *resolution);
		fmt.fmt.pix.width = width;
		fmt.fmt.pix.height = height;
		set = true;
	}

	if (*pixelformat != -1) {
		fmt.fmt.pix.pixelformat = *pixelformat;
		set = true;
	}

	if (set && (v4l2_ioctl(dev, VIDIOC_S_FMT, &fmt) < 0))
		return -1;

	*resolution = v4l2_pack_tuple(fmt.fmt.pix.width, fmt.fmt.pix.height);
	*pixelformat = fmt.fmt.pix.pixelformat;
	*bytesperline = fmt.fmt.pix.bytesperline;
	return 0;
}

int_fast32_t v4l2_set_framerate(int_fast32_t dev, int *framerate)
{
	bool set = false;
	int num, denom;
	struct v4l2_streamparm par;

	if (!dev || !framerate)
		return -1;

	/* We need to set the type in order to query the stream settings */
	par.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (v4l2_ioctl(dev, VIDIOC_G_PARM, &par) < 0)
		return -1;

	if (*framerate != -1) {
		v4l2_unpack_tuple(&num, &denom, *framerate);
		par.parm.capture.timeperframe.numerator = num;
		par.parm.capture.timeperframe.denominator = denom;
		set = true;
	}

	if (set && (v4l2_ioctl(dev, VIDIOC_S_PARM, &par) < 0))
		return -1;

	*framerate = v4l2_pack_tuple(par.parm.capture.timeperframe.numerator,
				     par.parm.capture.timeperframe.denominator);
	return 0;
}

int_fast32_t v4l2_set_standard(int_fast32_t dev, int *standard)
{
	if (!dev || !standard)
		return -1;

	if (*standard == -1) {
		if (v4l2_ioctl(dev, VIDIOC_G_STD, standard) < 0)
			return -1;
	} else {
		if (v4l2_ioctl(dev, VIDIOC_S_STD, standard) < 0)
			return -1;
	}

	return 0;
}

int_fast32_t v4l2_enum_dv_timing(int_fast32_t dev, struct v4l2_dv_timings *dvt,
				 int index)
{
#if !defined(VIDIOC_ENUM_DV_TIMINGS) || !defined(V4L2_IN_CAP_DV_TIMINGS)
	UNUSED_PARAMETER(dev);
	UNUSED_PARAMETER(dvt);
	UNUSED_PARAMETER(index);
	return -1;
#else
	if (!dev || !dvt)
		return -1;

	struct v4l2_enum_dv_timings iter;
	memset(&iter, 0, sizeof(iter));
	iter.index = index;

	if (v4l2_ioctl(dev, VIDIOC_ENUM_DV_TIMINGS, &iter) < 0)
		return -1;

	memcpy(dvt, &iter.timings, sizeof(struct v4l2_dv_timings));

	return 0;
#endif
}

int_fast32_t v4l2_set_dv_timing(int_fast32_t dev, int *timing)
{
	if (!dev || !timing)
		return -1;

	if (*timing == -1)
		return 0;

	struct v4l2_dv_timings dvt;

	if (v4l2_enum_dv_timing(dev, &dvt, *timing) < 0)
		return -1;

	if (v4l2_ioctl(dev, VIDIOC_S_DV_TIMINGS, &dvt) < 0)
		return -1;

	return 0;
}
