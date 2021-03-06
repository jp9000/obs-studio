#include <util/dstr.hpp>
#include <obs-module.h>

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <vector>

#ifndef _WIN32
#include <AudioToolbox/AudioToolbox.h>
#include <util/apple/cfstring-utils.h>
#endif

#define CA_LOG(level, format, ...) \
	blog(level, "[CoreAudio encoder]: " format, ##__VA_ARGS__)
#define CA_LOG_ENCODER(format_name, encoder, level, format, ...)  \
	blog(level, "[CoreAudio %s: '%s']: " format, format_name, \
	     obs_encoder_get_name(encoder), ##__VA_ARGS__)
#define CA_BLOG(level, format, ...)                                 \
	CA_LOG_ENCODER(ca->format_name, ca->encoder, level, format, \
		       ##__VA_ARGS__)
#define CA_CO_LOG(level, format, ...)                          \
	do {                                                   \
		if (ca)                                        \
			CA_BLOG(level, format, ##__VA_ARGS__); \
		else                                           \
			CA_LOG(level, format, ##__VA_ARGS__);  \
	} while (false)

#ifdef _WIN32
#include "windows-imports.h"
#endif

using namespace std;

namespace {

struct asbd_builder {
	AudioStreamBasicDescription asbd;

	asbd_builder &sample_rate(Float64 rate)
	{
		asbd.mSampleRate = rate;
		return *this;
	}

	asbd_builder &format_id(UInt32 format)
	{
		asbd.mFormatID = format;
		return *this;
	}

	asbd_builder &format_flags(UInt32 flags)
	{
		asbd.mFormatFlags = flags;
		return *this;
	}

	asbd_builder &bytes_per_packet(UInt32 bytes)
	{
		asbd.mBytesPerPacket = bytes;
		return *this;
	}

	asbd_builder &frames_per_packet(UInt32 frames)
	{
		asbd.mFramesPerPacket = frames;
		return *this;
	}

	asbd_builder &bytes_per_frame(UInt32 bytes)
	{
		asbd.mBytesPerFrame = bytes;
		return *this;
	}

	asbd_builder &channels_per_frame(UInt32 channels)
	{
		asbd.mChannelsPerFrame = channels;
		return *this;
	}

	asbd_builder &bits_per_channel(UInt32 bits)
	{
		asbd.mBitsPerChannel = bits;
		return *this;
	}
};

struct ca_encoder {
	obs_encoder_t *encoder = nullptr;
	const char *format_name = nullptr;
	UInt32 format_id = 0;

	const initializer_list<UInt32> *allowed_formats = nullptr;

	AudioConverterRef converter = nullptr;

	size_t output_buffer_size = 0;
	vector<uint8_t> output_buffer;

	size_t out_frames_per_packet = 0;

	size_t in_packets = 0;
	size_t in_frame_size = 0;
	size_t in_bytes_required = 0;

	vector<uint8_t> input_buffer;
	vector<uint8_t> encode_buffer;

	uint64_t total_samples = 0;
	uint64_t samples_per_second = 0;

	vector<uint8_t> extra_data;

	size_t channels = 0;

	~ca_encoder()
	{
		if (converter)
			AudioConverterDispose(converter);
	}
};
typedef struct ca_encoder ca_encoder;

}

namespace std {

#ifndef _WIN32
template<> struct default_delete<remove_pointer<CFErrorRef>::type> {
	void operator()(remove_pointer<CFErrorRef>::type *err)
	{
		CFRelease(err);
	}
};

template<> struct default_delete<remove_pointer<CFStringRef>::type> {
	void operator()(remove_pointer<CFStringRef>::type *str)
	{
		CFRelease(str);
	}
};
#endif

template<> struct default_delete<remove_pointer<AudioConverterRef>::type> {
	void operator()(AudioConverterRef converter)
	{
		AudioConverterDispose(converter);
	}
};

}

template<typename T>
using cf_ptr = unique_ptr<typename remove_pointer<T>::type>;

#ifndef _MSC_VER
__attribute__((__format__(__printf__, 3, 4)))
#endif
static void
log_to_dstr(DStr &str, ca_encoder *ca, const char *fmt, ...)
{
	dstr prev_str = *static_cast<dstr *>(str);

	va_list args;
	va_start(args, fmt);
	dstr_vcatf(str, fmt, args);
	va_end(args);

	if (str->array)
		return;

	char array[4096];
	va_start(args, fmt);
	vsnprintf(array, 4096, fmt, args);
	va_end(args);

	array[4095] = 0;

	if (!prev_str.array && !prev_str.len)
		CA_CO_LOG(LOG_ERROR,
			  "Could not allocate buffer for logging:"
			  "\n'%s'",
			  array);
	else
		CA_CO_LOG(LOG_ERROR,
			  "Could not allocate buffer for logging:"
			  "\n'%s'\nPrevious log entries:\n%s",
			  array, prev_str.array);

	bfree(prev_str.array);
}

static const char *flush_log(DStr &log)
{
	if (!log->array || !log->len)
		return "";

	if (log->array[log->len - 1] == '\n') {
		log->array[log->len - 1] = 0; //Get rid of last newline
		log->len -= 1;
	}

	return log->array;
}

#define CA_CO_DLOG_(level, format) \
	CA_CO_LOG(level, format "%s%s", log->array ? ":\n" : "", flush_log(log))
#define CA_CO_DLOG(level, format, ...)                 \
	CA_CO_LOG(level, format "%s%s", ##__VA_ARGS__, \
		  log->array ? ":\n" : "", flush_log(log))

static const char *aac_get_name(void *)
{
	return obs_module_text("CoreAudioAAC");
}

static const char *code_to_str(OSStatus code)
{
	switch (code) {
#define HANDLE_CODE(c) \
	case c:        \
		return #c
		HANDLE_CODE(kAudio_UnimplementedError);
		HANDLE_CODE(kAudio_FileNotFoundError);
		HANDLE_CODE(kAudio_FilePermissionError);
		HANDLE_CODE(kAudio_TooManyFilesOpenError);
		HANDLE_CODE(kAudio_BadFilePathError);
		HANDLE_CODE(kAudio_ParamError);
		HANDLE_CODE(kAudio_MemFullError);

		HANDLE_CODE(kAudioConverterErr_FormatNotSupported);
		HANDLE_CODE(kAudioConverterErr_OperationNotSupported);
		HANDLE_CODE(kAudioConverterErr_PropertyNotSupported);
		HANDLE_CODE(kAudioConverterErr_InvalidInputSize);
		HANDLE_CODE(kAudioConverterErr_InvalidOutputSize);
		HANDLE_CODE(kAudioConverterErr_UnspecifiedError);
		HANDLE_CODE(kAudioConverterErr_BadPropertySizeError);
		HANDLE_CODE(kAudioConverterErr_RequiresPacketDescriptionsError);
		HANDLE_CODE(kAudioConverterErr_InputSampleRateOutOfRange);
		HANDLE_CODE(kAudioConverterErr_OutputSampleRateOutOfRange);
#undef HANDLE_CODE

	default:
		break;
	}

	return NULL;
}

static DStr osstatus_to_dstr(OSStatus code)
{
	DStr result;

#ifndef _WIN32
	cf_ptr<CFErrorRef> err{CFErrorCreate(
		kCFAllocatorDefault, kCFErrorDomainOSStatus, code, NULL)};
	cf_ptr<CFStringRef> str{CFErrorCopyDescription(err.get())};

	if (cfstr_copy_dstr(str.get(), kCFStringEncodingUTF8, result))
		return result;
#endif

	const char *code_str = code_to_str(code);
	dstr_printf(result, "%s%s%d%s", code_str ? code_str : "",
		    code_str ? " (" : "", static_cast<int>(code),
		    code_str ? ")" : "");
	return result;
}

static void log_osstatus(int log_level, ca_encoder *ca, const char *context,
			 OSStatus code)
{
	DStr str = osstatus_to_dstr(code);
	if (ca)
		CA_BLOG(log_level, "Error in %s: %s", context, str->array);
	else
		CA_LOG(log_level, "Error in %s: %s", context, str->array);
}

static const char *format_id_to_str(UInt32 format_id)
{
#define FORMAT_TO_STR(x) \
	case x:          \
		return #x
	switch (format_id) {
		FORMAT_TO_STR(kAudioFormatLinearPCM);
		FORMAT_TO_STR(kAudioFormatAC3);
		FORMAT_TO_STR(kAudioFormat60958AC3);
		FORMAT_TO_STR(kAudioFormatAppleIMA4);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC);
		FORMAT_TO_STR(kAudioFormatMPEG4CELP);
		FORMAT_TO_STR(kAudioFormatMPEG4HVXC);
		FORMAT_TO_STR(kAudioFormatMPEG4TwinVQ);
		FORMAT_TO_STR(kAudioFormatMACE3);
		FORMAT_TO_STR(kAudioFormatMACE6);
		FORMAT_TO_STR(kAudioFormatULaw);
		FORMAT_TO_STR(kAudioFormatALaw);
		FORMAT_TO_STR(kAudioFormatQDesign);
		FORMAT_TO_STR(kAudioFormatQDesign2);
		FORMAT_TO_STR(kAudioFormatQUALCOMM);
		FORMAT_TO_STR(kAudioFormatMPEGLayer1);
		FORMAT_TO_STR(kAudioFormatMPEGLayer2);
		FORMAT_TO_STR(kAudioFormatMPEGLayer3);
		FORMAT_TO_STR(kAudioFormatTimeCode);
		FORMAT_TO_STR(kAudioFormatMIDIStream);
		FORMAT_TO_STR(kAudioFormatParameterValueStream);
		FORMAT_TO_STR(kAudioFormatAppleLossless);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_HE);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_LD);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_ELD);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_ELD_SBR);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_HE_V2);
		FORMAT_TO_STR(kAudioFormatMPEG4AAC_Spatial);
		FORMAT_TO_STR(kAudioFormatAMR);
		FORMAT_TO_STR(kAudioFormatAudible);
		FORMAT_TO_STR(kAudioFormatiLBC);
		FORMAT_TO_STR(kAudioFormatDVIIntelIMA);
		FORMAT_TO_STR(kAudioFormatMicrosoftGSM);
		FORMAT_TO_STR(kAudioFormatAES3);
	}
