#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMediaIO/CMIOHardware.h>

#include <obs-module.h>
#include <obs.hpp>
#include <media-io/video-io.h>

#include <util/dstr.hpp>

#include <algorithm>
#include <initializer_list>
#include <cinttypes>
#include <limits>
#include <memory>
#include <vector>

#include "left-right.hpp"
#include "scope-guard.hpp"

#define NBSP "\xC2\xA0"

using namespace std;

namespace std {

template<> struct default_delete<obs_data_t> {
	void operator()(obs_data_t *data) { obs_data_release(data); }
};

template<> struct default_delete<obs_data_item_t> {
	void operator()(obs_data_item_t *item) { obs_data_item_release(&item); }
};

}

#define TEXT_AVCAPTURE obs_module_text("AVCapture")
#define TEXT_DEVICE obs_module_text("Device")
#define TEXT_USE_PRESET obs_module_text("UsePreset")
#define TEXT_PRESET obs_module_text("Preset")
#define TEXT_RESOLUTION obs_module_text("Resolution")
#define TEXT_FRAME_RATE obs_module_text("FrameRate")
#define TEXT_MATCH_OBS obs_module_text("MatchOBS")
#define TEXT_INPUT_FORMAT obs_module_text("InputFormat")
#define TEXT_COLOR_SPACE obs_module_text("ColorSpace")
#define TEXT_VIDEO_RANGE obs_module_text("VideoRange")
#define TEXT_RANGE_PARTIAL obs_module_text("VideoRange.Partial")
#define TEXT_RANGE_FULL obs_module_text("VideoRange.Full")
#define TEXT_AUTO obs_module_text("Auto")

#define TEXT_COLOR_UNKNOWN_NAME "Unknown"
#define TEXT_RANGE_UNKNOWN_NAME "Unknown"

static const FourCharCode INPUT_FORMAT_AUTO = -1;
static const int COLOR_SPACE_AUTO = -1;
static const int VIDEO_RANGE_AUTO = -1;

#define MILLI_TIMESCALE 1000
#define MICRO_TIMESCALE (MILLI_TIMESCALE * 1000)
#define NANO_TIMESCALE (MICRO_TIMESCALE * 1000)

#define AV_FOURCC_STR(code)                                     \
	(char[5])                                               \
	{                                                       \
		static_cast<char>((code >> 24) & 0xFF),         \
			static_cast<char>((code >> 16) & 0xFF), \
			static_cast<char>((code >> 8) & 0xFF),  \
			static_cast<char>(code & 0xFF), 0       \
	}

struct av_capture;

#define AVLOG(level, format, ...)                                        \
	blog(level, "%s: " format, obs_source_get_name(capture->source), \
	     ##__VA_ARGS__)

@interface OBSAVCaptureDelegate
	: NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
@public
	struct av_capture *capture;
}
- (void)captureOutput:(AVCaptureOutput *)out
	didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
	     fromConnection:(AVCaptureConnection *)connection;
- (void)captureOutput:(AVCaptureOutput *)captureOutput
	didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
	       fromConnection:(AVCaptureConnection *)connection;
@end

namespace {

static auto remove_observer = [](id observer) {
	[[NSNotificationCenter defaultCenter] removeObserver:observer];
};

struct observer_handle
	: unique_ptr<remove_pointer<id>::type, decltype(remove_observer)> {

	using base =
		unique_ptr<remove_pointer<id>::type, decltype(remove_observer)>;

	explicit observer_handle(id observer = nullptr)
		: base(observer, remove_observer)
	{
	}
};

struct av_video_info {
	video_colorspace colorspace;
	video_range_type video_range;
	bool video_params_valid = false;
};

}

struct av_capture {
	OBSAVCaptureDelegate *delegate;
	dispatch_queue_t queue;
	bool has_clock;

	bool device_locked;

	left_right::left_right<av_video_info> video_info;

	AVCaptureVideoDataOutput *out;
	AVCaptureDevice *device;
	AVCaptureDeviceInput *device_input;
	AVCaptureSession *session;

	NSString *uid;
	observer_handle connect_observer;
	observer_handle disconnect_observer;

	FourCharCode fourcc;
	video_format video_format;

	bool use_preset = false;
	int requested_colorspace = COLOR_SPACE_AUTO;
	int requested_video_range = VIDEO_RANGE_AUTO;

	obs_source_t *source;

	obs_source_frame frame;
};

static NSString *get_string(obs_data_t *data, char const *name)
{
	return @(obs_data_get_string(data, name));
}

static AVCaptureDevice *get_device(obs_data_t *settings)
{
	auto uid = get_string(settings, "device");
	return [AVCaptureDevice deviceWithUniqueID:uid];
}

template<typename T, typename U>
static void clamp(T &store, U val, T low = numeric_limits<T>::min(),
		  T high = numeric_limits<T>::max())
{
	store = static_cast<intmax_t>(val) < static_cast<intmax_t>(low)
			? low
			: (static_cast<intmax_t>(val) >
					   static_cast<intmax_t>(high)
				   ? high
				   : static_cast<T>(val));
}

static bool get_resolution(obs_data_t *settings, CMVideoDimensions &dims)
{
	using item_ptr = unique_ptr<obs_data_item_t>;
	item_ptr item{obs_data_item_byname(settings, "resolution")};
	if (!item)
		return false;

	auto res_str = obs_data_item_get_string(item.get());
	unique_ptr<obs_data_t> res{obs_data_create_from_json(res_str)};
	if (!res)
		return false;

	item_ptr width{obs_data_item_byname(res.get(), "width")};
	item_ptr height{obs_data_item_byname(res.get(), "height")};

	if (!width || !height)
		return false;

	clamp(dims.width, obs_data_item_get_int(width.get()), 0);
	clamp(dims.height, obs_data_item_get_int(height.get()), 0);

	if (!dims.width || !dims.height)
		return false;

	return true;
}

static bool get_input_format(obs_data_t *settings, FourCharCode &fourcc)
{
	auto item = unique_ptr<obs_data_item_t>{
		obs_data_item_byname(settings, "input_format")};
	if (!item)
		return false;

	fourcc = obs_data_item_get_int(item.get());
	return true;
}

namespace {

struct config_helper {
	obs_data_t *settings = nullptr;

	AVCaptureDevice *dev_ = nullptr;

	bool dims_valid : 1;
	bool fr_valid : 1;
	bool fps_valid : 1;
	bool if_valid : 1;

	CMVideoDimensions dims_{};

	const char *frame_rate_ = nullptr;
	media_frames_per_second fps_{};

	FourCharCode input_format_ = INPUT_FORMAT_AUTO;

	explicit config_helper(obs_data_t *settings) : settings(settings)
	{
		dev_ = get_device(settings);

		dims_valid = get_resolution(settings, dims_);

		fr_valid = obs_data_get_frames_per_second(
			settings, "frame_rate", nullptr, &frame_rate_);
		fps_valid = obs_data_get_frames_per_second(
			settings, "frame_rate", &fps_, nullptr);

		if_valid = get_input_format(settings, input_format_);
	}

	AVCaptureDevice *dev() const { return dev_; }

	const CMVideoDimensions *dims() const
	{
		return dims_valid ? &dims_ : nullptr;
	}

	const char *frame_rate() const
	{
		return fr_valid ? frame_rate_ : nullptr;
	}

	const media_frames_per_second *fps() const
	{
		return fps_valid ? &fps_ : nullptr;
	}

	const FourCharCode *input_format() const
	{
		return if_valid ? &input_format_ : nullptr;
	}
};

struct av_capture_ref {
	av_capture *capture = nullptr;
	OBSSource source;

	av_capture_ref() = default;

	av_capture_ref(av_capture *capture_, obs_weak_source_t *weak_source)
		: source(OBSGetStrongRef(weak_source))
	{
		if (!source)
			return;

		capture = capture_;
	}

	operator av_capture *() { return capture; }

	av_capture *operator->() { return capture; }
};

struct properties_param {
	av_capture *capture = nullptr;
	OBSWeakSource weak_source;

	properties_param(av_capture *capture) : capture(capture)
	{
		if (!capture)
			return;

		weak_source = OBSGetWeakRef(capture->source);
	}

	av_capture_ref get_ref() { return {capture, weak_source}; }
};

}

static av_capture_ref get_ref(obs_properties_t *props)
{
	void *param = obs_properties_get_param(props);
	if (!param)
		return {};

	return static_cast<properties_param *>(param)->get_ref();
}

