/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include <cinttypes>
#include <util/base.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/util.hpp>
#include <graphics/matrix3.h>
#include <winternl.h>
#include <d3d9.h>
#include "d3d11-subsystem.hpp"
#include "d3d11-config.h"
#include "intel-nv12-support.hpp"

struct UnsupportedHWError : HRError {
	inline UnsupportedHWError(const char *str, HRESULT hr)
		: HRError(str, hr)
	{
	}
};

#ifdef _MSC_VER
/* alignment warning - despite the fact that alignment is already fixed */
#pragma warning(disable : 4316)
#endif

static inline void LogD3D11ErrorDetails(HRError error, gs_device_t *device)
{
	if (error.hr == DXGI_ERROR_DEVICE_REMOVED) {
		HRESULT DeviceRemovedReason =
			device->device->GetDeviceRemovedReason();
		blog(LOG_ERROR, "  Device Removed Reason: %08lX",
		     DeviceRemovedReason);
	}
}

static const IID dxgiFactory2 = {0x50c83a1c,
				 0xe072,
				 0x4c48,
				 {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6,
				  0xd0}};

gs_obj::gs_obj(gs_device_t *device_, gs_type type)
	: device(device_), obj_type(type)
{
	prev_next = &device->first_obj;
	next = device->first_obj;
	device->first_obj = this;
	if (next)
		next->prev_next = &next;
}

gs_obj::~gs_obj()
{
	if (prev_next)
		*prev_next = next;
	if (next)
		next->prev_next = prev_next;
}

static inline void make_swap_desc(DXGI_SWAP_CHAIN_DESC &desc,
				  const gs_init_data *data)
{
	memset(&desc, 0, sizeof(desc));
	desc.BufferCount = data->num_backbuffers;
	desc.BufferDesc.Format = ConvertGSTextureFormatView(data->format);
	desc.BufferDesc.Width = data->cx;
	desc.BufferDesc.Height = data->cy;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = (HWND)data->window.hwnd;
	desc.SampleDesc.Count = 1;
	desc.Windowed = true;
}

void gs_swap_chain::InitTarget(uint32_t cx, uint32_t cy)
{
	HRESULT hr;

	target.width = cx;
	target.height = cy;

	hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
			     (void **)target.texture.Assign());
	if (FAILED(hr))
		throw HRError("Failed to get swap buffer texture", hr);

	D3D11_RENDER_TARGET_VIEW_DESC rtv;
	rtv.Format = target.dxgiFormatView;
	rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtv.Texture2D.MipSlice = 0;
	hr = device->device->CreateRenderTargetView(
		target.texture, &rtv, target.renderTarget[0].Assign());
	if (FAILED(hr))
		throw HRError("Failed to create swap RTV", hr);
	if (target.dxgiFormatView == target.dxgiFormatViewLinear) {
		target.renderTargetLinear[0] = target.renderTarget[0];
	} else {
		rtv.Format = target.dxgiFormatViewLinear;
		hr = device->device->CreateRenderTargetView(
			target.texture, &rtv,
			target.renderTargetLinear[0].Assign());
		if (FAILED(hr))
			throw HRError("Failed to create linear swap RTV", hr);
	}
}

void gs_swap_chain::InitZStencilBuffer(uint32_t cx, uint32_t cy)
{
	zs.width = cx;
	zs.height = cy;

	if (zs.format != GS_ZS_NONE && cx != 0 && cy != 0) {
		zs.InitBuffer();
	} else {
		zs.texture.Clear();
		zs.view.Clear();
	}
}

void gs_swap_chain::Resize(uint32_t cx, uint32_t cy)
{
	RECT clientRect;
	HRESULT hr;

	target.texture.Clear();
	target.renderTarget[0].Clear();
	target.renderTargetLinear[0].Clear();
	zs.texture.Clear();
	zs.view.Clear();

	initData.cx = cx;
	initData.cy = cy;

	if (cx == 0 || cy == 0) {
		GetClientRect(hwnd, &clientRect);
		if (cx == 0)
			cx = clientRect.right;
		if (cy == 0)
			cy = clientRect.bottom;
	}

	hr = swap->ResizeBuffers(numBuffers, cx, cy, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr))
		throw HRError("Failed to resize swap buffers", hr);

	InitTarget(cx, cy);
	InitZStencilBuffer(cx, cy);
}

void gs_swap_chain::Init()
{
	target.device = device;
	target.isRenderTarget = true;
	target.format = initData.format;
	target.dxgiFormatResource =
		ConvertGSTextureFormatResource(initData.format);
	target.dxgiFormatView = ConvertGSTextureFormatView(initData.format);
	target.dxgiFormatViewLinear =
		ConvertGSTextureFormatViewLinear(initData.format);
	InitTarget(initData.cx, initData.cy);

	zs.device = device;
	zs.format = initData.zsformat;
	zs.dxgiFormat = ConvertGSZStencilFormat(initData.zsformat);
	InitZStencilBuffer(initData.cx, initData.cy);
}

gs_swap_chain::gs_swap_chain(gs_device *device, const gs_init_data *data)
	: gs_obj(device, gs_type::gs_swap_chain),
	  numBuffers(data->num_backbuffers),
	  hwnd((HWND)data->window.hwnd),
	  initData(*data)
{
	HRESULT hr;

	make_swap_desc(swapDesc, data);
	hr = device->factory->CreateSwapChain(device->device, &swapDesc,
					      swap.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create swap chain", hr);

	/* Ignore Alt+Enter */
	device->factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	Init();
}

void gs_device::InitCompiler()
{
	char d3dcompiler[40] = {};
	int ver = 49;

	while (ver > 30) {
		sprintf(d3dcompiler, "D3DCompiler_%02d.dll", ver);

		HMODULE module = LoadLibraryA(d3dcompiler);
		if (module) {
			d3dCompile = (pD3DCompile)GetProcAddress(module,
								 "D3DCompile");

#ifdef DISASSEMBLE_SHADERS
			d3dDisassemble = (pD3DDisassemble)GetProcAddress(
				module, "D3DDisassemble");
#endif
			if (d3dCompile) {
				return;
			}

			FreeLibrary(module);
		}

		ver--;
	}

	throw "Could not find any D3DCompiler libraries. Make sure you've "
	      "installed the <a href=\"https://obsproject.com/go/dxwebsetup\">"
	      "DirectX components</a> that OBS Studio requires.";
}

void gs_device::InitFactory(uint32_t adapterIdx)
{
	HRESULT hr;
	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2
						: __uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void **)factory.Assign());
	if (FAILED(hr))
		throw UnsupportedHWError("Failed to create DXGIFactory", hr);

	hr = factory->EnumAdapters1(adapterIdx, &adapter);
	if (FAILED(hr))
		throw UnsupportedHWError("Failed to enumerate DXGIAdapter", hr);
}

const static D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};

/* ------------------------------------------------------------------------- */

#define VERT_IN_OUT \
	"\
struct VertInOut { \
	float4 pos : POSITION; \
}; "

#define NV12_Y_PS \
	VERT_IN_OUT "\
float main(VertInOut vert_in) : TARGET \
{ \
	return 1.0; \
}"

#define NV12_UV_PS \
	VERT_IN_OUT "\
float2 main(VertInOut vert_in) : TARGET \
{ \
	return float2(1.0, 1.0); \
}"

#define NV12_VS \
	VERT_IN_OUT "\
VertInOut main(VertInOut vert_in) \
{ \
	VertInOut vert_out; \
	vert_out.pos = float4(vert_in.pos.xyz, 1.0); \
	return vert_out; \
} "

/* ------------------------------------------------------------------------- */

#define NV12_CX 128
#define NV12_CY 128

bool gs_device::HasBadNV12Output()
try {
	vec3 points[4];
	vec3_set(&points[0], -1.0f, -1.0f, 0.0f);
	vec3_set(&points[1], -1.0f, 1.0f, 0.0f);
	vec3_set(&points[2], 1.0f, -1.0f, 0.0f);
	vec3_set(&points[3], 1.0f, 1.0f, 0.0f);

	gs_texture_2d nv12_y(this, NV12_CX, NV12_CY, GS_R8, 1, nullptr,
			     GS_RENDER_TARGET | GS_SHARED_KM_TEX, GS_TEXTURE_2D,
			     false, true);
	gs_texture_2d nv12_uv(this, nv12_y.texture,
			      GS_RENDER_TARGET | GS_SHARED_KM_TEX);
	gs_vertex_shader nv12_vs(this, "", NV12_VS);
	gs_pixel_shader nv12_y_ps(this, "", NV12_Y_PS);
	gs_pixel_shader nv12_uv_ps(this, "", NV12_UV_PS);
	gs_stage_surface nv12_stage(this, NV12_CX, NV12_CY);

	gs_vb_data *vbd = gs_vbdata_create();
	vbd->num = 4;
	vbd->points = (vec3 *)bmemdup(&points, sizeof(points));

	gs_vertex_buffer buf(this, vbd, 0);

	device_load_vertexbuffer(this, &buf);
	device_load_vertexshader(this, &nv12_vs);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	device_set_viewport(this, 0, 0, NV12_CX, NV12_CY);
	device_set_cull_mode(this, GS_NEITHER);
	device_enable_depth_test(this, false);
	device_enable_blending(this, false);
	LoadVertexBufferData();

	device_set_render_target(this, &nv12_y, nullptr);
	device_load_pixelshader(this, &nv12_y_ps);
	UpdateBlendState();
	UpdateRasterState();
	UpdateZStencilState();
	FlushOutputViews();
	context->Draw(4, 0);

	device_set_viewport(this, 0, 0, NV12_CX / 2, NV12_CY / 2);
	device_set_render_target(this, &nv12_uv, nullptr);
	device_load_pixelshader(this, &nv12_uv_ps);
	UpdateBlendState();
	UpdateRasterState();
	UpdateZStencilState();
	FlushOutputViews();
	context->Draw(4, 0);

	device_load_pixelshader(this, nullptr);
	device_load_vertexbuffer(this, nullptr);
	device_load_vertexshader(this, nullptr);
	device_set_render_target(this, nullptr, nullptr);

	device_stage_texture(this, &nv12_stage, &nv12_y);

	uint8_t *data;
	uint32_t linesize;
	bool bad_driver = false;

	if (gs_stagesurface_map(&nv12_stage, &data, &linesize)) {
		bad_driver = data[linesize * NV12_CY] == 0;
		gs_stagesurface_unmap(&nv12_stage);
	} else {
		throw "Could not map surface";
	}

	if (bad_driver) {
		blog(LOG_WARNING, "Bad NV12 texture handling detected!  "
				  "Disabling NV12 texture support.");
	}
	return bad_driver;

} catch (const HRError &error) {
	blog(LOG_WARNING, "HasBadNV12Output failed: %s (%08lX)", error.str,
	     error.hr);
	return false;
} catch (const char *error) {
	blog(LOG_WARNING, "HasBadNV12Output failed: %s", error);
	return false;
}