#undef FORMAT_TO_STR

	return "Unknown format";
}

static void aac_destroy(void *data)
{
	ca_encoder *ca = static_cast<ca_encoder *>(data);

	delete ca;
}

template<typename Func>
static bool query_converter_property_raw(DStr &log, ca_encoder *ca,
					 AudioFormatPropertyID property,
					 const char *get_property_info,
					 const char *get_property,
					 AudioConverterRef converter,
					 Func &&func)
{
	UInt32 size = 0;
	OSStatus code = AudioConverterGetPropertyInfo(converter, property,
						      &size, nullptr);
	if (code) {
		log_to_dstr(log, ca, "%s: %s\n", get_property_info,
			    osstatus_to_dstr(code)->array);
		return false;
	}

	if (!size) {
		log_to_dstr(log, ca, "%s returned 0 size\n", get_property_info);
		return false;
	}

	vector<uint8_t> buffer;

	try {
		buffer.resize(size);
	} catch (...) {
		log_to_dstr(log, ca, "Failed to allocate %u bytes for %s\n",
			    static_cast<uint32_t>(size), get_property);
		return false;
	}

	code = AudioConverterGetProperty(converter, property, &size,
					 buffer.data());
	if (code) {
		log_to_dstr(log, ca, "%s: %s\n", get_property,
			    osstatus_to_dstr(code)->array);
		return false;
	}

	func(size, static_cast<void *>(buffer.data()));

	return true;
}

