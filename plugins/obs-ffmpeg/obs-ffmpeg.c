#include <obs-module.h>
#include <util/platform.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "obs-ffmpeg-config.h"

#ifdef _WIN32
#include <dxgi.h>
#include <util/dstr.h>
#include <util/windows/win-version.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ffmpeg", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "FFmpeg based sources/outputs/encoders";
}

extern struct obs_source_info ffmpeg_source;
extern struct obs_output_info ffmpeg_output;
extern struct obs_output_info ffmpeg_muxer;
extern struct obs_output_info ffmpeg_mpegts_muxer;
extern struct obs_output_info replay_buffer;
extern struct obs_output_info ffmpeg_hls_muxer;
extern struct obs_encoder_info aac_encoder_info;
extern struct obs_encoder_info opus_encoder_info;
extern struct obs_encoder_info nvenc_encoder_info;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 27, 100)
#define LIBAVUTIL_VAAPI_AVAILABLE
#endif

#ifdef LIBAVUTIL_VAAPI_AVAILABLE
extern struct obs_encoder_info vaapi_encoder_info;
#endif

#ifndef __APPLE__

static const char *nvenc_check_name = "nvenc_check";

#ifdef _WIN32
static const int blacklisted_adapters[] = {
	0x1298, // GK208M [GeForce GT 720M]
	0x1140, // GF117M [GeForce 610M/710M/810M/820M / GT 620M/625M/630M/720M]
	0x1293, // GK208M [GeForce GT 730M]
	0x1290, // GK208M [GeForce GT 730M]
	0x0fe1, // GK107M [GeForce GT 730M]
	0x0fdf, // GK107M [GeForce GT 740M]
	0x1294, // GK208M [GeForce GT 740M]
	0x1292, // GK208M [GeForce GT 740M]
	0x0fe2, // GK107M [GeForce GT 745M]
	0x0fe3, // GK107M [GeForce GT 745M]
	0x1140, // GF117M [GeForce 610M/710M/810M/820M / GT 620M/625M/630M/720M]
	0x0fed, // GK107M [GeForce 820M]
	0x1340, // GM108M [GeForce 830M]
	0x1393, // GM107M [GeForce 840M]
	0x1341, // GM108M [GeForce 840M]
	0x1398, // GM107M [GeForce 845M]
	0x1390, // GM107M [GeForce 845M]
	0x1344, // GM108M [GeForce 845M]
	0x1299, // GK208BM [GeForce 920M]
	0x134f, // GM108M [GeForce 920MX]
	0x134e, // GM108M [GeForce 930MX]
	0x1349, // GM108M [GeForce 930M]
	0x1346, // GM108M [GeForce 930M]
	0x179c, // GM107 [GeForce 940MX]
	0x139c, // GM107M [GeForce 940M]
	0x1347, // GM108M [GeForce 940M]
	0x134d, // GM108M [GeForce 940MX]
	0x134b, // GM108M [GeForce 940MX]
	0x1399, // GM107M [GeForce 945M]
	0x1348, // GM108M [GeForce 945M / 945A]
	0x1d01, // GP108 [GeForce GT 1030]
	0x0fc5, // GK107 [GeForce GT 1030]
	0x174e, // GM108M [GeForce MX110]
	0x174d, // GM108M [GeForce MX130]
	0x1d10, // GP108M [GeForce MX150]
	0x1d12, // GP108M [GeForce MX150]
	0x1d11, // GP108M [GeForce MX230]
	0x1d13, // GP108M [GeForce MX250]
	0x1d52, // GP108BM [GeForce MX250]
	0x1c94, // GP107 [GeForce MX350]
	0x137b, // GM108GLM [Quadro M520 Mobile]
	0x1d33, // GP108GLM [Quadro P500 Mobile]
	0x137a, // GM108GLM [Quadro K620M / Quadro M500M]
};

static const size_t num_blacklisted =
	sizeof(blacklisted_adapters) / sizeof(blacklisted_adapters[0]);

static bool is_blacklisted(const int device_id)
{
	for (size_t i = 0; i < num_blacklisted; i++) {
		const int blacklisted_adapter = blacklisted_adapters[i];
		if (device_id == blacklisted_adapter) {
			return true;
		}
	}

	return false;
}

typedef HRESULT(WINAPI *create_dxgi_proc)(const IID *, IDXGIFactory1 **);

