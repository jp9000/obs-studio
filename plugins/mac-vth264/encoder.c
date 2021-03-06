#include <obs-module.h>
#include <util/darray.h>
#include <obs-avc.h>

#include <CoreFoundation/CoreFoundation.h>
#include <VideoToolbox/VideoToolbox.h>
#include <VideoToolbox/VTVideoEncoderList.h>
#include <CoreMedia/CoreMedia.h>

#include <util/apple/cfstring-utils.h>

#include <assert.h>

#define VT_LOG(level, format, ...) \
	blog(level, "[VideoToolbox encoder]: " format, ##__VA_ARGS__)
#define VT_LOG_ENCODER(encoder, level, format, ...)       \
	blog(level, "[VideoToolbox %s: 'h264']: " format, \
	     obs_encoder_get_name(encoder), ##__VA_ARGS__)
#define VT_BLOG(level, format, ...) \
	VT_LOG_ENCODER(enc->encoder, level, format, ##__VA_ARGS__)

// Clipped from NSApplication as it is in a ObjC header
extern const double NSAppKitVersionNumber;
#define NSAppKitVersionNumber10_8 1187

// Get around missing symbol on 10.8 during compilation
enum { kCMFormatDescriptionBridgeError_InvalidParameter_ = -12712,
};

static bool is_appkit10_9_or_greater()
{
	return floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_8;
}

static DARRAY(struct vt_encoder {
	const char *name;
	const char *disp_name;
	const char *id;
	const char *codec_name;
}) vt_encoders;

struct vt_h264_encoder {
	obs_encoder_t *encoder;

	const char *vt_encoder_id;
	uint32_t width;
	uint32_t height;
	uint32_t keyint;
	uint32_t fps_num;
	uint32_t fps_den;
	uint32_t bitrate;
	bool limit_bitrate;
	uint32_t rc_max_bitrate;
	float rc_max_bitrate_window;
	const char *profile;
	bool bframes;

	enum video_format obs_pix_fmt;
	int vt_pix_fmt;
	enum video_colorspace colorspace;
	bool fullrange;

	VTCompressionSessionRef session;
	CMSimpleQueueRef queue;
	bool hw_enc;
	DARRAY(uint8_t) packet_data;
	DARRAY(uint8_t) extra_data;
};

static void log_osstatus(int log_level, struct vt_h264_encoder *enc,
			 const char *context, OSStatus code)
{
	char *c_str = NULL;
	CFErrorRef err = CFErrorCreate(kCFAllocatorDefault,
				       kCFErrorDomainOSStatus, code, NULL);
	CFStringRef str = CFErrorCopyDescription(err);

	c_str = cfstr_copy_cstr(str, kCFStringEncodingUTF8);
	if (c_str) {
		if (enc)
			VT_BLOG(log_level, "Error in %s: %s", context, c_str);
		else
			VT_LOG(log_level, "Error in %s: %s", context, c_str);
	}

	bfree(c_str);
	CFRelease(str);
	CFRelease(err);
}

static CFStringRef obs_to_vt_profile(const char *profile)
{
	if (strcmp(profile, "baseline") == 0)
		return kVTProfileLevel_H264_Baseline_AutoLevel;
	else if (strcmp(profile, "main") == 0)
		return kVTProfileLevel_H264_Main_AutoLevel;
	else if (strcmp(profile, "high") == 0)
		return kVTProfileLevel_H264_High_AutoLevel;
	else
		return kVTProfileLevel_H264_Main_AutoLevel;
}

static CFStringRef obs_to_vt_colorspace(enum video_colorspace cs)
{
	if (cs == VIDEO_CS_709)
		return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
	else if (cs == VIDEO_CS_601)
		return kCVImageBufferYCbCrMatrix_ITU_R_601_4;

	return NULL;
}

#define STATUS_CHECK(c)                                 \
	code = c;                                       \
	if (code) {                                     \
		log_osstatus(LOG_ERROR, enc, #c, code); \
		goto fail;                              \
	}

#define SESSION_CHECK(x)           \
	if ((code = (x)) != noErr) \
		return code;

static OSStatus session_set_prop_int(VTCompressionSessionRef session,
				     CFStringRef key, int32_t val)
{
	CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &val);
	OSStatus code = VTSessionSetProperty(session, key, n);
	CFRelease(n);

	return code;
}

static OSStatus session_set_prop_str(VTCompressionSessionRef session,
				     CFStringRef key, char *val)
{
	CFStringRef s = CFStringCreateWithFileSystemRepresentation(NULL, val);
	OSStatus code = VTSessionSetProperty(session, key, s);
	CFRelease(s);

	return code;
}

static OSStatus session_set_prop(VTCompressionSessionRef session,
				 CFStringRef key, CFTypeRef val)
{
	return VTSessionSetProperty(session, key, val);
}

static OSStatus session_set_bitrate(VTCompressionSessionRef session,
				    int new_bitrate, bool limit_bitrate,
				    int max_bitrate, float max_bitrate_window)
{
	OSStatus code;

	SESSION_CHECK(session_set_prop_int(
		session, kVTCompressionPropertyKey_AverageBitRate,
		new_bitrate * 1000));

	if (limit_bitrate) {
		int32_t cpb_size = max_bitrate * 125 * max_bitrate_window;

		CFNumberRef cf_cpb_size =
			CFNumberCreate(NULL, kCFNumberIntType, &cpb_size);
		CFNumberRef cf_cpb_window_s = CFNumberCreate(
			NULL, kCFNumberFloatType, &max_bitrate_window);

		CFMutableArrayRef rate_control = CFArrayCreateMutable(
			kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);

		CFArrayAppendValue(rate_control, cf_cpb_size);
		CFArrayAppendValue(rate_control, cf_cpb_window_s);

		code = session_set_prop(
			session, kVTCompressionPropertyKey_DataRateLimits,
			rate_control);

		CFRelease(cf_cpb_size);
		CFRelease(cf_cpb_window_s);
		CFRelease(rate_control);

		if (code == kVTPropertyNotSupportedErr) {
			log_osstatus(LOG_WARNING, NULL,
				     "setting DataRateLimits on session", code);
			return noErr;
		}
	}

	return noErr;
}

static OSStatus session_set_colorspace(VTCompressionSessionRef session,
				       enum video_colorspace cs)
{
	CFStringRef matrix = obs_to_vt_colorspace(cs);
	OSStatus code;

	if (matrix != NULL) {
		SESSION_CHECK(session_set_prop(
			session, kVTCompressionPropertyKey_ColorPrimaries,
			kCVImageBufferColorPrimaries_ITU_R_709_2));
		SESSION_CHECK(session_set_prop(
			session, kVTCompressionPropertyKey_TransferFunction,
			kCVImageBufferTransferFunction_ITU_R_709_2));
		SESSION_CHECK(session_set_prop(
			session, kVTCompressionPropertyKey_YCbCrMatrix,
			matrix));
	}

	return noErr;
}

#undef SESSION_CHECK

void sample_encoded_callback(void *data, void *source, OSStatus status,
			     VTEncodeInfoFlags info_flags,
			     CMSampleBufferRef buffer)
{
	UNUSED_PARAMETER(status);
	UNUSED_PARAMETER(info_flags);

	CMSimpleQueueRef queue = data;
	CVPixelBufferRef pixbuf = source;
	if (buffer != NULL) {
		CFRetain(buffer);
		CMSimpleQueueEnqueue(queue, buffer);
	}
	CFRelease(pixbuf);
}
#define ENCODER_ID kVTVideoEncoderSpecification_EncoderID
#define ENABLE_HW_ACCEL \
	kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder
#define REQUIRE_HW_ACCEL \
	kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder
static inline CFMutableDictionaryRef
create_encoder_spec(const char *vt_encoder_id)
{
	CFMutableDictionaryRef encoder_spec = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);

	CFStringRef id =
		CFStringCreateWithFileSystemRepresentation(NULL, vt_encoder_id);
	CFDictionaryAddValue(encoder_spec, ENCODER_ID, id);
	CFRelease(id);

	CFDictionaryAddValue(encoder_spec, ENABLE_HW_ACCEL, kCFBooleanTrue);
	CFDictionaryAddValue(encoder_spec, REQUIRE_HW_ACCEL, kCFBooleanFalse);

	return encoder_spec;
}
#undef ENCODER_ID
#undef REQUIRE_HW_ACCEL
#undef ENABLE_HW_ACCEL

static inline CFMutableDictionaryRef
create_pixbuf_spec(struct vt_h264_encoder *enc)
{
	CFMutableDictionaryRef pixbuf_spec = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);

	CFNumberRef n =
		CFNumberCreate(NULL, kCFNumberSInt32Type, &enc->vt_pix_fmt);
	CFDictionaryAddValue(pixbuf_spec, kCVPixelBufferPixelFormatTypeKey, n);
	CFRelease(n);

	n = CFNumberCreate(NULL, kCFNumberSInt32Type, &enc->width);
	CFDictionaryAddValue(pixbuf_spec, kCVPixelBufferWidthKey, n);
	CFRelease(n);

	n = CFNumberCreate(NULL, kCFNumberSInt32Type, &enc->height);
	CFDictionaryAddValue(pixbuf_spec, kCVPixelBufferHeightKey, n);
	CFRelease(n);

	return pixbuf_spec;
}

static bool create_encoder(struct vt_h264_encoder *enc)
{
	OSStatus code;

	VTCompressionSessionRef s;

	CFDictionaryRef encoder_spec = create_encoder_spec(enc->vt_encoder_id);
	CFDictionaryRef pixbuf_spec = create_pixbuf_spec(enc);

	STATUS_CHECK(VTCompressionSessionCreate(
		kCFAllocatorDefault, enc->width, enc->height,
		kCMVideoCodecType_H264, encoder_spec, pixbuf_spec, NULL,
		&sample_encoded_callback, enc->queue, &s));

	CFRelease(encoder_spec);
	CFRelease(pixbuf_spec);

	CFBooleanRef b = NULL;
	code = VTSessionCopyProperty(
		s,
		kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
		NULL, &b);

	if (code == noErr && (enc->hw_enc = CFBooleanGetValue(b)))
		VT_BLOG(LOG_INFO, "session created with hardware encoding");
	else
		enc->hw_enc = false;

	if (b != NULL)
		CFRelease(b);

	STATUS_CHECK(session_set_prop_int(
		s, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration,
		enc->keyint));
	STATUS_CHECK(session_set_prop_int(
		s, kVTCompressionPropertyKey_MaxKeyFrameInterval,
		enc->keyint * ((float)enc->fps_num / enc->fps_den)));
	STATUS_CHECK(session_set_prop_int(
		s, kVTCompressionPropertyKey_ExpectedFrameRate,
		ceil((float)enc->fps_num / enc->fps_den)));
	STATUS_CHECK(session_set_prop(
		s, kVTCompressionPropertyKey_AllowFrameReordering,
		enc->bframes ? kCFBooleanTrue : kCFBooleanFalse));

	// This can fail depending on hardware configuration
	code = session_set_prop(s, kVTCompressionPropertyKey_RealTime,
				kCFBooleanTrue);
	if (code != noErr)
		log_osstatus(LOG_WARNING, enc,
			     "setting "
			     "kVTCompressionPropertyKey_RealTime, "
			     "frame delay might be increased",
			     code);

	STATUS_CHECK(session_set_prop(s, kVTCompressionPropertyKey_ProfileLevel,
				      obs_to_vt_profile(enc->profile)));

	STATUS_CHECK(session_set_bitrate(s, enc->bitrate, enc->limit_bitrate,
					 enc->rc_max_bitrate,
					 enc->rc_max_bitrate_window));

	STATUS_CHECK(session_set_colorspace(s, enc->colorspace));

	STATUS_CHECK(VTCompressionSessionPrepareToEncodeFrames(s));

	enc->session = s;

	return true;

fail:
	if (encoder_spec != NULL)
		CFRelease(encoder_spec);
	if (pixbuf_spec != NULL)
		CFRelease(pixbuf_spec);

	return false;
}

static void vt_h264_destroy(void *data)
{
	struct vt_h264_encoder *enc = data;

	if (enc) {
		if (enc->session != NULL) {
			VTCompressionSessionInvalidate(enc->session);
			CFRelease(enc->session);
		}
		da_free(enc->packet_data);
		da_free(enc->extra_data);
		bfree(enc);
	}
}

static void dump_encoder_info(struct vt_h264_encoder *enc)
{
	VT_BLOG(LOG_INFO,
		"settings:\n"
		"\tvt_encoder_id          %s\n"
		"\tbitrate:               %d (kbps)\n"
		"\tfps_num:               %d\n"
		"\tfps_den:               %d\n"
		"\twidth:                 %d\n"
		"\theight:                %d\n"
		"\tkeyint:                %d (s)\n"
		"\tlimit_bitrate:         %s\n"
		"\trc_max_bitrate:        %d (kbps)\n"
		"\trc_max_bitrate_window: %f (s)\n"
		"\thw_enc:                %s\n"
		"\tprofile:               %s\n",
		enc->vt_encoder_id, enc->bitrate, enc->fps_num, enc->fps_den,
		enc->width, enc->height, enc->keyint,
		enc->limit_bitrate ? "on" : "off", enc->rc_max_bitrate,
		enc->rc_max_bitrate_window, enc->hw_enc ? "on" : "off",
		(enc->profile != NULL && !!strlen(enc->profile)) ? enc->profile
								 : "default");
}

static void vt_h264_video_info(void *data, struct video_scale_info *info)
{
	struct vt_h264_encoder *enc = data;

	if (info->format == VIDEO_FORMAT_I420) {
		enc->obs_pix_fmt = info->format;
		enc->vt_pix_fmt =
			enc->fullrange
				? kCVPixelFormatType_420YpCbCr8PlanarFullRange
				: kCVPixelFormatType_420YpCbCr8Planar;
		return;
	}

	if (info->format == VIDEO_FORMAT_I444)
		VT_BLOG(LOG_WARNING, "I444 color format not supported");

	// Anything else, return default
	enc->obs_pix_fmt = VIDEO_FORMAT_NV12;
	enc->vt_pix_fmt =
		enc->fullrange
			? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
			: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

	info->format = enc->obs_pix_fmt;
}

static void update_params(struct vt_h264_encoder *enc, obs_data_t *settings)
{
	video_t *video = obs_encoder_video(enc->encoder);
	const struct video_output_info *voi = video_output_get_info(video);

	struct video_scale_info info = {.format = voi->format};

	enc->fullrange = voi->range == VIDEO_RANGE_FULL;

	// also sets the enc->vt_pix_fmt
	vt_h264_video_info(enc, &info);

	enc->colorspace = voi->colorspace;

	enc->width = obs_encoder_get_width(enc->encoder);
	enc->height = obs_encoder_get_height(enc->encoder);
	enc->fps_num = voi->fps_num;
	enc->fps_den = voi->fps_den;
	enc->keyint = (uint32_t)obs_data_get_int(settings, "keyint_sec");
	enc->bitrate = (uint32_t)obs_data_get_int(settings, "bitrate");
	enc->profile = obs_data_get_string(settings, "profile");
	enc->limit_bitrate = obs_data_get_bool(settings, "limit_bitrate");
	enc->rc_max_bitrate = obs_data_get_int(settings, "max_bitrate");
	enc->rc_max_bitrate_window =
		obs_data_get_double(settings, "max_bitrate_window");
	enc->bframes = obs_data_get_bool(settings, "bframes");
}

static bool vt_h264_update(void *data, obs_data_t *settings)
{
	struct vt_h264_encoder *enc = data;

	uint32_t old_bitrate = enc->bitrate;
	bool old_limit_bitrate = enc->limit_bitrate;

	update_params(enc, settings);

	if (old_bitrate == enc->bitrate &&
	    old_limit_bitrate == enc->limit_bitrate)
		return true;

	OSStatus code = session_set_bitrate(enc->session, enc->bitrate,
					    enc->limit_bitrate,
					    enc->rc_max_bitrate,
					    enc->rc_max_bitrate_window);
	if (code != noErr)
		VT_BLOG(LOG_WARNING, "failed to set bitrate to session");

	CFNumberRef n;
	VTSessionCopyProperty(enc->session,
			      kVTCompressionPropertyKey_AverageBitRate, NULL,
			      &n);

	uint32_t session_bitrate;
	CFNumberGetValue(n, kCFNumberIntType, &session_bitrate);
	CFRelease(n);

	if (session_bitrate == old_bitrate) {
		VT_BLOG(LOG_WARNING,
			"failed to update current session "
			" bitrate from %d->%d",
			old_bitrate, enc->bitrate);
	}

	dump_encoder_info(enc);
	return true;
}

static void *vt_h264_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	struct vt_h264_encoder *enc = bzalloc(sizeof(struct vt_h264_encoder));

	OSStatus code;

	enc->encoder = encoder;
	enc->vt_encoder_id = obs_encoder_get_id(encoder);

	update_params(enc, settings);

	STATUS_CHECK(CMSimpleQueueCreate(NULL, 100, &enc->queue));

	if (!create_encoder(enc))
		goto fail;

	dump_encoder_info(enc);

	return enc;

fail:
	vt_h264_destroy(enc);
	return NULL;
}