#define EXPAND_CONVERTER_NAMES(x)                   \
	x, "AudioConverterGetPropertyInfo(" #x ")", \
		"AudioConverterGetProperty(" #x ")"

template<typename Func>
static bool enumerate_bitrates(DStr &log, ca_encoder *ca,
			       AudioConverterRef converter, Func &&func)
{
	auto helper = [&](UInt32 size, void *data) {
		auto range = static_cast<AudioValueRange *>(data);
		size_t num_ranges = size / sizeof(AudioValueRange);
		for (size_t i = 0; i < num_ranges; i++)
			func(static_cast<UInt32>(range[i].mMinimum),
			     static_cast<UInt32>(range[i].mMaximum));
	};

	return query_converter_property_raw(
		log, ca,
		EXPAND_CONVERTER_NAMES(kAudioConverterApplicableEncodeBitRates),
		converter, helper);
}

static bool bitrate_valid(DStr &log, ca_encoder *ca,
			  AudioConverterRef converter, UInt32 bitrate)
{
	bool valid = false;

	auto helper = [&](UInt32 min_, UInt32 max_) {
		if (min_ == bitrate || max_ == bitrate)
			valid = true;
	};

	enumerate_bitrates(log, ca, converter, helper);

	return valid;
}

static bool create_encoder(DStr &log, ca_encoder *ca,
			   AudioStreamBasicDescription *in,
			   AudioStreamBasicDescription *out, UInt32 format_id,
			   UInt32 bitrate, UInt32 samplerate,
			   UInt32 rate_control)
{
#define STATUS_CHECK(c)                                     \
	code = c;                                           \
	if (code) {                                         \
		log_to_dstr(log, ca, #c " returned %s",     \
			    osstatus_to_dstr(code)->array); \
		return false;                               \
	}

	Float64 srate = samplerate ? (Float64)samplerate
				   : (Float64)ca->samples_per_second;

	auto out_ = asbd_builder()
			    .sample_rate(srate)
			    .channels_per_frame((UInt32)ca->channels)
			    .format_id(format_id)
			    .asbd;

	UInt32 size = sizeof(*out);
	OSStatus code;
	STATUS_CHECK(AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0,
					    NULL, &size, &out_));

	*out = out_;

	STATUS_CHECK(AudioConverterNew(in, out, &ca->converter))

	STATUS_CHECK(AudioConverterSetProperty(
		ca->converter, kAudioCodecPropertyBitRateControlMode,
		sizeof(rate_control), &rate_control));

	if (!bitrate_valid(log, ca, ca->converter, bitrate)) {
		log_to_dstr(log, ca,
			    "Encoder does not support bitrate %u "
			    "for format %s (0x%x)\n",
			    (uint32_t)bitrate, format_id_to_str(format_id),
			    (uint32_t)format_id);
		return false;
	}

	ca->format_id = format_id;

	return true;
#undef STATUS_CHECK
}

static const initializer_list<UInt32> aac_formats = {
	kAudioFormatMPEG4AAC_HE_V2,
	kAudioFormatMPEG4AAC_HE,
	kAudioFormatMPEG4AAC,
};

static const initializer_list<UInt32> aac_lc_formats = {
	kAudioFormatMPEG4AAC,
};