static inline video_format format_from_subtype(FourCharCode subtype)
{
	//TODO: uncomment VIDEO_FORMAT_NV12 and VIDEO_FORMAT_ARGB once libobs
	//      gains matching GPU conversions or a CPU fallback is implemented
	switch (subtype) {
	case kCVPixelFormatType_422YpCbCr8:
		return VIDEO_FORMAT_UYVY;
	case kCVPixelFormatType_422YpCbCr8_yuvs:
		return VIDEO_FORMAT_YUY2;
	/*case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
	case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
		return VIDEO_FORMAT_NV12;*/
	/*case kCVPixelFormatType_32ARGB:
		return VIDEO_FORMAT_ARGB;*/
	case kCVPixelFormatType_32BGRA:
		return VIDEO_FORMAT_BGRA;
	default:
		return VIDEO_FORMAT_NONE;
	}
}

static const char *fourcc_subtype_name(FourCharCode fourcc);

static const char *
format_description_subtype_name(CMFormatDescriptionRef desc,
				FourCharCode *fourcc_ = nullptr)
{
	FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc);
	if (fourcc_)
		*fourcc_ = fourcc;

	return fourcc_subtype_name(fourcc);
}

static const char *fourcc_subtype_name(FourCharCode fourcc)
{
	switch (fourcc) {
	case kCVPixelFormatType_422YpCbCr8:
		return "UYVY - 422YpCbCr8"; //VIDEO_FORMAT_UYVY;
	case kCVPixelFormatType_422YpCbCr8_yuvs:
		return "YUY2 - 422YpCbCr8_yuvs"; //VIDEO_FORMAT_YUY2;
	case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
	case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
		return "NV12 - 420YpCbCr8BiPlanar"; //VIDEO_FORMAT_NV12;
	case kCVPixelFormatType_32ARGB:
		return "ARGB - 32ARGB"; //VIDEO_FORMAT_ARGB;*/
	case kCVPixelFormatType_32BGRA:
		return "BGRA - 32BGRA"; //VIDEO_FORMAT_BGRA;

	case kCMVideoCodecType_Animation:
		return "Apple Animation";
	case kCMVideoCodecType_Cinepak:
		return "Cinepak";
	case kCMVideoCodecType_JPEG:
		return "JPEG";
	case kCMVideoCodecType_JPEG_OpenDML:
		return "MJPEG - JPEG OpenDML";
	case kCMVideoCodecType_SorensonVideo:
		return "Sorenson Video";
	case kCMVideoCodecType_SorensonVideo3:
		return "Sorenson Video 3";
	case kCMVideoCodecType_H263:
		return "H.263";
	case kCMVideoCodecType_H264:
		return "H.264";
	case kCMVideoCodecType_MPEG4Video:
		return "MPEG-4";
	case kCMVideoCodecType_MPEG2Video:
		return "MPEG-2";
	case kCMVideoCodecType_MPEG1Video:
		return "MPEG-1";

	case kCMVideoCodecType_DVCNTSC:
		return "DV NTSC";
	case kCMVideoCodecType_DVCPAL:
		return "DV PAL";
	case kCMVideoCodecType_DVCProPAL:
		return "Panasonic DVCPro PAL";
	case kCMVideoCodecType_DVCPro50NTSC:
		return "Panasonic DVCPro-50 NTSC";
	case kCMVideoCodecType_DVCPro50PAL:
		return "Panasonic DVCPro-50 PAL";
	case kCMVideoCodecType_DVCPROHD720p60:
		return "Panasonic DVCPro-HD 720p60";
	case kCMVideoCodecType_DVCPROHD720p50:
		return "Panasonic DVCPro-HD 720p50";
	case kCMVideoCodecType_DVCPROHD1080i60:
		return "Panasonic DVCPro-HD 1080i60";
	case kCMVideoCodecType_DVCPROHD1080i50:
		return "Panasonic DVCPro-HD 1080i50";
	case kCMVideoCodecType_DVCPROHD1080p30:
		return "Panasonic DVCPro-HD 1080p30";
	case kCMVideoCodecType_DVCPROHD1080p25:
		return "Panasonic DVCPro-HD 1080p25";

	case kCMVideoCodecType_AppleProRes4444:
		return "Apple ProRes 4444";
	case kCMVideoCodecType_AppleProRes422HQ:
		return "Apple ProRes 422 HQ";
	case kCMVideoCodecType_AppleProRes422:
		return "Apple ProRes 422";
	case kCMVideoCodecType_AppleProRes422LT:
		return "Apple ProRes 422 LT";
	case kCMVideoCodecType_AppleProRes422Proxy:
		return "Apple ProRes 422 Proxy";

	default:
		blog(LOG_INFO, "Unknown format %s", AV_FOURCC_STR(fourcc));
		return "unknown";
	}
}

static inline bool is_fullrange_yuv(FourCharCode pixel_format)
{
	switch (pixel_format) {
	case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
	case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
	case kCVPixelFormatType_422YpCbCr8FullRange:
		return true;

	default:
		return false;
	}
}

static inline video_colorspace get_colorspace(CMFormatDescriptionRef desc)
{
	CFPropertyListRef matrix = CMFormatDescriptionGetExtension(
		desc, kCMFormatDescriptionExtension_YCbCrMatrix);

	if (!matrix)
		return VIDEO_CS_DEFAULT;

	if (CFStringCompare(static_cast<CFStringRef>(matrix),
			    kCVImageBufferYCbCrMatrix_ITU_R_601_4,
			    0) == kCFCompareEqualTo)
		return VIDEO_CS_601;

	return VIDEO_CS_709;
}

static inline bool update_colorspace(av_capture *capture,
				     obs_source_frame *frame,
				     CMFormatDescriptionRef desc,
				     bool full_range, av_video_info &vi)
{
	auto cs_auto = capture->use_preset ||
		       capture->requested_colorspace == COLOR_SPACE_AUTO;
	auto vr_auto = capture->use_preset ||
		       capture->requested_video_range == VIDEO_RANGE_AUTO;

	video_colorspace colorspace = get_colorspace(desc);
	video_range_type range = full_range ? VIDEO_RANGE_FULL
					    : VIDEO_RANGE_PARTIAL;

	bool cs_matches = false;
	if (cs_auto) {
		cs_matches = colorspace == vi.colorspace;
	} else {
		colorspace = static_cast<video_colorspace>(
			capture->requested_colorspace);
		cs_matches = colorspace == vi.colorspace;
	}

	bool vr_matches = false;
	if (vr_auto) {
		vr_matches = range == vi.video_range;
	} else {
		range = static_cast<video_range_type>(
			capture->requested_video_range);
		vr_matches = range == vi.video_range;
		full_range = range == VIDEO_RANGE_FULL;
	}

	if (cs_matches && vr_matches) {
		if (!vi.video_params_valid)
			capture->video_info.update([&](av_video_info &vi_) {
				vi_.video_params_valid = vi.video_params_valid =
					true;
			});
		return true;
	}

	frame->full_range = full_range;

	if (!video_format_get_parameters(colorspace, range, frame->color_matrix,
					 frame->color_range_min,
					 frame->color_range_max)) {
		AVLOG(LOG_ERROR,
		      "Failed to get colorspace parameters for "
		      "colorspace %u range %u",
		      colorspace, range);

		if (vi.video_params_valid)
			capture->video_info.update([&](av_video_info &vi_) {
				vi_.video_params_valid = vi.video_params_valid =
					false;
			});

		return false;
	}

	capture->video_info.update([&](av_video_info &vi_) {
		vi_.colorspace = colorspace;
		vi_.video_range = range;
		vi_.video_params_valid = vi.video_params_valid = true;
	});

	return true;
}