static const uint8_t annexb_startcode[4] = {0, 0, 0, 1};

static void packet_put(struct darray *packet, const uint8_t *buf, size_t size)
{
	darray_push_back_array(sizeof(uint8_t), packet, buf, size);
}

static void packet_put_startcode(struct darray *packet, int size)
{
	assert(size == 3 || size == 4);

	packet_put(packet, &annexb_startcode[4 - size], size);
}

static void convert_block_nals_to_annexb(struct vt_h264_encoder *enc,
					 struct darray *packet,
					 CMBlockBufferRef block,
					 int nal_length_bytes)
{
	size_t block_size;
	uint8_t *block_buf;

	CMBlockBufferGetDataPointer(block, 0, NULL, &block_size,
				    (char **)&block_buf);

	size_t bytes_remaining = block_size;

	while (bytes_remaining > 0) {
		uint32_t nal_size;
		if (nal_length_bytes == 1)
			nal_size = block_buf[0];
		else if (nal_length_bytes == 2)
			nal_size = CFSwapInt16BigToHost(
				((uint16_t *)block_buf)[0]);
		else if (nal_length_bytes == 4)
			nal_size = CFSwapInt32BigToHost(
				((uint32_t *)block_buf)[0]);
		else
			return;

		bytes_remaining -= nal_length_bytes;
		block_buf += nal_length_bytes;

		if (bytes_remaining < nal_size) {
			VT_BLOG(LOG_ERROR, "invalid nal block");
			return;
		}

		packet_put_startcode(packet, 3);
		packet_put(packet, block_buf, nal_size);

		bytes_remaining -= nal_size;
		block_buf += nal_size;
	}
}