static void *aac_create(obs_data_t *settings, obs_encoder_t *encoder)
{
#define STATUS_CHECK(c)                                      \
	code = c;                                            \
	if (code) {                                          \
		log_osstatus(LOG_ERROR, ca.get(), #c, code); \
		return nullptr;                              \
	}

	UInt32 bitrate = (UInt32)obs_data_get_int(settings, "bitrate") * 1000;
	if (!bitrate) {
		CA_LOG_ENCODER("AAC", encoder, LOG_ERROR,
			       "Invalid bitrate specified");
		return NULL;
	}

	const enum audio_format format = AUDIO_FORMAT_FLOAT;

	if (is_audio_planar(format)) {
		CA_LOG_ENCODER("AAC", encoder, LOG_ERROR,
			       "Got non-interleaved audio format %d", format);
		return NULL;
	}

	unique_ptr<ca_encoder> ca;

	try {
		ca.reset(new ca_encoder());
	} catch (...) {
		CA_LOG_ENCODER("AAC", encoder, LOG_ERROR,
			       "Could not allocate encoder");
		return nullptr;
	}

	ca->encoder = encoder;
	ca->format_name = "AAC";

	audio_t *audio = obs_encoder_audio(encoder);
	const struct audio_output_info *aoi = audio_output_get_info(audio);

	ca->channels = audio_output_get_channels(audio);
	ca->samples_per_second = audio_output_get_sample_rate(audio);

	size_t bytes_per_frame = get_audio_size(format, aoi->speakers, 1);
	size_t bits_per_channel = get_audio_bytes_per_channel(format) * 8;

	auto in = asbd_builder()
			  .sample_rate((Float64)ca->samples_per_second)
			  .channels_per_frame((UInt32)ca->channels)
			  .bytes_per_frame((UInt32)bytes_per_frame)
			  .frames_per_packet(1)
			  .bytes_per_packet((UInt32)(1 * bytes_per_frame))
			  .bits_per_channel((UInt32)bits_per_channel)
			  .format_id(kAudioFormatLinearPCM)
			  .format_flags(kAudioFormatFlagsNativeEndian |
					kAudioFormatFlagIsPacked |
					kAudioFormatFlagIsFloat | 0)
			  .asbd;

	AudioStreamBasicDescription out;

	UInt32 rate_control = kAudioCodecBitRateControlMode_Constant;

	if (obs_data_get_bool(settings, "allow he-aac") && ca->channels != 3) {
		ca->allowed_formats = &aac_formats;
	} else {
		ca->allowed_formats = &aac_lc_formats;
	}

	auto samplerate =
		static_cast<UInt32>(obs_data_get_int(settings, "samplerate"));

	DStr log;

	bool encoder_created = false;
	for (UInt32 format_id : *ca->allowed_formats) {
		log_to_dstr(log, ca.get(), "Trying format %s (0x%x)\n",
			    format_id_to_str(format_id), (uint32_t)format_id);

		if (!create_encoder(log, ca.get(), &in, &out, format_id,
				    bitrate, samplerate, rate_control))
			continue;

		encoder_created = true;
		break;
	}

	if (!encoder_created) {
		CA_CO_DLOG(LOG_ERROR,
			   "Could not create encoder for "
			   "selected format%s",
			   ca->allowed_formats->size() == 1 ? "" : "s");
		return nullptr;
	}

	if (log->len)
		CA_CO_DLOG_(LOG_DEBUG, "Encoder created");

	OSStatus code;
	UInt32 converter_quality = kAudioConverterQuality_Max;
	STATUS_CHECK(AudioConverterSetProperty(
		ca->converter, kAudioConverterCodecQuality,
		sizeof(converter_quality), &converter_quality));

	STATUS_CHECK(AudioConverterSetProperty(ca->converter,
					       kAudioConverterEncodeBitRate,
					       sizeof(bitrate), &bitrate));

	UInt32 size = sizeof(in);
	STATUS_CHECK(AudioConverterGetProperty(
		ca->converter, kAudioConverterCurrentInputStreamDescription,
		&size, &in));

	size = sizeof(out);
	STATUS_CHECK(AudioConverterGetProperty(
		ca->converter, kAudioConverterCurrentOutputStreamDescription,
		&size, &out));

	/*
	 * Fix channel map differences between CoreAudio AAC, FFmpeg, Wav
	 * New channel mappings below assume 2.1, 4.0, 4.1, 5.1, 7.1 resp.
	 */

	if (ca->channels == 3) {
		SInt32 channelMap3[3] = {2, 0, 1};
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterChannelMap,
					  sizeof(channelMap3), channelMap3);

	} else if (ca->channels == 4) {
		/*
		 * For four channels coreaudio encoder has default channel "quad"
		 * instead of 4.0. So explicitly set channel layout to
		 * kAudioChannelLayoutTag_MPEG_4_0_B = (116L << 16) | 4.
		 */
		AudioChannelLayout inAcl = {0};
		inAcl.mChannelLayoutTag = (116L << 16) | 4;
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterInputChannelLayout,
					  sizeof(inAcl), &inAcl);
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterOutputChannelLayout,
					  sizeof(inAcl), &inAcl);
		SInt32 channelMap4[4] = {2, 0, 1, 3};
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterChannelMap,
					  sizeof(channelMap4), channelMap4);

	} else if (ca->channels == 5) {
		SInt32 channelMap5[5] = {2, 0, 1, 3, 4};
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterChannelMap,
					  sizeof(channelMap5), channelMap5);

	} else if (ca->channels == 6) {
		SInt32 channelMap6[6] = {2, 0, 1, 4, 5, 3};
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterChannelMap,
					  sizeof(channelMap6), channelMap6);

	} else if (ca->channels == 8) {
		SInt32 channelMap8[8] = {2, 0, 1, 6, 7, 4, 5, 3};
		AudioConverterSetProperty(ca->converter,
					  kAudioConverterChannelMap,
					  sizeof(channelMap8), channelMap8);
	}

	ca->in_frame_size = in.mBytesPerFrame;
	ca->in_packets = out.mFramesPerPacket / in.mFramesPerPacket;
	ca->in_bytes_required = ca->in_packets * ca->in_frame_size;

	ca->out_frames_per_packet = out.mFramesPerPacket;

	ca->output_buffer_size = out.mBytesPerPacket;

	if (out.mBytesPerPacket == 0) {
		UInt32 max_packet_size = 0;
		size = sizeof(max_packet_size);

		code = AudioConverterGetProperty(
			ca->converter,
			kAudioConverterPropertyMaximumOutputPacketSize, &size,
			&max_packet_size);
		if (code) {
			log_osstatus(LOG_WARNING, ca.get(),
				     "AudioConverterGetProperty(PacketSz)",
				     code);
			ca->output_buffer_size = 32768;
		} else {
			ca->output_buffer_size = max_packet_size;
		}
	}

	try {
		ca->output_buffer.resize(ca->output_buffer_size);
	} catch (...) {
		CA_BLOG(LOG_ERROR, "Failed to allocate output buffer");
		return nullptr;
	}

	const char *format_name =
		out.mFormatID == kAudioFormatMPEG4AAC_HE_V2
			? "HE-AAC v2"
			: out.mFormatID == kAudioFormatMPEG4AAC_HE ? "HE-AAC"
								   : "AAC";
	CA_BLOG(LOG_INFO,
		"settings:\n"
		"\tmode:          %s\n"
		"\tbitrate:       %u\n"
		"\tsample rate:   %llu\n"
		"\tcbr:           %s\n"
		"\toutput buffer: %lu",
		format_name, (unsigned int)bitrate / 1000,
		ca->samples_per_second,
		rate_control == kAudioCodecBitRateControlMode_Constant ? "on"
								       : "off",
		(unsigned long)ca->output_buffer_size);

	return ca.release();
