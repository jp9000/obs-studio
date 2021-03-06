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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <linux/videodev2.h>
#include <libv4l2.h>

#include <util/threading.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <obs-module.h>

#include "v4l2-controls.h"
#include "v4l2-helpers.h"

#if HAVE_UDEV
#include "v4l2-udev.h"
#endif

/* The new dv timing api was introduced in Linux 3.4
 * Currently we simply disable dv timings when this is not defined */
#if !defined(VIDIOC_ENUM_DV_TIMINGS) || !defined(V4L2_IN_CAP_DV_TIMINGS)
#define V4L2_IN_CAP_DV_TIMINGS 0
#endif

#define V4L2_DATA(voidptr) struct v4l2_data *data = voidptr;

#define timeval2ns(tv) \
	(((uint64_t)tv.tv_sec * 1000000000) + ((uint64_t)tv.tv_usec * 1000))

#define V4L2_FOURCC_STR(code)                                         \
	(char[5])                                                     \
	{                                                             \
		code & 0xFF, (code >> 8) & 0xFF, (code >> 16) & 0xFF, \
			(code >> 24) & 0xFF, 0                        \
	}

#define blog(level, msg, ...) blog(level, "v4l2-input: " msg, ##__VA_ARGS__)

/**
 * Data structure for the v4l2 source
 */
struct v4l2_data {
	/* settings */
	char *device_id;
	int input;
	int pixfmt;
	int standard;
	int dv_timing;
	int resolution;
	int framerate;
	int color_range;

	/* internal data */
	obs_source_t *source;
	pthread_t thread;
	os_event_t *event;

	bool framerate_unchanged;
	bool resolution_unchanged;
	int_fast32_t dev;
	int width;
	int height;
	int linesize;
	struct v4l2_buffer_data buffers;

	bool auto_reset;
	int timeout_frames;
};

/* forward declarations */
static void v4l2_init(struct v4l2_data *data);
static void v4l2_terminate(struct v4l2_data *data);
static void v4l2_update(void *vptr, obs_data_t *settings);

/**
 * Prepare the output frame structure for obs and compute plane offsets
 *
 * Basically all data apart from memory pointers and the timestamp is known
 * before the capture starts. This function prepares the obs_source_frame
 * struct with all the data that is already known.
 *
 * v4l2 uses a continuous memory segment for all planes so we simply compute
 * offsets to add to the start address in order to give obs the correct data
 * pointers for the individual planes.
 */
static void v4l2_prep_obs_frame(struct v4l2_data *data,
				struct obs_source_frame *frame,
				size_t *plane_offsets)
{
	memset(frame, 0, sizeof(struct obs_source_frame));
	memset(plane_offsets, 0, sizeof(size_t) * MAX_AV_PLANES);

	frame->width = data->width;
	frame->height = data->height;
	frame->format = v4l2_to_obs_video_format(data->pixfmt);
	video_format_get_parameters(VIDEO_CS_DEFAULT, data->color_range,
				    frame->color_matrix, frame->color_range_min,
				    frame->color_range_max);

	switch (data->pixfmt) {
	case V4L2_PIX_FMT_NV12:
		frame->linesize[0] = data->linesize;
		frame->linesize[1] = data->linesize;
		plane_offsets[1] = data->linesize * data->height;
		break;
	case V4L2_PIX_FMT_YVU420:
		frame->linesize[0] = data->linesize;
		frame->linesize[1] = data->linesize / 2;
		frame->linesize[2] = data->linesize / 2;
		plane_offsets[1] = data->linesize * data->height * 5 / 4;
		plane_offsets[2] = data->linesize * data->height;
		break;
	case V4L2_PIX_FMT_YUV420:
		frame->linesize[0] = data->linesize;
		frame->linesize[1] = data->linesize / 2;
		frame->linesize[2] = data->linesize / 2;
		plane_offsets[1] = data->linesize * data->height;
		plane_offsets[2] = data->linesize * data->height * 5 / 4;
		break;
	default:
		frame->linesize[0] = data->linesize;
		break;
	}
}

/*
 * Worker thread to get video data
 */