static bool handle_keyframe(struct vt_h264_encoder *enc,
			    CMFormatDescriptionRef format_desc,
			    size_t param_count, struct darray *packet,
			    struct darray *extra_data)
{
	OSStatus code;
	const uint8_t *param;
	size_t param_size;

	for (size_t i = 0; i < param_count; i++) {
		code = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
			format_desc, i, &param, &param_size, NULL, NULL);
		if (code != noErr) {
			log_osstatus(LOG_ERROR, enc,
				     "getting NAL parameter "
				     "at index",
				     code);
			return false;
		}

		packet_put_startcode(packet, 4);
		packet_put(packet, param, param_size);
	}

	// if we were passed an extra_data array, fill it with
	// SPS, PPS, etc.
	if (extra_data != NULL)
		packet_put(extra_data, packet->array, packet->num);

	return true;
}

static bool convert_sample_to_annexb(struct vt_h264_encoder *enc,
				     struct darray *packet,
				     struct darray *extra_data,
				     CMSampleBufferRef buffer, bool keyframe)
{
	OSStatus code;
	CMFormatDescriptionRef format_desc =
		CMSampleBufferGetFormatDescription(buffer);

	size_t param_count;
	int nal_length_bytes;
	code = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
		format_desc, 0, NULL, NULL, &param_count, &nal_length_bytes);
	// it is not clear what errors this function can return
	// so we check the two most reasonable
	if (code == kCMFormatDescriptionBridgeError_InvalidParameter_ ||
	    code == kCMFormatDescriptionError_InvalidParameter) {
		VT_BLOG(LOG_WARNING, "assuming 2 parameter sets "
				     "and 4 byte NAL length header");
		param_count = 2;
		nal_length_bytes = 4;

	} else if (code != noErr) {
		log_osstatus(LOG_ERROR, enc,
			     "getting parameter count from sample", code);
		return false;
	}

	if (keyframe &&
	    !handle_keyframe(enc, format_desc, param_count, packet, extra_data))
		return false;

	CMBlockBufferRef block = CMSampleBufferGetDataBuffer(buffer);
	convert_block_nals_to_annexb(enc, packet, block, nal_length_bytes);

	return true;
}