static inline bool update_frame(av_capture *capture, obs_source_frame *frame,
				CMSampleBufferRef sample_buffer)
{
	CMFormatDescriptionRef desc =
		CMSampleBufferGetFormatDescription(sample_buffer);

	FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(desc);
	video_format format = format_from_subtype(fourcc);
	CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(desc);

	CVImageBufferRef img = CMSampleBufferGetImageBuffer(sample_buffer);

	auto vi = capture->video_info.read();

	bool video_params_were_valid = vi.video_params_valid;
	SCOPE_EXIT
	{
		if (video_params_were_valid != vi.video_params_valid)
			obs_source_update_properties(capture->source);
	};

	if (format == VIDEO_FORMAT_NONE) {
		if (capture->fourcc == fourcc)
			return false;

		capture->fourcc = fourcc;
		AVLOG(LOG_ERROR, "Unhandled fourcc: %s (0x%x) (%zu planes)",
		      AV_FOURCC_STR(fourcc), fourcc,
		      CVPixelBufferGetPlaneCount(img));
		return false;
	}

	if (frame->format != format)
		AVLOG(LOG_DEBUG,
		      "Switching fourcc: "
		      "'%s' (0x%x) -> '%s' (0x%x)",
		      AV_FOURCC_STR(capture->fourcc), capture->fourcc,
		      AV_FOURCC_STR(fourcc), fourcc);

	bool was_yuv = format_is_yuv(frame->format);

	capture->fourcc = fourcc;
	frame->format = format;
	frame->width = dims.width;
	frame->height = dims.height;

	if (format_is_yuv(format) &&
	    !update_colorspace(capture, frame, desc, is_fullrange_yuv(fourcc),
			       vi)) {
		return false;
	} else if (was_yuv == format_is_yuv(format)) {
		capture->video_info.update([&](av_video_info &vi_) {
			vi_.video_params_valid = vi.video_params_valid = true;
		});
	}

	CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);

	if (!CVPixelBufferIsPlanar(img)) {
		frame->linesize[0] = CVPixelBufferGetBytesPerRow(img);
		frame->data[0] = static_cast<uint8_t *>(
			CVPixelBufferGetBaseAddress(img));
		return true;
	}

	size_t count = CVPixelBufferGetPlaneCount(img);
	for (size_t i = 0; i < count; i++) {
		frame->linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(img, i);
		frame->data[i] = static_cast<uint8_t *>(
			CVPixelBufferGetBaseAddressOfPlane(img, i));
	}
	return true;
}

