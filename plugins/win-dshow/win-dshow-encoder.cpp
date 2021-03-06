#include <obs-module.h>
#include <obs-avc.h>
#include <util/darray.h>
#include <util/dstr.hpp>
#include "libdshowcapture/dshowcapture.hpp"

using namespace DShow;
using namespace std;

struct DShowEncoder {
	obs_encoder_t *context;
	VideoEncoder encoder;

	VideoEncoderConfig config;

	const wchar_t *device;
	video_format format;
	long long frameInterval;

	bool first = true;
	DARRAY(uint8_t) firstPacket;
	DARRAY(uint8_t) header;

	inline DShowEncoder(obs_encoder_t *context_, const wchar_t *device_)
		: context(context_), device(device_)
	{
		da_init(firstPacket);
		da_init(header);
	}

	inline ~DShowEncoder()
	{
		da_free(firstPacket);
		da_free(header);
	}

	inline void ParseFirstPacket(const uint8_t *data, size_t size);

	inline bool Update(obs_data_t *settings);
	inline bool Encode(struct encoder_frame *frame,
			   struct encoder_packet *packet,
			   bool *received_packet);
};

static const char *GetC985EncoderName(void *)
{
	return obs_module_text("Encoder.C985");
}

static const char *GetC353EncoderName(void *)
{
	return obs_module_text("Encoder.C353");
}

static inline void FindDevice(DeviceId &id, const wchar_t *name)
{
	vector<DeviceId> devices;
	DShow::VideoEncoder::EnumEncoders(devices);

	for (const DeviceId &device : devices) {
		if (device.name.find(name) != string::npos) {
			id = device;
			break;
		}
	}
}

/*
 * As far as I can tell the devices only seem to support 1024x768 and 1280x720
 * resolutions, so I'm just going to cap it to those resolutions by aspect.
 *
 * XXX: This should really not be hard-coded.  Problem is I don't know how to
 * properly query the encoding capabilities of the devices.
 */
static const double standardAspect = 1024.0 / 768.0;
static const double wideAspect = 1280.0 / 720.0;

inline bool DShowEncoder::Update(obs_data_t *settings)
{
	DStr deviceName;
	DeviceId id;

	FindDevice(id, device);

	video_t *video = obs_encoder_video(context);
	const struct video_output_info *voi = video_output_get_info(video);

	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	int keyint_sec = (int)obs_data_get_int(settings, "keyint_sec");
	int width = (int)obs_encoder_get_width(context);
	int height = (int)obs_encoder_get_height(context);

	double aspect = double(width) / double(height);

	if (keyint_sec == 0)
		keyint_sec = 2;
	if (fabs(aspect - standardAspect) < fabs(aspect - wideAspect)) {
		width = 1024;
		height = 768;
	} else {
		width = 1280;
		height = 720;
	}

	int keyint = keyint_sec * voi->fps_num / voi->fps_den;

	frameInterval = voi->fps_den * 10000000 / voi->fps_num;

	config.fpsNumerator = voi->fps_num;
	config.fpsDenominator = voi->fps_den;
	config.bitrate = bitrate;
	config.keyframeInterval = keyint;
	config.cx = width;
	config.cy = height;
	config.name = id.name;
	config.path = id.path;

	first = true;
	da_resize(firstPacket, 0);
	da_resize(header, 0);

	dstr_from_wcs(deviceName, id.name.c_str());

	DStr encoder_name;
	dstr_from_wcs(encoder_name, config.name.c_str());
	blog(LOG_DEBUG,
	     "win-dshow-encoder:\n"
	     "\tencoder: %s\n"
	     "\twidth:   %d\n"
	     "\theight:  %d\n"
	     "\tfps_num: %d\n"
	     "\tfps_den: %d",
	     deviceName->array, (int)width, (int)height, (int)voi->fps_num,
	     (int)voi->fps_den);

	return encoder.SetConfig(config);
}

static bool UpdateDShowEncoder(void *data, obs_data_t *settings)
{
	DShowEncoder *encoder = reinterpret_cast<DShowEncoder *>(data);

	if (!obs_encoder_active(encoder->context))
		return encoder->Update(settings);

	return true;
}

static inline void *CreateDShowEncoder(obs_data_t *settings,
				       obs_encoder_t *context,
				       const wchar_t *device)
{
	DShowEncoder *encoder = nullptr;

	try {
		encoder = new DShowEncoder(context, device);
		UpdateDShowEncoder(encoder, settings);

	} catch (const char *error) {
		blog(LOG_ERROR, "Could not create DirectShow encoder '%s': %s",
		     obs_encoder_get_name(context), error);
	}

	UNUSED_PARAMETER(settings);
	return encoder;
}

static void *CreateC985Encoder(obs_data_t *settings, obs_encoder_t *context)
{
	return CreateDShowEncoder(settings, context, L"C985");
}

static void *CreateC353Encoder(obs_data_t *settings, obs_encoder_t *context)
{
	return CreateDShowEncoder(settings, context, L"C353");
}

static void DestroyDShowEncoder(void *data)
{
	delete reinterpret_cast<DShowEncoder *>(data);
}

/* the first packet contains the SPS/PPS (header) NALs, so parse the first
 * packet and separate the NALs */