static bool is_sample_keyframe(CMSampleBufferRef buffer)
{
	CFArrayRef attachments =
		CMSampleBufferGetSampleAttachmentsArray(buffer, false);
	if (attachments != NULL) {
		CFDictionaryRef attachment;
		CFBooleanRef has_dependencies;
		attachment =
			(CFDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
		has_dependencies = (CFBooleanRef)CFDictionaryGetValue(
			attachment, kCMSampleAttachmentKey_DependsOnOthers);
		return has_dependencies == kCFBooleanFalse;
	}

	return false;
}

static bool parse_sample(struct vt_h264_encoder *enc, CMSampleBufferRef buffer,
			 struct encoder_packet *packet, CMTime off)
{
	int type;

	CMTime pts = CMSampleBufferGetPresentationTimeStamp(buffer);
	CMTime dts = CMSampleBufferGetDecodeTimeStamp(buffer);

	pts = CMTimeMultiplyByFloat64(pts,
				      ((Float64)enc->fps_num / enc->fps_den));
	dts = CMTimeMultiplyByFloat64(dts,
				      ((Float64)enc->fps_num / enc->fps_den));

	// imitate x264's negative dts when bframes might have pts < dts
	if (enc->bframes)
		dts = CMTimeSubtract(dts, off);

	bool keyframe = is_sample_keyframe(buffer);

	da_resize(enc->packet_data, 0);

	// If we are still looking for extra data
	struct darray *extra_data = NULL;
	if (enc->extra_data.num == 0)
		extra_data = &enc->extra_data.da;

	if (!convert_sample_to_annexb(enc, &enc->packet_data.da, extra_data,
				      buffer, keyframe))
		goto fail;

	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = (int64_t)(CMTimeGetSeconds(pts));
	packet->dts = (int64_t)(CMTimeGetSeconds(dts));
	packet->data = enc->packet_data.array;
	packet->size = enc->packet_data.num;
	packet->keyframe = keyframe;

	// VideoToolbox produces packets with priority lower than the RTMP code
	// expects, which causes it to be unable to recover from frame drops.
	// Fix this by manually adjusting the priority.
	uint8_t *start = enc->packet_data.array;
	uint8_t *end = start + enc->packet_data.num;

	start = (uint8_t *)obs_avc_find_startcode(start, end);
	while (true) {
		while (start < end && !*(start++))
			;

		if (start == end)
			break;

		type = start[0] & 0x1F;
		if (type == OBS_NAL_SLICE_IDR || type == OBS_NAL_SLICE) {
			uint8_t prev_type = (start[0] >> 5) & 0x3;
			start[0] &= ~(3 << 5);

			if (type == OBS_NAL_SLICE_IDR)
				start[0] |= OBS_NAL_PRIORITY_HIGHEST << 5;
			else if (type == OBS_NAL_SLICE &&
				 prev_type != OBS_NAL_PRIORITY_DISPOSABLE)
				start[0] |= OBS_NAL_PRIORITY_HIGH << 5;
			else
				start[0] |= prev_type << 5;
		}

		start = (uint8_t *)obs_avc_find_startcode(start, end);
	}

	CFRelease(buffer);
	return true;

fail:
	CFRelease(buffer);
	return false;
}

bool get_cached_pixel_buffer(struct vt_h264_encoder *enc, CVPixelBufferRef *buf)
{
	OSStatus code;
	CVPixelBufferPoolRef pool =
		VTCompressionSessionGetPixelBufferPool(enc->session);
	if (!pool)
		return kCVReturnError;

	CVPixelBufferRef pixbuf;
	STATUS_CHECK(CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &pixbuf));

	// Why aren't these already set on the pixel buffer?
	// I would have expected pixel buffers from the session's
	// pool to have the correct color space stuff set

	CFStringRef matrix = obs_to_vt_colorspace(enc->colorspace);

	CVBufferSetAttachment(pixbuf, kCVImageBufferYCbCrMatrixKey, matrix,
			      kCVAttachmentMode_ShouldPropagate);
	CVBufferSetAttachment(pixbuf, kCVImageBufferColorPrimariesKey,
			      kCVImageBufferColorPrimaries_ITU_R_709_2,
			      kCVAttachmentMode_ShouldPropagate);
	CVBufferSetAttachment(pixbuf, kCVImageBufferTransferFunctionKey,
			      kCVImageBufferTransferFunction_ITU_R_709_2,
			      kCVAttachmentMode_ShouldPropagate);

	*buf = pixbuf;
	return true;

fail:
	return false;
}

