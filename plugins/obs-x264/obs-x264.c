/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>
#include <obs-module.h>
#include "obs-x264-options.h"

#ifndef _STDINT_H_INCLUDED
#define _STDINT_H_INCLUDED
#endif

#include <x264.h>

#define do_log(level, format, ...)                  \
	blog(level, "[x264 encoder: '%s'] " format, \
	     obs_encoder_get_name(obsx264->encoder), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

//#define ENABLE_VFR

/* ------------------------------------------------------------------------- */

struct obs_x264 {
	obs_encoder_t *encoder;

	x264_param_t params;
	x264_t *context;

	DARRAY(uint8_t) packet_data;

	uint8_t *extra_data;
	uint8_t *sei;

	size_t extra_data_size;
	size_t sei_size;

	os_performance_token_t *performance_token;
};

/* ------------------------------------------------------------------------- */

static const char *obs_x264_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "x264";
}

static void obs_x264_stop(void *data);

static void clear_data(struct obs_x264 *obsx264)
{
	if (obsx264->context) {
		x264_encoder_close(obsx264->context);
		bfree(obsx264->sei);
		bfree(obsx264->extra_data);

		obsx264->context = NULL;
		obsx264->sei = NULL;
		obsx264->extra_data = NULL;
	}
}

static void obs_x264_destroy(void *data)
{
	struct obs_x264 *obsx264 = data;

	if (obsx264) {
		os_end_high_performance(obsx264->performance_token);
		clear_data(obsx264);
		da_free(obsx264->packet_data);
		bfree(obsx264);
	}
}

static void obs_x264_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 2500);
	obs_data_set_default_bool(settings, "use_bufsize", false);
	obs_data_set_default_int(settings, "buffer_size", 2500);
	obs_data_set_default_int(settings, "keyint_sec", 0);
	obs_data_set_default_int(settings, "crf", 23);
#ifdef ENABLE_VFR
	obs_data_set_default_bool(settings, "vfr", false);
#endif
	obs_data_set_default_string(settings, "rate_control", "CBR");

	obs_data_set_default_string(settings, "preset", "veryfast");
	obs_data_set_default_string(settings, "profile", "");
	obs_data_set_default_string(settings, "tune", "");
	obs_data_set_default_string(settings, "x264opts", "");
	obs_data_set_default_bool(settings, "repeat_headers", false);
}

static inline void add_strings(obs_property_t *list, const char *const *strings)
{
	while (*strings) {
		obs_property_list_add_string(list, *strings, *strings);
		strings++;
	}
}

#define TEXT_RATE_CONTROL obs_module_text("RateControl")
#define TEXT_BITRATE obs_module_text("Bitrate")
#define TEXT_CUSTOM_BUF obs_module_text("CustomBufsize")
#define TEXT_BUF_SIZE obs_module_text("BufferSize")
#define TEXT_VFR obs_module_text("VFR")
#define TEXT_CRF obs_module_text("CRF")
#define TEXT_KEYINT_SEC obs_module_text("KeyframeIntervalSec")
#define TEXT_PRESET obs_module_text("CPUPreset")
#define TEXT_PROFILE obs_module_text("Profile")
#define TEXT_TUNE obs_module_text("Tune")
#define TEXT_NONE obs_module_text("None")
#define TEXT_X264_OPTS obs_module_text("EncoderOptions")

static bool use_bufsize_modified(obs_properties_t *ppts, obs_property_t *p,
				 obs_data_t *settings)
{
	bool use_bufsize = obs_data_get_bool(settings, "use_bufsize");
	const char *rc = obs_data_get_string(settings, "rate_control");
	bool rc_crf = astrcmpi(rc, "CRF") == 0;

	p = obs_properties_get(ppts, "buffer_size");
	obs_property_set_visible(p, use_bufsize && !rc_crf);
	return true;
}

static bool rate_control_modified(obs_properties_t *ppts, obs_property_t *p,
				  obs_data_t *settings)
{
	const char *rc = obs_data_get_string(settings, "rate_control");
	bool use_bufsize = obs_data_get_bool(settings, "use_bufsize");
	bool abr = astrcmpi(rc, "CBR") == 0 || astrcmpi(rc, "ABR") == 0;
	bool rc_crf = astrcmpi(rc, "CRF") == 0;

	p = obs_properties_get(ppts, "crf");
	obs_property_set_visible(p, !abr);

	p = obs_properties_get(ppts, "bitrate");
	obs_property_set_visible(p, !rc_crf);
	p = obs_properties_get(ppts, "use_bufsize");
	obs_property_set_visible(p, !rc_crf);
	p = obs_properties_get(ppts, "buffer_size");
	obs_property_set_visible(p, !rc_crf && use_bufsize);
	return true;
}

