#include <obs-module.h>

#ifdef DEBUG
#ifndef _DEBUG
#define _DEBUG
#endif
#undef DEBUG
#endif

#include <fdk-aac/aacenc_lib.h>

static const char *libfdk_get_error(AACENC_ERROR err)
{
	switch (err) {
	case AACENC_OK:
		return "No error";
	case AACENC_INVALID_HANDLE:
		return "Invalid handle";
	case AACENC_MEMORY_ERROR:
		return "Memory allocation error";
	case AACENC_UNSUPPORTED_PARAMETER:
		return "Unsupported parameter";
	case AACENC_INVALID_CONFIG:
		return "Invalid config";
	case AACENC_INIT_ERROR:
		return "Initialization error";
	case AACENC_INIT_AAC_ERROR:
		return "AAC library initialization error";
	case AACENC_INIT_SBR_ERROR:
		return "SBR library initialization error";
	case AACENC_INIT_TP_ERROR:
		return "Transport library initialization error";
	case AACENC_INIT_META_ERROR:
		return "Metadata library initialization error";
	case AACENC_ENCODE_ERROR:
		return "Encoding error";
	case AACENC_ENCODE_EOF:
		return "End of file";
	default:
		return "Unknown error";
	}
}

typedef struct libfdk_encoder {
	obs_encoder_t *encoder;

	int channels, sample_rate;

	HANDLE_AACENCODER fdkhandle;
	AACENC_InfoStruct info;

	uint64_t total_samples;

	int frame_size_bytes;

	uint8_t *packet_buffer;
	int packet_buffer_size;
} libfdk_encoder_t;

static const char *libfdk_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LibFDK");
}

static obs_properties_t *libfdk_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int(props, "bitrate", obs_module_text("Bitrate"), 32,
			       1024, 32);
	obs_properties_add_bool(props, "afterburner",
				obs_module_text("Afterburner"));

	return props;
}

static void libfdk_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 128);
	obs_data_set_default_bool(settings, "afterburner", true);
}

#define CHECK_LIBFDK(r)                                                   \
	if ((err = (r)) != AACENC_OK) {                                   \
		blog(LOG_ERROR, #r " failed: %s", libfdk_get_error(err)); \
		goto fail;                                                \
	}

static void *libfdk_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	bool hasFdkHandle = false;
	libfdk_encoder_t *enc = 0;
	int bitrate = (int)obs_data_get_int(settings, "bitrate") * 1000;
	int afterburner = obs_data_get_bool(settings, "afterburner") ? 1 : 0;
	audio_t *audio = obs_encoder_audio(encoder);
	int mode = 0;
	AACENC_ERROR err;

	if (!bitrate) {
		blog(LOG_ERROR, "Invalid bitrate");
		return NULL;
	}

	enc = bzalloc(sizeof(libfdk_encoder_t));
	enc->encoder = encoder;

	enc->channels = (int)audio_output_get_channels(audio);
	enc->sample_rate = audio_output_get_sample_rate(audio);

	switch (enc->channels) {
	case 1:
		mode = MODE_1;
		break;
	case 2:
		mode = MODE_2;
		break;
	case 3:
		mode = MODE_1_2;
		break;
	case 4:
		mode = MODE_1_2_1;
		break;
	case 5:
		mode = MODE_1_2_2;
		break;
	case 6:
		mode = MODE_1_2_2_1;
		break;

		/* lib_fdk-aac > 1.3 required for 7.1 surround;
         * uncomment if available on linux build
         */
#ifndef __linux__
	case 8:
		mode = MODE_7_1_REAR_SURROUND;
		break;
#endif

	default:
		blog(LOG_ERROR, "Invalid channel count");
		goto fail;
	}

	CHECK_LIBFDK(aacEncOpen(&enc->fdkhandle, 0, enc->channels));
	hasFdkHandle = true;

	CHECK_LIBFDK(aacEncoder_SetParam(enc->fdkhandle, AACENC_AOT,
					 2)); // MPEG-4 AAC-LC
	CHECK_LIBFDK(aacEncoder_SetParam(enc->fdkhandle, AACENC_SAMPLERATE,
					 enc->sample_rate));
	CHECK_LIBFDK(
		aacEncoder_SetParam(enc->fdkhandle, AACENC_CHANNELMODE, mode));
	CHECK_LIBFDK(
		aacEncoder_SetParam(enc->fdkhandle, AACENC_CHANNELORDER, 1));
	CHECK_LIBFDK(
		aacEncoder_SetParam(enc->fdkhandle, AACENC_BITRATEMODE, 0));
	CHECK_LIBFDK(
		aacEncoder_SetParam(enc->fdkhandle, AACENC_BITRATE, bitrate));
	CHECK_LIBFDK(aacEncoder_SetParam(enc->fdkhandle, AACENC_TRANSMUX, 0));
	CHECK_LIBFDK(aacEncoder_SetParam(enc->fdkhandle, AACENC_AFTERBURNER,
					 afterburner));

	CHECK_LIBFDK(aacEncEncode(enc->fdkhandle, NULL, NULL, NULL, NULL));

	CHECK_LIBFDK(aacEncInfo(enc->fdkhandle, &enc->info));

	enc->frame_size_bytes = enc->info.frameLength * 2 * enc->channels;

	enc->packet_buffer_size = enc->channels * 768;
	if (enc->packet_buffer_size < 8192)
		enc->packet_buffer_size = 8192;

	enc->packet_buffer = bmalloc(enc->packet_buffer_size);

	blog(LOG_INFO, "libfdk_aac encoder created");

	blog(LOG_INFO, "libfdk_aac bitrate: %d, channels: %d", bitrate / 1000,
	     enc->channels);

	return enc;

fail:

	if (hasFdkHandle)
		aacEncClose(&enc->fdkhandle);

	if (enc->packet_buffer)
		bfree(enc->packet_buffer);

	if (enc)
		bfree(enc);

	blog(LOG_WARNING, "libfdk_aac encoder creation failed");

	return 0;
}

