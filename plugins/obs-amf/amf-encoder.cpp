#include "AMF/include/components/VideoEncoderVCE.h"
#include "AMF/include/components/VideoEncoderHEVC.h"
#include "amf-encoder.hpp"
#include <obs-avc.h>
#include "obs-amf.hpp"

#define INITGUID
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <thread>
#include <chrono>
#define PTS_PROP L"PtsProp"
#define NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))

#include <iostream>
#include <fstream>

AMF_RESULT init_d3d11(obs_data_t *settings, struct amf_data *enc)
{
	ATL::CComPtr<IDXGIFactory> pFactory;
	ATL::CComPtr<ID3D11Device> pD3D11Device;
	ATL::CComPtr<ID3D11DeviceContext> pD3D11Context;
	ATL::CComPtr<IDXGIAdapter> pAdapter;
	HRESULT hr;
	uint32_t adapterIndex = 0;
	DXGI_ADAPTER_DESC desc;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void **)(&pFactory));
	if (FAILED(hr)) {
		AMF_LOG_WARNING("CreateDXGIFactory1 failed");
		return AMF_FAIL;
	}
	obs_video_info info;
	if (obs_get_video_info(&info)) {
		adapterIndex = info.adapter;
	}
	hr = pFactory->EnumAdapters(adapterIndex, &pAdapter);
	if (FAILED(hr)) {
		AMF_LOG_WARNING("EnumAdapters failed");
		return AMF_FAIL;
	}

	pAdapter->GetDesc(&desc);

	if (desc.VendorId != 0x1002) {
		AMF_LOG_WARNING("D3D11CreateDevice failed. Invalid vendor.");
		return AMF_FAIL;
	}

	hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL,
			       0, D3D11_SDK_VERSION, &pD3D11Device, NULL,
			       &pD3D11Context);
	if (FAILED(hr)) {
		AMF_LOG_WARNING("D3D11CreateDevice failed");
		return AMF_FAIL;
	}

	enc->pD3D11Device = pD3D11Device;
	enc->pD3D11Context = pD3D11Context;
	return AMF_OK;
}

ID3D11Texture2D *get_tex_from_handle(amf_data *enc, uint32_t handle,
				     IDXGIKeyedMutex **km_out)
{
	ATL::CComPtr<ID3D11Device> device = enc->pD3D11Device;
	IDXGIKeyedMutex *km;
	ID3D11Texture2D *input_tex;
	HRESULT hr;

	for (size_t i = 0; i < enc->input_textures.num; i++) {
		struct handle_tex *ht = &enc->input_textures.array[i];
		if (ht->handle == handle) {
			*km_out = ht->km;
			return ht->tex;
		}
	}

	hr = device->OpenSharedResource((HANDLE)(uintptr_t)handle,
					__uuidof(ID3D11Resource),
					(void **)(&input_tex));
	if (FAILED(hr)) {
		AMF_LOG_ERROR("OpenSharedResource failed");
		return NULL;
	}

	hr = input_tex->QueryInterface(_uuidof(IDXGIKeyedMutex), (void **)&km);
	if (FAILED(hr)) {
		AMF_LOG_ERROR("QueryInterface(IDXGIKeyedMutex) failed");
		input_tex->Release();
		return NULL;
	}

	input_tex->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);
	*km_out = km;
	struct handle_tex new_ht = {handle, input_tex, km};
	da_push_back(enc->input_textures, &new_ht);
	return input_tex;
}

bool amf_data_to_encoder_packet(void *encData, amf::AMFDataPtr &data,
				struct encoder_packet *packet)
{
	if (data == nullptr)
		return true;
	struct amf_data *enc = (amf_data *)encData;

	enc->packet_data = amf::AMFBufferPtr(data);
	auto clk_start = std::chrono::high_resolution_clock::now();

	// Timestamps
	packet->type = OBS_ENCODER_VIDEO;
	/// Present Timestamp
	data->GetProperty(AMF_PRESENT_TIMESTAMP, &packet->pts);
	/// Decode Timestamp
	packet->dts =
		(int64_t)round((double_t)data->GetPts() / enc->timestamp_step);
	/// Data
	if (enc->codec == amf_data::CODEC_ID_HEVC) {
		uint64_t pktType;
		data->GetProperty(AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE,
				  &pktType);
		switch ((AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_ENUM)pktType) {
		case AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_IDR:
			packet->keyframe = true;
			break;
		case AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_I:
			packet->keyframe = true;
			packet->priority = 1;
			break;
		case AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_P:
			packet->priority = 0;
			break;
		}
	} else {
		uint64_t pktType;
		data->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &pktType);
		switch ((AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_ENUM)pktType) {
		case AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR:
			packet->keyframe = true;
			break;
		case AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_I:
			packet->priority = 3;
			break;
		case AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_P:
			packet->priority = 2;
			break;
		case AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_B:
			packet->priority = 0;
			break;
		}
	}

	packet->data = (uint8_t *)enc->packet_data->GetNative();
	packet->size = enc->packet_data->GetSize();
	packet->type = OBS_ENCODER_VIDEO;

	return true;
}