static obs_properties_t *obs_x264_props(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *list;
	obs_property_t *p;
	obs_property_t *headers;

	list = obs_properties_add_list(props, "rate_control", TEXT_RATE_CONTROL,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, "CBR", "CBR");
	obs_property_list_add_string(list, "ABR", "ABR");
	obs_property_list_add_string(list, "VBR", "VBR");
	obs_property_list_add_string(list, "CRF", "CRF");

	obs_property_set_modified_callback(list, rate_control_modified);

	p = obs_properties_add_int(props, "bitrate", TEXT_BITRATE, 50, 10000000,
				   50);
	obs_property_int_set_suffix(p, " Kbps");

	p = obs_properties_add_bool(props, "use_bufsize", TEXT_CUSTOM_BUF);
	obs_property_set_modified_callback(p, use_bufsize_modified);
	obs_properties_add_int(props, "buffer_size", TEXT_BUF_SIZE, 0, 10000000,
			       1);

	obs_properties_add_int(props, "crf", TEXT_CRF, 0, 51, 1);

	obs_properties_add_int(props, "keyint_sec", TEXT_KEYINT_SEC, 0, 20, 1);

	list = obs_properties_add_list(props, "preset", TEXT_PRESET,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	add_strings(list, x264_preset_names);

	list = obs_properties_add_list(props, "profile", TEXT_PROFILE,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, TEXT_NONE, "");
	obs_property_list_add_string(list, "baseline", "baseline");
	obs_property_list_add_string(list, "main", "main");
	obs_property_list_add_string(list, "high", "high");

	list = obs_properties_add_list(props, "tune", TEXT_TUNE,
				       OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, TEXT_NONE, "");
	add_strings(list, x264_tune_names);

#ifdef ENABLE_VFR
	obs_properties_add_bool(props, "vfr", TEXT_VFR);
#endif

	obs_properties_add_text(props, "x264opts", TEXT_X264_OPTS,
				OBS_TEXT_DEFAULT);

	headers = obs_properties_add_bool(props, "repeat_headers",
					  "repeat_headers");
	obs_property_set_visible(headers, false);

	return props;
}

static bool getparam(const char *param, char **name, const char **value)
{
	const char *assign;

	if (!param || !*param || (*param == '='))
		return false;

	assign = strchr(param, '=');
	if (!assign || !*assign || !*(assign + 1))
		return false;

	*name = bstrdup_n(param, assign - param);
	*value = assign + 1;
	return true;
}

static const char *validate(struct obs_x264 *obsx264, const char *val,
			    const char *name, const char *const *list)
{
	if (!val || !*val)
		return val;

	while (*list) {
		if (strcmp(val, *list) == 0)
			return val;

		list++;
	}

	warn("Invalid %s: %s", name, val);
	return NULL;
}

static void override_base_param(struct obs_x264 *obsx264,
				struct obs_x264_option option, char **preset,
				char **profile, char **tune)
{
	const char *name = option.name;
	const char *val = option.value;
	if (astrcmpi(name, "preset") == 0) {
		const char *valid_name =
			validate(obsx264, val, "preset", x264_preset_names);
		if (valid_name) {
			bfree(*preset);
			*preset = bstrdup(val);
		}

	} else if (astrcmpi(name, "profile") == 0) {
		const char *valid_name =
			validate(obsx264, val, "profile", x264_profile_names);
		if (valid_name) {
			bfree(*profile);
			*profile = bstrdup(val);
		}

	} else if (astrcmpi(name, "tune") == 0) {
		const char *valid_name =
			validate(obsx264, val, "tune", x264_tune_names);
		if (valid_name) {
			bfree(*tune);
			*tune = bstrdup(val);
		}
	}
}

static inline void override_base_params(struct obs_x264 *obsx264,
					const struct obs_x264_options *options,
					char **preset, char **profile,
					char **tune)
{
	for (size_t i = 0; i < options->count; ++i)
		override_base_param(obsx264, options->options[i], preset,
				    profile, tune);
}

#define OPENCL_ALIAS "opencl_is_experimental_and_potentially_unstable"

static inline void set_param(struct obs_x264 *obsx264,
			     struct obs_x264_option option)
{
	const char *name = option.name;
	const char *val = option.value;
	if (strcmp(name, "preset") != 0 && strcmp(name, "profile") != 0 &&
	    strcmp(name, "tune") != 0 && strcmp(name, "fps") != 0 &&
	    strcmp(name, "force-cfr") != 0 && strcmp(name, "width") != 0 &&
	    strcmp(name, "height") != 0 && strcmp(name, "opencl") != 0) {
		if (strcmp(option.name, OPENCL_ALIAS) == 0)
			name = "opencl";
		if (x264_param_parse(&obsx264->params, name, val) != 0)
			warn("x264 param: %s=%s failed", name, val);
	}
}

static inline void apply_x264_profile(struct obs_x264 *obsx264,
				      const char *profile)
{
	if (!obsx264->context && profile && *profile) {
		int ret = x264_param_apply_profile(&obsx264->params, profile);
		if (ret != 0)
			warn("Failed to set x264 profile '%s'", profile);
	}
}

static inline const char *validate_preset(struct obs_x264 *obsx264,
					  const char *preset)
{
	const char *new_preset =
		validate(obsx264, preset, "preset", x264_preset_names);
	return new_preset ? new_preset : "veryfast";
}

static bool reset_x264_params(struct obs_x264 *obsx264, const char *preset,
			      const char *tune)
{
	int ret = x264_param_default_preset(
		&obsx264->params, validate_preset(obsx264, preset),
		validate(obsx264, tune, "tune", x264_tune_names));
	return ret == 0;
}

static void log_x264(void *param, int level, const char *format, va_list args)
{
	struct obs_x264 *obsx264 = param;
	char str[1024];

	vsnprintf(str, 1024, format, args);
	info("%s", str);

	UNUSED_PARAMETER(level);
}

static inline int get_x264_cs_val(const char *const name,
				  const char *const names[])
{
	int idx = 0;
	do {
		if (strcmp(names[idx], name) == 0)
			return idx;
	} while (!!names[++idx]);

	return 0;
}

static void obs_x264_video_info(void *data, struct video_scale_info *info);

enum rate_control {
	RATE_CONTROL_CBR,
	RATE_CONTROL_VBR,
	RATE_CONTROL_ABR,
	RATE_CONTROL_CRF
};

static void update_params(struct obs_x264 *obsx264, obs_data_t *settings,
			  const struct obs_x264_options *options, bool update)
{
	video_t *video = obs_encoder_video(obsx264->encoder);
	const struct video_output_info *voi = video_output_get_info(video);
	struct video_scale_info info;

	info.format = voi->format;
	info.colorspace = voi->colorspace;
	info.range = voi->range;

	obs_x264_video_info(obsx264, &info);

	const char *rate_control =
		obs_data_get_string(settings, "rate_control");

	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	int buffer_size = (int)obs_data_get_int(settings, "buffer_size");
	int keyint_sec = (int)obs_data_get_int(settings, "keyint_sec");
	int crf = (int)obs_data_get_int(settings, "crf");
	int width = (int)obs_encoder_get_width(obsx264->encoder);
	int height = (int)obs_encoder_get_height(obsx264->encoder);
	int bf = (int)obs_data_get_int(settings, "bf");
	bool use_bufsize = obs_data_get_bool(settings, "use_bufsize");
	bool cbr_override = obs_data_get_bool(settings, "cbr");
	enum rate_control rc;

#ifdef ENABLE_VFR
	bool vfr = obs_data_get_bool(settings, "vfr");
#endif

	/* XXX: "cbr" setting has been deprecated */
	if (cbr_override) {
		warn("\"cbr\" setting has been deprecated for all encoders!  "
		     "Please set \"rate_control\" to \"CBR\" instead.  "
		     "Forcing CBR mode.  "
		     "(Note to all: this is why you shouldn't use strings for "
		     "common settings)");
		rate_control = "CBR";
	}

	if (astrcmpi(rate_control, "ABR") == 0) {
		rc = RATE_CONTROL_ABR;
		crf = 0;

	} else if (astrcmpi(rate_control, "VBR") == 0) {
		rc = RATE_CONTROL_VBR;

	} else if (astrcmpi(rate_control, "CRF") == 0) {
		rc = RATE_CONTROL_CRF;
		bitrate = 0;
		buffer_size = 0;

	} else { /* CBR */
		rc = RATE_CONTROL_CBR;
		crf = 0;
	}

	if (keyint_sec)
		obsx264->params.i_keyint_max =
			keyint_sec * voi->fps_num / voi->fps_den;

	if (!use_bufsize)
		buffer_size = bitrate;

#ifdef ENABLE_VFR
	obsx264->params.b_vfr_input = vfr;
#else
	obsx264->params.b_vfr_input = false;
#endif
	obsx264->params.rc.i_vbv_max_bitrate = bitrate;
	obsx264->params.rc.i_vbv_buffer_size = buffer_size;
	obsx264->params.rc.i_bitrate = bitrate;
	obsx264->params.i_width = width;
	obsx264->params.i_height = height;
	obsx264->params.i_fps_num = voi->fps_num;
	obsx264->params.i_fps_den = voi->fps_den;
	obsx264->params.pf_log = log_x264;
	obsx264->params.p_log_private = obsx264;
	obsx264->params.i_log_level = X264_LOG_WARNING;

	if (obs_data_has_user_value(settings, "bf"))
		obsx264->params.i_bframe = bf;

	static const char *const smpte170m = "smpte170m";
	static const char *const bt709 = "bt709";
	static const char *const iec61966_2_1 = "iec61966-2-1";
	const char *colorprim = NULL;
	const char *transfer = NULL;
	const char *colmatrix = NULL;
	switch (info.colorspace) {
	case VIDEO_CS_601:
		colorprim = smpte170m;
		transfer = smpte170m;
		colmatrix = smpte170m;
		break;
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_709:
		colorprim = bt709;
		transfer = bt709;
		colmatrix = bt709;
		break;
	case VIDEO_CS_SRGB:
		colorprim = bt709;
		transfer = iec61966_2_1;
		colmatrix = bt709;
		break;
	}

	obsx264->params.vui.b_fullrange = info.range == VIDEO_RANGE_FULL;
	obsx264->params.vui.i_colorprim =
		get_x264_cs_val(colorprim, x264_colorprim_names);
	obsx264->params.vui.i_transfer =
		get_x264_cs_val(transfer, x264_transfer_names);
	obsx264->params.vui.i_colmatrix =
		get_x264_cs_val(colmatrix, x264_colmatrix_names);

	/* use the new filler method for CBR to allow real-time adjusting of
	 * the bitrate */
	if (rc == RATE_CONTROL_CBR || rc == RATE_CONTROL_ABR) {
		obsx264->params.rc.i_rc_method = X264_RC_ABR;

		if (rc == RATE_CONTROL_CBR) {
#if X264_BUILD >= 139
			obsx264->params.rc.b_filler = true;
#else
			obsx264->params.i_nal_hrd = X264_NAL_HRD_CBR;
#endif
		}
	} else {
		obsx264->params.rc.i_rc_method = X264_RC_CRF;
	}

	obsx264->params.rc.f_rf_constant = (float)crf;

	if (info.format == VIDEO_FORMAT_NV12)
		obsx264->params.i_csp = X264_CSP_NV12;
	else if (info.format == VIDEO_FORMAT_I420)
		obsx264->params.i_csp = X264_CSP_I420;
	else if (info.format == VIDEO_FORMAT_I444)
		obsx264->params.i_csp = X264_CSP_I444;
	else
		obsx264->params.i_csp = X264_CSP_NV12;

	for (size_t i = 0; i < options->ignored_word_count; ++i)
		warn("ignoring invalid x264 option: %s",
		     options->ignored_words[i]);
	for (size_t i = 0; i < options->count; ++i)
		set_param(obsx264, options->options[i]);

	if (!update) {
		info("settings:\n"
		     "\trate_control: %s\n"
		     "\tbitrate:      %d\n"
		     "\tbuffer size:  %d\n"
		     "\tcrf:          %d\n"
		     "\tfps_num:      %d\n"
		     "\tfps_den:      %d\n"
		     "\twidth:        %d\n"
		     "\theight:       %d\n"
		     "\tkeyint:       %d\n",
		     rate_control, obsx264->params.rc.i_vbv_max_bitrate,
		     obsx264->params.rc.i_vbv_buffer_size,
		     (int)obsx264->params.rc.f_rf_constant, voi->fps_num,
		     voi->fps_den, width, height, obsx264->params.i_keyint_max);
	}
}

static void log_custom_options(struct obs_x264 *obsx264,
			       const struct obs_x264_options *options)
{
	if (options->count == 0) {
		return;
	}
	size_t settings_string_length = 0;
	for (size_t i = 0; i < options->count; ++i)
		settings_string_length += strlen(options->options[i].name) +
					  strlen(options->options[i].value) + 5;
	size_t buffer_size = settings_string_length + 1;
	char *settings_string = bmalloc(settings_string_length + 1);
	char *p = settings_string;
	size_t remaining_buffer_size = buffer_size;
	for (size_t i = 0; i < options->count; ++i) {
		int chars_written = snprintf(p, remaining_buffer_size,
					     "\n\t%s = %s",
					     options->options[i].name,
					     options->options[i].value);
		assert(chars_written >= 0);
		assert((size_t)chars_written <= remaining_buffer_size);
		p += chars_written;
		remaining_buffer_size -= chars_written;
	}
	assert(remaining_buffer_size == 1);
	assert(*p == '\0');
	info("custom settings: %s", settings_string);
	bfree(settings_string);
}

static bool update_settings(struct obs_x264 *obsx264, obs_data_t *settings,
			    bool update)
{
	char *preset = bstrdup(obs_data_get_string(settings, "preset"));
	char *profile = bstrdup(obs_data_get_string(settings, "profile"));
	char *tune = bstrdup(obs_data_get_string(settings, "tune"));
	struct obs_x264_options options = obs_x264_parse_options(
		obs_data_get_string(settings, "x264opts"));
	bool repeat_headers = obs_data_get_bool(settings, "repeat_headers");

	bool success = true;

	if (!update)
		blog(LOG_INFO, "---------------------------------");

	if (!obsx264->context) {
		override_base_params(obsx264, &options, &preset, &profile,
				     &tune);

		if (preset && *preset)
			info("preset: %s", preset);
		if (profile && *profile)
			info("profile: %s", profile);
		if (tune && *tune)
			info("tune: %s", tune);

		success = reset_x264_params(obsx264, preset, tune);
	}

	if (repeat_headers) {
		obsx264->params.b_repeat_headers = 1;
		obsx264->params.b_annexb = 1;
		obsx264->params.b_aud = 1;
	}

	if (success) {
		update_params(obsx264, settings, &options, update);
		if (!update) {
			log_custom_options(obsx264, &options);
		}

		if (!obsx264->context)
			apply_x264_profile(obsx264, profile);
	}

	obs_x264_free_options(options);
	bfree(preset);
	bfree(profile);
	bfree(tune);

	return success;
}

static bool obs_x264_update(void *data, obs_data_t *settings)
{
	struct obs_x264 *obsx264 = data;
	bool success = update_settings(obsx264, settings, true);
	int ret;

	if (success) {
		ret = x264_encoder_reconfig(obsx264->context, &obsx264->params);
		if (ret != 0)
			warn("Failed to reconfigure: %d", ret);
		return ret == 0;
	}

	return false;
}

static void load_headers(struct obs_x264 *obsx264)
{
	x264_nal_t *nals;
	int nal_count;
	DARRAY(uint8_t) header;
	DARRAY(uint8_t) sei;

	da_init(header);
	da_init(sei);

	x264_encoder_headers(obsx264->context, &nals, &nal_count);

	for (int i = 0; i < nal_count; i++) {
		x264_nal_t *nal = nals + i;

		if (nal->i_type == NAL_SEI)
			da_push_back_array(sei, nal->p_payload, nal->i_payload);
		else
			da_push_back_array(header, nal->p_payload,
					   nal->i_payload);
	}

	obsx264->extra_data = header.array;
	obsx264->extra_data_size = header.num;
	obsx264->sei = sei.array;
	obsx264->sei_size = sei.num;
}

static void *obs_x264_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	struct obs_x264 *obsx264 = bzalloc(sizeof(struct obs_x264));
	obsx264->encoder = encoder;

	if (update_settings(obsx264, settings, false)) {
		obsx264->context = x264_encoder_open(&obsx264->params);

		if (obsx264->context == NULL)
			warn("x264 failed to load");
		else
			load_headers(obsx264);
	} else {
		warn("bad settings specified");
	}

	if (!obsx264->context) {
		bfree(obsx264);
		return NULL;
	}

	obsx264->performance_token =
		os_request_high_performance("x264 encoding");

	return obsx264;
}

