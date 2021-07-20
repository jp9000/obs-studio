#pragma once

#include <obs-module.h>
#include <obs-encoder.h>
#include "AMF/include/core/Factory.h"
#include <d3d11.h>
#include <d3d11_1.h>
#include <util/circlebuf.h>
#include <cinttypes>
#include <condition_variable>
#include <vector>
#include <atlbase.h>

#define AMF_PRESENT_TIMESTAMP L"PTS"
#define SET_AMF_VALUE_OR_FAIL(object, name, val)                             \
	result = object->SetProperty(name, val);                             \
	if (result != AMF_OK) {                                              \
		AMF_LOG_WARNING(                                             \
			"Failed to set %ls, error: %ls.", name,              \
			AMF::Instance()->GetTrace()->GetResultText(result)); \
		goto fail;                                                   \
	}
#define SET_AMF_VALUE(object, name, val)                                     \
	result = object->SetProperty(name, val);                             \
	if (result != AMF_OK) {                                              \
		AMF_LOG_WARNING(                                             \
			"Failed to set %ls, error: %ls.", name,              \
			AMF::Instance()->GetTrace()->GetResultText(result)); \
	}

#define RATE_CONTROL_METHOD "rate_control"
#define TEXT_RATE_CONTROL obs_module_text("RateControl")
#define VBAQ "VBAQ"
#define TEXT_VBAQ obs_module_text("VBAQ")
#define BITRATE "bitrate"
#define TEXT_BITRATE obs_module_text("Bitrate")
#define BITRATE_PEAK "max_bitrate"
#define TEXT_BITRATE_PEAK obs_module_text("MaxBitrate")
#define ENFORCE_HRD "enforce_HRD"
#define TEXT_ENFORCE_HRD obs_module_text("Enforce HRD")
#define HIGHMOTIONQUALITYBOOST "high_motion_quality_boost"
#define TEXT_HIGHMOTIONQUALITYBOOST obs_module_text("High motion quality boost")
#define VBVBUFFER_SIZE "buffer_size"
#define TEXT_VBVBUFFER_SIZE obs_module_text("VBV Buffer size")
#define TEXT_BUF_SIZE obs_module_text("BufferSize")
#define KEYFRAME_INTERVAL "keyint_sec"
#define TEXT_KEYFRAME_INTERVAL obs_module_text("KeyframeIntervalSec")

#define LOG_LEVEL "log_level"
#define TEXT_LOG_LEVEL obs_module_text("Log level")
#define LOG_LEVEL_ERROR "LogLevel.ERROR"
#define LOG_LEVEL_WARNING "LogLevel.WARNING"
#define LOG_LEVEL_INFO "LogLevel.INFO"
#define LOG_LEVEL_DEBUG "LogLevel.DEBUG"
#define LOG_LEVEL_TRACE "LogLevel.TRACE"

#define DEBLOCKINGFILTER "deblocking_filter"
#define TEXT_DEBLOCKINGFILTER obs_module_text("Deblocking filter")
#define PRESET "preset"
#define TEXT_PRESET obs_module_text("Preset")
#define PRESET_STREAMING obs_module_text("Preset.Streaming")
#define PRESET_RECORDING obs_module_text("Preset.Recording")
#define PRESET_LOWLATENCY obs_module_text("Preset.LowLatency")

#define QUALITYPRESET "quality_preset"
#define TEXT_QUALITY_PRESET obs_module_text("QualityPreset")
#define QUALITYPRESET_SPEED obs_module_text("QualityPreset.Speed")
#define QUALITYPRESET_BALANCED obs_module_text("QualityPreset.Balanced")
#define QUALITYPRESET_QUALITY obs_module_text("QualityPreset.Quality")

#define PROFILE "profile"
#define TEXT_PROFILE obs_module_text("Profile")
#define LOW_LATENCY_MODE "low_latency_mode"
#define TEXT_LOW_LATENCY_MODE "Low latency mode"
#define PREPASSMODE "pre_encode_mode"
#define TEXT_PREPASSMODE obs_module_text("Pre-encode mode")
#define CODINGTYPE "coding_type"
#define TEXT_CODINGTYPE obs_module_text("Coding type")

#define VIEW_MODE "view_mode"
#define TEXT_VIEW_MODE obs_module_text("View mode")
#define VIEW_BASIC "View.Basic"
#define VIEW_ADVANCED "View.Advanced"

#ifdef __cplusplus
extern "C" {
#endif

using namespace amf;

struct handle_tex {
	uint32_t handle;
	CComPtr<ID3D11Texture2D> tex;
	IDXGIKeyedMutex *km;
};

struct amf_data {
	enum CODEC_ID { CODEC_ID_H264, CODEC_ID_HEVC };
	obs_encoder_t *encoder;

	void *session;
	amf::AMFContextPtr context;
	amf::AMFComponentPtr encoder_amf;
	CODEC_ID codec;

	ATL::CComPtr<ID3D11Device> pD3D11Device;
	ATL::CComPtr<ID3D11DeviceContext> pD3D11Context;

	int frameW;
	int frameH;
	AMFRate frame_rate;

	AMFBufferPtr header;
	AMFBufferPtr sei;

	std::chrono::nanoseconds query_wait_time;
	amf::AMFBufferPtr packet_data;

	/// Timings
	double_t timestamp_step;
	DARRAY(struct handle_tex) input_textures;
	CComPtr<ID3D11Texture2D> texture;
};

enum class Presets : int8_t {
	None = -1,
	Streaming,
	Recording,
	LowLatency,
};

enum class ViewMode : uint8_t { Basic, Advanced };

AMF_RESULT init_d3d11(obs_data_t *settings, struct amf_data *enc);
bool amf_encode_tex(void *data, uint32_t handle, int64_t pts, uint64_t lock_key,
		    uint64_t *next_key, struct encoder_packet *packet,
		    bool *received_packet);
void amf_destroy(void *data);

bool amf_extra_data(void *data, uint8_t **header, size_t *size);

AMF_RESULT amf_create_encoder(obs_data_t *settings, amf_data *enc);

void register_amf_h264_encoder();
void register_amf_hevc_encoder();

#ifdef __cplusplus
}
#endif
