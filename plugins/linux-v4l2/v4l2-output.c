#include <obs-module.h>
#include <util/platform.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_DEVICES 64

struct virtualcam_data {
	obs_output_t *output;
	int device;
	uint32_t frame_size;
};

static const char *virtualcam_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Virtual Camera Output";
}

static void virtualcam_destroy(void *data)
{
	struct virtualcam_data *vcam = (struct virtualcam_data *)data;
	close(vcam->device);
	bfree(data);
}

static bool loopback_module_loaded()
{
	bool loaded = false;

	char temp[512];

	FILE *fp = fopen("/proc/modules", "r");

	if (!fp)
		return false;

	while (fgets(temp, sizeof(temp), fp)) {
		if (strstr(temp, "v4l2loopback")) {
			loaded = true;
			break;
		}
	}

	if (fp)
		fclose(fp);

	return loaded;
}

bool loopback_module_available()
{
	if (loopback_module_loaded()) {
		return true;
	}

	if (system("PATH=\"$PATH:/sbin\" modinfo v4l2loopback >/dev/null 2>&1") ==
	    0) {
		return true;
	}

	return false;
}

static int loopback_module_load()
{
	return system(
		"PATH=\"$PATH:/sbin\" pkexec modprobe v4l2loopback exclusive_caps=1 card_label='OBS Virtual Camera' && sleep 0.5");
}

static void *virtualcam_create(obs_data_t *settings, obs_output_t *output)
{
	struct virtualcam_data *vcam =
		(struct virtualcam_data *)bzalloc(sizeof(*vcam));
	vcam->output = output;

	UNUSED_PARAMETER(settings);
	return vcam;
}

static bool try_connect(void *data, int device)
{
	struct virtualcam_data *vcam = (struct virtualcam_data *)data;
	struct v4l2_format format;
	struct v4l2_capability capability;
	struct v4l2_streamparm parm;

	uint32_t width = obs_output_get_width(vcam->output);
	uint32_t height = obs_output_get_height(vcam->output);

	vcam->frame_size = width * height * 2;

	char new_device[16];
	if (device < 0 || device >= MAX_DEVICES)
		return false;
	snprintf(new_device, 16, "/dev/video%d", device);

	vcam->device = open(new_device, O_RDWR);

	if (vcam->device < 0)
		return false;

	if (ioctl(vcam->device, VIDIOC_QUERYCAP, &capability) < 0)
		return false;

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (ioctl(vcam->device, VIDIOC_G_FMT, &format) < 0)
		return false;

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);

	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	parm.parm.output.capability = V4L2_CAP_TIMEPERFRAME;
	parm.parm.output.timeperframe.numerator = ovi.fps_den;
	parm.parm.output.timeperframe.denominator = ovi.fps_num;

	if (ioctl(vcam->device, VIDIOC_S_PARM, &parm) < 0)
		return false;

	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.sizeimage = vcam->frame_size;

	if (ioctl(vcam->device, VIDIOC_S_FMT, &format) < 0)
		return false;

	struct video_scale_info vsi = {0};
	vsi.format = VIDEO_FORMAT_YUY2;
	vsi.width = width;
	vsi.height = height;
	obs_output_set_video_conversion(vcam->output, &vsi);

	blog(LOG_INFO, "Virtual camera started");
	obs_output_begin_data_capture(vcam->output, 0);

	return true;
}

static bool virtualcam_start(void *data)
{
	struct virtualcam_data *vcam = (struct virtualcam_data *)data;

	if (!loopback_module_loaded()) {
		if (loopback_module_load() != 0)
			return false;
	}

	for (int i = 0; i < MAX_DEVICES; i++) {
		if (!try_connect(vcam, i))
			continue;
		else
			return true;
	}

	blog(LOG_WARNING, "Failed to start virtual camera");
	return false;
}

static void virtualcam_stop(void *data, uint64_t ts)
{
	struct virtualcam_data *vcam = (struct virtualcam_data *)data;
	obs_output_end_data_capture(vcam->output);
	close(vcam->device);

	blog(LOG_INFO, "Virtual camera stopped");

	UNUSED_PARAMETER(ts);
}

static void virtual_video(void *param, struct video_data *frame)
{
	struct virtualcam_data *vcam = (struct virtualcam_data *)param;
	uint32_t frame_size = vcam->frame_size;
	while (frame_size > 0) {
		ssize_t written =
			write(vcam->device, frame->data[0], vcam->frame_size);
		if (written == -1)
			break;
		frame_size -= written;
	}
}

struct obs_output_info virtualcam_info = {
	.id = "virtualcam_output",
	.flags = OBS_OUTPUT_VIDEO,
	.get_name = virtualcam_name,
	.create = virtualcam_create,
	.destroy = virtualcam_destroy,
	.start = virtualcam_start,
	.stop = virtualcam_stop,
	.raw_video = virtual_video,
};