static bool increase_maximum_frame_latency(ID3D11Device *device)
{
	ComQIPtr<IDXGIDevice1> dxgiDevice(device);
	if (!dxgiDevice) {
		blog(LOG_DEBUG, "%s: Failed to get IDXGIDevice1", __FUNCTION__);
		return false;
	}

	const HRESULT hr = dxgiDevice->SetMaximumFrameLatency(16);
	if (FAILED(hr)) {
		blog(LOG_DEBUG, "%s: SetMaximumFrameLatency failed",
		     __FUNCTION__);
		return false;
	}

	blog(LOG_INFO, "DXGI increase maximum frame latency success");
	return true;
}

#if USE_GPU_PRIORITY
static bool set_priority(ID3D11Device *device)
{
	typedef enum _D3DKMT_SCHEDULINGPRIORITYCLASS {
		D3DKMT_SCHEDULINGPRIORITYCLASS_IDLE,
		D3DKMT_SCHEDULINGPRIORITYCLASS_BELOW_NORMAL,
		D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL,
		D3DKMT_SCHEDULINGPRIORITYCLASS_ABOVE_NORMAL,
		D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH,
		D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME
	} D3DKMT_SCHEDULINGPRIORITYCLASS;

	ComQIPtr<IDXGIDevice> dxgiDevice(device);
	if (!dxgiDevice) {
		blog(LOG_DEBUG, "%s: Failed to get IDXGIDevice", __FUNCTION__);
		return false;
	}

	HMODULE gdi32 = GetModuleHandleW(L"GDI32");
	if (!gdi32) {
		blog(LOG_DEBUG, "%s: Failed to get GDI32", __FUNCTION__);
		return false;
	}

	NTSTATUS(WINAPI * d3dkmt_spspc)(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS);
	d3dkmt_spspc = (decltype(d3dkmt_spspc))GetProcAddress(
		gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
	if (!d3dkmt_spspc) {
		blog(LOG_DEBUG, "%s: Failed to get d3dkmt_spspc", __FUNCTION__);
		return false;
	}

	NTSTATUS status = d3dkmt_spspc(GetCurrentProcess(),
				       D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME);
	if (status != 0) {
		blog(LOG_DEBUG, "%s: Failed to set process priority class: %d",
		     __FUNCTION__, (int)status);
		return false;
	}

	HRESULT hr = dxgiDevice->SetGPUThreadPriority(GPU_PRIORITY_VAL);
	if (FAILED(hr)) {
		blog(LOG_DEBUG, "%s: SetGPUThreadPriority failed",
		     __FUNCTION__);
		return false;
	}

	blog(LOG_INFO, "D3D11 GPU priority setup success");
	return true;
}

#endif

void gs_device::InitDevice(uint32_t adapterIdx)
{
	wstring adapterName;
	DXGI_ADAPTER_DESC desc;
	D3D_FEATURE_LEVEL levelUsed = D3D_FEATURE_LEVEL_10_0;
	HRESULT hr = 0;

	adpIdx = adapterIdx;

	uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	//createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	adapterName = (adapter->GetDesc(&desc) == S_OK) ? desc.Description
							: L"<unknown>";

	BPtr<char> adapterNameUTF8;
	os_wcs_to_utf8_ptr(adapterName.c_str(), 0, &adapterNameUTF8);
	blog(LOG_INFO, "Loading up D3D11 on adapter %s (%" PRIu32 ")",
	     adapterNameUTF8.Get(), adapterIdx);

	hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
			       createFlags, featureLevels,
			       sizeof(featureLevels) /
				       sizeof(D3D_FEATURE_LEVEL),
			       D3D11_SDK_VERSION, device.Assign(), &levelUsed,
			       context.Assign());
	if (FAILED(hr))
		throw UnsupportedHWError("Failed to create device", hr);

	blog(LOG_INFO, "D3D11 loaded successfully, feature level used: %x",
	     (unsigned int)levelUsed);

	/* prevent stalls sometimes seen in Present calls */
	if (!increase_maximum_frame_latency(device)) {
		blog(LOG_INFO, "DXGI increase maximum frame latency failed");
	}

	/* adjust gpu thread priority on non-intel GPUs */
#if USE_GPU_PRIORITY
	if (desc.VendorId != 0x8086 && !set_priority(device)) {
		blog(LOG_INFO, "D3D11 GPU priority setup "
			       "failed (not admin?)");
	}
#endif

	/* ---------------------------------------- */
	/* check for nv12 texture output support    */

	nv12Supported = false;

	/* WARP NV12 support is suspected to be buggy on older Windows */
	if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c) {
		return;
	}

	/* Intel CopyResource is very slow with NV12 */
	if (desc.VendorId == 0x8086 && IsOldIntelPlatform(desc.DeviceId)) {
		return;
	}

	ComQIPtr<ID3D11Device1> d3d11_1(device);
	if (!d3d11_1) {
		return;
	}

	/* needs to support extended resource sharing */
	D3D11_FEATURE_DATA_D3D11_OPTIONS opts = {};
	hr = d3d11_1->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &opts,
					  sizeof(opts));
	if (FAILED(hr) || !opts.ExtendedResourceSharing) {
		return;
	}

	/* needs to support the actual format */
	UINT support = 0;
	hr = device->CheckFormatSupport(DXGI_FORMAT_NV12, &support);
	if (FAILED(hr)) {
		return;
	}

	if ((support & D3D11_FORMAT_SUPPORT_TEXTURE2D) == 0) {
		return;
	}

	/* must be usable as a render target */
	if ((support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0) {
		return;
	}

	if (HasBadNV12Output()) {
		return;
	}

	nv12Supported = true;
}

static inline void ConvertStencilSide(D3D11_DEPTH_STENCILOP_DESC &desc,
				      const StencilSide &side)
{
	desc.StencilFunc = ConvertGSDepthTest(side.test);
	desc.StencilFailOp = ConvertGSStencilOp(side.fail);
	desc.StencilDepthFailOp = ConvertGSStencilOp(side.zfail);
	desc.StencilPassOp = ConvertGSStencilOp(side.zpass);
}