@implementation OBSAVCaptureDelegate
- (void)captureOutput:(AVCaptureOutput *)out
	didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
	     fromConnection:(AVCaptureConnection *)connection
{
	UNUSED_PARAMETER(out);
	UNUSED_PARAMETER(sampleBuffer);
	UNUSED_PARAMETER(connection);
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
	didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
	       fromConnection:(AVCaptureConnection *)connection
{
	UNUSED_PARAMETER(captureOutput);
	UNUSED_PARAMETER(connection);

	CMItemCount count = CMSampleBufferGetNumSamples(sampleBuffer);
	if (count < 1 || !capture)
		return;

	obs_source_frame *frame = &capture->frame;

	CMTime target_pts =
		CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
	CMTime target_pts_nano = CMTimeConvertScale(
		target_pts, NANO_TIMESCALE, kCMTimeRoundingMethod_Default);
	frame->timestamp = target_pts_nano.value;

	if (!update_frame(capture, frame, sampleBuffer)) {
		obs_source_output_video(capture->source, nullptr);
		return;
	}

	obs_source_output_video(capture->source, frame);

	CVImageBufferRef img = CMSampleBufferGetImageBuffer(sampleBuffer);
	CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
}
@end

static void av_capture_enable_buffering(av_capture *capture, bool enabled)
{
	obs_source_set_async_unbuffered(capture->source, !enabled);
}

static const char *av_capture_getname(void *)
{
	return TEXT_AVCAPTURE;
}

static void unlock_device(av_capture *capture, AVCaptureDevice *dev = nullptr)
{
	if (!dev)
		dev = capture->device;

	if (dev && capture->device_locked)
		[dev unlockForConfiguration];

	capture->device_locked = false;
}

static void start_capture(av_capture *capture)
{
	if (capture->session && !capture->session.running)
		[capture->session startRunning];
}

static void clear_capture(av_capture *capture)
{
	if (capture->session && capture->session.running)
		[capture->session stopRunning];

	obs_source_output_video(capture->source, nullptr);
}

static void remove_device(av_capture *capture)
{
	clear_capture(capture);

	[capture->session removeInput:capture->device_input];

	unlock_device(capture);

	capture->device_input = nullptr;
	capture->device = nullptr;
}

static void av_capture_destroy(void *data)
{
	auto capture = static_cast<av_capture *>(data);

	delete capture;
}

static bool init_session(av_capture *capture)
{
	auto session = [[AVCaptureSession alloc] init];
	if (!session) {
		AVLOG(LOG_ERROR, "Could not create AVCaptureSession");
		return false;
	}

	auto delegate = [[OBSAVCaptureDelegate alloc] init];
	if (!delegate) {
		AVLOG(LOG_ERROR, "Could not create OBSAVCaptureDelegate");
		return false;
	}

	delegate->capture = capture;

	auto out = [[AVCaptureVideoDataOutput alloc] init];
	if (!out) {
		AVLOG(LOG_ERROR, "Could not create AVCaptureVideoDataOutput");
		return false;
	}

	auto queue = dispatch_queue_create(NULL, NULL);
	if (!queue) {
		AVLOG(LOG_ERROR, "Could not create dispatch queue");
		return false;
	}

	capture->session = session;
	capture->delegate = delegate;
	capture->out = out;
	capture->queue = queue;

	[capture->session addOutput:capture->out];
	[capture->out setSampleBufferDelegate:capture->delegate
					queue:capture->queue];

	return true;
}

static bool init_format(av_capture *capture, AVCaptureDevice *dev);

static bool init_device_input(av_capture *capture, AVCaptureDevice *dev)
{
	NSError *err = nil;
	AVCaptureDeviceInput *device_input =
		[AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
	if (!device_input) {
		AVLOG(LOG_ERROR, "Error while initializing device input: %s",
		      err.localizedFailureReason.UTF8String);
		return false;
	}

	[capture->session addInput:device_input];

	if (!init_format(capture, dev)) {
		[capture->session removeInput:device_input];
		return false;
	}

	capture->device_input = device_input;

	return true;
}

static uint32_t uint_from_dict(NSDictionary *dict, CFStringRef key)
{
	return ((NSNumber *)dict[(__bridge NSString *)key]).unsignedIntValue;
}

static bool init_format(av_capture *capture, AVCaptureDevice *dev)
{
	AVCaptureDeviceFormat *format = dev.activeFormat;

	CMMediaType mtype =
		CMFormatDescriptionGetMediaType(format.formatDescription);
	// TODO: support other media types
	if (mtype != kCMMediaType_Video && mtype != kCMMediaType_Muxed) {
		AVLOG(LOG_ERROR, "CMMediaType '%s' is unsupported",
		      AV_FOURCC_STR(mtype));
		return false;
	}

	capture->out.videoSettings = nil;
	FourCharCode subtype = uint_from_dict(capture->out.videoSettings,
					      kCVPixelBufferPixelFormatTypeKey);
	if (format_from_subtype(subtype) != VIDEO_FORMAT_NONE) {
		AVLOG(LOG_DEBUG, "Using native fourcc '%s'",
		      AV_FOURCC_STR(subtype));
		return true;
	}

	AVLOG(LOG_DEBUG, "Using fallback fourcc '%s' ('%s' 0x%08x unsupported)",
	      AV_FOURCC_STR(kCVPixelFormatType_32BGRA), AV_FOURCC_STR(subtype),
	      subtype);

	capture->out.videoSettings = @{
		(__bridge NSString *)
		kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
	};
	return true;
}

static NSArray *presets(void);
static NSString *preset_names(NSString *preset);

static NSString *select_preset(AVCaptureDevice *dev, NSString *cur_preset)
{
	NSString *new_preset = nil;
	bool found_previous_preset = false;
	for (NSString *preset in presets().reverseObjectEnumerator) {
		if (!found_previous_preset)
			found_previous_preset =
				[cur_preset isEqualToString:preset];

		if (![dev supportsAVCaptureSessionPreset:preset])
			continue;

		if (!new_preset || !found_previous_preset)
			new_preset = preset;
	}

	return new_preset;
}

static bool init_preset(av_capture *capture, AVCaptureDevice *dev,
			obs_data_t *settings)
{
	clear_capture(capture);

	unlock_device(capture, dev);

	NSString *preset = get_string(settings, "preset");
	if (![dev supportsAVCaptureSessionPreset:preset]) {
		AVLOG(LOG_WARNING, "Preset %s not available",
		      preset_names(preset).UTF8String);
		preset = select_preset(dev, preset);
	}

	if (!preset) {
		AVLOG(LOG_WARNING, "Could not select a preset, "
				   "initialization failed");
		return false;
	}

	capture->session.sessionPreset = preset;
	AVLOG(LOG_INFO, "Using preset %s", preset_names(preset).UTF8String);

	return true;
}

static bool operator==(const CMVideoDimensions &a, const CMVideoDimensions &b);
static CMVideoDimensions get_dimensions(AVCaptureDeviceFormat *format);

static AVCaptureDeviceFormat *find_format(AVCaptureDevice *dev,
					  CMVideoDimensions dims)
{
	for (AVCaptureDeviceFormat *format in dev.formats) {
		if (get_dimensions(format) == dims)
			return format;
	}

	return nullptr;
}

static CMTime convert(media_frames_per_second fps)
{
	CMTime time{};
	time.value = fps.denominator;
	time.timescale = fps.numerator;
	time.flags = 1;
	return time;
}

static bool lock_device(av_capture *capture, AVCaptureDevice *dev)
{
	if (!dev)
		dev = capture->device;

	NSError *err;
	if (![dev lockForConfiguration:&err]) {
		AVLOG(LOG_WARNING,
		      "Could not lock device for configuration: "
		      "%s",
		      err.localizedDescription.UTF8String);
		return false;
	}

	capture->device_locked = true;
	return true;
}

template<typename Func>
static void find_formats(media_frames_per_second fps, AVCaptureDevice *dev,
			 const CMVideoDimensions *dims, Func &&f)
{
	auto time = convert(fps);

	for (AVCaptureDeviceFormat *format in dev.formats) {
		if (!(get_dimensions(format) == *dims))
			continue;

		for (AVFrameRateRange *range in format
			     .videoSupportedFrameRateRanges) {
			if (CMTimeCompare(range.maxFrameDuration, time) >= 0 &&
			    CMTimeCompare(range.minFrameDuration, time) <= 0)
				if (f(format))
					return;
		}
	}
}

static bool color_space_valid(int color_space)
{
	switch (color_space) {
	case COLOR_SPACE_AUTO:
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_601:
	case VIDEO_CS_709:
		return true;
	}

	return false;
}

static const char *color_space_name(int color_space)
{
	switch (color_space) {
	case COLOR_SPACE_AUTO:
		return "Auto";
	case VIDEO_CS_DEFAULT:
		return "Default";
	case VIDEO_CS_601:
		return "CS 601";
	case VIDEO_CS_709:
		return "CS 709";
	}

	return "Unknown";
}

static bool video_range_valid(int video_range)
{
	switch (video_range) {
	case VIDEO_RANGE_AUTO:
	case VIDEO_RANGE_DEFAULT:
	case VIDEO_RANGE_PARTIAL:
	case VIDEO_RANGE_FULL:
		return true;
	}

	return false;
}

static const char *video_range_name(int video_range)
{
	switch (video_range) {
	case VIDEO_RANGE_AUTO:
		return "Auto";
	case VIDEO_RANGE_DEFAULT:
		return "Default";
	case VIDEO_RANGE_PARTIAL:
		return "Partial";
	case VIDEO_RANGE_FULL:
		return "Full";
	}

	return "Unknown";
}

static bool init_manual(av_capture *capture, AVCaptureDevice *dev,
			obs_data_t *settings)
{
	clear_capture(capture);

	auto input_format = obs_data_get_int(settings, "input_format");
	FourCharCode actual_format = input_format;

	SCOPE_EXIT
	{
		bool refresh = false;
		if (input_format != actual_format) {
			refresh = obs_data_get_autoselect_int(settings,
							      "input_format") !=
				  actual_format;
			obs_data_set_autoselect_int(settings, "input_format",
						    actual_format);
		} else {
			refresh = obs_data_has_autoselect_value(settings,
								"input_format");
			obs_data_unset_autoselect_value(settings,
							"input_format");
		}

		if (refresh)
			obs_source_update_properties(capture->source);
	};

	capture->requested_colorspace =
		obs_data_get_int(settings, "color_space");
	if (!color_space_valid(capture->requested_colorspace)) {
		AVLOG(LOG_WARNING, "Unsupported color space: %d",
		      capture->requested_colorspace);
		return false;
	}

	capture->requested_video_range =
		obs_data_get_int(settings, "video_range");
	if (!video_range_valid(capture->requested_video_range)) {
		AVLOG(LOG_WARNING, "Unsupported color range: %d",
		      capture->requested_video_range);
		return false;
	}

	CMVideoDimensions dims{};
	if (!get_resolution(settings, dims)) {
		AVLOG(LOG_WARNING, "Could not load resolution");
		return false;
	}

	media_frames_per_second fps{};
	if (!obs_data_get_frames_per_second(settings, "frame_rate", &fps,
					    nullptr)) {
		AVLOG(LOG_WARNING, "Could not load frame rate");
		return false;
	}

	AVCaptureDeviceFormat *format = nullptr;
	find_formats(fps, dev, &dims, [&](AVCaptureDeviceFormat *format_) {
		auto desc = format_.formatDescription;
		auto fourcc = CMFormatDescriptionGetMediaSubType(desc);
		if (input_format != INPUT_FORMAT_AUTO && fourcc != input_format)
			return false;

		actual_format = fourcc;
		format = format_;
		return true;
	});

	if (!format) {
		AVLOG(LOG_WARNING,
		      "Frame rate is not supported: %g FPS "
		      "(%u/%u)",
		      media_frames_per_second_to_fps(fps), fps.numerator,
		      fps.denominator);
		return false;
	}

	if (!lock_device(capture, dev))
		return false;

	const char *if_name = input_format == INPUT_FORMAT_AUTO
				      ? "Auto"
				      : fourcc_subtype_name(input_format);

#define IF_AUTO(x) (input_format != INPUT_FORMAT_AUTO ? "" : x)
	AVLOG(LOG_INFO,
	      "Capturing '%s' (%s):\n"
	      "	Resolution: %ux%u\n"
	      "	FPS: %g (%" PRIu32 "/%" PRIu32 ")\n"
	      "	Frame interval: %g" NBSP "s\n"
	      "	Input format: %s%s%s (%s)%s\n"
	      "	Requested color space: %s (%d)\n"
	      "	Requested video range: %s (%d)\n"
	      "	Using format: %s",
	      dev.localizedName.UTF8String, dev.uniqueID.UTF8String, dims.width,
	      dims.height, media_frames_per_second_to_fps(fps), fps.numerator,
	      fps.denominator, media_frames_per_second_to_frame_interval(fps),
	      if_name, IF_AUTO(" (actual: "),
	      IF_AUTO(fourcc_subtype_name(actual_format)),
	      AV_FOURCC_STR(actual_format), IF_AUTO(")"),
	      color_space_name(capture->requested_colorspace),
	      capture->requested_colorspace,
	      video_range_name(capture->requested_video_range),
	      capture->requested_video_range, format.description.UTF8String);
#undef IF_AUTO

	dev.activeFormat = format;
	dev.activeVideoMinFrameDuration = convert(fps);
	dev.activeVideoMaxFrameDuration = convert(fps);

	capture->video_info.update(
		[&](av_video_info &vi) { vi.video_params_valid = false; });

	return true;
}

static void capture_device(av_capture *capture, AVCaptureDevice *dev,
			   obs_data_t *settings)
{
	const char *name = dev.localizedName.UTF8String;
	obs_data_set_string(settings, "device_name", name);
	obs_data_set_string(settings, "device", dev.uniqueID.UTF8String);
	AVLOG(LOG_INFO, "Selected device '%s'", name);

	if ((capture->use_preset = obs_data_get_bool(settings, "use_preset"))) {
		if (!init_preset(capture, dev, settings))
			return;

	} else {
		if (!init_manual(capture, dev, settings))
			return;
	}

	if (!init_device_input(capture, dev))
		return;

	AVCaptureInputPort *port = capture->device_input.ports[0];
	capture->has_clock = [port respondsToSelector:@selector(clock)];

	capture->device = dev;
	start_capture(capture);
	return;
}

static inline void handle_disconnect_capture(av_capture *capture,
					     AVCaptureDevice *dev)
{
	if (![dev.uniqueID isEqualTo:capture->uid])
		return;

	if (!capture->device) {
		AVLOG(LOG_INFO, "Received disconnect for inactive device '%s'",
		      capture->uid.UTF8String);
		return;
	}

	AVLOG(LOG_WARNING, "Device with unique ID '%s' disconnected",
	      dev.uniqueID.UTF8String);

	remove_device(capture);
}

static inline void handle_disconnect(av_capture *capture, AVCaptureDevice *dev)
{
	if (!dev)
		return;

	handle_disconnect_capture(capture, dev);
	obs_source_update_properties(capture->source);
}

static inline void handle_connect_capture(av_capture *capture,
					  AVCaptureDevice *dev,
					  obs_data_t *settings)
{
	if (![dev.uniqueID isEqualTo:capture->uid])
		return;

	if (capture->device) {
		AVLOG(LOG_ERROR, "Received connect for in-use device '%s'",
		      capture->uid.UTF8String);
		return;
	}

	AVLOG(LOG_INFO,
	      "Device with unique ID '%s' connected, "
	      "resuming capture",
	      dev.uniqueID.UTF8String);

	capture_device(capture, dev, settings);
}

static inline void handle_connect(av_capture *capture, AVCaptureDevice *dev,
				  obs_data_t *settings)
{
	if (!dev)
		return;

	handle_connect_capture(capture, dev, settings);
	obs_source_update_properties(capture->source);
}

static bool av_capture_init(av_capture *capture, obs_data_t *settings)
{
	if (!init_session(capture))
		return false;

	capture->uid = get_string(settings, "device");

	NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
	capture->disconnect_observer.reset([nc
		addObserverForName:AVCaptureDeviceWasDisconnectedNotification
			    object:nil
			     queue:[NSOperationQueue mainQueue]
			usingBlock:^(NSNotification *note) {
				handle_disconnect(capture, note.object);
			}]);

	capture->connect_observer.reset([nc
		addObserverForName:AVCaptureDeviceWasConnectedNotification
			    object:nil
			     queue:[NSOperationQueue mainQueue]
			usingBlock:^(NSNotification *note) {
				handle_connect(capture, note.object, settings);
			}]);

	AVCaptureDevice *dev =
		[AVCaptureDevice deviceWithUniqueID:capture->uid];

	if (!dev) {
		if (capture->uid.length < 1)
			AVLOG(LOG_INFO, "No device selected");
		else
			AVLOG(LOG_WARNING,
			      "Could not initialize device "
			      "with unique ID '%s'",
			      capture->uid.UTF8String);
		return true;
	}

	capture_device(capture, dev, settings);

	return true;
}

static void *av_capture_create(obs_data_t *settings, obs_source_t *source)
{
	unique_ptr<av_capture> capture;

	try {
		capture.reset(new av_capture());
	} catch (...) {
		return capture.release();
	}

	capture->source = source;

	if (!av_capture_init(capture.get(), settings)) {
		AVLOG(LOG_ERROR, "av_capture_init failed");
		return nullptr;
	}

	av_capture_enable_buffering(capture.get(),
				    obs_data_get_bool(settings, "buffering"));

	return capture.release();
}

static NSArray *presets(void)
{
	if (@available(macOS 10.15, *)) {
		return @[
			//AVCaptureSessionPresetiFrame1280x720,
			//AVCaptureSessionPresetiFrame960x540,
			AVCaptureSessionPreset3840x2160,
			AVCaptureSessionPreset1920x1080,
			AVCaptureSessionPreset1280x720,
			AVCaptureSessionPreset960x540,
			AVCaptureSessionPreset640x480,
			AVCaptureSessionPreset352x288,
			AVCaptureSessionPreset320x240,
			AVCaptureSessionPresetHigh,
			//AVCaptureSessionPresetMedium,
			//AVCaptureSessionPresetLow,
			//AVCaptureSessionPresetPhoto,
		];
	} else {
		return @[
			//AVCaptureSessionPresetiFrame1280x720,
			//AVCaptureSessionPresetiFrame960x540,
			AVCaptureSessionPreset1280x720,
			AVCaptureSessionPreset960x540,
			AVCaptureSessionPreset640x480,
			AVCaptureSessionPreset352x288,
			AVCaptureSessionPreset320x240,
			AVCaptureSessionPresetHigh,
			//AVCaptureSessionPresetMedium,
			//AVCaptureSessionPresetLow,
			//AVCaptureSessionPresetPhoto,
		];
	}
}

static NSString *preset_names(NSString *preset)
{
	NSDictionary *preset_names = nil;
	if (@available(macOS 10.15, *)) {
		preset_names = @{
			AVCaptureSessionPresetLow: @"Low",
			AVCaptureSessionPresetMedium: @"Medium",
			AVCaptureSessionPresetHigh: @"High",
			AVCaptureSessionPreset320x240: @"320x240",
			AVCaptureSessionPreset352x288: @"352x288",
			AVCaptureSessionPreset640x480: @"640x480",
			AVCaptureSessionPreset960x540: @"960x540",
			AVCaptureSessionPreset1280x720: @"1280x720",
			AVCaptureSessionPreset1920x1080: @"1920x1080",
			AVCaptureSessionPreset3840x2160: @"3840x2160",
			AVCaptureSessionPresetHigh: @"High",
		};
	} else {
		preset_names = @{
			AVCaptureSessionPresetLow: @"Low",
			AVCaptureSessionPresetMedium: @"Medium",
			AVCaptureSessionPresetHigh: @"High",
			AVCaptureSessionPreset320x240: @"320x240",
			AVCaptureSessionPreset352x288: @"352x288",
			AVCaptureSessionPreset640x480: @"640x480",
			AVCaptureSessionPreset960x540: @"960x540",
			AVCaptureSessionPreset1280x720: @"1280x720",
			AVCaptureSessionPresetHigh: @"High",
		};
	}
	NSString *name = preset_names[preset];
	if (name)
		return name;
	return [NSString stringWithFormat:@"Unknown (%@)", preset];
}

static void av_capture_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "uid", "");
	obs_data_set_default_bool(settings, "use_preset", true);

	obs_data_set_default_string(settings, "preset",
				    AVCaptureSessionPreset1280x720.UTF8String);

	obs_data_set_default_int(settings, "input_format", INPUT_FORMAT_AUTO);
	obs_data_set_default_int(settings, "color_space", COLOR_SPACE_AUTO);
	obs_data_set_default_int(settings, "video_range", VIDEO_RANGE_AUTO);
}