static void *v4l2_thread(void *vptr)
{
	V4L2_DATA(vptr);
	int r;
	fd_set fds;
	uint8_t *start;
	uint64_t frames;
	uint64_t first_ts;
	struct timeval tv;
	struct v4l2_buffer buf;
	struct obs_source_frame out;
	size_t plane_offsets[MAX_AV_PLANES];
	int fps_num, fps_denom;
	float ffps;
	uint64_t timeout_usec;

	blog(LOG_DEBUG, "%s: new capture thread", data->device_id);
	os_set_thread_name("v4l2: capture");

	/* Get framerate and calculate appropriate select timeout value. */
	v4l2_unpack_tuple(&fps_num, &fps_denom, data->framerate);
	ffps = (float)fps_denom / fps_num;
	blog(LOG_DEBUG, "%s: framerate: %.2f fps", data->device_id, ffps);
	/* Timeout set to 5 frame periods. */
	timeout_usec = (1000000 * data->timeout_frames) / ffps;
	blog(LOG_INFO, "%s: select timeout set to %ldus (%dx frame periods)",
	     data->device_id, timeout_usec, data->timeout_frames);

	if (v4l2_start_capture(data->dev, &data->buffers) < 0)
		goto exit;

	blog(LOG_DEBUG, "%s: new capture started", data->device_id);

	frames = 0;
	first_ts = 0;
	v4l2_prep_obs_frame(data, &out, plane_offsets);

	blog(LOG_DEBUG, "%s: obs frame prepared", data->device_id);

	while (os_event_try(data->event) == EAGAIN) {
		FD_ZERO(&fds);
		FD_SET(data->dev, &fds);

		/* Set timeout timevalue. */
		tv.tv_sec = 0;
		tv.tv_usec = timeout_usec;

		r = select(data->dev + 1, &fds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			blog(LOG_ERROR, "%s: select failed", data->device_id);
			break;
		} else if (r == 0) {
			blog(LOG_ERROR, "%s: select timed out",
			     data->device_id);

#ifdef _DEBUG
			v4l2_query_all_buffers(data->dev, &data->buffers);
#endif

			if (v4l2_ioctl(data->dev, VIDIOC_LOG_STATUS) < 0) {
				blog(LOG_ERROR, "%s: failed to log status",
				     data->device_id);
			}

			if (data->auto_reset) {
				if (v4l2_reset_capture(data->dev,
						       &data->buffers) == 0)
					blog(LOG_INFO,
					     "%s: stream reset successful",
					     data->device_id);
				else
					blog(LOG_ERROR, "%s: failed to reset",
					     data->device_id);
			}

			continue;
		}

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (v4l2_ioctl(data->dev, VIDIOC_DQBUF, &buf) < 0) {
			if (errno == EAGAIN) {
				blog(LOG_DEBUG, "%s: ioctl dqbuf eagain",
				     data->device_id);
				continue;
			}
			blog(LOG_ERROR, "%s: failed to dequeue buffer",
			     data->device_id);
			break;
		}

		blog(LOG_DEBUG,
		     "%s: ts: %06ld buf id #%d, flags 0x%08X, seq #%d, len %d, used %d",
		     data->device_id, buf.timestamp.tv_usec, buf.index,
		     buf.flags, buf.sequence, buf.length, buf.bytesused);

		out.timestamp = timeval2ns(buf.timestamp);
		if (!frames)
			first_ts = out.timestamp;
		out.timestamp -= first_ts;

		start = (uint8_t *)data->buffers.info[buf.index].start;
		for (uint_fast32_t i = 0; i < MAX_AV_PLANES; ++i)
			out.data[i] = start + plane_offsets[i];
		obs_source_output_video(data->source, &out);

		if (v4l2_ioctl(data->dev, VIDIOC_QBUF, &buf) < 0) {
			blog(LOG_ERROR, "%s: failed to enqueue buffer",
			     data->device_id);
			break;
		}

		frames++;
	}

	blog(LOG_INFO, "%s: Stopped capture after %" PRIu64 " frames",
	     data->device_id, frames);

exit:
	v4l2_stop_capture(data->dev);
	return NULL;
}

static const char *v4l2_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("V4L2Input");
}

static void v4l2_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "input", -1);
	obs_data_set_default_int(settings, "pixelformat", -1);
	obs_data_set_default_int(settings, "standard", -1);
	obs_data_set_default_int(settings, "dv_timing", -1);
	obs_data_set_default_int(settings, "resolution", -1);
	obs_data_set_default_int(settings, "framerate", -1);
	obs_data_set_default_int(settings, "color_range", VIDEO_RANGE_DEFAULT);
	obs_data_set_default_bool(settings, "buffering", true);
	obs_data_set_default_bool(settings, "auto_reset", false);
	obs_data_set_default_int(settings, "timeout_frames", 5);
}

