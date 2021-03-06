/******************************************************************************
    Copyright (C) 2016 by Hugh Bailey <obs.jim@gmail.com>

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

#include "d3d11-subsystem.hpp"

void gs_vertex_buffer::Rebuild()
{
	uvBuffers.clear();
	uvSizes.clear();
	BuildBuffers();
}

void gs_index_buffer::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateBuffer(&bd, &srd, &indexBuffer);
	if (FAILED(hr))
		throw HRError("Failed to create buffer", hr);
}

void gs_texture_2d::RebuildSharedTextureFallback()
{
	static const gs_color_format format = GS_BGRA;
	static const DXGI_FORMAT dxgi_format_resource =
		ConvertGSTextureFormatResource(format);
	static const DXGI_FORMAT dxgi_format_view =
		ConvertGSTextureFormatView(format);
	static const DXGI_FORMAT dxgi_format_view_linear =
		ConvertGSTextureFormatViewLinear(format);

	td = {};
	td.Width = 2;
	td.Height = 2;
	td.MipLevels = 1;
	td.Format = dxgi_format_resource;
	td.ArraySize = 1;
	td.SampleDesc.Count = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	width = td.Width;
	height = td.Height;
	dxgiFormatResource = dxgi_format_resource;
	dxgiFormatView = dxgi_format_view;
	dxgiFormatViewLinear = dxgi_format_view_linear;
	levels = 1;

	viewDesc = {};
	viewDesc.Format = dxgi_format_view;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = 1;

	viewDescLinear = viewDesc;
	viewDescLinear.Format = dxgi_format_view_linear;

	isShared = false;
}

void gs_texture_2d::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	if (isShared) {
		hr = dev->OpenSharedResource((HANDLE)(uintptr_t)sharedHandle,
					     __uuidof(ID3D11Texture2D),
					     (void **)&texture);
		if (FAILED(hr)) {
			blog(LOG_WARNING,
			     "Failed to rebuild shared texture: ", "0x%08lX",
			     hr);
			RebuildSharedTextureFallback();
		}
	}

	if (!isShared) {
		hr = dev->CreateTexture2D(
			&td, data.size() ? srd.data() : nullptr, &texture);
		if (FAILED(hr))
			throw HRError("Failed to create 2D texture", hr);
	}

	hr = dev->CreateShaderResourceView(texture, &viewDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create SRV", hr);

	if (viewDesc.Format == viewDescLinear.Format) {
		shaderResLinear = shaderRes;
	} else {
		hr = dev->CreateShaderResourceView(texture, &viewDescLinear,
						   &shaderResLinear);
		if (FAILED(hr))
			throw HRError("Failed to create linear SRV", hr);
	}

	if (isRenderTarget)
		InitRenderTargets();

	if (isGDICompatible) {
		hr = texture->QueryInterface(__uuidof(IDXGISurface1),
					     (void **)&gdiSurface);
		if (FAILED(hr))
			throw HRError("Failed to create GDI surface", hr);
	}

	acquired = false;

	if ((td.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
		ComQIPtr<IDXGIResource> dxgi_res(texture);
		if (dxgi_res)
			GetSharedHandle(dxgi_res);
		device_texture_acquire_sync(this, 0, INFINITE);
	}
}

void gs_texture_2d::RebuildNV12_Y(ID3D11Device *dev)
{
	gs_texture_2d *tex_uv = pairedNV12texture;
	HRESULT hr;

	hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create 2D texture", hr);

	hr = dev->CreateShaderResourceView(texture, &viewDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create Y SRV", hr);

	if (viewDesc.Format == viewDescLinear.Format) {
		shaderResLinear = shaderRes;
	} else {
		hr = dev->CreateShaderResourceView(texture, &viewDescLinear,
						   &shaderResLinear);
		if (FAILED(hr))
			throw HRError("Failed to create linear Y SRV", hr);
	}

	if (isRenderTarget)
		InitRenderTargets();

	tex_uv->RebuildNV12_UV(dev);

	acquired = false;

	if ((td.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
		ComQIPtr<IDXGIResource> dxgi_res(texture);
		if (dxgi_res)
			GetSharedHandle(dxgi_res);
		device_texture_acquire_sync(this, 0, INFINITE);
	}
}

void gs_texture_2d::RebuildNV12_UV(ID3D11Device *dev)
{
	gs_texture_2d *tex_y = pairedNV12texture;
	HRESULT hr;

	texture = tex_y->texture;

	hr = dev->CreateShaderResourceView(texture, &viewDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create UV SRV", hr);

	if (viewDesc.Format == viewDescLinear.Format) {
		shaderResLinear = shaderRes;
	} else {
		hr = dev->CreateShaderResourceView(texture, &viewDescLinear,
						   &shaderResLinear);
		if (FAILED(hr))
			throw HRError("Failed to create linear UV SRV", hr);
	}

	if (isRenderTarget)
		InitRenderTargets();
}

void gs_zstencil_buffer::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil texture", hr);

	hr = dev->CreateDepthStencilView(texture, &dsvd, &view);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil view", hr);
}

void gs_stage_surface::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateTexture2D(&td, nullptr, &texture);
	if (FAILED(hr))
		throw HRError("Failed to create staging surface", hr);
}

void gs_sampler_state::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateSamplerState(&sd, state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create sampler state", hr);
}

void gs_vertex_shader::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	hr = dev->CreateVertexShader(data.data(), data.size(), nullptr,
				     &shader);
	if (FAILED(hr))
		throw HRError("Failed to create vertex shader", hr);

	const UINT layoutSize = (UINT)layoutData.size();
	if (layoutSize > 0) {
		hr = dev->CreateInputLayout(layoutData.data(), layoutSize,
					    data.data(), data.size(), &layout);
		if (FAILED(hr))
			throw HRError("Failed to create input layout", hr);
	}

	if (constantSize) {
		hr = dev->CreateBuffer(&bd, NULL, &constants);
		if (FAILED(hr))
			throw HRError("Failed to create constant buffer", hr);
	}

	for (gs_shader_param &param : params) {
		param.nextSampler = nullptr;
		param.curValue.clear();
		gs_shader_set_default(&param);
	}
}

void gs_pixel_shader::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;

	hr = dev->CreatePixelShader(data.data(), data.size(), nullptr, &shader);
	if (FAILED(hr))
		throw HRError("Failed to create pixel shader", hr);

	if (constantSize) {
		hr = dev->CreateBuffer(&bd, NULL, &constants);
		if (FAILED(hr))
			throw HRError("Failed to create constant buffer", hr);
	}

	for (gs_shader_param &param : params) {
		param.nextSampler = nullptr;
		param.curValue.clear();
		gs_shader_set_default(&param);
	}
}

void gs_swap_chain::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = device->factory->CreateSwapChain(dev, &swapDesc, &swap);
	if (FAILED(hr))
		throw HRError("Failed to create swap chain", hr);
	Init();
}

void gs_timer::Rebuild(ID3D11Device *dev)
{
	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_TIMESTAMP;
	desc.MiscFlags = 0;
	HRESULT hr = dev->CreateQuery(&desc, &query_begin);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
	hr = dev->CreateQuery(&desc, &query_end);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
}

void gs_timer_range::Rebuild(ID3D11Device *dev)
{
	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	desc.MiscFlags = 0;
	HRESULT hr = dev->CreateQuery(&desc, &query_disjoint);
	if (FAILED(hr))
		throw HRError("Failed to create timer", hr);
}

void gs_texture_3d::RebuildSharedTextureFallback()
{
	static const gs_color_format format = GS_BGRA;
	static const DXGI_FORMAT dxgi_format_resource =
		ConvertGSTextureFormatResource(format);
	static const DXGI_FORMAT dxgi_format_view =
		ConvertGSTextureFormatView(format);
	static const DXGI_FORMAT dxgi_format_view_linear =
		ConvertGSTextureFormatViewLinear(format);

	td = {};
	td.Width = 2;
	td.Height = 2;
	td.Depth = 2;
	td.MipLevels = 1;
	td.Format = dxgi_format_resource;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	width = td.Width;
	height = td.Height;
	depth = td.Depth;
	dxgiFormatResource = dxgi_format_resource;
	dxgiFormatView = dxgi_format_view;
	dxgiFormatViewLinear = dxgi_format_view_linear;
	levels = 1;

	viewDesc = {};
	viewDesc.Format = dxgi_format_view;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	viewDesc.Texture3D.MostDetailedMip = 0;
	viewDesc.Texture3D.MipLevels = 1;

	viewDescLinear = viewDesc;
	viewDescLinear.Format = dxgi_format_view_linear;

	isShared = false;
}

void gs_texture_3d::Rebuild(ID3D11Device *dev)
{
	HRESULT hr;
	if (isShared) {
		hr = dev->OpenSharedResource((HANDLE)(uintptr_t)sharedHandle,
					     __uuidof(ID3D11Texture3D),
					     (void **)&texture);
		if (FAILED(hr)) {
			blog(LOG_WARNING,
			     "Failed to rebuild shared texture: ", "0x%08lX",
			     hr);
			RebuildSharedTextureFallback();
		}
	}

	if (!isShared) {
		hr = dev->CreateTexture3D(
			&td, data.size() ? srd.data() : nullptr, &texture);
		if (FAILED(hr))
			throw HRError("Failed to create 3D texture", hr);
	}

	hr = dev->CreateShaderResourceView(texture, &viewDesc, &shaderRes);
	if (FAILED(hr))
		throw HRError("Failed to create 3D SRV", hr);

	if (viewDesc.Format == viewDescLinear.Format) {
		shaderResLinear = shaderRes;
	} else {
		hr = dev->CreateShaderResourceView(texture, &viewDescLinear,
						   &shaderResLinear);
		if (FAILED(hr))
			throw HRError("Failed to create linear 3D SRV", hr);
	}

	acquired = false;

	if ((td.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0) {
		ComQIPtr<IDXGIResource> dxgi_res(texture);
		if (dxgi_res)
			GetSharedHandle(dxgi_res);
		device_texture_acquire_sync(this, 0, INFINITE);
	}
}

void SavedBlendState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateBlendState(&bd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create blend state", hr);
}

void SavedZStencilState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateDepthStencilState(&dsd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil state", hr);
}

void SavedRasterState::Rebuild(ID3D11Device *dev)
{
	HRESULT hr = dev->CreateRasterizerState(&rd, &state);
	if (FAILED(hr))
		throw HRError("Failed to create rasterizer state", hr);
}

const static D3D_FEATURE_LEVEL featureLevels[] = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
};

void gs_device::RebuildDevice()
try {
	ID3D11Device *dev = nullptr;
	HRESULT hr;

	blog(LOG_WARNING, "Device Remove/Reset!  Rebuilding all assets...");

	/* ----------------------------------------------------------------- */

	for (gs_device_loss &callback : loss_callbacks)
		callback.device_loss_release(callback.data);

	gs_obj *obj = first_obj;

	while (obj) {
		switch (obj->obj_type) {
		case gs_type::gs_vertex_buffer:
			((gs_vertex_buffer *)obj)->Release();
			break;
		case gs_type::gs_index_buffer:
			((gs_index_buffer *)obj)->Release();
			break;
		case gs_type::gs_texture_2d:
			((gs_texture_2d *)obj)->Release();
			break;
		case gs_type::gs_zstencil_buffer:
			((gs_zstencil_buffer *)obj)->Release();
			break;
		case gs_type::gs_stage_surface:
			((gs_stage_surface *)obj)->Release();
			break;
		case gs_type::gs_sampler_state:
			((gs_sampler_state *)obj)->Release();
			break;
		case gs_type::gs_vertex_shader:
			((gs_vertex_shader *)obj)->Release();
			break;
		case gs_type::gs_pixel_shader:
			((gs_pixel_shader *)obj)->Release();
			break;
		case gs_type::gs_duplicator:
			((gs_duplicator *)obj)->Release();
			break;
		case gs_type::gs_swap_chain:
			((gs_swap_chain *)obj)->Release();
			break;
		case gs_type::gs_timer:
			((gs_timer *)obj)->Release();
			break;
		case gs_type::gs_timer_range:
			((gs_timer_range *)obj)->Release();
			break;
		case gs_type::gs_texture_3d:
			((gs_texture_3d *)obj)->Release();
			break;
		}

		obj = obj->next;
	}

	for (auto &state : zstencilStates)
		state.Release();
	for (auto &state : rasterStates)
		state.Release();
	for (auto &state : blendStates)
		state.Release();

	context->ClearState();
	context->Flush();

	context.Release();
	device.Release();
	adapter.Release();
	factory.Release();

	/* ----------------------------------------------------------------- */

	InitFactory(adpIdx);

	uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
			       createFlags, featureLevels,
			       sizeof(featureLevels) /
				       sizeof(D3D_FEATURE_LEVEL),
			       D3D11_SDK_VERSION, &device, nullptr, &context);
	if (FAILED(hr))
		throw HRError("Failed to create device", hr);

	dev = device;

	obj = first_obj;
	while (obj) {
		switch (obj->obj_type) {
		case gs_type::gs_vertex_buffer:
			((gs_vertex_buffer *)obj)->Rebuild();
			break;
		case gs_type::gs_index_buffer:
			((gs_index_buffer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_texture_2d: {
			gs_texture_2d *tex = (gs_texture_2d *)obj;
			if (!tex->nv12) {
				tex->Rebuild(dev);
			} else if (!tex->chroma) {
				tex->RebuildNV12_Y(dev);
			}
		} break;
		case gs_type::gs_zstencil_buffer:
			((gs_zstencil_buffer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_stage_surface:
			((gs_stage_surface *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_sampler_state:
			((gs_sampler_state *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_vertex_shader:
			((gs_vertex_shader *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_pixel_shader:
			((gs_pixel_shader *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_duplicator:
			try {
				((gs_duplicator *)obj)->Start();
			} catch (...) {
				((gs_duplicator *)obj)->Release();
			}
			break;
		case gs_type::gs_swap_chain:
			((gs_swap_chain *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_timer:
			((gs_timer *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_timer_range:
			((gs_timer_range *)obj)->Rebuild(dev);
			break;
		case gs_type::gs_texture_3d:
			((gs_texture_3d *)obj)->Rebuild(dev);
			break;
		}

		obj = obj->next;
	}

	curRenderTarget = nullptr;
	curZStencilBuffer = nullptr;
	curRenderSide = 0;
	memset(&curTextures, 0, sizeof(curTextures));
	memset(&curSamplers, 0, sizeof(curSamplers));
	curVertexBuffer = nullptr;
	curIndexBuffer = nullptr;
	curVertexShader = nullptr;
	curPixelShader = nullptr;
	curSwapChain = nullptr;
	zstencilStateChanged = true;
	rasterStateChanged = true;
	blendStateChanged = true;
	curDepthStencilState = nullptr;
	curRasterState = nullptr;
	curBlendState = nullptr;
	curToplogy = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	for (auto &state : zstencilStates)
		state.Rebuild(dev);
	for (auto &state : rasterStates)
		state.Rebuild(dev);
	for (auto &state : blendStates)
		state.Rebuild(dev);

	for (gs_device_loss &callback : loss_callbacks)
		callback.device_loss_rebuild(device.Get(), callback.data);

} catch (const char *error) {
	bcrash("Failed to recreate D3D11: %s", error);

} catch (const HRError &error) {
	bcrash("Failed to recreate D3D11: %s (%08lX)", error.str, error.hr);
}