static bool vt_h264_encode(void *data, struct encoder_frame *frame,
			   struct encoder_packet *packet, bool *received_packet)
{
	struct vt_h264_encoder *enc = data;

	OSStatus code;

	CMTime dur = CMTimeMake(enc->fps_den, enc->fps_num);
	CMTime off = CMTimeMultiply(dur, 2);
	CMTime pts = CMTimeMultiply(dur, frame->pts);

	CVPixelBufferRef pixbuf = NULL;

	if (!get_cached_pixel_buffer(enc, &pixbuf)) {
		VT_BLOG(LOG_ERROR, "Unable to create pixel buffer");
		goto fail;
	}

	STATUS_CHECK(CVPixelBufferLockBaseAddress(pixbuf, 0));

	for (int i = 0; i < MAX_AV_PLANES; i++) {
		if (frame->data[i] == NULL)
			break;
		uint8_t *p = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(
			pixbuf, i);
		uint8_t *f = frame->data[i];
		size_t plane_linesize =
			CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
		size_t plane_height = CVPixelBufferGetHeightOfPlane(pixbuf, i);

		for (size_t j = 0; j < plane_height; j++) {
			memcpy(p, f, frame->linesize[i]);
			p += plane_linesize;
			f += frame->linesize[i];
		}
	}