/**
 * Enable/Disable all properties for the source.
 *
 * @note A property that should be ignored can be specified
 *
 * @param props the source properties
 * @param ignore ignore this property
 * @param enable enable/disable all properties
 */
static void v4l2_props_set_enabled(obs_properties_t *props,
				   obs_property_t *ignore, bool enable)
{
	if (!props)
		return;

	for (obs_property_t *prop = obs_properties_first(props); prop != NULL;
	     obs_property_next(&prop)) {
		if (prop == ignore)
			continue;

		obs_property_set_enabled(prop, enable);
	}
}

/*
 * List available devices
 */
static void v4l2_device_list(obs_property_t *prop, obs_data_t *settings)
{
	DIR *dirp;
	struct dirent *dp;
	struct dstr device;
	bool cur_device_found;
	size_t cur_device_index;
	const char *cur_device_name;

#ifdef __FreeBSD__
	dirp = opendir("/dev");
#else
	dirp = opendir("/sys/class/video4linux");
#endif
	if (!dirp)
		return;

	cur_device_found = false;
	cur_device_name = obs_data_get_string(settings, "device_id");

	obs_property_list_clear(prop);

	dstr_init_copy(&device, "/dev/");

	while ((dp = readdir(dirp)) != NULL) {
		int fd;
		uint32_t caps;
		struct v4l2_capability video_cap;

#ifdef __FreeBSD__
		if (strstr(dp->d_name, "video") == NULL)
			continue;
#endif

		if (dp->d_type == DT_DIR)
			continue;

		dstr_resize(&device, 5);
		dstr_cat(&device, dp->d_name);

		if ((fd = v4l2_open(device.array, O_RDWR | O_NONBLOCK)) == -1) {
			blog(LOG_INFO, "Unable to open %s", device.array);
			continue;
		}

		if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &video_cap) == -1) {
			blog(LOG_INFO, "Failed to query capabilities for %s",
			     device.array);
			v4l2_close(fd);
			continue;
		}

#ifndef V4L2_CAP_DEVICE_CAPS
		caps = video_cap.capabilities;
#else
		/* ... since Linux 3.3 */
		caps = (video_cap.capabilities & V4L2_CAP_DEVICE_CAPS)
			       ? video_cap.device_caps
			       : video_cap.capabilities;
#endif

		if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) {
			blog(LOG_INFO, "%s seems to not support video capture",
			     device.array);
			v4l2_close(fd);
			continue;
		}

		/* make sure device names are unique */
		char unique_device_name[68];
		sprintf(unique_device_name, "%s (%s)", video_cap.card,
			video_cap.bus_info);
		obs_property_list_add_string(prop, unique_device_name,
					     device.array);
		blog(LOG_INFO, "Found device '%s' at %s", video_cap.card,
		     device.array);

		/* check if this is the currently used device */
		if (cur_device_name && !strcmp(cur_device_name, device.array))
			cur_device_found = true;

		v4l2_close(fd);
	}

	/* add currently selected device if not present, but disable it ... */
	if (!cur_device_found && cur_device_name && strlen(cur_device_name)) {
		cur_device_index = obs_property_list_add_string(
			prop, cur_device_name, cur_device_name);
		obs_property_list_item_disable(prop, cur_device_index, true);
	}

	closedir(dirp);
	dstr_free(&device);
}

/*
 * List inputs for device
 */
static void v4l2_input_list(int_fast32_t dev, obs_property_t *prop)
{
	struct v4l2_input in;
	memset(&in, 0, sizeof(in));

	obs_property_list_clear(prop);

	while (v4l2_ioctl(dev, VIDIOC_ENUMINPUT, &in) == 0) {
		obs_property_list_add_int(prop, (char *)in.name, in.index);
		blog(LOG_INFO, "Found input '%s' (Index %d)", in.name,
		     in.index);
		in.index++;
	}
}

/*
 * List formats for device
 */
static void v4l2_format_list(int dev, obs_property_t *prop)
{
	struct v4l2_fmtdesc fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.index = 0;
	struct dstr buffer;
	dstr_init(&buffer);

	obs_property_list_clear(prop);

	while (v4l2_ioctl(dev, VIDIOC_ENUM_FMT, &fmt) == 0) {
		dstr_copy(&buffer, (char *)fmt.description);
		if (fmt.flags & V4L2_FMT_FLAG_EMULATED)
			dstr_cat(&buffer, " (Emulated)");

		if (v4l2_to_obs_video_format(fmt.pixelformat) !=
		    VIDEO_FORMAT_NONE) {
			obs_property_list_add_int(prop, buffer.array,
						  fmt.pixelformat);
			blog(LOG_INFO, "Pixelformat: %s (available)",
			     buffer.array);
		} else {
			blog(LOG_INFO, "Pixelformat: %s (unavailable)",
			     buffer.array);
		}
		fmt.index++;
	}

	dstr_free(&buffer);
}