static bool update_device_list(obs_property_t *list, NSString *uid,
			       NSString *name, bool disconnected)
{
	bool dev_found = false;
	bool list_modified = false;

	size_t size = obs_property_list_item_count(list);
	for (size_t i = 0; i < size;) {
		const char *uid_ = obs_property_list_item_string(list, i);
		bool found = [uid isEqualToString:@(uid_ ? uid_ : "")];
		bool disabled = obs_property_list_item_disabled(list, i);
		if (!found && !disabled) {
			i += 1;
			continue;
		}

		if (disabled && !found) {
			list_modified = true;
			obs_property_list_item_remove(list, i);
			continue;
		}

		if (disabled != disconnected)
			list_modified = true;

		dev_found = true;
		obs_property_list_item_disable(list, i, disconnected);
		i += 1;
	}

	if (dev_found)
		return list_modified;

	size_t idx = obs_property_list_add_string(list, name.UTF8String,
						  uid.UTF8String);
	obs_property_list_item_disable(list, idx, disconnected);

	return true;
}

static void fill_presets(AVCaptureDevice *dev, obs_property_t *list,
			 NSString *current_preset)
{
	obs_property_list_clear(list);

	bool preset_found = false;
	for (NSString *preset in presets()) {
		bool is_current = [preset isEqualToString:current_preset];
		bool supported = dev &&
				 [dev supportsAVCaptureSessionPreset:preset];

		if (is_current)
			preset_found = true;

		if (!supported && !is_current)
			continue;

		size_t idx = obs_property_list_add_string(
			list, preset_names(preset).UTF8String,
			preset.UTF8String);
		obs_property_list_item_disable(list, idx, !supported);
	}

	if (preset_found)
		return;

	size_t idx = obs_property_list_add_string(
		list, preset_names(current_preset).UTF8String,
		current_preset.UTF8String);
	obs_property_list_item_disable(list, idx, true);
}

static bool check_preset(AVCaptureDevice *dev, obs_property_t *list,
			 obs_data_t *settings)
{
	NSString *current_preset = get_string(settings, "preset");

	size_t size = obs_property_list_item_count(list);
	NSMutableSet *listed = [NSMutableSet setWithCapacity:size];

	for (size_t i = 0; i < size; i++)
		[listed addObject:@(obs_property_list_item_string(list, i))];

	bool presets_changed = false;
	for (NSString *preset in presets()) {
		bool is_listed = [listed member:preset] != nil;
		bool supported = dev &&
				 [dev supportsAVCaptureSessionPreset:preset];

		if (supported == is_listed)
			continue;

		presets_changed = true;
	}

	if (!presets_changed && [listed member:current_preset] != nil)
		return false;

	fill_presets(dev, list, current_preset);
	return true;
}