	STATUS_CHECK(CVPixelBufferUnlockBaseAddress(pixbuf, 0));

	STATUS_CHECK(VTCompressionSessionEncodeFrame(enc->session, pixbuf, pts,
						     dur, NULL, pixbuf, NULL));

	CMSampleBufferRef buffer =
		(CMSampleBufferRef)CMSimpleQueueDequeue(enc->queue);

	// No samples waiting in the queue
	if (buffer == NULL)
		return true;

	*received_packet = true;
	return parse_sample(enc, buffer, packet, off);

fail:
	return false;
}

#undef STATUS_CHECK
#undef CFNUM_INT

static bool vt_h264_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	struct vt_h264_encoder *enc = (struct vt_h264_encoder *)data;
	*extra_data = enc->extra_data.array;
	*size = enc->extra_data.num;
	return true;
}

static const char *vt_h264_getname(void *data)
{
	const char *disp_name = vt_encoders.array[(int)data].disp_name;

	if (strcmp("Apple H.264 (HW)", disp_name) == 0) {
		return obs_module_text("VTH264EncHW");
	} else if (strcmp("Apple H.264 (SW)", disp_name) == 0) {
		return obs_module_text("VTH264EncSW");
	}
	return disp_name;
}

#define TEXT_VT_ENCODER obs_module_text("VTEncoder")
#define TEXT_BITRATE obs_module_text("Bitrate")
#define TEXT_USE_MAX_BITRATE obs_module_text("UseMaxBitrate")
#define TEXT_MAX_BITRATE obs_module_text("MaxBitrate")
#define TEXT_MAX_BITRATE_WINDOW obs_module_text("MaxBitrateWindow")
#define TEXT_KEYINT_SEC obs_module_text("KeyframeIntervalSec")
#define TEXT_PROFILE obs_module_text("Profile")
#define TEXT_NONE obs_module_text("None")
#define TEXT_DEFAULT obs_module_text("DefaultEncoder")
#define TEXT_BFRAMES obs_module_text("UseBFrames")