bool amf_encode_tex(void *data, uint32_t handle, int64_t pts, uint64_t lock_key,
		    uint64_t *next_key, encoder_packet *packet,
		    bool *received_packet)
{
	AMF_RESULT res = AMF_FAIL;
	int block_and_wait;
	AMFSurfacePtr surface;
	AMFDataPtr pData = NULL;
	AMFDataPtr pOutData = NULL;
	struct amf_data *enc = (amf_data *)data;
	CComPtr<ID3D11Texture2D> input_tex;
	CComPtr<ID3D11Texture2D> output_tex = enc->texture;
	IDXGIKeyedMutex *km;
	ATL::CComPtr<ID3D11DeviceContext> context = enc->pD3D11Context;

	if (handle == GS_INVALID_HANDLE) {
		AMF_LOG_WARNING("Encode failed: bad texture handle");
		*next_key = lock_key;
		return false;
	}

	static const GUID AMFTextureArrayIndexGUID = {0x28115527,
						      0xe7c3,
						      0x4b66,
						      {0x99, 0xd3, 0x4f, 0x2a,
						       0xe6, 0xb4, 0x7f, 0xaf}};
	input_tex = get_tex_from_handle(enc, handle, &km);
	km->AcquireSync(lock_key, INFINITE);
	context->CopyResource((ID3D11Resource *)output_tex.p,
			      (ID3D11Resource *)input_tex.p);
	context->Flush();
	km->ReleaseSync(*next_key);

	res = enc->context->CreateSurfaceFromDX11Native(output_tex.p, &surface,
							NULL);

	int64_t tsLast = (int64_t)round((pts - 1) * enc->timestamp_step);
	int64_t tsNow = (int64_t)round(pts * enc->timestamp_step);

	surface->SetPts(tsNow);
	surface->SetProperty(AMF_PRESENT_TIMESTAMP, pts);
	surface->SetDuration(tsNow - tsLast);

	if (res != AMF_OK) {
		AMF_LOG_ERROR(
			"CreateSurfaceFromDX11Native() failed  with error %d\n",
			res);
		return false;
	}

	bool frameSubmitted = false, packetRetrieved = false;

	bool keepLooping = true;
	res = enc->encoder_amf->SubmitInput(surface);
	if (res != AMF_OK) {
		AMF_LOG_ERROR("Fialed to sub  with error %d\n", res);
	}
	while (true) {
		res = enc->encoder_amf->QueryOutput(&pOutData);
		if (res == AMF_OK)
			break;
		switch (res) {
		case AMF_NEED_MORE_INPUT: {
			*received_packet = false;
			return true;
		}
		case AMF_REPEAT: {
			break;
		}
		default: {
			AMF_LOG_WARNING(
				"Fialed to QueryOutput  with code: %ls\n",
				AMF::Instance()->GetTrace()->GetResultText(
					res));
			break;
		}
		}

		std::this_thread::sleep_for(enc->query_wait_time);
	}
	*received_packet = true;
	return amf_data_to_encoder_packet(data, pOutData, packet);
}

void amf_destroy(void *data)
{
	struct amf_data *enc = (amf_data *)data;

	for (size_t i = 0; i < enc->input_textures.num; i++) {
		ID3D11Texture2D *tex = enc->input_textures.array[i].tex;
		IDXGIKeyedMutex *km = enc->input_textures.array[i].km;
		tex->Release();
		km->Release();
	}

	da_free(enc->input_textures);

	bfree(enc);
}

bool amf_extra_data(void *data, uint8_t **header, size_t *size)
{
	struct amf_data *enc = (amf_data *)data;

	if (!enc->header) {
		return false;
	}

	*header = (uint8_t *)enc->header->GetNative();
	*size = enc->header->GetSize();
	return true;
}

static ID3D11Texture2D *create_texture(struct amf_data *enc)
{
	ATL::CComPtr<ID3D11Device> device = enc->pD3D11Device;
	ID3D11Texture2D *tex;
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = enc->frameW;
	desc.Height = enc->frameH;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_NV12;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	hr = device->CreateTexture2D(&desc, NULL, &tex);
	if (FAILED(hr)) {
		AMF_LOG_ERROR("Failed to create texture");
		return nullptr;
	}

	return tex;
}
AMF_RESULT amf_create_encoder(obs_data_t *settings, amf_data *enc)
{
	AMF_RESULT result = AMF_FAIL;

	// OBS Settings
	video_t *obsVideoInfo = obs_encoder_video(enc->encoder);
	const struct video_output_info *voi =
		video_output_get_info(obsVideoInfo);
	uint32_t obsFPSnum = voi->fps_num;
	uint32_t obsFPSden = voi->fps_den;

	enc->frameW = voi->width;
	enc->frameH = voi->height;
	enc->frame_rate = AMFConstructRate(obsFPSnum, obsFPSden);
	enc->query_wait_time = std::chrono::milliseconds(1);
	double_t frameRateFraction =
		((double_t)obsFPSden / (double_t)obsFPSnum);
	enc->timestamp_step = AMF_SECOND * frameRateFraction;
	enc->texture = create_texture(enc);

	//////////////////////////////////////////////////////////////////////////
	/// Initialize Encoder

	result = AMF::Instance()->GetFactory()->CreateContext(&enc->context);
	if (result != AMF_OK) {
		AMF_LOG_WARNING("Context Failed");
		goto clean;
	}

	result = enc->context->InitDX11(enc->pD3D11Device, AMF_DX11_1);
	if (result != AMF_OK) {
		AMF_LOG_WARNING("Failed to init from dx11.");
		goto clean;
	}

	if (result != AMF_OK) {
		AMF_LOG_WARNING(
			"Failed to set converter transfer characteristic");
		goto clean;
	}

	// Create Encoder
	if (enc->codec == amf_data::CODEC_ID_H264) {
		result = AMF::Instance()->GetFactory()->CreateComponent(
			enc->context, AMFVideoEncoderVCE_AVC,
			&enc->encoder_amf);
	} else if (enc->codec == amf_data::CODEC_ID_HEVC) {
		result = AMF::Instance()->GetFactory()->CreateComponent(
			enc->context, AMFVideoEncoder_HEVC, &enc->encoder_amf);
	}

	if (result != AMF_OK) {
		goto clean;
	}

clean:
	return result;
}