#undef STATUS_CHECK
}

static OSStatus
complex_input_data_proc(AudioConverterRef inAudioConverter,
			UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
			AudioStreamPacketDescription **outDataPacketDescription,
			void *inUserData)
{
	UNUSED_PARAMETER(inAudioConverter);
	UNUSED_PARAMETER(outDataPacketDescription);

	ca_encoder *ca = static_cast<ca_encoder *>(inUserData);

	if (ca->input_buffer.size() < ca->in_bytes_required) {
		*ioNumberDataPackets = 0;
		ioData->mBuffers[0].mData = NULL;
		return 1;
	}

	auto start = begin(ca->input_buffer);
	auto stop = begin(ca->input_buffer) + ca->in_bytes_required;
	ca->encode_buffer.assign(start, stop);
	ca->input_buffer.erase(start, stop);

	*ioNumberDataPackets =
		(UInt32)(ca->in_bytes_required / ca->in_frame_size);
	ioData->mNumberBuffers = 1;

	ioData->mBuffers[0].mData = ca->encode_buffer.data();
	ioData->mBuffers[0].mNumberChannels = (UInt32)ca->channels;
	ioData->mBuffers[0].mDataByteSize = (UInt32)ca->in_bytes_required;

	return 0;
}

#ifdef _MSC_VER
// disable warning that recommends if ((foo = bar > 0) == false) over
// if (!(foo = bar > 0))
#pragma warning(push)
#pragma warning(disable : 4706)
#endif
static bool aac_encode(void *data, struct encoder_frame *frame,
		       struct encoder_packet *packet, bool *received_packet)
{
	ca_encoder *ca = static_cast<ca_encoder *>(data);

	ca->input_buffer.insert(end(ca->input_buffer), frame->data[0],
				frame->data[0] + frame->linesize[0]);

	if (ca->input_buffer.size() < ca->in_bytes_required)
		return true;

	UInt32 packets = 1;

	AudioBufferList buffer_list = {0};
	buffer_list.mNumberBuffers = 1;
	buffer_list.mBuffers[0].mNumberChannels = (UInt32)ca->channels;
	buffer_list.mBuffers[0].mDataByteSize = (UInt32)ca->output_buffer_size;
	buffer_list.mBuffers[0].mData = ca->output_buffer.data();

	AudioStreamPacketDescription out_desc = {0};

	OSStatus code = AudioConverterFillComplexBuffer(
		ca->converter, complex_input_data_proc, ca, &packets,
		&buffer_list, &out_desc);
	if (code && code != 1) {
		log_osstatus(LOG_ERROR, ca, "AudioConverterFillComplexBuffer",
			     code);
		return false;
	}

	if (!(*received_packet = packets > 0))
		return true;

	packet->pts = ca->total_samples;
	packet->dts = ca->total_samples;
	packet->timebase_num = 1;
	packet->timebase_den = (uint32_t)ca->samples_per_second;
	packet->type = OBS_ENCODER_AUDIO;
	packet->size = out_desc.mDataByteSize;
	packet->data = (uint8_t *)buffer_list.mBuffers[0].mData +
		       out_desc.mStartOffset;

	ca->total_samples += ca->in_bytes_required / ca->in_frame_size;

	return true;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static void aac_audio_info(void *data, struct audio_convert_info *info)
{
	UNUSED_PARAMETER(data);

	info->format = AUDIO_FORMAT_FLOAT;
}

static size_t aac_frame_size(void *data)
{
	ca_encoder *ca = static_cast<ca_encoder *>(data);
	return ca->out_frames_per_packet;
}

/* The following code was extracted from encca_aac.c in HandBrake's libhb */
#define MP4ESDescrTag 0x03
#define MP4DecConfigDescrTag 0x04
#define MP4DecSpecificDescrTag 0x05

// based off of mov_mp4_read_descr_len from mov.c in ffmpeg's libavformat
static int read_descr_len(uint8_t **buffer)
{
	int len = 0;
	int count = 4;
	while (count--) {
		int c = *(*buffer)++;
		len = (len << 7) | (c & 0x7f);
		if (!(c & 0x80))
			break;
	}
	return len;
}

// based off of mov_mp4_read_descr from mov.c in ffmpeg's libavformat
static int read_descr(uint8_t **buffer, int *tag)
{
	*tag = *(*buffer)++;
	return read_descr_len(buffer);
}

// based off of mov_read_esds from mov.c in ffmpeg's libavformat
static void read_esds_desc_ext(uint8_t *desc_ext, vector<uint8_t> &buffer,
			       bool version_flags)
{
	uint8_t *esds = desc_ext;
	int tag, len;

	if (version_flags)
		esds += 4; // version + flags

	read_descr(&esds, &tag);
	esds += 2; // ID
	if (tag == MP4ESDescrTag)
		esds++; // priority

	read_descr(&esds, &tag);
	if (tag == MP4DecConfigDescrTag) {
		esds++;    // object type id
		esds++;    // stream type
		esds += 3; // buffer size db
		esds += 4; // max bitrate
		esds += 4; // average bitrate

		len = read_descr(&esds, &tag);
		if (tag == MP4DecSpecificDescrTag)
			try {
				buffer.assign(esds, esds + len);
			} catch (...) {
				//leave buffer empty
			}
	}
}
/* extracted code ends here */

static void query_extra_data(ca_encoder *ca)
{
	UInt32 size = 0;

	OSStatus code;
	code = AudioConverterGetPropertyInfo(
		ca->converter, kAudioConverterCompressionMagicCookie, &size,
		NULL);
	if (code) {
		log_osstatus(LOG_ERROR, ca,
			     "AudioConverterGetPropertyInfo(magic_cookie)",
			     code);
		return;
	}

	if (!size) {
		CA_BLOG(LOG_WARNING, "Got 0 data size info for magic_cookie");
		return;
	}

	vector<uint8_t> extra_data;

	try {
		extra_data.resize(size);
	} catch (...) {
		CA_BLOG(LOG_WARNING, "Could not allocate extra data buffer");
		return;
	}

	code = AudioConverterGetProperty(ca->converter,
					 kAudioConverterCompressionMagicCookie,
					 &size, extra_data.data());
	if (code) {
		log_osstatus(LOG_ERROR, ca,
			     "AudioConverterGetProperty(magic_cookie)", code);
		return;
	}

	if (!size) {
		CA_BLOG(LOG_WARNING, "Got 0 data size for magic_cookie");
		return;
	}

	read_esds_desc_ext(extra_data.data(), ca->extra_data, false);
}

static bool aac_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	ca_encoder *ca = static_cast<ca_encoder *>(data);