inline void DShowEncoder::ParseFirstPacket(const uint8_t *data, size_t size)
{
	const uint8_t *nal_start, *nal_end, *nal_codestart;
	const uint8_t *end = data + size;
	int type;

	nal_start = obs_avc_find_startcode(data, end);
	nal_end = nullptr;
	while (nal_end != end) {
		nal_codestart = nal_start;

		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		type = nal_start[0] & 0x1F;

		nal_end = obs_avc_find_startcode(nal_start, end);
		if (!nal_end)
			nal_end = end;

		if (type == OBS_NAL_SPS || type == OBS_NAL_PPS) {
			da_push_back_array(header, nal_codestart,
					   nal_end - nal_codestart);

		} else {
			da_push_back_array(firstPacket, nal_codestart,
					   nal_end - nal_codestart);
		}

		nal_start = nal_end;
	}
}

inline bool DShowEncoder::Encode(struct encoder_frame *frame,
				 struct encoder_packet *packet,
				 bool *received_packet)
{
	unsigned char *frame_data[DSHOW_MAX_PLANES] = {};
	size_t frame_sizes[DSHOW_MAX_PLANES] = {};
	EncoderPacket dshowPacket;
	bool new_packet = false;

	/* The encoders expect YV12, so swap the chroma planes for encoding */
	if (format == VIDEO_FORMAT_I420) {
		frame_data[0] = frame->data[0];
		frame_data[1] = frame->data[2];
		frame_data[2] = frame->data[1];
		frame_sizes[0] = frame->linesize[0] * config.cy;
		frame_sizes[1] = frame->linesize[2] * config.cy / 2;
		frame_sizes[2] = frame->linesize[1] * config.cy / 2;
	}

	long long actualPTS = frame->pts * frameInterval;

	bool success = encoder.Encode(frame_data, frame_sizes, actualPTS,
				      actualPTS + frameInterval, dshowPacket,
				      new_packet);
	if (!success)
		return false;

	if (new_packet && !!dshowPacket.data && !!dshowPacket.size) {
		packet->data = dshowPacket.data;
		packet->size = dshowPacket.size;
		packet->type = OBS_ENCODER_VIDEO;
		packet->pts = dshowPacket.pts / frameInterval;
		packet->dts = dshowPacket.dts / frameInterval;
		packet->keyframe = obs_avc_keyframe(packet->data, packet->size);

		/* first packet must be parsed in order to retrieve header */
		if (first) {
			first = false;
			ParseFirstPacket(packet->data, packet->size);
			packet->data = firstPacket.array;
			packet->size = firstPacket.num;
		}

		*received_packet = true;
	}

	return true;
}

static bool DShowEncode(void *data, struct encoder_frame *frame,
			struct encoder_packet *packet, bool *received_packet)
{
	return reinterpret_cast<DShowEncoder *>(data)->Encode(frame, packet,
							      received_packet);
}

static bool GetDShowExtraData(void *data, uint8_t **extra_data, size_t *size)
{
	DShowEncoder *encoder = reinterpret_cast<DShowEncoder *>(data);

	*extra_data = encoder->header.array;
	*size = encoder->header.num;

	return *size > 0;
}

static inline bool ValidResolution(uint32_t width, uint32_t height)
{
	return (width == 1280 && height == 720) ||
	       (width == 1024 && height == 768);
}

static void GetDShowVideoInfo(void *data, struct video_scale_info *info)
{
	DShowEncoder *encoder = reinterpret_cast<DShowEncoder *>(data);
	encoder->format = VIDEO_FORMAT_I420;

	if (info->format == VIDEO_FORMAT_I420 &&
	    ValidResolution(info->width, info->height))
		return;

	info->format = VIDEO_FORMAT_I420;
	info->width = info->width;
	info->height = info->height;
	info->range = VIDEO_RANGE_DEFAULT;
	info->colorspace = VIDEO_CS_DEFAULT;

	double aspect = double(info->width) / double(info->height);

	if (fabs(aspect - standardAspect) < fabs(aspect - wideAspect)) {
		info->width = 1024;
		info->height = 768;
	} else {
		info->width = 1280;
		info->height = 720;
	}
}

static void GetDShowEncoderDefauts(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 1000);
}

static obs_properties_t *GetDShowEncoderProperties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();

	obs_properties_add_int(ppts, "bitrate", obs_module_text("Bitrate"),
			       1000, 60000, 1);

	UNUSED_PARAMETER(data);
	return ppts;
}

void RegisterDShowEncoders()
{
	obs_encoder_info info = {};
	info.type = OBS_ENCODER_VIDEO;
	info.codec = "h264";
	info.destroy = DestroyDShowEncoder;
	info.encode = DShowEncode;
	info.update = UpdateDShowEncoder;
	info.get_defaults = GetDShowEncoderDefauts;
	info.get_properties = GetDShowEncoderProperties;
	info.get_extra_data = GetDShowExtraData;
	info.get_video_info = GetDShowVideoInfo;

	vector<DeviceId> devices;
	DShow::VideoEncoder::EnumEncoders(devices);

	bool foundC985 = false;

	for (const DeviceId &device : devices) {
		if (!foundC985 && device.name.find(L"C985") != string::npos) {
			info.id = "dshow_c985_h264";
			info.get_name = GetC985EncoderName;
			info.create = CreateC985Encoder;
			obs_register_encoder(&info);
			foundC985 = true;

		} else if (device.name.find(L"C353") != string::npos) {
			info.id = "dshow_c353_h264";
			info.get_name = GetC353EncoderName;
			info.create = CreateC353Encoder;
			obs_register_encoder(&info);
		}
	}
}