static bool limit_bitrate_modified(obs_properties_t *ppts, obs_property_t *p,
				   obs_data_t *settings)
{
	bool use_max_bitrate = obs_data_get_bool(settings, "limit_bitrate");
	p = obs_properties_get(ppts, "max_bitrate");
	obs_property_set_visible(p, use_max_bitrate);
	p = obs_properties_get(ppts, "max_bitrate_window");
	obs_property_set_visible(p, use_max_bitrate);
	return true;
}

static obs_properties_t *vt_h264_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_int(props, "bitrate", TEXT_BITRATE, 50, 10000000,
				   50);
	obs_property_int_set_suffix(p, " Kbps");

	p = obs_properties_add_bool(props, "limit_bitrate",
				    TEXT_USE_MAX_BITRATE);
	obs_property_set_modified_callback(p, limit_bitrate_modified);

	p = obs_properties_add_int(props, "max_bitrate", TEXT_MAX_BITRATE, 50,
				   10000000, 50);
	obs_property_int_set_suffix(p, " Kbps");

	obs_properties_add_float(props, "max_bitrate_window",
				 TEXT_MAX_BITRATE_WINDOW, 0.10f, 10.0f, 0.25f);

	obs_properties_add_int(props, "keyint_sec", TEXT_KEYINT_SEC, 0, 20, 1);

	p = obs_properties_add_list(props, "profile", TEXT_PROFILE,
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, TEXT_NONE, "");
	obs_property_list_add_string(p, "baseline", "baseline");
	obs_property_list_add_string(p, "main", "main");
	obs_property_list_add_string(p, "high", "high");

	obs_properties_add_bool(props, "bframes", TEXT_BFRAMES);

	return props;
}