static bool nvenc_device_available(void)
{
	static HMODULE dxgi = NULL;
	static create_dxgi_proc create = NULL;
	IDXGIFactory1 *factory;
	IDXGIAdapter1 *adapter;
	bool available = false;
	HRESULT hr;
	UINT i = 0;

	if (!dxgi) {
		dxgi = GetModuleHandleW(L"dxgi");
		if (!dxgi) {
			dxgi = LoadLibraryW(L"dxgi");
			if (!dxgi) {
				return true;
			}
		}
	}

	if (!create) {
		create = (create_dxgi_proc)GetProcAddress(dxgi,
							  "CreateDXGIFactory1");
		if (!create) {
			return true;
		}
	}

	hr = create(&IID_IDXGIFactory1, &factory);
	if (FAILED(hr)) {
		return true;
	}

	while (factory->lpVtbl->EnumAdapters1(factory, i++, &adapter) == S_OK) {
		DXGI_ADAPTER_DESC desc;

		hr = adapter->lpVtbl->GetDesc(adapter, &desc);
		adapter->lpVtbl->Release(adapter);

		if (FAILED(hr)) {
			continue;
		}

		// 0x10de = NVIDIA Corporation
		if (desc.VendorId == 0x10de && !is_blacklisted(desc.DeviceId)) {
			available = true;
			goto finish;
		}
	}

finish:
	factory->lpVtbl->Release(factory);
	return available;
}
#endif

#ifdef _WIN32
extern bool load_nvenc_lib(void);
#endif

static bool nvenc_supported(void)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif

	profile_start(nvenc_check_name);

	AVCodec *nvenc = avcodec_find_encoder_by_name("nvenc_h264");
	void *lib = NULL;
	bool success = false;

	if (!nvenc) {
		goto cleanup;
	}

#if defined(_WIN32)
	if (!nvenc_device_available()) {
		goto cleanup;
	}
	if (load_nvenc_lib()) {
		success = true;
		goto finish;
	}
#else
	lib = os_dlopen("libnvidia-encode.so.1");
#endif

	/* ------------------------------------------- */

	success = !!lib;

cleanup:
	if (lib)
		os_dlclose(lib);
#if defined(_WIN32)
finish:
#endif
	profile_end(nvenc_check_name);
	return success;
}

#endif

#ifdef LIBAVUTIL_VAAPI_AVAILABLE
static bool vaapi_supported(void)
{
	AVCodec *vaenc = avcodec_find_encoder_by_name("h264_vaapi");
	return !!vaenc;
}
#endif

#ifdef _WIN32
extern void jim_nvenc_load(void);
extern void jim_nvenc_unload(void);
#endif

#if ENABLE_FFMPEG_LOGGING
extern void obs_ffmpeg_load_logging(void);
extern void obs_ffmpeg_unload_logging(void);
#endif

bool obs_module_load(void)
{
	obs_register_source(&ffmpeg_source);
	obs_register_output(&ffmpeg_output);
	obs_register_output(&ffmpeg_muxer);
	obs_register_output(&ffmpeg_mpegts_muxer);
	obs_register_output(&ffmpeg_hls_muxer);
	obs_register_output(&replay_buffer);
	obs_register_encoder(&aac_encoder_info);
	obs_register_encoder(&opus_encoder_info);
#ifndef __APPLE__
	if (nvenc_supported()) {
		blog(LOG_INFO, "NVENC supported");
#ifdef _WIN32
		if (get_win_ver_int() > 0x0601) {
			jim_nvenc_load();
		} else {
			// if on Win 7, new nvenc isn't available so there's
			// no nvenc encoder for the user to select, expose
			// the old encoder directly
			nvenc_encoder_info.caps &= ~OBS_ENCODER_CAP_INTERNAL;
		}
#endif
		obs_register_encoder(&nvenc_encoder_info);
	}
#if !defined(_WIN32) && defined(LIBAVUTIL_VAAPI_AVAILABLE)
	if (vaapi_supported()) {
		blog(LOG_INFO, "FFMPEG VAAPI supported");
		obs_register_encoder(&vaapi_encoder_info);
	}
#endif
#endif

#if ENABLE_FFMPEG_LOGGING
	obs_ffmpeg_load_logging();
#endif
	return true;
}

void obs_module_unload(void)
{
#if ENABLE_FFMPEG_LOGGING
	obs_ffmpeg_unload_logging();
#endif

#ifdef _WIN32
	jim_nvenc_unload();
#endif
}