static bool autoselect_preset(AVCaptureDevice *dev, obs_data_t *settings)
{
	NSString *preset = get_string(settings, "preset");
	if (!dev || [dev supportsAVCaptureSessionPreset:preset]) {
		if (obs_data_has_autoselect_value(settings, "preset")) {
			obs_data_unset_autoselect_value(settings, "preset");
			return true;
		}

	} else {
		preset = select_preset(dev, preset);
		const char *autoselect =
			obs_data_get_autoselect_string(settings, "preset");
		if (![preset isEqualToString:@(autoselect)]) {
			obs_data_set_autoselect_string(settings, "preset",
						       preset.UTF8String);
			return true;
		}
	}

	return false;
}

static CMVideoDimensions get_dimensions(AVCaptureDeviceFormat *format)
{
	auto desc = format.formatDescription;
	return CMVideoFormatDescriptionGetDimensions(desc);
}

using resolutions_t = vector<CMVideoDimensions>;

static resolutions_t enumerate_resolutions(AVCaptureDevice *dev)
{
	resolutions_t res;

	if (!dev)
		return res;

	res.reserve(dev.formats.count + 1);

	for (AVCaptureDeviceFormat *format in dev.formats) {
		auto dims = get_dimensions(format);

		if (find(begin(res), end(res), dims) == end(res))
			res.push_back(dims);
	}

	return res;
}

static void sort_resolutions(vector<CMVideoDimensions> &resolutions)
{
	auto cmp = [](const CMVideoDimensions &a, const CMVideoDimensions &b) {
		return a.width * a.height > b.width * b.height;
	};

	sort(begin(resolutions), end(resolutions), cmp);
}

static void data_set_resolution(obs_data_t *data, const CMVideoDimensions &dims)
{
	obs_data_set_int(data, "width", dims.width);
	obs_data_set_int(data, "height", dims.height);
}

static void data_set_resolution(const unique_ptr<obs_data_t> &data,
				const CMVideoDimensions &dims)
{
	data_set_resolution(data.get(), dims);
}

static bool add_resolution_to_list(vector<CMVideoDimensions> &res,
				   const CMVideoDimensions &dims)
{
	if (find(begin(res), end(res), dims) != end(res))
		return false;

	res.push_back(dims);
	return true;
}

static const char *obs_data_get_json(const unique_ptr<obs_data_t> &data)
{
	return obs_data_get_json(data.get());
}

static bool operator==(const CMVideoDimensions &a, const CMVideoDimensions &b)
{
	return a.width == b.width && a.height == b.height;
}

static bool resolution_property_needs_update(obs_property_t *p,
					     const resolutions_t &resolutions)
{
	vector<bool> res_found(resolutions.size());

	auto num = obs_property_list_item_count(p);
	for (size_t i = 1; i < num; i++) { // skip empty entry
		const char *json = obs_property_list_item_string(p, i);
		unique_ptr<obs_data_t> buffer{obs_data_create_from_json(json)};

		CMVideoDimensions dims{};
		if (!get_resolution(buffer.get(), dims))
			return true;

		auto pos = find(begin(resolutions), end(resolutions), dims);
		if (pos == end(resolutions))
			return true;

		res_found[pos - begin(resolutions)] = true;
	}

	return any_of(begin(res_found), end(res_found),
		      [](bool b) { return !b; });
}

static bool update_resolution_property(obs_properties_t *props,
				       const config_helper &conf,
				       obs_property_t *p = nullptr)
{
	if (!p)
		p = obs_properties_get(props, "resolution");

	if (!p)
		return false;

	auto valid_dims = conf.dims();
	auto resolutions = enumerate_resolutions(conf.dev());

	bool unsupported = true;
	if (valid_dims)
		unsupported = add_resolution_to_list(resolutions, *valid_dims);

	bool was_enabled = obs_property_enabled(p);
	obs_property_set_enabled(p, !!conf.dev());

	if (!resolution_property_needs_update(p, resolutions))
		return was_enabled != obs_property_enabled(p);

	sort_resolutions(resolutions);

	obs_property_list_clear(p);
	obs_property_list_add_string(p, "", "{}");

	DStr name;
	unique_ptr<obs_data_t> buffer{obs_data_create()};
	for (const CMVideoDimensions &dims : resolutions) {
		data_set_resolution(buffer, dims);
		auto json = obs_data_get_json(buffer);
		dstr_printf(name, "%dx%d", dims.width, dims.height);
		size_t idx = obs_property_list_add_string(p, name->array, json);

		if (unsupported && valid_dims && dims == *valid_dims)
			obs_property_list_item_disable(p, idx, true);
	}

	return true;
}

static media_frames_per_second convert(CMTime time_)
{
	media_frames_per_second res{};
	clamp(res.numerator, time_.timescale);
	clamp(res.denominator, time_.value);
	return res;
}

using frame_rates_t =
	vector<pair<media_frames_per_second, media_frames_per_second>>;
static frame_rates_t
enumerate_frame_rates(AVCaptureDevice *dev,
		      const CMVideoDimensions *dims = nullptr)
{
	frame_rates_t res;

	if (!dev || !dims)
		return res;

	auto add_unique_frame_rate_range = [&](AVFrameRateRange *range) {
		auto min = convert(range.maxFrameDuration);
		auto max = convert(range.minFrameDuration);

		auto pair = make_pair(min, max);

		if (find(begin(res), end(res), pair) != end(res))
			return;

		res.push_back(pair);
	};

	for (AVCaptureDeviceFormat *format in dev.formats) {
		if (!(get_dimensions(format) == *dims))
			continue;

		for (AVFrameRateRange *range in format
			     .videoSupportedFrameRateRanges) {
			add_unique_frame_rate_range(range);

			if (CMTimeCompare(range.minFrameDuration,
					  range.maxFrameDuration) != 0) {
				blog(LOG_WARNING,
				     "Got actual frame rate range:"
				     " %g - %g "
				     "({%lld, %d} - {%lld, %d})",
				     range.minFrameRate, range.maxFrameRate,
				     range.maxFrameDuration.value,
				     range.maxFrameDuration.timescale,
				     range.minFrameDuration.value,
				     range.minFrameDuration.timescale);
			}
		}
	}

	return res;
}

static bool operator==(const media_frames_per_second &a,
		       const media_frames_per_second &b)
{
	return a.numerator == b.numerator && a.denominator == b.denominator;
}

static bool operator!=(const media_frames_per_second &a,
		       const media_frames_per_second &b)
{
	return !(a == b);
}

static bool frame_rate_property_needs_update(obs_property_t *p,
					     const frame_rates_t &frame_rates)
{
	auto fps_num = frame_rates.size();
	auto num = obs_property_frame_rate_fps_ranges_count(p);
	if (fps_num != num)
		return true;

	vector<bool> fps_found(fps_num);
	for (size_t i = 0; i < num; i++) {
		auto min_ = obs_property_frame_rate_fps_range_min(p, i);
		auto max_ = obs_property_frame_rate_fps_range_max(p, i);

		auto it = find(begin(frame_rates), end(frame_rates),
			       make_pair(min_, max_));
		if (it == end(frame_rates))
			return true;

		fps_found[it - begin(frame_rates)] = true;
	}

	return any_of(begin(fps_found), end(fps_found),
		      [](bool b) { return !b; });
}

static bool update_frame_rate_property(obs_properties_t *props,
				       const config_helper &conf,
				       obs_property_t *p = nullptr)
{
	if (!p)
		p = obs_properties_get(props, "frame_rate");

	if (!p)
		return false;

	auto valid_dims = conf.dims();
	auto frame_rates = enumerate_frame_rates(conf.dev(), valid_dims);

	bool was_enabled = obs_property_enabled(p);
	obs_property_set_enabled(p, !frame_rates.empty());

	if (!frame_rate_property_needs_update(p, frame_rates))
		return was_enabled != obs_property_enabled(p);

	obs_property_frame_rate_fps_ranges_clear(p);
	for (auto &pair : frame_rates)
		obs_property_frame_rate_fps_range_add(p, pair.first,
						      pair.second);

	return true;
}

static vector<AVCaptureDeviceFormat *>
enumerate_formats(AVCaptureDevice *dev, const CMVideoDimensions &dims,
		  const media_frames_per_second &fps)
{
	vector<AVCaptureDeviceFormat *> result;

	find_formats(fps, dev, &dims, [&](AVCaptureDeviceFormat *format) {
		result.push_back(format);
		return false;
	});

	return result;
}