	if (!ca->extra_data.size())
		query_extra_data(ca);

	if (!ca->extra_data.size())
		return false;

	*extra_data = ca->extra_data.data();
	*size = ca->extra_data.size();
	return true;
}

static asbd_builder fill_common_asbd_fields(asbd_builder builder,
					    bool in = false,
					    UInt32 channels = 2)
{
	UInt32 bytes_per_frame = sizeof(float) * channels;
	UInt32 bits_per_channel = bytes_per_frame / channels * 8;

	builder.channels_per_frame(channels);

	if (in) {
		builder.bytes_per_frame(bytes_per_frame)
			.frames_per_packet(1)
			.bytes_per_packet(1 * bytes_per_frame)
			.bits_per_channel(bits_per_channel);
	}

	return builder;
}

static AudioStreamBasicDescription get_default_in_asbd()
{
	return fill_common_asbd_fields(asbd_builder(), true)
		.sample_rate(44100)
		.format_id(kAudioFormatLinearPCM)
		.format_flags(kAudioFormatFlagsNativeEndian |
			      kAudioFormatFlagIsPacked |
			      kAudioFormatFlagIsFloat | 0)
		.asbd;
}

static asbd_builder get_default_out_asbd_builder(UInt32 channels)
{
	return fill_common_asbd_fields(asbd_builder(), false, channels)
		.sample_rate(44100);
}

static cf_ptr<AudioConverterRef>
get_converter(DStr &log, ca_encoder *ca, AudioStreamBasicDescription out,
	      AudioStreamBasicDescription in = get_default_in_asbd())
{
	UInt32 size = sizeof(out);
	OSStatus code;

#define STATUS_CHECK(x)                                     \
	code = x;                                           \
	if (code) {                                         \
		log_to_dstr(log, ca, "%s: %s\n", #x,        \
			    osstatus_to_dstr(code)->array); \
		return nullptr;                             \
	}

	STATUS_CHECK(AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0,
					    NULL, &size, &out));

	AudioConverterRef converter;
	STATUS_CHECK(AudioConverterNew(&in, &out, &converter));

	return cf_ptr<AudioConverterRef>{converter};
#undef STATUS_CHECK
}

static bool find_best_match(DStr &log, ca_encoder *ca, UInt32 bitrate,
			    UInt32 &best_match)
{
	UInt32 actual_bitrate = bitrate * 1000;
	bool found_match = false;

	auto handle_bitrate = [&](UInt32 candidate) {
		if (abs(static_cast<intmax_t>(actual_bitrate - candidate)) <
		    abs(static_cast<intmax_t>(actual_bitrate - best_match))) {
			log_to_dstr(log, ca, "Found new best match %u\n",
				    static_cast<uint32_t>(candidate));

			found_match = true;
			best_match = candidate;
		}
	};

	auto helper = [&](UInt32 min_, UInt32 max_) {
		handle_bitrate(min_);

		if (min_ == max_)
			return;

		log_to_dstr(log, ca, "Got actual bit rate range: %u<->%u\n",
			    static_cast<uint32_t>(min_),
			    static_cast<uint32_t>(max_));

		handle_bitrate(max_);
	};

	for (UInt32 format_id : aac_formats) {
		log_to_dstr(log, ca, "Trying %s (0x%x)\n",
			    format_id_to_str(format_id), format_id);

		auto out = get_default_out_asbd_builder(2)
				   .format_id(format_id)
				   .asbd;

		auto converter = get_converter(log, ca, out);

		if (converter)
			enumerate_bitrates(log, ca, converter.get(), helper);
		else
			log_to_dstr(log, ca, "Could not get converter\n");
	}

	best_match /= 1000;

	return found_match;
}