static void vt_h264_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 2500);
	obs_data_set_default_bool(settings, "limit_bitrate", false);
	obs_data_set_default_int(settings, "max_bitrate", 2500);
	obs_data_set_default_double(settings, "max_bitrate_window", 1.5f);
	obs_data_set_default_int(settings, "keyint_sec", 0);
	obs_data_set_default_string(settings, "profile", "");
	obs_data_set_default_bool(settings, "bframes", true);
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mac-h264", "en-US")

void encoder_list_create()
{
	CFArrayRef encoder_list;
	VTCopyVideoEncoderList(NULL, &encoder_list);
	CFIndex size = CFArrayGetCount(encoder_list);

	for (CFIndex i = 0; i < size; i++) {
		CFDictionaryRef encoder_dict =
			CFArrayGetValueAtIndex(encoder_list, i);

#define VT_DICTSTR(key, name)                                             \
	CFStringRef name##_ref = CFDictionaryGetValue(encoder_dict, key); \
	CFIndex name##_len = CFStringGetLength(name##_ref);               \
	char *name = bzalloc(name##_len + 1);                             \
	CFStringGetFileSystemRepresentation(name##_ref, name, name##_len);

		VT_DICTSTR(kVTVideoEncoderList_CodecName, codec_name);
		if (strcmp("H.264", codec_name) != 0) {
			bfree(codec_name);
			continue;
		}
		VT_DICTSTR(kVTVideoEncoderList_EncoderName, name);
		VT_DICTSTR(kVTVideoEncoderList_EncoderID, id);
		VT_DICTSTR(kVTVideoEncoderList_DisplayName, disp_name);
		struct vt_encoder enc = {
			.name = name,
			.id = id,
			.disp_name = disp_name,
			.codec_name = codec_name,
		};
		da_push_back(vt_encoders, &enc);
#undef VT_DICTSTR
	}

	CFRelease(encoder_list);
}

void encoder_list_destroy()
{
	for (size_t i = 0; i < vt_encoders.num; i++) {
		bfree((char *)vt_encoders.array[i].name);
		bfree((char *)vt_encoders.array[i].id);
		bfree((char *)vt_encoders.array[i].codec_name);
		bfree((char *)vt_encoders.array[i].disp_name);
	}
	da_free(vt_encoders);
}

void register_encoders()
{
	struct obs_encoder_info info = {
		.type = OBS_ENCODER_VIDEO,
		.codec = "h264",
		.destroy = vt_h264_destroy,
		.encode = vt_h264_encode,
		.update = vt_h264_update,
		.get_properties = vt_h264_properties,
		.get_defaults = vt_h264_defaults,
		.get_video_info = vt_h264_video_info,
		.get_extra_data = vt_h264_extra_data,
		.caps = OBS_ENCODER_CAP_DYN_BITRATE,
	};

	for (size_t i = 0; i < vt_encoders.num; i++) {
		info.id = vt_encoders.array[i].id;
		info.type_data = (void *)i;
		info.get_name = vt_h264_getname;
		info.create = vt_h264_create;
		obs_register_encoder(&info);
	}
}

bool obs_module_load(void)
{
	if (!is_appkit10_9_or_greater()) {
		VT_LOG(LOG_WARNING, "Not adding VideoToolbox H264 encoder; "
				    "AppKit must be version 10.9 or greater");
		return false;
	}

	encoder_list_create();
	register_encoders();

	VT_LOG(LOG_INFO, "Adding VideoToolbox H264 encoders");

	return true;
}

void obs_module_unload(void)
{
	encoder_list_destroy();
}