static bool input_format_property_needs_update(
	obs_property_t *p, const vector<AVCaptureDeviceFormat *> &formats,
	const FourCharCode *fourcc_)
{
	bool fourcc_found = !fourcc_;
	vector<bool> if_found(formats.size());

	auto num = obs_property_list_item_count(p);
	for (size_t i = 1; i < num; i++) { // skip auto entry
		FourCharCode fourcc = obs_property_list_item_int(p, i);
		fourcc_found = fourcc_found || fourcc == *fourcc_;

		auto pos = find_if(begin(formats), end(formats),
				   [&](AVCaptureDeviceFormat *format) {
					   FourCharCode fourcc_ = 0;
					   format_description_subtype_name(
						   format.formatDescription,
						   &fourcc_);
					   return fourcc_ == fourcc;
				   });
		if (pos == end(formats))
			return true;

		if_found[pos - begin(formats)] = true;
	}

	return fourcc_found || any_of(begin(if_found), end(if_found),
				      [](bool b) { return !b; });
}

static bool update_input_format_property(obs_properties_t *props,
					 const config_helper &conf,
					 obs_property_t *p = nullptr)
{
	if (!p)
		p = obs_properties_get(props, "input_format");

	if (!p)
		return false;

	auto update_enabled = [&](bool enabled) {
		bool was_enabled = obs_property_enabled(p);
		obs_property_set_enabled(p, enabled);
		return was_enabled != enabled;
	};

	auto valid_dims = conf.dims();
	auto valid_fps = conf.fps();
	auto valid_if = conf.input_format();

	if (!valid_dims || !valid_fps)
		return update_enabled(false);

	auto formats = enumerate_formats(conf.dev(), *valid_dims, *valid_fps);
	if (!input_format_property_needs_update(p, formats, valid_if))
		return update_enabled(!formats.empty());

	while (obs_property_list_item_count(p) > 1)
		obs_property_list_item_remove(p, 1);

	bool fourcc_found = !valid_if || *valid_if == INPUT_FORMAT_AUTO;
	for (auto &format : formats) {
		FourCharCode fourcc = 0;
		const char *name = format_description_subtype_name(
			format.formatDescription, &fourcc);
		obs_property_list_add_int(p, name, fourcc);
		fourcc_found = fourcc_found || fourcc == *valid_if;
	}

	if (!fourcc_found) {
		const char *name = fourcc_subtype_name(*valid_if);
		obs_property_list_add_int(p, name, *valid_if);
	}

	return update_enabled(!formats.empty());
}

static bool update_int_list_property(obs_property_t *p, const int *val,
				     const size_t count,
				     const char *localization_name)
{
	size_t num = obs_property_list_item_count(p);
	if (num > count) {
		if (!val || obs_property_list_item_int(p, count) != *val) {
			obs_property_list_item_remove(p, count);

			if (!val)
				return true;
		} else {
			return false;
		}
	}

	if (!val)
		return false;

	DStr buf, label;
	dstr_printf(buf, "%d", *val);
	dstr_init_copy(label, obs_module_text(localization_name));
	dstr_replace(label, "$1", buf->array);
	size_t idx = obs_property_list_add_int(p, label->array, *val);
	obs_property_list_item_disable(p, idx, true);

	return true;
}

template<typename Func>
static bool
update_int_list_property(const char *prop_name, const char *localization_name,
			 size_t count, int auto_val, bool (*valid_func)(int),
			 obs_properties_t *props, const config_helper &conf,
			 obs_property_t *p, Func get_val)
{
	auto ref = get_ref(props);
	if (!p)
		p = obs_properties_get(props, prop_name);

	int val = obs_data_get_int(conf.settings, prop_name);

	av_video_info vi;
	if (ref)
		vi = ref->video_info.read();

	bool params_valid = vi.video_params_valid;
	bool enabled = obs_property_enabled(p);
	bool should_enable = false;
	bool has_autoselect =
		obs_data_has_autoselect_value(conf.settings, prop_name);

	if ((params_valid && format_is_yuv(ref->frame.format)) ||
	    !valid_func(val))
		should_enable = true;

	obs_property_set_enabled(p, should_enable);
	bool updated = enabled != should_enable;

	updated = update_int_list_property(p, valid_func(val) ? nullptr : &val,
					   count, localization_name) ||
		  updated;

	if (!should_enable) {
		if (has_autoselect)
			obs_data_unset_autoselect_value(conf.settings,
							prop_name);
		return updated || has_autoselect;
	}

	bool use_autoselect = ref && val == auto_val;
	if (!use_autoselect) {
		if (has_autoselect)
			obs_data_unset_autoselect_value(conf.settings,
							prop_name);
		return updated || has_autoselect;
	}

	if (params_valid && get_val(vi) != obs_data_get_autoselect_int(
						   conf.settings, prop_name)) {
		obs_data_set_autoselect_int(conf.settings, prop_name,
					    get_val(vi));
		return true;
	}

	return updated;
}

static bool update_color_space_property(obs_properties_t *props,
					const config_helper &conf,
					obs_property_t *p = nullptr)
{
	return update_int_list_property("color_space", TEXT_COLOR_UNKNOWN_NAME,
					4, COLOR_SPACE_AUTO, color_space_valid,
					props, conf, p, [](av_video_info vi) {
						return vi.colorspace;
					});
}

static bool update_video_range_property(obs_properties_t *props,
					const config_helper &conf,
					obs_property_t *p = nullptr)
{
	return update_int_list_property("video_range", TEXT_RANGE_UNKNOWN_NAME,
					5, VIDEO_RANGE_AUTO, video_range_valid,
					props, conf, p, [](av_video_info vi) {
						return vi.video_range;
					});
}

static bool properties_device_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	NSString *uid = get_string(settings, "device");
	AVCaptureDevice *dev = [AVCaptureDevice deviceWithUniqueID:uid];

	NSString *name = get_string(settings, "device_name");
	bool dev_list_updated =
		update_device_list(p, uid, name, !dev && uid.length);

	p = obs_properties_get(props, "preset");
	bool preset_list_changed = check_preset(dev, p, settings);
	bool autoselect_changed = autoselect_preset(dev, settings);

	config_helper conf{settings};
	bool res_changed = update_resolution_property(props, conf);
	bool fps_changed = update_frame_rate_property(props, conf);
	bool if_changed = update_input_format_property(props, conf);

	return preset_list_changed || autoselect_changed || dev_list_updated ||
	       res_changed || fps_changed || if_changed;
}

static bool properties_use_preset_changed(obs_properties_t *props,
					  obs_property_t *,
					  obs_data_t *settings)
{
	auto use_preset = obs_data_get_bool(settings, "use_preset");

	config_helper conf{settings};

	bool updated = false;
	bool visible = false;
	obs_property_t *p = nullptr;

	auto noop = [](obs_properties_t *, const config_helper &,
		       obs_property_t *) { return false; };

#define UPDATE_PROPERTY(prop, uses_preset, func)                 \
	p = obs_properties_get(props, prop);                     \
	visible = use_preset == uses_preset;                     \
	updated = obs_property_visible(p) != visible || updated; \
	obs_property_set_visible(p, visible);                    \
	updated = func(props, conf, p) || updated;

	UPDATE_PROPERTY("preset", true, noop);
	UPDATE_PROPERTY("resolution", false, update_resolution_property);
	UPDATE_PROPERTY("frame_rate", false, update_frame_rate_property);
	UPDATE_PROPERTY("input_format", false, update_input_format_property);
	UPDATE_PROPERTY("color_space", false, update_color_space_property);
	UPDATE_PROPERTY("video_range", false, update_video_range_property);

	return updated;
}

static bool properties_preset_changed(obs_properties_t *, obs_property_t *p,
				      obs_data_t *settings)
{
	NSString *uid = get_string(settings, "device");
	AVCaptureDevice *dev = [AVCaptureDevice deviceWithUniqueID:uid];

	bool preset_list_changed = check_preset(dev, p, settings);
	bool autoselect_changed = autoselect_preset(dev, settings);

	return preset_list_changed || autoselect_changed;
}

static bool properties_resolution_changed(obs_properties_t *props,
					  obs_property_t *p,
					  obs_data_t *settings)
{
	config_helper conf{settings};

	bool res_updated = update_resolution_property(props, conf, p);
	bool fps_updated = update_frame_rate_property(props, conf);
	bool if_updated = update_input_format_property(props, conf);
	bool cs_updated = update_color_space_property(props, conf);
	bool cr_updated = update_video_range_property(props, conf);

	return res_updated || fps_updated || if_updated || cs_updated ||
	       cr_updated;
}