static void libfdk_destroy(void *data)
{
	libfdk_encoder_t *enc = data;

	aacEncClose(&enc->fdkhandle);

	bfree(enc->packet_buffer);
	bfree(enc);

	blog(LOG_INFO, "libfdk_aac encoder destroyed");
}

static bool libfdk_encode(void *data, struct encoder_frame *frame,
			  struct encoder_packet *packet, bool *received_packet)
{
	libfdk_encoder_t *enc = data;

	AACENC_BufDesc in_buf = {0};
	AACENC_BufDesc out_buf = {0};
	AACENC_InArgs in_args = {0};
	AACENC_OutArgs out_args = {0};
	int in_identifier = IN_AUDIO_DATA;
	int in_size, in_elem_size;
	int out_identifier = OUT_BITSTREAM_DATA;
	int out_size, out_elem_size;
	void *in_ptr;
	void *out_ptr;
	AACENC_ERROR err;
	int64_t encoderDelay;

	in_ptr = frame->data[0];
	in_size = enc->frame_size_bytes;
	in_elem_size = 2;

	in_args.numInSamples = enc->info.frameLength * enc->channels;
	in_buf.numBufs = 1;
	in_buf.bufs = &in_ptr;
	in_buf.bufferIdentifiers = &in_identifier;
	in_buf.bufSizes = &in_size;
	in_buf.bufElSizes = &in_elem_size;

	out_ptr = enc->packet_buffer;
	out_size = enc->packet_buffer_size;
	out_elem_size = 1;

	out_buf.numBufs = 1;
	out_buf.bufs = &out_ptr;
	out_buf.bufferIdentifiers = &out_identifier;
	out_buf.bufSizes = &out_size;
	out_buf.bufElSizes = &out_elem_size;

	if ((err = aacEncEncode(enc->fdkhandle, &in_buf, &out_buf, &in_args,
				&out_args)) != AACENC_OK) {
		blog(LOG_ERROR, "Failed to encode frame: %s",
		     libfdk_get_error(err));
		return false;
	}

	enc->total_samples += enc->info.frameLength;

	if (out_args.numOutBytes == 0) {
		*received_packet = false;
		return true;
	}

	*received_packet = true;
#if (AACENCODER_LIB_VL0 >= 4)
	encoderDelay = enc->info.nDelay;
#else
	encoderDelay = enc->info.encoderDelay;
#endif
	packet->pts = enc->total_samples - encoderDelay;
	packet->dts = enc->total_samples - encoderDelay;
	packet->data = enc->packet_buffer;
	packet->size = out_args.numOutBytes;
	packet->type = OBS_ENCODER_AUDIO;
	packet->timebase_num = 1;
	packet->timebase_den = enc->sample_rate;

	return true;
}

static bool libfdk_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	libfdk_encoder_t *enc = data;

	*size = enc->info.confSize;
	*extra_data = enc->info.confBuf;

	return true;
}

static void libfdk_audio_info(void *data, struct audio_convert_info *info)
{
	UNUSED_PARAMETER(data);
	info->format = AUDIO_FORMAT_16BIT;
}

static size_t libfdk_frame_size(void *data)
{
	libfdk_encoder_t *enc = data;

	return enc->info.frameLength;
}

struct obs_encoder_info obs_libfdk_encoder = {
	.id = "libfdk_aac",
	.type = OBS_ENCODER_AUDIO,
	.codec = "AAC",
	.get_name = libfdk_getname,
	.create = libfdk_create,
	.destroy = libfdk_destroy,
	.encode = libfdk_encode,
	.get_frame_size = libfdk_frame_size,
	.get_defaults = libfdk_defaults,
	.get_properties = libfdk_properties,
	.get_extra_data = libfdk_extra_data,
	.get_audio_info = libfdk_audio_info,
};

bool obs_module_load(void)
{
	obs_register_encoder(&obs_libfdk_encoder);
	return true;
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-libfdk", "en-US")