ID3D11DepthStencilState *gs_device::AddZStencilState()
{
	HRESULT hr;
	D3D11_DEPTH_STENCIL_DESC dsd;
	ID3D11DepthStencilState *state;

	dsd.DepthEnable = zstencilState.depthEnabled;
	dsd.DepthFunc = ConvertGSDepthTest(zstencilState.depthFunc);
	dsd.DepthWriteMask = zstencilState.depthWriteEnabled
				     ? D3D11_DEPTH_WRITE_MASK_ALL
				     : D3D11_DEPTH_WRITE_MASK_ZERO;
	dsd.StencilEnable = zstencilState.stencilEnabled;
	dsd.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	dsd.StencilWriteMask = zstencilState.stencilWriteEnabled
				       ? D3D11_DEFAULT_STENCIL_WRITE_MASK
				       : 0;
	ConvertStencilSide(dsd.FrontFace, zstencilState.stencilFront);
	ConvertStencilSide(dsd.BackFace, zstencilState.stencilBack);

	SavedZStencilState savedState(zstencilState, dsd);
	hr = device->CreateDepthStencilState(&dsd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil state", hr);

	state = savedState.state;
	zstencilStates.push_back(savedState);

	return state;
}

ID3D11RasterizerState *gs_device::AddRasterState()
{
	HRESULT hr;
	D3D11_RASTERIZER_DESC rd;
	ID3D11RasterizerState *state;

	memset(&rd, 0, sizeof(rd));
	/* use CCW to convert to a right-handed coordinate system */
	rd.FrontCounterClockwise = true;
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = ConvertGSCullMode(rasterState.cullMode);
	rd.DepthClipEnable = true;
	rd.ScissorEnable = rasterState.scissorEnabled;

	SavedRasterState savedState(rasterState, rd);
	hr = device->CreateRasterizerState(&rd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create rasterizer state", hr);

	state = savedState.state;
	rasterStates.push_back(savedState);

	return state;
}

ID3D11BlendState *gs_device::AddBlendState()
{
	HRESULT hr;
	D3D11_BLEND_DESC bd;
	ID3D11BlendState *state;

	memset(&bd, 0, sizeof(bd));
	for (int i = 0; i < 8; i++) {
		bd.RenderTarget[i].BlendEnable = blendState.blendEnabled;
		bd.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].SrcBlend =
			ConvertGSBlendType(blendState.srcFactorC);
		bd.RenderTarget[i].DestBlend =
			ConvertGSBlendType(blendState.destFactorC);
		bd.RenderTarget[i].SrcBlendAlpha =
			ConvertGSBlendType(blendState.srcFactorA);
		bd.RenderTarget[i].DestBlendAlpha =
			ConvertGSBlendType(blendState.destFactorA);
		bd.RenderTarget[i].RenderTargetWriteMask =
			(blendState.redEnabled ? D3D11_COLOR_WRITE_ENABLE_RED
					       : 0) |
			(blendState.greenEnabled
				 ? D3D11_COLOR_WRITE_ENABLE_GREEN
				 : 0) |
			(blendState.blueEnabled ? D3D11_COLOR_WRITE_ENABLE_BLUE
						: 0) |
			(blendState.alphaEnabled
				 ? D3D11_COLOR_WRITE_ENABLE_ALPHA
				 : 0);
	}

	SavedBlendState savedState(blendState, bd);
	hr = device->CreateBlendState(&bd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create blend state", hr);

	state = savedState.state;
	blendStates.push_back(savedState);

	return state;
}

void gs_device::UpdateZStencilState()
{
	ID3D11DepthStencilState *state = NULL;

	if (!zstencilStateChanged)
		return;

	for (size_t i = 0; i < zstencilStates.size(); i++) {
		SavedZStencilState &s = zstencilStates[i];
		if (memcmp(&s, &zstencilState, sizeof(zstencilState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddZStencilState();

	if (state != curDepthStencilState) {
		context->OMSetDepthStencilState(state, 0);
		curDepthStencilState = state;
	}

	zstencilStateChanged = false;
}

void gs_device::UpdateRasterState()
{
	ID3D11RasterizerState *state = NULL;

	if (!rasterStateChanged)
		return;

	for (size_t i = 0; i < rasterStates.size(); i++) {
		SavedRasterState &s = rasterStates[i];
		if (memcmp(&s, &rasterState, sizeof(rasterState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddRasterState();

	if (state != curRasterState) {
		context->RSSetState(state);
		curRasterState = state;
	}

	rasterStateChanged = false;
}

void gs_device::UpdateBlendState()
{
	ID3D11BlendState *state = NULL;

	if (!blendStateChanged)
		return;

	for (size_t i = 0; i < blendStates.size(); i++) {
		SavedBlendState &s = blendStates[i];
		if (memcmp(&s, &blendState, sizeof(blendState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddBlendState();

	if (state != curBlendState) {
		float f[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		context->OMSetBlendState(state, f, 0xFFFFFFFF);
		curBlendState = state;
	}

	blendStateChanged = false;
}

void gs_device::UpdateViewProjMatrix()
{
	gs_matrix_get(&curViewMatrix);

	/* negate Z col of the view matrix for right-handed coordinate system */
	curViewMatrix.x.z = -curViewMatrix.x.z;
	curViewMatrix.y.z = -curViewMatrix.y.z;
	curViewMatrix.z.z = -curViewMatrix.z.z;
	curViewMatrix.t.z = -curViewMatrix.t.z;

	matrix4_mul(&curViewProjMatrix, &curViewMatrix, &curProjMatrix);
	matrix4_transpose(&curViewProjMatrix, &curViewProjMatrix);

	if (curVertexShader->viewProj)
		gs_shader_set_matrix4(curVertexShader->viewProj,
				      &curViewProjMatrix);
}

void gs_device::FlushOutputViews()
{
	if (curFramebufferInvalidate) {
		ID3D11RenderTargetView *rtv = nullptr;
		if (curRenderTarget) {
			const int i = curRenderSide;
			rtv = curFramebufferSrgb
				      ? curRenderTarget->renderTargetLinear[i]
						.Get()
				      : curRenderTarget->renderTarget[i].Get();
			if (!rtv) {
				blog(LOG_ERROR,
				     "device_draw (D3D11): texture is not a render target");
				return;
			}
		}
		ID3D11DepthStencilView *dsv = nullptr;
		if (curZStencilBuffer)
			dsv = curZStencilBuffer->view;
		context->OMSetRenderTargets(1, &rtv, dsv);
		curFramebufferInvalidate = false;
	}
}

gs_device::gs_device(uint32_t adapterIdx)
	: curToplogy(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
{
	matrix4_identity(&curProjMatrix);
	matrix4_identity(&curViewMatrix);
	matrix4_identity(&curViewProjMatrix);

	memset(&viewport, 0, sizeof(viewport));

	for (size_t i = 0; i < GS_MAX_TEXTURES; i++) {
		curTextures[i] = NULL;
		curSamplers[i] = NULL;
	}

	InitCompiler();
	InitFactory(adapterIdx);
	InitDevice(adapterIdx);
	device_set_render_target(this, NULL, NULL);
}

gs_device::~gs_device()
{
	context->ClearState();
}

const char *device_get_name(void)
{
	return "Direct3D 11";
}

int device_get_type(void)
{
	return GS_DEVICE_DIRECT3D_11;
}

const char *device_preprocessor_name(void)
{
	return "_D3D11";
}

static inline void
EnumD3DAdapters(bool (*callback)(void *, const char *, uint32_t), void *param)
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i;

	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2
						: __uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void **)factory.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create DXGIFactory", hr);

	for (i = 0; factory->EnumAdapters1(i, adapter.Assign()) == S_OK; ++i) {
		DXGI_ADAPTER_DESC desc;
		char name[512] = "";

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		os_wcs_to_utf8(desc.Description, 0, name, sizeof(name));

		if (!callback(param, name, i))
			break;
	}
}

bool device_enum_adapters(bool (*callback)(void *param, const char *name,
					   uint32_t id),
			  void *param)
{
	try {
		EnumD3DAdapters(callback, param);
		return true;

	} catch (const HRError &error) {
		blog(LOG_WARNING, "Failed enumerating devices: %s (%08lX)",
		     error.str, error.hr);
		return false;
	}
}

static bool GetMonitorTarget(const MONITORINFOEX &info,
			     DISPLAYCONFIG_TARGET_DEVICE_NAME &target)
{
	bool found = false;

	UINT32 numPath, numMode;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPath,
					&numMode) == ERROR_SUCCESS) {
		std::vector<DISPLAYCONFIG_PATH_INFO> paths(numPath);
		std::vector<DISPLAYCONFIG_MODE_INFO> modes(numMode);
		if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPath,
				       paths.data(), &numMode, modes.data(),
				       nullptr) == ERROR_SUCCESS) {
			paths.resize(numPath);
			for (size_t i = 0; i < numPath; ++i) {
				const DISPLAYCONFIG_PATH_INFO &path = paths[i];

				DISPLAYCONFIG_SOURCE_DEVICE_NAME
				source;
				source.header.type =
					DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
				source.header.size = sizeof(source);
				source.header.adapterId =
					path.sourceInfo.adapterId;
				source.header.id = path.sourceInfo.id;
				if (DisplayConfigGetDeviceInfo(
					    &source.header) == ERROR_SUCCESS &&
				    wcscmp(info.szDevice,
					   source.viewGdiDeviceName) == 0) {
					target.header.type =
						DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
					target.header.size = sizeof(target);
					target.header.adapterId =
						path.sourceInfo.adapterId;
					target.header.id = path.targetInfo.id;
					found = DisplayConfigGetDeviceInfo(
							&target.header) ==
						ERROR_SUCCESS;
					break;
				}
			}
		}
	}

	return found;
}

static inline void LogAdapterMonitors(IDXGIAdapter1 *adapter)
{
	UINT i;
	ComPtr<IDXGIOutput> output;

	for (i = 0; adapter->EnumOutputs(i, &output) == S_OK; ++i) {
		DXGI_OUTPUT_DESC desc;
		if (FAILED(output->GetDesc(&desc)))
			continue;

		unsigned refresh = 0;

		bool target_found = false;
		DISPLAYCONFIG_TARGET_DEVICE_NAME target;

		MONITORINFOEX info;
		info.cbSize = sizeof(info);
		if (GetMonitorInfo(desc.Monitor, &info)) {
			target_found = GetMonitorTarget(info, target);

			DEVMODE mode;
			mode.dmSize = sizeof(mode);
			mode.dmDriverExtra = 0;
			if (EnumDisplaySettings(info.szDevice,
						ENUM_CURRENT_SETTINGS, &mode)) {
				refresh = mode.dmDisplayFrequency;
			}
		}

		if (!target_found) {
			target.monitorFriendlyDeviceName[0] = 0;
		}

		const RECT &rect = desc.DesktopCoordinates;
		blog(LOG_INFO,
		     "\t  output %u: "
		     "pos={%d, %d}, "
		     "size={%d, %d}, "
		     "attached=%s, "
		     "refresh=%u, "
		     "name=%ls",
		     i, rect.left, rect.top, rect.right - rect.left,
		     rect.bottom - rect.top,
		     desc.AttachedToDesktop ? "true" : "false", refresh,
		     target.monitorFriendlyDeviceName);
	}
}

static inline void LogD3DAdapters()
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i;

	blog(LOG_INFO, "Available Video Adapters: ");

	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2
						: __uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void **)factory.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create DXGIFactory", hr);

	for (i = 0; factory->EnumAdapters1(i, adapter.Assign()) == S_OK; ++i) {
		DXGI_ADAPTER_DESC desc;
		char name[512] = "";

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		os_wcs_to_utf8(desc.Description, 0, name, sizeof(name));
		blog(LOG_INFO, "\tAdapter %u: %s", i, name);
		blog(LOG_INFO, "\t  Dedicated VRAM: %u",
		     desc.DedicatedVideoMemory);
		blog(LOG_INFO, "\t  Shared VRAM:    %u",
		     desc.SharedSystemMemory);
		blog(LOG_INFO, "\t  PCI ID:         %x:%x", desc.VendorId,
		     desc.DeviceId);

		/* driver version */
		LARGE_INTEGER umd;
		hr = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),
						    &umd);
		if (SUCCEEDED(hr)) {
			const uint64_t version = umd.QuadPart;
			const uint16_t aa = (version >> 48) & 0xffff;
			const uint16_t bb = (version >> 32) & 0xffff;
			const uint16_t ccccc = (version >> 16) & 0xffff;
			const uint16_t ddddd = version & 0xffff;
			blog(LOG_INFO,
			     "\t  Driver Version: %" PRIu16 ".%" PRIu16
			     ".%" PRIu16 ".%" PRIu16,
			     aa, bb, ccccc, ddddd);
		} else {
			blog(LOG_INFO, "\t  Driver Version: Unknown (0x%X)",
			     (unsigned)hr);
		}

		LogAdapterMonitors(adapter);
	}
}

int device_create(gs_device_t **p_device, uint32_t adapter)
{
	gs_device *device = NULL;
	int errorcode = GS_SUCCESS;

	try {
		blog(LOG_INFO, "---------------------------------");
		blog(LOG_INFO, "Initializing D3D11...");
		LogD3DAdapters();

		device = new gs_device(adapter);

	} catch (const UnsupportedHWError &error) {
		blog(LOG_ERROR, "device_create (D3D11): %s (%08lX)", error.str,
		     error.hr);
		errorcode = GS_ERROR_NOT_SUPPORTED;

	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_create (D3D11): %s (%08lX)", error.str,
		     error.hr);
		errorcode = GS_ERROR_FAIL;
	}

	*p_device = device;
	return errorcode;
}

void device_destroy(gs_device_t *device)
{
	delete device;
}

void device_enter_context(gs_device_t *device)
{
	/* does nothing */
	UNUSED_PARAMETER(device);
}

void device_leave_context(gs_device_t *device)
{
	/* does nothing */
	UNUSED_PARAMETER(device);
}

void *device_get_device_obj(gs_device_t *device)
{
	return (void *)device->device.Get();
}

gs_swapchain_t *device_swapchain_create(gs_device_t *device,
					const struct gs_init_data *data)
{
	gs_swap_chain *swap = NULL;

	try {
		swap = new gs_swap_chain(device, data);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_swapchain_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return swap;
}

void device_resize(gs_device_t *device, uint32_t cx, uint32_t cy)
{
	if (!device->curSwapChain) {
		blog(LOG_WARNING, "device_resize (D3D11): No active swap");
		return;
	}

	try {
		ID3D11RenderTargetView *renderView = NULL;
		device->context->OMSetRenderTargets(1, &renderView, NULL);
		device->curSwapChain->Resize(cx, cy);
		device->curFramebufferInvalidate = true;
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_resize (D3D11): %s (%08lX)", error.str,
		     error.hr);
		LogD3D11ErrorDetails(error, device);
	}
}

void device_get_size(const gs_device_t *device, uint32_t *cx, uint32_t *cy)
{
	if (device->curSwapChain) {
		*cx = device->curSwapChain->target.width;
		*cy = device->curSwapChain->target.height;
	} else {
		blog(LOG_ERROR, "device_get_size (D3D11): no active swap");
		*cx = 0;
		*cy = 0;
	}
}

uint32_t device_get_width(const gs_device_t *device)
{
	if (device->curSwapChain) {
		return device->curSwapChain->target.width;
	} else {
		blog(LOG_ERROR, "device_get_size (D3D11): no active swap");
		return 0;
	}
}

uint32_t device_get_height(const gs_device_t *device)
{
	if (device->curSwapChain) {
		return device->curSwapChain->target.height;
	} else {
		blog(LOG_ERROR, "device_get_size (D3D11): no active swap");
		return 0;
	}
}

gs_texture_t *device_texture_create(gs_device_t *device, uint32_t width,
				    uint32_t height,
				    enum gs_color_format color_format,
				    uint32_t levels, const uint8_t **data,
				    uint32_t flags)
{
	gs_texture *texture = NULL;
	try {
		texture = new gs_texture_2d(device, width, height, color_format,
					    levels, data, flags, GS_TEXTURE_2D,
					    false);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_texture_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_texture_create (D3D11): %s", error);
	}

	return texture;
}

gs_texture_t *device_cubetexture_create(gs_device_t *device, uint32_t size,
					enum gs_color_format color_format,
					uint32_t levels, const uint8_t **data,
					uint32_t flags)
{
	gs_texture *texture = NULL;
	try {
		texture = new gs_texture_2d(device, size, size, color_format,
					    levels, data, flags,
					    GS_TEXTURE_CUBE, false);
	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_cubetexture_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_cubetexture_create (D3D11): %s", error);
	}

	return texture;
}

gs_texture_t *device_voltexture_create(gs_device_t *device, uint32_t width,
				       uint32_t height, uint32_t depth,
				       enum gs_color_format color_format,
				       uint32_t levels,
				       const uint8_t *const *data,
				       uint32_t flags)
{
	gs_texture *texture = NULL;
	try {
		texture = new gs_texture_3d(device, width, height, depth,
					    color_format, levels, data, flags);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_voltexture_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_voltexture_create (D3D11): %s", error);
	}

	return texture;
}

gs_zstencil_t *device_zstencil_create(gs_device_t *device, uint32_t width,
				      uint32_t height,
				      enum gs_zstencil_format format)
{
	gs_zstencil_buffer *zstencil = NULL;
	try {
		zstencil =
			new gs_zstencil_buffer(device, width, height, format);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_zstencil_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return zstencil;
}

gs_stagesurf_t *device_stagesurface_create(gs_device_t *device, uint32_t width,
					   uint32_t height,
					   enum gs_color_format color_format)
{
	gs_stage_surface *surf = NULL;
	try {
		surf = new gs_stage_surface(device, width, height,
					    color_format);
	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_stagesurface_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return surf;
}

gs_samplerstate_t *
device_samplerstate_create(gs_device_t *device,
			   const struct gs_sampler_info *info)
{
	gs_sampler_state *ss = NULL;
	try {
		ss = new gs_sampler_state(device, info);
	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_samplerstate_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return ss;
}

gs_shader_t *device_vertexshader_create(gs_device_t *device,
					const char *shader_string,
					const char *file, char **error_string)
{
	gs_vertex_shader *shader = NULL;
	try {
		shader = new gs_vertex_shader(device, file, shader_string);

	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_vertexshader_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);

	} catch (const ShaderError &error) {
		const char *buf =
			(const char *)error.errors->GetBufferPointer();
		if (error_string)
			*error_string = bstrdup(buf);
		blog(LOG_ERROR,
		     "device_vertexshader_create (D3D11): "
		     "Compile warnings/errors for %s:\n%s",
		     file, buf);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_vertexshader_create (D3D11): %s",
		     error);
	}

	return shader;
}

gs_shader_t *device_pixelshader_create(gs_device_t *device,
				       const char *shader_string,
				       const char *file, char **error_string)
{
	gs_pixel_shader *shader = NULL;
	try {
		shader = new gs_pixel_shader(device, file, shader_string);

	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_pixelshader_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);

	} catch (const ShaderError &error) {
		const char *buf =
			(const char *)error.errors->GetBufferPointer();
		if (error_string)
			*error_string = bstrdup(buf);
		blog(LOG_ERROR,
		     "device_pixelshader_create (D3D11): "
		     "Compiler warnings/errors for %s:\n%s",
		     file, buf);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_pixelshader_create (D3D11): %s", error);
	}

	return shader;
}

gs_vertbuffer_t *device_vertexbuffer_create(gs_device_t *device,
					    struct gs_vb_data *data,
					    uint32_t flags)
{
	gs_vertex_buffer *buffer = NULL;
	try {
		buffer = new gs_vertex_buffer(device, data, flags);
	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_vertexbuffer_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_vertexbuffer_create (D3D11): %s",
		     error);
	}

	return buffer;
}

gs_indexbuffer_t *device_indexbuffer_create(gs_device_t *device,
					    enum gs_index_type type,
					    void *indices, size_t num,
					    uint32_t flags)
{
	gs_index_buffer *buffer = NULL;
	try {
		buffer = new gs_index_buffer(device, type, indices, num, flags);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_indexbuffer_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return buffer;
}

gs_timer_t *device_timer_create(gs_device_t *device)
{
	gs_timer *timer = NULL;
	try {
		timer = new gs_timer(device);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_timer_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return timer;
}

gs_timer_range_t *device_timer_range_create(gs_device_t *device)
{
	gs_timer_range *range = NULL;
	try {
		range = new gs_timer_range(device);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_timer_range_create (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return range;
}

enum gs_texture_type device_get_texture_type(const gs_texture_t *texture)
{
	return texture->type;
}

void gs_device::LoadVertexBufferData()
{
	if (curVertexBuffer == lastVertexBuffer &&
	    curVertexShader == lastVertexShader)
		return;

	vector<ID3D11Buffer *> buffers;
	vector<uint32_t> strides;
	vector<uint32_t> offsets;

	if (curVertexBuffer && curVertexShader) {
		curVertexBuffer->MakeBufferList(curVertexShader, buffers,
						strides);
	} else {
		size_t buffersToClear =
			curVertexShader ? curVertexShader->NumBuffersExpected()
					: 0;
		buffers.resize(buffersToClear);
		strides.resize(buffersToClear);
	}

	offsets.resize(buffers.size());
	context->IASetVertexBuffers(0, (UINT)buffers.size(), buffers.data(),
				    strides.data(), offsets.data());

	lastVertexBuffer = curVertexBuffer;
	lastVertexShader = curVertexShader;
}

void device_load_vertexbuffer(gs_device_t *device, gs_vertbuffer_t *vertbuffer)
{
	if (device->curVertexBuffer == vertbuffer)
		return;

	device->curVertexBuffer = vertbuffer;
}

void device_load_indexbuffer(gs_device_t *device, gs_indexbuffer_t *indexbuffer)
{
	DXGI_FORMAT format;
	ID3D11Buffer *buffer;

	if (device->curIndexBuffer == indexbuffer)
		return;

	if (indexbuffer) {
		switch (indexbuffer->indexSize) {
		case 2:
			format = DXGI_FORMAT_R16_UINT;
			break;
		default:
		case 4:
			format = DXGI_FORMAT_R32_UINT;
			break;
		}

		buffer = indexbuffer->indexBuffer;
	} else {
		buffer = NULL;
		format = DXGI_FORMAT_R32_UINT;
	}

	device->curIndexBuffer = indexbuffer;
	device->context->IASetIndexBuffer(buffer, format, 0);
}

static void device_load_texture_internal(gs_device_t *device, gs_texture_t *tex,
					 int unit,
					 ID3D11ShaderResourceView *view)
{
	if (device->curTextures[unit] == tex)
		return;

	device->curTextures[unit] = tex;
	device->context->PSSetShaderResources(unit, 1, &view);
}

void device_load_texture(gs_device_t *device, gs_texture_t *tex, int unit)
{
	ID3D11ShaderResourceView *view;
	if (tex)
		view = tex->shaderRes;
	else
		view = NULL;
	return device_load_texture_internal(device, tex, unit, view);
}

void device_load_texture_srgb(gs_device_t *device, gs_texture_t *tex, int unit)
{
	ID3D11ShaderResourceView *view;
	if (tex)
		view = tex->shaderResLinear;
	else
		view = NULL;
	return device_load_texture_internal(device, tex, unit, view);
}

void device_load_samplerstate(gs_device_t *device,
			      gs_samplerstate_t *samplerstate, int unit)
{
	ID3D11SamplerState *state = NULL;

	if (device->curSamplers[unit] == samplerstate)
		return;

	if (samplerstate)
		state = samplerstate->state;

	device->curSamplers[unit] = samplerstate;
	device->context->PSSetSamplers(unit, 1, &state);
}

void device_load_vertexshader(gs_device_t *device, gs_shader_t *vertshader)
{
	ID3D11VertexShader *shader = NULL;
	ID3D11InputLayout *layout = NULL;
	ID3D11Buffer *constants = NULL;

	if (device->curVertexShader == vertshader)
		return;

	gs_vertex_shader *vs = static_cast<gs_vertex_shader *>(vertshader);

	if (vertshader) {
		if (vertshader->type != GS_SHADER_VERTEX) {
			blog(LOG_ERROR, "device_load_vertexshader (D3D11): "
					"Specified shader is not a vertex "
					"shader");
			return;
		}

		shader = vs->shader;
		layout = vs->layout;
		constants = vs->constants;
	}

	device->curVertexShader = vs;
	device->context->VSSetShader(shader, NULL, 0);
	device->context->IASetInputLayout(layout);
	device->context->VSSetConstantBuffers(0, 1, &constants);
}

static inline void clear_textures(gs_device_t *device)
{
	ID3D11ShaderResourceView *views[GS_MAX_TEXTURES];
	memset(views, 0, sizeof(views));
	memset(device->curTextures, 0, sizeof(device->curTextures));
	device->context->PSSetShaderResources(0, GS_MAX_TEXTURES, views);
}

void device_load_pixelshader(gs_device_t *device, gs_shader_t *pixelshader)
{
	ID3D11PixelShader *shader = NULL;
	ID3D11Buffer *constants = NULL;
	ID3D11SamplerState *states[GS_MAX_TEXTURES];

	if (device->curPixelShader == pixelshader)
		return;

	gs_pixel_shader *ps = static_cast<gs_pixel_shader *>(pixelshader);

	if (pixelshader) {
		if (pixelshader->type != GS_SHADER_PIXEL) {
			blog(LOG_ERROR, "device_load_pixelshader (D3D11): "
					"Specified shader is not a pixel "
					"shader");
			return;
		}

		shader = ps->shader;
		constants = ps->constants;
		ps->GetSamplerStates(states);
	} else {
		memset(states, 0, sizeof(states));
	}

	clear_textures(device);

	device->curPixelShader = ps;
	device->context->PSSetShader(shader, NULL, 0);
	device->context->PSSetConstantBuffers(0, 1, &constants);
	device->context->PSSetSamplers(0, GS_MAX_TEXTURES, states);

	for (int i = 0; i < GS_MAX_TEXTURES; i++)
		if (device->curSamplers[i] &&
		    device->curSamplers[i]->state != states[i])
			device->curSamplers[i] = nullptr;
}

void device_load_default_samplerstate(gs_device_t *device, bool b_3d, int unit)
{
	/* TODO */
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(b_3d);
	UNUSED_PARAMETER(unit);
}

gs_shader_t *device_get_vertex_shader(const gs_device_t *device)
{
	return device->curVertexShader;
}

gs_shader_t *device_get_pixel_shader(const gs_device_t *device)
{
	return device->curPixelShader;
}

gs_texture_t *device_get_render_target(const gs_device_t *device)
{
	if (device->curRenderTarget == &device->curSwapChain->target)
		return NULL;

	return device->curRenderTarget;
}

gs_zstencil_t *device_get_zstencil_target(const gs_device_t *device)
{
	if (device->curZStencilBuffer == &device->curSwapChain->zs)
		return NULL;

	return device->curZStencilBuffer;
}

void device_set_render_target(gs_device_t *device, gs_texture_t *tex,
			      gs_zstencil_t *zstencil)
{
	if (device->curSwapChain) {
		if (!tex)
			tex = &device->curSwapChain->target;
		if (!zstencil)
			zstencil = &device->curSwapChain->zs;
	}

	if (device->curRenderTarget == tex &&
	    device->curZStencilBuffer == zstencil)
		return;

	if (tex && tex->type != GS_TEXTURE_2D) {
		blog(LOG_ERROR,
		     "device_set_render_target (D3D11): texture is not a 2D texture");
		return;
	}

	gs_texture_2d *const tex2d = static_cast<gs_texture_2d *>(tex);
	if (device->curRenderTarget != tex2d || device->curRenderSide != 0 ||
	    device->curZStencilBuffer != zstencil) {
		device->curRenderTarget = tex2d;
		device->curRenderSide = 0;
		device->curZStencilBuffer = zstencil;
		device->curFramebufferInvalidate = true;
	}
}

void device_set_cube_render_target(gs_device_t *device, gs_texture_t *tex,
				   int side, gs_zstencil_t *zstencil)
{
	if (device->curSwapChain) {
		if (!tex) {
			tex = &device->curSwapChain->target;
			side = 0;
		}

		if (!zstencil)
			zstencil = &device->curSwapChain->zs;
	}

	if (device->curRenderTarget == tex && device->curRenderSide == side &&
	    device->curZStencilBuffer == zstencil)
		return;

	if (tex->type != GS_TEXTURE_CUBE) {
		blog(LOG_ERROR, "device_set_cube_render_target (D3D11): "
				"texture is not a cube texture");
		return;
	}

	gs_texture_2d *const tex2d = static_cast<gs_texture_2d *>(tex);
	if (device->curRenderTarget != tex2d || device->curRenderSide != side ||
	    device->curZStencilBuffer != zstencil) {
		device->curRenderTarget = tex2d;
		device->curRenderSide = side;
		device->curZStencilBuffer = zstencil;
		device->curFramebufferInvalidate = true;
	}
}

void device_enable_framebuffer_srgb(gs_device_t *device, bool enable)
{
	if (device->curFramebufferSrgb != enable) {
		device->curFramebufferSrgb = enable;
		device->curFramebufferInvalidate = true;
	}
}

bool device_framebuffer_srgb_enabled(gs_device_t *device)
{
	return device->curFramebufferSrgb;
}

inline void gs_device::CopyTex(ID3D11Texture2D *dst, uint32_t dst_x,
			       uint32_t dst_y, gs_texture_t *src,
			       uint32_t src_x, uint32_t src_y, uint32_t src_w,
			       uint32_t src_h)
{
	if (src->type != GS_TEXTURE_2D)
		throw "Source texture must be a 2D texture";

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(src);

	if (dst_x == 0 && dst_y == 0 && src_x == 0 && src_y == 0 &&
	    src_w == 0 && src_h == 0) {
		context->CopyResource(dst, tex2d->texture);
	} else {
		D3D11_BOX sbox;

		sbox.left = src_x;
		if (src_w > 0)
			sbox.right = src_x + src_w;
		else
			sbox.right = tex2d->width - 1;

		sbox.top = src_y;
		if (src_h > 0)
			sbox.bottom = src_y + src_h;
		else
			sbox.bottom = tex2d->height - 1;

		sbox.front = 0;
		sbox.back = 1;

		context->CopySubresourceRegion(dst, 0, dst_x, dst_y, 0,
					       tex2d->texture, 0, &sbox);
	}
}

void device_copy_texture_region(gs_device_t *device, gs_texture_t *dst,
				uint32_t dst_x, uint32_t dst_y,
				gs_texture_t *src, uint32_t src_x,
				uint32_t src_y, uint32_t src_w, uint32_t src_h)
{
	try {
		gs_texture_2d *src2d = static_cast<gs_texture_2d *>(src);
		gs_texture_2d *dst2d = static_cast<gs_texture_2d *>(dst);

		if (!src)
			throw "Source texture is NULL";
		if (!dst)
			throw "Destination texture is NULL";
		if (src->type != GS_TEXTURE_2D || dst->type != GS_TEXTURE_2D)
			throw "Source and destination textures must be a 2D "
			      "textures";
		if (dst->format != src->format)
			throw "Source and destination formats do not match";

		/* apparently casting to the same type that the variable
		 * already exists as is supposed to prevent some warning
		 * when used with the conditional operator? */
		uint32_t copyWidth = (uint32_t)src_w ? (uint32_t)src_w
						     : (src2d->width - src_x);
		uint32_t copyHeight = (uint32_t)src_h ? (uint32_t)src_h
						      : (src2d->height - src_y);

		uint32_t dstWidth = dst2d->width - dst_x;
		uint32_t dstHeight = dst2d->height - dst_y;

		if (dstWidth < copyWidth || dstHeight < copyHeight)
			throw "Destination texture region is not big "
			      "enough to hold the source region";

		if (dst_x == 0 && dst_y == 0 && src_x == 0 && src_y == 0 &&
		    src_w == 0 && src_h == 0) {
			copyWidth = 0;
			copyHeight = 0;
		}

		device->CopyTex(dst2d->texture, dst_x, dst_y, src, src_x, src_y,
				copyWidth, copyHeight);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_copy_texture (D3D11): %s", error);
	}
}

void device_copy_texture(gs_device_t *device, gs_texture_t *dst,
			 gs_texture_t *src)
{
	device_copy_texture_region(device, dst, 0, 0, src, 0, 0, 0, 0);
}

void device_stage_texture(gs_device_t *device, gs_stagesurf_t *dst,
			  gs_texture_t *src)
{
	try {
		gs_texture_2d *src2d = static_cast<gs_texture_2d *>(src);

		if (!src)
			throw "Source texture is NULL";
		if (src->type != GS_TEXTURE_2D)
			throw "Source texture must be a 2D texture";
		if (!dst)
			throw "Destination surface is NULL";
		if (dst->format != GS_UNKNOWN && dst->format != src->format)
			throw "Source and destination formats do not match";
		if (dst->width != src2d->width || dst->height != src2d->height)
			throw "Source and destination must have the same "
			      "dimensions";

		device->CopyTex(dst->texture, 0, 0, src, 0, 0, 0, 0);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_copy_texture (D3D11): %s", error);
	}
}

extern "C" void reset_duplicators(void);

void device_begin_frame(gs_device_t *device)
{
	/* does nothing in D3D11 */
	UNUSED_PARAMETER(device);

	reset_duplicators();
}

void device_begin_scene(gs_device_t *device)
{
	clear_textures(device);
}

void device_draw(gs_device_t *device, enum gs_draw_mode draw_mode,
		 uint32_t start_vert, uint32_t num_verts)
{
	try {
		if (!device->curVertexShader)
			throw "No vertex shader specified";

		if (!device->curPixelShader)
			throw "No pixel shader specified";

		if (!device->curVertexBuffer && (num_verts == 0))
			throw "No vertex buffer specified";

		if (!device->curSwapChain && !device->curRenderTarget)
			throw "No render target or swap chain to render to";

		device->FlushOutputViews();

		gs_effect_t *effect = gs_get_effect();
		if (effect)
			gs_effect_update_params(effect);

		device->LoadVertexBufferData();
		device->UpdateBlendState();
		device->UpdateRasterState();
		device->UpdateZStencilState();
		device->UpdateViewProjMatrix();
		device->curVertexShader->UploadParams();
		device->curPixelShader->UploadParams();

	} catch (const char *error) {
		blog(LOG_ERROR, "device_draw (D3D11): %s", error);
		return;

	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_draw (D3D11): %s (%08lX)", error.str,
		     error.hr);
		LogD3D11ErrorDetails(error, device);
		return;
	}

	D3D11_PRIMITIVE_TOPOLOGY newTopology = ConvertGSTopology(draw_mode);
	if (device->curToplogy != newTopology) {
		device->context->IASetPrimitiveTopology(newTopology);
		device->curToplogy = newTopology;
	}

	if (device->curIndexBuffer) {
		if (num_verts == 0)
			num_verts = (uint32_t)device->curIndexBuffer->num;
		device->context->DrawIndexed(num_verts, start_vert, 0);
	} else {
		if (num_verts == 0)
			num_verts = (uint32_t)device->curVertexBuffer->numVerts;
		device->context->Draw(num_verts, start_vert);
	}
}

void device_end_scene(gs_device_t *device)
{
	/* does nothing in D3D11 */
	UNUSED_PARAMETER(device);
}

void device_load_swapchain(gs_device_t *device, gs_swapchain_t *swapchain)
{
	gs_texture_t *target = device->curRenderTarget;
	gs_zstencil_t *zs = device->curZStencilBuffer;
	bool is_cube =
		device->curRenderTarget
			? (device->curRenderTarget->type == GS_TEXTURE_CUBE)
			: false;

	if (device->curSwapChain) {
		if (target == &device->curSwapChain->target)
			target = NULL;
		if (zs == &device->curSwapChain->zs)
			zs = NULL;
	}

	device->curSwapChain = swapchain;

	if (is_cube)
		device_set_cube_render_target(device, target,
					      device->curRenderSide, zs);
	else
		device_set_render_target(device, target, zs);
}

void device_clear(gs_device_t *device, uint32_t clear_flags,
		  const struct vec4 *color, float depth, uint8_t stencil)
{
	int side = device->curRenderSide;
	if ((clear_flags & GS_CLEAR_COLOR) != 0 && device->curRenderTarget)
		device->context->ClearRenderTargetView(
			device->curRenderTarget->renderTarget[side],
			color->ptr);

	if (device->curZStencilBuffer) {
		uint32_t flags = 0;
		if ((clear_flags & GS_CLEAR_DEPTH) != 0)
			flags |= D3D11_CLEAR_DEPTH;
		if ((clear_flags & GS_CLEAR_STENCIL) != 0)
			flags |= D3D11_CLEAR_STENCIL;

		if (flags && device->curZStencilBuffer->view)
			device->context->ClearDepthStencilView(
				device->curZStencilBuffer->view, flags, depth,
				stencil);
	}
}

void device_present(gs_device_t *device)
{
	HRESULT hr;

	if (device->curSwapChain) {
		hr = device->curSwapChain->swap->Present(0, 0);
		if (hr == DXGI_ERROR_DEVICE_REMOVED ||
		    hr == DXGI_ERROR_DEVICE_RESET) {
			device->RebuildDevice();
		}
	} else {
		blog(LOG_WARNING, "device_present (D3D11): No active swap");
	}
}

void device_flush(gs_device_t *device)
{
	device->context->Flush();
}

void device_set_cull_mode(gs_device_t *device, enum gs_cull_mode mode)
{
	if (mode == device->rasterState.cullMode)
		return;

	device->rasterState.cullMode = mode;
	device->rasterStateChanged = true;
}

enum gs_cull_mode device_get_cull_mode(const gs_device_t *device)
{
	return device->rasterState.cullMode;
}

void device_enable_blending(gs_device_t *device, bool enable)
{
	if (enable == device->blendState.blendEnabled)
		return;

	device->blendState.blendEnabled = enable;
	device->blendStateChanged = true;
}

void device_enable_depth_test(gs_device_t *device, bool enable)
{
	if (enable == device->zstencilState.depthEnabled)
		return;

	device->zstencilState.depthEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_stencil_test(gs_device_t *device, bool enable)
{
	if (enable == device->zstencilState.stencilEnabled)
		return;

	device->zstencilState.stencilEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_stencil_write(gs_device_t *device, bool enable)
{
	if (enable == device->zstencilState.stencilWriteEnabled)
		return;

	device->zstencilState.stencilWriteEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_color(gs_device_t *device, bool red, bool green, bool blue,
			 bool alpha)
{
	if (device->blendState.redEnabled == red &&
	    device->blendState.greenEnabled == green &&
	    device->blendState.blueEnabled == blue &&
	    device->blendState.alphaEnabled == alpha)
		return;

	device->blendState.redEnabled = red;
	device->blendState.greenEnabled = green;
	device->blendState.blueEnabled = blue;
	device->blendState.alphaEnabled = alpha;
	device->blendStateChanged = true;
}

void device_blend_function(gs_device_t *device, enum gs_blend_type src,
			   enum gs_blend_type dest)
{
	if (device->blendState.srcFactorC == src &&
	    device->blendState.destFactorC == dest &&
	    device->blendState.srcFactorA == src &&
	    device->blendState.destFactorA == dest)
		return;

	device->blendState.srcFactorC = src;
	device->blendState.destFactorC = dest;
	device->blendState.srcFactorA = src;
	device->blendState.destFactorA = dest;
	device->blendStateChanged = true;
}

void device_blend_function_separate(gs_device_t *device,
				    enum gs_blend_type src_c,
				    enum gs_blend_type dest_c,
				    enum gs_blend_type src_a,
				    enum gs_blend_type dest_a)
{
	if (device->blendState.srcFactorC == src_c &&
	    device->blendState.destFactorC == dest_c &&
	    device->blendState.srcFactorA == src_a &&
	    device->blendState.destFactorA == dest_a)
		return;

	device->blendState.srcFactorC = src_c;
	device->blendState.destFactorC = dest_c;
	device->blendState.srcFactorA = src_a;
	device->blendState.destFactorA = dest_a;
	device->blendStateChanged = true;
}

void device_depth_function(gs_device_t *device, enum gs_depth_test test)
{
	if (device->zstencilState.depthFunc == test)
		return;

	device->zstencilState.depthFunc = test;
	device->zstencilStateChanged = true;
}

static inline void update_stencilside_test(gs_device_t *device,
					   StencilSide &side,
					   gs_depth_test test)
{
	if (side.test == test)
		return;

	side.test = test;
	device->zstencilStateChanged = true;
}

void device_stencil_function(gs_device_t *device, enum gs_stencil_side side,
			     enum gs_depth_test test)
{
	int sideVal = (int)side;

	if (sideVal & GS_STENCIL_FRONT)
		update_stencilside_test(
			device, device->zstencilState.stencilFront, test);
	if (sideVal & GS_STENCIL_BACK)
		update_stencilside_test(
			device, device->zstencilState.stencilBack, test);
}

static inline void update_stencilside_op(gs_device_t *device, StencilSide &side,
					 enum gs_stencil_op_type fail,
					 enum gs_stencil_op_type zfail,
					 enum gs_stencil_op_type zpass)
{
	if (side.fail == fail && side.zfail == zfail && side.zpass == zpass)
		return;

	side.fail = fail;
	side.zfail = zfail;
	side.zpass = zpass;
	device->zstencilStateChanged = true;
}

void device_stencil_op(gs_device_t *device, enum gs_stencil_side side,
		       enum gs_stencil_op_type fail,
		       enum gs_stencil_op_type zfail,
		       enum gs_stencil_op_type zpass)
{
	int sideVal = (int)side;

	if (sideVal & GS_STENCIL_FRONT)
		update_stencilside_op(device,
				      device->zstencilState.stencilFront, fail,
				      zfail, zpass);
	if (sideVal & GS_STENCIL_BACK)
		update_stencilside_op(device, device->zstencilState.stencilBack,
				      fail, zfail, zpass);
}

void device_set_viewport(gs_device_t *device, int x, int y, int width,
			 int height)
{
	D3D11_VIEWPORT vp;
	memset(&vp, 0, sizeof(vp));
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = (float)x;
	vp.TopLeftY = (float)y;
	vp.Width = (float)width;
	vp.Height = (float)height;
	device->context->RSSetViewports(1, &vp);

	device->viewport.x = x;
	device->viewport.y = y;
	device->viewport.cx = width;
	device->viewport.cy = height;
}

void device_get_viewport(const gs_device_t *device, struct gs_rect *rect)
{
	memcpy(rect, &device->viewport, sizeof(gs_rect));
}

void device_set_scissor_rect(gs_device_t *device, const struct gs_rect *rect)
{
	D3D11_RECT d3drect;

	device->rasterState.scissorEnabled = (rect != NULL);

	if (rect != NULL) {
		d3drect.left = rect->x;
		d3drect.top = rect->y;
		d3drect.right = rect->x + rect->cx;
		d3drect.bottom = rect->y + rect->cy;
		device->context->RSSetScissorRects(1, &d3drect);
	}

	device->rasterStateChanged = true;
}

void device_ortho(gs_device_t *device, float left, float right, float top,
		  float bottom, float zNear, float zFar)
{
	matrix4 *dst = &device->curProjMatrix;

	float rml = right - left;
	float bmt = bottom - top;
	float fmn = zFar - zNear;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x = 2.0f / rml;
	dst->t.x = (left + right) / -rml;

	dst->y.y = 2.0f / -bmt;
	dst->t.y = (bottom + top) / bmt;

	dst->z.z = 1.0f / fmn;
	dst->t.z = zNear / -fmn;

	dst->t.w = 1.0f;
}

void device_frustum(gs_device_t *device, float left, float right, float top,
		    float bottom, float zNear, float zFar)
{
	matrix4 *dst = &device->curProjMatrix;

	float rml = right - left;
	float bmt = bottom - top;
	float fmn = zFar - zNear;
	float nearx2 = 2.0f * zNear;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x = nearx2 / rml;
	dst->z.x = (left + right) / -rml;

	dst->y.y = nearx2 / -bmt;
	dst->z.y = (bottom + top) / bmt;

	dst->z.z = zFar / fmn;
	dst->t.z = (zNear * zFar) / -fmn;

	dst->z.w = 1.0f;
}

void device_projection_push(gs_device_t *device)
{
	mat4float mat;
	memcpy(&mat, &device->curProjMatrix, sizeof(matrix4));
	device->projStack.push_back(mat);
}

void device_projection_pop(gs_device_t *device)
{
	if (device->projStack.empty())
		return;

	const mat4float &mat = device->projStack.back();
	memcpy(&device->curProjMatrix, &mat, sizeof(matrix4));
	device->projStack.pop_back();
}

void gs_swapchain_destroy(gs_swapchain_t *swapchain)
{
	if (swapchain->device->curSwapChain == swapchain)
		device_load_swapchain(swapchain->device, nullptr);

	delete swapchain;
}

void gs_texture_destroy(gs_texture_t *tex)
{
	delete tex;
}

uint32_t gs_texture_get_width(const gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return 0;

	return static_cast<const gs_texture_2d *>(tex)->width;
}

uint32_t gs_texture_get_height(const gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return 0;

	return static_cast<const gs_texture_2d *>(tex)->height;
}

enum gs_color_format gs_texture_get_color_format(const gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return GS_UNKNOWN;

	return static_cast<const gs_texture_2d *>(tex)->format;
}

bool gs_texture_map(gs_texture_t *tex, uint8_t **ptr, uint32_t *linesize)
{
	HRESULT hr;

	if (tex->type != GS_TEXTURE_2D)
		return false;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(tex);

	D3D11_MAPPED_SUBRESOURCE map;
	hr = tex2d->device->context->Map(tex2d->texture, 0,
					 D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (FAILED(hr))
		return false;

	*ptr = (uint8_t *)map.pData;
	*linesize = map.RowPitch;
	return true;
}

void gs_texture_unmap(gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(tex);
	tex2d->device->context->Unmap(tex2d->texture, 0);
}

void *gs_texture_get_obj(gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return nullptr;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(tex);
	return tex2d->texture.Get();
}

void gs_cubetexture_destroy(gs_texture_t *cubetex)
{
	delete cubetex;
}

uint32_t gs_cubetexture_get_size(const gs_texture_t *cubetex)
{
	if (cubetex->type != GS_TEXTURE_CUBE)
		return 0;

	const gs_texture_2d *tex = static_cast<const gs_texture_2d *>(cubetex);
	return tex->width;
}

enum gs_color_format
gs_cubetexture_get_color_format(const gs_texture_t *cubetex)
{
	if (cubetex->type != GS_TEXTURE_CUBE)
		return GS_UNKNOWN;

	const gs_texture_2d *tex = static_cast<const gs_texture_2d *>(cubetex);
	return tex->format;
}

void gs_voltexture_destroy(gs_texture_t *voltex)
{
	delete voltex;
}

uint32_t gs_voltexture_get_width(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

uint32_t gs_voltexture_get_height(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

uint32_t gs_voltexture_get_depth(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return 0;
}

enum gs_color_format gs_voltexture_get_color_format(const gs_texture_t *voltex)
{
	/* TODO */
	UNUSED_PARAMETER(voltex);
	return GS_UNKNOWN;
}

void gs_stagesurface_destroy(gs_stagesurf_t *stagesurf)
{
	delete stagesurf;
}

uint32_t gs_stagesurface_get_width(const gs_stagesurf_t *stagesurf)
{
	return stagesurf->width;
}

uint32_t gs_stagesurface_get_height(const gs_stagesurf_t *stagesurf)
{
	return stagesurf->height;
}

enum gs_color_format
gs_stagesurface_get_color_format(const gs_stagesurf_t *stagesurf)
{
	return stagesurf->format;
}

bool gs_stagesurface_map(gs_stagesurf_t *stagesurf, uint8_t **data,
			 uint32_t *linesize)
{
	D3D11_MAPPED_SUBRESOURCE map;
	if (FAILED(stagesurf->device->context->Map(stagesurf->texture, 0,
						   D3D11_MAP_READ, 0, &map)))
		return false;

	*data = (uint8_t *)map.pData;
	*linesize = map.RowPitch;
	return true;
}

void gs_stagesurface_unmap(gs_stagesurf_t *stagesurf)
{
	stagesurf->device->context->Unmap(stagesurf->texture, 0);
}

void gs_zstencil_destroy(gs_zstencil_t *zstencil)
{
	delete zstencil;
}

void gs_samplerstate_destroy(gs_samplerstate_t *samplerstate)
{
	if (!samplerstate)
		return;

	if (samplerstate->device)
		for (int i = 0; i < GS_MAX_TEXTURES; i++)
			if (samplerstate->device->curSamplers[i] ==
			    samplerstate)
				samplerstate->device->curSamplers[i] = nullptr;

	delete samplerstate;
}

void gs_vertexbuffer_destroy(gs_vertbuffer_t *vertbuffer)
{
	if (vertbuffer && vertbuffer->device->lastVertexBuffer == vertbuffer)
		vertbuffer->device->lastVertexBuffer = nullptr;
	delete vertbuffer;
}

static inline void gs_vertexbuffer_flush_internal(gs_vertbuffer_t *vertbuffer,
						  const gs_vb_data *data)
{
	size_t num_tex = data->num_tex < vertbuffer->uvBuffers.size()
				 ? data->num_tex
				 : vertbuffer->uvBuffers.size();

	if (!vertbuffer->dynamic) {
		blog(LOG_ERROR, "gs_vertexbuffer_flush: vertex buffer is "
				"not dynamic");
		return;
	}

	if (data->points)
		vertbuffer->FlushBuffer(vertbuffer->vertexBuffer, data->points,
					sizeof(vec3));

	if (vertbuffer->normalBuffer && data->normals)
		vertbuffer->FlushBuffer(vertbuffer->normalBuffer, data->normals,
					sizeof(vec3));

	if (vertbuffer->tangentBuffer && data->tangents)
		vertbuffer->FlushBuffer(vertbuffer->tangentBuffer,
					data->tangents, sizeof(vec3));

	if (vertbuffer->colorBuffer && data->colors)
		vertbuffer->FlushBuffer(vertbuffer->colorBuffer, data->colors,
					sizeof(uint32_t));

	for (size_t i = 0; i < num_tex; i++) {
		gs_tvertarray &tv = data->tvarray[i];
		vertbuffer->FlushBuffer(vertbuffer->uvBuffers[i], tv.array,
					tv.width * sizeof(float));
	}
}

void gs_vertexbuffer_flush(gs_vertbuffer_t *vertbuffer)
{
	gs_vertexbuffer_flush_internal(vertbuffer, vertbuffer->vbd.data);
}

void gs_vertexbuffer_flush_direct(gs_vertbuffer_t *vertbuffer,
				  const gs_vb_data *data)
{
	gs_vertexbuffer_flush_internal(vertbuffer, data);
}

struct gs_vb_data *gs_vertexbuffer_get_data(const gs_vertbuffer_t *vertbuffer)
{
	return vertbuffer->vbd.data;
}

void gs_indexbuffer_destroy(gs_indexbuffer_t *indexbuffer)
{
	delete indexbuffer;
}

static inline void gs_indexbuffer_flush_internal(gs_indexbuffer_t *indexbuffer,
						 const void *data)
{
	HRESULT hr;

	if (!indexbuffer->dynamic)
		return;

	D3D11_MAPPED_SUBRESOURCE map;
	hr = indexbuffer->device->context->Map(
		indexbuffer->indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (FAILED(hr))
		return;

	memcpy(map.pData, data, indexbuffer->num * indexbuffer->indexSize);

	indexbuffer->device->context->Unmap(indexbuffer->indexBuffer, 0);
}

void gs_indexbuffer_flush(gs_indexbuffer_t *indexbuffer)
{
	gs_indexbuffer_flush_internal(indexbuffer, indexbuffer->indices.data);
}

void gs_indexbuffer_flush_direct(gs_indexbuffer_t *indexbuffer,
				 const void *data)
{
	gs_indexbuffer_flush_internal(indexbuffer, data);
}

void *gs_indexbuffer_get_data(const gs_indexbuffer_t *indexbuffer)
{
	return indexbuffer->indices.data;
}

size_t gs_indexbuffer_get_num_indices(const gs_indexbuffer_t *indexbuffer)
{
	return indexbuffer->num;
}

enum gs_index_type gs_indexbuffer_get_type(const gs_indexbuffer_t *indexbuffer)
{
	return indexbuffer->type;
}

void gs_timer_destroy(gs_timer_t *timer)
{
	delete timer;
}

void gs_timer_begin(gs_timer_t *timer)
{
	timer->device->context->End(timer->query_begin);
}

void gs_timer_end(gs_timer_t *timer)
{
	timer->device->context->End(timer->query_end);
}

bool gs_timer_get_data(gs_timer_t *timer, uint64_t *ticks)
{
	uint64_t begin, end;
	HRESULT hr_begin, hr_end;
	do {
		hr_begin = timer->device->context->GetData(
			timer->query_begin, &begin, sizeof(begin), 0);
	} while (hr_begin == S_FALSE);
	do {
		hr_end = timer->device->context->GetData(timer->query_end, &end,
							 sizeof(end), 0);
	} while (hr_end == S_FALSE);

	const bool succeeded = SUCCEEDED(hr_begin) && SUCCEEDED(hr_end);
	if (succeeded)
		*ticks = end - begin;

	return succeeded;
}

void gs_timer_range_destroy(gs_timer_range_t *range)
{
	delete range;
}

void gs_timer_range_begin(gs_timer_range_t *range)
{
	range->device->context->Begin(range->query_disjoint);
}

void gs_timer_range_end(gs_timer_range_t *range)
{
	range->device->context->End(range->query_disjoint);
}

bool gs_timer_range_get_data(gs_timer_range_t *range, bool *disjoint,
			     uint64_t *frequency)
{
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestamp_disjoint;
	HRESULT hr;
	do {
		hr = range->device->context->GetData(range->query_disjoint,
						     &timestamp_disjoint,
						     sizeof(timestamp_disjoint),
						     0);
	} while (hr == S_FALSE);

	const bool succeeded = SUCCEEDED(hr);
	if (succeeded) {
		*disjoint = timestamp_disjoint.Disjoint;
		*frequency = timestamp_disjoint.Frequency;
	}

	return succeeded;
}

gs_timer::gs_timer(gs_device_t *device) : gs_obj(device, gs_type::gs_timer)
{
	Rebuild(device->device);
}

gs_timer_range::gs_timer_range(gs_device_t *device)
	: gs_obj(device, gs_type::gs_timer_range)
{
	Rebuild(device->device);
}

extern "C" EXPORT bool device_gdi_texture_available(void)
{
	return true;
}

extern "C" EXPORT bool device_shared_texture_available(void)
{
	return true;
}

extern "C" EXPORT bool device_nv12_available(gs_device_t *device)
{
	return device->nv12Supported;
}

extern "C" EXPORT void device_debug_marker_begin(gs_device_t *,
						 const char *markername,
						 const float color[4])
{
	D3DCOLOR bgra = D3DCOLOR_ARGB((DWORD)(255.0f * color[3]),
				      (DWORD)(255.0f * color[0]),
				      (DWORD)(255.0f * color[1]),
				      (DWORD)(255.0f * color[2]));

	wchar_t wide[64];
	os_utf8_to_wcs(markername, 0, wide, _countof(wide));

	D3DPERF_BeginEvent(bgra, wide);
}

extern "C" EXPORT void device_debug_marker_end(gs_device_t *)
{
	D3DPERF_EndEvent();
}

extern "C" EXPORT gs_texture_t *
device_texture_create_gdi(gs_device_t *device, uint32_t width, uint32_t height)
{
	gs_texture *texture = nullptr;
	try {
		texture = new gs_texture_2d(device, width, height,
					    GS_BGRA_UNORM, 1, nullptr,
					    GS_RENDER_TARGET, GS_TEXTURE_2D,
					    true);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "device_texture_create_gdi (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_texture_create_gdi (D3D11): %s", error);
	}

	return texture;
}

static inline bool TextureGDICompatible(gs_texture_2d *tex2d, const char *func)
{
	if (!tex2d->isGDICompatible) {
		blog(LOG_ERROR, "%s (D3D11): Texture is not GDI compatible",
		     func);
		return false;
	}

	return true;
}

extern "C" EXPORT void *gs_texture_get_dc(gs_texture_t *tex)
{
	HDC hDC = nullptr;

	if (tex->type != GS_TEXTURE_2D)
		return nullptr;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(tex);
	if (!TextureGDICompatible(tex2d, "gs_texture_get_dc"))
		return nullptr;

	if (!tex2d->gdiSurface)
		return nullptr;

	tex2d->gdiSurface->GetDC(true, &hDC);
	return hDC;
}

extern "C" EXPORT void gs_texture_release_dc(gs_texture_t *tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d *>(tex);
	if (!TextureGDICompatible(tex2d, "gs_texture_release_dc"))
		return;

	tex2d->gdiSurface->ReleaseDC(nullptr);
}

extern "C" EXPORT gs_texture_t *device_texture_open_shared(gs_device_t *device,
							   uint32_t handle)
{
	gs_texture *texture = nullptr;
	try {
		texture = new gs_texture_2d(device, handle);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "gs_texture_open_shared (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "gs_texture_open_shared (D3D11): %s", error);
	}

	return texture;
}

extern "C" EXPORT uint32_t device_texture_get_shared_handle(gs_texture_t *tex)
{
	gs_texture_2d *tex2d = reinterpret_cast<gs_texture_2d *>(tex);
	if (tex->type != GS_TEXTURE_2D)
		return GS_INVALID_HANDLE;

	return tex2d->isShared ? tex2d->sharedHandle : GS_INVALID_HANDLE;
}

extern "C" EXPORT gs_texture_t *device_texture_wrap_obj(gs_device_t *device,
							void *obj)
{
	gs_texture *texture = nullptr;
	try {
		texture = new gs_texture_2d(device, (ID3D11Texture2D *)obj);
	} catch (const HRError &error) {
		blog(LOG_ERROR, "gs_texture_wrap_obj (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	} catch (const char *error) {
		blog(LOG_ERROR, "gs_texture_wrap_obj (D3D11): %s", error);
	}

	return texture;
}

int device_texture_acquire_sync(gs_texture_t *tex, uint64_t key, uint32_t ms)
{
	gs_texture_2d *tex2d = reinterpret_cast<gs_texture_2d *>(tex);
	if (tex->type != GS_TEXTURE_2D)
		return -1;

	if (tex2d->acquired)
		return 0;

	ComQIPtr<IDXGIKeyedMutex> keyedMutex(tex2d->texture);
	if (!keyedMutex)
		return -1;

	HRESULT hr = keyedMutex->AcquireSync(key, ms);
	if (hr == S_OK) {
		tex2d->acquired = true;
		return 0;
	} else if (hr == WAIT_TIMEOUT) {
		return ETIMEDOUT;
	}

	return -1;
}

extern "C" EXPORT int device_texture_release_sync(gs_texture_t *tex,
						  uint64_t key)
{
	gs_texture_2d *tex2d = reinterpret_cast<gs_texture_2d *>(tex);
	if (tex->type != GS_TEXTURE_2D)
		return -1;

	if (!tex2d->acquired)
		return 0;

	ComQIPtr<IDXGIKeyedMutex> keyedMutex(tex2d->texture);
	if (!keyedMutex)
		return -1;

	HRESULT hr = keyedMutex->ReleaseSync(key);
	if (hr == S_OK) {
		tex2d->acquired = false;
		return 0;
	}

	return -1;
}

extern "C" EXPORT bool
device_texture_create_nv12(gs_device_t *device, gs_texture_t **p_tex_y,
			   gs_texture_t **p_tex_uv, uint32_t width,
			   uint32_t height, uint32_t flags)
{
	if (!device->nv12Supported)
		return false;

	*p_tex_y = nullptr;
	*p_tex_uv = nullptr;

	gs_texture_2d *tex_y;
	gs_texture_2d *tex_uv;

	try {
		tex_y = new gs_texture_2d(device, width, height, GS_R8, 1,
					  nullptr, flags, GS_TEXTURE_2D, false,
					  true);
		tex_uv = new gs_texture_2d(device, tex_y->texture, flags);

	} catch (const HRError &error) {
		blog(LOG_ERROR, "gs_texture_create_nv12 (D3D11): %s (%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
		return false;

	} catch (const char *error) {
		blog(LOG_ERROR, "gs_texture_create_nv12 (D3D11): %s", error);
		return false;
	}

	tex_y->pairedNV12texture = tex_uv;
	tex_uv->pairedNV12texture = tex_y;

	*p_tex_y = tex_y;
	*p_tex_uv = tex_uv;
	return true;
}

extern "C" EXPORT gs_stagesurf_t *
device_stagesurface_create_nv12(gs_device_t *device, uint32_t width,
				uint32_t height)
{
	gs_stage_surface *surf = NULL;
	try {
		surf = new gs_stage_surface(device, width, height);
	} catch (const HRError &error) {
		blog(LOG_ERROR,
		     "device_stagesurface_create (D3D11): %s "
		     "(%08lX)",
		     error.str, error.hr);
		LogD3D11ErrorDetails(error, device);
	}

	return surf;
}

extern "C" EXPORT void
device_register_loss_callbacks(gs_device_t *device,
			       const gs_device_loss *callbacks)
{
	device->loss_callbacks.emplace_back(*callbacks);
}

extern "C" EXPORT void device_unregister_loss_callbacks(gs_device_t *device,
							void *data)
{
	for (auto iter = device->loss_callbacks.begin();
	     iter != device->loss_callbacks.end(); ++iter) {
		if (iter->data == data) {
			device->loss_callbacks.erase(iter);
			break;
		}
	}
}