static void parse_packet(struct obs_x264 *obsx264,
			 struct encoder_packet *packet, x264_nal_t *nals,
			 int nal_count, x264_picture_t *pic_out)
{
	if (!nal_count)
		return;

	da_resize(obsx264->packet_data, 0);

	for (int i = 0; i < nal_count; i++) {
		x264_nal_t *nal = nals + i;
		da_push_back_array(obsx264->packet_data, nal->p_payload,
				   nal->i_payload);
	}

	packet->data = obsx264->packet_data.array;
	packet->size = obsx264->packet_data.num;
	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = pic_out->i_pts;
	packet->dts = pic_out->i_dts;
	packet->keyframe = pic_out->b_keyframe != 0;
}

static inline void init_pic_data(struct obs_x264 *obsx264, x264_picture_t *pic,
				 struct encoder_frame *frame)
{
	x264_picture_init(pic);

	pic->i_pts = frame->pts;
	pic->img.i_csp = obsx264->params.i_csp;

	if (obsx264->params.i_csp == X264_CSP_NV12)
		pic->img.i_plane = 2;
	else if (obsx264->params.i_csp == X264_CSP_I420)
		pic->img.i_plane = 3;
	else if (obsx264->params.i_csp == X264_CSP_I444)
		pic->img.i_plane = 3;