static UInt32 find_matching_bitrate(UInt32 bitrate)
{
	static UInt32 match = bitrate;

	static once_flag once;

	call_once(once, [&]() {
		DStr log;
		ca_encoder *ca = nullptr;

		if (!find_best_match(log, ca, bitrate, match)) {
			CA_CO_DLOG(LOG_ERROR,
				   "No matching bitrates found for "
				   "target bitrate %u",
				   static_cast<uint32_t>(bitrate));

			match = bitrate;
			return;
		}

		if (match != bitrate) {
			CA_CO_DLOG(LOG_INFO,
				   "Default bitrate (%u) isn't "
				   "supported, returning %u as closest match",
				   static_cast<uint32_t>(bitrate),
				   static_cast<uint32_t>(match));
			return;
		}

		if (log->len)
			CA_CO_DLOG(LOG_DEBUG,
				   "Default bitrate matching log "
				   "for bitrate %u",
				   static_cast<uint32_t>(bitrate));
	});

	return match;
}

static void aac_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "samplerate", 0); //match input
	obs_data_set_default_int(settings, "bitrate",
				 find_matching_bitrate(128));
	obs_data_set_default_bool(settings, "allow he-aac", true);
}

template<typename Func>
static bool
query_property_raw(DStr &log, ca_encoder *ca, AudioFormatPropertyID property,
		   const char *get_property_info, const char *get_property,
		   AudioStreamBasicDescription &desc, Func &&func)
{
	UInt32 size = 0;
	OSStatus code = AudioFormatGetPropertyInfo(
		property, sizeof(AudioStreamBasicDescription), &desc, &size);
	if (code) {
		log_to_dstr(log, ca, "%s: %s\n", get_property_info,
			    osstatus_to_dstr(code)->array);
		return false;
	}

	if (!size) {
		log_to_dstr(log, ca, "%s returned 0 size\n", get_property_info);
		return false;
	}

	vector<uint8_t> buffer;

	try {
		buffer.resize(size);
	} catch (...) {
		log_to_dstr(log, ca, "Failed to allocate %u bytes for %s\n",
			    static_cast<uint32_t>(size), get_property);
		return false;
	}

	code = AudioFormatGetProperty(property,
				      sizeof(AudioStreamBasicDescription),
				      &desc, &size, buffer.data());
	if (code) {
		log_to_dstr(log, ca, "%s: %s\n", get_property,
			    osstatus_to_dstr(code)->array);
		return false;
	}

	func(size, static_cast<void *>(buffer.data()));

	return true;
}

#define EXPAND_PROPERTY_NAMES(x)                 \
	x, "AudioFormatGetPropertyInfo(" #x ")", \
		"AudioFormatGetProperty(" #x ")"

template<typename Func>
static bool enumerate_samplerates(DStr &log, ca_encoder *ca,
				  AudioStreamBasicDescription &desc,
				  Func &&func)
{
	auto helper = [&](UInt32 size, void *data) {
		auto range = static_cast<AudioValueRange *>(data);
		size_t num_ranges = size / sizeof(AudioValueRange);
		for (size_t i = 0; i < num_ranges; i++)
			func(range[i]);
	};

	return query_property_raw(
		log, ca,
		EXPAND_PROPERTY_NAMES(
			kAudioFormatProperty_AvailableEncodeSampleRates),
		desc, helper);
}

#if 0
// Unused because it returns bitrates that aren't actually usable, i.e.
// Available bitrates vs Applicable bitrates

template <typename Func>
static bool enumerate_bitrates(DStr &log, ca_encoder *ca,
		AudioStreamBasicDescription &desc, Func &&func)
{
	auto helper = [&](UInt32 size, void *data)
	{
		auto range = static_cast<AudioValueRange*>(data);
		size_t num_ranges = size / sizeof(AudioValueRange);
		for (size_t i = 0; i < num_ranges; i++)
			func(range[i]);
	};

	return query_property_raw(log, ca, EXPAND_PROPERTY_NAMES(
			kAudioFormatProperty_AvailableEncodeBitRates),
			desc, helper);
}
#endif

static vector<UInt32> get_samplerates(DStr &log, ca_encoder *ca)
{
	vector<UInt32> samplerates;

	auto handle_samplerate = [&](UInt32 rate) {
		if (find(begin(samplerates), end(samplerates), rate) ==
		    end(samplerates)) {
			log_to_dstr(log, ca, "Adding sample rate %u\n",
				    static_cast<uint32_t>(rate));
			samplerates.push_back(rate);
		} else {
			log_to_dstr(log, ca, "Sample rate %u already added\n",
				    static_cast<uint32_t>(rate));
		}
	};

	auto helper = [&](const AudioValueRange &range) {
		auto min_ = static_cast<UInt32>(range.mMinimum);
		auto max_ = static_cast<UInt32>(range.mMaximum);

		handle_samplerate(min_);

		if (min_ == max_)
			return;

		log_to_dstr(log, ca, "Got actual sample rate range: %u<->%u\n",
			    static_cast<uint32_t>(min_),
			    static_cast<uint32_t>(max_));

		handle_samplerate(max_);
	};

	for (UInt32 format : (ca ? *ca->allowed_formats : aac_formats)) {
		log_to_dstr(log, ca, "Trying %s (0x%x)\n",
			    format_id_to_str(format),
			    static_cast<uint32_t>(format));

		auto asbd = asbd_builder().format_id(format).asbd;

		enumerate_samplerates(log, ca, asbd, helper);
	}

	return samplerates;
}

static void add_samplerates(obs_property_t *prop, ca_encoder *ca)
{
	obs_property_list_add_int(prop, obs_module_text("UseInputSampleRate"),
				  0);

	DStr log;

	auto samplerates = get_samplerates(log, ca);

	if (!samplerates.size()) {
		CA_CO_DLOG_(LOG_ERROR, "Couldn't find available sample rates");
		return;
	}

	if (log->len)
		CA_CO_DLOG_(LOG_DEBUG, "Sample rate enumeration log");

	sort(begin(samplerates), end(samplerates));

	DStr buffer;
	for (UInt32 samplerate : samplerates) {
		dstr_printf(buffer, "%d", static_cast<uint32_t>(samplerate));
		obs_property_list_add_int(prop, buffer->array, samplerate);
	}
}