/*
 * List video standards for the device
 */
static void v4l2_standard_list(int dev, obs_property_t *prop)
{
	struct v4l2_standard std;
	std.index = 0;

	obs_property_list_clear(prop);

	obs_property_list_add_int(prop, obs_module_text("LeaveUnchanged"), -1);

	while (v4l2_ioctl(dev, VIDIOC_ENUMSTD, &std) == 0) {
		obs_property_list_add_int(prop, (char *)std.name, std.id);
		std.index++;
	}
}

/*
 * List dv timings for the device
 */
static void v4l2_dv_timing_list(int dev, obs_property_t *prop)
{
	struct v4l2_dv_timings dvt;
	struct dstr buf;
	int index = 0;
	dstr_init(&buf);

	obs_property_list_clear(prop);

	obs_property_list_add_int(prop, obs_module_text("LeaveUnchanged"), -1);

	while (v4l2_enum_dv_timing(dev, &dvt, index) == 0) {
		/* i do not pretend to understand, this is from qv4l2 ... */
		double h = (double)dvt.bt.height + dvt.bt.vfrontporch +
			   dvt.bt.vsync + dvt.bt.vbackporch +
			   dvt.bt.il_vfrontporch + dvt.bt.il_vsync +
			   dvt.bt.il_vbackporch;
		double w = (double)dvt.bt.width + dvt.bt.hfrontporch +
			   dvt.bt.hsync + dvt.bt.hbackporch;
		double i = (dvt.bt.interlaced) ? 2.0f : 1.0f;
		double rate = (double)dvt.bt.pixelclock / (w * (h / i));

		dstr_printf(&buf, "%ux%u%c %.2f", dvt.bt.width, dvt.bt.height,
			    (dvt.bt.interlaced) ? 'i' : 'p', rate);

		obs_property_list_add_int(prop, buf.array, index);

		index++;
	}

	dstr_free(&buf);
}

/*
 * List resolutions for device and format
 */