static bool properties_frame_rate_changed(obs_properties_t *props,
					  obs_property_t *p,
					  obs_data_t *settings)
{
	config_helper conf{settings};

	bool fps_updated = update_frame_rate_property(props, conf, p);
	bool if_updated = update_input_format_property(props, conf);
	bool cs_updated = update_color_space_property(props, conf);
	bool cr_updated = update_video_range_property(props, conf);

	return fps_updated || if_updated || cs_updated || cr_updated;
}

static bool properties_input_format_changed(obs_properties_t *props,
					    obs_property_t *p,
					    obs_data_t *settings)
{
	config_helper conf{settings};

	bool if_updated = update_input_format_property(props, conf, p);
	bool cs_updated = update_color_space_property(props, conf);
	bool cr_updated = update_video_range_property(props, conf);

	return if_updated || cs_updated || cr_updated;
}

static bool properties_color_space_changed(obs_properties_t *props,
					   obs_property_t *p,
					   obs_data_t *settings)
{
	config_helper conf{settings};

	return update_color_space_property(props, conf, p);
}

static bool properties_video_range_changed(obs_properties_t *props,
					   obs_property_t *p,
					   obs_data_t *settings)
{
	config_helper conf{settings};

	return update_video_range_property(props, conf, p);
}

static void add_properties_param(obs_properties_t *props, av_capture *capture)
{
	auto param =
		unique_ptr<properties_param>(new properties_param(capture));

	obs_properties_set_param(props, param.release(), [](void *param) {
		delete static_cast<properties_param *>(param);
	});
}

static void add_preset_properties(obs_properties_t *props)
{
	obs_property_t *preset_list = obs_properties_add_list(
		props, "preset", TEXT_PRESET, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	for (NSString *preset in presets())
		obs_property_list_add_string(preset_list,
					     preset_names(preset).UTF8String,
					     preset.UTF8String);

	obs_property_set_modified_callback(preset_list,
					   properties_preset_changed);
}

static void add_manual_properties(obs_properties_t *props)
{
	obs_property_t *resolutions = obs_properties_add_list(
		props, "resolution", TEXT_RESOLUTION, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_set_enabled(resolutions, false);
	obs_property_set_modified_callback(resolutions,
					   properties_resolution_changed);

	obs_property_t *frame_rates = obs_properties_add_frame_rate(
		props, "frame_rate", TEXT_FRAME_RATE);
	/*obs_property_frame_rate_option_add(frame_rates, "match obs",
			TEXT_MATCH_OBS);*/
	obs_property_set_enabled(frame_rates, false);
	obs_property_set_modified_callback(frame_rates,
					   properties_frame_rate_changed);

	obs_property_t *input_format = obs_properties_add_list(
		props, "input_format", TEXT_INPUT_FORMAT, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(input_format, TEXT_AUTO, INPUT_FORMAT_AUTO);
	obs_property_set_enabled(input_format, false);
	obs_property_set_modified_callback(input_format,
					   properties_input_format_changed);

	obs_property_t *color_space = obs_properties_add_list(
		props, "color_space", TEXT_COLOR_SPACE, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(color_space, TEXT_AUTO, COLOR_SPACE_AUTO);
	obs_property_list_add_int(color_space, "Rec. 601", VIDEO_CS_601);
	obs_property_list_add_int(color_space, "Rec. 709", VIDEO_CS_709);
	obs_property_set_enabled(color_space, false);
	obs_property_set_modified_callback(color_space,
					   properties_color_space_changed);

#define ADD_RANGE(x) obs_property_list_add_int(video_range, TEXT_##x, VIDEO_##x)
	obs_property_t *video_range = obs_properties_add_list(
		props, "video_range", TEXT_VIDEO_RANGE, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(video_range, TEXT_AUTO, VIDEO_RANGE_AUTO);
	ADD_RANGE(RANGE_PARTIAL);
	ADD_RANGE(RANGE_FULL);
	obs_property_set_enabled(video_range, false);
	obs_property_set_modified_callback(video_range,
					   properties_video_range_changed);
#undef ADD_RANGE
}

static obs_properties_t *av_capture_properties(void *capture)
{
	obs_properties_t *props = obs_properties_create();

	add_properties_param(props, static_cast<av_capture *>(capture));

	obs_property_t *dev_list = obs_properties_add_list(
		props, "device", TEXT_DEVICE, OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(dev_list, "", "");

	for (AVCaptureDevice *dev in [AVCaptureDevice devices]) {
		if ([dev hasMediaType:AVMediaTypeVideo] ||
		    [dev hasMediaType:AVMediaTypeMuxed]) {
			obs_property_list_add_string(
				dev_list, dev.localizedName.UTF8String,
				dev.uniqueID.UTF8String);
		}
	}

	obs_property_set_modified_callback(dev_list, properties_device_changed);

	obs_property_t *use_preset =
		obs_properties_add_bool(props, "use_preset", TEXT_USE_PRESET);
	obs_property_set_modified_callback(use_preset,
					   properties_use_preset_changed);

	add_preset_properties(props);

	add_manual_properties(props);

	obs_properties_add_bool(props, "buffering",
				obs_module_text("Buffering"));

	return props;
}

static void switch_device(av_capture *capture, NSString *uid,
			  obs_data_t *settings)
{
	if (!uid)
		return;

	if (capture->device)
		remove_device(capture);

	capture->uid = uid;

	if (!uid.length) {
		AVLOG(LOG_INFO, "No device selected, stopping capture");
		return;
	}

	AVCaptureDevice *dev = [AVCaptureDevice deviceWithUniqueID:uid];
	if (!dev) {
		AVLOG(LOG_WARNING, "Device with unique id '%s' not found",
		      uid.UTF8String);
		return;
	}

	capture_device(capture, dev, settings);
}

static void update_preset(av_capture *capture, obs_data_t *settings)
{
	unlock_device(capture);

	NSString *preset = get_string(settings, "preset");
	if (![capture->device supportsAVCaptureSessionPreset:preset]) {
		AVLOG(LOG_WARNING, "Preset %s not available",
		      preset.UTF8String);
		preset = select_preset(capture->device, preset);
	}

	capture->session.sessionPreset = preset;
	AVLOG(LOG_INFO, "Selected preset %s", preset.UTF8String);

	start_capture(capture);
}

static void update_manual(av_capture *capture, obs_data_t *settings)
{
	if (init_manual(capture, capture->device, settings))
		start_capture(capture);
}

static void av_capture_update(void *data, obs_data_t *settings)
{
	auto capture = static_cast<av_capture *>(data);

	NSString *uid = get_string(settings, "device");

	if (!capture->device || ![capture->device.uniqueID isEqualToString:uid])
		return switch_device(capture, uid, settings);

	if ((capture->use_preset = obs_data_get_bool(settings, "use_preset"))) {
		update_preset(capture, settings);
	} else {
		update_manual(capture, settings);
	}

	av_capture_enable_buffering(capture,
				    obs_data_get_bool(settings, "buffering"));
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mac-avcapture", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "MacOS AVFoundation Capture source";
}

bool obs_module_load(void)
{
#ifdef __MAC_10_10
	// Enable iOS device to show up as AVCapture devices
	// From WWDC video 2014 #508 at 5:34
	// https://developer.apple.com/videos/wwdc/2014/#508
	CMIOObjectPropertyAddress prop = {
		kCMIOHardwarePropertyAllowScreenCaptureDevices,
		kCMIOObjectPropertyScopeGlobal,
		kCMIOObjectPropertyElementMaster};
	UInt32 allow = 1;
	CMIOObjectSetPropertyData(kCMIOObjectSystemObject, &prop, 0, NULL,
				  sizeof(allow), &allow);
#endif

	obs_source_info av_capture_info = {
		.id = "av_capture_input",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_ASYNC_VIDEO |
				OBS_SOURCE_DO_NOT_DUPLICATE,
		.get_name = av_capture_getname,
		.create = av_capture_create,
		.destroy = av_capture_destroy,
		.get_defaults = av_capture_defaults,
		.get_properties = av_capture_properties,
		.update = av_capture_update,
		.icon_type = OBS_ICON_TYPE_CAMERA,
	};

	obs_register_source(&av_capture_info);
	return true;
}