#define NBSP "\xC2\xA0"

static vector<UInt32> get_bitrates(DStr &log, ca_encoder *ca,
				   Float64 samplerate)
{
	vector<UInt32> bitrates;
	struct obs_audio_info aoi;
	int channels;

	obs_get_audio_info(&aoi);
	channels = get_audio_channels(aoi.speakers);

	auto handle_bitrate = [&](UInt32 bitrate) {
		if (find(begin(bitrates), end(bitrates), bitrate) ==
		    end(bitrates)) {
			log_to_dstr(log, ca, "Adding bitrate %u\n",
				    static_cast<uint32_t>(bitrate));
			bitrates.push_back(bitrate);
		} else {
			log_to_dstr(log, ca, "Bitrate %u already added\n",
				    static_cast<uint32_t>(bitrate));
		}
	};

	auto helper = [&](UInt32 min_, UInt32 max_) {
		handle_bitrate(min_);

		if (min_ == max_)
			return;

		log_to_dstr(log, ca, "Got actual bitrate range: %u<->%u\n",
			    static_cast<uint32_t>(min_),
			    static_cast<uint32_t>(max_));

		handle_bitrate(max_);
	};

	for (UInt32 format_id : (ca ? *ca->allowed_formats : aac_formats)) {
		log_to_dstr(log, ca, "Trying %s (0x%x) at %g" NBSP "hz\n",
			    format_id_to_str(format_id),
			    static_cast<uint32_t>(format_id), samplerate);

		auto out = get_default_out_asbd_builder(channels)
				   .format_id(format_id)
				   .sample_rate(samplerate)
				   .asbd;

		auto converter = get_converter(log, ca, out);

		if (converter)
			enumerate_bitrates(log, ca, converter.get(), helper);
	}

	return bitrates;
}

static void add_bitrates(obs_property_t *prop, ca_encoder *ca,
			 Float64 samplerate = 44100.,
			 UInt32 *selected = nullptr)
{
	obs_property_list_clear(prop);

	DStr log;

	auto bitrates = get_bitrates(log, ca, samplerate);

	if (!bitrates.size()) {
		CA_CO_DLOG_(LOG_ERROR, "Couldn't find available bitrates");
		return;
	}

	if (log->len)
		CA_CO_DLOG_(LOG_DEBUG, "Bitrate enumeration log");

	bool selected_in_range = true;
	if (selected) {
		selected_in_range = find(begin(bitrates), end(bitrates),
					 *selected * 1000) != end(bitrates);

		if (!selected_in_range)
			bitrates.push_back(*selected * 1000);
	}

	sort(begin(bitrates), end(bitrates));

	DStr buffer;
	for (UInt32 bitrate : bitrates) {
		dstr_printf(buffer, "%u", (uint32_t)bitrate / 1000);
		size_t idx = obs_property_list_add_int(prop, buffer->array,
						       bitrate / 1000);

		if (selected_in_range || bitrate / 1000 != *selected)
			continue;

		obs_property_list_item_disable(prop, idx, true);
	}
}

static bool samplerate_updated(obs_properties_t *props, obs_property_t *prop,
			       obs_data_t *settings)
{
	auto samplerate =
		static_cast<UInt32>(obs_data_get_int(settings, "samplerate"));
	if (!samplerate)
		samplerate = 44100;

	prop = obs_properties_get(props, "bitrate");
	if (prop) {
		auto bitrate = static_cast<UInt32>(
			obs_data_get_int(settings, "bitrate"));

		add_bitrates(prop, nullptr, samplerate, &bitrate);

		return true;
	}

	return false;
}

static obs_properties_t *aac_properties(void *data)
{
	ca_encoder *ca = static_cast<ca_encoder *>(data);

	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_list(
		props, "samplerate", obs_module_text("OutputSamplerate"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	add_samplerates(p, ca);
	obs_property_set_modified_callback(p, samplerate_updated);

	p = obs_properties_add_list(props, "bitrate",
				    obs_module_text("Bitrate"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	add_bitrates(p, ca);

	obs_properties_add_bool(props, "allow he-aac",
				obs_module_text("AllowHEAAC"));

	return props;
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("coreaudio-encoder", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Apple CoreAudio based encoder";
}

bool obs_module_load(void)
{
#ifdef _WIN32
	if (!load_core_audio()) {
		CA_LOG(LOG_WARNING, "CoreAudio AAC encoder not installed on "
				    "the system or couldn't be loaded");
		return true;
	}

	CA_LOG(LOG_INFO, "Adding CoreAudio AAC encoder");
#endif

	struct obs_encoder_info aac_info {
	};
	aac_info.id = "CoreAudio_AAC";
	aac_info.type = OBS_ENCODER_AUDIO;
	aac_info.codec = "AAC";
	aac_info.get_name = aac_get_name;
	aac_info.destroy = aac_destroy;
	aac_info.create = aac_create;
	aac_info.encode = aac_encode;
	aac_info.get_frame_size = aac_frame_size;
	aac_info.get_audio_info = aac_audio_info;
	aac_info.get_extra_data = aac_extra_data;
	aac_info.get_defaults = aac_defaults;
	aac_info.get_properties = aac_properties;

	obs_register_encoder(&aac_info);
	return true;
}

#ifdef _WIN32
void obs_module_unload(void)
{
	unload_core_audio();
}
#endif