static void v4l2_resolution_list(int dev, uint_fast32_t pixelformat,
				 obs_property_t *prop)
{
	struct v4l2_frmsizeenum frmsize;
	frmsize.pixel_format = pixelformat;
	frmsize.index = 0;
	struct dstr buffer;
	dstr_init(&buffer);

	obs_property_list_clear(prop);

	obs_property_list_add_int(prop, obs_module_text("LeaveUnchanged"), -1);

	v4l2_ioctl(dev, VIDIOC_ENUM_FRAMESIZES, &frmsize);

	switch (frmsize.type) {
	case V4L2_FRMSIZE_TYPE_DISCRETE:
		while (v4l2_ioctl(dev, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
			dstr_printf(&buffer, "%dx%d", frmsize.discrete.width,
				    frmsize.discrete.height);
			obs_property_list_add_int(
				prop, buffer.array,
				v4l2_pack_tuple(frmsize.discrete.width,
						frmsize.discrete.height));
			frmsize.index++;
		}
		break;
	default:
		blog(LOG_INFO, "Stepwise and Continuous framesizes "
			       "are currently hardcoded");

		for (const int *packed = v4l2_framesizes; *packed; ++packed) {
			int width;
			int height;
			v4l2_unpack_tuple(&width, &height, *packed);
			dstr_printf(&buffer, "%dx%d", width, height);
			obs_property_list_add_int(prop, buffer.array, *packed);
		}
		break;
	}

	dstr_free(&buffer);
}

/*
 * List framerates for device and resolution
 */
static void v4l2_framerate_list(int dev, uint_fast32_t pixelformat,
				uint_fast32_t width, uint_fast32_t height,
				obs_property_t *prop)
{
	struct v4l2_frmivalenum frmival;
	frmival.pixel_format = pixelformat;
	frmival.width = width;
	frmival.height = height;
	frmival.index = 0;
	struct dstr buffer;
	dstr_init(&buffer);

	obs_property_list_clear(prop);

	obs_property_list_add_int(prop, obs_module_text("LeaveUnchanged"), -1);

	v4l2_ioctl(dev, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);

	switch (frmival.type) {
	case V4L2_FRMIVAL_TYPE_DISCRETE:
		while (v4l2_ioctl(dev, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) ==
		       0) {
			float fps = (float)frmival.discrete.denominator /
				    frmival.discrete.numerator;
			int pack =
				v4l2_pack_tuple(frmival.discrete.numerator,
						frmival.discrete.denominator);
			dstr_printf(&buffer, "%.2f", fps);
			obs_property_list_add_int(prop, buffer.array, pack);
			frmival.index++;
		}
		break;
	default:
		blog(LOG_INFO, "Stepwise and Continuous framerates "
			       "are currently hardcoded");

		for (const int *packed = v4l2_framerates; *packed; ++packed) {
			int num;
			int denom;
			v4l2_unpack_tuple(&num, &denom, *packed);
			float fps = (float)denom / num;
			dstr_printf(&buffer, "%.2f", fps);
			obs_property_list_add_int(prop, buffer.array, *packed);
		}
		break;
	}

	dstr_free(&buffer);
}

/*
 * Device selected callback
 */
static bool device_selected(obs_properties_t *props, obs_property_t *p,
			    obs_data_t *settings)
{
	int dev = v4l2_open(obs_data_get_string(settings, "device_id"),
			    O_RDWR | O_NONBLOCK);

	v4l2_props_set_enabled(props, p, (dev == -1) ? false : true);

	if (dev == -1)
		return false;

	obs_property_t *prop = obs_properties_get(props, "input");
	obs_properties_t *ctrl_props_new = obs_properties_create();

	obs_properties_remove_by_name(props, "controls");

	v4l2_input_list(dev, prop);
	v4l2_update_controls(dev, ctrl_props_new, settings);
	v4l2_close(dev);

	obs_properties_add_group(props, "controls",
				 obs_module_text("CameraCtrls"),
				 OBS_GROUP_NORMAL, ctrl_props_new);

	obs_property_modified(prop, settings);

	return true;
}

/*
 * Input selected callback
 */
static bool input_selected(obs_properties_t *props, obs_property_t *p,
			   obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	int dev = v4l2_open(obs_data_get_string(settings, "device_id"),
			    O_RDWR | O_NONBLOCK);
	if (dev == -1)
		return false;

	obs_property_t *prop = obs_properties_get(props, "pixelformat");
	v4l2_format_list(dev, prop);
	v4l2_close(dev);

	obs_property_modified(prop, settings);

	return true;
}

/*
 * Format selected callback
 */
static bool format_selected(obs_properties_t *props, obs_property_t *p,
			    obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	int dev = v4l2_open(obs_data_get_string(settings, "device_id"),
			    O_RDWR | O_NONBLOCK);
	if (dev == -1)
		return false;

	int input = (int)obs_data_get_int(settings, "input");
	uint32_t caps = 0;
	if (v4l2_get_input_caps(dev, input, &caps) < 0)
		return false;
	caps &= V4L2_IN_CAP_STD | V4L2_IN_CAP_DV_TIMINGS;

	obs_property_t *resolution = obs_properties_get(props, "resolution");
	obs_property_t *framerate = obs_properties_get(props, "framerate");
	obs_property_t *standard = obs_properties_get(props, "standard");
	obs_property_t *dv_timing = obs_properties_get(props, "dv_timing");

	obs_property_set_visible(resolution, (!caps) ? true : false);
	obs_property_set_visible(framerate, (!caps) ? true : false);
	obs_property_set_visible(standard,
				 (caps & V4L2_IN_CAP_STD) ? true : false);
	obs_property_set_visible(
		dv_timing, (caps & V4L2_IN_CAP_DV_TIMINGS) ? true : false);

	if (!caps) {
		v4l2_resolution_list(dev,
				     obs_data_get_int(settings, "pixelformat"),
				     resolution);
	}
	if (caps & V4L2_IN_CAP_STD)
		v4l2_standard_list(dev, standard);
	if (caps & V4L2_IN_CAP_DV_TIMINGS)
		v4l2_dv_timing_list(dev, dv_timing);

	v4l2_close(dev);

	if (!caps)
		obs_property_modified(resolution, settings);
	if (caps & V4L2_IN_CAP_STD)
		obs_property_modified(standard, settings);
	if (caps & V4L2_IN_CAP_DV_TIMINGS)
		obs_property_modified(dv_timing, settings);

	return true;
}

/*
 * Resolution selected callback
 */
static bool resolution_selected(obs_properties_t *props, obs_property_t *p,
				obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	int width, height;
	int dev = v4l2_open(obs_data_get_string(settings, "device_id"),
			    O_RDWR | O_NONBLOCK);
	if (dev == -1)
		return false;

	obs_property_t *prop = obs_properties_get(props, "framerate");
	v4l2_unpack_tuple(&width, &height,
			  obs_data_get_int(settings, "resolution"));
	v4l2_framerate_list(dev, obs_data_get_int(settings, "pixelformat"),
			    width, height, prop);
	v4l2_close(dev);

	obs_property_modified(prop, settings);

	return true;
}

#if HAVE_UDEV
/**
 * Device added callback
 *
 * If everything went fine we can start capturing again when the device is
 * reconnected
 */
static void device_added(void *vptr, calldata_t *calldata)
{
	V4L2_DATA(vptr);

	obs_source_update_properties(data->source);

	const char *dev;
	calldata_get_string(calldata, "device", &dev);

	if (strcmp(data->device_id, dev))
		return;

	blog(LOG_INFO, "Device %s reconnected", dev);

	v4l2_init(data);
}
/**
 * Device removed callback
 *
 * We stop recording here so we don't block the device node
 */
static void device_removed(void *vptr, calldata_t *calldata)
{
	V4L2_DATA(vptr);

	obs_source_update_properties(data->source);

	const char *dev;
	calldata_get_string(calldata, "device", &dev);

	if (strcmp(data->device_id, dev))
		return;

	blog(LOG_INFO, "Device %s disconnected", dev);

	v4l2_terminate(data);
}

#endif

static obs_properties_t *v4l2_properties(void *vptr)
{
	V4L2_DATA(vptr);

	obs_properties_t *props = obs_properties_create();

	obs_property_t *device_list = obs_properties_add_list(
		props, "device_id", obs_module_text("Device"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_t *input_list = obs_properties_add_list(
		props, "input", obs_module_text("Input"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);

	obs_property_t *format_list = obs_properties_add_list(
		props, "pixelformat", obs_module_text("VideoFormat"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_t *standard_list = obs_properties_add_list(
		props, "standard", obs_module_text("VideoStandard"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_set_visible(standard_list, false);

	obs_property_t *dv_timing_list = obs_properties_add_list(
		props, "dv_timing", obs_module_text("DVTiming"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_set_visible(dv_timing_list, false);

	obs_property_t *resolution_list = obs_properties_add_list(
		props, "resolution", obs_module_text("Resolution"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_properties_add_list(props, "framerate",
				obs_module_text("FrameRate"),
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_t *color_range_list = obs_properties_add_list(
		props, "color_range", obs_module_text("ColorRange"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(color_range_list,
				  obs_module_text("ColorRange.Default"),
				  VIDEO_RANGE_DEFAULT);
	obs_property_list_add_int(color_range_list,
				  obs_module_text("ColorRange.Partial"),
				  VIDEO_RANGE_PARTIAL);
	obs_property_list_add_int(color_range_list,
				  obs_module_text("ColorRange.Full"),
				  VIDEO_RANGE_FULL);

	obs_properties_add_bool(props, "buffering",
				obs_module_text("UseBuffering"));

	obs_properties_add_bool(props, "auto_reset",
				obs_module_text("AutoresetOnTimeout"));

	obs_properties_add_int(props, "timeout_frames",
			       obs_module_text("FramesUntilTimeout"), 2, 120,
			       1);

	// a group to contain the camera control
	obs_properties_t *ctrl_props = obs_properties_create();
	obs_properties_add_group(props, "controls",
				 obs_module_text("CameraCtrls"),
				 OBS_GROUP_NORMAL, ctrl_props);

	obs_data_t *settings = obs_source_get_settings(data->source);
	v4l2_device_list(device_list, settings);
	obs_data_release(settings);

	obs_property_set_modified_callback(device_list, device_selected);
	obs_property_set_modified_callback(input_list, input_selected);
	obs_property_set_modified_callback(format_list, format_selected);
	obs_property_set_modified_callback(resolution_list,
					   resolution_selected);

	return props;
}

static void v4l2_terminate(struct v4l2_data *data)
{
	if (data->thread) {
		os_event_signal(data->event);
		pthread_join(data->thread, NULL);
		os_event_destroy(data->event);
		data->thread = 0;
	}

	v4l2_destroy_mmap(&data->buffers);

	if (data->dev != -1) {
		v4l2_close(data->dev);
		data->dev = -1;
	}
}

static void v4l2_destroy(void *vptr)
{
	V4L2_DATA(vptr);

	if (!data)
		return;

	v4l2_terminate(data);

	if (data->device_id)
		bfree(data->device_id);

#if HAVE_UDEV
	signal_handler_t *sh = v4l2_get_udev_signalhandler();

	signal_handler_disconnect(sh, "device_added", device_added, data);
	signal_handler_disconnect(sh, "device_removed", device_removed, data);

	v4l2_unref_udev();
#endif

	bfree(data);
}

/**
 * Initialize the v4l2 device
 *
 * This function:
 * - tries to open the device
 * - sets pixelformat and requested resolution
 * - sets the requested framerate
 * - maps the buffers
 * - starts the capture thread
 */
static void v4l2_init(struct v4l2_data *data)
{
	uint32_t input_caps;
	int fps_num, fps_denom;

	blog(LOG_INFO, "Start capture from %s", data->device_id);
	data->dev = v4l2_open(data->device_id, O_RDWR | O_NONBLOCK);
	if (data->dev == -1) {
		blog(LOG_ERROR, "Unable to open device");
		goto fail;
	}

	/* set input */
	if (v4l2_set_input(data->dev, &data->input) < 0) {
		blog(LOG_ERROR, "Unable to set input %d", data->input);
		goto fail;
	}
	blog(LOG_INFO, "Input: %d", data->input);
	if (v4l2_get_input_caps(data->dev, -1, &input_caps) < 0) {
		blog(LOG_ERROR, "Unable to get input capabilities");
		goto fail;
	}

	/* set video standard if supported */
	if (input_caps & V4L2_IN_CAP_STD) {
		if (v4l2_set_standard(data->dev, &data->standard) < 0) {
			blog(LOG_ERROR, "Unable to set video standard");
			goto fail;
		}
		data->resolution = -1;
		data->framerate = -1;
	}
	/* set dv timing if supported */
	if (input_caps & V4L2_IN_CAP_DV_TIMINGS) {
		if (v4l2_set_dv_timing(data->dev, &data->dv_timing) < 0) {
			blog(LOG_ERROR, "Unable to set dv timing");
			goto fail;
		}
		data->resolution = -1;
		data->framerate = -1;
	}

	/* set pixel format and resolution */
	if (v4l2_set_format(data->dev, &data->resolution, &data->pixfmt,
			    &data->linesize) < 0) {
		blog(LOG_ERROR, "Unable to set format");
		goto fail;
	}
	if (v4l2_to_obs_video_format(data->pixfmt) == VIDEO_FORMAT_NONE) {
		blog(LOG_ERROR, "Selected video format not supported");
		goto fail;
	}
	v4l2_unpack_tuple(&data->width, &data->height, data->resolution);
	blog(LOG_INFO, "Resolution: %dx%d", data->width, data->height);
	blog(LOG_INFO, "Pixelformat: %s", V4L2_FOURCC_STR(data->pixfmt));
	blog(LOG_INFO, "Linesize: %d Bytes", data->linesize);

	/* set framerate */
	if (v4l2_set_framerate(data->dev, &data->framerate) < 0) {
		blog(LOG_ERROR, "Unable to set framerate");
		goto fail;
	}
	v4l2_unpack_tuple(&fps_num, &fps_denom, data->framerate);
	blog(LOG_INFO, "Framerate: %.2f fps", (float)fps_denom / fps_num);

	/* map buffers */
	if (v4l2_create_mmap(data->dev, &data->buffers) < 0) {
		blog(LOG_ERROR, "Failed to map buffers");
		goto fail;
	}

	/* start the capture thread */
	if (os_event_init(&data->event, OS_EVENT_TYPE_MANUAL) != 0)
		goto fail;
	if (pthread_create(&data->thread, NULL, v4l2_thread, data) != 0)
		goto fail;
	return;
fail:
	blog(LOG_ERROR, "Initialization failed");
	v4l2_terminate(data);
}

/** Update source flags depending on the settings */
static void v4l2_update_source_flags(struct v4l2_data *data,
				     obs_data_t *settings)
{
	obs_source_set_async_unbuffered(
		data->source, !obs_data_get_bool(settings, "buffering"));
}

/**
 * Checking if any of the settings have changed so that we can restart the
 * stream
 */
static bool v4l2_settings_changed(struct v4l2_data *data, obs_data_t *settings)
{
	bool res = false;

	if (obs_data_get_string(settings, "device_id") != NULL &&
	    data->device_id != NULL) {
		res |= strcmp(data->device_id,
			      obs_data_get_string(settings, "device_id")) != 0;
		res |= data->input != obs_data_get_int(settings, "input");
		res |= data->pixfmt !=
		       obs_data_get_int(settings, "pixelformat");
		res |= data->standard != obs_data_get_int(settings, "standard");
		res |= data->dv_timing !=
		       obs_data_get_int(settings, "dv_timing");

		if (obs_data_get_int(settings, "resolution") == -1 &&
		    !data->resolution_unchanged) {
			data->resolution_unchanged = true;
			res |= true;
		} else if (obs_data_get_int(settings, "resolution") == -1 &&
			   data->resolution_unchanged) {
			res |= false;
		} else {
			data->resolution_unchanged = false;
			res |= (data->resolution !=
				obs_data_get_int(settings, "resolution")) &&
			       (obs_data_get_int(settings, "resolution") != -1);
		}

		if (obs_data_get_int(settings, "framerate") == -1 &&
		    !data->framerate_unchanged) {
			data->framerate_unchanged = true;
			res |= true;
		} else if (obs_data_get_int(settings, "framerate") == -1 &&
			   data->framerate_unchanged) {
			res |= false;
		} else {
			data->framerate_unchanged = false;
			res |= (data->framerate !=
				obs_data_get_int(settings, "framerate")) &&
			       (obs_data_get_int(settings, "framerate") != -1);
		}

		res |= data->color_range !=
		       obs_data_get_int(settings, "color_range");
	} else {
		res = true;
	}

	return res;
}

/**
 * Update the settings for the v4l2 source
 *
 * There are a few settings that can be changed without restarting the stream
 * Whenever this is called the currently active stream (if exists) is stopped,
 * the settings are updated and finally the new stream is started.
 */
static void v4l2_update(void *vptr, obs_data_t *settings)
{

	V4L2_DATA(vptr);

	bool needs_restart = v4l2_settings_changed(data, settings);

	if (needs_restart)
		v4l2_terminate(data);

	if (data->device_id)
		bfree(data->device_id);

	data->device_id = bstrdup(obs_data_get_string(settings, "device_id"));
	data->input = obs_data_get_int(settings, "input");
	data->pixfmt = obs_data_get_int(settings, "pixelformat");
	data->standard = obs_data_get_int(settings, "standard");
	data->dv_timing = obs_data_get_int(settings, "dv_timing");
	data->resolution = obs_data_get_int(settings, "resolution");
	data->framerate = obs_data_get_int(settings, "framerate");
	data->color_range = obs_data_get_int(settings, "color_range");
	data->auto_reset = obs_data_get_bool(settings, "auto_reset");
	data->timeout_frames = obs_data_get_int(settings, "timeout_frames");

	v4l2_update_source_flags(data, settings);

	if (needs_restart)
		v4l2_init(data);
}

static void *v4l2_create(obs_data_t *settings, obs_source_t *source)
{
	struct v4l2_data *data = bzalloc(sizeof(struct v4l2_data));
	data->dev = -1;
	data->source = source;
	data->resolution_unchanged = false;
	data->framerate_unchanged = false;

	/* Bitch about build problems ... */
#ifndef V4L2_CAP_DEVICE_CAPS
	blog(LOG_WARNING, "Plugin built without device caps support!");
#endif
#if !defined(VIDIOC_ENUM_DV_TIMINGS) || !defined(V4L2_IN_CAP_DV_TIMINGS)
	blog(LOG_WARNING, "Plugin built without dv-timing support!");
#endif

	v4l2_update(data, settings);

#if HAVE_UDEV
	v4l2_init_udev();
	signal_handler_t *sh = v4l2_get_udev_signalhandler();

	signal_handler_connect(sh, "device_added", &device_added, data);
	signal_handler_connect(sh, "device_removed", &device_removed, data);
#endif

	return data;
}

struct obs_source_info v4l2_input = {
	.id = "v4l2_input",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = v4l2_getname,
	.create = v4l2_create,
	.destroy = v4l2_destroy,
	.update = v4l2_update,
	.get_defaults = v4l2_defaults,
	.get_properties = v4l2_properties,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};