	for (int i = 0; i < pic->img.i_plane; i++) {
		pic->img.i_stride[i] = (int)frame->linesize[i];
		pic->img.plane[i] = frame->data[i];
	}
}

static bool obs_x264_encode(void *data, struct encoder_frame *frame,
			    struct encoder_packet *packet,
			    bool *received_packet)
{
	struct obs_x264 *obsx264 = data;
	x264_nal_t *nals;
	int nal_count;
	int ret;
	x264_picture_t pic, pic_out;

	if (!frame || !packet || !received_packet)
		return false;

	if (frame)
		init_pic_data(obsx264, &pic, frame);

	ret = x264_encoder_encode(obsx264->context, &nals, &nal_count,
				  (frame ? &pic : NULL), &pic_out);
	if (ret < 0) {
		warn("encode failed");
		return false;
	}

	*received_packet = (nal_count != 0);
	parse_packet(obsx264, packet, nals, nal_count, &pic_out);

	return true;
}

static bool obs_x264_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	struct obs_x264 *obsx264 = data;

	if (!obsx264->context)
		return false;

	*extra_data = obsx264->extra_data;
	*size = obsx264->extra_data_size;
	return true;
}

static bool obs_x264_sei(void *data, uint8_t **sei, size_t *size)
{
	struct obs_x264 *obsx264 = data;

	if (!obsx264->context)
		return false;

	*sei = obsx264->sei;
	*size = obsx264->sei_size;
	return true;
}

static inline bool valid_format(enum video_format format)
{
	return format == VIDEO_FORMAT_I420 || format == VIDEO_FORMAT_NV12 ||
	       format == VIDEO_FORMAT_I444;
}

static void obs_x264_video_info(void *data, struct video_scale_info *info)
{
	struct obs_x264 *obsx264 = data;
	enum video_format pref_format;

	pref_format = obs_encoder_get_preferred_video_format(obsx264->encoder);

	if (!valid_format(pref_format)) {
		pref_format = valid_format(info->format) ? info->format
							 : VIDEO_FORMAT_NV12;
	}

	info->format = pref_format;
}

struct obs_encoder_info obs_x264_encoder = {
	.id = "obs_x264",
	.type = OBS_ENCODER_VIDEO,
	.codec = "h264",
	.get_name = obs_x264_getname,
	.create = obs_x264_create,
	.destroy = obs_x264_destroy,
	.encode = obs_x264_encode,
	.update = obs_x264_update,
	.get_properties = obs_x264_props,
	.get_defaults = obs_x264_defaults,
	.get_extra_data = obs_x264_extra_data,
	.get_sei_data = obs_x264_sei,
	.get_video_info = obs_x264_video_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE,
};
